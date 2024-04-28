#ifndef WLED_ENABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

#pragma once

#include "SHT85.h"

#define USERMOD_SHT_TYPE_SHT30 0
#define USERMOD_SHT_TYPE_SHT31 1
#define USERMOD_SHT_TYPE_SHT35 2
#define USERMOD_SHT_TYPE_SHT85 3

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

// Strings to reduce flash memory usage (used more than twice)
const char ShtUsermod::_name[]            PROGMEM = "SHT-Sensor";
const char ShtUsermod::_enabled[]         PROGMEM = "Enabled";
const char ShtUsermod::_shtType[]         PROGMEM = "SHT-Type";
const char ShtUsermod::_unitOfTemp[]      PROGMEM = "Unit";
const char ShtUsermod::_haMqttDiscovery[] PROGMEM = "Add-To-HA-MQTT-Discovery";

/**
 * Initialise SHT sensor.
 *
 * Using the correct constructor according to config and initialises it using the
 * global i2c pins.
 *
 * @return void
 */
void ShtUsermod::initShtTempHumiditySensor()
{
  switch (shtType) {
    case USERMOD_SHT_TYPE_SHT30: shtTempHumidSensor = (SHT *) new SHT30(); break;
    case USERMOD_SHT_TYPE_SHT31: shtTempHumidSensor = (SHT *) new SHT31(); break;
    case USERMOD_SHT_TYPE_SHT35: shtTempHumidSensor = (SHT *) new SHT35(); break;
    case USERMOD_SHT_TYPE_SHT85: shtTempHumidSensor = (SHT *) new SHT85(); break;
  }

  shtTempHumidSensor->begin(shtI2cAddress); // uses &Wire
  if (shtTempHumidSensor->readStatus() == 0xFFFF) {
    DEBUG_PRINTF("[%s] SHT init failed!\n", _name);
    cleanup();
    return;
  }

  shtInitDone = true;
}

/**
 * Cleanup the SHT sensor.
 *
 * Properly calls "reset" for the sensor then releases it from memory.
 *
 * @return void
 */
void ShtUsermod::cleanupShtTempHumiditySensor()
{
  if (isShtReady()) {
    shtTempHumidSensor->reset();
    delete shtTempHumidSensor;
    shtTempHumidSensor = nullptr;
  }
  shtInitDone = false;
}

/**
 * Cleanup the mod completely.
 *
 * Calls ::cleanupShtTempHumiditySensor() to cleanup the SHT sensor and
 * deallocates pins.
 *
 * @return void
 */
void ShtUsermod::cleanup()
{
  cleanupShtTempHumiditySensor();
  enabled = false;
}

/**
 * Publish temperature and humidity to WLED device topic.
 *
 * Will add a "/temperature" and "/humidity" topic to the WLED device topic.
 * Temperature will be written in configured unit.
 *
 * @return void
 */
void ShtUsermod::publishTemperatureAndHumidityViaMqtt() {
  if (!WLED_MQTT_CONNECTED) return;
  char buf[128];

  snprintf_P(buf, 127, PSTR("%s/temperature"), mqttDeviceTopic);
  mqtt->publish(buf, 0, false, String(getTemperature()).c_str());
  snprintf_P(buf, 127, PSTR("%s/humidity"), mqttDeviceTopic);
  mqtt->publish(buf, 0, false, String(getHumidity()).c_str());
}

/**
 * If enabled, publishes HA MQTT device discovery topics.
 *
 * Will make Home Assistant add temperature and humidity as entities automatically.
 *
 * Note: Whenever usermods are part of the WLED integration in HA, this can be dropped.
 *
 * @return void
 */
void ShtUsermod::publishHomeAssistantAutodiscovery() {
  if (!WLED_MQTT_CONNECTED) return;

  char json_str[1024], buf[128];
  size_t payload_size;
  StaticJsonDocument<1024> json;

  snprintf_P(buf, 127, PSTR("%s Temperature"), serverDescription);
  json[F("name")] = buf;
  snprintf_P(buf, 127, PSTR("%s/temperature"), mqttDeviceTopic);
  json[F("stat_t")] = buf;
  json[F("dev_cla")] = F("temperature");
  json[F("stat_cla")] = F("measurement");
  snprintf_P(buf, 127, PSTR("%s-temperature"), escapedMac.c_str());
  json[F("uniq_id")] = buf;
  json[F("unit_of_meas")] = unitOfTemp ? F("°F") : F("°C");
  appendDeviceToMqttDiscoveryMessage(json);
  payload_size = serializeJson(json, json_str);
  snprintf_P(buf, 127, PSTR("homeassistant/sensor/%s/%s-temperature/config"), escapedMac.c_str(), escapedMac.c_str());
  mqtt->publish(buf, 0, true, json_str, payload_size);

  json.clear();

  snprintf_P(buf, 127, PSTR("%s Humidity"), serverDescription);
  json[F("name")] = buf;
  snprintf_P(buf, 127, PSTR("%s/humidity"), mqttDeviceTopic);
  json[F("stat_t")] = buf;
  json[F("dev_cla")] = F("humidity");
  json[F("stat_cla")] = F("measurement");
  snprintf_P(buf, 127, PSTR("%s-humidity"), escapedMac.c_str());
  json[F("uniq_id")] = buf;
  json[F("unit_of_meas")] = F("%");
  appendDeviceToMqttDiscoveryMessage(json);
  payload_size = serializeJson(json, json_str);
  snprintf_P(buf, 127, PSTR("homeassistant/sensor/%s/%s-humidity/config"), escapedMac.c_str(), escapedMac.c_str());
  mqtt->publish(buf, 0, true, json_str, payload_size);

  haMqttDiscoveryDone = true;
}

