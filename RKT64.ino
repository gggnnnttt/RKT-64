#include "RGBmatrixPanel.h"
#include "bit_bmp.h"
#include "fonts.h"
#include <string.h>
#include <stdlib.h>
#include "Arduino.h"
#include "DFRobotDFPlayerMini.h"

//led matrix pins
#define CLK 11
#define OE 9
#define LAT 10
#define A A0
#define B A1
#define C A2
#define D A3
#define E A4

//serial1 = rx tx 18 19 pins
#define FPSerial Serial1

RGBmatrixPanel matrix(A, B, C, D, E, CLK, LAT, OE, false, 64);
DFRobotDFPlayerMini myDFPlayer;

int currentTrack{ 6 };

const int VOLUME = 25;

enum GameState {
  STATE_START_SCREEN,
  STATE_PLAYING,
  STATE_GAME_OVER
};

GameState currentState = STATE_START_SCREEN;

int rocketX = 28;
int rocketY = 54;
int rocketWidth = 8;
int rocketHeight = 10;
int rocketSpeed = 3;

#define MAX_METEORS 8
struct Meteor {
  int x;
  int y;
  int size;
  int speed;
  bool active;
  uint16_t color;
};

Meteor meteors[MAX_METEORS];

int score = 0;
unsigned long lastMeteorSpawn = 0;
unsigned long meteorSpawnInterval = 800;
unsigned long gameStartTime = 0;
unsigned long stateChangeTime = 0; 
int difficulty = 1;
int lastDifficulty = 0;

bool startScreenDrawn = false;
bool gameOverScreenDrawn = false;

#define BUTTON_LEFT_PIN 3  // Move left 

#define BUTTON_RIGHT_PIN 2  // Move right 

volatile bool buttonLeftPressed = false;
volatile bool buttonRightPressed = false;
volatile unsigned long buttonLeftLastInterrupt = 0;
volatile unsigned long buttonRightLastInterrupt = 0;
const unsigned long DEBOUNCE_DELAY = 200;

void setup() {  
  matrix.begin();
  randomSeed(analogRead(A5));

  pinMode(BUTTON_LEFT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_LEFT_PIN), buttonLeftISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_RIGHT_PIN), buttonRightISR, FALLING);

  myDFPlayer.setTimeOut(500);
  myDFPlayer.volume(VOLUME);
  myDFPlayer.outputDevice(DFPLAYER_DEVICE_SD);
  delay(1000);
  myDFPlayer.loop(6);

  for (int i = 0; i < MAX_METEORS; i++) {
    meteors[i].active = false;
  }

  buttonLeftPressed = false;
  buttonRightPressed = false;

  currentState = STATE_START_SCREEN;
  stateChangeTime = millis();

  delay(500); 

}

void buttonLeftISR() {
  unsigned long currentTime = millis();
  if (currentTime - buttonLeftLastInterrupt > DEBOUNCE_DELAY) {
    buttonLeftPressed = true;
    buttonLeftLastInterrupt = currentTime;
  }
}

void buttonRightISR() {
  unsigned long currentTime = millis();
  if (currentTime - buttonRightLastInterrupt > DEBOUNCE_DELAY) {
    buttonRightPressed = true;
    buttonRightLastInterrupt = currentTime;
  }
}

bool checkButtonLeft() {
  if (buttonLeftPressed) {
    buttonLeftPressed = false;
    return true;
  }
  return false;
}

bool checkButtonRight() {
  if (buttonRightPressed) {
    buttonRightPressed = false;
    return true;
  }
  return false;
}

void loop() {
  if (myDFPlayer.available()) {
  }

  switch (currentState) {
    case STATE_START_SCREEN:
      updateStartScreen();
      break;

    case STATE_PLAYING:
      updatePlaying();
      break;

    case STATE_GAME_OVER:
      updateGameOver();
      break;
  }
}

void updateStartScreen() {

  if (!startScreenDrawn) {

    drawStartScreen();
    startScreenDrawn = true;

    //fixes instant start of the game
    buttonLeftPressed = false;   
    buttonRightPressed = false; 
    startScreenDrawn = true;
  }
  if (millis() - stateChangeTime < 500) {
    buttonLeftPressed = false;
    buttonRightPressed = false;
    return; 
  }

  if (checkButtonLeft() || checkButtonRight()) {
    startScreenDrawn = false;
    startGame();
  }


  delay(10);
}

