#pragma once
#include "wled.h"

class Usermod_RotaryRackDimmer : public Usermod {
private:
  int pinA = 33;  // aanpassen naar jouw GPIO
  int pinB = 13;

  int lastState = 0;

public:
  void setup() override;
  void loop() override;
};
