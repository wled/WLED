// Configurable settings for the INA2xx Usermod
#pragma once

#include "wled.h"

// Choose sensor type: 219 or 226
#ifndef INA_SENSOR_TYPE
  #define INA_SENSOR_TYPE 219
#endif

#if INA_SENSOR_TYPE == 219
  #include <INA219_WE.h>
  using INA_SENSOR_CLASS = INA219_WE;
#elif INA_SENSOR_TYPE == 226
  #include <INA226_WE.h>
  using INA_SENSOR_CLASS = INA226_WE;
#else
  #error "INA_SENSOR_TYPE must be 219 or 226"
#endif

// logging macro:
#define _logUsermodInaSensor(fmt, ...) \
  DEBUG_PRINTF("[INA2xx_Sensor] " fmt "\n", ##__VA_ARGS__)

#ifndef INA2XX_ENABLED
  #define INA2XX_ENABLED false // Default disabled value
#endif
#ifndef INA2XX_I2C_ADDRESS
  #define INA2XX_I2C_ADDRESS 0x40 // Default I2C address
#endif
#ifndef INA2XX_CHECK_INTERVAL
  #define INA2XX_CHECK_INTERVAL 5 // Default 5 seconds (will be converted to ms)
#endif
#ifndef INA2XX_CORRECTION_FACTOR
  #define INA2XX_CORRECTION_FACTOR 1.0 // Default correction factor. Default 1.0
#endif
#ifndef INA2XX_CONVERSION_TIME
  #if INA_SENSOR_TYPE == 219
    #define INA2XX_CONVERSION_TIME BIT_MODE_12 // Conversion Time, Default 12-bit resolution
  #elif INA_SENSOR_TYPE == 226
    #define INA2XX_CONVERSION_TIME CONV_TIME_1100 // Conversion Time
  #endif
#endif
#ifndef INA2XX_SHUNT_RESISTOR
  #define INA2XX_SHUNT_RESISTOR 0.1 // Shunt Resistor value. Default 0.1 ohms
#endif
#ifndef INA2XX_DECIMAL_FACTOR
  #define INA2XX_DECIMAL_FACTOR 3 // Decimal factor for current/power readings. Default 3 decimal places
#endif

#if INA_SENSOR_TYPE == 219
	#ifndef INA2XX_BUSRANGE
	  #define INA2XX_BUSRANGE BRNG_32 // BRNG_16, BRNG_32
	#endif
	#ifndef INA2XX_P_GAIN
	  #define INA2XX_P_GAIN PG_320 // PG_40, PG_80, PG_160, PG_320
	#endif
	#ifndef INA2XX_SHUNT_VOLT_OFFSET
	  #define INA2XX_SHUNT_VOLT_OFFSET 0.0 // mV offset at zero current
	#endif
#elif INA_SENSOR_TYPE == 226
	#ifndef INA2XX_AVERAGES
	  #define INA2XX_AVERAGES AVERAGE_1
	#endif
	#ifndef INA2XX_RANGE
	  #define INA2XX_RANGE 1.3 //Current Range, Max 10.0 (10A)
	#endif
#endif

#ifndef INA2XX_MQTT_PUBLISH
  #define INA2XX_MQTT_PUBLISH false // Default: do not publish to MQTT
#endif
#ifndef INA2XX_MQTT_PUBLISH_ALWAYS
  #define INA2XX_MQTT_PUBLISH_ALWAYS false // Default: only publish on change
#endif
#ifndef INA2XX_HA_DISCOVERY
  #define INA2XX_HA_DISCOVERY false // Default: Home Assistant discovery disabled
#endif

#define UPDATE_CONFIG(obj, key, var, fmt)                     \
  do {                                                         \
    auto _tmp = var;                                           \
    if ( getJsonValue((obj)[(key)], _tmp) ) {                  \
      if (_tmp != var) {                                       \
        _logUsermodInaSensor("%s updated to: " fmt, key, _tmp);\
        var = _tmp;                                            \
      }                                                        \
    } else {                                                   \
      configComplete = false;                                  \
    }                                                          \
  } while(0)

class UsermodINA2xx : public Usermod {
private:
  static const char _name[];
  bool initDone = false;  // Flag for successful initialization
  unsigned long lastCheck = 0; // Timestamp for the last check
  bool alreadyLoggedDisabled = false;

  // Define the variables using the pre-defined or default values
  bool enabled = INA2XX_ENABLED;
  uint8_t _i2cAddress = INA2XX_I2C_ADDRESS;
  uint16_t _checkInterval = INA2XX_CHECK_INTERVAL; // seconds
  uint32_t checkInterval = static_cast<uint32_t>(_checkInterval) * 1000UL; // ms
  uint8_t _decimalFactor = INA2XX_DECIMAL_FACTOR;
  float shuntResistor = INA2XX_SHUNT_RESISTOR;
  float correctionFactor = INA2XX_CORRECTION_FACTOR;

#if INA_SENSOR_TYPE == 219
  INA219_PGAIN pGain            = static_cast<INA219_PGAIN>(INA2XX_P_GAIN);
  INA219_BUS_RANGE busRange     = static_cast<INA219_BUS_RANGE>(INA2XX_BUSRANGE);
  float shuntVoltOffset_mV      = INA2XX_SHUNT_VOLT_OFFSET;
  
  INA219_ADC_MODE conversionTime = static_cast<INA219_ADC_MODE>(INA2XX_CONVERSION_TIME);
#elif INA_SENSOR_TYPE == 226
  INA226_AVERAGES average     = static_cast<INA226_AVERAGES>(INA2XX_AVERAGES);
  INA226_CONV_TIME conversionTime     = static_cast<INA226_CONV_TIME>(INA2XX_CONVERSION_TIME);
  float currentRange = INA2XX_RANGE;
#endif

  bool mqttPublish = INA2XX_MQTT_PUBLISH;
  bool mqttPublishSent = !INA2XX_MQTT_PUBLISH;
  bool mqttPublishAlways = INA2XX_MQTT_PUBLISH_ALWAYS;
  bool haDiscovery = INA2XX_HA_DISCOVERY;
  bool haDiscoverySent = !INA2XX_HA_DISCOVERY;

  // Variables to store sensor readings
  float busVoltage = 0.0;
  float current = 0.0;
  float current_mA = 0.0;
  float power = 0.0;
  float power_mW = 0.0;
  float shuntVoltage = 0.0;
  float loadVoltage = 0.0;
  bool overflow = false;

  // Last sent variables
  float last_sent_shuntVoltage = 0;
  float last_sent_busVoltage = 0;
  float last_sent_loadVoltage = 0;
  float last_sent_current = 0;
  float last_sent_current_mA = 0;
  float last_sent_power = 0;
  float last_sent_power_mW = 0;
  bool last_sent_overflow = false;

  float dailyEnergy_kWh = 0.0; // Daily energy in kWh
  float monthlyEnergy_kWh = 0.0; // Monthly energy in kWh
  float totalEnergy_kWh = 0.0; // Total energy in kWh
  unsigned long lastPublishTime = 0; // Track the last publish time

  // Variables to store last reset timestamps
  unsigned long dailyResetTime = 0;
  unsigned long monthlyResetTime = 0;

  // epoch timestamps (seconds since epoch) of the most recent resets (midnight)
  unsigned long dailyResetTimestamp = 0;
  unsigned long monthlyResetTimestamp = 0;

  bool mqttStateRestored = false;
  bool mqttRestorePending = false;
  bool mqttRestoreDeferredLogged = false;

  struct MqttRestoreData {
    bool hasDailyEnergy = false;
    bool hasMonthlyEnergy = false;
    bool hasTotalEnergy = false;
    bool hasDailyResetTime = false;
    bool hasMonthlyResetTime = false;
    bool hasDailyResetTimestamp = false;
    bool hasMonthlyResetTimestamp = false;
    float dailyEnergy = 0.0f;
    float monthlyEnergy = 0.0f;
    float totalEnergy = 0.0f;
    uint32_t dailyResetTime = 0;
    uint32_t monthlyResetTime = 0;
    uint32_t dailyResetTimestamp = 0;
    uint32_t monthlyResetTimestamp = 0;
  };

  MqttRestoreData mqttRestoreData;

  INA_SENSOR_CLASS *_ina2xx = nullptr; // INA2xx sensor object

  float roundDecimals(float val);
  bool hasSignificantChange(float oldValue, float newValue, float threshold = 0.01f);
  bool hasValueChanged();
  void checkForI2cErrors();
  bool isTimeValid() const;
  void applyMqttRestoreIfReady();
  bool updateINA2xxSettings();
  String sanitizeMqttClientID(const String &clientID);
  void updateEnergy(float power, unsigned long durationMs);
#ifndef WLED_DISABLE_MQTT
  void publishMqtt(float shuntVoltage, float busVoltage, float loadVoltage,
                   float current, float current_mA, float power,
                   float power_mW, bool overflow);
  void mqttCreateHassSensor(const String &name, const String &topic,
                            const String &deviceClass, const String &unitOfMeasurement,
                            const String &jsonKey, const String &SensorType);
  void mqttRemoveHassSensor(const String &name, const String &SensorType);
#endif

public:
  ~UsermodINA2xx();
  void setup() override;
  void loop() override;
#ifndef WLED_DISABLE_MQTT
  bool onMqttMessage(char* topic, char* payload) override;
  void onMqttConnect(bool sessionPresent) override;
#endif
  void addToJsonInfo(JsonObject &root) override;
  void addToJsonState(JsonObject &root) override;
  void readFromJsonState(JsonObject &root) override;
  void addToConfig(JsonObject& root) override;
  void appendConfigData() override;
  bool readFromConfig(JsonObject& root) override;
  uint16_t getId() override;
};

extern UsermodINA2xx ina2xx_v2;
