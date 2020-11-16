
/*
Benötigte Bibliotheken
teilweise zusätzliche Deklarierung in platformio.ini mit Verweis auf Versionen
--> Erklärung Versionierung von Bibliotheken, ansprechen von Bibliotheken
*/
#include <Adafruit_Sensor.h>     //Basisklasse für viele Sensoren
#include <Arduino.h>             //Ansprechen des Arduinos
#include <BH1750.h>              //Lichtsensor
#include <DHT.h>                 //Feuchtigkeits- und Temperatursensor
#include <WiFi.h>                //WiFi-Verbindung
#include <WiFiUdp.h>             //Uhrzeitabfrage über WiFi
#include <Wire.h>                //Kommunikation mit I2C
#include <menu.h>                //Menu
#include <menuIO/chainStream.h>  //Verbindung von mehreren Input-Streams zu einem
#include <menuIO/serialIn.h>  //Verwendung von standardmäßigem seriellen Input
#include <menuIO/serialOut.h>  //Verwendung von standardmäßigem seriellen Output
#include <menuIO/softkeyIn.h>  //generische Schaltflächen
#include <menuIO/u8g2Out.h>    //Nutzung von u8g2 Display

#include "fan.h"  //Klasse für den Ventilator
//#include <NTPClient.h>           //Uhrzeitabfrage über WiFi

// Parameter für das Display und das Menü
#define MAX_DEPTH 1
#define fontName u8g2_font_7x13_mf
#define fontX 7
#define fontY 16
#define offsetX 0
#define offsetY 3
#define U8_Width 128
#define U8_Height 64

// für den DHT11-Sensor zum Messen der Luftfeuchtigkeit und Temperatur
#define DHTPIN 32
#define DHTTYPE DHT11
/*
benötigte Variablen,um unsere Hardware ansprechen
--> Datentypen erläutern
*/
int PinCapacitiveSoil = 15;  // Pin-Belegung Feuchtigkeitssensor
long last_change;            // Zeitstempel der letzten Änderung im Display
int duration = 5000;         // Dauer in ms für Displayupdate
int display_timeout =
    10000;  // Display wechselt in super Menu Modus nach 30 min=18000000ms
menuNode *last_selected_prompt = nullptr;
long last_light = 0;
long last_active_display = 0;  // Zeitstempel der letzen Benutzung
// Messwerte
float h;    // abgefragter Luftfeuchtigkeitswert --> umbennen
float t;    // abgefragter Temperaturwert --> umbennen
int water;  // abgefragter Wasserstand -->umbennen
int hum;    // umgewandelter Feuchtigkeitswert im Bereich 0-100 -->umbennen
float light = 0.0;  // abgefragter Lichtwert -->umbennen
// Bool-Variablen als Flags für Fehlermeldungen
int counter_warnings = 0;  // Zähler Fehlermeldungen --> umbennen
bool flag_temp = false;    // Statuskennzeichen für Temperaturwarnungen
bool flag_water = false;   // Statuskennzeichen für Wasserwarnungen
bool flag_hum = false;     // Statuskennzeichen für Luftfeuchtigkeitswarnungen
bool flag_light = false;   // Statuskennzeichen für Lichtwarnungen
bool flag_idling = false;
int last_path = 0;
// Buttons
int PinTasterSelect = 16;  // Schalter zum Bestätigen
int PinTasterUp = 17;      // Taster zum Auswählen nach oben
int PinTasterDown = 18;    // Taster zum Auswählen nach unten
int PinTasterEsc = 19;     // Taster zum zurück gehen
// Growing-LED
int PinLED = 25;           // Pin-Belegung LED
int dutyCycleLED = 0;      // Regulierung des PWM Signals --> PWM erläutern
const int freq = 5000;     // Arduino-PWM-Frequenz ist bei 500Hz
                           // (https://www.arduino.cc/en/tutorial/PWM)
const int ledChannel = 0;  // Vergebung eines internen Channels, beliebig
                           // wählbar, hier Nutzung des ersten
const int resolution = 8;  // Auflösung in Bits von 0 bis 32, bei 8-Bit
                           // (Standard) erhält man Werte von 0-255
