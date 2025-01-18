#pragma once

#include "wled.h"

#define XY(x,y) SEGMENT.XY(x,y)

// Minimal data struct for hearth effect
typedef struct {
  uint32_t lastTime;
} HearthData;

// 16x16 heart data: 1 = bright red, 2 = dimmer red
static const int heart[16][16] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 2, 2, 2, 2, 0, 0, 2, 2, 2, 2, 0, 0, 0},
  {0, 0, 2, 2, 1, 1, 2, 2, 2, 2, 1, 1, 2, 2, 0, 0},
  {0, 2, 2, 1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 2, 2, 0},
  {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
  {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
  {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
  {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
  {0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 0},
  {0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 0, 0},
  {0, 0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 0, 0, 0},
  {0, 0, 0, 0, 2, 2, 1, 1, 1, 1, 2, 2, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 2, 2, 1, 1, 2, 2, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

// Simple pulsing heart effect
uint16_t mode_hearth(void) {
  if (!SEGENV.allocateData(sizeof(HearthData))) return FRAMETIME;
  HearthData* data = reinterpret_cast<HearthData*>(SEGENV.data);

  if (SEGENV.call == 0) {
    data->lastTime = millis();
  }

  // Only valid in 2D
  if (!SEGMENT.is2D()) {
    SEGMENT.fill(BLACK);
    return FRAMETIME;
  }

  int rows = SEGMENT.virtualHeight();
  int cols = SEGMENT.virtualWidth();

  // Heartbeat timing
  const float pulsePeriod = 1000.0f; // 1 second for a full cycle
  float timeNow = (millis() % (uint32_t)pulsePeriod) / pulsePeriod; // 0..1
  float pulse   = 0.5f + 0.5f * sinf(2.0f * 3.14159f * timeNow); // pulsing factor

  // Define two shades of red
  // 1 = bright red, scaled by pulse
  // 2 = dim red, scaled by pulse
  
  for (int y = 0; y < rows && y < 16; y++) {
    for (int x = 0; x < cols && x < 16; x++) {
      int val = heart[y][x];
      if (val == 1) {
        uint8_t r = (uint8_t)(255 * pulse);
        SEGMENT.setPixelColorXY(x, y, RGBW32(r, 0, 0, 0));
      } else if (val == 2) {
        uint8_t r = (uint8_t)(100 * pulse);
        SEGMENT.setPixelColorXY(x, y, RGBW32(r, 0, 0, 0));
      } else {
        SEGMENT.setPixelColorXY(x, y, BLACK);
      }
    }
  }

  return FRAMETIME;
}

// Effect data
static const char _data_FX_MODE_HEARTH[] PROGMEM = "Red Hearth@Speed;;1;2";

// Usermod class
class UsermodHearth : public Usermod {
public:
  void setup() {
    // Register new effect in a free slot (example: 253)
    strip.addEffect(255, &mode_hearth, _data_FX_MODE_HEARTH);
  }
  void loop() {}
  uint16_t getId() { return USERMOD_ID_RESERVED; }
};