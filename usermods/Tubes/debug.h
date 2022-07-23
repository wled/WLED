#pragma once

#include "controller.h"
#include "bluetooth.h"

class DebugController {
  public:
    PatternController *controller;
    LEDs *strip;
    BLEMeshNode *mesh;
    uint32_t lastPhraseTime;
    uint32_t lastFrame;

  DebugController(PatternController *controller) {
    this->controller = controller;
    this->strip = controller->led_strip;
    this->mesh = controller->mesh;
  }
  
  void setup()
  {
    this->lastPhraseTime = globalTimer.now_micros;
    this->lastFrame = (uint32_t)-1;
  }

  void update()
  {
    EVERY_N_MILLISECONDS( 10000 ) {
      Serial.printf("%s    IP: %u.%u.%u.%u   Free memory: %d\n",
        this->controller->mesh->node_name,
        WiFi.localIP()[0],
        WiFi.localIP()[1],
        WiFi.localIP()[2],
        WiFi.localIP()[3],
        freeMemory()
      );
    }

    // Show the beat on the master OR if debugging

    if (this->controller->options.debugging) {
      uint8_t p1 = (this->controller->current_state.beat_frame >> 8) % 16;
      this->strip->leds[p1] = CRGB::White;

      uint8_t p2 = scale8(this->controller->mesh->ids.id, this->strip->num_leds-1);
      this->strip->leds[p2] = CRGB::White;

      uint8_t p3 = scale8(this->controller->mesh->ids.uplinkId, this->strip->num_leds-1);
      if (p3 == p2) {
        this->strip->leds[p3] = CRGB::Green;
      } else {
        this->strip->leds[p3] = CRGB::Yellow; 
      }
    }
    
  }
};
