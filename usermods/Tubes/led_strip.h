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

  LEDs(int num_leds=MAX_REAL_LEDS) {
    this->num_leds = num_leds;
  }
  
  void setup() {
    Serial.println((char *)F("LEDs: ok"));
  }

  void reverse() {
    for (int i=1; i<8; i++) {
      CRGB c = this->leds[i];
      this->leds[i] = this->leds[16-i];
      this->leds[16-i] = c;
    }
  }

  void update(bool reverse=false) {
    EVERY_N_MILLISECONDS( this->TARGET_REFRESH ) {
      this->fps++;
    }
    EVERY_N_MILLISECONDS( 1000 ) {
      if (this->fps < (TARGET_FRAMES_PER_SECOND - 30)) {
        Serial.print(this->fps);
        Serial.println((char *)F(" fps!"));
      }
      this->fps = 0;
    }
  }

  CRGB getPixelColor(uint8_t pos) {
    if (pos > this->num_leds)
      return CRGB::Black;
    return this->leds[pos];
  }
};
