// usermods/usermod_7segment_countdown/usermod_7segment_countdown.cpp
#include "wled.h"

/*
  Step 2:
  - LED layout (6x 7-seg digits, 2x separators)
  - Segment mapping
  - Apply mask: ON keeps current WLED effect/color, OFF becomes black
  - Test display: 88:88:88
  - Info UI: collapsible group under [u] -> "7 Segment Counter"
  - Enable/Disable switch:
      * runtime via /json/state
      * persistent via WLED config (readFromConfig/addToConfig)
*/

class Usermod7SegmentCountdown : public Usermod {
private:
  // ---- Layout ----
  static constexpr uint16_t LEDS_PER_SEG   = 5;
  static constexpr uint8_t  SEGS_PER_DIGIT = 7;
  static constexpr uint16_t LEDS_PER_DIGIT = LEDS_PER_SEG * SEGS_PER_DIGIT; // 35

  static constexpr uint16_t SEP_LEDS = 10; // 5 upper dot, 5 lower dot

  // Stream: Z1(35) Z2(35) Sep1(10) Z3(35) Z4(35) Sep2(10) Z5(35) Z6(35)
  static constexpr uint16_t TOTAL_PANEL_LEDS = 6 * LEDS_PER_DIGIT + 2 * SEP_LEDS; // 230

  // Phys segment order per digit (0..6): F-A-B-G-E-D-C
  // Logical segment indices: A=0,B=1,C=2,D=3,E=4,F=5,G=6
  static inline constexpr uint8_t PHYS_TO_LOG[SEGS_PER_DIGIT] = {
    5, // F
    0, // A
    1, // B
    6, // G
    4, // E
    3, // D
    2  // C
  };

  // Digit bitmasks (bits A..G -> 0..6)
  static inline constexpr uint8_t DIGIT_MASKS[10] = {
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

  // Mask: 1 = on (keep current color), 0 = off (force black)
  std::vector<uint8_t> mask;

  bool enabled = true;   // switchable + persistent
  bool sepsOn  = true;   // will be configurable later; kept for now

  // ---- Index helpers ----
  static uint16_t digitBase(uint8_t d) {
    // d: 0..5 (Z1..Z6)
    // Bases:
    // Z1 0
    // Z2 35
    // Sep1 70
    // Z3 80
    // Z4 115
    // Sep2 150
    // Z5 160
    // Z6 195
    switch (d) {
      case 0: return 0;
      case 1: return 35;
      case 2: return 80;
      case 3: return 115;
      case 4: return 160;
      case 5: return 195;
      default: return 0;
    }
  }

  static constexpr uint16_t sep1Base() { return 70; }
  static constexpr uint16_t sep2Base() { return 150; }

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

  void setDigit(uint8_t digitIndex, int8_t value) {
    // value: 0..9, -1 = blank
    if (digitIndex > 5) return;
    if (value < 0) return;

    uint16_t base = digitBase(digitIndex);
    uint8_t bits  = DIGIT_MASKS[(uint8_t)value];

    for (uint8_t physSeg = 0; physSeg < SEGS_PER_DIGIT; physSeg++) {
      uint8_t logSeg = PHYS_TO_LOG[physSeg];      // A..G index
      bool segOn = (bits >> logSeg) & 0x01;

      if (segOn) {
        uint16_t segStart = base + (uint16_t)physSeg * LEDS_PER_SEG;
        setRangeOn(segStart, LEDS_PER_SEG);
      }
    }
  }

  void setSeparator(uint8_t which, bool on) {
    uint16_t base = (which == 1) ? sep1Base() : sep2Base();
    if (on) setRangeOn(base, SEP_LEDS);
  }

  void applyMaskToStrip() {
    // OFF -> black, ON -> unchanged (keeps effect/color)
    uint16_t stripLen = strip.getLengthTotal();
    uint16_t limit = (stripLen < (uint16_t)mask.size()) ? stripLen : (uint16_t)mask.size();

    for (uint16_t i = 0; i < limit; i++) {
      if (!mask[i]) strip.setPixelColor(i, 0);
    }
  }

public:
  void setup() override {
    ensureMaskSize();
    Serial.print("7Segment Setup - MOD: ");
    Serial.println(enabled ? "enabled" : "disabled");
  }

  void loop() override {
  }

  void handleOverlayDraw(){
    if (!enabled) return;
    clearMask();
    setDigit(0, hour(localTime) / 10);      // Z1
    setDigit(1, hour(localTime) % 10);      // Z2
    setDigit(2, minute(localTime) / 10);    // Z3
    setDigit(3, minute(localTime) % 10);    // Z4 
    setDigit(4, second(localTime) / 10);    // Z5
    setDigit(5, second(localTime) % 10);    // Z6
    if(second(localTime) % 2){
      setSeparator(1, sepsOn);               // Sep1
      setSeparator(2, sepsOn);               // Sep2
    }
    applyMaskToStrip();
  }

  // ---- Info UI (collapsible group under "u") ----
  void addToJsonInfo(JsonObject& root) override {
    JsonObject user = root["u"].as<JsonObject>();
    if (user.isNull()) user = root.createNestedObject("u");

    // Parent group (collapsible in UI depending on WLED UI build)
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
  }

  // ---- Runtime control via /json/state ----
  void addToJsonState(JsonObject& root) override {
    JsonObject s = root[F("7seg")].as<JsonObject>();
    if (s.isNull()) s = root.createNestedObject(F("7seg"));
    s[F("enabled")] = enabled;
  }

  void readFromJsonState(JsonObject& root) override {
    JsonObject s = root[F("7seg")].as<JsonObject>();
    if (s.isNull()) return;

    if (s.containsKey(F("enabled"))) {
      enabled = s[F("enabled")].as<bool>();
    }
  }

  // ---- Persistent config (survives reboot) ----
  void addToConfig(JsonObject& root) override {
    JsonObject s = root[F("7seg")].as<JsonObject>();
    if (s.isNull()) s = root.createNestedObject(F("7seg"));
    s[F("enabled")] = enabled;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject s = root[F("7seg")].as<JsonObject>();
    if (s.isNull()) return false;

    enabled = s[F("enabled")] | true; // default true
    return true;
  }

  uint16_t getId() override {
    return 0x7A01;
  }
};

static Usermod7SegmentCountdown usermod;
REGISTER_USERMOD(usermod);
