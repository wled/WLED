// force the compiler to show a warning to confirm that this file is included
#warning **** Included USERMOD_BME280 version 2.0 ****

#ifndef WLED_ENABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

#pragma once

#include "wled.h"
#include <Arduino.h>
#include <BME280I2C.h>               // BME280 sensor
#include <EnvironmentCalculations.h> // BME280 extended measurements

/**
 * Read sensor values from the BME280/BMP280 and update internal state.
 *
 * Reads temperature, pressure and (when available) humidity from the sensor using the configured
 * units (Celsius or Fahrenheit). Updates sensorTemperature, sensorHumidity, sensorPressure and
 * tempScale. For BME280 (sensorType == 1) also computes sensorHeatIndex and sensorDewPoint.
 *
 * @param SensorType sensor model identifier (1 = BME280, 2 = BMP280); used to determine whether
 *                   humidity-derived values should be computed.
 */
/**
 * Initialize MQTT discovery topics and create Home Assistant discovery sensors if enabled.
 *
 * Builds topic strings for temperature, humidity, pressure, heat index and dew point and calls
 * _createMqttSensor(...) for each when HomeAssistantDiscovery is true.
 */
/**
 * Create and publish a Home Assistant discovery payload for a single sensor.
 *
 * Constructs the discovery JSON for a sensor (name, state_topic, unique_id, optional unit and
 * device_class) and attaches device metadata so Home Assistant associates the sensor with the
 * WLED device, then publishes the payload to the Home Assistant discovery topic.
 *
 * @param name Human-readable sensor name (used in discovery topic and sensor name).
 * @param topic MQTT state topic that the sensor will publish to.
 * @param deviceClass Optional Home Assistant device_class string (empty to omit).
 * @param unitOfMeasurement Optional unit string (empty to omit).
 */
/**
 * Publish a single state to the device's MQTT subtree.
 *
 * Publishes `state` to the topic formed by "<mqttDeviceTopic>/<topic>" if MQTT is connected.
 *
 * @param topic Topic suffix under the device topic (e.g., "temperature").
 * @param state Null-terminated C-string state payload to publish.
 */
/**
 * Configure and initialize the BME280/BMP280 sensor interface.
 *
 * Applies preferred oversampling/mode/standby/filter settings, attempts to start I2C
 * communication with the configured I2CAddress and sets sensorType:
 *  - 0: not found/unknown
 *  - 1: BME280 (temperature, pressure, humidity)
 *  - 2: BMP280 (temperature, pressure)
 *
 * Diagnostic messages are emitted via DEBUG_PRINTLN but no exceptions are thrown.
 */
/**
 * Initialize the usermod: validate I2C pins and initialize sensor communications.
 *
 * If I2C pins are invalid the usermod is disabled and sensorType is set to 0. Otherwise calls
 * initializeBmeComms() and marks initDone.
 */
/**
 * Main periodic handler called from WLED loop().
 *
 * When enabled and a sensor is present, periodically reads sensor data at configured
 * TemperatureInterval and PressureInterval (seconds). Values are rounded to the configured
 * decimal places and published to MQTT when changed or when PublishAlways is true. Handles
 * temperature, humidity, pressure, heat index and dew point as appropriate for the detected
 * sensor type.
 */
/**
 * Called when MQTT connects.
 *
 * Performs one-time MQTT discovery initialization (_mqttInitialize) if MQTT is available and
 * discovery has not been initialized yet.
 *
 * @param sessionPresent Indicates whether the MQTT session is a persistent session (passed through).
 */
/**
 * Return the current temperature in Celsius, using the configured rounding and unit preference.
 *
 * If UseCelsius is true, returns sensorTemperature rounded to TemperatureDecimals; otherwise
 * converts the internally-stored temperature to Fahrenheit before returning.
 */
/**
 * Return the current temperature in Fahrenheit, using the configured rounding and unit preference.
 *
 * If UseCelsius is true, converts the internally-stored Celsius temperature to Fahrenheit;
 * otherwise returns the stored temperature rounded to TemperatureDecimals.
 */
/**
 * Return the current humidity rounded to the configured HumidityDecimals.
 */
/**
 * Return the current pressure rounded to the configured PressureDecimals.
 */
/**
 * Return the current dew point in Celsius, honoring UseCelsius and TemperatureDecimals.
 */
/**
 * Return the current dew point in Fahrenheit, honoring UseCelsius and TemperatureDecimals.
 */
/**
 * Return the current heat index in Celsius, honoring UseCelsius and TemperatureDecimals.
 */
/**
 * Return the current heat index in Fahrenheit, honoring UseCelsius and TemperatureDecimals.
 */
