/*
 * Usermod for detecting people entering/leaving a staircase and switching the
 * staircase on/off.
 *
 * Edit the Animated_Staircase_config.h file to compile this usermod for your
 * specific configuration.
 * 
 * See the accompanying README.md file for more info.
 */
#pragma once
#include "wled.h"

class Animated_Staircase : public Usermod {
  private:

    /* configuration (available in API and stored in flash) */
    bool enabled = false;                   // Enable this usermod
    unsigned long segment_delay_ms = 150;   // Time between switching each segment
    unsigned long on_time_ms       = 30000; // The time for the light to stay on
    int8_t topPIRorTriggerPin      = -1;    // disabled
    int8_t bottomPIRorTriggerPin   = -1;    // disabled
    int8_t topEchoPin              = -1;    // disabled
    int8_t bottomEchoPin           = -1;    // disabled
    bool useUSSensorTop            = false; // using PIR or UltraSound sensor?
    bool useUSSensorBottom         = false; // using PIR or UltraSound sensor?
    unsigned int topMaxDist        = 50;    // default maximum measured distance in cm, top
    unsigned int bottomMaxDist     = 50;    // default maximum measured distance in cm, bottom
    bool togglePower               = false; // toggle power on/off with staircase on/off

    /* runtime variables */
    bool initDone = false;

    // Time between checking of the sensors
    const unsigned int scanDelay = 100;

    // Lights on or off.
    // Flipping this will start a transition.
    bool on = false;

    // Swipe direction for current transition
  #define SWIPE_UP true
  #define SWIPE_DOWN false
    bool swipe = SWIPE_UP;

    // Indicates which Sensor was seen last (to determine
    // the direction when swiping off)
  #define LOWER false
  #define UPPER true
    bool lastSensor = LOWER;

    // Time of the last transition action
    unsigned long lastTime = 0;

    // Time of the last sensor check
    unsigned long lastScanTime = 0;

    // Last time the lights were switched on or off
    unsigned long lastSwitchTime = 0;

    // segment id between onIndex and offIndex are on.
    // controll the swipe by setting/moving these indices around.
    // onIndex must be less than or equal to offIndex
    byte onIndex = 0;
    byte offIndex = 0;

    // The maximum number of configured segments.
    // Dynamically updated based on user configuration.
    byte maxSegmentId = 1;
    byte minSegmentId = 0;

    // These values are used by the API to read the
    // last sensor state, or trigger a sensor
    // through the API
    bool topSensorRead     = false;
    bool topSensorWrite    = false;
    bool bottomSensorRead  = false;
    bool bottomSensorWrite = false;
    bool topSensorState    = false;
    bool bottomSensorState = false;

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _segmentDelay[];
    static const char _onTime[];
    static const char _useTopUltrasoundSensor[];
    static const char _topPIRorTrigger_pin[];
    static const char _topEcho_pin[];
    static const char _useBottomUltrasoundSensor[];
    static const char _bottomPIRorTrigger_pin[];
    static const char _bottomEcho_pin[];
    static const char _topEchoCm[];
    static const char _bottomEchoCm[];
    static const char _togglePower[];

    void publishMqtt(bool bottom, const char* state) {
#ifndef WLED_DISABLE_MQTT
      //Check if MQTT Connected, otherwise it will crash the 8266
      if (WLED_MQTT_CONNECTED){
        char subuf[64];
        sprintf_P(subuf, PSTR("%s/motion/%d"), mqttDeviceTopic, (int)bottom);
        mqtt->publish(subuf, 0, false, state);
      }
#endif
    }

