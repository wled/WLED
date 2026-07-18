#include "wled.h"
#if !defined(WLED_DISABLE_ESPNOW) && defined(ARDUINO_ARCH_ESP32)
#include <atomic>

// ESP-NOW callbacks run outside WLED's loop context. Keep callback work small: copy received
// frames into a small queue and send one outbound frame at a time.
// Espressif callback guidance: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_now.html

namespace {

static constexpr uint8_t ESPNOW_TRANSPORT_RX_QUEUE_SIZE = 2;
static constexpr uint8_t ESPNOW_TRANSPORT_RX_PER_LOOP = 2;
static constexpr size_t ESPNOW_TRANSPORT_MAX_PAYLOAD = 250;

struct EspNowTransportFrame {
  uint8_t address[6];
  uint8_t data[ESPNOW_TRANSPORT_MAX_PAYLOAD];
  uint8_t len;
  int8_t rssi;
  bool broadcast;
};

static EspNowTransportFrame* rxQueue = nullptr;
static uint8_t rxRead = 0, rxWrite = 0, rxCount = 0;
static std::atomic_flag rxLock = ATOMIC_FLAG_INIT;
static std::atomic<bool> txInFlight{false};
static std::atomic<bool> sentEventPending{false};
static uint8_t sentAddress[6] = {};
static uint8_t sentStatus = 0;
static std::atomic<bool> transportActive{false};
static bool transportUsesAP = false;

static void resetQueues() {
  while (rxLock.test_and_set(std::memory_order_acquire)) delay(1);
  rxRead = rxWrite = rxCount = 0;
  rxLock.clear(std::memory_order_release);
  txInFlight.store(false, std::memory_order_release);
  sentEventPending.store(false, std::memory_order_release);
}

static void freeReceiveQueue() {
  free(rxQueue);
  rxQueue = nullptr;
}

static void queueReceivedFrame(const uint8_t* address, const uint8_t* data, size_t len) {
  if (!transportActive.load(std::memory_order_acquire) || !rxQueue || !address || !data || !len ||
      len > ESPNOW_TRANSPORT_MAX_PAYLOAD) return;
  if (rxLock.test_and_set(std::memory_order_acquire)) return;
  if (rxCount < ESPNOW_TRANSPORT_RX_QUEUE_SIZE) {
    EspNowTransportFrame &frame = rxQueue[rxWrite];
    memcpy(frame.address, address, sizeof(frame.address));
    memcpy(frame.data, data, len);
    frame.len = len;
    frame.rssi = 0; // legacy ESP-NOW callbacks do not provide portable RSSI metadata
    frame.broadcast = data[0] == 'W'; // only WLED sync packets require broadcast classification
    rxWrite = (rxWrite + 1) % ESPNOW_TRANSPORT_RX_QUEUE_SIZE;
    rxCount++;
  }
  rxLock.clear(std::memory_order_release);
}

#ifdef ESP8266
static void onEspNowReceive(uint8_t* address, uint8_t* data, uint8_t len) {
  queueReceivedFrame(address, data, len);
}

static void onEspNowSent(uint8_t* address, uint8_t status) {
  if (!transportActive.load(std::memory_order_acquire)) return;
  memcpy(sentAddress, address, sizeof(sentAddress));
  sentStatus = status;
  sentEventPending.store(true, std::memory_order_release);
}
#else
static void onEspNowReceive(const uint8_t* address, const uint8_t* data, int len) {
  if (len > 0) queueReceivedFrame(address, data, size_t(len));
}

static void onEspNowSent(const uint8_t* address, esp_now_send_status_t status) {
  if (!transportActive.load(std::memory_order_acquire)) return;
  memcpy(sentAddress, address, sizeof(sentAddress));
  sentStatus = uint8_t(status);
  sentEventPending.store(true, std::memory_order_release);
}
#endif

static bool checkForPeer(const uint8_t* address) {
#ifdef ESP8266
  if (esp_now_is_peer_exist((uint8_t*)address)) return true;
  return esp_now_add_peer((uint8_t*)address, ESP_NOW_ROLE_COMBO, 0, nullptr, 0) == 0;
#else
  if (esp_now_is_peer_exist(address)) return true;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, address, sizeof(peer.peer_addr));
  peer.channel = 0;
  peer.ifidx = transportUsesAP ? WIFI_IF_AP : WIFI_IF_STA;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
#endif
}

