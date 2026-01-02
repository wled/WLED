// 7-segment countdown usermod overlay: builds a display mask over the LED strip
#include "wled.h"
#include "usermod_7segment_countdown.h"

// Layout/Segment mapping (keep these comments)
// Phys segment order per digit (0..6): F-A-B-G-E-D-C
// Logical segment indices: A=0,B=1,C=2,D=3,E=4,F=5,G=6
static constexpr uint8_t PHYS_TO_LOG[SEGS_PER_DIGIT] = {
    5, // F
    0, // A
    1, // B
    6, // G
    4, // E
    3, // D
    2  // C
};

// Digit bitmasks (bits A..G -> 0..6) (keep these comments)
static constexpr uint8_t DIGIT_MASKS[10] = {
  0b00111111, // 0 (A..F)
  0b00000110, // 1 (B,C)
  0b01011011, // 2 (A,B,D,E,G)
  0b01001111, // 3 (A,B,C,D,G)
  0b01100110, // 4 (B,C,F,G)
  0b01101101, // 5 (A,C,D,F,G)
  0b01111101, // 6 (A,C,D,E,F,G)
  0b00000111, // 7 (A,B,C)
  0b01111111, // 8 (A..G)
  0b01101111  // 9 (A,B,C,D,F,G)
};

class Usermod7SegmentCountdown : public Usermod {
private:
  // Mask helpers --------------------------------------------------------------
  void ensureMaskSize() {
    if (mask.size() != TOTAL_PANEL_LEDS) mask.assign(TOTAL_PANEL_LEDS, 0);
  }
  void clearMask() {
    std::fill(mask.begin(), mask.end(), 0);
  }
  void setRangeOn(uint16_t start, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
      uint16_t idx = start + i;
      if (idx < mask.size()) mask[idx] = 1;
    }
  }

  // Drawing helpers -----------------------------------------------------------
  void drawClock() {
    setDigit(0, hour(localTime) / 10);
    setDigit(1, hour(localTime) % 10);
    setDigit(2, minute(localTime) / 10);
    setDigit(3, minute(localTime) % 10);
    setDigit(4, second(localTime) / 10);
    setDigit(5, second(localTime) % 10);
    if (second(localTime) % 2) {
      setSeparator(1, sepsOn);
      setSeparator(2, sepsOn);
    }
  }

  // Compute remaining time to targetUnix; also provide full totals (h/min/sec)
  void drawCountdown() {
    int64_t diff = (int64_t)targetUnix - (int64_t)localTime;

    remDays    = diff / 86400u;
    remHours   = (uint8_t)((diff % 86400u) / 3600u);
    remMinutes = (uint8_t)((diff % 3600u) / 60u);
    remSeconds = (uint8_t)(diff % 60u);

    fullHours   = diff / 3600u;
    fullMinutes = diff / 60u;
    fullSeconds = diff;
  }

  // Turn on segments for a single digit according to bitmask
  void setDigit(uint8_t digitIndex, int8_t value) {
    if (digitIndex > 5) return;
    if (value < 0) return;

    uint16_t base = digitBase(digitIndex);
    uint8_t bits  = DIGIT_MASKS[(uint8_t)value];

    for (uint8_t physSeg = 0; physSeg < SEGS_PER_DIGIT; physSeg++) {
      uint8_t logSeg = PHYS_TO_LOG[physSeg];
      bool segOn = (bits >> logSeg) & 0x01;
      if (segOn) {
        uint16_t segStart = base + (uint16_t)physSeg * LEDS_PER_SEG;
        setRangeOn(segStart, LEDS_PER_SEG);
      }
    }
  }

  // Turn on both separator dots if requested
  void setSeparator(uint8_t which, bool on) {
    uint16_t base = (which == 1) ? sep1Base() : sep2Base();
    if (on) setRangeOn(base, SEP_LEDS);
  }

  // Apply mask to strip: 1 keeps color/effect, 0 forces black
  void applyMaskToStrip() {
    uint16_t stripLen = strip.getLengthTotal();
    uint16_t limit = (stripLen < (uint16_t)mask.size()) ? stripLen : (uint16_t)mask.size();
    for (uint16_t i = 0; i < limit; i++) {
      if (!mask[i]) strip.setPixelColor(i, 0);
    }
  }

  template <typename T>
  static T clampVal(T v, T lo, T hi) {
    return (v < lo) ? lo : (v > hi ? hi : v);
  }

  // Clamp target fields and derive targetUnix; optional debug on change
  void validateTarget(bool changed = false) {
    targetYear   = clampVal(targetYear, 1970, 2099);
    targetMonth  = clampVal<uint8_t>(targetMonth, 1, 12);
    targetDay    = clampVal<uint8_t>(targetDay, 1, 31);
    targetHour   = clampVal<uint8_t>(targetHour, 0, 23);
    targetMinute = clampVal<uint8_t>(targetMinute, 0, 59);

    tmElements_t tm;
    tm.Second = 0;
    tm.Minute = targetMinute;
    tm.Hour   = targetHour;
    tm.Day    = targetDay;
    tm.Month  = targetMonth;
    tm.Year   = CalendarYrToTm(targetYear);
    targetUnix = makeTime(tm);

    if (changed) {
      char buf[24];
      snprintf(buf, sizeof(buf), "%04d-%02u-%02u-%02u-%02u",
               targetYear, targetMonth, targetDay, targetHour, targetMinute);
      Serial.printf("[7seg] Target changed: %s | unix=%lu\r\n", buf, (unsigned long)targetUnix);
    }
  }

