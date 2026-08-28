#include "wled.h"

/*
 * PIR_Dual_Lock - WLED 0.16.x
 *
 * Stair automation:
 * PIR -> START preset -> LIGHT preset -> END preset -> OFF.
 *
 * GPIO, presets and times are configurable from WLED Usermods settings.
 * Default GPIOs: PIR DOWN=33, PIR UP=12.
 *
 * The PIR lock logic is edge based: one HIGH->LOW transition starts a
 * cycle; keeping the PIR LOW does not retrigger it.
 *
 * For Wipe/Sweep presets, Auto Speed changes the effect speed so the
 * transition is approximately the configured stage duration. This is
 * intentionally limited to Wipe/Sweep because other effects interpret
 * Speed differently.
 */

class PIRDualLockUsermod : public Usermod {
private:
  static constexpr uint8_t DEFAULT_DOWN_PIN = 33;
  static constexpr uint8_t DEFAULT_UP_PIN = 12;

  static constexpr uint16_t DEFAULT_DOWN_START = 41;
  static constexpr uint16_t DEFAULT_DOWN_LIGHT = 42;
  static constexpr uint16_t DEFAULT_DOWN_END = 43;
  static constexpr uint16_t DEFAULT_UP_START = 40;
  static constexpr uint16_t DEFAULT_UP_LIGHT = 42;
  static constexpr uint16_t DEFAULT_UP_END = 44;

  static constexpr uint32_t DEFAULT_STAGE_SEC = 8;
  static constexpr uint32_t DEFAULT_LIGHT_SEC = 30;
  static constexpr uint32_t DEFAULT_LOCK_SEC = 60;
  static constexpr uint32_t MAX_STAGE_SEC = 300;
  static constexpr uint32_t MAX_LIGHT_SEC = 3600;
  static constexpr uint32_t MAX_LOCK_SEC = 3600;

  uint8_t downPin = DEFAULT_DOWN_PIN;
  uint8_t upPin = DEFAULT_UP_PIN;
  bool downActiveLow = true;
  bool upActiveLow = true;

  uint16_t downStart = DEFAULT_DOWN_START;
  uint16_t downLight = DEFAULT_DOWN_LIGHT;
  uint16_t downEnd = DEFAULT_DOWN_END;
  uint16_t upStart = DEFAULT_UP_START;
  uint16_t upLight = DEFAULT_UP_LIGHT;
  uint16_t upEnd = DEFAULT_UP_END;

  uint32_t downStartSec = DEFAULT_STAGE_SEC;
  uint32_t downLightSec = DEFAULT_LIGHT_SEC;
  uint32_t downEndSec = DEFAULT_STAGE_SEC;
  uint32_t upStartSec = DEFAULT_STAGE_SEC;
  uint32_t upLightSec = DEFAULT_LIGHT_SEC;
  uint32_t upEndSec = DEFAULT_STAGE_SEC;

  uint32_t downLockSec = DEFAULT_LOCK_SEC;
  uint32_t upLockSec = DEFAULT_LOCK_SEC;

  bool autoSpeed = true;
  bool enabled = true;

  bool downReady = false;
  bool upReady = false;
  bool lastDown = false;
  bool lastUp = false;

  uint32_t lockUntil = 0;
  uint8_t lockOwner = 0;

  bool locked() const {
    return lockOwner != 0 && (int32_t)(millis() - lockUntil) < 0;
  }

  void updateLock() {
    if (lockOwner && !locked()) {
      lockOwner = 0;
      lockUntil = 0;
    }
  }

  bool readActive(uint8_t pin, bool activeLow) const {
    const int state = digitalRead(pin);
    return activeLow ? (state == LOW) : (state == HIGH);
  }

  bool allocateInput(uint8_t pin, bool &ready) {
    ready = false;
    if (pin > 39) return false;
    if (!PinManager::allocatePin(pin, false, PinOwner::UM_Unspecified)) {
      DEBUG_PRINTF_P(PSTR("PIR_Dual_Lock: GPIO %u unavailable\n"), pin);
      return false;
    }
    pinMode(pin, INPUT_PULLUP);
    ready = true;
    return true;
  }

