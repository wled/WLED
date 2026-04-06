#include "wled.h"
#include "battery_defaults.h"
#include "UMBattery.h"
#include "types/UnkownUMBattery.h"
#include "types/LionUMBattery.h"
#include "types/LipoUMBattery.h"

/*
 * Usermod by Maximilian Mewes
 * E-mail: mewes.maximilian@gmx.de
 * Created at: 25.12.2022
 * If you have any questions, please feel free to contact me.
 */
class UsermodBattery : public Usermod 
{
  private:
    // battery pin can be defined in my_config.h
    int8_t batteryPin = USERMOD_BATTERY_MEASUREMENT_PIN;
    
    UMBattery* bat = new UnkownUMBattery();
    batteryConfig cfg;

    // Initial delay before first reading to allow voltage stabilization
    unsigned long initialDelay = USERMOD_BATTERY_INITIAL_DELAY;
    bool initialDelayComplete = false;
    bool isFirstVoltageReading = true;
    // how often to read the battery voltage
    unsigned long readingInterval = USERMOD_BATTERY_MEASUREMENT_INTERVAL;
    unsigned long nextReadTime = 0;
    unsigned long lastReadTime = 0;
    // between 0 and 1, to control strength of voltage smoothing filter
    float alpha = USERMOD_BATTERY_AVERAGING_ALPHA;

    // auto shutdown/shutoff/master off feature
    bool autoOffEnabled = USERMOD_BATTERY_AUTO_OFF_ENABLED;
    uint8_t autoOffThreshold = USERMOD_BATTERY_AUTO_OFF_THRESHOLD;

    // low power indicator feature
    bool lowPowerIndicatorEnabled = USERMOD_BATTERY_LOW_POWER_INDICATOR_ENABLED;
    uint8_t lowPowerIndicatorPreset = USERMOD_BATTERY_LOW_POWER_INDICATOR_PRESET;
    uint8_t lowPowerIndicatorThreshold = USERMOD_BATTERY_LOW_POWER_INDICATOR_THRESHOLD;
    uint8_t lowPowerIndicatorReactivationThreshold = lowPowerIndicatorThreshold+10;
    uint8_t lowPowerIndicatorDuration = USERMOD_BATTERY_LOW_POWER_INDICATOR_DURATION;
    bool lowPowerIndicationDone = false;
    unsigned long lowPowerActivationTime = 0; // used temporary during active time
    uint8_t lastPreset = 0;

    //
    bool initDone = false;
    bool initializing = true;
    bool HomeAssistantDiscovery = false;

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _readInterval[];
    static const char _enabled[];
    static const char _threshold[];
    static const char _preset[];
    static const char _duration[];
    static const char _init[];
    static const char _haDiscovery[];

    /**
     * Helper for rounding floating point values 
     */
    float dot2round(float x) 
    {
      float nx = (int)(x * 100 + .5);
      return (float)(nx / 100);
    }

    /**
     * Helper for converting a string to lowercase
     */
    String stringToLower(String str)
    {
      for(int i = 0; i < str.length(); i++)
        if(str[i] >= 'A' && str[i] <= 'Z')
            str[i] += 32;
      return str;
    }

    /**
     * Turn off all leds
     */
    void turnOff()
    {
      bri = 0;
      stateUpdated(CALL_MODE_DIRECT_CHANGE);
    }

