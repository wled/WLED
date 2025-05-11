#include "LM75.h"

static Generic_LM75 LM75temperature(USERMOD_LM75TEMPERATURE_I2C_ADDRESS);

void  UsermodLM75Temperature::overtempFailure() {
    overtempTriggered = true;
    if(bri >0) {
      toggleOnOff();
      stateUpdated(CALL_MODE_BUTTON);
    }
  }
  
  void  UsermodLM75Temperature::overtempReset() {
    overtempTriggered = false;
    if(bri == 0) {
      toggleOnOff();
      stateUpdated(CALL_MODE_BUTTON);
    }
  }
  
  /**
   * Get auto off Temperature at which WLED Output is swiched off
   */
  int8_t  UsermodLM75Temperature::getAutoOffHighThreshold() {
    return autoOffHighThreshold;
  }
  
  /**
   * Get auto off Temperature at which WLED Output is swiched on again
   */
  int8_t  UsermodLM75Temperature::getAutoOffLowThreshold() {
    return autoOffLowThreshold;
  }
  
  /**
   * Set auto off Temperature at which WLED Output is swiched off 
   */
  void  UsermodLM75Temperature::setAutoOffHighThreshold(uint8_t threshold) {
    autoOffHighThreshold = min((uint8_t)212, max((uint8_t)1, threshold));
  }
  
  /**
   * Set auto off Temperature at which WLED Output is swiched on again
   */
  void  UsermodLM75Temperature::setAutoOffLowThreshold(uint8_t threshold) {
    autoOffLowThreshold = min((uint8_t)211, max((uint8_t)0, threshold));
    // when low power indicator is enabled the auto-off threshold cannot be above indicator threshold
    autoOffLowThreshold  = autoOffEnabled ? min(autoOffHighThreshold-1, (int)autoOffLowThreshold) : autoOffLowThreshold;
  }

  bool UsermodLM75Temperature::findSensor() {
    uint8_t devicepresent;
    //Let's try to communicate with the LM75 sensor
    Wire.beginTransmission(USERMOD_LM75TEMPERATURE_I2C_ADDRESS);
    // End Transmission will return 0 is device has acknowledged communication 
    devicepresent = Wire.endTransmission();
    if(devicepresent == 0) {
        DEBUG_PRINTLN(F("Sensor found."));
        sensorFound = 1;
        return true;
    } else {
        DEBUG_PRINTLN(F("Sensor NOT found."));
        return false;
    }
  }

