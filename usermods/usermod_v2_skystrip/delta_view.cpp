#include <algorithm>
#include <cmath>

#include "delta_view.h"
#include "skymodel.h"
#include "util.h"
#include "wled.h"

static constexpr int16_t DEFAULT_SEG_ID = -1; // -1 means disabled
const char CFG_SEG_ID[] PROGMEM = "SegmentId";

struct Stop {
  double f;
  float h;
};
// Delta color ramp (°F)
static const Stop kStopsF[] = {
    {-20, 240.f}, // very cooling (blue)
    {-10, 210.f}, // cooling
    {-5, 180.f},  // slight cooling (cyan)
    {0, 120.f},   // neutral (green)
    {5, 60.f},    // slight warming (yellow)
    {10, 30.f},   // warming (orange)
    {20, 0.f},    // very warming (red)
};

static float hueForDeltaF(double f) {
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

static inline float satFromDewDiffDelta(float delta) {
  constexpr float kMinSat = 0.30f;
  constexpr float kMaxDelta = 15.0f; // +/-15F covers typical range
  float u = skystrip::util::clamp01((delta + kMaxDelta) / (2.f * kMaxDelta));
  return kMinSat + (1.f - kMinSat) * u;
}

static inline float intensityFromDeltas(double tempDelta, float humidDelta) {
  constexpr float kMaxTempDelta = 20.0f; // +/-20F covers intensity range
  constexpr float kMaxHumDelta = 15.0f;  // +/-15F covers typical humidity range
  float uT = skystrip::util::clamp01(float(std::fabs(tempDelta)) / kMaxTempDelta);
  float uH = skystrip::util::clamp01(std::fabs(humidDelta) / kMaxHumDelta);
  return skystrip::util::clamp01(std::sqrt(uT * uT + uH * uH)) * 0.9;
}

DeltaView::DeltaView() : segId_(DEFAULT_SEG_ID) {
  DEBUG_PRINTLN("SkyStrip: DV::CTOR");
  snprintf(debugPixelString, sizeof(debugPixelString), "%s:\\n",
           name().c_str());
  debugPixelString[sizeof(debugPixelString) - 1] = '\0';
}

void DeltaView::view(time_t now, SkyModel const &model, int16_t dbgPixelIndex) {
  if (dbgPixelIndex < 0) {
    snprintf(debugPixelString, sizeof(debugPixelString), "%s:\\n",
             name().c_str());
    debugPixelString[sizeof(debugPixelString) - 1] = '\0';
  }
  if (segId_ == DEFAULT_SEG_ID)
    return;
  if (model.temperature_forecast.empty())
    return;
  if (segId_ < 0 || segId_ >= strip.getMaxSegments())
    return;

  Segment &seg = strip.getSegment((uint8_t)segId_);
  int start = seg.start;
  int end = seg.stop - 1;
  int len = end - start + 1;
  if (len <= 0)
    return;
  skystrip::util::FreezeGuard freezeGuard(seg);

  constexpr double kHorizonSec = 48.0 * 3600.0;
  const double step = (len > 1) ? (kHorizonSec / double(len - 1)) : 0.0;
  const time_t day = 24 * 3600;

  for (int i = 0; i < len; ++i) {
    const time_t t = now + time_t(std::llround(step * i));
    int idx = seg.reverse ? (end - i) : (start + i);

    double tempNow, tempPrev;
    bool foundTempNow =
        skystrip::util::estimateAt(model.temperature_forecast, t, step, tempNow);
    bool foundTempPrev =
        skystrip::util::estimateAt(model.temperature_forecast, t - day, step, tempPrev);

    if (!foundTempNow || !foundTempPrev) {
      if (dbgPixelIndex >= 0) {
        static time_t lastDebug = 0;
        if (now - lastDebug > 1 && i == dbgPixelIndex) {
          char nowbuf[20];
          skystrip::util::fmt_local(nowbuf, sizeof(nowbuf), now);
          char dbgbuf[20];
          skystrip::util::fmt_local(dbgbuf, sizeof(dbgbuf), t);
          char prvbuf[20];
          skystrip::util::fmt_local(prvbuf, sizeof(prvbuf), t - day);
          snprintf(debugPixelString, sizeof(debugPixelString),
                   "%s: nowtm=%s dbgndx=%d dbgtm=%s prvtm=%s "
                   "foundTempPrev=%d foundTempNow=%d\\n",
                   name().c_str(), nowbuf, i, dbgbuf, prvbuf, foundTempPrev,
                   foundTempNow);
          lastDebug = now;
        }
      }
      strip.setPixelColor(idx, 0);
      continue;
    }
    double deltaT = tempNow - tempPrev;

    double dewNow, dewPrev;
    float sat = 1.0f;
    float spreadDelta = 0.f;
    if (skystrip::util::estimateAt(model.dew_point_forecast, t, step, dewNow) &&
        skystrip::util::estimateAt(model.dew_point_forecast, t - day, step, dewPrev)) {
      float spreadNow = float(tempNow - dewNow);
      float spreadPrev = float(tempPrev - dewPrev);
      spreadDelta = spreadNow - spreadPrev;
      sat = satFromDewDiffDelta(spreadDelta);
    }

    float hue = hueForDeltaF(deltaT);
    float val = intensityFromDeltas(deltaT, spreadDelta);
    uint32_t col = skystrip::util::hsv2rgb(hue, sat, val);

    if (dbgPixelIndex >= 0) {
      static time_t lastDebug = 0;
      if (now - lastDebug > 1 && i == dbgPixelIndex) {
        char nowbuf[20];
        skystrip::util::fmt_local(nowbuf, sizeof(nowbuf), now);
        char dbgbuf[20];
        skystrip::util::fmt_local(dbgbuf, sizeof(dbgbuf), t);
        char prvbuf[20];
        skystrip::util::fmt_local(prvbuf, sizeof(prvbuf), t - day);
        snprintf(debugPixelString, sizeof(debugPixelString),
                 "%s: nowtm=%s dbgndx=%d dbgtm=%s prvtm=%s "
                 "dT=%.1f dSpread=%.1f "
                 "H=%.0f S=%.0f V=%.0f\\n",
                 name().c_str(), nowbuf, i, dbgbuf, prvbuf, deltaT, spreadDelta,
                 hue, sat * 100, val * 100);
        lastDebug = now;
      }
    }

    strip.setPixelColor(idx, skystrip::util::blinkDebug(i, dbgPixelIndex, col));
  }
}

void DeltaView::addToConfig(JsonObject &subtree) {
  subtree[FPSTR(CFG_SEG_ID)] = segId_;
}

void DeltaView::appendConfigData(Print &s) {
  // Keep the hint INLINE (BEFORE the input = 4th arg):
  s.print(F("addInfo('SkyStrip:DeltaView:SegmentId',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(-1 disables)</small>'"
            ");"));
}

bool DeltaView::readFromConfig(JsonObject &subtree, bool startup_complete,
                               bool &invalidate_history) {
  bool configComplete = !subtree.isNull();
  configComplete &=
      getJsonValue(subtree[FPSTR(CFG_SEG_ID)], segId_, DEFAULT_SEG_ID);
  return configComplete;
}
