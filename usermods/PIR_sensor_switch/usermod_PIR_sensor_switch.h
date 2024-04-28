#pragma once

#include "wled.h"

#ifndef PIR_SENSOR_PIN
  // compatible with QuinLED-Dig-Uno
  #ifdef ARDUINO_ARCH_ESP32
    #define PIR_SENSOR_PIN 23 // Q4
  #else //ESP8266 boards
    #define PIR_SENSOR_PIN 13 // Q4 (D7 on D1 mini)
  #endif
#endif

#ifndef PIR_SENSOR_OFF_SEC
  #define PIR_SENSOR_OFF_SEC 600
#endif


/*
 * This usermod handles PIR sensor states.
 * The strip will be switched on and the off timer will be resetted when the sensor goes HIGH. 
 * When the sensor state goes LOW, the off timer is started and when it expires, the strip is switched off. 
 * Maintained by: @blazoncek
 * 
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * 
 * v2 usermods are class inheritance based and can (but don't have to) implement more functions, each of them is shown in this example.
 * Multiple v2 usermods can be added to one compilation easily.
 */

class PIRsensorSwitch : public Usermod
{
public:
  // constructor
  PIRsensorSwitch() {}
  // destructor
  ~PIRsensorSwitch() {}

  //Enable/Disable the PIR sensor
  inline void EnablePIRsensor(bool en) { enabled = en; }
  
  // Get PIR sensor enabled/disabled state
  inline bool PIRsensorEnabled() { return enabled; }

private:

  byte prevPreset   = 0;
  byte prevPlaylist = 0;

  volatile unsigned long offTimerStart = 0;     // off timer start time
  volatile bool PIRtriggered           = false; // did PIR trigger?
  byte NotifyUpdateMode  = CALL_MODE_NO_NOTIFY; // notification mode for stateUpdated(): CALL_MODE_NO_NOTIFY or CALL_MODE_DIRECT_CHANGE
  byte sensorPinState    = LOW;                 // current PIR sensor pin state
  bool initDone          = false;               // status of initialization
  unsigned long lastLoop = 0;

  // configurable parameters
  bool enabled              = true;           // PIR sensor enabled
  int8_t PIRsensorPin       = PIR_SENSOR_PIN; // PIR sensor pin
  uint32_t m_switchOffDelay = PIR_SENSOR_OFF_SEC*1000;  // delay before switch off after the sensor state goes LOW (10min)
  uint8_t m_onPreset        = 0;              // on preset
  uint8_t m_offPreset       = 0;              // off preset
  bool m_nightTimeOnly      = false;          // flag to indicate that PIR sensor should activate WLED during nighttime only
  bool m_mqttOnly           = false;          // flag to send MQTT message only (assuming it is enabled)
  // flag to enable triggering only if WLED is initially off (LEDs are not on, preventing running effect being overwritten by PIR)
  bool m_offOnly            = false;
  bool m_offMode            = offMode;
  bool m_override           = false;

  // Home Assistant
  bool HomeAssistantDiscovery = false;        // is HA discovery turned on

  // strings to reduce flash memory usage (used more than twice)
  static const char _name[];
  static const char _switchOffDelay[];
  static const char _enabled[];
  static const char _onPreset[];
  static const char _offPreset[];
  static const char _nightTime[];
  static const char _mqttOnly[];
  static const char _offOnly[];
  static const char _haDiscovery[];
  static const char _notify[];
  static const char _override[];

  /**
   * check if it is daytime
   * if sunrise/sunset is not defined (no NTP or lat/lon) default to nighttime
   */
  static bool isDayTime();

  /**
   * switch strip on/off
   */
  void switchStrip(bool switchOn);
  void publishMqtt(const char* state);

  // Create an MQTT Binary Sensor for Home Assistant Discovery purposes, this includes a pointer to the topic that is published to in the Loop.
  void publishHomeAssistantAutodiscovery();

  /**
   * Read and update PIR sensor state.
   * Initialize/reset switch off timer
   */
  bool updatePIRsensorState();

  /**
   * switch off the strip if the delay has elapsed 
   */
  bool handleOffTimer();

public:
  //Functions called by WLED

