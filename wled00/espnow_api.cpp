#include "wled.h"
#if !defined(WLED_DISABLE_ESPNOW) && defined(ARDUINO_ARCH_ESP32) && defined(WLED_ENABLE_ESPNOW_API)

// Bidirectional JSON transport for linked ESP-NOW remotes. Frames are fragmented to fit
// the 250-byte ESP-NOW payload limit
// Received frames are reassembled in loop context before touching JSON, FS or LED state.

#define ESPNOW_LIVE_INTERVAL 100            // ESP-NOW live peek cadence (ms), bounded to avoid radio saturation
#define ESPNOW_LIVE_TIMEOUT 30000           // stop live peek if {"lv":true} is not re-armed within this window
#define ESPNOW_API_PRESENCE_TIMEOUT 120000  // push state only while an API remote has been seen this recently
#define ESPNOW_API_DEDUPE_TIMEOUT 10000      // exceeds the bounded retry horizon without spanning normal msgId wrap

#define ESPNOW_API_CAP_COMPACT 0x01
#define ESPNOW_API_CAP_CATALOGS 0x02
#define ESPNOW_API_CAP_PUSH     0x04
#define ESPNOW_API_CAP_LIVE     0x08

// Wire error codes.
#define ESPNOW_API_ERR_BUSY   3             // transient (JSON buffer or TX slot busy, low heap) - retry
#define ESPNOW_API_ERR_SIZE   8             // response exceeds ESPNOW_API_MAX_JSON - do not retry
#define ESPNOW_API_ERR_JSON   9             // request failed to parse
#define ESPNOW_API_ERR_GET   10             // unknown {"get":...} catalog key

#if ESPNOW_API_MAX_FRAGS <= 16
typedef uint16_t espnow_frag_mask_t;        // ESP8266: 9 fragments max, avoid 64-bit shifts in the RX callback
#else
typedef uint64_t espnow_frag_mask_t;
#endif

struct EspNowApiInbox {
  uint8_t  srcMac[6];
  uint8_t  msgId;
  uint8_t* json;          // NUL-terminated heap buffer; ownership passes to the loop
  size_t   len;
};
// Two slots absorb a request arriving while the main loop is finishing the previous one.
// The remote still retries on timeout because ESP-NOW itself is best-effort.
static EspNowApiInbox apiInbox[2] = {};
static uint8_t apiInboxRead = 0;
static uint8_t apiInboxWrite = 0;
static uint8_t apiInboxCount = 0;

// One in-progress fragmented request. Completed buffers move to apiInbox.
static uint8_t*           apiReasmBuf   = nullptr;
static uint8_t            apiReasmSrc[6]= {0};
static uint8_t            apiReasmId    = 0;
static uint8_t            apiReasmTotal = 0;
static uint8_t            apiReasmCount = 0;
static espnow_frag_mask_t apiReasmFlags = 0;   // received-fragment bitmask
static size_t             apiReasmLen   = 0;
static unsigned long      apiReasmLast  = 0;
static unsigned long apiRemoteSeen = 0; // last API frame; gates state pushes/reconnect deferral

// Live preview is best-effort and tracks one subscriber at a time.
static bool          apiLiveActive  = false;
static uint8_t       apiLiveMac[6]  = {0};
static uint8_t       apiLiveMsgId   = 0;
static uint8_t       apiLiveSendFailures = 0;
static unsigned long apiLastLiveTime = 0;
static unsigned long apiLiveExpiry  = 0;       // live peek is a keepalive (no disconnect signal over ESP-NOW)

// Pushes are coalesced because only the newest state matters.
static bool apiPushPending = false;             // coalesced state push waiting for the reliable TX slot
static unsigned long apiPushDue = 0;            // MAC-derived jitter avoids simultaneous multi-WLED broadcasts
struct EspNowApiDiscoveryReply {
  uint8_t mac[6];
  uint8_t msgId;
  unsigned long due;
  bool pending;
};
// Two slots allow two whitelisted remotes to discover concurrently without one replacing the other.
static EspNowApiDiscoveryReply apiDiscoveryReplies[2] = {};

struct EspNowApiCompletedMutation {
  uint8_t mac[6];
  uint8_t msgId;
  uint32_t hash;
  unsigned long completedAt;
};
// Retaining a few completed mutations makes retries idempotent even when their first response
// was lost after WLED had already applied the state change.
static EspNowApiCompletedMutation apiCompletedMutations[4] = {};
static uint8_t apiCompletedMutationNext = 0;

