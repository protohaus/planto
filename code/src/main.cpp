
/*
Benötigte Bibliotheken
teilweise zusätzliche Deklarierung in platformio.ini mit Verweis auf Versionen
--> Erklärung Versionierung von Bibliotheken, ansprechen von Bibliotheken
*/

#include <Arduino.h>          //Ansprechen des Arduinos
//#include <BH1750.h>           //Lichtsensor
#include <NTPClient.h>        //Uhrzeitabfrage über WiFi
#include <WiFi.h>             //WiFi-Verbindung
#include <WiFiUdp.h>          //Uhrzeitabfrage über WiFi

#include <planto_menu.h>      //eigene Klasse für Initialisierungen
#include "bootstrap.h"        //eigene Klasse für Wlan-Wechsel


/*
benötigte Variablen,um unsere Hardware anzusprechen
--> Datentypen erläutern
*/

long last_change;  // Zeitstempel der letzten Änderung im Display

// Messwerte

float setuplight = 0.0;  // abgefragter Lichtwert -->umbennen
// Bool-Variablen als Flags für Fehlermeldungen
int counter_warnings = 0;  // Zähler Fehlermeldungen --> umbennen
bool flag_temp = false;    // Statuskennzeichen für Temperaturwarnungen
bool flag_water = false;   // Statuskennzeichen für Wasserwarnungen
bool flag_hum = false;     // Statuskennzeichen für Luftfeuchtigkeitswarnungen
bool flag_light = false;   // Statuskennzeichen für Lichtwarnungen
int last_path = 0;


//enum Klassen, um die Zustände von LED und FAN zu erfassen 
enum class Ledzustand {
  an, 
  aus
} ; 
Ledzustand ledzustand = Ledzustand::aus; 

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
bool warningout = false;  // Warnungen ein und ausschalten manuell
                          // besser die Warnungen aus sind

// Growing-LED
int PinLED = 25;  // Pin-Belegung LED
const int freq = 5000;     // Arduino-PWM-Frequenz ist bei 500Hz
                           // (https://www.arduino.cc/en/tutorial/PWM)
const int ledChannel = 0;  // Vergebung eines internen Channels, beliebig
                           // wählbar, hier Nutzung des ersten
const int resolution = 8;  // Auflösung in Bits von 0 bis 32, bei 8-Bit
                           // (Standard) erhält man Werte von 0-255


// Set web server port number to 80
WiFiServer server(80);
//Variable um zu gucken ob Webpage refreshed wurde

enum class Buttonzustand {
  an, 
  aus
} ; 
Buttonzustand LedButtonzustand = Buttonzustand::aus; 
Buttonzustand FanButtonzustand = Buttonzustand::aus; 

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String outputFan = "off";

// Current time
unsigned long currentTime = millis();
// Previous time
unsigned long previousTime = 0;
// Define timeout time in milliseconds (example: 2000ms = 2s)
const long timeoutTime = 2000;

// Zeitverschiebung UTC <-> MEZ (Winterzeit) = 3600 Sekunden (1 Stunde)
// Zeitverschiebung UTC <-> MEZ (Sommerzeit) = 7200 Sekunden (2 Stunden)
const long utcOffsetInSeconds = 3600;

int thistime = 0;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

// Funktionsdeklarieung, damit im Menü direkt darauf zugegriffen werden kann
// --> Hierarchie/Struktur von Programmen
result updateGrowLED();
result updateFan();
result doAlert(eventMask enterEvent, prompt &item);
result doAlertIPAdress(eventMask enterEvent, prompt & item); 
result doAlertBootstrap(eventMask enterEvent, prompt &item); 
void warnings(menuOut &o);
void printIpAdresse(menuOut &o); 

// Methode zur Regelung der LED-Helligkeit

result updateGrowLED() {
  Ledzustand ledzustandAktuell; 
  if (dutyCycleLED > 0) {
    ledzustandAktuell = Ledzustand::an; 
  } else {
    ledzustandAktuell = Ledzustand::aus; 
  }
  if (ledzustandAktuell != ledzustand){
    ledzustand = ledzustandAktuell;
    ledcWrite(ledChannel, dutyCycleLED);
  }
  return proceed;
}

