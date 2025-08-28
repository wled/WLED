#include "platform_view.h"
#include "util.h"
#include "wled.h"

// helper to map TrainColor enum → CRGB
static CRGB colorFromTrainColor(TrainColor tc) {
  switch (tc) {
  case TrainColor::Red:
    return CRGB(255, 0, 0);
  case TrainColor::Orange:
    return CRGB(255, 150, 30);
  case TrainColor::Yellow:
    return CRGB(255, 255, 0);
  case TrainColor::Green:
    return CRGB(0, 255, 0);
  case TrainColor::Blue:
    return CRGB(0, 0, 255);
  case TrainColor::White:
    return CRGB(255, 255, 255);
  default:
    return CRGB(0, 0, 0);
  }
}

void PlatformView::view(std::time_t now, const BartStationModel &model,
                        int16_t dbgPixelIndex) {
  if (segmentId_ == -1)
    return; // disabled

  // find matching platform data first
  const BartStationModel::Platform* match = nullptr;
  for (const auto &platform : model.platforms) {
    if (platform.platformId() == platformId_) {
      match = &platform;
      break;
    }
  }
  if (!match)
    return;

  const auto &history = match->history();
  if (match->platformId().isEmpty())
    return;
  if (history.empty())
    return;

  static uint8_t frameCnt = 0;      // 0..99
  bool preferFirst = frameCnt < 50; // true for first 0.5 s
  frameCnt = (frameCnt + 1) % 100;

  const auto &batch = history.back();

  if (segmentId_ < 0 || segmentId_ >= strip.getMaxSegments())
    return;
  Segment &seg = strip.getSegment((uint8_t)segmentId_);
  int len = seg.virtualLength();
  if (len <= 0)
    return;

  // Ensure mapping is initialized (mirroring, grouping, reverse, etc.)
  seg.beginDraw();
  // Do not alter freeze state during overlay draws
  bartdepart::util::FreezeGuard freezeGuard(seg, false);

  auto brightness = [&](uint32_t c) {
    return (uint16_t)(((c >> 16) & 0xFF) + ((c >> 8) & 0xFF) + (c & 0xFF));
  };

  for (int i = 0; i < len; ++i) {
    uint32_t bestColor = 0;
    uint16_t bestB = 0;

    for (auto &e : batch.etds) {
      float diff = float(updateSecs_ + e.estDep - now) * len / 3600.0f;
      if (diff < 0.0f || diff >= len)
        continue;

      int idx = int(floor(diff));
      float frac = diff - float(idx);

      uint8_t b = 0;
      if (i == idx) {
        b = uint8_t((1.0f - frac) * 255);
      } else if (i == idx + 1) {
        b = uint8_t(frac * 255);
      } else {
        continue;
      }

      CRGB col = colorFromTrainColor(e.color);
      uint32_t newColor = (((uint32_t)col.r * b / 255) << 16) |
                          (((uint32_t)col.g * b / 255) << 8) |
                          ((uint32_t)col.b * b / 255);

      uint16_t newB = brightness(newColor);
      if ((preferFirst && (bestColor == 0 || newB > 2 * bestB)) ||
          (!preferFirst && newB * 2 >= bestB)) {
        bestColor = newColor;
        bestB = newB;
      }
    }

    seg.setPixelColor(i, bartdepart::util::blinkDebug(i, dbgPixelIndex, bestColor));
  }
}

void PlatformView::appendConfigData(Print &s, const BartStationModel *model) {
  // 4th arg (pre) = BEFORE input -> keep (-1 disables) right next to the field
  // 3rd arg (post) = AFTER input -> show the destinations note, visually separated
  s.print(F("addInfo('BartDepart:"));
  s.print(configKey()); // e.g. "PlatformView1"
  s.print(F(":SegmentId',1,'<div style=\\'margin-top:12px;\\'>"));
  if (model) {
    auto dests = model->destinationsForPlatform(platformId_);
    if (!dests.empty()) {
      s.print(F("Destinations: "));
      for (size_t i = 0; i < dests.size(); ++i) {
        if (i) s.print(F(", "));
        s.print(dests[i]);
      }
    } else {
      s.print(F("No destinations known"));
    }
  } else {
    s.print(F("No destinations known"));
  }
  s.print(F("</div>',"));
  s.print(F("'&nbsp;<small style=\\'opacity:.8\\'>(-1 disables)</small>'"));
  s.println(F(");"));
}
