#pragma once

#include "wled.h"

#define PALETTE_SOLID_WRAP   (strip.paletteBlend == 1 || strip.paletteBlend == 3)

// Minimal data struct for hearth effect
typedef struct {
  uint32_t lastTime;
} HearthData;

// 16x16 heart data: 1 = bright red, 2 = dimmer red
static const int heart[16][16] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 2, 2, 2, 2, 0, 0, 2, 2, 2, 2, 0, 0, 0},
  {0, 0, 2, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1, 2, 0, 0},
  {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
  {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
  {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
  {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
  {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
  {0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
  {0, 0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0},
  {0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0},
  {0, 0, 0, 0, 2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 2, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0}
};

// Simple pulsing heart effect
uint16_t mode_hearth() {
  if (!SEGENV.allocateData(sizeof(HearthData))) return FRAMETIME;
  HearthData* data = reinterpret_cast<HearthData*>(SEGENV.data);

  // Initialize step/time counters on first call
  if (SEGENV.call == 0) {
    data->lastTime = strip.now;
    SEGENV.step = strip.now;
    SEGENV.aux0 = 0;   
    SEGENV.aux1 = UINT16_MAX; 
  }

  // Calculate BPM from segment speed
  unsigned bpm = 40 + (SEGMENT.speed >> 3);
  uint32_t msPerBeat = 60000L / bpm;  
  uint32_t secondBeat = msPerBeat / 3;  
  uint32_t bri_lower = SEGENV.aux1;  
  unsigned long beatTimer = strip.now - SEGENV.step;

  // Gradually lower brightness
  bri_lower = bri_lower * 2042 / (2048 + SEGMENT.intensity);
  SEGENV.aux1 = bri_lower;

  // Check for second smaller beat
  if ((beatTimer > secondBeat) && !SEGENV.aux0) {
    SEGENV.aux1 = UINT16_MAX;  
    SEGENV.aux0 = 1;           
  }
  // Full beat reset
  if (beatTimer > msPerBeat) {
    SEGENV.aux1 = UINT16_MAX;  
    SEGENV.aux0 = 0;
    SEGENV.step = strip.now;   
  }

  // If not a 2D segment, apply the pulsing brightness to every pixel
  if (!SEGMENT.is2D()) {
    for (int i = 0; i < SEGLEN; i++) {
      SEGMENT.setPixelColor(
        i, 
        color_blend(
          SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0),
          SEGCOLOR(1),
          255 - (SEGENV.aux1 >> 8)
        )
      );
    }
    return FRAMETIME;
  }

  // If 2D, overlay the heart pattern for 1's and 2's only
  int rows = SEGMENT.virtualHeight();
  int cols = SEGMENT.virtualWidth();
  uint8_t blendAmount = 255 - (SEGENV.aux1 >> 8);

  for (int y = 0; y < rows && y < 16; y++) {
    for (int x = 0; x < cols && x < 16; x++) {
      int val = heart[y][x];
      if (val == 0) {
        // No heart pixel here, set to black
        SEGMENT.setPixelColorXY(x, y, BLACK);
      } else if (val == 1) {
        // Bright red with pulse
        SEGMENT.setPixelColorXY(
          x, y,
          color_blend(
            SEGMENT.color_from_palette(XY(x, y), true, PALETTE_SOLID_WRAP, 0),
            SEGCOLOR(1),
            blendAmount
          )
        );
      } else if (val == 2) {
        // Dimmer red with pulse
        SEGMENT.setPixelColorXY(
          x, y,
          color_blend(
            SEGMENT.color_from_palette(XY(x, y), true, PALETTE_SOLID_WRAP, 0),
            SEGCOLOR(1),
            blendAmount / 2
          )
        );
      }
    }
  }

  return FRAMETIME;
}

// Effect data

static const char _data_FX_MODE_HEART[] PROGMEM = "Heart@!,!;!,!;!;01;1";//"Heart@Speed;;1;1";

// Usermod class
class UsermodHearth : public Usermod {
public:
  void setup() {
    // Register new effect in a free slot (example: 253)
    strip.addEffect(FX_MODE_HEART, &mode_hearth, _data_FX_MODE_HEART);
  }
  void loop() {}
  uint16_t getId() { return USERMOD_ID_HEART; }
};