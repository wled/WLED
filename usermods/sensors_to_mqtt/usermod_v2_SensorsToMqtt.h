#ifndef WLED_ENABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

#pragma once

#include "wled.h"
#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_CCS811.h>
#include <Adafruit_Si7021.h>

Adafruit_BMP280 bmp;
Adafruit_Si7021 si7021;
Adafruit_CCS811 ccs811;

class UserMod_SensorsToMQTT : public Usermod
{
private:
  bool initialized = false;
  bool mqttInitialized = false;
  float SensorPressure = 0;
  float SensorTemperature = 0;
  float SensorHumidity = 0;
  char *SensorIaq = "Unknown";
  String mqttTemperatureTopic = "";
  String mqttHumidityTopic = "";
  String mqttPressureTopic = "";
  String mqttTvocTopic = "";
  String mqttEco2Topic = "";
  String mqttIaqTopic = "";
  unsigned int SensorTvoc = 0;
  unsigned int SensorEco2 = 0;
  unsigned long nextMeasure = 0;

  void _initialize()
  {
    initialized = bmp.begin(BMP280_ADDRESS_ALT);
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,      /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X16,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,     /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,       /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_2000); /* Refresh values every 20 seconds */

    initialized &= si7021.begin();
    initialized &= ccs811.begin();
    ccs811.setDriveMode(CCS811_DRIVE_MODE_10SEC); /* Refresh values every 10s */
    Serial.print(initialized);
  }

  void _mqttInitialize()
  {
    mqttTemperatureTopic = String(mqttDeviceTopic) + "/temperature";
    mqttPressureTopic = String(mqttDeviceTopic) + "/pressure";
    mqttHumidityTopic = String(mqttDeviceTopic) + "/humidity";
    mqttTvocTopic = String(mqttDeviceTopic) + "/tvoc";
    mqttEco2Topic = String(mqttDeviceTopic) + "/eco2";
    mqttIaqTopic = String(mqttDeviceTopic) + "/iaq";

    String t = String("homeassistant/sensor/") + mqttClientID + "/temperature/config";

    _createMqttSensor("temperature", mqttTemperatureTopic, "temperature", "°C");
    _createMqttSensor("pressure", mqttPressureTopic, "pressure", "hPa");
    _createMqttSensor("humidity", mqttHumidityTopic, "humidity", "%");
    _createMqttSensor("tvoc", mqttTvocTopic, "", "ppb");
    _createMqttSensor("eco2", mqttEco2Topic, "", "ppm");
    _createMqttSensor("iaq", mqttIaqTopic, "", "");
  }

  void _createMqttSensor(const String &name, const String &topic, const String &deviceClass, const String &unitOfMeasurement)
  {
    String t = String("homeassistant/sensor/") + mqttClientID + "/" + name + "/config";

    StaticJsonDocument<300> doc;

    doc["name"] = name;
    doc["state_topic"] = topic;
    doc["unique_id"] = String(mqttClientID) + name;
    if (unitOfMeasurement != "")
      doc["unit_of_measurement"] = unitOfMeasurement;
    if (deviceClass != "")
      doc["device_class"] = deviceClass;
    doc["expire_after"] = 1800;

    JsonObject device = doc.createNestedObject("device"); // attach the sensor to the same device
    device["identifiers"] = String("wled-sensor-") + mqttClientID;
    device["manufacturer"] = "Aircoookie";
    device["model"] = "WLED";
    device["sw_version"] = VERSION;
    device["name"] = mqttClientID;

    String temp;
    serializeJson(doc, temp);
    Serial.println(t);
    Serial.println(temp);

    mqtt->publish(t.c_str(), 0, true, temp.c_str());
  }

  void _updateSensorData()
  {
    SensorTemperature = bmp.readTemperature();
    SensorHumidity = si7021.readHumidity();
    SensorPressure = (bmp.readPressure() / 100.0F);
    ccs811.setEnvironmentalData(SensorHumidity, SensorTemperature);
    ccs811.readData();
    SensorTvoc = ccs811.getTVOC();
    SensorEco2 = ccs811.geteCO2();
    SensorIaq = _getIaqIndex(SensorHumidity, SensorTvoc, SensorEco2);

    Serial.printf("%f c, %f humidity, %f hPA, %u tvoc, %u Eco2, %s iaq\n",
                  SensorTemperature, SensorHumidity, SensorPressure,
                  SensorTvoc, SensorEco2, SensorIaq);
  }

  /**
   * Credits: Bouke_Regnerus @ https://community.home-assistant.io/t/example-indoor-air-quality-text-sensor-using-ccs811-sensor/125854 
   */
  char *_getIaqIndex(float humidity, int tvoc, int eco2)
  {
    int iaq_index = 0;

    /*
       * Transform indoor humidity values to IAQ points according to Indoor Air Quality UK: 
       * http://www.iaquk.org.uk/
       */
    if (humidity < 10 or humidity > 90)
    {
      iaq_index += 1;
    }
    else if (humidity < 20 or humidity > 80)
    {
      iaq_index += 2;
    }
    else if (humidity < 30 or humidity > 70)
    {
      iaq_index += 3;
    }
    else if (humidity < 40 or humidity > 60)
    {
      iaq_index += 4;
    }
    else if (humidity >= 40 and humidity <= 60)
    {
      iaq_index += 5;
    }

    /*
       * Transform eCO2 values to IAQ points according to Indoor Air Quality UK: 
       * http://www.iaquk.org.uk/
       */
    if (eco2 <= 600)
    {
      iaq_index += 5;
    }
    else if (eco2 <= 800)
    {
      iaq_index += 4;
    }
    else if (eco2 <= 1500)
    {
      iaq_index += 3;
    }
    else if (eco2 <= 1800)
    {
      iaq_index += 2;
    }
    else if (eco2 > 1800)
    {
      iaq_index += 1;
    }

    /*
       * Transform TVOC values to IAQ points according to German environmental guidelines: 
       * https://www.repcomsrl.com/wp-content/uploads/2017/06/Environmental_Sensing_VOC_Product_Brochure_EN.pdf
       */
    if (tvoc <= 65)
    {
      iaq_index += 5;
    }
    else if (tvoc <= 220)
    {
      iaq_index += 4;
    }
    else if (tvoc <= 660)
    {
      iaq_index += 3;
    }
    else if (tvoc <= 2200)
    {
      iaq_index += 2;
    }
    else if (tvoc > 2200)
    {
      iaq_index += 1;
    }

    if (iaq_index <= 6)
    {
      return "Unhealty";
    }
    else if (iaq_index <= 9)
    {
      return "Poor";
    }
    else if (iaq_index <= 12)
    {
      return "Moderate";
    }
    else if (iaq_index <= 14)
    {
      return "Good";
    }
    else if (iaq_index > 14)
    {
      return "Excellent";
    }
  }

