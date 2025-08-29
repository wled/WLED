#include <limits>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <cctype>
#include <strings.h>

#include "open_weather_map_source.h"
#include "skymodel.h"
#include "util.h"

static constexpr const char* DEFAULT_API_BASE = "http://api.openweathermap.org";
static constexpr const char * DEFAULT_API_KEY = "";
static constexpr const char * DEFAULT_LOCATION = "";
static constexpr const double DEFAULT_LATITUDE = 37.80486;
static constexpr const double DEFAULT_LONGITUDE = -122.2716;
static constexpr unsigned DEFAULT_INTERVAL_SEC = 3600;	// 1 hour

// - these are user visible in the webapp settings UI
// - they are scoped to this module, don't need to be globally unique
//
const char CFG_API_BASE[] PROGMEM = "ApiBase";
const char CFG_API_KEY[] PROGMEM = "ApiKey";
const char CFG_LATITUDE[] PROGMEM = "Latitude";
const char CFG_LONGITUDE[] PROGMEM = "Longitude";
const char CFG_INTERVAL_SEC[] PROGMEM = "IntervalSec";
const char CFG_LOCATION[] PROGMEM = "Location";

// keep commas; encode spaces etc.
static void urlEncode(const char* src, char* dst, size_t dstSize) {
  static const char hex[] = "0123456789ABCDEF";
  if (!dst || dstSize == 0) return;
  size_t di = 0;
  if (!src) { dst[0] = '\0'; return; }
  for (size_t i = 0; src[i]; ++i) {
    unsigned char c = static_cast<unsigned char>(src[i]);
    // Unreserved characters per RFC 3986 (plus ',') are copied as-is
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~' || c == ',') {
      if (di + 1 < dstSize) {
        dst[di++] = c;
      } else {
        break; // no room for this char plus NUL
      }
    } else if (c == ' ') {
      if (di + 3 < dstSize) {
        dst[di++] = '%'; dst[di++] = '2'; dst[di++] = '0';
      } else {
        break; // not enough room for %20 + NUL
      }
    } else {
      if (di + 3 < dstSize) {
        dst[di++] = '%'; dst[di++] = hex[c >> 4]; dst[di++] = hex[c & 0xF];
      } else {
        break; // not enough room for %XY + NUL
      }
    }
  }
  if (di < dstSize) dst[di] = '\0'; else dst[dstSize - 1] = '\0';
}

// Redact the API key in a URL by replacing the value after "appid=" with '*'.
static void redactApiKeyInUrl(const char* in, char* out, size_t outLen) {
  if (!in || !out || outLen == 0) return;
  const char* p = strstr(in, "appid=");
  if (!p) {
    strncpy(out, in, outLen);
    out[outLen - 1] = '\0';
    return;
  }
  size_t prefixLen = (size_t)(p - in) + 6; // include "appid="
  if (prefixLen >= outLen) {
    // Not enough space; best effort copy and terminate
    strncpy(out, in, outLen);
    out[outLen - 1] = '\0';
    return;
  }
  memcpy(out, in, prefixLen);
  out[prefixLen] = '*';
  out[prefixLen + 1] = '\0';
}

// Normalize "Oakland, CA, USA" → "Oakland,CA,US" in-place
static void normalizeLocation(char* q) {
  // trim spaces and commas
  size_t len = strlen(q);
  char* out = q;
  for (size_t i = 0; i < len; ++i) {
    if (q[i] != ' ') *out++ = q[i];
  }
  *out = '\0';
  len = strlen(q);
  if (len >= 4 && strcasecmp(q + len - 4, ",USA") == 0) {
    // Truncate the trailing 'A' so ",USA" → ",US" without corrupting chars
    q[len - 1] = '\0';
  }
}

