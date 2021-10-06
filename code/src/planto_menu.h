/*Ausgelagerte Klasse, in der alle nötigen Definitionen und Methoden für das Menu enthalten sind
*/

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>      //Basisklasse für viele Sensoren
#include <BH1750.h>              //Lichtsensor
#include <Adafruit_BME280.h>  //neuer Luftfeuchtigkeits und Temperatursensor
#include <menu.h>                //Menu
#include <menuIO/chainStream.h>  //Verbindung von mehreren Input-Streams zu einem
#include <menuIO/serialIn.h>  //Verwendung von standardmäßigem seriellen Input
#include <menuIO/serialOut.h>  //Verwendung von standardmäßigem seriellen Output
#include <menuIO/softkeyIn.h>  //generische Schaltflächen
#include <menuIO/u8g2Out.h>    //Nutzung von u8g2 Display

#include <functional>
#include "fan.h"  //Klasse für den Ventilator

// Parameter für das Display und das Menü
#define MAX_DEPTH 1
#define fontName u8g2_font_7x13_mf
#define fontX 7
#define fontY 16
#define offsetX 0
#define offsetY 3
#define U8_Width 128
#define U8_Height 64
#define display_SDA 21
#define display_SCL 22


//Pin-Definitionen für Temperatur- und luftfeuchtigkeitssensor
#define BME_SDA 21
#define BME_SCL 22
Adafruit_BME280 bme; 

//Wasserstandssensor
int PinCapacitiveSoil = 35;  // Pin-Belegung Feuchtigkeitssensor


//Display und Menü Variablen
int duration = 500;        // Dauer in ms für Displayupdate
int display_timeout =
    20000;  // Display wechselt in super Menu Modus nach 30 min=18000000ms

menuNode *last_selected_prompt = nullptr;
long last_light = 0;
long last_active_display = 0;  // Zeitstempel der letzen Benutzung

bool flag_idling = false; 

BH1750 lightMeter(0x23);  // I2C Adresse für den Lichtsensor, häufig 0x23, sonst oft 0x5C

// Die Klasse Fan zum Ansprechen des Ventilators wurde ausgelagert in fan.cpp
planto::Fan fan;

// Messwerte
float h;    // abgefragter Luftfeuchtigkeitswert --> umbennen
float t;    // abgefragter Temperaturwert --> umbennen
int water;  // abgefragter Wasserstand -->umbennen
int hum;    // umgewandelter Feuchtigkeitswert im Bereich 0-100 -->umbennen
float light = 0.0;  // abgefragter Lichtwert -->umbennen

// Buttons
int PinTasterSelect = 5;  // Schalter zum Bestätigen
int PinTasterUp = 17;     // Taster zum Auswählen nach oben
int PinTasterDown = 19;   // Taster zum Auswählen nach unten
int PinTasterEsc = 18;    // Taster zum zurück gehen

result idleIPAdress(menuOut &o, idleEvent e);


// Klasse
namespace planto {

class MenuService {
 public:
  void SetGrowLEDCallback(std::function<result()> callback) {
    grow_led_callback_ = callback;
  }
  void SetFanCallback(std::function<result()> callback) {
    fan_callback_ = callback;
  }
  void SetDoAlertCallback(std::function<result(eventMask, prompt &)> callback) {
    do_alert_callback_ = callback;
  }
  void SetWarningsCallback(std::function<void(menuOut &)> callback) {
    warnings_callback_ = callback;
  }
  void SetIPAdresseCallback(std::function<void(menuOut &)> callback) {
    ipadress_callback_ = callback; 
  }
  void SetDoAlertIPAdress(std::function<result(eventMask, prompt &)> callback){
    do_alert_ipadress_callback_ = callback; 
  }

  std::function<result()> grow_led_callback_;
  std::function<result()> fan_callback_;
  std::function<result(eventMask, prompt &)> do_alert_callback_;
  std::function<void(menuOut &)> warnings_callback_;
  std::function<void(menuOut &)> ipadress_callback_; 
  std::function<result(eventMask, prompt &)> do_alert_ipadress_callback_; 
};
MenuService menuService;

result updateGrowLEDLink() { return menuService.grow_led_callback_(); }
// Funktionsdeklarieung, damit im Menü direkt darauf zugegriffen werden kann
// --> Hierarchie/Struktur von Programmen
result updateFanLink() { return menuService.fan_callback_(); }
result doAlertLink(eventMask enterEvent, prompt &item) {
  return menuService.do_alert_callback_(enterEvent, item);
}
result doAlertIPAdressLink(eventMask enterEvent, prompt &item) {
  return menuService.do_alert_ipadress_callback_(enterEvent, item); 
}

}  // namespace planto

// Display
const colorDef<uint8_t> colors[6] MEMMODE = {
    {{0, 0}, {0, 1, 1}},  // bgColor
    {{1, 1}, {1, 0, 0}},  // fgColor
    {{1, 1}, {1, 0, 0}},  // valColor
    {{1, 1}, {1, 0, 0}},  // unitColor
    {{0, 1}, {0, 0, 1}},  // cursorColor
    {{1, 1}, {1, 0, 0}},  // titleColor
};

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

