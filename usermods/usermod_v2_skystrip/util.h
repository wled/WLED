#pragma once

#include "skymodel.h"
#include "wled.h"
#include <cmath>
#include <cstdint>
#include <ctime>

namespace skystrip {
namespace util {

// Tracks freeze ownership for a single view so it only mutates
// its own segment’s freeze flag.
class SegmentFreezeHandle {
public:
  SegmentFreezeHandle() = default;
  ~SegmentFreezeHandle() { release(); }

  Segment *acquire(int16_t segId) {
    if (segId < 0) {
      release();
      return nullptr;
    }
    uint8_t maxSeg = strip.getMaxSegments();
    if (segId >= maxSeg) {
      release();
      return nullptr;
    }
    Segment &seg = strip.getSegment((uint8_t)segId);
    if (active_ && heldId_ == segId) {
      seg_ = &seg;
      if (!seg.freeze) {
        seg.freeze = true;
      }
      return seg_;
    }

    release();

    prevFreeze_ = seg.freeze;
    seg.freeze = true;
    active_ = true;
    heldId_ = segId;
    seg_ = &seg;
    return seg_;
  }

  void release() {
    if (!active_) return;
    if (seg_) seg_->freeze = prevFreeze_;
    seg_ = nullptr;
    heldId_ = -1;
    active_ = false;
  }

private:
  bool active_ = false;
  int16_t heldId_ = -1;
  bool prevFreeze_ = false;
  Segment *seg_ = nullptr;
};

// UTC now from WLED’s clock (same source the UI uses)
inline time_t time_now_utc() { return (time_t)toki.getTime().sec; }

// Current UTC→local offset in seconds (derived from WLED’s own localTime)
inline long current_offset() {
  long off = (long)localTime - (long)toki.getTime().sec;
  // sanity clamp ±15h (protects against early-boot junk)
  if (off < -54000 || off > 54000)
    off = 0;
  return off;
}

// Format any UTC epoch using WLED’s *current* offset
inline void fmt_local(char *out, size_t n, time_t utc_ts,
                      const char *fmt = "%m-%d %H:%M") {
  const time_t local_sec = utc_ts + current_offset();
  struct tm tmLocal;
  gmtime_r(&local_sec, &tmLocal); // local_sec is already local seconds
  strftime(out, n, fmt, &tmLocal);
}

// Clamp to [0,1]
template <typename T> inline T clamp01(T v) {
  return v < T(0) ? T(0) : (v > T(1) ? T(1) : v);
}

// Linear interpolation
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

// Forecast interpolation helper
static constexpr int GRACE_SEC = 60 * 60 * 3; // fencepost + slide
template <class Series>
bool estimateAt(const Series &v, time_t t, double /* step */, double &out) {
  if (v.empty())
    return false;
  // if it's too far away we didn't find estimate
  if (t < v.front().tstamp - GRACE_SEC)
    return false;
  if (t > v.back().tstamp + GRACE_SEC)
    return false;
  // just off the end uses end value
  if (t <= v.front().tstamp) {
    out = v.front().value;
    return true;
  }
  if (t >= v.back().tstamp) {
    out = v.back().value;
    return true;
  }
  // otherwise interpolate
  for (size_t i = 1; i < v.size(); ++i) {
    if (t <= v[i].tstamp) {
      const auto &a = v[i - 1];
      const auto &b = v[i];
      const double span = double(b.tstamp - a.tstamp);
      const double u = clamp01(span > 0 ? double(t - a.tstamp) / span : 0.0);
      out = lerp(a.value, b.value, u);
      return true;
    }
  }
  return false;
}

inline bool estimateTempAt(const SkyModel &m, time_t t, double step,
                           double &outF) {
  return estimateAt(m.temperature_forecast, t, step, outF);
}
inline bool estimateDewPtAt(const SkyModel &m, time_t t, double step,
                            double &outFdp) {
  return estimateAt(m.dew_point_forecast, t, step, outFdp);
}
inline bool estimateSpeedAt(const SkyModel &m, time_t t, double step,
                            double &out) {
  return estimateAt(m.wind_speed_forecast, t, step, out);
}
inline bool estimateDirAt(const SkyModel &m, time_t t, double /*step*/,
                          double &out) {
  const auto &v = m.wind_dir_forecast;
  if (v.empty()) return false;
  if (t < v.front().tstamp - GRACE_SEC) return false;
  if (t > v.back().tstamp + GRACE_SEC) return false;
  if (t <= v.front().tstamp) { out = fmod(v.front().value, 360.0); if (out < 0) out += 360.0; return true; }
  if (t >= v.back().tstamp)  { out = fmod(v.back().value, 360.0);  if (out < 0) out += 360.0;  return true; }

  for (size_t i = 1; i < v.size(); ++i) {
    if (t <= v[i].tstamp) {
      const auto &a = v[i-1];
      const auto &b = v[i];
      const double span = double(b.tstamp - a.tstamp);
      const double u = clamp01(span > 0 ? double(t - a.tstamp) / span : 0.0);
      double aAng = a.value;
      double bAng = b.value;
      // shortest signed angular difference in (-180,180]
      double delta = bAng - aAng;
      delta = fmod(delta + 540.0, 360.0) - 180.0;
      double val = aAng + u * delta;
      // normalize to [0,360)
      val = fmod(val, 360.0);
      if (val < 0) val += 360.0;
      out = val;
      return true;
    }
  }
  return false;
}
inline bool estimateGustAt(const SkyModel &m, time_t t, double step,
                           double &out) {
  return estimateAt(m.wind_gust_forecast, t, step, out);
}
inline bool estimateCloudAt(const SkyModel &m, time_t t, double step,
                            double &out) {
  return estimateAt(m.cloud_cover_forecast, t, step, out);
}
inline bool estimatePrecipTypeAt(const SkyModel &m, time_t t, double step,
                                 double &out) {
  return estimateAt(m.precip_type_forecast, t, step, out);
}
inline bool estimatePrecipProbAt(const SkyModel &m, time_t t, double step,
                                 double &out) {
  return estimateAt(m.precip_prob_forecast, t, step, out);
}

uint32_t hsv2rgb(float h, float s, float v);

inline uint32_t applySaturation(uint32_t col, float sat) {
  if (sat < 0.f)
    sat = 0.f;
  else if (sat > 1.f)
    sat = 1.f;

  const float r = float((col >> 16) & 0xFF);
  const float g = float((col >> 8) & 0xFF);
  const float b = float((col) & 0xFF);

  const float y = 0.2627f * r + 0.6780f * g + 0.0593f * b;

  auto mixc = [&](float c) {
    float v = y + sat * (c - y);
    if (v < 0.f)
      v = 0.f;
    if (v > 255.f)
      v = 255.f;
    return v;
  };

  const uint8_t r2 = uint8_t(lrintf(mixc(r)));
  const uint8_t g2 = uint8_t(lrintf(mixc(g)));
  const uint8_t b2 = uint8_t(lrintf(mixc(b)));
  return RGBW32(r2, g2, b2, 0);
}

// Blink a specific pixel between its color and a gray debug color.
// Call this at setPixel time to highlight dbgPixelIndex once per second.
inline uint32_t blinkDebug(int i, int16_t dbgPixelIndex, uint32_t col) {
  if (dbgPixelIndex >= 0 && i == dbgPixelIndex) {
    static const uint32_t dbgCol = hsv2rgb(0.f, 0.f, 0.4f);
    if ((millis() / 1000) & 1)
      return dbgCol;
  }
  return col;
}

} // namespace util
} // namespace skystrip
