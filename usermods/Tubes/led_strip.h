#pragma once

#define USE_WLED
#include "wled.h"


class LEDs {
  public:
    const static int TARGET_FRAMES_PER_SECOND = 150;
    const static int TARGET_REFRESH = 1000 / TARGET_FRAMES_PER_SECOND;
    uint16_t fps = 0;

  LEDs() { };
  
  void setup() {
    Serial.println((char *)F("LEDs: ok"));
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
};
