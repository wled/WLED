#include "wled.h"
#include <ld2410.h>

#ifdef WLED_DISABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

class LD2410Usermod : public Usermod {

  private:

    bool enabled = true;
    bool initDone = false;
    bool sensorFound = false;
    unsigned long lastTime = 0;
    unsigned long last_mqtt_sent = 0;

    int8_t default_uart_rx = 19;
    int8_t default_uart_tx = 18;


    String mqttMovementTopic = F("");
    String mqttStationaryTopic = F("");
    bool mqttInitialized = false;
    bool HomeAssistantDiscovery = true; // Publish Home Assistant Discovery messages


    ld2410 radar;
    bool stationary_detected = false;
    bool last_stationary_state = false;
    bool movement_detected = false;
    bool last_movement_state = false;

    // These config variables have defaults set inside readFromConfig()
    int8_t uart_rx_pin;
    int8_t uart_tx_pin;

    // string that are used multiple time (this will save some flash memory)
    static const char _name[];
    static const char _enabled[];

    void publishMqtt(const char* topic, const char* state, bool retain); // example for publishing MQTT message

    void _mqttInitialize()
    {
      mqttMovementTopic = String(mqttDeviceTopic) + F("/ld2410/movement");
      mqttStationaryTopic = String(mqttDeviceTopic) + F("/ld2410/stationary");
      if (HomeAssistantDiscovery){
        _createMqttSensor(F("Movement"), mqttMovementTopic, F("motion"), F(""));
        _createMqttSensor(F("Stationary"), mqttStationaryTopic, F("occupancy"), F(""));
      } 
    }

    // Create an MQTT Sensor for Home Assistant Discovery purposes, this includes a pointer to the topic that is published to in the Loop.
    void _createMqttSensor(const String &name, const String &topic, const String &deviceClass, const String &unitOfMeasurement)
    {
      String t = String(F("homeassistant/binary_sensor/")) + mqttClientID + F("/") + name + F("/config");
      
      StaticJsonDocument<600> doc;
      
      doc[F("name")] = String(serverDescription) + F(" Module");
      doc[F("state_topic")] = topic;
      doc[F("unique_id")] = String(mqttClientID) + name;
      if (unitOfMeasurement != "")
        doc[F("unit_of_measurement")] = unitOfMeasurement;
      if (deviceClass != "")
        doc[F("device_class")] = deviceClass;
      doc[F("expire_after")] = 1800;
      doc[F("payload_off")] = "OFF";
      doc[F("payload_on")] = "ON";

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

    inline bool isEnabled() { return enabled; }

    void setup() {
      Serial1.begin(256000, SERIAL_8N1, uart_rx_pin, uart_tx_pin);
      Serial.print(F("\nLD2410 radar sensor initialising: "));
      if(radar.begin(Serial1)){
        Serial.println(F("OK"));
      } else {
        Serial.println(F("not connected"));
      }
      initDone = true;
    }


    void loop() {
      // NOTE: on very long strips strip.isUpdating() may always return true so update accordingly
      if (!enabled || strip.isUpdating()) return;
      radar.read();
      unsigned long curr_time = millis();
      if(curr_time - lastTime > 1000)  //Try to Report every 1000ms
      {
        lastTime = curr_time;
        sensorFound = radar.isConnected();
        if(!sensorFound) return;
        stationary_detected = radar.presenceDetected();
        if(stationary_detected != last_stationary_state){
          if (WLED_MQTT_CONNECTED){
            publishMqtt("/ld2410/stationary", stationary_detected ? "ON":"OFF", false);
            last_stationary_state = stationary_detected;
          }
        }
        movement_detected = radar.movingTargetDetected();
        if(movement_detected != last_movement_state){
          if (WLED_MQTT_CONNECTED){
            publishMqtt("/ld2410/movement", movement_detected ? "ON":"OFF", false);
            last_movement_state = movement_detected;
          }
        }
        // If there hasn't been any activity, send current state to confirm sensor is alive
        if(curr_time - last_mqtt_sent > 1000*60*5 && WLED_MQTT_CONNECTED){
          publishMqtt("/ld2410/stationary", stationary_detected ? "ON":"OFF", false);
          publishMqtt("/ld2410/movement", movement_detected ? "ON":"OFF", false);
        }
      }
    }


    void addToJsonInfo(JsonObject& root)
    {
      // if "u" object does not exist yet wee need to create it
      JsonObject user = root[F("u")];
      if (user.isNull()) user = root.createNestedObject(F("u"));

      JsonArray ld2410_sta_json = user.createNestedArray(F("LD2410 Stationary"));
      JsonArray ld2410_mov_json = user.createNestedArray(F("LD2410 Movement"));
      if (!enabled){
        ld2410_sta_json.add(F("disabled"));
        ld2410_mov_json.add(F("disabled"));
      } else if(!sensorFound){
        ld2410_sta_json.add(F("LD2410"));
        ld2410_sta_json.add(" Not Found");
      } else {
        ld2410_sta_json.add("Sta ");
        ld2410_sta_json.add(stationary_detected ? "ON":"OFF");
        ld2410_mov_json.add("Mov ");
        ld2410_mov_json.add(movement_detected ? "ON":"OFF");
      }
    }

    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      //save these vars persistently whenever settings are saved
      top["uart_rx_pin"] = default_uart_rx;
      top["uart_tx_pin"] = default_uart_tx;
    }


    bool readFromConfig(JsonObject& root)
    {
      // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
      // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)

      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();
      if (!configComplete)
      {
        DEBUG_PRINT(FPSTR(_name));
        DEBUG_PRINT(F("LD2410"));
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      configComplete &= getJsonValue(top["uart_rx_pin"], uart_rx_pin, default_uart_rx);
      configComplete &= getJsonValue(top["uart_tx_pin"], uart_tx_pin, default_uart_tx);

      return configComplete;
    }


#ifndef WLED_DISABLE_MQTT
    /**
     * onMqttConnect() is called when MQTT connection is established
     */
    void onMqttConnect(bool sessionPresent) {
      // do any MQTT related initialisation here
      if(!radar.isConnected()) return;
      publishMqtt("/ld2410/status", "I am alive!", false);
      if (!mqttInitialized)
      {
        _mqttInitialize();
        mqttInitialized = true;
      }
    }
#endif

    uint16_t getId()
    {
      return USERMOD_ID_LD2410;
    }
};


// add more strings here to reduce flash memory usage
const char LD2410Usermod::_name[]    PROGMEM = "LD2410Usermod";
const char LD2410Usermod::_enabled[] PROGMEM = "enabled";


// implementation of non-inline member methods

void LD2410Usermod::publishMqtt(const char* topic, const char* state, bool retain)
{
#ifndef WLED_DISABLE_MQTT
  //Check if MQTT Connected, otherwise it will crash
  if (WLED_MQTT_CONNECTED) {
    last_mqtt_sent = millis();
    char subuf[64];
    strcpy(subuf, mqttDeviceTopic);
    strcat(subuf, topic);
    mqtt->publish(subuf, 0, retain, state);
  }
#endif
}


static LD2410Usermod ld2410_v2;
REGISTER_USERMOD(ld2410_v2);