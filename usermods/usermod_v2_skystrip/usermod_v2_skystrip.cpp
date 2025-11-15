#include <cstdint>
#include <ctime>
#include <memory>

#include "usermod_v2_skystrip.h"
#include "interfaces.h"
#include "util.h"

#include "skymodel.h"
#include "open_weather_map_source.h"
#include "temperature_view.h"
#include "wind_view.h"
#include "cloud_view.h"
#include "delta_view.h"
#include "test_pattern_view.h"

const char CFG_NAME[] PROGMEM = "SkyStrip";
const char CFG_ENABLED[] PROGMEM = "Enabled";
const char CFG_PIXEL_DBG_NAME[] PROGMEM = "DebugPixel";
const char CFG_DBG_PIXEL_INDEX[] PROGMEM = "Index";

static SkyStrip skystrip_usermod;
REGISTER_USERMOD(skystrip_usermod);

// Don't handle the loop function for SAFETY_DELAY_MS. If we've
// coded a deadlock or crash in the loop handler this will give us a
// chance to offMode the device so we can use the OTA update to fix
// the problem.
const uint32_t SAFETY_DELAY_MS = 10u * 1000u;

// runs before readFromConfig() and setup()
SkyStrip::SkyStrip() {
  DEBUG_PRINTLN(F("SkyStrip::SkyStrip CTOR"));
  sources_.push_back(::make_unique<OpenWeatherMapSource>());
  model_ = ::make_unique<SkyModel>();
  views_.push_back(::make_unique<CloudView>());
  views_.push_back(::make_unique<WindView>());
  views_.push_back(::make_unique<TemperatureView>());
  views_.push_back(::make_unique<DeltaView>());
  views_.push_back(::make_unique<TestPatternView>());
}

void SkyStrip::setup() {
  // NOTE - it's a really bad idea to crash or deadlock in this
  // method; you won't be able to use OTA update and will have to
  // resort to a serial connection to unbrick your controller ...

  // NOTE - if you are using UDP logging the DEBUG_PRINTLNs in this
  // routine will likely not show up because this is prior to WiFi
  // being up.

  DEBUG_PRINTLN(F("SkyStrip::setup starting"));

  uint32_t now_ms = millis();
  safeToStart_ = now_ms + SAFETY_DELAY_MS;

  // Serial.begin(115200);

  // Print version number
  DEBUG_PRINT(F("SkyStrip version: "));
  DEBUG_PRINTLN(SKYSTRIP_VERSION);

  // Start a nice chase so we know its booting
  showBooting();

  state_ = SkyStripState::Setup;

  DEBUG_PRINTLN(F("SkyStrip::setup finished"));
}

void SkyStrip::loop() {
  uint32_t now_ms = millis();

  // init edge baselines once
  if (!edgeInit_) {
    lastOff_     = offMode;
    lastEnabled_ = enabled_;
    edgeInit_    = true;
  }

  time_t const now = skystrip::util::time_now_utc();

  // defer a short bit after reboot
  if (state_ == SkyStripState::Setup) {
    if (now_ms < safeToStart_) {
      return;
    } else {
      DEBUG_PRINTLN(F("SkyStrip::loop SkyStripState is Running"));
      state_ = SkyStripState::Running;
      doneBooting();
      reloadSources(now); // load right away
    }
  }

  // detect OFF->ON and disabled->enabled edges
  const bool becameOn       = (lastOff_ && !offMode);
  const bool becameEnabled  = (!lastEnabled_ && enabled_);
  if (becameOn || becameEnabled) {
    reloadSources(now);
  }
  lastOff_     = offMode;
  lastEnabled_ = enabled_;

  // make sure we are enabled and on
  if (!enabled_ || offMode) return;

  // check the sources for updates, apply to model if found
  for (auto &source : sources_) {
    if (auto frmsrc = source->fetch(now)) {
      // this happens relatively infrequently, once an hour
      model_->update(now, std::move(*frmsrc));
    }
    if (auto hist = source->checkhistory(now, model_->oldest())) {
      model_->update(now, std::move(*hist));
    }
  }
}

void SkyStrip::handleOverlayDraw() {
    // this happens a hundred times a second
  if (!enabled_) {
    for (auto &view : views_) view->deactivate();
    return;
  }
  if (offMode) {
    return;
  }
  time_t now = skystrip::util::time_now_utc();
  for (auto &view : views_) {
    view->view(now, *model_, dbgPixelIndex_);
  }
}

