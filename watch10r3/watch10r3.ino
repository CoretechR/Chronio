/*
 Created in 2016 by Maximilian Kern
 https://hackaday.io/project/12876-chronio
 Low power Arduino based (smart)watch code
 
 License: GNU GPLv3
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Adafruit_GFX.h> //https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SharpMem.h> //https://github.com/adafruit/Adafruit_SHARP_Memory_Display
#include <Wire.h>
#include "ds3231.h"
#include <LowPower.h> // https://github.com/rocketscream/Low-Power
#include <Button.h> // https://github.com/JChristensen/Button

#define SCK 13
#define MOSI 11
#define SS 10
#define LED 9
#define EXTMODE 16

#define MBUT 3
#define UBUT 4
#define DBUT 5
Button buttonMid(MBUT, true, true, 20);
Button buttonUp(UBUT, true, true, 20);
Button buttonDown(DBUT, true, true, 20);

Adafruit_SharpMem display(SCK, MOSI, SS);

#define BLACK 0
#define WHITE 1

#define CONFIG_UNIXTIME 1

struct ts t;
struct ts cfg; //configuration time values
float temperature;
const String gday[7] = {"Sonntag","Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag"}; 
//const String gday[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"}; 
unsigned long standbyTimer;
byte activeTime = 15; //how many sec until entering standby
boolean active = false;
byte currentPage = 0; // start page
byte menuVal = 0;
byte configVal = 0;
boolean flicker = false;
boolean showVoltage = true;
unsigned long stopwatchTimer = 0;
boolean stopwatchActive = false;
unsigned long stopwatchMs = 0;

void setup(void) {
  
  //Serial.begin(9600);
  Wire.begin();
  DS3231_init(DS3231_INTCN);
  //setTime(20, 56, 00, 19, 4, 2016, 4); //hour, min, sec, day, month, year
  DS3231_clear_a2f();
  uint8_t flags[4] = { 1, 1, 1, 1 }; //A2M2, A2M3, A2M4, DY/DT
  DS3231_set_a2(0, 0, 0, flags); //min, hour, day
  DS3231_set_creg(DS3231_INTCN | DS3231_A2IE); //set 1 min alarm

  Wire.beginTransmission(0x6F);
  Wire.write(0x07);
  Wire.write(B01000000); // enable MFP for 1Hz
  Wire.endTransmission();
  
  Wire.beginTransmission(0x6F);
  Wire.write(0x00);
  Wire.write(0x80); //start oscillator
  Wire.endTransmission();
  
  display.begin();
  display.setRotation(0); //Rotate 180° 
  display.clearDisplay();

  pinMode(2, INPUT_PULLUP); // RTC Interrupt Pullup
  pinMode(MBUT, INPUT_PULLUP); // Middle Button Pullup
  pinMode(UBUT, INPUT_PULLUP); // Up Button Pullup
  pinMode(DBUT, INPUT_PULLUP); // Down Button Pullup
  pinMode(EXTMODE, OUTPUT); //VCOM Mode (h=ext l=sw)
  digitalWrite(EXTMODE, HIGH); // switch VCOM to external
  
  attachInterrupt(0, quickWake, FALLING); // RTC Interrupt
  attachInterrupt(1, wake, FALLING); // Middle Button Interrupt

  standbyTimer = millis()+activeTime*1000;

  buttonMid.read();
  buttonUp.read();
  buttonDown.read();
}

void loop(void){
  active = (millis()<=standbyTimer); //check if active
  buttonMid.read(); //read Button
  if(active && (buttonUp.read() || buttonDown.read()))
    standbyTimer = millis()+activeTime*1000; //buttons reset Standby Timer
  if(currentPage == 0 && buttonMid.wasPressed()){ //read Buttons
    currentPage = 9; //switch to menu
    menuVal = 0;
    buttonMid.read(); //make sure wasPressed is not activated again
  }
  
  DS3231_get(&t); //Get time
  temperature = DS3231_get_treg(); //Get temperature
  if (DS3231_triggered_a2()) DS3231_clear_a2f(); //clear alarm 2 flag

  if(currentPage == 0 || !active) drawWatchface();
  else if(currentPage == 9) drawMainMenu();
  else if(currentPage == 1) drawStopwatch();
  else if(currentPage == 2) drawTimer();
  else if(currentPage == 3) drawTimeConfig();
  else if(currentPage == 4) drawGames();
  else if(currentPage == 5) drawSettings();
  else if(currentPage == 6) drawDebug();
  else display.clearDisplay();

  //if(digitalRead(UBUT) == LOW) display.fillRect(0, 0, 30, 8, 0); // visualize button presses
  //if(digitalRead(MBUT) == LOW) display.fillRect(33, 0, 30, 8, 0);
  //if(digitalRead(DBUT) == LOW) display.fillRect(66, 0, 30, 8, 0);
  
  display.refresh();
  
  //digitalWrite(LED, millis()/500%2); // blink LED
  
  if(!active) { //switch to watchface and sleep
    currentPage = 0;
    digitalWrite(EXTMODE, HIGH); // switch VCOM to external
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
  } 
  else if(!flicker) digitalWrite(SCK, HIGH); // this somehow removes flickering
}


void quickWake() { // Wake from RTC
}

void wake() { //Wake from Middle Button
  noInterrupts();
    standbyTimer = millis()+activeTime*1000; // reset Standby Timer
    digitalWrite(EXTMODE, LOW); // switch VCOM to software
  interrupts();
}

void drawWatchface(){
  display.clearDisplay();
  drawDigit( 2, 20, t.hour/10, 0);
  drawDigit(24, 20, t.hour%10, 0);
  drawDigit(52, 20, t.min/10, 0);
  drawDigit(74, 20, t.min%10, 0);
  display.setCursor(4, 62);
  display.setTextColor(BLACK, WHITE);
  display.setTextSize(1);
  display.print(gday[t.wday-1]);
  display.setCursor(4, 72);
  display.print(t.mday); display.print(".");
  display.print(t.mon); display.print(".");
  display.println(t.year);
  display.setCursor(4, 82);
  display.print((int) temperature); display.print(",");
  display.print((int) (temperature*10)%10);
  display.print("C     ");
  if(showVoltage){
    unsigned int batVoltage = readVcc();
    display.print((int) batVoltage/1000); display.print(",");
    display.print((int) (batVoltage/10)%100);
    display.print("V");
  }
  //Second-Blinking
  if(t.sec%2 || !active){
    display.fillRect(46, 30, 4, 4, 0); 
    display.fillRect(46, 46, 4, 4, 0); 
  }
  standbyTimer = millis(); //force Standby to save power
}


void drawMainMenu(){
  
  if(buttonUp.wasPressed()) menuVal--;
  if(buttonDown.wasPressed()) menuVal++;
  
  if(menuVal>6 && menuVal<99) menuVal = 0; //circle navigation
  else if(menuVal>99) menuVal = 6;

  if(buttonMid.wasPressed()){
    currentPage = menuVal;
    buttonMid.read(); //make sure wasPressed is not activated again
  }
  
  display.setCursor(0, 5);
  display.clearDisplay();
  display.setTextSize(1);
  
  display.setTextColor((menuVal == 0), (menuVal != 0));
  display.println(F("Uhr"));

  display.setTextColor((menuVal == 1), (menuVal != 1));
  display.println(F("Stoppuhr"));

  display.setTextColor((menuVal == 2), (menuVal != 2));
  display.println(F("Timer"));
  
  display.setTextColor((menuVal == 3), (menuVal != 3));
  display.println(F("Zeit einstellen"));

  display.setTextColor((menuVal == 4), (menuVal != 4));
  display.println(F("Spiele"));

  display.setTextColor((menuVal == 5), (menuVal != 5));
  display.println(F("Einstellungen"));
  
  display.setTextColor((menuVal == 6), (menuVal != 6));
  display.println(F("Debugging"));

  
}

void drawStopwatch(){
  standbyTimer = millis()+activeTime*1000; //force reset Standby Timer
  
  if(buttonUp.wasPressed()) stopwatchActive = !stopwatchActive;
  if(buttonDown.wasPressed()){
    stopwatchTimer = millis();
    stopwatchMs = 0;
  }  
  
  if(!stopwatchActive) stopwatchTimer = millis() - stopwatchMs;  
  stopwatchMs = millis()-stopwatchTimer;
  
  display.clearDisplay();
  drawDigit( 2, 20, ((stopwatchMs/60000)/10), 0);
  drawDigit(24, 20, ((stopwatchMs/60000)%10), 0);
  drawDigit(52, 20, (stopwatchMs/1000%60)/10, 0);
  drawDigit(74, 20, (stopwatchMs/1000%60)%10, 0);
  display.setCursor(4, 63);
  display.setTextColor(BLACK, WHITE);
  display.setTextSize(2);
  if(stopwatchMs%1000 < 10) display.print(0);
  if(stopwatchMs%1000 < 100) display.print(0);
  display.print(stopwatchMs%1000); display.print("ms");
  display.fillRect(46, 30, 4, 4, 0); 
  display.fillRect(46, 46, 4, 4, 0); 
  
  if(buttonMid.wasPressed()){
    stopwatchActive = false;
    stopwatchMs = 0;
    currentPage = 9; //switch back to menu
    buttonMid.read(); //make sure wasPressed is not activated again
  }
}

void drawTimer(){
  display.clearDisplay();
  display.setTextColor(BLACK, WHITE);
  display.setCursor(0, 0);
  display.println(F("Hier ist\nnoch nichts"));

  if(buttonMid.wasPressed()){
    currentPage = 9; //switch back to menu
    buttonMid.read(); //make sure wasPressed is not activated again
  }
}


void drawTimeConfig(){
  standbyTimer = millis()+activeTime*1000; //force reset Standby Timer
  static boolean firstRunConfig = true;
  if(firstRunConfig){
    cfg = t; //get time for initial values
    configVal = 0;
  }  
  firstRunConfig = false;
  
  if(buttonMid.wasPressed()) configVal++;
  
  display.clearDisplay();
  display.setCursor(0, 25);
  
  if(configVal == 0){
    if(buttonUp.wasPressed() || buttonUp.pressedFor(500)) cfg.hour++;
    if(buttonDown.wasPressed() || buttonDown.pressedFor(500)) cfg.hour--;
    display.setTextColor(WHITE, BLACK);
    if(cfg.hour>23 && cfg.hour<99) cfg.hour = 0; //circle navigation
    else if(cfg.hour>99) cfg.hour = 23;
  }
  else display.setTextColor(BLACK, WHITE);
  if(cfg.hour<10) display.print("0");
  display.print(cfg.hour);

  display.setTextColor(BLACK, WHITE);
  display.print(":");
  if(configVal == 1){
    if(buttonUp.wasPressed() || buttonUp.pressedFor(500)) cfg.min++;
    if(buttonDown.wasPressed() || buttonDown.pressedFor(500)) cfg.min--;
    display.setTextColor(WHITE, BLACK);
    if(cfg.min>59 && cfg.min<99) cfg.min = 0; //circle navigation
    else if(cfg.min>99) cfg.min = 59;
  }
  else display.setTextColor(BLACK, WHITE);
  if(cfg.min<10) display.print("0");
  display.println(cfg.min); 
 
  if(configVal == 2){
    if(buttonUp.wasPressed() || buttonUp.pressedFor(500)) cfg.wday++;
    if(buttonDown.wasPressed() || buttonDown.pressedFor(500)) cfg.wday--;
    display.setTextColor(WHITE, BLACK);
    if(cfg.wday>7 && cfg.wday<15) cfg.wday = 1; //circle navigation
    else if(cfg.wday<1 || cfg.wday>15) cfg.wday = 7;
  }
  else display.setTextColor(BLACK, WHITE);
  display.println(gday[cfg.wday-1]);
   
  if(configVal == 3){
    if(buttonUp.wasPressed() || buttonUp.pressedFor(500)) cfg.mday++;
    if(buttonDown.wasPressed() || buttonDown.pressedFor(500)) cfg.mday--;
    display.setTextColor(WHITE, BLACK);
    if(cfg.mday>31 && cfg.mday<99) cfg.mday = 1; //circle navigation
    else if(cfg.mday<1 || cfg.mday>99) cfg.mday = 31;
  }
  else display.setTextColor(BLACK, WHITE);
  display.print(cfg.mday);
  display.setTextColor(BLACK, WHITE); 
  display.print(".");

  if(configVal == 4){
    if(buttonUp.wasPressed() || buttonUp.pressedFor(500)) cfg.mon++;
    if(buttonDown.wasPressed() || buttonDown.pressedFor(500)) cfg.mon--;
    display.setTextColor(WHITE, BLACK);
    if(cfg.mon>12 && cfg.mon<15) cfg.mon = 1; //circle navigation
    else if(cfg.mon<1 || cfg.mon>15) cfg.mon = 12;
  }
  else display.setTextColor(BLACK, WHITE);
  display.print(cfg.mon);
  display.setTextColor(BLACK, WHITE); 
  display.print(".");
    
  if(configVal == 5){
    if(buttonUp.wasPressed() || buttonUp.pressedFor(500)) cfg.year++;
    if(buttonDown.wasPressed() || buttonDown.pressedFor(500)) cfg.year--;
    display.setTextColor(WHITE, BLACK);
    if(cfg.year>2060) cfg.year = 2000; //circle navigation
    else if(cfg.year<2000) cfg.year = 2060;
  }
  else display.setTextColor(BLACK, WHITE);
  display.print(cfg.year);
  display.setTextColor(BLACK, WHITE); 
  display.println(); display.println();

  display.setTextColor((configVal == 6), (configVal != 6));
  display.println(F("OK"));
  
  if(buttonMid.wasPressed() && configVal == 7){
    setTime(cfg.hour, cfg.min, 0, cfg.mday, cfg.mon, cfg.year, cfg.wday);
    currentPage = 0;
    firstRunConfig = true;
  }
}

void drawGames(){
  standbyTimer = millis()+activeTime*1000; //force reset Standby Timer
  display.clearDisplay();
  static boolean firstRunGames = true;
  static float ySpeed = 0;
  static int py = 0;
  static boolean gameOver = false;
  static byte score = 0;
  static int wallPos[3] = {100, 143, 186};
  static int wallGap[3] = {40, 60, 0};
  static unsigned long lastTime = millis();

  if(firstRunGames){
    ySpeed = py = score = 0;
    wallPos[0] = 100;
    wallPos[1] = 143;
    wallPos[2] = 186;
    lastTime = millis();
    gameOver = false;
  }

  float deltaTime = float(millis()-lastTime);

  ySpeed += deltaTime/80;
  py += ySpeed;
  for(int i = 0; i<3; i++) { // draw walls
    display.fillRect(wallPos[i]-10, 0, 10, wallGap[i], 0);
    display.fillRect(wallPos[i]-10, wallGap[i]+30, 10, 96, 0);
    if(wallPos[i] > 5 && wallPos[i] < 25){ // detect wall
      if(wallGap[i] > py-5 || wallGap[i] < py-25) gameOver = true; //detect gap
    }
    if(wallPos[i]<=0){ // reset wall
      wallPos[i] += 129;
      wallGap[i] = random(5, 70);
      score++;
    }
    wallPos[i] -= deltaTime/80; // move walls
  }
  
  py = constrain(py, 5, 91);
  display.fillCircle(10, py, 5, 0); // draw bird

  display.setTextColor(BLACK, WHITE);
  display.setCursor(40, 2);
  display.print(F("SCORE: "));
  display.println(score);

  if(buttonUp.isPressed()) ySpeed = -3;
  lastTime = millis();

  if(gameOver) {
    firstRunGames = true;
    display.clearDisplay();
    display.setCursor(20, 30);
    display.println(F("GAME OVER"));
    display.setCursor(20, 40);
    display.print(F("SCORE:"));
    display.println(score);
    display.refresh();
    delay(3000);
  }
  else firstRunGames = false;
  
  if(buttonMid.wasPressed()){
    firstRunGames = true;
    currentPage = 9; //switch back to menu
    buttonMid.read(); //make sure wasPressed is not activated again
  }
}

void drawSettings(){
  standbyTimer = millis()+activeTime*1000; //force reset Standby Timer
  static int settingsVal = 0;
  
  if(buttonMid.wasPressed()) settingsVal++;
  
  display.clearDisplay();
  display.setCursor(0, 2);

  if(settingsVal == 0){
    if(buttonUp.wasPressed() || buttonUp.pressedFor(500)) activeTime++;
    if(buttonDown.wasPressed() || buttonDown.pressedFor(500)) activeTime--;
    display.setTextColor(WHITE, BLACK);
    if(activeTime>60) activeTime = 30;
    else if(activeTime<5) activeTime = 5;
  }
  else display.setTextColor(BLACK, WHITE);
  display.print(F("Standby: "));
  display.print(activeTime);
  display.println("s");

  if(settingsVal == 1){
    if(buttonUp.wasPressed() || buttonDown.wasPressed()) flicker = !flicker;
    display.setTextColor(WHITE, BLACK);
  }
  else display.setTextColor(BLACK, WHITE);
  display.print(F("Flicker: "));
  display.println(flicker);

  if(settingsVal == 2){
    if(buttonUp.wasPressed() || buttonDown.wasPressed()) showVoltage = !showVoltage;
    display.setTextColor(WHITE, BLACK);
  }
  else display.setTextColor(BLACK, WHITE);
  display.print(F("V anzeigen: "));
  display.println(showVoltage);

  display.setTextColor((settingsVal == 3), (settingsVal != 3));
  display.println(F("OK"));

  if(buttonMid.wasPressed() && settingsVal == 4){
    settingsVal = 0;
    currentPage = 9; //switch back to menu
    menuVal = 5;
    buttonMid.read(); //make sure wasPressed is not activated again
  }
}

void drawDebug(){
  display.clearDisplay();
  display.setTextColor(BLACK, WHITE);
  display.setCursor(0, 0);
  display.println(F("Hier ist\nnoch nichts"));

  if(buttonMid.wasPressed()){
    currentPage = 9; //switch back to menu
    buttonMid.read(); //make sure wasPressed is not activated again
  }
}

void drawDigit(int posX, int posY, int digit, boolean col){
  switch (digit){
    case 0:
      display.fillRect(posX, posY, 20, 8, col); 
      display.fillRect(posX, posY, 7, 40, col);
      display.fillRect(posX+13, posY, 7, 40, col);
      display.fillRect(posX, posY+32, 20, 8, col);
      break;
    case 1:
      display.fillRect(posX+13, posY, 7, 40, col);
      break;
    case 2:
      display.fillRect(posX, posY, 20, 8, col); 
      display.fillRect(posX, posY+16, 20, 8, col);
      display.fillRect(posX, posY+24, 7, 8, col);
      display.fillRect(posX+13, posY, 7, 16, col);
      display.fillRect(posX, posY+32, 20, 8, col);
      break;
    case 3:
      display.fillRect(posX, posY, 20, 8, col); 
      display.fillRect(posX, posY+16, 20, 8, col);
      display.fillRect(posX+13, posY, 7, 40, col);
      display.fillRect(posX, posY+32, 20, 8, col);
      break;
    case 4:
      display.fillRect(posX, posY, 7, 16, col); 
      display.fillRect(posX, posY+16, 20, 8, col);
      display.fillRect(posX+13, posY, 7, 40, col);
      break;
    case 5:
      display.fillRect(posX, posY, 20, 8, col); 
      display.fillRect(posX, posY+16, 20, 8, col);
      display.fillRect(posX+13, posY+24, 7, 8, col);
      display.fillRect(posX, posY, 7, 16, col);
      display.fillRect(posX, posY+32, 20, 8, col);
      break;
    case 6:
      display.fillRect(posX, posY, 20, 8, col); 
      display.fillRect(posX, posY+16, 20, 8, col);
      display.fillRect(posX+13, posY+24, 7, 8, col);
      display.fillRect(posX, posY, 7, 40, col);
      display.fillRect(posX, posY+32, 20, 8, col);
      break;
    case 7:
      display.fillRect(posX, posY, 20, 8, col); 
      display.fillRect(posX+13, posY, 7, 40, col);
      break;
    case 8:
      display.fillRect(posX, posY, 20, 8, col); 
      display.fillRect(posX, posY, 7, 40, col);
      display.fillRect(posX, posY+16, 20, 8, col);
      display.fillRect(posX+13, posY, 7, 40, col);
      display.fillRect(posX, posY+32, 20, 8, col);
      break;
    case 9:
      display.fillRect(posX, posY, 20, 8, col); 
      display.fillRect(posX, posY, 7, 16, col); 
      display.fillRect(posX, posY+16, 20, 8, col);
      display.fillRect(posX+13, posY, 7, 40, col);
      display.fillRect(posX, posY+32, 20, 8, col);
      break;
  }
}

void setTime(int h, int m, int s, int d, int mn, int y, int wd){
  t.hour = h;
  t.min = m;
  t.sec = s;
  t.mday = d;
  t.mon = mn;
  t.year = y;
  t.wday = wd;
  DS3231_set(t);
}

long readVcc(){
  // http://provideyourown.com/2012/secret-arduino-voltmeter-measure-battery-voltage/
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
