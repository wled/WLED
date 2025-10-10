#include "siri_source.h"
#include "util.h"

#include <algorithm>
#include <cstring>
#include <cctype>
#include <cstdlib>

namespace {
static constexpr size_t SIRI_SNIFF_PREFIX = 256;

// Emit a short printable preview of the response payload for debugging.
static void debugLogPayloadPrefix(const char* data, size_t len, bool truncated) {
#if defined(WLED_DEBUG)
  if (!data || len == 0) {
    DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: payload prefix: <empty>"));
    return;
  }
  String snippet;
  snippet.reserve(len + 16);
  for (size_t i = 0; i < len; ++i) {
    char c = data[i];
    if (c == '\n') {
      snippet += F("\\n");
    } else if (c == '\r') {
      snippet += F("\\r");
    } else if (c == '\t') {
      snippet += F("\\t");
    } else if (c >= 32 && c <= 126) {
      snippet += c;
    } else {
      snippet += '.';
    }
  }
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: payload prefix (%u bytes%s): %s\n",
               (unsigned)len, truncated ? " truncated" : "", snippet.c_str());
#else
  (void)data;
  (void)len;
  (void)truncated;
#endif
}

// Stream that decodes HTTP/1.1 chunked transfer encoding.
struct ChunkedDecodingStream : public Stream {
  Stream& in;
  size_t chunkRemaining = 0;
  bool finished = false;
  bool error = false;
  int peekBuf = -1;
  size_t dataRead = 0;

  explicit ChunkedDecodingStream(Stream& s) : in(s) {}

  size_t write(uint8_t) override { return 0; }
  size_t write(const uint8_t*, size_t) override { return 0; }
  void flush() override { in.flush(); }

  int available() override {
    if (peekBuf >= 0) return 1;
    if (finished) return 0;
    return in.available();
  }

  int read() override {
    if (peekBuf >= 0) {
      int c = peekBuf;
      peekBuf = -1;
      return c;
    }
    return readNext();
  }

  int peek() override {
    if (peekBuf >= 0) return peekBuf;
    int c = readNext();
    if (c >= 0) peekBuf = c;
    return c;
  }

  size_t readBytes(char* buffer, size_t length) override {
    size_t n = 0;
    while (n < length) {
      int c = read();
      if (c < 0) break;
      buffer[n++] = (char)c;
    }
    return n;
  }

  size_t readBytes(uint8_t* buffer, size_t length) override { return readBytes((char*)buffer, length); }

  size_t bytesDecoded() const { return dataRead; }

private:
  int readByte() {
    uint8_t b;
    size_t got = in.readBytes(&b, 1);
    if (got == 1) return b;
    return -1;
  }

  int readNext() {
    if (finished) return -1;
    if (!ensureChunk()) return -1;
    int c = readByte();
    if (c < 0) {
      finished = true;
      error = true;
      return -1;
    }
    if (chunkRemaining == 0) {
      // Should not happen, but guard anyway
      return -1;
    }
    --chunkRemaining;
    ++dataRead;
    if (chunkRemaining == 0) {
      if (!consumeCRLF()) {
        finished = true;
        error = true;
      }
    }
    return c;
  }

  bool ensureChunk() {
    while (chunkRemaining == 0) {
      if (finished) return false;
      if (!readChunkHeader()) {
        finished = true;
        return false;
      }
      if (finished) return false;
    }
    return true;
  }

  bool readChunkHeader() {
    char line[64];
    size_t len = 0;
    if (!readLine(line, sizeof(line), len)) {
      error = true;
      return false;
    }
    char* start = line;
    while (*start && isspace((unsigned char)*start)) ++start;
    char* semi = strchr(start, ';');
    if (semi) *semi = '\0';
    if (*start == '\0') {
      error = true;
      return false;
    }
    char* endptr = nullptr;
    unsigned long chunk = strtoul(start, &endptr, 16);
    if (endptr == start) {
      error = true;
      return false;
    }
    chunkRemaining = (size_t)chunk;
    if (chunkRemaining == 0) {
      consumeTrailers();
      finished = true;
      return false;
    }
    return true;
  }

  bool readLine(char* out, size_t cap, size_t& outLen) {
    outLen = 0;
    while (true) {
      int c = readByte();
      if (c < 0) return false;
      if (c == '\n') break;
      if (c != '\r') {
        if (outLen + 1 < cap) out[outLen++] = (char)c;
      }
    }
    out[outLen] = '\0';
    return true;
  }

