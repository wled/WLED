#include "cta_source.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <utility>

namespace {
// Days from civil algorithm copied from SiriSource; converts Y/M/D to days since 1970-01-01.
static int days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + yoe / 400 + doy;
  return era * 146097 + (int)doe - 719468;
}

static void trimString(String& s) {
  int start = 0;
  int end = s.length();
  while (start < end) {
    char c = s.charAt(start);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') ++start;
    else break;
  }
  while (end > start) {
    char c = s.charAt(end - 1);
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') --end;
    else break;
  }
  if (start == 0 && end == s.length()) return;
  s = s.substring(start, end);
}
} // namespace

CtaTrainTrackerSource::CtaTrainTrackerSource(const char* key) : configKey_(key ? key : "cta_source") {
  baseUrl_ = F("https://lapi.transitchicago.com/api/1.0/ttarrivals.aspx?mapid={mapid}&max=10&key={apiKey}");
}

void CtaTrainTrackerSource::reload(std::time_t now) {
  nextFetch_ = now;
  backoffMult_ = 1;
  lastBackoffLog_ = 0;
}

String CtaTrainTrackerSource::sourceKey() const {
  String key;
  key.reserve(agency_.length() + 1 + stopKey_.length());
  key = agency_;
  key += ':';
  key += stopKey_;
  return key;
}

void CtaTrainTrackerSource::addToConfig(JsonObject& root) {
  root["Enabled"] = enabled_;
  root["Type"] = F("cta");
  root["UpdateSecs"] = updateSecs_;
  root["TemplateUrl"] = baseUrl_;
  root["ApiKey"] = apiKey_;
  String combined;
  combined.reserve(agency_.length() + 1 + stopKey_.length());
  combined = agency_;
  combined += ':';
  combined += stopKey_;
  root["AgencyStopCode"] = combined;
}

bool CtaTrainTrackerSource::readFromConfig(JsonObject& root, bool startup_complete, bool& invalidate_history) {
  bool ok = true;
  bool prevEnabled = enabled_;
  String prevAgency = agency_;
  String prevStops = stopKey_;
  String prevBase = baseUrl_;
  std::vector<StopQuery> prevQueries = queries_;

  ok &= getJsonValue(root["Enabled"], enabled_, enabled_);
  ok &= getJsonValue(root["UpdateSecs"], updateSecs_, updateSecs_);
  ok &= getJsonValue(root["TemplateUrl"], baseUrl_, baseUrl_);
  ok &= getJsonValue(root["ApiKey"], apiKey_, apiKey_);

  String keyField;
  bool haveKey = getJsonValue(root["AgencyStopCode"], keyField, (const char*)nullptr);
  if (!haveKey) haveKey = getJsonValue(root["Key"], keyField, (const char*)nullptr);

  String stopList;
  if (haveKey && keyField.length() > 0) {
    keyField.trim();
    int colon = keyField.indexOf(':');
    if (colon > 0) {
      agency_ = keyField.substring(0, colon);
      stopList = keyField.substring(colon + 1);
    } else {
      stopList = keyField;
    }
  } else {
    ok &= getJsonValue(root["Agency"], agency_, agency_);
    String stopField;
    if (getJsonValue(root["StopCode"], stopField, (const char*)nullptr)) {
      stopList = stopField;
    } else {
      stopList = stopKey_;
    }
  }
  stopList.trim();

  std::vector<String> tokens;
  if (stopList.length() > 0) splitStopCodes(stopList, tokens);

  queries_.clear();
  String inferredAgency = agency_;
  for (auto& tok : tokens) {
    StopQuery parsed;
    String explicitAgency;
    if (!parseStopToken(tok, parsed, explicitAgency)) continue;
    if (explicitAgency.length() > 0) inferredAgency = explicitAgency;
    bool dup = false;
    for (const auto& existing : queries_) {
      if (existing.canonical.equalsIgnoreCase(parsed.canonical)) {
        dup = true;
        break;
      }
    }
    if (!dup) queries_.push_back(std::move(parsed));
  }

  if (agency_.length() == 0 && inferredAgency.length() > 0) agency_ = inferredAgency;
  if (agency_.length() == 0) agency_ = F("CTA");

  if (queries_.empty()) {
    if (!prevQueries.empty()) {
      queries_ = prevQueries;
    } else if (stopList.length() > 0) {
      StopQuery fallback;
      String dummy;
      if (parseStopToken(stopList, fallback, dummy)) {
        queries_.push_back(fallback);
      }
    }
  }

  stopKey_.clear();
  for (size_t i = 0; i < queries_.size(); ++i) {
    if (i > 0) stopKey_ += ',';
    stopKey_ += queries_[i].canonical;
  }

  {
    String sec = F("CtaSource_");
    sec += agency_;
    sec += '_';
    sec += stopKey_;
    configKey_ = std::string(sec.c_str());
  }

  if (updateSecs_ < 15) updateSecs_ = 15;

  invalidate_history |= (agency_ != prevAgency) || (stopKey_ != prevStops) || (baseUrl_ != prevBase);
  if (startup_complete && !prevEnabled && enabled_) reload(departstrip::util::time_now_utc());
  return ok;
}

