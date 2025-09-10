#include <cstdint>
#include <ctime>
#include <memory>
#include <algorithm>

#include "usermod_v2_departstrip.h"
#include "interfaces.h"
#include "util.h"

#include "depart_model.h"
#include "siri_source.h"
#include "departure_view.h"

const char CFG_NAME[] PROGMEM = "DepartStrip";
const char CFG_ENABLED[] PROGMEM = "Enabled";

static DepartStrip departstrip_usermod;
REGISTER_USERMOD(departstrip_usermod);

// Delay after boot (milliseconds) to allow disabling before heavy work
static const uint32_t SAFETY_DELAY_MS = 10u * 1000u;

DepartStrip::DepartStrip() {
  sources_.reserve(4);
  model_ = ::make_unique<DepartModel>();
  views_.reserve(4);
}

void DepartStrip::setup() {
  DEBUG_PRINTLN(F("DepartStrip: DepartStrip::setup: starting"));
  uint32_t now_ms = millis();
  safeToStart_ = now_ms + SAFETY_DELAY_MS;
  if (enabled_) showBooting();
  DEBUG_PRINTLN(F("DepartStrip: DepartStrip::setup: finished"));
}

void DepartStrip::loop() {
  uint32_t now_ms = millis();
  if (!edgeInit_) {
    lastOff_ = offMode;
    lastEnabled_ = enabled_;
    edgeInit_ = true;
  }

  time_t const now = departstrip::util::time_now_utc();

  if (now_ms < safeToStart_) return;

  bool becameOn = (lastOff_ && !offMode);
  bool becameEnabled = (!lastEnabled_ && enabled_);
  if (becameOn || becameEnabled) {
    if (now > 0) reloadSources(now);
  }
  lastOff_ = offMode;
  lastEnabled_ = enabled_;

  if (!enabled_ || offMode || strip.isUpdating()) return;

  for (auto& src : sources_) {
    if (auto data = src->fetch(now)) {
      model_->update(now, std::move(*data));
    }
  }
}

void DepartStrip::handleOverlayDraw() {
  if (!enabled_ || offMode) return;
  time_t now = departstrip::util::time_now_utc();
  for (auto& view : views_) {
    view->view(now, *model_);
  }
}

