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

  //Time helpers --------------------------------------------------------------
    //hundreds of seconds calculation
  int getHundredths(int currentSeconds) {
  unsigned long now = millis();

  // Sekunde hat sich geändert → neu synchronisieren
  if (currentSeconds != lastSecondValue) {
    lastSecondValue = currentSeconds;
    lastSecondMillis = now;
    return 99; // start at 99 when a new second begins (counting down)
  }

  unsigned long delta = now - lastSecondMillis;

  // Count down from 99 to 0 across the second
  int hundredths = 99 - (delta / 10);
  if (hundredths < 0) hundredths = 0;
  if (hundredths > 99) hundredths = 99;

  return hundredths;
}
  // Drawing helpers -----------------------------------------------------------
  void drawClock() {
    setDigitInt(0, hour(localTime) / 10);
    setDigitInt(1, hour(localTime) % 10);
    setDigitInt(2, minute(localTime) / 10);
    setDigitInt(3, minute(localTime) % 10);
    setDigitInt(4, second(localTime) / 10);
    setDigitInt(5, second(localTime) % 10);
    // Separator behavior for clock view is controlled by SeperatorOn/SeperatorOff:
    // - both true: blink (as before)
    // - SeperatorOn true only: always on
    // - SeperatorOff true only: always off
    // - fallback: blink
    if (SeperatorOn && SeperatorOff) {
      if (second(localTime) % 2) {
        setSeparator(1, sepsOn);
        setSeparator(2, sepsOn);
      }
    }
    else if (SeperatorOn) {
      setSeparator(1, sepsOn);
      setSeparator(2, sepsOn);
    }
    else if (SeperatorOff) {
      // explicitly off for clock view → do nothing
    }
    else {
      if (second(localTime) % 2) {
        setSeparator(1, sepsOn);
        setSeparator(2, sepsOn);
      }
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

      // > 99 Tage
  if (remDays > 99) {
    setDigitChar(0, ' ');
    setDigitInt(1, (remDays / 100) % 10);
    setDigitInt(2, (remDays / 10) % 10);
    setDigitInt(3, remDays % 10);
    setDigitChar(4, 't');
    setDigitChar(5, ' ');
    return;
  }

  // < 99 Tage
  if (remDays <=99 && fullHours > 99) {
    setDigitInt(0, remDays / 10);
    setDigitInt(1, remDays % 10);
    setSeparator(1, sepsOn);
    setDigitInt(2, remHours / 10);
    setDigitInt(3, remHours % 10);
    setSeparator(2, sepsOn);
    setDigitInt(4, remMinutes / 10);
    setDigitInt(5, remMinutes % 10);
    return;
  }

  // < 99 Stunden
  if (fullHours <=99 && fullMinutes > 99) {
    setDigitInt(0, fullHours / 10);
    setDigitInt(1, fullHours % 10);
    setSeparator(1, sepsOn);
    setDigitInt(2, remMinutes / 10);
    setDigitInt(3, remMinutes % 10);
    setSeparator(2, sepsOn);
    setDigitInt(4, remSeconds / 10);
    setDigitInt(5, remSeconds % 10);
    return;
  }

  // < 99 Minuten → MM SS HH (Hundertstel)
  int hs = getHundredths(remSeconds);

  setDigitInt(0, fullMinutes / 10);
  setDigitInt(1, fullMinutes % 10);
  setSeparatorHalf(1, true, sepsOn);  // upper dot
  setDigitInt(2, remSeconds / 10);
  setDigitInt(3, remSeconds % 10);
  setSeparator(2, sepsOn);
  setDigitInt(4, hs / 10);
  setDigitInt(5, hs % 10);
  }

  // Turn on segments for a single digit according to bitmask
  void setDigitInt(uint8_t digitIndex, int8_t value) {
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
    void setDigitChar(uint8_t digitIndex, char c) {
    if (digitIndex > 5) return;

    uint16_t base = digitBase(digitIndex);
    uint8_t bits  = LETTER_MASK(c);

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

  // Turn on a single half (upper/lower) of the separator (each half = SEP_LEDS/2)
  void setSeparatorHalf(uint8_t which, bool upper, bool on) {
    uint16_t base = (which == 1) ? sep1Base() : sep2Base();
    uint16_t halfLen = SEP_LEDS / 2;
    uint16_t start = base + (upper ? 0 : halfLen);
    if (on) setRangeOn(start, halfLen);
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
  }
  void loop() override {}

 void handleOverlayDraw() {
  if (!enabled) return;

  clearMask();

  // Beide aktiv -> im alternatingTime-Sekunden-Takt wechseln
  if (showClock && showCountdown) {
    uint32_t period = (alternatingTime > 0) ? (uint32_t)alternatingTime : 10U;

    // Blockindex (0,1,2,...) über Unix-Sekunden
    uint32_t block = (uint32_t)localTime / period;

    // Gerade Blöcke: Uhr, ungerade: Countdown (oder umgekehrt, wenn du willst)
    if ((block & 1U) == 0U) drawClock();
    else                    drawCountdown();
  }
  else if (showClock) {
    drawClock();
  }
  else { // showCountdown oder Default
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

    JsonArray se_cfg = grp.createNestedArray(F("seps cfg"));
    se_cfg.add(SeperatorOn ? F("on") : F("off"));
    se_cfg.add(SeperatorOff ? F("on") : F("off"));

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

    s[F("SeperatorOn")]  = SeperatorOn;
    s[F("SeperatorOff")] = SeperatorOff;

    s[F("targetYear")]   = targetYear;
    s[F("targetMonth")]  = targetMonth;
    s[F("targetDay")]    = targetDay;
    s[F("targetHour")]   = targetHour;
    s[F("targetMinute")] = targetMinute;

    s[F("showClock")]     = showClock;
    s[F("showCountdown")] = showCountdown;
    s[F("alternatingTime")] = alternatingTime;
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
    if (s.containsKey(F("alternatingTime"))) alternatingTime = s[F("alternatingTime")].as<uint16_t>();
    if (s.containsKey(F("SeperatorOn")))  SeperatorOn  = s[F("SeperatorOn")].as<bool>();
    if (s.containsKey(F("SeperatorOff"))) SeperatorOff = s[F("SeperatorOff")].as<bool>();

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
    s[F("alternatingTime")] = alternatingTime; // seconds to alternate when both modes enabled
    s[F("SeperatorOn")]  = SeperatorOn;
    s[F("SeperatorOff")] = SeperatorOff;
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
    alternatingTime = s[F("alternatingTime")] | 10; // default 10s
    SeperatorOn  = s[F("SeperatorOn")]  | true;
    SeperatorOff = s[F("SeperatorOff")] | true;

    validateTarget(true);
    return true;
  }

  uint16_t getId() override { return 0x7A01; }
};

static Usermod7SegmentCountdown usermod;
REGISTER_USERMOD(usermod);
