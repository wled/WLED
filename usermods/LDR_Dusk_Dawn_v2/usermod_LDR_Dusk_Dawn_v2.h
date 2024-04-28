#pragma once
#include "wled.h"

#ifndef ARDUINO_ARCH_ESP32
  // 8266 does not support analogRead on user selectable pins 
  #error only ESP32 is supported by usermod LDR_DUSK_DAWN
#endif

class LDR_Dusk_Dawn_v2 : public Usermod {
  private:
    // Defaults
    bool ldrEnabled = false;
    int ldrPin = 34; //A2 on Adafruit Huzzah32
    int ldrThresholdMinutes = 5; // How many minutes of readings above/below threshold until it switches LED state
    int ldrThreshold = 1000; // Readings higher than this number will turn off LED. 
    int ldrOnPreset = 1; // Default "On" Preset
    int ldrOffPreset = 2; // Default "Off" Preset

    // Variables
    bool initDone = false;
    bool ldrEnabledPreviously = false; // Was LDR enabled for the previous check? First check is always no.
    int ldrOffCount; // Number of readings above the threshold
    int ldrOnCount; // Number of readings below the threshold
    int ldrReading = 0; // Last LDR reading
    int ldrLEDState; // Current LED on/off state
    unsigned long lastMillis = 0;
    static const char _name[];

  public:
    void setup() {
      // register ldrPin
      if ((ldrPin >= 0) && (digitalPinToAnalogChannel(ldrPin) >= 0)) {
        if(!pinManager.allocatePin(ldrPin, false, PinOwner::UM_LDR_DUSK_DAWN)) ldrEnabled = false; // pin already in use -> disable usermod
        else pinMode(ldrPin, INPUT);                                                               // alloc success -> configure pin for input
      } else ldrEnabled = false;                                                                   // invalid pin -> disable usermod
      initDone = true;
    }

    void loop() {
      // Only update every 10 seconds
      if (millis() - lastMillis > 10000) {      
        if (  (ldrEnabled == true)
           && (ldrPin >= 0) && (digitalPinToAnalogChannel(ldrPin) >= 0) ) { // make sure that pin is valid for analogread()
          // Default state is off
          if (ldrEnabledPreviously == false) {
              applyPreset(ldrOffPreset);
              ldrEnabledPreviously = true;
              ldrLEDState = 0;
          }

          // Get LDR reading and increment counter by number of seconds since last read
          ldrReading = analogRead(ldrPin);
          if (ldrReading <= ldrThreshold) {
            ldrOnCount = ldrOnCount + 10;
            ldrOffCount = 0;
          } else {
            ldrOffCount = ldrOffCount + 10;
            ldrOnCount = 0;
          }

          if (ldrOnCount >= (ldrThresholdMinutes * 60)) {
            ldrOnCount = 0;
            // If LEDs were previously off, turn on
            if (ldrLEDState == 0) {
              applyPreset(ldrOnPreset);
              ldrLEDState = 1;
            }
          }

          if (ldrOffCount >= (ldrThresholdMinutes * 60)) {
            ldrOffCount = 0;
            // If LEDs were previously on, turn off
            if (ldrLEDState == 1) {
              applyPreset(ldrOffPreset);
              ldrLEDState = 0;
            }
          }
        } else {
          // LDR is disabled, reset variables to default
          ldrReading = 0;
          ldrOnCount = 0;
          ldrOffCount = 0;
          ldrLEDState = 0;
          ldrEnabledPreviously = false;
        }
        lastMillis = millis();
      }
    }

  void addToConfig(JsonObject& root) {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top["Enabled"] = ldrEnabled;
      top["LDR Pin"] = ldrPin;
      top["Threshold Minutes"] = ldrThresholdMinutes;
      top["Threshold"] = ldrThreshold;
      top["On Preset"] = ldrOnPreset;
      top["Off Preset"] = ldrOffPreset;
  }

  bool readFromConfig(JsonObject& root) {
      int8_t oldLdrPin = ldrPin;
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top["Enabled"], ldrEnabled);
      configComplete &= getJsonValue(top["LDR Pin"], ldrPin);
      configComplete &= getJsonValue(top["Threshold Minutes"], ldrThresholdMinutes);
      configComplete &= getJsonValue(top["Threshold"], ldrThreshold);
      configComplete &= getJsonValue(top["On Preset"], ldrOnPreset);
      configComplete &= getJsonValue(top["Off Preset"], ldrOffPreset);

      if (initDone && (ldrPin != oldLdrPin)) {
         // pin changed - un-register previous pin, register new pin
        if (oldLdrPin >= 0) pinManager.deallocatePin(oldLdrPin, PinOwner::UM_LDR_DUSK_DAWN);
        setup();             // setup new pin
      }
      return configComplete;
  }

  void addToJsonInfo(JsonObject& root) {
      // If "u" object does not exist yet we need to create it
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray LDR_Enabled = user.createNestedArray("LDR dusk/dawn enabled");
      LDR_Enabled.add(ldrEnabled);
      if (!ldrEnabled) return; // do not add more if usermod is disabled

      JsonArray LDR_Reading = user.createNestedArray("LDR reading");
      LDR_Reading.add(ldrReading);

      JsonArray LDR_State = user.createNestedArray("LDR turned LEDs on");
      LDR_State.add(bool(ldrLEDState));

      // Optional debug information:
      //JsonArray LDR_On_Count = user.createNestedArray("LDR on count");
      //LDR_On_Count.add(ldrOnCount);

      //JsonArray LDR_Off_Count = user.createNestedArray("LDR off count");
      //LDR_Off_Count.add(ldrOffCount);

      //bool pinValid = ((ldrPin >= 0) && (digitalPinToAnalogChannel(ldrPin) >= 0));
      //if (pinManager.getPinOwner(ldrPin) != PinOwner::UM_LDR_DUSK_DAWN) pinValid = false;
      //JsonArray LDR_valid = user.createNestedArray(F("LDR pin"));
      //LDR_valid.add(ldrPin);
      //LDR_valid.add(pinValid ? F(" OK"): F(" invalid"));
  }

  uint16_t getId() {
    return USERMOD_ID_LDR_DUSK_DAWN;
  }
};

const char LDR_Dusk_Dawn_v2::_name[]    PROGMEM = "LDR_Dusk_Dawn_v2";
