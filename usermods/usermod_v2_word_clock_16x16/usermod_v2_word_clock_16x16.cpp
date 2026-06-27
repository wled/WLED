#include "wled.h"

/*
 * Word Clock MK2 - 16x16 RGBW matrix, English, exact-minute phrasing.
 *
 * Unlike usermod_v2_word_clock (which is a German clock drawn as an OVERLAY in
 * handleOverlayDraw), this usermod registers the word clock as a first-class WLED
 * *Effect* ("Word Clock 16x16"). That means it can be transitioned/crossfaded,
 * colored, palette-mapped and stored per-preset like any other effect.
 *
 * The effect is 2D: configure a 16x16 matrix in WLED's LED preferences (set the
 * serpentine/rotation there to match the physical wiring). This code works purely
 * in logical X/Y (0..15) and never touches raw LED indices.
 *
 * Grammar (exact minute), e.g. "IT IS TWENTY ONE MINUTES PAST SEVEN IN THE EVENING".
 *
 * Letter grid (row 0 = top), see readme.md:
 *   IT KIS S TWENTY TONE / TWO TEN THIRTEEN / FIVE ELEVEN FOUR / THREE NINETEEN ...
 *
 * Out of scope (future): temperature words (WARM/COOL/HOT/COLD, &), corner RGBW
 * LEDs and corner buttons, smooth per-minute crossfade inside the effect.
 */

// A word is a horizontal run of letters: top-left cell (x,y) and length.
struct WCWord { uint8_t x, y, len; };

// ---- Fixed words / connectors -------------------------------------------------
static const WCWord wcIT        = { 0, 0, 2};
static const WCWord wcIS        = { 3, 0, 2};
static const WCWord wcMINUTES   = { 0, 7, 7};
static const WCWord wcQUARTER   = { 8, 7, 7};
static const WCWord wcHALF      = { 0, 8, 4};
static const WCWord wcPAST      = { 5, 8, 4};
static const WCWord wcUNTIL     = {10, 8, 5};
static const WCWord wcOCLOCK    = { 1,12, 6};
static const WCWord wcIN        = {10,12, 2};
static const WCWord wcTHE       = {13,12, 3};
static const WCWord wcAT        = { 0,14, 2}; // same row as NIGHT -> "AT NIGHT"
static const WCWord wcMORNING   = { 9,13, 7};
static const WCWord wcAFTERNOON = { 0,13, 9};
static const WCWord wcEVENING   = { 9,14, 7};
static const WCWord wcNIGHT     = { 3,14, 5};

// ---- Temperature words (bottom row 15): "& WARM COOL HOT COLD" -----------------
// Index == temperature band (1..4); index 0 = none/unknown.
static const WCWord wcTempWord[5] = {
  { 0, 0, 0}, // 0 none
  {12,15, 4}, // 1 COLD
  { 5,15, 4}, // 2 COOL
  { 1,15, 4}, // 3 WARM
  { 9,15, 3}, // 4 HOT
};

// ---- Minute number words (top region, rows 0..8); index == the number ---------
// Index 0 unused. 21..29 are composed as TWENTY (20) + ones (1..9).
static const WCWord wcMinuteNum[21] = {
  { 0, 0, 0}, //  0 unused
  {13, 0, 3}, //  1 ONE
  { 0, 1, 3}, //  2 TWO
  { 0, 3, 5}, //  3 THREE
  {12, 2, 4}, //  4 FOUR
  { 0, 2, 4}, //  5 FIVE
  { 0, 5, 3}, //  6 SIX      (inside SIXTEEN)
  { 0, 6, 5}, //  7 SEVEN    (inside SEVENTEEN)
  { 8, 5, 5}, //  8 EIGHT    (inside EIGHTEEN)
  { 6, 3, 4}, //  9 NINE     (inside NINETEEN)
  { 4, 1, 3}, // 10 TEN
  { 5, 2, 6}, // 11 ELEVEN
  {10, 6, 6}, // 12 TWELVE
  { 8, 1, 8}, // 13 THIRTEEN
  { 0, 4, 8}, // 14 FOURTEEN
  { 8, 7, 7}, // 15 QUARTER  (no FIFTEEN tile; handled specially)
  { 0, 5, 7}, // 16 SIXTEEN
  { 0, 6, 9}, // 17 SEVENTEEN
  { 8, 5, 8}, // 18 EIGHTEEN
  { 6, 3, 8}, // 19 NINETEEN
  { 6, 0, 6}, // 20 TWENTY
};

