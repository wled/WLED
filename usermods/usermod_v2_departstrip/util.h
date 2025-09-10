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

inline long current_offset() {
  long off = (long)localTime - (long)toki.getTime().sec;
  if (off < -54000 || off > 54000) off = 0;
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

} // namespace util
} // namespace departstrip
