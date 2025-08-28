#pragma once

#include "interfaces.h"
#include "skymodel.h"

class SkyModel;

class DeltaView : public IDataViewT<SkyModel> {
public:
  DeltaView();
  ~DeltaView() override = default;

  void view(time_t now, SkyModel const & model, int16_t dbgPixelIndex) override;
  std::string name() const override { return "DV"; }
  void appendDebugPixel(Print& s) const override { s.print(debugPixelString); }

  void addToConfig(JsonObject& subtree) override;
  void appendConfigData(Print& s) override;
  bool readFromConfig(JsonObject& subtree,
                      bool startup_complete,
                      bool& invalidate_history) override;
  const char* configKey() const override { return "DeltaView"; }

private:
  int16_t segId_;
  char debugPixelString[256];
};