/**
 * Add current sensor status and readings to the UI JSON info object.
 *
 * Appends a nested "u" object with:
 *  - "Not Found" when no sensor is present.
 *  - Temperature and Pressure for BMP280.
 *  - Temperature, Humidity, Pressure, Heat Index and Dew Point for BME280.
 *
 * Values are rounded according to their configured decimal places and temperature entries
 * include the tempScale unit string.
 *
 * @param root JSON object to append info into.
 */
/**
 * Persist usermod configuration into the provided JSON object.
 *
 * Writes the user-visible configuration namespace (named by _name) including enabled flag,
 * I2CAddress, decimal settings, measurement intervals, PublishAlways, UseCelsius and
 * HomeAssistantDiscovery.
 *
 * @param root JSON object to write configuration into.
 */
/**
 * Load usermod configuration from the provided JSON object.
 *
 * Reads the configuration block named by _name and updates user-configurable fields.
 * Sets tempScale according to UseCelsius. On subsequent reloads (when initDone is true)
 * resets cached sensor values and reinitializes sensor communications.
 *
 * @param root JSON object to read configuration from.
 * @return true if the configuration block was present; false if missing (defaults remain).
 */
/**
 * Return the numeric usermod identifier.
 *
 * @return USERMOD_ID_BME280
 */
class UsermodBME280 : public Usermod
{
private:
  
  // NOTE: Do not implement any compile-time variables, anything the user needs to configure
  // should be configurable from the Usermod menu using the methods below
  // key settings set via usermod menu
  uint8_t  TemperatureDecimals = 0;  // Number of decimal places in published temperaure values
  uint8_t  HumidityDecimals = 0;    // Number of decimal places in published humidity values
  uint8_t  PressureDecimals = 0;    // Number of decimal places in published pressure values
  uint16_t TemperatureInterval = 5; // Interval to measure temperature (and humidity, dew point if available) in seconds
  uint16_t PressureInterval = 300;  // Interval to measure pressure in seconds
  BME280I2C::I2CAddr I2CAddress = BME280I2C::I2CAddr_0x76;  // i2c address, defaults to 0x76
  bool PublishAlways = false;             // Publish values even when they have not changed
  bool UseCelsius = true;                 // Use Celsius for Reporting
  bool HomeAssistantDiscovery = false;    // Publish Home Assistant Device Information
  bool enabled = true;

  // set the default pins based on the architecture, these get overridden by Usermod menu settings
  #ifdef ESP8266
    //uint8_t RST_PIN = 16; // Un-comment for Heltec WiFi-Kit-8
  #endif
  bool initDone = false;

  BME280I2C bme;

  uint8_t sensorType;

  // Measurement timers
  long timer;
  long lastTemperatureMeasure = 0;
  long lastPressureMeasure = 0;

  // Current sensor values
  float sensorTemperature;
  float sensorHumidity;
  float sensorHeatIndex;
  float sensorDewPoint;
  float sensorPressure;
  String tempScale;
  // Track previous sensor values
  float lastTemperature;
  float lastHumidity;
  float lastHeatIndex;
  float lastDewPoint;
  float lastPressure;

  // MQTT topic strings for publishing Home Assistant discovery topics
  bool mqttInitialized = false;

  // strings to reduce flash memory usage (used more than twice)
  static const char _name[];
  static const char _enabled[];

  // Read the BME280/BMP280 Sensor (which one runs depends on whether Celsius or Fahrenheit being set in Usermod Menu)
  void UpdateBME280Data(int SensorType)
  {
    float _temperature, _humidity, _pressure;

    if (UseCelsius) {
      BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
      EnvironmentCalculations::TempUnit envTempUnit(EnvironmentCalculations::TempUnit_Celsius);
      BME280::PresUnit presUnit(BME280::PresUnit_hPa);

      bme.read(_pressure, _temperature, _humidity, tempUnit, presUnit);

      sensorTemperature = _temperature;
      sensorHumidity = _humidity;
      sensorPressure = _pressure;
      tempScale = F("°C");
      if (sensorType == 1)
      {
        sensorHeatIndex = EnvironmentCalculations::HeatIndex(_temperature, _humidity, envTempUnit);
        sensorDewPoint = EnvironmentCalculations::DewPoint(_temperature, _humidity, envTempUnit);
      }
    } else {
      BME280::TempUnit tempUnit(BME280::TempUnit_Fahrenheit);
      EnvironmentCalculations::TempUnit envTempUnit(EnvironmentCalculations::TempUnit_Fahrenheit);
      BME280::PresUnit presUnit(BME280::PresUnit_hPa);

      bme.read(_pressure, _temperature, _humidity, tempUnit, presUnit);

      sensorTemperature = _temperature;
      sensorHumidity = _humidity;
      sensorPressure = _pressure;
      tempScale = F("°F");
      if (sensorType == 1)
      {
        sensorHeatIndex = EnvironmentCalculations::HeatIndex(_temperature, _humidity, envTempUnit);
        sensorDewPoint = EnvironmentCalculations::DewPoint(_temperature, _humidity, envTempUnit);
      }
    }
  }

