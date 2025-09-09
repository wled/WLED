#include "siri_source.h"
#include "util.h"

#include <algorithm>
#include <cstring>

namespace {
// Stream wrapper that discards a leading UTF-8 BOM if present, then passes through bytes unchanged.
struct SkipBOMStream : public Stream {
  Stream& in;
  bool init = false;
  uint8_t head[3];
  size_t headLen = 0;
  size_t headPos = 0;
  explicit SkipBOMStream(Stream& s) : in(s) {}
  // Satisfy Print's pure virtuals
  size_t write(uint8_t) override { return 0; }
  size_t write(const uint8_t*, size_t) { return 0; }
  void ensureInit() {
    if (init) return;
    init = true;
    uint8_t buf[3];
    size_t got = in.readBytes(buf, sizeof(buf));
    if (got == 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
      headLen = 0; headPos = 0; // skip BOM
    } else {
      headLen = got; headPos = 0;
      for (size_t i = 0; i < got; ++i) head[i] = buf[i];
    }
  }
  int available() override {
    ensureInit();
    return (int)(headLen - headPos) + in.available();
  }
  int read() override {
    ensureInit();
    if (headPos < headLen) return head[headPos++];
    return in.read();
  }
  int peek() override {
    ensureInit();
    if (headPos < headLen) return head[headPos];
    return in.peek();
  }
  void flush() override { in.flush(); }
  size_t readBytes(char* buffer, size_t length) override {
    ensureInit();
    size_t n = 0;
    while (n < length && headPos < headLen) buffer[n++] = head[headPos++];
    if (n < length) n += in.readBytes(buffer + n, length - n);
    return n;
  }
  size_t readBytes(uint8_t* buffer, size_t length) { return readBytes((char*)buffer, length); }
  using Stream::readBytes;
};
} // namespace

using departstrip::util::time_now;

SiriSource::SiriSource(const char* key, const char* defAgency, const char* defStopCode, const char* defBaseUrl) {
  if (key && *key) configKey_ = key;
  if (defAgency) agency_ = defAgency;
  if (defStopCode) stopCode_ = defStopCode;
  if (defBaseUrl) baseUrl_ = defBaseUrl;
}

void SiriSource::reload(std::time_t now) {
  nextFetch_ = now;
  backoffMult_ = 1;
}

String SiriSource::composeUrl(const String& agency, const String& stopCode) const {
  // Treat baseUrl_ strictly as a template: perform placeholder substitutions if present
  // and otherwise use it as-is without appending legacy query parameters.
  String url = baseUrl_;
  // Placeholder substitutions (case variants)
  url.replace(F("{agency}"), agency);
  url.replace(F("{AGENCY}"), agency);
  url.replace(F("{stopcode}"), stopCode);
  url.replace(F("{stopCode}"), stopCode);
  if (apiKey_.length() > 0) {
    url.replace(F("{apikey}"), apiKey_);
    url.replace(F("{apiKey}"), apiKey_);
    url.replace(F("{APIKEY}"), apiKey_);
  }
  return url;
}

// Minimal RFC3339 parser to UTC epoch (seconds). Supports:
// YYYY-MM-DDTHH:MM:SS[.fff][Z|±HH:MM]
static int days_from_civil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const int era = (y >= 0 ? y : y-399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153*(m + (m > 2 ? -3 : 9)) + 2)/5 + d - 1;
  const unsigned doe = yoe*365 + yoe/4 - yoe/100 + yoe/400 + doy;
  return era*146097 + (int)doe - 719468;
}

