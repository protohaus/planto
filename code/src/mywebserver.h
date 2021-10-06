#include <NTPClient.h>        //Uhrzeitabfrage über WiFi
#include <WiFi.h>             //WiFi-Verbindung
#include <WiFiUdp.h>      
#include <planto_menu.h>

// Messwerte

float setuplight = 0.0;  // abgefragter Lichtwert -->umbennen
// Bool-Variablen als Flags für Fehlermeldungen

// Auxiliar variables to store the current output state
String outputFan = "off";

// Set web server port number to 80
WiFiServer server(80);
//Variable um zu gucken ob Webpage refreshed wurde

// Variable to store the HTTP request
String header;
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


//enum Klassen, um die Zustände zu erfassen 
enum class Ledzustand {
  an, 
  aus
} ; 
Ledzustand ledzustand = Ledzustand::aus; 

enum class Buttonzustand {
  an, 
  aus
} ; 
Buttonzustand LedButtonzustand = Buttonzustand::aus; 
Buttonzustand FanButtonzustand = Buttonzustand::aus; 



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

void webserverInit(){
    timeClient.begin();
}

void webserverLoop(){
    // Listen for incoming clients
    //WiFiClient client = server.available(); //kann sein, dass in main loop stehen muss?
    timeClient.update();
    // Abfrage der Uhrzeit (s.o. Winter- und Sommerzeit manuell einstellbat)
    // Serial.print(daysOfTheWeek[timeClient.getDay()]); (Wenn man Tag haben
    // möchte)
    //automatisches an/ausschalten des Lichtes
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
}

void aktualisierungWebserver(){
  WiFiClient client = server.available();
  //Code zur Aktualisierung durch den Webserver 
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

