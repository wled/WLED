// force the compiler to show a warning to confirm that this file is included WLEDMM: commented this warning as we want serious warnings ;-)
// #warning **** Included USERMOD_BH1750 ****

#ifndef WLED_ENABLE_MQTT
#warning "This user mod expects MQTT to be enabled."
#endif

#pragma once

#include <Arduino.h>   // WLEDMM: make sure that I2C drivers have the "right" Wire Object

#include "wled.h"
#include <BH1750.h>

// the max frequency to check photoresistor, 10 seconds
#ifndef USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL
#define USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL 10000
#endif

// the min frequency to check photoresistor, 500 ms
#ifndef USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL
//#define USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL 500
#define USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL 2500   // WLEDMM this makes more sense
#endif

// how many seconds after boot to take first measurement, 10 seconds
#ifndef USERMOD_BH1750_FIRST_MEASUREMENT_AT
#define USERMOD_BH1750_FIRST_MEASUREMENT_AT 10000
#endif

// only report if difference grater than offset value
#ifndef USERMOD_BH1750_OFFSET_VALUE
#define USERMOD_BH1750_OFFSET_VALUE 2                 // WLEDMM this makes more sense
#endif

class Usermod_BH1750 : public Usermod
{
private:
  int8_t offset = USERMOD_BH1750_OFFSET_VALUE;

  unsigned long maxReadingInterval = USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL;
  unsigned long minReadingInterval = USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL;
  unsigned long lastMeasurement = UINT32_MAX - (USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL - USERMOD_BH1750_FIRST_MEASUREMENT_AT);
  unsigned long lastSend = UINT32_MAX - (USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL - USERMOD_BH1750_FIRST_MEASUREMENT_AT);
  // flag to indicate we have finished the first readLightLevel call
  // allows this library to report to the user how long until the first
  // measurement
  bool getLuminanceComplete = false;

  // flag set at startup
  //bool enabled = true;  //WLEDMM not needed as we use global attributes

  // strings to reduce flash memory usage (used more than twice)
  //static const char _name[];    //WLEDMM not needed as we use global attributes
  //static const char _enabled[]; //WLEDMM not needed as we use global attributes
  static const char _maxReadInterval[];
  static const char _minReadInterval[];
  static const char _offset[];
  static const char _HomeAssistantDiscovery[];

  // set the default pins based on the architecture, these get overridden by Usermod menu settings
  // #ifdef ARDUINO_ARCH_ESP32 // ESP32 boards -- WLEDMM: don't override already defined HW pins
  //   #ifndef HW_PIN_SDA
  //     #define HW_PIN_SCL 22
  //   #endif
  //   #ifndef HW_PIN_SDA
  //     #define HW_PIN_SDA 21
  //   #endif
  // #else // ESP8266 boards
  //   #define HW_PIN_SCL 5
  //   #define HW_PIN_SDA 4
  // #endif
  //int8_t ioPin[2] = {HW_PIN_SCL, HW_PIN_SDA};        // I2C pins: SCL, SDA...defaults to Arch hardware pins but overridden at setup()
  int8_t ioPin[2] = {-1, -1};        // WLEDMM - I2C pins: wait until hw pins get allocated
  bool initDone = false;
  bool sensorFound = false;

  // Home Assistant and MQTT  
  String mqttLuminanceTopic;
  bool mqttInitialized = false;
  bool HomeAssistantDiscovery = true; // Publish Home Assistant Discovery messages

  BH1750 lightMeter;
  float lastLux = -1000;

  bool checkBoundSensor(float newValue, float prevValue, float maxDiff)
  {
    return isnan(prevValue) || newValue <= prevValue - maxDiff || newValue >= prevValue + maxDiff || (newValue == 0.0 && prevValue > 0.0);
  }
  
