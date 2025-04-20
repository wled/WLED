#include "wled.h"
#ifndef UM_ADC_MQTT_PIN_MAX_NUMBER
#ifdef ESP8266
#define UM_ADC_MQTT_PIN_MAX_NUMBER 1
#else
#define UM_ADC_MQTT_PIN_MAX_NUMBER 3
#endif
#endif

#ifdef ESP8266 // static assert
#if (UM_ADC_MQTT_PIN_MAX_NUMBER > 1)
#error UM_ADC_MQTT_PIN_MAX_NUMBER > 1 not supported on ESP8266
#endif
#endif


#ifdef ARDUINO_ARCH_ESP32 // esp8266 always use A0 no use of pin choice
#if defined(CONFIG_IDF_TARGET_ESP32)
      #define _valid_adc_pin(__pin)  (__pin >= 32U && __pin <= 39U) // only ADC1 available on ESP32 with wifi
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2)
      #define _valid_adc_pin(__pin) (__pin >= 1 && __pin <= 10) // only ADC1 available on ESP32-S2/ S3 with wifi
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
      #define _valid_adc_pin(__pin)  (__pin < 5) // only ADC1 available on ESP32-C3 with wifi
#else
#error "Unknown ESP32 target"
#endif
#endif 

class adc_sensor_mqtt : public Usermod
{
private:
  unsigned long update_interval = 2000;
  float change_threshold = 1;
  static const char MQTT_TOPIC[];
  static const char _name[];
  bool HomeAssistantDiscovery = true; // is HA discovery turned on by default
  bool publishRawValue = false;       // publish raw value to MQTT
  int8_t adc_pin[UM_ADC_MQTT_PIN_MAX_NUMBER];
  unsigned long lastTime = 0;
  uint16_t adc_value[UM_ADC_MQTT_PIN_MAX_NUMBER];
  uint16_t adc_last_value[UM_ADC_MQTT_PIN_MAX_NUMBER];
  float adc_Percentage[UM_ADC_MQTT_PIN_MAX_NUMBER];
  bool hassDiscoverySent = false;
  bool adc_enabled = false;
  bool inverted = false;
  bool initDone = false;
  String device_class[UM_ADC_MQTT_PIN_MAX_NUMBER];
  String unit_of_meas[UM_ADC_MQTT_PIN_MAX_NUMBER];
  bool published_initial_value = false;
public:
  adc_sensor_mqtt()
  {
    for (uint8_t i = 0; i < UM_ADC_MQTT_PIN_MAX_NUMBER; i++)
    {
#ifdef ESP8266         // static assert
      adc_pin[0] = 17; // A0
#else
      adc_pin[i] = -1;
#endif
      adc_value[i] = 0;
      adc_Percentage[i] = 0;
      adc_last_value[i] = 0;
      device_class[i] = "voltage"; // default device class
      unit_of_meas[i] = "V";       // default unit of measurement
    }
    // customize here your hass adc device class and unit of measurement
  }

  void setup()
  {
    uint8_t valid_pins = 0; // count valid pins so at least one pin is valid to enable adc , otherwise ignore invalud pins
    for (uint8_t i = 0; i < UM_ADC_MQTT_PIN_MAX_NUMBER; i++)
    {
      // register adc_pin
      if ((adc_pin[i] != -1) && adc_enabled)
      {
#ifdef ESP8266
        if (adc_pin[i] == 17) // only one Pin supported : A0 for esp8266 //
#else
        if ((adc_pin[i] >= 32) && PinManager::allocatePin(adc_pin[i], false, PinOwner::UM_ADC_MQTT)) // ESP32 only pins 32-39 are available for ADC
#endif
        {
          pinMode(adc_pin[i], INPUT); // alloc success -> configure pin for input
          valid_pins++;
        }
        else
        {
          DEBUG_PRINT(F("adc_sensor_mqtt: Pin allocation failed!"));
          adc_pin[i] = -1; // invalid pin -> disable this pin
        }
      }
    }
    if (valid_pins > 0)
    {
      adc_enabled = true; // at least one pin is valid
    }
    else
    {
      adc_enabled = false; // no valid pin at all -> disable adc
    }
    initDone = true;
  }

  void connected()
  {
    if (WLED_MQTT_CONNECTED && HomeAssistantDiscovery)
    {
      for (uint8_t i = 0; i < UM_ADC_MQTT_PIN_MAX_NUMBER; i++)
      {
        DEBUG_PRINT(F("Publishing discovery message..."));
        publishDiscovery(i);
      }
    }
  }

