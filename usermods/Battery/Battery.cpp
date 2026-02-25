#include "wled.h"
#include "battery_defaults.h"
#include "UMBattery.h"
#include "types/LipoUMBattery.h"
#include "types/LionUMBattery.h"
#include "types/Lifepo4UMBattery.h"

/*
 * Usermod by Maximilian Mewes
 * E-mail: mewes.maximilian@gmx.de
 * Created at: 25.12.2022
 * If you have any questions, please feel free to contact me.
 */
class UsermodBattery : public Usermod {
  private:
    // --- Hardware ---
    int8_t batteryPin = USERMOD_BATTERY_MEASUREMENT_PIN;

    // --- Battery state ---
    UMBattery* bat = new LipoUMBattery();
    batteryConfig cfg;
    float alpha = USERMOD_BATTERY_AVERAGING_ALPHA;

    // --- Timing ---
    unsigned long initialDelay = USERMOD_BATTERY_INITIAL_DELAY;
    bool initialDelayComplete = false;
    bool isFirstVoltageReading = true;
    unsigned long readingInterval = USERMOD_BATTERY_MEASUREMENT_INTERVAL;
    unsigned long nextReadTime = 0;

    // --- Auto-off ---
    bool autoOffEnabled = USERMOD_BATTERY_AUTO_OFF_ENABLED;
    uint8_t autoOffThreshold = USERMOD_BATTERY_AUTO_OFF_THRESHOLD;

    // --- Low power indicator ---
    bool lowPowerIndicatorEnabled = USERMOD_BATTERY_LOW_POWER_INDICATOR_ENABLED;
    uint8_t lowPowerIndicatorPreset = USERMOD_BATTERY_LOW_POWER_INDICATOR_PRESET;
    uint8_t lowPowerIndicatorThreshold = USERMOD_BATTERY_LOW_POWER_INDICATOR_THRESHOLD;
    uint8_t lowPowerIndicatorReactivationThreshold = lowPowerIndicatorThreshold + 10;
    uint8_t lowPowerIndicatorDuration = USERMOD_BATTERY_LOW_POWER_INDICATOR_DURATION;
    bool lowPowerIndicationDone = false;
    bool lowPowerIndicatorActive = false;
    unsigned long lowPowerActivationTime = 0;
    uint8_t lastPreset = 0;

    // --- Charging detection ---
    bool charging = false;
    uint8_t chargingRiseCount = 0;
    float previousVoltage = 0.0f;
    static const uint8_t CHARGE_DETECT_THRESHOLD = 3;
    static const uint8_t CHARGE_COUNT_MAX = 6;
    static constexpr float CHARGE_VOLTAGE_DEADBAND = 0.01f;

    // --- Estimated runtime (requires INA226) ---
    #ifdef USERMOD_INA226
    bool estimatedRuntimeEnabled = USERMOD_BATTERY_ESTIMATED_RUNTIME_ENABLED;
    uint16_t batteryCapacity = USERMOD_BATTERY_CAPACITY;
    int32_t estimatedTimeLeft = -1;
    float smoothedEstimate = -1.0f;
    static constexpr float ESTIMATE_SMOOTHING = 0.3f;
    #endif

    // --- Inter-usermod data exchange ---
    float umVoltage = 0.0f;
    int8_t umLevel = -1;

    // --- State ---
    bool initDone = false;
    bool initializing = true;
    bool HomeAssistantDiscovery = false;

    // --- PROGMEM strings (used more than twice) ---
    static const char _name[];
    static const char _readInterval[];
    static const char _enabled[];
    static const char _threshold[];
    static const char _preset[];
    static const char _duration[];
    static const char _init[];
    static const char _haDiscovery[];

    // =============================================
    //  Private helpers
    // =============================================

    float dot2round(float x) {
      float nx = (int)(x * 100 + .5);
      return (float)(nx / 100);
    }

    void turnOff() {
      bri = 0;
      stateUpdated(CALL_MODE_DIRECT_CHANGE);
    }

    void lowPowerIndicator() {
      if (!lowPowerIndicatorEnabled) return;
      if (batteryPin < 0) return;
      if (lowPowerIndicationDone && lowPowerIndicatorReactivationThreshold <= bat->getLevel()) lowPowerIndicationDone = false;
      if (lowPowerIndicatorThreshold <= bat->getLevel()) return;
      if (lowPowerIndicationDone) return;
      if (!lowPowerIndicatorActive) {
        lowPowerIndicatorActive = true;
        lowPowerActivationTime = millis();
        lastPreset = currentPreset;
        applyPreset(lowPowerIndicatorPreset);
      }

      if (millis() - lowPowerActivationTime >= (unsigned long)lowPowerIndicatorDuration * 1000) {
        lowPowerIndicationDone = true;
        lowPowerIndicatorActive = false;
        applyPreset(lastPreset);
      }
    }

