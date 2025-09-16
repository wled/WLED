#pragma once

#include "interfaces.h"
#include "skymodel.h"
#include "util.h"

class SkyModel;

class CloudView : public IDataViewT<SkyModel> {
public:
  CloudView();
  ~CloudView() override = default;

  void view(time_t now, SkyModel const & model, int16_t dbgPixelIndex) override;
  std::string name() const override { return "CV"; }
  void appendDebugPixel(Print& s) const override { s.print(debugPixelString); }
  void deactivate() override;

  void addToConfig(JsonObject& subtree) override;
  void appendConfigData(Print& s) override;
  bool readFromConfig(JsonObject& subtree,
                      bool startup_complete,
                      bool& invalidate_history) override;
  const char* configKey() const override { return "CloudView"; }

private:
  int16_t segId_;
  char debugPixelString[128];
  skystrip::util::SegmentFreezeHandle freezeHandle_;
};