void updatePlaying() {
  if (digitalRead(BUTTON_LEFT_PIN) == LOW) {
    rocketX -= rocketSpeed;
    if (rocketX < 0) rocketX = 0;
  }

  if (digitalRead(BUTTON_RIGHT_PIN) == LOW) {
    rocketX += rocketSpeed;
    if (rocketX + rocketWidth > 64) rocketX = 64 - rocketWidth;
  }

  if (millis() - lastMeteorSpawn > meteorSpawnInterval) {
    spawnMeteor();
    lastMeteorSpawn = millis();
  }

  updateDifficulty();

  if (difficulty != lastDifficulty) {
    lastDifficulty = difficulty;
  }

  updateMeteors();
  checkCollisions();

  matrix.fillScreen(matrix.Color333(0, 0, 0));
  drawRocket();
  drawMeteors();
  drawScore();

  delay(50);
}

void updateGameOver() {
  if (!gameOverScreenDrawn) {
    drawGameOverScreen();
    gameOverScreenDrawn = true;
  }
  if (checkButtonLeft()) {
    gameOverScreenDrawn = false;
    returnToStart();
  } else if (checkButtonRight()) {
    gameOverScreenDrawn = false;
    startGame();
  }

  delay(10);
}

void drawStyledText(const char* text, int startX, int startY, uint16_t color) {
  matrix.setCursor(startX, startY);
  matrix.setTextColor(color);
  matrix.print(text);
}

void drawStartScreen() {
  matrix.fillScreen(matrix.Color333(0, 0, 1));  

  // bright stars
  matrix.drawPixel(5, 5, matrix.Color333(7, 7, 7));
  matrix.drawPixel(58, 8, matrix.Color333(7, 7, 7));
  matrix.drawPixel(12, 3, matrix.Color333(6, 6, 7));
  matrix.drawPixel(45, 6, matrix.Color333(7, 7, 6));
  matrix.drawPixel(30, 10, matrix.Color333(7, 7, 7));

  // medium sized stars
  matrix.drawPixel(10, 15, matrix.Color333(5, 5, 5));
  matrix.drawPixel(55, 18, matrix.Color333(5, 5, 5));
  matrix.drawPixel(20, 12, matrix.Color333(4, 4, 5));
  matrix.drawPixel(40, 14, matrix.Color333(5, 5, 4));
  matrix.drawPixel(8, 25, matrix.Color333(5, 5, 5));
  matrix.drawPixel(60, 28, matrix.Color333(4, 4, 4));

  // dim stars
  matrix.drawPixel(15, 8, matrix.Color333(3, 3, 3));
  matrix.drawPixel(35, 4, matrix.Color333(3, 3, 3));
  matrix.drawPixel(50, 22, matrix.Color333(2, 2, 3));
  matrix.drawPixel(25, 20, matrix.Color333(3, 3, 2));
  matrix.drawPixel(3, 18, matrix.Color333(2, 2, 2));

  // planet 
  matrix.fillCircle(52, 52, 8, matrix.Color333(4, 2, 1));

  matrix.fillCircle(50, 50, 3, matrix.Color333(5, 3, 2));
  matrix.drawPixel(49, 49, matrix.Color333(6, 4, 2));

  matrix.drawPixel(56, 54, matrix.Color333(3, 1, 0));
  matrix.drawPixel(57, 53, matrix.Color333(2, 1, 0));
  matrix.drawPixel(55, 56, matrix.Color333(3, 1, 0));

  // rocket
  int rocketStartX = 8;
  int rocketStartY = 52;
  matrix.fillRect(rocketStartX + 2, rocketStartY, 4, 8, matrix.Color333(5, 5, 5));
  matrix.drawPixel(rocketStartX + 3, rocketStartY - 1, matrix.Color333(7, 0, 0));
  matrix.drawPixel(rocketStartX + 4, rocketStartY - 1, matrix.Color333(7, 0, 0));
  matrix.drawPixel(rocketStartX + 2, rocketStartY, matrix.Color333(6, 0, 0));
  matrix.drawPixel(rocketStartX + 5, rocketStartY, matrix.Color333(6, 0, 0));
  matrix.drawPixel(rocketStartX, rocketStartY + 5, matrix.Color333(0, 3, 6));
  matrix.drawPixel(rocketStartX + 1, rocketStartY + 5, matrix.Color333(0, 4, 7));
  matrix.drawPixel(rocketStartX + 6, rocketStartY + 5, matrix.Color333(0, 4, 7));
  matrix.drawPixel(rocketStartX + 7, rocketStartY + 5, matrix.Color333(0, 3, 6));
  matrix.drawPixel(rocketStartX, rocketStartY + 6, matrix.Color333(0, 2, 5));
  matrix.drawPixel(rocketStartX + 7, rocketStartY + 6, matrix.Color333(0, 2, 5));
  matrix.drawPixel(rocketStartX + 2, rocketStartY + 8, matrix.Color333(7, 3, 0));
  matrix.drawPixel(rocketStartX + 3, rocketStartY + 8, matrix.Color333(7, 5, 0));
  matrix.drawPixel(rocketStartX + 4, rocketStartY + 8, matrix.Color333(7, 5, 0));
  matrix.drawPixel(rocketStartX + 5, rocketStartY + 8, matrix.Color333(7, 3, 0));
  matrix.drawPixel(rocketStartX + 3, rocketStartY + 9, matrix.Color333(7, 4, 0));
  matrix.drawPixel(rocketStartX + 4, rocketStartY + 9, matrix.Color333(7, 4, 0));
  matrix.drawPixel(rocketStartX + 3, rocketStartY + 3, matrix.Color333(0, 6, 7));
  matrix.drawPixel(rocketStartX + 4, rocketStartY + 3, matrix.Color333(0, 6, 7));
  matrix.setFont(NULL);

  // RKT-64 title
  matrix.setTextSize(1);
  uint16_t titleColor = matrix.Color333(7, 4, 0);
  drawStyledText("RKT-64", 12, 18, titleColor);

  // press Button text
  matrix.setTextSize(1);
  matrix.setCursor(17, 32);
  matrix.setTextColor(matrix.Color333(0, 7, 7)); 
  matrix.print("PRESS");
  matrix.setCursor(11, 40);
  matrix.setTextColor(matrix.Color333(7, 7, 7));
  matrix.print("BUTTON!");
}

