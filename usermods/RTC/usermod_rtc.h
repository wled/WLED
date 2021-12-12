#pragma once

#include "src/dependencies/time/DS1307RTC.h"
#include "wled.h"

#ifdef ARDUINO_ARCH_ESP32
  #define HW_PIN_SCL 22
  #define HW_PIN_SDA 21
#else
  #define HW_PIN_SCL 5
  #define HW_PIN_SDA 4
#endif

//Connect DS1307 to standard I2C pins (ESP32: GPIO 21 (SDA)/GPIO 22 (SCL))

class RTCUsermod : public Usermod {
  private:
    unsigned long lastTime = 0;
    bool disabled = false;
  public:

    void setup() {
      PinManagerPinType pins[2] = { { HW_PIN_SCL, true }, { HW_PIN_SDA, true } };
      if (!pinManager.allocateMultiplePins(pins, 2, PinOwner::HW_I2C)) { disabled = true; return; }
      time_t rtcTime = RTC.get();
      if (rtcTime) {
        toki.setTime(rtcTime,TOKI_NO_MS_ACCURACY,TOKI_TS_RTC);
        updateLocalTime();
      } else {
        if (!RTC.chipPresent()) disabled = true; //don't waste time if H/W error
      }
    }

    void loop() {
      if (strip.isUpdating()) return;
      if (!disabled && toki.isTick()) {
        time_t t = toki.second();
        if (t != RTC.get()) RTC.set(t); //set RTC to NTP/UI-provided value
      }
    }

    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject("RTC");
      JsonArray pins = top.createNestedArray("pin");
      pins.add(HW_PIN_SCL);
      pins.add(HW_PIN_SDA);
    }

    uint16_t getId()
    {
      return USERMOD_ID_RTC;
    }
};