    float readVoltage() {
      #ifdef ARDUINO_ARCH_ESP32
        return (analogReadMilliVolts(batteryPin) / 1000.0f) * bat->getVoltageMultiplier() + bat->getCalibration();
      #else
        return (analogRead(batteryPin) / 1023.0f) * bat->getVoltageMultiplier() + bat->getCalibration();
      #endif
    }

    #ifdef USERMOD_INA226
    float getINA226Current() {
      um_data_t *data = nullptr;
      if (!UsermodManager::getUMData(&data, USERMOD_ID_INA226) || !data) return -1.0f;
      return *(float*)data->u_data[0];
    }
    #endif

#ifndef WLED_DISABLE_MQTT
    void addMqttSensor(const String &name, const String &type, const String &topic, const String &deviceClass, const String &unitOfMeasurement = "", const bool &isDiagnostic = false) {
      StaticJsonDocument<600> doc;
      char uid[128], json_str[1024], buf[128];

      doc[F("name")] = name;
      doc[F("stat_t")] = topic;
      String nameLower = name;
      nameLower.toLowerCase();
      sprintf_P(uid, PSTR("%s_%s_%s"), escapedMac.c_str(), nameLower.c_str(), type);
      doc[F("uniq_id")] = uid;
      doc[F("dev_cla")] = deviceClass;
      doc[F("exp_aft")] = 1800;

      if (type == "binary_sensor") {
        doc[F("pl_on")]  = "on";
        doc[F("pl_off")] = "off";
      }

      if (unitOfMeasurement != "")
        doc[F("unit_of_measurement")] = unitOfMeasurement;

      if (isDiagnostic)
        doc[F("entity_category")] = "diagnostic";

      JsonObject device = doc.createNestedObject(F("device"));
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

    void publishMqtt(const char* topic, const char* state) {
      if (WLED_MQTT_CONNECTED) {
        char buf[128];
        snprintf_P(buf, 127, PSTR("%s/%s"), mqttDeviceTopic, topic);
        mqtt->publish(buf, 0, false, state);
      }
    }
#endif

  public:
    // =============================================
    //  Factory & Lifecycle
    // =============================================

    static UMBattery* createBattery(batteryType type) {
      switch (type) {
        case lion:    return new LionUMBattery();
        case lifepo4: return new Lifepo4UMBattery();
        case lipo:
        default:      return new LipoUMBattery();
      }
    }

    void setup() {
      delete bat;
      bat = createBattery(cfg.type);
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
          batteryPin = -1;
        } else {
          pinMode(batteryPin, INPUT);
        }
      #else // ESP8266 boards have only one analog input pin A0
        pinMode(batteryPin, INPUT);
      #endif

      nextReadTime = millis() + initialDelay;

      // expose battery data for other usermods via getUMData()
      if (!um_data) {
        um_data = new um_data_t;
        um_data->u_size = 4;
        um_data->u_type = new um_types_t[4];
        um_data->u_data = new void*[4];
        um_data->u_data[0] = &umVoltage;   // float, Volts
        um_data->u_type[0] = UMT_FLOAT;
        um_data->u_data[1] = &umLevel;     // int8_t, 0-100%
        um_data->u_type[1] = UMT_BYTE;
        um_data->u_data[2] = &charging;    // bool
        um_data->u_type[2] = UMT_BYTE;
        um_data->u_data[3] = &cfg.type;    // batteryType enum (1=lipo,2=lion,3=lifepo4)
        um_data->u_type[3] = UMT_BYTE;
      }

      initDone = true;
    }