std::unique_ptr<DepartModel> CtaTrainTrackerSource::fetch(std::time_t now) {
  if (!enabled_) return nullptr;
  if (queries_.empty()) return nullptr;
  if (now == 0) return nullptr;
  if (nextFetch_ != 0 && now < nextFetch_) {
    time_t interval = updateSecs_ > 0 ? (time_t)updateSecs_ : (time_t)60;
    if (lastBackoffLog_ == 0 || now - lastBackoffLog_ >= interval) {
      lastBackoffLog_ = now;
      long rem = (long)(nextFetch_ - now);
      if (rem < 0) rem = 0;
      String key = sourceKey();
      if (backoffMult_ > 1) {
        DEBUG_PRINTF("DepartStrip: CTA fetch: backoff x%u %s, next in %lds\n",
                     (unsigned)backoffMult_, key.c_str(), rem);
      } else {
        DEBUG_PRINTF("DepartStrip: CTA fetch: waiting %s, next in %lds\n",
                     key.c_str(), rem);
      }
    }
    return nullptr;
  }

  std::unique_ptr<DepartModel> model(new DepartModel());
  model->boards.reserve(queries_.size());
  size_t totalItems = 0;

  DEBUG_PRINTLN(F("DepartStrip: CTA fetch: begin"));

  for (const auto& query : queries_) {
    String dirLog = (query.direction > 0) ? String(query.direction) : String(F("-"));
    DEBUG_PRINTF("DepartStrip: CTA fetch: requesting mapId=%s dir=%s key=%s\n",
                 query.mapIdStr.c_str(),
                 dirLog.c_str(),
                 configKey_.c_str());
    String url = composeUrl(query);
    HTTPClient http;
    bool usedSecure = false;
    int lenHint = -1;
    int httpStatus = 0;
    if (!httpBegin(url, lenHint, httpStatus, http, usedSecure)) {
      long delay = (long)updateSecs_ * (long)backoffMult_;
      if (now - lastBackoffLog_ >= 10) {
        DEBUG_PRINTF("DepartStrip: CTA fetch HTTP error key=%s status=%d scheduling backoff x%u for %lds\n",
                     sourceKey().c_str(),
                     httpStatus,
                     (unsigned)backoffMult_,
                     delay);
        lastBackoffLog_ = now;
      }
      nextFetch_ = now + delay;
      if (backoffMult_ < 16) backoffMult_ *= 2;
      return nullptr;
    }

    String body;
    bool gotBody = readHttpBody(http, lenHint, body);
    closeHttp(http, usedSecure);
    if (!gotBody) {
      long delay = (long)updateSecs_ * (long)backoffMult_;
      if (now - lastBackoffLog_ >= 10) {
        DEBUG_PRINTF("DepartStrip: CTA fetch empty body key=%s scheduling backoff x%u for %lds\n",
                     sourceKey().c_str(),
                     (unsigned)backoffMult_,
                     delay);
        lastBackoffLog_ = now;
      }
      nextFetch_ = now + delay;
      if (backoffMult_ < 16) backoffMult_ *= 2;
      return nullptr;
    }

    DepartModel::Entry::Batch batch;
    batch.ourTs = now;
    if (!parseResponse(body, query, now, batch)) {
      long delay = (long)updateSecs_ * (long)backoffMult_;
      if (now - lastBackoffLog_ >= 10) {
        DEBUG_PRINTF("DepartStrip: CTA fetch parse error key=%s scheduling backoff x%u for %lds\n",
                     sourceKey().c_str(),
                     (unsigned)backoffMult_,
                     delay);
        lastBackoffLog_ = now;
      }
      nextFetch_ = now + delay;
      if (backoffMult_ < 16) backoffMult_ *= 2;
      return nullptr;
    }

    std::sort(batch.items.begin(), batch.items.end(),
              [](const DepartModel::Entry::Item& a, const DepartModel::Entry::Item& b) {
                if (a.estDep != b.estDep) return a.estDep < b.estDep;
                return a.lineRef.compareTo(b.lineRef) < 0;
              });
    batch.items.erase(std::unique(batch.items.begin(), batch.items.end(),
                    [](const DepartModel::Entry::Item& a, const DepartModel::Entry::Item& b) {
                      return a.estDep == b.estDep && a.lineRef == b.lineRef;
                    }),
                      batch.items.end());
    const size_t MAX_ITEMS = 16;
    if (batch.items.size() > MAX_ITEMS) batch.items.resize(MAX_ITEMS);

    if (batch.apiTs == 0) batch.apiTs = now;

    DepartModel::Entry board;
    board.key.reserve(agency_.length() + 1 + query.canonical.length());
    board.key = agency_;
    board.key += ':';
    board.key += query.canonical;
    board.batch = std::move(batch);
    totalItems += board.batch.items.size();
    DEBUG_PRINTF("DepartStrip: CTA fetch: key %s: items=%u\n",
                 board.key.c_str(),
                 (unsigned)board.batch.items.size());
    String dbg = DepartModel::describeBatch(board.batch);
    if (dbg.length()) DEBUG_PRINTF("DepartStrip: CTA fetch: model %s\n", dbg.c_str());
    model->boards.push_back(std::move(board));
  }

  nextFetch_ = now + updateSecs_;
  backoffMult_ = 1;
  lastBackoffLog_ = 0;

  if (model->boards.empty() || totalItems == 0) {
    DEBUG_PRINTF("DepartStrip: CTA fetch: no departures parsed key=%s\n", sourceKey().c_str());
  }

  return model;
}

