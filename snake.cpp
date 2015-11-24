#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>


enum Direction {UP,RIGHT,DOWN,LEFT};
enum Event {MOVEDUP,MOVEDRIGHT,MOVEDOWN,MOVEDLEFT,MOVEDHIGHER,MOVEDLOWER};
const int snakeSpeed = 1;
const int snakeWidth = 1;

// standard U of A library settings, assuming Atmel Mega SPI pins
#define SD_CS    5  // Chip select line for SD card
#define TFT_CS   6  // Chip select line for TFT display
#define TFT_DC   7  // Data/command line for TFT
#define TFT_RST  8  // Reset line for TFT (or connect to +5V)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

const int VERT = 0;
const int HOR = 1;
const int SEL = 9;

struct snakeEvent{
      uint8_t x;
      uint8_t y;
      Event e;
  };

class JoystickListener{
  private:
    int horizontalPin;
    int verticalPin;
    int depressionPin;
    int verticalBaseline;
    int horizontalBaseline;
    int threshold;
  public:
    // Pass the joystickListener the pin numbers that the joystick is hooked up to as well as the minimum threshold for input.
    JoystickListener(int vert, int hort, int sel, int thresh){
      verticalPin = vert;
      horizontalPin = hort;
      depressionPin = sel; 
      digitalWrite(depressionPin, HIGH);
      verticalBaseline = analogRead(verticalPin);
      horizontalBaseline = analogRead(horizontalPin);
      threshold = thresh;
      
    }
    //Returns the value of the vertical pin
   int getVertical(){
     return analogRead(verticalPin);
   }
   //Returns the original baseline of the vertical pin
   int getVerticalBaseline(){
     return verticalBaseline;
   }
   //Returns the value of the horizontal pin
   int getHorizontal(){
     return analogRead(horizontalPin); 
   }
   //Returs the original baseline of the horizontal pin
   int getHorizontalBaseline(){
     return horizontalBaseline; 
   }   
   //Returns the state of the Joystick's button
   boolean isDepressed(){
     return !digitalRead(depressionPin); 
   }
   //Returns true if the joystick is pushed horizontally past the minimum threshold, false otherwise
   boolean isPushedHorizontally(){
     return (abs(horizontalBaseline - getHorizontal()) > threshold);
   }
   //Returns true if the joystick is pushed vertically past the minimum threshold, false otherwise
   boolean isPushedVertically(){
     return (abs(verticalBaseline - getVertical()) > threshold);
   }
   //Returns true if the joystick is pushed vertically or horiztonally, false if not pushed past either threshold.
   boolean isPushed(){
     return isPushedVertically() || isPushedHorizontally();
   } 
};

class Snake{
  private:
  class SnakeComponent{
    private:
    uint8_t x;
    uint8_t y;
    uint16_t colour;
    Direction dir;
    public:
    SnakeComponent(uint8_t startX,uint8_t startY,Direction startDir,uint16_t col){
      x = startX;
      y = startY;
      dir = startDir;
      colour = col;
    }  
    void update(){
      x -= (dir%2 == 1) ? dir - 2 : 0;  
      y += (dir%2 == 0) ? dir - 1 : 0;
      tft.drawPixel(x,y,colour);
    }
    void setDirection(Direction newDirection){
      dir = newDirection;  
    }
    Direction getDirection(){
      return dir;  
    }
    uint8_t getX(){
      return x;  
    }
    uint8_t getY(){
      return y;  
    }
  };
  snakeEvent eventQueue[128];
  uint8_t queueIndex;
  int pendingLength;
  int layer;
  SnakeComponent* headComp;
  SnakeComponent* tailComp;
  boolean dead;
  public:
  Snake(uint8_t startX, uint8_t startY,Direction startDir, uint16_t col, int startingLength) :
    headComp(new SnakeComponent(startX,startY,startDir,col)) , 
    tailComp(new SnakeComponent(startX,startY,startDir,0x0)) ,
    pendingLength(startingLength),
    layer(1),
    queueIndex(0),
    dead(false)
    {
    }
  void update(){
    headComp->update();
    if(pendingLength != 0){
        pendingLength--;
    }
    else{
      Serial.println(eventQueue[0].x);
      Serial.println(eventQueue[0].y);
      Serial.println(eventQueue[0].e);
      if(tailComp->getX() == eventQueue[0].x && tailComp->getY() == eventQueue[0].y){
        tailComp->setDirection((Direction)(eventQueue[0].e));
        memmove(&eventQueue[0], &eventQueue[1], sizeof(eventQueue) - sizeof(*eventQueue));
        queueIndex--;
      }
      tailComp->update();  
    }
  }
  void setDirection(Direction newDirection){
      if(abs(newDirection - headComp->getDirection()) != 2){
          headComp->setDirection(newDirection);
          snakeEvent temp;
          temp.x = headComp->getX();
          temp.y = headComp->getY();
          temp.e = (Event) newDirection;
          eventQueue[queueIndex++] = temp; 
      }   
  }
  uint8_t getX(){
    return headComp->getX();
  }
  uint8_t getY(){
    return headComp->getY();
  }
  Direction getDirection(){
    return headComp->getDirection();
  }
  void setDirection(int d){
    setDirection((d == 0) ? UP : (d == 1) ? RIGHT : (d == 2) ? DOWN : LEFT);  
  }
  void kill(){
    dead = true;
  }
  boolean isDead(){
      return dead;  
  }
  void setLayer(int newLayer){
    layer = newLayer;
  }
  int getLayer(){
    return layer;
  }
  uint8_t getQIndex(){
    return queueIndex;
  }
  snakeEvent getEvent(int index){
    return eventQueue[index];
  }
};