bool SiriSource::parseRFC3339ToUTC(const char* s, time_t& outUtc) {
  if (!s || !*s) return false;
  int Y=0,M=0,D=0,h=0,m=0,sec=0;
  if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y,&M,&D,&h,&m,&sec) != 6) return false;
  // Find timezone indicator only after the time component (after 'T', and optional fractional seconds)
  const char* tmark = strchr(s, 'T');
  const char* tz = nullptr;
  if (tmark) {
    // Search in the remainder for 'Z', 'z', '+' or '-' (timezone designators)
    tz = strpbrk(tmark + 1, "Zz+-");
  }
  int tzSign = 0, tzH=0, tzM=0;
  if (!tz) {
    tzSign = 0; tzH = tzM = 0; // no explicit timezone -> assume Z/UTC
  } else if (*tz == 'Z' || *tz == 'z') {
    tzSign = 0; tzH = tzM = 0;
  } else if (*tz == '+' || *tz == '-') {
    tzSign = (*tz == '+') ? +1 : -1;
    int th=0, tmn=0;
    // Expect "+HH:MM" or "-HH:MM" first; fallback to "+HHMM" or "-HHMM"
    if (sscanf(tz+1, "%2d:%2d", &th, &tmn) == 2) {
      tzH = th; tzM = tmn;
    } else if (sscanf(tz+1, "%2d%2d", &th, &tmn) == 2) {
      tzH = th; tzM = tmn;
    }
  }
  int days = days_from_civil(Y, (unsigned)M, (unsigned)D);
  long sec_of_day = h*3600L + m*60L + sec;
  long tz_off = tzSign * (tzH*3600L + tzM*60L);
  long long epoch = (long long)days*86400LL + (long long)sec_of_day - tz_off;
  outUtc = (time_t)epoch;
  return true;
}

bool SiriSource::httpBegin(const String& url, int& outLen) {
  http_.setTimeout(10000);
  if (!http_.begin(client_, url)) {
    http_.end();
    DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: begin() failed"));
    return false;
  }
  // Try to mimic curl behavior to avoid gzip responses
  http_.useHTTP10(true);
  http_.setUserAgent("curl/7.79.1");
  http_.setReuse(false);
  http_.addHeader("Connection", "close");
  http_.addHeader("Accept", "*/*", true, true);
  http_.addHeader("Accept-Encoding", "identity", true, true);
  static const char* hdrs[] = {"Content-Type", "Content-Encoding", "Content-Length", "RateLimit-Remaining"};
  http_.collectHeaders(hdrs, 4);
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: free heap before GET: %u\n", ESP.getFreeHeap());
  int httpCode = http_.GET();
  // Log rate limit remaining if provided by the server
  {
    String rl = http_.header("RateLimit-Remaining");
    if (rl.length() > 0) {
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: RateLimit-Remaining=%s\n", rl.c_str());
    }
  }
  if (httpCode < 200 || httpCode >= 300) {
    http_.end();
    if (httpCode < 0) {
      String err = HTTPClient::errorToString(httpCode);
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: HTTP error %d (%s)\n", httpCode, err.c_str());
    } else {
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: HTTP status %d\n", httpCode);
    }
    return false;
  }
  String enc = http_.header("Content-Encoding");
  String ctype = http_.header("Content-Type");
  outLen = http_.getSize();
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: enc='%s' type='%s' len=%d\n", enc.c_str(), ctype.c_str(), outLen);
  return true;
}