String CtaTrainTrackerSource::composeUrl(const StopQuery& query) const {
  String url = baseUrl_;
  if (url.length() == 0) {
    url = F("http://lapi.transitchicago.com/api/1.0/ttarrivals.aspx?mapid={mapid}&max=50");
    if (apiKey_.length() > 0) {
      url += F("&key={apiKey}");
    }
  }
  auto replaceAll = [&](const __FlashStringHelper* needle, const String& value) {
    if (!needle) return;
    String key(needle);
    if (!key.length()) return;
    url.replace(key, value);
  };

  replaceAll(F("{agency}"), agency_);
  replaceAll(F("{AGENCY}"), agency_);
  replaceAll(F("{Agency}"), agency_);

  replaceAll(F("{mapid}"), query.mapIdStr);
  replaceAll(F("{mapId}"), query.mapIdStr);
  replaceAll(F("{MAPID}"), query.mapIdStr);

  String dirStr;
  if (query.direction > 0) dirStr = String(query.direction);
  replaceAll(F("{direction}"), dirStr);
  replaceAll(F("{Direction}"), dirStr);
  replaceAll(F("{DIRECTION}"), dirStr);
  replaceAll(F("{trdr}"), dirStr);
  replaceAll(F("{trDr}"), dirStr);
  replaceAll(F("{TRDR}"), dirStr);

  if (apiKey_.length() > 0) {
    replaceAll(F("{apikey}"), apiKey_);
    replaceAll(F("{apiKey}"), apiKey_);
    replaceAll(F("{APIKEY}"), apiKey_);
  }
  return url;
}

bool CtaTrainTrackerSource::httpBegin(const String& url, int& outLen, int& outStatus, HTTPClient& http, bool& usedSecure) {
  http.setTimeout(10000);
  bool localSecure = false;
  WiFiClient* client = httpTransport_.begin(url, 10000, localSecure);
  if (!client) {
    DEBUG_PRINTLN(F("DepartStrip: CTA: no HTTP client available"));
    return false;
  }

  if (!http.begin(*client, url)) {
    http.end();
    httpTransport_.end(localSecure);
    DEBUG_PRINTLN(F("DepartStrip: CTA: HTTP begin failed"));
    return false;
  }
  usedSecure = localSecure;
  http.setUserAgent("WLED-CTA/0.1");
  http.setReuse(true);
  http.addHeader("Connection", "keep-alive", true, true);
  http.addHeader("Accept", "application/xml", true, true);
  static const char* hdrs[] = {"Content-Type", "Content-Length", "Content-Encoding", "Transfer-Encoding"};
  http.collectHeaders(hdrs, 4);

  int status = http.GET();
  if (status < 200 || status >= 300) {
    if (status < 0) {
      String err = HTTPClient::errorToString(status);
      DEBUG_PRINTF("DepartStrip: CTA HTTP error %d (%s)\n", status, err.c_str());
    } else {
      DEBUG_PRINTF("DepartStrip: CTA HTTP status %d\n", status);
    }
    http.end();
    httpTransport_.end(localSecure);
    outStatus = status;
    return false;
  }

  outStatus = status;
  outLen = http.getSize();
  return true;
}

