#include "cloud_view.h"
#include "skymodel.h"
#include "util.h"
#include "wled.h"
#include <algorithm>
#include <cmath>
#include <limits>

static constexpr int16_t DEFAULT_SEG_ID = -1; // -1 means disabled
const char CFG_SEG_ID[] PROGMEM = "SegmentId";

static bool isDay(const SkyModel &m, time_t t) {
  const time_t MAXTT = std::numeric_limits<time_t>::max();
  if (m.sunrise_ == 0 && m.sunset_ == MAXTT)
    return true; // 24h day
  if (m.sunset_ == 0 && m.sunrise_ == MAXTT)
    return false; // 24h night
  constexpr time_t DAY = 24 * 60 * 60;
  time_t sr = m.sunrise_;
  time_t ss = m.sunset_;
  while (t >= ss) {
    sr += DAY;
    ss += DAY;
  }
  while (t < sr) {
    sr -= DAY;
    ss -= DAY;
  }
  return t >= sr && t < ss;
}

CloudView::CloudView() : segId_(DEFAULT_SEG_ID) {
  DEBUG_PRINTLN("SkyStrip: CV::CTOR");
  snprintf(debugPixelString, sizeof(debugPixelString), "%s:\\n",
           name().c_str());
  debugPixelString[sizeof(debugPixelString) - 1] = '\0';
}

