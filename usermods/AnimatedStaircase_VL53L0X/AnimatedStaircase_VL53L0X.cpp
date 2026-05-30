#include "wled.h"
#include <VL53L0X.h>

class AnimatedStaircase_VL53L0X : public Usermod {
  TaskHandle_t taskHandle = nullptr;
  volatile bool loopRequested = true;
  volatile bool pendingColorUpdated = false;

private:
  bool enabled = false;
  bool configLoaded = false;
  bool sensorsInitialised = false;

  unsigned long segment_delay_ms = 150;
  unsigned long on_time_ms = 30000;

  uint16_t bottomThresholdMM = 900;
  uint16_t topThresholdMM = 900;

  int8_t xshutTopPin = -1;
  int8_t xshutBottomPin = -1;

  VL53L0X topVL53;
  VL53L0X bottomVL53;

  bool initDone = false;
  static constexpr uint16_t VL53L0X_MAX_DISTANCE_MM = 2000;
  const unsigned int scanDelay = 250;
  bool on = false;

#define SWIPE_UP true
#define SWIPE_DOWN false
  bool swipe = SWIPE_UP;

#define LOWER false
#define UPPER true
  bool lastSensor = LOWER;

  unsigned long lastTime = 0;
  unsigned long lastScanTime = 0;
  unsigned long lastSwitchTime = 0;

  byte onIndex = 0;
  byte offIndex = 0;
  byte maxSegmentId = 1;
  byte minSegmentId = 0;

  bool topSensorRead = false;
  bool topSensorWrite = false;
  bool bottomSensorRead = false;
  bool bottomSensorWrite = false;
  bool topSensorState = false;
  bool bottomSensorState = false;

  bool togglePower = false;

  static const char _name[];
  static const char _enabled[];
  static const char _segmentDelay[];
  static const char _onTime[];
  static const char _togglePower[];
  static const char _xshutTopPin[];
  static const char _xshutBottomPin[];
  static const char _topThreshold[];
  static const char _bottomThreshold[];

  void publishMqtt(bool bottom, const char* state) {
#ifndef WLED_DISABLE_MQTT
    if (WLED_MQTT_CONNECTED) {
      char subuf[64];
      sprintf_P(subuf, PSTR("%s/motion/%d"), mqttDeviceTopic, (int)bottom);
      mqtt->publish(subuf, 0, false, state);
    }
#endif
  }

#ifndef WLED_DISABLE_MQTT
  bool onMqttMessage(char* topic, char* payload) {
    if (strlen(topic) == 6 && strncmp_P(topic, PSTR("/swipe"), 6) == 0) {
      String action = payload;

      if (action == "up") {
        bottomSensorWrite = true;
        return true;
      }

      if (action == "down") {
        topSensorWrite = true;
        return true;
      }

      if (action == "on") {
        enable(true);
        return true;
      }

      if (action == "off") {
        enable(false);
        return true;
      }
    }

    return false;
  }

  void onMqttConnect(bool sessionPresent) {
    char subuf[64];

    if (mqttDeviceTopic[0] != 0) {
      strcpy(subuf, mqttDeviceTopic);
      strcat_P(subuf, PSTR("/swipe"));
      mqtt->subscribe(subuf, 0);
    }
  }
#endif

  void resetRuntimeState() {
    on = false;
    swipe = SWIPE_UP;
    lastSensor = LOWER;
    lastTime = 0;
    lastScanTime = 0;
    lastSwitchTime = 0;
    onIndex = 0;
    offIndex = 0;
    minSegmentId = 0;
    maxSegmentId = 1;
  }

  void resetSensorState() {
    sensorsInitialised = false;
    initDone = false;

    topSensorRead = false;
    topSensorWrite = false;
    topSensorState = false;

    bottomSensorRead = false;
    bottomSensorWrite = false;
    bottomSensorState = false;
  }

  uint16_t readThreshold(JsonObject& top, const __FlashStringHelper* key, uint16_t currentValue) {
    JsonVariant value = top[key];
    if (value.isNull()) return currentValue;

    if (!value.is<long>()) return currentValue;

    long threshold = value.as<long>();
    return constrain(threshold, 0L, (long)VL53L0X_MAX_DISTANCE_MM);
  }