void CtaTrainTrackerSource::closeHttp(HTTPClient& http, bool usedSecure) {
  http.end();
  httpTransport_.end(usedSecure);
}

bool CtaTrainTrackerSource::readHttpBody(HTTPClient& http, int /*lenHint*/, String& outBody) {
  outBody = http.getString();
  return outBody.length() > 0;
}

bool CtaTrainTrackerSource::parseResponse(const String& xml,
                                          const StopQuery& query,
                                          std::time_t now,
                                          DepartModel::Entry::Batch& batch) {
  batch.items.clear();
  batch.apiTs = 0;
  batch.ourTs = now;
  String firstStopName;

  String timestamp = extractTagValue(xml, F("tmst"));
  timestamp.trim();
  if (timestamp.length() > 0) {
    time_t apiTs = 0;
    if (parseTimestamp(timestamp, apiTs)) batch.apiTs = apiTs;
  }

  int searchPos = 0;
  while (true) {
    int open = xml.indexOf(F("<eta"), searchPos);
    if (open < 0) break;
    int close = xml.indexOf(F("</eta>"), open);
    if (close < 0) break;
    int blockStart = xml.indexOf('>', open);
    if (blockStart < 0 || blockStart >= close) {
      searchPos = close + 5;
      continue;
    }
    ++blockStart;
    String block = xml.substring(blockStart, close);
    searchPos = close + 5;

    String dirStr = extractTagValue(block, F("trDr"));
    trimString(dirStr);
    String staName = extractTagValue(block, F("staNm"));
    trimString(staName);
    if (firstStopName.length() == 0 && staName.length() > 0) firstStopName = staName;

    if (query.direction > 0) {
      if (dirStr.length() == 0) continue;
      uint8_t dirVal = 0;
      if (!parseUnsigned8(dirStr, dirVal)) continue;
      if (dirVal != query.direction) continue;
    }

    String arrStr = extractTagValue(block, F("arrT"));
    trimString(arrStr);
    if (arrStr.length() == 0) {
      arrStr = extractTagValue(block, F("prdt"));
      trimString(arrStr);
    }
    if (arrStr.length() == 0) continue;

    time_t arrivalUtc = 0;
    if (!parseTimestamp(arrStr, arrivalUtc)) continue;

    if (now > 0 && arrivalUtc + 300 < now) continue;

    String route = extractTagValue(block, F("rt"));
    trimString(route);
    if (route.length() == 0) {
      route = extractTagValue(block, F("destNm"));
      trimString(route);
    }
    if (route.length() == 0) {
      route = extractTagValue(block, F("stpDe"));
      trimString(route);
    }
    if (route.length() == 0) route = F("Service");

    if (query.labelSuffix.length() > 0) {
      route += ' ';
      route += query.labelSuffix;
    }

    DepartModel::Entry::Item item;
    item.estDep = arrivalUtc;
    item.lineRef = route;
    batch.items.push_back(std::move(item));
  }

  if (firstStopName.length() > 0) lastStopName_ = firstStopName;

  return true;
}

bool CtaTrainTrackerSource::parseTimestamp(const String& value, time_t& outUtc) const {
  if (value.length() < 17) return false;
  int Y=0, M=0, D=0, h=0, m=0, s=0;
  if (sscanf(value.c_str(), "%4d%2d%2d %2d:%2d:%2d", &Y, &M, &D, &h, &m, &s) != 6) return false;
  if (M < 1 || M > 12 || D < 1 || D > 31) return false;
  if (h < 0 || h > 23 || m < 0 || m > 59 || s < 0 || s > 60) return false;
  int days = days_from_civil(Y, (unsigned)M, (unsigned)D);
  long localSeconds = (long)days * 86400L + (long)h * 3600L + (long)m * 60L + (long)s;
  long offset = departstrip::util::current_offset();
  outUtc = (time_t)(localSeconds - offset);
  return true;
}