public:
  void setup() override {
    ensureMaskSize();
    Serial.print("7Segment Setup - MOD: ");
    Serial.println(enabled ? "enabled" : "disabled");
  }
  void loop() override {}

  void handleOverlayDraw(){
    if (!enabled) return;
    clearMask();
    if (showClock && !showCountdown) {
      drawClock();
    } else {
      drawCountdown();
    }
    applyMaskToStrip();
  }

  // Info UI (u-group)
  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"].as<JsonObject>();
    if (user.isNull()) user = root.createNestedObject("u");
    JsonObject grp = user.createNestedObject(F("7 Segment Counter"));

    JsonArray state = grp.createNestedArray(F("state"));
    state.add(enabled ? F("active") : F("disabled"));
    state.add("");

    JsonArray se = grp.createNestedArray(F("seps"));
    se.add(sepsOn ? F("on") : F("off"));
    se.add("");

    JsonArray pl = grp.createNestedArray(F("panel leds"));
    pl.add(TOTAL_PANEL_LEDS);
    pl.add(F(" px"));

    JsonArray tgt = grp.createNestedArray(F("target"));
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02u-%02u %02u:%02u",
             targetYear, targetMonth, targetDay, targetHour, targetMinute);
    tgt.add(buf);
    tgt.add("");
  }

  // JSON state/config ---------------------------------------------------------
  void addToJsonState(JsonObject& root) override {
    JsonObject s = root[F("7seg")].as<JsonObject>();
    if (s.isNull()) s = root.createNestedObject(F("7seg"));
    s[F("enabled")] = enabled;

    s[F("targetYear")]   = targetYear;
    s[F("targetMonth")]  = targetMonth;
    s[F("targetDay")]    = targetDay;
    s[F("targetHour")]   = targetHour;
    s[F("targetMinute")] = targetMinute;

    s[F("showClock")]     = showClock;
    s[F("showCountdown")] = showCountdown;
  }

  void readFromJsonState(JsonObject& root) override {
    JsonObject s = root[F("7seg")].as<JsonObject>();
    if (s.isNull()) return;

    if (s.containsKey(F("enabled"))) enabled = s[F("enabled")].as<bool>();

    bool changed = false;
    if (s.containsKey(F("targetYear")))   { targetYear   = s[F("targetYear")].as<int>();      changed = true; }
    if (s.containsKey(F("targetMonth")))  { targetMonth  = s[F("targetMonth")].as<uint8_t>(); changed = true; }
    if (s.containsKey(F("targetDay")))    { targetDay    = s[F("targetDay")].as<uint8_t>();   changed = true; }
    if (s.containsKey(F("targetHour")))   { targetHour   = s[F("targetHour")].as<uint8_t>();  changed = true; }
    if (s.containsKey(F("targetMinute"))) { targetMinute = s[F("targetMinute")].as<uint8_t>();changed = true; }

    if (s.containsKey(F("showClock")))     showClock     = s[F("showClock")].as<bool>();
    if (s.containsKey(F("showCountdown"))) showCountdown = s[F("showCountdown")].as<bool>();

    if (changed) validateTarget(true);
  }

  void addToConfig(JsonObject& root) override {
    JsonObject s = root[F("7seg")].as<JsonObject>();
    if (s.isNull()) s = root.createNestedObject(F("7seg"));
    s[F("enabled")] = enabled;

    s[F("targetYear")]   = targetYear;
    s[F("targetMonth")]  = targetMonth;
    s[F("targetDay")]    = targetDay;
    s[F("targetHour")]   = targetHour;
    s[F("targetMinute")] = targetMinute;

    s[F("showClock")]     = showClock;
    s[F("showCountdown")] = showCountdown;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject s = root[F("7seg")].as<JsonObject>();
    if (s.isNull()) return false;

    enabled       = s[F("enabled")]       | true;
    targetYear    = s[F("targetYear")]    | year(localTime);
    targetMonth   = s[F("targetMonth")]   | 1;
    targetDay     = s[F("targetDay")]     | 1;
    targetHour    = s[F("targetHour")]    | 0;
    targetMinute  = s[F("targetMinute")]  | 0;

    showClock     = s[F("showClock")]     | true;
    showCountdown = s[F("showCountdown")] | false;

    validateTarget(true);
    return true;
  }

  uint16_t getId() override { return 0x7A01; }
};

static Usermod7SegmentCountdown usermod;
REGISTER_USERMOD(usermod);
