#pragma once
#include <memory>
#include <vector>

#include "interfaces.h"
#include "wled.h"

#define DEPARTSTRIP_VERSION "0.0.1"
#define USERMOD_ID_DEPARTSTRIP 561

class DepartModel;
class SiriSource;
class DepartureView;

class DepartStrip : public Usermod {
private:
  bool enabled_ = false;
  uint16_t displayMinutes_ = 60; // usermod-wide view window in minutes
  uint32_t safeToStart_ = 0;
  bool edgeInit_ = false;
  bool lastOff_ = false;
  bool lastEnabled_ = false;

  std::vector<std::unique_ptr<IDataSourceT<DepartModel>>> sources_;
  std::unique_ptr<DepartModel> model_;
  std::vector<std::unique_ptr<IDataViewT<DepartModel>>> views_;
  bool skipColorMapLoad_ = false; // one-shot: skip loading ColorMap after reset

public:
  DepartStrip();
  ~DepartStrip() override = default;
  void setup() override;
  void loop() override;
  void handleOverlayDraw() override;
  void addToConfig(JsonObject &obj) override;
  void appendConfigData(Print& s) override;
  bool readFromConfig(JsonObject& obj) override;
  uint16_t getId() override { return USERMOD_ID_DEPARTSTRIP; }

  inline void enable(bool en) { enabled_ = en; }
  inline bool isEnabled() const { return enabled_; }

protected:
  void showBooting();
  void doneBooting();
  void reloadSources(std::time_t now);
};