  bool consumeCRLF() {
    uint8_t buf[2];
    if (in.readBytes(buf, 2) != 2) {
      error = true;
      return false;
    }
    if (buf[0] != '\r' || buf[1] != '\n') {
      error = true;
      return false;
    }
    return true;
  }

  void consumeTrailers() {
    char line[64];
    size_t len = 0;
    while (true) {
      if (!readLine(line, sizeof(line), len)) {
        error = true;
        break;
      }
      if (len == 0) break;
    }
  }
};

// Stream wrapper that discards a leading UTF-8 BOM if present, then passes through bytes unchanged.
struct SkipBOMStream : public Stream {
  Stream& in;
  bool init = false;
  uint8_t head[3];
  size_t headLen = 0;
  size_t headPos = 0;
  size_t rawRead = 0;
  char* sniffBuf = nullptr;
  size_t sniffCap = 0;
  size_t sniffLen = 0;
  bool sniffOverflow = false;
  explicit SkipBOMStream(Stream& s, char* sniff = nullptr, size_t sniffCapBytes = 0)
      : in(s), sniffBuf(sniff), sniffCap(sniffCapBytes) {}
  // Satisfy Print's pure virtuals
  size_t write(uint8_t) override { return 0; }
  size_t write(const uint8_t*, size_t) override { return 0; }
  void sniff(uint8_t b) {
    if (!sniffBuf || sniffCap == 0) return;
    if (sniffLen < sniffCap) {
      sniffBuf[sniffLen++] = (char)b;
    } else {
      sniffOverflow = true;
    }
  }
  void ensureInit() {
    if (init) return;
    init = true;
    uint8_t buf[3];
    size_t got = in.readBytes(buf, sizeof(buf));
    rawRead += got;
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
    if (headPos < headLen) {
      uint8_t b = head[headPos++];
      sniff(b);
      return b;
    }
    int v = in.read();
    if (v >= 0) {
      ++rawRead;
      sniff((uint8_t)v);
    }
    return v;
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
    while (n < length && headPos < headLen) {
      uint8_t b = head[headPos++];
      buffer[n++] = b;
      sniff(b);
    }
    if (n < length) {
      size_t got = in.readBytes(buffer + n, length - n);
      rawRead += got;
      if (got > 0) {
        size_t canStore = (sniffCap > sniffLen) ? std::min(sniffCap - sniffLen, got) : (size_t)0;
        if (canStore > 0) {
          memcpy(sniffBuf + sniffLen, buffer + n, canStore);
          sniffLen += canStore;
        }
        if (got > canStore) sniffOverflow = true;
      }
      n += got;
    }
    return n;
  }
  size_t readBytes(uint8_t* buffer, size_t length) { return readBytes((char*)buffer, length); }
  using Stream::readBytes;
  size_t bytesConsumed() const { return rawRead; }
  size_t sniffedLength() const { return sniffLen; }
  bool sniffTruncated() const { return sniffOverflow; }
};

// Extract the first meaningful string from an ArduinoJson variant that might be
// a string, object with a "value" member, or an array of those.
static String jsonFirstString(JsonVariantConst v) {
  if (v.is<const char*>()) {
    const char* s = v.as<const char*>();
    return (s && *s) ? String(s) : String();
  }
  if (v.is<JsonObjectConst>()) {
    JsonObjectConst obj = v.as<JsonObjectConst>();
    const char* s = obj["value"] | obj["Value"] | (const char*)nullptr;
    return (s && *s) ? String(s) : String();
  }
  if (v.is<JsonArrayConst>()) {
    JsonArrayConst arr = v.as<JsonArrayConst>();
    for (JsonVariantConst child : arr) {
      String s = jsonFirstString(child);
      if (s.length()) return s;
    }
  }
  return String();
}

struct ParsedStopQuery {
  String canonical;
  String monitoringRef;
  String destinationRef;
  String notViaRef;
  String labelSuffix;
};

