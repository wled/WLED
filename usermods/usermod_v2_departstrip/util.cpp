#include "util.h"

namespace departstrip { namespace util {

// Same simple HSV to RGB helper used in bartdepart
uint32_t hsv2rgb(float h, float s, float v) {
  if (s <= 0.f) return RGBW32(v * 255, v * 255, v * 255, 0);
  h = fmodf(h, 1.f); if (h < 0.f) h += 1.f;
  float i = floorf(h * 6.f);
  float f = h * 6.f - i;
  float p = v * (1.f - s);
  float q = v * (1.f - s * f);
  float t = v * (1.f - s * (1.f - f));
  float r=0,g=0,b=0;
  switch(((int)i) % 6) {
    case 0: r=v,g=t,b=p; break;
    case 1: r=q,g=v,b=p; break;
    case 2: r=p,g=v,b=t; break;
    case 3: r=p,g=q,b=v; break;
    case 4: r=t,g=p,b=v; break;
    case 5: r=v,g=p,b=q; break;
  }
  return RGBW32((uint8_t)(r*255),(uint8_t)(g*255),(uint8_t)(b*255),0);
}

} } // namespace