// ---- Hour words (rows 9..11); index == the hour (1..12) -----------------------
static const WCWord wcHour[13] = {
  { 0, 0, 0}, //  0 unused (00 -> 12)
  {13,11, 3}, //  1 ONE
  {11,11, 3}, //  2 TWO
  { 5, 9, 5}, //  3 THREE
  { 7,11, 4}, //  4 FOUR
  { 3,11, 4}, //  5 FIVE
  { 0,11, 3}, //  6 SIX
  { 0, 9, 5}, //  7 SEVEN
  { 0,10, 5}, //  8 EIGHT
  { 6,10, 4}, //  9 NINE
  { 4,10, 3}, // 10 TEN  (the "TEN" inside row 10 "EIGHTEN")
  {10, 9, 6}, // 11 ELEVEN
  {10,10, 6}, // 12 TWELVE
};

// Config/state mirrors, maintained by the usermod, read by the (free) effect function.
static bool    wc16_showPeriod = true;
static bool    wc16_showTemp   = false;   // light a temperature word on the bottom row
static uint8_t wc16_tempBand   = 0;       // 0 none, 1 COLD, 2 COOL, 3 WARM, 4 HOT

// Live temperature pathway (shared by the JSON API and the sensor bridge below).
// Stored in the configured display unit; wc16_fahrenheit mirrors the unit config so the
// Celsius bridge can convert on entry.
static bool          wc16_fahrenheit = false;
static float         wc16_liveTemp   = 0.0f;
static bool          wc16_liveValid  = false;
static unsigned long wc16_liveTime   = 0;

// Weak-symbol bridge: any companion sensor usermod can push a Celsius reading without a
// hard link dependency by declaring this symbol weak and calling it only if present:
//   extern "C" void wc16_setLiveTempC(float) __attribute__((weak));
//   if (wc16_setLiveTempC) wc16_setLiveTempC(tempC);
extern "C" void wc16_setLiveTempC(float celsius) {
  wc16_liveTemp  = wc16_fahrenheit ? (celsius * 9.0f / 5.0f + 32.0f) : celsius;
  wc16_liveValid = true;
  wc16_liveTime  = millis();
}

// OR a word's cells into a 16-row bitmask (bit x of row y == cell lit).
static inline void wcSet(uint16_t *mask, const WCWord &w) {
  if (w.len == 0 || w.y > 15) return;
  for (uint8_t i = 0; i < w.len && (w.x + i) < 16; i++) {
    mask[w.y] |= (uint16_t)(1u << (w.x + i));
  }
}

// Light the spoken minute value (1..30) in the top region.
static void wcSetMinuteValue(uint16_t *mask, int val) {
  if (val == 15)      { wcSet(mask, wcQUARTER); return; } // no MINUTES word
  else if (val == 30) { wcSet(mask, wcHALF);    return; } // no MINUTES word
  if (val <= 20) {
    wcSet(mask, wcMinuteNum[val]);
  } else { // 21..29 -> TWENTY + ones
    wcSet(mask, wcMinuteNum[20]);
    wcSet(mask, wcMinuteNum[val - 20]);
  }
  wcSet(mask, wcMINUTES);
}

static inline void wcSetHour(uint16_t *mask, int h12) {
  if (h12 < 1 || h12 > 12) return;
  wcSet(mask, wcHour[h12]);
}