  bool initialiseSensors() {
    if (!configLoaded) {
      DEBUG_PRINTLN(F("[StaircaseVL53] Config not loaded yet"));
      return false;
    }

    if (xshutTopPin < 0 || xshutBottomPin < 0) {
      DEBUG_PRINTLN(F("[StaircaseVL53] XSHUT pins not configured"));
      return false;
    }

    if (xshutTopPin == xshutBottomPin) {
      DEBUG_PRINTLN(F("[StaircaseVL53] XSHUT pins must be different"));
      return false;
    }

    PinManagerPinType pins[] = {
      { xshutTopPin, true },
      { xshutBottomPin, true },
    };

    if (!PinManager::allocateMultiplePins(pins, 2, PinOwner::UM_AnimatedStaircase)) {
      DEBUG_PRINTLN(F("[StaircaseVL53] Failed to allocate XSHUT pins"));

      PinManager::deallocatePin(xshutTopPin, PinOwner::UM_AnimatedStaircase);
      PinManager::deallocatePin(xshutBottomPin, PinOwner::UM_AnimatedStaircase);

      resetSensorState();
      enabled = false;
      return false;
    }

    static bool wireStarted = false;
    if (!wireStarted) {
      Wire.begin();
      wireStarted = true;
    }

    pinMode(xshutTopPin, OUTPUT);
    pinMode(xshutBottomPin, OUTPUT);

    digitalWrite(xshutTopPin, LOW);
    digitalWrite(xshutBottomPin, LOW);
    delay(20);

    bottomVL53.setTimeout(50);
    topVL53.setTimeout(50);

    digitalWrite(xshutBottomPin, HIGH);
    delay(20);

    if (!bottomVL53.init()) {
      DEBUG_PRINTLN(F("[StaircaseVL53] Bottom VL53L0X init failed"));
      resetSensorState();
      return false;
    }

    bottomVL53.setAddress(0x30);
    bottomVL53.startContinuous();

    digitalWrite(xshutTopPin, HIGH);
    delay(20);

    if (!topVL53.init()) {
      DEBUG_PRINTLN(F("[StaircaseVL53] Top VL53L0X init failed"));
      resetSensorState();
      return false;
    }

    topVL53.setAddress(0x31);
    topVL53.startContinuous();

    sensorsInitialised = true;
    initDone = true;

    DEBUG_PRINTF(
      "[StaircaseVL53] Sensors initialised top=%d bottom=%d\n",
      xshutTopPin,
      xshutBottomPin
    );

    return true;
  }

  void ensureTask() {
    if (taskHandle != nullptr) return;

<<<<<<< HEAD
    xTaskCreate(
=======
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
>>>>>>> origin/codex/fix-thread-safety-in-animatedstaircase-usermod
      [](void* param) {
        const TickType_t xFrequency = 250 / portTICK_PERIOD_MS;
        TickType_t xLastWakeTime = xTaskGetTickCount();

        auto* self = static_cast<AnimatedStaircase_VL53L0X*>(param);

        while (true) {
          vTaskDelayUntil(&xLastWakeTime, xFrequency);
          if (self) self->loopRequested = true;
        }
      },
      "StairVL53Task",
      4096,
      this,
      1,
      &taskHandle
    );

    if (taskCreated != pdPASS) {
      DEBUG_PRINTLN(F("[StaircaseVL53] Failed to create background task"));
      taskHandle = nullptr;
      enabled = false;
    }
  }

  void updateSegments() {
    uint8_t lastSeg = strip.getLastActiveSegmentId();

    for (int i = minSegmentId; i <= lastSeg && i < maxSegmentId; i++) {
      Segment& seg = strip.getSegment(i);
      if (!seg.isActive()) continue;

      seg.setOption(SEG_OPTION_ON, i >= onIndex && i < offIndex);
    }

    strip.trigger();
    stateChanged = true;
    pendingColorUpdated = true;
  }

  bool checkSensors() {
    bool sensorChanged = false;

    if ((millis() - lastScanTime) < scanDelay) return false;
    lastScanTime = millis();

    uint16_t bottomReading = bottomVL53.readRangeContinuousMillimeters();
    uint16_t topReading = topVL53.readRangeContinuousMillimeters();

    if (bottomVL53.timeoutOccurred()) return false;
    if (topVL53.timeoutOccurred()) return false;

    bottomSensorRead = bottomSensorWrite || (bottomReading < bottomThresholdMM);
    topSensorRead = topSensorWrite || (topReading < topThresholdMM);

    if (bottomSensorRead != bottomSensorState) {
      bottomSensorState = bottomSensorRead;
      sensorChanged = true;
      publishMqtt(true, bottomSensorState ? "on" : "off");
    }

    if (topSensorRead != topSensorState) {
      topSensorState = topSensorRead;
      sensorChanged = true;
      publishMqtt(false, topSensorState ? "on" : "off");
    }

    topSensorWrite = false;
    bottomSensorWrite = false;

    if (topSensorRead != bottomSensorRead) {
      lastSwitchTime = millis();

      if (on) {
        lastSensor = topSensorRead;
      } else {
        if (togglePower && onIndex == offIndex && offMode) toggleOnOff();

        swipe = bottomSensorRead;

        if (onIndex == offIndex) {
          if (swipe == SWIPE_UP) {
            onIndex = minSegmentId;
            offIndex = minSegmentId;
          } else {
            onIndex = maxSegmentId;
            offIndex = maxSegmentId;
          }
        }

        on = true;
      }
    }

    return sensorChanged;
  }

