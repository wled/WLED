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

/*
 * This usermod handles PIR sensor states.
 * The strip will be switched on and the off timer will be resetted when the sensor goes HIGH. 
 * When the sensor state goes LOW, the off timer is started and when it expires, the strip is switched off. 
 * 
 * 
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/Aircoookie/WLED/wiki/Add-own-functionality
 * 
 * v2 usermods are class inheritance based and can (but don't have to) implement more functions, each of them is shown in this example.
 * Multiple v2 usermods can be added to one compilation easily.
 * 
 * Creating a usermod:
 * This file serves as an example. If you want to create a usermod, it is recommended to use usermod_v2_empty.h from the usermods folder as a template.
 * Please remember to rename the class and file to a descriptive name.
 * You may also use multiple .h and .cpp files.
 * 
 * Using a usermod:
 * 1. Copy the usermod into the sketch folder (same folder as wled00.ino)
 * 2. Register the usermod by adding #include "usermod_filename.h" in the top and registerUsermod(new MyUsermodClass()) in the bottom of usermods_list.cpp
 */

class PIRsensorSwitch : public Usermod
{
public:
  /**
   * constructor
   */
  PIRsensorSwitch() {}
  /**
   * desctructor
   */
  ~PIRsensorSwitch() {}

  /**
   * Enable/Disable the PIR sensor
   */
  void EnablePIRsensor(bool en) { enabled = en; }
  /**
   * Get PIR sensor enabled/disabled state
   */
  bool PIRsensorEnabled() { return enabled; }

private:

  byte prevPreset   = 0;
  byte prevPlaylist = 0;
  bool savedState   = false;

  uint32_t offTimerStart = 0;                   // off timer start time
  byte NotifyUpdateMode  = CALL_MODE_NO_NOTIFY; // notification mode for stateUpdated(): CALL_MODE_NO_NOTIFY or CALL_MODE_DIRECT_CHANGE
  byte sensorPinState    = LOW;                 // current PIR sensor pin state
  bool initDone          = false;               // status of initialization
  bool PIRtriggered      = false;
  unsigned long lastLoop = 0;

  // configurable parameters
  bool enabled              = true;           // PIR sensor enabled
  int8_t PIRsensorPin       = PIR_SENSOR_PIN; // PIR sensor pin
  uint32_t m_switchOffDelay = 600000;         // delay before switch off after the sensor state goes LOW (10min)
  uint8_t m_onPreset        = 0;              // on preset
  uint8_t m_offPreset       = 0;              // off preset
  bool m_nightTimeOnly      = false;          // flag to indicate that PIR sensor should activate WLED during nighttime only
  bool m_mqttOnly           = false;          // flag to send MQTT message only (assuming it is enabled)
  // flag to enable triggering only if WLED is initially off (LEDs are not on, preventing running effect being overwritten by PIR)
  bool m_offOnly            = false;

  // strings to reduce flash memory usage (used more than twice)
  static const char _name[];
  static const char _switchOffDelay[];
  static const char _enabled[];
  static const char _onPreset[];
  static const char _offPreset[];
  static const char _nightTime[];
  static const char _mqttOnly[];
  static const char _offOnly[];
  static const char _notify[];

