#pragma once
#include "wled.h"

// the frequency to check photoresistor, 10 seconds
#ifndef USERMOD_SN_PHOTORESISTOR_MEASUREMENT_INTERVAL
#define USERMOD_SN_PHOTORESISTOR_MEASUREMENT_INTERVAL 10000
#endif

// how many seconds after boot to take first measurement, 10 seconds
#ifndef USERMOD_SN_PHOTORESISTOR_FIRST_MEASUREMENT_AT
#define USERMOD_SN_PHOTORESISTOR_FIRST_MEASUREMENT_AT 10000
#endif

// supplied voltage
#ifndef USERMOD_SN_PHOTORESISTOR_REFERENCE_VOLTAGE
#define USERMOD_SN_PHOTORESISTOR_REFERENCE_VOLTAGE 5
#endif

// 10 bits
#ifndef USERMOD_SN_PHOTORESISTOR_ADC_PRECISION
#define USERMOD_SN_PHOTORESISTOR_ADC_PRECISION 1024.0f
#endif

// resistor size 10K hms
#ifndef USERMOD_SN_PHOTORESISTOR_RESISTOR_VALUE
#define USERMOD_SN_PHOTORESISTOR_RESISTOR_VALUE 10000.0f
#endif

// only report if difference grater than offset value
#ifndef USERMOD_SN_PHOTORESISTOR_OFFSET_VALUE
#define USERMOD_SN_PHOTORESISTOR_OFFSET_VALUE 5
#endif

class Usermod_SN_Photoresistor : public Usermod
{
private:
  float referenceVoltage = USERMOD_SN_PHOTORESISTOR_REFERENCE_VOLTAGE;
  float resistorValue = USERMOD_SN_PHOTORESISTOR_RESISTOR_VALUE;
  float adcPrecision = USERMOD_SN_PHOTORESISTOR_ADC_PRECISION;
  int8_t offset = USERMOD_SN_PHOTORESISTOR_OFFSET_VALUE;

  unsigned long readingInterval = USERMOD_SN_PHOTORESISTOR_MEASUREMENT_INTERVAL;
  // set last reading as "40 sec before boot", so first reading is taken after 20 sec
  unsigned long lastMeasurement = UINT32_MAX - (USERMOD_SN_PHOTORESISTOR_MEASUREMENT_INTERVAL - USERMOD_SN_PHOTORESISTOR_FIRST_MEASUREMENT_AT);
  // flag to indicate we have finished the first getTemperature call
  // allows this library to report to the user how long until the first
  // measurement
  bool getLuminanceComplete = false;
  uint16_t lastLDRValue = 65535;

  // flag set at startup
  bool disabled = false;

  // strings to reduce flash memory usage (used more than twice)
  static const char _name[];
  static const char _enabled[];
  static const char _readInterval[];
  static const char _referenceVoltage[];
  static const char _resistorValue[];
  static const char _adcPrecision[];
  static const char _offset[];

  uint16_t getLuminance();

public:
  void setup();
  void loop();

  uint16_t getLastLDRValue()
  {
    return lastLDRValue;
  }

  void addToJsonInfo(JsonObject &root);

  uint16_t getId()
  {
    return USERMOD_ID_SN_PHOTORESISTOR;
  }

  /**
     * addToConfig() (called from set.cpp) stores persistent properties to cfg.json
     */
  void addToConfig(JsonObject &root);

  /**
  * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
  */
  bool readFromConfig(JsonObject &root);
};
