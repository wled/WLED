#pragma once

#include "wled.h"

// Default pin map for ESP8266 D1 mini:
// CLK=D1(GPIO5), DT=D2(GPIO4), SW=D5(GPIO14), FAN PWM=D6(GPIO12)
#ifndef REFP_DEFAULT_CLK_PIN
  #define REFP_DEFAULT_CLK_PIN 5
#endif

#ifndef REFP_DEFAULT_DT_PIN
  #define REFP_DEFAULT_DT_PIN 4
#endif

#ifndef REFP_DEFAULT_SW_PIN
  #define REFP_DEFAULT_SW_PIN 14
#endif

#ifndef REFP_DEFAULT_PWM_PIN
  #define REFP_DEFAULT_PWM_PIN 12
#endif

class RotaryEncoderFanPwmPresetUsermod : public Usermod {
  private:
    bool initDone = false;
    bool enabled = true;
    bool fanOn = true;

    int8_t pinClk = REFP_DEFAULT_CLK_PIN;
    int8_t pinDt  = REFP_DEFAULT_DT_PIN;
    int8_t pinSw  = REFP_DEFAULT_SW_PIN;
    int8_t pinPwm = REFP_DEFAULT_PWM_PIN;

    uint8_t dutyPercent = 50;
    uint8_t stepPercent = 5;
    uint8_t pulsesPerStep = 4;
    bool invertEncoder = false;
    uint16_t pwmFrequency = 25000;
    uint16_t buttonDebounceMs = 40;

    unsigned long lastPollMs = 0;
    unsigned long lastButtonEdgeMs = 0;
    unsigned long buttonPressStartMs = 0;

    uint8_t lastAB = 0;
    int16_t encoderAccumulator = 0;
    bool lastButtonPressed = false;
    bool longPressHandled = false;
    byte lastPresetRequested = 0;

    static const uint16_t BUTTON_LONG_PRESS_MS = 3000;

    static const char _name[];
    static const char _enabled[];
    static const char _clkPin[];
    static const char _dtPin[];
    static const char _swPin[];
    static const char _pwmPin[];
    static const char _dutyPercent[];
    static const char _stepPercent[];
    static const char _pulsesPerStep[];
    static const char _invertEncoder[];
    static const char _pwmFrequency[];
    static const char _buttonDebounceMs[];
    static const char _on[];
    static const char _speed[];
  #ifndef WLED_DISABLE_MQTT
    bool haDiscoveryEnabled = true;
    bool haDiscoveryPublished = false;
    int16_t lastPublishedSpeed = -1;
    int8_t lastPublishedOn = -1;
  #endif

    static int8_t decodeTransition(uint8_t previousAB, uint8_t currentAB) {
      static const int8_t transitionTable[16] = {
         0, -1,  1,  0,
         1,  0,  0, -1,
        -1,  0,  0,  1,
         0,  1, -1,  0
      };
      return transitionTable[(previousAB << 2) | currentAB];
    }

    inline uint8_t readAB() const {
      uint8_t a = digitalRead(pinClk) ? 1 : 0;
      uint8_t b = digitalRead(pinDt)  ? 1 : 0;
      return (a << 1) | b;
    }

    void applyPwmDuty() {
      if (pinPwm < 0) return;
      uint8_t duty = (enabled && fanOn) ? (uint8_t)map(dutyPercent, 0, 100, 0, 255) : 0;
      analogWrite(pinPwm, duty);
#ifndef WLED_DISABLE_MQTT
      publishMqttState(false);
#endif
    }

#ifndef WLED_DISABLE_MQTT
    void publishMqttState(bool force) {
      if (!WLED_MQTT_CONNECTED) return;

      char topic[128];
      int8_t onVal = fanOn ? 1 : 0;
      if (force || lastPublishedOn != onVal) {
        snprintf_P(topic, 127, PSTR("%s/RotaryFanPWM/state"), mqttDeviceTopic);
        mqtt->publish(topic, 0, true, fanOn ? "ON" : "OFF");
        lastPublishedOn = onVal;
      }

      int16_t speedVal = dutyPercent;
      if (force || lastPublishedSpeed != speedVal) {
        snprintf_P(topic, 127, PSTR("%s/RotaryFanPWM/speed"), mqttDeviceTopic);
        mqtt->publish(topic, 0, true, String(speedVal).c_str());
        lastPublishedSpeed = speedVal;
      }
    }