  /**
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup();

  /**
   * connected() is called every time the WiFi is (re)connected
   * Use it to initialize network interfaces
   */
  //void connected();

  /**
   * onMqttConnect() is called when MQTT connection is established
   */
  void onMqttConnect(bool sessionPresent);

  /**
   * loop() is called continuously. Here you can check for events, read sensors, etc.
   */
  void loop();

  /**
   * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
   * 
   * Add PIR sensor state and switch off timer duration to jsoninfo
   */
  void addToJsonInfo(JsonObject &root);

  /**
   * onStateChanged() is used to detect WLED state change
   */
  void onStateChange(uint8_t mode);

  /**
   * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  //void addToJsonState(JsonObject &root);

  /**
   * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  void readFromJsonState(JsonObject &root);

  /**
   * provide the changeable values
   */
  void addToConfig(JsonObject &root);

  /**
   * provide UI information and allow extending UI options
   */
  void appendConfigData();

  /**
   * restore the changeable values
   * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
   *
   * The function should return true if configuration was successfully loaded or false if there was no configuration.
   */
  bool readFromConfig(JsonObject &root);

  /**
   * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
   * This could be used in the future for the system to determine whether your usermod is installed.
   */
  uint16_t getId() { return USERMOD_ID_PIRSWITCH; }
};

// strings to reduce flash memory usage (used more than twice)
const char PIRsensorSwitch::_name[]           PROGMEM = "PIRsensorSwitch";
const char PIRsensorSwitch::_enabled[]        PROGMEM = "PIRenabled";
const char PIRsensorSwitch::_switchOffDelay[] PROGMEM = "PIRoffSec";
const char PIRsensorSwitch::_onPreset[]       PROGMEM = "on-preset";
const char PIRsensorSwitch::_offPreset[]      PROGMEM = "off-preset";
const char PIRsensorSwitch::_nightTime[]      PROGMEM = "nighttime-only";
const char PIRsensorSwitch::_mqttOnly[]       PROGMEM = "mqtt-only";
const char PIRsensorSwitch::_offOnly[]        PROGMEM = "off-only";
const char PIRsensorSwitch::_haDiscovery[]    PROGMEM = "HA-discovery";
const char PIRsensorSwitch::_notify[]         PROGMEM = "notifications";
const char PIRsensorSwitch::_override[]       PROGMEM = "override";

bool PIRsensorSwitch::isDayTime() {
  updateLocalTime();
  uint8_t hr = hour(localTime);
  uint8_t mi = minute(localTime);

  if (sunrise && sunset) {
    if (hour(sunrise)<hr && hour(sunset)>hr) {
      return true;
    } else {
      if (hour(sunrise)==hr && minute(sunrise)<mi) {
        return true;
      }
      if (hour(sunset)==hr && minute(sunset)>mi) {
        return true;
      }
    }
  }
  return false;
}

void PIRsensorSwitch::switchStrip(bool switchOn)
{
  if (m_offOnly && bri && (switchOn || (!PIRtriggered && !switchOn))) return; //if lights on and off only, do nothing
  if (PIRtriggered && switchOn) return; //if already on and triggered before, do nothing
  PIRtriggered = switchOn;
  DEBUG_PRINT(F("PIR: strip=")); DEBUG_PRINTLN(switchOn?"on":"off");
  if (switchOn) {
    if (m_onPreset) {
      if (currentPlaylist>0 && !offMode) {
        prevPlaylist = currentPlaylist;
        unloadPlaylist();
      } else if (currentPreset>0 && !offMode) {
        prevPreset   = currentPreset;
      } else {
        saveTemporaryPreset();
        prevPlaylist = 0;
        prevPreset   = 255;
      }
      applyPreset(m_onPreset, NotifyUpdateMode);
      return;
    }
    // preset not assigned
    if (bri == 0) {
      bri = briLast;
      stateUpdated(NotifyUpdateMode);
    }
  } else {
    if (m_offPreset) {
      applyPreset(m_offPreset, NotifyUpdateMode);
      return;
    } else if (prevPlaylist) {
      if (currentPreset==m_onPreset || currentPlaylist==m_onPreset) applyPreset(prevPlaylist, NotifyUpdateMode);
      prevPlaylist = 0;
      return;
    } else if (prevPreset) {
      if (prevPreset<255) { if (currentPreset==m_onPreset || currentPlaylist==m_onPreset) applyPreset(prevPreset, NotifyUpdateMode); }
      else                { if (currentPreset==m_onPreset || currentPlaylist==m_onPreset) applyTemporaryPreset(); }
      prevPreset = 0;
      return;
    }
    // preset not assigned
    if (bri != 0) {
      briLast = bri;
      bri = 0;
      stateUpdated(NotifyUpdateMode);
    }
  }
}

