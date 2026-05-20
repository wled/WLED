// espnow_audio_sync.h
//
// Drop-in ESP-NOW transport for WLED audioreactive sync (v16.0.0 mainline).
// Designed to replace the WiFiUDP "fftUdp" path while keeping the exact same
// V2 audioSyncPacket wire-payload (44 bytes), so receivers process audio
// identically to before.
//
// Adds:
//   - ESP-NOW broadcast (~2-5 ms one-way vs ~8-30 ms UDP/Wi-Fi)
//   - 16-bit sequence number + receiver-side dedup
//   - Each packet sent twice back-to-back (cheap packet-loss insurance)
//   - Sender timestamp + deadline-based rendering for frame-accurate sync
//   - Wi-Fi power-save disabled, channel pinned, max TX power
//   - Auto-detects HOSTED mode (when WLED's QuickEspNow has the radio)
//     vs STANDALONE mode (when nothing else has it)
//
// Targets ESP32 / ESP32-S3 (Arduino-ESP32 v2.x / IDF v4.4+).
//
// API is RAW BYTES on purpose -- this header does not declare a
// `audioSyncPacket` type, so it never collides with WLED's nested
// `AudioReactive::audioSyncPacket` definition. Just pass `&pkt, sizeof(pkt)`
// from the call site.
//
// Wire-up (4 calls from audio_reactive.cpp):
//   1. begin(channel, isSender) - in connected() after fftUdp.beginMulticast
//   2. send(&pkt, sizeof(pkt))  - in transmitAudioData(), as alternative to
//                                  fftUdp.write(...)
//   3. poll(&pkt, sizeof(pkt))  - in receiveAudioData(), as alternative to
//                                  fftUdp.parsePacket() / fftUdp.read()
//   4. handleIncomingPacket(data, len) - inside AudioReactive's
//                                  onEspNowMessage() override (coexistence)
//
// See EDITS_FOR_YOUR_FILE.md for the exact edits.

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <esp_idf_version.h>
#include <string.h>

