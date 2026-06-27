#include "wled.h"
#ifdef ESP8266
  #include <ESP8266HTTPClient.h>
#else
  #include <HTTPClient.h>
#endif

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
 * It also has an integrated Open-Meteo weather client (free, no API key) that:
 *   - feeds the outdoor temperature to the WARM/COOL/HOT/COLD words, and
 *   - applies a user-chosen preset when the weather state changes (e.g. a "rain" preset).
 * Temperature can also be pushed via the JSON API ({"WordClock16x16":{"temp":N}}).
 */

// A word is a horizontal run of letters: top-left cell (x,y) and length.
struct WCWord { uint8_t x, y, len; };

// ---- Fixed words / connectors -------------------------------------------------
static const WCWord wcIT        = { 0, 0, 2};
static const WCWord wcIS        = { 3, 0, 2};
static const WCWord wcMINUTES   = { 0, 7, 7};
static const WCWord wcQUARTER   = { 8, 7, 7};
static const WCWord wcA         = { 7, 5, 1}; // standalone 'A' (between SIXTEEN/EIGHTEEN) for "A QUARTER"
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
static const WCWord wcAmp = { 0,15, 1}; // '&' at LED 240 (row 15, col 0)
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

// Config/state mirrors maintained by the usermod, read by the (free) effect function.
static bool    wc16_showPeriod = true;
static bool    wc16_showTemp   = false;   // light a temperature word on the bottom row
static uint8_t wc16_tempBand   = 0;       // 0 none, 1 COLD, 2 COOL, 3 WARM, 4 HOT

// OR a word's cells into a 16-row bitmask (bit x of row y == cell lit).
static inline void wcSet(uint16_t *mask, const WCWord &w) {
  if (w.len == 0 || w.y > 15) return;
  for (uint8_t i = 0; i < w.len && (w.x + i) < 16; i++) {
    mask[w.y] |= (uint16_t)(1u << (w.x + i));
  }
}

