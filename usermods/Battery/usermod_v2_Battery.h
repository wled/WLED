#pragma once

#include "wled.h"
#include "battery_defaults.h"

/*
 * Usermod by Maximilian Mewes
 * Mail: mewes.maximilian@gmx.de
 * GitHub: itCarl
 * Date: 25.12.2022
 * If you have any questions, please feel free to contact me.
 */
class UsermodBattery : public Usermod 
{
  private:
    // battery pin can be defined in my_config.h
    int8_t batteryPin = USERMOD_BATTERY_MEASUREMENT_PIN;
    // how often to read the battery voltage
    unsigned long readingInterval = USERMOD_BATTERY_MEASUREMENT_INTERVAL;
    unsigned long nextReadTime = 0;
    unsigned long lastReadTime = 0;
    // battery min. voltage
    float minBatteryVoltage = USERMOD_BATTERY_MIN_VOLTAGE;
    // battery max. voltage
    float maxBatteryVoltage = USERMOD_BATTERY_MAX_VOLTAGE;
    // all battery cells summed up
    unsigned int totalBatteryCapacity = USERMOD_BATTERY_TOTAL_CAPACITY;
    // raw analog reading 
    float rawValue = 0.0f;
    // calculated voltage            
    float voltage = maxBatteryVoltage;
    // between 0 and 1, to control strength of voltage smoothing filter
    float alpha = 0.05f;
    // multiplier for the voltage divider that is in place between ADC pin and battery, default will be 2 but might be adapted to readout voltages over ~5v ESP32 or ~6.6v ESP8266
    float voltageMultiplier = USERMOD_BATTERY_VOLTAGE_MULTIPLIER;
    // mapped battery level based on voltage
    int8_t batteryLevel = 100;
    // offset or calibration value to fine tune the calculated voltage
    float calibration = USERMOD_BATTERY_CALIBRATION;
    
    // time left estimation feature
    // bool calculateTimeLeftEnabled = USERMOD_BATTERY_CALCULATE_TIME_LEFT_ENABLED;
    // float estimatedTimeLeft = 0.0;

    // auto shutdown/shutoff/master off feature
    bool autoOffEnabled = USERMOD_BATTERY_AUTO_OFF_ENABLED;
    int8_t autoOffThreshold = USERMOD_BATTERY_AUTO_OFF_THRESHOLD;

    // low power indicator feature
    bool lowPowerIndicatorEnabled = USERMOD_BATTERY_LOW_POWER_INDICATOR_ENABLED;
    int8_t lowPowerIndicatorPreset = USERMOD_BATTERY_LOW_POWER_INDICATOR_PRESET;
    int8_t lowPowerIndicatorThreshold = USERMOD_BATTERY_LOW_POWER_INDICATOR_THRESHOLD;
    int8_t lowPowerIndicatorReactivationThreshold = lowPowerIndicatorThreshold+10;
    int8_t lowPowerIndicatorDuration = USERMOD_BATTERY_LOW_POWER_INDICATOR_DURATION;
    bool lowPowerIndicationDone = false;
    unsigned long lowPowerActivationTime = 0; // used temporary during active time
    int8_t lastPreset = 0;

    bool initDone = false;
    bool initializing = true;

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _readInterval[];
    static const char _enabled[];
    static const char _threshold[];
    static const char _preset[];
    static const char _duration[];
    static const char _init[];
    

    // custom map function
    // https://forum.arduino.cc/t/floating-point-using-map-function/348113/2
    double mapf(double x, double in_min, double in_max, double out_min, double out_max) 
    {
      return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
    }

    float dot2round(float x) 
    {
      float nx = (int)(x * 100 + .5);
      return (float)(nx / 100);
    }

    /*
     * Turn off all leds
     */
    void turnOff()
    {
      bri = 0;
      stateUpdated(CALL_MODE_DIRECT_CHANGE);
    }

    /*
     * Indicate low power by activating a configured preset for a given time and then switching back to the preset that was selected previously
     */
    void lowPowerIndicator()
    {
      if (!lowPowerIndicatorEnabled) return;
      if (batteryPin < 0) return;  // no measurement
      if (lowPowerIndicationDone && lowPowerIndicatorReactivationThreshold <= batteryLevel) lowPowerIndicationDone = false;
      if (lowPowerIndicatorThreshold <= batteryLevel) return;
      if (lowPowerIndicationDone) return;
      if (lowPowerActivationTime <= 1) {
        lowPowerActivationTime = millis();
        lastPreset = currentPreset;
        applyPreset(lowPowerIndicatorPreset);
      }

      if (lowPowerActivationTime+(lowPowerIndicatorDuration*1000) <= millis()) {
        lowPowerIndicationDone = true;
        lowPowerActivationTime = 0;
        applyPreset(lastPreset);
      }      
    }