void DepartStrip::addToConfig(JsonObject& root) {
  JsonObject top = root.createNestedObject(FPSTR(CFG_NAME));
  top[FPSTR(CFG_ENABLED)] = enabled_;
  // ColorMap: reset control rendered after ColorMap
  // Emit sources sorted by their Key (AGENCY:StopCode) for stable order
  struct SrcRef { String key; IDataSourceT<DepartModel>* ptr; };
  std::vector<SrcRef> sorder; sorder.reserve(sources_.size());
  for (auto& up : sources_) {
    SiriSource* ss = static_cast<SiriSource*>(up.get());
    String k = ss ? ss->sourceKey() : String();
    sorder.push_back(SrcRef{k, up.get()});
  }
  std::sort(sorder.begin(), sorder.end(), [](const SrcRef& a, const SrcRef& b){ return a.key.compareTo(b.key) < 0; });
  for (auto& r : sorder) {
    IConfigurable* ic = static_cast<IConfigurable*>(r.ptr);
    JsonObject sub = top.createNestedObject(ic->configKey());
    ic->addToConfig(sub);
    sub["Delete"] = false;
  }
  // Placeholder to add new source right after existing sources
  {
    JsonObject ns = top.createNestedObject("NewSource");
    ns["Enabled"] = false;
    ns["UpdateSecs"] = 60;
    ns["TemplateUrl"] = String(F(""));
    ns["ApiKey"] = "";
    ns["AgencyStopCode"] = ""; // format AGENCY:StopCode
  }
  // Then existing views (sorted by Key for stable order)
  struct VwRef { String key; IDataViewT<DepartModel>* ptr; };
  std::vector<VwRef> vorder; vorder.reserve(views_.size());
  for (auto& up : views_) {
    DepartureView* dv = static_cast<DepartureView*>(up.get());
    String k = dv ? dv->viewKey() : String();
    vorder.push_back(VwRef{k, up.get()});
  }
  std::sort(vorder.begin(), vorder.end(), [](const VwRef& a, const VwRef& b){ return a.key.compareTo(b.key) < 0; });
  for (auto& r : vorder) {
    IConfigurable* ic = static_cast<IConfigurable*>(r.ptr);
    JsonObject sub = top.createNestedObject(ic->configKey());
    ic->addToConfig(sub);
    sub["Delete"] = false;
  }
  {
    JsonObject nv = top.createNestedObject("NewView");
    nv["SegmentId"] = -1;
    nv["AgencyStopCodes"] = ""; // one or more, separated by comma/space; tokens may be AGENCY:StopCode or StopCode
  }
  // Emit global color map last so it appears at the bottom of the page
  JsonObject cmap = top.createNestedObject("ColorMap");
  // Sort entries by (agency, numeric route, suffix) for stable and natural display
  std::vector<std::pair<String,uint32_t>> entries;
  entries.reserve(DepartModel::colorMap().size());
  for (const auto& ce : DepartModel::colorMap()) entries.emplace_back(ce.key, ce.rgb);
  struct Cmp {
    static void splitKey(const String& key, String& agency, String& line) {
      int c = key.indexOf(':');
      if (c > 0) { agency = key.substring(0, c); line = key.substring(c+1); }
      else { agency = key; line = String(); }
    }
    static bool parseLeadingInt(const String& s, int& val, int& consumed) {
      val = 0; consumed = 0; bool any = false;
      for (unsigned i = 0; i < s.length(); ++i) {
        char ch = s.charAt(i);
        if (ch >= '0' && ch <= '9') { any = true; val = val*10 + (ch - '0'); ++consumed; }
        else break;
      }
      return any;
    }
    static int cmpLine(const String& a, const String& b) {
      int na=0, nb=0, ca=0, cb=0;
      bool ha = parseLeadingInt(a, na, ca);
      bool hb = parseLeadingInt(b, nb, cb);
      if (ha && hb) {
        if (na != nb) return (na < nb) ? -1 : 1;
        String sa = a.substring(ca);
        String sb = b.substring(cb);
        int r = sa.compareTo(sb);
        if (r != 0) return r < 0 ? -1 : 1;
        return 0;
      }
      if (ha != hb) return ha ? -1 : 1; // numeric routes before non-numeric
      int r = a.compareTo(b);
      if (r != 0) return r < 0 ? -1 : 1;
      return 0;
    }
    static bool lt(const std::pair<String,uint32_t>& a, const std::pair<String,uint32_t>& b) {
      String aa, al, ba, bl; splitKey(a.first, aa, al); splitKey(b.first, ba, bl);
      int ar = aa.compareTo(ba);
      if (ar != 0) return ar < 0;
      return cmpLine(al, bl) < 0;
    }
  };
  std::sort(entries.begin(), entries.end(), Cmp::lt);
  for (const auto& kv : entries) {
    cmap[kv.first] = DepartModel::colorToString(kv.second);
  }
  top["ColorMapReset"] = false;      // user can set true to clear map on next read

}

void DepartStrip::appendConfigData(Print& s) {
  for (auto& src : sources_) src->appendConfigData(s);
  for (auto& vw : views_) vw->appendConfigData(s, model_.get());

  // Per-source Stop name and current-route swatches (anchor below Delete)
  for (auto& src : sources_) {
    SiriSource* ss = static_cast<SiriSource*>(src.get());
    if (!ss) continue;
    String section = String(src->configKey());
    String anchor = String(F("DepartStrip:")) + section + F(":Delete");

    // Collect current lines seen for this stop from the model
    String agency = ss->agency();
    std::vector<String> lines;
    if (model_) model_->currentLinesForBoard(ss->sourceKey(), lines);
    std::vector<std::pair<String,uint32_t>> entries;
    entries.reserve(lines.size());
    for (const auto& line : lines) {
      uint32_t rgb = 0x606060;
      if (agency.length() > 0) DepartModel::getColorRGB(agency, line, rgb);
      String fullKey = agency; fullKey += ":"; fullKey += line;
      entries.emplace_back(fullKey, rgb);
    }

    // Build HTML suffix placed AFTER the field (3rd arg)
    s.print(F("addInfo('")); s.print(anchor); s.print(F("',1,'"));
    s.print(F("<div style=\\'margin-top:8px;text-align:center;\\'>"));
    // Stop label (optional)
    if (ss->stopName().length()) {
      String nm = ss->stopName();
      // Escape for HTML and JS single-quoted string context
      nm.replace("<","&lt;"); nm.replace(">","&gt;");
      nm.replace("\\","\\\\"); nm.replace("'","\\'");
      s.print(F("<div style=\\'margin-bottom:4px;\\'><b>Stop:</b> ")); s.print(nm); s.print(F("</div>"));
    }
    if (!entries.empty()) {
      s.print(F("<div style=\\'font-weight:600;margin-bottom:4px;\\'>Routes</div>"));
      s.print(F("<div style=\\'display:flex;flex-wrap:wrap;gap:8px;justify-content:center;\\'>"));
      for (const auto& kv : entries) {
        String col = DepartModel::colorToString(kv.second);
        s.print(F("<span style=\\'display:inline-flex;align-items:center;border:1px solid #888;padding:2px 6px;border-radius:4px;\\'>"));
        s.print(F("<span style=\\'display:inline-block;width:14px;height:14px;margin-right:6px;background:"));
        s.print(col);
        s.print(F(";\\'></span><code>"));
        int cpos = kv.first.indexOf(':');
        if (cpos > 0) s.print(kv.first.substring(cpos+1)); else s.print(kv.first);
        s.print(F("</code></span>"));
      }
      s.print(F("</div>"));
    }
    s.print(F("</div>"));
    s.println(F("','');"));
  }
}

