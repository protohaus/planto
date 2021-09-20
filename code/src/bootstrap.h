/*Klasse zur Verbindung mit dem lokalen WIFi ohne, dass das Passwort manuel über den COode aktualisiert werden muss
Einfach mit dem Hotspot des Esp32 über wlan verbinden, in den Browser gehen und http:/10.0.0.0 eingeben, dann das passende Wlan und Passwort eingeben, 
damit sich der Esp32 mit dem lokalen Wlan verbindet. 
Der ESP32 verbindet sich anschließend mit dem richtigen wlan und funktioniert. Er kann dann über den Webserver gesteuert werden 
*/

#include <Arduino.h>
#include <functional> 

#define _DEBUG_

#ifdef _DEBUG_
#define _PP(a) Serial.print(a);
#define _PL(a) Serial.println(a);
#else
#define _PP(a)
#define _PL(a)
#endif
/*
   Copyright (c) 2020, Anatoli Arkhipenko
   All rights reserved.
   This is example #1 without comments or debug messages
*/

#define CTOKEN  "EBS1B"


#include <ParametersEEPROMMap.h>  // Parameters stored in EEPROM 
#include <EspBootstrapMap.h>      // ESP Bootstrap for memory structure
#include <JsonConfigHttpMap.h>    // Json parsing from HTTP host into memory structure

const String TOKEN(CTOKEN);
const int NPARS = 7;

typedef struct {
  char token[6];
  char ssid[32];
  char pwd[32];
  char cfg_url[128];
  char ota_host[32];
  char ota_port[6];
  char ota_url[32];
} Params;
Params eg;

//enum Klasse, für Displayausgabe während Bootstrapping 
enum class Bootstrapzustand {
  lookingForWifi,  
  wifiConnctedWith
} ; 
Bootstrapzustand bootstrapzustand = Bootstrapzustand::lookingForWifi; 


Params defaults = { CTOKEN,
                    "<wifi ssid>",
                    "<wifi password>",
                    "http://raw.githubusercontent.com/arkhipenko/EspBootstrap/master/examples/EBS_example01_ParametersEEPROMMap/config.json",
                    "<ota.server.com>",
                    "1234",
                    "/esp/ota.php"
                  };

char* PARS[] = { eg.ssid,
                 eg.pwd,
                 eg.cfg_url,
                 eg.ota_host,
                 eg.ota_port,
                 eg.ota_url
               };

const int NPARS_BTS = 3;
const char* PAGE[] = { "EspBootstrap",
                       "WiFi SSID",
                       "WiFi PWD",
                       "Config URL",
                     };




void printConfig() {
  _PL();
  _PL("Config dump");
  _PP("\ttoken    : "); _PL(eg.token);
  _PP("\tssid     : "); _PL(eg.ssid);
  _PP("\tpasswd   : "); _PL(eg.pwd);
  _PP("\tconfig   : "); _PL(eg.cfg_url);
  _PP("\tota host : "); _PL(eg.ota_host);
  _PP("\tota port : "); _PL(eg.ota_port);
  _PP("\tota url  : "); _PL(eg.ota_url);
  _PL();
}

void setupWifi(const char* ssid, const char* pwd) {
  _PP("Setup_wifi()");
  _PL("Connecting to WiFi...");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pwd);
}

bool waitForWifi(unsigned long aTimeout) {
  _PL("WaitForWifi()");
  Serial.println("WaitforWifi()"); 
  unsigned long timeNow = millis();

  while ( WiFi.status() != WL_CONNECTED ) {
    delay(1000);
    if ( millis() - timeNow > aTimeout ) {
      Serial.println(" WiFi connection timeout");
      return true;
    }
  }

  _PP(" WiFi connected");
  _PL("IP address: "); _PL(WiFi.localIP());
  _PP("SSID: ");_PL(WiFi.SSID());
  _PP("mac: "); _PL(WiFi.macAddress());
  bootstrapzustand = Bootstrapzustand::wifiConnctedWith; 
  return false;
}

/*Hilfsmethode, um den EProm manuel zu leeren, bei Problemen mit dem Bootstrapping 
*/
void cleaneprom(){
  //idf.py erase_flash
  EEPROM.begin(512);
  // write a 0 to all 512 bytes of the EEPROM
  for (int i = 0; i < 512; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.end();
}
/*void printBootstrap(){
  bootstr::bootingservice.print_bootstrapping_(bootstrapzustand); 
}*/


void setupbootstrap(void) {
  bool wifiTimeout;
  int rc;

  
  #ifdef _DEBUG_
  Serial.begin(115200);
  delay(3000);
  {
    _PL("EspBootStrap Example"); _PL();
  #if defined( ARDUINO_ARCH_ESP8266 )
    String id = "ESP8266-" + String(ESP.getChipId(), HEX);
  #endif
  #if defined( ARDUINO_ARCH_ESP32 )
    String id = "ESP32-" + String((uint32_t)( ESP.getEfuseMac() & 0xFFFFFFFFL ), HEX);
  #endif
    _PP("ESP Chip ID: "); _PL(id);
    _PP("Parameter structure size: "); _PL( sizeof(Params) );
    _PL();
  }
  #endif

  //cleaneprom(); 
  
  ParametersEEPROMMap *p_ptr = new ParametersEEPROMMap(TOKEN, &eg, &defaults, 0, sizeof(Params));
  ParametersEEPROMMap& p = *p_ptr;
  
  rc = p.begin();
  
  _PP(": EspBootStrap ParametersEEPROMMap initialized. rc = "); _PL(rc);
  rc = p.load();
  _PP("Configuration loaded. rc = "); _PL(rc);

  

  if (rc == PARAMS_OK) {
    _PP("Connecting to WiFi for 30 sec:");
    setupWifi(eg.ssid, eg.pwd);
    wifiTimeout = waitForWifi(60 * BOOTSTRAP_SECOND);
  }

  if (rc != PARAMS_OK || wifiTimeout) {
    _PP("Bootstrapping...");
    
    rc = ESPBootstrap.run(PAGE, PARS, NPARS_BTS, 3 * BOOTSTRAP_MINUTE);
    if (rc == BOOTSTRAP_OK) {
      p.save();
      _PL("Bootstrapped OK. Rebooting.");
      bootstrapzustand = Bootstrapzustand::wifiConnctedWith; 
    }
    _PP("Bootstrap timed out. Rebooting.");
    bootstrapzustand = Bootstrapzustand::lookingForWifi; 
    delay(1000);
    
  }
  
  rc = JSONConfig.parse(eg.cfg_url, PARS, NPARS - 1);
  _PP("JSONConfig finished. rc = "); _PL(rc);
  if (rc == 0) p.save();
  
}

