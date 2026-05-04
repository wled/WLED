#include "temperature_view.h"
#include "skymodel.h"
#include "util.h"
#include "wled.h" // Segment, strip, RGBW32
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include <vector>

static constexpr int16_t DEFAULT_SEG_ID = -1; // -1 means disabled

// - these are user visible in the webapp settings UI
// - they are scoped to this module, don't need to be globally unique
//
const char CFG_SEG_ID[] PROGMEM = "SegmentId";
const char CFG_COLOR_MAP[] PROGMEM = "ColorMap";
const char CFG_MOVE_OFFSET[] PROGMEM = "MoveOffset";

// Default map: 15°F centers with short wraps at the ends.
static constexpr const char kDefaultColorMapStr[] =
    "-45:yellow|-30:orange|-15:red|0:magenta|15:purple|30:blue|45:cyan|"
    "60:green|75:yellow|90:orange|105:red|120:magenta|135:purple|150:blue";

static constexpr int16_t kCenterMinF = -200;
static constexpr int16_t kCenterMaxF = 200;
static constexpr int16_t kMaxOffsetF = 400;
static constexpr size_t kMaxColorMapLen = 512;
static constexpr size_t kMaxParsedEntries = 64;
static constexpr float kValue = 0.5f;

struct NamedColor {
  const char *name;
  float hue;
};

static const NamedColor kNamedColors[] = {
    {"magenta", 300.f},
    {"purple", 275.f},
    {"blue", 220.f},
    {"cyan", 185.f},
    {"green", 130.f},
    {"yellow", 60.f},
    {"orange", 30.f},
    {"red", 0.f},
};

static inline bool hueToNamedToken(float hue, String &out) {
  for (const auto &nc : kNamedColors) {
    if (fabsf(hue - nc.hue) < 0.5f) {
      out = nc.name;
      return true;
    }
  }
  return false;
}

static char *trimInPlace(char *s) {
  while (*s && std::isspace((unsigned char)*s))
    ++s;
  char *end = s + strlen(s);
  while (end > s && std::isspace((unsigned char)*(end - 1)))
    --end;
  *end = '\0';
  return s;
}

static bool parseHueToken(const char *tok, float &hue, String &normToken) {
  if (!tok)
    return false;

  for (const auto &nc : kNamedColors) {
    if (strcasecmp(tok, nc.name) == 0) {
      hue = nc.hue;
      normToken = nc.name;
      return true;
    }
  }

  char *endptr = nullptr;
  long deg = strtol(tok, &endptr, 10);
  if (!endptr || *endptr != '\0')
    return false;
  float h = fmodf((float)deg, 360.f);
  if (h < 0.f)
    h += 360.f;
  hue = h;
  normToken = String((int)std::lround(h));
  return true;
}