    void updateSegments() {
      for (int i = minSegmentId; i < maxSegmentId; i++) {
        Segment &seg = strip.getSegment(i);
        if (!seg.isActive()) continue; // skip gaps
        if (i >= onIndex && i < offIndex) {
          seg.setOption(SEG_OPTION_ON, true);
          // We may need to copy mode and colors from segment 0 to make sure
          // changes are propagated even when the config is changed during a wipe
          // seg.setMode(mainsegment.mode);
          // seg.setColor(0, mainsegment.colors[0]);
        } else {
          seg.setOption(SEG_OPTION_ON, false);
        }
        // Always mark segments as "transitional", we are animating the staircase
        //seg.setOption(SEG_OPTION_TRANSITIONAL, true); // not needed anymore as setOption() does it
      }
      strip.trigger();  // force strip refresh
      stateChanged = true;  // inform external devices/UI of change
      colorUpdated(CALL_MODE_DIRECT_CHANGE);
    }

    /*
    * Detects if an object is within ultrasound range.
    * signalPin: The pin where the pulse is sent
    * echoPin:   The pin where the echo is received
    * maxTimeUs: Detection timeout in microseconds. If an echo is
    *            received within this time, an object is detected
    *            and the function will return true.
    *
    * The speed of sound is 343 meters per second at 20 degrees Celsius.
    * Since the sound has to travel back and forth, the detection
    * distance for the sensor in cm is (0.0343 * maxTimeUs) / 2.
    *
    * For practical reasons, here are some useful distances:
    *
    * Distance =	maxtime
    *     5 cm =  292 uS
    *    10 cm =  583 uS
    *    20 cm = 1166 uS
    *    30 cm = 1749 uS
    *    50 cm = 2915 uS
    *   100 cm = 5831 uS
    */
    bool ultrasoundRead(int8_t signalPin, int8_t echoPin, unsigned int maxTimeUs) {
      if (signalPin<0 || echoPin<0) return false;
      digitalWrite(signalPin, LOW);
      delayMicroseconds(2);
      digitalWrite(signalPin, HIGH);
      delayMicroseconds(10);
      digitalWrite(signalPin, LOW);
      return pulseIn(echoPin, HIGH, maxTimeUs) > 0;
    }

    bool checkSensors() {
      bool sensorChanged = false;

      if ((millis() - lastScanTime) > scanDelay) {
        lastScanTime = millis();

        bottomSensorRead = bottomSensorWrite ||
          (!useUSSensorBottom ?
            (bottomPIRorTriggerPin<0 ? false : digitalRead(bottomPIRorTriggerPin)) :
            ultrasoundRead(bottomPIRorTriggerPin, bottomEchoPin, bottomMaxDist*59)  // cm to us
          );
        topSensorRead = topSensorWrite ||
          (!useUSSensorTop ?
            (topPIRorTriggerPin<0 ? false : digitalRead(topPIRorTriggerPin)) :
            ultrasoundRead(topPIRorTriggerPin, topEchoPin, topMaxDist*59)   // cm to us
          );

        if (bottomSensorRead != bottomSensorState) {
          bottomSensorState = bottomSensorRead; // change previous state
          sensorChanged = true;
          publishMqtt(true, bottomSensorState ? "on" : "off");
          DEBUG_PRINTLN(F("Bottom sensor changed."));
        }

        if (topSensorRead != topSensorState) {
          topSensorState = topSensorRead; // change previous state
          sensorChanged = true;
          publishMqtt(false, topSensorState ? "on" : "off");
          DEBUG_PRINTLN(F("Top sensor changed."));
        }

        // Values read, reset the flags for next API call
        topSensorWrite = false;
        bottomSensorWrite = false;

        if (topSensorRead != bottomSensorRead) {
          lastSwitchTime = millis();

          if (on) {
            lastSensor = topSensorRead;
          } else {
            if (togglePower && onIndex == offIndex && offMode) toggleOnOff(); // toggle power on if off
            // If the bottom sensor triggered, we need to swipe up, ON
            swipe = bottomSensorRead;

            DEBUG_PRINT(F("ON -> Swipe "));
            DEBUG_PRINTLN(swipe ? F("up.") : F("down."));

            if (onIndex == offIndex) {
              // Position the indices for a correct on-swipe
              if (swipe == SWIPE_UP) {
                onIndex = minSegmentId;
              } else {
                onIndex = maxSegmentId;
              }
              offIndex = onIndex;
            }
            on = true;
          }
        }
      }
      return sensorChanged;
    }

