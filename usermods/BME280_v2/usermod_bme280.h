// force the compiler to show a warning to confirm that this file is included
#warning **** Included USERMOD_BME280 version 2.0 ****

#pragma once

#include "wled.h"
#include <Arduino.h>
#include <Wire.h>
#include <BME280I2C.h>               // BME280 sensor
#include <EnvironmentCalculations.h> // BME280 extended measurements

class UsermodBME280 : public Usermod
{
private:
  
  // NOTE: Do not implement any compile-time variables, anything the user needs to configure
  // should be configurable from the Usermod menu using the methods below
  // key settings set via usermod menu
  unsigned long TemperatureDecimals = 0;  // Number of decimal places in published temperaure values
  unsigned long  HumidityDecimals = 0;    // Number of decimal places in published humidity values
  unsigned long  PressureDecimals = 0;    // Number of decimal places in published pressure values
  unsigned long  TemperatureInterval = 5; // Interval to measure temperature (and humidity, dew point if available) in seconds
  unsigned long  PressureInterval = 300;  // Interval to measure pressure in seconds
  bool PublishAlways = false;             // Publish values even when they have not changed
  bool UseCelsius = true;                 // Use Celsius for Reporting
  bool HomeAssistantDiscovery = false;    // Publish Home Assistant Device Information

  // set the default pins based on the architecture, these get overridden by Usermod menu settings
  #ifdef ARDUINO_ARCH_ESP32 // ESP32 boards
    #define HW_PIN_SCL 22
    #define HW_PIN_SDA 21
  #else // ESP8266 boards
    #define HW_PIN_SCL 5
    #define HW_PIN_SDA 4
    //uint8_t RST_PIN = 16; // Uncoment for Heltec WiFi-Kit-8
  #endif
  int8_t ioPin[2] = {HW_PIN_SCL, HW_PIN_SDA};        // I2C pins: SCL, SDA...defaults to Arch hardware pins but overridden at setup()
  bool initDone = false;

  // BME280 sensor settings
  BME280I2C::Settings settings{
      BME280::OSR_X16, // Temperature oversampling x16
      BME280::OSR_X16, // Humidity oversampling x16
      BME280::OSR_X16, // Pressure oversampling x16
      // Defaults
      BME280::Mode_Forced,
      BME280::StandbyTime_1000ms,
      BME280::Filter_Off,
      BME280::SpiEnable_False,
      BME280I2C::I2CAddr_0x76 // I2C address. I2C specific. Default 0x76
  };

  BME280I2C bme{settings};

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
  String mqttTemperatureTopic = "";
  String mqttHumidityTopic = "";
  String mqttPressureTopic = "";
  String mqttHeatIndexTopic = "";
  String mqttDewPointTopic = "";

  // Store packet IDs of MQTT publications
  uint16_t mqttTemperaturePub = 0;
  uint16_t mqttPressurePub = 0;

  // Read the BME280/BMP280 Sensor (which one runs depends on whether Celsius or Farenheit being set in Usermod Menu)
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
      tempScale = "°C";
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
      tempScale = "°F";
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
    mqttTemperatureTopic = String(mqttDeviceTopic) + F("/temperature");
    mqttPressureTopic = String(mqttDeviceTopic) + F("/pressure");
    mqttHumidityTopic = String(mqttDeviceTopic) + F("/humidity");
    mqttHeatIndexTopic = String(mqttDeviceTopic) + F("/heat_index");
    mqttDewPointTopic = String(mqttDeviceTopic) + F("/dew_point");

