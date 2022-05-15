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
    byte mainSegmentId = 0;

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
    
    void publishMqtt(bool bottom, const char* state)
    {
      //Check if MQTT Connected, otherwise it will crash the 8266
      if (WLED_MQTT_CONNECTED){
        char subuf[64];
        sprintf_P(subuf, PSTR("%s/motion/%d"), mqttDeviceTopic, (int)bottom);
        mqtt->publish(subuf, 0, false, state);
      }
    }

    void updateSegments() {
      mainSegmentId = strip.getMainSegmentId();
      WS2812FX::Segment* segments = strip.getSegments();
      for (int i = 0; i < MAX_NUM_SEGMENTS; i++, segments++) {
        if (!segments->isActive()) {
          maxSegmentId = i - 1;
          break;
        }

        if (i >= onIndex && i < offIndex) {
          segments->setOption(SEG_OPTION_ON, 1, i);

          // We may need to copy mode and colors from segment 0 to make sure
          // changes are propagated even when the config is changed during a wipe
          // segments->mode = mainsegment.mode;
          // segments->colors[0] = mainsegment.colors[0];
        } else {
          segments->setOption(SEG_OPTION_ON, 0, i);
        }
        // Always mark segments as "transitional", we are animating the staircase
        segments->setOption(SEG_OPTION_TRANSITIONAL, 1, i);
      }
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
    * The speed of sound is 343 meters per second at 20 degress Celcius.
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
            // If the bottom sensor triggered, we need to swipe up, ON
            swipe = bottomSensorRead;

            DEBUG_PRINT(F("ON -> Swipe "));
            DEBUG_PRINTLN(swipe ? F("up.") : F("down."));

            if (onIndex == offIndex) {
              // Position the indices for a correct on-swipe
              if (swipe == SWIPE_UP) {
                onIndex = mainSegmentId;
              } else {
                onIndex = maxSegmentId+1;
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
      if (on && ((millis() - lastSwitchTime) > on_time_ms)) {
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

        if (on) {
          // Turn on all segments
          onIndex = MAX(mainSegmentId, onIndex - 1);
          offIndex = MIN(maxSegmentId + 1, offIndex + 1);
        } else {
          if (swipe == SWIPE_UP) {
            onIndex = MIN(offIndex, onIndex + 1);
          } else {
            offIndex = MAX(onIndex, offIndex - 1);
          }
        }
        updateSegments();
      }
    }

    // send sesnor values to JSON API
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
      } else {
        // Restore segment options
        WS2812FX::Segment* segments = strip.getSegments();
        for (int i = 0; i < MAX_NUM_SEGMENTS; i++, segments++) {
          if (!segments->isActive()) {
            maxSegmentId = i - 1;
            break;
          }
          segments->setOption(SEG_OPTION_ON, 1, i);
        }
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
      checkSensors();
      autoPowerOff();
      updateSwipe();
    }

    uint16_t getId() { return USERMOD_ID_ANIMATED_STAIRCASE; }

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
      JsonObject staircase = root[FPSTR(_name)];
      if (!staircase.isNull()) {
        if (staircase[FPSTR(_enabled)].is<bool>()) {
          enabled   = staircase[FPSTR(_enabled)].as<bool>();
        } else {
          String str = staircase[FPSTR(_enabled)]; // checkbox -> off or on
          enabled = (bool)(str!="off"); // off is guaranteed to be present
        }
        readSensorsFromJson(staircase);
        DEBUG_PRINTLN(F("Staircase sensor state read from API."));
      }
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
      topMaxDist    = min(150,max(30,(int)topMaxDist));     // max distnace ~1.5m (a lag of 9ms may be expected)
      bottomMaxDist = top[FPSTR(_bottomEchoCm)] | bottomMaxDist;
      bottomMaxDist = min(150,max(30,(int)bottomMaxDist));  // max distance ~1.5m (a lag of 9ms may be expected)

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
      return true;
    }

    /*
    * Shows the delay between steps and power-off time in the "info"
    * tab of the web-UI.
    */
    void addToJsonInfo(JsonObject& root) {
      JsonObject staircase = root["u"];
      if (staircase.isNull()) {
        staircase = root.createNestedObject("u");
      }

      JsonArray usermodEnabled = staircase.createNestedArray(F("Staircase"));  // name
      String btn = F("<button class=\"btn infobtn\" onclick=\"requestJson({staircase:{enabled:");
      if (enabled) {
        btn += F("false}});\">");
        btn += F("enabled");
      } else {
        btn += F("true}});\">");
        btn += F("disabled");
      }
      btn += F("</button>");
      usermodEnabled.add(btn);                             // value
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