    void autoPowerOff() {
      if ((millis() - lastSwitchTime) > on_time_ms) {
        // if sensors are still on, do nothing
        if (bottomSensorState || topSensorState) return;

        // Swipe OFF in the direction of the last sensor detection
        swipe = lastSensor;
        on = false;

        DEBUG_PRINT(F("OFF -> Swipe "));
        DEBUG_PRINTLN(swipe ? F("up.") : F("down."));
      }
    }

    void updateSwipe() {
      if ((millis() - lastTime) > segment_delay_ms) {
        lastTime = millis();

        byte oldOn  = onIndex;
        byte oldOff = offIndex;
        if (on) {
          // Turn on all segments
          onIndex  = MAX(minSegmentId, onIndex - 1);
          offIndex = MIN(maxSegmentId, offIndex + 1);
        } else {
          if (swipe == SWIPE_UP) {
            onIndex = MIN(offIndex, onIndex + 1);
          } else {
            offIndex = MAX(onIndex, offIndex - 1);
          }
        }
        if (oldOn != onIndex || oldOff != offIndex) {
          updateSegments(); // reduce the number of updates to necessary ones
          if (togglePower && onIndex == offIndex && !offMode && !on) toggleOnOff();  // toggle power off for all segments off
        }
      }
    }

    // send sensor values to JSON API
    void writeSensorsToJson(JsonObject& staircase) {
      staircase[F("top-sensor")]    = topSensorRead;
      staircase[F("bottom-sensor")] = bottomSensorRead;
    }

    // allow overrides from JSON API
    void readSensorsFromJson(JsonObject& staircase) {
      bottomSensorWrite = bottomSensorState || (staircase[F("bottom-sensor")].as<bool>());
      topSensorWrite    = topSensorState    || (staircase[F("top-sensor")].as<bool>());
    }

    void enable(bool enable) {
      if (enable) {
        DEBUG_PRINTLN(F("Animated Staircase enabled."));
        DEBUG_PRINT(F("Delay between steps: "));
        DEBUG_PRINT(segment_delay_ms);
        DEBUG_PRINT(F(" milliseconds.\nStairs switch off after: "));
        DEBUG_PRINT(on_time_ms / 1000);
        DEBUG_PRINTLN(F(" seconds."));

        if (!useUSSensorBottom)
          pinMode(bottomPIRorTriggerPin, INPUT_PULLUP);
        else {
          pinMode(bottomPIRorTriggerPin, OUTPUT);
          pinMode(bottomEchoPin, INPUT);
        }

        if (!useUSSensorTop)
          pinMode(topPIRorTriggerPin, INPUT_PULLUP);
        else {
          pinMode(topPIRorTriggerPin, OUTPUT);
          pinMode(topEchoPin, INPUT);
        }
        onIndex  = minSegmentId = strip.getMainSegmentId(); // it may not be the best idea to start with main segment as it may not be the first one
        offIndex = maxSegmentId = strip.getLastActiveSegmentId() + 1;

        // shorten the strip transition time to be equal or shorter than segment delay
        transitionDelay = segment_delay_ms;
        strip.setTransition(segment_delay_ms);
        strip.trigger();
      } else {
        if (togglePower && !on && offMode) toggleOnOff(); // toggle power on if off
        // Restore segment options
        for (int i = 0; i <= strip.getLastActiveSegmentId(); i++) {
          Segment &seg = strip.getSegment(i);
          if (!seg.isActive()) continue; // skip vector gaps
          seg.setOption(SEG_OPTION_ON, true);
        }
        strip.trigger();  // force strip update
        stateChanged = true;  // inform external devices/UI of change
        colorUpdated(CALL_MODE_DIRECT_CHANGE);
        DEBUG_PRINTLN(F("Animated Staircase disabled."));
      }
      enabled = enable;
    }