    void loop() {
      if (strip.isUpdating()) return;

      lowPowerIndicator();

      if (!initialDelayComplete && millis() < nextReadTime) return;

      if (!initialDelayComplete) {
        initialDelayComplete = true;
        nextReadTime = millis() + readingInterval;
      }

      if (isFirstVoltageReading) {
        bat->setVoltage(readVoltage());
        isFirstVoltageReading = false;
      }

      if (millis() < nextReadTime) return;
      nextReadTime = millis() + readingInterval;

      if (batteryPin < 0) return;

      initializing = false;
      float rawValue = readVoltage();

      // exponential smoothing — ADC in ESP32 fluctuates too much for single readout
      float filteredVoltage = bat->getVoltage() + alpha * (rawValue - bat->getVoltage());

      bat->setVoltage(filteredVoltage);
      bat->calculateAndSetLevel(filteredVoltage);

      umVoltage = filteredVoltage;
      umLevel = bat->getLevel();

      // charging detection based on voltage trend
      if (previousVoltage > 0.0f) {
        if (filteredVoltage > previousVoltage + CHARGE_VOLTAGE_DEADBAND) {
          chargingRiseCount = min((uint8_t)(chargingRiseCount + 1), CHARGE_COUNT_MAX);
        } else if (filteredVoltage < previousVoltage - CHARGE_VOLTAGE_DEADBAND) {
          if (chargingRiseCount > 0) chargingRiseCount--;
        }
        charging = (chargingRiseCount >= CHARGE_DETECT_THRESHOLD);
      }
      previousVoltage = filteredVoltage;

      // estimated runtime via INA226 current sensor
      #ifdef USERMOD_INA226
      if (estimatedRuntimeEnabled) {
        float current_A = getINA226Current();
        if (!charging && current_A > 0.01f && batteryCapacity > 0 && bat->getLevel() > 0) {
          float remaining_Ah = (bat->getLevel() / 100.0f) * (batteryCapacity / 1000.0f);
          float rawEstimate = min(remaining_Ah / current_A * 60.0f, 14400.0f);

          if (smoothedEstimate < 0.0f) {
            smoothedEstimate = rawEstimate;
          } else {
            smoothedEstimate = ESTIMATE_SMOOTHING * rawEstimate + (1.0f - ESTIMATE_SMOOTHING) * smoothedEstimate;
          }
          estimatedTimeLeft = (int32_t)smoothedEstimate;
        } else {
          estimatedTimeLeft = -1;
          smoothedEstimate = -1.0f;
        }
      }
      #endif

      // auto-off
      if (autoOffEnabled && (autoOffThreshold >= bat->getLevel()))
        turnOff();

#ifndef WLED_DISABLE_MQTT
      publishMqtt("battery", String(bat->getLevel(), 0).c_str());
      publishMqtt("voltage", String(bat->getVoltage()).c_str());
      publishMqtt("charging", charging ? "on" : "off");
      #ifdef USERMOD_INA226
      if (estimatedRuntimeEnabled && estimatedTimeLeft >= 0) {
        publishMqtt("runtime", String(estimatedTimeLeft).c_str());
      }
      #endif
#endif
    }

    uint16_t getId() {
      return USERMOD_ID_BATTERY;
    }

    /*
     * Battery data exposed to other usermods via getUMData():
     *   slot 0: voltage  (float, Volts)
     *   slot 1: level    (int8_t, 0-100%)
     *   slot 2: charging (bool)
     *   slot 3: type     (uint8_t, 1=lipo,2=lion,3=lifepo4)
     */
    bool getUMData(um_data_t **data) override {
      if (!data || !initDone) return false;
      *data = um_data;
      return true;
    }

    // =============================================
    //  JSON Info & State
    // =============================================

    void addToJsonInfo(JsonObject& root) {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      if (batteryPin < 0) {
        JsonArray infoVoltage = user.createNestedArray(F("Battery voltage"));
        infoVoltage.add(F("n/a"));
        infoVoltage.add(F(" invalid GPIO"));
        return;
      }

      JsonArray infoPercentage = user.createNestedArray(F("Battery level"));
      JsonArray infoVoltage = user.createNestedArray(F("Battery voltage"));
      JsonArray infoCharging = user.createNestedArray(F("Battery charging"));
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

      infoCharging.add(charging ? F("Yes") : F("No"));

      #ifdef USERMOD_INA226
      if (estimatedRuntimeEnabled) {
        JsonArray infoRuntime = user.createNestedArray(F("Est. runtime"));
        if (charging) {
          infoRuntime.add(F("charging"));
        } else if (estimatedTimeLeft < 0) {
          infoRuntime.add(F("calculating"));
        } else if (estimatedTimeLeft < 60) {
          infoRuntime.add(estimatedTimeLeft);
          infoRuntime.add(F(" min"));
        } else {
          char buf[16];
          snprintf_P(buf, sizeof(buf), PSTR("%dh %dm"), estimatedTimeLeft / 60, estimatedTimeLeft % 60);
          infoRuntime.add(buf);
        }
      }
      #endif
    }

    void addToJsonState(JsonObject& root) {
      JsonObject battery = root.createNestedObject(FPSTR(_name));
      addBatteryToJsonObject(battery, true);
      DEBUG_PRINTLN(F("Battery state exposed in JSON API."));
    }

