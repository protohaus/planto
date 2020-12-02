#include <Arduino.h>
#include <menu.h>                //Menu
#include <menuIO/chainStream.h>  //Verbindung von mehreren Input-Streams zu einem
#include <menuIO/serialIn.h>  //Verwendung von standardmäßigem seriellen Input
#include <menuIO/serialOut.h>  //Verwendung von standardmäßigem seriellen Output
#include <menuIO/softkeyIn.h>  //generische Schaltflächen
#include <menuIO/u8g2Out.h>    //Nutzung von u8g2 Display

#include <functional>

// Parameter für das Display und das Menü
#define MAX_DEPTH 1
#define fontName u8g2_font_7x13_mf
#define fontX 7
#define fontY 16
#define offsetX 0
#define offsetY 3
#define U8_Width 128
#define U8_Height 64

int duration = 30000;  // Dauer in ms für Displayupdate
int display_timeout =
    10000;  // Display wechselt in super Menu Modus nach 30 min=18000000ms

menuNode *last_selected_prompt = nullptr;
long last_light = 0;
long last_active_display = 0;  // Zeitstempel der letzen Benutzung

// Buttons
int PinTasterSelect = 16;  // Schalter zum Bestätigen
int PinTasterUp = 17;      // Taster zum Auswählen nach oben
int PinTasterDown = 18;    // Taster zum Auswählen nach unten
int PinTasterEsc = 19;     // Taster zum zurück gehen

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
  /*void SetIdleMenuCallback(
      std::function<result(menuOut &, idleEvent)> callback) {
    idle_menu_callback_ = callback;
  }*/

  std::function<result()> grow_led_callback_;
  std::function<result()> fan_callback_;
  std::function<result(eventMask, prompt &)> do_alert_callback_;
  // std::function<result(menuOut &, idleEvent)> idle_menu_callback_;
  bool flag_idling = false;
};
MenuService menuService;

result updateGrowLEDLink() { return menuService.grow_led_callback_(); }
// Funktionsdeklarieung, damit im Menü direkt darauf zugegriffen werden kann
// --> Hierarchie/Struktur von Programmen
result updateFanLink() { return menuService.fan_callback_(); }
result doAlertLink(eventMask enterEvent, prompt &item) {
  return menuService.do_alert_callback_(enterEvent, item);
}

// result alert(menuOut &o, idleEvent e) {return proceed;}
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
      warnings(o);
      break;
    case idleEnd:
      o.println("resuming menu.");
      flag_idling = false;
      last_active_display = millis();
      break;
  }
  return proceed;
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
/*FIELD(fan.dutyCycleFan_, "Ventilator", " ", 0, 255, 25, 10, updateFan,
           eventMask::exitEvent, noStyle),*/
int dummy = 2;

MENU(mainMenu, "Einstellungen", Menu::doNothing, Menu::noEvent, Menu::wrapStyle,
     FIELD(dutyCycleLED, "LED", "%", 0, 255, 25, 10, planto::updateGrowLEDLink,
           eventMask::exitEvent, noStyle),
     FIELD(dummy, "Ventilator", " ", 0, 255, 25, 10, planto::updateFanLink,
           eventMask::exitEvent, noStyle),
     OP("Messwerte", planto::doAlertLink, enterEvent), EXIT("<Back"));

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