void drawGameOverScreen() {
  matrix.fillScreen(matrix.Color333(0, 0, 0));  
  matrix.setFont(NULL);
  matrix.setTextSize(1);

  // game over text
  matrix.setCursor(20, 8);
  matrix.setTextColor(matrix.Color333(7, 0, 0));  
  matrix.print("GAME");
  matrix.setCursor(20, 16);
  matrix.setTextColor(matrix.Color333(7, 0, 0));
  matrix.print("OVER");

  matrix.setCursor(14, 28);
  matrix.setTextColor(matrix.Color333(7, 7, 0));  
  matrix.print("Score:");

  // score
  char scoreStr[6];
  itoa(score, scoreStr, 10);
  int numDigits = strlen(scoreStr);
  int scorePixels = numDigits * 6;
  int scoreCursor = (64 - scorePixels) / 2;

  matrix.setCursor(scoreCursor, 36);
  matrix.setTextColor(matrix.Color333(0, 7, 0));  // green
  matrix.print(scoreStr);

  // instruction text
  matrix.setCursor(14, 48);
  matrix.setTextColor(matrix.Color333(0, 5, 7));  //  blue
  matrix.print("L:PLAY");

  matrix.setCursor(14, 56);
  matrix.setTextColor(matrix.Color333(7, 4, 0));  // orange
  matrix.print("R:MENU");
}

void startGame() {
  //setting the variables to default again
  score = 0;
  rocketX = 28;
  gameStartTime = millis();
  difficulty = 1;
  lastDifficulty = 1;
  meteorSpawnInterval = 800;
  lastMeteorSpawn = millis();

  //clear meteors
  for (int i = 0; i < MAX_METEORS; i++) {
    meteors[i].active = false;
  }

  // reset button flags
  buttonLeftPressed = false;
  buttonRightPressed = false;

  //clear screen
  matrix.fillScreen(matrix.Color333(0, 0, 0));

  // update game state
  currentState = STATE_PLAYING;

  stateChangeTime = millis();

}

void returnToStart() {
  // reset evreything again
  score = 0;
  rocketX = 28;
  difficulty = 1;
  lastDifficulty = 1;
  meteorSpawnInterval = 800;

  // clearing all meteors
  for (int i = 0; i < MAX_METEORS; i++) {
    meteors[i].active = false;
  }
  // reset button states to avoid instant swithcing of game state 
  buttonLeftPressed = false;
  buttonRightPressed = false;

  // reset screen
  startScreenDrawn = false;

  stateChangeTime = millis();

  // return to start screen
  currentState = STATE_START_SCREEN;

  delay(300);
}