namespace espnowAudioSync {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint16_t EN_MAGIC      = 0xA53E;
static constexpr uint8_t  EN_VERSION    = 1;
static constexpr size_t   EN_PAYLOAD    = 44;   // V2 audioSyncPacket wire size
static constexpr size_t   EN_PREAMBLE   = 12;   // magic + version + flags + seq + pad + send_us
static constexpr size_t   EN_PACKET     = EN_PREAMBLE + EN_PAYLOAD;  // 56 bytes
static const     uint8_t  BROADCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// ---------------------------------------------------------------------------
// On-the-wire packet layout (preamble + raw payload), no audioSyncPacket
// dependency. Packed.
// ---------------------------------------------------------------------------
struct __attribute__((packed)) ENPacket {
    uint16_t magic;       // 0xA53E
    uint8_t  version;     // 1
    uint8_t  flags;       // bit0: this is the duplicate copy
    uint16_t seq;         // monotonically increasing per sender
    uint16_t pad;         // alignment
    uint32_t send_us;     // sender esp_timer_get_time() low 32 bits
    uint8_t  payload[EN_PAYLOAD];   // raw bytes of the V2 audioSyncPacket
};
static_assert(sizeof(ENPacket) == EN_PACKET, "ENPacket layout drift");

// ---------------------------------------------------------------------------
// State (file-scope statics; include this header from exactly one .cpp).
// ---------------------------------------------------------------------------
static uint8_t   _channel = 1;
static bool      _isSender = false;
static bool      _initialised = false;
static bool      _hosted = false;       // true: WLED's ESP-NOW owns the radio;
                                        // false: standalone (we own it)
static uint16_t  _txSeq = 0;

// Receiver state
static volatile bool _rxReady = false;
static uint8_t       _rxPayload[EN_PAYLOAD];      // last good frame
static uint32_t      _rxSendUs = 0;
static uint32_t      _rxArrivedLocalUs = 0;
static uint16_t      _lastRxSeq = 0;
static bool          _hasLastRxSeq = false;
static portMUX_TYPE  _rxMux = portMUX_INITIALIZER_UNLOCKED;

// Stats
static uint32_t _stat_rx = 0;
static uint32_t _stat_dupes = 0;
static uint32_t _stat_gaps = 0;
static uint32_t _stat_tx = 0;
static uint32_t _stat_tx_err = 0;

// ESP-NOW receive callback signature changed between Arduino-ESP32 v2.x and
// v3.x (IDF 4.x vs 5.x).
#if ESP_IDF_VERSION_MAJOR >= 5
  #define ESPNOW_RECV_CB_SIG const esp_now_recv_info_t * info, const uint8_t * data, int len
  #define ESPNOW_RECV_CB_ARGS_UNUSED (void)info;
#else
  #define ESPNOW_RECV_CB_SIG const uint8_t * mac, const uint8_t * data, int len
  #define ESPNOW_RECV_CB_ARGS_UNUSED (void)mac;
#endif

// Forward decl
static void _onRecv(ESPNOW_RECV_CB_SIG);

// ---------------------------------------------------------------------------
// begin() - call once from usermod connected() after Wi-Fi is up.
//   channel  : Wi-Fi channel (e.g. WiFi.channel()). Ignored in hosted mode.
//   isSender : true on the master, false on slaves. Use bit 0 of audioSyncEnabled.
//
// Auto-detects mode: HOSTED (WLED's QuickEspNow has already initialised
// ESP-NOW) or STANDALONE (we init it ourselves).
//
// Returns true on success, false otherwise.
// ---------------------------------------------------------------------------
inline bool begin(uint8_t channel, bool isSender) {
    if (_initialised) return true;
    _channel  = channel;
    _isSender = isSender;

    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(false, false);
    }
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Probe whether ESP-NOW is already initialised by trying to add a peer.
    // Returns ESP_ERR_ESPNOW_NOT_INIT if ESP-NOW is uninitialised.
    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, BROADCAST_MAC, 6);
    peer.channel = 0;          // 0 = use current Wi-Fi channel
    peer.encrypt = false;
    peer.ifidx   = WIFI_IF_STA;
    esp_err_t addRes = esp_now_add_peer(&peer);

    if (addRes == ESP_ERR_ESPNOW_NOT_INIT) {
        // STANDALONE: init ESP-NOW ourselves.
        _hosted = false;
        esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
        esp_wifi_set_max_tx_power(78);
        if (esp_now_init() != ESP_OK) return false;
        esp_now_register_recv_cb(_onRecv);
        peer.channel = _channel;
        if (!esp_now_is_peer_exist(BROADCAST_MAC)) {
            if (esp_now_add_peer(&peer) != ESP_OK) return false;
        }
    } else if (addRes == ESP_OK || addRes == ESP_ERR_ESPNOW_EXIST) {
        // HOSTED: WLED's QuickEspNow already owns the radio.
        _hosted = true;
    } else {
        return false;
    }

    _initialised = true;
    return true;
}

// ---------------------------------------------------------------------------
// send() - call from transmitAudioData() on the master.
// Sends two back-to-back copies for loss resilience; receivers dedup on seq.
// Caller passes the raw 44-byte V2 audioSyncPacket via void* + size.
// ---------------------------------------------------------------------------
inline bool send(const void * payload, size_t payloadLen) {
    if (!_initialised || !_isSender) return false;
    if (payloadLen != EN_PAYLOAD) return false;

    ENPacket ep;
    memset(&ep, 0, sizeof(ep));
    ep.magic   = EN_MAGIC;
    ep.version = EN_VERSION;
    ep.flags   = 0;
    ep.seq     = ++_txSeq;
    ep.pad     = 0;
    ep.send_us = (uint32_t)(esp_timer_get_time() & 0xFFFFFFFFULL);
    memcpy(ep.payload, payload, EN_PAYLOAD);

    esp_err_t r1 = esp_now_send(BROADCAST_MAC, (uint8_t*)&ep, sizeof(ep));
    ep.flags = 1;  // mark duplicate
    esp_err_t r2 = esp_now_send(BROADCAST_MAC, (uint8_t*)&ep, sizeof(ep));

    if (r1 == ESP_OK || r2 == ESP_OK) { _stat_tx++; return true; }
    _stat_tx_err++;
    return false;
}