// Single pending outbound message, drained incrementally by serviceEspNowApiTx().
struct EspNowApiTx {
  uint8_t  mac[6];
  uint8_t  msgType;
  uint8_t  msgId;
  uint8_t* payload;       // heap buffer, owned; nullptr = slot idle
  size_t   len;
  uint8_t  fragTotal;
  uint8_t  fragNext;
};
static EspNowApiTx apiTx = {{0}, 0, 0, nullptr, 0, 0, 0};

// Drop the partial message and release its buffer.
static void apiReasmReset() {
  if (apiReasmBuf) { free(apiReasmBuf); apiReasmBuf = nullptr; }
  apiReasmTotal = apiReasmCount = 0;
  apiReasmFlags = 0;
  apiReasmLen = 0;
}

#ifdef WLED_DEBUG
static const char* apiTypeName(uint8_t type) {
  switch (type) {
    case ESPNOW_API_REQUEST:  return "REQUEST";
    case ESPNOW_API_RESPONSE: return "RESPONSE";
    case ESPNOW_API_PUSH:     return "PUSH";
    case ESPNOW_API_DISCOVER: return "DISCOVER";
    case ESPNOW_API_LIVE:     return "LIVE";
    case ESPNOW_API_ANNOUNCE: return "ANNOUNCE";
    default:                  return "UNKNOWN";
  }
}
#endif

// Release completed requests that have not yet reached the main handler.
static void apiInboxReset() {
  for (auto &inbox : apiInbox) {
    if (inbox.json) { free(inbox.json); inbox.json = nullptr; }
    inbox.len = 0;
  }
  apiInboxRead = apiInboxWrite = apiInboxCount = 0;
}

// Clear the subscriber and its delivery health counters.
static void apiLiveReset() {
  apiLiveActive = false;
  apiLiveMsgId = 0;
  apiLiveSendFailures = 0;
  apiLastLiveTime = 0;
  apiLiveExpiry = 0;
}

// Returns a stable per-instance delay so several WLEDs do not answer one broadcast in lockstep.
static uint16_t apiInstanceJitter(uint16_t window, uint16_t minimum = 0) {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  const uint16_t hash = (uint16_t(mac[3]) << 8) ^ (uint16_t(mac[4]) << 4) ^ mac[5];
  return minimum + (window ? hash % window : 0);
}

static uint32_t apiPayloadHash(const uint8_t* data, size_t len) {
  // This only distinguishes retries; it is not used for authentication!!
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < len; i++) hash = (hash ^ data[i]) * 16777619UL;
  return hash;
}

// Match a retry against a recently applied state-changing request.
static bool apiMutationWasCompleted(const uint8_t* mac, uint8_t msgId, uint32_t hash) {
  const unsigned long now = millis();
  for (const auto &record : apiCompletedMutations) {
    if (!record.completedAt || now - record.completedAt > ESPNOW_API_DEDUPE_TIMEOUT) continue;
    if (record.msgId == msgId && record.hash == hash && memcmp(record.mac, mac, sizeof(record.mac)) == 0) return true;
  }
  return false;
}

// Store mutations in a small ring so old entries naturally cycle out.
static void rememberCompletedMutation(const uint8_t* mac, uint8_t msgId, uint32_t hash) {
  EspNowApiCompletedMutation &record = apiCompletedMutations[apiCompletedMutationNext];
  memcpy(record.mac, mac, sizeof(record.mac));
  record.msgId = msgId;
  record.hash = hash;
  const unsigned long now = millis();
  record.completedAt = now ? now : 1;
  apiCompletedMutationNext = (apiCompletedMutationNext + 1) % (sizeof(apiCompletedMutations) / sizeof(apiCompletedMutations[0]));
}

// Stop a stale live stream after repeated MAC-level failures; a refresh request restarts it.
void espNowApiOnSendResult(uint8_t* address, uint8_t status) {
  if (!apiLiveActive || !address || memcmp(address, apiLiveMac, sizeof(apiLiveMac)) != 0) return;
  if (!status) {
    apiLiveSendFailures = 0;
    return;
  }
  if (++apiLiveSendFailures >= 3) {
    DEBUG_PRINTLN(F("ESP-NOW API stopping live stream after 3 send failures."));
    apiLiveReset();
  }
}

// Release the current outbound payload and mark the single TX slot idle.
static void apiTxReset() {
  if (apiTx.payload) { free(apiTx.payload); apiTx.payload = nullptr; }
}