    void publishHomeAssistantAutodiscovery() {
      if (!WLED_MQTT_CONNECTED || !haDiscoveryEnabled) return;

      StaticJsonDocument<1024> json;
      char topic[128];
      char uid[64];

      snprintf_P(uid, 63, PSTR("%s_rotaryfan"), escapedMac.c_str());
      json[F("name")] = F("WLED Rotary Fan");
      json[F("uniq_id")] = uid;

      snprintf_P(topic, 127, PSTR("%s/RotaryFanPWM/state"), mqttDeviceTopic);
      json[F("stat_t")] = topic;
      snprintf_P(topic, 127, PSTR("%s/RotaryFanPWM/set"), mqttDeviceTopic);
      json[F("cmd_t")] = topic;

      snprintf_P(topic, 127, PSTR("%s/RotaryFanPWM/speed"), mqttDeviceTopic);
      json[F("pct_stat_t")] = topic;
      snprintf_P(topic, 127, PSTR("%s/RotaryFanPWM/speed/set"), mqttDeviceTopic);
      json[F("pct_cmd_t")] = topic;
      json[F("pct_val_tpl")] = F("{{ value | int }}");
      json[F("pl_on")] = F("ON");
      json[F("pl_off")] = F("OFF");
      json[F("pl_avail")] = F("online");
      json[F("pl_not_avail")] = F("offline");
      json[F("ret")] = true;

      snprintf_P(topic, 127, PSTR("%s/status"), mqttDeviceTopic);
      json[F("avty_t")] = topic;

      JsonObject dev = json.createNestedObject(F("device"));
      dev[F("name")] = serverDescription;
      dev[F("identifiers")] = String(F("wled-")) + escapedMac;
      dev[F("manufacturer")] = F(WLED_BRAND);
      dev[F("model")] = F(WLED_PRODUCT_NAME);
      dev[F("sw_version")] = versionString;

      String payload;
      serializeJson(json, payload);

      snprintf_P(topic, 127, PSTR("homeassistant/fan/%s/config"), uid);
      mqtt->publish(topic, 0, true, payload.c_str());
      haDiscoveryPublished = true;
    }
#endif

    void updateDutyByStep(int8_t direction) {
      int16_t newDuty = dutyPercent + (direction > 0 ? stepPercent : -stepPercent);
      dutyPercent = (uint8_t)constrain(newDuty, 0, 100);
      fanOn = true;
      applyPwmDuty();
    }

    void cycleToNextPreset() {
      // Cycle through existing presets only, wrapping to the lowest existing slot.
      byte start = lastPresetRequested;
      if (start < 1 || start > 250) start = currentPreset;
      if (start < 1 || start > 250) start = presetCycCurr;
      if (start < 1 || start > 250) start = 0;

      if (!requestJSONBufferLock(23)) return;
      for (byte offset = 1; offset <= 250; offset++) {
        byte next = (byte)(((start + offset - 1) % 250) + 1);
        if (readObjectFromFileUsingId(getPresetsFileName(), next, pDoc)) {
          JsonObject candidate = pDoc->as<JsonObject>();
          if (candidate.size() == 0) continue; // deleted/empty slot

          releaseJSONBufferLock();
          lastPresetRequested = next;
          applyPreset(next, CALL_MODE_BUTTON_PRESET);
          return;
        }
      }

      releaseJSONBufferLock();
    }

    void pollEncoder() {
      if (pinClk < 0 || pinDt < 0) return;
      uint8_t currentAB = readAB();
      if (currentAB == lastAB) return;

      int8_t delta = decodeTransition(lastAB, currentAB);
      if (invertEncoder) delta = -delta;
      encoderAccumulator += delta;
      lastAB = currentAB;

      while (encoderAccumulator >= pulsesPerStep) {
        encoderAccumulator -= pulsesPerStep;
        updateDutyByStep(1);
      }
      while (encoderAccumulator <= -(int16_t)pulsesPerStep) {
        encoderAccumulator += pulsesPerStep;
        updateDutyByStep(-1);
      }
    }

