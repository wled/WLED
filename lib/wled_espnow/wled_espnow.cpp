#include "wled.h" // includes wled_espnow.h
/*
 * Lightweight ESP-NOW driver for WLED
 * by @dedehai (2026) licensed under EUPL 1.2 license (same as WLED)
 * note: currently supports only broadcast sending, callback kept compatible with quickEspNow
 */

#ifndef WLED_DISABLE_ESPNOW

namespace wled {

EspNow espNow;
//EspNowBroadcast espnowBroadcast; // note: EspNowBroadcast was added using AI with the goal of enabling porting the WLEDtubes usermod but I did not investigate if this is viable or useful so commented out for now

static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // static broadcast MAC

// =========================================================================
// 802.11 Action frame layout for ESP-NOW — used to walk backwards from the payload pointer to reach the wifi_pkt_rx_ctrl_t (which carries RSSI).
// Reference: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
typedef struct {
  uint16_t frame_head;
  uint16_t duration;
  uint8_t  destination_address[6];
  uint8_t  source_address[6];
  uint8_t  broadcast_address[6];
  uint16_t sequence_control;
  uint8_t  category_code;
  uint8_t  organization_identifier[3]; // 0x18fe34
  uint8_t  random_values[4];
  struct {
    uint8_t element_id;                 // 0xdd
    uint8_t length;
    uint8_t organization_identifier[3]; // 0x18fe34
    uint8_t type;                       // 4
    uint8_t version;
    uint8_t body[0];
  } vendor_specific_content;
} __attribute__((packed)) espnow_frame_format_t;

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
  bool isBroadcast = (info->des_addr && memcmp(info->des_addr, BCAST, 6) == 0);

  //espnowBroadcast.dispatch(mac, data, (uint8_t)len, rssi);
  if (espNow._rcvdCB)
    espNow._rcvdCB(const_cast<uint8_t*>(mac), const_cast<uint8_t*>(data), (uint8_t)len, (signed int)rssi, isBroadcast);
}

#else  // IDF < 5.0

static void _espnowRecvCB(const uint8_t *mac, const uint8_t *data, int len) {
  if (!mac || !data || len <= 0) return;
  // Walk back through the WiFi frame buffer to reach wifi_pkt_rx_ctrl_t to get RSSI. Reference: https://github.com/gmag11/QuickESPNow
  const espnow_frame_format_t *espnow_data = (const espnow_frame_format_t *)(data - sizeof(espnow_frame_format_t));
  const wifi_promiscuous_pkt_t *promiscuous_pkt = (const wifi_promiscuous_pkt_t *)(data - sizeof(wifi_pkt_rx_ctrl_t) - sizeof(espnow_frame_format_t));
  const wifi_pkt_rx_ctrl_t *rx_ctrl = &promiscuous_pkt->rx_ctrl;
  int8_t rssi = (int8_t)rx_ctrl->rssi;
  bool isBroadcast = (memcmp(espnow_data->destination_address, BCAST, 6) == 0);
  //espnowBroadcast.dispatch(mac, data, (uint8_t)len, rssi);
  if (espNow._rcvdCB)
    espNow._rcvdCB(const_cast<uint8_t*>(mac), const_cast<uint8_t*>(data), (uint8_t)len, (signed int)rssi, isBroadcast);
}

#endif // ESP_IDF_VERSION

#else  // ESP8266

// define wifi_pkt_rx_ctrl_t to match the hardware layout so we can extract RSSI
// https://github.com/espressif/ESP8266_RTOS_SDK/blob/master/components/esp8266/include/esp_wifi_types.h

