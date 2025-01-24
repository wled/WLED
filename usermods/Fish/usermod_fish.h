#pragma once

#include "wled.h"

#define PALETTE_SOLID_WRAP   (strip.paletteBlend == 1 || strip.paletteBlend == 3)

// Minimal data struct for hearth effect
typedef struct {
  uint32_t lastTime;
} FishData;



#define XY(x,y) SEGMENT.XY(x,y)
const int fish[6][9] = {
    {0, 0, 1, 1, 1, 0, 0, 0, 1},
    {0, 1, 1, 1, 1, 1, 0, 1, 1},
    {1, 1, 2, 1, 1, 1, 1, 1, 0},
    {1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 0, 1, 1},
    {0, 0, 1, 1, 1, 0, 0, 0, 1},    
};

// Simple pulsing heart effect
uint16_t mode_fish() {
 

  return FRAMETIME;
}

// Effect data

static const char _data_FX_MODE_FISH[] PROGMEM = "Fish@!,!;!,!;!;01;1";

// Usermod class
class UsermodFish : public Usermod {
public:
  void setup() {
    // Register new effect in a free slot (example: 253)
    strip.addEffect(FX_MODE_FISH, &mode_fish, _data_FX_MODE_FISH);
  }
  void loop() {}
  uint16_t getId() { return USERMOD_ID_FISH; }
};