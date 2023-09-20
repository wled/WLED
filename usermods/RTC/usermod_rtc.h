#pragma once

#include <Arduino.h>   // WLEDMM: make sure that I2C drivers have the "right" Wire Object
#include <Wire.h>

#include "src/dependencies/time/DS1307RTC.h"
#include "wled.h"

#define RTC_DELTA 2   // only modify RTC time if delta exceeds this number of seconds

//Connect DS1307 or DS3231 to standard I2C pins (ESP32: GPIO 21 (SDA)/GPIO 22 (SCL))

class RTCUsermod : public Usermod {
  private:
    unsigned long lastTime = 0;
    bool RTCfound = false;      // WLEDMM to prevent errors after anabling the usermod (Wire.cpp:526] write(): NULL TX buffer pointer)
  public:

    RTCUsermod(const char *name, bool enabled):Usermod(name, enabled) {} //WLEDMM: this shouldn't be necessary (passthrough of constructor), maybe because Usermod is an abstract class

    void setup() {
      RTCfound = false;   // WLEDMM
      PinManagerPinType pins[2] = { { i2c_scl, true }, { i2c_sda, true } };
      if (pins[1].pin < 0 || pins[0].pin < 0)  { enabled=false; return; }  //WLEDMM bugfix - ensure that "final" GPIO are valid and no "-1" sneaks trough
      if (!enabled) { DEBUG_PRINTLN(F("RTC usermod not enabled.")); return; }

      // WLEDMM join hardware I2C
      if (!pinManager.joinWire()) {  // WLEDMM - this allocates global I2C pins, then starts Wire - if not started previously
        enabled = false;
        return;
      }

      //if (!pinManager.allocateMultiplePins(pins, 2, PinOwner::HW_I2C)) { disabled = true; return; }
#if defined(ARDUINO_ARCH_ESP32)
      //Wire.begin(pins[1].pin, pins[0].pin);  // WLEDMM this might silently fail, which is OK as it just means that I2C bus is already running.
#else
      //Wire.begin();  // WLEDMM - i2c pins on 8266 are fixed.
#endif

      RTC.begin();
      time_t rtcTime = RTC.get();
      if (rtcTime) {
        toki.setTime(rtcTime,TOKI_NO_MS_ACCURACY,TOKI_TS_RTC);
        updateLocalTime();
			  USER_PRINTLN(F("Localtime updated from RTC."));
      } else {
        if (!RTC.chipPresent()) {
          enabled = false; //don't waste time if H/W error
          USER_PRINTLN(F("RTC board not present."));
        }
      }
      if (enabled) RTCfound = true;  // WLEDMM
    }

    void loop() {
      // WLEDMM begin
      if (!enabled) return;
      if (!RTCfound) return; // WLEDMM
      unsigned long currentTime = millis(); // get the current elapsed time
      if (strip.isUpdating() && (currentTime - lastTime < 10000)) return;  // WLEDMM: be nice
      // WLEDMM end

      if (enabled && toki.isTick()) {  // WLEDMM
        time_t t = toki.second();
        lastTime = millis();                            // WLEDMM

        if (abs(t - RTC.get())> RTC_DELTA) {            // WLEDMM only consider time diffs > 2 seconds
        	if (  (toki.getTimeSource() == TOKI_TS_NTP) 
              ||(  (toki.getTimeSource() != TOKI_TS_NONE) && (toki.getTimeSource() != TOKI_TS_RTC) 
                && (toki.getTimeSource() != TOKI_TS_BAD)  && (toki.getTimeSource() != TOKI_TS_UDP_SEC) && (toki.getTimeSource() != TOKI_TS_UDP))) 
          { // WLEDMM update RTC if we have a reliable time source
        	  RTC.set(t); //set RTC to NTP/UI-provided value - WLEDMM allow up to 3 sec deviation
			      USER_PRINTLN(F("RTC updated using localtime."));
		      } else {
            // WLEDMM if no reliable time -> update from RTC
            time_t rtcTime = RTC.get();
            if (rtcTime) {
              toki.setTime(rtcTime,TOKI_NO_MS_ACCURACY,TOKI_TS_RTC);
              updateLocalTime();
			        USER_PRINTLN(F("Localtime updated from RTC."));
            }
          }
        }
      }
    }

    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
//    void addToConfig(JsonObject& root)
//    {
//      JsonObject top = root.createNestedObject("RTC");
//      JsonArray pins = top.createNestedArray("pin");
//      pins.add(i2c_scl);
//      pins.add(i2c_sda);
//    }

    uint16_t getId()
    {
      return USERMOD_ID_RTC;
    }
};