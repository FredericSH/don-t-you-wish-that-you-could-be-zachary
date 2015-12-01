#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>

#include "mem_syms.h"


enum Direction {UP = 0, RIGHT = 1, DOWN = 2, LEFT = 3};
enum Orientation {HORIZONTAL = 0, VERTICAL = 1, NEITHER = 2};

// standard U of A library settings, assuming Atmel Mega SPI pins
#define SD_CS    5  // Chip select line for SD card
#define TFT_CS   6  // Chip select line for TFT display
#define TFT_DC   7  // Data/command line for TFT
#define TFT_RST  8  // Reset line for TFT (or connect to +5V)
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

const int VERT = 0;
const int HOR = 1;
const int SEL = 9;

// maximum index of an element in line segment array i.e. [0, maxSegs]
#define maxSegs 30

struct snakeSeg{
  uint8_t x1;
  uint8_t y1;
  uint8_t x2;
  uint8_t y2;
  uint8_t layer;
  Direction dir;
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
  public:
    // circular buffer containing all the line segments of the snake, with the form
    // x1,y1,x2,y2,layer,dir 
    snakeSeg lineSegments[maxSegs + 1];
    uint8_t head;
    uint8_t tail;

    uint8_t pendingLength;
    int layer;

    // number of segments in snake
    uint8_t length;
  
    uint16_t colour;

    // current state of the life of the snake    
    boolean dead;

    Snake(uint8_t startX, uint8_t startY, Direction startDir, uint16_t col, int startingLength) :
      pendingLength(startingLength) {
        head = 0;
        tail = 0;
        colour = col;
        pendingLength = startingLength;
        dead = false;
        layer = 0;
        lineSegments[head].x1 = startX;
        lineSegments[head].y1 = startY;
        lineSegments[head].x2 = startX;
        lineSegments[head].y2 = startY;
        lineSegments[head].dir = startDir;
        length = 1;
      }
    
    // safe incrementing of x and y as long as n < 255 - 127
    // don't initialize snake with more than 127 pending length
    void incrSafe(uint8_t &x, uint8_t n, uint8_t max){
      x += n;
      if(x >= max){
        x -= max;
      }
    }
    void decrSafe(uint8_t &x, uint8_t n, uint8_t max){
      if(x >= n){
        x -= n;
      }
      else{
        x = (max + x) - n;
      }
    }