  public:
    void setup() {
      // standardize invalid pin numbers to -1
      if (topPIRorTriggerPin    < 0) topPIRorTriggerPin    = -1;
      if (topEchoPin            < 0) topEchoPin            = -1;
      if (bottomPIRorTriggerPin < 0) bottomPIRorTriggerPin = -1;
      if (bottomEchoPin         < 0) bottomEchoPin         = -1;
      // allocate pins
      PinManagerPinType pins[4] = {
        { topPIRorTriggerPin, useUSSensorTop },
        { topEchoPin, false },
        { bottomPIRorTriggerPin, useUSSensorBottom },
        { bottomEchoPin, false },
      };
      // NOTE: this *WILL* return TRUE if all the pins are set to -1.
      //       this is *BY DESIGN*.
      if (!pinManager.allocateMultiplePins(pins, 4, PinOwner::UM_AnimatedStaircase)) {
        topPIRorTriggerPin = -1;
        topEchoPin = -1;
        bottomPIRorTriggerPin = -1;
        bottomEchoPin = -1;
        enabled = false;
      }
      enable(enabled);
      initDone = true;
    }

    void loop() {
      if (!enabled || strip.isUpdating()) return;
      minSegmentId = strip.getMainSegmentId();  // it may not be the best idea to start with main segment as it may not be the first one
      maxSegmentId = strip.getLastActiveSegmentId() + 1;
      checkSensors();
      if (on) autoPowerOff();
      updateSwipe();
    }

    uint16_t getId() { return USERMOD_ID_ANIMATED_STAIRCASE; }

#ifndef WLED_DISABLE_MQTT
    /**
     * handling of MQTT message
     * topic only contains stripped topic (part after /wled/MAC)
     * topic should look like: /swipe with amessage of [up|down]
     */
    bool onMqttMessage(char* topic, char* payload) {
      if (strlen(topic) == 6 && strncmp_P(topic, PSTR("/swipe"), 6) == 0) {
        String action = payload;
        if (action == "up") {
          bottomSensorWrite = true;
          return true;
        } else if (action == "down") {
          topSensorWrite = true;
          return true;
        } else if (action == "on") {
          enable(true);
          return true;
        } else if (action == "off") {
          enable(false);
          return true;
        }
      }
      return false;
    }

    /**
     * subscribe to MQTT topic for controlling usermod
     */
    void onMqttConnect(bool sessionPresent) {
      //(re)subscribe to required topics
      char subuf[64];
      if (mqttDeviceTopic[0] != 0) {
        strcpy(subuf, mqttDeviceTopic);
        strcat_P(subuf, PSTR("/swipe"));
        mqtt->subscribe(subuf, 0);
      }
    }
#endif

    void addToJsonState(JsonObject& root) {
      JsonObject staircase = root[FPSTR(_name)];
      if (staircase.isNull()) {
        staircase = root.createNestedObject(FPSTR(_name));
      }
      writeSensorsToJson(staircase);
      DEBUG_PRINTLN(F("Staircase sensor state exposed in API."));
    }

    /*
    * Reads configuration settings from the json API.
    * See void addToJsonState(JsonObject& root)
    */
    void readFromJsonState(JsonObject& root) {
      if (!initDone) return;  // prevent crash on boot applyPreset()
      bool en = enabled;
      JsonObject staircase = root[FPSTR(_name)];
      if (!staircase.isNull()) {
        if (staircase[FPSTR(_enabled)].is<bool>()) {
          en = staircase[FPSTR(_enabled)].as<bool>();
        } else {
          String str = staircase[FPSTR(_enabled)]; // checkbox -> off or on
          en = (bool)(str!="off"); // off is guaranteed to be present
        }
        if (en != enabled) enable(en);
        readSensorsFromJson(staircase);
        DEBUG_PRINTLN(F("Staircase sensor state read from API."));
      }
    }