void CloudView::view(time_t now, SkyModel const &model, int16_t dbgPixelIndex) {
  if (dbgPixelIndex < 0) {
    snprintf(debugPixelString, sizeof(debugPixelString), "%s:\\n",
             name().c_str());
    debugPixelString[sizeof(debugPixelString) - 1] = '\0';
  }
  if (segId_ == DEFAULT_SEG_ID)
    return;
  if (model.cloud_cover_forecast.empty())
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

  const time_t markerTol = time_t(std::llround(step * 0.5));
  const time_t sunrise = model.sunrise_;
  const time_t sunset = model.sunset_;
  constexpr time_t DAY = 24 * 60 * 60;
  const time_t MAXTT = std::numeric_limits<time_t>::max();

  long offset = skystrip::util::current_offset();

  bool useSunrise = (sunrise != 0 && sunrise != MAXTT);
  bool useSunset = (sunset != 0 && sunset != MAXTT);
  time_t sunriseTOD = 0;
  time_t sunsetTOD = 0;
  if (useSunrise)
    sunriseTOD = (((sunrise + offset) % DAY) + DAY) % DAY; // normalize to [0, DAY)
  if (useSunset)
    sunsetTOD = (((sunset + offset) % DAY) + DAY) % DAY;   // normalize to [0, DAY)

  auto nearTOD = [&](time_t a, time_t b) {
    time_t diff = (a >= b) ? (a - b) : (b - a);
    if (diff <= markerTol)
      return true;
    return (DAY - diff) <= markerTol;
  };

  auto isMarker = [&](time_t t) {
    if (!useSunrise && !useSunset)
      return false;
    time_t tod = (((t + offset) % DAY) + DAY) % DAY; // normalize to [0, DAY)
    if (useSunrise && nearTOD(tod, sunriseTOD))
      return true;
    if (useSunset && nearTOD(tod, sunsetTOD))
      return true;
    return false;
  };

  constexpr float kCloudMaskThreshold = 0.05f;
  constexpr float kDayHue = 60.f;
  constexpr float kNightHue = 300.f;
  constexpr float kDaySat = 0.30f;
  constexpr float kNightSat = 0.00f;
  constexpr float kDayVMax  = 0.40f;
  constexpr float kNightVMax= 0.40f;

  // Brightness floor as a fraction of Vmax so mid/low clouds stay visible.
  constexpr float kDayVMinFrac   = 0.50f;  // try 0.40–0.60 to taste
  constexpr float kNightVMinFrac = 0.50f;  // night can be a bit lower if preferred

  constexpr float kMarkerHue= 25.f;
  constexpr float kMarkerSat= 0.60f;
  constexpr float kMarkerVal= 0.50f;

  for (int i = 0; i < len; ++i) {
    const time_t t = now + time_t(std::llround(step * i));
    double clouds, precipTypeVal, precipProb;
    if (!skystrip::util::estimateCloudAt(model, t, step, clouds))
      continue;
    if (!skystrip::util::estimatePrecipTypeAt(model, t, step, precipTypeVal))
      precipTypeVal = 0.0;
    if (!skystrip::util::estimatePrecipProbAt(model, t, step, precipProb))
      precipProb = 0.0;

    float clouds01 = skystrip::util::clamp01(float(clouds / 100.0));
    int p = int(std::round(precipTypeVal));
    bool daytime = isDay(model, t);
    int idx = seg.reverse ? (end - i) : (start + i);

    float hue = 0.f, sat = 0.f, val = 0.f;
    if (isMarker(t)) {
      // always put the sunrise sunset markers in
      hue = kMarkerHue;
      sat = kMarkerSat;
      val = kMarkerVal;
    } else if (p != 0 && precipProb > 0.0) {
      // precipitation has next priority: rain=blue, snow=lavender,
      // mixed=indigo-ish blend
      constexpr float kHueRain = 210.f; // deep blue
      constexpr float kSatRain = 1.00f;

      constexpr float kHueSnow = 285.f; // lavender for snow
      constexpr float kSatSnow = 0.35f; // pastel-ish (tune to taste)

      float ph, ps;
      if (p == 1) {
        // rain
        ph = kHueRain;
        ps = kSatRain;
      } else if (p == 2) {
        // snow → lavender
        ph = kHueSnow;
        ps = kSatSnow;
      } else {
        // mixed → halfway between blue and lavender
        ph = 0.5f * (kHueRain + kHueSnow); // ~247.5° (indigo-ish)
        ps = 0.5f * (kSatRain + kSatSnow); // ~0.675
      }

      float pv = skystrip::util::clamp01(float(precipProb));
      pv = 0.3f + 0.7f * pv; // brightness ramp
      hue = ph;
      sat = ps;
      val = pv;
    } else {
      // finally show daytime or nightime clouds
      if (clouds01 < kCloudMaskThreshold) {
        hue = 0.f;
        sat = 0.f;
        val = 0.f;
      } else {
        float vmax = daytime ? kDayVMax : kNightVMax;
        float vmin = (daytime ? kDayVMinFrac : kNightVMinFrac) * vmax;
        // Use sqrt curve to boost brightness at lower cloud coverage
        val  = vmin + (vmax - vmin) * sqrtf(clouds01);
        hue = daytime ? kDayHue : kNightHue;
        sat = daytime ? kDaySat : kNightSat;
      }
    }

    uint32_t col = skystrip::util::hsv2rgb(hue, sat, val);
    strip.setPixelColor(idx, skystrip::util::blinkDebug(i, dbgPixelIndex, col));

    if (dbgPixelIndex >= 0) {
      static time_t lastDebug = 0;
      if (now - lastDebug > 1 && i == dbgPixelIndex) {
        char nowbuf[20];
        skystrip::util::fmt_local(nowbuf, sizeof(nowbuf), now);
        char dbgbuf[20];
        skystrip::util::fmt_local(dbgbuf, sizeof(dbgbuf), t);
        snprintf(debugPixelString, sizeof(debugPixelString),
                 "%s: nowtm=%s dbgndx=%d dbgtm=%s day=%d clouds01=%.2f precip=%d pop=%.2f H=%.0f S=%.0f V=%.0f\\n",
                 name().c_str(), nowbuf, i, dbgbuf, daytime, clouds01, p,
                 precipProb, hue, sat * 100, val * 100);
        lastDebug = now;
      }
    }
  }
}

void CloudView::addToConfig(JsonObject &subtree) {
  subtree[FPSTR(CFG_SEG_ID)] = segId_;
}

void CloudView::appendConfigData(Print &s) {
  // Keep the hint INLINE (BEFORE the input = 4th arg):
  s.print(F("addInfo('SkyStrip:CloudView:SegmentId',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(-1 disables)</small>'"
            ");"));
}

bool CloudView::readFromConfig(JsonObject &subtree, bool startup_complete,
                               bool &invalidate_history) {
  bool configComplete = !subtree.isNull();
  configComplete &=
      getJsonValue(subtree[FPSTR(CFG_SEG_ID)], segId_, DEFAULT_SEG_ID);
  return configComplete;
}