  void loop()
  {
    if (initDone && adc_enabled)
    {
       // force first update if mqtt is connected and no value was published yet
      bool force_update = (millis() - lastTime) > (update_interval * 100); // to keep the mqtt alive if the light is not changing
      bool regular_update = (millis() - lastTime) > update_interval;
      if (regular_update || force_update ) // force update if the time is up or if the light is not changing to retain mqtt hass activity in case
      {
        bool force_first_update = ((published_initial_value == false) && WLED_MQTT_CONNECTED);
        lastTime = millis(); // reset lastTime to current time
        for (uint8_t i = 0; i < UM_ADC_MQTT_PIN_MAX_NUMBER; i++)
        {
          if (adc_pin[i] < 0)
          {
            // DEBUG_PRINTLN(F("adc_sensor_mqtt:loop Pin not valid!"));
            continue; // skip if pin is not valid
          }
          adc_value[i] = analogRead(adc_pin[i]);
#ifdef ESP32
          const float mapping = 4096.0; // ESP32 ADC resolution is 12 bit, so 4096 values
#else
          const float mapping = 1024.0; // ESP8266 ADC resolution is 10 bit, so 1024 values
#endif
          if (inverted)
          {
            adc_Percentage[i] = 100.0 - ((float)adc_value[i] * -1 + mapping) / mapping * 100.0f;
          }
          else
          {
            adc_Percentage[i] = ((float)adc_value[i] * -1 + mapping) / mapping * 100.0f;
          }

          if (abs(adc_value[i] - adc_last_value[i]) > change_threshold || force_update || force_first_update)
          {
            adc_last_value[i] = adc_value[i];
            char buf[64];
            sprintf_P(buf, PSTR("adc %d: %d (%d)"), i, static_cast<uint8_t>(adc_Percentage[i]), adc_value[i]);
            DEBUG_PRINTLN(buf);
            if (WLED_MQTT_CONNECTED)
            {
              if (publishRawValue)
              {
                publishMqtt(i, adc_value[i]);
              }
              else
              {
                publishMqtt(i, adc_Percentage[i]);
              }
              published_initial_value = true;
            }
          }
        }
      }
    }
  }

  void addToConfig(JsonObject &root)
  {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top["Enabled"] = adc_enabled;
    for (uint8_t i = 0; i < UM_ADC_MQTT_PIN_MAX_NUMBER; i++)
    {
      // debug
      DEBUG_PRINT(F("configuring adc_sensor_mqtt: Pin "));
      DEBUG_PRINT(i);
#ifndef ESP8266 // esp8266 always use A0 no use of pin choice
      String adc_pin_name = F("AdcPin_");
      adc_pin_name += String(i);
      top[adc_pin_name] = adc_pin[i];
#endif
      // debug print new pin
      String device_class_name = F("DeviceClass_");
      device_class_name += String(i);
      top[device_class_name] = device_class[i].c_str();
      String unit_of_meas_name = F("UnitOfMeas_");
      unit_of_meas_name += String(i);
      top[unit_of_meas_name] = unit_of_meas[i].c_str();
    }
    top["AdcUpdateInterval"] = update_interval;
    top["Inverted"] = inverted;
    top["ChangeThreshold"] = change_threshold;
    top["HASS"] = HomeAssistantDiscovery;
    top["Raw"] = publishRawValue;
    DEBUG_PRINTLN(F("adc_sensor_mqtt: Config saved."));
    // print config
    // String jsonString;
    // serializeJson(top, jsonString);
    // DEBUG_PRINTLN(jsonString.c_str());
  }