    void pollButton() {
      if (pinSw < 0) return;
      bool pressed = (digitalRead(pinSw) == LOW);

      if (pressed != lastButtonPressed && (millis() - lastButtonEdgeMs) >= buttonDebounceMs) {
        lastButtonEdgeMs = millis();
        lastButtonPressed = pressed;

        if (pressed) {
          buttonPressStartMs = millis();
          longPressHandled = false;
        } else {
          if (!longPressHandled) cycleToNextPreset();
        }
      }

      if (pressed && !longPressHandled && (millis() - buttonPressStartMs) >= BUTTON_LONG_PRESS_MS) {
        bool lightsAreOn = (bri > 0);
        bool fanIsOn = (fanOn && dutyPercent > 0);
        bool turnOn = !(lightsAreOn || fanIsOn);

        fanOn = turnOn;
        applyPwmDuty();

        if ((bri > 0) != turnOn) {
          toggleOnOff();
          stateUpdated(CALL_MODE_BUTTON);
        } else {
          stateUpdated(CALL_MODE_BUTTON);
        }

        longPressHandled = true;
      }
    }

    void initPins() {
      if (pinClk < 0 || !PinManager::allocatePin(pinClk, false, PinOwner::UM_Unspecified)) pinClk = -1;
      if (pinDt  < 0 || !PinManager::allocatePin(pinDt,  false, PinOwner::UM_Unspecified)) pinDt  = -1;
      if (pinSw  < 0 || !PinManager::allocatePin(pinSw,  false, PinOwner::UM_Unspecified)) pinSw  = -1;
      if (pinPwm < 0 || !PinManager::allocatePin(pinPwm, true,  PinOwner::UM_Unspecified)) pinPwm = -1;

      if (pinClk >= 0) pinMode(pinClk, INPUT_PULLUP);
      if (pinDt  >= 0) pinMode(pinDt,  INPUT_PULLUP);
      if (pinSw  >= 0) pinMode(pinSw,  INPUT_PULLUP);

      if (pinPwm >= 0) {
        analogWriteRange(255);
        analogWriteFreq(pwmFrequency);
        applyPwmDuty();
      }

      if (pinClk >= 0 && pinDt >= 0) {
        lastAB = readAB();
      }
      if (pinSw >= 0) {
        lastButtonPressed = (digitalRead(pinSw) == LOW);
      }
    }

    void deinitPins() {
      if (pinClk >= 0) PinManager::deallocatePin(pinClk, PinOwner::UM_Unspecified);
      if (pinDt  >= 0) PinManager::deallocatePin(pinDt,  PinOwner::UM_Unspecified);
      if (pinSw  >= 0) PinManager::deallocatePin(pinSw,  PinOwner::UM_Unspecified);
      if (pinPwm >= 0) {
        analogWrite(pinPwm, 0);
        PinManager::deallocatePin(pinPwm, PinOwner::UM_Unspecified);
      }
    }

  public:
    void setup() override {
      initPins();
      initDone = true;
    }

    void loop() override {
      if (!enabled || strip.isUpdating()) return;

      unsigned long now = millis();
      if ((now - lastPollMs) < 2) return;
      lastPollMs = now;

      pollEncoder();
      pollButton();
#ifndef WLED_DISABLE_MQTT
      publishMqttState(false);
#endif
    }

#ifndef WLED_DISABLE_MQTT
    bool onMqttMessage(char* topic, char* payload) override {
      if (!WLED_MQTT_CONNECTED || topic == nullptr || payload == nullptr) return false;

      // WLED passes topic stripped to the device/group suffix (e.g. "/RotaryFanPWM/set").
      if (strcmp_P(topic, PSTR("/RotaryFanPWM/set")) == 0) {
        bool turnOn = (payload[0] == '1' || strcasecmp(payload, "ON") == 0 || strcasecmp(payload, "TRUE") == 0);
        fanOn = turnOn;
        applyPwmDuty();
        return true;
      }

      if (strcmp_P(topic, PSTR("/RotaryFanPWM/speed/set")) == 0) {
        int spd = atoi(payload);
        dutyPercent = (uint8_t)constrain(spd, 0, 100);
        fanOn = (dutyPercent > 0);
        applyPwmDuty();
        return true;
      }

      return false;
    }

