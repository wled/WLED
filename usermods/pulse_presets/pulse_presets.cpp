#include "wled.h"

/*
 * Usermod "pulse_presets"
 *
 * Watches a GPIO pin wired to a PLC dry relay contact. Counts the pulses in a
 * "burst" (a run of pulses separated by gaps shorter than gapTimeoutMs), measures
 * their average width, and compares that (count, width) pair against a small
 * table of patterns. On a match it calls applyPreset() to fire the configured
 * WLED preset.
 *
 * All of pin, timing and the pattern table are editable live under
 * Config -> Usermods -> "pulse_presets" - no recompiling needed to retune them.
 *
 * Wiring: the PLC relay's dry contact should short the pin to GND when active.
 * With "Active low" enabled (the default) the pin is configured INPUT_PULLUP,
 * so no external resistor is needed - just PLC-relay-common -> GND and
 * PLC-relay-NO -> this GPIO.
 */

#ifndef PULSE_PRESETS_MAX_PATTERNS
  #define PULSE_PRESETS_MAX_PATTERNS 6
#endif

class UsermodPulsePresets : public Usermod {
  private:
    // ---- persistent config ----
    bool    enabled         = true;
    int8_t  pin             = -1;    // -1 = not configured
    bool    activeLow       = true;  // true: contact pulls pin to GND when active (use INPUT_PULLUP)
    uint16_t debounceMs      = 30;    // ignore edges faster than this (contact bounce)
    uint16_t gapTimeoutMs    = 600;   // silence on the line for this long closes the current burst
    uint16_t widthToleranceMs = 60;   // +/- allowed difference between measured and configured pulse width

    uint8_t  patCount[PULSE_PRESETS_MAX_PATTERNS]  = {0}; // 0 = slot unused
    uint16_t patWidthMs[PULSE_PRESETS_MAX_PATTERNS] = {0};
    uint8_t  patPreset[PULSE_PRESETS_MAX_PATTERNS] = {0};

    // ---- runtime state ----
    bool          initDone        = false;
    bool          rawActive       = false; // last raw (undebounced) active reading
    bool          debouncedActive = false; // debounced logical pin state
    unsigned long lastEdgeTime    = 0;     // time of last raw transition, for debounce
    unsigned long pulseStartTime  = 0;
    unsigned long lastPulseEndTime = 0;
    bool          burstOpen       = false; // true while a burst is still waiting on the gap timeout
    uint8_t       burstPulseCount = 0;
    uint32_t      burstWidthSum   = 0;

    // ---- diagnostics, shown in Info panel ----
    uint32_t totalBursts       = 0;
    uint32_t totalMatches      = 0;
    uint8_t  lastBurstCount    = 0;
    uint16_t lastBurstAvgWidth = 0;
    bool     lastBurstMatched  = false;
    uint8_t  lastMatchedPreset = 0;

    static const char _name[];
    static const char _enabled[];
    static const char _pin[];
    static const char _activeLow[];
    static const char _debounceMs[];
    static const char _gapTimeoutMs[];
    static const char _widthToleranceMs[];
    static const char _patCount[];
    static const char _patWidthMs[];
    static const char _patPreset[];

    void deallocate() {
      if (pin >= 0) PinManager::deallocatePin(pin, PinOwner::UM_Unspecified);
    }

    void configurePin() {
      if (pin < 0) return;
      if (PinManager::allocatePin(pin, false, PinOwner::UM_Unspecified)) {
        pinMode(pin, activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);
        bool raw = digitalRead(pin);
        rawActive = activeLow ? !raw : raw;
        debouncedActive = rawActive;
        lastEdgeTime = millis();
      } else {
        DEBUG_PRINTLN(F("PulsePresets: pin allocation failed"));
        pin = -1;
      }
    }

