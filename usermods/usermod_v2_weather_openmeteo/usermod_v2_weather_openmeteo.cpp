#include "wled.h"
#ifdef ESP8266
  #include <ESP8266HTTPClient.h>
#else
  #include <HTTPClient.h>
#endif

/*
 * Weather source via Open-Meteo (open-meteo.com) — free, no API key.
 *
 * Fetches the current outdoor temperature (°C) and WMO weather code on an interval and:
 *   1. feeds the temperature to the Word Clock 16x16 usermod (WARM/COOL/HOT/COLD words)
 *      through a weak-symbol bridge (wc16_setLiveTempC) — no hard dependency, no core edits.
 *      Open-Meteo returns temperature_2m in Celsius, which is what the bridge expects.
 *   2. maps the weather code to a small set of weather states (clear/clouds/fog/drizzle/
 *      rain/snow/thunder) and, when the state changes, applies a user-configured preset
 *      for that state (e.g. a "rain" preset when it starts raining). 0 = no preset.
 *
 * Location: uses WLED's configured latitude/longitude (Time settings) by default, or an
 * override set in this usermod's settings. (0,0) is treated as "unset" and disables it.
 *
 * Note: HTTP GET is synchronous; it runs only every interval (default 15 min), so the
 * brief stall is rare.
 */

extern "C" void wc16_setLiveTempC(float) __attribute__((weak));

// Weather states (index into preset table / state names). 0 = unknown.
enum WxState : uint8_t { WX_UNKNOWN=0, WX_CLEAR, WX_CLOUDS, WX_FOG, WX_DRIZZLE, WX_RAIN, WX_SNOW, WX_THUNDER, WX_COUNT };

class WeatherOpenMeteoUsermod : public Usermod {
  private:
    bool     enabled         = true;
    bool     feedWordClock   = true;
    bool     useWledLocation = true;
    bool     enablePresets   = false;
    uint16_t intervalMinutes = 15;
    float    latOverride     = 0.0f;
    float    lonOverride     = 0.0f;
    // Preset per weather state (index 1..7 used); 0 = no preset for that state.
    uint8_t  preset[WX_COUNT] = {0,0,0,0,0,0,0,0};

    bool          haveTemp  = false;
    float         tempC     = 0.0f;
    uint8_t       wxState   = WX_UNKNOWN; // current weather state
    uint8_t       lastApplied = WX_UNKNOWN;
    bool          firstDone = false;
    unsigned long lastFetch = 0;

    static const char _name[];
    static const char _enabled[];
    static const char _feed[];
    static const char _useWled[];
    static const char _interval[];
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

