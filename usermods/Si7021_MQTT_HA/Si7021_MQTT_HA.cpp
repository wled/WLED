// this is remixed from usermod_v2_SensorsToMqtt.h (sensors_to_mqtt usermod)
// and usermod_multi_relay.h (multi_relay usermod)

#include "wled.h"
#include <Adafruit_Si7021.h>
#include <EnvironmentCalculations.h> // EnvironmentCalculations::HeatIndex(), ::DewPoint(), ::AbsoluteHumidity()

#ifdef WLED_DISABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

static Adafruit_Si7021 si7021;

class Si7021_MQTT_HA : public Usermod
{
  private:
    bool sensorInitialized = false;
    bool mqttInitialized = false;
    float sensorTemperature = 0;
    float sensorHumidity = 0;
    float sensorHeatIndex = 0;
    float sensorDewPoint = 0;
    float sensorAbsoluteHumidity= 0;
    String mqttTemperatureTopic = "";
    String mqttHumidityTopic = "";
    String mqttHeatIndexTopic = "";
    String mqttDewPointTopic = "";
    String mqttAbsoluteHumidityTopic = "";
    unsigned long nextMeasure = 0;
    bool enabled = false;
    bool haAutoDiscovery = true;
    bool sendAdditionalSensors = true;

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _sendAdditionalSensors[];
    static const char _haAutoDiscovery[];

    void _initializeSensor()
    {
      sensorInitialized = si7021.begin();
      Serial.printf("Si7021_MQTT_HA: sensorInitialized = %d\n", sensorInitialized);
    }

    void _initializeMqtt()
    {
      mqttTemperatureTopic = String(mqttDeviceTopic) + "/si7021_temperature";
      mqttHumidityTopic = String(mqttDeviceTopic) + "/si7021_humidity";
      mqttHeatIndexTopic = String(mqttDeviceTopic) + "/si7021_heat_index";
      mqttDewPointTopic = String(mqttDeviceTopic) + "/si7021_dew_point";
      mqttAbsoluteHumidityTopic = String(mqttDeviceTopic) + "/si7021_absolute_humidity";

      // Update and publish sensor data
      _updateSensorData();
      _publishSensorData();

      if (haAutoDiscovery) {
        _publishHAMqttSensor("temperature", "Temperature", mqttTemperatureTopic, "temperature", "°C");
        _publishHAMqttSensor("humidity", "Humidity", mqttHumidityTopic, "humidity", "%");
        if (sendAdditionalSensors) {
          _publishHAMqttSensor("heat_index", "Heat Index", mqttHeatIndexTopic, "temperature", "°C");
          _publishHAMqttSensor("dew_point", "Dew Point", mqttDewPointTopic, "", "°C");
          _publishHAMqttSensor("absolute_humidity", "Absolute Humidity", mqttAbsoluteHumidityTopic, "", "g/m³");
        }
      }
      
      mqttInitialized = true;
    }

    void _publishHAMqttSensor(
      const String &name, 
      const String &friendly_name, 
      const String &state_topic, 
      const String &deviceClass, 
      const String &unitOfMeasurement)
    {
      if (WLED_MQTT_CONNECTED) {
        String topic = String("homeassistant/sensor/") + mqttClientID + "/" + name + "/config";

        StaticJsonDocument<300> doc;

        doc["name"] = String(serverDescription) + " " + friendly_name;
        doc["state_topic"] = state_topic;
        doc["unique_id"] = String(mqttClientID) + name;
        if (unitOfMeasurement != "")
          doc["unit_of_measurement"] = unitOfMeasurement;
        if (deviceClass != "")
          doc["device_class"] = deviceClass;
        doc["expire_after"] = 1800;

        JsonObject device = doc.createNestedObject("device"); // attach the sensor to the same device
        device["name"] = String(serverDescription);
        device["model"] = F(WLED_PRODUCT_NAME);
        device["manufacturer"] = F(WLED_BRAND);
        device["identifiers"] = String("wled-") + String(serverDescription);
        device["sw_version"] = VERSION;

        String payload;
        serializeJson(doc, payload);

        mqtt->publish(topic.c_str(), 0, true, payload.c_str());
      }
    }

