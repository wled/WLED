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

// User-config mirror, read in readFromConfig(), used by the (free) effect function.
static bool wc16_showPeriod = true;

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
  const uint16_t stamp = (uint16_t)(h24 * 60 + m);
  if (SEGENV.call == 0 || SEGENV.aux0 != stamp) {
    for (int y = 0; y < 16; y++) prevMask[y] = (SEGENV.call == 0) ? 0 : curMask[y];
    wcBuildMask(curMask, h24, m);
    transStart = strip.now;
    SEGENV.aux0 = stamp;
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

    static const char _name[];
    static const char _enabled[];
    static const char _showPeriod[];

  public:
    void setup() override {
      if (enabled) {
        strip.addEffect(255, &mode_word_clock_16x16, _data_FX_mode_word_clock_16x16);
      }
      wc16_showPeriod = showPeriod;
      initDone = true;
    }

    void loop() override {}

    void addToConfig(JsonObject &root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)]    = enabled;
      top[FPSTR(_showPeriod)] = showPeriod;
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_showPeriod)], showPeriod);
      wc16_showPeriod = showPeriod;
      return configComplete;
    }

    void appendConfigData() {
      oappend(F("addInfo('WordClock16x16:enabled', 1, 'reboot required to (un)register effect');"));
      oappend(F("addInfo('WordClock16x16:showPeriodOfDay', 1, 'light MORNING/AFTERNOON/EVENING/NIGHT');"));
    }

    // No getId() override: this usermod needs no unique id (it isn't detected by other
    // usermods and exchanges no um_data), so it keeps USERMOD_ID_UNSPECIFIED and avoids
    // touching wled00/const.h. See the note at the USERMOD_ID list in const.h.
};

const char WordClock16x16Usermod::_name[]       PROGMEM = "WordClock16x16";
const char WordClock16x16Usermod::_enabled[]    PROGMEM = "enabled";
const char WordClock16x16Usermod::_showPeriod[] PROGMEM = "showPeriodOfDay";

static WordClock16x16Usermod usermod_v2_word_clock_16x16;
REGISTER_USERMOD(usermod_v2_word_clock_16x16);