static bool parseCoordToken(char* token, double& out) {
  while (isspace((unsigned char)*token)) ++token;
  bool neg = false;
  if (*token == 's' || *token == 'S' || *token == 'w' || *token == 'W') {
    neg = true; ++token;
  } else if (*token == 'n' || *token == 'N' || *token == 'e' || *token == 'E') {
    ++token;
  }
  while (isspace((unsigned char)*token)) ++token;
  char* end = token + strlen(token);
  while (end > token && isspace((unsigned char)end[-1])) --end;
  if (end > token) {
    char c = end[-1];
    if (c == 's' || c == 'S' || c == 'w' || c == 'W') { neg = true; --end; }
    else if (c == 'n' || c == 'N' || c == 'e' || c == 'E') { --end; }
  }
  *end = '\0';
  for (char* p = token; *p; ++p) {
    if (*p == '\"' || *p == '\'' ) *p = ' ';
    if ((unsigned char)*p == 0xC2 || (unsigned char)*p == 0xB0) *p = ' ';
  }
  char* rest = nullptr;
  double deg = strtod(token, &rest);
  if (rest == token) return false;
  bool negNum = deg < 0; deg = fabs(deg);
  double min = 0, sec = 0;
  if (*rest) {
    min = strtod(rest, &rest);
    if (*rest) {
      sec = strtod(rest, &rest);
    }
  }
  if (negNum) neg = true;
  out = deg + min / 60.0 + sec / 3600.0;
  if (neg) out = -out;
  return true;
}

static bool parseLatLon(const char* s, double& lat, double& lon) {
  char buf[64];
  if (s == nullptr) return false;
  if (strlen(s) >= sizeof(buf)) return false;
  strncpy(buf, s, sizeof(buf));
  buf[sizeof(buf)-1] = '\0';
  char *a = nullptr, *b = nullptr;
  char *comma = strchr(buf, ',');
  if (comma) {
    *comma = '\0';
    a = buf; b = comma + 1;
  } else {
    char *space = strrchr(buf, ' ');
    if (!space) return false;
    *space = '\0';
    a = buf; b = space + 1;
  }
  if (!parseCoordToken(a, lat)) return false;
  if (!parseCoordToken(b, lon)) return false;
  return true;
}

OpenWeatherMapSource::OpenWeatherMapSource()
    : apiBase_(DEFAULT_API_BASE)
    , apiKey_(DEFAULT_API_KEY)
    , location_(DEFAULT_LOCATION)
    , latitude_(DEFAULT_LATITUDE)
    , longitude_(DEFAULT_LONGITUDE)
    , intervalSec_(DEFAULT_INTERVAL_SEC)
    , lastFetch_(0)
    , lastHistFetch_(0) {
  DEBUG_PRINTF("SkyStrip: %s::CTOR\n", name().c_str());

}

void OpenWeatherMapSource::addToConfig(JsonObject& subtree) {
  subtree[FPSTR(CFG_API_BASE)] = apiBase_;
  subtree[FPSTR(CFG_API_KEY)] = apiKey_;
  subtree[FPSTR(CFG_LOCATION)] = location_;
  subtree[FPSTR(CFG_LATITUDE)] = latitude_;
  subtree[FPSTR(CFG_LONGITUDE)] = longitude_;
  subtree[FPSTR(CFG_INTERVAL_SEC)] = intervalSec_;
}

