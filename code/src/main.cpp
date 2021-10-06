
/*
Benötigte Bibliotheken
teilweise zusätzliche Deklarierung in platformio.ini mit Verweis auf Versionen
--> Erklärung Versionierung von Bibliotheken, ansprechen von Bibliotheken
*/

#include <Arduino.h>          //Ansprechen des Arduinos
//#include <BH1750.h>           //Lichtsensor


#include <mywebserver.h>      //eigene Klasse für Initialisierungen
#include "bootstrap.h"        //eigene Klasse für Wlan-Wechsel


/*
benötigte Variablen,um unsere Hardware anzusprechen
--> Datentypen erläutern
*/

long last_change;  // Zeitstempel der letzten Änderung im Display

// Messwerte

int counter_warnings = 0;  // Zähler Fehlermeldungen --> umbennen
bool flag_temp = false;    // Statuskennzeichen für Temperaturwarnungen
bool flag_water = false;   // Statuskennzeichen für Wasserwarnungen
bool flag_hum = false;     // Statuskennzeichen für Luftfeuchtigkeitswarnungen
bool flag_light = false;   // Statuskennzeichen für Lichtwarnungen
int last_path = 0;


//enum Klassen, um die Zustände der Sensoren und Änderungen in echtzeit zu erfassen und nur
//dann in einen Warnhinweis zu wechseln 
enum class Wasserzustand {
  ok,
  zuWenig, 
  zuViel 
} ;

Wasserzustand wasserzustand = Wasserzustand::ok;
const int wasserstandProzentZuViel = 95; 
const int wasserstandProzentZuWenig = 5; 

enum class Temperaturzustand {
  ok, 
  zuWarm, 
  zuKalt, 
} ; 

Temperaturzustand temperaturzustand = Temperaturzustand::ok; 
const float temperaturGradZuWarm = 30; 
const float temperaturGradZuKalt = 15; 

enum class Luftfeuchtigkeitzustand {
  ok, 
  zuTrocken, 
  zuFeucht, 
} ; 

Luftfeuchtigkeitzustand luftfeuchttigkeitzustand = Luftfeuchtigkeitzustand::ok; 
const float luftfeuchtigkeitzustandZuTrocken = 15; 
const float luftfeuchtigkeitzustandZuFeucht = 70; 

enum class Lichtzustand {
  ok, 
  zuHell, 
  zuDunkel, 
} ; 

Lichtzustand lichtzustand = Lichtzustand::ok; 
const float lichtLuxZuHell = 2500; 
const float lichtLuxZuDunkel = 30; 



bool updateIdleScreen = false; //Flag um zu gucken, ob eine Änderung zum vorherigen Zustand vorkommt


// Funktionsdeklarieung, damit im Menü direkt darauf zugegriffen werden kann
// --> Hierarchie/Struktur von Programmen
result updateGrowLED();
result updateFan();
result doAlert(eventMask enterEvent, prompt &item);
result doAlertIPAdress(eventMask enterEvent, prompt & item); 
result doAlertBootstrap(eventMask enterEvent, prompt &item); 
void warnings(menuOut &o);
void printIpAdresse(menuOut &o); 



//Methode zur Ausgabe der IP Adresse 

void printIpAdresse(menuOut &o){
  o.setCursor(0,0); 
  o.println("IP-Adresse"); 
  o.setCursor(0,1); 
  o.println("Webserver:");
  o.setCursor(0,3); 
  o.println(WiFi.localIP()); 
}


// Methoden zum aktivieren der Callbacks und Methoden in planto_menu.h

result doAlert(eventMask e, prompt &item) {
  nav.idleOn(alert);
  return proceed;
}

result doAlertIPAdress(eventMask e, prompt &item){
  nav.idleOn(idleIPAdress); 
  return proceed; 
}


