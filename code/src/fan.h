#pragma once

#include <Arduino.h>

namespace planto {
class Fan {
 public:
  Fan(int pinPWM = 27, /*int pinTacho = 26,*/ int channel = 1);
  ~Fan() = default;

  // void setSpeed(float percent);
  void updateSpeed();
  void init();

  int dutyCycleFan_;
  
  enum class Zustand {
    an, 
    aus
  } ; 
  Zustand zustand; 

 private:
  int pinPWM_;
  //int pinTacho_;
  int fanFreq_;
  int fanResolution_;
  int fanChannel_;
};
}  // namespace planto