bool OpenWeatherMapSource::readFromConfig(JsonObject &subtree,
                                          bool running,
                                          bool& invalidate_history) {
  // note what the prior values of latitude_ and longitude_ are
  double oldLatitude = latitude_;
  double oldLongitude = longitude_;

  bool configComplete = !subtree.isNull();
  configComplete &= getJsonValue(subtree[FPSTR(CFG_API_BASE)], apiBase_, DEFAULT_API_BASE);
  configComplete &= getJsonValue(subtree[FPSTR(CFG_API_KEY)], apiKey_, DEFAULT_API_KEY);
  configComplete &= getJsonValue(subtree[FPSTR(CFG_LOCATION)], location_, DEFAULT_LOCATION);
  configComplete &= getJsonValue(subtree[FPSTR(CFG_LATITUDE)], latitude_, DEFAULT_LATITUDE);
  configComplete &= getJsonValue(subtree[FPSTR(CFG_LONGITUDE)], longitude_, DEFAULT_LONGITUDE);
  configComplete &= getJsonValue(subtree[FPSTR(CFG_INTERVAL_SEC)], intervalSec_, DEFAULT_INTERVAL_SEC);

  // If the location changed update lat/long via parsing or lookup
  if (location_ == lastLocation_) {
    // if the user changed the lat and long directly clear the location
    if (latitude_ != oldLatitude || longitude_ != oldLongitude)
      location_ = "";
  } else {
    lastLocation_ = location_;
    if (location_.length() > 0) {
      double lat = 0, lon = 0;
      if (parseLatLon(location_.c_str(), lat, lon)) {
        latitude_ = lat;
        longitude_ = lon;
      } else if (running) {
        int matches = 0;
        bool ok = geocodeOWM(location_, lat, lon, &matches);
        latitude_ = ok ? lat : 0.0;
        longitude_ = ok ? lon : 0.0;
      }
    }
  }

  // if the lat/long changed we need to invalidate_history
  if (latitude_ != oldLatitude || longitude_ != oldLongitude)
    invalidate_history = true;

  return configComplete;
}

void OpenWeatherMapSource::composeApiUrl(char* buf, size_t len) const {
  if (!buf || len == 0) return;
  (void)snprintf(buf, len,
                 "%s/data/3.0/onecall?exclude=minutely,daily,alerts&units=imperial&lat=%.6f&lon=%.6f&appid=%s",
                 apiBase_.c_str(), latitude_, longitude_, apiKey_.c_str());
  buf[len - 1] = '\0';
}

std::unique_ptr<SkyModel> OpenWeatherMapSource::fetch(std::time_t now) {
  // Wait for scheduled time
  if ((now - lastFetch_) < static_cast<std::time_t>(intervalSec_))
    return nullptr;

  // Update lastFetch_ and lastHistFetch_ upfront to reduce API
  // thrash if things don't work out
  lastFetch_ = now;
  lastHistFetch_ = now; // history fetches should wait

  // Fetch JSON
  char url[256];
  composeApiUrl(url, sizeof(url));
  char redacted[256];
  redactApiKeyInUrl(url, redacted, sizeof(redacted));
  DEBUG_PRINTF("SkyStrip: %s::fetch URL: %s\n", name().c_str(), redacted);

  auto doc = getJson(url);
  if (!doc) {
    DEBUG_PRINTF("SkyStrip: %s::fetch failed: no JSON\n", name().c_str());
    return nullptr;
  }

  // Top-level object
  JsonObject root = doc->as<JsonObject>();

  if (!root.containsKey("hourly")) {
    DEBUG_PRINTF("SkyStrip: %s::fetch failed: no \"hourly\" field\n", name().c_str());
    return nullptr;
  }

  time_t sunrise = 0;
  time_t sunset = 0;
  if (root.containsKey("current")) {
    JsonObject cur = root["current"].as<JsonObject>();
    if (cur.containsKey("sunrise") && cur.containsKey("sunset")) {
      sunrise = cur["sunrise"].as<time_t>();
      sunset  = cur["sunset"].as<time_t>();
    } else {
      bool night = false;
      JsonArray wArrCur = cur["weather"].as<JsonArray>();
      if (!wArrCur.isNull() && wArrCur.size() > 0) {
        const char* icon = wArrCur[0]["icon"] | "";
        size_t ilen = strlen(icon);
        if (ilen > 0 && icon[ilen-1] == 'n') night = true;
      }
      if (night) {
        sunrise = std::numeric_limits<time_t>::max();
        sunset = 0;
      } else {
        sunrise = 0;
        sunset = std::numeric_limits<time_t>::max();
      }
    }
  }

  // Iterate the hourly array
  JsonArray hourly = root["hourly"].as<JsonArray>();
  auto model = ::make_unique<SkyModel>();
  model->lcl_tstamp = now;
  model->sunrise_ = sunrise;
  model->sunset_ = sunset;
  for (JsonObject hour : hourly) {
    time_t dt    = hour["dt"].as<time_t>();
    model->temperature_forecast.push_back({ dt, (float)hour["temp"].as<double>() });
    model->dew_point_forecast.push_back({ dt, (float)hour["dew_point"].as<double>() });
    model->wind_speed_forecast.push_back({ dt, (float)hour["wind_speed"].as<double>() });
    model->wind_dir_forecast.push_back({ dt, (float)hour["wind_deg"].as<double>() });
    model->wind_gust_forecast.push_back({ dt, (float)hour["wind_gust"].as<double>() });
    model->cloud_cover_forecast.push_back({ dt, (float)hour["clouds"].as<double>() });
    JsonArray wArr = hour["weather"].as<JsonArray>();
    bool hasRain = false, hasSnow = false;
    if (hour.containsKey("rain")) {
      double v = hour["rain"]["1h"] | 0.0;
      if (v > 0.0) hasRain = true;
    }
    if (hour.containsKey("snow")) {
      double v = hour["snow"]["1h"] | 0.0;
      if (v > 0.0) hasSnow = true;
    }
    if (!hasRain && !hasSnow && !wArr.isNull() && wArr.size() > 0) {
      const char* main = wArr[0]["main"] | "";
      if (strcasecmp(main, "rain") == 0 || strcasecmp(main, "drizzle") == 0 ||
          strcasecmp(main, "thunderstorm") == 0)
        hasRain = true;
      else if (strcasecmp(main, "snow") == 0)
        hasSnow = true;
    }
    int ptype = hasRain && hasSnow ? 3 : (hasSnow ? 2 : (hasRain ? 1 : 0));
    model->precip_type_forecast.push_back({ dt, (float)ptype });
    model->precip_prob_forecast.push_back({ dt, (float)hour["pop"].as<double>() });
  }

  // Stagger history fetch to avoid back-to-back GETs in same loop iteration
  // and reduce risk of watchdog resets. Enforce at least 15s before history.
  lastHistFetch_ = skystrip::util::time_now_utc();
  return model;
}