/*
Methode zur Ausgabe von Fehlermeldungen
--> Prinzip if/else if/else, Displayausgabe, Funktion map, Abfrage Sensoren,
Logik der Flags
*/
void warnings(menuOut &o) {
  if (warningout == false) {
    
    if (temperaturzustand == Temperaturzustand::zuKalt) {
      o.setCursor(0, 2);
      o.println("zu kalt");
      
    } else if (temperaturzustand == Temperaturzustand::zuWarm) {
      o.setCursor(0, 2);
      o.println("zu warm");
    }
    
    if (wasserzustand == Wasserzustand::zuWenig) {
      o.setCursor(0, 1);
      o.println("zu wenig Wasser");
    } else if (wasserzustand == Wasserzustand::zuViel) {
      o.setCursor(0, 1);
      o.println("zu viel Wasser");
    }
    if (luftfeuchttigkeitzustand == Luftfeuchtigkeitzustand::zuTrocken) {
      o.setCursor(0, 3);
      o.println("Luft ist zu trocken");
    } else if (luftfeuchttigkeitzustand == Luftfeuchtigkeitzustand::zuFeucht) {
      o.setCursor(0, 3);
      o.println("zu feucht");
    }
    if (lichtzustand == Lichtzustand::zuHell) {
      o.setCursor(0, 0);
      o.println("zu viel Licht");
    } else if (lichtzustand == Lichtzustand::zuDunkel) {
      o.setCursor(0, 0);
      o.println("zu wenig Licht");
    }
    if (temperaturzustand == Temperaturzustand::ok && wasserzustand == Wasserzustand::ok && luftfeuchttigkeitzustand == Luftfeuchtigkeitzustand::ok && lichtzustand == Lichtzustand::ok) {
      /*o.setCursor(0, 1);
      o.println("Die Pflanze ist");
      o.setCursor(0, 2);
      o.println("gut versorgt :)");*/
      nav.idleOff(); 
    }
  } else if (warningout == true) {
    Serial.println(
        "es ist nachts, es werden keine Warnungen angezeit Zzzzzzzzz");
    nav.idleOff(); 
  }
}

//Methode um die Sensoren abzufragen und für Warnungen einzustufen bezüglich Werte

void checkSensors(){
  int wasserstandProzent = map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0); 
  Wasserzustand wasserzustandAktuell; 
  if (wasserstandProzent < wasserstandProzentZuWenig){
    wasserzustandAktuell = Wasserzustand::zuWenig; 
  } else if (wasserstandProzent > wasserstandProzentZuViel){
    wasserzustandAktuell = Wasserzustand::zuViel; 
  } else{
    wasserzustandAktuell = Wasserzustand::ok; 
  }
  if (wasserzustandAktuell != wasserzustand){
    wasserzustand = wasserzustandAktuell; 
    updateIdleScreen = true; 
  }

  float temperaturGrad = bme.readTemperature(); 
  temperaturGrad = temperaturGrad-2.8; 
  Temperaturzustand temperaturzustandAktuell; 
  if (temperaturGrad < temperaturGradZuKalt){
    temperaturzustandAktuell = Temperaturzustand::zuKalt;
  } else if (temperaturGrad > temperaturGradZuWarm){
    temperaturzustandAktuell = Temperaturzustand::zuWarm; 
  } else {
    temperaturzustandAktuell = Temperaturzustand::ok; 
  }
  if (temperaturzustandAktuell != temperaturzustand){
    temperaturzustand = temperaturzustandAktuell; 
    updateIdleScreen = true; 
  }

  float luftfeuchtigkeit = bme.readHumidity(); 
  Luftfeuchtigkeitzustand luftfeuchtigkeitzustandAktuell; 
  if (luftfeuchtigkeit < luftfeuchtigkeitzustandZuTrocken){
    luftfeuchtigkeitzustandAktuell = Luftfeuchtigkeitzustand::zuTrocken;
  } else if (luftfeuchtigkeit > luftfeuchtigkeitzustandZuFeucht){
    luftfeuchtigkeitzustandAktuell = Luftfeuchtigkeitzustand::zuFeucht;  
  } else {
    luftfeuchtigkeitzustandAktuell = Luftfeuchtigkeitzustand::ok; 
  }
  if (luftfeuchtigkeitzustandAktuell != luftfeuchttigkeitzustand){
    luftfeuchttigkeitzustand = luftfeuchtigkeitzustandAktuell; 
    updateIdleScreen = true; 
  }

  light = lightMeter.readLightLevel(); 
  Lichtzustand lichtzustandAktuell; 
  if (light < lichtLuxZuDunkel){
    lichtzustandAktuell = Lichtzustand::zuDunkel;
  } else if (light > lichtLuxZuHell){
    lichtzustandAktuell = Lichtzustand::zuHell;  
  } else {
    lichtzustandAktuell = Lichtzustand::ok; 
  }
  if (lichtzustandAktuell != lichtzustand){
    lichtzustand = lichtzustandAktuell; 
    updateIdleScreen = true; 
  }
}

//Methode zur Ausgabe der Booting Hinweise