    /**
     * Indicate low power by activating a configured preset for a given time and then switching back to the preset that was selected previously
     */
    void lowPowerIndicator()
    {
      if (!lowPowerIndicatorEnabled) return;
      if (batteryPin < 0) return;  // no measurement
      if (lowPowerIndicationDone && lowPowerIndicatorReactivationThreshold <= bat->getLevel()) lowPowerIndicationDone = false;
      if (lowPowerIndicatorThreshold <= bat->getLevel()) return;
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

    /**
     * read the battery voltage in different ways depending on the architecture 
     */
    float readVoltage()
    {
      #ifdef ARDUINO_ARCH_ESP32
        // use calibrated millivolts analogread on esp32 (150 mV ~ 2450 mV default attentuation) and divide by 1000 to get from milivolts to volts and multiply by voltage multiplier and apply calibration value
        return (analogReadMilliVolts(batteryPin) / 1000.0f) * bat->getVoltageMultiplier()  + bat->getCalibration();
      #else
        // use analog read on esp8266 ( 0V ~ 1V no attenuation options) and divide by ADC precision 1023 and multiply by voltage multiplier and apply calibration value
        return (analogRead(batteryPin) / 1023.0f) * bat->getVoltageMultiplier() + bat->getCalibration();
      #endif
    }

#ifndef WLED_DISABLE_MQTT
    void addMqttSensor(const String &name, const String &type, const String &topic, const String &deviceClass, const String &unitOfMeasurement = "", const bool &isDiagnostic = false)
    {      
      StaticJsonDocument<600> doc;
      char uid[128], json_str[1024], buf[128];

      doc[F("name")] = name;
      doc[F("stat_t")] = topic;
      sprintf_P(uid, PSTR("%s_%s_%s"), escapedMac.c_str(), stringToLower(name).c_str(), type);
      doc[F("uniq_id")] = uid;
      doc[F("dev_cla")] = deviceClass;
      doc[F("exp_aft")] = 1800;

      if(type == "binary_sensor") {
        doc[F("pl_on")]  = "on";
        doc[F("pl_off")] = "off";
      }

      if(unitOfMeasurement != "")
        doc[F("unit_of_measurement")] = unitOfMeasurement;

      if(isDiagnostic)
        doc[F("entity_category")] = "diagnostic";

      JsonObject device = doc.createNestedObject(F("device")); // attach the sensor to the same device
      device[F("name")] = serverDescription;
      device[F("ids")]  = String(F("wled-sensor-")) + mqttClientID;
      device[F("mf")]   = F(WLED_BRAND);
      device[F("mdl")]  = F(WLED_PRODUCT_NAME);
      device[F("sw")]   = versionString;

      sprintf_P(buf, PSTR("homeassistant/%s/%s/%s/config"), type, mqttClientID, uid);
      DEBUG_PRINTLN(buf);
      size_t payload_size = serializeJson(doc, json_str);
      DEBUG_PRINTLN(json_str);

      mqtt->publish(buf, 0, true, json_str, payload_size);
    }

    void publishMqtt(const char* topic, const char* state)
    {
      if (WLED_MQTT_CONNECTED) {
        char buf[128];
        snprintf_P(buf, 127, PSTR("%s/%s"), mqttDeviceTopic, topic);
        mqtt->publish(buf, 0, false, state);
      }
    }
#endif

  public:
    //Functions called by WLED

    /**
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() 
    {
      // plug in the right battery type
      if(cfg.type == (batteryType)lipo) {
        bat = new LipoUMBattery();
      } else if(cfg.type == (batteryType)lion) {
        bat = new LionUMBattery();
      }

      // update the choosen battery type with configured values
      bat->update(cfg);

      #ifdef ARDUINO_ARCH_ESP32
        bool success = false;
        DEBUG_PRINTLN(F("Allocating battery pin..."));
        if (batteryPin >= 0 && digitalPinToAnalogChannel(batteryPin) >= 0) 
          if (PinManager::allocatePin(batteryPin, false, PinOwner::UM_Battery)) {
            DEBUG_PRINTLN(F("Battery pin allocation succeeded."));
            success = true;
          }

        if (!success) {
          DEBUG_PRINTLN(F("Battery pin allocation failed."));
          batteryPin = -1;  // allocation failed
        } else {
          pinMode(batteryPin, INPUT);
        }
      #else //ESP8266 boards have only one analog input pin A0
        pinMode(batteryPin, INPUT);
      #endif

      // First voltage reading is delayed to allow voltage stabilization after powering up
      nextReadTime = millis() + initialDelay;
      lastReadTime = millis();

      initDone = true;
    }


    /**
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

      // Handling the initial delay
      if (!initialDelayComplete && millis() < nextReadTime)
        return; // Continue to return until the initial delay is over

      // Once the initial delay is over, set it as complete
      if (!initialDelayComplete)
        {
          initialDelayComplete = true;
          // Set the regular interval after initial delay
          nextReadTime = millis() + readingInterval;
        }

      // Make the first voltage reading after the initial delay has elapsed
      if (isFirstVoltageReading)
        {
          bat->setVoltage(readVoltage());
          isFirstVoltageReading = false;
        }

      // check the battery level every USERMOD_BATTERY_MEASUREMENT_INTERVAL (ms)
      if (millis() < nextReadTime) return;

      nextReadTime = millis() + readingInterval;
      lastReadTime = millis();

      if (batteryPin < 0) return;  // nothing to read

      initializing = false;
      float rawValue = readVoltage();

      // filter with exponential smoothing because ADC in esp32 is fluctuating too much for a good single readout
      float filteredVoltage = bat->getVoltage() + alpha * (rawValue - bat->getVoltage());

      bat->setVoltage(filteredVoltage);
      // translate battery voltage into percentage
      bat->calculateAndSetLevel(filteredVoltage);

      // Auto off -- Master power off
      if (autoOffEnabled && (autoOffThreshold >= bat->getLevel()))
        turnOff();

#ifndef WLED_DISABLE_MQTT
      publishMqtt("battery", String(bat->getLevel(), 0).c_str());
      publishMqtt("voltage", String(bat->getVoltage()).c_str());
#endif

    }

    /**
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
      JsonArray infoNextUpdate = user.createNestedArray(F("Next update"));

      infoNextUpdate.add((nextReadTime - millis()) / 1000);
      infoNextUpdate.add(F(" sec"));
      
      if (initializing) {
        infoPercentage.add(FPSTR(_init));
        infoVoltage.add(FPSTR(_init));
        return;
      }

      if (bat->getLevel() < 0) {
        infoPercentage.add(F("invalid"));
      } else {
        infoPercentage.add(bat->getLevel());
      }
      infoPercentage.add(F(" %"));

      if (bat->getVoltage() < 0) {
        infoVoltage.add(F("invalid"));
      } else {
        infoVoltage.add(dot2round(bat->getVoltage()));
      }
      infoVoltage.add(F(" V"));
    }

    void addBatteryToJsonObject(JsonObject& battery, bool forJsonState)
    {
      if(forJsonState) { battery[F("type")] = cfg.type; } else {battery[F("type")] = (String)cfg.type; }  // has to be a String otherwise it won't get converted to a Dropdown
      battery[F("min-voltage")] = bat->getMinVoltage();
      battery[F("max-voltage")] = bat->getMaxVoltage();
      battery[F("calibration")] = bat->getCalibration();
      battery[F("voltage-multiplier")] = bat->getVoltageMultiplier();
      battery[FPSTR(_readInterval)] = readingInterval;
      battery[FPSTR(_haDiscovery)] = HomeAssistantDiscovery;

      JsonObject ao = battery.createNestedObject(F("auto-off"));  // auto off section
      ao[FPSTR(_enabled)] = autoOffEnabled;
      ao[FPSTR(_threshold)] = autoOffThreshold;

      JsonObject lp = battery.createNestedObject(F("indicator")); // low power section
      lp[FPSTR(_enabled)] = lowPowerIndicatorEnabled;
      lp[FPSTR(_preset)] = lowPowerIndicatorPreset; // dropdown trickery (String)lowPowerIndicatorPreset; 
      lp[FPSTR(_threshold)] = lowPowerIndicatorThreshold;
      lp[FPSTR(_duration)] = lowPowerIndicatorDuration;
    }

    void getUsermodConfigFromJsonObject(JsonObject& battery)
    {
      getJsonValue(battery[F("type")], cfg.type);
      getJsonValue(battery[F("min-voltage")], cfg.minVoltage);
      getJsonValue(battery[F("max-voltage")], cfg.maxVoltage);
      getJsonValue(battery[F("calibration")], cfg.calibration);
      getJsonValue(battery[F("voltage-multiplier")], cfg.voltageMultiplier);
      setReadingInterval(battery[FPSTR(_readInterval)] | readingInterval);
      setHomeAssistantDiscovery(battery[FPSTR(_haDiscovery)] | HomeAssistantDiscovery);

      JsonObject ao = battery[F("auto-off")];
      setAutoOffEnabled(ao[FPSTR(_enabled)] | autoOffEnabled);
      setAutoOffThreshold(ao[FPSTR(_threshold)] | autoOffThreshold);

      JsonObject lp = battery[F("indicator")];
      setLowPowerIndicatorEnabled(lp[FPSTR(_enabled)] | lowPowerIndicatorEnabled);
      setLowPowerIndicatorPreset(lp[FPSTR(_preset)] | lowPowerIndicatorPreset);
      setLowPowerIndicatorThreshold(lp[FPSTR(_threshold)] | lowPowerIndicatorThreshold);
      lowPowerIndicatorReactivationThreshold = lowPowerIndicatorThreshold+10;
      setLowPowerIndicatorDuration(lp[FPSTR(_duration)] | lowPowerIndicatorDuration);
      
      if(initDone) 
        bat->update(cfg);
    }

    /**
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root)
    {
      JsonObject battery = root.createNestedObject(FPSTR(_name));

      if (battery.isNull())
        battery = root.createNestedObject(FPSTR(_name));
      
      addBatteryToJsonObject(battery, true);
      
      DEBUG_PRINTLN(F("Battery state exposed in JSON API."));
    }


    /**
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    /*
    void readFromJsonState(JsonObject& root)
    {
      if (!initDone) return;  // prevent crash on boot applyPreset()

      JsonObject battery = root[FPSTR(_name)];

      if (!battery.isNull()) {
        getUsermodConfigFromJsonObject(battery);
      
        DEBUG_PRINTLN(F("Battery state read from JSON API."));
      }
    }
    */


    /**
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
      JsonObject battery = root.createNestedObject(FPSTR(_name));
      
      if (battery.isNull()) {
        battery = root.createNestedObject(FPSTR(_name));
      }

      #ifdef ARDUINO_ARCH_ESP32
        battery[F("pin")] = batteryPin;
      #endif
      
      addBatteryToJsonObject(battery, false);

      // read voltage in case calibration or voltage multiplier changed to see immediate effect
      bat->setVoltage(readVoltage());

      DEBUG_PRINTLN(F("Battery config saved."));
    }

    void appendConfigData()
    {
      // Total: 462 Bytes
      oappend(F("td=addDropdown('Battery','type');"));              // 34 Bytes
      oappend(F("addOption(td,'Unkown','0');"));                    // 28 Bytes
      oappend(F("addOption(td,'LiPo','1');"));                      // 26 Bytes
      oappend(F("addOption(td,'LiOn','2');"));                      // 26 Bytes
      oappend(F("addInfo('Battery:type',1,'<small style=\"color:orange\">requires reboot</small>');")); // 81 Bytes
      oappend(F("addInfo('Battery:min-voltage',1,'v');"));          // 38 Bytes
      oappend(F("addInfo('Battery:max-voltage',1,'v');"));          // 38 Bytes
      oappend(F("addInfo('Battery:interval',1,'ms');"));            // 36 Bytes
      oappend(F("addInfo('Battery:HA-discovery',1,'');"));          // 38 Bytes
      oappend(F("addInfo('Battery:auto-off:threshold',1,'%');"));   // 45 Bytes
      oappend(F("addInfo('Battery:indicator:threshold',1,'%');"));  // 46 Bytes
      oappend(F("addInfo('Battery:indicator:duration',1,'s');"));   // 45 Bytes
      
      // this option list would exeed the oappend() buffer
      // a list of all presets to select one from
      // oappend(F("bd=addDropdown('Battery:low-power-indicator', 'preset');"));
      // the loop generates: oappend(F("addOption(bd, 'preset name', preset id);"));
      // for(int8_t i=1; i < 42; i++) {
      //   oappend(F("addOption(bd, 'Preset#"));
      //   oappendi(i);
      //   oappend(F("',"));
      //   oappendi(i);
      //   oappend(F(");"));
      // }
    }


    /**
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
      setMinBatteryVoltage(battery[F("min-voltage")] | bat->getMinVoltage());
      setMaxBatteryVoltage(battery[F("max-voltage")] | bat->getMaxVoltage());
      setCalibration(battery[F("calibration")] | bat->getCalibration());
      setVoltageMultiplier(battery[F("voltage-multiplier")] | bat->getVoltageMultiplier());
      setReadingInterval(battery[FPSTR(_readInterval)] | readingInterval);
      setHomeAssistantDiscovery(battery[FPSTR(_haDiscovery)] | HomeAssistantDiscovery);

      getUsermodConfigFromJsonObject(battery);

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
            PinManager::deallocatePin(batteryPin, PinOwner::UM_Battery);
            batteryPin = newBatteryPin;
            // initialise
            setup();
          }
        }
      #endif

      return !battery[FPSTR(_readInterval)].isNull();
    }

#ifndef WLED_DISABLE_MQTT
    void onMqttConnect(bool sessionPresent)
    {
      // Home Assistant Autodiscovery
      if (!HomeAssistantDiscovery)
        return;

      // battery percentage
      char mqttBatteryTopic[128];
      snprintf_P(mqttBatteryTopic, 127, PSTR("%s/battery"), mqttDeviceTopic);
      this->addMqttSensor(F("Battery"), "sensor", mqttBatteryTopic, "battery", "%", true);

      // voltage
      char mqttVoltageTopic[128];
      snprintf_P(mqttVoltageTopic, 127, PSTR("%s/voltage"), mqttDeviceTopic);
      this->addMqttSensor(F("Voltage"), "sensor", mqttVoltageTopic, "voltage", "V", true);
    }
#endif   

    /*
     *
     * Getter and Setter. Just in case some other usermod wants to interact with this in the future
     *
     */

    /**
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ID_BATTERY;
    }

    /**
     * get currently active battery type
     */
    batteryType getBatteryType()
    {
      return cfg.type;
    }

    /**
     * 
     */
    unsigned long getReadingInterval()
    {
      return readingInterval;
    }

    /**
     * minimum repetition is 3000ms (3s) 
     */
    void setReadingInterval(unsigned long newReadingInterval)
    {
      readingInterval = max((unsigned long)3000, newReadingInterval);
    }

    /**
     * Get lowest configured battery voltage
     */
    float getMinBatteryVoltage()
    {
      return bat->getMinVoltage();
    }

    /**
     * Set lowest battery voltage
     * can't be below 0 volt
     */
    void setMinBatteryVoltage(float voltage)
    {
      bat->setMinVoltage(voltage);
    }

    /**
     * Get highest configured battery voltage
     */
    float getMaxBatteryVoltage()
    {
      return bat->getMaxVoltage();
    }
    
    /**
     * Set highest battery voltage
     * can't be below minBatteryVoltage
     */
    void setMaxBatteryVoltage(float voltage)
    {
      bat->setMaxVoltage(voltage);
    }


    /**
     * Get the calculated voltage
     * formula: (adc pin value / adc precision * max voltage) + calibration
     */
    float getVoltage()
    {
      return bat->getVoltage();
    }

    /**
     * Get the mapped battery level (0 - 100) based on voltage
     * important: voltage can drop when a load is applied, so its only an estimate
     */
    int8_t getBatteryLevel()
    {
      return bat->getLevel();
    }

    /**
     * Get the configured calibration value
     * a offset value to fine-tune the calculated voltage.
     */
    float getCalibration()
    {
      return bat->getCalibration();
    }

    /**
     * Set the voltage calibration offset value
     * a offset value to fine-tune the calculated voltage.
     */
    void setCalibration(float offset)
    {
      bat->setCalibration(offset);
    }

    /**
     * Set the voltage multiplier value
     * A multiplier that may need adjusting for different voltage divider setups
     */
    void setVoltageMultiplier(float multiplier)
    {
      bat->setVoltageMultiplier(multiplier);
    }

    /*
     * Get the voltage multiplier value
     * A multiplier that may need adjusting for different voltage divider setups
     */
    float getVoltageMultiplier()
    {
      return bat->getVoltageMultiplier();
    }

    /**
     * Get auto-off feature enabled status
     * is auto-off enabled, true/false
     */
    bool getAutoOffEnabled()
    {
      return autoOffEnabled;
    }

    /**
     * Set auto-off feature status 
     */
    void setAutoOffEnabled(bool enabled)
    {
      autoOffEnabled = enabled;
    }
    
    /**
     * Get auto-off threshold in percent (0-100)
     */
    int8_t getAutoOffThreshold()
    {
      return autoOffThreshold;
    }

    /**
     * Set auto-off threshold in percent (0-100) 
     */
    void setAutoOffThreshold(int8_t threshold)
    {
      autoOffThreshold = min((int8_t)100, max((int8_t)0, threshold));
      // when low power indicator is enabled the auto-off threshold cannot be above indicator threshold
      autoOffThreshold  = lowPowerIndicatorEnabled /*&& autoOffEnabled*/ ? min(lowPowerIndicatorThreshold-1, (int)autoOffThreshold) : autoOffThreshold;
    }

    /**
     * Get low-power-indicator feature enabled status
     * is the low-power-indicator enabled, true/false
     */
    bool getLowPowerIndicatorEnabled()
    {
      return lowPowerIndicatorEnabled;
    }

    /**
     * Set low-power-indicator feature status 
     */
    void setLowPowerIndicatorEnabled(bool enabled)
    {
      lowPowerIndicatorEnabled = enabled;
    }

    /**
     * Get low-power-indicator preset to activate when low power is detected
     */
    int8_t getLowPowerIndicatorPreset()
    {
      return lowPowerIndicatorPreset;
    }

    /** 
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

    /**
     * Set low-power-indicator threshold in percent (0-100)
     */
    void setLowPowerIndicatorThreshold(int8_t threshold)
    {
      lowPowerIndicatorThreshold = threshold;
      // when auto-off is enabled the indicator threshold cannot be below auto-off threshold
      lowPowerIndicatorThreshold  = autoOffEnabled /*&& lowPowerIndicatorEnabled*/ ? max(autoOffThreshold+1, (int)lowPowerIndicatorThreshold) : max(5, (int)lowPowerIndicatorThreshold);
    }

    /**
     * Get low-power-indicator duration in seconds
     */
    int8_t getLowPowerIndicatorDuration()
    {
      return lowPowerIndicatorDuration;
    }

    /**
     * Set low-power-indicator duration in seconds
     */
    void setLowPowerIndicatorDuration(int8_t duration)
    {
      lowPowerIndicatorDuration = duration;
    }

    /**
     * Get low-power-indicator status when the indication is done this returns true
     */
    bool getLowPowerIndicatorDone()
    {
      return lowPowerIndicationDone;
    }

    /**
     * Set Home Assistant auto discovery
     */
    void setHomeAssistantDiscovery(bool enable)
    {
      HomeAssistantDiscovery = enable;
    }

    /**
     * Get Home Assistant auto discovery
     */
    bool getHomeAssistantDiscovery()
    {
      return HomeAssistantDiscovery;
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
const char UsermodBattery::_haDiscovery[]   PROGMEM = "HA-discovery";


static UsermodBattery battery;
REGISTER_USERMOD(battery);