static void splitStopCodes(const String& input, std::vector<String>& out) {
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

static void extractAgencyAndCode(String token, String& agencyOut, String& codeOut) {
  token.trim();
  int colon = token.indexOf(':');
  if (colon > 0) {
    agencyOut = token.substring(0, colon);
    codeOut = token.substring(colon + 1);
  } else {
    agencyOut = String();
    codeOut = token;
  }
  codeOut.trim();
}

static bool parseStopExpression(const String& token,
                                ParsedStopQuery& out,
                                String& explicitAgency) {
  explicitAgency = String();
  out = ParsedStopQuery();

  String work = token;
  work.trim();
  if (work.length() == 0) return false;

  int equalsPos = work.indexOf('=');
  if (equalsPos >= 0) {
    out.labelSuffix = work.substring(equalsPos + 1);
    work = work.substring(0, equalsPos);
    out.labelSuffix.trim();
  }
  if (out.labelSuffix.length() > 0) {
    out.labelSuffix.replace(' ', '-');
    out.labelSuffix.replace('\t', '-');
  }

  String remainder;
  int arrowPos = work.indexOf(F("->"));
  String base = work;
  if (arrowPos >= 0) {
    base = work.substring(0, arrowPos);
    remainder = work.substring(arrowPos + 2);
  }
  base.trim();
  String baseAgency;
  extractAgencyAndCode(base, baseAgency, out.monitoringRef);
  if (out.monitoringRef.length() == 0) return false;
  if (baseAgency.length() > 0) explicitAgency = baseAgency;

  while (remainder.length() > 0) {
    int nextArrow = remainder.indexOf(F("->"));
    String segment = (nextArrow >= 0) ? remainder.substring(0, nextArrow) : remainder;
    remainder = (nextArrow >= 0) ? remainder.substring(nextArrow + 2) : String();
    segment.trim();
    if (segment.length() == 0) continue;
    bool isNotVia = segment.charAt(0) == '!';
    if (isNotVia) segment = segment.substring(1);
    segment.trim();
    if (segment.length() == 0) continue;
    String segAgency;
    String segCode;
    extractAgencyAndCode(segment, segAgency, segCode);
    if (segCode.length() == 0) continue;
    if (isNotVia) {
      if (out.notViaRef.length() == 0) out.notViaRef = segCode;
    } else {
      if (out.destinationRef.length() == 0) out.destinationRef = segCode;
    }
  }

  out.canonical = out.monitoringRef;
  if (out.notViaRef.length() > 0) {
    out.canonical += F("->!");
    out.canonical += out.notViaRef;
  }
  if (out.destinationRef.length() > 0) {
    out.canonical += F("->");
    out.canonical += out.destinationRef;
  }
  if (out.labelSuffix.length() > 0) {
    out.canonical += '=';
    out.canonical += out.labelSuffix;
  }
  return true;
}

// Shared JSON buffer reused across all Siri sources to limit heap fragmentation.
static constexpr size_t SIRI_JSON_MAX_CAP = 24576;
static constexpr size_t SIRI_JSON_MIN_CAP = 4096;
static constexpr size_t SIRI_JSON_QUANTUM = 2048;
static DynamicJsonDocument* g_siriSharedDoc = nullptr;
static bool g_siriSharedInUse = false;
static size_t g_siriSharedDocCap = 0;
} // namespace

SiriSource::SiriSource(const char* key, const char* defAgency, const char* defStopCode, const char* defBaseUrl) {
  if (key && *key) configKey_ = key;
  if (defAgency) agency_ = defAgency;
  if (defStopCode) stopCode_ = defStopCode;
  if (defBaseUrl) baseUrl_ = defBaseUrl;
  if (stopCode_.length() > 0) {
    StopQuery q;
    q.canonical = stopCode_;
    q.monitoringRef = stopCode_;
    queries_.push_back(q);
  }
}

void SiriSource::reload(std::time_t now) {
  nextFetch_ = now;
  backoffMult_ = 1;
}