    void onMqttConnect(bool sessionPresent) override {
      if (!WLED_MQTT_CONNECTED || mqttDeviceTopic[0] == 0) return;

      char topic[128];
      snprintf_P(topic, 127, PSTR("%s/RotaryFanPWM/set"), mqttDeviceTopic);
      mqtt->subscribe(topic, 0);
      snprintf_P(topic, 127, PSTR("%s/RotaryFanPWM/speed/set"), mqttDeviceTopic);
      mqtt->subscribe(topic, 0);

      if (haDiscoveryEnabled) publishHomeAssistantAutodiscovery();
      publishMqttState(true);
    }
#endif

    void addToJsonInfo(JsonObject& root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray fanDuty = user.createNestedArray(FPSTR(_name));
      fanDuty.add(dutyPercent);
      fanDuty.add(F("%"));

      JsonArray onCtl = user.createNestedArray(F("Fan"));
      String onButton = F("<button class=\"btn btn-xs\" onclick=\"requestJson({'");
      onButton += FPSTR(_name);
      onButton += F("':{'");
      onButton += FPSTR(_on);
      onButton += F("':");
      onButton += fanOn ? "false" : "true";
      onButton += F("}});\"><i class=\"icons ");
      onButton += fanOn ? "on" : "off";
      onButton += F("\">&#xe08f;</i></button>");
      onCtl.add(onButton);

      JsonArray speedCtl = user.createNestedArray(F("Fan Speed"));
      String speedSlider = F("<div class=\"slider\"><div class=\"sliderwrap il\"><input class=\"noslide\" onchange=\"requestJson({'");
      speedSlider += FPSTR(_name);
      speedSlider += F("':{'");
      speedSlider += FPSTR(_speed);
      speedSlider += F("':parseInt(this.value),'");
      speedSlider += FPSTR(_on);
      speedSlider += F("':true}});\" oninput=\"updateTrail(this);\" max=100 min=0 type=\"range\" value=");
      speedSlider += dutyPercent;
      speedSlider += F(" /><div class=\"sliderdisplay\"></div></div></div>");
      speedCtl.add(speedSlider);
    }