public:
  void setup()
  {
    Serial.println("Starting!");
    Serial.println("Initializing sensors.. ");
    _initialize();
  }

  // gets called every time WiFi is (re-)connected.
  void connected()
  {
    nextMeasure = millis() + 5000; // Schedule next measure in 5 seconds
  }

  void loop()
  {
    unsigned long tempTimer = millis();

    if (tempTimer > nextMeasure)
    {
      nextMeasure = tempTimer + 60000; // Schedule next measure in 60 seconds

      if (!initialized)
      {
        Serial.println("Error! Sensors not initialized in loop()!");
        _initialize();
        return; // lets try again next loop
      }

      if (mqtt != nullptr && mqtt->connected())
      {
        if (!mqttInitialized)
        {
          _mqttInitialize();
          mqttInitialized = true;
        }

        // Update sensor data
        _updateSensorData();

        // Create string populated with user defined device topic from the UI,
        // and the read temperature, humidity and pressure.
        // Then publish to MQTT server.
        mqtt->publish(mqttTemperatureTopic.c_str(), 0, true, String(SensorTemperature).c_str());
        mqtt->publish(mqttPressureTopic.c_str(), 0, true, String(SensorPressure).c_str());
        mqtt->publish(mqttHumidityTopic.c_str(), 0, true, String(SensorHumidity).c_str());
        mqtt->publish(mqttTvocTopic.c_str(), 0, true, String(SensorTvoc).c_str());
        mqtt->publish(mqttEco2Topic.c_str(), 0, true, String(SensorEco2).c_str());
        mqtt->publish(mqttIaqTopic.c_str(), 0, true, String(SensorIaq).c_str());
      }
      else
      {
        Serial.println("Missing MQTT connection. Not publishing data");
        mqttInitialized = false;
      }
    }
  }
};
