
#pragma once
#include "wled.h"
#include <BH1750.h>

#ifdef WLED_DISABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

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
  String mqttLuminanceTopic;
  bool mqttInitialized = false;
  bool HomeAssistantDiscovery = true; // Publish Home Assistant Discovery messages

  BH1750 lightMeter;
  float lastLux = -1000;

  // set up Home Assistant discovery entries
  void _mqttInitialize();

  // Create an MQTT Sensor for Home Assistant Discovery purposes, this includes a pointer to the topic that is published to in the Loop.
  void _createMqttSensor(const String &name, const String &topic, const String &deviceClass, const String &unitOfMeasurement);

public:
  void setup();
  void loop();
  inline float getIlluminance()  {
    return (float)lastLux;
  }

  void addToJsonInfo(JsonObject &root);

  // (called from set.cpp) stores persistent properties to cfg.json
  void addToConfig(JsonObject &root);

  // called before setup() to populate properties from values stored in cfg.json
  bool readFromConfig(JsonObject &root);

  inline uint16_t getId()
  {
    return USERMOD_ID_BH1750;
  }

};
