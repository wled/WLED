#pragma once

#include "wled.h"
#include <cmath>
#include <cstdint>
#include <ctime>

namespace departstrip {
namespace util {

struct FreezeGuard {
  Segment &seg;
  bool prev;
  explicit FreezeGuard(Segment &s, bool freezeNow = true) : seg(s), prev(s.freeze) {
    seg.freeze = freezeNow;
  }
  ~FreezeGuard() noexcept { seg.freeze = prev; }
  FreezeGuard(const FreezeGuard &) = delete;
  FreezeGuard &operator=(const FreezeGuard &) = delete;
};

inline time_t time_now_utc() { return (time_t)toki.getTime().sec; }
inline time_t time_now() { return time_now_utc(); }

static constexpr long kMaxOffsetSec = 15L * 3600L; // +/-15h safety clamp
inline long current_offset() {
  long off = (long)localTime - (long)toki.getTime().sec;
  if (off < -kMaxOffsetSec || off > kMaxOffsetSec) off = 0;
  return off;
}

inline void fmt_local(char *out, size_t n, time_t utc_ts,
                      const char *fmt = "%m-%d %H:%M") {
  const time_t local_sec = utc_ts + current_offset();
  struct tm tmLocal;
  gmtime_r(&local_sec, &tmLocal);
  strftime(out, n, fmt, &tmLocal);
}

uint32_t hsv2rgb(float h, float s, float v);

// Natural compare for route/line strings: compare by leading integer value
// if present (e.g. "10" < "100" < "100A"), then by the remaining suffix
// lexicographically. Returns negative if a<b, zero if equal, positive if a>b.
int cmpLineRefNatural(const String& a, const String& b);

// Global display time scale for views (in minutes).
// Default 60; configurable via usermod top-level setting.
uint16_t getDisplayMinutes();
void setDisplayMinutes(uint16_t minutes);

} // namespace util
} // namespace departstrip
