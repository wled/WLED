#pragma once
#include "wled.h"

#ifdef WLED_DISABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

#define USERMOD_SHT_TYPE_SHT30 0
#define USERMOD_SHT_TYPE_SHT31 1
#define USERMOD_SHT_TYPE_SHT35 2
#define USERMOD_SHT_TYPE_SHT85 3

class SHT;

class ShtUsermod : public Usermod
{
  private:
    bool enabled = false; // Is usermod enabled or not
    bool firstRunDone = false; // Remembers if the first config load run had been done
    bool initDone = false; // Remembers if the mod has been completely initialised
    bool haMqttDiscovery = false; // Is MQTT discovery enabled or not
    bool haMqttDiscoveryDone = false; // Remembers if we already published the HA discovery topics

    // SHT vars
    SHT *shtTempHumidSensor = nullptr; // Instance of SHT lib
    byte shtType = 0; // SHT sensor type to be used. Default: SHT30
    byte unitOfTemp = 0; // Temperature unit to be used. Default: Celsius (0 = Celsius, 1 = Fahrenheit)
    bool shtInitDone = false; // Remembers if SHT sensor has been initialised
    bool shtReadDataSuccess = false; // Did we have a successful data read and is a valid temperature and humidity available?
    const byte shtI2cAddress = 0x44; // i2c address of the sensor. 0x44 is the default for all SHT sensors. Change this, if needed
    unsigned long shtLastTimeUpdated = 0; // Remembers when we read data the last time
    bool shtDataRequested = false; // Reading data is done async. This remembers if we asked the sensor to read data
    float shtCurrentTempC = 0.0f; // Last read temperature in Celsius
    float shtCurrentHumidity = 0.0f; // Last read humidity in RH%


    void initShtTempHumiditySensor();
    void cleanupShtTempHumiditySensor();
    void cleanup();
    inline bool isShtReady() { return shtInitDone; } // Checks if the SHT sensor has been initialised.

    void publishTemperatureAndHumidityViaMqtt();
    void publishHomeAssistantAutodiscovery();
    void appendDeviceToMqttDiscoveryMessage(JsonDocument& root);

  public:
    // Strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _shtType[];
    static const char _unitOfTemp[];
    static const char _haMqttDiscovery[];

    void setup();
    void loop();
    void onMqttConnect(bool sessionPresent);
    void appendConfigData();
    void addToConfig(JsonObject &root);
    bool readFromConfig(JsonObject &root);
    void addToJsonInfo(JsonObject& root);

    bool isEnabled() { return enabled; }

    float getTemperature();
    float getTemperatureC() { return roundf(shtCurrentTempC * 10.0f) / 10.0f; }
    float getTemperatureF() { return (getTemperatureC() * 1.8f) + 32.0f; }
    float getHumidity() { return roundf(shtCurrentHumidity * 10.0f) / 10.0f; }
    const char* getUnitString();

    uint16_t getId() { return USERMOD_ID_SHT; }
};
