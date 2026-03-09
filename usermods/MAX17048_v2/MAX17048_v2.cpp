// force the compiler to show a warning to confirm that this file is included
#warning **** Included USERMOD_MAX17048 V2.0 ****

#include "wled.h"
#include "Adafruit_MAX1704X.h"


// the max interval to check battery level, 10 seconds
#ifndef USERMOD_MAX17048_MAX_MONITOR_INTERVAL
#define USERMOD_MAX17048_MAX_MONITOR_INTERVAL 10000
#endif

// the min  interval to check battery level, 500 ms
#ifndef USERMOD_MAX17048_MIN_MONITOR_INTERVAL
#define USERMOD_MAX17048_MIN_MONITOR_INTERVAL 500
#endif

// how many seconds after boot to perform the first check, 10 seconds
#ifndef USERMOD_MAX17048_FIRST_MONITOR_AT
#define USERMOD_MAX17048_FIRST_MONITOR_AT 10000
#endif

/* 
 * Usermod to display Battery Life using Adafruit's MAX17048 LiPoly/ LiIon Fuel Gauge and Battery Monitor.
 */
class  Usermod_MAX17048 : public Usermod {

  private:

    bool enabled = true;

    unsigned long maxReadingInterval = USERMOD_MAX17048_MAX_MONITOR_INTERVAL;
    unsigned long minReadingInterval = USERMOD_MAX17048_MIN_MONITOR_INTERVAL;
    unsigned long lastCheck = UINT32_MAX - (USERMOD_MAX17048_MAX_MONITOR_INTERVAL - USERMOD_MAX17048_FIRST_MONITOR_AT);
    unsigned long lastSend = UINT32_MAX - (USERMOD_MAX17048_MAX_MONITOR_INTERVAL - USERMOD_MAX17048_FIRST_MONITOR_AT);


    unsigned VoltageDecimals = 3;  // Number of decimal places in published voltage values
    unsigned PercentDecimals = 1;  // Number of decimal places in published percent values

    // string that are used multiple time (this will save some flash memory)
    static const char _name[];
    static const char _enabled[];
    static const char _maxReadInterval[];
    static const char _minReadInterval[];
    static const char _HomeAssistantDiscovery[];

    bool monitorFound = false;
    bool firstReadComplete = false;
    bool initDone = false;

    Adafruit_MAX17048 maxLipo;
    float lastBattVoltage = -10;
    float lastBattPercent = -1;

    // MQTT and Home Assistant Variables
    bool HomeAssistantDiscovery = false;    // Publish Home Assistant Device Information
    bool mqttInitialized = false; 

    void _mqttInitialize()
    {
        char mqttBatteryVoltageTopic[128];
        char mqttBatteryPercentTopic[128];

        snprintf_P(mqttBatteryVoltageTopic, 127, PSTR("%s/batteryVoltage"), mqttDeviceTopic);
        snprintf_P(mqttBatteryPercentTopic, 127, PSTR("%s/batteryPercent"), mqttDeviceTopic);

        if (HomeAssistantDiscovery) {
        _createMqttSensor(F("BatteryVoltage"), mqttBatteryVoltageTopic, "voltage", F("V"));
        _createMqttSensor(F("BatteryPercent"), mqttBatteryPercentTopic, "battery", F("%"));
        }
    }

    void _createMqttSensor(const String &name, const String &topic, const String &deviceClass, const String &unitOfMeasurement)
    {
        String t = String(F("homeassistant/sensor/")) + mqttClientID + F("/") + name + F("/config");

        StaticJsonDocument<600> doc;

        doc[F("name")] = String(serverDescription) + " " + name;
        doc[F("state_topic")] = topic;
        doc[F("unique_id")] = String(mqttClientID) + name;
        if (unitOfMeasurement != "")
        doc[F("unit_of_measurement")] = unitOfMeasurement;
        if (deviceClass != "")
        doc[F("device_class")] = deviceClass;
        doc[F("expire_after")] = 1800;

        JsonObject device = doc.createNestedObject(F("device")); // attach the sensor to the same device
        device[F("name")] = serverDescription;
        device[F("identifiers")] = "wled-sensor-" + String(mqttClientID);
        device[F("manufacturer")] = F("WLED");
        device[F("model")] = F("FOSS");
        device[F("sw_version")] = versionString;

        String temp;
        serializeJson(doc, temp);
        DEBUG_PRINTLN(t);
        DEBUG_PRINTLN(temp);

        mqtt->publish(t.c_str(), 0, true, temp.c_str());
    }

    void publishMqtt(const char *topic, const char* state) {
    #ifndef WLED_DISABLE_MQTT
      //Check if MQTT Connected, otherwise it will crash the 8266
      if (WLED_MQTT_CONNECTED){
        char subuf[128];
        snprintf_P(subuf, 127, PSTR("%s/%s"), mqttDeviceTopic, topic);
        mqtt->publish(subuf, 0, false, state);
      }
    #endif
    }

  public:

    inline void enable(bool enable) { enabled = enable; }

    inline bool isEnabled() { return enabled; }

    void setup() {
      // do your set-up here
      if (i2c_scl<0 || i2c_sda<0) { enabled = false; return; }
      monitorFound = maxLipo.begin();
      initDone = true;
    }

