#pragma once

#include "wled.h"


const int fish[6][9] = {
    {0, 0, 1, 1, 1, 0, 0, 0, 1},
    {0, 1, 1, 1, 1, 1, 0, 1, 1},
    {1, 1, 2, 1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 0, 1, 1},
    {0, 0, 1, 1, 1, 0, 0, 0, 1},    
};

// Fish mode
uint16_t mode_fish() {
 

  return FRAMETIME;
}

// Effect data

static const char _data_FX_MODE_FISH[] PROGMEM = "Fish@Speed;;1;1";

// Usermod class
class UsermodFish : public Usermod {
  public:
    void setup() {
      Serial.println("Fish mod active!");
      // Register new effect in a free slot (example: 253)
      strip.addEffect(FX_MODE_FISH, &mode_fish, _data_FX_MODE_FISH);
    }
    void loop() {}
    uint16_t getId() { return USERMOD_ID_FISH; }
};