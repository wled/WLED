#include "wled.h"
#ifndef WLED_DISABLE_ESPNOW
#include <atomic>

#define ESPNOW_BUSWAIT_TIMEOUT 24 // one frame timeout to wait for bus to finish updating
#define ESPNOW_LIVE_INTERVAL 40         // live peek cadence (ms), matching the WS liveview
#define ESPNOW_LIVE_TIMEOUT 3000        // stop live peek if {"lv":true} is not re-armed within this window
#define ESPNOW_API_PRESENCE_TIMEOUT 120000  // push state only while an API remote has been seen this recently

#define NIGHT_MODE_DEACTIVATED     -1
#define NIGHT_MODE_BRIGHTNESS      5

#define WIZMOTE_BUTTON_ON          1
#define WIZMOTE_BUTTON_OFF         2
#define WIZMOTE_BUTTON_NIGHT       3
#define WIZMOTE_BUTTON_ONE         16
#define WIZMOTE_BUTTON_TWO         17
#define WIZMOTE_BUTTON_THREE       18
#define WIZMOTE_BUTTON_FOUR        19
#define WIZMOTE_BUTTON_FIVE        20
#define WIZMOTE_BUTTON_SIX         21
#define WIZMOTE_BUTTON_SEVEN       22
#define WIZMOTE_BUTTON_BRIGHT_UP   9
#define WIZMOTE_BUTTON_BRIGHT_DOWN 8

#define WIZ_SMART_BUTTON_ON          100
#define WIZ_SMART_BUTTON_OFF         101
#define WIZ_SMART_BUTTON_BRIGHT_UP   102
#define WIZ_SMART_BUTTON_BRIGHT_DOWN 103

// This is kind of an esoteric strucure because it's pulled from the "Wizmote"
// product spec. That remote is used as the baseline for behavior and availability
// since it's broadly commercially available and works out of the box as a drop-in
typedef struct WizMoteMessageStructure {
  uint8_t program;  // 0x91 for ON button, 0x81 for all others
  uint8_t seq[4];   // Incremetal sequence number 32 bit unsigned integer LSB first
  uint8_t dt1;      // Button Data Type (0x20)
  uint8_t button;   // Identifies which button is being pressed
  uint8_t dt2;      // Battery Level Data Type (0x01)
  uint8_t batLevel; // Battery Level 0-100
  
  uint8_t byte10;   // Unknown, maybe checksum
  uint8_t byte11;   // Unknown, maybe checksum
  uint8_t byte12;   // Unknown, maybe checksum
  uint8_t byte13;   // Unknown, maybe checksum
} message_structure_t;

static uint32_t last_seq = UINT32_MAX;
static int brightnessBeforeNightMode = NIGHT_MODE_DEACTIVATED;
static int16_t ESPNowButton = -1; // set in callback if new button value is received

// Pulled from the IR Remote logic but reduced to 10 steps with a constant of 3
static const byte brightnessSteps[] = {
  6, 9, 14, 22, 33, 50, 75, 113, 170, 255
};
static const size_t numBrightnessSteps = sizeof(brightnessSteps) / sizeof(byte);

inline bool nightModeActive() {
  return brightnessBeforeNightMode != NIGHT_MODE_DEACTIVATED;
}

static void activateNightMode() {
  if (nightModeActive()) return;
  brightnessBeforeNightMode = bri;
  bri = NIGHT_MODE_BRIGHTNESS;
  stateUpdated(CALL_MODE_BUTTON);
}

static bool resetNightMode() {
  if (!nightModeActive()) return false;
  bri = brightnessBeforeNightMode;
  brightnessBeforeNightMode = NIGHT_MODE_DEACTIVATED;
  stateUpdated(CALL_MODE_BUTTON);
  return true;
}

// increment `bri` to the next `brightnessSteps` value
static void brightnessUp() {
  if (nightModeActive()) return;
  // dumb incremental search is efficient enough for so few items
  for (unsigned index = 0; index < numBrightnessSteps; ++index) {
    if (brightnessSteps[index] > bri) {
      bri = brightnessSteps[index];
      break;
    }
  }
  stateUpdated(CALL_MODE_BUTTON);
}