String SiriSource::composeUrl(const String& agency, const StopQuery& query) const {
  // Treat baseUrl_ strictly as a template: perform placeholder substitutions if present
  // and otherwise use it as-is without appending legacy query parameters.
  String url = baseUrl_;
  auto replaceAll = [&](const __FlashStringHelper* needle, const String& value) {
    if (!needle) return;
    String key(needle);
    if (key.length() == 0) return;
    url.replace(key, value);
  };

  const String& stop = query.monitoringRef;
  const String& dest = query.destinationRef;
  const String& notVia = query.notViaRef;

  replaceAll(F("{agency}"), agency);
  replaceAll(F("{AGENCY}"), agency);
  replaceAll(F("{monitoringref}"), stop);
  replaceAll(F("{MonitoringRef}"), stop);
  replaceAll(F("{MONITORINGREF}"), stop);
  replaceAll(F("{MonitoringRefg}"), stop);
  replaceAll(F("{monitoringrefg}"), stop);
  replaceAll(F("{MONITORINGREFG}"), stop);
  replaceAll(F("{stopcode}"), stop);
  replaceAll(F("{stopCode}"), stop);
  replaceAll(F("{STOPCODE}"), stop);
  replaceAll(F("{destinationref}"), dest);
  replaceAll(F("{DestinationRef}"), dest);
  replaceAll(F("{DESTINATIONREF}"), dest);
  replaceAll(F("{notviaref}"), notVia);
  replaceAll(F("{NotViaRef}"), notVia);
  replaceAll(F("{NOTVIAREF}"), notVia);
  if (apiKey_.length() > 0) {
    replaceAll(F("{apikey}"), apiKey_);
    replaceAll(F("{apiKey}"), apiKey_);
    replaceAll(F("{APIKEY}"), apiKey_);
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
  if (httpActive_) endHttp();
  http_.setTimeout(10000);
  bool usedSecure = false;
  WiFiClient* client = httpTransport_.begin(url, 10000, usedSecure);
  if (!client) {
    DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: no HTTP client available"));
    return false;
  }
  if (!http_.begin(*client, url)) {
    http_.end();
    httpTransport_.end(usedSecure);
    DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: begin() failed"));
    return false;
  }
  httpActive_ = true;
  httpUsedSecure_ = usedSecure;
  // Try to mimic curl behavior to avoid gzip responses
  http_.setUserAgent("curl/7.79.1");
  http_.setReuse(true);
  http_.addHeader("Connection", "keep-alive", true, true);
  http_.addHeader("Accept", "*/*", true, true);
  http_.addHeader("Accept-Encoding", "identity", true, true);
  static const char* hdrs[] = {"Content-Type", "Content-Encoding", "Content-Length", "Transfer-Encoding", "RateLimit-Remaining"};
  http_.collectHeaders(hdrs, 5);
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
    endHttp();
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
  String clen = http_.header("Content-Length");
  String tenc = http_.header("Transfer-Encoding");
  outLen = http_.getSize();
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: enc='%s' type='%s' len=%d contentLength='%s' transfer='%s'\n",
               enc.c_str(), ctype.c_str(), outLen, clen.c_str(), tenc.c_str());
  return true;
}

bool SiriSource::parseJsonFromHttp(JsonDocument& doc,
                                   bool chunked,
                                   char* sniffBuf,
                                   size_t sniffCap,
                                   size_t* sniffLenOut,
                                   bool* sniffTruncatedOut) {
  Stream& base = http_.getStream();
  ChunkedDecodingStream chunkStream(base);
  Stream& source = chunked ? static_cast<Stream&>(chunkStream) : base;
  SkipBOMStream s(source, sniffBuf, sniffCap);
  DeserializationError err;
  // Always apply the narrow filter
  static StaticJsonDocument<4096> filter;
  filter.clear();

  auto addMVJMask = [&](JsonObject mvj) {
    mvj["LineRef"] = true; // allow string or object
    mvj["PublishedLineName"] = true;
    JsonObject call = mvj["MonitoredCall"].to<JsonObject>();
    call["StopPointName"] = true;
    call["ExpectedDepartureTime"] = true;
    call["ExpectedArrivalTime"] = true;
    call["AimedDepartureTime"] = true;
    call["AimedArrivalTime"] = true;
  };

  // With Siri wrapper: StopMonitoringDelivery is an array for many providers (e.g., MTA)
  filter["Siri"]["ServiceDelivery"]["ResponseTimestamp"] = true;
  {
    JsonObject mvj = filter["Siri"]["ServiceDelivery"]["StopMonitoringDelivery"][0]
                           ["MonitoredStopVisit"][0]
                           ["MonitoredVehicleJourney"].to<JsonObject>();
    addMVJMask(mvj);
  }

  // Without Siri wrapper (top-level ServiceDelivery): some providers (e.g., AC) use an object
  filter["ServiceDelivery"]["ResponseTimestamp"] = true;
  {
    JsonObject mvj = filter["ServiceDelivery"]["StopMonitoringDelivery"]
                           ["MonitoredStopVisit"][0]
                           ["MonitoredVehicleJourney"].to<JsonObject>();
    addMVJMask(mvj);
  }

  err = deserializeJson(doc, s,
                        DeserializationOption::Filter(filter),
                        DeserializationOption::NestingLimit(80));
  if (sniffLenOut) *sniffLenOut = s.sniffedLength();
  if (sniffTruncatedOut) *sniffTruncatedOut = s.sniffTruncated();
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: after parse, consumed=%u, memUsage=%u, free heap=%u\n",
               (unsigned)s.bytesConsumed(), (unsigned)doc.memoryUsage(), ESP.getFreeHeap());
  if (err) {
    DEBUG_PRINTF("DepartStrip: SiriSource::fetch: deserializeJson failed: %s\n", err.c_str());
    if (err == DeserializationError::NoMemory && lastJsonCapacity_ < SIRI_JSON_MAX_CAP) {
      size_t bumped = lastJsonCapacity_ + SIRI_JSON_QUANTUM;
      if (bumped > SIRI_JSON_MAX_CAP) bumped = SIRI_JSON_MAX_CAP;
      lastJsonCapacity_ = bumped;
    }
    return false;
  }
  return true;
}