// Reassembly can expire even when no further API packets arrive.
static void apiReasmCleanupStale() {
  if (apiReasmBuf && millis() - apiReasmLast > ESPNOW_API_REASM_TIMEOUT) apiReasmReset();
}

static void scheduleEspNowAnnounce(const uint8_t* mac, uint8_t msgId);

bool espNowApiReady() {
  return enableESPNow && statusESPNow == ESP_NOW_STATE_ON;
}

// Recent traffic keeps pushes and the fallback AP useful to a companion remote.
bool espNowApiRemoteActive() {
  const unsigned long seen = apiRemoteSeen;
  return seen && millis() - seen < ESPNOW_API_PRESENCE_TIMEOUT;
}

// AI: below section was partly generated by an AI
// Reassemble a frame delivered by the native transport in loop context.
void handleEspNowApiData(uint8_t* address, uint8_t* data, uint8_t len) {
  if (len < ESPNOW_API_HEADER_SIZE) return;
  const uint8_t msgType   = data[2];
  const uint8_t msgId     = data[3];
  const uint8_t fragIndex = data[4];
  const uint8_t fragTotal = data[5];
  const uint8_t payloadLen = len - ESPNOW_API_HEADER_SIZE;

  // Check untrusted header values before indexing or allocating.
  if (msgType != ESPNOW_API_REQUEST && msgType != ESPNOW_API_DISCOVER) return; // inbound direction only
  if (fragTotal < 1 || fragTotal > ESPNOW_API_MAX_FRAGS) return;
  if (fragIndex >= fragTotal) return;
  if (payloadLen > ESPNOW_API_FRAG_SIZE) return;
  if (fragIndex < fragTotal - 1 && payloadLen != ESPNOW_API_FRAG_SIZE) return; // non-final fragments are full so offsets align
  if (msgType == ESPNOW_API_DISCOVER &&
      (fragIndex != 0 || fragTotal != 1 ||
       (payloadLen != 0 && (payloadLen != 2 || data[ESPNOW_API_HEADER_SIZE] != '{' ||
                            data[ESPNOW_API_HEADER_SIZE + 1] != '}')))) return;

  unsigned long now = millis();
  apiRemoteSeen = now ? now : 1;
  // DISCOVER is already fully validated and carries no useful body. Scheduling it directly
  // avoids periodic heap allocation/reassembly churn on constrained WLED targets.
  if (msgType == ESPNOW_API_DISCOVER) {
    scheduleEspNowAnnounce(address, msgId);
    return;
  }
  bool newMsg = (apiReasmBuf == nullptr) || (now - apiReasmLast > ESPNOW_API_REASM_TIMEOUT) ||
                (memcmp(apiReasmSrc, address, 6) != 0) || (apiReasmId != msgId) ||
                (apiReasmTotal != fragTotal);
  if (newMsg) {
    // Only one fragmented request is assembled at a time; remotes retry anything displaced here.
    if (apiReasmBuf) {
      DEBUG_PRINTF_P(PSTR("ESP-NOW API RX replacing incomplete %s id=%u fragments=%u/%u age=%lums\n"),
                     apiTypeName(ESPNOW_API_REQUEST), apiReasmId,
                     apiReasmCount, apiReasmTotal, now - apiReasmLast);
    }
    apiReasmReset();
    if (fragIndex != 0) {
      DEBUG_PRINTF_P(PSTR("ESP-NOW API RX dropped orphan %s id=%u fragment=%u/%u\n"),
                     apiTypeName(msgType), msgId, fragIndex + 1, fragTotal);
      return;
    }
    apiReasmBuf = (uint8_t*)d_malloc((size_t)fragTotal * ESPNOW_API_FRAG_SIZE + 1);
    if (!apiReasmBuf) return;
    memcpy(apiReasmSrc, address, 6);
    apiReasmId    = msgId;
    apiReasmTotal = fragTotal;
  }
  const espnow_frag_mask_t bit = (espnow_frag_mask_t)1 << fragIndex;
  if (apiReasmFlags & bit) return; // duplicate
  apiReasmLast = now;
  memcpy(apiReasmBuf + (size_t)fragIndex * ESPNOW_API_FRAG_SIZE, data + ESPNOW_API_HEADER_SIZE, payloadLen);
  apiReasmFlags |= bit;
  apiReasmCount++;
  if (fragIndex == fragTotal - 1) apiReasmLen = (size_t)fragIndex * ESPNOW_API_FRAG_SIZE + payloadLen;

  if (apiReasmCount < fragTotal) return;
  if (apiInboxCount >= sizeof(apiInbox) / sizeof(apiInbox[0])) {
    DEBUG_PRINTF_P(PSTR("ESP-NOW API RX inbox full; dropped %s id=%u\n"),
                   apiTypeName(msgType), msgId);
    apiReasmReset();
    return;
  }
  if (apiReasmLen > ESPNOW_API_MAX_JSON) { apiReasmReset(); return; }
  // Hand the completed allocation to the inbox without copying it again.
  apiReasmBuf[apiReasmLen] = '\0';
  EspNowApiInbox &inbox = apiInbox[apiInboxWrite];
  inbox.json    = apiReasmBuf;
  inbox.len     = apiReasmLen;
  inbox.msgId   = apiReasmId;
  memcpy(inbox.srcMac, apiReasmSrc, 6);
  apiInboxWrite = (apiInboxWrite + 1) % (sizeof(apiInbox) / sizeof(apiInbox[0]));
  apiInboxCount++;
  DEBUG_PRINTF_P(PSTR("ESP-NOW API RX complete %s id=%u bytes=%u fragments=%u inbox=%u\n"),
                 apiTypeName(msgType), msgId,
                 unsigned(apiReasmLen), fragTotal, apiInboxCount);
  apiReasmBuf = nullptr; // ownership moved to the inbox; reset clears the remaining state
  apiReasmReset();
}
// AI: end