    void addBatteryToJsonObject(JsonObject& battery, bool forJsonState) {
      if (forJsonState) {
        battery[F("type")] = cfg.type;
        battery[F("charging")] = charging;
        #ifdef USERMOD_INA226
        if (estimatedRuntimeEnabled) {
          battery[F("estimated-runtime")] = estimatedTimeLeft;
        }
        #endif
      } else {
        battery[F("type")] = (String)cfg.type;
      }
      battery[F("min-voltage")] = bat->getMinVoltage();
      battery[F("max-voltage")] = bat->getMaxVoltage();
      battery[F("calibration")] = bat->getCalibration();
      battery[F("voltage-multiplier")] = bat->getVoltageMultiplier();
      #ifdef USERMOD_INA226
      battery[F("capacity")] = batteryCapacity;
      battery[F("estimated-runtime-enabled")] = estimatedRuntimeEnabled;
      #endif
      battery[FPSTR(_readInterval)] = readingInterval;
      battery[FPSTR(_haDiscovery)] = HomeAssistantDiscovery;

      JsonObject ao = battery.createNestedObject(F("auto-off"));
      ao[FPSTR(_enabled)] = autoOffEnabled;
      ao[FPSTR(_threshold)] = autoOffThreshold;

      JsonObject lp = battery.createNestedObject(F("indicator"));
      lp[FPSTR(_enabled)] = lowPowerIndicatorEnabled;
      lp[FPSTR(_preset)] = lowPowerIndicatorPreset;
      lp[FPSTR(_threshold)] = lowPowerIndicatorThreshold;
      lp[FPSTR(_duration)] = lowPowerIndicatorDuration;
    }

    // =============================================
    //  Configuration
    // =============================================

    void addToConfig(JsonObject& root) {
      JsonObject battery = root.createNestedObject(FPSTR(_name));

      #ifdef ARDUINO_ARCH_ESP32
        battery[F("pin")] = batteryPin;
      #endif

      addBatteryToJsonObject(battery, false);

      // re-read voltage in case calibration or multiplier changed
      bat->setVoltage(readVoltage());

      DEBUG_PRINTLN(F("Battery config saved."));
    }

    void appendConfigData() {
      oappend(F("td=addDropdown('Battery','type');"));              // 34 Bytes
      oappend(F("addOption(td,'LiPo','1');"));                      // 26 Bytes
      oappend(F("addOption(td,'LiOn','2');"));                      // 26 Bytes
      oappend(F("addOption(td,'LiFePO4','3');"));                    // 30 Bytes
      oappend(F("addInfo('Battery:type',1,'<small style=\"color:orange\">requires reboot</small>');")); // 81 Bytes
      oappend(F("addInfo('Battery:min-voltage',1,'v');"));          // 38 Bytes
      oappend(F("addInfo('Battery:max-voltage',1,'v');"));          // 38 Bytes
      #ifdef USERMOD_INA226
      oappend(F("addInfo('Battery:capacity',1,'mAh');"));                // 37 Bytes
      oappend(F("addInfo('Battery:estimated-runtime-enabled',1,'');"));  // 51 Bytes
      #endif
      oappend(F("addInfo('Battery:interval',1,'ms');"));            // 36 Bytes
      oappend(F("addInfo('Battery:HA-discovery',1,'');"));          // 38 Bytes
      oappend(F("addInfo('Battery:auto-off:threshold',1,'%');"));   // 45 Bytes
      oappend(F("addInfo('Battery:indicator:threshold',1,'%');"));  // 46 Bytes
      oappend(F("addInfo('Battery:indicator:duration',1,'s');"));   // 45 Bytes
    }

