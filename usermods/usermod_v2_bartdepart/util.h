#pragma once

#include "wled.h"
#include <cmath>
#include <cstdint>
#include <ctime>

namespace bartdepart {
namespace util {

// RAII guard to temporarily freeze a segment during rendering and always
// restore its original freeze state on exit (including early returns).
struct FreezeGuard {
  Segment &seg;
  bool prev;
  explicit FreezeGuard(Segment &s, bool freezeNow = true) : seg(s), prev(s.freeze) {
    seg.freeze = freezeNow;
  }
  ~FreezeGuard() { seg.freeze = prev; }
  FreezeGuard(const FreezeGuard &) = delete;
  FreezeGuard &operator=(const FreezeGuard &) = delete;
};

inline time_t time_now_utc() { return (time_t)toki.getTime().sec; }

inline time_t time_now() { return time_now_utc(); }

inline long current_offset() {
  long off = (long)localTime - (long)toki.getTime().sec;
  if (off < -54000 || off > 54000)
    off = 0;
  return off;
}

inline void fmt_local(char *out, size_t n, time_t utc_ts,
                      const char *fmt = "%m-%d %H:%M") {
  const time_t local_sec = utc_ts + current_offset();
  struct tm tmLocal;
  gmtime_r(&local_sec, &tmLocal);
  strftime(out, n, fmt, &tmLocal);
}

template <typename T> inline T clamp01(T v) {
  return v < T(0) ? T(0) : (v > T(1) ? T(1) : v);
}

inline double lerp(double a, double b, double t) { return a + (b - a) * t; }

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

// Blink a pixel between its color and a gray debug color to mark dbgPixelIndex.
inline uint32_t blinkDebug(int i, int16_t dbgPixelIndex, uint32_t col) {
  if (dbgPixelIndex >= 0 && i == dbgPixelIndex) {
    static const uint32_t dbgCol = hsv2rgb(0.f, 0.f, 0.4f);
    if ((millis() / 1000) & 1)
      return dbgCol;
  }
  return col;
}

} // namespace util
} // namespace bartdepart