/*
Code für das Menü (extern runter geladene Lib)
kurz Unterschiede von Field und Op erläutern
(https://github.com/neu-rah/ArduinoMenu/wiki/Menu-definition)
*/
int dutyCycleLED = 0;
// FIELD(fan.dutyCycleFan_, "Ventilator", " ", 0, 255, 25, 10, updateFan,
//            eventMask::exitEvent, noStyle);
int dummy = 2;

int drehzahl = 0;


//eigentliche Menu-Einstellungen

MENU(mainMenu, "Einstellungen", Menu::doNothing, Menu::noEvent, Menu::wrapStyle,
     FIELD(dutyCycleLED, "LED", "%", 0, 255, 25, 10, planto::updateGrowLEDLink,
           eventMask::exitEvent, noStyle),
     FIELD(fan.dutyCycleFan_, "Ventilator", "%", 0, 255, 25, 10,
           planto::updateFanLink, eventMask::exitEvent, noStyle),
     //SUBMENU(dirFanMenu), 
     OP("Messwerte", planto::doAlertLink, enterEvent),
     OP("Webserver", planto::doAlertIPAdressLink, enterEvent),
     EXIT("<Back"));

MENU_OUTPUTS(out, MAX_DEPTH,
             U8G2_OUT(u8g2, colors, fontX, fontY, offsetX, offsetY,
                      {0, 0, U8_Width / fontX, U8_Height / fontY}),
             SERIAL_OUT(Serial), NONE  // must have 2 items at least
);

// Funktionsweise der Buttons
Menu::keyMap joystickBtn_map[] = {
    {-PinTasterSelect, defaultNavCodes[Menu::enterCmd].ch},
    {-PinTasterEsc, defaultNavCodes[Menu::escCmd].ch},
    {-PinTasterUp, defaultNavCodes[Menu::upCmd].ch},
    {-PinTasterDown, defaultNavCodes[Menu::downCmd].ch},
};
Menu::softKeyIn<4> joystickBtns(joystickBtn_map);

serialIn serial(Serial);
// MENU_INPUTS(in, &serial);
Menu::menuIn *inputsList[] = {&joystickBtns, &serial};
Menu::chainStream<2> in(inputsList);  // 2 is the number of inputs

NAVROOT(nav, mainMenu, MAX_DEPTH, in, out);

// Methode zum Updaten des Displays
void updateDisplay() {
  // change checking leaves more time for other tasks
  u8g2.firstPage();
  do {
    nav.doOutput();
  } while (u8g2.nextPage());
}

// Wofür genau ist diese Methode? Was macht sie? Warum ist sie Wichtig? Warum
// heißt sie alert, wenn sie das Menue zeigt?

result alert(menuOut &o, idleEvent e) {
  
  switch (e) {
    case Menu::idleStart:
      break;
    case Menu::idling:
      t = bme.readTemperature(); 
      light = lightMeter.readLightLevel(); 
      o.println("messwerte"); 
      water = map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0);
      if (water < 0) {
        water = 0;
      }
      if (water > 100) {
        water = 100;
      }
      h = bme.readHumidity();
      //hum = ((int)(h * 10)) / 10.0;
      o.setCursor(0, 0);
      o.print("Temperatur ");
      o.print(t-2.8, 1);
      o.setCursor(16, 0);
      o.print("C");
      o.setCursor(0, 1);
      o.print("Wasserstand ");
      o.print(water);
      o.setCursor(15, 1);
      o.print("%");
      o.setCursor(0, 2);
      o.print("Feuchtigkeit ");
      o.print(h);
      o.setCursor(17, 2);
      o.print("%");
      o.setCursor(0, 3);
      o.print("Helligkeit ");
      o.print(light);
      o.setCursor(15, 3);
      o.print("lx");
      break;
    case Menu::idleEnd:
      break;
    default:
      break;
  }
  return proceed;
}
/*result idleMenuLink(menuOut &o, idleEvent e) {
  return menuService.idle_menu_callback_(o, e);
}*/
result idleMenu(menuOut &o, idleEvent e) {
  o.clear();
  switch (e) {
    case idleStart:
      o.println("suspending menu!");
      flag_idling = true;
      break;
    case idling:
      o.clear();
      planto::menuService.warnings_callback_(o);
      break;
    case idleEnd:
      o.println("resuming menu.");
      flag_idling = false;
      last_active_display = millis();
      break;
  }
  return proceed;
}
/*Methode zur Darstellung der IP Adresse, wenn man im Menüpunkt auf Webserver klickt
*/
result idleIPAdress(menuOut &o, idleEvent e){
  o.clear(); 
  switch(e){
    case Menu::idleStart:
      break;
    case Menu::idling:
      o.clear(); 
      planto::menuService.ipadress_callback_(o); 
      break; 
    case Menu::idleEnd:
      break;
    default:
      break;
  }
  return proceed;
}