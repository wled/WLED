#include "departure_view.h"
#include "util.h"
#include "wled.h"
#include <vector>
#include <algorithm>
#include <cstring>

using departstrip::util::FreezeGuard;

static CRGB colorForAgencyLine(const String& agency, const String& lineRef) {
  uint32_t rgb;
  // Model provides defaults (neutral gray) when unknown
  DepartModel::getColorRGB(agency, lineRef, rgb);
  return CRGB((rgb>>16)&0xFF, (rgb>>8)&0xFF, rgb&0xFF);
}

void DepartureView::view(std::time_t now, const DepartModel &model) {
  if (segmentId_ == -1) return;

  struct Src { const DepartModel::Entry::Batch* batch; String agency; };
  std::vector<Src> sources;
  // Collect latest batch from each configured board key
  if (!keys_.empty()) {
    for (const auto& k : keys_) {
      const auto* e = model.find(k);
      if (!e) continue;
      // Agency is prefix before ':'
      String ag; int c = k.indexOf(':'); if (c > 0) ag = k.substring(0, c);
      sources.push_back(Src{ &e->batch, ag });
    }
  } else {
    // Backward safety: attempt to use raw viewKey if parsing failed
    const auto* e = model.find(keysStr_);
    if (e) {
      String ag; int c = keysStr_.indexOf(':'); if (c > 0) ag = keysStr_.substring(0, c);
      sources.push_back(Src{ &e->batch, ag });
    }
  }
  if (sources.empty()) return;

  if (segmentId_ < 0 || segmentId_ >= strip.getMaxSegments()) return;
  Segment &seg = strip.getSegment((uint8_t)segmentId_);
  int len = seg.virtualLength();
  if (len <= 0) return;

  seg.beginDraw();
  FreezeGuard freezeGuard(seg, false);

  auto brightness = [&](uint32_t c) {
    return (uint16_t)(((c >> 16) & 0xFF) + ((c >> 8) & 0xFF) + (c & 0xFF));
  };

  const uint8_t CYCLE_ALPHA = 100; // require to participate in cycling

  // Avoid copying Strings per pixel; keep pointer to existing lineRef
  struct Cand { uint32_t color; uint16_t bsum; const String* key; uint8_t alpha; };
  size_t estCap = 0;
  for (const auto& src : sources) estCap += src.batch->items.size();
  std::vector<Cand> cands; cands.reserve(estCap);
  std::vector<Cand> strong; strong.reserve(8);

  for (int i = 0; i < len; ++i) {
    cands.clear();
    strong.clear();

    for (const auto& src : sources) {
      for (auto &e : src.batch->items) {
        float diff = float(boardingSecs_ + e.estDep - now) * len / 3600.0f;
        if (diff < 0.0f || diff >= len) continue;
        int idx = int(floor(diff));
        float frac = diff - float(idx);

        uint8_t alpha = 0;
        if (i == idx) alpha = uint8_t((1.0f - frac) * 255);
        else if (i == idx + 1) alpha = uint8_t(frac * 255);
        else continue;

        CRGB col = colorForAgencyLine(src.agency, e.lineRef);
        uint32_t colScaled = (((uint32_t)col.r * alpha / 255) << 16) |
                             (((uint32_t)col.g * alpha / 255) << 8) |
                             ((uint32_t)col.b * alpha / 255);
        cands.push_back(Cand{colScaled, brightness(colScaled), &e.lineRef, alpha});
      }
    }

    uint32_t out = 0;
    if (!cands.empty()) {
      // Build strong set for cycling (alpha above threshold)
      for (const auto& c : cands) if (c.alpha >= CYCLE_ALPHA) strong.push_back(c);
      if (strong.empty()) {
        // No strong overlaps; choose the brightest single candidate (no cycle)
        const Cand* best = nullptr;
        for (const auto& c : cands) if (!best || c.bsum > best->bsum) best = &c;
        if (best) out = best->color;
      } else if (strong.size() == 1) {
        out = strong[0].color;
      } else {
        // Stable, deterministic order by entity key (lineRef)
        std::sort(strong.begin(), strong.end(), [](const Cand& a, const Cand& b){
          const char* ak = (a.key && a.key->length()) ? a.key->c_str() : "";
          const char* bk = (b.key && b.key->length()) ? b.key->c_str() : "";
          return strcmp(ak, bk) < 0;
        });
        uint32_t nowMs = millis();
        size_t n = strong.size();
        if (n == 0) {
          // Should be unreachable due to branch conditions; leave pixel off as a safe default
          out = 0;
        } else {
          uint32_t n32 = (uint32_t)n;
          uint32_t slice = 1000u / n32; // total cycle ~1s; clamp to >=1ms
          if (slice == 0) slice = 1;
          uint32_t phase = (nowMs / slice) % n32;
          out = strong[phase].color;
        }
      }
    }

    seg.setPixelColor(i, out);
  }
}

void DepartureView::appendConfigData(Print &s, const DepartModel *model) {
  // Placeholder for UI helpers (e.g., show recent LineRefs)
  s.print(F("addInfo('DepartStrip:"));
  s.print(configKey());
  s.print(F(":SegmentId',1,'<div style=\\'margin-top:12px;\\'>"));
  if (model) {
    int total = 0; int boards = 0;
    if (!keys_.empty()) {
      for (const auto& k : keys_) {
        const auto* e = model->find(k);
        if (e) { total += (int)e->batch.items.size(); ++boards; }
      }
    } else {
      const auto* e = model->find(keysStr_);
      if (e) { total += (int)e->batch.items.size(); boards = 1; }
    }
    if (boards > 0) { s.print(F("Items: ")); s.print(total); }
    else { s.print(F("No data yet")); }
  } else { s.print(F("No data yet")); }
  s.print(F("</div>',"));
  s.print(F("'&nbsp;<small style=\\'opacity:.8\\'>(-1 disables)</small>'"));
  s.println(F(");"));
}