static bool apiTxIdle() { return apiTx.payload == nullptr; }

// Send the next pending fragment; the transport admits another only after completion.
static void serviceEspNowApiTx() {
  if (apiTxIdle()) return;
  if (statusESPNow != ESP_NOW_STATE_ON) { apiTxReset(); return; }
  if (!espNowTransportReadyToSend()) return;
  uint8_t frame[ESPNOW_API_HEADER_SIZE + ESPNOW_API_FRAG_SIZE];
  frame[0] = ESPNOW_API_MAGIC;
  frame[1] = ESPNOW_API_VERSION;
  frame[2] = apiTx.msgType;
  frame[3] = apiTx.msgId;
  frame[5] = apiTx.fragTotal;
  size_t off = (size_t)apiTx.fragNext * ESPNOW_API_FRAG_SIZE;
  size_t chunk = apiTx.len - off;
  if (chunk > ESPNOW_API_FRAG_SIZE) chunk = ESPNOW_API_FRAG_SIZE;
  frame[4] = apiTx.fragNext;
  memcpy(frame + ESPNOW_API_HEADER_SIZE, apiTx.payload + off, chunk);
  // Submission errors abort this message; link failures arrive later via the send callback.
  if (espNowTransportSend(apiTx.mac, frame, ESPNOW_API_HEADER_SIZE + chunk)) { apiTxReset(); return; }
  if (++apiTx.fragNext >= apiTx.fragTotal) apiTxReset();
}

// Queue a payload for transmission; takes ownership of the heap buffer on success.
// A pending PUSH or LIVE frame is droppable and is preempted; anything else keeps the slot.
static bool apiTxEnqueue(const uint8_t* mac, uint8_t msgType, uint8_t msgId, uint8_t* payload, size_t len) {
  if (statusESPNow != ESP_NOW_STATE_ON || !mac || !payload || !len) return false;
  size_t total = (len + ESPNOW_API_FRAG_SIZE - 1) / ESPNOW_API_FRAG_SIZE;
  if (total > ESPNOW_API_MAX_FRAGS) return false;
  if (!apiTxIdle()) {
    if (apiTx.msgType == ESPNOW_API_PUSH || apiTx.msgType == ESPNOW_API_LIVE) apiTxReset();
    else return false;
  }
  memcpy(apiTx.mac, mac, 6);
  apiTx.msgType   = msgType;
  apiTx.msgId     = msgId;
  apiTx.payload   = payload;
  apiTx.len       = len;
  apiTx.fragTotal = (uint8_t)total;
  apiTx.fragNext  = 0;
  if (msgType != ESPNOW_API_LIVE) {
    DEBUG_PRINTF_P(PSTR("ESP-NOW API TX queued %s id=%u bytes=%u fragments=%u to " MACSTR " ch=%u\n"),
                   apiTypeName(msgType), msgId, unsigned(len),
                   unsigned(total), MAC2STR(mac), WiFi.channel());
  }
  serviceEspNowApiTx();
  return true;
}

