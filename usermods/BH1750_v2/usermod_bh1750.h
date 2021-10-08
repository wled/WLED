#pragma once

#include "wled.h"
#include <Wire.h>
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

// only report if differance grater than offset value
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
  bool disabled = false;

  // strings to reduce flash memory usage (used more than twice)
  static const char _name[];
  static const char _enabled[];
  static const char _maxReadInterval[];
  static const char _minReadInterval[];
  static const char _offset[];

  BH1750 lightMeter;
  float lastLux = -1000;

  bool checkBoundSensor(float newValue, float prevValue, float maxDiff)
  {
    return isnan(prevValue) || newValue <= prevValue - maxDiff || newValue >= prevValue + maxDiff || (newValue == 0.0 && prevValue > 0.0);
  }

public:
  void setup()
  {
    Wire.begin();
    lightMeter.begin();
  }

  void loop()
  {
    if (disabled || strip.isUpdating())
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
      if (WLED_MQTT_CONNECTED)
      {
        char subuf[45];
        strcpy(subuf, mqttDeviceTopic);
        strcat_P(subuf, PSTR("/luminance"));
        mqtt->publish(subuf, 0, true, String(lux).c_str());
      }
      else
      {
        DEBUG_PRINTLN("Missing MQTT connection. Not publishing data");
      }
    }
  }

  void addToJsonInfo(JsonObject &root)
  {
    JsonObject user = root[F("u")];
    if (user.isNull())
      user = root.createNestedObject(F("u"));

    JsonArray lux_json = user.createNestedArray(F("Luminance"));

    if (!getLuminanceComplete)
    {
      // if we haven't read the sensor yet, let the user know
      // that we are still waiting for the first measurement
      lux_json.add((USERMOD_BH1750_FIRST_MEASUREMENT_AT - millis()) / 1000);
      lux_json.add(F(" sec until read"));
      return;
    }

    lux_json.add(lastLux);
    lux_json.add(F(" lx"));
  }

  uint16_t getId()
  {
    return USERMOD_ID_BH1750;
  }

  /**
     * addToConfig() (called from set.cpp) stores persistent properties to cfg.json
     */
  void addToConfig(JsonObject &root)
  {
    // we add JSON object.
    JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
    top[FPSTR(_enabled)] = !disabled;
    top[FPSTR(_maxReadInterval)] = maxReadingInterval;
    top[FPSTR(_minReadInterval)] = minReadingInterval;
    top[FPSTR(_offset)] = offset;

    DEBUG_PRINTLN(F("Photoresistor config saved."));
  }

  /**
  * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
  */
  bool readFromConfig(JsonObject &root)
  {
    // we look for JSON object.
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull())
    {
      DEBUG_PRINT(FPSTR(_name));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }

    disabled = !(top[FPSTR(_enabled)] | !disabled);
    maxReadingInterval = (top[FPSTR(_maxReadInterval)] | maxReadingInterval); // ms
    minReadingInterval = (top[FPSTR(_minReadInterval)] | minReadingInterval); // ms
    offset = top[FPSTR(_offset)] | offset;
    DEBUG_PRINT(FPSTR(_name));
    DEBUG_PRINTLN(F(" config (re)loaded."));

    // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
    return true;
  }
};

// strings to reduce flash memory usage (used more than twice)
const char Usermod_BH1750::_name[] PROGMEM = "BH1750";
const char Usermod_BH1750::_enabled[] PROGMEM = "enabled";
const char Usermod_BH1750::_maxReadInterval[] PROGMEM = "max-read-interval-ms";
const char Usermod_BH1750::_minReadInterval[] PROGMEM = "min-read-interval-ms";
const char Usermod_BH1750::_offset[] PROGMEM = "offset-lx";
