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

static bool parseLeadingInt(const String& s, int& val, int& consumed) {
  val = 0; consumed = 0; bool any = false;
  for (unsigned i = 0; i < s.length(); ++i) {
    char ch = s.charAt(i);
    if (ch >= '0' && ch <= '9') { any = true; val = val*10 + (ch - '0'); ++consumed; }
    else break;
  }
  return any;
}

int cmpLineRefNatural(const String& a, const String& b) {
  int na=0, nb=0, ca=0, cb=0;
  bool ha = parseLeadingInt(a, na, ca);
  bool hb = parseLeadingInt(b, nb, cb);
  if (ha && hb) {
    if (na != nb) return (na < nb) ? -1 : 1;
    String sa = a.substring(ca);
    String sb = b.substring(cb);
    int r = sa.compareTo(sb);
    if (r != 0) return (r < 0) ? -1 : 1;
    return 0;
  }
  if (ha != hb) return ha ? -1 : 1; // numeric routes before non-numeric
  int r = a.compareTo(b);
  if (r != 0) return (r < 0) ? -1 : 1;
  return 0;
}

// Usermod-wide view window in minutes
static uint16_t g_display_minutes = 60;
uint16_t getDisplayMinutes() { return g_display_minutes; }
void setDisplayMinutes(uint16_t minutes) {
  // Clamp to a sane range to avoid degenerate behavior
  if (minutes < 10) minutes = 10;        // minimum 10 minutes
  if (minutes > 240) minutes = 240;      // maximum 4 hours
  g_display_minutes = minutes;
}

} } // namespace
