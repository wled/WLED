#include "util.h"

namespace skystrip {
namespace util {

uint32_t hsv2rgb(float h, float s, float v) {
  // Normalize inputs to safe ranges
  if (s < 0.f) s = 0.f; else if (s > 1.f) s = 1.f;
  if (v < 0.f) v = 0.f; else if (v > 1.f) v = 1.f;
  float hh = fmodf(h, 360.f);
  if (hh < 0.f) hh += 360.f;

  float c = v * s;
  hh = hh / 60.f;
  float x = c * (1.f - fabsf(fmodf(hh, 2.f) - 1.f));
  float r1, g1, b1;
  if (hh < 1.f)      { r1 = c; g1 = x; b1 = 0.f; }
  else if (hh < 2.f) { r1 = x; g1 = c; b1 = 0.f; }
  else if (hh < 3.f) { r1 = 0.f; g1 = c; b1 = x; }
  else if (hh < 4.f) { r1 = 0.f; g1 = x; b1 = c; }
  else if (hh < 5.f) { r1 = x; g1 = 0.f; b1 = c; }
  else               { r1 = c; g1 = 0.f; b1 = x; }
  float m = v - c;
  // Clamp final channels into [0,1] to avoid rounding drift outside range
  float rf = r1 + m; if (rf < 0.f) rf = 0.f; else if (rf > 1.f) rf = 1.f;
  float gf = g1 + m; if (gf < 0.f) gf = 0.f; else if (gf > 1.f) gf = 1.f;
  float bf = b1 + m; if (bf < 0.f) bf = 0.f; else if (bf > 1.f) bf = 1.f;
  uint8_t r = uint8_t(lrintf(rf * 255.f));
  uint8_t g = uint8_t(lrintf(gf * 255.f));
  uint8_t b = uint8_t(lrintf(bf * 255.f));
  return RGBW32(r, g, b, 0);
}

} // namespace util
} // namespace skystrip
