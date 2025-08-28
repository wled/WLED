#pragma once

#include "wled.h"

#include <deque>
#include <vector>
#include <ctime>

#include "train_color.h"

struct BartStationModel {
  struct Platform {
    struct ETD {
      time_t     estDep;
      TrainColor color;
    };
    struct ETDBatch {
      time_t           apiTs;
      time_t           ourTs;
      std::vector<ETD> etds;
    };

    explicit Platform(const String& platformId);

    void update(const JsonObject& root);
    void merge(const Platform& other);
    time_t oldest() const;
    const String& platformId() const;
    const std::deque<ETDBatch>& history() const;
    const std::vector<String>& destinations() const;
    String toString() const;

   private:
    String platformId_;
    std::deque<ETDBatch> history_;
    std::vector<String> destinations_;

    // return UTC tstamp
    time_t parseHeaderTimestamp(const char* dateStr, const char* timeStr) const;
  };

  std::vector<Platform> platforms;

  void update(std::time_t now, BartStationModel&& delta);
  time_t oldest() const;
  std::vector<String> destinationsForPlatform(const String& platformId) const;
};
