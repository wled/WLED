#include "wled.h"
#ifndef WLED_DISABLE_ESPNOW
#include <atomic>

// Bidirectional JSON transport for linked ESP-NOW remotes. Frames are fragmented to fit
// the 250-byte ESP-NOW payload limit
// Received frames are reassembled in loop context before touching JSON, FS or LED state.

#define ESPNOW_API_STRIPWAIT_TIMEOUT 24     // one frame timeout to wait for the strip to finish updating
#define ESPNOW_LIVE_INTERVAL 40             // live peek cadence (ms), matching the WS liveview
#define ESPNOW_LIVE_TIMEOUT 3000            // stop live peek if {"lv":true} is not re-armed within this window
#define ESPNOW_API_PRESENCE_TIMEOUT 120000  // push state only while an API remote has been seen this recently
#define ESPNOW_API_TX_PER_LOOP 3            // max fragments transmitted per loop() pass (bounds loop stall)

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
  volatile bool ready;
  uint8_t  srcMac[6];
  uint8_t  msgType;
  uint8_t  msgId;
  uint8_t* json;          // NUL-terminated heap buffer; ownership passes to the loop
  size_t   len;
};
static EspNowApiInbox apiInbox = {false, {0}, 0, 0, nullptr, 0};

static uint8_t*           apiReasmBuf   = nullptr;
static uint8_t            apiReasmSrc[6]= {0};
static uint8_t            apiReasmId    = 0;
static uint8_t            apiReasmType  = 0;
static uint8_t            apiReasmTotal = 0;
static uint8_t            apiReasmCount = 0;
static espnow_frag_mask_t apiReasmFlags = 0;   // received-fragment bitmask
static size_t             apiReasmLen   = 0;
static unsigned long      apiReasmLast  = 0;
static unsigned long      apiRemoteSeen = 0;   // last time an API frame arrived; gates state pushes

static bool          apiLiveActive  = false;
static uint8_t       apiLiveMac[6]  = {0};
static uint8_t       apiLiveMsgId   = 0;
static unsigned long apiLastLiveTime = 0;
static unsigned long apiLiveExpiry  = 0;       // live peek is a keepalive (no disconnect signal over ESP-NOW)

// single pending outbound message, drained incrementally by serviceEspNowApiTx()
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

// The receive callback runs on a separate task; this try-lock guards the shared reassembly
// state. It never blocks: contention just drops a fragment, which the remote re-sends.
static std::atomic_flag apiStateLock = ATOMIC_FLAG_INIT;

struct EspNowApiStateGuard {
  bool locked;
  EspNowApiStateGuard() : locked(!apiStateLock.test_and_set(std::memory_order_acquire)) {}
  ~EspNowApiStateGuard() { if (locked) apiStateLock.clear(std::memory_order_release); }
  operator bool() const { return locked; }
};

static void apiReasmReset() {
  if (apiReasmBuf) { free(apiReasmBuf); apiReasmBuf = nullptr; }
  apiReasmTotal = apiReasmCount = 0;
  apiReasmFlags = 0;
  apiReasmLen = 0;
}

static void apiInboxReset() {
  if (apiInbox.json) { free(apiInbox.json); apiInbox.json = nullptr; }
  apiInbox.ready = false;
  apiInbox.len = 0;
}

static void apiLiveReset() {
  apiLiveActive = false;
  apiLiveMsgId = 0;
  apiLastLiveTime = 0;
  apiLiveExpiry = 0;
}

static void apiTxReset() {
  if (apiTx.payload) { free(apiTx.payload); apiTx.payload = nullptr; }
}

static void apiReasmCleanupStale() {
  if (apiReasmBuf && millis() - apiReasmLast > ESPNOW_API_REASM_TIMEOUT) apiReasmReset();
}

bool espNowApiReady() {
  return enableESPNow && statusESPNow == ESP_NOW_STATE_ON;
}

bool espNowApiRemoteActive() {
  return apiRemoteSeen && millis() - apiRemoteSeen < ESPNOW_API_PRESENCE_TIMEOUT;
}