size_t SiriSource::computeJsonCapacity(int contentLen) {
  size_t target = lastJsonCapacity_;
  if (contentLen > 0) {
    size_t len = (size_t)contentLen;
    // Empirical: filtered docs for 3–32 visits use ~0.35–0.55x of payload.
    // Heuristic: 0.6 * payload + 2KB headroom.
    size_t estimate = (len * 3) / 5 + 2048;
    if (estimate > target) target = estimate;
  }
  if (target < SIRI_JSON_MIN_CAP) target = SIRI_JSON_MIN_CAP;
  if (target > SIRI_JSON_MAX_CAP) target = SIRI_JSON_MAX_CAP;
  target = ((target + SIRI_JSON_QUANTUM - 1) / SIRI_JSON_QUANTUM) * SIRI_JSON_QUANTUM;
  if (target > SIRI_JSON_MAX_CAP) target = SIRI_JSON_MAX_CAP;
  return target;
}

JsonDocument* SiriSource::acquireJsonDoc(size_t capacity, bool& fromPool) {
  if (capacity < 1024) capacity = 1024;
  fromPool = false;
  if (!g_siriSharedDoc) {
    g_siriSharedDoc = new DynamicJsonDocument(capacity);
    g_siriSharedDocCap = g_siriSharedDoc ? capacity : 0;
    if (!g_siriSharedDoc) return nullptr;
  } else if (!g_siriSharedInUse) {
    if (capacity > g_siriSharedDocCap || (g_siriSharedDocCap > capacity + SIRI_JSON_QUANTUM && capacity >= SIRI_JSON_MIN_CAP)) {
      delete g_siriSharedDoc;
      g_siriSharedDoc = new DynamicJsonDocument(capacity);
      g_siriSharedDocCap = g_siriSharedDoc ? capacity : 0;
      if (!g_siriSharedDoc) return nullptr;
    }
  }
  if (g_siriSharedDoc && !g_siriSharedInUse) {
    g_siriSharedDoc->clear();
    g_siriSharedInUse = true;
    fromPool = true;
    return g_siriSharedDoc;
  }
  // Fallback allocation; caller is responsible for deleting via releaseJsonDoc.
  DynamicJsonDocument* temp = new DynamicJsonDocument(capacity);
  if (temp) temp->clear();
  return temp;
}

void SiriSource::releaseJsonDoc(JsonDocument* doc, bool fromPool) {
  if (!doc) return;
  if (doc == g_siriSharedDoc) {
    g_siriSharedInUse = false;
    doc->clear();
    return;
  }
  if (!fromPool) delete static_cast<DynamicJsonDocument*>(doc);
}

