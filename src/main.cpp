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
int duration=200;
float h;
float t;
int hum;

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

result doAlert(eventMask enterEvent, prompt& item);

MENU(mainMenu, "Einstellungen", Menu::doNothing, Menu::noEvent, Menu::wrapStyle,
     FIELD(dutyCycleLED, "LED", "%", 0, 255, 25, 10, updateGrowLED, eventMask::exitEvent, noStyle),
     OP("Messwerte", doAlert, enterEvent),
     //FIELD(dutty,"Duty","%",0,100,1,0,updateDutty),
     //OP("temp",temp)
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
      h = dht.readHumidity();
      hum=((int)(h*10)) / 10.0;
      t = dht.readTemperature();
      o.setCursor(0, 0);
      o.print("Temperatur ");
      o.print(t,1);
      o.setCursor(16,0);
      o.print("C");
      o.setCursor(0,1);
      o.print("Wasserstand ");
      o.print(map(analogRead(PinCapacitiveSoil), 0, 4095, 100, 0));
      o.setCursor(15,1);
      o.print("%");
      o.setCursor(0,2);
      o.print("Feuchtigkeit ");
      o.print(hum);
      o.setCursor(16,2);
      o.print("%");
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

result doAlert(eventMask e, prompt& item) //showTemperature()
{
  nav.idleOn(alert);
  return proceed;
}

void updateDisplay() {
  // change checking leaves more time for other tasks
  u8g2.firstPage();
  do {
    nav.doOutput();
  } while (u8g2.nextPage());
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

  Serial.begin(115200);
  while (!Serial)
    ;
  Serial.println("Planto Menu");
  Serial.println("Use keys + - * /");
  Serial.println("to control the menu navigation");
}

void loop()
{
  if(last_change + duration <  millis())
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