    float readVoltage()
    {
      #ifdef ARDUINO_ARCH_ESP32
        // use calibrated millivolts analogread on esp32 (150 mV ~ 2450 mV default attentuation) and divide by 1000 to get from milivolts to volts and multiply by voltage multiplier and apply calibration value
        return (analogReadMilliVolts(batteryPin) / 1000.0f) * voltageMultiplier  + calibration;
      #else
        // use analog read on esp8266 ( 0V ~ 1V no attenuation options) and divide by ADC precision 1023 and multiply by voltage multiplier and apply calibration value
        return (analogRead(batteryPin) / 1023.0f) * voltageMultiplier + calibration;
      #endif
    }

  public:
    //Functions called by WLED

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() 
    {
      #ifdef ARDUINO_ARCH_ESP32
        bool success = false;
        DEBUG_PRINTLN(F("Allocating battery pin..."));
        if (batteryPin >= 0 && digitalPinToAnalogChannel(batteryPin) >= 0) 
          if (pinManager.allocatePin(batteryPin, false, PinOwner::UM_Battery)) {
            DEBUG_PRINTLN(F("Battery pin allocation succeeded."));
            success = true;
            voltage = readVoltage();
          }

        if (!success) {
          DEBUG_PRINTLN(F("Battery pin allocation failed."));
          batteryPin = -1;  // allocation failed
        } else {
          pinMode(batteryPin, INPUT);
        }
      #else //ESP8266 boards have only one analog input pin A0
        pinMode(batteryPin, INPUT);
        voltage = readVoltage();
      #endif

      nextReadTime = millis() + readingInterval;
      lastReadTime = millis();

      initDone = true;
    }


    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() 
    {
      //Serial.println("Connected to WiFi!");
    }