std::unique_ptr<SkyModel> OpenWeatherMapSource::checkhistory(time_t now, std::time_t oldestTstamp) {
  if (oldestTstamp == 0) return nullptr;
  if ((now - lastHistFetch_) < 15) return nullptr;
  lastHistFetch_ = now;

  static constexpr time_t HISTORY_SEC = 24 * 60 * 60;
  if (oldestTstamp <= now - HISTORY_SEC) return nullptr;

  time_t fetchDt = oldestTstamp - 3600;
  char url[256];
  snprintf(url, sizeof(url),
           "%s/data/3.0/onecall/timemachine?lat=%.6f&lon=%.6f&dt=%ld&units=imperial&appid=%s",
           apiBase_.c_str(), latitude_, longitude_, (long)fetchDt, apiKey_.c_str());
  char redacted[256];
  redactApiKeyInUrl(url, redacted, sizeof(redacted));
  DEBUG_PRINTF("SkyStrip: %s::checkhistory URL: %s\n", name().c_str(), redacted);

  auto doc = getJson(url);
  if (!doc) {
    DEBUG_PRINTF("SkyStrip: %s::checkhistory failed: no JSON\n", name().c_str());
    return nullptr;
  }

  JsonObject root = doc->as<JsonObject>();
  JsonArray hourly = root["hourly"].as<JsonArray>();
  if (hourly.isNull()) hourly = root["data"].as<JsonArray>();
  if (hourly.isNull()) {
    DEBUG_PRINTF("SkyStrip: %s::checkhistory failed: no hourly/data field\n", name().c_str());
    return nullptr;
  }

  auto model = ::make_unique<SkyModel>();
  model->lcl_tstamp = now;
  model->sunrise_ = 0;
  model->sunset_ = 0;
  for (JsonObject hour : hourly) {
    time_t dt = hour["dt"].as<time_t>();
    if (dt >= oldestTstamp) continue;
    model->temperature_forecast.push_back({ dt, (float)hour["temp"].as<double>() });
    model->dew_point_forecast.push_back({ dt, (float)hour["dew_point"].as<double>() });
    model->wind_speed_forecast.push_back({ dt, (float)hour["wind_speed"].as<double>() });
    model->wind_dir_forecast.push_back({ dt, (float)hour["wind_deg"].as<double>() });
    model->wind_gust_forecast.push_back({ dt, (float)hour["wind_gust"].as<double>() });
    model->cloud_cover_forecast.push_back({ dt, (float)hour["clouds"].as<double>() });
    JsonArray wArr = hour["weather"].as<JsonArray>();
    bool hasRain = false, hasSnow = false;
    if (hour.containsKey("rain")) {
      double v = hour["rain"]["1h"] | 0.0;
      if (v > 0.0) hasRain = true;
    }
    if (hour.containsKey("snow")) {
      double v = hour["snow"]["1h"] | 0.0;
      if (v > 0.0) hasSnow = true;
    }
    if (!hasRain && !hasSnow && !wArr.isNull() && wArr.size() > 0) {
      const char* main = wArr[0]["main"] | "";
      if (strcasecmp(main, "rain") == 0 || strcasecmp(main, "drizzle") == 0 ||
          strcasecmp(main, "thunderstorm") == 0)
        hasRain = true;
      else if (strcasecmp(main, "snow") == 0)
        hasSnow = true;
    }
    int ptype = hasRain && hasSnow ? 3 : (hasSnow ? 2 : (hasRain ? 1 : 0));
    model->precip_type_forecast.push_back({ dt, (float)ptype });
    model->precip_prob_forecast.push_back({ dt, (float)hour["pop"].as<double>() });
  }

  if (model->temperature_forecast.empty()) return nullptr;
  return model;
}

