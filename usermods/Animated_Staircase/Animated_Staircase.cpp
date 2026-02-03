/*
 * Usermod for detecting people entering/leaving a staircase and switching the
 * staircase on/off.
 *
 * Edit the Animated_Staircase_config.h file to compile this usermod for your
 * specific configuration.
 * 
 * See the accompanying README.md file for more info.
 */
#include "wled.h"
#include "Wire.h"
#include <VL53L0X.h>

class Animated_Staircase : public Usermod {
  private:

    /* configuration (available in API and stored in flash) */
    bool enabled = false;                   // Enable this usermod

    bool useV53l0x = false;                 // Use VL53L0X Laser Range Sensors
    // address we will assign if dual sensor is present
    #define LOXTOP_ADDRESS 0x41 // Default address should be 0x41
    #define LOXBOT_ADDRESS 0x42

    // set the pins to shutdown
    //#define SHT_LOX1 13 //Top D7
    //#define SHT_LOX2 12 //Bottom D6

    // objects for the vl53l0x
    VL53L0X loxBot; 
    VL53L0X loxTop;

    int16_t v53l0x_bottom_distance = -1;
    int16_t v53l0x_top_distance    = -1;
    int8_t topXSHUTPin             = -1;    // disabled
    int8_t botXSHUTPin             = -1;    // disabled
    bool topSensorFailed           = false;
    bool botSensorFailed           = false;
    unsigned long showDebugData    = 0;
    //VL53L0X_RangingMeasurementData_t measureBot;


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
    static const char _useV53l0x[];
    static const char _topXSHUT_pin[];
    static const char _botXSHUT_pin[];

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

    bool vl53l0xRead(bool top, unsigned int maxDist) {
        int16_t mm = 0;

        if (top) {
          v53l0x_top_distance = loxTop.readRangeContinuousMillimeters();
          if (loxTop.timeoutOccurred()) {
              v53l0x_top_distance = 999;
              return false;
          } else {
            mm = v53l0x_top_distance;
          }
        } else { 
          v53l0x_bottom_distance = loxBot.readRangeContinuousMillimeters();
          if (loxBot.timeoutOccurred()) {
              v53l0x_bottom_distance = 999;
              return false;
          } else {
            mm = v53l0x_bottom_distance;
          }
        }
        
        if (mm>20) {
          return (mm < maxDist ? true : false);
        } else {
          return false;
        }
        
    }

