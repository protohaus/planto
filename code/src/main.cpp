
/*
Benötigte Bibliotheken
teilweise zusätzliche Deklarierung in platformio.ini mit Verweis auf Versionen
--> Erklärung Versionierung von Bibliotheken, ansprechen von Bibliotheken
*/
#include <Adafruit_Sensor.h>  //Basisklasse für viele Sensoren
#include <Arduino.h>          //Ansprechen des Arduinos
#include <BH1750.h>           //Lichtsensor
#include <DHT.h>              //Feuchtigkeits- und Temperatursensor
#include <NTPClient.h>        //Uhrzeitabfrage über WiFi
#include <WiFi.h>             //WiFi-Verbindung
#include <WiFiUdp.h>          //Uhrzeitabfrage über WiFi
#include <Wire.h>             //Kommunikation mit I2C
#include <planto_menu.h>

#include "fan.h"  //Klasse für den Ventilator
#include "secrets.h"

/*
benötigte Variablen,um unsere Hardware ansprechen
--> Datentypen erläutern
*/

long last_change;            // Zeitstempel der letzten Änderung im Display

// Messwerte

float setuplight = 0.0;  // abgefragter Lichtwert -->umbennen
// Bool-Variablen als Flags für Fehlermeldungen
int counter_warnings = 0;  // Zähler Fehlermeldungen --> umbennen
bool flag_temp = false;    // Statuskennzeichen für Temperaturwarnungen
bool flag_water = false;   // Statuskennzeichen für Wasserwarnungen
bool flag_hum = false;     // Statuskennzeichen für Luftfeuchtigkeitswarnungen
bool flag_light = false;   // Statuskennzeichen für Lichtwarnungen
int last_path = 0;

bool warningout = false;  // Flag um zu gucken ob es nachts ist und demnach
                          // besser die Warnungen aus sind

// Growing-LED
int PinLED = 25;  // Pin-Belegung LED
// int dutyCycleLED = 0;      // Regulierung des PWM Signals --> PWM erläutern
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

int thistime = 0;
boolean ledon = false;
boolean manualled = true;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

int freq_buzzer = 2000;
int channel_buzzer = 1;
int resolution_buzzer = 8;

// Funktionsdeklarieung, damit im Menü direkt darauf zugegriffen werden kann
// --> Hierarchie/Struktur von Programmen
result updateGrowLED();
result updateFan();
result doAlert(eventMask enterEvent, prompt &item);
void warnings(menuOut &o);


// Methode zur Regelung der LED-Helligkeit

