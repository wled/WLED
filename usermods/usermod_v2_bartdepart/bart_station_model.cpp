#include "bart_station_model.h"
#include "util.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

BartStationModel::Platform::Platform(const String& platformId)
  : platformId_(platformId) {}

void BartStationModel::Platform::update(const JsonObject& root) {
  if (platformId_.isEmpty()) return;

  ETDBatch batch;
  const char* dateStr = root["date"] | "";
  const char* timeStr = root["time"] | "";
  batch.apiTs = parseHeaderTimestamp(dateStr, timeStr);
  batch.ourTs = bartdepart::util::time_now();

  if (root["station"].is<JsonArray>()) {
    for (JsonObject station : root["station"].as<JsonArray>()) {
      if (!station["etd"].is<JsonArray>()) continue;
      for (JsonObject etd : station["etd"].as<JsonArray>()) {
        if (!etd["estimate"].is<JsonArray>()) continue;
        bool matches = false;
        for (JsonObject est : etd["estimate"].as<JsonArray>()) {
          if (String(est["platform"] | "0") != platformId_) continue;
          matches = true;
          int mins = atoi(est["minutes"] | "0");
          time_t dep = batch.apiTs + mins * 60;
          TrainColor col = parseTrainColor(est["color"] | "");
          batch.etds.push_back(ETD{dep, col});
        }
        if (matches) {
          String dest = etd["destination"] | "";
          if (!dest.isEmpty()) {
            auto it = std::find(destinations_.begin(), destinations_.end(), dest);
            if (it == destinations_.end()) destinations_.push_back(dest);
          }
        }
      }
    }
  }

  std::sort(batch.etds.begin(), batch.etds.end(),
    [](const ETD& a, const ETD& b){ return a.estDep < b.estDep; });

  history_.push_back(std::move(batch));
  while (history_.size() > 5) {
    history_.pop_front();
  }

  DEBUG_PRINTF("BartDepart::update platform %s: %s\n",
               platformId_.c_str(), toString().c_str());
}

void BartStationModel::Platform::merge(const Platform& other) {
  for (auto const& b : other.history_) {
    history_.push_back(b);
    if (history_.size() > 5) history_.pop_front();
  }

  for (auto const& d : other.destinations_) {
    auto it = std::find(destinations_.begin(), destinations_.end(), d);
    if (it == destinations_.end()) destinations_.push_back(d);
  }
}

time_t BartStationModel::Platform::oldest() const {
  if (history_.empty()) return 0;
  return history_.front().ourTs;
}

const String& BartStationModel::Platform::platformId() const {
  return platformId_;
}

const std::deque<BartStationModel::Platform::ETDBatch>&
BartStationModel::Platform::history() const {
  return history_;
}

const std::vector<String>& BartStationModel::Platform::destinations() const {
  return destinations_;
}

String BartStationModel::Platform::toString() const {
  if (history_.empty()) return String();

  const ETDBatch& batch = history_.back();
  const auto& etds = batch.etds;
  if (etds.empty()) return String();

  char nowBuf[20];
  bartdepart::util::fmt_local(nowBuf, sizeof(nowBuf), batch.ourTs, "%H:%M:%S");
  int lagSecs = batch.ourTs - batch.apiTs;

  String out;
  out += nowBuf;
  out += ": lag ";
  out += lagSecs;
  out += ":";

  time_t prevTs = batch.ourTs;

  for (const auto& e : etds) {
    out += " +";
    int diff = e.estDep - prevTs;
    out += diff / 60;
    out += " (";
    prevTs = e.estDep;

    char depBuf[20];
    bartdepart::util::fmt_local(depBuf, sizeof(depBuf), e.estDep, "%H:%M:%S");
    out += depBuf;
    out += ":";
    out += ::toString(e.color);
    out += ")";
  }
  return out;
}

time_t BartStationModel::Platform::parseHeaderTimestamp(const char* dateStr,
                                                        const char* timeStr) const {
  int month=0, day=0, year=0;
  int hour=0, min=0, sec=0;
  char ampm[3] = {0};
  sscanf(dateStr, "%d/%d/%d", &month, &day, &year);
  sscanf(timeStr, "%d:%d:%d %2s", &hour, &min, &sec, ampm);
  if (strcasecmp(ampm, "PM") == 0 && hour < 12) hour += 12;
  if (strcasecmp(ampm, "AM") == 0 && hour == 12) hour = 0;
  struct tm tm{};
  tm.tm_year = year - 1900;
  tm.tm_mon  = month - 1;
  tm.tm_mday = day;
  tm.tm_hour = hour;
  tm.tm_min  = min;
  tm.tm_sec  = sec;
  return mktime(&tm) - bartdepart::util::current_offset(); // return UTC
}

void BartStationModel::update(std::time_t now, BartStationModel&& delta) {
  for (auto &p : delta.platforms) {
    auto it = std::find_if(platforms.begin(), platforms.end(),
      [&](const Platform& x){ return x.platformId() == p.platformId(); });
    if (it != platforms.end()) {
      it->merge(p);
    } else {
      platforms.push_back(std::move(p));
    }
  }
}

time_t BartStationModel::oldest() const {
  time_t oldest = 0;
  for (auto const& p : platforms) {
    time_t o = p.oldest();
    if (!oldest || (o && o < oldest)) oldest = o;
  }
  return oldest;
}

std::vector<String> BartStationModel::destinationsForPlatform(const String& platformId) const {
  for (auto const& p : platforms) {
    if (p.platformId() == platformId) {
      return p.destinations();
    }
  }
  return {};
}