// Build the full active-letter bitmap from 24h hour and minute.
static void wcBuildMask(uint16_t *mask, int h24, int m) {
  for (int y = 0; y < 16; y++) mask[y] = 0;

  wcSet(mask, wcIT);
  wcSet(mask, wcIS);

  int h12;
  if (m == 0) {
    h12 = h24 % 12; if (h12 == 0) h12 = 12;
    wcSetHour(mask, h12);
    wcSet(mask, wcOCLOCK);
  } else if (m <= 30) {              // ... PAST <this hour>
    h12 = h24 % 12; if (h12 == 0) h12 = 12;
    wcSetMinuteValue(mask, m);
    wcSet(mask, wcPAST);
    wcSetHour(mask, h12);
  } else {                          // ... UNTIL <next hour>
    int hn = (h24 + 1) % 24;
    h12 = hn % 12; if (h12 == 0) h12 = 12;
    wcSetMinuteValue(mask, 60 - m);
    wcSet(mask, wcUNTIL);
    wcSetHour(mask, h12);
  }

  if (wc16_showPeriod) {            // period of day based on real 24h hour
    if (h24 >= 5 && h24 < 12)       { wcSet(mask, wcIN); wcSet(mask, wcTHE); wcSet(mask, wcMORNING); }
    else if (h24 >= 12 && h24 < 17) { wcSet(mask, wcIN); wcSet(mask, wcTHE); wcSet(mask, wcAFTERNOON); }
    else if (h24 >= 17 && h24 < 21) { wcSet(mask, wcIN); wcSet(mask, wcTHE); wcSet(mask, wcEVENING); }
    else                            { wcSet(mask, wcAT); wcSet(mask, wcNIGHT); } // 21..04
  }

  if (wc16_showTemp && wc16_tempBand >= 1 && wc16_tempBand <= 4) {
    wcSet(mask, wcTempWord[wc16_tempBand]); // WARM/COOL/HOT/COLD on the bottom row
  }
}

// ---- The effect ---------------------------------------------------------------
void mode_word_clock_16x16(void) {
  // Requires a 2D matrix; fall back to solid color otherwise.
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); return; }

  const int cols = SEG_W;
  const int rows = SEG_H;

  const int h24 = hour(localTime);
  const int m   = minute(localTime);

  // Recompute the letter map only when the minute changes. On a change we keep the
  // previous map and crossfade to the new one over the segment's transition time, so
  // minute-to-minute changes fade in/out like a normal effect transition.
  static uint16_t curMask[16];
  static uint16_t prevMask[16];
  static unsigned long transStart = 0;
  static uint8_t lastBand = 255;
  const uint16_t stamp = (uint16_t)(h24 * 60 + m);
  if (SEGENV.call == 0 || SEGENV.aux0 != stamp || lastBand != wc16_tempBand) {
    for (int y = 0; y < 16; y++) prevMask[y] = (SEGENV.call == 0) ? 0 : curMask[y];
    wcBuildMask(curMask, h24, m);
    transStart = strip.now;
    SEGENV.aux0 = stamp;
    lastBand = wc16_tempBand;
  }

  // Crossfade progress 0..255 driven by the segment/global transition setting.
  const uint16_t dur = strip.getTransition();
  uint8_t prog = 255;
  if (dur > 0) {
    const unsigned long el = strip.now - transStart;
    prog = (el >= dur) ? 255 : (uint8_t)((el * 255) / dur);
  }

  const bool usePalette = SEGMENT.palette;
  const uint8_t bg = SEGMENT.intensity;            // "Background" dim level (0 = off)
  const int span = (cols * rows) > 0 ? (cols * rows) : 1;

  for (int y = 0; y < rows; y++) {
    const uint16_t curRow  = (y < 16) ? curMask[y]  : 0;
    const uint16_t prevRow = (y < 16) ? prevMask[y] : 0;
    for (int x = 0; x < cols; x++) {
      const bool nowOn = (x < 16) && (curRow  & (uint16_t)(1u << x));
      const bool wasOn = (x < 16) && (prevRow & (uint16_t)(1u << x));
      uint32_t base = usePalette
        ? SEGMENT.color_from_palette((uint16_t)((x + y * cols) * 255 / span), true, false, 0)
        : SEGCOLOR(0);
      const uint32_t bgCol = bg ? color_fade(base, bg >> 2, true) : 0; // background ~1/4 brightness
      const uint32_t from = wasOn ? base : bgCol;
      const uint32_t to   = nowOn ? base : bgCol;
      SEGMENT.setPixelColorXY(x, y, (from == to) ? to : color_blend(from, to, prog));
    }
  }
}