  bool readFromConfig(JsonObject &root)
  {
    JsonObject top = root[FPSTR(_name)];
    bool configComplete = !top.isNull();
    if (!configComplete)
    {
      return false;
    }
    // read config
    // debug
    int8_t __attribute__((unused)) oldLdrPin[UM_ADC_MQTT_PIN_MAX_NUMBER];
    for (uint8_t i = 0; i < UM_ADC_MQTT_PIN_MAX_NUMBER; i++)
    {
      // debug
      DEBUG_PRINT(F("adc_sensor_mqtt: old Pin "));
      DEBUG_PRINTLN(adc_pin[i]);
      oldLdrPin[i] = adc_pin[i]; // store old pin for later deallocation
      String adc_pin_name = F("AdcPin_");
      adc_pin_name += String(i);
      String device_class_name = F("DeviceClass_");
      device_class_name += String(i);
      String unit_of_meas_name = F("UnitOfMeas_");
      unit_of_meas_name += String(i);
      configComplete &= getJsonValue(top[device_class_name], device_class[i]);
      configComplete &= getJsonValue(top[unit_of_meas_name], unit_of_meas[i]);
#ifdef ARDUINO_ARCH_ESP32 // esp8266 always use A0 no use of pin choice
      configComplete &= getJsonValue(top[adc_pin_name], adc_pin[i]);
      const bool valid_adc_pin = _valid_adc_pin(adc_pin[i]);  // only ADC1 available on ESP32 with wifi
      if (!valid_adc_pin || !PinManager::isPinOk(adc_pin[i], false)) // // ESP32 only pins 32-39 are available for ADC
      {
        DEBUG_PRINT(F("adc_sensor_mqtt: Pin "));
        DEBUG_PRINT(i);
        DEBUG_PRINT(F(" is not valid!"));
        configComplete = false; // pin not valid -> disable usermod
        adc_pin[i] = -1;        // invalid pin -> disable usermod
      }
      else
      {
        if (initDone && oldLdrPin[i] != -1) // config changed & new pin is fine  // if old pin was possibly allocated before , try to de allocate it
        {
          DEBUG_PRINTLN(F("adc_sensor_mqtt: Pin changed , deallocating old pin..."));
          PinManager::deallocatePin(oldLdrPin[i], PinOwner::UM_ADC_MQTT);
        }
      }
#endif // ESP8266 always use A0 no use of pin choice
    }
    configComplete &= getJsonValue(top["Enabled"], adc_enabled);
    configComplete &= getJsonValue(top["AdcUpdateInterval"], update_interval);
    configComplete &= getJsonValue(top["Inverted"], inverted);
    configComplete &= getJsonValue(top["ChangeThreshold"], change_threshold);
    configComplete &= getJsonValue(top["HASS"], HomeAssistantDiscovery);
    configComplete &= getJsonValue(top["Raw"], publishRawValue);
    // if pin changed after init -
    if (adc_enabled && initDone)
    {
      if (HomeAssistantDiscovery)
      {
        hassDiscoverySent = false; // reset discovery flag to publish again rework the config
      }
      DEBUG_PRINTLN(F("adc_sensor_mqtt: Pin changed , rerunning setup "));
      setup(); // setup again
    }
    return configComplete;
  }
  void appendConfigData(Print &uiScript) override
  {
    uiScript.print(F("addInfo('adc_sensor_mqtt:HASS',1,'HASS=Home Assistant Discovery');"));              // 0 is field type, 1 is actual field
    uiScript.print(F("addInfo('adc_sensor_mqtt:Enabled',1,'ADC enabled');"));                             // 0 is field type, 1 is actual field
    uiScript.print(F("addInfo('adc_sensor_mqtt:AdcUpdateInterval',1,'ADC update interval');"));           // 0 is field type, 1 is actual field
    uiScript.print(F("addInfo('adc_sensor_mqtt:Inverted',1,'ADC mapping voltage inverted');"));           // 0 is field type, 1 is actual field
    uiScript.print(F("addInfo('adc_sensor_mqtt:ChangeThreshold',1,'ADC change threshold');"));            // 0 is field type, 1 is actual field
    uiScript.print(F("addInfo('adc_sensor_mqtt:Raw',1,'ADC publish raw value instead of percentage');")); // 0 is field type, 1 is actual field
    uiScript.print(F("addInfo('adc_sensor_mqtt:DeviceClass_1',1,'ADC HASS device class');"));             // 0 is field type, 1 is actual field
    uiScript.print(F("addInfo('adc_sensor_mqtt:UnitOfMeas_1',1,'ADC HASS unit of measurement');"));       // 0 is field type, 1 is actual field
  }