    // Called BEFORE setup() on boot and on settings save
    bool readFromConfig(JsonObject& root) {
      #ifdef ARDUINO_ARCH_ESP32
        int8_t newBatteryPin = batteryPin;
      #endif

      JsonObject battery = root[FPSTR(_name)];
      if (battery.isNull()) {
        DEBUG_PRINT(FPSTR(_name));
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      #ifdef ARDUINO_ARCH_ESP32
        newBatteryPin = battery[F("pin")] | newBatteryPin;
      #endif

      // read directly into bat (handles zero values correctly, unlike bat->update)
      bat->setMinVoltage(battery[F("min-voltage")] | bat->getMinVoltage());
      bat->setMaxVoltage(battery[F("max-voltage")] | bat->getMaxVoltage());
      bat->setCalibration(battery[F("calibration")] | bat->getCalibration());
      bat->setVoltageMultiplier(battery[F("voltage-multiplier")] | bat->getVoltageMultiplier());

      // read into cfg struct for bat->update()
      getJsonValue(battery[F("type")], cfg.type);
      getJsonValue(battery[F("min-voltage")], cfg.minVoltage);
      getJsonValue(battery[F("max-voltage")], cfg.maxVoltage);
      getJsonValue(battery[F("calibration")], cfg.calibration);
      getJsonValue(battery[F("voltage-multiplier")], cfg.voltageMultiplier);

      #ifdef USERMOD_INA226
      batteryCapacity = battery[F("capacity")] | batteryCapacity;
      estimatedRuntimeEnabled = battery[F("estimated-runtime-enabled")] | estimatedRuntimeEnabled;
      #endif

      setReadingInterval(battery[FPSTR(_readInterval)] | readingInterval);
      HomeAssistantDiscovery = battery[FPSTR(_haDiscovery)] | HomeAssistantDiscovery;

      JsonObject ao = battery[F("auto-off")];
      autoOffEnabled = ao[FPSTR(_enabled)] | autoOffEnabled;
      setAutoOffThreshold(ao[FPSTR(_threshold)] | autoOffThreshold);

      JsonObject lp = battery[F("indicator")];
      lowPowerIndicatorEnabled = lp[FPSTR(_enabled)] | lowPowerIndicatorEnabled;
      lowPowerIndicatorPreset = lp[FPSTR(_preset)] | lowPowerIndicatorPreset;
      setLowPowerIndicatorThreshold(lp[FPSTR(_threshold)] | lowPowerIndicatorThreshold);
      lowPowerIndicatorReactivationThreshold = lowPowerIndicatorThreshold + 10;
      lowPowerIndicatorDuration = lp[FPSTR(_duration)] | lowPowerIndicatorDuration;

      if (initDone)
        bat->update(cfg);

      #ifdef ARDUINO_ARCH_ESP32
        if (!initDone) {
          batteryPin = newBatteryPin;
          DEBUG_PRINTLN(F(" config loaded."));
        } else {
          DEBUG_PRINTLN(F(" config (re)loaded."));
          if (newBatteryPin != batteryPin) {
            PinManager::deallocatePin(batteryPin, PinOwner::UM_Battery);
            batteryPin = newBatteryPin;
            setup();
          }
        }
      #endif

      bool configComplete = !battery[FPSTR(_readInterval)].isNull()
          && !battery[F("type")].isNull()
          && !battery[F("auto-off")].isNull()
          && !battery[F("indicator")].isNull();
      #ifdef USERMOD_INA226
      configComplete = configComplete && !battery[F("estimated-runtime-enabled")].isNull();
      #endif
      return configComplete;
    }

    // =============================================
    //  MQTT
    // =============================================

#ifndef WLED_DISABLE_MQTT
    void onMqttConnect(bool sessionPresent) {
      if (!HomeAssistantDiscovery) return;

      registerMqttSensor("battery",  F("Battery"),  "sensor",        "battery",          "%");
      registerMqttSensor("voltage",  F("Voltage"),  "sensor",        "voltage",          "V");
      registerMqttSensor("charging", F("Charging"), "binary_sensor", "battery_charging");
      #ifdef USERMOD_INA226
      if (estimatedRuntimeEnabled) {
        registerMqttSensor("runtime",  F("Runtime"),  "sensor",        "duration",         "min");
      }
      #endif
    }

    void registerMqttSensor(const char* subtopic, const String &name, const char* type, const char* deviceClass, const char* unit = "", bool diagnostic = true) {
      char topic[128];
      snprintf_P(topic, 127, PSTR("%s/%s"), mqttDeviceTopic, subtopic);
      this->addMqttSensor(name, type, topic, deviceClass, unit, diagnostic);
    }
#endif

    // =============================================
    //  Validated setters
    // =============================================

    void setReadingInterval(unsigned long newReadingInterval) {
      readingInterval = max((unsigned long)3000, newReadingInterval);
    }

    void setAutoOffThreshold(int8_t threshold) {
      autoOffThreshold = min((int8_t)100, max((int8_t)0, threshold));
      autoOffThreshold = lowPowerIndicatorEnabled /*&& autoOffEnabled*/ ? min(lowPowerIndicatorThreshold - 1, (int)autoOffThreshold) : autoOffThreshold;
    }

    void setLowPowerIndicatorThreshold(int8_t threshold) {
      lowPowerIndicatorThreshold = threshold;
      lowPowerIndicatorThreshold = autoOffEnabled /*&& lowPowerIndicatorEnabled*/ ? max(autoOffThreshold + 1, (int)lowPowerIndicatorThreshold) : max(5, (int)lowPowerIndicatorThreshold);
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
