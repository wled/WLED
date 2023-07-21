#pragma once

#include "wled.h"
#include "OneWire.h"

//Pin defaults for QuinLed Dig-Uno if not overriden
#ifndef TEMPERATURE_PIN
  #ifdef ARDUINO_ARCH_ESP32
    #define TEMPERATURE_PIN 18
  #else //ESP8266 boards
    #define TEMPERATURE_PIN 14
  #endif
#endif

// the frequency to check temperature, 1 minute
#ifndef USERMOD_DALLASTEMPERATURE_MEASUREMENT_INTERVAL
#define USERMOD_DALLASTEMPERATURE_MEASUREMENT_INTERVAL 60000
#endif

class UsermodTemperature : public Usermod {

  private:

    bool initDone = false;
    OneWire *oneWire;
    // GPIO pin used for sensor (with a default compile-time fallback)
    int8_t temperaturePin = TEMPERATURE_PIN;
    // measurement unit (true==°C, false==°F)
    bool degC = true;
    // using parasite power on the sensor
    bool parasite = false;
    int8_t parasitePin = -1;
    // how often do we read from sensor?
    unsigned long readingInterval = USERMOD_DALLASTEMPERATURE_MEASUREMENT_INTERVAL;
    // set last reading as "40 sec before boot", so first reading is taken after 20 sec
    unsigned long lastMeasurement = UINT32_MAX - USERMOD_DALLASTEMPERATURE_MEASUREMENT_INTERVAL;
    // last time requestTemperatures was called
    // used to determine when we can read the sensors temperature
    // we have to wait at least 93.75 ms after requestTemperatures() is called
    unsigned long lastTemperaturesRequest;
    float temperature;
    // indicates requestTemperatures has been called but the sensor measurement is not complete
    bool waitingForConversion = false;
    // flag set at startup if DS18B20 sensor not found, avoids trying to keep getting
    // temperature if flashed to a board without a sensor attached
    byte sensorFound;

    bool enabled = true;

    bool HApublished = false;

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _readInterval[];
    static const char _parasite[];
    static const char _parasitePin[];

    //Dallas sensor quick (& dirty) reading. Credit to - Author: Peter Scargill, August 17th, 2013
    float readDallas();
    void requestTemperatures();
    void readTemperature();
    bool findSensor();
#ifndef WLED_DISABLE_MQTT
    void publishHomeAssistantAutodiscovery();
#endif

  public:

    /*
     * API calls te enable data exchange between WLED modules
     */
    inline float getTemperatureC() { return temperature; }
    inline float getTemperatureF() { return temperature * 1.8f + 32.0f; }
    float getTemperature();
    const char *getTemperatureUnit();
    uint16_t getId() { return USERMOD_ID_TEMPERATURE; }

    void setup();
    void loop();
    //void connected();
#ifndef WLED_DISABLE_MQTT
    void onMqttConnect(bool sessionPresent);
#endif
    //void onUpdateBegin(bool init);

    //bool handleButton(uint8_t b);
    //void handleOverlayDraw();

    void addToJsonInfo(JsonObject& root);
    //void addToJsonState(JsonObject &root);
    //void readFromJsonState(JsonObject &root);
    void addToConfig(JsonObject &root);
    bool readFromConfig(JsonObject &root);

    void appendConfigData();
};

//Dallas sensor quick (& dirty) reading. Credit to - Author: Peter Scargill, August 17th, 2013
float UsermodTemperature::readDallas() {
  byte data[9];
  int16_t result;                         // raw data from sensor
  float retVal = -127.0f;
  if (oneWire->reset()) {                 // if reset() fails there are no OneWire devices
    oneWire->skip();                      // skip ROM
    oneWire->write(0xBE);                 // read (temperature) from EEPROM
    oneWire->read_bytes(data, 9);         // first 2 bytes contain temperature
    #ifdef WLED_DEBUG
    if (OneWire::crc8(data,8) != data[8]) {
      DEBUG_PRINTLN(F("CRC error reading temperature."));
      for (byte i=0; i < 9; i++) DEBUG_PRINTF("0x%02X ", data[i]);
      DEBUG_PRINT(F(" => "));
      DEBUG_PRINTF("0x%02X\n", OneWire::crc8(data,8));
    }
    #endif
    switch(sensorFound) {
      case 0x10:  // DS18S20 has 9-bit precision
        result = (data[1] << 8) | data[0];
        retVal = float(result) * 0.5f;
        break;
      case 0x22:  // DS18B20
      case 0x28:  // DS1822
      case 0x3B:  // DS1825
      case 0x42:  // DS28EA00
        result = (data[1]<<4) | (data[0]>>4);   // we only need whole part, we will add fraction when returning
        if (data[1] & 0x80) result |= 0xF000;   // fix negative value
        retVal = float(result) + ((data[0] & 0x08) ? 0.5f : 0.0f);
        break;
    }
  }
  for (byte i=1; i<9; i++) data[0] &= data[i];
  return data[0]==0xFF ? -127.0f : retVal;
}

