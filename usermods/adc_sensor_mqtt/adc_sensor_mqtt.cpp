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
#define _valid_adc_pin(__pin) (__pin >= 32U && __pin <= 39U) // only ADC1 available on ESP32 with wifi
#define ADC_SUPPORTED_PINS F("32..39")
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32S2)
#define _valid_adc_pin(__pin) (__pin >= 1 && __pin <= 10) // only ADC1 available on ESP32-S2/ S3 with wifi
#define ADC_SUPPORTED_PINS F("1..10")
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define _valid_adc_pin(__pin) (__pin < 5 && __pin >= 0) // only ADC1 available on ESP32-C3 with wifi
#define ADC_SUPPORTED_PINS F("0..4")
#else
#error "Unknown ESP32 target"
#endif
#endif

class adc_sensor_mqtt : public Usermod
{
private:
  static const uint8_t NUMBER_OF_DEFAULT_SENSOR_CLASSES = 5; // number of default sensor classes
  uint16_t update_interval = 2000;                           // update interval in ms
  float change_threshold = 1.0f;                             // change threshold in mapped / raw value as needed
  bool HomeAssistantDiscovery = true;                        // is HA discovery turned on by default
  bool publishRawValue = false;                              // publish raw value to MQTT instead of mapped value
  bool hassDiscoverySent = false;                            // flag to check if discovery message was sent
  bool adc_enabled = false;                                  // flag to check if ADC is enabled
  bool initDone = false;                                     // flag to check if setup is done
  bool published_initial_value = false;                      // flag to check if initial value was published
  unsigned long lastTime = 0;                                // last time the value was published

  int8_t adc_pin[UM_ADC_MQTT_PIN_MAX_NUMBER];         // ADC pin number (A0 for ESP8266, 32-39 for ESP32 ..etc checked in setup)
  uint16_t adc_value[UM_ADC_MQTT_PIN_MAX_NUMBER];     // raw ADC values
  float adc_mapped_value[UM_ADC_MQTT_PIN_MAX_NUMBER]; // mapped ADC values
  String device_class[UM_ADC_MQTT_PIN_MAX_NUMBER];
  String unit_of_meas[UM_ADC_MQTT_PIN_MAX_NUMBER];
  static const char *device_classes[NUMBER_OF_DEFAULT_SENSOR_CLASSES];
  static const char *device_unit_of_measurement[NUMBER_OF_DEFAULT_SENSOR_CLASSES];
  static const char MQTT_TOPIC[];
  static const char _name[];
  static bool inverted;

  static float read_adc_mapping(uint16_t rawValue);

