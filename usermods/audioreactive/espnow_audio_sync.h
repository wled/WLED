/**
 * @file    espnow_audio_sync.h
 * @brief   Low-latency ESP-NOW transport for WLED audioreactive sync.
 *
 * Drop-in replacement for the UDP multicast path used by WLED's
 * audioreactive usermod. Keeps the exact same V2 audioSyncPacket
 * wire-payload (44 bytes), so receivers process audio identically to
 * before.
 *
 * Features:
 *   - ESP-NOW broadcast (~2-5 ms one-way vs ~8-30 ms UDP/Wi-Fi)
 *   - 16-bit sequence number + receiver-side dedup
 *   - Each packet sent twice back-to-back (cheap packet-loss insurance)
 *   - Sender timestamp + deadline-based rendering for frame-accurate sync
 *   - Wi-Fi power-save disabled, channel pinned, max TX power
 *   - Auto-detects HOSTED mode (alongside WLED's QuickEspNow) vs
 *     STANDALONE mode (own the radio)
 *
 * @note Targets ESP32 / ESP32-S3 (Arduino-ESP32 v2.x / IDF v4.4+).
 * @note API is intentionally raw bytes (void* + size_t) so this header
 *       does not require visibility of WLED's nested
 *       `AudioReactive::audioSyncPacket` type.
 *
 * Wire-up (4 calls from audio_reactive.cpp):
 *   1. begin(channel, isSender) - in connected() after fftUdp.beginMulticast
 *   2. send(&pkt, sizeof(pkt))  - in transmitAudioData(), as an
 *                                  alternative to fftUdp.write(...)
 *   3. poll(&pkt, sizeof(pkt))  - in receiveAudioData(), as an
 *                                  alternative to fftUdp.parsePacket()
 *   4. handleIncomingPacket(data, len) - inside AudioReactive's
 *                                  onEspNowMessage() override
 *
 * @see EDITS_COMPLETE.md for the precise call-site changes.
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_timer.h>
#include <esp_idf_version.h>
#include <string.h>

/**
 * @namespace espnowAudioSync
 * @brief    Encapsulates all ESP-NOW audio-sync transport state and API.
 *
 * The namespace is meant to be included from exactly one .cpp per
 * firmware image, since it uses file-scope statics for its state.
 */