void printBooting(){
  
  if (bootstrapzustand == Bootstrapzustand::lookingForWifi){
    u8g2.firstPage();
    do {
      u8g2.setFont(fontName);
      u8g2.drawStr(0,15,"Wifi not found");
      u8g2.drawStr(0,30,"Connect with");
      u8g2.drawStr(0,45,"esp32 and");
      u8g2.drawStr(0,60,"http://10.1.1.1");
    } while ( u8g2.nextPage() );
    delay(5000);
  }
  else if (bootstrapzustand == Bootstrapzustand::wifiConnctedWith){
    u8g2.firstPage();
    do {
      u8g2.setFont(fontName);
      u8g2.drawStr(0,15,"Wifi");
      u8g2.drawStr(0,30,"Connected");
    } while ( u8g2.nextPage() );
    delay(5000);
  }
}

/*
Setup des Programms
Einmalige Ausführung zu Beginn
Initialisierung von Pins, Sensoren etc.
*/
void setup() {
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(PinLED, ledChannel);

  Wire.begin(21,22); 
  u8g2.begin(21, 22, 0x3C);
  u8g2.setFont(fontName);

  pinMode(PinTasterSelect, INPUT_PULLUP);
  pinMode(PinTasterUp, INPUT_PULLUP);
  pinMode(PinTasterDown, INPUT_PULLUP);
  pinMode(PinTasterEsc, INPUT_PULLUP);

  fan.init();

  Serial.begin(115200);
  while (!Serial)
    ;

  /* Anzeige auf dem Bildschirm, ob der Panto mit dem Internet verbunden ist oder nicht
  */
  do{
    u8g2.firstPage();
    do {
      u8g2.setFont(fontName);
      u8g2.drawStr(0,15,"Connecting");
      u8g2.drawStr(0,30,"with Wifi ...");
    } while ( u8g2.nextPage() );
    setupbootstrap();
    if(bootstrapzustand == Bootstrapzustand::wifiConnctedWith){
      printBooting(); 
    } else {
      printBooting(); 
      ESP.restart();
    }
  } while (bootstrapzustand == Bootstrapzustand::lookingForWifi); 
  

  webserverInit(); 

  // nav.idleTask = planto::idleMenu;
  nav.idleTask = idleMenu;
  

  planto::menuService.SetGrowLEDCallback(updateGrowLED);
  planto::menuService.SetFanCallback(updateFan);
  planto::menuService.SetDoAlertCallback(doAlert);
  planto::menuService.SetWarningsCallback(warnings);
  planto::menuService.SetIPAdresseCallback(printIpAdresse); 
  planto::menuService.SetDoAlertIPAdress(doAlertIPAdress); 

  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  Serial.println(WiFi.SSID()); 

  
  bool status; 

  nav.doInput();
  
  if (nav.changed(0)) {
    updateDisplay();
  }
  nav.doOutput();

  status = bme.begin(0x76); 
  /*if (!status) {  
    Serial.println("Could not find a valid BMP280 !");
    while (1);
  }*/

  lightMeter.begin(); 
  
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }

}
/*
Schleife des Programms
wiederholt sich endlos
*/
void loop() {
  //setuplight = lightMeter.readLightLevel();  // Abfrage Licht
  
  //delay(150);
  webserverLoop();

  // Aktualisierung in Zeitintervall
  // aktuell 5000 ms, d.h. alle 5 Sek ohne Änderung der Messwerte
  // eine höhere Zeitspanne reduziert den Energieverbrauch
  if (labs(last_change - millis()) > duration) {
    last_change = millis();
    checkSensors(); 

    if(updateIdleScreen){
      updateIdleScreen = false; 
      nav.idleChanged = true;
      nav.refresh(); //wozu? 
      nav.idleOn(idleMenu); //so springt er direkt ins Menu
    }
  }
  nav.doInput();

  if (nav.changed(0)) {
    updateDisplay();
  }
  nav.doOutput();

  if (last_path != nav.path->sel) {
    last_path = nav.path->sel;
    last_active_display = millis();
  }

  if (flag_idling == false &&
      labs(last_active_display - millis()) > display_timeout) {
    nav.idleOn(idleMenu);
  }
  aktualisierungWebserver(); 
}

// generell: Wo siehst du hier Code, den wir ggf. für Kids vereinfachen können?
// oder andere Möglichkeiten, wie wir den code verständlich den Kindern näher
// bringen können? Durch das vorgegebene Menü ist das ja jetzt nicht so super
// easy nachzuvollziehen...
// Was fehlt hier jetzt noch?
// Wo können wir welche erledigten Aufgaben hinter schreiben? (Energieersparnis,
// Ansteuerung xy-Sensor etc.)