// decrement `bri` to the next `brightnessSteps` value
static void brightnessDown() {
  if (nightModeActive()) return;
  // dumb incremental search is efficient enough for so few items
  for (int index = numBrightnessSteps - 1; index >= 0; --index) {
    if (brightnessSteps[index] < bri) {
      bri = brightnessSteps[index];
      break;
    }
  }
  stateUpdated(CALL_MODE_BUTTON);
}

static void setOn() {
  resetNightMode();
  if (!bri) {
    toggleOnOff();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

static void setOff() {
  resetNightMode();
  if (bri) {
    toggleOnOff();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

static void presetWithFallback(uint8_t presetID, uint8_t effectID, uint8_t paletteID) {
  resetNightMode();
  applyPresetWithFallback(presetID, CALL_MODE_BUTTON_PRESET, effectID, paletteID);
}

// this function follows the same principle as decodeIRJson()
static bool remoteJson(int button)
{
  char objKey[10];
  bool parsed = false;

  if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) return false;

  sprintf_P(objKey, PSTR("\"%d\":"), button);

  unsigned long start = millis();
  while (strip.isUpdating() && millis()-start < ESPNOW_BUSWAIT_TIMEOUT) yield(); // wait for strip to finish updating, accessing FS during sendout causes glitches

  // attempt to read command from remote.json
  readObjectFromFile(PSTR("/remote.json"), objKey, pDoc);
  JsonObject fdo = pDoc->as<JsonObject>();
  if (fdo.isNull()) {
    // the received button does not exist
    //if (!WLED_FS.exists(F("/remote.json"))) errorFlag = ERR_FS_RMLOAD; //warn if file itself doesn't exist
    releaseJSONBufferLock();
    return parsed;
  }

  String cmdStr = fdo["cmd"].as<String>();
  JsonObject jsonCmdObj = fdo["cmd"]; //object

  if (jsonCmdObj.isNull())  // we could also use: fdo["cmd"].is<String>()
  {
    if (cmdStr.startsWith("!")) {
      // call limited set of C functions
      if (cmdStr.startsWith(F("!incBri"))) {
        brightnessUp();
        parsed = true;
      } else if (cmdStr.startsWith(F("!decBri"))) {
        brightnessDown();
        parsed = true;
      } else if (cmdStr.startsWith(F("!presetF"))) { //!presetFallback
        uint8_t p1 = fdo["PL"] | 1;
        uint8_t p2 = fdo["FX"] | hw_random8(strip.getModeCount() -1);
        uint8_t p3 = fdo["FP"] | 0;
        presetWithFallback(p1, p2, p3);
        parsed = true;
      }
    } else {
      // HTTP API command
      String apireq = "win"; apireq += '&';                        // reduce flash string usage
      //if (cmdStr.indexOf("~") || fdo["rpt"]) lastValidCode = code; // repeatable action
      if (!cmdStr.startsWith(apireq)) cmdStr = apireq + cmdStr;    // if no "win&" prefix
      if (!irApplyToAllSelected && cmdStr.indexOf(F("SS="))<0) {
        char tmp[10];
        sprintf_P(tmp, PSTR("&SS=%d"), strip.getMainSegmentId());
        cmdStr += tmp;
      }
      fdo.clear();                                                 // clear JSON buffer (it is no longer needed)
      handleSet(nullptr, cmdStr, false);                           // no stateUpdated() call here
      stateUpdated(CALL_MODE_BUTTON);
      parsed = true;
    }
  } else {
    // command is JSON object (TODO: currently will not handle irApplyToAllSelected correctly)
    deserializeState(jsonCmdObj, CALL_MODE_BUTTON);
    parsed = true;
  }
  releaseJSONBufferLock();
  return parsed;
}

// Callback function that will be executed when data is received from a linked remote
void handleWiZdata(uint8_t *incomingData, size_t len) {
  message_structure_t *incoming = reinterpret_cast<message_structure_t *>(incomingData);

  if (len != sizeof(message_structure_t)) {
    DEBUG_PRINTF_P(PSTR("Unknown incoming ESP Now message received of length %u\n"), len);
    return;
  }

  uint32_t cur_seq = incoming->seq[0] | (incoming->seq[1] << 8) | (incoming->seq[2] << 16) | (incoming->seq[3] << 24);
  if (cur_seq == last_seq) {
    return;
  }

  DEBUG_PRINT(F("Incoming ESP Now Packet ["));
  DEBUG_PRINT(cur_seq);
  DEBUG_PRINT(F("] from sender ["));
  DEBUG_PRINT(last_signal_src);
  DEBUG_PRINT(F("] button: "));
  DEBUG_PRINTLN(incoming->button);

  ESPNowButton = incoming->button; // save state, do not process in callback (can cause glitches)
  last_seq = cur_seq;
}

// Bidirectional JSON transport for linked ESP-NOW remotes. Frames are fragmented to fit
// the 250-byte ESP-NOW payload limit; see docs/espnow-json-protocol.md.
// Completed messages are applied in loop context, where FS and LED state are safe to touch.

struct EspNowApiInbox {
  volatile bool ready;
  uint8_t  srcMac[6];
  uint8_t  msgType;
  uint8_t  msgId;
  uint8_t* json;          // NUL-terminated heap buffer; ownership passes to the loop
  size_t   len;
};
static EspNowApiInbox apiInbox = {false, {0}, 0, 0, nullptr, 0};

static uint8_t*      apiReasmBuf   = nullptr;
static uint8_t       apiReasmSrc[6]= {0};
static uint8_t       apiReasmId    = 0;
static uint8_t       apiReasmType  = 0;
static uint8_t       apiReasmTotal = 0;
static uint8_t       apiReasmCount = 0;
static uint64_t      apiReasmFlags = 0;       // received-fragment bitmask (fragTotal <= 64)
static size_t        apiReasmLen   = 0;
static unsigned long apiReasmLast  = 0;
static unsigned long apiRemoteSeen = 0;       // last time any API frame arrived; gates state pushes

static bool          apiLiveActive  = false;
static uint8_t       apiLiveMac[6]  = {0};
static uint8_t       apiLiveMsgId   = 0;
static unsigned long apiLastLiveTime = 0;
static unsigned long apiLiveExpiry  = 0;       // live peek is a keepalive (no disconnect signal over ESP-NOW)

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

static void apiReasmCleanupStale() {
  if (apiReasmBuf && millis() - apiReasmLast > ESPNOW_API_REASM_TIMEOUT) apiReasmReset();
}

static void apiResetAll() {
  EspNowApiStateGuard guard;
  if (guard) {
    apiReasmReset();
    apiInboxReset();
  }
  apiLiveReset();
}

bool espNowApiReady() {
  return enableESPNow && statusESPNow == ESP_NOW_STATE_ON && (interfacesInited || apActive) && (apActive || WLED_CONNECTED);
}

// Reassemble an inbound frame in receive-task context: validated, bounded copies only.
void handleEspNowApiData(uint8_t* address, uint8_t* data, uint8_t len, bool broadcast) {
  if (len < ESPNOW_API_HEADER_SIZE) return;
  const uint8_t msgType   = data[2];
  const uint8_t msgId     = data[3];
  const uint8_t fragIndex = data[4];
  const uint8_t fragTotal = data[5];
  const uint8_t payloadLen = len - ESPNOW_API_HEADER_SIZE;

  // reject untrusted header values before any indexing or allocation
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

  const uint64_t bit = (uint64_t)1 << fragIndex;
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
  apiReasmBuf = nullptr; // ownership moved to the inbox
  apiReasmTotal = apiReasmCount = 0;
  apiReasmFlags = 0;
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

// Fragment a payload and send it, pacing against the small TX ring buffer.
static bool sendEspNowApiPayload(const uint8_t* mac, uint8_t msgType, uint8_t msgId, const uint8_t* payload, size_t payloadLen) {
  if (statusESPNow != ESP_NOW_STATE_ON || !mac) return false;
  if (!payload && payloadLen) return false;
  size_t total = payloadLen ? (payloadLen + ESPNOW_API_FRAG_SIZE - 1) / ESPNOW_API_FRAG_SIZE : 1;
  if (total > ESPNOW_API_MAX_FRAGS) return false;
  espNowEnsurePeer(mac);

  uint8_t frame[ESPNOW_API_HEADER_SIZE + ESPNOW_API_FRAG_SIZE];
  frame[0] = ESPNOW_API_MAGIC;
  frame[1] = ESPNOW_API_VERSION;
  frame[2] = msgType;
  frame[3] = msgId;
  frame[5] = (uint8_t)total;
  for (size_t i = 0; i < total; i++) {
    size_t off = i * ESPNOW_API_FRAG_SIZE;
    size_t chunk = payloadLen > off ? payloadLen - off : 0;
    if (chunk > ESPNOW_API_FRAG_SIZE) chunk = ESPNOW_API_FRAG_SIZE;
    frame[4] = (uint8_t)i;
    if (chunk) memcpy(frame + ESPNOW_API_HEADER_SIZE, payload + off, chunk);
    unsigned long start = millis();
    while (!quickEspNow.readyToSendData() && millis() - start < ESPNOW_BUSWAIT_TIMEOUT) yield();
    if (quickEspNow.send(mac, frame, ESPNOW_API_HEADER_SIZE + chunk)) return false;
  }
  return true;
}

static bool sendEspNowApiJson(const uint8_t* mac, uint8_t msgType, uint8_t msgId, const char* json, size_t jsonLen) {
  return sendEspNowApiPayload(mac, msgType, msgId, reinterpret_cast<const uint8_t*>(json), jsonLen);
}

// Serialize the prepared pDoc and send it. Caller holds JSON_LOCK_REMOTE; this releases it
// before the (slow) send so the global JSON buffer is not held during transmission.
static bool sendApiDocLocked(const uint8_t* mac, uint8_t msgType, uint8_t msgId) {
  size_t len = measureJson(*pDoc);
  if (len == 0 || len > ESPNOW_API_MAX_JSON) { releaseJSONBufferLock(); return false; }
  char* buf = (char*)d_malloc(len + 1);
  if (!buf) { releaseJSONBufferLock(); return false; }
  serializeJson(*pDoc, buf, len + 1);
  releaseJSONBufferLock();
  bool ok = sendEspNowApiJson(mac, msgType, msgId, buf, len);
  free(buf);
  return ok;
}

static bool sendEspNowApiState(const uint8_t* mac, uint8_t msgType, uint8_t msgId) {
  if (statusESPNow != ESP_NOW_STATE_ON) return false;
  if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) return false;
  pDoc->clear();
  JsonObject state = pDoc->createNestedObject("state");
  serializeState(state);
  JsonObject info  = pDoc->createNestedObject("info");
  serializeInfo(info);
  return sendApiDocLocked(mac, msgType, msgId);
}

static void sendEspNowApiResponse(const uint8_t* mac, uint8_t msgId, bool verbose) {
  if (verbose) {
    if (!sendEspNowApiState(mac, ESPNOW_API_RESPONSE, msgId)) {
      sendEspNowApiJson(mac, ESPNOW_API_RESPONSE, msgId, "{\"error\":8}", 11); // response exceeded ESPNOW_API_MAX_JSON
    }
  } else {
    static const char ok[] = "{\"success\":true}";
    sendEspNowApiJson(mac, ESPNOW_API_RESPONSE, msgId, ok, strlen(ok));
  }
}

static void sendEspNowHello(const uint8_t* mac) {
  if (statusESPNow != ESP_NOW_STATE_ON) return;
  if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) return;
  pDoc->clear();
  JsonObject hello = pDoc->createNestedObject("hello");
  hello[F("name")] = serverDescription;
  uint8_t myMac[6];
  WiFi.macAddress(myMac);
  char macStr[13];
  sprintf_P(macStr, PSTR("%02x%02x%02x%02x%02x%02x"), myMac[0], myMac[1], myMac[2], myMac[3], myMac[4], myMac[5]);
  hello[F("mac")] = macStr;
  hello[F("ver")] = VERSION;
  hello[F("ch")]  = WiFi.channel();
  sendApiDocLocked(mac, ESPNOW_API_HELLO, 0);
}

// Answer a {"get":"fx|pal|ps"} catalog request so a remote can populate effect, palette and
// preset lists instead of hardcoding them. These mirror the WebUI's /json effects/palettes
// and presets.json. Large lists may exceed ESPNOW_API_MAX_JSON on an ESP8266 host (-> error 8).
static void sendEspNowApiCatalog(const uint8_t* mac, uint8_t msgId, const char* what) {
  if (statusESPNow != ESP_NOW_STATE_ON || !what) return;

  if (!strcmp_P(what, PSTR("ps"))) {
    // getPresetName() takes the JSON lock and uses pDoc, so collect names before locking.
    std::vector<std::pair<uint8_t, String>> presets;
    uint8_t misses = 0;
    for (uint16_t i = 1; i <= 250 && misses < 16; i++) {
      String name;
      if (getPresetName(i, name)) { presets.push_back({(uint8_t)i, name}); misses = 0; }
      else misses++;
    }
    if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) return;
    pDoc->clear();
    JsonObject ps = pDoc->createNestedObject("presets");
    for (auto& p : presets) ps[String(p.first)] = p.second;
    sendApiDocLocked(mac, ESPNOW_API_RESPONSE, msgId);
    return;
  }

  if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) return;
  pDoc->clear();
  if (!strcmp_P(what, PSTR("fx"))) {
    JsonArray effects = pDoc->createNestedArray("effects");
    serializeModeNames(effects);
  } else if (!strcmp_P(what, PSTR("pal"))) {
    (*pDoc)[F("palettes")] = serialized((const __FlashStringHelper*)JSON_palette_names);
  } else {
    releaseJSONBufferLock();
    sendEspNowApiJson(mac, ESPNOW_API_RESPONSE, msgId, "{\"error\":9}", 11);
    return;
  }
  sendApiDocLocked(mac, ESPNOW_API_RESPONSE, msgId);
}

