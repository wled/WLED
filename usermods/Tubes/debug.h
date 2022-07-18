#pragma once

#include "controller.h"
#include "radio.h"


class DebugController {
  public:
    PatternController *controller;
    LEDs *strip;
    Radio *radio;
    uint32_t lastPhraseTime;
    uint32_t lastFrame;

  DebugController(PatternController *controller) {
    this->controller = controller;
    this->strip = controller->led_strip;
    this->radio = controller->radio;
  }
  
  void setup()
  {
    this->lastPhraseTime = globalTimer.now_micros;
    this->lastFrame = (uint32_t)-1;
  }

  void update()
  {
    EVERY_N_MILLISECONDS( 10000 ) {
      Serial.printf("IP: %u.%u.%u.%u   ",
        WiFi.localIP()[0],
        WiFi.localIP()[1],
        WiFi.localIP()[2],
        WiFi.localIP()[3]
      );
      Serial.print(F("Free memory: "));
      Serial.println( freeMemory() );
    }

    // Show the beat on the master OR if debugging

    if (this->controller->options.debugging) {
      uint8_t p1 = (this->controller->current_state.beat_frame >> 8) % 16;
      this->strip->leds[p1] = CRGB::White;

      uint8_t p2 = scale8(this->controller->radio->tubeId, this->strip->num_leds-1);
      this->strip->leds[p2] = CRGB::White;

      uint8_t p3 = scale8(this->controller->radio->uplinkTubeId, this->strip->num_leds-1);
      if (p3 == p2) {
        this->strip->leds[p3] = CRGB::Green;
      } else {
        this->strip->leds[p3] = CRGB::Yellow; 
      }
      if (this->radio->radioRestarts) {
        this->strip->leds[1] = CRGB::Red;
      }
    }
    
    if (this->radio->radioFailures && !this->radio->radioRestarts) {
      this->strip->leds[0] = CRGB::Red;
    }
  }
};
