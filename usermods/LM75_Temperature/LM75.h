#pragma once

#include "wled.h"
#include "Temperature_LM75_Derived.h"

// the frequency to check temperature, 1 minute
#ifndef USERMOD_LM75TEMPERATURE_MEASUREMENT_INTERVAL
#define USERMOD_LM75TEMPERATURE_MEASUREMENT_INTERVAL 60000
#endif

// auto-off feature
#ifndef USERMOD_LM75TEMPERATURE_AUTO_OFF_ENABLED
  #define USERMOD_LM75TEMPERATURE_AUTO_OFF_ENABLED true
#endif

#ifndef USERMOD_LM75TEMPERATURE_AUTO_OFF_HIGH_THRESHOLD
  #define USERMOD_LM75TEMPERATURE_AUTO_OFF_HIGH_THRESHOLD 60
#endif

#ifndef USERMOD_LM75TEMPERATURE_AUTO_OFF_LOW_THRESHOLD
  #define USERMOD_LM75TEMPERATURE_AUTO_OFF_LOW_THRESHOLD 40
#endif

#ifndef USERMOD_LM75TEMPERATURE_AUTO_TEMPERATURE_OFF_ENABLED
  #define USERMOD_LM75TEMPERATURE_AUTO_TEMPERATURE_OFF_ENABLED true
#endif

#ifndef USERMOD_LM75TEMPERATURE_I2C_ADRESS
  #define USERMOD_LM75TEMPERATURE_I2C_ADRESS 0x48
#endif

//Generic_LM75 LM75temperature(0x4F);

class UsermodLM75Temperature : public Usermod {

  private:
    bool initDone = false;

    // measurement unit (true==°C, false==°F)
    bool degC = true;

    // how often do we read from sensor?
    unsigned long readingInterval = USERMOD_LM75TEMPERATURE_MEASUREMENT_INTERVAL;
    // set last reading as "40 sec before boot", so first reading is taken after 20 sec
    unsigned long lastMeasurement = UINT32_MAX - USERMOD_LM75TEMPERATURE_MEASUREMENT_INTERVAL;
    // last time requestTemperatures was called
    // used to determine when we can read the sensors temperature
    // we have to wait at least 93.75 ms after requestTemperatures() is called
    unsigned long lastTemperaturesRequest;
    float temperature;
    // flag set at startup if LM75 sensor not found, avoids trying to keep getting
    // temperature if flashed to a board without a sensor attached
    byte sensorFound;
    // Temperature limits for master off feature
    uint8_t autoOffHighThreshold = USERMOD_LM75TEMPERATURE_AUTO_OFF_HIGH_THRESHOLD;
    uint8_t autoOffLowThreshold = USERMOD_LM75TEMPERATURE_AUTO_OFF_LOW_THRESHOLD;
    // Has an overtemperature event occured ?
    bool overtemptriggered = false;

    bool enabled = USERMOD_LM75TEMPERATURE_AUTO_OFF_ENABLED;

    bool HApublished = false;

    // auto shutdown/shutoff/master off feature
    bool autoOffEnabled = USERMOD_LM75TEMPERATURE_AUTO_TEMPERATURE_OFF_ENABLED;

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _readInterval[];
    static const char _ao[];
    static const char _thresholdhigh[];
    static const char _thresholdlow[];

    // Functions for LM75 sensor
    void readTemperature();
    bool findSensor();
#ifndef WLED_DISABLE_MQTT
    void publishHomeAssistantAutodiscovery();
#endif

    /*
     * API calls te enable data exchan)ge between WLED modules
     */
    float getTemperature();
    const char *getTemperatureUnit();
    uint16_t getId() { return USERMOD_ID_LM75TEMPERATURE; }

    void setup();
    void loop();
    //void connected();
#ifndef WLED_DISABLE_MQTT
    void onMqttConnect(bool sessionPresent);
#endif

    void addToJsonInfo(JsonObject& root);
    void addToConfig(JsonObject &root);
    bool readFromConfig(JsonObject &root);

    void appendConfigData();

    void overtempfailure();
    void overtempreset();
    int8_t getAutoOffHighThreshold();
    int8_t getAutoOffLowThreshold();
    void setAutoOffHighThreshold(uint8_t threshold);
    void setAutoOffLowThreshold(uint8_t threshold);
};