// Copy short stack-backed JSON into the owned TX buffer.
static bool sendEspNowApiJson(const uint8_t* mac, uint8_t msgType, uint8_t msgId, const char* json, size_t jsonLen) {
  uint8_t* buf = (uint8_t*)d_malloc(jsonLen);
  if (!buf) return false;
  memcpy(buf, json, jsonLen);
  if (!apiTxEnqueue(mac, msgType, msgId, buf, jsonLen)) { free(buf); return false; }
  return true;
}

static void sendEspNowApiError(const uint8_t* mac, uint8_t msgId, uint8_t code) {
  char buf[16];
  int len = sprintf_P(buf, PSTR("{\"error\":%u}"), code);
  sendEspNowApiJson(mac, ESPNOW_API_RESPONSE, msgId, buf, len);
}

static void sendEspNowApiSuccess(const uint8_t* mac, uint8_t msgId) {
  static const char response[] = "{\"success\":true}";
  sendEspNowApiJson(mac, ESPNOW_API_RESPONSE, msgId, response, sizeof(response) - 1);
}

// Serialize the prepared pDoc and queue it. Caller holds JSON_LOCK_REMOTE; this releases it.
// Returns 0 on success or the API error code to report.
static uint8_t queueApiDocLocked(const uint8_t* mac, uint8_t msgType, uint8_t msgId) {
  size_t len = measureJson(*pDoc);
  if (len == 0 || len > ESPNOW_API_MAX_JSON) { releaseJSONBufferLock(); return ESPNOW_API_ERR_SIZE; }
  uint8_t* buf = (uint8_t*)d_malloc(len + 1);
  if (!buf) { releaseJSONBufferLock(); return ESPNOW_API_ERR_BUSY; }
  serializeJson(*pDoc, (char*)buf, len + 1);
  releaseJSONBufferLock();
  if (!apiTxEnqueue(mac, msgType, msgId, buf, len)) { free(buf); return ESPNOW_API_ERR_BUSY; }
  return 0;
}

// Build the full WebSocket-compatible state and info response.
static uint8_t queueEspNowApiState(const uint8_t* mac, uint8_t msgType, uint8_t msgId) {
  if (statusESPNow != ESP_NOW_STATE_ON) return ESPNOW_API_ERR_BUSY;
  if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) return ESPNOW_API_ERR_BUSY;
  pDoc->clear();
  JsonObject state = pDoc->createNestedObject("state");
  serializeState(state);
  JsonObject info  = pDoc->createNestedObject("info");
  serializeInfo(info);
  return queueApiDocLocked(mac, msgType, msgId);
}

// Serialize only the fields used by companion remotes so routine state fits in one frame.
static uint8_t queueEspNowApiCompactState(const uint8_t* mac, uint8_t msgType, uint8_t msgId) {
  if (statusESPNow != ESP_NOW_STATE_ON) return ESPNOW_API_ERR_BUSY;
  const uint8_t mainSegmentId = strip.getMainSegmentId();
  const Segment &mainseg = strip.getMainSegment();
  char json[176];
  int len = snprintf_P(json, sizeof(json), PSTR("{\"state\":{\"on\":%s,\"bri\":%u,\"ps\":%d,\"mainseg\":%u,\"seg\":[{\"id\":%u,\"fx\":%u,\"pal\":%u,\"sx\":%u,\"ix\":%u,\"c1\":%u,\"c2\":%u,\"c3\":%u,\"col\":[[%u,%u,%u]]}]}}"),
                       bri > 0 ? "true" : "false", unsigned(briLast), currentPreset > 0 ? int(currentPreset) : -1,
                       unsigned(mainSegmentId), unsigned(mainSegmentId),
                       unsigned(mainseg.mode), unsigned(mainseg.palette), unsigned(mainseg.speed), unsigned(mainseg.intensity),
                       unsigned(mainseg.custom1), unsigned(mainseg.custom2), unsigned(mainseg.custom3),
                       unsigned(R(mainseg.colors[0])), unsigned(G(mainseg.colors[0])), unsigned(B(mainseg.colors[0])));
  if (len <= 0 || len >= int(sizeof(json))) return ESPNOW_API_ERR_SIZE;
  return sendEspNowApiJson(mac, msgType, msgId, json, len) ? 0 : ESPNOW_API_ERR_BUSY;
}