void UsermodTemperature::requestTemperatures() {
  DEBUG_PRINTLN(F("Requesting temperature."));
  oneWire->reset();
  oneWire->skip();                        // skip ROM
  oneWire->write(0x44,parasite);          // request new temperature reading
  if (parasite && parasitePin >=0 ) digitalWrite(parasitePin, HIGH); // has to happen within 10us (open MOSFET)
  lastTemperaturesRequest = millis();
  waitingForConversion = true;
}

void UsermodTemperature::readTemperature() {
  if (parasite && parasitePin >=0 ) digitalWrite(parasitePin, LOW); // deactivate power (close MOSFET)
  temperature = readDallas();
  lastMeasurement = millis();
  waitingForConversion = false;
  //DEBUG_PRINTF("Read temperature %2.1f.\n", temperature); // does not work properly on 8266
  DEBUG_PRINT(F("Read temperature "));
  DEBUG_PRINTLN(temperature);
}

bool UsermodTemperature::findSensor() {
  DEBUG_PRINTLN(F("Searching for sensor..."));
  uint8_t deviceAddress[8] = {0,0,0,0,0,0,0,0};
  // find out if we have DS18xxx sensor attached
  oneWire->reset_search();
  delay(10);
  while (oneWire->search(deviceAddress)) {
    DEBUG_PRINTLN(F("Found something..."));
    if (oneWire->crc8(deviceAddress, 7) == deviceAddress[7]) {
      switch (deviceAddress[0]) {
        case 0x10:  // DS18S20
        case 0x22:  // DS18B20
        case 0x28:  // DS1822
        case 0x3B:  // DS1825
        case 0x42:  // DS28EA00
          DEBUG_PRINTLN(F("Sensor found."));
          sensorFound = deviceAddress[0];
          DEBUG_PRINTF("0x%02X\n", sensorFound);
          return true;
      }
    }
  }
  DEBUG_PRINTLN(F("Sensor NOT found."));
  return false;
}

#ifndef WLED_DISABLE_MQTT
void UsermodTemperature::publishHomeAssistantAutodiscovery() {
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
  json[F("unit_of_measurement")] = F("°C");
  payload_size = serializeJson(json, json_str);

  sprintf_P(buf, PSTR("homeassistant/sensor/%s/config"), escapedMac.c_str());
  mqtt->publish(buf, 0, true, json_str, payload_size);
  HApublished = true;
}
#endif

void UsermodTemperature::setup() {
  int retries = 10;
  sensorFound = 0;
  temperature = -127.0f; // default to -127, DS18B20 only goes down to -50C
  if (enabled) {
    // config says we are enabled
    DEBUG_PRINTLN(F("Allocating temperature pin..."));
    // pin retrieved from cfg.json (readFromConfig()) prior to running setup()
    if (temperaturePin >= 0 && pinManager.allocatePin(temperaturePin, true, PinOwner::UM_Temperature)) {
      oneWire = new OneWire(temperaturePin);
      if (oneWire->reset()) {
        while (!findSensor() && retries--) {
          delay(25); // try to find sensor
        }
      }
      if (parasite && pinManager.allocatePin(parasitePin, true, PinOwner::UM_Temperature)) {
        pinMode(parasitePin, OUTPUT);
        digitalWrite(parasitePin, LOW); // deactivate power (close MOSFET)
      } else {
        parasitePin = -1;
      }
    } else {
      if (temperaturePin >= 0) {
        DEBUG_PRINTLN(F("Temperature pin allocation failed."));
      }
      temperaturePin = -1;  // allocation failed
    }
  }
  lastMeasurement = millis() - readingInterval + 10000;
  initDone = true;
}

void UsermodTemperature::loop() {
  if (!enabled || !sensorFound || strip.isUpdating()) return;

  static uint8_t errorCount = 0;
  unsigned long now = millis();

  // check to see if we are due for taking a measurement
  // lastMeasurement will not be updated until the conversion
  // is complete the the reading is finished
  if (now - lastMeasurement < readingInterval) return;

  // we are due for a measurement, if we are not already waiting
  // for a conversion to complete, then make a new request for temps
  if (!waitingForConversion) {
    requestTemperatures();
    return;
  }

  // we were waiting for a conversion to complete, have we waited log enough?
  if (now - lastTemperaturesRequest >= 750 /* 93.75ms per the datasheet but can be up to 750ms */) {
    readTemperature();
    if (getTemperatureC() < -100.0f) {
      if (++errorCount > 10) sensorFound = 0;
      lastMeasurement = now - readingInterval + 300; // force new measurement in 300ms
      return;
    }
    errorCount = 0;

#ifndef WLED_DISABLE_MQTT
    if (WLED_MQTT_CONNECTED) {
      char subuf[64];
      strcpy(subuf, mqttDeviceTopic);
      if (temperature > -100.0f) {
        // dont publish super low temperature as the graph will get messed up
        // the DallasTemperature library returns -127C or -196.6F when problem
        // reading the sensor
        strcat_P(subuf, PSTR("/temperature"));
        mqtt->publish(subuf, 0, false, String(getTemperatureC()).c_str());
        strcat_P(subuf, PSTR("_f"));
        mqtt->publish(subuf, 0, false, String(getTemperatureF()).c_str());
      } else {
        // publish something else to indicate status?
      }
    }
#endif
  }
}