// Light the spoken minute value (1..30) in the top region.
static void wcSetMinuteValue(uint16_t *mask, int val) {
  if (val == 15)      { wcSet(mask, wcA); wcSet(mask, wcQUARTER); return; } // "A QUARTER", no MINUTES word
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
    if (h24 < 12)                   { wcSet(mask, wcIN); wcSet(mask, wcTHE); wcSet(mask, wcMORNING); }   // 00..11 (after midnight is morning)
    else if (h24 < 17)              { wcSet(mask, wcIN); wcSet(mask, wcTHE); wcSet(mask, wcAFTERNOON); } // 12..16
    else if (h24 < 21)              { wcSet(mask, wcIN); wcSet(mask, wcTHE); wcSet(mask, wcEVENING); }   // 17..20
    else                            { wcSet(mask, wcAT); wcSet(mask, wcNIGHT); }                         // 21..23
  }

  if (wc16_showTemp && wc16_tempBand >= 1 && wc16_tempBand <= 4) {
    wcSet(mask, wcAmp);                      // '&' lights whenever a temperature word shows
    wcSet(mask, wcTempWord[wc16_tempBand]);  // WARM/COOL/HOT/COLD on the bottom row
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

// Weather states (index into preset table / state names). 0 = unknown.
enum WxState : uint8_t { WX_UNKNOWN=0, WX_CLEAR, WX_CLOUDS, WX_FOG, WX_DRIZZLE, WX_RAIN, WX_SNOW, WX_THUNDER, WX_COUNT };

// ---- Usermod: registers the effect, resolves temperature, drives weather/presets --
class WordClock16x16Usermod : public Usermod {
  private:
    bool enabled    = true;
    bool showPeriod = true;
    bool initDone   = false;

    // Temperature words. All temperature numbers (thresholds, manualTemp, JSON-API "temp")
    // are in the unit chosen by tempFahrenheit, so band selection is a plain comparison.
    bool  showTemp      = false;
    bool  tempFahrenheit= false;
    float thrColdCool   = 10.0f;  // below this -> COLD
    float thrCoolWarm   = 18.0f;  // below this -> COOL
    float thrWarmHot    = 27.0f;  // below this -> WARM, at/above -> HOT
    float manualTemp    = 20.0f;  // fallback when no live value is available

    // Live temperature (from Open-Meteo or the JSON API); falls back to manualTemp once stale.
    static constexpr unsigned long LIVE_TTL = 30UL * 60UL * 1000UL; // 30 min
    unsigned long lastEval  = 0;
    float         curTemp   = 0.0f;       // last resolved value (for the Info page)
    float         liveTemp  = 0.0f;       // in the configured display unit
    bool          liveValid = false;
    unsigned long liveTime  = 0;

    // Open-Meteo weather client.
    bool     fetchWeather    = false;
    bool     useWledLocation = true;
    bool     weatherPresets  = false;
    uint16_t fetchMinutes    = 15;
    String   place;                               // city or ZIP (used when not useWledLocation and set)
    float    latOverride     = 0.0f;
    float    lonOverride     = 0.0f;
    uint8_t  preset[WX_COUNT]= {0,0,0,0,0,0,0,0}; // preset id per state (0 = none)
    uint8_t  wxState         = WX_UNKNOWN;
    uint8_t  lastApplied     = WX_UNKNOWN;
    bool     firstFetchDone  = false;
    bool     forceFetch      = false;             // "Update now" request
    unsigned long lastFetch  = 0;
    float    humidity        = 0.0f;              // % relative humidity
    bool     haveHumidity    = false;
    // Geocoding cache for place -> coordinates.
    float    geoLat = 0.0f, geoLon = 0.0f;
    bool     geoDone = false;
    String   geoFor;                              // place string the geo* were resolved for

    static const char _name[];
    static const char _enabled[];
    static const char _showPeriod[];
    static const char _showTemp[];
    static const char _fahrenheit[];
    static const char _thrColdCool[];
    static const char _thrCoolWarm[];
    static const char _thrWarmHot[];
    static const char _manualTemp[];
    static const char _fetch[];
    static const char _useWled[];
    static const char _interval[];
    static const char _place[];
    static const char _lat[];
    static const char _lon[];
    static const char _presets[];
    static const char _pClear[];
    static const char _pClouds[];
    static const char _pFog[];
    static const char _pDrizzle[];
    static const char _pRain[];
    static const char _pSnow[];
    static const char _pThunder[];

    uint8_t bandFor(float t) const {
      if (!showTemp) return 0;
      if (t < thrColdCool) return 1; // COLD
      if (t < thrCoolWarm) return 2; // COOL
      if (t < thrWarmHot)  return 3; // WARM
      return 4;                      // HOT
    }

    void setTempCelsius(float c) {           // from Open-Meteo (always Celsius)
      liveTemp = tempFahrenheit ? (c * 9.0f / 5.0f + 32.0f) : c;
      liveValid = true; liveTime = millis();
    }

    float useLat() const {
      if (useWledLocation) return latitude;
      if (place.length())  return geoDone ? geoLat : 0.0f;
      return latOverride;
    }
    float useLon() const {
      if (useWledLocation) return longitude;
      if (place.length())  return geoDone ? geoLon : 0.0f;
      return lonOverride;
    }

    static String urlEncode(const String &s) {
      String o; char b[4];
      for (unsigned i = 0; i < s.length(); i++) {
        char c = s[i];
        if (isalnum((unsigned char)c)) o += c;
        else { sprintf(b, "%%%02X", (unsigned char)c); o += b; }
      }
      return o;
    }

    // Resolve place (city or ZIP) -> lat/lon via the Open-Meteo geocoding API.
    void geocode() {
      WiFiClient client;
      HTTPClient http;
      String url = F("http://geocoding-api.open-meteo.com/v1/search?count=1&name=");
      url += urlEncode(place);
      if (!http.begin(client, url)) return;
      http.setTimeout(5000);
      if (http.GET() == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<96> filter;
        filter["results"][0]["latitude"]  = true;
        filter["results"][0]["longitude"] = true;
        StaticJsonDocument<256> doc;
        if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
          JsonVariant la = doc["results"][0]["latitude"];
          JsonVariant lo = doc["results"][0]["longitude"];
          if (!la.isNull() && !lo.isNull()) {
            geoLat = la.as<float>(); geoLon = lo.as<float>();
            geoDone = true; geoFor = place;
          }
        }
      }
      http.end();
    }

    static const char* stateName(uint8_t s) {
      switch (s) {
        case WX_CLEAR:   return "clear";
        case WX_CLOUDS:  return "clouds";
        case WX_FOG:     return "fog";
        case WX_DRIZZLE: return "drizzle";
        case WX_RAIN:    return "rain";
        case WX_SNOW:    return "snow";
        case WX_THUNDER: return "thunder";
        default:         return "--";
      }
    }

    // Map a WMO weather interpretation code to a weather state.
    static uint8_t codeToState(int c) {
      if (c <= 1)                       return WX_CLEAR;     // 0 clear, 1 mainly clear
      if (c == 2 || c == 3)             return WX_CLOUDS;
      if (c == 45 || c == 48)           return WX_FOG;
      if (c >= 51 && c <= 57)           return WX_DRIZZLE;
      if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return WX_RAIN;
      if ((c >= 71 && c <= 77) || c == 85 || c == 86)   return WX_SNOW;
      if (c >= 95)                      return WX_THUNDER;   // 95,96,99
      return WX_UNKNOWN;
    }

    void applyStatePreset() {
      if (!weatherPresets || wxState == WX_UNKNOWN || wxState == lastApplied) return;
      const uint8_t p = preset[wxState];
      lastApplied = wxState;                 // remember even if 0, so we only act on changes
      if (p > 0) applyPreset(p, CALL_MODE_DIRECT_CHANGE);
    }

    void fetch() {
      if (!useWledLocation && place.length() && (!geoDone || geoFor != place)) geocode();
      const float la = useLat(), lo = useLon();
      if (la == 0.0f && lo == 0.0f) return;  // location not set

      WiFiClient client;                     // plain HTTP (Open-Meteo serves the API on :80)
      HTTPClient http;
      char url[208];
      snprintf(url, sizeof(url),
        "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,weather_code",
        la, lo);
      if (!http.begin(client, url)) return;
      http.setTimeout(5000);
      if (http.GET() == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<160> filter;
        filter["current"]["temperature_2m"]      = true;
        filter["current"]["relative_humidity_2m"]= true;
        filter["current"]["weather_code"]        = true;
        StaticJsonDocument<256> doc;
        if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
          JsonVariant t = doc["current"]["temperature_2m"];
          if (!t.isNull()) setTempCelsius(t.as<float>());
          JsonVariant h = doc["current"]["relative_humidity_2m"];
          if (!h.isNull()) { humidity = h.as<float>(); haveHumidity = true; }
          JsonVariant w = doc["current"]["weather_code"];
          if (!w.isNull()) { wxState = codeToState(w.as<int>()); applyStatePreset(); }
        }
      }
      http.end();
    }

  public:
    void setup() override {
      if (enabled) {
        strip.addEffect(255, &mode_word_clock_16x16, _data_FX_mode_word_clock_16x16);
      }
      wc16_showPeriod = showPeriod;
      wc16_showTemp   = showTemp;
      initDone = true;
    }

    void loop() override {
      if (!enabled) return;
      const unsigned long now = millis();

      // Resolve temperature -> band at most once per second.
      if (now - lastEval >= 1000) {
        lastEval = now;
        curTemp = (liveValid && (now - liveTime) < LIVE_TTL) ? liveTemp : manualTemp;
        wc16_showTemp = showTemp;
        wc16_tempBand = bandFor(curTemp);
      }

      if (!WLED_CONNECTED) return;

      // "Update now" request (from the settings button / JSON API).
      if (forceFetch) { forceFetch = false; fetch(); lastFetch = now; firstFetchDone = true; return; }

      // Periodic Open-Meteo fetch (temperature + humidity + weather state/presets).
      if (fetchWeather) {
        if (!firstFetchDone) {
          if (now >= 30000) { fetch(); firstFetchDone = true; lastFetch = now; } // settle 30 s after boot
        } else if (now - lastFetch >= (unsigned long)fetchMinutes * 60000UL) {
          fetch(); lastFetch = now;
        }
      }
    }

    // JSON API: {"WordClock16x16":{"temp":22.5}} pushes a temperature (configured unit);
    //           {"WordClock16x16":{"update":true}} fetches weather now.
    void readFromJsonState(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) return;
      float t;
      if (getJsonValue(top[F("temp")], t)) { liveTemp = t; liveValid = true; liveTime = millis(); }
      if (top[F("update")].as<bool>()) forceFetch = true;
    }

    void addToJsonInfo(JsonObject &root) override {
      if (!showTemp && !fetchWeather) return;
      JsonObject user = root[F("u")];
      if (user.isNull()) user = root.createNestedObject(F("u"));
      char buf[24];

      snprintf(buf, sizeof(buf), "%.1f", curTemp);
      JsonArray aT = user.createNestedArray(F("Word Clock temperature"));
      aT.add(buf); aT.add(tempFahrenheit ? F(" °F") : F(" °C"));

      if (fetchWeather) {
        if (haveHumidity) {
          snprintf(buf, sizeof(buf), "%.0f", humidity);
          JsonArray aH = user.createNestedArray(F("Word Clock humidity"));
          aH.add(buf); aH.add(F(" %"));
        }
        JsonArray aC = user.createNestedArray(F("Word Clock condition"));
        aC.add(stateName(wxState));
      }
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
      top[FPSTR(_fetch)]       = fetchWeather;
      top[FPSTR(_useWled)]     = useWledLocation;
      top[FPSTR(_interval)]    = fetchMinutes;
      top[FPSTR(_place)]       = place;
      top[FPSTR(_lat)]         = latOverride;
      top[FPSTR(_lon)]         = lonOverride;
      top[FPSTR(_presets)]     = weatherPresets;
      top[FPSTR(_pClear)]      = preset[WX_CLEAR];
      top[FPSTR(_pClouds)]     = preset[WX_CLOUDS];
      top[FPSTR(_pFog)]        = preset[WX_FOG];
      top[FPSTR(_pDrizzle)]    = preset[WX_DRIZZLE];
      top[FPSTR(_pRain)]       = preset[WX_RAIN];
      top[FPSTR(_pSnow)]       = preset[WX_SNOW];
      top[FPSTR(_pThunder)]    = preset[WX_THUNDER];
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
      configComplete &= getJsonValue(top[FPSTR(_fetch)],       fetchWeather);
      configComplete &= getJsonValue(top[FPSTR(_useWled)],     useWledLocation);
      configComplete &= getJsonValue(top[FPSTR(_interval)],    fetchMinutes);
      configComplete &= getJsonValue(top[FPSTR(_place)],       place);
      configComplete &= getJsonValue(top[FPSTR(_lat)],         latOverride);
      configComplete &= getJsonValue(top[FPSTR(_lon)],         lonOverride);
      if (geoFor != place) geoDone = false; // place changed -> re-geocode on next fetch
      configComplete &= getJsonValue(top[FPSTR(_presets)],     weatherPresets);
      configComplete &= getJsonValue(top[FPSTR(_pClear)],      preset[WX_CLEAR]);
      configComplete &= getJsonValue(top[FPSTR(_pClouds)],     preset[WX_CLOUDS]);
      configComplete &= getJsonValue(top[FPSTR(_pFog)],        preset[WX_FOG]);
      configComplete &= getJsonValue(top[FPSTR(_pDrizzle)],    preset[WX_DRIZZLE]);
      configComplete &= getJsonValue(top[FPSTR(_pRain)],       preset[WX_RAIN]);
      configComplete &= getJsonValue(top[FPSTR(_pSnow)],       preset[WX_SNOW]);
      configComplete &= getJsonValue(top[FPSTR(_pThunder)],    preset[WX_THUNDER]);
      if (fetchMinutes < 1) fetchMinutes = 1;
      wc16_showPeriod = showPeriod;
      wc16_showTemp   = showTemp;
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
      oappend(F("addInfo('WordClock16x16:manualTemp', 1, 'used until a live value is available');"));
      oappend(F("addInfo('WordClock16x16:fetchWeather', 1, 'fetch temperature/weather from Open-Meteo');"));
      oappend(F("addInfo('WordClock16x16:useWledLocation', 1, 'use Time-settings lat/lon (off = use Place or lat/lon below)');"));
      oappend(F("addInfo('WordClock16x16:fetchMinutes', 1, 'minutes between fetches');"));
      oappend(F("addInfo('WordClock16x16:place', 1, 'city or ZIP (geocoded); leave blank to use lat/lon');"));
      oappend(F("addInfo('WordClock16x16:longitude', 1, \"<a href='https://www.latlong.net' target='_blank'>find lat/lon</a>\");"));
      oappend(F("addInfo('WordClock16x16:weatherPresets', 1, 'apply a preset when weather changes');"));
      // "Update now" button -> POST {"WordClock16x16":{"update":true}} to /json/state.
      oappend(F("wc16upd=function(){fetch('/json/state',{method:'POST',headers:{'Content-Type':'application/json'},body:'{\"WordClock16x16\":{\"update\":true}}'});};"));
      oappend(F("addInfo('WordClock16x16:fetchMinutes', 1, \"&nbsp;<button type='button' onclick='wc16upd()'>Update now</button>\");"));
      // Turn the per-state preset number fields into dropdowns populated from /presets.json.
      oappend(F("(function(){function f(j){var ks=['presetClear','presetClouds','presetFog','presetDrizzle','presetRain','presetSnow','presetThunder'];"
                "for(var i=0;i<ks.length;i++){var dd=addDropdown('WordClock16x16',ks[i]);if(!dd)continue;addOption(dd,'None',0);"
                "for(var p of Object.entries(j)){if(p[0]==='0')continue;var n=(p[1]&&p[1].n)?p[1].n:('Preset '+p[0]);addOption(dd,p[0]+': '+n,p[0]);}}}"
                "fetch('/presets.json').then(function(r){return r.json();}).then(f).catch(function(){});})();"));
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
const char WordClock16x16Usermod::_fetch[]       PROGMEM = "fetchWeather";
const char WordClock16x16Usermod::_useWled[]     PROGMEM = "useWledLocation";
const char WordClock16x16Usermod::_interval[]    PROGMEM = "fetchMinutes";
const char WordClock16x16Usermod::_place[]       PROGMEM = "place";
const char WordClock16x16Usermod::_lat[]         PROGMEM = "latitude";
const char WordClock16x16Usermod::_lon[]         PROGMEM = "longitude";
const char WordClock16x16Usermod::_presets[]     PROGMEM = "weatherPresets";
const char WordClock16x16Usermod::_pClear[]      PROGMEM = "presetClear";
const char WordClock16x16Usermod::_pClouds[]     PROGMEM = "presetClouds";
const char WordClock16x16Usermod::_pFog[]        PROGMEM = "presetFog";
const char WordClock16x16Usermod::_pDrizzle[]    PROGMEM = "presetDrizzle";
const char WordClock16x16Usermod::_pRain[]       PROGMEM = "presetRain";
const char WordClock16x16Usermod::_pSnow[]       PROGMEM = "presetSnow";
const char WordClock16x16Usermod::_pThunder[]    PROGMEM = "presetThunder";

static WordClock16x16Usermod usermod_v2_word_clock_16x16;
REGISTER_USERMOD(usermod_v2_word_clock_16x16);
