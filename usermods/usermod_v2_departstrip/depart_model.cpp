#include "depart_model.h"
#include <algorithm>
#include "util.h"

void DepartModel::update(std::time_t now, DepartModel&& delta) {
  for (auto &e : delta.boards) {
    auto it = std::find_if(boards.begin(), boards.end(), [&](const Entry& x){ return x.key == e.key; });
    if (it != boards.end()) {
      it->batch = e.batch;
      DEBUG_PRINTF("DepartStrip: DepartModel::update: key %s: items=%u\n",
                   it->key.c_str(), (unsigned)it->batch.items.size());
      String dbg = describeBatch(it->batch);
      if (dbg.length()) DEBUG_PRINTF("DepartStrip: DepartModel::update: model %s\n", dbg.c_str());
    } else {
      boards.push_back(std::move(e));
      auto &ne = boards.back();
      DEBUG_PRINTF("DepartStrip: DepartModel::update: added key %s\n", ne.key.c_str());
      DEBUG_PRINTF("DepartStrip: DepartModel::update: key %s: items=%u\n",
                   ne.key.c_str(), (unsigned)ne.batch.items.size());
      String dbg = describeBatch(ne.batch);
      if (dbg.length()) DEBUG_PRINTF("DepartStrip: DepartModel::update: model %s\n", dbg.c_str());
    }
  }
}

const DepartModel::Entry* DepartModel::find(const String& key) const {
  for (auto const& e : boards) if (e.key == key) return &e;
  return nullptr;
}

void DepartModel::currentLinesForBoard(const String& key, std::vector<String>& out) const {
  out.clear();
  const Entry* e = find(key);
  if (!e) return;
  const auto& items = e->batch.items;
  // Deduplicate lines
  for (const auto& it : items) {
    const String& lr = it.lineRef;
    if (lr.length() == 0) continue;
    bool seen = false;
    for (const auto& s : out) { if (s == lr) { seen = true; break; } }
    if (!seen) out.push_back(lr);
  }
  // Natural sort: numeric prefix, then suffix
  struct CmpLine {
    static bool parseLeadingInt(const String& s, int& val, int& consumed) {
      val=0; consumed=0; bool any=false; for (unsigned i=0;i<s.length();++i){ char ch=s.charAt(i); if (ch>='0'&&ch<='9'){ any=true; val=val*10+(ch-'0'); ++consumed; } else break; } return any;
    }
    static bool lt(const String& a, const String& b) {
      int na=0, nb=0, ca=0, cb=0; bool ha=parseLeadingInt(a,na,ca), hb=parseLeadingInt(b,nb,cb);
      if (ha&&hb){ if(na!=nb) return na<nb; int r=a.substring(ca).compareTo(b.substring(cb)); if(r!=0) return r<0; return false; }
      if (ha!=hb) return ha; // numeric first
      return a.compareTo(b) < 0;
    }
  };
  std::sort(out.begin(), out.end(), CmpLine::lt);
}

String DepartModel::describeBatch(const DepartModel::Entry::Batch& batch) {
  if (batch.items.empty()) return String();
  char nowBuf[20];
  departstrip::util::fmt_local(nowBuf, sizeof(nowBuf), batch.ourTs, "%H:%M:%S");
  int lagSecs = (int)(batch.ourTs - batch.apiTs);
  String out;
  out += nowBuf;
  out += ": lag ";
  out += lagSecs;
  out += ":";
  time_t prevTs = batch.ourTs;
  for (const auto& e : batch.items) {
    out += " +";
    int diffMin = (int)((long)e.estDep - (long)prevTs) / 60;
    out += diffMin;
    out += " (";
    prevTs = e.estDep;
    char depBuf[20];
    departstrip::util::fmt_local(depBuf, sizeof(depBuf), e.estDep, "%H:%M:%S");
    out += depBuf;
    out += ":";
    out += e.lineRef;
    out += ")";
  }
  return out;
}

// ----- Color mapping storage -----
namespace {
  std::vector<DepartModel::ColorEntry> g_colors; // key = "AGENCY:LineRef"

  String makeKey(const String& agency, const String& lineRef) {
    String k = agency; k += ":"; k += lineRef; return k;
  }

  bool bartDefaultColor(const String& lineRef, uint32_t& rgbOut) {
    // Accept Red/Orange/Yellow/Green/Blue with optional -N/-S suffix
    int dash = lineRef.indexOf('-');
    String base = (dash > 0) ? lineRef.substring(0, dash) : lineRef;
    String u = base; u.toUpperCase();
    if (u == F("RED"))    { rgbOut = 0xFF0000; return true; }
    if (u == F("ORANGE")) { rgbOut = 0xFF961E; return true; }
    if (u == F("YELLOW")) { rgbOut = 0xFFFF00; return true; }
    if (u == F("GREEN"))  { rgbOut = 0x00FF00; return true; }
    if (u == F("BLUE"))   { rgbOut = 0x0000FF; return true; }
    if (u == F("WHITE"))  { rgbOut = 0xFFFFFF; return true; }
    return false;
  }