    /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     * 
     */
    void loop() 
    {
      if(strip.isUpdating()) return;

      lowPowerIndicator();

      // check the battery level every USERMOD_BATTERY_MEASUREMENT_INTERVAL (ms)
      if (millis() < nextReadTime) return;

      nextReadTime = millis() + readingInterval;
      lastReadTime = millis();

      if (batteryPin < 0) return;  // nothing to read

      initializing = false;

      rawValue = readVoltage();
      // filter with exponential smoothing because ADC in esp32 is fluctuating too much for a good single readout
      voltage = voltage + alpha * (rawValue - voltage);

      // check if voltage is within specified voltage range, allow 10% over/under voltage - removed cause this just makes it hard for people to troubleshoot as the voltage in the web gui will say invalid instead of displaying a voltage
      //voltage = ((voltage < minBatteryVoltage * 0.85f) || (voltage > maxBatteryVoltage * 1.1f)) ? -1.0f : voltage;

      // translate battery voltage into percentage
      /*
        the standard "map" function doesn't work
        https://www.arduino.cc/reference/en/language/functions/math/map/  notes and warnings at the bottom
      */
      #ifdef USERMOD_BATTERY_USE_LIPO
        batteryLevel = mapf(voltage, minBatteryVoltage, maxBatteryVoltage, 0, 100); // basic mapping
        // LiPo batteries have a differnt dischargin curve, see 
        //  https://blog.ampow.com/lipo-voltage-chart/
        if (batteryLevel < 40.0f) 
          batteryLevel = mapf(batteryLevel, 0, 40, 0, 12);       // last 45% -> drops very quickly
        else {
          if (batteryLevel < 90.0f)
            batteryLevel = mapf(batteryLevel, 40, 90, 12, 95);   // 90% ... 40% -> almost linear drop
          else // level >  90%
            batteryLevel = mapf(batteryLevel, 90, 105, 95, 100); // highest 15% -> drop slowly
        }
      #else
        batteryLevel = mapf(voltage, minBatteryVoltage, maxBatteryVoltage, 0, 100);
      #endif
      if (voltage > -1.0f) batteryLevel = constrain(batteryLevel, 0.0f, 110.0f);

      // if (calculateTimeLeftEnabled) {
      //   float currentBatteryCapacity = totalBatteryCapacity;
      //   estimatedTimeLeft = (currentBatteryCapacity/strip.currentMilliamps)*60;
      // }

      // Auto off -- Master power off
      if (autoOffEnabled && (autoOffThreshold >= batteryLevel))
        turnOff();

#ifndef WLED_DISABLE_MQTT
      // SmartHome stuff
      // still don't know much about MQTT and/or HA
      if (WLED_MQTT_CONNECTED) {
        char buf[64]; // buffer for snprintf()
        snprintf_P(buf, 63, PSTR("%s/voltage"), mqttDeviceTopic);
        mqtt->publish(buf, 0, false, String(voltage).c_str());
      }
#endif

    }

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject& root)
    {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      if (batteryPin < 0) {
        JsonArray infoVoltage = user.createNestedArray(F("Battery voltage"));
        infoVoltage.add(F("n/a"));
        infoVoltage.add(F(" invalid GPIO"));
        return;  // no GPIO - nothing to report
      }

      // info modal display names
      JsonArray infoPercentage = user.createNestedArray(F("Battery level"));
      JsonArray infoVoltage = user.createNestedArray(F("Battery voltage"));
      // if (calculateTimeLeftEnabled)
      // {
      //   JsonArray infoEstimatedTimeLeft = user.createNestedArray(F("Estimated time left"));
      //   if (initializing) {
      //     infoEstimatedTimeLeft.add(FPSTR(_init));
      //   } else {
      //     infoEstimatedTimeLeft.add(estimatedTimeLeft);
      //     infoEstimatedTimeLeft.add(F(" min"));
      //   }
      // }
      JsonArray infoNextUpdate = user.createNestedArray(F("Next update"));

      infoNextUpdate.add((nextReadTime - millis()) / 1000);
      infoNextUpdate.add(F(" sec"));
      
      if (initializing) {
        infoPercentage.add(FPSTR(_init));
        infoVoltage.add(FPSTR(_init));
        return;
      }

      if (batteryLevel < 0) {
        infoPercentage.add(F("invalid"));
      } else {
        infoPercentage.add(batteryLevel);
      }
      infoPercentage.add(F(" %"));

      if (voltage < 0) {
        infoVoltage.add(F("invalid"));
      } else {
        infoVoltage.add(dot2round(voltage));
      }
      infoVoltage.add(F(" V"));
    }


    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    /*
    void addToJsonState(JsonObject& root)
    {

    }
    */


    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    /*
    void readFromJsonState(JsonObject& root)
    {
    }
    */


    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current state, use serializeConfig() in your loop().
     * 
     * CAUTION: serializeConfig() will initiate a filesystem write operation.
     * It might cause the LEDs to stutter and will cause flash wear if called too often.
     * Use it sparingly and always in the loop, never in network callbacks!
     * 
     * addToConfig() will make your settings editable through the Usermod Settings page automatically.
     *
     * Usermod Settings Overview:
     * - Numeric values are treated as floats in the browser.
     *   - If the numeric value entered into the browser contains a decimal point, it will be parsed as a C float
     *     before being returned to the Usermod.  The float data type has only 6-7 decimal digits of precision, and
     *     doubles are not supported, numbers will be rounded to the nearest float value when being parsed.
     *     The range accepted by the input field is +/- 1.175494351e-38 to +/- 3.402823466e+38.
     *   - If the numeric value entered into the browser doesn't contain a decimal point, it will be parsed as a
     *     C int32_t (range: -2147483648 to 2147483647) before being returned to the usermod.
     *     Overflows or underflows are truncated to the max/min value for an int32_t, and again truncated to the type
     *     used in the Usermod when reading the value from ArduinoJson.
     * - Pin values can be treated differently from an integer value by using the key name "pin"
     *   - "pin" can contain a single or array of integer values
     *   - On the Usermod Settings page there is simple checking for pin conflicts and warnings for special pins
     *     - Red color indicates a conflict.  Yellow color indicates a pin with a warning (e.g. an input-only pin)
     *   - Tip: use int8_t to store the pin value in the Usermod, so a -1 value (pin not set) can be used
     *
     * See usermod_v2_auto_save.h for an example that saves Flash space by reusing ArduinoJson key name strings
     * 
     * If you need a dedicated settings page with custom layout for your Usermod, that takes a lot more work.  
     * You will have to add the setting to the HTML, xml.cpp and set.cpp manually.
     * See the WLED Soundreactive fork (code and wiki) for reference.  https://github.com/atuline/WLED
     * 
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root)
    {
      JsonObject battery = root.createNestedObject(FPSTR(_name));           // usermodname
      #ifdef ARDUINO_ARCH_ESP32
        battery[F("pin")] = batteryPin;
      #endif

      // battery[F("time-left")] = calculateTimeLeftEnabled;
      battery[F("min-voltage")] = minBatteryVoltage;
      battery[F("max-voltage")] = maxBatteryVoltage;
      battery[F("capacity")] = totalBatteryCapacity;
      battery[F("calibration")] = calibration;
      battery[F("voltage-multiplier")] = voltageMultiplier;
      battery[FPSTR(_readInterval)] = readingInterval;
      
      JsonObject ao = battery.createNestedObject(F("auto-off"));               // auto off section
      ao[FPSTR(_enabled)] = autoOffEnabled;
      ao[FPSTR(_threshold)] = autoOffThreshold;

      JsonObject lp = battery.createNestedObject(F("indicator"));    // low power section
      lp[FPSTR(_enabled)] = lowPowerIndicatorEnabled;
      lp[FPSTR(_preset)] = lowPowerIndicatorPreset; // dropdown trickery (String)lowPowerIndicatorPreset; 
      lp[FPSTR(_threshold)] = lowPowerIndicatorThreshold;
      lp[FPSTR(_duration)] = lowPowerIndicatorDuration;

      // read voltage in case calibration or voltage multiplier changed to see immediate effect
      voltage = readVoltage();

      DEBUG_PRINTLN(F("Battery config saved."));
    }

    void appendConfigData()
    {
      oappend(SET_F("addInfo('Battery:min-voltage', 1, 'v');"));
      oappend(SET_F("addInfo('Battery:max-voltage', 1, 'v');"));
      oappend(SET_F("addInfo('Battery:capacity', 1, 'mAh');"));
      oappend(SET_F("addInfo('Battery:interval', 1, 'ms');"));
      oappend(SET_F("addInfo('Battery:auto-off:threshold', 1, '%');"));
      oappend(SET_F("addInfo('Battery:indicator:threshold', 1, '%');"));
      oappend(SET_F("addInfo('Battery:indicator:duration', 1, 's');"));
      
      // cannot quite get this mf to work. its exeeding some buffer limit i think
      // what i wanted is a list of all presets to select one from
      // oappend(SET_F("bd=addDropdown('Battery:low-power-indicator', 'preset');"));
      // the loop generates: oappend(SET_F("addOption(bd, 'preset name', preset id);"));
      // for(int8_t i=1; i < 42; i++) {
      //   oappend(SET_F("addOption(bd, 'Preset#"));
      //   oappendi(i);
      //   oappend(SET_F("',"));
      //   oappendi(i);
      //   oappend(SET_F(");"));
      // }
    }


    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
     * 
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     * 
     * Return true in case the config values returned from Usermod Settings were complete, or false if you'd like WLED to save your defaults to disk (so any missing values are editable in Usermod Settings)
     * 
     * getJsonValue() returns false if the value is missing, or copies the value into the variable provided and returns true if the value is present
     * The configComplete variable is true only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to save them
     * 
     * This function is guaranteed to be called on boot, but could also be called every time settings are updated
     */
    bool readFromConfig(JsonObject& root)
    {
      #ifdef ARDUINO_ARCH_ESP32
        int8_t newBatteryPin = batteryPin;
      #endif
      
      JsonObject battery = root[FPSTR(_name)];
      if (battery.isNull()) 
      {
        DEBUG_PRINT(FPSTR(_name));
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      #ifdef ARDUINO_ARCH_ESP32
        newBatteryPin     = battery[F("pin")] | newBatteryPin;
      #endif
      // calculateTimeLeftEnabled = battery[F("time-left")] | calculateTimeLeftEnabled;
      setMinBatteryVoltage(battery[F("min-voltage")] | minBatteryVoltage);
      setMaxBatteryVoltage(battery[F("max-voltage")] | maxBatteryVoltage);
      setTotalBatteryCapacity(battery[F("capacity")] | totalBatteryCapacity);
      setCalibration(battery[F("calibration")] | calibration);
      setVoltageMultiplier(battery[F("voltage-multiplier")] | voltageMultiplier);
      setReadingInterval(battery[FPSTR(_readInterval)] | readingInterval);

      JsonObject ao = battery[F("auto-off")];
      setAutoOffEnabled(ao[FPSTR(_enabled)] | autoOffEnabled);
      setAutoOffThreshold(ao[FPSTR(_threshold)] | autoOffThreshold);

      JsonObject lp = battery[F("indicator")];
      setLowPowerIndicatorEnabled(lp[FPSTR(_enabled)] | lowPowerIndicatorEnabled);
      setLowPowerIndicatorPreset(lp[FPSTR(_preset)] | lowPowerIndicatorPreset); // dropdown trickery (int)lp["preset"]
      setLowPowerIndicatorThreshold(lp[FPSTR(_threshold)] | lowPowerIndicatorThreshold);
      lowPowerIndicatorReactivationThreshold = lowPowerIndicatorThreshold+10;
      setLowPowerIndicatorDuration(lp[FPSTR(_duration)] | lowPowerIndicatorDuration);

      DEBUG_PRINT(FPSTR(_name));

      #ifdef ARDUINO_ARCH_ESP32
        if (!initDone) 
        {
          // first run: reading from cfg.json
          batteryPin = newBatteryPin;
          DEBUG_PRINTLN(F(" config loaded."));
        } 
        else 
        {
          DEBUG_PRINTLN(F(" config (re)loaded."));

          // changing parameters from settings page
          if (newBatteryPin != batteryPin) 
          {
            // deallocate pin
            pinManager.deallocatePin(batteryPin, PinOwner::UM_Battery);
            batteryPin = newBatteryPin;
            // initialise
            setup();
          }
        }
      #endif

      return !battery[FPSTR(_readInterval)].isNull();
    }

    /*
     * Generate a preset sample for low power indication 
     */
    void generateExamplePreset()
    {
      // StaticJsonDocument<300> j;
      // JsonObject preset = j.createNestedObject();
      // preset["mainseg"] = 0;
      // JsonArray seg = preset.createNestedArray("seg");
      // JsonObject seg0 = seg.createNestedObject();
      // seg0["id"] = 0;
      // seg0["start"] = 0;
      // seg0["stop"] = 60;
      // seg0["grp"] = 0;
      // seg0["spc"] = 0;
      // seg0["on"] = true;
      // seg0["bri"] = 255;

      // JsonArray col0 = seg0.createNestedArray("col");
      // JsonArray col00 = col0.createNestedArray();
      // col00.add(255);
      // col00.add(0);
      // col00.add(0);

      // seg0["fx"] = 1;
      // seg0["sx"] = 128;
      // seg0["ix"] = 128;

      // savePreset(199, "Low power Indicator", preset);
    }
   

    /*
     *
     * Getter and Setter. Just in case some other usermod wants to interact with this in the future
     *
     */

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ID_BATTERY;
    }


    unsigned long getReadingInterval()
    {
      return readingInterval;
    }

    /*
     * minimum repetition is 3000ms (3s) 
     */
    void setReadingInterval(unsigned long newReadingInterval)
    {
      readingInterval = max((unsigned long)3000, newReadingInterval);
    }


    /*
     * Get lowest configured battery voltage
     */
    float getMinBatteryVoltage()
    {
      return minBatteryVoltage;
    }

    /*
     * Set lowest battery voltage
     * can't be below 0 volt
     */
    void setMinBatteryVoltage(float voltage)
    {
      minBatteryVoltage = max(0.0f, voltage);
    }

    /*
     * Get highest configured battery voltage
     */
    float getMaxBatteryVoltage()
    {
      return maxBatteryVoltage;
    }
    
    /*
     * Set highest battery voltage
     * can't be below minBatteryVoltage
     */
    void setMaxBatteryVoltage(float voltage)
    {
      #ifdef USERMOD_BATTERY_USE_LIPO
        maxBatteryVoltage = max(getMinBatteryVoltage()+0.7f, voltage);
      #else
        maxBatteryVoltage = max(getMinBatteryVoltage()+1.0f, voltage);
      #endif
    }


    /*
     * Get the capacity of all cells in parralel sumed up
     * unit: mAh
     */
    unsigned int getTotalBatteryCapacity()
    {
      return totalBatteryCapacity;
    }

    void setTotalBatteryCapacity(unsigned int capacity)
    {
      totalBatteryCapacity = capacity;
    }



    /*
     * Get the calculated voltage
     * formula: (adc pin value / adc precision * max voltage) + calibration
     */
    float getVoltage()
    {
      return voltage;
    }

    /*
     * Get the mapped battery level (0 - 100) based on voltage
     * important: voltage can drop when a load is applied, so its only an estimate
     */
    int8_t getBatteryLevel()
    {
      return batteryLevel;
    }

    /*
     * Get the configured calibration value
     * a offset value to fine-tune the calculated voltage.
     */
    float getCalibration()
    {
      return calibration;
    }

    /*
     * Set the voltage calibration offset value
     * a offset value to fine-tune the calculated voltage.
     */
    void setCalibration(float offset)
    {
      calibration = offset;
    }

    /*
     * Set the voltage multiplier value
     * A multiplier that may need adjusting for different voltage divider setups
     */
    void setVoltageMultiplier(float multiplier)
    {
      voltageMultiplier = multiplier;
    }

    /*
     * Get the voltage multiplier value
     * A multiplier that may need adjusting for different voltage divider setups
     */
    float getVoltageMultiplier()
    {
      return voltageMultiplier;
    }

    /*
     * Get auto-off feature enabled status
     * is auto-off enabled, true/false
     */
    bool getAutoOffEnabled()
    {
      return autoOffEnabled;
    }

    /*
     * Set auto-off feature status 
     */
    void setAutoOffEnabled(bool enabled)
    {
      autoOffEnabled = enabled;
    }
    
    /*
     * Get auto-off threshold in percent (0-100)
     */
    int8_t getAutoOffThreshold()
    {
      return autoOffThreshold;
    }

    /*
     * Set auto-off threshold in percent (0-100) 
     */
    void setAutoOffThreshold(int8_t threshold)
    {
      autoOffThreshold = min((int8_t)100, max((int8_t)0, threshold));
      // when low power indicator is enabled the auto-off threshold cannot be above indicator threshold
      autoOffThreshold  = lowPowerIndicatorEnabled /*&& autoOffEnabled*/ ? min(lowPowerIndicatorThreshold-1, (int)autoOffThreshold) : autoOffThreshold;
    }


    /*
     * Get low-power-indicator feature enabled status
     * is the low-power-indicator enabled, true/false
     */
    bool getLowPowerIndicatorEnabled()
    {
      return lowPowerIndicatorEnabled;
    }

    /*
     * Set low-power-indicator feature status 
     */
    void setLowPowerIndicatorEnabled(bool enabled)
    {
      lowPowerIndicatorEnabled = enabled;
    }

    /*
     * Get low-power-indicator preset to activate when low power is detected
     */
    int8_t getLowPowerIndicatorPreset()
    {
      return lowPowerIndicatorPreset;
    }

    /* 
     * Set low-power-indicator preset to activate when low power is detected
     */
    void setLowPowerIndicatorPreset(int8_t presetId)
    {
      // String tmp = ""; For what ever reason this doesn't work :(
      // lowPowerIndicatorPreset = getPresetName(presetId, tmp) ? presetId : lowPowerIndicatorPreset;
      lowPowerIndicatorPreset = presetId;
    }

    /*
     * Get low-power-indicator threshold in percent (0-100)
     */
    int8_t getLowPowerIndicatorThreshold()
    {
      return lowPowerIndicatorThreshold;
    }

    /*
     * Set low-power-indicator threshold in percent (0-100)
     */
    void setLowPowerIndicatorThreshold(int8_t threshold)
    {
      lowPowerIndicatorThreshold = threshold;
      // when auto-off is enabled the indicator threshold cannot be below auto-off threshold
      lowPowerIndicatorThreshold  = autoOffEnabled /*&& lowPowerIndicatorEnabled*/ ? max(autoOffThreshold+1, (int)lowPowerIndicatorThreshold) : max(5, (int)lowPowerIndicatorThreshold);
    }

    /*
     * Get low-power-indicator duration in seconds
     */
    int8_t getLowPowerIndicatorDuration()
    {
      return lowPowerIndicatorDuration;
    }

    /*
     * Set low-power-indicator duration in seconds
     */
    void setLowPowerIndicatorDuration(int8_t duration)
    {
      lowPowerIndicatorDuration = duration;
    }


    /*
     * Get low-power-indicator status when the indication is done thsi returns true
     */
    bool getLowPowerIndicatorDone()
    {
      return lowPowerIndicationDone;
    }
};

// strings to reduce flash memory usage (used more than twice)
const char UsermodBattery::_name[]          PROGMEM = "Battery";
const char UsermodBattery::_readInterval[]  PROGMEM = "interval";
const char UsermodBattery::_enabled[]       PROGMEM = "enabled";
const char UsermodBattery::_threshold[]     PROGMEM = "threshold";
const char UsermodBattery::_preset[]        PROGMEM = "preset";
const char UsermodBattery::_duration[]      PROGMEM = "duration";
const char UsermodBattery::_init[]          PROGMEM = "init";
