#pragma once

#define USE_WLED
#include "wled.h"

#define MAX_REAL_LEDS      100


class LEDs {
  public:
    CRGB leds[MAX_REAL_LEDS];
    // CRGB led_array[MAX_REAL_LEDS];

    const static int FRAMES_PER_SECOND = 300;  // how often we refresh the strip, in frames per second
    const static int REFRESH_PERIOD = 1000 / FRAMES_PER_SECOND;  // how often we refresh the strip, in milliseconds
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

  void show() {
    CRGB *external_buffer = WS2812FX::get_external_buffer();
    for (int i = 0; i < num_leds; i++) {
      external_buffer[i] = leds[i];
    }
  }
  
  void update(bool reverse=false) {
    EVERY_N_MILLISECONDS( this->REFRESH_PERIOD ) {
      // Update the LEDs
      if (reverse)
        this->reverse();
      show();
      this->fps++;
    }

    EVERY_N_MILLISECONDS( 1000 ) {
      if (this->fps < (FRAMES_PER_SECOND - 30)) {
        Serial.print(this->fps);
        Serial.println((char *)F(" fps!"));
      }
      this->fps = 0;
    }
  }
};
