// force the compiler to show a warning to confirm that this file is included
#warning **** Included USERMOD_BH1750 ****

#ifndef WLED_ENABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

#pragma once

#include "wled.h"
#include <BH1750.h>

// the max frequency to check photoresistor, 10 seconds
#ifndef USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL
#define USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL 10000
#endif

// the min frequency to check photoresistor, 500 ms
#ifndef USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL
#define USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL 500
#endif

// how many seconds after boot to take first measurement, 10 seconds
#ifndef USERMOD_BH1750_FIRST_MEASUREMENT_AT
#define USERMOD_BH1750_FIRST_MEASUREMENT_AT 10000
#endif

// only report if difference grater than offset value
#ifndef USERMOD_BH1750_OFFSET_VALUE
#define USERMOD_BH1750_OFFSET_VALUE 1
#endif

class Usermod_BH1750 : public Usermod
{
private:
  int8_t offset = USERMOD_BH1750_OFFSET_VALUE;

  unsigned long maxReadingInterval = USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL;
  unsigned long minReadingInterval = USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL;
  unsigned long lastMeasurement = UINT32_MAX - (USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL - USERMOD_BH1750_FIRST_MEASUREMENT_AT);
  unsigned long lastSend = UINT32_MAX - (USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL - USERMOD_BH1750_FIRST_MEASUREMENT_AT);
  // flag to indicate we have finished the first readLightLevel call
  // allows this library to report to the user how long until the first
  // measurement
  bool getLuminanceComplete = false;

  // flag set at startup
  bool enabled = true;

  // strings to reduce flash memory usage (used more than twice)
  static const char _name[];
  static const char _enabled[];
  static const char _maxReadInterval[];
  static const char _minReadInterval[];
  static const char _offset[];
  static const char _HomeAssistantDiscovery[];

  bool initDone = false;
  bool sensorFound = false;

  // Home Assistant and MQTT  
  String mqttLuminanceTopic = F("");
  bool mqttInitialized = false;
  bool HomeAssistantDiscovery = true; // Publish Home Assistant Discovery messages

  BH1750 lightMeter;
  float lastLux = -1000;

  bool checkBoundSensor(float newValue, float prevValue, float maxDiff)
  {
    return isnan(prevValue) || newValue <= prevValue - maxDiff || newValue >= prevValue + maxDiff || (newValue == 0.0 && prevValue > 0.0);
  }
  
  // set up Home Assistant discovery entries
  void _mqttInitialize()
  {
    mqttLuminanceTopic = String(mqttDeviceTopic) + F("/brightness");

    if (HomeAssistantDiscovery) _createMqttSensor(F("Brightness"), mqttLuminanceTopic, F("Illuminance"), F(" lx"));
  }

