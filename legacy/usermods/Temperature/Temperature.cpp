#include "UsermodTemperature.h"

static uint16_t mode_temperature();

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
      for (unsigned i=0; i < 9; i++) DEBUG_PRINTF_P(PSTR("0x%02X "), data[i]);
      DEBUG_PRINT(F(" => "));
      DEBUG_PRINTF_P(PSTR("0x%02X\n"), OneWire::crc8(data,8));
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
  for (unsigned i=1; i<9; i++) data[0] &= data[i];
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
  //DEBUG_PRINTF_P(PSTR("Read temperature %2.1f.\n"), temperature); // does not work properly on 8266
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
          DEBUG_PRINTF_P(PSTR("0x%02X\n"), sensorFound);
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
  strcat_P(buf, _Temperature);
  json[F("state_topic")] = buf;
  json[F("device_class")] = FPSTR(_temperature);
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
    if (temperaturePin >= 0 && PinManager::allocatePin(temperaturePin, true, PinOwner::UM_Temperature)) {
      oneWire = new OneWire(temperaturePin);
      if (oneWire->reset()) {
        while (!findSensor() && retries--) {
          delay(25); // try to find sensor
        }
      }
      if (parasite && PinManager::allocatePin(parasitePin, true, PinOwner::UM_Temperature)) {
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
    if (sensorFound && !initDone) strip.addEffect(255, &mode_temperature, _data_fx);
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
      char subuf[128];
      strcpy(subuf, mqttDeviceTopic);
      if (temperature > -100.0f) {
        // dont publish super low temperature as the graph will get messed up
        // the DallasTemperature library returns -127C or -196.6F when problem
        // reading the sensor
        strcat_P(subuf, _Temperature);
        mqtt->publish(subuf, 0, false, String(getTemperatureC()).c_str());
        strcat_P(subuf, PSTR("_f"));
        mqtt->publish(subuf, 0, false, String(getTemperatureF()).c_str());
        if (idx > 0) {
          StaticJsonDocument <128> msg;
          msg[F("idx")]    = idx;
          msg[F("RSSI")]   = WiFi.RSSI();
          msg[F("nvalue")] = 0;
          msg[F("svalue")] = String(getTemperatureC());
          serializeJson(msg, subuf, 127);
          mqtt->publish("domoticz/in", 0, false, subuf);
        }
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

  JsonObject sensor = root[FPSTR(_sensor)];
  if (sensor.isNull()) sensor = root.createNestedObject(FPSTR(_sensor));
  temp = sensor.createNestedArray(FPSTR(_temperature));
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
  top[F("degC")] = degC;  // usermodparam
  top[FPSTR(_readInterval)] = readingInterval / 1000;
  top[FPSTR(_parasite)] = parasite;
  top[FPSTR(_parasitePin)] = parasitePin;
  top[FPSTR(_domoticzIDX)] = idx;
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
  degC              = top[F("degC")] | degC;
  readingInterval   = top[FPSTR(_readInterval)] | readingInterval/1000;
  readingInterval   = min(120,max(10,(int)readingInterval)) * 1000;  // convert to ms
  parasite          = top[FPSTR(_parasite)] | parasite;
  parasitePin       = top[FPSTR(_parasitePin)] | parasitePin;
  idx               = top[FPSTR(_domoticzIDX)] | idx;

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
      PinManager::deallocatePin(temperaturePin, PinOwner::UM_Temperature);
      temperaturePin = newTemperaturePin;
      PinManager::deallocatePin(parasitePin, PinOwner::UM_Temperature);
      // initialise
      setup();
    }
  }
  // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
  return !top[FPSTR(_domoticzIDX)].isNull();
}

void UsermodTemperature::appendConfigData() {
  oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_parasite)).c_str());
  oappend(F("',1,'<i>(if no Vcc connected)</i>');"));  // 0 is field type, 1 is actual field
  oappend(F("addInfo('")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":")); oappend(String(FPSTR(_parasitePin)).c_str());
  oappend(F("',1,'<i>(for external MOSFET)</i>');"));  // 0 is field type, 1 is actual field
}

float UsermodTemperature::getTemperature() {
  return degC ? getTemperatureC() : getTemperatureF();
}

const char *UsermodTemperature::getTemperatureUnit() {
  return degC ? "°C" : "°F";
}

UsermodTemperature* UsermodTemperature::_instance = nullptr;

// strings to reduce flash memory usage (used more than twice)
const char UsermodTemperature::_name[]         PROGMEM = "Temperature";
const char UsermodTemperature::_enabled[]      PROGMEM = "enabled";
const char UsermodTemperature::_readInterval[] PROGMEM = "read-interval-s";
const char UsermodTemperature::_parasite[]     PROGMEM = "parasite-pwr";
const char UsermodTemperature::_parasitePin[]  PROGMEM = "parasite-pwr-pin";
const char UsermodTemperature::_domoticzIDX[]  PROGMEM = "domoticz-idx";
const char UsermodTemperature::_sensor[]       PROGMEM = "sensor";
const char UsermodTemperature::_temperature[]  PROGMEM = "temperature";
const char UsermodTemperature::_Temperature[]  PROGMEM = "/temperature";
const char UsermodTemperature::_data_fx[]      PROGMEM = "Temperature@Min,Max;;!;01;pal=54,sx=255,ix=0";

static uint16_t mode_temperature() {
  float low  = roundf(mapf((float)SEGMENT.speed, 0.f, 255.f, -150.f, 150.f));    // default: 15°C, range: -15°C to 15°C
  float high = roundf(mapf((float)SEGMENT.intensity, 0.f, 255.f, 300.f, 600.f));  // default: 30°C, range 30°C to 60°C
  float temp = constrain(UsermodTemperature::getInstance()->getTemperatureC()*10.f, low, high);   // get a little better resolution (*10)
  unsigned i = map(roundf(temp), (unsigned)low, (unsigned)high, 0, 248);
  SEGMENT.fill(SEGMENT.color_from_palette(i, false, false, 255));
  return FRAMETIME;
}


static UsermodTemperature temperature;
REGISTER_USERMOD(temperature);