void PIRsensorSwitch::publishMqtt(const char* state)
{
#ifndef WLED_DISABLE_MQTT
  //Check if MQTT Connected, otherwise it will crash the 8266
  if (WLED_MQTT_CONNECTED) {
    char buf[64];
    sprintf_P(buf, PSTR("%s/motion"), mqttDeviceTopic);   //max length: 33 + 7 = 40
    mqtt->publish(buf, 0, false, state);
  }
#endif
}

void PIRsensorSwitch::publishHomeAssistantAutodiscovery()
{
#ifndef WLED_DISABLE_MQTT
  if (WLED_MQTT_CONNECTED) {
    StaticJsonDocument<600> doc;
    char uid[24], json_str[1024], buf[128];

    sprintf_P(buf, PSTR("%s Motion"), serverDescription); //max length: 33 + 7 = 40
    doc[F("name")] = buf;
    sprintf_P(buf, PSTR("%s/motion"), mqttDeviceTopic);   //max length: 33 + 7 = 40
    doc[F("stat_t")] = buf;
    doc[F("pl_on")]  = "on";
    doc[F("pl_off")] = "off";
    sprintf_P(uid, PSTR("%s_motion"), escapedMac.c_str());
    doc[F("uniq_id")] = uid;
    doc[F("dev_cla")] = F("motion");
    doc[F("exp_aft")] = 1800;

    JsonObject device = doc.createNestedObject(F("device")); // attach the sensor to the same device
    device[F("name")] = serverDescription;
    device[F("ids")]  = String(F("wled-sensor-")) + mqttClientID;
    device[F("mf")]   = "WLED";
    device[F("mdl")]  = F("FOSS");
    device[F("sw")]   = versionString;
    
    sprintf_P(buf, PSTR("homeassistant/binary_sensor/%s/config"), uid);
    DEBUG_PRINTLN(buf);
    size_t payload_size = serializeJson(doc, json_str);
    DEBUG_PRINTLN(json_str);

    mqtt->publish(buf, 0, true, json_str, payload_size); // do we really need to retain?
  }
#endif
}

bool PIRsensorSwitch::updatePIRsensorState()
{
  bool pinState = digitalRead(PIRsensorPin);
  if (pinState != sensorPinState) {
    sensorPinState = pinState; // change previous state

    if (sensorPinState == HIGH) {
      offTimerStart = 0;
      if (!m_mqttOnly && (!m_nightTimeOnly || (m_nightTimeOnly && !isDayTime()))) switchStrip(true);
      else if (NotifyUpdateMode != CALL_MODE_NO_NOTIFY) updateInterfaces(CALL_MODE_WS_SEND);
      publishMqtt("on");
    } else {
      // start switch off timer
      offTimerStart = millis();
      if (NotifyUpdateMode != CALL_MODE_NO_NOTIFY) updateInterfaces(CALL_MODE_WS_SEND);
    }
    return true;
  }
  return false;
}

bool PIRsensorSwitch::handleOffTimer()
{
  if (offTimerStart > 0 && millis() - offTimerStart > m_switchOffDelay) {
    offTimerStart = 0;
    if (enabled == true) {
      if (!m_mqttOnly && (!m_nightTimeOnly || (m_nightTimeOnly && !isDayTime()) || PIRtriggered)) switchStrip(false);
      else if (NotifyUpdateMode != CALL_MODE_NO_NOTIFY) updateInterfaces(CALL_MODE_WS_SEND);
      publishMqtt("off");
    }
    return true;
  }
  return false;
}