// Reassemble an inbound frame in receive-task context: validated, bounded copies only.
void handleEspNowApiData(uint8_t* address, uint8_t* data, uint8_t len) {
  if (len < ESPNOW_API_HEADER_SIZE) return;
  const uint8_t msgType   = data[2];
  const uint8_t msgId     = data[3];
  const uint8_t fragIndex = data[4];
  const uint8_t fragTotal = data[5];
  const uint8_t payloadLen = len - ESPNOW_API_HEADER_SIZE;

  // reject untrusted header values before any indexing or allocation
  if (msgType != ESPNOW_API_REQUEST && msgType != ESPNOW_API_HELLO) return; // inbound direction only
  if (fragTotal < 1 || fragTotal > ESPNOW_API_MAX_FRAGS) return;
  if (fragIndex >= fragTotal) return;
  if (payloadLen > ESPNOW_API_FRAG_SIZE) return;
  if (fragIndex < fragTotal - 1 && payloadLen != ESPNOW_API_FRAG_SIZE) return; // non-final fragments are full so offsets align

  EspNowApiStateGuard guard;
  if (!guard) return;

  unsigned long now = millis();
  apiRemoteSeen = now;
  bool newMsg = (apiReasmBuf == nullptr) || (now - apiReasmLast > ESPNOW_API_REASM_TIMEOUT) ||
                (memcmp(apiReasmSrc, address, 6) != 0) || (apiReasmId != msgId) || (apiReasmTotal != fragTotal);
  if (newMsg) {
    apiReasmReset();
    if (fragIndex != 0) return;
    apiReasmBuf = (uint8_t*)d_malloc((size_t)fragTotal * ESPNOW_API_FRAG_SIZE + 1);
    if (!apiReasmBuf) return;
    memcpy(apiReasmSrc, address, 6);
    apiReasmId    = msgId;
    apiReasmType  = msgType;
    apiReasmTotal = fragTotal;
  }
  apiReasmLast = now;

  const espnow_frag_mask_t bit = (espnow_frag_mask_t)1 << fragIndex;
  if (apiReasmFlags & bit) return; // duplicate
  memcpy(apiReasmBuf + (size_t)fragIndex * ESPNOW_API_FRAG_SIZE, data + ESPNOW_API_HEADER_SIZE, payloadLen);
  apiReasmFlags |= bit;
  apiReasmCount++;
  if (fragIndex == fragTotal - 1) apiReasmLen = (size_t)fragIndex * ESPNOW_API_FRAG_SIZE + payloadLen;

  if (apiReasmCount < fragTotal) return;
  if (apiInbox.ready) { apiReasmReset(); return; } // loop hasn't drained the previous message; drop this one
  if (apiReasmLen > ESPNOW_API_MAX_JSON) { apiReasmReset(); return; }
  apiReasmBuf[apiReasmLen] = '\0';
  apiInbox.json    = apiReasmBuf;
  apiInbox.len     = apiReasmLen;
  apiInbox.msgType = apiReasmType;
  apiInbox.msgId   = apiReasmId;
  memcpy(apiInbox.srcMac, apiReasmSrc, 6);
  apiInbox.ready   = true;
  apiReasmBuf = nullptr; // ownership moved to the inbox; reset clears the remaining state
  apiReasmReset();
}

// ESP8266 QuickESPNow does not auto-register unicast peers (ESP32 does).
static void espNowEnsurePeer(const uint8_t* mac) {
#ifdef ESP8266
  if (memcmp(mac, ESPNOW_BROADCAST_ADDRESS, 6) == 0) return;
  if (!esp_now_is_peer_exist((uint8_t*)mac)) {
    esp_now_add_peer((uint8_t*)mac, ESP_NOW_ROLE_COMBO, 0, nullptr, 0); // channel 0 = current
  }
#else
  (void)mac;
#endif
}

static bool apiTxIdle() { return apiTx.payload == nullptr; }

// Send a few pending fragments, then return; called every loop pass and after each enqueue.
static void serviceEspNowApiTx() {
  if (apiTxIdle()) return;
  if (statusESPNow != ESP_NOW_STATE_ON) { apiTxReset(); return; }
  uint8_t frame[ESPNOW_API_HEADER_SIZE + ESPNOW_API_FRAG_SIZE];
  frame[0] = ESPNOW_API_MAGIC;
  frame[1] = ESPNOW_API_VERSION;
  frame[2] = apiTx.msgType;
  frame[3] = apiTx.msgId;
  frame[5] = apiTx.fragTotal;
  for (unsigned i = 0; i < ESPNOW_API_TX_PER_LOOP && !apiTxIdle(); i++) {
    if (!quickEspNow.readyToSendData()) return; // TX ring full; resume next loop pass
    size_t off = (size_t)apiTx.fragNext * ESPNOW_API_FRAG_SIZE;
    size_t chunk = apiTx.len - off;
    if (chunk > ESPNOW_API_FRAG_SIZE) chunk = ESPNOW_API_FRAG_SIZE;
    frame[4] = apiTx.fragNext;
    memcpy(frame + ESPNOW_API_HEADER_SIZE, apiTx.payload + off, chunk);
    if (quickEspNow.send(apiTx.mac, frame, ESPNOW_API_HEADER_SIZE + chunk)) { apiTxReset(); return; } // link error; drop, remote retries
    if (++apiTx.fragNext >= apiTx.fragTotal) apiTxReset();
  }
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
  espNowEnsurePeer(mac);
  serviceEspNowApiTx();
  return true;
}

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
  char buf[20];
  strcpy_P(buf, PSTR("{\"success\":true}"));
  sendEspNowApiJson(mac, ESPNOW_API_RESPONSE, msgId, buf, strlen(buf));
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

static void sendEspNowApiResponse(const uint8_t* mac, uint8_t msgId, bool verbose) {
  if (!verbose) { sendEspNowApiSuccess(mac, msgId); return; }
  uint8_t err = queueEspNowApiState(mac, ESPNOW_API_RESPONSE, msgId);
  if (err) sendEspNowApiError(mac, msgId, err);
}

