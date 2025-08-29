#pragma once
#include <memory>
#include <vector>

#include "interfaces.h"
#include "wled.h"

#define BARTDEPART2_VERSION "0.0.1"

#define USERMOD_ID_BARTDEPART 560

class BartStationModel;
class LegacyBartSource;
class PlatformView;

enum class BartDepartState {
  Initial,
  Setup,
  Running
};

class BartDepart : public Usermod {
private:
  bool enabled_ = false;
  int16_t dbgPixelIndex_ = -1;
  BartDepartState state_ = BartDepartState::Initial;
  uint32_t safeToStart_ = 0;
  bool edgeInit_ = false;
  bool lastOff_ = false;
  bool lastEnabled_ = false;

  std::vector<std::unique_ptr<IDataSourceT<BartStationModel>>> sources_;
  std::unique_ptr<BartStationModel> model_;
  std::vector<std::unique_ptr<IDataViewT<BartStationModel>>> views_;

public:
  BartDepart();
  ~BartDepart() override = default;
  void setup() override;
  void loop() override;
  void handleOverlayDraw() override;
  void addToConfig(JsonObject &obj) override;
  void appendConfigData(Print& s) override;
  bool readFromConfig(JsonObject& obj) override;
  uint16_t getId() override { return USERMOD_ID_BARTDEPART; }

  inline void enable(bool en) { enabled_ = en; }
  inline bool isEnabled() { return enabled_; }

protected:
  void showBooting();
  void doneBooting();
  void reloadSources(std::time_t now);
};