JsonObject SiriSource::getSiriRoot(JsonDocument& doc, bool& usedTopLevelFallback) {
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

bool SiriSource::appendItemsFromSiri(JsonObject siri,
                                     const StopQuery& query,
                                     std::time_t now,
                                     DepartModel::Entry::Batch& batch,
                                     String& firstStopName) {
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
    return true;
  }

  if (batch.apiTs == 0 || apiTs > batch.apiTs) batch.apiTs = apiTs;
  if (batch.items.capacity() < batch.items.size() + visits.size()) {
    batch.items.reserve(batch.items.size() + visits.size());
  }

  int totalVisits = 0, hadTime = 0, parsedTime = 0;
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

    const char* expectedStr = call["ExpectedDepartureTime"] | call["ExpectedArrivalTime"] | (const char*)nullptr;
    const char* aimedStr = call["AimedDepartureTime"] | call["AimedArrivalTime"] | (const char*)nullptr;
    bool hasTime = false;
    time_t expectedUtc = 0;
    time_t aimedUtc = 0;
    if (expectedStr && *expectedStr) {
      hasTime = true;
      if (!parseRFC3339ToUTC(expectedStr, expectedUtc) && parsedTime < 3) {
        DEBUG_PRINTF("DepartStrip: SiriSource::fetch: failed to parse expected time '%s'\n", expectedStr);
      }
    }
    if (aimedStr && *aimedStr) {
      hasTime = true;
      if (!parseRFC3339ToUTC(aimedStr, aimedUtc) && parsedTime < 3) {
        DEBUG_PRINTF("DepartStrip: SiriSource::fetch: failed to parse aimed time '%s'\n", aimedStr);
      }
    }
    if (hasTime) ++hadTime;
    time_t depUtc = expectedUtc ? expectedUtc : aimedUtc;
    if (depUtc) {
      ++parsedTime;
      DepartModel::Entry::Item item;
      item.estDep = depUtc;
      // LineRef may be nested or plain; handle both
      String label;
      if (mvj["LineRef"].is<JsonObject>()) {
        const char* raw = mvj["LineRef"]["value"] | "";
        if (raw && *raw) label = raw;
      } else {
        const char* raw = mvj["LineRef"] | "";
        if (raw && *raw) label = raw;
      }
      if (label.length() == 0) {
        label = jsonFirstString(mvj["PublishedLineName"]);
      }
      String formatted = departstrip::util::formatLineLabel(agency_, label);
      item.lineRef = std::move(formatted);
      if (query.labelSuffix.length() > 0) {
        item.lineRef += '-';
        item.lineRef += query.labelSuffix;
      }
      batch.items.push_back(std::move(item));
    } else if (hasTime && parsedTime < 3) {
      const char* dbg = expectedStr ? expectedStr : aimedStr;
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: failed to derive departure time from '%s'\n", dbg ? dbg : "<null>");
    }
  }
  DEBUG_PRINTF("DepartStrip: SiriSource::fetch: visits=%d hadTime=%d parsed=%d items=%u\n", totalVisits, hadTime, parsedTime, (unsigned)batch.items.size());

  if (firstStopName.length() > 0) lastStopName_ = firstStopName;
  return true;
}

std::unique_ptr<DepartModel> SiriSource::fetch(std::time_t now) {
  if (!enabled_) return nullptr;
  if (now == 0) return nullptr;
  if (queries_.empty()) return nullptr;
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

  size_t desiredHint = lastJsonCapacity_;
  bool haveAnyItems = false;
  std::unique_ptr<DepartModel> model(new DepartModel());
  model->boards.reserve(queries_.size());

  for (size_t qi = 0; qi < queries_.size(); ++qi) {
    DepartModel::Entry::Batch batch;
    batch.apiTs = 0;
    batch.ourTs = now;
    String stopName;

    const StopQuery& query = queries_[qi];
    String url = composeUrl(agency_, query);
    // Redact API key in debug logs
    String redacted = url;
    auto redactParam = [&](const __FlashStringHelper* kfs) {
      String k(kfs);
      int p = redacted.indexOf(k);
      if (p >= 0) {
        int valStart = p + k.length();
        int valEnd = redacted.indexOf('&', valStart);
        if (valEnd < 0) valEnd = redacted.length();
        String pre = redacted.substring(0, valStart);
        String post = redacted.substring(valEnd);
        redacted = pre + F("REDACTED") + post;
      }
    };
    redactParam(F("api_key="));
    redactParam(F("apikey="));
    redactParam(F("key="));
    if (queries_.size() > 1) {
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: URL[%u/%u %s]: %s\n",
                   (unsigned)(qi + 1),
                   (unsigned)queries_.size(),
                   query.canonical.c_str(),
                   redacted.c_str());
    } else {
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: URL: %s\n", redacted.c_str());
    }

    int len = 0;
    if (!httpBegin(url, len)) {
      long delay = (long)updateSecs_ * (long)backoffMult_;
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: scheduling backoff x%u %s for %lds (begin/HTTP error)\n",
                   (unsigned)backoffMult_, sourceKey().c_str(), delay);
      nextFetch_ = now + delay;
      if (backoffMult_ < 16) backoffMult_ *= 2;
      return nullptr;
    }

    size_t jsonSz = computeJsonCapacity(len);
    if (jsonSz > desiredHint) desiredHint = jsonSz;
    DEBUG_PRINTF("DepartStrip: SiriSource::fetch: json capacity=%u (hint=%u) free heap=%u\n",
                 (unsigned)jsonSz, (unsigned)lastJsonCapacity_, ESP.getFreeHeap());

    bool fromPool = false;
    JsonDocument* docPtr = acquireJsonDoc(jsonSz, fromPool);
    if (!docPtr) {
      DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: failed to acquire JSON buffer"));
      endHttp();
      long delay = (long)updateSecs_ * (long)backoffMult_;
      nextFetch_ = now + delay;
      if (backoffMult_ < 16) backoffMult_ *= 2;
      return nullptr;
    }
    JsonDocument& doc = *docPtr;
    DEBUG_PRINTF("DepartStrip: SiriSource::fetch: filter=on (len=%d)\n", len);
    String respTransfer = http_.header("Transfer-Encoding");
    bool isChunked = respTransfer.length() > 0 && respTransfer.equalsIgnoreCase("chunked");