  // set up Home Assistant discovery entries
  void _mqttInitialize()
  {
#ifdef WLED_ENABLED_MQTT
    mqttLuminanceTopic = String(mqttDeviceTopic) + F("/brightness");

    if (HomeAssistantDiscovery) _createMqttSensor(F("Brightness"), mqttLuminanceTopic, F("Illuminance"), F(" lx"));
#endif
  }

  // Create an MQTT Sensor for Home Assistant Discovery purposes, this includes a pointer to the topic that is published to in the Loop.
  void _createMqttSensor(const String &name, const String &topic, const String &deviceClass, const String &unitOfMeasurement)
  {
#ifdef WLED_ENABLED_MQTT
    String t = String(F("homeassistant/sensor/")) + mqttClientID + F("/") + name + F("/config");
    
    StaticJsonDocument<600> doc;
    
    doc[F("name")] = String(serverDescription) + F(" ") + name;
    doc[F("state_topic")] = topic;
    doc[F("unique_id")] = String(mqttClientID) + name;
    if (unitOfMeasurement != "")
      doc[F("unit_of_measurement")] = unitOfMeasurement;
    if (deviceClass != "")
      doc[F("device_class")] = deviceClass;
    doc[F("expire_after")] = 1800;

    JsonObject device = doc.createNestedObject(F("device")); // attach the sensor to the same device
    device[F("name")] = serverDescription;
    device[F("identifiers")] = "wled-sensor-" + String(mqttClientID);
    device[F("manufacturer")] = F(WLED_BRAND); //WLEDMM + Moustachauve/Wled-Native
    device[F("model")] = F(WLED_PRODUCT_NAME); //WLEDMM + Moustachauve/Wled-Native
    device[F("sw_version")] = versionString;

    String temp;
    serializeJson(doc, temp);
    DEBUG_PRINTLN(t);
    DEBUG_PRINTLN(temp);

    mqtt->publish(t.c_str(), 0, true, temp.c_str());
#endif
  }

public:
  Usermod_BH1750(const char *name, bool enabled):Usermod(name, enabled) {} //WLEDMM