// called by WLED when settings are saved
void SkyStrip::addToConfig(JsonObject& root) {
  JsonObject top = root.createNestedObject(FPSTR(CFG_NAME));

  // write our state
  top[FPSTR(CFG_ENABLED)] = enabled_;

  // write the sources
  for (auto& src : sources_) {
    JsonObject sub = top.createNestedObject(src->configKey());
    src->addToConfig(sub);
  }

  // write the views
  for (auto& vw : views_) {
    JsonObject sub = top.createNestedObject(vw->configKey());
    vw->addToConfig(sub);
  }

  JsonObject sub = top.createNestedObject(FPSTR(CFG_PIXEL_DBG_NAME));
  sub[FPSTR(CFG_DBG_PIXEL_INDEX)] = dbgPixelIndex_;
}

void SkyStrip::appendConfigData(Print& s) {
  for (auto& src : sources_) {
    src->appendConfigData(s);
  }

  for (auto& vw : views_) {
    vw->appendConfigData(s);
  }

  // Keep the hint INLINE (BEFORE the input = 4th arg):
  s.print(F(
            "addInfo('SkyStrip:DebugPixel:Index',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(-1 disables)</small>'"
            ");"
            ));

  // Open a read-only textarea region for the pixel debugging
  s.print(F(
            "addInfo('SkyStrip:DebugPixel:Index',1,"
            "'<br><textarea id=\\'skystrip_pre\\' rows=\\'8\\' wrap=\\'off\\' readonly "
            "spellcheck=\\'false\\' "
            "style=\\'width:calc(100% - 12rem);margin:0 6rem;"
            "background:#000;color:#fff;text-align:left;"
            "font-family:monospace;line-height:1.3;tab-size:2;"
            "overflow:auto;-webkit-user-select:text;user-select:text;\\' "
            "title=\\'Click to select; then copy\\' "
            "onclick=\\'this.focus();this.select();\\'>"
            ));

  // append the most recent debug pixel info from each view
  for (auto& vw : views_) {
    vw->appendDebugPixel(s);
  }

  // Close the textarea region
  s.print(F(
            "</textarea>'"
            ");"
            ));
}

// called by WLED when settings are restored
bool SkyStrip::readFromConfig(JsonObject& root) {
  JsonObject top = root[FPSTR(CFG_NAME)];
  if (top.isNull()) return false;

  bool ok = true;
  bool invalidate_history = false;

  // It is not safe to make API calls during startup
  bool  startup_complete = state_ == SkyStripState::Running;

  ok &= getJsonValue(top[FPSTR(CFG_ENABLED)], enabled_, false);

  JsonObject sub = top[FPSTR(CFG_PIXEL_DBG_NAME)];
  ok &= getJsonValue(sub[FPSTR(CFG_DBG_PIXEL_INDEX)], dbgPixelIndex_, -1);

  // read the sources
  for (auto& src : sources_) {
    JsonObject sub1 = top[src->configKey()];
    ok &= src->readFromConfig(sub1, startup_complete, invalidate_history);
    DEBUG_PRINTF("SkyStrip:readFromConfig: after source %s invalidate_history=%d\n",
                 src->name().c_str(), invalidate_history);
  }

  // read the views
  for (auto& vw : views_) {
    JsonObject sub2 = top[vw->configKey()];
    ok &= vw->readFromConfig(sub2, startup_complete, invalidate_history);
    DEBUG_PRINTF("SkyStrip:readFromConfig: after view %s invalidate_history=%d\n",
                 vw->name().c_str(), invalidate_history);
  }

  if (invalidate_history) {
    DEBUG_PRINTLN(F("SkyStrip::readFromConfig invalidating history"));
    time_t const now = skystrip::util::time_now_utc();
    model_->invalidate_history(now);
    if (startup_complete) reloadSources(now); // not safe during startup
  }

  return ok;
}

void SkyStrip::showBooting() {
  Segment& seg = strip.getMainSegment();
  seg.setMode(28); // Set to chase
  seg.speed = 200;
  // seg.intensity = 255; // preserve user's settings via webapp
  seg.setPalette(128);
  seg.setColor(0, 0x404060);
  seg.setColor(1, 0x000000);
  seg.setColor(2, 0x303040);
}

void SkyStrip::doneBooting() {
  Segment& seg = strip.getMainSegment();
  seg.setMode(0);       // static palette/color mode
  // seg.intensity = 255;  // preserve user's settings via webapp
}

void SkyStrip::reloadSources(std::time_t now) {
  char nowBuf[20];
  skystrip::util::fmt_local(nowBuf, sizeof(nowBuf), now);
  DEBUG_PRINTF("SkyStrip::ReloadSources at %s\n", nowBuf);

  for (auto &src : sources_) src->reload(now);
}
