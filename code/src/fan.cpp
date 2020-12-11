#include <Arduino.h>
#include "fan.h"

namespace planto {
Fan::Fan(int pinPWM, /*int pinTacho,*/ int channel)
    : dutyCycleFan_(0),
      pinPWM_(pinPWM),
      //pinTacho_(pinTacho),
      fanFreq_(25000),
      fanResolution_(8),
      fanChannel_(1) {}

void Fan::updateSpeed() { 
      ledcWrite(fanChannel_, dutyCycleFan_); 
      Serial.println("in updateSpeed");
      Serial.println(dutyCycleFan_);
      }

void Fan::init() {
  pinMode(pinPWM_, OUTPUT);
  //pinMode(pinTacho_, INPUT);

  ledcSetup(fanChannel_, fanFreq_, fanResolution_);

  ledcAttachPin(pinPWM_, fanChannel_);
}

}  // namespace planto