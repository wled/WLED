#pragma once

#define USE_WLED
#include "wled.h"

#define MAX_REAL_LEDS      150


class LEDs {
  public:
    CRGB leds[MAX_REAL_LEDS];
    // CRGB led_array[MAX_REAL_LEDS];

    const static int TARGET_FRAMES_PER_SECOND = 150;
    const static int TARGET_REFRESH = 1000 / TARGET_FRAMES_PER_SECOND;
    int num_leds;

    uint16_t fps = 0;

  LEDs(int num=MAX_REAL_LEDS) : num_leds(num) { };
  
  void setup() {
    Serial.println((char *)F("LEDs: ok"));
  }

  void reverse() {
    for (int i=1; i<8; i++) {
      CRGB c = leds[i];
      leds[i] = leds[16-i];
      leds[16-i] = c;
    }
  }

  void update() {
    EVERY_N_MILLISECONDS( TARGET_REFRESH ) {
      fps++;
    }
    EVERY_N_MILLISECONDS( 1000 ) {
      if (fps < (TARGET_FRAMES_PER_SECOND - 30)) {
        Serial.print(fps);
        Serial.println((char *)F(" fps!"));
      }
      fps = 0;
    }
  }

  CRGB getPixelColor(uint8_t pos) const {
    if (pos > num_leds)
      return CRGB::Black;
    return leds[pos];
  }
};
