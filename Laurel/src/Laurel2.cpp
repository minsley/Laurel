#include <Arduino.h>
//#include "Adafruit_seesaw.h"
#include "config.h"
#include "oled.h"
#include "soilSensor.h"

void updateDisplay();
void displayDebug();
void printDebug();
bool soilIsTooDry();
void handleSetThreshold(AdafruitIO_Data *data);
void handleRemoteWater(AdafruitIO_Data *data);
void handleRemoteWaterDuration(AdafruitIO_Data *data);
void handleRemoteSoakDuration(AdafruitIO_Data *data);
void handleMillis(char *data, uint16_t len);

bool showDebug = false;

int waterDuration = 30000; // in millis
int soakDuration = 180000; // in millis
int soilDryLevel = 2200;
int lastWatered;

int tempC;
int soilCap; // bone dry 350, pretty dry 870, 1010 max and slightly damp
//int waterLevel; // 1300 submerged

bool motorOn = false;
//bool hasWater = false;
int wateringStartTime;
int wateringDoneTime;
int soakDoneTime;

// Soil meter drawing
int height = 60;
int lineGap = 5;
int lineStart = lineGap;
int lineEnd = 63 - lineGap;

#if defined(ESP32) && !defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)
  // NOTE: 0,2,4,15 are reserved for esp32 wifi
  #define MOTOR    27
  #define LED 13
#elif defined(ARDUINO_NRF52832_FEATHER)
  #define MOTOR    7
  #define LED 17
#endif

//int waterSensorPin = A5;

//Adafruit_seesaw ss;

// set up the 'time/milliseconds' topic
AdafruitIO_Time *msecs = io.time(AIO_TIME_MILLIS);
long currTime;
long realTime;
long timeOffset;

AdafruitIO_Feed *waterThreshold = io.feed("laurelWaterThreshold");
AdafruitIO_Feed *waterDurationFeed = io.feed("laurelWaterDuration");
AdafruitIO_Feed *soakDurationFeed = io.feed("laurelSoakDuration");

AdafruitIO_Feed *moistnessFeed = io.feed("laurelCurrentMoisture");
AdafruitIO_Feed *lastWateredFeed = io.feed("laurelLastWatered");

AdafruitIO_Feed *remoteWater = io.feed("laurelRemoteWaterCommand");
bool aioSaysWaterNow = false;
int nextAioUpdate;
int aioUpdatePeriod = 30000;