// Choose between a small acknowledgement and a full state snapshot.
static void sendEspNowApiResponse(const uint8_t* mac, uint8_t msgId, bool verbose) {
  if (!verbose) { sendEspNowApiSuccess(mac, msgId); return; }
  uint8_t err = queueEspNowApiState(mac, ESPNOW_API_RESPONSE, msgId);
  if (err) sendEspNowApiError(mac, msgId, err);
}

// Reply to discovery by reliable unicast. The transport registers the already-whitelisted
// remote as a peer, and the echoed message ID binds this announcement to one discovery scan.
static bool sendEspNowAnnounce(const uint8_t* mac, uint8_t msgId) {
  if (statusESPNow != ESP_NOW_STATE_ON || !mac) return false;
  if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) return false;
  pDoc->clear();
  JsonObject announce = pDoc->createNestedObject("announce");
  announce[F("name")] = serverDescription;
  announce[F("mac")]  = escapedMac;
  announce[F("ver")]  = VERSION;
  announce[F("ch")]   = WiFi.channel();
  announce[F("proto")] = ESPNOW_API_VERSION;
  announce[F("cap")] = ESPNOW_API_CAP_COMPACT | ESPNOW_API_CAP_CATALOGS |
                         ESPNOW_API_CAP_PUSH | ESPNOW_API_CAP_LIVE;
  return queueApiDocLocked(mac, ESPNOW_API_ANNOUNCE, msgId) == 0;
}

// Answer a {"get":"fx|pal|ps"} catalog request so a remote can populate effect, palette and
// preset lists instead of hardcoding them. These mirror the WebUI's /json effects/palettes
// and presets.json. Every request is answered: with the catalog or with an error code.
static void sendEspNowApiCatalog(const uint8_t* mac, uint8_t msgId, const char* what) {
  if (statusESPNow != ESP_NOW_STATE_ON || !what) return;
  uint8_t err;

  if (!strcmp_P(what, PSTR("ps"))) {
    if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) { sendEspNowApiError(mac, msgId, ESPNOW_API_ERR_BUSY); return; }
    // one filtered pass over presets.json (names only) instead of a per-id file scan
    StaticJsonDocument<64> filter;
    filter["*"]["n"] = true;
    pDoc->clear();
    if (!readObjectFromFile(getPresetsFileName(), nullptr, pDoc, &filter)) pDoc->clear(); // no file = no presets
    std::vector<std::pair<uint8_t, String>> presets;
    for (JsonPair kv : pDoc->as<JsonObject>()) {
      int id = atoi(kv.key().c_str());
      const char* name = kv.value()["n"] | "";
      if (id >= 1 && id <= 250 && *name) presets.push_back({(uint8_t)id, String(name)});
    }
    // Preserve the names before reusing the shared document for the response.
    pDoc->clear();
    JsonObject ps = pDoc->createNestedObject("presets");
    for (auto& p : presets) ps[String(p.first)] = p.second;
    err = queueApiDocLocked(mac, ESPNOW_API_RESPONSE, msgId);
  } else if (!strcmp_P(what, PSTR("fx"))) {
    if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) { sendEspNowApiError(mac, msgId, ESPNOW_API_ERR_BUSY); return; }
    pDoc->clear();
    JsonArray effects = pDoc->createNestedArray("effects");
    serializeModeNames(effects);
    err = queueApiDocLocked(mac, ESPNOW_API_RESPONSE, msgId);
  } else if (!strcmp_P(what, PSTR("pal"))) {
    if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) { sendEspNowApiError(mac, msgId, ESPNOW_API_ERR_BUSY); return; }
    pDoc->clear();
    (*pDoc)[F("palettes")] = serialized((const __FlashStringHelper*)JSON_palette_names);
    err = queueApiDocLocked(mac, ESPNOW_API_RESPONSE, msgId);
  } else {
    sendEspNowApiError(mac, msgId, ESPNOW_API_ERR_GET);
    return;
  }
  if (err) sendEspNowApiError(mac, msgId, err);
}