//Functions called by WLED

void PIRsensorSwitch::setup()
{
  if (enabled) {
    // pin retrieved from cfg.json (readFromConfig()) prior to running setup()
    if (PIRsensorPin >= 0 && pinManager.allocatePin(PIRsensorPin, false, PinOwner::UM_PIR)) {
      // PIR Sensor mode INPUT_PULLUP
      pinMode(PIRsensorPin, INPUT_PULLUP);
      sensorPinState = digitalRead(PIRsensorPin);
    } else {
      if (PIRsensorPin >= 0) {
        DEBUG_PRINTLN(F("PIRSensorSwitch pin allocation failed."));
      }
      PIRsensorPin = -1;  // allocation failed
      enabled = false;
    }
  }
  initDone = true;
}

void PIRsensorSwitch::onMqttConnect(bool sessionPresent)
{
  if (HomeAssistantDiscovery) {
    publishHomeAssistantAutodiscovery();
  }
}

void PIRsensorSwitch::loop()
{
  // only check sensors 4x/s
  if (!enabled || millis() - lastLoop < 250 || strip.isUpdating()) return;
  lastLoop = millis();

  if (!updatePIRsensorState()) {
    handleOffTimer();
  }
}

void PIRsensorSwitch::addToJsonInfo(JsonObject &root)
{
  JsonObject user = root["u"];
  if (user.isNull()) user = root.createNestedObject("u");

  JsonArray infoArr = user.createNestedArray(FPSTR(_name));

  String uiDomString;
  if (enabled) {
    if (offTimerStart > 0)
    {
      uiDomString = "";
      unsigned int offSeconds = (m_switchOffDelay - (millis() - offTimerStart)) / 1000;
      if (offSeconds >= 3600)
      {
        uiDomString += (offSeconds / 3600);
        uiDomString += F("h ");
        offSeconds %= 3600;
      }
      if (offSeconds >= 60)
      {
        uiDomString += (offSeconds / 60);
        offSeconds %= 60;
      }
      else if (uiDomString.length() > 0)
      {
        uiDomString += 0;
      }
      if (uiDomString.length() > 0)
      {
        uiDomString += F("min ");
      }
      uiDomString += (offSeconds);
      infoArr.add(uiDomString + F("s"));
    } else {
      infoArr.add(sensorPinState ? F("sensor on") : F("inactive"));
    }
  } else {
    infoArr.add(F("disabled"));
  }

  uiDomString  = F(" <button class=\"btn btn-xs\" onclick=\"requestJson({");
  uiDomString += FPSTR(_name);
  uiDomString += F(":{");
  uiDomString += FPSTR(_enabled);
  if (enabled) {
    uiDomString += F(":false}});\">");
    uiDomString += F("<i class=\"icons on\">&#xe325;</i>");
  } else {
    uiDomString += F(":true}});\">");
    uiDomString += F("<i class=\"icons off\">&#xe08f;</i>");
  }
  uiDomString += F("</button>");
  infoArr.add(uiDomString);

  JsonObject sensor = root[F("sensor")];
  if (sensor.isNull()) sensor = root.createNestedObject(F("sensor"));
  sensor[F("motion")] = sensorPinState || offTimerStart>0 ? true : false;
}

void PIRsensorSwitch::onStateChange(uint8_t mode) {
  if (!initDone) return;
  DEBUG_PRINT(F("PIR: offTimerStart=")); DEBUG_PRINTLN(offTimerStart);
  if (m_override && PIRtriggered && offTimerStart) { // debounce
    // checking PIRtriggered and offTimerStart will prevent cancellation upon On trigger
    DEBUG_PRINTLN(F("PIR: Canceled."));
    offTimerStart = 0;
    PIRtriggered = false;
  }
}

void PIRsensorSwitch::readFromJsonState(JsonObject &root)
{
  if (!initDone) return;  // prevent crash on boot applyPreset()
  JsonObject usermod = root[FPSTR(_name)];
  if (!usermod.isNull()) {
    if (usermod[FPSTR(_enabled)].is<bool>()) {
      enabled = usermod[FPSTR(_enabled)].as<bool>();
    }
  }
}