// Methode zur Regelung der Ventilator-Geschwindigkeit

result updateFan() {
  planto::Fan::Zustand zustandAktuell; 
  if (fan.dutyCycleFan_ > 0){
    Serial.println("fan an"); 
    zustandAktuell = planto::Fan::Zustand::an; 
  } else if (fan.dutyCycleFan_ <= 0) {
    Serial.println("fan aus"); 
    zustandAktuell = planto::Fan::Zustand::aus; 
  }
  if (zustandAktuell != fan.zustand){
    fan.zustand = zustandAktuell; 
    Serial.println("change");
    fan.updateSpeed();
  }
  return proceed;
}

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
  Serial.print("wasserzustand: "); Serial.println(wasserstandProzent);
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
  Serial.print("licht: "); Serial.println(light);
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


// Methode um LED via Zeit anschalten

void turnonLED() {
  dutyCycleLED = 100;
  updateGrowLED(); 
  Serial.println("LED on");
}

// Methode um LED via Zeit auszuschalten
void turnoffLED() {
  dutyCycleLED = 0;
  updateGrowLED(); 
  Serial.println("LED off");
}

// Methoden zum an und ausschalten des Fans
void turnonFan(){
  fan.dutyCycleFan_ = 100; 
  updateFan(); 
}

void turnoffFan(){
  fan.dutyCycleFan_ = 0; 
  updateFan(); 
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
  

  timeClient.begin();

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

  timeClient.begin();
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

  WiFiClient client = server.available();  // Listen for incoming clients

  // Abfrage der Uhrzeit (s.o. Winter- und Sommerzeit manuell einstellbat)

  timeClient.update();
  // Serial.print(daysOfTheWeek[timeClient.getDay()]); (Wenn man Tag haben
  // möchte)

  thistime = timeClient.getHours();
  if (thistime >= 18 && thistime <= 7) {
    if (ledzustand == Ledzustand::aus) {
      turnonLED();
    }
  } else if (thistime == 19) {
    if (ledzustand == Ledzustand::an) {
      turnoffLED();
    }
  }
  if (thistime >= 17 && thistime <= 7) {
    warningout = true;
  }


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
  
  if (client) {  // If a new client connects,
    currentTime = millis();
    previousTime = currentTime;

    Serial.println("New Client.");  // print a message out in the serial port
    String currentLine =
        "";  // make a String to hold incoming data from the client
    while (client.connected() &&
           currentTime - previousTime <=
               timeoutTime) {  // loop while the client's connected
      currentTime = millis();
      if (client.available()) {  // if there's bytes to read from the client,
        char c = client.read();  // read a byte, then
        Serial.write(c);         // print it out the serial monitor
        header += c;
        if (c == '\n') {  // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a
          // row. that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200
            // OK) and a content-type so the client knows what's coming, then a
            // blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the LED & Fan on and off -> Methode einfügen um Status LED/Ventilator zu haben?? 
            
            if (ledzustand == Ledzustand::an){
              if (LedButtonzustand == Buttonzustand::aus){
                turnonLED(); 
                LedButtonzustand = Buttonzustand::an; 
              } else {
                if (header.indexOf("GET /led/on") >= 0) {
                  Serial.println("LED ist bereits an"); 
                } else if (header.indexOf("GET /led/off") >= 0) {
                  Serial.println("LED off");
                  turnoffLED();
                } 
              }  
            } else {
              if (LedButtonzustand == Buttonzustand::an){
                turnoffLED(); 
                LedButtonzustand = Buttonzustand::aus; 
              } else {
                if (header.indexOf("GET /led/on") >= 0){
                  Serial.println("LED on");
                  turnonLED();
                } else if (header.indexOf("GET /led/off") >= 0){
                  Serial.println("LED bereits aus"); 
                }
              }  
            }
           
            if (fan.zustand == planto::Fan::Zustand::an){
              if (FanButtonzustand == Buttonzustand::aus){
                turnonFan(); 
                FanButtonzustand = Buttonzustand::an; 
              } else {
                if (header.indexOf("GET /fan/on") >= 0) {
                  Serial.println("fan ist bereits an"); 
                } else if (header.indexOf("GET /fan/off") >= 0) {
                  Serial.println("fan off");
                  turnoffFan();
                }
              }  
            } else {
              if (FanButtonzustand == Buttonzustand::an){
                turnoffFan(); 
                FanButtonzustand = Buttonzustand::aus; 
              } else {
                if (header.indexOf("GET /fan/on") >= 0){
                  Serial.println("fan on");
                  turnonFan();
                } else if (header.indexOf("GET /fan/off") >= 0){
                  Serial.println("fan bereits aus"); 
                }
              }  
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println(
                "<head><meta name=\"viewport\" content=\"width=device-width, "
                "initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes
            // to fit your preferences
            client.println(
                "<style>html { font-family: Helvetica; display: inline-block; "
                "margin: 0px auto; text-align: center;}");
            client.println(
                ".button { background-color: #4CAF50; border: none; color: "
                "white; padding: 16px 40px;");
            client.println(
                "text-decoration: none; font-size: 30px; margin: 2px; cursor: "
                "pointer;}");
            client.println(
                ".button2 {background-color: #555555;}</style></head>");
            // client.println("text-decoration: none; font-size: 30px; margin:
            // 2px; cursor: pointer;}"); client.println(".button3
            // {background-color: #555555;}</style></head>");

            // Web Page Heading
            client.println("<body><h1>Planto Web Server</h1>");
            client.println("<p> Temperatur</p>");
            t = bme.readTemperature()-2.8; 
            client.println(String("<p>") + t + " C</p>");
            client.println("<p> Wasserstand </p>");
            water = map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0);
            if (water < 0) {
              water = 0;
            }
            if (water > 100) {
              water = 100;
            }
            client.println(String("<p>") + water + " %</p>");
            client.println("<p> Luftfeuchtigkeit </p>");
            h = bme.readHumidity();
            //hum = ((int)(h * 10)) / 10.0;
            client.println(String("<p>") + h + " % </p>");
            light = lightMeter.readLightLevel();
            client.println("<p> Helligkeit </p>");
            client.println(String("<p>") + light + " lx </p>");
            client.println("</body></html>");

            // Display current state, and ON/OFF buttons for GPIO 26
            client.println("<p>LED </p>");
            if (ledzustand == Ledzustand::an) {
              client.println(
                  "<p><a href=\"/led/off\"><button "
                  "class=\"button\">ON</button></a></p>");
              LedButtonzustand = Buttonzustand::an; 
            } else if (ledzustand == Ledzustand::aus) {
              client.println(
                  "<p><a href=\"/led/on\"><button class=\"button "
                  "button2\">OFF</button></a></p>");
              LedButtonzustand = Buttonzustand::aus;      
            }

            client.println("<p>Ventilator </p>");
            // If the output26State is off, it displays the ON button
            if (fan.zustand == planto::Fan::Zustand::an) {
              client.println(
                  "<p><a href=\"/fan/off\"><button "
                  "class=\"button\">ON</button></a></p>");
              FanButtonzustand = Buttonzustand::an;     
            } else {
              client.println(
                  "<p><a href=\"/fan/on\"><button class=\"button "
                  "button2\">OFF</button></a></p>");  
              FanButtonzustand = Buttonzustand::aus;     
            }

            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else {  // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage
                                 // return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

// generell: Wo siehst du hier Code, den wir ggf. für Kids vereinfachen können?
// oder andere Möglichkeiten, wie wir den code verständlich den Kindern näher
// bringen können? Durch das vorgegebene Menü ist das ja jetzt nicht so super
// easy nachzuvollziehen...
// Was fehlt hier jetzt noch?
// Wo können wir welche erledigten Aufgaben hinter schreiben? (Energieersparnis,
// Ansteuerung xy-Sensor etc.)
