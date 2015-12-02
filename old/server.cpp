#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>


enum Direction {UP,RIGHT,DOWN,LEFT};
enum Event {MOVEDUP,MOVEDRIGHT,MOVEDOWN,MOVEDLEFT,MOVEDOUTOF,MOVEDINTO};
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
    int layer;
    public:
    SnakeComponent(uint8_t startX,uint8_t startY,Direction startDir,uint16_t col){
      x = startX;
      y = startY;
      dir = startDir;
      colour = col;
      layer = 0;
    }  
    void update(){
      x -= (dir%2 == 1) ? dir - 2 : 0;  
      y += (dir%2 == 0) ? dir - 1 : 0;
      if(x == 128) x = 0;
      if(x == 255) x = 128;
      if(y == 160) y = 0;
      if(y == 255) y = 159;
      if(layer == 0){
        tft.drawPixel(x,y,colour);
      }
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
    int getLayer(){
      return layer; 
    }
    void setLayer(int newLayer){
      layer = newLayer;  
    }
  };
  struct snakeEvent{
      uint8_t x;
      uint8_t y;
      Event e;
  };
  snakeEvent eventQueue[128];
  uint8_t queueIndex;
  int activeEventIndex;
  int pendingLength;
  SnakeComponent* headComp;
  SnakeComponent* tailComp;
  boolean dead;
  int number;
  public:
  Snake(uint8_t startX, uint8_t startY,Direction startDir, uint16_t col, int startingLength,int num) :
    headComp(new SnakeComponent(startX,startY,startDir,col)) , 
    tailComp(new SnakeComponent(startX,startY,startDir,0x0)) ,
    pendingLength(startingLength),
    queueIndex(0),
    activeEventIndex(0),
    dead(false),
    number(num)
    {
    }
  void update(){
    headComp->update();
    if(pendingLength != 0){
        pendingLength--;
    }
    else{
      if(activeEventIndex != queueIndex && tailComp->getX() == eventQueue[activeEventIndex].x && tailComp->getY() == eventQueue[activeEventIndex].y){
         if(eventQueue[activeEventIndex].e == 4){
                       
         	tailComp->setLayer(1);
         
         	activeEventIndex++;
         }
         else if(eventQueue[activeEventIndex].e == 5){
         	tailComp->setLayer(0);
         	activeEventIndex++;
         }
         else{
         	tailComp->setDirection((Direction)(eventQueue[activeEventIndex++].e));
         }
         if(activeEventIndex == 128) activeEventIndex = 0; 
      }
      tailComp->update();  
    }
  }
  void addEvent(struct snakeEvent e){
    eventQueue[queueIndex++] = e; 
    if(queueIndex == 128)queueIndex = 0;
  }
  void generateEvent(int event){
    snakeEvent temp;
    temp.x = headComp->getX();
    temp.y = headComp->getY();
    temp.e = (Event) event;
    addEvent(temp);
  }
  void setDirection(Direction newDirection){
      if(newDirection%2 != headComp->getDirection()%2){
          headComp->setDirection(newDirection);
          generateEvent(newDirection);
          writeSerializedSnake();
      }   
  }
  void setDirection(int d){
    setDirection((d == 0) ? UP : (d == 1) ? RIGHT : (d == 2) ? DOWN : LEFT);  
  }
  boolean isDead(){
      return dead;  
  }
  void setDead(boolean b){
    dead = b;  
  }
  uint8_t getX(){
    return headComp->getX();  
  }
  uint8_t getY(){
    return headComp->getY();  
  }
  uint8_t getTailX(){
    return tailComp->getX();  
  }
  uint8_t getTailY(){
    return tailComp->getY();  
  }
  int getLayer(){
    return headComp->getLayer();  
  }
  int getTailLayer(){
    return tailComp->getLayer();
  }
  void setLayer(int newLayer){
    if(getLayer() != newLayer){
      headComp->setLayer(newLayer);
      if(newLayer == 1){
        generateEvent(4);
        writeSerializedSnake();
      }
      else{
        generateEvent(5);  
      }
      
    }  
  }
  void writeSerializedSnake(){
    Serial2.write(headComp->getX());
    Serial2.write(headComp->getY());
    Serial2.write(headComp->getDirection());  
    Serial2.write(headComp->getLayer());
    Serial2.write(dead);
    Serial2.write(number);
  }
};