// Ventilator
/*
const int fanPWM = 27;       //Pin-Belegung für das PWM-Signal
const int fanTacho = 26;     //nicht genutzt --> löschen
int dutyCycleFan = 0;        //Regulierung des PWM-Signals
const int fanFreq = 25000;   //Ventilator-PWM-Frequenz zwischen 21kHz and 28kHz,
hier 25kHz const int fanResolution = 8; //Auflösung in Bits von 0 bis 32, bei
8-Bit (Standard) erhält man Werte von 0-255 const int fanChannel = 0;
//Vergebung eines internen Channels, beliebig wählbar, hier Nutzung des ersten
*/
// Die Klasse Fan zum Ansprechen des Ventilators wurde ausgelagert in fan.cpp
planto::Fan fan;

BH1750 lightMeter(
    0x5C);  // I2C Adresse für den Lichtsensor, häufig 0x23, sonst oft 0x5C

DHT dht(DHTPIN, DHTTYPE);  // Initialisierung des DHT Sensors für Temperatur-
                           // und Luftfeuchtigkeit

// WiFi-Verbindung
// Replace with your network credentials
// const char *ssid = "PROTOHAUS";
// const char *password = "PH-Wlan-2016#";
// const char *ssid = "FRITZ!Box 7520 FJ 2-4";
// const char *password = "75113949923584998220";
 const char *ssid = "Protohaus_Villa";
 const char *password = "PH-Wlan-2018#";

// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String outputLed = "off";
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

WiFiUDP ntpUDP;
// NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

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

// Funktionsdeklarieung, damit im Menü direkt darauf zugegriffen werden kann
// --> Hierarchie/Struktur von Programmen
result updateGrowLED();
result warnungen(float licht);
result updateFan();
result doAlert(eventMask enterEvent, prompt &item);

/*
Code für das Menü (extern runter geladene Lib)
kurz Unterschiede von Field und Op erläutern
(https://github.com/neu-rah/ArduinoMenu/wiki/Menu-definition)
*/
MENU(mainMenu, "Einstellungen", Menu::doNothing, Menu::noEvent, Menu::wrapStyle,
     FIELD(dutyCycleLED, "LED", "%", 0, 255, 25, 10, updateGrowLED,
           eventMask::exitEvent, noStyle),
     FIELD(fan.dutyCycleFan_, "Ventilator", " ", 0, 255, 25, 10, updateFan,
           eventMask::exitEvent, noStyle),
     OP("Messwerte", doAlert, enterEvent), EXIT("<Back"));

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
// Wofür genau ist diese Methode? Was macht sie? Warum ist sie Wichtig? Warum
// heißt sie alert, wenn sie das Menue zeigt?
result alert(menuOut &o, idleEvent e) {
  switch (e) {
    case Menu::idleStart:
      break;
    case Menu::idling:
      t = dht.readTemperature();
      water = map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0);
      if (water < 0) {
        water = 0;
      }
      if (water > 100) {
        water = 100;
      }
      h = dht.readHumidity();
      hum = ((int)(h * 10)) / 10.0;
      o.setCursor(0, 0);
      o.print("Temperatur ");
      o.print(t, 1);
      o.setCursor(16, 0);
      o.print("C");
      o.setCursor(0, 1);
      o.print("Wasserstand ");
      o.print(water);
      o.setCursor(15, 1);
      o.print("%");
      o.setCursor(0, 2);
      o.print("Feuchtigkeit ");
      o.print(hum);
      o.setCursor(16, 2);
      o.print("%");
      o.setCursor(0, 3);
      o.print("Helligkeit ");
      o.print(light, 0);
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

// Methode zur Regelung der LED-Helligkeit
result updateGrowLED() {
  ledcWrite(ledChannel, dutyCycleLED);
  return proceed;
}

// Methode zur Regelung der Ventilator-Geschwindigkeit
result updateFan() {
  fan.updateSpeed();
  return proceed;
}

// Wozu brauchen wir die Methode nochmal genau? Warum ist die Wichtig oder
// Funktionalität zum menue?

result doAlert(eventMask e, prompt &item) {
  nav.idleOn(alert);
  return proceed;
}

/*
Methode zur Ausgabe von Fehlermeldungen
--> Prinzip if/else if/else, Displayausgabe, Funktion map, Abfrage Sensoren,
Logik der Flags
*/
void warnings(menuOut &o) {
  if (dht.readTemperature() < 15) {
    o.setCursor(0, 2);
    o.println("zu kalt");
    flag_temp = true;
  } else if (dht.readTemperature() > 30) {
    o.setCursor(0, 2);
    o.println("zu warm");
    flag_temp = true;
  } else {
    flag_temp = false;
  }
  if (map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0) < 10) {
    o.setCursor(0, 1);
    o.println("zu wenig Wasser");
    flag_water = true;
  } else if (map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0) > 95) {
    o.setCursor(0, 1);
    o.println("zu viel Wasser");
    flag_water = true;
  } else {
    flag_water = false;
  }
  if (dht.readHumidity() < 15) {
    o.setCursor(0, 3);
    o.println("Luft ist zu trocken");
    flag_hum = true;
  } else if (dht.readHumidity() > 70) {
    o.setCursor(0, 3);
    o.println("zu feucht");
    flag_hum = true;
  } else {
    flag_hum = false;
  }
  /*
  if (lightMeter.readLightLevel() > 2000)
  {
    o.setCursor(0, 0);
    o.println("zu viel Licht");
    flag_light = true;
  }
  else if (lightMeter.readLightLevel() < 50)
  {
    o.setCursor(0, 0);
    o.println("zu wenig Licht");
    flag_light = true;
  }
  else
  {
    flag_light = false;
  }
  */

  // unten einfügen: && flag_light == false
  // aktuell ist der Lichtsensor nicht angeschlossen, deswegen dessen
  // Fehlermeldung nicht berücksichtigen
  if (flag_hum == false && flag_water == false && flag_water == false) {
    o.setCursor(0, 1);
    o.println("Die Pflanze ist");
    o.setCursor(0, 2);
    o.println("gut versorgt :)");
  }
  counter_warnings = flag_water + flag_light + flag_temp + flag_hum;
}