typedef struct {
    signed rssi: 8;           /**< signal intensity of packet */
    unsigned rate: 4;         /**< data rate */
    unsigned is_group: 1;     /**< usually not used */
    unsigned : 1;             /**< reserve */
    unsigned sig_mode: 2;     /**< 0:is not 11n packet; 1:is 11n packet */
    unsigned legacy_length: 12; /**< Length of 11bg mode packet */
    unsigned damatch0: 1;     /**< usually not used */
    unsigned damatch1: 1;     /**< usually not used */
    unsigned bssidmatch0: 1;  /**< usually not used */
    unsigned bssidmatch1: 1;  /**< usually not used */
    unsigned mcs: 7;          /**< if is 11n packet, shows the modulation(range from 0 to 76) */
    unsigned cwb: 1;          /**< if is 11n packet, shows if is HT40 packet or not */
    unsigned HT_length: 16;   /**< Length of 11n mode packet */
    unsigned smoothing: 1;    /**< reserve */
    unsigned not_sounding: 1; /**< reserve */
    unsigned : 1;             /**< reserve */
    unsigned aggregation: 1;  /**< Aggregation */
    unsigned stbc: 2;         /**< STBC */
    unsigned fec_coding: 1;   /**< Flag is set for 11n packets which are LDPC */
    unsigned sgi: 1;          /**< SGI */
    unsigned rxend_state: 8;  /**< usually not used */
    unsigned ampdu_cnt: 8;    /**< ampdu cnt */
    unsigned channel: 4;      /**< which channel this packet in */
    unsigned : 4;             /**< reserve */
    signed noise_floor: 8;    /**< usually not used */
} wifi_pkt_rx_ctrl_t;

typedef struct {
    wifi_pkt_rx_ctrl_t rx_ctrl;
    uint8_t payload[0]; /* ieee80211 packet buff */
} wifi_promiscuous_pkt_t;


// ----- ESP8266 sent callback ---------------------------------------------
static void _espnowSentCB(uint8_t *mac, uint8_t status) {
  if (espNow._inFlight > 0) espNow._inFlight--;
  if (espNow._sentCB)
    espNow._sentCB(mac, status);
}

// ----- ESP8266 recv callback ---------------------------------------------
static void _espnowRecvCB(uint8_t *mac, uint8_t *data, uint8_t len) {
  if (!mac || !data || len == 0) return;
  // Walk back through the WiFi frame buffer to reach the rx control header to get RSSI.
  const espnow_frame_format_t *espnow_data = (const espnow_frame_format_t *)(data - sizeof(espnow_frame_format_t));
  bool isBroadcast = (memcmp(espnow_data->destination_address, BCAST, 6) == 0);
  const wifi_promiscuous_pkt_t *promiscuous_pkt = (const wifi_promiscuous_pkt_t *)(data - sizeof(wifi_pkt_rx_ctrl_t) - sizeof(espnow_frame_format_t));
  const wifi_pkt_rx_ctrl_t *rx_ctrl = &promiscuous_pkt->rx_ctrl;
  int8_t rssi = (int8_t)(rx_ctrl->rssi - 100); // ESP8266: raw RSSI is offset by ~+100 dBm vs actual signal strength
  //espnowBroadcast.dispatch(mac, data, len, rssi);
  if (espNow._rcvdCB)
    espNow._rcvdCB(mac, data, len, (signed int)rssi, isBroadcast);
}

#endif

bool EspNow::begin(uint8_t channel, uint8_t iface) {
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
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO); // TODO: found no official documentation on this... quickespnow uses ESP_NOW_ROLE_SLAVE in STA mode and ESP_NOW_ROLE_CONTROLLER in AP mode
  esp_now_register_recv_cb(_espnowRecvCB);
  esp_now_register_send_cb(_espnowSentCB);
  esp_now_add_peer(const_cast<uint8_t*>(BCAST), ESP_NOW_ROLE_COMBO, channel, nullptr, 0);
  _running = true;
  return true;
#endif
}

// STA mode: derives the channel from the current WiFi connection (channel 0 means "use current channel" for both ESP32 and ESP8266).
bool EspNow::begin() {
  return begin(0, WIFI_IF_STA);
}

void EspNow::stop() {
  if (!_running) return;
  _running = false;
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
  _inFlight = 0; // reset after unregistering callbacks
  esp_now_del_peer(const_cast<uint8_t*>(BCAST));
  esp_now_deinit(); // esp_now_deinit() frees any pending TX buffers
}

