#pragma once

#include "wled.h"

class Usermod_RotaryRackDimmer : public Usermod {
private:
  int pinA = 33;  // A (CLK) pin van de encoder
  int pinB = 13;  // B (DT) pin van de encoder
  int lastState = 0;

public:
  void setup() override {
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
    lastState = digitalRead(pinA);
  }

  void loop() override {
    int currentState = digitalRead(pinA);
    if (currentState != lastState) {
      if (digitalRead(pinB) != currentState) {
        // Rechtsom → dim omhoog
        bri = min(255, bri + 5);
      } else {
        // Linksom → dim omlaag
        bri = max(0, bri - 5);
      }
      lastState = currentState;
      colorUpdated(CALL_MODE_DIRECT_CHANGE);
    }
  }

  void addToConfig(JsonObject &root) override {}
  bool readFromConfig(JsonObject &root) override { return true; }

  String getId() override { return F("RotaryRackDimmer"); }
};
