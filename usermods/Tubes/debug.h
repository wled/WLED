#pragma once

#include "controller.h"
#include "node.h"
#include "wled.h"

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
    LEDs *led_strip;
    LightNode *node;
    uint32_t lastPhraseTime;
    uint32_t lastFrame;

  DebugController(PatternController *controller) {
    this->controller = controller;
    this->led_strip = controller->led_strip;
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
      // Dump internal status
      auto knownSsid = apActive ? WiFi.softAPSSID() : WiFi.SSID();
      auto knownIp = apActive ? IPAddress(4, 3, 2, 1) : WiFi.localIP();
      Serial.printf("\n=== %s%s    WiFi[ch%d] %s IP: %u.%u.%u.%u   Free memory: %d  space: %u/%u  Uptime: %s\n",
        this->controller->node->node_name,
        status_code(this->controller->node->status).c_str(),
        WiFi.channel(),
        knownSsid.c_str(),
        knownIp[0],
        knownIp[1],
        knownIp[2],
        knownIp[3],
        freeMemory(),
        LITTLEFS.usedBytes(),
        LITTLEFS.totalBytes(),
        formatted_time(millis()).c_str()
      );

      // Dump WLED status
      char mode_name[50];
      char palette_name[50];
      auto seg = strip.getMainSegment();
      extractModeName(seg.mode, JSON_mode_names, mode_name, 50);
      extractModeName(seg.palette, JSON_palette_names, palette_name, 50);
      Serial.printf("=== WLED: %s(%u) %s(%u) speed:%u intensity:%u  at %d\n",
        mode_name,
        seg.mode,
        palette_name,
        seg.palette,
        seg.speed,
        seg.intensity,
        this->controller->wled_fader
      );

      Serial.printf("=== OTA: v%d state %d  SSID %s  %u.%u.%u.%u \n\n",
        this->controller->auto_updater.location.version,
        this->controller->auto_updater.status,
        this->controller->auto_updater.location.ssid,
        this->controller->auto_updater.location.host[0],
        this->controller->auto_updater.location.ssid[1],
        this->controller->auto_updater.location.ssid[2],
        this->controller->auto_updater.location.ssid[3]
      );

    }
  }

  void handleOverlayDraw() 
  {
    // Show the beat on the master OR if debugging
    if (this->controller->options.debugging) {
      uint16_t num_leds = strip.getLengthTotal();

      uint8_t p1 = (this->controller->current_state.beat_frame >> 8) % 16;
      strip.setPixelColor(p1, CRGB::White);

      uint8_t p2 = scale8(this->controller->node->header.id>>4, num_leds-1);
      strip.setPixelColor(p2, CRGB::Yellow);

      uint8_t p3 = scale8(this->controller->node->header.uplinkId>>4, num_leds-1);
      if (p3 == p2) {
        strip.setPixelColor(p3, CRGB::Green);
      } else {
        strip.setPixelColor(p3, CRGB::Blue);
      }
    }
    
  }
};