    void appendConfigData() {
      //oappend(SET_F("dd=addDropdown('staircase','selectfield');"));
      //oappend(SET_F("addOption(dd,'1st value',0);"));
      //oappend(SET_F("addOption(dd,'2nd value',1);"));
      //oappend(SET_F("addInfo('staircase:selectfield',1,'additional info');"));  // 0 is field type, 1 is actual field
    }


    /*
    * Writes the configuration to internal flash memory.
    */
    void addToConfig(JsonObject& root) {
      JsonObject staircase = root[FPSTR(_name)];
      if (staircase.isNull()) {
        staircase = root.createNestedObject(FPSTR(_name));
      }
      staircase[FPSTR(_enabled)]                   = enabled;
      staircase[FPSTR(_segmentDelay)]              = segment_delay_ms;
      staircase[FPSTR(_onTime)]                    = on_time_ms / 1000;
      staircase[FPSTR(_useTopUltrasoundSensor)]    = useUSSensorTop;
      staircase[FPSTR(_topPIRorTrigger_pin)]       = topPIRorTriggerPin;
      staircase[FPSTR(_topEcho_pin)]               = useUSSensorTop ? topEchoPin : -1;
      staircase[FPSTR(_useBottomUltrasoundSensor)] = useUSSensorBottom;
      staircase[FPSTR(_bottomPIRorTrigger_pin)]    = bottomPIRorTriggerPin;
      staircase[FPSTR(_bottomEcho_pin)]            = useUSSensorBottom ? bottomEchoPin : -1;
      staircase[FPSTR(_topEchoCm)]                 = topMaxDist;
      staircase[FPSTR(_bottomEchoCm)]              = bottomMaxDist;
      staircase[FPSTR(_togglePower)]               = togglePower;
      DEBUG_PRINTLN(F("Staircase config saved."));
    }

    /*
    * Reads the configuration to internal flash memory before setup() is called.
    * 
    * The function should return true if configuration was successfully loaded or false if there was no configuration.
    */
    bool readFromConfig(JsonObject& root) {
      bool oldUseUSSensorTop = useUSSensorTop;
      bool oldUseUSSensorBottom = useUSSensorBottom;
      int8_t oldTopAPin = topPIRorTriggerPin;
      int8_t oldTopBPin = topEchoPin;
      int8_t oldBottomAPin = bottomPIRorTriggerPin;
      int8_t oldBottomBPin = bottomEchoPin;

      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) {
        DEBUG_PRINT(FPSTR(_name));
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      enabled   = top[FPSTR(_enabled)] | enabled;

      segment_delay_ms = top[FPSTR(_segmentDelay)] | segment_delay_ms;
      segment_delay_ms = (unsigned long) min((unsigned long)10000,max((unsigned long)10,(unsigned long)segment_delay_ms));  // max delay 10s

      on_time_ms = top[FPSTR(_onTime)] | on_time_ms/1000;
      on_time_ms = min(900,max(10,(int)on_time_ms)) * 1000; // min 10s, max 15min

      useUSSensorTop     = top[FPSTR(_useTopUltrasoundSensor)] | useUSSensorTop;
      topPIRorTriggerPin = top[FPSTR(_topPIRorTrigger_pin)] | topPIRorTriggerPin;
      topEchoPin         = top[FPSTR(_topEcho_pin)] | topEchoPin;

      useUSSensorBottom     = top[FPSTR(_useBottomUltrasoundSensor)] | useUSSensorBottom;
      bottomPIRorTriggerPin = top[FPSTR(_bottomPIRorTrigger_pin)] | bottomPIRorTriggerPin;
      bottomEchoPin         = top[FPSTR(_bottomEcho_pin)] | bottomEchoPin;

      topMaxDist    = top[FPSTR(_topEchoCm)] | topMaxDist;
      topMaxDist    = min(150,max(30,(int)topMaxDist));     // max distance ~1.5m (a lag of 9ms may be expected)
      bottomMaxDist = top[FPSTR(_bottomEchoCm)] | bottomMaxDist;
      bottomMaxDist = min(150,max(30,(int)bottomMaxDist));  // max distance ~1.5m (a lag of 9ms may be expected)

      togglePower = top[FPSTR(_togglePower)] | togglePower;  // staircase toggles power on/off

      DEBUG_PRINT(FPSTR(_name));
      if (!initDone) {
        // first run: reading from cfg.json
        DEBUG_PRINTLN(F(" config loaded."));
      } else {
        // changing parameters from settings page
        DEBUG_PRINTLN(F(" config (re)loaded."));
        bool changed = false;
        if ((oldUseUSSensorTop != useUSSensorTop) ||
            (oldUseUSSensorBottom != useUSSensorBottom) ||
            (oldTopAPin != topPIRorTriggerPin) ||
            (oldTopBPin != topEchoPin) ||
            (oldBottomAPin != bottomPIRorTriggerPin) ||
            (oldBottomBPin != bottomEchoPin)) {
          changed = true;
          pinManager.deallocatePin(oldTopAPin, PinOwner::UM_AnimatedStaircase);
          pinManager.deallocatePin(oldTopBPin, PinOwner::UM_AnimatedStaircase);
          pinManager.deallocatePin(oldBottomAPin, PinOwner::UM_AnimatedStaircase);
          pinManager.deallocatePin(oldBottomBPin, PinOwner::UM_AnimatedStaircase);
        }
        if (changed) setup();
      }
      // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
      return !top[FPSTR(_togglePower)].isNull();
    }