// Effect metadata: name @ <speed(hidden)>,<intensity="Background"> ; color1 ; palette ; 2D ; default ix=0
static const char _data_FX_mode_word_clock_16x16[] PROGMEM = "Word Clock 16x16@,Background;!;!;2;ix=0";

// ---- Usermod (thin wrapper: registers the effect + a tiny config) -------------
class WordClock16x16Usermod : public Usermod {
  private:
    bool enabled    = true;
    bool showPeriod = true;
    bool initDone   = false;

    // Temperature words. All temperature numbers below (thresholds, manual value, and
    // the JSON-API "temp" value) are in the unit chosen by tempFahrenheit, so band
    // selection is a plain numeric comparison and needs no conversion.
    bool  showTemp      = false;
    bool  tempFahrenheit= false;
    float thrColdCool   = 10.0f;  // below this -> COLD
    float thrCoolWarm   = 18.0f;  // below this -> COOL
    float thrWarmHot    = 27.0f;  // below this -> WARM, at/above -> HOT
    float manualTemp    = 20.0f;  // fallback when no live value is available

    // Live value pushed via JSON API or the sensor bridge; falls back to manualTemp once stale.
    static constexpr unsigned long LIVE_TTL = 30UL * 60UL * 1000UL; // 30 min
    unsigned long lastEval  = 0;
    float         curTemp   = 0.0f;       // last resolved value (for the Info page)

    static const char _name[];
    static const char _enabled[];
    static const char _showPeriod[];
    static const char _showTemp[];
    static const char _fahrenheit[];
    static const char _thrColdCool[];
    static const char _thrCoolWarm[];
    static const char _thrWarmHot[];
    static const char _manualTemp[];

    uint8_t bandFor(float t) const {
      if (!showTemp) return 0;
      if (t < thrColdCool) return 1; // COLD
      if (t < thrCoolWarm) return 2; // COOL
      if (t < thrWarmHot)  return 3; // WARM
      return 4;                      // HOT
    }

  public:
    void setup() override {
      if (enabled) {
        strip.addEffect(255, &mode_word_clock_16x16, _data_FX_mode_word_clock_16x16);
      }
      wc16_showPeriod = showPeriod;
      wc16_showTemp   = showTemp;
      wc16_fahrenheit = tempFahrenheit;
      initDone = true;
    }

    void loop() override {
      if (!enabled) return;
      const unsigned long now = millis();
      if (now - lastEval < 1000) return; // re-evaluate at most once per second
      lastEval = now;

      curTemp = (wc16_liveValid && (now - wc16_liveTime) < LIVE_TTL) ? wc16_liveTemp : manualTemp;
      wc16_showTemp = showTemp;
      wc16_tempBand = bandFor(curTemp);
    }

