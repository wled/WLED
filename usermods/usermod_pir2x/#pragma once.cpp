#pragma once
#include "wled.h"

class UsermodPIRStairsPro : public Usermod {
private:
  // ===== KONFIG =====
  int pinA = 12;
  int pinB = 33;

  int presetUp = 1;
  int presetDown = 2;
  int presetOff = 0;

  unsigned long lockTime = 8000;       // blokada po aktywacji
  unsigned long offDelay = 90000;      // auto OFF 90s
  unsigned long debounceTime = 500;
  unsigned long directionTimeout = 2000;

  // ===== STAN =====
  unsigned long globalLockUntil = 0;
  unsigned long lastMotionTime = 0;
  unsigned long lastTriggerA = 0;
  unsigned long lastTriggerB = 0;
  unsigned long lastMotionA = 0;
  unsigned long lastMotionB = 0;

  bool lastStateA = true;
  bool lastStateB = true;

  void triggerUp(unsigned long now) {
    applyPreset(presetUp, CALL_MODE_BUTTON_PRESET);
    globalLockUntil = now + lockTime;
    lastMotionTime = now;
  }

  void triggerDown(unsigned long now) {
    applyPreset(presetDown, CALL_MODE_BUTTON_PRESET);
    globalLockUntil = now + lockTime;
    lastMotionTime = now;
  }

public:
  void setup() {
    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
  }

  void loop() {
    unsigned long now = millis();

    bool stateA = digitalRead(pinA);
    bool stateB = digitalRead(pinB);

    bool motionA = (stateA == LOW);
    bool motionB = (stateB == LOW);

    // PIR A
    if (motionA && !lastStateA && (now - lastTriggerA > debounceTime)) {
      lastTriggerA = now;
      lastMotionA = now;
      if (now > globalLockUntil) {
        if (now - lastMotionB < directionTimeout) triggerDown(now);
        else triggerUp(now);
      }
    }

    // PIR B
    if (motionB && !lastStateB && (now - lastTriggerB > debounceTime)) {
      lastTriggerB = now;
      lastMotionB = now;
      if (now > globalLockUntil) {
        if (now - lastMotionA < directionTimeout) triggerUp(now);
        else triggerDown(now);
      }
    }

    // Auto OFF
    if (lastMotionTime > 0 && (now - lastMotionTime > offDelay)) {
      applyPreset(presetOff, CALL_MODE_BUTTON_PRESET);
      lastMotionTime = 0;
      globalLockUntil = 0;
    }

    lastStateA = motionA;
    lastStateB = motionB;
  }

  void addToConfig(JsonObject& root) {
    JsonObject top = root.createNestedObject("pir_stairs_pro");
    top["pinA"] = pinA;
    top["pinB"] = pinB;
    top["presetUp"] = presetUp;
    top["presetDown"] = presetDown;
    top["presetOff"] = presetOff;
    top["lockTime"] = lockTime;
    top["offDelay"] = offDelay;
    top["debounce"] = debounceTime;
    top["dirTimeout"] = directionTimeout;
  }

  bool readFromConfig(JsonObject& root) {
    JsonObject top = root["pir_stairs_pro"];
    if (top.isNull()) return false;

    pinA = top["pinA"] | pinA;
    pinB = top["pinB"] | pinB;
    presetUp = top["presetUp"] | presetUp;
    presetDown = top["presetDown"] | presetDown;
    presetOff = top["presetOff"] | presetOff;
    lockTime = top["lockTime"] | lockTime;
    offDelay = top["offDelay"] | offDelay;
    debounceTime = top["debounce"] | debounceTime;
    directionTimeout = top["dirTimeout"] | directionTimeout;

    return true;
  }

  void addToJsonInfo(JsonObject& root) {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    user["PIR A pin"] = pinA;
    user["PIR B pin"] = pinB;
    user["Lock(ms)"] = lockTime;
  }

  uint16_t getId() { return USERMOD_ID_UNSPECIFIED; }
};