#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "delta_view.h"
#include "skymodel.h"
#include "util.h"
#include "wled.h"

static constexpr int16_t DEFAULT_SEG_ID = -1; // -1 means disabled
const char CFG_SEG_ID[] PROGMEM = "SegmentId";
const char CFG_THRESHOLDS[] PROGMEM = "DeltaThresholds";

static constexpr float kDefaultThresholds[3] = {5.f, 10.f, 15.f};
static constexpr const char kDefaultThresholdStr[] = "5:10:15";
static constexpr float kMinThresholdF = 0.5f;
static constexpr float kMaxThresholdF = 60.0f;

static constexpr float kDeltaSat = 0.95f;
static constexpr float kValModerate = 0.35f;
static constexpr float kValStrong = 0.65f;
static constexpr float kValBright = 1.0f;

struct Stop {
  double f;
  float h;
};

static bool parseThresholdString(const char *in, float (&out)[3],
                                 String &normalized) {
  if (!in)
    return false;

  char buf[64];
  strncpy(buf, in, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';

  float vals[5];
  size_t count = 0;
  char *saveptr;
  for (char *tok = strtok_r(buf, ":, \t\r\n", &saveptr);
       tok && count < sizeof(vals) / sizeof(vals[0]);
       tok = strtok_r(nullptr, ":, \t\r\n", &saveptr)) {
    if (*tok == '\0')
      continue;
    float v = atof(tok);
    if (!std::isfinite(v))
      return false;
    vals[count++] = v;
  }

  if (count < 3)
    return false;

  if (vals[0] < kMinThresholdF || vals[0] > kMaxThresholdF)
    return false;
  for (size_t i = 1; i < count; ++i) {
    if (vals[i] <= vals[i - 1])
      return false;
    if (vals[i] < kMinThresholdF || vals[i] > kMaxThresholdF)
      return false;
  }

  normalized = "";
  for (size_t i = 0; i < 3; ++i) {
    out[i] = vals[i];
    if (i > 0)
      normalized += ':';
    float rounded = std::round(vals[i]);
    if (std::fabs(vals[i] - rounded) < 0.01f)
      normalized += String((int)rounded);
    else
      normalized += String(vals[i], 1);
  }
  return true;
}

static void buildHueStops(const float (&thr)[3], Stop (&stops)[7]) {
  stops[0] = {-thr[2], 270.f}; // purple
  stops[1] = {-thr[1], 240.f}; // indigo/blue-purple
  stops[2] = {-thr[0], 200.f}; // cyan-blue
  stops[3] = {0.0, 120.f};     // green (unused because < threshold)
  stops[4] = {thr[0], 60.f};   // yellow
  stops[5] = {thr[1], 30.f};   // orange
  stops[6] = {thr[2], 0.f};    // red
}

static float hueForDeltaF(double f, const Stop (&stops)[7]) {
  if (f <= stops[0].f)
    return stops[0].h;
  for (size_t i = 1; i < 7; ++i) {
    if (f <= stops[i].f) {
      const auto &A = stops[i - 1];
      const auto &B = stops[i];
      const double u = (f - A.f) / (B.f - A.f);
      return float(skystrip::util::lerp(A.h, B.h, u));
    }
  }
  return stops[6].h;
}

static inline float valueForDelta(double deltaF, const float (&thr)[3]) {
  float mag = float(std::fabs(deltaF));
  const float t1 = thr[0];
  const float t2 = thr[1];
  const float t3 = thr[2];

  if (mag < t1)
    return 0.f;
  if (mag < t2)
    return kValModerate;
  if (mag < t3) {
    float u = (mag - t2) / (t3 - t2);
    return float(skystrip::util::lerp(kValModerate, kValStrong, u));
  }
  float over = t3 - t2;
  if (over < 1.f)
    over = 1.f;
  float u = (mag - t3) / over;
  u = skystrip::util::clamp01(u);
  return float(skystrip::util::lerp(kValStrong, kValBright, u));
}

DeltaView::DeltaView() : segId_(DEFAULT_SEG_ID) {
  DEBUG_PRINTLN("SkyStrip: DV::CTOR");
  resetThresholdsToDefault();
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
  if (segId_ == DEFAULT_SEG_ID) {
    freezeHandle_.release();
    return;
  }
  if (model.temperature_forecast.empty())
    return;
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
  const time_t day = 24 * 3600;
  Stop hueStops[7];
  buildHueStops(thresholdsF_, hueStops);

  for (int i = 0; i < len; ++i) {
    const time_t t = now + time_t(std::llround(step * i));

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
      seg.setPixelColor(i, 0);
      continue;
    }
    double deltaT = tempNow - tempPrev;

    float hue = hueForDeltaF(deltaT, hueStops);
    float val = valueForDelta(deltaT, thresholdsF_);
    uint32_t col =
        (val > 0.f) ? skystrip::util::hsv2rgb(hue, kDeltaSat, val) : 0;

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
                 "dT=%.1f H=%.0f S=%.0f V=%.0f thr=%s\\n",
                 name().c_str(), nowbuf, i, dbgbuf, prvbuf, deltaT,
                 hue, kDeltaSat * 100, val * 100, thresholdsStr_.c_str());
        lastDebug = now;
      }
    }

    seg.setPixelColor(i, skystrip::util::blinkDebug(i, dbgPixelIndex, col));
  }
}

void DeltaView::deactivate() {
  freezeHandle_.release();
}

void DeltaView::addToConfig(JsonObject &subtree) {
  subtree[FPSTR(CFG_SEG_ID)] = segId_;
  subtree[FPSTR(CFG_THRESHOLDS)] = thresholdsStr_;
}

void DeltaView::appendConfigData(Print &s) {
  // Keep the hint INLINE (BEFORE the input = 4th arg):
  s.print(F("addInfo('SkyStrip:DeltaView:SegmentId',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(-1 disables)</small>'"
            ");"));
  s.print(F("addInfo('SkyStrip:DeltaView:DeltaThresholds',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(format: 5:10:15 = off/mod/strong/shout)</small>'"
            ");"));
}

bool DeltaView::readFromConfig(JsonObject &subtree, bool startup_complete,
                               bool &invalidate_history) {
  bool configComplete = !subtree.isNull();
  configComplete &=
      getJsonValue(subtree[FPSTR(CFG_SEG_ID)], segId_, DEFAULT_SEG_ID);
  if (!subtree[FPSTR(CFG_THRESHOLDS)].isNull()) {
    const char *cfg = subtree[FPSTR(CFG_THRESHOLDS)];
    bool parsed = applyThresholdConfig(cfg);
    configComplete &= parsed;
    if (!parsed)
      resetThresholdsToDefault();
  } else {
    resetThresholdsToDefault();
  }
  return configComplete;
}

void DeltaView::resetThresholdsToDefault() {
  thresholdsF_[0] = kDefaultThresholds[0];
  thresholdsF_[1] = kDefaultThresholds[1];
  thresholdsF_[2] = kDefaultThresholds[2];
  thresholdsStr_ = kDefaultThresholdStr;
}

bool DeltaView::applyThresholdConfig(const char *cfg) {
  float parsed[3];
  String normalized;
  if (!parseThresholdString(cfg, parsed, normalized))
    return false;
  memcpy(thresholdsF_, parsed, sizeof(thresholdsF_));
  thresholdsStr_ = normalized;
  return true;
}
