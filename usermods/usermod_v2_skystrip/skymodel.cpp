#include <cassert>
#include <limits>
#include <algorithm>

#include "wled.h"

#include "skymodel.h"
#include "util.h"

namespace {
  static constexpr time_t HISTORY_SEC = 25 * 60 * 60;  // keep an extra history point
  // Preallocate enough space for forecast (48h) plus backfilled history (~24h)
  // without imposing a hard cap; vectors can still grow beyond this reserve.
  static constexpr size_t MAX_POINTS = 80;

template <class Series>
void mergeSeries(Series &current, Series &&fresh, time_t now) {
  if (fresh.empty()) return;

  if (current.empty()) {
    current = std::move(fresh);
  } else if (fresh.back().tstamp < current.front().tstamp) {
    // Fresh points are entirely earlier than current data; prepend in-place.
    fresh.reserve(current.size() + fresh.size());
    fresh.insert(fresh.end(), current.begin(), current.end());
    current = std::move(fresh);
  } else {
    auto it = std::lower_bound(current.begin(), current.end(), fresh.front().tstamp,
                               [](const DataPoint& dp, time_t t){ return dp.tstamp < t; });
    current.erase(it, current.end());
    current.insert(current.end(), fresh.begin(), fresh.end());
  }

  time_t cutoff = now - HISTORY_SEC;
  auto itCut = std::lower_bound(current.begin(), current.end(), cutoff,
                                [](const DataPoint& dp, time_t t){ return dp.tstamp < t; });
  current.erase(current.begin(), itCut);
}
} // namespace

SkyModel::SkyModel() {
  temperature_forecast.reserve(MAX_POINTS);
  dew_point_forecast.reserve(MAX_POINTS);
  wind_speed_forecast.reserve(MAX_POINTS);
  wind_gust_forecast.reserve(MAX_POINTS);
  wind_dir_forecast.reserve(MAX_POINTS);
  cloud_cover_forecast.reserve(MAX_POINTS);
  precip_type_forecast.reserve(MAX_POINTS);
  precip_prob_forecast.reserve(MAX_POINTS);
}

SkyModel & SkyModel::update(time_t now, SkyModel && other) {
  lcl_tstamp = other.lcl_tstamp;

  mergeSeries(temperature_forecast, std::move(other.temperature_forecast), now);
  mergeSeries(dew_point_forecast, std::move(other.dew_point_forecast), now);
  mergeSeries(wind_speed_forecast, std::move(other.wind_speed_forecast), now);
  mergeSeries(wind_gust_forecast, std::move(other.wind_gust_forecast), now);
  mergeSeries(wind_dir_forecast, std::move(other.wind_dir_forecast), now);
  mergeSeries(cloud_cover_forecast, std::move(other.cloud_cover_forecast), now);
  mergeSeries(precip_type_forecast, std::move(other.precip_type_forecast), now);
  mergeSeries(precip_prob_forecast, std::move(other.precip_prob_forecast), now);

  if (!(other.sunrise_ == 0 && other.sunset_ == 0)) {
    sunrise_ = other.sunrise_;
    sunset_  = other.sunset_;
  }

#ifdef WLED_DEBUG
  emitDebug(now, DEBUGOUT);
#endif

  return *this;
}

void SkyModel::invalidate_history(time_t now) {
  temperature_forecast.clear();
  dew_point_forecast.clear();
  wind_speed_forecast.clear();
  wind_gust_forecast.clear();
  wind_dir_forecast.clear();
  cloud_cover_forecast.clear();
  precip_type_forecast.clear();
  precip_prob_forecast.clear();
  sunrise_ = 0;
  sunset_  = 0;
}

time_t SkyModel::oldest() const {
  time_t out = std::numeric_limits<time_t>::max();
  auto upd = [&](const std::vector<DataPoint>& s){
    if (!s.empty() && s.front().tstamp < out) out = s.front().tstamp;
  };
  upd(temperature_forecast);
  upd(dew_point_forecast);
  upd(wind_speed_forecast);
  upd(wind_gust_forecast);
  upd(wind_dir_forecast);
  upd(cloud_cover_forecast);
  upd(precip_type_forecast);
  upd(precip_prob_forecast);
  if (out == std::numeric_limits<time_t>::max()) return 0;
  return out;
}

// Streamed/line-by-line variant to keep packets small.
template <class Series>
static inline void emitSeriesMDHM(Print &out, time_t now, const char *label,
                                  const Series &s) {
  char tb[20];
  skystrip::util::fmt_local(tb, sizeof(tb), now);
  char line[256];
  int len = snprintf(line, sizeof(line), "SkyModel: now=%s: %s(%u):[\n",
                     tb, label, (unsigned)s.size());
  out.write((const uint8_t*)line, len);

  if (s.empty()) {
    len = snprintf(line, sizeof(line), "SkyModel: ]\n");
    out.write((const uint8_t*)line, len);
    return;
  }

  size_t i = 0;
  size_t off = 0;
  const size_t cap = sizeof(line);
  for (const auto& dp : s) {
    if (i % 6 == 0) {
      int n = snprintf(line, cap, "SkyModel:");
      off = (n < 0) ? 0u : ((size_t)n >= cap ? cap - 1 : (size_t)n);
    }
    skystrip::util::fmt_local(tb, sizeof(tb), dp.tstamp);
    if (off < cap) {
      size_t rem = cap - off;
      int n = snprintf(line + off, rem, " (%s, %6.2f)", tb, dp.value);
      if (n > 0) off += ((size_t)n >= rem ? rem - 1 : (size_t)n);
    }
    if (i % 6 == 5 || i == s.size() - 1) {
      if (i == s.size() - 1 && off < cap) {
        size_t rem = cap - off;
        int n = snprintf(line + off, rem, " ]");
        if (n > 0) off += ((size_t)n >= rem ? rem - 1 : (size_t)n);
      }
      if (off >= cap) off = cap - 1; // ensure space for newline
      line[off++] = '\n';
      out.write((const uint8_t*)line, off);
    }
    ++i;
  }
}

void SkyModel::emitDebug(time_t now, Print& out) const {
  emitSeriesMDHM(out, now, " temp",  temperature_forecast);
  emitSeriesMDHM(out, now, " dwpt",  dew_point_forecast);
  emitSeriesMDHM(out, now, " wspd",  wind_speed_forecast);
  emitSeriesMDHM(out, now, " wgst",  wind_gust_forecast);
  emitSeriesMDHM(out, now, " wdir",  wind_dir_forecast);
  emitSeriesMDHM(out, now, " clds",  cloud_cover_forecast);
  emitSeriesMDHM(out, now, " prcp",  precip_type_forecast);
  emitSeriesMDHM(out, now, " pop",   precip_prob_forecast);

  char tb[20];
  char line[64];
  skystrip::util::fmt_local(tb, sizeof(tb), sunrise_);
  int len = snprintf(line, sizeof(line), "SkyModel: sunrise %s\n", tb);
  out.write((const uint8_t*)line, len);
  skystrip::util::fmt_local(tb, sizeof(tb), sunset_);
  len = snprintf(line, sizeof(line), "SkyModel: sunset %s\n", tb);
  out.write((const uint8_t*)line, len);
}