void CtaTrainTrackerSource::splitStopCodes(const String& input, std::vector<String>& out) {
  out.clear();
  String normalized = input;
  normalized.replace(';', ',');
  normalized.replace(' ', ',');
  normalized.replace('\t', ',');
  int start = 0;
  int len = normalized.length();
  while (start < len) {
    int comma = normalized.indexOf(',', start);
    String token = (comma >= 0) ? normalized.substring(start, comma) : normalized.substring(start);
    token.trim();
    if (token.length() > 0) out.push_back(token);
    if (comma < 0) break;
    start = comma + 1;
  }
}

bool CtaTrainTrackerSource::parseStopToken(const String& token, StopQuery& out, String& explicitAgency) {
  explicitAgency = String();
  StopQuery parsed;

  String temp = token;
  temp.trim();
  if (temp.length() == 0) return false;

  int eq = temp.indexOf('=');
  if (eq >= 0) {
    parsed.labelSuffix = temp.substring(eq + 1);
    temp = temp.substring(0, eq);
    parsed.labelSuffix.trim();
    if (parsed.labelSuffix.length() > 0) {
      parsed.labelSuffix.replace(' ', '-');
      parsed.labelSuffix.replace('\t', '-');
    }
  }

  String agencyToken;
  String codeToken;
  int colon = temp.indexOf(':');
  if (colon > 0) {
    agencyToken = temp.substring(0, colon);
    codeToken = temp.substring(colon + 1);
  } else {
    codeToken = temp;
  }
  codeToken.trim();
  if (agencyToken.length() > 0) explicitAgency = agencyToken;
  if (codeToken.length() == 0) return false;

  String mapToken = codeToken;
  String dirToken;
  int dot = codeToken.lastIndexOf('.');
  if (dot > 0) {
    mapToken = codeToken.substring(0, dot);
    dirToken = codeToken.substring(dot + 1);
  }
  mapToken.trim();
  dirToken.trim();
  if (mapToken.length() == 0) return false;

  uint32_t mapId = 0;
  if (!parseUnsigned(mapToken, mapId)) return false;
  parsed.mapId = mapId;
  parsed.mapIdStr = mapToken;

  if (dirToken.length() > 0) {
    uint32_t dirWide = 0;
    if (!parseUnsigned(dirToken, dirWide)) return false;
    if (dirWide > 255) return false;
    parsed.direction = (uint8_t)dirWide;
  }

  parsed.canonical = mapToken;
  if (dirToken.length() > 0) {
    parsed.canonical += '.';
    parsed.canonical += dirToken;
  }
  if (parsed.labelSuffix.length() > 0) {
    parsed.canonical += '=';
    parsed.canonical += parsed.labelSuffix;
  }

  out = parsed;
  return true;
}

bool CtaTrainTrackerSource::parseUnsigned(const String& s, uint32_t& out) {
  out = 0;
  if (s.length() == 0) return false;
  for (unsigned i = 0; i < s.length(); ++i) {
    char c = s.charAt(i);
    if (c < '0' || c > '9') return false;
    out = out * 10u + (uint32_t)(c - '0');
  }
  return true;
}

bool CtaTrainTrackerSource::parseUnsigned8(const String& s, uint8_t& out) {
  uint32_t wide = 0;
  if (!parseUnsigned(s, wide)) return false;
  if (wide > 255) return false;
  out = (uint8_t)wide;
  return true;
}

String CtaTrainTrackerSource::extractTagValue(const String& block, const __FlashStringHelper* tag) {
  if (!tag) return String();
  String key(tag);
  return extractTagValue(block, key.c_str());
}

String CtaTrainTrackerSource::extractTagValue(const String& block, const char* tag) {
  if (!tag || !*tag) return String();
  String open;
  open.reserve(strlen(tag) + 2);
  open += '<';
  open += tag;
  open += '>';
  String close;
  close.reserve(strlen(tag) + 3);
  close += '<';
  close += '/';
  close += tag;
  close += '>';
  int start = block.indexOf(open);
  if (start < 0) return String();
  start += open.length();
  int end = block.indexOf(close, start);
  if (end < 0) return String();
  return block.substring(start, end);
}
