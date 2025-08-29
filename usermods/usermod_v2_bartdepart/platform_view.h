#pragma once

#include "interfaces.h"
#include "bart_station_model.h"
#include "util.h"

class PlatformView : public IDataViewT<BartStationModel> {
private:
  uint16_t updateSecs_ = 60;
  String platformId_;
  int16_t segmentId_ = -1;
  std::string configKey_;
public:
  PlatformView(const String& platformId)
    : platformId_(platformId), segmentId_(-1),
      configKey_(std::string("PlatformView") + platformId.c_str()) {}

  void view(std::time_t now, const BartStationModel& model, int16_t dbgPixelIndex) override;
  void appendDebugPixel(Print& s) const override { /* empty */ }
  void addToConfig(JsonObject& root) override { root["SegmentId"] = segmentId_; }
  void appendConfigData(Print& s) override { appendConfigData(s, nullptr); }
  void appendConfigData(Print& s, const BartStationModel* model) override;
  bool readFromConfig(JsonObject& root, bool startup_complete, bool& invalidate_history) override {
    return getJsonValue(root["SegmentId"], segmentId_, segmentId_);
  }
  const char* configKey() const override { return configKey_.c_str(); }
  std::string name() const override { return configKey_; }
};