  bool acDefaultColor(const String& lineRef, uint32_t& rgbOut) {
    // Map common AC Transit routes to provided colors
    int dash = lineRef.indexOf('-');
    String base = (dash > 0) ? lineRef.substring(0, dash) : lineRef;
    String u = base; u.toUpperCase();
    if (u == F("6"))    { rgbOut = 0x003680; return true; }
    if (u == F("12"))   { rgbOut = 0x496F80; return true; }
    if (u == F("18"))   { rgbOut = 0x800000; return true; }
    if (u == F("27"))   { rgbOut = 0x00807F; return true; }
    if (u == F("51A"))  { rgbOut = 0x806000; return true; }
    if (u == F("72"))   { rgbOut = 0x00804D; return true; }
    if (u == F("72M"))  { rgbOut = 0x630080; return true; }
    if (u == F("88"))   { rgbOut = 0x808040; return true; }
    if (u == F("633"))  { rgbOut = 0x808080; return true; }
    if (u == F("651"))  { rgbOut = 0x808080; return true; }
    if (u == F("800"))  { rgbOut = 0x4C8032; return true; }
    if (u == F("802"))  { rgbOut = 0x804F59; return true; }
    if (u == F("805"))  { rgbOut = 0x800058; return true; }
    if (u == F("851"))  { rgbOut = 0x496F80; return true; }
    return false;
  }
}

const std::vector<DepartModel::ColorEntry>& DepartModel::colorMap() {
  return g_colors;
}

void DepartModel::clearColorMap() {
  g_colors.clear();
}

bool DepartModel::removeColorKeyByKey(const String& key) {
  for (auto it = g_colors.begin(); it != g_colors.end(); ++it) {
    if (it->key == key) { g_colors.erase(it); return true; }
  }
  return false;
}

bool DepartModel::removeColorKey(const String& agency, const String& lineRef) {
  String key = makeKey(agency, lineRef);
  return removeColorKeyByKey(key);
}

void DepartModel::setColorRGB(const String& agency, const String& lineRef, uint32_t rgb) {
  String key = makeKey(agency, lineRef);
  for (auto &e : g_colors) {
    if (e.key == key) { e.rgb = rgb; return; }
  }
  g_colors.push_back(ColorEntry{key, rgb});
}

static bool lookupKey(const String& key, uint32_t& rgbOut) {
  for (const auto& e : g_colors) if (e.key == key) { rgbOut = e.rgb; return true; }
  return false;
}

bool DepartModel::getColorRGB(const String& agency, const String& lineRef, uint32_t& rgbOut) {
  // Exact match
  String key = makeKey(agency, lineRef);
  if (lookupKey(key, rgbOut)) return true;
  // Fallback: strip direction suffix after '-'
  int dash = lineRef.indexOf('-');
  if (dash > 0) {
    String base = lineRef.substring(0, dash);
    key = makeKey(agency, base);
    if (lookupKey(key, rgbOut)) return true;
  }
  // Unknown: choose default and persist entry
  uint32_t def = 0x606060; // neutral gray
  if (agency == F("BA")) {
    uint32_t bart;
    if (bartDefaultColor(lineRef, bart)) def = bart;
  } else if (agency == F("AC")) {
    uint32_t ac;
    if (acDefaultColor(lineRef, ac)) def = ac;
  }
  setColorRGB(agency, lineRef, def);
  rgbOut = def;
  return true;
}

bool DepartModel::parseColorString(const String& s, uint32_t& rgbOut) {
  if (s.length() >= 7 && s[0] == '#') {
    // #RRGGBB
    char buf[3] = {0,0,0};
    long r=0,g=0,b=0;
    buf[0]=s[1]; buf[1]=s[2]; r = strtol(buf,nullptr,16);
    buf[0]=s[3]; buf[1]=s[4]; g = strtol(buf,nullptr,16);
    buf[0]=s[5]; buf[1]=s[6]; b = strtol(buf,nullptr,16);
    rgbOut = ((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
    return true;
  }
  if (s.startsWith("rgb:")) {
    int r=0,g=0,b=0;
    if (sscanf(s.c_str()+4, "%d,%d,%d", &r,&g,&b) == 3) {
      r = std::max(0, std::min(255, r));
      g = std::max(0, std::min(255, g));
      b = std::max(0, std::min(255, b));
      rgbOut = ((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
      return true;
    }
  }
  // Future: hsv:... format
  return false;
}

String DepartModel::colorToString(uint32_t rgb) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%06X", (unsigned)rgb & 0xFFFFFF);
  String s("#"); s += buf; return s;
}