result updateGrowLED() {
  ledcWrite(ledChannel, dutyCycleLED);

  if (dutyCycleLED > 0) {
    outputLed = "on";
  } else {
    outputLed = "off";
  }

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
  if (warningout == false) {
    ledcWriteTone(channel_buzzer, 2000);
    ledcWrite(channel_buzzer, 0);
    if (dht.readTemperature() < 15) {
      o.setCursor(0, 2);
      o.println("zu kalt");
      ledcWrite(channel_buzzer, 75);
      flag_temp = true;
    } else if (dht.readTemperature() > 30) {
      o.setCursor(0, 2);
      o.println("zu warm");
      ledcWrite(channel_buzzer, 75);
      flag_temp = true;
    } else {
      ledcWrite(channel_buzzer, 0);
      flag_temp = false;
    }
    if (map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0) < 10) {
      o.setCursor(0, 1);
      o.println("zu wenig Wasser");
      // ledcWrite(channel_buzzer, 75);
      flag_water = true;
    } else if (map(analogRead(PinCapacitiveSoil), 500, 2500, 100, 0) > 95) {
      o.setCursor(0, 1);
      o.println("zu viel Wasser");
      ledcWrite(channel_buzzer, 75);
      flag_water = true;
    } else {
      ledcWrite(channel_buzzer, 0);
      flag_water = false;
    }
    if (dht.readHumidity() < 15) {
      o.setCursor(0, 3);
      o.println("Luft ist zu trocken");
      ledcWrite(channel_buzzer, 50);
      flag_hum = true;
    } else if (dht.readHumidity() > 70) {
      o.setCursor(0, 3);
      o.println("zu feucht");
      ledcWrite(channel_buzzer, 75);
      flag_hum = true;
    } else {
      ledcWrite(channel_buzzer, 0);
      flag_hum = false;
    }
    /*
    if (lightMeter.readLightLevel() > 2000)
    {
      o.setCursor(0, 0);
      o.println("zu viel Licht");
      ledcWrite(channel_buzzer, 75);
      flag_light = true;
    }
    else if (lightMeter.readLightLevel() < 50)
    {
      o.setCursor(0, 0);
      o.println("zu wenig Licht");
      ledcWrite(channel_buzzer, 75);
      flag_light = true;
    }
    else
    {
      ledcWrite(channel_buzzer, 0);
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
      ledcWriteTone(channel_buzzer, 2000);
      ledcWrite(channel_buzzer, 0);
    }
    counter_warnings = flag_water + flag_light + flag_temp + flag_hum;
  } else if (warningout == true) {
    Serial.println(
        "es ist nachts, es werden keine Warnungen angezeit Zzzzzzzzz");
    ledcWriteTone(channel_buzzer, 2000);
    ledcWrite(channel_buzzer, 0);
  }
}

// Methode um LED via Zeit anschalten

void turnonLED() {
  ledon = true;
  dutyCycleLED = 100;
  ledcWrite(ledChannel, dutyCycleLED);
  Serial.println("LED on");
}

// Methode um LED via Zeit auszuschalten
void turnoffLED() {
  dutyCycleLED = 0;
  ledcWrite(ledChannel, dutyCycleLED);
  Serial.println("LED off");
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

  // nav.idleTask = planto::idleMenu;
  nav.idleTask = idleMenu;

  Serial.begin(115200);
  while (!Serial)
    ;

  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }

  planto::menuService.SetGrowLEDCallback(updateGrowLED);
  planto::menuService.SetFanCallback(updateFan);
  planto::menuService.SetDoAlertCallback(doAlert);
  planto::menuService.SetWarningsCallback(warnings); 

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
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

  timeClient.begin();

  ledcSetup(channel_buzzer, freq_buzzer, resolution_buzzer);
  ledcAttachPin(33, channel_buzzer);
}
/*
Schleife des Programms
wiederholt sich endlos
*/
void loop() {
  setuplight = lightMeter.readLightLevel();  // Abfrage Licht
  delay(150);

  WiFiClient client = server.available();  // Listen for incoming clients

  // Abfrage der Uhrzeit (s.o. Winter- und Sommerzeit manuell einstellbat)

  timeClient.update();
  // Serial.print(daysOfTheWeek[timeClient.getDay()]); (Wenn man Tag haben
  // möchte)

  thistime = timeClient.getHours();
  if (thistime >= 16 && thistime <= 18) {
    Serial.print("HourTime: ");
    Serial.println(thistime);
    if (ledon == false) {
      turnonLED();
    }
  } else if (thistime == 19) {
    if (ledon == true) {
      turnoffLED();
    }
  }
  if (thistime >= 17 && thistime <= 7) {
    warningout = true;
  }

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
            // turns the LED & Fan on and off
            if (header.indexOf("GET /led/on") >= 0) {
              Serial.println("LED on");
              outputLed = "on";
              ledcWrite(ledChannel, 255);
              dutyCycleLED = 255;
            } else if (header.indexOf("GET /led/off") >= 0) {
              Serial.println("LED off");
              outputLed = "off";
              ledcWrite(ledChannel, 0);
              dutyCycleLED = 0;
            }

            if (header.indexOf("GET /fan/on") >= 0) {
              Serial.println("Fan on");
              outputFan = "on";
              // fan.updateSpeed(fan.fanChannel_, 255);
              // ledcWrite(fan.fanChannel_, 255);
            } else if (header.indexOf("GET /fan/off") >= 0) {
              Serial.println("Fan off");
              outputFan = "off";
              // ledcWrite(fan.fanChannel_, 0);
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
            client.println(String("<p>") + dht.readTemperature() + " C</p>");
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
            h = dht.readHumidity();
            hum = ((int)(h * 10)) / 10.0;
            client.println(String("<p>") + hum + " % </p>");
            client.println("<p> Helligkeit </p>");
            client.println(String("<p>") + light + " lx </p>");
            client.println("</body></html>");

            // Display current state, and ON/OFF buttons for GPIO 26
            // client.println("<p>LED" + outputLed + "</p>");
            client.println("<p>LED </p>");
            // If the output26State is off, it displays the ON button
            if (outputLed == "off") {
              client.println(
                  "<p><a href=\"/led/on\"><button "
                  "class=\"button\">ON</button></a></p>");
            } else {
              client.println(
                  "<p><a href=\"/led/off\"><button class=\"button "
                  "button2\">OFF</button></a></p>");
            }

            client.println("<p>Ventilator </p>");
            // If the output26State is off, it displays the ON button
            if (outputFan == "off") {
              client.println(
                  "<p><a href=\"/fan/on\"><button "
                  "class=\"button\">ON</button></a></p>");
            } else {
              client.println(
                  "<p><a href=\"/fan/off\"><button class=\"button "
                  "button2\">OFF</button></a></p>");
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