    if (HomeAssistantDiscovery) {
      _createMqttSensor(F("Temperature"), mqttTemperatureTopic, F("temperature"), tempScale);
      _createMqttSensor(F("Pressure"), mqttPressureTopic, F("pressure"), F("hPa"));
      _createMqttSensor(F("Humidity"), mqttHumidityTopic, F("humidity"), F("%"));
      _createMqttSensor(F("HeatIndex"), mqttHeatIndexTopic, F("temperature"), tempScale);
      _createMqttSensor(F("DewPoint"), mqttDewPointTopic, F("temperature"), tempScale);
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
    bool HW_Pins_Used = (ioPin[0]==HW_PIN_SCL && ioPin[1]==HW_PIN_SDA); // note whether architecture-based hardware SCL/SDA pins used
    PinOwner po = PinOwner::UM_BME280; // defaults to being pinowner for SCL/SDA pins
    PinManagerPinType pins[2] = { { ioPin[0], true }, { ioPin[1], true } };  // allocate pins
    if (HW_Pins_Used) po = PinOwner::HW_I2C; // allow multiple allocations of HW I2C bus pins
    if (!pinManager.allocateMultiplePins(pins, 2, po)) { sensorType=0; return; }
    
    Wire.begin(ioPin[1], ioPin[0]);

    if (!bme.begin())
    {
      sensorType = 0;
      DEBUG_PRINTLN(F("Could not find BME280I2C sensor!"));
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
    initDone=true;
  }

  void loop()
  {
    // BME280 sensor MQTT publishing
    // Check if sensor present and MQTT Connected, otherwise it will crash the MCU
    if (sensorType != 0 && WLED_MQTT_CONNECTED)
    {
      // Timer to fetch new temperature, humidity and pressure data at intervals
      timer = millis();

      if (timer - lastTemperatureMeasure >= TemperatureInterval * 1000 || mqttTemperaturePub == 0)
      {
        lastTemperatureMeasure = timer;

        UpdateBME280Data(sensorType);

        float temperature = roundf(sensorTemperature * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);
        float humidity, heatIndex, dewPoint;

        if (WLED_MQTT_CONNECTED && !mqttInitialized)
        {
          _mqttInitialize();
          mqttInitialized = true;
        }

        // If temperature has changed since last measure, create string populated with device topic
        // from the UI and values read from sensor, then publish to broker
        if (temperature != lastTemperature || PublishAlways)
        {
          String topic = String(mqttDeviceTopic) + "/temperature";
          mqttTemperaturePub = mqtt->publish(topic.c_str(), 0, false, String(temperature, TemperatureDecimals).c_str());
        }

        lastTemperature = temperature; // Update last sensor temperature for next loop

        if (sensorType == 1) // Only if sensor is a BME280
        {
          humidity = roundf(sensorHumidity * powf(10, HumidityDecimals)) / powf(10, HumidityDecimals);
          heatIndex = roundf(sensorHeatIndex * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);
          dewPoint = roundf(sensorDewPoint * powf(10, TemperatureDecimals)) / powf(10, TemperatureDecimals);

          if (humidity != lastHumidity || PublishAlways)
          {
            String topic = String(mqttDeviceTopic) + F("/humidity");
            mqtt->publish(topic.c_str(), 0, false, String(humidity, HumidityDecimals).c_str());
          }

          if (heatIndex != lastHeatIndex || PublishAlways)
          {
            String topic = String(mqttDeviceTopic) + F("/heat_index");
            mqtt->publish(topic.c_str(), 0, false, String(heatIndex, TemperatureDecimals).c_str());
          }

          if (dewPoint != lastDewPoint || PublishAlways)
          {
            String topic = String(mqttDeviceTopic) + F("/dew_point");
            mqtt->publish(topic.c_str(), 0, false, String(dewPoint, TemperatureDecimals).c_str());
          }

          lastHumidity = humidity;
          lastHeatIndex = heatIndex;
          lastDewPoint = dewPoint;
        }
      }

      if (timer - lastPressureMeasure >= PressureInterval * 1000 || mqttPressurePub == 0)
      {
        lastPressureMeasure = timer;

        float pressure = roundf(sensorPressure * powf(10, PressureDecimals)) / powf(10, PressureDecimals);

        if (pressure != lastPressure || PublishAlways)
        {
          String topic = String(mqttDeviceTopic) + F("/pressure");
          mqttPressurePub = mqtt->publish(topic.c_str(), 0, true, String(pressure, PressureDecimals).c_str());
        }

        lastPressure = pressure;
      }
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
    }inline float getHeatIndexF() {
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
      temperature_json.add(roundf(sensorTemperature * powf(10, TemperatureDecimals)));
      temperature_json.add(tempScale);
      pressure_json.add(roundf(sensorPressure * powf(10, PressureDecimals)));
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
      humidity_json.add(roundf(sensorHumidity * powf(10, HumidityDecimals)));
      humidity_json.add(F("%"));
      pressure_json.add(roundf(sensorPressure * powf(10, PressureDecimals)));
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
    JsonObject top = root.createNestedObject(F("BME280/BMP280"));
    top[F("TemperatureDecimals")] = TemperatureDecimals;
    top[F("HumidityDecimals")] = HumidityDecimals;
    top[F("PressureDecimals")] = PressureDecimals;
    top[F("TemperatureInterval")] = TemperatureInterval;
    top[F("PressureInterval")] = PressureInterval;
    top[F("PublishAlways")] = PublishAlways;
    top[F("UseCelsius")] = UseCelsius;
    top[F("HomeAssistantDiscovery")] = HomeAssistantDiscovery;
    JsonArray io_pin = top.createNestedArray(F("pin"));
    for (byte i=0; i<2; i++) io_pin.add(ioPin[i]);
    top[F("help4Pins")] = F("SCL,SDA"); // help for Settings page
    DEBUG_PRINTLN(F("BME280 config saved."));
  }

  // Read Usermod Config Settings
  bool readFromConfig(JsonObject& root)
  {
    // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
    // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)


    int8_t newPin[2]; for (byte i=0; i<2; i++) newPin[i] = ioPin[i]; // prepare to note changed pins

    JsonObject top = root[F("BME280/BMP280")];
    if (top.isNull()) {
      DEBUG_PRINT(F("BME280/BMP280"));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }
    bool configComplete = !top.isNull();

    // A 3-argument getJsonValue() assigns the 3rd argument as a default value if the Json value is missing
    configComplete &= getJsonValue(top[F("TemperatureDecimals")], TemperatureDecimals, 1);
    configComplete &= getJsonValue(top[F("HumidityDecimals")], HumidityDecimals, 0);
    configComplete &= getJsonValue(top[F("PressureDecimals")], PressureDecimals, 0);
    configComplete &= getJsonValue(top[F("TemperatureInterval")], TemperatureInterval, 30);
    configComplete &= getJsonValue(top[F("PressureInterval")], PressureInterval, 30);
    configComplete &= getJsonValue(top[F("PublishAlways")], PublishAlways, false);
    configComplete &= getJsonValue(top[F("UseCelsius")], UseCelsius, true);
    configComplete &= getJsonValue(top[F("HomeAssistantDiscovery")], HomeAssistantDiscovery, false);
    for (byte i=0; i<2; i++) configComplete &= getJsonValue(top[F("pin")][i], newPin[i], ioPin[i]);

    DEBUG_PRINT(FPSTR(F("BME280/BMP280")));
    if (!initDone) {
      // first run: reading from cfg.json
      for (byte i=0; i<2; i++) ioPin[i] = newPin[i];
      DEBUG_PRINTLN(F(" config loaded."));
    } else {
      DEBUG_PRINTLN(F(" config (re)loaded."));
      // changing parameters from settings page
      bool pinsChanged = false;
      for (byte i=0; i<2; i++) if (ioPin[i] != newPin[i]) { pinsChanged = true; break; } // check if any pins changed
      if (pinsChanged) { //if pins changed, deallocate old pins and allocate new ones
        PinOwner po = PinOwner::UM_BME280;
        if (ioPin[0]==HW_PIN_SCL && ioPin[1]==HW_PIN_SDA) po = PinOwner::HW_I2C;  // allow multiple allocations of HW I2C bus pins
        pinManager.deallocateMultiplePins((const uint8_t *)ioPin, 2, po);  // deallocate pins
        for (byte i=0; i<2; i++) ioPin[i] = newPin[i];
        setup();
      }
      // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
      return !top[F("pin")].isNull();
    }

    return configComplete;
  }

  uint16_t getId() {
    return USERMOD_ID_BME280;
  }
};