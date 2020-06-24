#include <Arduino.h>
#include <Wire.h>
#include <menu.h>
#include <menuIO/serialOut.h>
#include <menuIO/chainStream.h>
#include <menuIO/serialIn.h>
#include <menuIO/keyIn.h>
#include <menuIO/softKeyIn.h>
#include <menuIO/u8g2Out.h>
#define OLED_RESET 4
#define SELECT_PIN 3
#define UP_PIN 4
#define DOWN_PIN 2
#define fontName u8g2_font_7x13_mf
#define fontX 7
#define fontY 16
#define offsetX 0
#define offsetY 3
#define U8_Width 128
#define U8_Height 64

U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


int relaisPin = 9;

bool moisture_sensor = false;
//bool grenzwerte_setzen = false;
bool wasser_pumpen = false;
bool wasser_versickern = false;

const unsigned long pumpen_zeit = 5000UL;
const unsigned long versickern_zeit = 5000UL;
const unsigned long anzeige_dauer =10000UL;

bool request_update = false;
const unsigned long update_duration = 400;
unsigned long last_update_ms = 0UL;
unsigned long messung_start_ms = 0UL;
unsigned long pumpen_start_ms = 0UL;
unsigned long versickern_start_ms = 0UL;


int Grenzwert1 = 0;
int Grenzwert2 = 0;

const int AirValue = 620;   
const int WaterValue = 310;  
  
int soilMoistureValue = 0;
int soilmoisturepercent=0;

void kakteen(eventMask e, prompt& item);
void sukkulenten(eventMask e, prompt& item);
void normalePflanzen(eventMask e, prompt& item);
void durstigePflanzen(eventMask e, prompt& item);
void wasserpflanzen(eventMask e, prompt& item);
void messenBoden(menuOut& o);
void pumpen(menuOut& o);
void versickern(menuOut& o);


const Menu::colorDef<uint8_t> colors[6] MEMMODE = {
    {{0, 0}, {0, 1, 1}},  // bgColor
    {{1, 1}, {1, 0, 0}},  // fgColor
    {{1, 1}, {1, 0, 0}},  // valColor
    {{1, 1}, {1, 0, 0}},  // unitColor
    {{0, 1}, {0, 0, 1}},  // cursorColor
    {{1, 1}, {1, 0, 0}},  // titleColor
};

MENU(mainMenu, "Pflanzen", doNothing, noEvent, wrapStyle,
     OP("Kakteen", kakteen, enterEvent),OP("Sukkulenten", sukkulenten, enterEvent),
     OP("normale Pflanzen", normalePflanzen, enterEvent), OP("durstige Pflanzen", durstigePflanzen, enterEvent),
     OP("Wasserpflanzen", wasserpflanzen, enterEvent),
     EXIT("<Exit"));

#define MAX_DEPTH 2

Menu::keyMap joystickBtn_map[] = {
    {-SELECT_PIN, defaultNavCodes[enterCmd].ch},
    {-UP_PIN, defaultNavCodes[upCmd].ch},
    {-DOWN_PIN, defaultNavCodes[downCmd].ch},
};
Menu::softKeyIn<4> joystickBtns(joystickBtn_map);

Menu::serialIn serial(Serial);
Menu::menuIn* inputsList[] = {&joystickBtns, &serial};
Menu::chainStream<2> in(inputsList);  // 3 is the number of inputs

MENU_OUTPUTS(out, MAX_DEPTH,
             U8G2_OUT(u8g2, colors, fontX, fontY, offsetX, offsetY,
                      {0, 0, U8_Width / fontX, U8_Height / fontY}),
             SERIAL_OUT(Serial));

NAVROOT(nav, mainMenu, MAX_DEPTH, in, out);

// when menu is suspended
Menu::result idle(Menu::menuOut& o, Menu::idleEvent e) {
  o.clear();
  switch (e) {
    case Menu::idleStart:
      o.println("suspending menu!");
      break;
    case Menu::idling:
      o.println("suspended...");
      break;
    case Menu::idleEnd:
      o.println("resuming menu.");
      break;
  }
  return Menu::proceed;
}

void updateDisplay() {
  // change checking leaves more time for other tasks
  u8g2.firstPage();
  do {
    nav.doOutput();
  } while (u8g2.nextPage());
}

void uebergabeZeit() {
  if (wasser_pumpen == true && moisture_sensor == false && wasser_versickern == false){
    pumpen_start_ms = messung_start_ms;
  }
  else if (wasser_versickern == true && moisture_sensor == false && wasser_pumpen == false){
    versickern_start_ms = pumpen_start_ms;
  }
  else if (wasser_versickern == false && moisture_sensor == true && wasser_pumpen == false) {
    messung_start_ms = versickern_start_ms;
  }
}


result alert(menuOut& o, idleEvent e) {
  switch (e) {
    case Menu::idleStart:
      moisture_sensor = true; //Messung starten
      
      break;
    case Menu::idling:
      messenBoden(o);
      o.setCursor(0, 3);
      o.print("Escape zu beenden");
      break;
    case Menu::idleEnd:
      // Serial.println("Ending Messen");
      moisture_sensor = false;
      break;
    default:
      break;
  }

  return proceed;
}