// Binary live LED peek payload, identical in format to the WebSocket liveview.
static bool sendEspNowLiveLeds(const uint8_t* mac, uint8_t msgId) {
  if (statusESPNow != ESP_NOW_STATE_ON || !mac) return false;

  size_t used = strip.getLengthTotal();
  if (!used) return false;
#ifdef ESP8266
  const size_t MAX_LIVE_LEDS_ESPNOW = 256U;
#else
  const size_t MAX_LIVE_LEDS_ESPNOW = 1024U;
#endif
  size_t n = ((used - 1) / MAX_LIVE_LEDS_ESPNOW) + 1; // serve every n'th LED when over the cap
  size_t pos = 2;
#ifndef WLED_DISABLE_2D
  if (strip.isMatrix) {
    used = Segment::maxWidth * Segment::maxHeight;
    n = 1;
    if (used > MAX_LIVE_LEDS_ESPNOW) n = 2;
    if (used > MAX_LIVE_LEDS_ESPNOW * 4) n = 4;
    pos = 4;
  }
#endif
  size_t bufSize = pos + (used / n) * 3;
  if (bufSize > ESPNOW_API_MAX_JSON) return false;

  uint8_t* buffer = reinterpret_cast<uint8_t*>(d_malloc(bufSize));
  if (!buffer) return false;
  buffer[0] = 'L';
  buffer[1] = 1;

#ifndef WLED_DISABLE_2D
  if (strip.isMatrix) {
    buffer[1] = 2;
    buffer[2] = Segment::maxWidth / n;
    buffer[3] = Segment::maxHeight / n;
  }
#endif

  for (size_t i = 0; pos < bufSize - 2; i += n) {
#ifndef WLED_DISABLE_2D
    if (strip.isMatrix && n > 1 && (i / Segment::maxWidth) % n) i += Segment::maxWidth * (n - 1);
#endif
    uint32_t c = strip.getPixelColor(i);
    uint8_t r = R(c), g = G(c), b = B(c), w = W(c);
    buffer[pos++] = bri ? qadd8(w, r) : 0; // fold the white channel into RGB
    buffer[pos++] = bri ? qadd8(w, g) : 0;
    buffer[pos++] = bri ? qadd8(w, b) : 0;
  }

  bool ok = sendEspNowApiPayload(mac, ESPNOW_API_LIVE, msgId, buffer, bufSize);
  free(buffer);
  return ok;
}