    void finalizeBurst() {
      if (burstPulseCount == 0) return;
      uint16_t avgWidth = (uint16_t)(burstWidthSum / burstPulseCount);
      lastBurstCount    = burstPulseCount;
      lastBurstAvgWidth = avgWidth;
      lastBurstMatched  = false;
      totalBursts++;

      for (uint8_t i = 0; i < PULSE_PRESETS_MAX_PATTERNS; i++) {
        if (patCount[i] == 0) continue; // slot unused
        if (patCount[i] != burstPulseCount) continue;
        int32_t diff = (int32_t)avgWidth - (int32_t)patWidthMs[i];
        if (diff < 0) diff = -diff;
        if (diff <= (int32_t)widthToleranceMs) {
          applyPreset(patPreset[i]);
          lastBurstMatched  = true;
          lastMatchedPreset = patPreset[i];
          totalMatches++;
          break; // first matching slot wins
        }
      }

      burstPulseCount = 0;
      burstWidthSum   = 0;
    }

  public:
    void setup() override {
      configurePin();
      initDone = true;
    }

    void loop() override {
      if (!enabled || pin < 0 || !initDone) return;
      // avoid contending with LED output updates on very long strips
      if (strip.isUpdating()) return;

      unsigned long now = millis();
      bool raw = digitalRead(pin);
      bool activeRaw = activeLow ? !raw : raw;

      if (activeRaw != rawActive) {
        rawActive = activeRaw;
        lastEdgeTime = now;
      }

      if (activeRaw != debouncedActive && (now - lastEdgeTime) >= debounceMs) {
        debouncedActive = activeRaw;
        if (debouncedActive) {
          // rising edge: pulse starts
          pulseStartTime = now;
        } else {
          // falling edge: pulse ends
          unsigned long width = now - pulseStartTime;
          if (burstPulseCount < 250) burstPulseCount++; // guard against runaway counts
          burstWidthSum += width;
          lastPulseEndTime = now;
          burstOpen = true;
        }
      }

      if (burstOpen && !debouncedActive && (now - lastPulseEndTime) >= gapTimeoutMs) {
        finalizeBurst();
        burstOpen = false;
      }
    }

