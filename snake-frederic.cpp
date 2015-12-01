#include <Arduino.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library
#include <SPI.h>
#include <SD.h>


enum Direction {UP,RIGHT,DOWN,LEFT};
enum Event {MOVEDUP,MOVEDRIGHT,MOVEDDOWN,MOVEDLEFT,MOVEDINTO,MOVEDOUTOF};
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

const int maxSegs = 128;

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
    snakeSeg lineSegments[maxSegs];
    uint8_t head;
    uint8_t tail;

    // sorted arrays containing x or y and index of lines in the snake
    uint8_t vertLines[maxSegs][2];
    uint8_t horizLines[maxSegs][2];

    int pendingLength;
    int length;
    int layer;

    // current state of the life of the snake    
    boolean dead;

    Snake(uint8_t startX, uint8_t startY,Direction startDir, uint16_t col, int startingLength) :
      layer(0),
      length(startingLength),
      head(0),
      tail(0),
      dead(false)
  {
  }
    void update(){
      // TODO collisions right here

      // updates head first and then tail
      // and checks to make sure snake is on screen.

      // head movement
      lineSegment[head].x2 -= (lineSegment[head].dir % 2 == 1) ? dir - 2: 0;
      if(lineSegment[head].x2 == 255) { lineSegment[head].x2 = 127 } ;
      if(lineSegment[head].x2 == 128) { lineSegment[head].x2 = 0 };

      lineSegment[head].y2 += (lineSegment[head].dir % 2 == 0) ? dir - 1: 0;
      if(lineSegment[head].y2 == 255) { lineSegment[head].y2 = 159 } ;
      if(lineSegment[head].y2 == 160) { lineSegment[head].y2 = 0 };

      // check to see if the tail is finished with going through this segment 
      // and empty segment and update tail if it is
      if(lineSegment[tail].x1 == lineSegment[tail].x2 && linSegment[tail].y1 == lineSegment[tail].y2){
        lineSegment[tail] = NULL;
        tail = (tail < maxSegs) ? tail + 1 : 0;
      }
      // tail movement
      if(pendingLength > 0){
        if(lineSegment[tail].dir == UP) { lineSegment[tail].y1 += pendingLength };
        if(lineSegment[tail].dir == DOWN) { lineSegment[tail].y1 -= pendingLength };
        if(lineSegment[tail].dir == LEFT) { lineSegment[tail].x1 += pendingLength };
        if(lineSegment[tail].dir == RIGHT) { lineSegmen[tail].x1 -= pendingLength };
        pendingLength = 0;
      }
      else{
        lineSegment[tail].x1 -= (lineSegment[tail].dir % 2 == 1) ? dir - 2: 0;
        lineSegment[tail].y2 += (lineSegment[tail].dir % 2 == 0) ? dir - 1: 0;
      }

      // draw head and tail pixels
      tft.drawPixel(lineSegment[head].x2, lineSegment[head].y2, colour);
      tft.drawPixel(lineSegmnet[tail].x1, lineSegment[tail].y2, 0x0);
    }
    void setDirection(Direction newDir){

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
    void parseClientPacket(HardwareSerial &serial, Snake* s, int snakeNum){
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
    }
    boolean getLayerTile(uint8_t layer[16][160], uint8_t x, uint8_t y){
      return layer[x/8][y]&(1>>(x%8));
    }
    void setLayerTile(uint8_t layer[16][160], uint8_t x, uint8_t y , boolean value){
      if(getLayerTile(layer,x,y) != value){
        layer[x/8][y] = layer[x/8][y] ^ (1>>(x%8));
      }
    }
  public:    
    GameManager() : js (new JoystickListener(VERT,HOR,SEL,450)){
      tft.fillScreen(0);
      s[0] = new Snake(20,20,DOWN,0xFF00,20);
      s[1] = new Snake(20,100,RIGHT,0x0FF0,20);
      s[2] = new Snake(100,108,UP,0x00FF,20);
    }
    void run(){
      uint32_t time = millis();
      while(!(s[0]->isDead() && s[1]->isDead() && s[2]->isDead())){
        if(millis() - time > 1000/fps){
          time = millis();
          for(int i = 0; i < 3; i++){
            if(!s[i]->isDead()){
              s[i]->update();
              if(getLayerTile((s[i]->getLayer())? layer1 : layer2, s[i]->getX(), s[i]->getY())){
                Serial.print("Snake Died");
                //s[i]->kill();
              }
              else{
                setLayerTile((s[i]->getLayer())? layer1 : layer2, s[i]->getX(), s[i]->getY(), true);  
              }
            }
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
}
