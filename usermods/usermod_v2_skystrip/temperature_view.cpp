#include "temperature_view.h"
#include "skymodel.h"
#include "util.h"
#include "wled.h" // Segment, strip, RGBW32
#include <algorithm>
#include <cmath>

static constexpr int16_t DEFAULT_SEG_ID = -1; // -1 means disabled

// - these are user visible in the webapp settings UI
// - they are scoped to this module, don't need to be globally unique
//
const char CFG_SEG_ID[] PROGMEM = "SegmentId";

// Map dew-point depression (°F) -> saturation multiplier.
// dd<=2°F  -> minSat ; dd>=25°F -> 1.0 ; smooth in between.
static inline float satFromDewSpreadF(float tempF, float dewF) {
  float dd = tempF - dewF;
  if (dd < 0.f)
    dd = 0.f;                         // guard bad inputs
  constexpr float kMinSat = 0.40f;    // floor (muggy look)
  constexpr float kMaxSpread = 25.0f; // “very dry” cap
  float u = skystrip::util::clamp01(dd / kMaxSpread);
  float eased = u * u * (3.f - 2.f * u); // smoothstep
  return kMinSat + (1.f - kMinSat) * eased;
}

struct Stop {
  double f;
  float h;
};
// Cold→Hot ramp in °F: 14,32,50,68,77,86,95,104
static const Stop kStopsF[] = {
    {14, 234.9f}, // deep blue
    {32, 207.0f}, // blue/cyan
    {50, 180.0f}, // cyan
    {68, 138.8f}, // greenish
    {77, 60.0f},  // yellow
    {86, 38.8f},  // orange
    {95, 18.8f},  // orange-red
    {104, 0.0f},  // red
};

static float hueForTempF(double f) {
  if (f <= kStopsF[0].f)
    return kStopsF[0].h;
  for (size_t i = 1; i < sizeof(kStopsF) / sizeof(kStopsF[0]); ++i) {
    if (f <= kStopsF[i].f) {
      const auto &A = kStopsF[i - 1];
      const auto &B = kStopsF[i];
      const double u = (f - A.f) / (B.f - A.f);
      return float(skystrip::util::lerp(A.h, B.h, u));
    }
  }
  return kStopsF[sizeof(kStopsF) / sizeof(kStopsF[0]) - 1].h;
}

TemperatureView::TemperatureView() : segId_(DEFAULT_SEG_ID) {
  DEBUG_PRINTLN("SkyStrip: TV::CTOR");
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

    double tempF = 0.f;
    double dewF = 0.f;
    float hue = 0.f;
    float sat = 1.0f;
    constexpr float val = 0.5f;
    uint32_t col = 0;
    if (skystrip::util::estimateTempAt(model, t, step, tempF)) {
      hue = hueForTempF(tempF);
      if (skystrip::util::estimateDewPtAt(model, t, step, dewF)) {
        sat = satFromDewSpreadF((float)tempF, (float)dewF);
      }
      col = skystrip::util::hsv2rgb(hue, sat, val);
    }

    float m = markerWeight(t);
    if (m > 0.f) {
      uint8_t blend = uint8_t(std::lround(m * 255.f));
      col = color_blend(col, 0, blend);
    }

    if (dbgPixelIndex >= 0) {
      static time_t lastDebug = 0;
      if (now - lastDebug > 1 && i == dbgPixelIndex) {
        char nowbuf[20];
        skystrip::util::fmt_local(nowbuf, sizeof(nowbuf), now);
        char dbgbuf[20];
        skystrip::util::fmt_local(dbgbuf, sizeof(dbgbuf), t);
        snprintf(debugPixelString, sizeof(debugPixelString),
                 "%s: nowtm=%s dbgndx=%d dbgtm=%s "
                 "tempF=%.1f dewF=%.1f "
                 "H=%.0f S=%.0f V=%.0f\\n",
                 name().c_str(), nowbuf, i, dbgbuf, tempF, dewF, hue, sat * 100,
                 val * 100);
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
  subtree[FPSTR(CFG_SEG_ID)] = segId_;
}

void TemperatureView::appendConfigData(Print &s) {
  // Keep the hint INLINE (BEFORE the input = 4th arg):
  s.print(F("addInfo('SkyStrip:TemperatureView:SegmentId',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(-1 disables)</small>'"
            ");"));
}

bool TemperatureView::readFromConfig(JsonObject &subtree, bool startup_complete,
                                     bool &invalidate_history) {
  bool configComplete = !subtree.isNull();
  configComplete &=
      getJsonValue(subtree[FPSTR(CFG_SEG_ID)], segId_, DEFAULT_SEG_ID);
  return configComplete;
}
