#pragma once
#include "wled.h"

class Usermod_RotaryRackDimmer : public Usermod {
  public:
    void setup() override {
      Serial.println("RotaryRackDimmer setup klaar!");
    }

    void loop() override {
      // Simuleer helderheid verhogen (test!)
      if (millis() % 5000 < 50) {
        bri = (bri + 10) % 255;
        applyBri();
        Serial.printf("Helderheid aangepast naar: %d\n", bri);
      }
    }
};