void OpenWeatherMapSource::reload(std::time_t now) {
  const std::time_t iv = static_cast<std::time_t>(intervalSec_);
  // Force next fetch to be eligible immediately
  lastFetch_ = (now >= iv) ? (now - iv) : 0;

  // If you later add backoff/jitter, clear it here too.
  // backoffExp_ = 0; nextRetryAt_ = 0;
  DEBUG_PRINTF("SkyStrip: %s::reload (interval=%u)\n", name().c_str(), intervalSec_);
}

// Returns true iff exactly one match; sets lat/lon. Otherwise zeros them.
bool OpenWeatherMapSource::geocodeOWM(std::string const & rawQuery,
                                      double& lat, double& lon,
                                      int* outMatches)
{
  lat = lon = 0;
  char q[128];
  strncpy(q, rawQuery.c_str(), sizeof(q));
  q[sizeof(q)-1] = '\0';
  normalizeLocation(q);
  if (q[0] == '\0') { if (outMatches) *outMatches = 0; return false; }

  resetRateLimit();	// we might have done a fetch right before

  char enc[256];
  urlEncode(q, enc, sizeof(enc));
  char url[512];
  snprintf(url, sizeof(url),
           "%s/geo/1.0/direct?q=%s&limit=5&appid=%s",
           apiBase_.c_str(), enc, apiKey_.c_str());
  char redacted[512];
  redactApiKeyInUrl(url, redacted, sizeof(redacted));
  DEBUG_PRINTF("SkyStrip: %s::geocodeOWM URL: %s\n", name().c_str(), redacted);

  auto doc = getJson(url);
  resetRateLimit();	// we want to do a fetch immediately after ...
  if (!doc || !doc->is<JsonArray>()) {
    if (outMatches) *outMatches = -1;
    DEBUG_PRINTF("SkyStrip: %s::geocodeOWM failed\n", name().c_str());
    return false;
  }

  JsonArray arr = doc->as<JsonArray>();
  DEBUG_PRINTF("SkyStrip: %s::geocodeOWM %d matches found\n", name().c_str(), arr.size());
  if (outMatches) *outMatches = arr.size();
  if (arr.size() == 1) {
    lat = arr[0]["lat"] | 0.0;
    lon = arr[0]["lon"] | 0.0;
    return true;
  }
  return false;
}