    float useLat() const { return useWledLocation ? latitude  : latOverride; }
    float useLon() const { return useWledLocation ? longitude : lonOverride; }

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
      if (c == 2 || c == 3)             return WX_CLOUDS;    // partly cloudy, overcast
      if (c == 45 || c == 48)           return WX_FOG;
      if (c >= 51 && c <= 57)           return WX_DRIZZLE;   // drizzle / freezing drizzle
      if ((c >= 61 && c <= 67) || (c >= 80 && c <= 82)) return WX_RAIN; // rain + showers
      if ((c >= 71 && c <= 77) || c == 85 || c == 86)   return WX_SNOW; // snow + snow showers
      if (c >= 95)                      return WX_THUNDER;   // thunderstorm (95,96,99)
      return WX_UNKNOWN;
    }

    void applyStatePreset() {
      if (!enablePresets || wxState == WX_UNKNOWN || wxState == lastApplied) return;
      const uint8_t p = preset[wxState];
      lastApplied = wxState;                 // remember even if 0, so we only act on changes
      if (p > 0) applyPreset(p, CALL_MODE_DIRECT_CHANGE);
    }

    void fetch() {
      const float la = useLat(), lo = useLon();
      if (la == 0.0f && lo == 0.0f) return;  // location not set

      WiFiClient client;                     // plain HTTP (Open-Meteo serves the API on :80)
      HTTPClient http;
      char url[176];
      snprintf(url, sizeof(url),
        "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m,weather_code",
        la, lo);
      if (!http.begin(client, url)) return;
      http.setTimeout(5000);
      const int code = http.GET();
      if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<128> filter;
        filter["current"]["temperature_2m"] = true;
        filter["current"]["weather_code"]   = true;
        StaticJsonDocument<256> doc;
        if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
          JsonVariant t = doc["current"]["temperature_2m"];
          if (!t.isNull()) {
            tempC = t.as<float>();
            haveTemp = true;
            if (feedWordClock && wc16_setLiveTempC) wc16_setLiveTempC(tempC);
          }
          JsonVariant w = doc["current"]["weather_code"];
          if (!w.isNull()) {
            wxState = codeToState(w.as<int>());
            applyStatePreset();
          }
        }
      }
      http.end();
    }

  public:
    void setup() override {}

    void loop() override {
      if (!enabled || !WLED_CONNECTED) return;
      const unsigned long now = millis();
      if (!firstDone) { if (now < 30000) return; }            // settle 30 s after boot
      else if (now - lastFetch < (unsigned long)intervalMinutes * 60000UL) return;
      fetch();
      lastFetch = now;
      firstDone = true;
    }

    void addToJsonInfo(JsonObject &root) override {
      if (!enabled) return;
      JsonObject user = root[F("u")];
      if (user.isNull()) user = root.createNestedObject(F("u"));
      JsonArray arr = user.createNestedArray(F("Weather"));
      if (useLat() == 0.0f && useLon() == 0.0f) { arr.add(F("set location")); return; }
      if (!haveTemp) { arr.add(F("--")); return; }
      char buf[28];
      snprintf(buf, sizeof(buf), "%.1f°C %s", tempC, stateName(wxState));
      arr.add(buf);
    }

    void addToConfig(JsonObject &root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)]  = enabled;
      top[FPSTR(_feed)]     = feedWordClock;
      top[FPSTR(_useWled)]  = useWledLocation;
      top[FPSTR(_interval)] = intervalMinutes;
      top[FPSTR(_lat)]      = latOverride;
      top[FPSTR(_lon)]      = lonOverride;
      top[FPSTR(_presets)]  = enablePresets;
      top[FPSTR(_pClear)]   = preset[WX_CLEAR];
      top[FPSTR(_pClouds)]  = preset[WX_CLOUDS];
      top[FPSTR(_pFog)]     = preset[WX_FOG];
      top[FPSTR(_pDrizzle)] = preset[WX_DRIZZLE];
      top[FPSTR(_pRain)]    = preset[WX_RAIN];
      top[FPSTR(_pSnow)]    = preset[WX_SNOW];
      top[FPSTR(_pThunder)] = preset[WX_THUNDER];
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      configComplete &= getJsonValue(top[FPSTR(_enabled)],  enabled);
      configComplete &= getJsonValue(top[FPSTR(_feed)],     feedWordClock);
      configComplete &= getJsonValue(top[FPSTR(_useWled)],  useWledLocation);
      configComplete &= getJsonValue(top[FPSTR(_interval)], intervalMinutes);
      configComplete &= getJsonValue(top[FPSTR(_lat)],      latOverride);
      configComplete &= getJsonValue(top[FPSTR(_lon)],      lonOverride);
      configComplete &= getJsonValue(top[FPSTR(_presets)],  enablePresets);
      configComplete &= getJsonValue(top[FPSTR(_pClear)],   preset[WX_CLEAR]);
      configComplete &= getJsonValue(top[FPSTR(_pClouds)],  preset[WX_CLOUDS]);
      configComplete &= getJsonValue(top[FPSTR(_pFog)],     preset[WX_FOG]);
      configComplete &= getJsonValue(top[FPSTR(_pDrizzle)], preset[WX_DRIZZLE]);
      configComplete &= getJsonValue(top[FPSTR(_pRain)],    preset[WX_RAIN]);
      configComplete &= getJsonValue(top[FPSTR(_pSnow)],    preset[WX_SNOW]);
      configComplete &= getJsonValue(top[FPSTR(_pThunder)], preset[WX_THUNDER]);
      if (intervalMinutes < 1) intervalMinutes = 1;
      return configComplete;
    }

    void appendConfigData() {
      oappend(F("addInfo('OpenMeteo:feedWordClock', 1, 'push temperature to Word Clock 16x16');"));
      oappend(F("addInfo('OpenMeteo:useWledLocation', 1, 'use Time-settings lat/lon (off = use below)');"));
      oappend(F("addInfo('OpenMeteo:intervalMinutes', 1, 'minutes between fetches');"));
      oappend(F("addInfo('OpenMeteo:weatherPresets', 1, 'apply a preset when weather changes');"));
      // Turn the per-state preset number fields into dropdowns populated from /presets.json.
      oappend(F("(function(){function f(j){var ks=['presetClear','presetClouds','presetFog','presetDrizzle','presetRain','presetSnow','presetThunder'];"
                "for(var i=0;i<ks.length;i++){var dd=addDropdown('OpenMeteo',ks[i]);if(!dd)continue;addOption(dd,'None',0);"
                "for(var p of Object.entries(j)){if(p[0]==='0')continue;var n=(p[1]&&p[1].n)?p[1].n:('Preset '+p[0]);addOption(dd,p[0]+': '+n,p[0]);}}}"
                "fetch('/presets.json').then(function(r){return r.json();}).then(f).catch(function(){});})();"));
    }

    // No getId() override (USERMOD_ID_UNSPECIFIED) — keeps changes out of wled00/const.h.
};

const char WeatherOpenMeteoUsermod::_name[]     PROGMEM = "OpenMeteo";
const char WeatherOpenMeteoUsermod::_enabled[]  PROGMEM = "enabled";
const char WeatherOpenMeteoUsermod::_feed[]     PROGMEM = "feedWordClock";
const char WeatherOpenMeteoUsermod::_useWled[]  PROGMEM = "useWledLocation";
const char WeatherOpenMeteoUsermod::_interval[] PROGMEM = "intervalMinutes";
const char WeatherOpenMeteoUsermod::_lat[]      PROGMEM = "latitude";
const char WeatherOpenMeteoUsermod::_lon[]      PROGMEM = "longitude";
const char WeatherOpenMeteoUsermod::_presets[]  PROGMEM = "weatherPresets";
const char WeatherOpenMeteoUsermod::_pClear[]   PROGMEM = "presetClear";
const char WeatherOpenMeteoUsermod::_pClouds[]  PROGMEM = "presetClouds";
const char WeatherOpenMeteoUsermod::_pFog[]     PROGMEM = "presetFog";
const char WeatherOpenMeteoUsermod::_pDrizzle[] PROGMEM = "presetDrizzle";
const char WeatherOpenMeteoUsermod::_pRain[]    PROGMEM = "presetRain";
const char WeatherOpenMeteoUsermod::_pSnow[]    PROGMEM = "presetSnow";
const char WeatherOpenMeteoUsermod::_pThunder[] PROGMEM = "presetThunder";

static WeatherOpenMeteoUsermod usermod_v2_weather_openmeteo;
REGISTER_USERMOD(usermod_v2_weather_openmeteo);
