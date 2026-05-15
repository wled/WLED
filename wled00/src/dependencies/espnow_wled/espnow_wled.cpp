#include "wled.h"

/*
 * Lightweight ESP-NOW driver for WLED
 * note: currently supports only broadcast sending
 */

#ifndef WLED_DISABLE_ESPNOW

// -------------------------------------------------------------------------
// Global instances (extern-declared in espnow_wled.h)
// -------------------------------------------------------------------------
WledEspNow          espNow;
//WledEspNowBroadcast espnowBroadcast; // note: WledEspNowBroadcast was added using AI with the goal of enabling porting the WLEDtubes usermod but I did not investigate if this is viable or useful so commented out for now

// -------------------------------------------------------------------------
// Static broadcast MAC — used everywhere we need to address a peer.
// -------------------------------------------------------------------------
static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};


// =========================================================================
// Platform-specific SDK callbacks
// =========================================================================

#ifdef ARDUINO_ARCH_ESP32

// ----- ESP32 sent callback -----------------------------------------------
static void _espnowSentCB(const uint8_t *mac, esp_now_send_status_t status) {
  if (espNow._inFlight > 0) espNow._inFlight--;
  if (espNow._sentCB)
    espNow._sentCB(const_cast<uint8_t*>(mac), (uint8_t)status);
}

// ----- ESP32 recv callback -----------------------------------------------
// Signature changed in IDF 5.0: the first parameter became esp_now_recv_info_t*
// which carries the source address, destination address (useful to detect
// broadcast) and the rx_ctrl struct with RSSI.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// note: IDF V5 code is AI generated, unreviewed and untested
static void _espnowRecvCB(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (!info || !data || len <= 0) return;
  const uint8_t *mac  = info->src_addr;
  // rx_ctrl is a pointer to wifi_pkt_rx_ctrl_t; cast to int8_t to get signed RSSI.
  int8_t rssi = (info->rx_ctrl) ? (int8_t)info->rx_ctrl->rssi : 0;
  // Broadcast when the destination address has all bits set.
  bool isBroadcast = (info->des_addr &&
                      memcmp(info->des_addr, BCAST, 6) == 0);

  //espnowBroadcast.dispatch(mac, data, (uint8_t)len, rssi);
  if (espNow._rcvdCB)
    espNow._rcvdCB(const_cast<uint8_t*>(mac),
                        const_cast<uint8_t*>(data),
                        (uint8_t)len, (signed int)rssi, isBroadcast);
}

#else  // IDF < 5.0

static void _espnowRecvCB(const uint8_t *mac, const uint8_t *data, int len) {
  if (!mac || !data || len <= 0) return;
  // RSSI is not available in the IDF<5 callback; use 0.
  //espnowBroadcast.dispatch(mac, data, (uint8_t)len, 0);
  if (espNow._rcvdCB)
    espNow._rcvdCB(const_cast<uint8_t*>(mac), const_cast<uint8_t*>(data), (uint8_t)len, 0, true);
}

#endif // ESP_IDF_VERSION

#else  // ESP8266

// ----- ESP8266 sent callback ---------------------------------------------
static void _espnowSentCB(uint8_t *mac, uint8_t status) {
  if (espNow._inFlight > 0) espNow._inFlight--;
  if (espNow._sentCB)
    espNow._sentCB(mac, status);
}

// ----- ESP8266 recv callback ---------------------------------------------
static void _espnowRecvCB(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (!mac || !data || len == 0) return;
  //espnowBroadcast.dispatch(mac, data, len, 0);
  if (espNow._rcvdCB)
    espNow._rcvdCB(mac, data, len, 0, true);
}

#endif

bool WledEspNow::begin(uint8_t channel, uint8_t iface) {
  if (_running) stop(); // clean up before re-init

// note: channel must be 0-14 (14: used in Japan only), channel = 0 means "use current WiFi channel" (both on ESP8266 and ESP32, in AP and STA mode)
#ifdef ARDUINO_ARCH_ESP32
  if (esp_now_init() != ESP_OK) {
    DEBUG_PRINTLN(F("ESP-NOW esp_now_init() failed"));
    return false;
  }
  if (esp_now_register_recv_cb(_espnowRecvCB) != ESP_OK) {
    esp_now_deinit();
    return false;
  }
  if (esp_now_register_send_cb(_espnowSentCB) != ESP_OK) {
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    return false;
  }
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = channel;
  peer.ifidx   = (wifi_interface_t)iface;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) {
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    esp_now_deinit();
    return false;
  }
  _running = true;
  return true;