const int fps = 30;


//Receive changes in direction from the clients
//Send to clients if stuff has to be drawn on their screen 
//Changes in direction to snakes on clients screen

class GameManager{
  private:
    Snake* s[3];
    JoystickListener* js;
    void parseClientPacket(uint8_t packet, Snake* s){
      if(packet > 3){
        if(packet == 4){
          s->setLayer( s->getLayer() + 1 %3);
          
        }else if(packet ==5){
          s->setLayer( s->getLayer() - 1 %3);
          
        }
      }
      else{
        s->setDirection(packet);
      }
    }
  public:    
    GameManager() : js (new JoystickListener(VERT,HOR,SEL,450)){
      tft.fillScreen(0);
      s[0] = new Snake(20,20,DOWN,0xFF00,20);
      s[1] = new Snake(20,100,RIGHT,0x0FF0,20);
      s[2] = new Snake(100,108,UP,0x00FF,20);
    }
    void detectCollision(){
      bool isVert = (s->getDirection() % 2 == 0);
      
      int index = s->getQIndex() - 2;
      while(index > 0){
        Serial.println(index);
        uint8_t align1 = isVert ? s->getY() : s->getX();
        uint8_t align2 = isVert ? s->getEvent(index).y : s->getEvent(index).x;
        if(align1 == align2){
          uint8_t pos = isVert ? s->getX() : s->getY();
          uint8_t A = isVert ? s->getEvent(index).x : s->getEvent(index).y;
          uint8_t B = isVert ? s->getEvent(index-1).x : s->getEvent(index-1).y;
          if((pos >= A && pos <= B) || (pos <= A && pos >= B)) s->kill();
        }
        index-=2;
      }
    }
    void run(){
      uint32_t time = millis();
      while(!s[0]->isDead()){
        if(millis() - time > 1000/fps){
          time = millis();
          for(int i = 0; i < 3; i++){
            s[i]->update();
          }
          detectCollision();
          if(js->isPushed()){
            int deltaH = js->getHorizontal() - js->getHorizontalBaseline();
            int deltaV = js->getVertical() - js->getVerticalBaseline();
            if(abs(deltaH) > abs(deltaV)){
              s[0]->setDirection((deltaH > 0) ? 1 : 3);    
            }
            else{
              s[0]->setDirection((deltaV > 0) ? 2 : 0);
            }
          }
          if(Serial2.available()){
            parseClientPacket(Serial2.read(),s[1]);
          }
          if(Serial3.available()){
            parseClientPacket(Serial3.read(),s[2]);
          }
        }
      }  
    }
};
int main(){
  init();
  tft.initR(INITR_REDTAB); // initialize a ST7735R chip, green tab
  Serial.begin(9600);
  Serial2.begin(9600);
  Serial3.begin(9600);
  GameManager* gm = new GameManager();
  gm->run();
  Serial.end();
  return 0;
};