bool SiriSource::parseJsonFromHttp(DynamicJsonDocument& doc, bool withFilter, bool wideFilter) {
  SkipBOMStream s(http_.getStream());
  DeserializationError err;
  if (withFilter) {
    if (wideFilter) {
      // Wide filter: keep the whole StopMonitoringDelivery subtree to ensure visits are present.
      // Used when Content-Length is known (capacity sized accordingly).
      StaticJsonDocument<768> filter;
      filter["Siri"]["ServiceDelivery"]["ResponseTimestamp"] = true;
      filter["Siri"]["ServiceDelivery"]["StopMonitoringDelivery"] = true;
      filter["ServiceDelivery"]["ResponseTimestamp"] = true;
      filter["ServiceDelivery"]["StopMonitoringDelivery"] = true;
      err = deserializeJson(doc, s,
                            DeserializationOption::Filter(filter),
                            DeserializationOption::NestingLimit(80));
    } else {
      // Targeted/narrow filter for unknown Content-Length to limit memory.
      StaticJsonDocument<4096> filter;
      auto addMVJMask = [&](JsonObject mvj) {
        mvj["LineRef"] = true; // allow string or object
        JsonObject call = mvj["MonitoredCall"].to<JsonObject>();
        call["StopPointName"] = true;
        call["ExpectedDepartureTime"] = true;
        call["ExpectedArrivalTime"] = true;
        call["AimedDepartureTime"] = true;
        call["AimedArrivalTime"] = true;
      };
      // With Siri wrapper
      filter["Siri"]["ServiceDelivery"]["ResponseTimestamp"] = true;
      {
        JsonObject mvj = filter["Siri"]["ServiceDelivery"]["StopMonitoringDelivery"][0]
                               ["MonitoredStopVisit"][0]
                               ["MonitoredVehicleJourney"].to<JsonObject>();
        addMVJMask(mvj);
      }
      {
        JsonObject mvj = filter["Siri"]["ServiceDelivery"]["StopMonitoringDelivery"]
                               ["MonitoredStopVisit"][0]
                               ["MonitoredVehicleJourney"].to<JsonObject>();
        addMVJMask(mvj);
      }
      // Without Siri wrapper (top-level ServiceDelivery)
      filter["ServiceDelivery"]["ResponseTimestamp"] = true;
      {
        JsonObject mvj = filter["ServiceDelivery"]["StopMonitoringDelivery"][0]
                               ["MonitoredStopVisit"][0]
                               ["MonitoredVehicleJourney"].to<JsonObject>();
        addMVJMask(mvj);
      }
      {
        JsonObject mvj = filter["ServiceDelivery"]["StopMonitoringDelivery"]
                               ["MonitoredStopVisit"][0]
                               ["MonitoredVehicleJourney"].to<JsonObject>();
        addMVJMask(mvj);
      }
      err = deserializeJson(doc, s,
                            DeserializationOption::Filter(filter),
                            DeserializationOption::NestingLimit(80));
    }
  } else {
    err = deserializeJson(doc, s, DeserializationOption::NestingLimit(80));
  }
  http_.end();
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: after parse, memUsage=%u, free heap=%u\n", (unsigned)doc.memoryUsage(), ESP.getFreeHeap());
  if (err) {
    DEBUG_PRINTF("DepartStrip: SiriSource::fetch: deserializeJson failed: %s\n", err.c_str());
    return false;
  }
  return true;
}

size_t SiriSource::computeJsonCapacity(int contentLen) {
  if (contentLen > 0) return (size_t)contentLen * 2;
  // Unknown length: pick a slightly larger default to reduce NoMemory risk
  // while the filter keeps actual usage low on most feeds.
  return 20480; // 20 KB
}

JsonObject SiriSource::getSiriRoot(DynamicJsonDocument& doc, bool& usedTopLevelFallback) {
  usedTopLevelFallback = false;
  JsonObject siri = doc["Siri"].as<JsonObject>();
  if (siri.isNull()) {
    // Try treating top-level as the Siri container
    if (doc["ServiceDelivery"].is<JsonObject>()) {
      usedTopLevelFallback = true;
      return doc.as<JsonObject>();
    }
  }
  return siri;
}

