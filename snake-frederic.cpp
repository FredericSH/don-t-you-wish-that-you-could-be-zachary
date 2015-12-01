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
    // indices of head and tail of snake
    uint8_t head;
    uint8_t tail;

    uint8_t pendingLength;
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
        lineSegments[head].x1 = startX;
        lineSegments[head].y1 = startY;
        lineSegments[head].x2 = startX;
        lineSegments[head].y2 = startY;
        lineSegments[head].dir = startDir;
        lineSegments[head].layer = 0;
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
        x = (max+ x) - n;
      }
    }

    void update(){
      // updates head first and then tail
      // and checks to make sure snake is on screen.

      // head movement
      switch(lineSegments[head].dir){
        case LEFT:
          if(lineSegments[head].x2 == 0) { 
            kill();
            break;
          }
          decrSafe(lineSegments[head].x2, 1, 127);
          break;
        case RIGHT:
          if(lineSegments[head].x2 == 126) { 
            kill();
            break;
          }
          incrSafe(lineSegments[head].x2, 1, 127);
          break;
        case UP:
          if(lineSegments[head].y2 == 0) { 
            kill();
            break;
          }
          decrSafe(lineSegments[head].y2, 1, 159);
          break;
        case DOWN:
          if(lineSegments[head].y2 == 158) { 
            kill();
            break;
          }
          incrSafe(lineSegments[head].y2, 1, 159);
          break;
      }

      // check to see if the tail is finished with going through this segment 
      // and empty segment and update tail if it is
      if(lineSegments[tail].x1 == lineSegments[tail].x2 && 
          lineSegments[tail].y1 == lineSegments[tail].y2){
        // free up the tail
        length--;
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

      // draw head and tail pixels if they are on the correct layer
      // otherwise send drawing info to client
      if(lineSegments[head].layer == 0){
        tft.drawPixel(lineSegments[head].x2, lineSegments[head].y2, colour);
      }
      if(lineSegments[tail].layer == 0){
        tft.drawPixel(lineSegments[tail].x1, lineSegments[tail].y1, 0x0);
      }
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

      length++;

      uint8_t prevHead = head;
      incrSafe(head, 1, maxSegs);
      lineSegments[head].x1 = lineSegments[prevHead].x2;
      lineSegments[head].y1 = lineSegments[prevHead].y2;
      lineSegments[head].x2 = lineSegments[head].x1;
      lineSegments[head].y2 = lineSegments[head].y1;
      lineSegments[head].layer = newLayer; 
      lineSegments[head].dir = lineSegments[prevHead].dir;
    }
    bool checkLine(uint8_t x, uint8_t y, uint8_t i, Direction dir, uint8_t layer){
      uint8_t tmpX1 = lineSegments[i].x1;
      uint8_t tmpX2 = lineSegments[i].x2;
      uint8_t tmpY1 = lineSegments[i].y1;
      uint8_t tmpY2 = lineSegments[i].y2;
      Direction tmpDir = lineSegments[i].dir;
      if(tmpDir % 2 != dir % 2 && layer == lineSegments[i].layer){
        if(intersects(x, y, tmpX1, tmpY1, tmpX2, tmpY2)) return true;
      }
      return false;
    }
    bool intersects(uint8_t X, uint8_t Y, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2){
      if(x1 == x2){
        return(X == x1 && Y >= min(y1,y2) && Y <= max(y1,y2));
      }else{
        return(Y == y1 && X >= min(x1,x2) && X <= max(x1,x2));
      }
    }
    bool willCollide(uint8_t x, uint8_t y, Direction dir, uint8_t layer){
      if(head > tail){
        for(uint8_t i = tail; i < head; i++){
          if(checkLine(x, y, i, dir, layer)) return true;
        }
      }
      else if(head == tail){
        if(checkLine(x, y, head, dir, layer)) return true;
      }
      else{
        for(uint8_t i = head; i < tail; i++){
          if(checkLine(x, y, i, dir, layer)) return true;
        }
      }
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
      dirFlag = VERTICAL;
      s[0] = new Snake(64,80,DOWN,0xFF00,30);
      s[1] = new Snake(20,100,RIGHT,0x0FF0,30);
      s[2] = new Snake(100,108,UP,0x00FF,30);
    }
    // what's the previous direction that was pressed
    void run(){
      bool handled;
      uint32_t time = millis();
      while(!(s[0]->isDead() && s[1]->isDead() && s[2]->isDead())){
        if(millis() - time > 1000/fps){
          time = millis();
          for(int i = 0; i < 3; i++){
            if(!s[i]->isDead()){
              s[i]->update();
              uint8_t tmpX = s[i]->getX();
              uint8_t tmpY = s[i]->getY();
              uint8_t tmpLayer = s[i]->getLayer();
              Direction tmpDir = s[i]->getDirection();
              for(int j = 0; j < 3; j++){
                if(s[j]->willCollide(tmpX, tmpY, tmpDir, tmpLayer)){
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
          if(js->isDepressed()){
            if(!handled){
              s[0]->setLayer(!s[0]->getLayer());
              handled = 1;
            }
          }else{
            handled = 0;
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