void PIRsensorSwitch::addToConfig(JsonObject &root)
{
  JsonObject top = root.createNestedObject(FPSTR(_name));
  top[FPSTR(_enabled)]        = enabled;
  top[FPSTR(_switchOffDelay)] = m_switchOffDelay / 1000;
  top["pin"]                  = PIRsensorPin;
  top[FPSTR(_onPreset)]       = m_onPreset;
  top[FPSTR(_offPreset)]      = m_offPreset;
  top[FPSTR(_nightTime)]      = m_nightTimeOnly;
  top[FPSTR(_mqttOnly)]       = m_mqttOnly;
  top[FPSTR(_offOnly)]        = m_offOnly;
  top[FPSTR(_override)]       = m_override;
  top[FPSTR(_haDiscovery)]    = HomeAssistantDiscovery;
  top[FPSTR(_notify)]         = (NotifyUpdateMode != CALL_MODE_NO_NOTIFY);
  DEBUG_PRINTLN(F("PIR config saved."));
}

void PIRsensorSwitch::appendConfigData()
{
  oappend(SET_F("addInfo('PIRsensorSwitch:HA-discovery',1,'HA=Home Assistant');"));     // 0 is field type, 1 is actual field
  oappend(SET_F("addInfo('PIRsensorSwitch:notifications',1,'Periodic WS updates');"));  // 0 is field type, 1 is actual field
  oappend(SET_F("addInfo('PIRsensorSwitch:override',1,'Cancel timer on change');"));    // 0 is field type, 1 is actual field
}

bool PIRsensorSwitch::readFromConfig(JsonObject &root)
{
  bool oldEnabled = enabled;
  int8_t oldPin = PIRsensorPin;

  DEBUG_PRINT(FPSTR(_name));
  JsonObject top = root[FPSTR(_name)];
  if (top.isNull()) {
    DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
    return false;
  }

  PIRsensorPin = top["pin"] | PIRsensorPin;

  enabled = top[FPSTR(_enabled)] | enabled;

  m_switchOffDelay = (top[FPSTR(_switchOffDelay)] | m_switchOffDelay/1000) * 1000;

  m_onPreset = top[FPSTR(_onPreset)] | m_onPreset;
  m_onPreset = max(0,min(250,(int)m_onPreset));
  m_offPreset = top[FPSTR(_offPreset)] | m_offPreset;
  m_offPreset = max(0,min(250,(int)m_offPreset));

  m_nightTimeOnly = top[FPSTR(_nightTime)] | m_nightTimeOnly;
  m_mqttOnly      = top[FPSTR(_mqttOnly)] | m_mqttOnly;
  m_offOnly       = top[FPSTR(_offOnly)] | m_offOnly;
  m_override      = top[FPSTR(_override)] | m_override;
  HomeAssistantDiscovery = top[FPSTR(_haDiscovery)] | HomeAssistantDiscovery;

  NotifyUpdateMode = top[FPSTR(_notify)] ? CALL_MODE_DIRECT_CHANGE : CALL_MODE_NO_NOTIFY;

  if (!initDone) {
    // reading config prior to setup()
    DEBUG_PRINTLN(F(" config loaded."));
  } else {
    if (oldPin != PIRsensorPin || oldEnabled != enabled) {
      // check if pin is OK
      if (oldPin != PIRsensorPin && oldPin >= 0) {
        // if we are changing pin in settings page
        // deallocate old pin
        pinManager.deallocatePin(oldPin, PinOwner::UM_PIR);
        if (pinManager.allocatePin(PIRsensorPin, false, PinOwner::UM_PIR)) {
          pinMode(PIRsensorPin, INPUT_PULLUP);
        } else {
          // allocation failed
          PIRsensorPin = -1;
          enabled = false;
        }
      }
      if (enabled) {
        sensorPinState = digitalRead(PIRsensorPin);
      }
    }
    DEBUG_PRINTLN(F(" config (re)loaded."));
  }
  // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
  return !top[FPSTR(_override)].isNull();
}