bool SiriSource::buildModelFromSiri(JsonObject siri, std::time_t now, std::unique_ptr<DepartModel>& outModel) {
  if (siri.isNull()) return false;
  JsonObject sd = siri["ServiceDelivery"].as<JsonObject>();
  time_t apiTs = 0;
  parseRFC3339ToUTC(sd["ResponseTimestamp"] | (const char*)nullptr, apiTs);
  if (!apiTs) apiTs = now;

  JsonArray visits;
  if (sd["StopMonitoringDelivery"].is<JsonArray>()) {
    JsonObject del = sd["StopMonitoringDelivery"][0].as<JsonObject>();
    visits = del["MonitoredStopVisit"].as<JsonArray>();
  } else if (sd["StopMonitoringDelivery"].is<JsonObject>()) {
    JsonObject del = sd["StopMonitoringDelivery"].as<JsonObject>();
    visits = del["MonitoredStopVisit"].as<JsonArray>();
  }
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: visits null=%d size=%u\n", visits.isNull(), (unsigned)(visits.isNull() ? 0 : visits.size()));
  if (visits.isNull()) {
    DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: no MonitoredStopVisit array"));
    return false;
  }

  DepartModel::Entry board;
  board.key.reserve(agency_.length() + 1 + stopCode_.length());
  board.key = agency_; board.key += ":"; board.key += stopCode_;
  DepartModel::Entry::Batch batch;
  batch.apiTs = apiTs;
  batch.ourTs = now;

  int totalVisits = 0, hadTime = 0, parsedTime = 0;
  String firstStopName;
  for (JsonObject v : visits) {
    ++totalVisits;
    JsonObject mvj = v["MonitoredVehicleJourney"].as<JsonObject>();
    if (mvj.isNull()) continue;
    JsonObject call = mvj["MonitoredCall"].as<JsonObject>();
    if (call.isNull()) continue;
    if (firstStopName.length() == 0) {
      const char* nm = call["StopPointName"] | (const char*)nullptr;
      if (nm && *nm) firstStopName = nm;
    }

    const char* tstr = call["ExpectedDepartureTime"] | call["ExpectedArrivalTime"] |
                       call["AimedDepartureTime"] | call["AimedArrivalTime"] | (const char*)nullptr;
    time_t depUtc = 0;
    if (tstr) ++hadTime;
    if (tstr && parseRFC3339ToUTC(tstr, depUtc)) {
      ++parsedTime;
      DepartModel::Entry::Item item; item.estDep = depUtc;
      // LineRef may be nested or plain; handle both
      if (mvj["LineRef"].is<JsonObject>()) item.lineRef = (const char*)(mvj["LineRef"]["value"] | "");
      else item.lineRef = (const char*)(mvj["LineRef"] | "");
      batch.items.push_back(item);
    } else if (tstr && parsedTime < 3) {
      // Log a few samples of bad timestamps
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: failed to parse time '%s'\n", tstr);
    }
  }
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: visits=%d hadTime=%d parsed=%d items=%u\n", totalVisits, hadTime, parsedTime, (unsigned)batch.items.size());

  if (batch.items.empty()) {
    DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: no items parsed"));
    return false;
  }

  std::sort(batch.items.begin(), batch.items.end(), [](const DepartModel::Entry::Item& a, const DepartModel::Entry::Item& b){ return a.estDep < b.estDep; });

  board.batch = std::move(batch);

  std::unique_ptr<DepartModel> model(new DepartModel());
  model->boards.push_back(std::move(board));
  outModel = std::move(model);
  if (firstStopName.length() > 0) lastStopName_ = firstStopName;
  return true;
}

std::unique_ptr<DepartModel> SiriSource::fetch(std::time_t now) {
  if (!enabled_) return nullptr;
  if (now == 0) return nullptr;
  if (now < nextFetch_) {
    // Periodically log backoff status at roughly the source's update cadence
    time_t interval = updateSecs_ > 0 ? updateSecs_ : 60;
    if (lastBackoffLog_ == 0 || now - lastBackoffLog_ >= interval) {
      lastBackoffLog_ = now;
      long rem = (long)(nextFetch_ - now);
      if (rem < 0) rem = 0;
      String key = sourceKey();
      if (backoffMult_ > 1) {
        DEBUG_PRINTF("DepartStrip: SiriSource::fetch: backoff x%u %s, next in %lds\n",
                     (unsigned)backoffMult_, key.c_str(), rem);
      } else {
        DEBUG_PRINTF("DepartStrip: SiriSource::fetch: waiting %s, next in %lds\n",
                     key.c_str(), rem);
      }
    }
    return nullptr;
  }

  String url = composeUrl(agency_, stopCode_);
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: URL: %s\n", url.c_str());
  int len = 0;
  if (!httpBegin(url, len)) {
    // Schedule retry with exponential backoff (per source)
    long delay = (long)updateSecs_ * (long)backoffMult_;
    DEBUG_PRINTF("DepartStrip: SiriSource::fetch: scheduling backoff x%u %s for %lds (begin/HTTP error)\n",
                 (unsigned)backoffMult_, sourceKey().c_str(), delay);
    nextFetch_ = now + delay;
    if (backoffMult_ < 16) backoffMult_ *= 2;
    return nullptr;
  }

  size_t jsonSz = computeJsonCapacity(len);
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: json capacity=%u, free heap=%u\n", (unsigned)jsonSz, ESP.getFreeHeap());
  DynamicJsonDocument doc(jsonSz);
  // Use wide filter when Content-Length available; narrow otherwise.
  bool wideFilter = (len > 0);
  if (!parseJsonFromHttp(doc, true, wideFilter)) {
    long delay = (long)updateSecs_ * (long)backoffMult_;
    DEBUG_PRINTF("DepartStrip: SiriSource::fetch: scheduling backoff x%u %s for %lds (parse error)\n",
                 (unsigned)backoffMult_, sourceKey().c_str(), delay);
    nextFetch_ = now + delay;
    if (backoffMult_ < 16) backoffMult_ *= 2;
    return nullptr;
  }

  bool usedTop = false;
  JsonObject siri = getSiriRoot(doc, usedTop);
  if (usedTop) DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: using top-level as Siri container"));
  if (siri.isNull()) {
    DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: missing Siri root"));
    long delay = (long)updateSecs_ * (long)backoffMult_;
    DEBUG_PRINTF("DepartStrip: SiriSource::fetch: scheduling backoff x%u %s for %lds (bad JSON shape)\n",
                 (unsigned)backoffMult_, sourceKey().c_str(), delay);
    nextFetch_ = now + delay;
    if (backoffMult_ < 16) backoffMult_ *= 2;
    return nullptr;
  }

  std::unique_ptr<DepartModel> model;
  if (!buildModelFromSiri(siri, now, model)) {
    nextFetch_ = now + updateSecs_;
    backoffMult_ = 1;
    return nullptr;
  }

  // Model summary logging handled in DepartModel::update()
  nextFetch_ = now + updateSecs_;
  backoffMult_ = 1;
  return model;
}