  void autoPowerOff() {
    if ((millis() - lastSwitchTime) > on_time_ms) {
      if (bottomSensorState || topSensorState) return;

      swipe = lastSensor;
      on = false;
    }
  }

  void updateSwipe() {
    if ((millis() - lastTime) > segment_delay_ms) {
      lastTime = millis();

      byte oldOn = onIndex;
      byte oldOff = offIndex;

      if (on) {
        if (swipe == SWIPE_UP) {
          offIndex = MIN(maxSegmentId, offIndex + 1);
        } else {
          onIndex = MAX(minSegmentId, onIndex - 1);
        }
      } else {
        if (swipe == SWIPE_UP) {
          onIndex = MIN(offIndex, onIndex + 1);
        } else {
          offIndex = MAX(onIndex, offIndex - 1);
        }
      }

      if (oldOn != onIndex || oldOff != offIndex) {
        updateSegments();

        if (togglePower && onIndex == offIndex && !offMode && !on) {
          toggleOnOff();
        }
      }
    }
  }

  void enable(bool enable) {
    enabled = enable;
    resetRuntimeState();
    resetSensorState();

    if (!configLoaded) return;

    if (enable && !sensorsInitialised) {
      initialiseSensors();
    }

    if (strip.getMaxSegments() == 0 || strip.getSegment(0).stop == 0) {
      DEBUG_PRINTLN(F("[StaircaseVL53] Segments not initialized yet"));
      return;
    }

    if (enable) {
      onIndex = minSegmentId = strip.getMainSegmentId();
      offIndex = maxSegmentId = strip.getLastActiveSegmentId() + 1;

      strip.setTransition(segment_delay_ms);
      strip.trigger();
    } else {
      for (int i = 0; i <= strip.getLastActiveSegmentId(); i++) {
        Segment& seg = strip.getSegment(i);
        if (seg.stop == 0) continue;

        seg.setOption(SEG_OPTION_ON, true);
      }

      strip.trigger();
      stateChanged = true;
      pendingColorUpdated = true;
    }
  }

public:
  void setup() {
    DEBUG_PRINTF(
      "[StaircaseVL53] Setup called configLoaded=%d enabled=%d top=%d bottom=%d\n",
      configLoaded,
      enabled,
      xshutTopPin,
      xshutBottomPin
    );

    if (!configLoaded) return;

    if (enabled && !sensorsInitialised) {
      initialiseSensors();
    }

    ensureTask();
  }

  void loop() {
    if (pendingColorUpdated) {
      pendingColorUpdated = false;
      colorUpdated(CALL_MODE_DIRECT_CHANGE);
    }

    if (!loopRequested) return;
    loopRequested = false;

    if (!enabled || !configLoaded) return;

    if (!sensorsInitialised) {
      initialiseSensors();
      return;
    }

    if (!initDone || strip.getMaxSegments() == 0) return;
    if (strip.getSegment(0).stop == 0) return;

    minSegmentId = strip.getMainSegmentId();
    maxSegmentId = strip.getLastActiveSegmentId() + 1;

    checkSensors();

    if (on) autoPowerOff();

    updateSwipe();
  }

  void cleanup() {
    if (taskHandle != nullptr) {
      vTaskDelete(taskHandle);
      taskHandle = nullptr;
    }

    if (xshutTopPin >= 0) {
      digitalWrite(xshutTopPin, LOW);
      PinManager::deallocatePin(xshutTopPin, PinOwner::UM_AnimatedStaircase);
    }

    if (xshutBottomPin >= 0) {
      digitalWrite(xshutBottomPin, LOW);
      PinManager::deallocatePin(xshutBottomPin, PinOwner::UM_AnimatedStaircase);
    }

    resetSensorState();
  }

  uint16_t getId() {
    return USERMOD_ID_ANIMATED_STAIRCASE;
  }

  void addToConfig(JsonObject& root) {
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) top = root.createNestedObject(FPSTR(_name));

