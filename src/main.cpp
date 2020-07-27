#include <Arduino.h>
#include <menu.h>
#include <menuIO/serialOut.h>
#include <menuIO/chainStream.h>
#include <menuIO/serialIn.h>
#include <menuIO/u8g2Out.h>
#include <menuIO/softkeyIn.h> 
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "DHT.h"
#include <BH1750.h>
#define MAX_DEPTH 1
#define fontName u8g2_font_7x13_mf
#define fontX 7
#define fontY 16
#define offsetX 0
#define offsetY 3
#define U8_Width 128
#define U8_Height 64
#define DHTPIN 32
#define DHTTYPE DHT11

int test = 20;
int PinLED=25;
int PinCapacitiveSoil=15;
int PinTasterSelect=16;
int PinTasterUp=17;
int PinTasterDown=18;
int PinTasterEsc=19;
int dutyCycleLED = 0;
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;
long last_change;
int duration=5000;
float h;
float t;
int water;
int hum;
float light=0.0;
//int dutyCycleFan = 0;
//const int fanPin= ;
//const int stepwidth=20;
//const int minSpeed=75;
//int fanSpeed=minSpeed;

BH1750 lightMeter(0x5C);

DHT dht(DHTPIN, DHTTYPE);

const colorDef<uint8_t> colors[6] MEMMODE = {
    {{0, 0}, {0, 1, 1}},  // bgColor
    {{1, 1}, {1, 0, 0}},  // fgColor
    {{1, 1}, {1, 0, 0}},  // valColor
    {{1, 1}, {1, 0, 0}},  // unitColor
    {{0, 1}, {0, 0, 1}},  // cursorColor
    {{1, 1}, {1, 0, 0}},  // titleColor
};

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

result updateGrowLED();
result warnungen(float licht);
result doAlert(eventMask enterEvent, prompt& item);

MENU(mainMenu, "Einstellungen", Menu::doNothing, Menu::noEvent, Menu::wrapStyle,
     FIELD(dutyCycleLED, "LED", "%", 0, 255, 25, 10, updateGrowLED, eventMask::exitEvent, noStyle),
     //FIELD(dutyCycleFan, "Ventilator", "%", 0, 255, 25, 10, updateFan, eventMask::exitEvent, noStyle),
     //OP("Warnungen", warnungen, enterEvent),
     OP("Messwerte", doAlert, enterEvent),
     EXIT("<Back"));

MENU_OUTPUTS(out, MAX_DEPTH, U8G2_OUT(u8g2, colors, fontX, fontY, offsetX, offsetY,
                      {0, 0, U8_Width / fontX, U8_Height / fontY}),SERIAL_OUT(Serial), NONE //must have 2 items at least
);

Menu::keyMap joystickBtn_map[] = {
    {-PinTasterSelect, defaultNavCodes[Menu::enterCmd].ch},
    {-PinTasterEsc, defaultNavCodes[Menu::escCmd].ch},
    {-PinTasterUp, defaultNavCodes[Menu::upCmd].ch},
    {-PinTasterDown, defaultNavCodes[Menu::downCmd].ch},
};
Menu::softKeyIn<4> joystickBtns(joystickBtn_map);

serialIn serial(Serial);
// MENU_INPUTS(in, &serial);
Menu::menuIn* inputsList[] = {&joystickBtns, &serial};
Menu::chainStream<2> in(inputsList);  // 2 is the number of inputs

NAVROOT(nav, mainMenu, MAX_DEPTH, in, out);

