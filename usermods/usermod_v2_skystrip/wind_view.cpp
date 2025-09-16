#include "wind_view.h"
#include "skymodel.h"
#include "util.h"
#include "wled.h"
#include <algorithm>
#include <cmath>

static constexpr int16_t DEFAULT_SEG_ID = -1; // -1 means disabled
const char CFG_SEG_ID[] PROGMEM = "SegmentId";

static inline float hueFromDir(float dir) {
  // Normalize direction to [0, 360)
  dir = fmodf(dir, 360.f);
  if (dir < 0.f) dir += 360.f;
  float hue;
  if (dir <= 90.f)
    hue = 240.f + dir * ((30.f + 360.f - 240.f) / 90.f);
  else if (dir <= 180.f)
    hue = 30.f + (dir - 90.f) * ((60.f - 30.f) / 90.f);
  else if (dir <= 270.f)
    hue = 60.f + (dir - 180.f) * ((120.f - 60.f) / 90.f);
  else
    hue = 120.f + (dir - 270.f) * ((240.f - 120.f) / 90.f);
  hue = fmodf(hue, 360.f);
  return hue;
}

static inline float satFromGustDiff(float speed, float gust) {
  float diff = gust - speed;
  if (diff < 0.f)
    diff = 0.f;
  constexpr float kMinSat = 0.40f;
  constexpr float kMaxDiff = 20.0f;
  float u = skystrip::util::clamp01(diff / kMaxDiff);
  float eased = u * u * (3.f - 2.f * u);
  return kMinSat + (1.f - kMinSat) * eased;
}

WindView::WindView() : segId_(DEFAULT_SEG_ID) {
  DEBUG_PRINTLN("SkyStrip: WV::CTOR");
  snprintf(debugPixelString, sizeof(debugPixelString), "%s:\\n",
           name().c_str());
  debugPixelString[sizeof(debugPixelString) - 1] = '\0';
}

void WindView::view(time_t now, SkyModel const &model, int16_t dbgPixelIndex) {
  if (dbgPixelIndex < 0) {
    snprintf(debugPixelString, sizeof(debugPixelString), "%s:\\n",
             name().c_str());
    debugPixelString[sizeof(debugPixelString) - 1] = '\0';
  }
  if (segId_ == DEFAULT_SEG_ID) {
    freezeHandle_.release();
    return;
  }
  if (model.wind_speed_forecast.empty())
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

  for (int i = 0; i < len; ++i) {
    const time_t t = now + time_t(std::llround(step * i));
    double spd, dir, gst;
    if (!skystrip::util::estimateSpeedAt(model, t, step, spd))
      continue;
    if (!skystrip::util::estimateDirAt(model, t, step, dir))
      continue;
    if (!skystrip::util::estimateGustAt(model, t, step, gst))
      gst = spd;
    float hue = hueFromDir((float)dir);
    float sat = satFromGustDiff((float)spd, (float)gst);

    // Boost low winds with a floor so sub-10 values aren't lost to
    // quantization/gamma.
    float u = skystrip::util::clamp01(float(std::max(spd, gst)) / 50.f);
    constexpr float kMinV =
        0.18f; // visible floor when wind > 0 (tune 0.12–0.22)
    float val = (u <= 0.f) ? 0.f : (kMinV + (1.f - kMinV) * u);

    uint32_t col = skystrip::util::hsv2rgb(hue, sat, val);

    if (dbgPixelIndex >= 0) {
      static time_t lastDebug = 0;
      if (now - lastDebug > 1 && i == dbgPixelIndex) {
        char nowbuf[20];
        skystrip::util::fmt_local(nowbuf, sizeof(nowbuf), now);
        char dbgbuf[20];
        skystrip::util::fmt_local(dbgbuf, sizeof(dbgbuf), t);
        snprintf(debugPixelString, sizeof(debugPixelString),
                 "%s: nowtm=%s dbgndx=%d dbgtm=%s "
                 "spd=%.0f gst=%.0f dir=%.0f "
                 "H=%.0f S=%.0f V=%.0f\\n",
                 name().c_str(), nowbuf, i, dbgbuf, spd, gst, dir, hue,
                 sat * 100, val * 100);
        lastDebug = now;
      }
    }

    seg.setPixelColor(i, skystrip::util::blinkDebug(i, dbgPixelIndex, col));
  }
}

void WindView::deactivate() {
  freezeHandle_.release();
}

void WindView::addToConfig(JsonObject &subtree) {
  subtree[FPSTR(CFG_SEG_ID)] = segId_;
}

void WindView::appendConfigData(Print &s) {
  // Keep the hint INLINE (BEFORE the input = 4th arg):
  s.print(F("addInfo('SkyStrip:WindView:SegmentId',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(-1 disables)</small>'"
            ");"));
}

bool WindView::readFromConfig(JsonObject &subtree, bool startup_complete,
                              bool &invalidate_history) {
  bool configComplete = !subtree.isNull();
  configComplete &=
      getJsonValue(subtree[FPSTR(CFG_SEG_ID)], segId_, DEFAULT_SEG_ID);
  return configComplete;
}