/*
Beginn idle
--> idle erläutern
*/
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

// Methode zum Updaten des Displays
void updateDisplay() {
  // change checking leaves more time for other tasks
  u8g2.firstPage();
  do {
    nav.doOutput();
  } while (u8g2.nextPage());
}

/*
Setup des Programms
Einmalige Ausführung zu Beginn
Initialisierung von Pins, Sensoren etc.
*/
void setup() {
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

  fan.init();

  // timeClient.begin();

  nav.idleTask = idleMenu;

  Serial.begin(115200);
  while (!Serial)
    ;

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to  ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}
/*
Schleife des Programms
wiederholt sich endlos
*/
void loop() {
  light = lightMeter.readLightLevel();  // Abfrage Licht
  delay(150);

  WiFiClient client = server.available();  // Listen for incoming clients

  // Abfrage der Uhrzeit (s.o. Winter- und Sommerzeit manuell einstellbat)
  // timeClient.update();
  // Serial.print(daysOfTheWeek[timeClient.getDay()]);
  // Serial.print(", ");
  // Serial.println(timeClient.getFormattedTime());

  // Aktualisierung in Zeitintervall
  // aktuell 5000 ms, d.h. alle 5 Sek ohne Änderung der Messwerte
  // eine höhere Zeitspanne reduziert den Energieverbrauch
  if (abs(last_change - millis()) > duration) {
    last_change = millis();
    nav.idleChanged = true;
    nav.refresh();
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
        digitalWrite(PinLED, HIGH);
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
            // turns the GPIOs on and off
            if (header.indexOf("GET /led/on") >= 0)
            {
              Serial.println("LED on");
              outputLed = "on";
              ledcWrite(ledChannel, 255); 
            }
            else if (header.indexOf("GET /led/off") >= 0)
            {
              Serial.println("LED off");
              outputLed = "off";
              ledcWrite(ledChannel, 0);
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
            client.println("</body></html>");

            // Display current state, and ON/OFF buttons for GPIO 26
            //client.println("<p>LED" + outputLed + "</p>");
            client.println("<p>LED </p>");
            // If the output26State is off, it displays the ON button
            if (outputLed == "off")
            {
              client.println("<p><a href=\"/led/on\"><button class=\"button\">ON</button></a></p>");
            }
            else
            {
              client.println("<p><a href=\"/led/off\"><button class=\"button button2\">OFF</button></a></p>");
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
