#pragma once
#include "wled.h"
#include "OneWire.h"

//Pin defaults for QuinLed Dig-Uno if not overriden
#ifndef TEMPERATURE_PIN
  #ifdef ARDUINO_ARCH_ESP32
    #define TEMPERATURE_PIN 18
  #else //ESP8266 boards
    #define TEMPERATURE_PIN 14
  #endif
#endif

// the frequency to check temperature, 1 minute
#ifndef USERMOD_DALLASTEMPERATURE_MEASUREMENT_INTERVAL
#define USERMOD_DALLASTEMPERATURE_MEASUREMENT_INTERVAL 60000
#endif

class UsermodTemperature : public Usermod {

  private:

    bool initDone = false;
    OneWire *oneWire;
    // GPIO pin used for sensor (with a default compile-time fallback)
    int8_t temperaturePin = TEMPERATURE_PIN;
    // measurement unit (true==°C, false==°F)
    bool degC = true;
    // using parasite power on the sensor
    bool parasite = false;
    int8_t parasitePin = -1;
    // how often do we read from sensor?
    unsigned long readingInterval = USERMOD_DALLASTEMPERATURE_MEASUREMENT_INTERVAL;
    // set last reading as "40 sec before boot", so first reading is taken after 20 sec
    unsigned long lastMeasurement = UINT32_MAX - USERMOD_DALLASTEMPERATURE_MEASUREMENT_INTERVAL;
    // last time requestTemperatures was called
    // used to determine when we can read the sensors temperature
    // we have to wait at least 93.75 ms after requestTemperatures() is called
    unsigned long lastTemperaturesRequest;
    float temperature;
    // indicates requestTemperatures has been called but the sensor measurement is not complete
    bool waitingForConversion = false;
    // flag set at startup if DS18B20 sensor not found, avoids trying to keep getting
    // temperature if flashed to a board without a sensor attached
    byte sensorFound;

    bool enabled = true;

    bool HApublished = false;
    int16_t idx = -1;   // Domoticz virtual sensor idx

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _readInterval[];
    static const char _parasite[];
    static const char _parasitePin[];
    static const char _domoticzIDX[];
    static const char _sensor[];
    static const char _temperature[];
    static const char _Temperature[];
    static const char _data_fx[];
    
    //Dallas sensor quick (& dirty) reading. Credit to - Author: Peter Scargill, August 17th, 2013
    float readDallas();
    void requestTemperatures();
    void readTemperature();
    bool findSensor();
#ifndef WLED_DISABLE_MQTT
    void publishHomeAssistantAutodiscovery();
#endif

    static UsermodTemperature* _instance; // to overcome nonstatic getTemperatureC() method and avoid UsermodManager::lookup(USERMOD_ID_TEMPERATURE);

  public:

    UsermodTemperature() { _instance = this; }
    static UsermodTemperature *getInstance() { return UsermodTemperature::_instance; }

    /*
     * API calls te enable data exchange between WLED modules
     */
    inline float getTemperatureC() { return temperature; }
    inline float getTemperatureF() { return temperature * 1.8f + 32.0f; }
    float getTemperature();
    const char *getTemperatureUnit();
    uint16_t getId() override { return USERMOD_ID_TEMPERATURE; }

    void setup() override;
    void loop() override;
    //void connected() override;
#ifndef WLED_DISABLE_MQTT
    void onMqttConnect(bool sessionPresent) override;
#endif
    //void onUpdateBegin(bool init) override;

    //bool handleButton(uint8_t b) override;
    //void handleOverlayDraw() override;

    void addToJsonInfo(JsonObject& root) override;
    //void addToJsonState(JsonObject &root) override;
    //void readFromJsonState(JsonObject &root) override;
    void addToConfig(JsonObject &root) override;
    bool readFromConfig(JsonObject &root) override;

    void appendConfigData() override;
};

