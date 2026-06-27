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
// ICE = freezing rain/drizzle; HAIL = thunderstorm w/ hail; HEAT/WIND derived from
// temperature / wind gusts (Oklahoma: heat waves, hail, ice storms, high wind).
enum WxState : uint8_t {
  WX_UNKNOWN=0, WX_CLEAR, WX_CLOUDS, WX_FOG, WX_DRIZZLE, WX_RAIN, WX_SNOW, WX_THUNDER,
  WX_ICE, WX_HAIL, WX_HEAT, WX_WIND, WX_COUNT
};

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
    float    heatAbove       = 35.0f;             // temp (display unit) >= this -> HEAT (clear/cloudy)
    float    windAbove       = 60.0f;             // wind gust (km/h) >= this -> WIND (clear/cloudy)
    float    windGust        = 0.0f;
    bool     haveWind        = false;
    uint8_t  preset[WX_COUNT] = {};               // preset id per state (0 = none)
    uint8_t  wxState         = WX_UNKNOWN;
    uint8_t  lastApplied     = WX_UNKNOWN;
    bool     firstFetchDone  = false;
    bool     forceFetch      = false;             // "Update now" request
    bool     fetchSoon       = false;             // scheduled shortly after a (re)connect
    unsigned long fetchSoonAt= 0;
    uint8_t  pendingTest     = 0;                 // force-apply this state's preset (test)
    bool     lastTryFailed   = false;             // last fetch failed -> retry sooner
    unsigned long lastFetch  = 0;                 // last attempt
    unsigned long lastOkMs   = 0;                 // last successful weather parse
    bool     everOk          = false;
    static constexpr unsigned long RETRY_MS = 60UL * 1000UL; // retry 1 min after a failure
    float    humidity        = 0.0f;              // % relative humidity
    bool     haveHumidity    = false;
    // Geocoding cache for place -> coordinates.
    float    geoLat = 0.0f, geoLon = 0.0f;
    bool     geoDone = false;
    bool     geoFailed = false;                   // place could not be resolved
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
    static const char _heatAbove[];
    static const char _windAbove[];
    static const char _pClear[];
    static const char _pClouds[];
    static const char _pFog[];
    static const char _pDrizzle[];
    static const char _pRain[];
    static const char _pSnow[];
    static const char _pThunder[];
    static const char _pIce[];
    static const char _pHail[];
    static const char _pHeat[];
    static const char _pWind[];

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

    bool wledLocSet() const { return latitude != 0.0f || longitude != 0.0f; }

    // Location precedence: WLED Time-settings coords (if enabled AND actually set) ->
    // geocoded Place -> manual lat/lon. The "is it set" check means a Place still works
    // even with "use WLED location" ticked when WLED's coords are 0,0.
    float useLat() const {
      if (useWledLocation && wledLocSet()) return latitude;
      if (place.length())                  return geoDone ? geoLat : 0.0f;
      return latOverride;
    }
    float useLon() const {
      if (useWledLocation && wledLocSet()) return longitude;
      if (place.length())                  return geoDone ? geoLon : 0.0f;
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
    // Open-Meteo only matches the bare name, so drop any ", State/Country" qualifier.
    void geocode() {
      geoFor = place; geoDone = false; geoFailed = false;
      String name = place;
      int comma = name.indexOf(',');
      if (comma >= 0) name = name.substring(0, comma);
      name.trim();
      if (!name.length()) { geoFailed = true; return; }
      WiFiClient client;
      HTTPClient http;
      String url = F("http://geocoding-api.open-meteo.com/v1/search?count=1&name=");
      url += urlEncode(name);
      if (!http.begin(client, url)) { geoFailed = true; return; }
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
            geoDone = true;
          }
        }
      }
      http.end();
      if (!geoDone) geoFailed = true;
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
        case WX_ICE:     return "ice";
        case WX_HAIL:    return "hail";
        case WX_HEAT:    return "heat";
        case WX_WIND:    return "wind";
        default:         return "--";
      }
    }

    // Map a WMO weather interpretation code to a weather state.
    static uint8_t codeToState(int c) {
      if (c <= 1)                       return WX_CLEAR;     // 0 clear, 1 mainly clear
      if (c == 2 || c == 3)             return WX_CLOUDS;
      if (c == 45 || c == 48)           return WX_FOG;
      if (c == 56 || c == 57 || c == 66 || c == 67) return WX_ICE; // freezing drizzle/rain
      if (c >= 51 && c <= 55)           return WX_DRIZZLE;
      if ((c >= 61 && c <= 65) || (c >= 80 && c <= 82)) return WX_RAIN;
      if ((c >= 71 && c <= 77) || c == 85 || c == 86)   return WX_SNOW;
      if (c == 96 || c == 99)           return WX_HAIL;     // thunderstorm with hail
      if (c >= 95)                      return WX_THUNDER;   // 95
      return WX_UNKNOWN;
    }

    // Final state: condition code, with HEAT/WIND derived on otherwise calm skies.
    uint8_t computeState(int code, float tempDisp, float gust) const {
      uint8_t s = codeToState(code);
      if (s == WX_CLEAR || s == WX_CLOUDS) {     // only override on calm-sky conditions
        if (tempDisp >= heatAbove)      s = WX_HEAT;
        else if (gust  >= windAbove)    s = WX_WIND;
      }
      return s;
    }

    void applyStatePreset() {
      if (!weatherPresets || wxState == WX_UNKNOWN || wxState == lastApplied) return;
      const uint8_t p = preset[wxState];
      lastApplied = wxState;                 // remember even if 0, so we only act on changes
      if (p > 0) applyPreset(p, CALL_MODE_DIRECT_CHANGE);
    }

    bool fetch() {
      const bool needPlace = !(useWledLocation && wledLocSet()) && place.length();
      if (needPlace && (!geoDone || geoFor != place)) geocode();
      const float la = useLat(), lo = useLon();
      if (la == 0.0f && lo == 0.0f) return false; // location not set/unresolved

      WiFiClient client;                     // plain HTTP (Open-Meteo serves the API on :80)
      HTTPClient http;
      char url[224];
      snprintf(url, sizeof(url),
        "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
        "&current=temperature_2m,relative_humidity_2m,weather_code,wind_gusts_10m",
        la, lo);
      if (!http.begin(client, url)) return false;
      http.setTimeout(5000);
      bool ok = false;
      if (http.GET() == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<192> filter;
        filter["current"]["temperature_2m"]      = true;
        filter["current"]["relative_humidity_2m"]= true;
        filter["current"]["weather_code"]        = true;
        filter["current"]["wind_gusts_10m"]      = true;
        StaticJsonDocument<320> doc;
        if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
          JsonObject cur = doc["current"];
          float tC = NAN;
          JsonVariant t = cur["temperature_2m"];
          if (!t.isNull()) { tC = t.as<float>(); setTempCelsius(tC); ok = true; }
          JsonVariant h = cur["relative_humidity_2m"];
          if (!h.isNull()) { humidity = h.as<float>(); haveHumidity = true; }
          JsonVariant g = cur["wind_gusts_10m"];
          if (!g.isNull()) { windGust = g.as<float>(); haveWind = true; }
          JsonVariant w = cur["weather_code"];
          if (!w.isNull()) {
            const float tDisp = isnan(tC) ? curTemp : (tempFahrenheit ? tC * 9.0f/5.0f + 32.0f : tC);
            wxState = computeState(w.as<int>(), tDisp, haveWind ? windGust : 0.0f);
            applyStatePreset();
            ok = true;
          }
        }
      }
      http.end();
      if (ok) { lastOkMs = millis(); everOk = true; }
      return ok;
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

    // Called whenever WiFi (re)connects (like NTP): fetch weather shortly after, and
    // reset lastApplied so the matching preset is re-applied for the current weather.
    void connected() override {
      if (!enabled) return;
      fetchSoon   = true;
      fetchSoonAt = millis() + 3000; // let the network stack/NTP settle first
      lastApplied = WX_UNKNOWN;
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

      // Force-apply a weather state's preset for testing (bypasses the change check).
      if (pendingTest) {
        const uint8_t s = pendingTest; pendingTest = 0;
        wxState = s; lastApplied = s; everOk = true;
        if (preset[s] > 0) applyPreset(preset[s], CALL_MODE_DIRECT_CHANGE);
      }

      if (!WLED_CONNECTED) return;

      // "Update now" request (from the settings button / JSON API).
      if (forceFetch) { forceFetch = false; lastTryFailed = !fetch(); lastFetch = now; firstFetchDone = true; return; }

      if (!fetchWeather) return;

      if (fetchSoon) {                              // shortly after a (re)connect
        if ((long)(now - fetchSoonAt) >= 0) { fetchSoon = false; lastTryFailed = !fetch(); lastFetch = now; firstFetchDone = true; }
      } else if (!firstFetchDone) {                 // fallback if connected() never fired
        if (now >= 30000) { lastTryFailed = !fetch(); lastFetch = now; firstFetchDone = true; }
      } else {                                      // periodic; retry sooner after a failure
        const unsigned long due = lastTryFailed ? RETRY_MS : (unsigned long)fetchMinutes * 60000UL;
        if (now - lastFetch >= due) { lastTryFailed = !fetch(); lastFetch = now; }
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
      int n;
      if (getJsonValue(top[F("wxtest")], n) && n >= 1 && n < WX_COUNT) pendingTest = (uint8_t)n;
    }

    void addToJsonInfo(JsonObject &root) override {
      const bool wx = fetchWeather || everOk;   // also show once a manual fetch has succeeded
      if (!showTemp && !wx) return;
      JsonObject user = root[F("u")];
      if (user.isNull()) user = root.createNestedObject(F("u"));
      char buf[24];

      snprintf(buf, sizeof(buf), "%.1f", curTemp);
      JsonArray aT = user.createNestedArray(F("Word Clock temperature"));
      aT.add(buf); aT.add(tempFahrenheit ? F(" °F") : F(" °C"));

      if (wx) {
        if (haveHumidity) {
          snprintf(buf, sizeof(buf), "%.0f", humidity);
          JsonArray aH = user.createNestedArray(F("Word Clock humidity"));
          aH.add(buf); aH.add(F(" %"));
        }
        JsonArray aC = user.createNestedArray(F("Word Clock condition"));
        aC.add(stateName(wxState));

        char loc[52];
        locationInfo(loc, sizeof(loc));
        JsonArray aL = user.createNestedArray(F("Word Clock location"));
        aL.add(loc);

        JsonArray aU = user.createNestedArray(F("Word Clock updated"));
        if (!everOk) aU.add(F("never"));
        else {
          const unsigned long s = (millis() - lastOkMs) / 1000UL;
          if (s < 90)        { snprintf(buf, sizeof(buf), "%lus ago", s); aU.add(buf); }
          else               { snprintf(buf, sizeof(buf), "%lum ago", s / 60UL); aU.add(buf); }
        }
      }
    }

    void locationInfo(char *buf, size_t n) {
      if (useWledLocation && wledLocSet()) {
        snprintf(buf, n, "%.2f, %.2f (WLED)", latitude, longitude);
      } else if (place.length()) {
        if (geoDone)        snprintf(buf, n, "%s (%.2f, %.2f)", place.c_str(), geoLat, geoLon);
        else if (geoFailed) snprintf(buf, n, "'%s' not found", place.c_str());
        else                snprintf(buf, n, "geocoding...");
      } else if (latOverride != 0.0f || lonOverride != 0.0f) {
        snprintf(buf, n, "%.2f, %.2f", latOverride, lonOverride);
      } else {
        snprintf(buf, n, "location unset");
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
      top[FPSTR(_heatAbove)]   = heatAbove;
      top[FPSTR(_windAbove)]   = windAbove;
      top[FPSTR(_pClear)]      = preset[WX_CLEAR];
      top[FPSTR(_pClouds)]     = preset[WX_CLOUDS];
      top[FPSTR(_pFog)]        = preset[WX_FOG];
      top[FPSTR(_pDrizzle)]    = preset[WX_DRIZZLE];
      top[FPSTR(_pRain)]       = preset[WX_RAIN];
      top[FPSTR(_pSnow)]       = preset[WX_SNOW];
      top[FPSTR(_pThunder)]    = preset[WX_THUNDER];
      top[FPSTR(_pIce)]        = preset[WX_ICE];
      top[FPSTR(_pHail)]       = preset[WX_HAIL];
      top[FPSTR(_pHeat)]       = preset[WX_HEAT];
      top[FPSTR(_pWind)]       = preset[WX_WIND];
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
      configComplete &= getJsonValue(top[FPSTR(_heatAbove)],   heatAbove);
      configComplete &= getJsonValue(top[FPSTR(_windAbove)],   windAbove);
      configComplete &= getJsonValue(top[FPSTR(_pClear)],      preset[WX_CLEAR]);
      configComplete &= getJsonValue(top[FPSTR(_pClouds)],     preset[WX_CLOUDS]);
      configComplete &= getJsonValue(top[FPSTR(_pFog)],        preset[WX_FOG]);
      configComplete &= getJsonValue(top[FPSTR(_pDrizzle)],    preset[WX_DRIZZLE]);
      configComplete &= getJsonValue(top[FPSTR(_pRain)],       preset[WX_RAIN]);
      configComplete &= getJsonValue(top[FPSTR(_pSnow)],       preset[WX_SNOW]);
      configComplete &= getJsonValue(top[FPSTR(_pThunder)],    preset[WX_THUNDER]);
      configComplete &= getJsonValue(top[FPSTR(_pIce)],        preset[WX_ICE]);
      configComplete &= getJsonValue(top[FPSTR(_pHail)],       preset[WX_HAIL]);
      configComplete &= getJsonValue(top[FPSTR(_pHeat)],       preset[WX_HEAT]);
      configComplete &= getJsonValue(top[FPSTR(_pWind)],       preset[WX_WIND]);
      if (fetchMinutes < 1) fetchMinutes = 1;
      wc16_showPeriod = showPeriod;
      wc16_showTemp   = showTemp;
      return configComplete;
    }

    void appendConfigData() {
      // ---- styling ------------------------------------------------------------
      oappend(F("(function(){var s=document.createElement('style');s.innerHTML="
                "'.wc16h{margin:18px 0 6px;padding-bottom:3px;font-weight:600;color:#4aa3ff;border-bottom:1px solid #2c2c2c;letter-spacing:.3px}'"
                "+'#wc16stat,.wc16card{background:#101010;border:1px solid #2c2c2c;border-radius:8px;padding:9px 11px;margin:6px 0;display:block;line-height:1.7}'"
                "+'#um button{cursor:pointer;border-radius:6px;padding:3px 10px}';document.head.appendChild(s);})();"));

      // ---- concise field help -------------------------------------------------
      oappend(F("addInfo('WordClock16x16:enabled', 1, 'reboot to (un)register the effect');"));
      oappend(F("addInfo('WordClock16x16:coldBelow', 1, 'COLD below, else COOL');"));
      oappend(F("addInfo('WordClock16x16:coolBelow', 1, 'COOL below, else WARM');"));
      oappend(F("addInfo('WordClock16x16:warmBelow', 1, 'WARM below, HOT at/above');"));
      oappend(F("addInfo('WordClock16x16:manualTemp', 1, 'used until a live value arrives');"));
      oappend(F("addInfo('WordClock16x16:useWledLocation', 1, 'falls back to Place if WLED coords unset');"));
      oappend(F("addInfo('WordClock16x16:place', 1, 'city or ZIP (\", State\" ignored; ZIP if ambiguous)');"));
      oappend(F("addInfo('WordClock16x16:longitude', 1, \"<a href='https://www.latlong.net' target='_blank'>find lat/lon</a>\");"));
      oappend(F("addInfo('WordClock16x16:heatAbove', 1, 'on clear/cloudy skies only');"));
      oappend(F("addInfo('WordClock16x16:windAbove', 1, 'gust km/h, clear/cloudy skies only');"));

      // ---- section headers + friendlier labels --------------------------------
      oappend(F("wc16sec=function(fld,t){var a=d.getElementsByName('WordClock16x16:'+fld);if(!a.length)return;"
                "var f=a[0],r=f.previousSibling;var h=document.createElement('div');h.className='wc16h';h.textContent=t;"
                "f.parentNode.insertBefore(h,(r&&r.nodeType===3)?r:f);};"));
      oappend(F("wc16lbl=function(fld,t){var a=d.getElementsByName('WordClock16x16:'+fld);if(!a.length)return;"
                "var r=a[0].previousSibling;if(r&&r.nodeType===3)r.textContent=' '+t+' ';};"));
      oappend(F("wc16sec('enabled','Display');wc16sec('showTemperature','Temperature words');"
                "wc16sec('fetchWeather','Weather (Open-Meteo)');wc16sec('useWledLocation','Location');"
                "wc16sec('weatherPresets','Weather \\u2192 Presets');"));
      oappend(F("wc16lbl('enabled','Enabled');wc16lbl('showPeriodOfDay','Period of day');"
                "wc16lbl('showTemperature','Temperature words');wc16lbl('fahrenheit','Use \\u00B0F');"
                "wc16lbl('coldBelow','Cold below');wc16lbl('coolBelow','Cool below');wc16lbl('warmBelow','Warm below');"
                "wc16lbl('manualTemp','Manual temp');"));
      oappend(F("wc16lbl('fetchWeather','Fetch weather');wc16lbl('useWledLocation','Use WLED location');"
                "wc16lbl('fetchMinutes','Every (min)');wc16lbl('place','Place');wc16lbl('latitude','Latitude');"
                "wc16lbl('longitude','Longitude');wc16lbl('weatherPresets','Enable presets');"
                "wc16lbl('heatAbove','Heat above');wc16lbl('windAbove','Wind gust above');"));
      oappend(F("wc16lbl('presetClear','Clear');wc16lbl('presetClouds','Clouds');wc16lbl('presetFog','Fog');"
                "wc16lbl('presetDrizzle','Drizzle');wc16lbl('presetRain','Rain');wc16lbl('presetSnow','Snow');"
                "wc16lbl('presetThunder','Thunder');wc16lbl('presetIce','Ice');wc16lbl('presetHail','Hail');"
                "wc16lbl('presetHeat','Heat');wc16lbl('presetWind','Wind');"));

      // ---- live status panel + "Update now" -----------------------------------
      oappend(F("addInfo('WordClock16x16:fetchWeather', 1, \"<div id='wc16stat'>loading current weather...</div>\");"));
      oappend(F("wc16refresh=function(){fetch('/json/info').then(function(r){return r.json();}).then(function(j){"
                "var u=(j&&j.u)||{};function g(k){var a=u[k];return a?(Array.isArray(a)?a.join(''):a):'-';}"
                "var e=document.getElementById('wc16stat');if(!e)return;"
                "e.innerHTML='&#127777;&#65039; '+g('Word Clock temperature')+' &nbsp; &#128167; '+g('Word Clock humidity')+"
                "' &nbsp; '+g('Word Clock condition')+'<br>&#128205; '+g('Word Clock location')+"
                "' &nbsp; &#128260; '+g('Word Clock updated');}).catch(function(){"
                "var e=document.getElementById('wc16stat');if(e)e.innerHTML='(status unavailable)';});};"));
      oappend(F("wc16upd=function(){var e=document.getElementById('wc16stat');if(e)e.innerHTML='Updating...';"
                "fetch('/json/state',{method:'POST',headers:{'Content-Type':'application/json'},body:'{\"WordClock16x16\":{\"update\":true}}'})"
                ".then(function(){setTimeout(wc16refresh,3000);setTimeout(wc16refresh,7000);}).catch(function(){var e=document.getElementById('wc16stat');if(e)e.innerHTML='request failed';});};"));
      oappend(F("addInfo('WordClock16x16:fetchMinutes', 1, \"&nbsp;<button type='button' onclick='wc16upd()'>Update now</button>\");"));
      oappend(F("setTimeout(wc16refresh,300);"));

      // ---- per-state preset dropdowns (populated from /presets.json) -----------
      oappend(F("(function(){function f(j){var ks=['presetClear','presetClouds','presetFog','presetDrizzle','presetRain','presetSnow','presetThunder','presetIce','presetHail','presetHeat','presetWind'];"
                "for(var i=0;i<ks.length;i++){var dd=addDropdown('WordClock16x16',ks[i]);if(!dd)continue;addOption(dd,'None',0);"
                "for(var p of Object.entries(j)){if(p[0]==='0')continue;var n=(p[1]&&p[1].n)?p[1].n:('Preset '+p[0]);addOption(dd,p[0]+': '+n,p[0]);}}}"
                "fetch('/presets.json').then(function(r){return r.json();}).then(f).catch(function(){});})();"));

      // ---- "Test preset" control: force-apply a weather state's preset --------
      oappend(F("(function(){var a=d.getElementsByName('WordClock16x16:presetWind');if(!a.length)return;var anchor=a[a.length-1];"
                "var w=document.createElement('div');w.className='wc16card';w.appendChild(document.createTextNode('Test a weather preset: '));"
                "var sel=document.createElement('select');var st='clear clouds fog drizzle rain snow thunder ice hail heat wind'.split(' ');"
                "for(var i=0;i<st.length;i++){var o=document.createElement('option');o.text=st[i];o.value=i+1;sel.appendChild(o);}"
                "w.appendChild(sel);w.appendChild(document.createTextNode(' '));"
                "var b=document.createElement('button');b.type='button';b.textContent='Apply now';"
                "b.onclick=function(){fetch('/json/state',{method:'POST',headers:{'Content-Type':'application/json'},body:'{\"WordClock16x16\":{\"wxtest\":'+sel.value+'}}'});};"
                "w.appendChild(b);anchor.insertAdjacentElement('afterend',w);})();"));
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
const char WordClock16x16Usermod::_heatAbove[]   PROGMEM = "heatAbove";
const char WordClock16x16Usermod::_windAbove[]   PROGMEM = "windAbove";
const char WordClock16x16Usermod::_pClear[]      PROGMEM = "presetClear";
const char WordClock16x16Usermod::_pClouds[]     PROGMEM = "presetClouds";
const char WordClock16x16Usermod::_pFog[]        PROGMEM = "presetFog";
const char WordClock16x16Usermod::_pDrizzle[]    PROGMEM = "presetDrizzle";
const char WordClock16x16Usermod::_pRain[]       PROGMEM = "presetRain";
const char WordClock16x16Usermod::_pSnow[]       PROGMEM = "presetSnow";
const char WordClock16x16Usermod::_pThunder[]    PROGMEM = "presetThunder";
const char WordClock16x16Usermod::_pIce[]        PROGMEM = "presetIce";
const char WordClock16x16Usermod::_pHail[]       PROGMEM = "presetHail";
const char WordClock16x16Usermod::_pHeat[]       PROGMEM = "presetHeat";
const char WordClock16x16Usermod::_pWind[]       PROGMEM = "presetWind";

static WordClock16x16Usermod usermod_v2_word_clock_16x16;
REGISTER_USERMOD(usermod_v2_word_clock_16x16);