  void setup()
  {
#if 0
    bool HW_Pins_Used = (ioPin[0]==i2c_scl && ioPin[1]==i2c_sda); // note whether architecture-based hardware SCL/SDA pins used
    PinOwner po = PinOwner::UM_BH1750; // defaults to being pinowner for SCL/SDA pins
    if (HW_Pins_Used) po = PinOwner::HW_I2C; // allow multiple allocations of HW I2C bus pins
    if ((i2c_scl >= 0) && (i2c_sda >=0)) { // WLEDMM: make sure that global HW pins are used if defined 
      ioPin[0] = i2c_scl;
      ioPin[1] = i2c_sda;
      po = PinOwner::HW_I2C;
    }
    PinManagerPinType pins[2] = { { ioPin[0], true }, { ioPin[1], true } };  // allocate pins // WLEDMM: after selecting final pins
    if (!pinManager.allocateMultiplePins(pins, 2, po)) return;
    
#if defined(ARDUINO_ARCH_ESP32)
      if (pins[1].pin < 0 || pins[0].pin < 0)  { sensorFound=false; enabled=false; return; }  //WLEDMM bugfix - ensure that "final" GPIO are valid and no "-1" sneaks trough
      //Wire.begin(pins[1].pin, pins[0].pin);  // WLEDMM this might silently fail, which is OK as it just means that I2C bus is already running.
#else
      //Wire.begin();  // WLEDMM - i2c pins on 8266 are fixed.
#endif
#endif
    if (!enabled) return;
    if (!pinManager.joinWire()) {  // WLEDMM - this allocates global I2C pins, then starts Wire - if not started previously
      sensorFound = false;
      //enabled = false;
      USER_PRINTLN(F("[BH1750]: failed to join I2C bus.")); 
      return;
    }

    sensorFound = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);  // WLEDMM  set mode explicitly
    if (sensorFound) { USER_PRINTLN(F("[BH1750] sensor found.")); }
    else{ USER_PRINTLN(F("[BH1750] sensor not found.")); }
    initDone = true;
  }

  void loop()
  {
    if (!sensorFound || !initDone) return;  // WLEDMM bugfix
    if ((!enabled) || (strip.isUpdating() && (millis() - lastMeasurement < 450)))  // WLEDMM be nice, but not too nice
      return;

    unsigned long now = millis();

    // check to see if we are due for taking a measurement
    // lastMeasurement will not be updated until the conversion
    // is complete the the reading is finished
    if (now - lastMeasurement < minReadingInterval)
    {
      return;
    }

    bool shouldUpdate = now - lastSend > maxReadingInterval;
    float lux =  lastLux;
    if (lightMeter.measurementReady()) {  //WLEDMM do not block in case the sensor is still busy
      lux = lightMeter.readLightLevel();
      lastMeasurement = millis();
      getLuminanceComplete = true;
    }

    if (shouldUpdate || checkBoundSensor(lux, lastLux, offset))
    {
      lastLux = lux;
      lastSend = millis();
#ifndef WLED_DISABLE_MQTT
      if (WLED_MQTT_CONNECTED)
      {
        if (!mqttInitialized)
          {
            _mqttInitialize();
            mqttInitialized = true;
          }
        mqtt->publish(mqttLuminanceTopic.c_str(), 0, true, String(lux).c_str());
        DEBUG_PRINTLN(String("Brightness: ") + String(lux) + String("lx"));  // WLEDMM fix compilation warning
      }
      else
      {
        DEBUG_PRINTLN(F("Missing MQTT connection. Not publishing data"));
      }
#endif
    }
  }

  inline float getIlluminance() {
    return (float)lastLux;
  }

  void addToJsonInfo(JsonObject &root)
  {
    if ((!enabled) || (!sensorFound)) return;  // WLEDMM bugfix

    JsonObject user = root[F("u")];
    if (user.isNull())
      user = root.createNestedObject(F("u"));

    JsonArray lux_json = user.createNestedArray(F("Luminance"));
    if (!sensorFound) {
        // if no sensor 
        lux_json.add(F("BH1750 "));
        lux_json.add(F("Not Found"));
    } else if (!getLuminanceComplete) {
      // if we haven't read the sensor yet, let the user know
        // that we are still waiting for the first measurement
        lux_json.add((USERMOD_BH1750_FIRST_MEASUREMENT_AT - millis()) / 1000);
        lux_json.add(F(" sec until read"));
        return;
    } else {
      lux_json.add(round(lastLux));  // WLEDMM
      lux_json.add(F(" lx"));
    }
  }

  void appendConfigData() {
    oappend(SET_F("addHB('BH1750');"));
    // WLEDMM this usermod can ONLY use HW_I2C - so always use globals
    //oappend(SET_F("addInfo('BH1750:pin[]',0,'','I2C SCL');"));
    //oappend(SET_F("rOpt('BH1750:pin[]',0,'use global (")); oappendi(i2c_scl); oappend(")',-1);"); 
    //oappend(SET_F("addInfo('BH1750:pin[]',1,'','I2C SDA');"));
    //oappend(SET_F("rOpt('BH1750:pin[]',1,'use global (")); oappendi(i2c_sda); oappend(")',-1);"); 
  }    

  // (called from set.cpp) stores persistent properties to cfg.json
  void addToConfig(JsonObject &root)
  {
    // we add JSON object.
    JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
    top[F("enabled")] = enabled;
    top[FPSTR(_maxReadInterval)] = maxReadingInterval;
    top[FPSTR(_minReadInterval)] = minReadingInterval;
    top[FPSTR(_HomeAssistantDiscovery)] = HomeAssistantDiscovery;
    top[FPSTR(_offset)] = offset;

    // WLEDMM this usermod can ONLY use HW_I2C - so always use globals
    //JsonArray io_pin = top.createNestedArray(F("pin"));
    //WLEDMM: avoid global pin hijacking
    //io_pin.add((ioPin[0]==i2c_scl)?-1:ioPin[0]);
    //io_pin.add((ioPin[1]==i2c_sda)?-1:ioPin[1]);

    // top[F("help4Pins")] = F("SCL,SDA"); // help for Settings page

    DEBUG_PRINTLN(F("[BH1750] config saved."));
  }

  // called before setup() to populate properties from values stored in cfg.json
  bool readFromConfig(JsonObject &root)
  {
    int8_t newPin[2] = {-1, -1}; // WLEDMM this usermod can ONLY use HW_I2C - so always use globals
    //for (byte i=0; i<2; i++) newPin[i] = ioPin[i]; // prepare to note changed pins

    // we look for JSON object.
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull())
    {
      DEBUG_PRINT(FPSTR(_name));
      DEBUG_PRINT(F("[BH1750]"));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }
    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top[F("enabled")], enabled, false);
    configComplete &= getJsonValue(top[FPSTR(_maxReadInterval)], maxReadingInterval, 10000); //ms
    configComplete &= getJsonValue(top[FPSTR(_minReadInterval)], minReadingInterval, 500); //ms
    configComplete &= getJsonValue(top[FPSTR(_HomeAssistantDiscovery)], HomeAssistantDiscovery, false);
    configComplete &= getJsonValue(top[FPSTR(_offset)], offset, 1);
    // WLEDMM this usermod can ONLY use HW_I2C - so always use globals
    //for (byte i=0; i<2; i++) configComplete &= getJsonValue(top[F("pin")][i], newPin[i], ioPin[i]);

    DEBUG_PRINT(FPSTR(_name));
    if (!initDone) {
      // first run: reading from cfg.json
      for (byte i=0; i<2; i++) ioPin[i] = newPin[i];
      DEBUG_PRINTLN(F(" config loaded."));
    } else {
      DEBUG_PRINTLN(F(" config (re)loaded."));
      // changing parameters from settings page
#if 0
      bool pinsChanged = false;
      for (byte i=0; i<2; i++) if (ioPin[i] != newPin[i]) { pinsChanged = true; break; } // check if any pins changed
      if (pinsChanged) { //if pins changed, deallocate old pins and allocate new ones
        PinOwner po = PinOwner::UM_BH1750;
        if (ioPin[0]==i2c_scl && ioPin[1]==i2c_sda) po = PinOwner::HW_I2C;  // allow multiple allocations of HW I2C bus pins
        if (ioPin[0]==-1 && ioPin[1]==-1) po = PinOwner::HW_I2C;            // WLEDMM global HW I2C bus pins
        pinManager.deallocateMultiplePins((const uint8_t *)ioPin, 2, po);  // deallocate pins
        for (byte i=0; i<2; i++) ioPin[i] = newPin[i];
        setup();
      }
#else
      if (enabled && !sensorFound) setup();
#endif
      // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
      return !top[F("pin")].isNull();
    }

    return configComplete;
    
  }

  uint16_t getId()
  {
    return USERMOD_ID_BH1750;
  }

};

// strings to reduce flash memory usage (used more than twice)
//const char Usermod_BH1750::_name[] PROGMEM = "BH1750";     //WLEDMM not needed as we use global attributes
//const char Usermod_BH1750::_enabled[] PROGMEM = "enabled"; //WLEDMM not needed as we use global attributes
const char Usermod_BH1750::_maxReadInterval[] PROGMEM = "max-read-interval-ms";
const char Usermod_BH1750::_minReadInterval[] PROGMEM = "min-read-interval-ms";
const char Usermod_BH1750::_HomeAssistantDiscovery[] PROGMEM = "HomeAssistantDiscoveryLux";
const char Usermod_BH1750::_offset[] PROGMEM = "offset-lx";
