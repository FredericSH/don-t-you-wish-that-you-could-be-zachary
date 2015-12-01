#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>
#include "lines.h"
#include "queues.h"


enum Direction {UP,RIGHT,DOWN,LEFT};
enum Event {MOVEDUP,MOVEDRIGHT,MOVEDDOWN,MOVEDLEFT,MOVEDINTO,MOVEDOUTOF};
const int snakeSpeed = 1;
const int snakeWidth = 1;
llist snakeSegmentsX; //segments sorted by X pivot
llist snakeSegmentsY; //segments sorted by Y pivot

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
    int layer;
    boolean finishedDrawing;
    public:
    SnakeComponent(uint8_t startX,uint8_t startY,Direction startDir,uint16_t col,int layer) : layer(layer){
      x = startX;
      y = startY;
      dir = startDir;
      colour = col;
      finishedDrawing = false;
    }  
    void update(){
      x -= (dir%2 == 1) ? dir - 2 : 0;  
      if(x == 255) x = 127;
      if(x == 128) x = 0;
      y += (dir%2 == 0) ? dir - 1 : 0;
      if(y == 255) y = 159;
      if(y == 160) y = 0;
      draw();
    }
    void draw(){
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
    void setX(uint8_t newX){
      x = newX;
    }
    void setY(uint8_t newY){
      y = newY;  
    }
    boolean isFinishedDrawing(){
      return finishedDrawing;
    }
    void setFinishedDrawing(boolean b){
      finishedDrawing = b;
    }

  };
  queue events;
  int pendingLength;
  int length;
  int layer;
  SnakeComponent* headComp;
  SnakeComponent* tailComp;
  boolean dead;
  public:
  Snake(uint8_t startX, uint8_t startY,Direction startDir, uint16_t col, int startingLength) :
    layer(0),
    headComp(new SnakeComponent(startX,startY,startDir,col,layer)) , 
    tailComp(new SnakeComponent(startX,startY,startDir,0x0,layer)) ,
    pendingLength(startingLength),
    length(startingLength),
    events(qqcreate()),
    dead(false)
    {
    }
  void update(){
    headComp->update();
    if(pendingLength != 0){
        pendingLength--;
    }
    else{
      if(tailComp->getX() == qqlast(events)->x && tailComp->getY() == qqlast(events)->y){
          tailComp->setDirection((Direction)(qqlast(events)->direction));
          int pivot = (Direction)qqlast(events)->direction % 2 == 0 ? qqlast(events)->x : qqlast(events)->y;
          int indic = (Direction)qqlast(events)->direction % 2 == 0 ? qqlast(events)->y : qqlast(events)->x;
          int succ = llremove((Direction)qqlast(events)->direction % 2 == 0 ? snakeSegmentsX : snakeSegmentsY, pivot, indic);
          if(succ == 0)Serial.println("REMOVAL ERROR");
          qqpop(events);
      }
      tailComp->update();  
    }
  }
  void setDirection(Direction newDirection){
      if(abs(newDirection - headComp->getDirection()) != 2){
          headComp->setDirection(newDirection);
          int pivot = (Direction)qqfirst(events)->direction % 2 == 0 ? qqfirst(events)->x : qqfirst(events)->y;
          int pivhead = (Direction)qqfirst(events)->direction%2 == 0 ? qqfirst(events)->y : qqfirst(events)->x;
          int pivtail = (Direction)qqfirst(events)->direction%2 == 0 ? headComp->getY()   : headComp->getX();
          lladd((Direction)qqfirst(events)->direction % 2 == 0 ? snakeSegmentsX : snakeSegmentsY,
            pivot, pivhead, pivtail, qqfirst(events)->layer);
          qqadd(events,headComp->getX(),headComp->getY(),(int)newDirection, layer);
      }   
  }
  uint8_t getX(){
    return headComp->getX();
  }
  uint8_t getY(){
    return headComp->getY();
  }
  void setX(uint8_t x){
    headComp->setX(x);
  }
  void setY(uint8_t y){
    headComp->setY(y);  
  }
  uint8_t getTailX(){
    return tailComp->getX();
  }
  uint8_t getTailY(){
    return tailComp->getY();
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
    headComp->setFinishedDrawing(true);
  }
  int getLayer(){
    return layer;
  }
  uint8_t getQIndex(){
    return (uint8_t)qqlength(events);
  }
  queue getEvent(){
    return events;
  }
  void addEvent(int x, int y, int direction){
    qqadd(events,x,y,direction,layer);
  }
  boolean isHeadDrawing(){
    return headComp->isFinishedDrawing();
  }
  void setHeadDrawing(boolean b){
    headComp->setFinishedDrawing(b);
  }
  boolean isTailDrawing(){
    return headComp->isFinishedDrawing();
  }
  void setTailDrawing(boolean b){
    tailComp->setFinishedDrawing(b);
  }
  void writeSnakeToSerial(int snakeNum,HardwareSerial &s){
    s.write(snakeNum);
    s.write(getX());
    s.write(getY());
    s.write(getDirection());
  }
};