static void handleEspNowLive() {
  if (!apiLiveActive) return;
  if ((long)(millis() - apiLiveExpiry) >= 0) { apiLiveReset(); return; } // remote stopped re-arming
  if (millis() - apiLastLiveTime <= ESPNOW_LIVE_INTERVAL) return;
  bool success = sendEspNowLiveLeds(apiLiveMac, apiLiveMsgId++);
  apiLastLiveTime = millis();
  if (!success) apiLastLiveTime -= 20; // retry sooner if TX queue or heap was busy
}

// Apply a completed inbound message in loop context.
void handleEspNowApi() {
  if (!espNowApiReady()) {
    apiResetAll();
    return;
  }
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
  while (strip.isUpdating() && millis()-start < ESPNOW_BUSWAIT_TIMEOUT) yield();

  if (msgType == ESPNOW_API_HELLO) {
    sendEspNowHello(srcMac);
  } else if (msgType == ESPNOW_API_REQUEST) {
    bool verbose = true;
    if (!requestJSONBufferLock(JSON_LOCK_REMOTE)) {
      sendEspNowApiJson(srcMac, ESPNOW_API_RESPONSE, msgId, "{\"error\":3}", 11);
    } else {
      DeserializationError err = deserializeJson(*pDoc, json, jsonLen);
      JsonObject root = pDoc->as<JsonObject>();
      if (err || root.isNull()) {
        releaseJSONBufferLock();
        sendEspNowApiJson(srcMac, ESPNOW_API_RESPONSE, msgId, "{\"error\":9}", 11);
      } else {
        // mirror wsEvent(): {"v":true} polls state, {"lv":...} toggles live peek
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
          verbose = false;
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
        sendEspNowApiResponse(srcMac, msgId, verbose);
      }
    }
  }

  free(json);
}

