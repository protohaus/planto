/*Klasse, die die Methoden des Fans enthält 
*/
#include "fan.h"

#include <Arduino.h>

namespace planto {
//Konstruktor
Fan::Fan(int pinPWM, /*int pinTacho,*/ int channel)
    : dutyCycleFan_(0),
      zustand(Zustand::aus), 
      pinPWM_(pinPWM),
      // pinTacho_(pinTacho),
      fanFreq_(25000),
      fanResolution_(8),
      fanChannel_(1) {}

//Methode zur Anpassung der Geschwindigkeit 
void Fan::updateSpeed() {
  ledcWrite(fanChannel_, dutyCycleFan_);
}

//Methode zur Initialisierung des Fans 
void Fan::init() {
  pinMode(pinPWM_, OUTPUT);

  ledcSetup(fanChannel_, fanFreq_, fanResolution_);

  ledcAttachPin(pinPWM_, fanChannel_);
}

}  // namespace planto