// The reply is broadcast: a unicast reply needs a MAC-level ACK, which is unreliable while
// this radio time-shares with WiFi scanning/connecting; the remote identifies us by the
// frame's source MAC (and the "mac" field).
static void sendEspNowHello() {
  if (statusESPNow != ESP_NOW_STATE_ON) return;
  if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) return; // discovery is best-effort; remote re-broadcasts
  pDoc->clear();
  JsonObject hello = pDoc->createNestedObject("hello");
  hello[F("name")] = serverDescription;
  hello[F("mac")]  = escapedMac;
  hello[F("ver")]  = VERSION;
  hello[F("ch")]   = WiFi.channel();
  queueApiDocLocked(ESPNOW_BROADCAST_ADDRESS, ESPNOW_API_HELLO, 0);
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
#ifdef ESP8266
  const size_t MAX_LIVE_LEDS_ESPNOW = 256U;
#else
  const size_t MAX_LIVE_LEDS_ESPNOW = 1024U;
#endif
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

static void handleEspNowLive() {
  if (!apiLiveActive) return;
  if ((long)(millis() - apiLiveExpiry) >= 0) { apiLiveReset(); return; } // remote stopped re-arming
  if (millis() - apiLastLiveTime <= ESPNOW_LIVE_INTERVAL) return;
  bool success = queueEspNowLiveLeds(apiLiveMac, apiLiveMsgId++);
  apiLastLiveTime = millis();
  if (!success) apiLastLiveTime -= 20; // retry sooner if TX slot or heap was busy
}

static bool apiResetAll() {
  {
    EspNowApiStateGuard guard;
    if (!guard) return false;
    apiReasmReset();
    apiInboxReset();
  }
  apiLiveReset();
  apiTxReset();
  apiRemoteSeen = 0;
  return true;
}

// Apply a completed inbound message in loop context.
void handleEspNowApi() {
  static bool needCleanup = false;
  if (!espNowApiReady()) {
    if (needCleanup) needCleanup = !apiResetAll(); // once, on the on->off transition
    return;
  }
  needCleanup = true;

  serviceEspNowApiTx();
  handleEspNowLive();

  uint8_t srcMac[6];
  uint8_t msgType = 0, msgId = 0;
  uint8_t* json = nullptr;
  size_t jsonLen = 0;

  {
    EspNowApiStateGuard guard;
    if (!guard) return;
    apiReasmCleanupStale();
    if (!apiInbox.ready) return;
    memcpy(srcMac, apiInbox.srcMac, 6);
    msgType = apiInbox.msgType;
    msgId = apiInbox.msgId;
    json = apiInbox.json; // ownership moves to this invocation
    jsonLen = apiInbox.len;
    apiInbox.json = nullptr;
    apiInbox.len = 0;
    apiInbox.ready = false;
  }

  unsigned long start = millis();
  while (strip.isUpdating() && millis()-start < ESPNOW_API_STRIPWAIT_TIMEOUT) yield();

  if (msgType == ESPNOW_API_HELLO) {
    sendEspNowHello();
  } else if (msgType == ESPNOW_API_REQUEST) {
    if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) {
      sendEspNowApiError(srcMac, msgId, ESPNOW_API_ERR_BUSY);
    } else {
      DeserializationError err = deserializeJson(*pDoc, json, jsonLen);
      JsonObject root = pDoc->as<JsonObject>();
      if (err || root.isNull()) {
        releaseJSONBufferLock();
        sendEspNowApiError(srcMac, msgId, ESPNOW_API_ERR_JSON);
      } else {
        // mirror wsEvent(): {"v":true} polls state, {"lv":...} toggles live peek
        bool verbose = false;
        if (root["v"] && root.size() == 1) {
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
          releaseJSONBufferLock();
          sendEspNowApiCatalog(srcMac, msgId, what);
          free(json);
          return;
        } else {
          verbose = deserializeState(root, CALL_MODE_BUTTON);
        }
        releaseJSONBufferLock();
        // If the request changed state, a PUSH will follow soon. Acknowledge here
        // instead of serializing the same state twice.
        if (verbose && interfaceUpdateCallMode) verbose = false;
        sendEspNowApiResponse(srcMac, msgId, verbose);
      }
    }
  }

  free(json);
}

// Broadcast current state to paired remotes from updateInterfaces(), inheriting its cooldown.
// Gated on a recent API frame so WizMote-only setups never see these frames.
void pushEspNowState() {
  if (!espNowApiReady() || linked_remotes.empty() || !espNowApiRemoteActive()) return;
  if (!apiTxIdle()) return; // best-effort: a state push never waits behind an in-flight message
  static uint8_t pushId = 0;
  if (queueEspNowApiState(ESPNOW_BROADCAST_ADDRESS, ESPNOW_API_PUSH, pushId) == 0) pushId++;
}
#endif // WLED_DISABLE_ESPNOW
