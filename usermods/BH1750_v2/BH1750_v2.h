
#pragma once
#include "wled.h"
#include <BH1750.h>

#ifdef WLED_DISABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

// the max frecuencia to verificar photoresistor, 10 seconds
#ifndef USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL
#define USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL 10000
#endif

// the min frecuencia to verificar photoresistor, 500 ms
#ifndef USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL
#define USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL 500
#endif

// how many seconds after boot to take first measurement, 10 seconds
#ifndef USERMOD_BH1750_FIRST_MEASUREMENT_AT
#define USERMOD_BH1750_FIRST_MEASUREMENT_AT 10000
#endif

// only report if difference grater than desplazamiento valor
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
  // bandera to indicate we have finished the first readLightLevel call
  // allows this biblioteca to report to the usuario how long until the first
  // measurement
  bool getLuminanceComplete = false;

  // bandera set at startup
  bool enabled = true;

  // strings to reduce flash memoria usage (used more than twice)
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

  // Crear an MQTT Sensor for Home Assistant Discovery purposes, this includes a pointer to the topic that is published to in the Bucle.
  void _createMqttSensor(const String &name, const String &topic, const String &deviceClass, const String &unitOfMeasurement);

public:
  void setup();
  void loop();
  inline float getIlluminance()  {
    return (float)lastLux;
  }

  void addToJsonInfo(JsonObject &root);

  // (called from set.cpp) stores persistent properties to cfg.JSON
  void addToConfig(JsonObject &root);

  // called before configuración() to populate properties from values stored in cfg.JSON
  bool readFromConfig(JsonObject &root);

  inline uint16_t getId()
  {
    return USERMOD_ID_BH1750;
  }

};