    void addToJsonState(JsonObject& root) override {
      if (!initDone || !enabled) return;

      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_name));

      usermod[FPSTR(_on)] = fanOn;
      usermod[FPSTR(_speed)] = dutyPercent;
    }

    void readFromJsonState(JsonObject& root) override {
      if (!initDone) return;

      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull()) return;

      if (usermod[FPSTR(_on)].is<bool>()) {
        fanOn = usermod[FPSTR(_on)].as<bool>();
      }

      if (usermod[FPSTR(_speed)].is<int>()) {
        dutyPercent = (uint8_t)constrain(usermod[FPSTR(_speed)].as<int>(), 0, 100);
        if (dutyPercent > 0) fanOn = true;
      }

      applyPwmDuty();
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)]          = enabled;
      top[FPSTR(_clkPin)]           = pinClk;
      top[FPSTR(_dtPin)]            = pinDt;
      top[FPSTR(_swPin)]            = pinSw;
      top[FPSTR(_pwmPin)]           = pinPwm;
      top[FPSTR(_dutyPercent)]      = dutyPercent;
      top[FPSTR(_stepPercent)]      = stepPercent;
      top[FPSTR(_pulsesPerStep)]    = pulsesPerStep;
      top[FPSTR(_invertEncoder)]    = invertEncoder;
      top[FPSTR(_pwmFrequency)]     = pwmFrequency;
      top[FPSTR(_buttonDebounceMs)] = buttonDebounceMs;
      top[FPSTR(_on)]               = fanOn;
    #ifndef WLED_DISABLE_MQTT
      top[F("ha-discovery")]       = haDiscoveryEnabled;
    #endif
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) return false;

      int8_t newClk = pinClk;
      int8_t newDt = pinDt;
      int8_t newSw = pinSw;
      int8_t newPwm = pinPwm;
      uint16_t newPwmFrequency = pwmFrequency;

      bool configComplete = true;
      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_clkPin)], newClk);
      configComplete &= getJsonValue(top[FPSTR(_dtPin)], newDt);
      configComplete &= getJsonValue(top[FPSTR(_swPin)], newSw);
      configComplete &= getJsonValue(top[FPSTR(_pwmPin)], newPwm);
      configComplete &= getJsonValue(top[FPSTR(_dutyPercent)], dutyPercent);
      configComplete &= getJsonValue(top[FPSTR(_stepPercent)], stepPercent);
      configComplete &= getJsonValue(top[FPSTR(_pulsesPerStep)], pulsesPerStep);
      configComplete &= getJsonValue(top[FPSTR(_invertEncoder)], invertEncoder);
      configComplete &= getJsonValue(top[FPSTR(_pwmFrequency)], newPwmFrequency);
      configComplete &= getJsonValue(top[FPSTR(_buttonDebounceMs)], buttonDebounceMs);
      configComplete &= getJsonValue(top[FPSTR(_on)], fanOn);
    #ifndef WLED_DISABLE_MQTT
      configComplete &= getJsonValue(top[F("ha-discovery")], haDiscoveryEnabled);
    #endif

      dutyPercent = (uint8_t)constrain(dutyPercent, 0, 100);
      stepPercent = (uint8_t)max(1, min((int)stepPercent, 100));
      pulsesPerStep = (uint8_t)max(1, min((int)pulsesPerStep, 8));
      buttonDebounceMs = (uint16_t)max(10, min((int)buttonDebounceMs, 200));
      newPwmFrequency = (uint16_t)max(100, min((int)newPwmFrequency, 40000));

      bool needReinit = (newClk != pinClk) || (newDt != pinDt) || (newSw != pinSw) || (newPwm != pinPwm) || (newPwmFrequency != pwmFrequency);

      pinClk = newClk;
      pinDt = newDt;
      pinSw = newSw;
      pinPwm = newPwm;
      pwmFrequency = newPwmFrequency;

      if (initDone && needReinit) {
        deinitPins();
        initPins();
      } else if (initDone) {
        applyPwmDuty();
      }

      return configComplete;
    }

    uint16_t getId() override {
      return USERMOD_ID_UNSPECIFIED;
    }
};

const char RotaryEncoderFanPwmPresetUsermod::_name[]           PROGMEM = "RotaryFanPWM";
const char RotaryEncoderFanPwmPresetUsermod::_enabled[]        PROGMEM = "enabled";
const char RotaryEncoderFanPwmPresetUsermod::_clkPin[]         PROGMEM = "clk-pin";
const char RotaryEncoderFanPwmPresetUsermod::_dtPin[]          PROGMEM = "dt-pin";
const char RotaryEncoderFanPwmPresetUsermod::_swPin[]          PROGMEM = "sw-pin";
const char RotaryEncoderFanPwmPresetUsermod::_pwmPin[]         PROGMEM = "pwm-pin";
const char RotaryEncoderFanPwmPresetUsermod::_dutyPercent[]    PROGMEM = "duty-percent";
const char RotaryEncoderFanPwmPresetUsermod::_stepPercent[]    PROGMEM = "step-percent";
const char RotaryEncoderFanPwmPresetUsermod::_pulsesPerStep[]  PROGMEM = "pulses-per-step";
const char RotaryEncoderFanPwmPresetUsermod::_invertEncoder[]  PROGMEM = "invert-encoder";
const char RotaryEncoderFanPwmPresetUsermod::_pwmFrequency[]   PROGMEM = "pwm-frequency";
const char RotaryEncoderFanPwmPresetUsermod::_buttonDebounceMs[] PROGMEM = "button-debounce-ms";
const char RotaryEncoderFanPwmPresetUsermod::_on[]             PROGMEM = "on";
const char RotaryEncoderFanPwmPresetUsermod::_speed[]          PROGMEM = "speed";