    void update(){
      // updates head first and then tail
      // and checks to make sure snake is on screen.

      // head movement
      switch(lineSegments[head].dir){
        case LEFT:
          decrSafe(lineSegments[head].x2, 1, 127);
          break;
        case RIGHT:
          incrSafe(lineSegments[head].x2, 1, 127);
          break;
        case UP:
          decrSafe(lineSegments[head].y2, 1, 159);
          break;
        case DOWN:
          incrSafe(lineSegments[head].y2, 1, 159);
          break;
      }

      // check to see if the tail is finished with going through this segment 
      // and empty segment and update tail if it is
      if(lineSegments[tail].x1 == lineSegments[tail].x2 && 
            lineSegments[tail].y1 == lineSegments[tail].y2){
        length--;
        // free up the tail
        incrSafe(tail, 1, maxSegs);
      }

      // tail movement waits pendingLength head moves before starting tail movement
      if(pendingLength > 0){
        pendingLength --;
      }
      else{
        switch(lineSegments[tail].dir){
          case LEFT:
            decrSafe(lineSegments[tail].x1, 1, 127);
            break;
          case RIGHT:
            incrSafe(lineSegments[tail].x1, 1, 127);
            break;
          case UP:
            decrSafe(lineSegments[tail].y1, 1, 159);
            break;
          case DOWN:
            incrSafe(lineSegments[tail].y1, 1, 159);
            break;
        }
      }

      // draw head and tail pixels
      tft.drawPixel(lineSegments[head].x2, lineSegments[head].y2, colour);
      tft.drawPixel(lineSegments[tail].x1, lineSegments[tail].y1, 0x0);
    }
    void setDirection(Direction newDir){
      if(queueFull()) { return; }

      length++;
      uint8_t prevHead = head;
      incrSafe(head, 1, maxSegs);
      lineSegments[head].x1 = lineSegments[prevHead].x2;
      lineSegments[head].y1 = lineSegments[prevHead].y2;
      lineSegments[head].x2 = lineSegments[head].x1;
      lineSegments[head].y2 = lineSegments[head].y1;
      lineSegments[head].layer = lineSegments[prevHead].layer;
      lineSegments[head].dir = newDir;
    }
    void setLayer(uint8_t newLayer){
      if(queueFull()) { return; }

      uint8_t prevHead = head;
      incrSafe(head, 1, maxSegs);
      lineSegments[head].x1 = lineSegments[prevHead].x2;
      lineSegments[head].y1 = lineSegments[prevHead].y2;
      lineSegments[head].x2 = lineSegments[head].x1;
      lineSegments[head].y2 = lineSegments[head].y1;
      lineSegments[head].layer = newLayer; 
      lineSegments[head].dir = lineSegments[prevHead].dir;
    }
    bool willCollide(uint8_t x, uint8_t y, Direction dir, bool isSelf){
      uint8_t tHead = head;
      for(uint8_t i = tail; i < tail; incrSafe(i, 1, length + 1)){
        Serial.println(i);
        if(1){
          Serial.print("\ntHead is ");
          Serial.println(tHead);
          Serial.print("tail is ");
          Serial.println(tail);
          debug("Currentyly in collide");
        }
        // head always intersects itself, so we don't bother checking it
        if(!(isSelf && i == head)) {
          Serial.println("got to check 0");
          // checks if temp head is pointing in the direction we want
          uint8_t tmpX1 = lineSegments[i].x1;
          uint8_t tmpX2 = lineSegments[i].x2;
          uint8_t tmpY1 = lineSegments[i].y1;
          uint8_t tmpY2 = lineSegments[i].y2;
          Direction tmpDir = lineSegments[i].dir;
          if((lineSegments[i].dir % 2) != (dir % 2)){
            Serial.println("got to check #1");
            // are we vertical?
            if(tmpX1 = tmpX2){
              Serial.println("got to check #2");
              if(x == tmpY1){
                Serial.println("got to check #3");
                bool inBetween = min(tmpY1,tmpY2) <= y && y <= max(tmpY1, tmpY2);
                if((tmpDir == DOWN && tmpY2 > tmpY1) || tmpDir == DOWN && tmpY1 > tmpY2){
                  if(!inBetween){
                    Serial.println("collided");
                    return true;
                  }
                }
                else if(inBetween){
                  Serial.println("collided");
                  return true;
                }
              }           
            }
            // else we're horizontal
            else {
              if(y == tmpY1){
                Serial.println("got to check #2 2");
                bool inBetween = min(tmpX1,tmpX2) <= x && x <= max(tmpX1, tmpX2);
                if((tmpDir == LEFT && tmpX2 > tmpX1) || tmpDir == RIGHT && tmpX1 > tmpX2){
                  Serial.println("got to check #3 2");
                  if(!inBetween){
                    Serial.println("collided");
                    return true;
                  }
                }
                else if(inBetween){
                  Serial.println("collided");
                  return true;
                }
              }
            }
          }
        }
      }
      //debug("Leaving collide");
      return false;
    }
    uint8_t getX(){
      return lineSegments[head].x2;
    }
    uint8_t getY(){
      return lineSegments[head].y2;
    }
    void setX(uint8_t x){
      lineSegments[head].x2 = x;
    }
    void setY(uint8_t y){
      lineSegments[head].y2 = y;
    }
    uint8_t getTailX(){
      return lineSegments[tail].x1;
    }
    uint8_t getTailY(){
      return lineSegments[tail].y1;
    }
    Direction getDirection(){
      return lineSegments[head].dir;
    }
    uint8_t getLayer(){
      return lineSegments[head].layer;
    }
    bool queueFull(){
      if(head == maxSegs && tail == 0){
        // line segment queue is full
        return true;
      }
      if(head + 1 == tail){
        // line segment queue is full
        return true;
      }
      return false;
    }
    void debug(char* s){
      Serial.println(s);
      Serial.println("Available Mem");
      Serial.println(AVAIL_MEM);
      Serial.println("Debug: h, d, x1, y1, x2, y2");
      Serial.print(head);
      Serial.print(" ");
      Serial.print(lineSegments[head].dir);
      Serial.print(" ");
      Serial.print(lineSegments[head].x1);
      Serial.print(" ");
      Serial.print(lineSegments[head].y1);
      Serial.print(" ");
      Serial.print(lineSegments[head].x2);
      Serial.print(" ");
      Serial.println(lineSegments[head].y2);
      Serial.println("tail");
      Serial.print(tail);
      Serial.print(" ");
      Serial.print(lineSegments[tail].dir);
      Serial.print(" ");
      Serial.print(lineSegments[tail].x1);
      Serial.print(" ");
      Serial.print(lineSegments[tail].y1);
      Serial.print(" ");
      Serial.print(lineSegments[tail].x2);
      Serial.print(" ");
      Serial.println(lineSegments[tail].y2);
    }
    void kill(){
      dead = true;
    }
    boolean isDead(){
      return dead;  
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

class GameManager{
  private:
    Snake* s[3];
    JoystickListener* js;
    Orientation dirFlag;
    // TODO This client stuff needs to be implemented to work with the current snake
    /*void parseClientPacket(HardwareSerial &serial, Snake* s, int snakeNum){
      uint8_t packet = serial.read();
      if(packet > 3){
      int newlayer;
      if(packet == 4){
      newlayer = serial.read();
      s->setLayer(newlayer);
      switch(newlayer){
      case 0:
      snakeSeg e;
      e.x = s->getX();
      e.y = s->getY();
      e.e = MOVEDINTO;
      s->addEvent(e);
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
      }*/
  public:    
    GameManager() : js (new JoystickListener(VERT,HOR,SEL,450)){
      tft.fillScreen(0);
      dirFlag = NEITHER;
      s[0] = new Snake(64,80,DOWN,0xFF00,20);
      s[1] = new Snake(20,100,RIGHT,0x0FF0,20);
      s[2] = new Snake(100,108,UP,0x00FF,20);
    }
    // what's the previous direction that was pressed
    void run(){
      uint32_t time = millis();
      while(!(s[0]->isDead() && s[1]->isDead() && s[2]->isDead())){
        if(millis() - time > 1000/fps){
          time = millis();
          for(int i = 0; i < 3; i++){
            if(!s[i]->isDead()){
              s[i]->update();
              uint8_t tmpX = s[i]->getX();
              uint8_t tmpY = s[i]->getY();
              Direction tmpDir = s[i]->getDirection();
              for(int j = 0; j < 3; j++){
                if(s[j]->willCollide(tmpX, tmpY, tmpDir, (i == j))){
                  Serial.print("snake is : ");
                  Serial.println(i);
                  Serial.println("killed by : ");
                  Serial.println(j);
                  s[i]->kill();
                  break;
                }
              }
            }
          }
          if(js->isPushed() && !s[0]->queueFull()){
            int deltaH = js->getHorizontal() - js->getHorizontalBaseline();
            int deltaV = js->getVertical() - js->getVerticalBaseline();
            if(abs(deltaH) > abs(deltaV) && (dirFlag != HORIZONTAL)){
              s[0]->setDirection((deltaH > 0) ? RIGHT : LEFT);
              dirFlag = HORIZONTAL;
              //s[0]->debug("On horizontal");
            }
            else if(abs(deltaV) > abs(deltaH) && dirFlag != VERTICAL){
              s[0]->setDirection((deltaV > 0) ? DOWN : UP);
              dirFlag = VERTICAL;
              //s[0]->debug("On vertical");
            }
          }
          // TODO more client stuff
          /*if(Serial2.available()){
            parseClientPacket(Serial2,s[1], 1);
            }
            if(Serial3.available()){
            parseClientPacket(Serial3,s[2] ,2);
          }*/
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
}