    // Accept a temperature pushed via the JSON API, e.g.:
    //   {"WordClock16x16":{"temp":22.5}}   (value in the configured unit)
    void readFromJsonState(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) return;
      float t;
      if (getJsonValue(top[F("temp")], t)) { wc16_liveTemp = t; wc16_liveValid = true; wc16_liveTime = millis(); }
    }

    void addToJsonInfo(JsonObject &root) override {
      if (!showTemp) return;
      JsonObject user = root[F("u")];
      if (user.isNull()) user = root.createNestedObject(F("u"));
      static const char *bandWord[] = {"-", "COLD", "COOL", "WARM", "HOT"};
      char buf[24];
      snprintf(buf, sizeof(buf), "%.1f%s %s", curTemp, tempFahrenheit ? "°F" : "°C",
               bandWord[wc16_tempBand <= 4 ? wc16_tempBand : 0]);
      JsonArray arr = user.createNestedArray(F("Word Clock temperature"));
      arr.add(buf);
    }

    void addToConfig(JsonObject &root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)]     = enabled;
      top[FPSTR(_showPeriod)]  = showPeriod;
      top[FPSTR(_showTemp)]    = showTemp;
      top[FPSTR(_fahrenheit)]  = tempFahrenheit;
      top[FPSTR(_thrColdCool)] = thrColdCool;
      top[FPSTR(_thrCoolWarm)] = thrCoolWarm;
      top[FPSTR(_thrWarmHot)]  = thrWarmHot;
      top[FPSTR(_manualTemp)]  = manualTemp;
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)],     enabled);
      configComplete &= getJsonValue(top[FPSTR(_showPeriod)],  showPeriod);
      configComplete &= getJsonValue(top[FPSTR(_showTemp)],    showTemp);
      configComplete &= getJsonValue(top[FPSTR(_fahrenheit)],  tempFahrenheit);
      configComplete &= getJsonValue(top[FPSTR(_thrColdCool)], thrColdCool);
      configComplete &= getJsonValue(top[FPSTR(_thrCoolWarm)], thrCoolWarm);
      configComplete &= getJsonValue(top[FPSTR(_thrWarmHot)],  thrWarmHot);
      configComplete &= getJsonValue(top[FPSTR(_manualTemp)],  manualTemp);
      wc16_showPeriod = showPeriod;
      wc16_showTemp   = showTemp;
      wc16_fahrenheit = tempFahrenheit;
      return configComplete;
    }

    void appendConfigData() {
      oappend(F("addInfo('WordClock16x16:enabled', 1, 'reboot required to (un)register effect');"));
      oappend(F("addInfo('WordClock16x16:showPeriodOfDay', 1, 'light MORNING/AFTERNOON/EVENING/NIGHT');"));
      oappend(F("addInfo('WordClock16x16:showTemperature', 1, 'light WARM/COOL/HOT/COLD on bottom row');"));
      oappend(F("addInfo('WordClock16x16:fahrenheit', 1, 'thresholds & values in F (off = C)');"));
      oappend(F("addInfo('WordClock16x16:coldBelow', 1, 'COLD below, else COOL');"));
      oappend(F("addInfo('WordClock16x16:coolBelow', 1, 'COOL below, else WARM');"));
      oappend(F("addInfo('WordClock16x16:warmBelow', 1, 'WARM below, HOT at/above');"));
      oappend(F("addInfo('WordClock16x16:manualTemp', 1, 'used until a value is pushed via JSON API');"));
    }

    // No getId() override: this usermod needs no unique id (it isn't detected by other
    // usermods and exchanges no um_data), so it keeps USERMOD_ID_UNSPECIFIED and avoids
    // touching wled00/const.h. See the note at the USERMOD_ID list in const.h.
};

const char WordClock16x16Usermod::_name[]        PROGMEM = "WordClock16x16";
const char WordClock16x16Usermod::_enabled[]     PROGMEM = "enabled";
const char WordClock16x16Usermod::_showPeriod[]  PROGMEM = "showPeriodOfDay";
const char WordClock16x16Usermod::_showTemp[]    PROGMEM = "showTemperature";
const char WordClock16x16Usermod::_fahrenheit[]  PROGMEM = "fahrenheit";
const char WordClock16x16Usermod::_thrColdCool[] PROGMEM = "coldBelow";
const char WordClock16x16Usermod::_thrCoolWarm[] PROGMEM = "coolBelow";
const char WordClock16x16Usermod::_thrWarmHot[]  PROGMEM = "warmBelow";
const char WordClock16x16Usermod::_manualTemp[]  PROGMEM = "manualTemp";

static WordClock16x16Usermod usermod_v2_word_clock_16x16;
REGISTER_USERMOD(usermod_v2_word_clock_16x16);
