#pragma once
#include <memory>

#include "interfaces.h"
#include "wled.h"

#define USERMOD_ID_SKYSTRIP 559

#define SKYSTRIP_VERSION_FALLBACK "0.0.1"

#ifndef SKYSTRIP_GIT_DESCRIBE
#define SKYSTRIP_GIT_DESCRIBE SKYSTRIP_VERSION_FALLBACK
#endif

// Uncomment to restore the 10s startup safety delay that defers network calls.
// #define SKYSTRIP_ENABLE_SAFETY_DELAY

#define SKYSTRIP_VERSION SKYSTRIP_GIT_DESCRIBE

class SkyModel;

enum class SkyStripState {
  Initial,	   // initial state
  Setup,           // setup() has completed
  Running          // after a short delay to allow offMode
};

class SkyStrip : public Usermod {
private:
  bool enabled_ = false;
  int16_t dbgPixelIndex_ = -1; // if >=0 show periodic debugging for that pixel
  SkyStripState state_ = SkyStripState::Initial;
#ifdef SKYSTRIP_ENABLE_SAFETY_DELAY
  uint32_t safeToStart_ = 0;
#endif
  uint32_t lastLoop_ = 0;
  bool edgeInit_ = false;
  bool lastOff_ = false;
  bool lastEnabled_ = false;

  std::vector<std::unique_ptr<IDataSourceT<SkyModel>>> sources_;
  std::unique_ptr<SkyModel> model_;
  std::vector<std::unique_ptr<IDataViewT<SkyModel>>> views_;

public:
  SkyStrip();
  ~SkyStrip() override = default;
  void setup() override;
  void loop() override;
  void handleOverlayDraw() override;
  void addToConfig(JsonObject &obj) override;
  void appendConfigData(Print& s) override;
  bool readFromConfig(JsonObject &obj) override;
  uint16_t getId() override { return USERMOD_ID_SKYSTRIP; };

  // for other usermods
  inline void enable(bool enable) { enabled_ = enable; }
  inline bool isEnabled() { return enabled_; }

protected:
  void showBooting();
  void doneBooting();
  void reloadSources(std::time_t now);
};