namespace espnowAudioSync {

/** @brief Magic number identifying audio-sync packets on the wire. */
static constexpr uint16_t EN_MAGIC      = 0xA53E;

/** @brief On-the-wire protocol version. Bump on incompatible changes. */
static constexpr uint8_t  EN_VERSION    = 1;

/** @brief Raw payload size in bytes (= sizeof V2 audioSyncPacket). */
static constexpr size_t   EN_PAYLOAD    = 44;

/** @brief Preamble size in bytes (magic + version + flags + seq + pad + send_us). */
static constexpr size_t   EN_PREAMBLE   = 12;

/** @brief Total on-the-wire packet size (preamble + payload). */
static constexpr size_t   EN_PACKET     = EN_PREAMBLE + EN_PAYLOAD;

/** @brief ESP-NOW broadcast destination MAC (FF:FF:FF:FF:FF:FF). */
static const     uint8_t  BROADCAST_MAC[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

/**
 * @brief  On-the-wire ESP-NOW packet layout.
 *
 * Packed to ensure byte-identical layout across senders/receivers.
 * Carries a 12-byte preamble followed by the raw 44-byte
 * audioSyncPacket payload from WLED's audioreactive usermod.
 */
struct __attribute__((packed)) ENPacket {
    uint16_t magic;                ///< Equals EN_MAGIC (0xA53E).
    uint8_t  version;              ///< Equals EN_VERSION (1).
    uint8_t  flags;                ///< Bit 0: this is the duplicate copy.
    uint16_t seq;                  ///< Monotonically increasing per sender.
    uint16_t pad;                  ///< Alignment pad (set to 0).
    uint32_t send_us;              ///< Sender esp_timer_get_time() low 32 bits.
    uint8_t  payload[EN_PAYLOAD];  ///< Raw bytes of the V2 audioSyncPacket.
};
static_assert(sizeof(ENPacket) == EN_PACKET, "ENPacket layout drift");

/** @brief Wi-Fi channel ESP-NOW is pinned to. */
static uint8_t   _channel = 1;

/** @brief True on the master (FFT source), false on slaves. */
static bool      _isSender = false;

/** @brief True once begin() has succeeded. Subsequent begin() calls no-op. */
static bool      _initialised = false;

/**
 * @brief True when WLED's QuickEspNow already owned ESP-NOW at begin()
 *        time (HOSTED mode); false when this module owns the radio
 *        (STANDALONE mode).
 */
static bool      _hosted = false;

/** @brief Sender's transmit sequence counter (wraps at 65535). */
static uint16_t  _txSeq = 0;

/** @brief Set to true by handleIncomingPacket() when a fresh frame is latched. */
static volatile bool _rxReady = false;

/** @brief Latched copy of the most recent audio frame's raw bytes. */
static uint8_t   _rxPayload[EN_PAYLOAD];

/** @brief Sender's send_us timestamp for the latched frame. */
static uint32_t  _rxSendUs = 0;

/** @brief Local micros() at which the latched frame was received. */
static uint32_t  _rxArrivedLocalUs = 0;

/** @brief Sequence number of the most recently accepted frame (for dedup). */
static uint16_t  _lastRxSeq = 0;

/** @brief False until the first frame has been received post-boot. */
static bool      _hasLastRxSeq = false;

/** @brief Critical-section mutex protecting the receiver's latched state. */
static portMUX_TYPE  _rxMux = portMUX_INITIALIZER_UNLOCKED;

/** @brief Counter: unique frames delivered to poll(). */
static uint32_t _stat_rx = 0;

/** @brief Counter: duplicate copies discarded by the dedup logic. */
static uint32_t _stat_dupes = 0;

/** @brief Counter: sequence-number gaps observed (i.e. lost frames). */
static uint32_t _stat_gaps = 0;

/** @brief Counter: packets successfully queued for TX. */
static uint32_t _stat_tx = 0;

/** @brief Counter: TX attempts where both duplicate copies failed to queue. */
static uint32_t _stat_tx_err = 0;

// ESP-NOW receive callback signature changed between Arduino-ESP32 v2.x
// and v3.x (IDF 4.x vs 5.x). Branch on the IDF version.
#if ESP_IDF_VERSION_MAJOR >= 5
  /** @brief Receive callback signature for IDF 5.x. */
  #define ESPNOW_RECV_CB_SIG const esp_now_recv_info_t * info, const uint8_t * data, int len
  /** @brief Stub to suppress "unused parameter" warnings for IDF 5.x. */
  #define ESPNOW_RECV_CB_ARGS_UNUSED (void)info;
#else
  /** @brief Receive callback signature for IDF 4.x. */
  #define ESPNOW_RECV_CB_SIG const uint8_t * mac, const uint8_t * data, int len
  /** @brief Stub to suppress "unused parameter" warnings for IDF 4.x. */
  #define ESPNOW_RECV_CB_ARGS_UNUSED (void)mac;
#endif

/**
 * @brief  Internal ESP-NOW receive callback used in STANDALONE mode.
 *
 * Forwarded to handleIncomingPacket(). Declared up front so begin() can
 * register it without ordering issues.
 */
static void _onRecv(ESPNOW_RECV_CB_SIG);

/**
 * @brief  Initialise the ESP-NOW transport layer.
 *
 * Call once from the usermod's `connected()` callback after Wi-Fi is up.
 *
 * Auto-detects between two modes:
 *   - **HOSTED**: WLED's QuickEspNow already initialised ESP-NOW (e.g.
 *     `enableESPNow=true` for WiZmote or state-sync). We don't re-init
 *     and don't register our own receive callback. The caller is
 *     expected to forward incoming packets to handleIncomingPacket()
 *     via the AudioReactive::onEspNowMessage() override.
 *   - **STANDALONE**: Nothing else owns ESP-NOW. We init it, register
 *     our own receive callback, and own the radio.
 *
 * Both modes are byte-compatible on the wire.
 *
 * @param channel   Wi-Fi channel to pin to (e.g. WiFi.channel()).
 *                  Ignored in HOSTED mode (QuickEspNow sets the channel).
 * @param isSender  true on the FFT-producing master; false on receivers.
 *                  Use bit 0 of WLED's `audioSyncEnabled`.
 * @return true on success (radio ready to send/receive), false on
 *         initialisation or peer-registration failure.
 */
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

/**
 * @brief  Broadcast one audio-sync frame from the master.
 *
 * Sends two back-to-back copies of the same packet for packet-loss
 * resilience; receivers dedup on the sequence number. Caller passes the
 * raw 44-byte V2 audioSyncPacket via `payload` + `payloadLen`.
 *
 * @param payload     Pointer to a buffer containing the V2 audio sync
 *                    packet (typically `&transmitData`).
 * @param payloadLen  Must equal `EN_PAYLOAD` (44). Other values are
 *                    rejected.
 * @return true if at least one of the two copies was successfully
 *         queued for transmit; false otherwise (also true if the caller
 *         is not the sender or begin() has not been called, but no
 *         packet is sent in those cases).
 */
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

/**
 * @brief  Retrieve the most-recent received audio frame, if any.
 *
 * Returns true exactly once per arriving frame; subsequent calls
 * return false until another frame arrives. The raw 44-byte payload is
 * copied into `out`, suitable for passing directly to WLED's
 * `decodeAudioData()` cast as `uint8_t*`.
 *
 * Optionally returns a render deadline so the caller can synchronise
 * rendering to a shared time across slaves.
 *
 * @param[out] out             Buffer to fill with the V2 audioSyncPacket
 *                             payload (must be at least 44 bytes).
 * @param      outLen          Capacity of `out` in bytes (must be >=
 *                             EN_PAYLOAD).
 * @param[out] deadline_us     Optional. If non-null, filled with the
 *                             absolute local micros() at which this
 *                             frame should be rendered.
 * @param      renderDelayUs   Render budget in microseconds.
 *                             Recommended 10000-20000 (default 15000).
 * @return true if a fresh frame was returned in `out`; false if no new
 *         frame is available since the last call.
 */
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

/**
 * @brief  Inspect and consume an incoming ESP-NOW packet (HOSTED mode).
 *
 * Call from the audioreactive usermod's `onEspNowMessage()` override:
 * @code
 *   bool onEspNowMessage(uint8_t* sender, uint8_t* data, uint8_t len) override {
 *       return espnowAudioSync::handleIncomingPacket(data, len);
 *   }
 * @endcode
 *
 * If the packet matches the audio-sync magic bytes it is dedup'd and
 * latched for later retrieval by poll(); the function returns true,
 * signalling WLED's core dispatcher to skip its own WiZmote /
 * state-sync handling for this packet. Otherwise returns false and the
 * packet is left for WLED to process normally.
 *
 * In STANDALONE mode this is called automatically from the registered
 * ESP-NOW receive callback (_onRecv); the caller does not need to wire
 * it up.
 *
 * @param data  Raw bytes received from ESP-NOW.
 * @param len   Length of `data` in bytes.
 * @return true if the packet was an audio-sync frame (consumed);
 *         false otherwise (caller should handle it).
 */
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

/**
 * @brief  Query which integration mode begin() ended up in.
 *
 * @return true if running in HOSTED mode (alongside WLED's
 *         QuickEspNow); false if STANDALONE (owns the radio outright).
 */
inline bool isHosted() { return _hosted; }

/**
 * @brief Diagnostic snapshot of transport-layer counters.
 *
 * Exposed for surfacing on the WLED Info pane via addToJsonInfo().
 */
struct Stats {
    uint32_t rx;       ///< Unique frames delivered to poll().
    uint32_t dupes;    ///< Duplicate copies dropped by dedup.
    uint32_t gaps;     ///< Sequence gaps observed (lost frames).
    uint32_t tx;       ///< Packets successfully queued for TX.
    uint32_t tx_err;   ///< TX attempts where both copies failed.
};

/**
 * @brief  Snapshot the diagnostic counters.
 *
 * Cheap (a copy of five uint32_t). Safe to call from any task; counters
 * are read without locking because tearing between fields is harmless
 * for display purposes.
 *
 * @return Stats struct containing the current counter values.
 */
inline Stats stats() {
    return Stats{_stat_rx, _stat_dupes, _stat_gaps, _stat_tx, _stat_tx_err};
}

/**
 * @brief  STANDALONE-mode ESP-NOW receive callback.
 *
 * Runs in the Wi-Fi task (not an ISR). Forwards every incoming packet
 * straight to handleIncomingPacket().
 *
 * @note In HOSTED mode this function is never registered; WLED's
 *       QuickEspNow owns the receive callback and the usermod's
 *       onEspNowMessage() override is what invokes
 *       handleIncomingPacket() instead.
 */
static void _onRecv(ESPNOW_RECV_CB_SIG) {
    ESPNOW_RECV_CB_ARGS_UNUSED
    handleIncomingPacket(data, len);
}

} // namespace espnowAudioSync