#if defined(WLED_DEBUG)
    String respEnc = http_.header("Content-Encoding");
    String respType = http_.header("Content-Type");
    String respContentLen = http_.header("Content-Length");
    char sniffBuf[SIRI_SNIFF_PREFIX] = {0};
    size_t sniffLen = 0;
    bool sniffTruncated = false;
    bool parsed = parseJsonFromHttp(doc, isChunked, sniffBuf, sizeof(sniffBuf), &sniffLen, &sniffTruncated);
#else
    bool parsed = parseJsonFromHttp(doc, isChunked);
#endif
    endHttp();
    if (!parsed) {
      if (lastJsonCapacity_ < SIRI_JSON_MAX_CAP) {
        size_t bumped = jsonSz + SIRI_JSON_QUANTUM;
        if (bumped > SIRI_JSON_MAX_CAP) bumped = SIRI_JSON_MAX_CAP;
        if (bumped > lastJsonCapacity_) lastJsonCapacity_ = bumped;
      }
      releaseJsonDoc(docPtr, fromPool);
#if defined(WLED_DEBUG)
      debugLogPayloadPrefix(sniffBuf, sniffLen, sniffTruncated);
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: response headers: enc='%s' type='%s' len=%d contentLength='%s' transfer='%s'\n",
                   respEnc.c_str(), respType.c_str(), len,
                   respContentLen.c_str(), respTransfer.c_str());
#endif
      long delay = (long)updateSecs_ * (long)backoffMult_;
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: scheduling backoff x%u %s for %lds (parse error)\n",
                   (unsigned)backoffMult_, sourceKey().c_str(), delay);
      nextFetch_ = now + delay;
      if (backoffMult_ < 16) backoffMult_ *= 2;
      return nullptr;
    }

    bool usedTop = false;
    JsonObject siri = getSiriRoot(doc, usedTop);
    if (usedTop) {
      DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: using top-level as Siri container"));
    }
    {
      size_t used = doc.memoryUsage();
      size_t desired = used + 2048;
      if (desired < SIRI_JSON_MIN_CAP) desired = SIRI_JSON_MIN_CAP;
      desired = ((desired + SIRI_JSON_QUANTUM - 1) / SIRI_JSON_QUANTUM) * SIRI_JSON_QUANTUM;
      if (desired > SIRI_JSON_MAX_CAP) desired = SIRI_JSON_MAX_CAP;
      if (desired > desiredHint) desiredHint = desired;
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: json usage=%u nextHint=%u\n",
                   (unsigned)used, (unsigned)desiredHint);
    }
    if (siri.isNull()) {
      releaseJsonDoc(docPtr, fromPool);
#if defined(WLED_DEBUG)
      debugLogPayloadPrefix(sniffBuf, sniffLen, sniffTruncated);
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: response headers: enc='%s' type='%s' len=%d contentLength='%s' transfer='%s'\n",
                   respEnc.c_str(), respType.c_str(), len,
                   respContentLen.c_str(), respTransfer.c_str());
#endif
      DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: missing Siri root"));
      long delay = (long)updateSecs_ * (long)backoffMult_;
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: scheduling backoff x%u %s for %lds (bad JSON shape)\n",
                   (unsigned)backoffMult_, sourceKey().c_str(), delay);
      nextFetch_ = now + delay;
      if (backoffMult_ < 16) backoffMult_ *= 2;
      return nullptr;
    }

    bool parsedOk = appendItemsFromSiri(siri, query, now, batch, stopName);
    releaseJsonDoc(docPtr, fromPool);
    if (!parsedOk) {
      long delay = (long)updateSecs_ * (long)backoffMult_;
      DEBUG_PRINTF("DepartStrip: SiriSource::fetch: scheduling backoff x%u %s for %lds (append failure)\n",
                   (unsigned)backoffMult_, sourceKey().c_str(), delay);
      nextFetch_ = now + delay;
      if (backoffMult_ < 16) backoffMult_ *= 2;
      return nullptr;
    }
    if (!batch.items.empty()) {
      std::sort(batch.items.begin(), batch.items.end(),
                [](const DepartModel::Entry::Item& a, const DepartModel::Entry::Item& b) {
                  return a.estDep < b.estDep;
                });
      if (batch.apiTs == 0) batch.apiTs = now;
      DepartModel::Entry board;
      board.key.reserve(agency_.length() + 1 + query.canonical.length());
      board.key = agency_;
      board.key += ':';
      board.key += query.canonical;
      board.batch = std::move(batch);
      model->boards.push_back(std::move(board));
      haveAnyItems = true;
    }
  }

  lastJsonCapacity_ = desiredHint;
  nextFetch_ = now + updateSecs_;
  backoffMult_ = 1;

  if (!haveAnyItems || model->boards.empty()) {
    DEBUG_PRINTLN(F("DepartStrip: SiriSource::fetch: no departures after filters"));
    return nullptr;
  }

  return model;
}