  static inline float mapFloat(int x, int in_min, int in_max, float out_min, float out_max)
  {
    return ((float)(x - in_min) * (out_max - out_min)) / (float)(in_max - in_min) + out_min;
  }

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
      adc_mapped_value[i] = 0;
      device_class[i] = F("voltage"); // default device class
      unit_of_meas[i] = F("V");       // default unit of measurement
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
        if ((_valid_adc_pin(adc_pin[i])) && PinManager::allocatePin(adc_pin[i], false, PinOwner::UM_ADC_MQTT)) // ESP32 only pins 32-39 are available for ADC
#endif
        {
          pinMode(adc_pin[i], INPUT); // alloc success -> configure pin for input
          valid_pins++;
        }
        else
        {
          DEBUG_PRINTLN(F("adc_sensor_mqtt: Pin allocation failed!"));
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
      if (regular_update || force_update) // force update if the time is up or if the light is not changing to retain mqtt hass activity in case
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
          bool changed = this->publishRawValue ? (abs(adc_value[i] - analogRead(adc_pin[i])) > change_threshold) : (abs(adc_mapped_value[i] - read_adc_mapping(analogRead(adc_pin[i]))) > change_threshold);
          if (changed || force_update || force_first_update)
          {
            adc_value[i] = analogRead(adc_pin[i]);
            adc_mapped_value[i] = read_adc_mapping(adc_value[i]); // read adc value and map it to percentage
            char buf[64];
            sprintf_P(buf, PSTR("adc %d: %d (%d)"), i, static_cast<uint8_t>(adc_mapped_value[i]), adc_value[i]);
            DEBUG_PRINTLN(buf);
            if (WLED_MQTT_CONNECTED)
            {
              if (publishRawValue)
              {
                publishMqtt(i, adc_value[i]);
              }
              else
              {
                publishMqtt(i, adc_mapped_value[i]);
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
    top[F("Enabled")] = adc_enabled;
    top[F("AdcUpdateInterval")] = update_interval;
    top[F("Inverted")] = inverted;
    top[F("ChangeThreshold")] = change_threshold;
    top[F("HASS")] = HomeAssistantDiscovery;
    top[F("Raw")] = publishRawValue;
    for (uint8_t i = 0; i < UM_ADC_MQTT_PIN_MAX_NUMBER; i++)
    {
      // debug
      DEBUG_PRINT(F("configuring adc_sensor_mqtt: Pin "));
      DEBUG_PRINTLN(i);
      char str2[32];
      sprintf_P(str2, PSTR("ADC %d"), i);
      JsonObject ADC_object = top.createNestedObject(str2);
#ifndef ESP8266                                                            // esp8266 always use A0 no use of pin choice
      ADC_object[F("Pin")] = _valid_adc_pin(adc_pin[i]) ? adc_pin[i] : -1; // store pin number in config
#endif
      // debug print new pin
      ADC_object[F("DeviceClass")] = device_class[i]; // store device class in config
      ADC_object[F("UnitOfMeas")] = unit_of_meas[i];  // store unit of measurement in config
    }
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
      char str[10];
      sprintf_P(str, PSTR("ADC %d"), i);
      configComplete &= getJsonValue(top[str][F("DeviceClass")], device_class[i]);
      configComplete &= getJsonValue(top[str][F("UnitOfMeas")], unit_of_meas[i]);

#ifdef ARDUINO_ARCH_ESP32 // esp8266 always use A0 no use of pin choice
      configComplete &= getJsonValue(top[str][F("Pin")], adc_pin[i]);
      const bool valid_adc_pin = _valid_adc_pin(adc_pin[i]);         // only ADC1 available on ESP32 with wifi
      if (!valid_adc_pin || !PinManager::isPinOk(adc_pin[i], false)) // // ESP32 only pins 32-39 are available for ADC
      {
        DEBUG_PRINT(F("adc_sensor_mqtt: Pin "));
        DEBUG_PRINT(i);
        DEBUG_PRINTLN(F(" is not valid!"));
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
    configComplete &= getJsonValue(top[F("Enabled")], adc_enabled);
    configComplete &= getJsonValue(top[F("AdcUpdateInterval")], update_interval);
    configComplete &= getJsonValue(top[F("Inverted")], inverted);
    configComplete &= getJsonValue(top[F("ChangeThreshold")], change_threshold);
    configComplete &= getJsonValue(top[F("HASS")], HomeAssistantDiscovery);
    configComplete &= getJsonValue(top[F("Raw")], publishRawValue);
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
    uiScript.print(F("addInfo('adc_sensor_mqtt:Enabled',1,'<i>ADC enabled</i>');"));
    uiScript.print(F("addInfo('adc_sensor_mqtt:HASS',1,'<i>Home Assistant Sensor Discovery</i>');"));
    uiScript.print(F("addInfo('adc_sensor_mqtt:AdcUpdateInterval',1,'<i>ADC update interval <small>(in ms)</small> </i>');"));
    uiScript.print(F("addInfo('adc_sensor_mqtt:Inverted',1,'<i>ADC mapping voltage inverted</i>');"));
    uiScript.print(F("addInfo('adc_sensor_mqtt:ChangeThreshold',1,'<i>ADC change threshold</i>');"));
    uiScript.print(F("addInfo('adc_sensor_mqtt:Raw',1,'<i>ADC publish raw value</i> <small>(ignores UnitOfMeas)</small>');"));
    uiScript.print(F("ux='adc_sensor_mqtt';"));
    for (uint8_t i = 0; i < UM_ADC_MQTT_PIN_MAX_NUMBER; i++)
    {
      // Use consistent field name
      char str[128];
#ifndef ESP8266 // only A0 is supported on ESP8266
      sprintf_P(str, PSTR("addInfo('adc_sensor_mqtt:ADC %d:Pin',1,'<i>ADC pin <small>(%s)</small><i> ');"), i, ADC_SUPPORTED_PINS);
      uiScript.print(str);
#endif
      sprintf_P(str, PSTR("addInfo('adc_sensor_mqtt:ADC %d:DeviceClass',1,'<i>Sensor Device Class</i>');"), i);
      uiScript.print(str);
      sprintf_P(str, PSTR("addInfo('adc_sensor_mqtt:ADC %d:UnitOfMeas',1,'<i>the unit of measurement</i>');"), i);
      uiScript.print(str);
      sprintf_P(str, PSTR("dd=addDropdown(ux,'ADC %d:DeviceClass');"), i);
      uiScript.print(str);
      for (uint8_t j = 0; j < NUMBER_OF_DEFAULT_SENSOR_CLASSES; j++)
      {
        sprintf_P(str, PSTR("addOption(dd,'%s','%s');"), device_classes[j], device_classes[j]);
        uiScript.print(str);
      }
      sprintf_P(str, PSTR("dd=addDropdown(ux,'ADC %d:UnitOfMeas');"), i);
      uiScript.print(str);
      for (uint8_t j = 0; j < NUMBER_OF_DEFAULT_SENSOR_CLASSES; j++)
      {
        sprintf_P(str, PSTR("addOption(dd,'%s','%s');"), device_unit_of_measurement[j], device_unit_of_measurement[j]);
        uiScript.print(str);
      }
    }
  }

  void addToJsonInfo(JsonObject &root)
  {
    // If "u" object does not exist yet we need to create it
    JsonObject user = root[F("u")];
    if (user.isNull())
      user = root.createNestedObject(F("u"));

    if (!adc_enabled)
    {
      JsonArray adc_Enabled = user.createNestedArray(F("ADC enabled"));
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
        adc_pin_value += F("):");
      }
      if (publishRawValue)
      {
        adc_pin_value += F("raw:");
        adc_pin_value += String(adc_value[i]);
      }
      else
      {
        adc_pin_value += String(adc_mapped_value[i]);
        adc_pin_value += unit_of_meas[i];
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
    if (publishRawValue == false)
    {
      doc[F("unit_of_meas")] = unit_of_meas[pin_number].c_str();
    }
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

// Usermod static const variables
const char adc_sensor_mqtt::_name[] PROGMEM = "adc_sensor_mqtt";
const char adc_sensor_mqtt::MQTT_TOPIC[] PROGMEM = "/adc_";
bool adc_sensor_mqtt::inverted = false;

// customization settings for the usermod
//  default device class and unit of measurement // edit those for your own needs
const char *adc_sensor_mqtt::device_classes[NUMBER_OF_DEFAULT_SENSOR_CLASSES] = {"illuminance", "current", "power", "temperature", "voltage"}; // default device class
const char *adc_sensor_mqtt::device_unit_of_measurement[NUMBER_OF_DEFAULT_SENSOR_CLASSES] = {"lx", "A", "W", "°C", "V"};                       // default unit of measurement

/**
 * @brief Map the raw ADC value to a percentage value or add your own custom mapping.
 *
 * @param rawValue
 * @return float
 */
float adc_sensor_mqtt::read_adc_mapping(uint16_t rawValue)
{
#ifdef ESP32
  const uint16_t mapping = 4096; // ESP32 ADC resolution is 12 bit, so 4096 values
#else
  const uint16_t mapping = 1024; // ESP8266 ADC resolution is 10 bit, so 1024 values
#endif
  if (adc_sensor_mqtt::inverted) // default mapping value to 0-100  -- replace here your own mapping for your own unit of measurement
  {
    return mapFloat(rawValue, 0, mapping, 100, 0); // map raw value to percentage
  }
  else
  {
    return mapFloat(rawValue, 0, mapping, 0, 100); // map raw value to percentage
  }
}

// Register the usermod in the usermod manager
static adc_sensor_mqtt adc_sensor_mqtt_instance;
REGISTER_USERMOD(adc_sensor_mqtt_instance);