void setup() {
  Serial.begin(115200);

  Serial.print("Connecting to AdafruitIO");
  io.connect();

  while(io.mqttStatus() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(io.statusText());
  //
  // attach a message handler for the msecs feed
  msecs->onMessage(handleMillis);

  if (!initOled()) {
    Serial.println("ERROR! oled not found");
    while(1);
  }

  setupSoilSensor();

//  if (!ss.begin(0x36)) {
//    Serial.println("ERROR! seesaw not found");
//    while(1);
//  } else {
//    Serial.print("seesaw started! version: ");
//    Serial.println(ss.getVersion(), HEX);
//  }
  
  pinMode(MOTOR, OUTPUT);
  digitalWrite(MOTOR, LOW);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
  
//  pinMode(waterSensorPin, INPUT);

  waterThreshold->onMessage(handleSetThreshold);
  waterThreshold->save(soilDryLevel);

  waterDurationFeed->onMessage(handleRemoteWaterDuration);
  waterDurationFeed->save(waterDuration);

  soakDurationFeed->onMessage(handleRemoteSoakDuration);
  soakDurationFeed->save(soakDuration);

  remoteWater->onMessage(handleRemoteWater);
  remoteWater->save(aioSaysWaterNow);
  
  Serial.println();
}

void loop() 
{
  io.run();
  
//  waterLevel = analogRead(waterSensorPin);
//  hasWater = waterLevel > 300;

  updateSoil();
  
//  soilCap = ss.touchRead(0);
//  tempC = ss.getTemp();

  currTime = timeOffset + millis();

  // if it's dry, and we're not soaking, then start watering
  if(soilIsTooDry() && soakDoneTime < currTime)
  {
    // this is a pedantic way to set water and soak times,
    // because it's important to not mess it up
    
    wateringStartTime = currTime;
    wateringDoneTime = wateringStartTime + waterDuration;
    soakDoneTime = wateringDoneTime + soakDuration;
  }

  // run the motor if we're in a wateringWindow, or the test button is pressed
  motorOn = wateringDoneTime > currTime || !digitalRead(BUTTON_A) || aioSaysWaterNow;
  
  if(motorOn){
    digitalWrite(LED, HIGH);
    digitalWrite(MOTOR, HIGH);
    lastWatered = currTime;
  } else {
    digitalWrite(LED, LOW);
    digitalWrite(MOTOR, LOW);
  }

  if(showDebug || !digitalRead(BUTTON_C))
  {
    printDebug();
    displayDebug();
  }
  else
  {
    updateDisplay();
  }

  if(currTime > nextAioUpdate) {
    nextAioUpdate = currTime + aioUpdatePeriod;
    moistnessFeed->save(soilRes);
    lastWateredFeed->save(lastWatered);
  }
}

bool soilIsTooDry()
{
  // ensure greater than some sanity check value in case a plug was pulled
  return soilRes < soilDryLevel && soilRes > 100;
}

// message handler for the milliseconds feed
void handleMillis(char *data, uint16_t len) 
{
  // char timeBuf[len+1];
  // for(int i=0; i<len; i++)
  // {
  //   timeBuf[i] = data[i];
  // }
  // timeBuf[len+1] = '\0';
  
  // timeOffset = atol(timeBuf) - millis();
  // Serial.print("Millis Feed: ");
  // Serial.println(data);
}

void handleSetThreshold(AdafruitIO_Data *data) 
{
  int thresh = data->toInt();
  Serial.print("AIO set threshold: ");
  Serial.println(thresh);
  soilDryLevel = thresh;
}

void handleRemoteWater(AdafruitIO_Data *data) 
{
  int waterCmd = data->toBool();
  Serial.print("AIO set water cmd: ");
  Serial.println(waterCmd);
  aioSaysWaterNow = waterCmd;
}

void handleRemoteWaterDuration(AdafruitIO_Data *data) 
{
  int dur = data->toInt();
  Serial.print("AIO set water duration cmd: ");
  Serial.println(dur);
  waterDuration = dur;
}

void handleRemoteSoakDuration(AdafruitIO_Data *data) 
{
  int soak = data->toInt();
  Serial.print("AIO set soak: ");
  Serial.println(soak);
  soakDuration = soak;
}

void updateDisplay()
{
  display.clearDisplay();
  display.setCursor(0,0);
  
  display.setRotation(2);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.println();
  display.print("~ LAUREL ~");
  
  display.setCursor(4,height-12);
  display.println("Soil: ");
  display.writeFastHLine(lineStart, height, lineEnd, SH110X_WHITE);
  display.writeFastHLine(lineStart, height, lineEnd, SH110X_WHITE);
  
  int spot = (soilRes* (lineEnd-lineStart))/4095  + lineStart;
  display.drawCircle(spot, height, 2, SH110X_WHITE);
  
  int trigger = (soilDryLevel* (lineEnd-lineStart))/4095  + lineStart;
  display.writeFastVLine(trigger, height-2, 4, SH110X_WHITE);
  
//  display.println(soilRes);
//  display.println("Soil_trig: ");
//  display.println(soilDryLevel);
  
  display.setCursor(4,height+8);
  if(wateringDoneTime > currTime)
  {
    display.println("Watering!");
  }
  else if(soakDoneTime > currTime)
  { 
    display.print("Soak: ");
    display.print((soakDoneTime - millis())/1000);
    display.println("s");
  }
  
//  Serial.print("Soil_cap: ");
//  Serial.println(soilCap);

//  Serial.print("Temp: ");
//  Serial.println(tempC);

  display.setCursor(4,127-32);
  display.println("Last water");
  int lastWater = currTime - lastWatered;
  
  int d = (lastWater/86400000);
  display.setCursor(8,127-16);
  display.print(d);
  display.println(" days");

  display.setCursor(8,127-8);
  int h = (lastWater/3600000) % 24;
  if(h<10) display.print("0");
  display.print(h);
  display.print(":");
  
  int m = (lastWater/60000) % 60;
  if(m<10) display.print("0");
  display.print(m);
  display.print(":");
  
  int s = (lastWater/1000) % 60;
  if(s<10) display.print("0");
  display.print(s);
  
  delay(10);
  yield();
  display.display();
}

void printDebug()
{
  if(!Serial)
    return;
    
  //    Serial.print("Water:");
//    Serial.print(waterLevel);
//    Serial.print("\t");
    Serial.print("Soil_res:");
    Serial.print(soilRes);
    Serial.print("\t");
    Serial.print("Soil_trig:");
    Serial.print(soilDryLevel);
    Serial.print("\t");
//    Serial.print("Soil_cap:");
//    Serial.print(soilCap);
//    Serial.print("\t");
//    Serial.print("Temp:");
//    Serial.print(tempC);
//    Serial.print("\t");
    Serial.print("Motor:");
    Serial.print(motorOn);
    Serial.print("\t");
    Serial.print("Btn_A:");
    Serial.print(!digitalRead(BUTTON_A));
    Serial.print("\t");
    Serial.print("Btn_B:");
    Serial.print(!digitalRead(BUTTON_B));
    Serial.print("\t");
    Serial.print("Btn_C:");
    Serial.print(!digitalRead(BUTTON_C));
    Serial.println();
}

void displayDebug()
{
  display.clearDisplay();
  display.setCursor(0,0);
  
  display.setRotation(1);
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  display.print("Soil_res: ");
  display.println(soilRes);
  
  display.print("Soil_trig: ");
  display.println(soilDryLevel);
  
//  Serial.print("Soil_cap: ");
//  Serial.println(soilCap);

//  Serial.print("Temp: ");
//  Serial.println(tempC);
  
  display.print("Last water: ");
  display.println(lastWatered);

  display.print("Curr time: ");
  display.println(currTime);

  display.print("Motor: ");
  display.println(motorOn);

  display.print("Buttons: ");
  display.print(!digitalRead(BUTTON_A));
  display.print(" ");
  display.print(!digitalRead(BUTTON_B));
  display.print(" ");
  display.println(!digitalRead(BUTTON_C));
  
  delay(10);
  yield();
  display.display();
}