void SiriSource::addToConfig(JsonObject& root) {
  root["Enabled"] = enabled_;
  root["UpdateSecs"] = updateSecs_;
  root["TemplateUrl"] = baseUrl_;
  root["ApiKey"] = apiKey_;
  String key; key.reserve(agency_.length()+1+stopCode_.length());
  key = agency_; key += ":"; key += stopCode_;
  root["AgencyStopCode"] = key;
}

bool SiriSource::readFromConfig(JsonObject& root, bool startup_complete, bool& invalidate_history) {
  bool ok = true;
  bool prevEnabled = enabled_;
  String prevAgency = agency_;
  String prevStop = stopCode_;
  String prevBase = baseUrl_;

  ok &= getJsonValue(root["Enabled"], enabled_, enabled_);
  ok &= getJsonValue(root["UpdateSecs"], updateSecs_, updateSecs_);
  ok &= getJsonValue(root["TemplateUrl"], baseUrl_, baseUrl_);
  ok &= getJsonValue(root["ApiKey"], apiKey_, apiKey_);
  // Prefer combined Key ("AGENCY:StopCode"); fall back to legacy Agency/StopCode if not provided
  String keyStr;
  bool haveKey = getJsonValue(root["AgencyStopCode"], keyStr, (const char*)nullptr); if (!haveKey) haveKey = getJsonValue(root["Key"], keyStr, (const char*)nullptr);
  if (haveKey && keyStr.length() > 0) {
    int colon = keyStr.indexOf(':');
    if (colon > 0) {
      agency_ = keyStr.substring(0, colon);
      stopCode_ = keyStr.substring(colon+1);
    }
  } else {
    ok &= getJsonValue(root["Agency"], agency_, agency_);
    ok &= getJsonValue(root["StopCode"], stopCode_, stopCode_);
  }
  // Update friendly config key to mirror views: SiriSource_AGENCY_StopCode
  {
    String sec = F("SiriSource_"); sec += agency_; sec += '_'; sec += stopCode_;
    configKey_ = std::string(sec.c_str());
  }

  if (updateSecs_ < 10) updateSecs_ = 10; // minimum 10s cadence

  invalidate_history |= (agency_ != prevAgency) || (stopCode_ != prevStop) || (baseUrl_ != prevBase);
  if (startup_complete && !prevEnabled && enabled_) reload(departstrip::util::time_now_utc());
  return ok;
}