/**
 * Helper to add device information to MQTT discovery topic.
 *
 * @return void
 */
void ShtUsermod::appendDeviceToMqttDiscoveryMessage(JsonDocument& root) {
  JsonObject device = root.createNestedObject(F("dev"));
  device[F("ids")] = escapedMac.c_str();
  device[F("name")] = serverDescription;
  device[F("sw")] = versionString;
  device[F("mdl")] = ESP.getChipModel();
  device[F("mf")] = F("espressif");
}

/**
 * Setup the mod.
 *
 * Allocates i2c pins as PinOwner::HW_I2C, so they can be allocated multiple times.
 * And calls ::initShtTempHumiditySensor() to initialise the sensor.
 *
 * @see Usermod::setup()
 * @see UsermodManager::setup()
 *
 * @return void
 */
void ShtUsermod::setup()
{
  if (enabled) {
    // GPIOs can be set to -1 , so check they're gt zero
    if (i2c_sda < 0 || i2c_scl < 0) {
      DEBUG_PRINTF("[%s] I2C bus not initialised!\n", _name);
      cleanup();
      return;
    }

    initShtTempHumiditySensor();

    initDone = true;
  }

  firstRunDone = true;
}

/**
 * Actually reading data (async) from the sensor every 30 seconds.
 *
 * If last reading is at least 30 seconds, it will trigger a reading using
 * SHT::requestData(). We will then continiously check SHT::dataReady() if
 * data is ready to be read. If so, it's read, stored locally and published
 * via MQTT.
 *
 * @see Usermod::loop()
 * @see UsermodManager::loop()
 *
 * @return void
 */
void ShtUsermod::loop()
{
  if (!enabled || !initDone || strip.isUpdating()) return;

  if (isShtReady()) {
    if (millis() - shtLastTimeUpdated > 30000 && !shtDataRequested) {
      shtTempHumidSensor->requestData();
      shtDataRequested = true;

      shtLastTimeUpdated = millis();
    }

    if (shtDataRequested) {
      if (shtTempHumidSensor->dataReady()) {
        if (shtTempHumidSensor->readData(false)) {
          shtCurrentTempC = shtTempHumidSensor->getTemperature();
          shtCurrentHumidity = shtTempHumidSensor->getHumidity();

          publishTemperatureAndHumidityViaMqtt();
          shtReadDataSuccess = true;
        } else {
          shtReadDataSuccess = false;
        }

        shtDataRequested = false;
      }
    }
  }
}

/**
 * Whenever MQTT is connected, publish HA autodiscovery topics.
 *
 * Is only done once.
 *
 * @see Usermod::onMqttConnect()
 * @see UsermodManager::onMqttConnect()
 *
 * @return void
 */
void ShtUsermod::onMqttConnect(bool sessionPresent) {
  if (haMqttDiscovery && !haMqttDiscoveryDone) publishHomeAssistantAutodiscovery();
}

/**
 * Add dropdown for sensor type and unit to UM config page.
 *
 * @see Usermod::appendConfigData()
 * @see UsermodManager::appendConfigData()
 *
 * @return void
 */
void ShtUsermod::appendConfigData() {
  oappend(SET_F("dd=addDropdown('"));
  oappend(_name);
  oappend(SET_F("','"));
  oappend(_shtType);
  oappend(SET_F("');"));
  oappend(SET_F("addOption(dd,'SHT30',0);"));
  oappend(SET_F("addOption(dd,'SHT31',1);"));
  oappend(SET_F("addOption(dd,'SHT35',2);"));
  oappend(SET_F("addOption(dd,'SHT85',3);"));
  oappend(SET_F("dd=addDropdown('"));
  oappend(_name);
  oappend(SET_F("','"));
  oappend(_unitOfTemp);
  oappend(SET_F("');"));
  oappend(SET_F("addOption(dd,'Celsius',0);"));
  oappend(SET_F("addOption(dd,'Fahrenheit',1);"));
}