    void loop() {
      // if usermod is disabled or called during strip updating just exit
      // NOTE: on very long strips strip.isUpdating() may always return true so update accordingly
      if (!enabled || strip.isUpdating()) return;

        unsigned long now = millis();

        if (now - lastCheck < minReadingInterval) { return; }

        bool shouldUpdate = now - lastSend > maxReadingInterval;

        float battVoltage = maxLipo.cellVoltage();
        float battPercent = maxLipo.cellPercent();
        lastCheck = millis();
        firstReadComplete = true;

        if (shouldUpdate)
        {
          lastBattVoltage = roundf(battVoltage * powf(10, VoltageDecimals)) / powf(10, VoltageDecimals);
          lastBattPercent = roundf(battPercent * powf(10, PercentDecimals)) / powf(10, PercentDecimals);
          lastSend = millis();

          publishMqtt("batteryVoltage", String(lastBattVoltage, VoltageDecimals).c_str());
          publishMqtt("batteryPercent", String(lastBattPercent, PercentDecimals).c_str());
          DEBUG_PRINTLN(F("Battery Voltage: ") + String(lastBattVoltage, VoltageDecimals) + F("V"));
          DEBUG_PRINTLN(F("Battery Percent: ") + String(lastBattPercent, PercentDecimals) + F("%"));
        }
    }

    void onMqttConnect(bool sessionPresent)
    {
        if (WLED_MQTT_CONNECTED && !mqttInitialized)
        {
            _mqttInitialize();
            mqttInitialized = true;
        }
    }

    inline float getBatteryVoltageV() {
        return (float) lastBattVoltage;
    }

    inline float getBatteryPercent() {
        return (float) lastBattPercent;
    }

    void addToJsonInfo(JsonObject& root)
    {
      // if "u" object does not exist yet wee need to create it
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");


      JsonArray battery_json = user.createNestedArray(F("Battery Monitor"));
      if (!enabled) {
        battery_json.add(F("Disabled"));
      }
      else if(!monitorFound) {
        battery_json.add(F("MAX17048 Not Found"));
      }        
      else if (!firstReadComplete) {
        // if we haven't read the sensor yet, let the user know
        // that we are still waiting for the first measurement
        battery_json.add((USERMOD_MAX17048_FIRST_MONITOR_AT - millis()) / 1000);
        battery_json.add(F(" sec until read"));
      } else {
        battery_json.add(F("Enabled"));
        JsonArray voltage_json = user.createNestedArray(F("Battery Voltage"));
        voltage_json.add(lastBattVoltage);
        voltage_json.add(F("V"));
        JsonArray percent_json = user.createNestedArray(F("Battery Percent"));
        percent_json.add(lastBattPercent);
        percent_json.add(F("%"));
      }
    }

    void addToJsonState(JsonObject& root)
    {
        JsonObject usermod = root[FPSTR(_name)];
        if (usermod.isNull())
        {
        usermod = root.createNestedObject(FPSTR(_name));
        }
        usermod[FPSTR(_enabled)] = enabled;
    }

    void readFromJsonState(JsonObject& root)
    {
        JsonObject usermod = root[FPSTR(_name)];
        if (!usermod.isNull())
        {
            if (usermod[FPSTR(_enabled)].is<bool>())
            {
                enabled = usermod[FPSTR(_enabled)].as<bool>();
            }
        }
    }

    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_maxReadInterval)] = maxReadingInterval;
      top[FPSTR(_minReadInterval)] = minReadingInterval;
      top[FPSTR(_HomeAssistantDiscovery)] = HomeAssistantDiscovery;
      DEBUG_PRINT(F(_name));
      DEBUG_PRINTLN(F(" config saved."));
    }

    bool readFromConfig(JsonObject& root)
    {
      JsonObject top = root[FPSTR(_name)];

      if (top.isNull()) {
        DEBUG_PRINT(F(_name));
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_maxReadInterval)], maxReadingInterval, USERMOD_MAX17048_MAX_MONITOR_INTERVAL);
      configComplete &= getJsonValue(top[FPSTR(_minReadInterval)], minReadingInterval, USERMOD_MAX17048_MIN_MONITOR_INTERVAL);
      configComplete &= getJsonValue(top[FPSTR(_HomeAssistantDiscovery)], HomeAssistantDiscovery, false);

      DEBUG_PRINT(FPSTR(_name));
      if (!initDone) {
        // first run: reading from cfg.json
        DEBUG_PRINTLN(F(" config loaded."));
      } else {
        DEBUG_PRINTLN(F(" config (re)loaded."));
        // changing parameters from settings page
      }

      return configComplete;
    }

    uint16_t getId()
    {
      return USERMOD_ID_MAX17048;
    }

};


// add more strings here to reduce flash memory usage
const char Usermod_MAX17048::_name[]    PROGMEM = "Adafruit MAX17048 Battery Monitor";
const char Usermod_MAX17048::_enabled[] PROGMEM = "enabled";
const char Usermod_MAX17048::_maxReadInterval[] PROGMEM = "max-read-interval-ms";
const char Usermod_MAX17048::_minReadInterval[] PROGMEM = "min-read-interval-ms";
const char Usermod_MAX17048::_HomeAssistantDiscovery[] PROGMEM = "HomeAssistantDiscovery";


static Usermod_MAX17048 max17048_v2;
REGISTER_USERMOD(max17048_v2);