    void addToJsonInfo(JsonObject& root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray last = user.createNestedArray(FPSTR(_name));
      if (lastBurstCount == 0) {
        last.add(F("no pulses seen yet"));
      } else {
        char buf[64];
        snprintf_P(buf, sizeof(buf), PSTR("last: %ux %ums avg -> %s"),
                   lastBurstCount, lastBurstAvgWidth,
                   lastBurstMatched ? "matched" : "no match");
        last.add(String(buf));
      }

      JsonArray matched = user.createNestedArray(F("pulse_presets last preset"));
      if (lastBurstMatched) matched.add(lastMatchedPreset);
      else matched.add(F("none"));

      JsonArray stats = user.createNestedArray(F("pulse_presets bursts/matches"));
      stats.add(totalBursts);
      stats.add(F("/"));
      stats.add(totalMatches);
    }

    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)]           = enabled;
      top[FPSTR(_pin)]               = pin;
      top[FPSTR(_activeLow)]         = activeLow;
      top[FPSTR(_debounceMs)]        = debounceMs;
      top[FPSTR(_gapTimeoutMs)]      = gapTimeoutMs;
      top[FPSTR(_widthToleranceMs)]  = widthToleranceMs;

      JsonArray counts  = top.createNestedArray(FPSTR(_patCount));
      JsonArray widths   = top.createNestedArray(FPSTR(_patWidthMs));
      JsonArray presets = top.createNestedArray(FPSTR(_patPreset));
      for (uint8_t i = 0; i < PULSE_PRESETS_MAX_PATTERNS; i++) {
        counts.add(patCount[i]);
        widths.add(patWidthMs[i]);
        presets.add(patPreset[i]);
      }
    }

    bool readFromConfig(JsonObject& root) override {
      int8_t oldPin = pin;

      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, true);
      configComplete &= getJsonValue(top[FPSTR(_pin)], pin, -1);
      configComplete &= getJsonValue(top[FPSTR(_activeLow)], activeLow, true);
      configComplete &= getJsonValue(top[FPSTR(_debounceMs)], debounceMs, 30);
      configComplete &= getJsonValue(top[FPSTR(_gapTimeoutMs)], gapTimeoutMs, 600);
      configComplete &= getJsonValue(top[FPSTR(_widthToleranceMs)], widthToleranceMs, 60);

      JsonArray counts  = top[FPSTR(_patCount)];
      JsonArray widths   = top[FPSTR(_patWidthMs)];
      JsonArray presets = top[FPSTR(_patPreset)];
      for (uint8_t i = 0; i < PULSE_PRESETS_MAX_PATTERNS; i++) {
        patCount[i]  = (!counts.isNull()  && i < counts.size())  ? (uint8_t)(counts[i]   | patCount[i])  : patCount[i];
        patWidthMs[i] = (!widths.isNull()  && i < widths.size())  ? (uint16_t)(widths[i]  | patWidthMs[i]) : patWidthMs[i];
        patPreset[i] = (!presets.isNull() && i < presets.size()) ? (uint8_t)(presets[i] | patPreset[i]) : patPreset[i];
      }
      configComplete &= !(counts.isNull() || counts.size() != PULSE_PRESETS_MAX_PATTERNS);

      if (!initDone) {
        // first load, prior to setup() - setup() will configure the pin
      } else if (pin != oldPin) {
        // pin was changed live from the Usermod Settings page - reconfigure it now
        if (oldPin >= 0) PinManager::deallocatePin(oldPin, PinOwner::UM_Unspecified);
        configurePin();
      }

      return configComplete;
    }

    void appendConfigData() override {
      oappend(F("addInfo('pulse_presets:pin',1,'GPIO from the PLC relay dry contact');"));
      oappend(F("addInfo('pulse_presets:activeLow',1,'on = contact shorts pin to GND when active (use INPUT_PULLUP)');"));
      oappend(F("addInfo('pulse_presets:gapTimeoutMs',1,'silence on the line (ms) that closes a burst');"));
      oappend(F("addInfo('pulse_presets:widthToleranceMs',1,'+/- ms allowed when matching a pulse width');"));
      for (uint8_t i = 0; i < PULSE_PRESETS_MAX_PATTERNS; i++) {
        char str[96];
        snprintf_P(str, sizeof(str), PSTR("addInfo('pulse_presets:patCount[]',%u,'pulses in burst (0 = unused)','#%u count');"), i, i);
        oappend(str);
        snprintf_P(str, sizeof(str), PSTR("addInfo('pulse_presets:patWidthMs[]',%u,'expected pulse width, ms','#%u width');"), i, i);
        oappend(str);
        snprintf_P(str, sizeof(str), PSTR("addInfo('pulse_presets:patPreset[]',%u,'WLED preset to fire on match','#%u preset');"), i, i);
        oappend(str);
      }
    }
};

const char UsermodPulsePresets::_name[]             PROGMEM = "pulse_presets";
const char UsermodPulsePresets::_enabled[]          PROGMEM = "enabled";
const char UsermodPulsePresets::_pin[]              PROGMEM = "pin";
const char UsermodPulsePresets::_activeLow[]        PROGMEM = "activeLow";
const char UsermodPulsePresets::_debounceMs[]       PROGMEM = "debounceMs";
const char UsermodPulsePresets::_gapTimeoutMs[]     PROGMEM = "gapTimeoutMs";
const char UsermodPulsePresets::_widthToleranceMs[] PROGMEM = "widthToleranceMs";
const char UsermodPulsePresets::_patCount[]         PROGMEM = "patCount";
const char UsermodPulsePresets::_patWidthMs[]       PROGMEM = "patWidthMs";
const char UsermodPulsePresets::_patPreset[]        PROGMEM = "patPreset";

static UsermodPulsePresets pulse_presets;
REGISTER_USERMOD(pulse_presets);
