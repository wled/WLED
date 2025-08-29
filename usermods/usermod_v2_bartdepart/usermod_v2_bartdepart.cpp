#include <cstdint>
#include <ctime>
#include <memory>

#include "usermod_v2_bartdepart.h"
#include "interfaces.h"
#include "util.h"

#include "bart_station_model.h"
#include "legacy_bart_source.h"
#include "platform_view.h"

const char CFG_NAME[] PROGMEM = "BartDepart";
const char CFG_ENABLED[] PROGMEM = "Enabled";
const char CFG_DBG_PIXEL_INDEX[] PROGMEM = "DebugPixelIndex";

static BartDepart bartdepart_usermod;
REGISTER_USERMOD(bartdepart_usermod);

// Delay after boot (milliseconds) to allow disabling before heavy work
const uint32_t SAFETY_DELAY_MS = 10u * 1000u;

BartDepart::BartDepart() {
  sources_.push_back(::make_unique<LegacyBartSource>());
  model_ = ::make_unique<BartStationModel>();
  views_.push_back(::make_unique<PlatformView>("1"));
  views_.push_back(::make_unique<PlatformView>("2"));
  views_.push_back(::make_unique<PlatformView>("3"));
  views_.push_back(::make_unique<PlatformView>("4"));
}

void BartDepart::setup() {
  DEBUG_PRINTLN(F("BartDepart::setup starting"));
  uint32_t now_ms = millis();
  safeToStart_ = now_ms + SAFETY_DELAY_MS;
  showBooting();
  state_ = BartDepartState::Setup;
  DEBUG_PRINTLN(F("BartDepart::setup finished"));
}

void BartDepart::loop() {
  uint32_t now_ms = millis();
  if (!edgeInit_) {
    lastOff_ = offMode;
    lastEnabled_ = enabled_;
    edgeInit_ = true;
  }

  time_t const now = bartdepart::util::time_now_utc();

  if (state_ == BartDepartState::Setup) {
    if (now_ms < safeToStart_) return;
    state_ = BartDepartState::Running;
    doneBooting();
    reloadSources(now);
  }

  bool becameOn = (lastOff_ && !offMode);
  bool becameEnabled = (!lastEnabled_ && enabled_);
  if (becameOn || becameEnabled) {
    reloadSources(now);
  }
  lastOff_ = offMode;
  lastEnabled_ = enabled_;

  if (!enabled_ || offMode || strip.isUpdating()) return;

  for (auto& src : sources_) {
    if (auto data = src->fetch(now)) {
      model_->update(now, std::move(*data));
    }
    if (auto hist = src->checkhistory(now, model_->oldest())) {
      model_->update(now, std::move(*hist));
    }
  }
}

void BartDepart::handleOverlayDraw() {
  time_t now = bartdepart::util::time_now_utc();
  for (auto& view : views_) {
    view->view(now, *model_, dbgPixelIndex_);
  }
}

void BartDepart::addToConfig(JsonObject& root) {
  JsonObject top = root.createNestedObject(FPSTR(CFG_NAME));
  top[FPSTR(CFG_ENABLED)] = enabled_;
  top[FPSTR(CFG_DBG_PIXEL_INDEX)] = dbgPixelIndex_;
  for (auto& src : sources_) {
    JsonObject sub = top.createNestedObject(src->configKey());
    src->addToConfig(sub);
  }
  for (auto& vw : views_) {
    JsonObject sub = top.createNestedObject(vw->configKey());
    vw->addToConfig(sub);
  }
}

void BartDepart::appendConfigData(Print& s) {
  for (auto& src : sources_) {
    src->appendConfigData(s);
  }

  for (auto& vw : views_) {
    vw->appendConfigData(s, model_.get());
  }
}

bool BartDepart::readFromConfig(JsonObject& root) {
  JsonObject top = root[FPSTR(CFG_NAME)];
  if (top.isNull()) return true;

  bool ok = true;
  bool invalidate_history = false;
  bool startup_complete = state_ == BartDepartState::Running;

  ok &= getJsonValue(top[FPSTR(CFG_ENABLED)], enabled_, false);
  ok &= getJsonValue(top[FPSTR(CFG_DBG_PIXEL_INDEX)], dbgPixelIndex_, -1);

  for (auto& src : sources_) {
    JsonObject sub = top[src->configKey()];
    ok &= src->readFromConfig(sub, startup_complete, invalidate_history);
  }
  for (auto& vw : views_) {
    JsonObject sub = top[vw->configKey()];
    ok &= vw->readFromConfig(sub, startup_complete, invalidate_history);
  }

  if (invalidate_history) {
    model_->platforms.clear();
    if (startup_complete) reloadSources(bartdepart::util::time_now_utc());
  }

  return ok;
}

void BartDepart::showBooting() {
  Segment& seg = strip.getMainSegment();
  seg.setMode(28);
  seg.speed = 200;
  seg.setPalette(128);
  seg.setColor(0, 0x404060);
  seg.setColor(1, 0x000000);
  seg.setColor(2, 0x303040);
}

void BartDepart::doneBooting() {
  Segment& seg = strip.getMainSegment();
  seg.freeze = true;
  seg.setMode(0);
}

void BartDepart::reloadSources(std::time_t now) {
  for (auto& src : sources_) src->reload(now);
}