  // Procedure to define all MQTT discovery Topics 
  void _mqttInitialize()
  {
    char mqttTemperatureTopic[128];
    char mqttHumidityTopic[128];
    char mqttPressureTopic[128];
    char mqttHeatIndexTopic[128];
    char mqttDewPointTopic[128];
    snprintf_P(mqttTemperatureTopic, 127, PSTR("%s/temperature"), mqttDeviceTopic);
    snprintf_P(mqttPressureTopic, 127, PSTR("%s/pressure"), mqttDeviceTopic);
    snprintf_P(mqttHumidityTopic, 127, PSTR("%s/humidity"), mqttDeviceTopic);
    snprintf_P(mqttHeatIndexTopic, 127, PSTR("%s/heat_index"), mqttDeviceTopic);
    snprintf_P(mqttDewPointTopic, 127, PSTR("%s/dew_point"), mqttDeviceTopic);

    if (HomeAssistantDiscovery) {
      _createMqttSensor(F("Temperature"), mqttTemperatureTopic, "temperature", tempScale);
      _createMqttSensor(F("Pressure"), mqttPressureTopic, "pressure", F("hPa"));
      _createMqttSensor(F("Humidity"), mqttHumidityTopic, "humidity", F("%"));
      _createMqttSensor(F("HeatIndex"), mqttHeatIndexTopic, "temperature", tempScale);
      _createMqttSensor(F("DewPoint"), mqttDewPointTopic, "temperature", tempScale);
    }
  }

  // Create an MQTT Sensor for Home Assistant Discovery purposes, this includes a pointer to the topic that is published to in the Loop.
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
    device[F("manufacturer")] = F(WLED_BRAND);
    device[F("model")] = F(WLED_PRODUCT_NAME);
    device[F("sw_version")] = versionString;

    String temp;
    serializeJson(doc, temp);
    DEBUG_PRINTLN(t);
    DEBUG_PRINTLN(temp);

    mqtt->publish(t.c_str(), 0, true, temp.c_str());
  }

    void publishMqtt(const char *topic, const char* state) {
      //Check if MQTT Connected, otherwise it will crash the 8266
      if (WLED_MQTT_CONNECTED){
        char subuf[128];
        snprintf_P(subuf, 127, PSTR("%s/%s"), mqttDeviceTopic, topic);
        mqtt->publish(subuf, 0, false, state);
      }
    }

    /**
 * Initialize BME280/BMP280 sensor communications and detect sensor type.
 *
 * Configures BME280I2C interface (oversampling, mode, standby, filter, SPI disabled,
 * and I2C address), applies the settings, and attempts to initialize the sensor.
 * On success sets `sensorType` to 1 for BME280 or 2 for BMP280; on failure sets
 * `sensorType` to 0. Emits debug messages indicating the outcome.
 */
void initializeBmeComms()
    {
      BME280I2C::Settings settings{
            BME280::OSR_X16,    // Temperature oversampling x16
            BME280::OSR_X16,    // Humidity oversampling x16
            BME280::OSR_X16,    // Pressure oversampling x16
            BME280::Mode_Forced,
            BME280::StandbyTime_1000ms,
            BME280::Filter_Off,
            BME280::SpiEnable_False,
            I2CAddress
        };

      bme.setSettings(settings);
      
      if (!bme.begin())
      {
        sensorType = 0;
        DEBUG_PRINTLN(F("Could not find BME280 I2C sensor!"));
      }
      else
      {
        switch (bme.chipModel())
        {
        case BME280::ChipModel_BME280:
          sensorType = 1;
          DEBUG_PRINTLN(F("Found BME280 sensor! Success."));
          break;
        case BME280::ChipModel_BMP280:
          sensorType = 2;
          DEBUG_PRINTLN(F("Found BMP280 sensor! No Humidity available."));
          break;
        default:
          sensorType = 0;
          DEBUG_PRINTLN(F("Found UNKNOWN sensor! Error!"));
        }
      }
    }