  void addToJsonInfo(JsonObject &root)
  {
    // If "u" object does not exist yet we need to create it
    JsonObject user = root["u"];
    if (user.isNull())
      user = root.createNestedObject("u");

    if (!adc_enabled)
    {
      JsonArray adc_Enabled = user.createNestedArray("ADC enabled");
      adc_Enabled.add(adc_enabled);
      return; // do not add more if usermod is disabled
    }

    for (uint8_t i = 0; i < UM_ADC_MQTT_PIN_MAX_NUMBER; i++)
    {
      if (adc_pin[i] == -1)
      {
        // DEBUG_PRINTLN(F("addToJsonInfo adc_sensor_mqtt: Pin not valid!"));
        continue; // skip if pin is not valid
      }
      String adc_pin_name = F("ADC Reading ");
      adc_pin_name += String(i);
      adc_pin_name += F(":");
      JsonArray adc_Reading = user.createNestedArray(adc_pin_name);
      String adc_pin_value;
      if (HomeAssistantDiscovery)
      {
        adc_pin_value += F("(");
        adc_pin_value += device_class[i];
        adc_pin_value += F("-");
        adc_pin_value += unit_of_meas[i];
        adc_pin_value += F("):");
      }
      if (publishRawValue)
      {
        adc_pin_value += F("raw:");
        adc_pin_value += String(adc_value[i]);
      }
      else
      {
        adc_pin_value += String(adc_Percentage[i]);
        adc_pin_value += F("%");
      }
      adc_Reading.add(adc_pin_value);
    }
    // debug
    // String jsonString;
    // serializeJson(adc_Reading, jsonString);
    // DEBUG_PRINTLN(jsonString.c_str());
  }

  uint16_t getId()
  {
    return USERMOD_ID_ADC_MQTT;
  }

  void publishMqtt(int index, float state)
  {
    if (WLED_MQTT_CONNECTED)
    {
      DEBUG_PRINTLN(F("Publishing MQTT message..."));
      if (!hassDiscoverySent && HomeAssistantDiscovery)
      {
        // publish all discovery message if not already done
        for (uint8_t i = 0; i < UM_ADC_MQTT_PIN_MAX_NUMBER; i++)
        {
          publishDiscovery(i);
        }
      }
      String mqtt_stat_topic = mqttDeviceTopic;
      mqtt_stat_topic += MQTT_TOPIC;
      mqtt_stat_topic += String(index);
      mqtt->publish(mqtt_stat_topic.c_str(), 0, true, String(state, 2).c_str());
      published_initial_value = true;
      DEBUG_PRINT(F("MQTT message sent: "));
      DEBUG_PRINT(mqtt_stat_topic.c_str());
      DEBUG_PRINT(F(" -> "));
      DEBUG_PRINTLN(state);
    }
  }

  void publishDiscovery(uint8_t pin_number)
  {
    StaticJsonDocument<700> doc;
    String uid = escapedMac.c_str();
    uid += F("_adc_");
    uid += String(pin_number);
    doc[F("uniq_id")] = uid;
    String name = F("ADC Sensor ");
    name += String(pin_number);
    doc[F("name")] = name.c_str();
    String mqtt_stat_topic = mqttDeviceTopic;
    mqtt_stat_topic += MQTT_TOPIC;
    mqtt_stat_topic += String(pin_number);
    doc[F("stat_t")] = mqtt_stat_topic;
    doc[F("dev_cla")] = device_class[pin_number].c_str();
    doc[F("unit_of_meas")] = unit_of_meas[pin_number].c_str();
    doc[F("val_tpl")] = F("{{ value }}");
    // availablity topic
    String mqtt_avail_topic = mqttDeviceTopic;
    mqtt_avail_topic += F("/status");
    doc[F("avty_t")] = mqtt_avail_topic.c_str();
    doc[F("pl_on")] = F("online");
    doc[F("pl_off")] = F("offline");
    JsonObject device = doc.createNestedObject(F("device")); // attach the sensor to the same device
    device[F("name")] = serverDescription;
    device[F("ids")] = serverDescription;
    device[F("mf")] = F(WLED_BRAND);
    device[F("mdl")] = F(WLED_PRODUCT_NAME);
    device[F("sw")] = versionString;
#ifdef ESP32
    device[F("hw_version")] = F("esp32");
#elif defined(ESP8266)
    device[F("hw_version")] = F("esp8266");
#else
    device[F("hw_version")] = F("unknown");
#endif
    JsonArray connections = device[F("connections")].createNestedArray();
    connections.add(F("mac"));
    connections.add(WiFi.macAddress());
    connections.add(F("ip"));
    connections.add(WiFi.localIP().toString());
    String discovery_topic = F("homeassistant/sensor/");
    discovery_topic += uid;
    discovery_topic += F("/config");
    char json_str[700];
    size_t payload_size = serializeJson(doc, json_str);
    hassDiscoverySent = mqtt->publish(discovery_topic.c_str(), 1, true, json_str, payload_size) > 0; // publish discovery message
  }
};
const char adc_sensor_mqtt::_name[] PROGMEM = "adc_sensor_mqtt";
const char adc_sensor_mqtt::MQTT_TOPIC[] PROGMEM = "/adc_";

static adc_sensor_mqtt adc_sensor_mqtt_instance;
REGISTER_USERMOD(adc_sensor_mqtt_instance);