#pragma once

#include "wled.h"
#include <U8g2lib.h>
#include <Wire.h>

#define OLED_SDA 5
#define OLED_SCL 6
#define LED_PIN  8

class UsermodOLED72x40 : public Usermod {
  private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C* u8g2 = nullptr;
    bool enabled = true;
    bool initDone = false;
    unsigned long lastUpdate = 0;
    
    // Final calibrated offsets for AlexYeryomin 0.42" OLED
    int xOff = 28; 
    int yOff = 24;

    // Visualizer / UI state
    byte vValues[72]; 
    unsigned long screenTimeoutMS = 60000; // Turn off after 60s
    unsigned long lastInteraction = 0;
    bool displayOff = false;

  public:
    void setup() {
      pinMode(LED_PIN, OUTPUT); // Built-in LED
      Wire.begin(OLED_SDA, OLED_SCL);
      u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
      
      if (u8g2->begin()) {
        u8g2->setContrast(255);
        memset(vValues, 0, sizeof(vValues));
        initDone = true;
      }
    }

    void loop() {
      // Hardware Blink Heartbeat
      static unsigned long ledTimer = 0;
      if (millis() - ledTimer > 500) {
        ledTimer = millis();
        digitalWrite(8, !digitalRead(8));
      }

      if (!enabled || !initDone || strip.isUpdating()) return;

      // Burn-in protection: Sleep screen after inactivity
      if (millis() - lastInteraction > screenTimeoutMS) {
        if (!displayOff) {
          u8g2->setPowerSave(1);
          displayOff = true;
        }
        return;
      }

      if (millis() - lastUpdate > 100) {
        lastUpdate = millis();
        
        // Update Sparkline
        for (int i = 0; i < 71; i++) vValues[i] = vValues[i+1];
        vValues[71] = map(bri, 0, 255, 0, 15);

        if (displayOff) {
          u8g2->setPowerSave(0);
          displayOff = false;
        }

        u8g2->clearBuffer();
        
        // 1. Top Bar: Mode Name (using calibrated offsets)
        u8g2->setFont(u8g2_font_5x7_tf);
        char lineBuffer[17];
        extractModeName(effectCurrent, JSON_mode_names, lineBuffer, 16);
        u8g2->drawStr(xOff, yOff + 7, lineBuffer);
        u8g2->drawHLine(xOff, yOff + 9, 72);

        // 2. Middle: Visualizer
        for (int i = 0; i < 71; i++) {
          u8g2->drawLine(xOff + i, yOff + 26, xOff + i, yOff + 26 - vValues[i]);
        }

        // 3. Bottom: IP & Bri
        u8g2->setFont(u8g2_font_4x6_tf);
        u8g2->setCursor(xOff, yOff + 38);
        if (Network.isConnected()) {
          u8g2->print(Network.localIP()[3]); // Just last octet to save space
          u8g2->print(" | ");
        }
        u8g2->print(map(bri, 0, 255, 0, 100));
        u8g2->print("%");
        
        // Sync indicator
        if (receiveNotificationBrightness || receiveNotificationColor) {
           u8g2->drawStr(xOff + 66, yOff + 38, "S");
        }

        u8g2->sendBuffer();
      }
    }

    // Reset sleep timer when WLED state changes
    void onStateChange(uint8_t mode) {
      lastInteraction = millis();
    }

    void addToConfig(JsonObject& root) {
      JsonObject top = root.createNestedObject(F("OLED_72x40"));
      top[F("enabled")] = enabled;
      top["pin"] = 7;           // Force Button 2 to GPIO 7 (example)
      top["help"] = "Button 2 wakes & cycles display";
      top["x-offset"] = 28;      // Your calibrated X
      top["y-offset"] = 24;      // Your calibrated Y
      top["sleepTimeout"] = 30;  // 30 seconds default
    }

    bool readFromConfig(JsonObject& root) {
      JsonObject top = root[F("OLED_72x40")];
      enabled = top[F("enabled")] | enabled;
      xOff = top["x-offset"] | xOff;
      yOff = top["y-offset"] | yOff;
      screenTimeoutMS = (top["sleepTimeout"] | (screenTimeoutMS / 1000)) * 1000;
      return !top.isNull();
    }

    // void handleButton(uint8_t b) {
    //   if (b == 1) { // Index 1 is the second button
    //     // 2. If screen was off, wake it up
    //     if (displayOff) {
    //       u8g2->setPowerSave(0);
    //       displayOff = false;
    //     }
    // }
  // }

    uint16_t getId() { return USERMOD_ID_OLED_72x40; }
};