/**
 * connected() is called every time the WiFi is (re)connected
 * Use it to initialize network interfaces
 */
//void UsermodTemperature::connected() {}

#ifndef WLED_DISABLE_MQTT
/**
 * subscribe to MQTT topic if needed
 */
void UsermodTemperature::onMqttConnect(bool sessionPresent) {
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
void UsermodTemperature::addToJsonInfo(JsonObject& root) {
  // dont add temperature to info if we are disabled
  if (!enabled) return;

  JsonObject user = root["u"];
  if (user.isNull()) user = root.createNestedObject("u");

  JsonArray temp = user.createNestedArray(FPSTR(_name));

  if (temperature <= -100.0f) {
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
//void UsermodTemperature::addToJsonState(JsonObject &root)
//{
//}

/**
 * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
 * Values in the state object may be modified by connected clients
 * Read "<usermodname>_<usermodparam>" from json state and and change settings (i.e. GPIO pin) used.
 */
//void UsermodTemperature::readFromJsonState(JsonObject &root) {
//  if (!initDone) return;  // prevent crash on boot applyPreset()
//}

/**
 * addToConfig() (called from set.cpp) stores persistent properties to cfg.json
 */
void UsermodTemperature::addToConfig(JsonObject &root) {
  // we add JSON object: {"Temperature": {"pin": 0, "degC": true}}
  JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
  top[FPSTR(_enabled)] = enabled;
  top["pin"]  = temperaturePin;     // usermodparam
  top["degC"] = degC;  // usermodparam
  top[FPSTR(_readInterval)] = readingInterval / 1000;
  top[FPSTR(_parasite)] = parasite;
  top[FPSTR(_parasitePin)] = parasitePin;
  DEBUG_PRINTLN(F("Temperature config saved."));
}

/**
 * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
 *
 * The function should return true if configuration was successfully loaded or false if there was no configuration.
 */
bool UsermodTemperature::readFromConfig(JsonObject &root) {
  // we look for JSON object: {"Temperature": {"pin": 0, "degC": true}}
  int8_t newTemperaturePin = temperaturePin;
  DEBUG_PRINT(FPSTR(_name));

  JsonObject top = root[FPSTR(_name)];
  if (top.isNull()) {
    DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
    return false;
  }

  enabled           = top[FPSTR(_enabled)] | enabled;
  newTemperaturePin = top["pin"] | newTemperaturePin;
  degC              = top["degC"] | degC;
  readingInterval   = top[FPSTR(_readInterval)] | readingInterval/1000;
  readingInterval   = min(120,max(10,(int)readingInterval)) * 1000;  // convert to ms
  parasite          = top[FPSTR(_parasite)] | parasite;
  parasitePin       = top[FPSTR(_parasitePin)] | parasitePin;

  if (!initDone) {
    // first run: reading from cfg.json
    temperaturePin = newTemperaturePin;
    DEBUG_PRINTLN(F(" config loaded."));
  } else {
    DEBUG_PRINTLN(F(" config (re)loaded."));
    // changing paramters from settings page
    if (newTemperaturePin != temperaturePin) {
      DEBUG_PRINTLN(F("Re-init temperature."));
      // deallocate pin and release memory
      delete oneWire;
      pinManager.deallocatePin(temperaturePin, PinOwner::UM_Temperature);
      temperaturePin = newTemperaturePin;
      pinManager.deallocatePin(parasitePin, PinOwner::UM_Temperature);
      // initialise
      setup();
    }
  }
  // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
  return !top[FPSTR(_parasitePin)].isNull();
}

void UsermodTemperature::appendConfigData() {
  oappend(SET_F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F(":")); oappend(String(FPSTR(_parasite)).c_str());
  oappend(SET_F("',1,'<i>(if no Vcc connected)</i>');"));  // 0 is field type, 1 is actual field
  oappend(SET_F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(SET_F(":")); oappend(String(FPSTR(_parasitePin)).c_str());
  oappend(SET_F("',1,'<i>(for external MOSFET)</i>');"));  // 0 is field type, 1 is actual field
}

float UsermodTemperature::getTemperature() {
  return degC ? getTemperatureC() : getTemperatureF();
}

const char *UsermodTemperature::getTemperatureUnit() {
  return degC ? "°C" : "°F";
}

// strings to reduce flash memory usage (used more than twice)
const char UsermodTemperature::_name[]         PROGMEM = "Temperature";
const char UsermodTemperature::_enabled[]      PROGMEM = "enabled";
const char UsermodTemperature::_readInterval[] PROGMEM = "read-interval-s";
const char UsermodTemperature::_parasite[]     PROGMEM = "parasite-pwr";
const char UsermodTemperature::_parasitePin[]  PROGMEM = "parasite-pwr-pin";