// Binary live LED peek, sharing the payload builder (and format) of the WebSocket liveview.
static bool queueEspNowLiveLeds(const uint8_t* mac, uint8_t msgId) {
  if (statusESPNow != ESP_NOW_STATE_ON || !mac) return false;
  if (!apiTxIdle()) return false; // a live frame is stale the moment it waits; skip it
  const size_t MAX_LIVE_LEDS_ESPNOW = 256U;
  size_t bufSize = buildLiveLedsPayload(nullptr, 0, MAX_LIVE_LEDS_ESPNOW);
  if (!bufSize || bufSize > ESPNOW_API_MAX_JSON) return false;
  uint8_t* buffer = (uint8_t*)d_malloc(bufSize);
  if (!buffer) return false;
  if (!buildLiveLedsPayload(buffer, bufSize, MAX_LIVE_LEDS_ESPNOW) ||
      !apiTxEnqueue(mac, ESPNOW_API_LIVE, msgId, buffer, bufSize)) {
    free(buffer);
    return false;
  }
  return true;
}

// Send live frames at a fixed rate while the remote refreshes its "lease".
static void handleEspNowLive() {
  if (!apiLiveActive) return;
  if ((long)(millis() - apiLiveExpiry) >= 0) { apiLiveReset(); return; } // remote stopped re-arming
  if (millis() - apiLastLiveTime <= ESPNOW_LIVE_INTERVAL) return;
  bool success = queueEspNowLiveLeds(apiLiveMac, apiLiveMsgId++);
  apiLastLiveTime = millis();
  if (!success) apiLastLiveTime -= 20; // retry sooner if TX slot or heap was busy
}

// Release all API-owned state when ESP-NOW stops or changes interface.
static void apiResetAll() {
  apiReasmReset();
  apiInboxReset();
  apiLiveReset();
  apiTxReset();
  apiRemoteSeen = 0;
  apiPushPending = false;
  apiPushDue = 0;
  for (auto &reply : apiDiscoveryReplies) reply = EspNowApiDiscoveryReply{};
  for (auto &record : apiCompletedMutations) record = EspNowApiCompletedMutation{};
  apiCompletedMutationNext = 0;
}

// Retry a coalesced state push after responses have drained; live preview yields to state.
static bool handlePendingEspNowPush() {
  if (!apiPushPending || !apiTxIdle()) return false;
  if ((long)(millis() - apiPushDue) < 0) return false;
  if (!espNowApiReady() || linked_remotes.empty() || !espNowApiRemoteActive()) {
    apiPushPending = false;
    return false;
  }
  static uint8_t pushId = 0;
  if (queueEspNowApiCompactState(ESPNOW_BROADCAST_ADDRESS, ESPNOW_API_PUSH, pushId) != 0) return false;
  apiPushPending = false;
  pushId++;
  return true;
}

// Queue a discovery reply without letting simultaneous remotes overwrite each other.
static void scheduleEspNowAnnounce(const uint8_t* mac, uint8_t msgId) {
  EspNowApiDiscoveryReply* slot = nullptr;
  for (auto &reply : apiDiscoveryReplies) {
    if (reply.pending && memcmp(reply.mac, mac, sizeof(reply.mac)) == 0) { slot = &reply; break; }
    if (!reply.pending && !slot) slot = &reply;
  }
  if (!slot) slot = &apiDiscoveryReplies[0]; // bounded replacement; the remote repeats discovery
  memcpy(slot->mac, mac, sizeof(slot->mac));
  slot->msgId = msgId;
  slot->due = millis() + apiInstanceJitter(90, 10);
  slot->pending = true;
}

// Sends one due discovery response after the reliable response slot becomes available.
static bool handlePendingEspNowAnnounce() {
  if (!apiTxIdle()) return false;
  for (auto &reply : apiDiscoveryReplies) {
    if (!reply.pending || (long)(millis() - reply.due) < 0) continue;
    if (sendEspNowAnnounce(reply.mac, reply.msgId)) reply.pending = false;
    else reply.due = millis() + 20;
    return true;
  }
  return false;
}