  // Create an MQTT Sensor for Home Assistant Discovery purposes, this includes a pointer to the topic that is published to in the Loop.
  void _createMqttSensor(const String &name, const String &topic, const String &deviceClass, const String &unitOfMeasurement)
  {
    String t = String(F("homeassistant/sensor/")) + mqttClientID + F("/") + name + F("/config");
    
    StaticJsonDocument<600> doc;
    
    doc[F("name")] = String(serverDescription) + F(" ") + name;
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

public:
  void setup()
  {
    if (i2c_scl<0 || i2c_sda<0) { enabled = false; return; }
    sensorFound = lightMeter.begin();
    initDone = true;
  }

  void loop()
  {
    if ((!enabled) || strip.isUpdating())
      return;

    unsigned long now = millis();

    // check to see if we are due for taking a measurement
    // lastMeasurement will not be updated until the conversion
    // is complete the the reading is finished
    if (now - lastMeasurement < minReadingInterval)
    {
      return;
    }

    bool shouldUpdate = now - lastSend > maxReadingInterval;

    float lux = lightMeter.readLightLevel();
    lastMeasurement = millis();
    getLuminanceComplete = true;

    if (shouldUpdate || checkBoundSensor(lux, lastLux, offset))
    {
      lastLux = lux;
      lastSend = millis();
#ifndef WLED_DISABLE_MQTT
      if (WLED_MQTT_CONNECTED)
      {
        if (!mqttInitialized)
          {
            _mqttInitialize();
            mqttInitialized = true;
          }
        mqtt->publish(mqttLuminanceTopic.c_str(), 0, true, String(lux).c_str());
        DEBUG_PRINTLN(F("Brightness: ") + String(lux) + F("lx"));
      }
      else
      {
        DEBUG_PRINTLN(F("Missing MQTT connection. Not publishing data"));
      }
#endif
    }
  }

  inline float getIlluminance() {
    return (float)lastLux;
  }

  void addToJsonInfo(JsonObject &root)
  {
    JsonObject user = root[F("u")];
    if (user.isNull())
      user = root.createNestedObject(F("u"));

    JsonArray lux_json = user.createNestedArray(F("Luminance"));
    if (!enabled) {
      lux_json.add(F("disabled"));
    } else if (!sensorFound) {
        // if no sensor 
        lux_json.add(F("BH1750 "));
        lux_json.add(F("Not Found"));
    } else if (!getLuminanceComplete) {
      // if we haven't read the sensor yet, let the user know
        // that we are still waiting for the first measurement
        lux_json.add((USERMOD_BH1750_FIRST_MEASUREMENT_AT - millis()) / 1000);
        lux_json.add(F(" sec until read"));
        return;
    } else {
      lux_json.add(lastLux);
      lux_json.add(F(" lx"));
    }
  }

  // (called from set.cpp) stores persistent properties to cfg.json
  void addToConfig(JsonObject &root)
  {
    // we add JSON object.
    JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
    top[FPSTR(_enabled)] = enabled;
    top[FPSTR(_maxReadInterval)] = maxReadingInterval;
    top[FPSTR(_minReadInterval)] = minReadingInterval;
    top[FPSTR(_HomeAssistantDiscovery)] = HomeAssistantDiscovery;
    top[FPSTR(_offset)] = offset;

    DEBUG_PRINTLN(F("BH1750 config saved."));
  }

  // called before setup() to populate properties from values stored in cfg.json
  bool readFromConfig(JsonObject &root)
  {
    // we look for JSON object.
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull())
    {
      DEBUG_PRINT(FPSTR(_name));
      DEBUG_PRINT(F("BH1750"));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }
    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, false);
    configComplete &= getJsonValue(top[FPSTR(_maxReadInterval)], maxReadingInterval, 10000); //ms
    configComplete &= getJsonValue(top[FPSTR(_minReadInterval)], minReadingInterval, 500); //ms
    configComplete &= getJsonValue(top[FPSTR(_HomeAssistantDiscovery)], HomeAssistantDiscovery, false);
    configComplete &= getJsonValue(top[FPSTR(_offset)], offset, 1);

    DEBUG_PRINT(FPSTR(_name));
    if (!initDone) {
      DEBUG_PRINTLN(F(" config loaded."));
    } else {
      DEBUG_PRINTLN(F(" config (re)loaded."));
    }

    return configComplete;
    
  }

  uint16_t getId()
  {
    return USERMOD_ID_BH1750;
  }

};

// strings to reduce flash memory usage (used more than twice)
const char Usermod_BH1750::_name[] PROGMEM = "BH1750";
const char Usermod_BH1750::_enabled[] PROGMEM = "enabled";
const char Usermod_BH1750::_maxReadInterval[] PROGMEM = "max-read-interval-ms";
const char Usermod_BH1750::_minReadInterval[] PROGMEM = "min-read-interval-ms";
const char Usermod_BH1750::_HomeAssistantDiscovery[] PROGMEM = "HomeAssistantDiscoveryLux";
const char Usermod_BH1750::_offset[] PROGMEM = "offset-lx";