    bool checkSensors() {

      if (millis() -  showDebugData > 9999) {
        DEBUG_PRINTLN(F("---VL53L0X INFO---"));
        DEBUG_PRINT(F("Top Distance: "));     DEBUG_PRINTLN(v53l0x_top_distance);
        DEBUG_PRINT(F("Bot Distance: "));     DEBUG_PRINTLN(v53l0x_bottom_distance);
        showDebugData = millis();
      }

      bool sensorChanged = false;

      if ((millis() - lastScanTime) > scanDelay) {
        lastScanTime = millis();

        if (useV53l0x && (!(botSensorFailed)) ) {
          bottomSensorRead = bottomSensorWrite || vl53l0xRead(false, (bottomMaxDist * 10));
        } else {
          bottomSensorRead = bottomSensorWrite ||
          (!useUSSensorBottom ?
            (bottomPIRorTriggerPin<0 ? false : digitalRead(bottomPIRorTriggerPin)) :
            ultrasoundRead(bottomPIRorTriggerPin, bottomEchoPin, bottomMaxDist*59)  // cm to us
          );

        }
        
         if (useV53l0x && (!(topSensorFailed))) {
          topSensorRead = topSensorWrite || vl53l0xRead(true, (topMaxDist * 10));
         } else {
          topSensorRead = topSensorWrite ||
          (!useUSSensorTop ?
            (topPIRorTriggerPin<0 ? false : digitalRead(topPIRorTriggerPin)) :
            ultrasoundRead(topPIRorTriggerPin, topEchoPin, topMaxDist*59)   // cm to us
          );
        }

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

        if (!useV53l0x) {
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

    void setID() {
      // all reset
      digitalWrite(topXSHUTPin, LOW);    
      digitalWrite(botXSHUTPin, LOW);
      delay(10);
      // all unreset
      digitalWrite(topXSHUTPin, HIGH);
      digitalWrite(botXSHUTPin, HIGH);
      delay(10);

      digitalWrite(botXSHUTPin, LOW);
      delay(10);
      
      //Only one address needs to be set, other sensor will use default I2C address
      loxBot.setAddress(LOXBOT_ADDRESS);
      delay(10);
      digitalWrite(botXSHUTPin, HIGH);
      // initing LOX1
      if(!loxBot.init(false)) {
         DEBUG_PRINTLN(F("VL053L0X: Failed to boot Bottom VL53L0X"));
         botSensorFailed = true;
      }
      delay(10);

      // activating LOX2
     
      //initing LOX2
      if(!loxTop.init(false)) {
         DEBUG_PRINTLN(F("VL053L0X: Failed to boot Top VL53L0X"));
         topSensorFailed = true;
      }

      loxTop.setTimeout(500);
      loxBot.setTimeout(500);
      loxTop.startContinuous();
      loxBot.startContinuous();
    }

  public:
    void setup() {
      // standardize invalid pin numbers to -1
      if (topPIRorTriggerPin    < 0) topPIRorTriggerPin    = -1;
      if (topEchoPin            < 0) topEchoPin            = -1;
      if (bottomPIRorTriggerPin < 0) bottomPIRorTriggerPin = -1;
      if (bottomEchoPin         < 0) bottomEchoPin         = -1;

      //V53L0X
      if (botXSHUTPin           < 0) botXSHUTPin           = -1;
      if (topXSHUTPin           < 0) topXSHUTPin           = -1;

      // allocate pins
      PinManagerPinType pins[6] = {
        { topPIRorTriggerPin, useUSSensorTop },
        { topEchoPin, false },
        { bottomPIRorTriggerPin, useUSSensorBottom },
        { bottomEchoPin, false },
        { topXSHUTPin, false},
        { botXSHUTPin, false},
      };
      // NOTE: this *WILL* return TRUE if all the pins are set to -1.
      //       this is *BY DESIGN*.
      if (!PinManager::allocateMultiplePins(pins, 6, PinOwner::UM_AnimatedStaircase)) {
        topPIRorTriggerPin = -1;
        topEchoPin = -1;
        bottomPIRorTriggerPin = -1;
        bottomEchoPin = -1;
        enabled = false;
        //V53L0X
        topXSHUTPin = -1;
        botXSHUTPin = -1;
      }

      if (useV53l0x) {
        pinMode(topXSHUTPin, OUTPUT);
        pinMode(botXSHUTPin, OUTPUT);
        DEBUG_PRINTLN(F("VL053L0X: Shutdown pins inited..."));
        digitalWrite(topXSHUTPin, LOW);
        digitalWrite(botXSHUTPin, LOW);
        DEBUG_PRINTLN(F("VL053L0X: Both in reset mode...(pins are low)"));
        DEBUG_PRINTLN(F("VL053L0X: Starting..."));
        setID();
        showDebugData = millis();
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
      //oappend(F("dd=addDropdown('staircase','selectfield');"));
      //oappend(F("addOption(dd,'1st value',0);"));
      //oappend(F("addOption(dd,'2nd value',1);"));
      //oappend(F("addInfo('staircase:selectfield',1,'additional info');"));  // 0 is field type, 1 is actual field
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
      staircase[FPSTR(_useV53l0x)]                 = useV53l0x;
      staircase[FPSTR(_topXSHUT_pin)]              = topXSHUTPin;
      staircase[FPSTR(_botXSHUT_pin)]              = botXSHUTPin;
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

      //V53L0X
      bool oldUseV53l0x = useV53l0x;
      int8_t oldBotXSHUTPin = botXSHUTPin;
      int8_t oldTopXSHUTPin = topXSHUTPin;

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

      useV53l0x = top[FPSTR(_useV53l0x)] | useV53l0x;
      topXSHUTPin = top[FPSTR(_topXSHUT_pin)] | topXSHUTPin;
      botXSHUTPin = top[FPSTR(_botXSHUT_pin)] | botXSHUTPin;

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
            (oldBottomBPin != bottomEchoPin) ||
            (oldUseV53l0x != useV53l0x) ||
            (oldBotXSHUTPin != botXSHUTPin) ||
            (oldTopXSHUTPin != topXSHUTPin)) {
          changed = true;
          PinManager::deallocatePin(oldTopAPin, PinOwner::UM_AnimatedStaircase);
          PinManager::deallocatePin(oldTopBPin, PinOwner::UM_AnimatedStaircase);
          PinManager::deallocatePin(oldBottomAPin, PinOwner::UM_AnimatedStaircase);
          PinManager::deallocatePin(oldBottomBPin, PinOwner::UM_AnimatedStaircase);
          PinManager::deallocatePin(oldBotXSHUTPin, PinOwner::UM_AnimatedStaircase);
          PinManager::deallocatePin(oldTopXSHUTPin, PinOwner::UM_AnimatedStaircase);
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

      if (useV53l0x) {
         JsonArray topRange = user.createNestedArray(F("V53L0X Top Range"));  
         JsonArray botRange = user.createNestedArray(F("V53L0X Bottom Range"));
         if (topSensorFailed) {
            topRange.add(F("Sensor Failed!"));
         } else {
            topRange.add(v53l0x_top_distance);
         } 
         if (botSensorFailed) {
          botRange.add(F("Bottom Sensor Failed!"));
         } else {
          botRange.add(v53l0x_bottom_distance);
         }
          
      }
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
const char Animated_Staircase::_useV53l0x[]                 PROGMEM = "useV53l0x";
const char Animated_Staircase::_topXSHUT_pin[]              PROGMEM = "topXSHUTPin";
const char Animated_Staircase::_botXSHUT_pin[]              PROGMEM = "botXSHUTPin";


static Animated_Staircase animated_staircase;
REGISTER_USERMOD(animated_staircase);