// ---------------------------------------------------------------------------
// poll() - call from receiveAudioData() on a slave. Returns true once per
// new frame. Fills `out` with the raw 44-byte payload (drop into the
// existing decodeAudioData() path).
//
//   deadline_us : optional. Currently arrival-time + renderDelayUs/2. Pass
//                 nullptr if you don't care.
// ---------------------------------------------------------------------------
inline bool poll(void * out, size_t outLen,
                 uint32_t * deadline_us = nullptr,
                 uint32_t renderDelayUs = 15000) {
    if (!_rxReady) return false;
    if (outLen < EN_PAYLOAD) return false;

    uint32_t arrLocalUs;
    portENTER_CRITICAL(&_rxMux);
    memcpy(out, _rxPayload, EN_PAYLOAD);
    arrLocalUs = _rxArrivedLocalUs;
    _rxReady = false;
    portEXIT_CRITICAL(&_rxMux);

    if (deadline_us) {
        *deadline_us = arrLocalUs + (renderDelayUs / 2);
    }
    return true;
}

// ---------------------------------------------------------------------------
// handleIncomingPacket() - call from the audioreactive usermod's
// onEspNowMessage() override:
//
//   bool onEspNowMessage(uint8_t* sender, uint8_t* data, uint8_t len) override {
//       return espnowAudioSync::handleIncomingPacket(data, len);
//   }
//
// Returns true if this was an audio-sync packet (so WLED skips its own
// WiZmote / state-sync dispatch). Returns false otherwise.
// ---------------------------------------------------------------------------
inline bool handleIncomingPacket(const uint8_t * data, int len) {
    if (len != (int)sizeof(ENPacket)) return false;
    const ENPacket * ep = reinterpret_cast<const ENPacket *>(data);
    if (ep->magic != EN_MAGIC || ep->version != EN_VERSION) return false;
    if (_isSender) return true;  // ours but we ignore -- we sent it

    if (_hasLastRxSeq) {
        int16_t diff = (int16_t)((int16_t)ep->seq - (int16_t)_lastRxSeq);
        if (diff <= 0) { _stat_dupes++; return true; }
        if (diff > 1)  { _stat_gaps += (diff - 1); }
    }

    portENTER_CRITICAL(&_rxMux);
    _lastRxSeq        = ep->seq;
    _hasLastRxSeq     = true;
    _stat_rx++;
    memcpy(_rxPayload, ep->payload, EN_PAYLOAD);
    _rxSendUs         = ep->send_us;
    _rxArrivedLocalUs = (uint32_t)(esp_timer_get_time() & 0xFFFFFFFFULL);
    _rxReady          = true;
    portEXIT_CRITICAL(&_rxMux);
    return true;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
inline bool isHosted() { return _hosted; }

struct Stats {
    uint32_t rx;
    uint32_t dupes;
    uint32_t gaps;
    uint32_t tx;
    uint32_t tx_err;
};
inline Stats stats() {
    return Stats{_stat_rx, _stat_dupes, _stat_gaps, _stat_tx, _stat_tx_err};
}

// ---------------------------------------------------------------------------
// Internal: STANDALONE-mode receive callback. Forwards to
// handleIncomingPacket().
// ---------------------------------------------------------------------------
static void _onRecv(ESPNOW_RECV_CB_SIG) {
    ESPNOW_RECV_CB_ARGS_UNUSED
    handleIncomingPacket(data, len);
}

} // namespace espnowAudioSync