static bool parseColorMap(const char *cfg, std::vector<TemperatureView::ColorStop> &out,
                          String &normalized) {
  if (!cfg || *cfg == '\0')
    return false;

  char buf[kMaxColorMapLen];
  strncpy(buf, cfg, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';

  struct Tmp {
    int16_t center;
    float hue;
    String token;
  };
  std::vector<Tmp> tmp;
  tmp.reserve(16);

  char *saveptr;
  for (char *entry = strtok_r(buf, "|", &saveptr); entry;
       entry = strtok_r(nullptr, "|", &saveptr)) {
    if (tmp.size() >= kMaxParsedEntries)
      return false; // protect memory from junk input
    char *colon = strchr(entry, ':');
    if (!colon)
      return false;
    *colon = '\0';
    char *centerStr = trimInPlace(entry);
    char *colorStr = trimInPlace(colon + 1);
    if (!*centerStr || !*colorStr)
      return false;

    char *endptr = nullptr;
    long center = strtol(centerStr, &endptr, 10);
    if (!endptr || *endptr != '\0')
      return false;
    if (center < kCenterMinF || center > kCenterMaxF)
      return false;

    float hue;
    String norm;
    if (!parseHueToken(colorStr, hue, norm))
      return false;

    Tmp t;
    t.center = (int16_t)center;
    t.hue = hue;
    t.token = norm;
    tmp.push_back(t);
  }

  if (tmp.size() < 2)
    return false;

  std::sort(tmp.begin(), tmp.end(),
            [](const Tmp &a, const Tmp &b) { return a.center < b.center; });

  for (size_t i = 1; i < tmp.size(); ++i) {
    if (tmp[i].center == tmp[i - 1].center)
      return false;
  }

  normalized = "";
  out.clear();
  out.reserve(tmp.size());
  for (size_t i = 0; i < tmp.size(); ++i) {
    TemperatureView::ColorStop cs;
    cs.centerF = tmp[i].center;
    cs.hue = tmp[i].hue;
    out.push_back(cs);
    if (i > 0)
      normalized += '|';
    normalized += String(tmp[i].center);
    normalized += ':';
    normalized += tmp[i].token;
  }
  return true;
}

// Map dew-point depression (°F) -> saturation multiplier.
// dd<=2°F  -> minSat ; dd>=25°F -> 1.0 ; smooth in between.
static inline float satFromDewSpreadF(float tempF, float dewF) {
  float dd = tempF - dewF;
  if (dd < 0.f)
    dd = 0.f;                         // guard bad inputs
  constexpr float kMinSat = 0.55f;    // floor (muggy look)
  constexpr float kMaxSpread = 25.0f; // “very dry” cap
  float u = skystrip::util::clamp01(dd / kMaxSpread);
  float eased = u * u * (3.f - 2.f * u); // smoothstep
  return kMinSat + (1.f - kMinSat) * eased;
}

static void fmtColorHex(uint32_t col, char *out, size_t n) {
  uint8_t r = (col >> 16) & 0xFF;
  uint8_t g = (col >> 8) & 0xFF;
  uint8_t b = (col)&0xFF;
  snprintf(out, n, "#%02X%02X%02X", r, g, b);
}

TemperatureView::TemperatureView() : segId_(DEFAULT_SEG_ID) {
  DEBUG_PRINTLN("SkyStrip: TV::CTOR");
  resetColorMapToDefault();
  snprintf(debugPixelString, sizeof(debugPixelString), "%s:\\n",
           name().c_str());
  debugPixelString[sizeof(debugPixelString) - 1] = '\0';
}

void TemperatureView::view(time_t now, SkyModel const &model,
                           int16_t dbgPixelIndex) {
  if (dbgPixelIndex < 0) {
    snprintf(debugPixelString, sizeof(debugPixelString), "%s:\\n",
             name().c_str());
    debugPixelString[sizeof(debugPixelString) - 1] = '\0';
  }
  if (segId_ == DEFAULT_SEG_ID) {
    freezeHandle_.release();
    return; // disabled
  }
  if (model.temperature_forecast.empty())
    return; // nothing to render

  if (segId_ < 0 || segId_ >= strip.getMaxSegments()) {
    freezeHandle_.release();
    return;
  }
  Segment *segPtr = freezeHandle_.acquire(segId_);
  if (!segPtr)
    return;
  Segment &seg = *segPtr;
  int len = seg.virtualLength();
  if (len <= 0) {
    freezeHandle_.release();
    return;
  }
  // Initialize segment drawing parameters so virtualLength()/mapping are valid
  seg.beginDraw();

  constexpr double kHorizonSec = 48.0 * 3600.0;
  const double step = (len > 1) ? (kHorizonSec / double(len - 1)) : 0.0;
  constexpr time_t DAY = 24 * 60 * 60;
  const long tzOffset = skystrip::util::current_offset();

  std::vector<double> temps(len, NAN);
  std::vector<float> hues(len, 0.f);
  std::vector<float> sats(len, 1.f);
  std::vector<uint8_t> valid(len, 0);

  // Precompute temps/hues/sats so we can detect crossings.
  for (int i = 0; i < len; ++i) {
    const time_t t = now + time_t(std::llround(step * i));
    double tempF = 0.f;
    double dewF = 0.f;
    if (skystrip::util::estimateTempAt(model, t, step, tempF)) {
      float hue = hueForTempF(tempF);
      float sat = 1.0f;
      if (skystrip::util::estimateDewPtAt(model, t, step, dewF)) {
        sat = satFromDewSpreadF((float)tempF, (float)dewF);
      }
      temps[i] = tempF;
      hues[i] = hue;
      sats[i] = sat;
      valid[i] = 1;
    }
  }

  // Mark crossings at 5°F (low boost) and 10°F (full boost) boundaries.
  std::vector<uint8_t> crossingLevel(len, 0); // 0=none,1=5°F,2=10°F
  auto markCrossings = [&](double stepDeg, uint8_t level, bool dualHighlight) {
    for (int i = 0; i + 1 < len; ++i) {
      if (!valid[i] || !valid[i + 1])
        continue;
      double a = temps[i];
      double b = temps[i + 1];
      if (a == b)
        continue;
      double lo = std::min(a, b);
      double hi = std::max(a, b);
      double boundary = std::ceil(lo / stepDeg) * stepDeg;
      if (boundary > hi)
        continue;
      double da = fabs(a - boundary);
      double db = fabs(b - boundary);
      int idx = (da <= db) ? i : (i + 1);
      if (crossingLevel[idx] < level)
        crossingLevel[idx] = level;
      if (dualHighlight) {
        if (crossingLevel[i] < level)
          crossingLevel[i] = level;
        if (crossingLevel[i + 1] < level)
          crossingLevel[i + 1] = level;
      }
    }
  };
  markCrossings(5.0, 1, false);
  markCrossings(10.0, 2, true);

  // Returns [0,1] marker weight based on proximity to local-time markers.
  // Markers: 12a/12p (double width), plus 3a/3p, 6a/6p, 9a/9p (normal width).
  // Width=1 → fades to 0 at 1 pixel; width=2 → fades to 0 at 2 pixels.
  auto markerWeight = [&](time_t t) {
    if (step <= 0.0)
      return 0.f;

    time_t local = t + tzOffset; // convert to local seconds
    time_t s = (((local % DAY) + DAY) % DAY); // seconds since local midnight (normalized)

    // Seconds-of-day for markers + per-marker width multipliers.
    static const time_t kMarkers[] = {0 * 3600,  3 * 3600,  6 * 3600,
                                      9 * 3600,  12 * 3600, 15 * 3600,
                                      18 * 3600, 21 * 3600};
    static const float dayTW = 2.0f;
    static const float majorTW = 1.6f;
    static const float minorTW = 0.8f;
    static const float kWidth[] = {
        dayTW,   minorTW, minorTW, minorTW, // midnight, 3a, 6a, 9a
        majorTW, minorTW, minorTW, minorTW  // noon, 3p, 6p, 9p
    };

    constexpr time_t HALF_DAY = DAY / 2;
    float w = 0.f;

    const size_t N = sizeof(kMarkers) / sizeof(kMarkers[0]);
    for (size_t i = 0; i < N; ++i) {
      time_t m = kMarkers[i];
      time_t d = (s > m) ? (s - m) : (m - s);
      if (d > HALF_DAY)
        d = DAY - d; // wrap on 24h circle
      float wi = 1.f - float(d) / (float(step) * kWidth[i]);
      if (wi > w)
        w = wi; // max of all marker influences
    }

    return (w > 0.f) ? w : 0.f;
  };

  for (int i = 0; i < len; ++i) {
    const time_t t = now + time_t(std::llround(step * i));

    double tempF = temps[i];
    float hue = hues[i];
    float sat = sats[i];
    uint32_t col = 0;
    if (valid[i]) {
      float val = kValue;
      if (crossingLevel[i] == 2) {
        sat = 1.0f;
        val = 0.9f; // strong boost for 10°F crossings
      } else if (crossingLevel[i] == 1) {
        sat = 1.0f;
        val = 0.9f; // lighter boost for 5°F crossings
      }
      col = skystrip::util::hsv2rgb(hue, sat, val);
    }

    if (crossingLevel[i] == 0) {
      float m = markerWeight(t);
      if (m > 0.f) {
        uint8_t blend = uint8_t(std::lround(m * 255.f));
        col = color_blend(col, 0, blend);
      }
    }

    if (dbgPixelIndex >= 0) {
      static time_t lastDebug = 0;
      if (now - lastDebug > 1 && i == dbgPixelIndex) {
        char nowbuf[20];
        skystrip::util::fmt_local(nowbuf, sizeof(nowbuf), now);
        char dbgbuf[20];
        skystrip::util::fmt_local(dbgbuf, sizeof(dbgbuf), t);
        char colbuf[10];
        fmtColorHex(col, colbuf, sizeof(colbuf));
        snprintf(debugPixelString, sizeof(debugPixelString),
                 "%s: nowtm=%s dbgndx=%d dbgtm=%s "
                 "tempF=%.1f hue=%.0f sat=%.0f crossing=%d col=%s\\n",
                 name().c_str(), nowbuf, i, dbgbuf, tempF, hue, sat * 100,
                 crossingLevel[i], colbuf);
        lastDebug = now;
      }
    }

    seg.setPixelColor(i, skystrip::util::blinkDebug(i, dbgPixelIndex, col));
  }
}

void TemperatureView::deactivate() {
  freezeHandle_.release();
}

void TemperatureView::addToConfig(JsonObject &subtree) {
  if (colorMapStr_.isEmpty() || stops_.empty()) {
    resetColorMapToDefault();
  }
  subtree[FPSTR(CFG_SEG_ID)] = segId_;
  subtree[FPSTR(CFG_COLOR_MAP)] = colorMapStr_;
  subtree[FPSTR(CFG_MOVE_OFFSET)] = ""; // consumed on apply
}

void TemperatureView::appendConfigData(Print &s) {
  // Keep the hint INLINE (BEFORE the input = 4th arg):
  s.print(F("addInfo('SkyStrip:TemperatureView:SegmentId',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(-1 disables)</small>'"
            ");"));
  s.print(F("addInfo('SkyStrip:TemperatureView:ColorMap',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(center:hue|center:hue &nbsp;| hues: 0-359 or names: magenta/purple/blue/cyan/green/yellow/orange/red)</small>'"
            ");"));
  s.print(F("addInfo('SkyStrip:TemperatureView:MoveOffset',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(optional; enter +/- degrees F to shift all centers, applied on save)</small>'"
            ");"));
}

bool TemperatureView::readFromConfig(JsonObject &subtree, bool startup_complete,
                                     bool &invalidate_history) {
  bool configComplete = !subtree.isNull();
  configComplete &=
      getJsonValue(subtree[FPSTR(CFG_SEG_ID)], segId_, DEFAULT_SEG_ID);
  int16_t moveOffset = 0;
  bool hasMoveOffset = false;

  if (!subtree[FPSTR(CFG_COLOR_MAP)].isNull()) {
    const char *cfg = subtree[FPSTR(CFG_COLOR_MAP)];
    bool parsed = applyColorMapConfig(cfg);
    configComplete &= parsed;
    if (!parsed)
      resetColorMapToDefault();
  } else {
    resetColorMapToDefault();
  }

  if (!subtree[FPSTR(CFG_MOVE_OFFSET)].isNull()) {
    const char *ofs = subtree[FPSTR(CFG_MOVE_OFFSET)];
    if (ofs && *ofs) {
      char *endptr = nullptr;
      long v = strtol(ofs, &endptr, 10);
      if (endptr && *endptr == '\0' && v >= -kMaxOffsetF && v <= kMaxOffsetF) {
        moveOffset = (int16_t)v;
        hasMoveOffset = true;
      }
    }
  }

  if (hasMoveOffset && !stops_.empty()) {
    applyOffsetToStops(moveOffset);
  }

  return configComplete;
}

void TemperatureView::resetColorMapToDefault() {
  if (!applyColorMapConfig(kDefaultColorMapStr)) {
    stops_.clear();
    colorMapStr_.clear();
  }
}

bool TemperatureView::applyColorMapConfig(const char *cfg) {
  String normalized;
  std::vector<ColorStop> parsed;
  if (!parseColorMap(cfg, parsed, normalized))
    return false;
  stops_.swap(parsed);
  colorMapStr_ = normalized;
  return true;
}

float TemperatureView::hueForTempF(double f) const {
  if (stops_.empty())
    return 0.f;
  if (stops_.size() == 1)
    return stops_[0].hue;

  if (f <= stops_.front().centerF)
    return stops_.front().hue;
  if (f >= stops_.back().centerF)
    return stops_.back().hue;

  for (size_t i = 0; i + 1 < stops_.size(); ++i) {
    const auto &a = stops_[i];
    const auto &b = stops_[i + 1];
    if (f <= b.centerF) {
      double span = double(b.centerF - a.centerF);
      double u = (span > 0.0) ? skystrip::util::clamp01((f - a.centerF) / span) : 0.0;
      // shortest-path hue interpolation on the circle
      float dh = b.hue - a.hue;
      dh = fmodf(dh + 540.f, 360.f) - 180.f;
      float h = a.hue + float(u) * dh;
      h = fmodf(h, 360.f);
      if (h < 0.f)
        h += 360.f;
      return h;
    }
  }
  return stops_.back().hue;
}

void TemperatureView::applyOffsetToStops(int16_t deltaF) {
  if (deltaF == 0 || stops_.empty())
    return;

  String rebuilt;
  rebuilt.reserve(colorMapStr_.length() + 16);

  for (size_t i = 0; i < stops_.size(); ++i) {
    int32_t shifted = int32_t(stops_[i].centerF) + int32_t(deltaF);
    if (shifted < kCenterMinF)
      shifted = kCenterMinF;
    if (shifted > kCenterMaxF)
      shifted = kCenterMaxF;
    stops_[i].centerF = (int16_t)shifted;
    if (i > 0)
      rebuilt += '|';
    rebuilt += String(stops_[i].centerF);
    rebuilt += ':';
    String hueTok;
    if (!hueToNamedToken(stops_[i].hue, hueTok)) {
      hueTok = String((int)std::lround(stops_[i].hue));
    }
    rebuilt += hueTok;
  }
  colorMapStr_ = rebuilt;
}