  /**
   * check if it is daytime
   * if sunrise/sunset is not defined (no NTP or lat/lon) default to nighttime
   */
  bool isDayTime() {
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

  /**
   * switch strip on/off
   */
  void switchStrip(bool switchOn)
  {
    if (m_offOnly && bri && (switchOn || (!PIRtriggered && !switchOn))) return;
    PIRtriggered = switchOn;
    if (switchOn) {
      if (m_onPreset) {
        if (currentPlaylist>0)    prevPlaylist = currentPlaylist;
        else if (currentPreset>0) prevPreset   = currentPreset;
        else {
          saveTemporaryPreset();
          savedState   = true;
          prevPlaylist = 0;
          prevPreset   = 0;
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
        applyPreset(prevPlaylist, NotifyUpdateMode);
        prevPlaylist = 0;
        return;
      } else if (prevPreset) {
        applyPreset(prevPreset, NotifyUpdateMode);
        prevPreset = 0;
        return;
      } else if (savedState) {
        applyTemporaryPreset();
        savedState = false;
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

  void publishMqtt(const char* state)
  {
    //Check if MQTT Connected, otherwise it will crash the 8266
    if (WLED_MQTT_CONNECTED){
      char subuf[64];
      strcpy(subuf, mqttDeviceTopic);
      strcat_P(subuf, PSTR("/motion"));
      mqtt->publish(subuf, 0, false, state);
    }
  }

  /**
   * Read and update PIR sensor state.
   * Initilize/reset switch off timer
   */
  bool updatePIRsensorState()
  {
    bool pinState = digitalRead(PIRsensorPin);
    if (pinState != sensorPinState) {
      sensorPinState = pinState; // change previous state

      if (sensorPinState == HIGH) {
        offTimerStart = 0;
        if (!m_mqttOnly && (!m_nightTimeOnly || (m_nightTimeOnly && !isDayTime()))) switchStrip(true);
        publishMqtt("on");
      } else /*if (bri != 0)*/ {
        // start switch off timer
        offTimerStart = millis();
      }
      return true;
    }
    return false;
  }

  /**
   * switch off the strip if the delay has elapsed 
   */
  bool handleOffTimer()
  {
    if (offTimerStart > 0 && millis() - offTimerStart > m_switchOffDelay)
    {
      if (enabled == true)
      {
        if (!m_mqttOnly && (!m_nightTimeOnly || (m_nightTimeOnly && !isDayTime()))) switchStrip(false);
        publishMqtt("off");
      }
      offTimerStart = 0;
      return true;
    }
    return false;
  }

public:
  //Functions called by WLED

  /**
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup()
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

  /**
   * connected() is called every time the WiFi is (re)connected
   * Use it to initialize network interfaces
   */
  void connected()
  {
  }

  /**
   * loop() is called continuously. Here you can check for events, read sensors, etc.
   */
  void loop()
  {
    // only check sensors 4x/s
    if (!enabled || millis() - lastLoop < 250 || strip.isUpdating()) return;
    lastLoop = millis();

    if (!updatePIRsensorState()) {
      handleOffTimer();
    }
  }

  /**
   * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
   * 
   * Add PIR sensor state and switch off timer duration to jsoninfo
   */
  void addToJsonInfo(JsonObject &root)
  {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    String uiDomString = F("<button class=\"btn\" onclick=\"requestJson({");
    uiDomString += FPSTR(_name);
    uiDomString += F(":{");
    uiDomString += FPSTR(_enabled);
    if (enabled) {
      uiDomString += F(":false}});\">");
      uiDomString += F("PIR <i class=\"icons\">&#xe325;</i>");
    } else {
      uiDomString += F(":true}});\">");
      uiDomString += F("PIR <i class=\"icons\">&#xe08f;</i>");
    }
    uiDomString += F("</button>");
    JsonArray infoArr = user.createNestedArray(uiDomString); // timer value

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
  }

  /**
   * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
/*
  void addToJsonState(JsonObject &root)
  {
  }
*/

  /**
   * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */

  void readFromJsonState(JsonObject &root)
  {
    if (!initDone) return;  // prevent crash on boot applyPreset()
    JsonObject usermod = root[FPSTR(_name)];
    if (!usermod.isNull()) {
      if (usermod[FPSTR(_enabled)].is<bool>()) {
        enabled = usermod[FPSTR(_enabled)].as<bool>();
      }
    }
  }


  /**
   * provide the changeable values
   */
  void addToConfig(JsonObject &root)
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
    top[FPSTR(_notify)]         = (NotifyUpdateMode != CALL_MODE_NO_NOTIFY);
    DEBUG_PRINTLN(F("PIR config saved."));
  }

  /**
   * restore the changeable values
   * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
   *
   * The function should return true if configuration was successfully loaded or false if there was no configuration.
   */
  bool readFromConfig(JsonObject &root)
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
    return !top[FPSTR(_notify)].isNull();
  }

  /**
   * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
   * This could be used in the future for the system to determine whether your usermod is installed.
   */
  uint16_t getId()
  {
    return USERMOD_ID_PIRSWITCH;
  }
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
const char PIRsensorSwitch::_notify[]         PROGMEM = "notifications";