public:
  void setup()
  {
    if (i2c_scl<0 || i2c_sda<0) { enabled = false; sensorType = 0; return; }
    
    initializeBmeComms();
    initDone = true;
  }

  void loop()
  {
    if (!enabled || strip.isUpdating()) return;

    // BME280 sensor MQTT publishing
    // Check if sensor present and Connected, otherwise it will crash the MCU
    if (sensorType != 0)
    {
      // Timer to fetch new temperature, humidity and pressure data at intervals
      timer = millis();

      if (timer - lastTemperatureMeasure >= TemperatureInterval * 1000)
      {
        lastTemperatureMeasure = timer;

        UpdateBME280Data(sensorType);

        float temperature = roundf(sensorTemperature * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);
        float humidity, heatIndex, dewPoint;

        // If temperature has changed since last measure, create string populated with device topic
        // from the UI and values read from sensor, then publish to broker
        if (temperature != lastTemperature || PublishAlways)
        {
          publishMqtt("temperature", String(temperature, TemperatureDecimals).c_str());
        }

        lastTemperature = temperature; // Update last sensor temperature for next loop

        if (sensorType == 1) // Only if sensor is a BME280
        {
          humidity = roundf(sensorHumidity * powf(10, HumidityDecimals)) / powf(10, HumidityDecimals);
          heatIndex = roundf(sensorHeatIndex * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);
          dewPoint = roundf(sensorDewPoint * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);

          if (humidity != lastHumidity || PublishAlways)
          {
            publishMqtt("humidity", String(humidity, HumidityDecimals).c_str());
          }

          if (heatIndex != lastHeatIndex || PublishAlways)
          {
            publishMqtt("heat_index", String(heatIndex, TemperatureDecimals).c_str());
          }

          if (dewPoint != lastDewPoint || PublishAlways)
          {
            publishMqtt("dew_point", String(dewPoint, TemperatureDecimals).c_str());
          }

          lastHumidity = humidity;
          lastHeatIndex = heatIndex;
          lastDewPoint = dewPoint;
        }
      }

      if (timer - lastPressureMeasure >= PressureInterval * 1000)
      {
        lastPressureMeasure = timer;

        float pressure = roundf(sensorPressure * powf(10, PressureDecimals)) / powf(10, PressureDecimals);

        if (pressure != lastPressure || PublishAlways)
        {
          publishMqtt("pressure", String(pressure, PressureDecimals).c_str());
        }

        lastPressure = pressure;
      }
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

    /*
     * API calls te enable data exchange between WLED modules
     */
    inline float getTemperatureC() {
      if (UseCelsius) {
        return (float)roundf(sensorTemperature * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);
      } else {
        return (float)roundf(sensorTemperature * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals) * 1.8f + 32;
      }      
    }

    inline float getTemperatureF() {
      if (UseCelsius) {
        return ((float)roundf(sensorTemperature * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals) -32) * 0.56f;
      } else {
        return (float)roundf(sensorTemperature * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);
      }
    }

    inline float getHumidity() {
      return (float)roundf(sensorHumidity * powf(10, HumidityDecimals));
    }

    inline float getPressure() {
      return (float)roundf(sensorPressure * powf(10, PressureDecimals));
    }

    inline float getDewPointC() {
      if (UseCelsius) {
        return (float)roundf(sensorDewPoint * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);
      } else {
        return (float)roundf(sensorDewPoint * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals) * 1.8f + 32;
      }
    }

    inline float getDewPointF() {
      if (UseCelsius) {
        return ((float)roundf(sensorDewPoint * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals) -32) * 0.56f;
      } else {
        return (float)roundf(sensorDewPoint * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);
      }
    }

    inline float getHeatIndexC() {
      if (UseCelsius) {
        return (float)roundf(sensorHeatIndex * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);
      } else {
        return (float)roundf(sensorHeatIndex * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals) * 1.8f + 32;
      }
    }

    inline float getHeatIndexF() {
      if (UseCelsius) {
        return ((float)roundf(sensorHeatIndex * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals) -32) * 0.56f;
      } else {
        return (float)roundf(sensorHeatIndex * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);
      }
    }

  // Publish Sensor Information to Info Page
  void addToJsonInfo(JsonObject &root)
  {
    JsonObject user = root[F("u")];
    if (user.isNull()) user = root.createNestedObject(F("u"));
    
    if (sensorType==0) //No Sensor
    {
      // if we sensor not detected, let the user know
      JsonArray temperature_json = user.createNestedArray(F("BME/BMP280 Sensor"));
      temperature_json.add(F("Not Found"));
    }
    else if (sensorType==2) //BMP280
    {
      JsonArray temperature_json = user.createNestedArray(F("Temperature"));
      JsonArray pressure_json = user.createNestedArray(F("Pressure"));
      temperature_json.add(roundf(sensorTemperature * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals));
      temperature_json.add(tempScale);
      pressure_json.add(roundf(sensorPressure * powf(10, PressureDecimals)) / powf(10, PressureDecimals));
      pressure_json.add(F("hPa"));
    }
    else if (sensorType==1) //BME280
    {
      JsonArray temperature_json = user.createNestedArray(F("Temperature"));
      JsonArray humidity_json = user.createNestedArray(F("Humidity"));
      JsonArray pressure_json = user.createNestedArray(F("Pressure"));
      JsonArray heatindex_json = user.createNestedArray(F("Heat Index"));
      JsonArray dewpoint_json = user.createNestedArray(F("Dew Point"));
      temperature_json.add(roundf(sensorTemperature * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals));
      temperature_json.add(tempScale);
      humidity_json.add(roundf(sensorHumidity * powf(10, HumidityDecimals)) / powf(10, HumidityDecimals));
      humidity_json.add(F("%"));
      pressure_json.add(roundf(sensorPressure * powf(10, PressureDecimals)) / powf(10, PressureDecimals));
      pressure_json.add(F("hPa"));
      heatindex_json.add(roundf(sensorHeatIndex * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals));
      heatindex_json.add(tempScale);
      dewpoint_json.add(roundf(sensorDewPoint * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals));
      dewpoint_json.add(tempScale);
    }
      return;
  }

  // Save Usermod Config Settings
  void addToConfig(JsonObject& root)
  {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top[F("I2CAddress")] = static_cast<uint8_t>(I2CAddress);
    top[F("TemperatureDecimals")] = TemperatureDecimals;
    top[F("HumidityDecimals")] = HumidityDecimals;
    top[F("PressureDecimals")] = PressureDecimals;
    top[F("TemperatureInterval")] = TemperatureInterval;
    top[F("PressureInterval")] = PressureInterval;
    top[F("PublishAlways")] = PublishAlways;
    top[F("UseCelsius")] = UseCelsius;
    top[F("HomeAssistantDiscovery")] = HomeAssistantDiscovery;
    DEBUG_PRINTLN(F("BME280 config saved."));
  }

  // Read Usermod Config Settings
  bool readFromConfig(JsonObject& root)
  {
    // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
    // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)

    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) {
      DEBUG_PRINT(F(_name));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }
    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
    // A 3-argument getJsonValue() assigns the 3rd argument as a default value if the Json value is missing
    uint8_t tmpI2cAddress;
    configComplete &= getJsonValue(top[F("I2CAddress")], tmpI2cAddress, 0x76);
    I2CAddress = static_cast<BME280I2C::I2CAddr>(tmpI2cAddress);

    configComplete &= getJsonValue(top[F("TemperatureDecimals")], TemperatureDecimals, 1);
    configComplete &= getJsonValue(top[F("HumidityDecimals")], HumidityDecimals, 0);
    configComplete &= getJsonValue(top[F("PressureDecimals")], PressureDecimals, 0);
    configComplete &= getJsonValue(top[F("TemperatureInterval")], TemperatureInterval, 30);
    configComplete &= getJsonValue(top[F("PressureInterval")], PressureInterval, 30);
    configComplete &= getJsonValue(top[F("PublishAlways")], PublishAlways, false);
    configComplete &= getJsonValue(top[F("UseCelsius")], UseCelsius, true);
    configComplete &= getJsonValue(top[F("HomeAssistantDiscovery")], HomeAssistantDiscovery, false);
    tempScale = UseCelsius ? "°C" : "°F";

    DEBUG_PRINT(FPSTR(_name));
    if (!initDone) {
      // first run: reading from cfg.json
      DEBUG_PRINTLN(F(" config loaded."));
    } else {
      // changing parameters from settings page
      DEBUG_PRINTLN(F(" config (re)loaded."));

      // Reset all known values
      sensorType = 0;
      sensorTemperature = 0;
      sensorHumidity = 0;
      sensorHeatIndex = 0;
      sensorDewPoint = 0;
      sensorPressure = 0;
      lastTemperature = 0;
      lastHumidity = 0;
      lastHeatIndex = 0;
      lastDewPoint = 0;
      lastPressure = 0;
      
      initializeBmeComms();
    }

    return configComplete;
  }

  uint16_t getId() {
    return USERMOD_ID_BME280;
  }
};

const char UsermodBME280::_name[]                      PROGMEM = "BME280/BMP280";
const char UsermodBME280::_enabled[]                   PROGMEM = "enabled";
