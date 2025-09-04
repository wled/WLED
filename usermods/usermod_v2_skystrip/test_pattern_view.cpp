#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

#include "wled.h"

#include "skymodel.h"
#include "test_pattern_view.h"
#include "util.h"

static constexpr int16_t DEFAULT_SEG_ID = -1; // -1 means disabled
const char CFG_SEG_ID[] PROGMEM = "SegmentId";
// legacy individual HSV components
const char CFG_START_HUE[] PROGMEM = "StartHue";
const char CFG_START_SAT[] PROGMEM = "StartSat";
const char CFG_START_VAL[] PROGMEM = "StartVal";
const char CFG_END_HUE[] PROGMEM = "EndHue";
const char CFG_END_SAT[] PROGMEM = "EndSat";
const char CFG_END_VAL[] PROGMEM = "EndVal";

// combined HSV strings (hue 0-360, sat/val 0-100%)
const char CFG_START_HSV[] PROGMEM = "StartHSV";
const char CFG_END_HSV[] PROGMEM = "EndHSV";

namespace {

void formatHSV(char *out, size_t len, float h, float s, float v) {
  // store saturation/value as percentages for readability
  snprintf(out, len, "H:%.0f S:%.0f V:%.0f", h, s * 100.f, v * 100.f);
}

bool parseHSV(const char *in, float &h, float &s, float &v) {
  if (!in)
    return false;

  char buf[64];
  strncpy(buf, in, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';

  float values[3] = {0.f, 0.f, 0.f};
  bool found[3] = {false, false, false};
  char *saveptr;
  for (char *tok = strtok_r(buf, ", \t\r\n", &saveptr); tok;
       tok = strtok_r(nullptr, ", \t\r\n", &saveptr)) {
    char *sep = strpbrk(tok, "=:");
    if (sep) {
      char key = tolower((unsigned char)tok[0]);
      float val = atof(sep + 1);
      if (key == 'h') {
        values[0] = val;
        found[0] = true;
      } else if (key == 's') {
        values[1] = val;
        found[1] = true;
      } else if (key == 'v') {
        values[2] = val;
        found[2] = true;
      }
    } else {
      for (int i = 0; i < 3; ++i) {
        if (!found[i]) {
          values[i] = atof(tok);
          found[i] = true;
          break;
        }
      }
    }
  }

  if (found[0] && found[1] && found[2]) {
    h = values[0];
    // wrap hue to [0,360)
    float hh = fmodf(h, 360.f);
    if (hh < 0.f) hh += 360.f;
    h = hh;
    // clamp saturation/value to [0,1]
    s = skystrip::util::clamp01(values[1] / 100.f);
    v = skystrip::util::clamp01(values[2] / 100.f);
    return true;
  }
  return false;
}

} // namespace

TestPatternView::TestPatternView()
    : segId_(DEFAULT_SEG_ID), startHue_(0.f), startSat_(0.f), startVal_(0.f),
      endHue_(0.f), endSat_(0.f), endVal_(1.f) {
  DEBUG_PRINTLN("SkyStrip: TP::CTOR");
  snprintf(debugPixelString, sizeof(debugPixelString), "%s:\\n",
           name().c_str());
  debugPixelString[sizeof(debugPixelString) - 1] = '\0';
}

void TestPatternView::view(time_t now, SkyModel const &model,
                           int16_t dbgPixelIndex) {
  if (dbgPixelIndex < 0) {
    snprintf(debugPixelString, sizeof(debugPixelString), "%s:\\n",
             name().c_str());
    debugPixelString[sizeof(debugPixelString) - 1] = '\0';
  }
  if (segId_ == DEFAULT_SEG_ID)
    return;
  if (segId_ < 0 || segId_ >= strip.getMaxSegments())
    return;

  Segment &seg = strip.getSegment((uint8_t)segId_);
  int len = seg.virtualLength();
  if (len <= 0)
    return;
  // Initialize segment drawing parameters so virtualLength()/mapping are valid
  seg.beginDraw();
  skystrip::util::FreezeGuard freezeGuard(seg, false);

  for (int i = 0; i < len; ++i) {
    float u = (len > 1) ? float(i) / float(len - 1) : 0.f;
    float h = startHue_ + (endHue_ - startHue_) * u;
    float s = startSat_ + (endSat_ - startSat_) * u;
    float v = startVal_ + (endVal_ - startVal_) * u;
    uint32_t col = skystrip::util::hsv2rgb(h, s, v);
    if (dbgPixelIndex >= 0) {
      static time_t lastDebug = 0;
      if (now - lastDebug > 1 && i == dbgPixelIndex) {
        char nowbuf[20];
        skystrip::util::fmt_local(nowbuf, sizeof(nowbuf), now);
        snprintf(debugPixelString, sizeof(debugPixelString),
                 "%s: nowtm=%s dbgndx=%d H=%.0f S=%.0f V=%.0f\\n",
                 name().c_str(), nowbuf, i, h, s * 100, v * 100);
        lastDebug = now;
      }
    }
    seg.setPixelColor(i, skystrip::util::blinkDebug(i, dbgPixelIndex, col));
  }
}

void TestPatternView::addToConfig(JsonObject &subtree) {
  subtree[FPSTR(CFG_SEG_ID)] = segId_;

  char buf[32];
  formatHSV(buf, sizeof(buf), startHue_, startSat_, startVal_);
  subtree[FPSTR(CFG_START_HSV)] = buf;
  formatHSV(buf, sizeof(buf), endHue_, endSat_, endVal_);
  subtree[FPSTR(CFG_END_HSV)] = buf;
}

void TestPatternView::appendConfigData(Print &s) {
  // Keep the hint INLINE (BEFORE the input = 4th arg):
  s.print(F("addInfo('SkyStrip:TestPatternView:SegmentId',1,'',"
            "'&nbsp;<small style=\\'opacity:.8\\'>(-1 disables)</small>'"
            ");"));
}

bool TestPatternView::readFromConfig(JsonObject &subtree, bool startup_complete,
                                     bool &invalidate_history) {
  bool configComplete = !subtree.isNull();
  configComplete &=
      getJsonValue(subtree[FPSTR(CFG_SEG_ID)], segId_, DEFAULT_SEG_ID);

  bool parsed = false;
  if (!subtree[FPSTR(CFG_START_HSV)].isNull()) {
    parsed = parseHSV(subtree[FPSTR(CFG_START_HSV)], startHue_, startSat_,
                      startVal_);
    configComplete &= parsed;
  } else {
    configComplete &=
        getJsonValue(subtree[FPSTR(CFG_START_HUE)], startHue_, 0.f);
    configComplete &=
        getJsonValue(subtree[FPSTR(CFG_START_SAT)], startSat_, 0.f);
    configComplete &=
        getJsonValue(subtree[FPSTR(CFG_START_VAL)], startVal_, 0.f);
  }

  if (!subtree[FPSTR(CFG_END_HSV)].isNull()) {
    parsed = parseHSV(subtree[FPSTR(CFG_END_HSV)], endHue_, endSat_, endVal_);
    configComplete &= parsed;
  } else {
    configComplete &= getJsonValue(subtree[FPSTR(CFG_END_HUE)], endHue_, 0.f);
    configComplete &= getJsonValue(subtree[FPSTR(CFG_END_SAT)], endSat_, 0.f);
    configComplete &= getJsonValue(subtree[FPSTR(CFG_END_VAL)], endVal_, 1.f);
  }
  return configComplete;
}