  /*
   * WLED effect speed is 1..255. Wipe/Sweep are handled specially.
   * We use a monotonic estimate from the number of pixels and desired
   * duration. It is recalculated every time a stage starts, so a later
   * change in LED count does not require recompilation.
   *
   * The exact duration of a WLED effect can vary by platform/load; the
   * purpose here is automatic scaling rather than a hard real-time timer.
   */
  uint8_t speedForStage(uint32_t durationMs) const {
    if (durationMs < 100) durationMs = 100;
    const uint32_t pixels = strip.getLengthTotal();
    if (pixels < 1) return 128;

    // Approximate milliseconds per pixel at Speed 128.
    // Speed is then scaled for both pixel count and requested duration.
    const uint32_t baseMsPerPixel = 18;
    uint32_t speed = (pixels * baseMsPerPixel * 128UL) / durationMs;
    if (speed < 1) speed = 1;
    if (speed > 255) speed = 255;
    return (uint8_t)speed;
  }

  void applyPresetWithAutoSpeed(uint16_t preset, uint32_t stageSec) {
    if (!preset) return;

    // Let WLED load the complete preset first.
    applyPreset(preset, CALL_MODE_BUTTON_PRESET);

    if (!autoSpeed || stageSec == 0) return;

    // Wipe = 9, Sweep = 10 in WLED's standard effect table.
    // Only modify speed for these effects.
    if (strip.getMode() == 9 || strip.getMode() == 10) {
      strip.setSpeed(speedForStage(stageSec * 1000UL));
    }
  }

  void startCycle(uint8_t pir) {
    updateLock();
    if (!enabled || lockOwner || strip.getLengthTotal() == 0) return;

    const bool down = (pir == 1);
    const uint16_t start = down ? downStart : upStart;
    const uint32_t startSec = down ? downStartSec : upStartSec;
    const uint32_t lockSec = down ? downLockSec : upLockSec;

    if (!start) return;

    applyPresetWithAutoSpeed(start, startSec);

    // The three-stage sequence is scheduled by the WLED preset transition
    // timers below. Store the active cycle and its stage deadlines.
    activePir = pir;
    stage = 1;
    stageDeadline = millis() + startSec * 1000UL;

    lockOwner = pir;
    lockUntil = millis() + min(lockSec, MAX_LOCK_SEC) * 1000UL;
  }

  uint8_t activePir = 0;
  uint8_t stage = 0;
  uint32_t stageDeadline = 0;

  void runStageMachine() {
    if (!activePir || !stage) return;
    if ((int32_t)(millis() - stageDeadline) < 0) return;

    const bool down = activePir == 1;
    uint16_t preset = 0;
    uint32_t duration = 0;

    if (stage == 1) {
      preset = down ? downLight : upLight;
      duration = down ? downLightSec : upLightSec;
      stage = 2;
    } else if (stage == 2) {
      preset = down ? downEnd : upEnd;
      duration = down ? downEndSec : upEndSec;
      stage = 3;
    } else {
      // Final stage is finished: switch WLED fully off.
      bri = 0;
      strip.setBrightness(0);
      for (uint8_t s = 0; s < strip.getMaxSegments(); s++) {
        Segment &seg = strip.getSegment(s);
        if (seg.isActive()) seg.setOption(SEG_OPTION_ON, false);
      }
      strip.trigger();
      activePir = 0;
      stage = 0;
      stageDeadline = 0;
      return;
    }

    if (preset) applyPresetWithAutoSpeed(preset, duration);

    stageDeadline = millis() + duration * 1000UL;
  }

  uint32_t upLightSec = DEFAULT_LIGHT_SEC;

public:
  void setup() override {
    allocateInput(downPin, downReady);
    allocateInput(upPin, upReady);

    lastDown = downReady ? readActive(downPin, downActiveLow) : false;
    lastUp = upReady ? readActive(upPin, upActiveLow) : false;
  }

  void loop() override {
    updateLock();
    runStageMachine();

    if (downReady) {
      bool active = readActive(downPin, downActiveLow);
      if (active && !lastDown) startCycle(1);
      lastDown = active;
    }

    if (upReady) {
      bool active = readActive(upPin, upActiveLow);
      if (active && !lastUp) startCycle(2);
      lastUp = active;
    }
  }

  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject(F("PIR Dual Lock"));

    top[F("Enabled")] = enabled;
    top[F("Auto Speed Wipe")] = autoSpeed;

    top[F("PIR Down GPIO")] = downPin;
    top[F("PIR Down Active Low")] = downActiveLow;
    top[F("PIR Down Start Preset")] = downStart;
    top[F("PIR Down Start Sec")] = downStartSec;
    top[F("PIR Down Light Preset")] = downLight;
    top[F("PIR Down Light Sec")] = downLightSec;
    top[F("PIR Down End Preset")] = downEnd;
    top[F("PIR Down End Sec")] = downEndSec;
    top[F("PIR Down Lock Sec")] = downLockSec;

