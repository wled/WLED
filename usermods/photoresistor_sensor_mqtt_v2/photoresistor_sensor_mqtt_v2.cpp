#include "wled.h"

class photoresistor_sensor_mqtt_v2 : public Usermod
{
private:
  unsigned long update_interval = 3000;
  float change_threshold = 1;
  static const char MQTT_TOPIC[];
  static const char _name[];
  const bool HomeAssistantDiscovery = true; // is HA discovery turned on by default
  int ldrPin = A0;
  unsigned long lastTime = 0;
  unsigned int lightValue = 0;
  float lightPercentage = 0;
  float lastPercentage = 0;
  bool hassDiscoverySent = false;
  bool ldrEnabled = true;
  bool inverted = false;
  bool initDone = false;
  String topic_buf;

public:
  photoresistor_sensor_mqtt_v2() {}

  void setup()
  {
    // register ldrPin
    if ((ldrPin >= 0) && ldrEnabled)
    {
      if (!PinManager::allocatePin(ldrPin, false, PinOwner::UM_PHOTO_RESISTOR_MQTT))
      {
        ldrEnabled = false; // pin already in use -> disable usermod
      }
      else
      {
        pinMode(ldrPin, INPUT); // alloc success -> configure pin for input
      }
    }
    else
    {
      ldrEnabled = false; // invalid pin -> disable usermod
    }
    initDone = true;
  }

  void connected()
  {
    if (WLED_MQTT_CONNECTED && HomeAssistantDiscovery)
    {
      publishDiscovery();
    }
  }

  void loop()
  {
    if (initDone && ldrEnabled && WLED_MQTT_CONNECTED)
    {
      bool force_update = (millis() - lastTime) > (update_interval * 100); // to keep the mqtt alive if the light is not changing
      bool regular_update = (millis() - lastTime) > update_interval;
      if (regular_update || force_update)
      {
        lightValue = analogRead(ldrPin);
#ifdef ESP32
        const float mapping = 4096.0; // ESP32 ADC resolution is 12 bit, so 4096 values
#else
        const float mapping = 1024.0; // ESP8266 ADC resolution is 10 bit, so 1024 values
#endif
        if (inverted)
        {
          lightPercentage = 100.0 - ((float)lightValue * -1 + mapping) / (float)mapping * 100;
        }
        else
        {
          lightPercentage = ((float)lightValue * -1 + mapping) / (float)mapping * 100;
        }

        if (abs(lightPercentage - lastPercentage) > change_threshold || force_update)
        {
          if (WLED_MQTT_CONNECTED)
          {
            publishMqtt(lightPercentage);
          }
          lastTime = millis();
          lastPercentage = lightPercentage;
          // debug
          char buf[64];
          sprintf_P(buf, PSTR("LDR: %d (%d)"), (int)lightPercentage, lightValue);
          DEBUG_PRINTLN(buf);
        }
      }
    }
  }

  void addToConfig(JsonObject &root)
  {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top["Enabled"] = ldrEnabled;
    top["LDR Pin"] = ldrPin;
    top["LDR update interval"] = update_interval;
    top["LDR inverted"] = inverted;
    top["change threshould"] = change_threshold;
  }

  bool readFromConfig(JsonObject &root)
  {
    int8_t oldLdrPin = ldrPin;
    JsonObject top = root[FPSTR(_name)];
    bool configComplete = !top.isNull();
    configComplete &= getJsonValue(top["Enabled"], ldrEnabled);
    configComplete &= getJsonValue(top["LDR Pin"], ldrPin);
    configComplete &= getJsonValue(top["LDR update interval"], update_interval);
    configComplete &= getJsonValue(top["LDR inverted"], inverted);
    configComplete &= getJsonValue(top["change threshould"], change_threshold);
    // pin changed - un-register previous pin, register new pin
    PinManager::deallocatePin(oldLdrPin, PinOwner::UM_PHOTO_RESISTOR_MQTT);
    if (ldrEnabled && ldrPin > 0)
    {
      setup(); // setup again
    }
    return configComplete;
  }

  void addToJsonInfo(JsonObject &root)
  {
    // If "u" object does not exist yet we need to create it
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");

    JsonArray LDR_Enabled = user.createNestedArray("LDR enabled");
    LDR_Enabled.add(ldrEnabled);
    if (!ldrEnabled)
      return; // do not add more if usermod is disabled

    JsonArray LDR_Reading = user.createNestedArray("LDR reading");
    LDR_Reading.add(lightPercentage);

    JsonArray LDR_interval = user.createNestedArray("LDR interval");
    LDR_interval.add(update_interval);

    JsonArray LDR_change_threshold = user.createNestedArray("change threshould");
    LDR_change_threshold.add(change_threshold);
  }

  uint16_t getId()
  {
    return USERMOD_PHOTORESISTOR_MQTT_V2;
  }

  void publishMqtt(float state)
  {
    if (WLED_MQTT_CONNECTED)
    {
      topic_buf = mqttDeviceTopic;
      topic_buf += MQTT_TOPIC;
      if (!hassDiscoverySent && HomeAssistantDiscovery)
      {
        publishDiscovery();
      }
      mqtt->publish(topic_buf.c_str(), 0, true, String(state).c_str());
    }
  }

  void publishDiscovery()
  {
    StaticJsonDocument<600> doc;
    String uid = escapedMac.c_str();
    doc[F("name")] = F("Light Sensor");
    doc[F("stat_t")] = topic_buf.c_str();
    uid += F("_ldr");
    doc[F("uniq_id")] = uid;
    doc[F("dev_cla")] = F("illuminance");
    doc[F("unit_of_meas")] = F("lx");
    doc[F("val_tpl")] = F("{{ value }}");

    JsonObject device = doc.createNestedObject(F("device")); // attach the sensor to the same device
    device[F("name")] = serverDescription;
    device[F("ids")] = serverDescription;
    device[F("mf")] = F(WLED_BRAND);
    device[F("mdl")] = F(WLED_PRODUCT_NAME);
    device[F("sw")] = versionString;
#ifdef ESP32
    device[F("hw_version")] = F("esp32");
#else
    device[F("hw_version")] = F("esp8266");
#endif
    JsonArray connections = device[F("connections")].createNestedArray();
    connections.add(F("mac"));
    connections.add(WiFi.macAddress());

    String discovery_topic = F("homeassistant/sensor/");
    discovery_topic += uid;
    discovery_topic += F("/ldr/config");
    char json_str[600];
    size_t payload_size = serializeJson(doc, json_str);
    mqtt->publish(discovery_topic.c_str(), 0, true, json_str, payload_size);
    hassDiscoverySent = true;
  }
};
const char photoresistor_sensor_mqtt_v2::_name[] PROGMEM = "photoresistor_sensor_mqtt_v2";
const char photoresistor_sensor_mqtt_v2::MQTT_TOPIC[] PROGMEM = "/ldr";

static photoresistor_sensor_mqtt_v2 photoresistor_sensor_mqtt_v2_instance;
REGISTER_USERMOD(photoresistor_sensor_mqtt_v2_instance);