// Broadcast current state to paired remotes from updateInterfaces(), inheriting its cooldown.
// Gated on a recent API frame so WizMote-only setups never see these frames.
void pushEspNowState() {
  if (!espNowApiReady() || linked_remotes.empty()) return;
  if (!apiRemoteSeen || millis() - apiRemoteSeen > ESPNOW_API_PRESENCE_TIMEOUT) return;
  static uint8_t pushId = 0;
  sendEspNowApiState(ESPNOW_BROADCAST_ADDRESS, ESPNOW_API_PUSH, pushId++);
}
// process ESPNow button data (acesses FS, should not be called while update to avoid glitches)
void handleRemote() {
  if(ESPNowButton >= 0) {
  if (!remoteJson(ESPNowButton))
    switch (ESPNowButton) {
      case WIZMOTE_BUTTON_ON             : setOn();                                         break;
      case WIZMOTE_BUTTON_OFF            : setOff();                                        break;
      case WIZMOTE_BUTTON_ONE            : presetWithFallback(1, FX_MODE_STATIC,        0); break;
      case WIZMOTE_BUTTON_TWO            : presetWithFallback(2, FX_MODE_BREATH,        0); break;
      case WIZMOTE_BUTTON_THREE          : presetWithFallback(3, FX_MODE_FIRE_FLICKER,  0); break;
      case WIZMOTE_BUTTON_FOUR           : presetWithFallback(4, FX_MODE_RAINBOW,       0); break;
      case WIZMOTE_BUTTON_FIVE           : presetWithFallback(5, FX_MODE_CANDLE,        0); break;
      case WIZMOTE_BUTTON_SIX            : presetWithFallback(6, FX_MODE_RANDOM_COLOR,  0); break;
      case WIZMOTE_BUTTON_SEVEN          : presetWithFallback(7, FX_MODE_FADE,          0); break;
      case WIZMOTE_BUTTON_NIGHT          : activateNightMode();                             break;
      case WIZMOTE_BUTTON_BRIGHT_UP      : brightnessUp();                                  break;
      case WIZMOTE_BUTTON_BRIGHT_DOWN    : brightnessDown();                                break;
      case WIZ_SMART_BUTTON_ON           : setOn();                                         break;
      case WIZ_SMART_BUTTON_OFF          : setOff();                                        break;
      case WIZ_SMART_BUTTON_BRIGHT_UP    : brightnessUp();                                  break;
      case WIZ_SMART_BUTTON_BRIGHT_DOWN  : brightnessDown();                                break;
      default: break;
    }
  }
  ESPNowButton = -1;
}

#else
void handleRemote() {}
#endif
