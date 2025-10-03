#pragma once

#include "interfaces.h"
#include "skymodel.h"
#include "util.h"

class SkyModel;

class TemperatureView : public IDataViewT<SkyModel> {
public:
  TemperatureView();
  ~TemperatureView() override = default;

  // IDataViewT<SkyModel>
  void view(time_t now, SkyModel const & model, int16_t dbgPixelIndex) override;
  std::string name() const override { return "TV"; }
  void appendDebugPixel(Print& s) const override { s.print(debugPixelString); }
  void deactivate() override;

  // IConfigurable
  void addToConfig(JsonObject& subtree) override;
  void appendConfigData(Print& s) override;
  bool readFromConfig(JsonObject& subtree,
                      bool startup_complete,
                      bool& invalidate_history) override;
  const char* configKey() const override { return "TemperatureView"; }

private:
  int16_t segId_; // -1 means disabled
  char debugPixelString[128];
  skystrip::util::SegmentFreezeHandle freezeHandle_;
};