    top[FPSTR(_enabled)] = enabled;
    top[FPSTR(_segmentDelay)] = segment_delay_ms;
    top[FPSTR(_onTime)] = on_time_ms / 1000;
    top[FPSTR(_togglePower)] = togglePower;
    top[FPSTR(_xshutTopPin)] = xshutTopPin;
    top[FPSTR(_xshutBottomPin)] = xshutBottomPin;
    top[FPSTR(_topThreshold)] = topThresholdMM;
    top[FPSTR(_bottomThreshold)] = bottomThresholdMM;
  }

  bool readFromConfig(JsonObject& root) {
    JsonObject top = root[FPSTR(_name)];

    if (top.isNull()) {
      configLoaded = true;
      return false;
    }

    int8_t oldXshutTop = xshutTopPin;
    int8_t oldXshutBottom = xshutBottomPin;

    enabled = top[FPSTR(_enabled)] | enabled;
    segment_delay_ms = constrain(top[FPSTR(_segmentDelay)] | segment_delay_ms, 10UL, 10000UL);
    on_time_ms = constrain(top[FPSTR(_onTime)] | (on_time_ms / 1000), 10, 900) * 1000;
    togglePower = top[FPSTR(_togglePower)] | togglePower;
    xshutTopPin = top[FPSTR(_xshutTopPin)] | xshutTopPin;
    xshutBottomPin = top[FPSTR(_xshutBottomPin)] | xshutBottomPin;
    topThresholdMM = readThreshold(top, FPSTR(_topThreshold), topThresholdMM);
    bottomThresholdMM = readThreshold(top, FPSTR(_bottomThreshold), bottomThresholdMM);

    configLoaded = true;

    DEBUG_PRINTF(
      "[StaircaseVL53] Loaded config enabled=%d top=%d bottom=%d\n",
      enabled,
      xshutTopPin,
      xshutBottomPin
    );

    bool pinsChanged =
      oldXshutTop != xshutTopPin ||
      oldXshutBottom != xshutBottomPin;

    if (pinsChanged) {
      if (oldXshutTop >= 0) {
        digitalWrite(oldXshutTop, LOW);
        PinManager::deallocatePin(oldXshutTop, PinOwner::UM_AnimatedStaircase);
      }

      if (oldXshutBottom >= 0) {
        digitalWrite(oldXshutBottom, LOW);
        PinManager::deallocatePin(oldXshutBottom, PinOwner::UM_AnimatedStaircase);
      }

      resetSensorState();
    }

    setup();

    return true;
  }

  void addToJsonState(JsonObject& root) {
    JsonObject staircase = root[FPSTR(_name)];
    if (staircase.isNull()) staircase = root.createNestedObject(FPSTR(_name));

    staircase[F("top-sensor")] = topSensorRead;
    staircase[F("bottom-sensor")] = bottomSensorRead;
  }

  void readFromJsonState(JsonObject& root) {
    JsonObject staircase = root[FPSTR(_name)];

    if (!staircase.isNull()) {
      if (staircase[FPSTR(_enabled)].is<bool>()) {
        enable(staircase[FPSTR(_enabled)]);
      }

      if (staircase[F("top-sensor")]) topSensorWrite = true;
      if (staircase[F("bottom-sensor")]) bottomSensorWrite = true;
    }
  }

  void appendConfigData() {
  }

  void addToJsonInfo(JsonObject& root) {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray infoArr = user.createNestedArray(FPSTR(_name));

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

const char AnimatedStaircase_VL53L0X::_name[] PROGMEM = "staircase-vl53";
const char AnimatedStaircase_VL53L0X::_enabled[] PROGMEM = "enabled";
const char AnimatedStaircase_VL53L0X::_segmentDelay[] PROGMEM = "segment-delay-ms";
const char AnimatedStaircase_VL53L0X::_onTime[] PROGMEM = "on-time-s";
const char AnimatedStaircase_VL53L0X::_togglePower[] PROGMEM = "toggle-on-off";
const char AnimatedStaircase_VL53L0X::_xshutTopPin[] PROGMEM = "xshut-top-pin";
const char AnimatedStaircase_VL53L0X::_xshutBottomPin[] PROGMEM = "xshut-bottom-pin";
const char AnimatedStaircase_VL53L0X::_topThreshold[] PROGMEM = "top-threshold-mm";
const char AnimatedStaircase_VL53L0X::_bottomThreshold[] PROGMEM = "bottom-threshold-mm";

static AnimatedStaircase_VL53L0X animated_staircase_vl53;

REGISTER_USERMOD(animated_staircase_vl53);