// AI: below section was partly generated by an AI
// Apply a completed inbound message in loop context.
void handleEspNowApi() {
  static bool needCleanup = false;
  if (!espNowApiReady()) {
    if (needCleanup) { apiResetAll(); needCleanup = false; }
    return;
  }
  needCleanup = true;

  serviceEspNowApiTx();
  if (!apiTxIdle()) return; // finish the previous response before consuming another request

  uint8_t srcMac[6];
  uint8_t msgId = 0;
  uint8_t* json = nullptr;
  size_t jsonLen = 0;

  apiReasmCleanupStale();
  if (apiInboxCount) {
    EspNowApiInbox &inbox = apiInbox[apiInboxRead];
    memcpy(srcMac, inbox.srcMac, 6);
    msgId = inbox.msgId;
    json = inbox.json; // ownership moves to this invocation
    jsonLen = inbox.len;
    inbox.json = nullptr;
    inbox.len = 0;
    apiInboxRead = (apiInboxRead + 1) % (sizeof(apiInbox) / sizeof(apiInbox[0]));
    apiInboxCount--;
  }

  if (!json) {
    if (!handlePendingEspNowAnnounce() && !handlePendingEspNowPush()) handleEspNowLive();
    return;
  }

  unsigned long start = millis();
  const unsigned long stripWaitTimeout = strip.getFrameTime();
  // Avoid changing segment state mid-render, but never stall longer than one frame.
  while (strip.isUpdating() && millis() - start < stripWaitTimeout) yield();

  DEBUG_PRINTF_P(PSTR("ESP-NOW API handling REQUEST id=%u bytes=%u from " MACSTR "\n"),
                 msgId, unsigned(jsonLen), MAC2STR(srcMac));
  const uint32_t requestHash = apiPayloadHash(json, jsonLen);
  if (apiMutationWasCompleted(srcMac, msgId, requestHash)) {
    DEBUG_PRINTF_P(PSTR("ESP-NOW API suppressing duplicate mutation id=%u from " MACSTR "\n"),
                   msgId, MAC2STR(srcMac));
    sendEspNowApiSuccess(srcMac, msgId);
  } else if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) {
    sendEspNowApiError(srcMac, msgId, ESPNOW_API_ERR_BUSY);
  } else {
    DeserializationError err = deserializeJson(*pDoc, json, jsonLen);
    JsonObject root = pDoc->as<JsonObject>();
    if (err || root.isNull()) {
      releaseJSONBufferLock();
      sendEspNowApiError(srcMac, msgId, ESPNOW_API_ERR_JSON);
    } else {
      // Match wsEvent(): {"v":true} polls state, {"lv":...} toggles live peek.
      bool verbose = false;
      bool compact = false;
      const char* responseMode = root["v"].is<const char*>() ? root["v"].as<const char*>() : nullptr;
      // Treat these as polls only when no state-changing fields share the request.
      if (responseMode && !strcmp(responseMode, "compact") && root.size() == 1) {
        compact = true;
      } else if (root["v"] && root.size() == 1) {
        verbose = true;
      } else if (root.containsKey("lv")) {
        if (root["lv"] | false) {
          memcpy(apiLiveMac, srcMac, 6);
          apiLiveActive = true;
          apiLastLiveTime = 0;
          apiLiveExpiry = millis() + ESPNOW_LIVE_TIMEOUT;
        } else if (apiLiveActive && memcmp(apiLiveMac, srcMac, 6) == 0) {
          apiLiveReset();
        }
      } else if (root.containsKey("get")) {
        char what[8];
        strlcpy(what, root["get"] | "", sizeof(what));
        // Catalog generation needs pDoc, so release the parsing lock before dispatching it.
        releaseJSONBufferLock();
        sendEspNowApiCatalog(srcMac, msgId, what);
        free(json);
        return;
      } else {
        // Use button semantics so remote commands follow the normal notify path.
        verbose = deserializeState(root, CALL_MODE_BUTTON);
        rememberCompletedMutation(srcMac, msgId, requestHash);
      }
      releaseJSONBufferLock();
      // If the request changed state, a PUSH will follow soon. Acknowledge here
      // instead of serializing the same state twice.
      if (verbose && interfaceUpdateCallMode) verbose = false;
      if (compact) {
        uint8_t compactErr = queueEspNowApiCompactState(srcMac, ESPNOW_API_RESPONSE, msgId);
        if (compactErr) sendEspNowApiError(srcMac, msgId, compactErr);
      } else {
        sendEspNowApiResponse(srcMac, msgId, verbose);
      }
    }
  }

  free(json);
}
// AI: end

// Broadcast current state to paired remotes from updateInterfaces(), inheriting its cooldown.
// Gated on a recent API frame so WizMote-only setups never see these frames.
void pushEspNowState() {
  if (!espNowApiReady() || linked_remotes.empty() || !espNowApiRemoteActive()) return;
  apiPushPending = true; // coalesce rapid changes; handleEspNowApi() sends the latest state
  apiPushDue = millis() + apiInstanceJitter(35, 5);
}
#endif // !WLED_DISABLE_ESPNOW && ARDUINO_ARCH_ESP32 && WLED_ENABLE_ESPNOW_API