/**
 * Add config data to be stored in cfg.json.
 *
 * @see Usermod::addToConfig()
 * @see UsermodManager::addToConfig()
 *
 * @return void
 */
void ShtUsermod::addToConfig(JsonObject &root)
{
  JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname

  top[FPSTR(_enabled)] = enabled;
  top[FPSTR(_shtType)] = shtType;
  top[FPSTR(_unitOfTemp)] = unitOfTemp;
  top[FPSTR(_haMqttDiscovery)] = haMqttDiscovery;
}

/**
 * Apply config on boot or save of UM config page.
 *
 * This is called whenever WLED boots and loads cfg.json, or when the UM config
 * page is saved. Will properly re-instantiate the SHT class upon type change and
 * publish HA discovery after enabling.
 *
 * @see Usermod::readFromConfig()
 * @see UsermodManager::readFromConfig()
 *
 * @return bool
 */
bool ShtUsermod::readFromConfig(JsonObject &root)
{
  JsonObject top = root[FPSTR(_name)];
  if (top.isNull()) {
    DEBUG_PRINTF("[%s] No config found. (Using defaults.)\n", _name);
    return false;
  }

  bool oldEnabled = enabled;
  byte oldShtType = shtType;
  byte oldUnitOfTemp = unitOfTemp;
  bool oldHaMqttDiscovery = haMqttDiscovery;

  getJsonValue(top[FPSTR(_enabled)], enabled);
  getJsonValue(top[FPSTR(_shtType)], shtType);
  getJsonValue(top[FPSTR(_unitOfTemp)], unitOfTemp);
  getJsonValue(top[FPSTR(_haMqttDiscovery)], haMqttDiscovery);

  // First run: reading from cfg.json, nothing to do here, will be all done in setup()
  if (!firstRunDone) {
    DEBUG_PRINTF("[%s] First run, nothing to do\n", _name);
  }
  // Check if mod has been en-/disabled
  else if (enabled != oldEnabled) {
    enabled ? setup() : cleanup();
    DEBUG_PRINTF("[%s] Usermod has been en-/disabled\n", _name);
  }
  // Config has been changed, so adopt to changes
  else if (enabled) {
    if (oldShtType != shtType) {
      cleanupShtTempHumiditySensor();
      initShtTempHumiditySensor();
    }

    if (oldUnitOfTemp != unitOfTemp) {
      publishTemperatureAndHumidityViaMqtt();
      publishHomeAssistantAutodiscovery();
    }

    if (oldHaMqttDiscovery != haMqttDiscovery && haMqttDiscovery) {
      publishHomeAssistantAutodiscovery();
    }

    DEBUG_PRINTF("[%s] Config (re)loaded\n", _name);
  }

  return true;
}

/**
 * Adds the temperature and humidity actually to the info section and /json info.
 *
 * This is called every time the info section is opened ot /json is called.
 *
 * @see Usermod::addToJsonInfo()
 * @see UsermodManager::addToJsonInfo()
 *
 * @return void
 */
void ShtUsermod::addToJsonInfo(JsonObject& root)
{
  if (!enabled && !isShtReady()) {
    return;
  }

  JsonObject user = root["u"];
  if (user.isNull()) user = root.createNestedObject("u");

  JsonArray jsonTemp = user.createNestedArray(F("Temperature"));
  JsonArray jsonHumidity = user.createNestedArray(F("Humidity"));

  if (shtLastTimeUpdated == 0 || !shtReadDataSuccess) {
    jsonTemp.add(0);
    jsonHumidity.add(0);
    if (shtLastTimeUpdated == 0) {
      jsonTemp.add(F(" Not read yet"));
      jsonHumidity.add(F(" Not read yet"));
    } else {
      jsonTemp.add(F(" Error"));
      jsonHumidity.add(F(" Error"));
    }
    return;
  }

  jsonHumidity.add(getHumidity());
  jsonHumidity.add(F(" RH"));

  jsonTemp.add(getTemperature());
  jsonTemp.add(getUnitString());

  // sensor object
  JsonObject sensor = root[F("sensor")];
  if (sensor.isNull()) sensor = root.createNestedObject(F("sensor"));

  jsonTemp = sensor.createNestedArray(F("temp"));
  jsonTemp.add(getTemperature());
  jsonTemp.add(getUnitString());

  jsonHumidity = sensor.createNestedArray(F("humidity"));
  jsonHumidity.add(getHumidity());
  jsonHumidity.add(F(" RH"));
}

/**
 * Getter for last read temperature for configured unit.
 *
 * @return float
 */
float ShtUsermod::getTemperature() {
  return unitOfTemp ? getTemperatureF() : getTemperatureC();
}

/**
 * Returns the current configured unit as human readable string.
 *
 * @return const char*
 */
const char* ShtUsermod::getUnitString() {
  return unitOfTemp ? "°F" : "°C";
}