void UsermodLM75Temperature::readTemperature() {
    if(degC) {
      temperature = LM75temperature.readTemperatureC();
    } else {
      temperature = LM75temperature.readTemperatureF();
    }
    lastMeasurement = millis();
    DEBUG_PRINT(F("Read temperature "));
    DEBUG_PRINTLN(temperature);
  }
  
  
  #ifndef WLED_DISABLE_MQTT
  void UsermodLM75Temperature::publishHomeAssistantAutodiscovery() {
    if (!WLED_MQTT_CONNECTED) return;
  
    char json_str[1024], buf[128];
    size_t payload_size;
    StaticJsonDocument<1024> json;
  
    sprintf_P(buf, PSTR("%s Temperature"), serverDescription);
    json[F("name")] = buf;
    strcpy(buf, mqttDeviceTopic);
    strcat_P(buf, PSTR("/temperature"));
    json[F("state_topic")] = buf;
    json[F("device_class")] = F("temperature");
    json[F("unique_id")] = escapedMac.c_str();
    json[F("unit_of_measurement")] = F(getTemperatureUnit());
    payload_size = serializeJson(json, json_str);
  
    sprintf_P(buf, PSTR("homeassistant/sensor/%s/config"), escapedMac.c_str());
    mqtt->publish(buf, 0, true, json_str, payload_size);
    HApublished = true;
  }
  #endif
  
  void UsermodLM75Temperature::setup() {
    int retries = 5;
    sensorFound = 0;
    if (enabled) {
      // config says we are enabled
      DEBUG_PRINTLN(F("Searching LM75 IC..."));
      while (!findSensor() && retries--) {
          delay(25); // try to find sensor
      }
      if (sensorFound && !initDone) DEBUG_PRINTLN(F("Init not completed"));
    }
    lastMeasurement = millis() - readingInterval + 10000;
    initDone = true;
  }
  
  void UsermodLM75Temperature::loop() {
    if (!enabled || !sensorFound || strip.isUpdating()) return;
  
    unsigned long now = millis();
  
    // check to see if we are due for taking a measurement
    // lastMeasurement will not be updated until the conversion
    // is complete the the reading is finished
    if (now - lastMeasurement < readingInterval) return;
  
    readTemperature();
  
    if(autoOffEnabled) {
      if (!overtempTriggered && temperature >= autoOffHighThreshold) {
        overtempFailure();
      } else if(overtempTriggered && temperature <= autoOffLowThreshold) {
        overtempReset();
      }
    }

    #ifndef WLED_DISABLE_MQTT
        if (WLED_MQTT_CONNECTED) {
            char subuf[64];
            strcpy(subuf, mqttDeviceTopic);
            strcat_P(subuf, PSTR("/temperature"));
            mqtt->publish(subuf, 0, false, String(temperature).c_str());
        }
    #endif
  }
  
  
  #ifndef WLED_DISABLE_MQTT
  /**
   * subscribe to MQTT topic if needed
   */
  void UsermodLM75Temperature::onMqttConnect(bool sessionPresent) {
    //(re)subscribe to required topics
    //char subuf[64];
    if (mqttDeviceTopic[0] != 0) {
      publishHomeAssistantAutodiscovery();
    }
  }
  #endif
  
  /*
    * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
    * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
    * Below it is shown how this could be used for e.g. a light sensor
    */
  void UsermodLM75Temperature::addToJsonInfo(JsonObject& root) {
    // dont add temperature to info if we are disabled
    if (!enabled) return;
  
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
  
    JsonArray temp = user.createNestedArray(FPSTR(_name));
  
    if (!sensorFound) {
      temp.add(0);
      temp.add(F(" Sensor Error!"));
      return;
    }
  
    temp.add(getTemperature());
    temp.add(getTemperatureUnit());
  
    JsonObject sensor = root[F("sensor")];
    if (sensor.isNull()) sensor = root.createNestedObject(F("sensor"));
    temp = sensor.createNestedArray(F("temperature"));
    temp.add(getTemperature());
    temp.add(getTemperatureUnit());
  }
  
  /**
   * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  //void UsermodLM75Temperature::addToJsonState(JsonObject &root)
  //{
  //}
  
  /**
   * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   * Read "<usermodname>_<usermodparam>" from json state and and change settings (i.e. GPIO pin) used.
   */
  //void UsermodLM75Temperature::readFromJsonState(JsonObject &root) {
  //  if (!initDone) return;  // prevent crash on boot applyPreset()
  //}
  
  /**
   * addToConfig() (called from set.cpp) stores persistent properties to cfg.json
   */
  void UsermodLM75Temperature::addToConfig(JsonObject &root) {
    // we add JSON object: {"Temperature":}
    JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
    top[FPSTR(_enabled)] = enabled;
    top["degC"] = degC;  // usermodparam
    top[FPSTR(_readInterval)] = readingInterval / 1000;
    
    JsonObject ao = top.createNestedObject(FPSTR(_ao));  // auto off section
    ao[FPSTR(_enabled)] = autoOffEnabled;
    ao[FPSTR(_thresholdhigh)] = autoOffHighThreshold;
    ao[FPSTR(_thresholdlow)] = autoOffLowThreshold;
    DEBUG_PRINTLN(F("Temperature config saved."));
  }
  
  /**
   * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
   *
   * The function should return true if configuration was successfully loaded or false if there was no configuration.
   */
  bool UsermodLM75Temperature::readFromConfig(JsonObject &root) {
    // we look for JSON object: {"Temperature":}
    DEBUG_PRINT(FPSTR(_name));
  
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) {
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }
  
    enabled           = top[FPSTR(_enabled)] | enabled;
    degC              = top["degC"] | degC;
    readingInterval   = top[FPSTR(_readInterval)] | readingInterval/1000;
    readingInterval   = min(120,max(10,(int)readingInterval)) * 1000;  // convert to ms
  
    JsonObject ao = top[FPSTR(_ao)];
    autoOffEnabled        = ao[FPSTR(_enabled)] | autoOffEnabled;
    setAutoOffHighThreshold (ao[FPSTR(_thresholdhigh)] | autoOffHighThreshold);
    setAutoOffLowThreshold (ao[FPSTR(_thresholdlow)] | autoOffLowThreshold);
  
    if (!initDone) {
      // first run: reading from cfg.json
      DEBUG_PRINTLN(F(" config loaded."));
    } 
    else {
      // changing parameters from settings page
      setup();
      DEBUG_PRINTLN(F(" config (re)loaded."));
    }
    // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
    return !top[FPSTR(_name)].isNull();
  }
  
  void UsermodLM75Temperature::appendConfigData() {
    if(degC) {
      oappend(SET_F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F(":")); oappend(String(FPSTR(_ao)).c_str()); oappend(SET_F(":")); oappend(String(FPSTR(_thresholdhigh)).c_str());
      oappend(SET_F("',1,'°C');"));  // 0 is field type, 1 is actual field
      oappend(SET_F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F(":")); oappend(String(FPSTR(_ao)).c_str()); oappend(SET_F(":")); oappend(String(FPSTR(_thresholdlow)).c_str());
      oappend(SET_F("',1,'°C');"));  // 0 is field type, 1 is actual field
    } else {
      oappend(SET_F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F(":")); oappend(String(FPSTR(_ao)).c_str()); oappend(SET_F(":")); oappend(String(FPSTR(_thresholdhigh)).c_str());
      oappend(SET_F("',1,'°F');"));  // 0 is field type, 1 is actual field
      oappend(SET_F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F(":")); oappend(String(FPSTR(_ao)).c_str()); oappend(SET_F(":")); oappend(String(FPSTR(_thresholdlow)).c_str());
      oappend(SET_F("',1,'°F');"));  // 0 is field type, 1 is actual field 
    }
    
  }
  

  /**
+ * Gets the current temperature in the configured unit (C or F)
+ * @return Temperature in the unit specified by degC setting
+ */
  float UsermodLM75Temperature::getTemperature() {
    return temperature;
  }
  
  const char *UsermodLM75Temperature::getTemperatureUnit() {
    return degC ? "°C" : "°F";
  }
  
  // strings to reduce flash memory usage (used more than twice)
  const char UsermodLM75Temperature::_name[]         PROGMEM = "Temperature";
  const char UsermodLM75Temperature::_enabled[]      PROGMEM = "enabled";
  const char UsermodLM75Temperature::_readInterval[] PROGMEM = "read-interval-s";
  const char UsermodLM75Temperature::_ao[]           PROGMEM = "Overtemperature-Protection";
  const char UsermodLM75Temperature::_thresholdhigh[] PROGMEM = "shutdown-temperature";
  const char UsermodLM75Temperature::_thresholdlow[] PROGMEM = "reactivate-temperature";

static UsermodLM75Temperature temperature;
REGISTER_USERMOD(temperature);
