#include "wled.h"
#ifdef ESP8266
  #include <ESP8266HTTPClient.h>
#else
  #include <HTTPClient.h>
#endif

/*
 * Weather temperature source via Open-Meteo (open-meteo.com) — free, no API key.
 *
 * Fetches the current outdoor temperature (°C) on an interval and feeds it to the
 * Word Clock 16x16 usermod (WARM/COOL/HOT/COLD words) through the same weak-symbol
 * bridge (wc16_setLiveTempC) used by the I2C sensor usermod. No hard dependency, no
 * core edits. Open-Meteo returns temperature_2m in Celsius by default, which is exactly
 * what the bridge expects.
 *
 * Location: uses WLED's configured latitude/longitude (Time settings) by default, or an
 * override set in this usermod's settings. (0,0) is treated as "unset" and disables it.
 *
 * Note: HTTP GET is synchronous; it runs only every interval (default 15 min), so the
 * brief stall is rare. If both this and the sensor usermod feed the clock, the most
 * recent push wins (30 min TTL) — enable whichever source you want.
 */

extern "C" void wc16_setLiveTempC(float) __attribute__((weak));

class WeatherOpenMeteoUsermod : public Usermod {
  private:
    bool     enabled         = true;
    bool     feedWordClock   = true;
    bool     useWledLocation = true;
    uint16_t intervalMinutes = 15;
    float    latOverride     = 0.0f;
    float    lonOverride     = 0.0f;

    bool          haveTemp  = false;
    float         tempC     = 0.0f;
    bool          firstDone = false;
    unsigned long lastFetch = 0;

    static const char _name[];
    static const char _enabled[];
    static const char _feed[];
    static const char _useWled[];
    static const char _interval[];
    static const char _lat[];
    static const char _lon[];

    float useLat() const { return useWledLocation ? latitude  : latOverride; }
    float useLon() const { return useWledLocation ? longitude : lonOverride; }

    void fetch() {
      const float la = useLat(), lo = useLon();
      if (la == 0.0f && lo == 0.0f) return; // location not set

      WiFiClient client;                    // plain HTTP (Open-Meteo serves the API on :80)
      HTTPClient http;
      char url[160];
      snprintf(url, sizeof(url),
        "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&current=temperature_2m",
        la, lo);
      if (!http.begin(client, url)) return;
      http.setTimeout(5000);
      const int code = http.GET();
      if (code == HTTP_CODE_OK) {
        String payload = http.getString();
        StaticJsonDocument<96> filter;
        filter["current"]["temperature_2m"] = true;
        StaticJsonDocument<256> doc;
        if (!deserializeJson(doc, payload, DeserializationOption::Filter(filter))) {
          JsonVariant t = doc["current"]["temperature_2m"];
          if (!t.isNull()) {
            tempC = t.as<float>();
            haveTemp = true;
            if (feedWordClock && wc16_setLiveTempC) wc16_setLiveTempC(tempC);
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
      JsonArray arr = user.createNestedArray(F("Weather temperature"));
      if (useLat() == 0.0f && useLon() == 0.0f) { arr.add(F("set location")); return; }
      if (!haveTemp) { arr.add(F("--")); return; }
      char buf[16];
      snprintf(buf, sizeof(buf), "%.1f", tempC);
      arr.add(buf);
      arr.add(F(" °C"));
    }

    void addToConfig(JsonObject &root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)]  = enabled;
      top[FPSTR(_feed)]     = feedWordClock;
      top[FPSTR(_useWled)]  = useWledLocation;
      top[FPSTR(_interval)] = intervalMinutes;
      top[FPSTR(_lat)]      = latOverride;
      top[FPSTR(_lon)]      = lonOverride;
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
      if (intervalMinutes < 1) intervalMinutes = 1;
      return configComplete;
    }

    void appendConfigData() {
      oappend(F("addInfo('OpenMeteo:feedWordClock', 1, 'push temperature to Word Clock 16x16');"));
      oappend(F("addInfo('OpenMeteo:useWledLocation', 1, 'use Time-settings lat/lon (off = use below)');"));
      oappend(F("addInfo('OpenMeteo:intervalMinutes', 1, 'minutes between fetches');"));
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

static WeatherOpenMeteoUsermod usermod_v2_weather_openmeteo;
REGISTER_USERMOD(usermod_v2_weather_openmeteo);
