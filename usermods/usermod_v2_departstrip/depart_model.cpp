#include "depart_model.h"
#include <algorithm>
#include "util.h"

void DepartModel::update(std::time_t now, DepartModel&& delta) {
  for (auto &e : delta.boards) {
    auto it = std::find_if(boards.begin(), boards.end(), [&](const Entry& x){ return x.key == e.key; });
    if (it != boards.end()) {
      it->batch = std::move(e.batch);
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
  // Natural sort: numeric prefix, then suffix (shared helper)
  std::sort(out.begin(), out.end(), [](const String& a, const String& b){
    return departstrip::util::cmpLineRefNatural(a, b) < 0;
  });
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

  struct DefaultColorEntry {
    const char* agency;
    const char* line;
    uint32_t color;
    bool stripSuffixAfterDash;
  };

  // Unified default palette grouped by agency.
  const DefaultColorEntry g_defaultColors[] = {
    // BART core palette (match base name, ignore direction suffix)
    {"BA", "RED",    0xFF0000, true},
    {"BA", "ORANGE", 0xFF8000, true},
    {"BA", "YELLOW", 0xFFFF00, true},
    {"BA", "GREEN",  0x00FF00, true},
    {"BA", "BLUE",   0x0000FF, true},
    {"BA", "WHITE",  0xFFFFFF, true},

    // AC Transit routes (exact matches)
    {"AC", "6",    0x003680, false},
    {"AC", "12",   0x496F80, false},
    {"AC", "18",   0x800000, false},
    {"AC", "27",   0x00807F, false},
    {"AC", "51A",  0x806000, false},
    {"AC", "72",   0x00804D, false},
    {"AC", "72M",  0x630080, false},
    {"AC", "88",   0x808040, false},
    {"AC", "633",  0x808080, false},
    {"AC", "651",  0x808080, false},
    {"AC", "800",  0x4C8032, false},
    {"AC", "802",  0x804F59, false},
    {"AC", "805",  0x800058, false},
    {"AC", "851",  0x496F80, false},

    // Metro-North Railroad core line
    {"MNR", "1",         0x009B3A, false}, // Hudson Line brand green

    // MTA Express buses (Bronx) — warm colors
    {"MTA", "BC_BXM1",   0xD4A628, false}, // saturated golden yellow
    {"MTA", "BC_BXM2",   0xA14444, false}, // warm red
    {"MTA", "BC_BXM18",  0x6B4FB7, false}, // richer purple to stand out from buses

    // MTA Local buses (Bronx) — cool colors
    {"MTA", "NYCT_BX10", 0x4E80B8, false}, // blue
    {"MTA", "NYCT_BX20", 0x1010AE, false}, // deep cobalt-blue; customize suffix variants separately

    // Metra UP-N line (Chicago) — core plus DepartStrip variants
    {"UPN", "UP-N-Local",   0xF2D34C, false}, // vibrant yellow for Central->Main locals
    {"UPN", "UP-N-Express", 0x5A3BCB, false}, // richer Northwestern purple for Central->OTC express
    {"UPN", "UP-N-Davis",   0x1FA57A, false}, // more saturated UP-N green for Wilmette->OTC via Davis
  };

  String lineTokenForDefault(const String& agency, const String& lineRef) {
    String ag = agency;
    ag.trim();
    String line = lineRef;
    line.trim();
    if (line.length() == 0) return line;
    if (ag.length() == 0) return line;

    String agLower = ag; agLower.toLowerCase();
    String lineLower = line; lineLower.toLowerCase();
    if (lineLower.startsWith(agLower)) {
      unsigned pos = ag.length();
      while (pos < line.length()) {
        char c = line.charAt(pos);
        if (c == ' ' || c == '-' || c == '_' || c == ':') { ++pos; continue; }
        break;
      }
      line = line.substring(pos);
      line.trim();
    }
    return line;
  }

  bool lookupDefaultColor(const String& agency, const String& lineRef, uint32_t& rgbOut) {
    String ag = agency;
    ag.trim();
    String line = lineTokenForDefault(ag, lineRef);

    String agUpper = ag;
    agUpper.toUpperCase();
    String lineUpper = line;
    lineUpper.toUpperCase();

    for (const auto& entry : g_defaultColors) {
      if (agUpper != entry.agency) continue;
      String candidate = lineUpper;
      if (entry.stripSuffixAfterDash) {
        int dash = candidate.indexOf('-');
        if (dash > 0) candidate = candidate.substring(0, dash);
      }
      String entryLine(entry.line);
      entryLine.toUpperCase();
      if (candidate == entryLine) {
        rgbOut = entry.color;
        return true;
      }
    }
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
  uint32_t looked = 0;
  if (lookupDefaultColor(agency, lineRef, looked)) def = looked;
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