    top[F("PIR Up GPIO")] = upPin;
    top[F("PIR Up Active Low")] = upActiveLow;
    top[F("PIR Up Start Preset")] = upStart;
    top[F("PIR Up Start Sec")] = upStartSec;
    top[F("PIR Up Light Preset")] = upLight;
    top[F("PIR Up Light Sec")] = upLightSec;
    top[F("PIR Up End Preset")] = upEnd;
    top[F("PIR Up End Sec")] = upEndSec;
    top[F("PIR Up Lock Sec")] = upLockSec;
  }

  bool readFromConfig(JsonObject &root) override {
    JsonObject top = root[F("PIR Dual Lock")];
    if (top.isNull()) return false;

    bool ok = true;
    ok &= getJsonValue(top[F("Enabled")], enabled, true);
    ok &= getJsonValue(top[F("Auto Speed Wipe")], autoSpeed, true);

    ok &= getJsonValue(top[F("PIR Down GPIO")], downPin, DEFAULT_DOWN_PIN);
    ok &= getJsonValue(top[F("PIR Down Active Low")], downActiveLow, true);
    ok &= getJsonValue(top[F("PIR Down Start Preset")], downStart, DEFAULT_DOWN_START);
    ok &= getJsonValue(top[F("PIR Down Start Sec")], downStartSec, DEFAULT_STAGE_SEC);
    ok &= getJsonValue(top[F("PIR Down Light Preset")], downLight, DEFAULT_DOWN_LIGHT);
    ok &= getJsonValue(top[F("PIR Down Light Sec")], downLightSec, DEFAULT_LIGHT_SEC);
    ok &= getJsonValue(top[F("PIR Down End Preset")], downEnd, DEFAULT_DOWN_END);
    ok &= getJsonValue(top[F("PIR Down End Sec")], downEndSec, DEFAULT_STAGE_SEC);
    ok &= getJsonValue(top[F("PIR Down Lock Sec")], downLockSec, DEFAULT_LOCK_SEC);

    ok &= getJsonValue(top[F("PIR Up GPIO")], upPin, DEFAULT_UP_PIN);
    ok &= getJsonValue(top[F("PIR Up Active Low")], upActiveLow, true);
    ok &= getJsonValue(top[F("PIR Up Start Preset")], upStart, DEFAULT_UP_START);
    ok &= getJsonValue(top[F("PIR Up Start Sec")], upStartSec, DEFAULT_STAGE_SEC);
    ok &= getJsonValue(top[F("PIR Up Light Preset")], upLight, DEFAULT_UP_LIGHT);
    ok &= getJsonValue(top[F("PIR Up Light Sec")], upLightSec, DEFAULT_LIGHT_SEC);
    ok &= getJsonValue(top[F("PIR Up End Preset")], upEnd, DEFAULT_UP_END);
    ok &= getJsonValue(top[F("PIR Up End Sec")], upEndSec, DEFAULT_STAGE_SEC);
    ok &= getJsonValue(top[F("PIR Up Lock Sec")], upLockSec, DEFAULT_LOCK_SEC);

    if (downPin > 39) downPin = DEFAULT_DOWN_PIN;
    if (upPin > 39) upPin = DEFAULT_UP_PIN;

    downStartSec = min(downStartSec, MAX_STAGE_SEC);
    downEndSec = min(downEndSec, MAX_STAGE_SEC);
    upStartSec = min(upStartSec, MAX_STAGE_SEC);
    upEndSec = min(upEndSec, MAX_STAGE_SEC);
    downLightSec = min(downLightSec, MAX_LIGHT_SEC);
    upLightSec = min(upLightSec, MAX_LIGHT_SEC);
    downLockSec = min(downLockSec, MAX_LOCK_SEC);
    upLockSec = min(upLockSec, MAX_LOCK_SEC);

    return ok;
  }

  void appendConfigData() override {
    oappend(F("addInfo('PIR Dual Lock:PIR Down GPIO',1,'Domyslnie 33');"));
    oappend(F("addInfo('PIR Dual Lock:PIR Up GPIO',1,'Domyslnie 12');"));
    oappend(F("addInfo('PIR Dual Lock:Auto Speed Wipe',1,'Automatycznie dobiera Speed dla Wipe/Sweep');"));
    oappend(F("addInfo('PIR Dual Lock:PIR Down Lock Sec',1,'Blokada PIR dol; 0 = brak');"));
    oappend(F("addInfo('PIR Dual Lock:PIR Up Lock Sec',1,'Blokada PIR gora; 0 = brak');"));
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};

static PIRDualLockUsermod PIRDualLock;
REGISTER_USERMOD(PIRDualLock);