void drawRocket() {
  matrix.fillRect(rocketX + 2, rocketY + 3, 4, 6, matrix.Color333(5, 5, 5));
  matrix.drawPixel(rocketX + 3, rocketY + 2, matrix.Color333(7, 0, 0));
  matrix.drawPixel(rocketX + 4, rocketY + 2, matrix.Color333(7, 0, 0));
  matrix.drawLine(rocketX + 2, rocketY + 3, rocketX + 5, rocketY + 3, matrix.Color333(7, 0, 0));
  matrix.drawPixel(rocketX, rocketY + 6, matrix.Color333(0, 4, 7));
  matrix.drawPixel(rocketX + 1, rocketY + 6, matrix.Color333(0, 4, 7));
  matrix.drawPixel(rocketX + 6, rocketY + 6, matrix.Color333(0, 4, 7));
  matrix.drawPixel(rocketX + 7, rocketY + 6, matrix.Color333(0, 4, 7));
  int flameOffset = (millis() / 100) % 2;
  matrix.drawPixel(rocketX + 2, rocketY + 9, matrix.Color333(7, 3, 0));
  matrix.drawPixel(rocketX + 3, rocketY + 9 + flameOffset, matrix.Color333(7, 4, 0));
  matrix.drawPixel(rocketX + 4, rocketY + 9 + flameOffset, matrix.Color333(7, 4, 0));
  matrix.drawPixel(rocketX + 5, rocketY + 9, matrix.Color333(7, 3, 0));
}

void spawnMeteor() {
  for (int i = 0; i < MAX_METEORS; i++) {
    if (!meteors[i].active) {
      meteors[i].x = random(0, 60);
      meteors[i].y = -5;
      meteors[i].size = random(3, 6);
      meteors[i].speed = random(1, 2 + difficulty);
      meteors[i].active = true;
      int colorType = random(0, 3);
      if (colorType == 0) {
        meteors[i].color = matrix.Color333(5, 3, 1);
      } else if (colorType == 1) {
        meteors[i].color = matrix.Color333(4, 4, 4);
      } else {
        meteors[i].color = matrix.Color333(6, 1, 0);
      }
      break;
    }
  }
}

void updateMeteors() {
  for (int i = 0; i < MAX_METEORS; i++) {
    if (meteors[i].active) {
      meteors[i].y += meteors[i].speed;
      if (meteors[i].y > 64) {
        meteors[i].active = false;
        score++;
      }
    }
  }
}

void drawMeteors() {
  for (int i = 0; i < MAX_METEORS; i++) {
    if (meteors[i].active) {
      matrix.fillCircle(meteors[i].x, meteors[i].y, meteors[i].size / 2, meteors[i].color);
      matrix.drawPixel(meteors[i].x - 1, meteors[i].y - 1, matrix.Color333(7, 7, 7));
    }
  }
}

void checkCollisions() {
  for (int i = 0; i < MAX_METEORS; i++) {
    if (meteors[i].active) {
      int meteorLeft = meteors[i].x - meteors[i].size / 2;
      int meteorRight = meteors[i].x + meteors[i].size / 2;
      int meteorTop = meteors[i].y - meteors[i].size / 2;
      int meteorBottom = meteors[i].y + meteors[i].size / 2;
      int rocketLeft = rocketX;
      int rocketRight = rocketX + rocketWidth;
      int rocketTop = rocketY;
      int rocketBottom = rocketY + rocketHeight;

      if (meteorRight > rocketLeft && meteorLeft < rocketRight && meteorBottom > rocketTop && meteorTop < rocketBottom) {
        // game over
        buttonLeftPressed = false;
        buttonRightPressed = false;

        currentState = STATE_GAME_OVER;
      }
    }
  }
}

void drawScore() {
  matrix.setFont(NULL);
  matrix.setTextSize(1);
  matrix.setCursor(2, 1);
  matrix.setTextColor(matrix.Color333(7, 7, 0));
  matrix.print(score);
}

void updateDifficulty() {
  unsigned long elapsed = millis() - gameStartTime;
  difficulty = 1 + (elapsed / 15000);
  if (difficulty > 2) {
    difficulty = 2;
  }
  meteorSpawnInterval = max(300, 800 - (difficulty * 50));
}