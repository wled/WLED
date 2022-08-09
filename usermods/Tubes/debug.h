#pragma once

#include "controller.h"
#include "node.h"

std::string formatted_time(long ms) {
  long secs = ms / 1000; // set the seconds remaining
  long mins = secs / 60; //convert seconds to minutes
  long hours = mins / 60; //convert minutes to hours
  long days = hours / 24; //convert hours to days

  secs = secs % 60;
  mins = mins % 60;
  hours = hours % 24;

  char buffer[100];
  if (days > 0)
    sprintf(buffer, "%ld %02ld:%02ld:%02ld", days, hours, mins, secs);
  else
    sprintf(buffer, "%02ld:%02ld:%02ld", hours, mins, secs);
  return std::string(buffer);
}

class DebugController {
  public:
    PatternController *controller;
    LEDs *strip;
    LightNode *node;
    uint32_t lastPhraseTime;
    uint32_t lastFrame;

  DebugController(PatternController *controller) {
    this->controller = controller;
    this->strip = controller->led_strip;
    this->node = controller->node;
  }
  
  void setup()
  {
    this->lastPhraseTime = globalTimer.now_micros;
    this->lastFrame = (uint32_t)-1;
  }

  std::string status_code(NodeStatus status) {
    switch (status) {
      case NODE_STATUS_QUIET:
        return std::string(" (quiet)");
      case NODE_STATUS_STARTING:
        return std::string(" (starting)");
      case NODE_STATUS_STARTED:
        return std::string("");
      default:
        return std::string("??");
    }
  }

  void update()
  {
    EVERY_N_MILLISECONDS( 10000 ) {
      Serial.printf("\n=== %s%s    WiFi %d[ch%d] IP: %u.%u.%u.%u   Free memory: %d    Uptime: %s\n\n",
        this->controller->node->node_name,
        status_code(this->controller->node->status).c_str(),
        WiFi.status(),
        WiFi.channel(),
        WiFi.localIP()[0],
        WiFi.localIP()[1],
        WiFi.localIP()[2],
        WiFi.localIP()[3],
        freeMemory(),
        formatted_time(millis()).c_str()
      );
    }

    // Show the beat on the master OR if debugging

    if (this->controller->options.debugging) {
      uint8_t p1 = (this->controller->current_state.beat_frame >> 8) % 16;
      this->strip->leds[p1] = CRGB::White;

      uint8_t p2 = scale8(this->controller->node->header.id, this->strip->num_leds-1);
      this->strip->leds[p2] = CRGB::White;

      uint8_t p3 = scale8(this->controller->node->header.uplinkId, this->strip->num_leds-1);
      if (p3 == p2) {
        this->strip->leds[p3] = CRGB::Green;
      } else {
        this->strip->leds[p3] = CRGB::Yellow; 
      }
    }
    
  }
};