    /*
    * Shows the delay between steps and power-off time in the "info"
    * tab of the web-UI.
    */
    void addToJsonInfo(JsonObject& root) {
      JsonObject user = root["u"];
      if (user.isNull()) {
        user = root.createNestedObject("u");
      }

      JsonArray infoArr = user.createNestedArray(FPSTR(_name));  // name

      String uiDomString = F("<button class=\"btn btn-xs\" onclick=\"requestJson({");
      uiDomString += FPSTR(_name);
      uiDomString += F(":{");
      uiDomString += FPSTR(_enabled);
      uiDomString += enabled ? F(":false}});\">") : F(":true}});\">");
      uiDomString += F("<i class=\"icons ");
      uiDomString += enabled ? "on" : "off";
      uiDomString += F("\">&#xe08f;</i></button>");
      infoArr.add(uiDomString);
    }
};

// strings to reduce flash memory usage (used more than twice)
const char Animated_Staircase::_name[]                      PROGMEM = "staircase";
const char Animated_Staircase::_enabled[]                   PROGMEM = "enabled";
const char Animated_Staircase::_segmentDelay[]              PROGMEM = "segment-delay-ms";
const char Animated_Staircase::_onTime[]                    PROGMEM = "on-time-s";
const char Animated_Staircase::_useTopUltrasoundSensor[]    PROGMEM = "useTopUltrasoundSensor";
const char Animated_Staircase::_topPIRorTrigger_pin[]       PROGMEM = "topPIRorTrigger_pin";
const char Animated_Staircase::_topEcho_pin[]               PROGMEM = "topEcho_pin";
const char Animated_Staircase::_useBottomUltrasoundSensor[] PROGMEM = "useBottomUltrasoundSensor";
const char Animated_Staircase::_bottomPIRorTrigger_pin[]    PROGMEM = "bottomPIRorTrigger_pin";
const char Animated_Staircase::_bottomEcho_pin[]            PROGMEM = "bottomEcho_pin";
const char Animated_Staircase::_topEchoCm[]                 PROGMEM = "top-dist-cm";
const char Animated_Staircase::_bottomEchoCm[]              PROGMEM = "bottom-dist-cm";
const char Animated_Staircase::_togglePower[]               PROGMEM = "toggle-on-off";