result doAlert(eventMask e, prompt& item) {
  nav.idleOn(alert);
  return proceed;
}

void kakteen(eventMask e, prompt& item){
  Grenzwert1 = 5;
  Grenzwert2 = 20;
  doAlert(e, item);
}

void sukkulenten(eventMask e, prompt& item){
  Grenzwert1 = 20;
  Grenzwert2 = 40;
  doAlert(e, item);
}

void normalePflanzen(eventMask e, prompt& item){
  Grenzwert1 = 40;
  Grenzwert2 = 60;
  doAlert(e, item);
}

void durstigePflanzen(eventMask e, prompt& item){
  Grenzwert1 = 60;
  Grenzwert2 = 80;
  doAlert(e, item);
}

void wasserpflanzen(eventMask e, prompt& item){
  Grenzwert1 = 80;
  Grenzwert2 = 100;
  doAlert(e, item);
}

void messenBoden(menuOut& o) {
  unsigned long now_ms = millis();

  if (moisture_sensor == true) {
    soilMoistureValue = analogRead(A0); 
    Serial.println(soilMoistureValue);
    soilmoisturepercent = map(soilMoistureValue, AirValue, WaterValue, 0, 100);
    if(soilmoisturepercent > Grenzwert2 ) 
    {
     o.setCursor(0,0);
     o.print(soilmoisturepercent);
     o.setCursor(1,0);
     o.print("%");
     o.setCursor(0,1);
     o.print("Wassergehalt überschreitet Grenzwert"); //Funktion einbauen für Dauer bis zur nächsten Messung
    }
    else if(soilmoisturepercent < Grenzwert1)
    {
      if (now_ms - messung_start_ms < anzeige_dauer) {
        o.setCursor(0,0);
        o.print(soilmoisturepercent);
        o.setCursor(1,0);
       o.print("%");
       o.setCursor(0,1);
       o.print("Wassergehalt zu niedrig -> befeuchten");
      }
      else if (now_ms - messung_start_ms > anzeige_dauer) {
       Serial.println(now_ms);
       messung_start_ms = now_ms;
       moisture_sensor = false;
       wasser_pumpen = true;
       Serial.println(millis());
       uebergabeZeit();
       Serial.print("messung_start_ms: ");
       Serial.println(messung_start_ms);
       pumpen(o);
     }
    }
    else if(soilmoisturepercent > Grenzwert1 && soilmoisturepercent < Grenzwert2)
    {
     o.setCursor(0,0);
     o.print(soilmoisturepercent);
     o.setCursor(1,0);
     o.print("%");
     o.setCursor(0,1);
     o.print("Wassergehalt optimal"); //Funktion einbauen mit Dauer bis zur nächsten Messung
    }
  }
}

//Problem warum bei Pumpen hängen geblieben wird muss gefunden werden 

void pumpen(menuOut& o) {
  Serial.println(pumpen_start_ms);
  unsigned long now2_ms = millis();
  Serial.println(now2_ms);
  if (wasser_pumpen == true){
    digitalWrite(relaisPin, HIGH);
    o.setCursor(0,0);
    o.print("Es wird gepumpt");
    o.setCursor(0,1);
    o.print("pumpi pumpi pump");
    Serial.println(millis());
    Serial.print("now2_ms: ");
    Serial.println(now2_ms);
    Serial.print("pumpen_start_ms: ");
    Serial.println(pumpen_start_ms);
    if ((now2_ms - pumpen_start_ms) > pumpen_zeit ) {
      wasser_pumpen = false;
      wasser_versickern = true;
      pumpen_start_ms = now2_ms;
      Serial.println(pumpen_start_ms);
      uebergabeZeit();
      versickern(o);
    }
  }
}



void versickern(menuOut& o){
  unsigned long now3_ms = millis();
  Serial.println(now3_ms);
  if (wasser_versickern == true){
     digitalWrite(relaisPin, LOW);
     o.setCursor(0,2);
     o.print("pumpen fertig");
     if (now3_ms - versickern_start_ms > versickern_zeit){
     wasser_versickern = false;
     moisture_sensor = true;
     versickern_start_ms = now3_ms;
     uebergabeZeit();
    }  
  }
}

void setup() 
{
  Serial.begin(115200);
  while (!Serial)
    ;

  pinMode(SELECT_PIN, INPUT_PULLUP);
  pinMode(UP_PIN, INPUT_PULLUP);
  pinMode(DOWN_PIN, INPUT_PULLUP);
  pinMode(relaisPin, OUTPUT);

  u8g2.begin();
  u8g2.setFont(fontName);

  nav.idleTask = idle;
  Serial.println("setup done.");
  Serial.flush();
}

void loop()
{
 
 //updateTimer();
 
   // Run every 400 ms (update_duration) when measuring
 if (request_update && moisture_sensor) {
   request_update = false;

   nav.idleChanged = true;
   nav.refresh();
 } 

 nav.doInput();
 
 
 if (nav.changed(0)) {  // only draw if menu changed for gfx device
    updateDisplay();
 }

 nav.doOutput();
}