static bool popReceivedFrame(EspNowTransportFrame &frame) {
  if (rxLock.test_and_set(std::memory_order_acquire)) return false;
  if (!rxCount) {
    rxLock.clear(std::memory_order_release);
    return false;
  }
  frame = rxQueue[rxRead];
  rxRead = (rxRead + 1) % ESPNOW_TRANSPORT_RX_QUEUE_SIZE;
  rxCount--;
  rxLock.clear(std::memory_order_release);
  return true;
}

} // namespace

bool espNowTransportBegin(uint8_t channel, bool useAP) {
  espNowTransportStop();
  resetQueues();
  rxQueue = (EspNowTransportFrame*)d_malloc(sizeof(EspNowTransportFrame) * ESPNOW_TRANSPORT_RX_QUEUE_SIZE);
  if (!rxQueue) return false;
  transportUsesAP = useAP;

#ifdef ESP8266
  if (channel >= 1 && channel <= 13 && WiFi.channel() != channel) wifi_set_channel(channel);
  if (esp_now_init() != 0) { freeReceiveQueue(); return false; }
  if (esp_now_set_self_role(ESP_NOW_ROLE_COMBO) != 0) { esp_now_deinit(); freeReceiveQueue(); return false; }
  if (esp_now_register_recv_cb(onEspNowReceive) != 0 || esp_now_register_send_cb(onEspNowSent) != 0) {
    esp_now_deinit();
    freeReceiveQueue();
    return false;
  }
#else
  (void)channel; // WiFi owns the settled STA/AP home channel before this function is called
  if (esp_now_init() != ESP_OK) { freeReceiveQueue(); return false; }
  if (esp_now_register_recv_cb(onEspNowReceive) != ESP_OK ||
      esp_now_register_send_cb(onEspNowSent) != ESP_OK) {
    esp_now_deinit();
    freeReceiveQueue();
    return false;
  }
#endif
  transportActive.store(true, std::memory_order_release);
  DEBUG_PRINTF_P(PSTR("ESP-NOW transport ready: requestedCh=%u actualCh=%u interface=%s queue=%u\n"),
                 channel, WiFi.channel(), useAP ? "AP" : "STA", ESPNOW_TRANSPORT_RX_QUEUE_SIZE);
  return true;
}

void espNowTransportStop() {
  if (!transportActive.load(std::memory_order_acquire)) return;
  transportActive.store(false, std::memory_order_release);
  DEBUG_PRINTF_P(PSTR("ESP-NOW transport stopping: ch=%u interface=%s rx=%u inFlight=%u\n"),
                 WiFi.channel(), transportUsesAP ? "AP" : "STA", rxCount,
                 txInFlight.load(std::memory_order_acquire));
#ifdef ESP8266
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
#else
  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();
#endif
  esp_now_deinit();
  resetQueues();
  freeReceiveQueue();
}

bool espNowTransportReadyToSend() {
  return transportActive.load(std::memory_order_acquire) &&
         !txInFlight.load(std::memory_order_acquire) &&
         !sentEventPending.load(std::memory_order_acquire);
}

uint8_t espNowTransportSend(const uint8_t* address, const uint8_t* data, size_t len) {
  if (!transportActive.load(std::memory_order_acquire) || !address || !data || !len ||
      len > ESPNOW_TRANSPORT_MAX_PAYLOAD || !espNowTransportReadyToSend()) return 1;
  if (!checkForPeer(address)) return 1;
  txInFlight.store(true, std::memory_order_release);
#ifdef ESP8266
  const int result = esp_now_send((uint8_t*)address, (uint8_t*)data, len);
#else
  const esp_err_t result = esp_now_send(address, data, len);
#endif
  if (result != 0) {
    txInFlight.store(false, std::memory_order_release);
    espNowSentCB((uint8_t*)address, 1);
    return 1;
  }
  return 0;
}

void handleEspNowTransport() {
  uint8_t completedAddress[6];
  uint8_t completedStatus = 0;
  const bool haveSentEvent = sentEventPending.load(std::memory_order_acquire);
  if (haveSentEvent) {
    memcpy(completedAddress, sentAddress, sizeof(completedAddress));
    completedStatus = sentStatus;
    sentEventPending.store(false, std::memory_order_release);
    txInFlight.store(false, std::memory_order_release);
  }
  if (haveSentEvent) espNowSentCB(completedAddress, completedStatus);
  EspNowTransportFrame frame;
  for (uint8_t i = 0; i < ESPNOW_TRANSPORT_RX_PER_LOOP && popReceivedFrame(frame); i++)
    espNowReceiveCB(frame.address, frame.data, frame.len, frame.rssi, frame.broadcast);
}
#endif // !WLED_DISABLE_ESPNOW && ARDUINO_ARCH_ESP32