#else  // ESP8266
  if (esp_now_init() != 0) {
    DEBUG_PRINTLN(F("ESP-NOW esp_now_init() failed"));
    return false;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO); // TODO: found no official documentation on this... quickespnow ESP_NOW_ROLE_SLAVE in STA mode and ESP_NOW_ROLE_CONTROLLER in AP mode which seems wrong
  esp_now_register_recv_cb(_espnowRecvCB);
  esp_now_register_send_cb(_espnowSentCB);
  esp_now_add_peer(const_cast<uint8_t*>(BCAST), ESP_NOW_ROLE_COMBO, channel, nullptr, 0);
  _running = true;
  return true;
#endif
}

// STA mode: derives the channel from the current WiFi connection (channel 0 means "use current channel" for both ESP32 and ESP8266).
bool WledEspNow::begin() {
  return begin(0, WIFI_IF_STA);
}

void WledEspNow::stop() {
  if (!_running) return;
  _running = false;
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
  _inFlight = 0; // reset after unregistering callbacks
  esp_now_del_peer(const_cast<uint8_t*>(BCAST));
  esp_now_deinit(); // esp_now_deinit() frees any pending TX buffers
}

uint8_t WledEspNow::send(const uint8_t * /*addr*/, const uint8_t *data, uint8_t len) {
  // addr is ignored — we only support broadcast.
  // len must be < ESP_NOW_MAX_DATA_LEN (250 bytes).
  if (!_running || _inFlight >= ESPNOW_MAX_INFLIGHT) return 1;
  // ESP8266 SDK uses non-const uint8_t* parameters; const_cast is safe here.
  int err = esp_now_send(const_cast<uint8_t*>(BCAST),
                         const_cast<uint8_t*>(data), len);
  if (err == 0) _inFlight++; // ESP_OK == 0 on both platforms
  return (err == 0) ? 0 : 1;
}

#ifdef ARDUINO_ARCH_ESP32
void WledEspNow::setWiFiBandwidth(uint8_t iface, uint8_t bw) {
  esp_wifi_set_bandwidth((wifi_interface_t)iface, (wifi_bandwidth_t)bw);
}
#endif

/*
// start AI code, unreviewed, untested
// =========================================================================
// WledEspNowBroadcast — implementation
// =========================================================================

bool WledEspNowBroadcast::send(const uint8_t *msg, size_t len) {
  if (len > 250) return false; // ESP-NOW max payload
  return espNow.send(BCAST, msg, (uint8_t)len) == 0;
}

WledEspNowBroadcast::STATE WledEspNowBroadcast::getState() const {
  switch (statusESPNow) {
    case ESP_NOW_STATE_ON:    return STARTED;
    case ESP_NOW_STATE_UNINIT: // fall through
    case ESP_NOW_STATE_ERROR:
    default:                  return STOPPED;
  }
}

bool WledEspNowBroadcast::registerCallback(receive_callback_t cb) {
  for (size_t i = 0; i < WLED_ESPNOW_MAX_REGISTERED_CALLBACKS - 1; i++) {
    if (_callbacks[i] == cb)   return true;  // already registered
    if (_callbacks[i] == nullptr) {
      _callbacks[i] = cb;
      return true;
    }
  }
  return false; // array full
}

bool WledEspNowBroadcast::removeCallback(receive_callback_t cb) {
  size_t found = WLED_ESPNOW_MAX_REGISTERED_CALLBACKS;
  for (size_t i = 0; i < WLED_ESPNOW_MAX_REGISTERED_CALLBACKS - 1; i++) {
    if (_callbacks[i] == cb) { found = i; break; }
  }
  if (found == WLED_ESPNOW_MAX_REGISTERED_CALLBACKS) return false;
  // Shift remaining entries left to close the gap.
  for (size_t i = found; i < WLED_ESPNOW_MAX_REGISTERED_CALLBACKS - 1; i++)
    _callbacks[i] = _callbacks[i + 1];
  _callbacks[WLED_ESPNOW_MAX_REGISTERED_CALLBACKS - 1] = nullptr;
  return true;
}

WledEspNowBroadcast::receive_filter_t
WledEspNowBroadcast::registerFilter(receive_filter_t filter) {
  receive_filter_t old = _filter;
  _filter = filter;
  return old;
}

void WledEspNowBroadcast::dispatch(const uint8_t *mac, const uint8_t *data,
                                   uint8_t len, int8_t rssi) {
  if (_filter && !_filter(mac, data, len, rssi)) return;
  for (size_t i = 0; i < WLED_ESPNOW_MAX_REGISTERED_CALLBACKS - 1; i++) {
    if (_callbacks[i]) _callbacks[i](mac, data, len, rssi);
  }
}
// end AI code, unreviewed, untested
*/

#endif // WLED_DISABLE_ESPNOW