    void _updateSensorData()
    {
      sensorTemperature = si7021.readTemperature();
      sensorHumidity = si7021.readHumidity();

      // Serial.print("Si7021_MQTT_HA: Temperature: ");
      // Serial.print(sensorTemperature, 2);
      // Serial.print("\tHumidity: ");
      // Serial.print(sensorHumidity, 2);

      if (sendAdditionalSensors) {
        EnvironmentCalculations::TempUnit envTempUnit(EnvironmentCalculations::TempUnit_Celsius);
        sensorHeatIndex = EnvironmentCalculations::HeatIndex(sensorTemperature, sensorHumidity, envTempUnit);
        sensorDewPoint = EnvironmentCalculations::DewPoint(sensorTemperature, sensorHumidity, envTempUnit);
        sensorAbsoluteHumidity = EnvironmentCalculations::AbsoluteHumidity(sensorTemperature, sensorHumidity, envTempUnit);

        // Serial.print("\tHeat Index: ");
        // Serial.print(sensorHeatIndex, 2);
        // Serial.print("\tDew Point: ");
        // Serial.print(sensorDewPoint, 2);
        // Serial.print("\tAbsolute Humidity: ");
        // Serial.println(sensorAbsoluteHumidity, 2);
      }
      // else
      //   Serial.println("");
    }

    void _publishSensorData()
    {
      if (WLED_MQTT_CONNECTED) {
        mqtt->publish(mqttTemperatureTopic.c_str(), 0, false, String(sensorTemperature).c_str());
        mqtt->publish(mqttHumidityTopic.c_str(), 0, false, String(sensorHumidity).c_str());
        if (sendAdditionalSensors) {
          mqtt->publish(mqttHeatIndexTopic.c_str(), 0, false, String(sensorHeatIndex).c_str());
          mqtt->publish(mqttDewPointTopic.c_str(), 0, false, String(sensorDewPoint).c_str());
          mqtt->publish(mqttAbsoluteHumidityTopic.c_str(), 0, false, String(sensorAbsoluteHumidity).c_str());
        }
      }
    }

  public:
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_sendAdditionalSensors)] = sendAdditionalSensors;
      top[FPSTR(_haAutoDiscovery)] = haAutoDiscovery;
    }

    bool readFromConfig(JsonObject& root)
    {
      JsonObject top = root[FPSTR(_name)];
      
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_sendAdditionalSensors)], sendAdditionalSensors);
      configComplete &= getJsonValue(top[FPSTR(_haAutoDiscovery)], haAutoDiscovery);

      return configComplete;
    }

    void onMqttConnect(bool sessionPresent) {
      if (mqttDeviceTopic[0] != 0)
        _initializeMqtt();
    }

    void setup()
    {
      if (enabled) {
        Serial.println("Si7021_MQTT_HA: Starting!");
        Serial.println("Si7021_MQTT_HA: Initializing sensors.. ");
        _initializeSensor();
      }
    }

    // gets called every time WiFi is (re-)connected.
    void connected()
    {
      nextMeasure = millis() + 5000; // Schedule next measure in 5 seconds
    }

    void loop()
    {
      yield();
      if (!enabled || strip.isUpdating()) return; // !sensorFound || 

      unsigned long tempTimer = millis();

      if (tempTimer > nextMeasure) {
        nextMeasure = tempTimer + 60000; // Schedule next measure in 60 seconds

        if (!sensorInitialized) {
          Serial.println("Si7021_MQTT_HA: Error! Sensors not initialized in loop()!");
          _initializeSensor();
          return; // lets try again next loop
        }

        if (WLED_MQTT_CONNECTED) {
          if (!mqttInitialized)
            _initializeMqtt();

          // Update and publish sensor data
          _updateSensorData();
          _publishSensorData();
        }
        else {
          Serial.println("Si7021_MQTT_HA: Missing MQTT connection. Not publishing data");
          mqttInitialized = false;
        }
      }
    }
    
    uint16_t getId()
    {
      return USERMOD_ID_SI7021_MQTT_HA;
    }
};

// strings to reduce flash memory usage (used more than twice)
const char Si7021_MQTT_HA::_name[]                   PROGMEM = "Si7021 MQTT (Home Assistant)";
const char Si7021_MQTT_HA::_enabled[]                PROGMEM = "enabled";
const char Si7021_MQTT_HA::_sendAdditionalSensors[]  PROGMEM = "Send Dew Point, Abs. Humidity and Heat Index";
const char Si7021_MQTT_HA::_haAutoDiscovery[]        PROGMEM = "Home Assistant MQTT Auto-Discovery";


static Si7021_MQTT_HA si7021_mqtt_ha;
REGISTER_USERMOD(si7021_mqtt_ha);