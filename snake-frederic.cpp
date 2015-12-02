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

// is this arduino server or client
bool isServer;

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
      //if(isServer){
      //  if(lineSegments[head].layer == 0){
          tft.drawPixel(lineSegments[head].x2, lineSegments[head].y2, colour);
      /*  }
        if(lineSegments[tail].layer == 0){
          tft.drawPixel(lineSegments[tail].x1, lineSegments[tail].y1, 0x0);
        }
      }
      else{
        if(lineSegments[head].layer == 1){
          tft.drawPixel(lineSegments[head].x2, lineSegments[head].y2, colour);
        }
        if(lineSegments[tail].layer == 0){ */
          tft.drawPixel(lineSegments[tail].x1, lineSegments[tail].y1, 0x0);
    //    }
    ///  }
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
      if(layer == lineSegments[i].layer){
        if(tmpDir % 2 != dir % 2){
          if(intersects(x, y, tmpX1, tmpY1, tmpX2, tmpY2)) return true;
        }
        else{
          if(tmpDir == dir){
            if (x == tmpX1 && y == tmpY1) return true;
          }
          else{
            if (x == tmpX2 && y == tmpY2) return true;
          }
        }
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
    uint16_t getColour(){
      return colour;
    }
    uint8_t getLength(){
      return length;
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
};

const int fps = 13;

//Receive changes in direction from the clients
//Send to clients if stuff has to be drawn on their screen 
//Changes in direction to snakes on clients screen

class GameManager{
  private:
    uint8_t numSnakes;
    Snake* s[3];
    JoystickListener* js;
    Orientation dirFlag;
  public:    
    GameManager() : js (new JoystickListener(VERT,HOR,SEL,450)){
      tft.fillScreen(0);
      // initialize current direction of movement
      dirFlag = (isServer) ? VERTICAL : HORIZONTAL;
      numSnakes = 2;
      s[0] = new Snake(20,10,DOWN,0xFF00,30);
      s[1] = new Snake(40,10,RIGHT,0x0FF0,30);
    }
    bool waitOnSerial( uint8_t nbytes, long timeout, HardwareSerial &s) {
      unsigned long deadline = millis() + timeout; // wraparound not a problem
      while (s.available()<nbytes && (timeout<0 || millis()<deadline)) {
        delay(1); // be nice, no busy loop
      }
      return s.available()>=nbytes;
    }
    // what's the previous direction that was pressed
    void run(){
      bool handled;
      uint32_t time = millis();
      
      // init serial communication with server or client
      // handshake to ensure communication is happening before main loop      
      typedef enum{listen, start, ack, data} phase;
      phase currPhase;
      Serial.println("I am here");
      if(!isServer) {
        currPhase = start;
        while(currPhase != data) {
          switch(currPhase){
            case start:
              Serial.println("Client request sent");
              Serial2.write('C');
              currPhase = ack;
            case ack:
              Serial.println("Waiting for acknowledgement ... ");
              if(waitOnSerial(1, 1000, Serial2)){
                char A = Serial2.read();
                if(A != 'A'){
                  Serial.println("No acknowledgement recieved");
                  currPhase = start;
                  break;
                }
                Serial.println("Got acknowledgement");
                Serial2.write('A');
                currPhase = data;
              }
              else{
                Serial.println("Timeout");
                currPhase = start;
              }
              break;
          }
        }
      }
      else {
        currPhase = listen;
        while(currPhase != data){
          switch(currPhase){
            case listen:
              if(waitOnSerial(1, 1000, Serial2)){
                char C = Serial2.read();
                if(C == 'C') {
                  currPhase = ack;
                  Serial2.write('A');
                }
              }
              else{
                Serial.println("Timeout");
              }
            case ack:
              Serial.println("Waiting for acknowledgement ... ");
              if(waitOnSerial(1, 1000, Serial2)){
                char A = Serial2.read();
                if(A == 'A'){
                  Serial.println("Got Acknowledgement");
                  currPhase = data;
                }
              }
              else {
                Serial.println("Timeout");
                currPhase = listen;
              }
          }
        }
      }

      Serial.println("Beginning main snake loop");
      
      // send snake info to client
      /*if(isServer){
        Serial.println("Sending snake info");
        for(int i = 0; i < numSnakes; i++){
          Serial2.write(s[i]->getX());
          Serial2.write(s[i]->getY());
          Serial2.write(s[i]->getDirection());
          Serial2.write(s[i]->getColour());
          Serial2.write(s[i]->getLength());
        }
        Serial.println("Sent snake info");
      }
      else {
        Serial.println("Recieving snake info");
        for(int i = 0; i < numSnakes; i++){
          s[i] = new Snake(Serial2.read(), Serial2.read(), (Direction)Serial2.read(), Serial2.read(), Serial2.read());
        }
        Serial.println("Recieved snake info");
      }*/

      while(!(s[0]->isDead() && s[1]->isDead())){
        if(millis() - time > 1000/fps){
          int mySnake = isServer ? 0 : 1;
          char myName = isServer ? '0' : '1';
          time = millis();
          for(int i = 0; i < numSnakes; i++){
            if(!s[i]->isDead()){
              s[i]->update();
              uint8_t tmpX = s[i]->getX();
              uint8_t tmpY = s[i]->getY();
              uint8_t tmpLayer = s[i]->getLayer();
              Direction tmpDir = s[i]->getDirection();
              for(int j = 0; j < numSnakes; j++){
                if(s[j]->willCollide(tmpX, tmpY, tmpDir, tmpLayer)){
                  s[i]->kill();
                  Serial2.write('K');
                  Serial2.write(i ? '1' : '0');
                  Serial2.write(i ? '1' : '0');
                  break;
                }
              }
            }
          }
          if(js->isPushed() && !s[mySnake]->queueFull()){
            int deltaH = js->getHorizontal() - js->getHorizontalBaseline();
            int deltaV = js->getVertical() - js->getVerticalBaseline();
            if(abs(deltaH) > abs(deltaV) && (dirFlag != HORIZONTAL)){
              s[mySnake]->setDirection((deltaH > 0) ? RIGHT : LEFT);
              Serial2.write('D');
              Serial2.write(myName);
              Serial2.write((deltaH > 0) ? 'R' : 'L');
              dirFlag = HORIZONTAL;
              //s[0]->debug("On horizontal");
            }
            else if(abs(deltaV) > abs(deltaH) && dirFlag != VERTICAL){
              s[mySnake]->setDirection((deltaV > 0) ? DOWN : UP);
              Serial2.write('D');
              Serial2.write(myName);
              Serial2.write((deltaV > 0) ? 'D' : 'U');
              dirFlag = VERTICAL;
              //s[0]->debug("On vertical");
            }
          }
          if(js->isDepressed()){
            if(!handled){
              int currLayer = s[mySnake]->getLayer();
              s[mySnake]->setLayer(currLayer ? 0 : 1);
              Serial2.write('L');
              Serial2.write(myName);
              Serial2.write(currLayer ? '0' : '1');
              handled = 1;
            }
          }else{
            handled = 0;
          }
          if(Serial2.available() >= 3){
            char id = Serial2.read();
            char snakeName = Serial2.read();
            char in = Serial2.read();

            Serial.println(id);
            Serial.println(snakeName);
            Serial.println(in);

            switch(snakeName){
              case '0':
                snakeName = 0;
                break;
              case '1':
                snakeName = 1;
                break;
            }
            if(!s[snakeName]->isDead()){
              if(id == 'D'){
                switch(in){
                  case 'L':
                    s[snakeName]->setDirection(LEFT);
                    break;
                  case 'R':
                    s[snakeName]->setDirection(RIGHT);
                    break;
                  case 'U':
                    s[snakeName]->setDirection(UP);
                    break;
                  case 'D':
                    s[snakeName]->setDirection(DOWN);
                    break;
                }
              }
              else if(id == 'L'){
                switch(in){
                  case '0':
                    s[snakeName]->setLayer(0);
                    break;
                  case '1':
                    s[snakeName]->setLayer(1);
                }
              }
              else if(id == 'K'){
                s[snakeName]->kill();
              }
            }
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
  pinMode(11, INPUT);
  isServer = digitalRead(11);
  GameManager* gm = new GameManager();
  gm->run();
  Serial.end();
  return 0;
}