const int fps = 30;

//Receive changes in direction from the clients
//Send to clients if stuff has to be drawn on their screen 
//Changes in direction to snakes on clients screen
typedef struct{
  uint8_t pixel[16][160];
}board;

class GameManager{
  private:
    Snake* s[3];
    board* layer1;
    board* layer2;
    JoystickListener* js;
    void parseClientPacket(HardwareSerial &serial, Snake* s, int snakeNum){
      uint8_t packet = serial.read();
      if(packet > 3){
        int newlayer;
        if(packet == 4){
          newlayer = serial.read();
          s->setLayer(newlayer);
        switch(newlayer){
          case 0:
            s->addEvent(s->getX(),s->getY(),(int)MOVEDINTO);
            break;
          case 1:
            s->writeSnakeToSerial(snakeNum, Serial2);
            break;
        }
      }
      }
      else{
        s->setDirection(packet);
      }
    }
    bool intersects(int X, int Y, int x1, int y1, int x2, int y2){
      if(x1 == x2){
        return(X == x1 && Y > min(y1,y2) && Y < max(y1,y2));
      }else{
        return(Y == y1 && X > min(x1,x2) && X < max(x1,x2));
      }
    }
    
    void handleCollisions(){
      bool isVert;
      int index;
      uint8_t pos,align; //properties of the snake in focus
      
      for(int i = 0; i < 3; i++){
        if(s[i]->isDead())continue;

        for(int j = 0; j < 3; j++){
          int index = s[j]->getQIndex();
          if(i!=j){
            if(intersects(s[i]->getX(),s[i]->getY(), //head coordinates of snake i
              s[j]->getX(), s[j]->getY(), //head coordinates of snake j
              //then the neck of the snake
              index == 0 ? s[j]->getTailX() : qqfirst(s[j]->getEvent())->x,
              index == 0 ? s[j]->getTailY() : qqfirst(s[j]->getEvent())->y)){
                Serial.println("Died hitting neck");
                s[i]->kill();
                break;
            }
          }
          if(index > 0){
            if(intersects(s[i]->getX(),s[i]->getY(),
              s[j]->getTailX(),s[j]->getTailY(),
              qqlast(s[j]->getEvent())->x,
              qqlast(s[j]->getEvent())->y)){
                Serial.println("Died hitting tail");
                s[i]->kill();
                break;
            }
          }
        }
        
        isVert = (s[i]->getDirection() % 2 == 0);
        pos = isVert ? s[i]->getX() : s[i]->getY();
        align = isVert ? s[i]->getY() : s[i]->getX();
        
        line tocheck = llget(isVert ? snakeSegmentsX : snakeSegmentsY, pos); 
        if(tocheck == 0)continue;
        Serial.println("Trying segments");
        line_t* focus = tocheck->first;
        while(focus){
          if(align > min(focus->head,focus->tail) && align < max(focus->head,focus->tail)){
            Serial.println("Died hitting other");
            s[i]->kill();
            break;
          }
          focus = focus->next;
        }
      }
    }
  public:    
    GameManager() : js (new JoystickListener(VERT,HOR,SEL,450)){
      tft.fillScreen(0);
      s[0] = new Snake(20,20,DOWN,0xFF00,20);
      s[1] = new Snake(20,100,RIGHT,0x0FF0,20);
      s[2] = new Snake(100,108,UP,0x00FF,20);
      snakeSegmentsX = llmake();
      snakeSegmentsY = llmake();
      /*layer1 = (board*)malloc(sizeof(board));
      layer2 = (board*)malloc(sizeof(board));
      memset(layer1,2560,0);
      memset(layer2,2560,0);*/
    }
    void run(){
      uint32_t time = millis();
      while(!(s[0]->isDead() && s[1]->isDead() && s[2]->isDead())){
        if(millis() - time > 1000/fps){
          time = millis();
          for(int i = 0; i < 3; i++){
            if(s[i]->isDead())continue;
            s[i]->update();
            /*Serial.print(getLayerTile((s[i]->getLayer())? layer1 : layer2, s[i]->getX(), s[i]->getY()));
            if(getLayerTile((s[i]->getLayer())? layer1 : layer2, s[i]->getX(), s[i]->getY())){
              Serial.print("Snake Died");
              s[i]->kill();
            }
            else{
               setLayerTile((s[i]->getLayer())? layer1 : layer2, s[i]->getX(), s[i]->getY(), true);  
            }*/
            
            handleCollisions();
          }
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
            parseClientPacket(Serial2,s[1], 1);
          }
          if(Serial3.available()){
            parseClientPacket(Serial3,s[2] ,2);
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