uint8_t EspNow::send(const uint8_t * /*addr*/, const uint8_t *data, uint8_t len) {
  static bool isretransmit = false;
  int err = 1; // default to error
  // addr is ignored — we only support broadcast.
  // len must be < ESP_NOW_MAX_DATA_LEN (250 bytes).
  if (!_running) return err;
  // ESP8266 SDK uses non-const uint8_t* parameters; const_cast is safe here.
  if ( _inFlight < ESPNOW_MAX_INFLIGHT) {
    err = esp_now_send(const_cast<uint8_t*>(BCAST), const_cast<uint8_t*>(data), len);
  }
  if (err == 0) _inFlight++; // 0 is ESP_OK but is not defined on ESP8266
  else if (_inFlight > 0 && !isretransmit) {
    uint8_t lastInFlight = _inFlight;
    delay(2); // wait for a queued message to be sent, found that 2ms is usually enough, dont want to be too cautios (burst send is currently an edge case)
    // note: delay and general approach might need some tweaking for real world use, based on burst tests sending 16 messages
    if (_inFlight < lastInFlight) {
      isretransmit = true; // try once more
      err = esp_now_send(const_cast<uint8_t*>(BCAST), const_cast<uint8_t*>(data), len);  // A message was sent and the sent callback was called, so we can retry now.
      if (err == 0) _inFlight++; // 0 is ESP_OK but is not defined on ESP8266
      else DEBUG_PRINTF("ESP-NOW send failed with error %d, inflight=%d\n", err, (int)espNow._inFlight);
      return err;
    }
  }
  // TODO: should monitor somehow if sending fails repeatedly and do something about it
  isretransmit = false; // reset flag
  return err;
}

#ifdef ARDUINO_ARCH_ESP32
void EspNow::setWiFiBandwidth(uint8_t iface, uint8_t bw) {
  esp_wifi_set_bandwidth((wifi_interface_t)iface, (wifi_bandwidth_t)bw);
}
#endif

/*
// start AI code, unreviewed, untested
// =========================================================================
// EspNowBroadcast — implementation
// =========================================================================

bool EspNowBroadcast::send(const uint8_t *msg, size_t len) {
  if (len > 250) return false; // ESP-NOW max payload
  return espNow.send(BCAST, msg, (uint8_t)len) == 0;
}

EspNowBroadcast::STATE EspNowBroadcast::getState() const {
  switch (statusESPNow) {
    case ESP_NOW_STATE_ON:    return STARTED;
    case ESP_NOW_STATE_UNINIT: // fall through
    case ESP_NOW_STATE_ERROR:
    default:                  return STOPPED;
  }
}

bool EspNowBroadcast::registerCallback(receive_callback_t cb) {
  for (size_t i = 0; i < WLED_ESPNOW_MAX_REGISTERED_CALLBACKS - 1; i++) {
    if (_callbacks[i] == cb)   return true;  // already registered
    if (_callbacks[i] == nullptr) {
      _callbacks[i] = cb;
      return true;
    }
  }
  return false; // array full
}

bool EspNowBroadcast::removeCallback(receive_callback_t cb) {
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

EspNowBroadcast::receive_filter_t
EspNowBroadcast::registerFilter(receive_filter_t filter) {
  receive_filter_t old = _filter;
  _filter = filter;
  return old;
}

void EspNowBroadcast::dispatch(const uint8_t *mac, const uint8_t *data,
                                   uint8_t len, int8_t rssi) {
  if (_filter && !_filter(mac, data, len, rssi)) return;
  for (size_t i = 0; i < WLED_ESPNOW_MAX_REGISTERED_CALLBACKS - 1; i++) {
    if (_callbacks[i]) _callbacks[i](mac, data, len, rssi);
  }
}
// end AI code, unreviewed, untested
*/

} // namespace wled

#endif // WLED_DISABLE_ESPNOW
