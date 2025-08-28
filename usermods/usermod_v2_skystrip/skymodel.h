#pragma once

#include <ctime>
#include <vector>
#include <memory>

class Print;

#include "interfaces.h"

struct DataPoint {
  time_t tstamp;
  double value;
};

class SkyModel {
public:
  SkyModel();

  // move-only
  SkyModel(const SkyModel &) = delete;
  SkyModel &operator=(const SkyModel &) = delete;

  SkyModel(SkyModel &&) noexcept = default;
  SkyModel &operator=(SkyModel &&) noexcept = default;

  ~SkyModel() = default;

  SkyModel & update(time_t now, SkyModel && other);  // use std::move
  void invalidate_history(time_t now);
  time_t oldest() const;
  void emitDebug(time_t now, Print& out) const;

  std::time_t lcl_tstamp{0};			// update timestamp from our clock
  std::vector<DataPoint> temperature_forecast;
  std::vector<DataPoint> dew_point_forecast;
  std::vector<DataPoint> wind_speed_forecast;
  std::vector<DataPoint> wind_gust_forecast;
  std::vector<DataPoint> wind_dir_forecast;
  std::vector<DataPoint> cloud_cover_forecast;
  std::vector<DataPoint> precip_type_forecast;   // 0 none, 1 rain, 2 snow, 3 mixed
  std::vector<DataPoint> precip_prob_forecast;   // 0..1 probability of precip

  // sunrise/sunset times from current data
  time_t sunrise_{0};
  time_t sunset_{0};
};
