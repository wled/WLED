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
    const PatternController& controller;
    uint32_t lastPhraseTime;
    uint32_t lastFrame;

  DebugController(const PatternController& c) : controller(c) {}
  
  void setup()
  {
    lastPhraseTime = globalTimer.now_micros;
    lastFrame = (uint32_t)-1;
  }

  void update()
  {
    EVERY_N_MILLISECONDS( 10000 ) {
      // Dump internal status
      auto knownSsid = apActive ? WiFi.softAPSSID() : WiFi.SSID();
      auto knownIp = apActive ? IPAddress(4, 3, 2, 1) : WiFi.localIP();
      Serial.printf("\n=== %s%s    WiFi[ch%d] %s IP: %u.%u.%u.%u   Free memory: %d  space: %u/%u  Uptime: %s\n",
        controller.node.node_name,
        controller.node.status_code(),
        WiFi.channel(),
        knownSsid.c_str(),
        knownIp[0],
        knownIp[1],
        knownIp[2],
        knownIp[3],
        freeMemory(),
        WLED_FS.usedBytes(),
        WLED_FS.totalBytes(),
        formatted_time(millis()).c_str()
      );

      Serial.printf("=== Controller: ");
      if (controller.isMasterRole()) {
        Serial.print("PRIMARY ");
      }
      if (controller.sound.active) {
        Serial.print("SOUND ");
      }
      if (controller.node.isLeading()) {
        Serial.print("LEADING ");
      }
      if (controller.node.isFollowing()) {
        Serial.print("FOLLLOWING ");
      }
      Serial.printf("role=%d power_save=%d\n",
        controller.role,
        controller.power_save
      );

      // Dump WLED status
      char mode_name[50];
      char palette_name[50];
      auto seg = strip.getMainSegment();
      extractModeName(seg.mode, JSON_mode_names, mode_name, 50);
      extractModeName(seg.palette, JSON_palette_names, palette_name, 50);
      Serial.printf("=== WLED: %s(%u) %s(%u) speed:%u intensity:%u",
        mode_name,
        seg.mode,
        palette_name,
        seg.palette,
        seg.speed,
        seg.intensity
      );
      if (controller.patternOverride) {
        Serial.printf(" (PATTERN %d)", controller.patternOverride);
      } else {
        Serial.printf(" at %d", controller.wled_fader);
      }
      if (controller.paletteOverride) {
        Serial.printf(" (PALETTE %d)", controller.paletteOverride);
      }
      Serial.println();

      Serial.printf("=== firmware: v%d "
#ifdef TUBES_AUTOUPDATER
        "from SSID %s %u.%u.%u.%u "
#endif
        "OTA=%d\n\n",
        controller.updater.current_version.version,
#ifdef TUBES_AUTOUPDATER
        controller.updater.current_version.ssid,
        controller.updater.current_version.host[0],
        controller.updater.current_version.ssid[1],
        controller.updater.current_version.ssid[2],
        controller.updater.current_version.ssid[3],
#endif
        controller.updater.status
      );

    }
  }

  void handleOverlayDraw() 
  {
    // Show the beat on the master OR if debugging
    if (controller.options.debugging) {
      uint16_t num_leds = strip.getLengthTotal();

      uint8_t p1 = (controller.current_state.beat_frame >> 8) % 16;
      strip.setPixelColor(p1, CRGB::White);

      uint8_t p2 = scale8(controller.node.header.id>>4, num_leds-1);
      strip.setPixelColor(p2, CRGB::Yellow);

      uint8_t p3 = scale8(controller.node.header.uplinkId>>4, num_leds-1);
      if (p3 == p2) {
        strip.setPixelColor(p3, CRGB::Green);
      } else {
        strip.setPixelColor(p3, CRGB::Blue);
      }
    }
    
  }
};