const int fps = 24;


typedef struct{
  uint8_t pixel[16][160];
}board;
class GameManager{
  private:
    Snake* s[3];
    JoystickListener* js;
    board* layer1;
    board* layer2;
    uint8_t getPixel(board* layer,uint8_t x,uint8_t y){
      return ((layer->pixel[x/8][y]&(128 >> (x%8))) >> (7 - x%8));  
    }
    void setPixel(board* layer,uint8_t x,uint8_t y, uint8_t value){
      if(getPixel(layer,x,y) != value){
        layer->pixel[x/8][y] = layer->pixel[x/8][y] ^ (128 >> (x%8));
      }
    }
  public:    
    GameManager() :js(new JoystickListener(VERT,HOR,SEL,450)){
      tft.fillScreen(0);
      layer1 = (board*)calloc(1,sizeof(board));
      layer2 = (board*)calloc(1,sizeof(board));      
      s[0] = new Snake(20,20,DOWN,0xFF00,20,1);
      s[1] = new Snake(20,100,RIGHT,0x0FF0,20,2);
      s[2] = new Snake(100,108,UP,0x00FF,20,3);
      for(int i = 0; i < 3; i++){
        s[i]->writeSerializedSnake();  
      }
    }
    void parseClientPacket(HardwareSerial &serial, Snake* s, int snakeNum){
        int i = serial.read();
        switch(i){
          case 4:       
            if(s->getLayer() == 1){
              s->setLayer(0);
            }  
            else{
              s->setLayer(1);
            }
          break;
          default:
            s->setDirection(i);
          break;  
        }
    }
    void run(){
      uint32_t time = millis();
      boolean hasChangedLayer = false;
      while(!(s[0]->isDead()&&s[1]->isDead()&&s[2]->isDead())){
        if(millis() - time > 1000/fps){
          time = millis();
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
          else if(js->isDepressed()&&!hasChangedLayer){          
            s[0]->setLayer(!s[0]->getLayer());
            hasChangedLayer = true; 
          }
          else if(!js->isDepressed()){
            hasChangedLayer = false;  
          }
          if(Serial2.available()){
            //parseClientPacket(Serial2, s[1], 1);
          }
          if(Serial3.available()){
            //parseClientPacket(Serial3, s[2], 2);
          }
          for(int i = 0; i < 3; i++){
            if(s[i]->isDead())continue;
            s[i]->update();
            if(getPixel((s[i]->getLayer() == 0)? layer1 : layer2,s[i]->getX(),s[i]->getY()) == 1){
              s[i]->setDead(true);
              Serial.print("Collided at ");
              Serial.print(s[i]->getX());
              Serial.println(s[i]->getY());
              if(s[i]->getLayer() == 1){
              	s[i]->writeSerializedSnake();
              }
              continue;
            }
            else{
              setPixel((s[i]->getLayer() == 0)? layer1 : layer2,s[i]->getX(),s[i]->getY(),1);
              setPixel((s[i]->getTailLayer() == 0)? layer1 : layer2,s[i]->getTailX(),s[i]->getTailY(),0);
            }
          }          
        }
      }  
      tft.print("ALl Snakes are Dead");
    }
  
};
int main(){
  init();
  tft.initR(INITR_REDTAB); // initialize a ST7735R chip, green tab
  Serial.begin(9600);
  GameManager* gm = new GameManager();
  gm->run();
  Serial.end();
  return 0;
};