void SiriSource::endHttp() {
  if (!httpActive_) return;
  http_.end();
  httpTransport_.end(httpUsedSecure_);
  httpActive_ = false;
}

void SiriSource::addToConfig(JsonObject& root) {
  root["Enabled"] = enabled_;
  root["Type"] = F("siri");
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
  std::vector<StopQuery> prevQueries = queries_;

  queries_.clear();

  ok &= getJsonValue(root["Enabled"], enabled_, enabled_);
  ok &= getJsonValue(root["UpdateSecs"], updateSecs_, updateSecs_);
  ok &= getJsonValue(root["TemplateUrl"], baseUrl_, baseUrl_);
  ok &= getJsonValue(root["ApiKey"], apiKey_, apiKey_);
  // Prefer combined Key ("AGENCY:StopCode"); fall back to legacy Agency/StopCode if not provided
  String keyStr;
  bool haveKey = getJsonValue(root["AgencyStopCode"], keyStr, (const char*)nullptr);
  if (!haveKey) haveKey = getJsonValue(root["Key"], keyStr, (const char*)nullptr);
  String list;
  if (haveKey && keyStr.length() > 0) {
    keyStr.trim();
    int colon = keyStr.indexOf(':');
    if (colon > 0) {
      agency_ = keyStr.substring(0, colon);
      list = keyStr.substring(colon + 1);
    } else {
      list = keyStr;
    }
  } else {
    ok &= getJsonValue(root["Agency"], agency_, agency_);
    String stopField;
    if (getJsonValue(root["StopCode"], stopField, (const char*)nullptr)) {
      list = stopField;
    } else {
      list = stopCode_;
    }
  }
  list.trim();

  std::vector<String> tokens;
  if (list.length() > 0) splitStopCodes(list, tokens);
  std::vector<StopQuery> parsed;
  parsed.reserve(tokens.size());
  String inferredAgency = agency_;
  for (const auto& tok : tokens) {
    ParsedStopQuery pq;
    String explicitAgency;
    if (!parseStopExpression(tok, pq, explicitAgency)) continue;
    if (explicitAgency.length() > 0) inferredAgency = explicitAgency;
    StopQuery q;
    q.canonical = pq.canonical;
    q.monitoringRef = pq.monitoringRef;
    q.destinationRef = pq.destinationRef;
    q.notViaRef = pq.notViaRef;
    q.labelSuffix = pq.labelSuffix;
    parsed.push_back(q);
  }
  if (agency_.length() == 0 && inferredAgency.length() > 0) agency_ = inferredAgency;

  if (!parsed.empty()) {
    queries_.reserve(parsed.size());
    for (const auto& cand : parsed) {
      bool dup = false;
      for (const auto& existing : queries_) {
        if (existing.canonical.equalsIgnoreCase(cand.canonical)) {
          dup = true;
          break;
        }
      }
      if (!dup) queries_.push_back(cand);
    }
  }

  if (queries_.empty()) {
    String legacy = list;
    if (legacy.length() == 0 && prevStop.length() > 0) legacy = prevStop;
    legacy.trim();
    if (legacy.length() == 0 && !prevQueries.empty()) {
      legacy = prevQueries.front().monitoringRef;
      legacy.trim();
    }
    legacy.trim();
    if (legacy.length() > 0) {
      StopQuery q;
      q.canonical = legacy;
      q.monitoringRef = legacy;
      queries_.push_back(std::move(q));
    }
  }

  stopCode_.clear();
  for (size_t i = 0; i < queries_.size(); ++i) {
    if (i > 0) stopCode_ += ',';
    stopCode_ += queries_[i].canonical;
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