bool DepartStrip::readFromConfig(JsonObject& root) {
  JsonObject top = root[FPSTR(CFG_NAME)];
  if (top.isNull()) return true;

  bool ok = true;
  bool invalidate_history = false;
  bool startup_complete = false; // avoid fetch/reload during config save

  ok &= getJsonValue(top[FPSTR(CFG_ENABLED)], enabled_, false);

  // Handle color map reset (no versioning)
  bool doReset = top["ColorMapReset"] | false;
  if (doReset) {
    DepartModel::clearColorMap();
    skipColorMapLoad_ = true; // avoid re-adding from old config in this pass
  } else {
    skipColorMapLoad_ = false;
  }

  // Load optional color mapping overrides
  if (!skipColorMapLoad_ && top["ColorMap"].is<JsonObject>()) {
    JsonObject cmap = top["ColorMap"].as<JsonObject>();
    for (JsonPair kv : cmap) {
      const char* k = kv.key().c_str();
      JsonVariant vj = kv.value();
      String v = vj.as<String>();
      if (!k || !*k) continue;
      // Expect key format "AGENCY:LineRef"
      String keyStr(k);
      int colon = keyStr.indexOf(':');
      if (colon <= 0) continue;
      String agency = keyStr.substring(0, colon);
      String line   = keyStr.substring(colon+1);
      // Deletion semantics: empty, "-", or explicit null removes entry
      if (vj.isNull() || v.length() == 0 || v == F("-") || v.equalsIgnoreCase(F("delete"))) {
        DepartModel::removeColorKey(agency, line);
        continue;
      }
      uint32_t rgb;
      if (DepartModel::parseColorString(v, rgb)) {
        DepartModel::setColorRGB(agency, line, rgb);
      }
    }
  }

  // If starting with no in-memory sources/views (e.g., fresh boot),
  // rebuild them from any persisted sections under DepartStrip.
  if (sources_.empty() && views_.empty()) {
    for (JsonPair kv : top) {
      const char* secName = kv.key().c_str();
      if (!kv.value().is<JsonObject>() || !secName || !*secName) continue;
      String sName(secName);
      if (sName == F("NewSource") || sName == F("NewView") || sName == F("ColorMap")) continue;
      JsonObject obj = kv.value().as<JsonObject>();
      bool hasASC = obj.containsKey("AgencyStopCodes") || obj.containsKey("AgencyStopCode");
      bool hasKey = obj.containsKey("Key");
      bool isView = obj.containsKey("SegmentId") && (hasASC || hasKey);
      bool isSource = (hasASC || hasKey) && !isView &&
                      (obj.containsKey("UpdateSecs") || obj.containsKey("TemplateUrl") || obj.containsKey("BaseUrl") || obj.containsKey("ApiKey") ||
                       obj.containsKey("Agency") || obj.containsKey("StopCode"));
      if (isSource) {
        auto snew = ::make_unique<SiriSource>(secName, nullptr, nullptr, nullptr);
        bool dummy = false;
        ok &= snew->readFromConfig(obj, startup_complete, dummy);
        sources_.push_back(std::move(snew));
      } else if (isView) {
        String vkey = obj["AgencyStopCodes"] | obj["AgencyStopCode"] | obj["Key"] | "";
        if (vkey.length() == 0) continue;
        auto vnew = ::make_unique<DepartureView>(vkey);
        bool dummy = false;
        ok &= vnew->readFromConfig(obj, startup_complete, dummy);
        views_.push_back(std::move(vnew));
      }
    }
  }

  // Handle deletion of existing sources
  if (!sources_.empty()) {
    std::vector<size_t> delIdx;
    for (size_t i = 0; i < sources_.size(); ++i) {
      auto& src = sources_[i];
      JsonObject sub = top[src->configKey()];
      if (!sub.isNull() && (bool)(sub["Delete"] | false)) delIdx.push_back(i);
    }
    for (size_t k = 0; k < delIdx.size(); ++k) {
      size_t idx = delIdx[delIdx.size()-1-k];
      sources_.erase(sources_.begin() + idx);
    }
  }
  // Deduplicate sources by Key: keep first occurrence
  if (!sources_.empty()) {
    std::vector<size_t> delIdx;
    std::vector<String> seen;
    for (size_t i = 0; i < sources_.size(); ++i) {
      SiriSource* ss = static_cast<SiriSource*>(sources_[i].get());
      String k = ss ? ss->sourceKey() : String();
      bool dup = false;
      for (auto& sk : seen) if (sk == k) { dup = true; break; }
      if (dup) delIdx.push_back(i); else seen.push_back(k);
    }
    for (size_t k = 0; k < delIdx.size(); ++k) {
      size_t idx = delIdx[delIdx.size()-1-k];
      sources_.erase(sources_.begin() + idx);
    }
  }
  // Handle deletion of existing views
  if (!views_.empty()) {
    std::vector<size_t> delIdx;
    for (size_t i = 0; i < views_.size(); ++i) {
      auto& vw = views_[i];
      JsonObject sub = top[vw->configKey()];
      if (!sub.isNull() && (bool)(sub["Delete"] | false)) delIdx.push_back(i);
    }
    for (size_t k = 0; k < delIdx.size(); ++k) {
      size_t idx = delIdx[delIdx.size()-1-k];
      views_.erase(views_.begin() + idx);
    }
  }

  // Read remaining sources and views
  for (auto& src : sources_) {
    JsonObject sub = top[src->configKey()];
    ok &= src->readFromConfig(sub, startup_complete, invalidate_history);
  }
  for (auto& vw : views_) {
    JsonObject sub = top[vw->configKey()];
    ok &= vw->readFromConfig(sub, startup_complete, invalidate_history);
  }

  // Add new source if requested
  if (top["NewSource"].is<JsonObject>()) {
    JsonObject ns = top["NewSource"].as<JsonObject>();
    String nsKey = ns["AgencyStopCode"] | ns["Key"] | "";
    nsKey.trim();
    if (nsKey.length() > 0) {
      // avoid duplicate by Key
      bool existsKey = false;
      for (auto& s : sources_) {
        SiriSource* ss = static_cast<SiriSource*>(s.get());
        if (ss && ss->sourceKey() == nsKey) { existsKey = true; break; }
      }
      if (!existsKey) {
        // Generate unique configKey like "siri_sourceN"
        int next = 1;
        auto exists = [&](const char* k){ String kk(k); for (auto& s : sources_) if (String(s->configKey()) == kk) return true; return false; };
        String cfg;
        do { cfg = String(F("siri_source")); cfg += next++; } while (exists(cfg.c_str()));
        auto snew = ::make_unique<SiriSource>(cfg.c_str(), nullptr, nullptr, nullptr);
        bool dummy = false;
        ok &= snew->readFromConfig(ns, startup_complete, dummy);
        sources_.push_back(std::move(snew));
      } else {
        DEBUG_PRINTF("DepartStrip: NewSource ignored, duplicate Key %s\n", nsKey.c_str());
      }
    }
  }
  // Add new view if requested
  if (top["NewView"].is<JsonObject>()) {
    JsonObject nv = top["NewView"].as<JsonObject>();
    String key = nv["AgencyStopCodes"] | nv["AgencyStopCode"] | nv["Key"] | "";
    key.trim();
    if (key.length() > 0) {
      auto vnew = ::make_unique<DepartureView>(key);
      bool dummy = false;
      ok &= vnew->readFromConfig(nv, startup_complete, dummy);
      views_.push_back(std::move(vnew));
    }
  }

  if (invalidate_history) {
    model_->boards.clear();
    if (startup_complete) reloadSources(departstrip::util::time_now_utc());
  }

  // Always report success to persist changes
  return true;
}

void DepartStrip::showBooting() {
  Segment& seg = strip.getMainSegment();
  seg.setMode(28);
  seg.speed = 200;
  seg.setPalette(128);
  seg.setColor(0, 0x404060);
  seg.setColor(1, 0x000000);
  seg.setColor(2, 0x303040);
}

void DepartStrip::doneBooting() {
  Segment& seg = strip.getMainSegment();
  seg.setMode(0);
}

void DepartStrip::reloadSources(std::time_t now) {
  for (auto& src : sources_) src->reload(now);
}