result alert(menuOut& o, idleEvent e) {
  switch (e) {
    case Menu::idleStart:  
      break;
    case Menu::idling:
      t = dht.readTemperature();
      //water=map(analogRead(PinCapacitiveSoil), 0, 4095, 100, 0);
      water=map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0);
      if (water<0) { water =0; }
      if (water>100) { water =100; }
      h = dht.readHumidity();
      hum=((int)(h*10)) / 10.0;
      o.setCursor(0, 0);
      o.print("Temperatur ");
      o.print(t,1);
      o.setCursor(16,0);
      o.print("C");
      o.setCursor(0,1);
      o.print("Wasserstand ");
      o.print(water);
      o.setCursor(15,1);
      o.print("%");
      o.setCursor(0,2);
      o.print("Feuchtigkeit ");
      o.print(hum);
      o.setCursor(16,2);
      o.print("%");
      o.setCursor(0,3);
      o.print("Helligkeit ");
      o.print(light,0);
      o.setCursor(15,3);
      o.print("lx");
      //void warnungen(light);
      break;
    case Menu::idleEnd:
      break;
    default:
      break;
  }

  return proceed;
}

result updateGrowLED()
{
  ledcWrite(ledChannel, dutyCycleLED);
  return proceed;
}

/*
result updateFan()
{
  
  analogWrite(fanPin, fanSpeed)
  return proceed;
}
*/

result doAlert(eventMask e, prompt& item) 
{
  nav.idleOn(alert);
  return proceed;
}

void warnings(menuOut& o){
    if (dht.readTemperature()<15){
      o.setCursor(0,2);
      o.println("zu kalt");
    }
    if (dht.readTemperature()>30){
      o.setCursor(0,2);
      o.println("zu warm");
    }
    if (map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0) < 10){
      o.setCursor(0,1);
      o.println("zu wenig Wasser");
    }
    if (map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0) > 95){
      o.setCursor(0,1);
      o.println("zu viel Wasser");
    }
    if (dht.readHumidity()<15){
      o.setCursor(0,3);
      o.println("Luft ist zu trocken");
    }
    if (dht.readHumidity()>70){
      o.setCursor(0,3);
      o.println("zu feucht");
    }
    if(lightMeter.readLightLevel()>2000){
      o.setCursor(0,0);
      o.println("zu viel Licht");
    } 
    if(lightMeter.readLightLevel()<50){
      o.setCursor(0,0);
      o.println("zu wenig Licht");
    }
}

result idleMenu(menuOut& o,idleEvent e) {
  o.clear();
  switch(e) {
    case idleStart:o.println("suspending menu!");break;
    case idling:
    o.clear();
    warnings(o);
    break; 
    case idleEnd:o.println("resuming menu.");break;
  }
  return proceed;
}



void updateDisplay() {
  // change checking leaves more time for other tasks
  u8g2.firstPage();
  do {
    nav.doOutput();
  } while (u8g2.nextPage());
}

result warnungen(float licht ) {
  //void warnungen(float temperatur, int wasserstand, int feuchtigkeit, float licht){
  u8g2.firstPage();
  if(licht>5000){
    //u8g2.setAutoPageClear;
    u8g2.drawStr(0,3,"Warnung");
    //u8g2.print("Warnung");
  }
}

void setup()
{
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(PinLED, ledChannel);

  Wire.begin(21, 22);
  u8g2.begin();
  u8g2.setFont(fontName);
  dht.begin();

  pinMode(PinTasterSelect, INPUT_PULLUP);
  pinMode(PinTasterUp, INPUT_PULLUP);
  pinMode(PinTasterDown, INPUT_PULLUP);
  pinMode(PinTasterEsc, INPUT_PULLUP);

  //TCCR1B = TCCR1B & 0b11111000 | 0x01;

  nav.idleTask=idleMenu;

  Serial.begin(115200);
  while (!Serial);

  /*
  analogWrite(fanPin, 255);       
  delay(1000);
  analogWrite(fanPin, fanSpeed);
  */

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  }
  else {
    Serial.println(F("Error initialising BH1750"));
  }
}

void loop()
{
  light = lightMeter.readLightLevel();
  delay(150);

  if( abs(last_change - millis()) > duration)
  {
    last_change=millis();
    nav.idleChanged=true;
    nav.refresh();
  }

  nav.doInput();

  if (nav.changed(0)){
    updateDisplay();
  }
  nav.doOutput();

}
