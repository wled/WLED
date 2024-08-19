
#ifndef WLED_DISABLE_ESPNOW_NEW
#include <Arduino.h>
#include <atomic>

#if defined ESP32
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>

#elif defined ESP8266
#include <ESP8266WiFi.h>
#define WIFI_MODE_STA WIFI_STA
#else
#error "Unsupported platform"
#endif //ESP32

#include "espnow_broadcast.h"

#ifdef ESP32

#include <esp_now.h>
#include <esp_idf_version.h>
#include <freertos/ringbuf.h>

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
// Legacy Event Loop
ESP_EVENT_DEFINE_BASE(SYSTEM_EVENT);
#define WIFI_EVENT SYSTEM_EVENT
#define WIFI_EVENT_STA_START SYSTEM_EVENT_STA_START
#define WIFI_EVENT_STA_STOP SYSTEM_EVENT_STA_STOP
#define WIFI_EVENT_AP_START SYSTEM_EVENT_AP_START
#endif

//#define ESPNOW_DEBUGGING
//#define ESPNOW_CALLBACK_DEBUGGING // Serial is called from multiple threads

#define BROADCAST_ADDR_ARRAY_INITIALIZER {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
#define WLED_ESPNOW_WIFI_CHANNEL 1

#ifndef WLED_WIFI_POWER_SETTING
#define WLED_WIFI_POWER_SETTING WIFI_POWER_15dBm
#endif

typedef struct {
    uint8_t mac[6];
    uint8_t len;
    int8_t  rssi;
    uint8_t data[WLED_ESPNOW_MAX_MESSAGE_LENGTH];
} QueuedNetworkMessage;
static_assert(sizeof(QueuedNetworkMessage) == WLED_ESPNOW_MAX_MESSAGE_LENGTH+8, "QueuedNetworkMessage larger than needed");
static_assert(WLED_ESPNOW_MAX_MESSAGE_LENGTH <= ESP_NOW_MAX_DATA_LEN, "WLED_ESPNOW_MAX_MESSAGE_LENGTH must be <= 250 bytes");


class ESPNOWBroadcastImpl : public ESPNOWBroadcast {

    friend ESPNOWBroadcast;

    std::atomic<STATE> _state {STOPPED};
    STATE getState() const {
        return _state.load();
    }

    bool setupWiFi();

    void start();

    static esp_err_t onSystemEvent(void *ctx, system_event_t *event);

    static void onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    static void onESPNowRxCallback(const uint8_t *mac_addr, const uint8_t *data, int len);

    receive_filter_t _rxFilter = nullptr;

    class QueuedNetworkRingBuffer {
      protected:
        //QueuedNetworkMessage messages[WLED_ESPNOW_MAX_QUEUED_MESSAGES];
        RingbufHandle_t buf = nullptr;

      public:
        QueuedNetworkRingBuffer() {
            buf = xRingbufferCreateNoSplit(sizeof(QueuedNetworkMessage), WLED_ESPNOW_MAX_QUEUED_MESSAGES);
        }

        bool push(const uint8_t* mac, const uint8_t* data, uint8_t len, int8_t rssi);

        QueuedNetworkMessage* pop() {
            size_t size = 0;
            return (QueuedNetworkMessage*)xRingbufferReceive(buf, &size, 0);
        }

        void popComplete(QueuedNetworkMessage* msg) {
            vRingbufferReturnItem(buf, (void *)msg);
        }
    };

    QueuedNetworkRingBuffer queuedNetworkRingBuffer {};

};

ESPNOWBroadcastImpl espnowBroadcastImpl {};
#endif // ESP32

ESPNOWBroadcast espnowBroadcast {};


ESPNOWBroadcast::STATE ESPNOWBroadcast::getState() const {
#ifdef ESP32
    return espnowBroadcastImpl.getState();
#else
    return ESPNOWBroadcast::STOPPED;
#endif
}

bool ESPNOWBroadcast::setup() {

    static bool setup = false;
#ifdef ESP32
    if (setup) {
        return true;
    }

    #ifdef ESPNOW_DEBUGGING
        delay(2000);
    #endif

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
    tcpip_adapter_init();
    esp_event_loop_init(ESPNOWBroadcastImpl::onSystemEvent, nullptr);
#else

    auto err = esp_event_loop_create_default();
    if ( ESP_OK != err && ESP_ERR_INVALID_STATE != err ) {
        Serial.printf("esp_event_loop_create_default() err %d\n", err);
        return false;
    }
    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_START, ESPNOWBroadcastImpl::onWiFiEvent, nullptr, nullptr);
    if ( ESP_OK != err ) {
        Serial.printf("esp_event_handler_instance_register(WIFI_EVENT_STA_START) err %d\n", err);
        return false;
    }
    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_STOP, ESPNOWBroadcastImpl::onWiFiEvent, nullptr, nullptr);
    if ( ESP_OK != err ) {
        Serial.printf("esp_event_handler_instance_register(WIFI_EVENT_STA_STOP) err %d\n", err);
        return false;
    }
    err = esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_AP_START, ESPNOWBroadcastImpl::onWiFiEvent, nullptr, nullptr);
    if ( ESP_OK != err ) {
        Serial.printf("esp_event_handler_instance_register(WIFI_EVENT_AP_START) err %d\n", err);
        return false;
    }
#endif

    setup = espnowBroadcastImpl.setupWiFi();
#endif //ESP32
    return setup;
}

#ifdef ESP32
bool ESPNOWBroadcastImpl::setupWiFi() {
    Serial.println("ESPNOWBroadcast::setupWiFi()");

    _state.exchange(STOPPED);

    // To enable ESPNow, we need to be in WIFI_STA mode
    if ( !WiFi.mode(WIFI_STA) ) {
        Serial.println("WiFi.mode() failed");
        return false;
    }
    // and not have the WiFi connect
    // Calling discount with tigger an async Wifi Event
    if ( !WiFi.disconnect(false, true) ) {
        Serial.println("WiFi.disconnect() failed");
        return false;
    }

    return true;
}
#endif //ESP32


void ESPNOWBroadcast::loop(size_t maxMessagesToProcess /*= 1*/) {
#ifdef ESP32
    switch (espnowBroadcastImpl._state.load()) {
        case ESPNOWBroadcast::STARTING:
            // if WiFI is in starting state, actually stat ESPNow from our main task thread.
            espnowBroadcastImpl.start();
            break;
        case ESPNOWBroadcast::STARTED: {
            auto ndx = maxMessagesToProcess;
            while(ndx-- > 0) {
                auto *msg = espnowBroadcastImpl.queuedNetworkRingBuffer.pop();
                if (msg) {
                    auto callback = _rxCallbacks;
                    while( *callback ) {
                        (*callback)(msg->mac, msg->data, msg->len, msg->rssi);
                        callback++;
                    }
                    espnowBroadcastImpl.queuedNetworkRingBuffer.popComplete(msg);
                } else {
                    break;
                }
            }
            break;
        }
        default:
            break;
    }
#endif // ESP32
}

bool ESPNOWBroadcast::send(const uint8_t* msg, size_t len) {
#ifdef ESP32
    static const uint8_t broadcast[] = BROADCAST_ADDR_ARRAY_INITIALIZER;
    auto err = esp_now_send(broadcast, msg, len);
#ifdef ESPNOW_DEBUGGING
    if (ESP_OK != err) {
        Serial.printf( "esp_now_send() failed %d\n", err);
    }
#endif
    return ESP_OK == err;
#else
    return false;
#endif
}

bool ESPNOWBroadcast::registerCallback( ESPNOWBroadcast::receive_callback_t callback ) {
    // last element is always null
    size_t ndx;
    for (ndx = 0; ndx < _rxCallbacksSize-1; ndx++) {
        if (nullptr == _rxCallbacks[ndx]) {
            _rxCallbacks[ndx] = callback;
            break;
        }
    }
    return ndx < _rxCallbacksSize;
}

bool ESPNOWBroadcast::removeCallback( ESPNOWBroadcast::receive_callback_t callback ) {
    size_t ndx;
    for (ndx = 0; ndx < _rxCallbacksSize-1; ndx++) {
        if (_rxCallbacks[ndx] == callback ) {
            break;
        }
    }

    for (; ndx < _rxCallbacksSize-1; ndx++) {
        _rxCallbacks[ndx] = _rxCallbacks[ndx+1];
    }

    return ndx < _rxCallbacksSize;

}

ESPNOWBroadcast::receive_filter_t ESPNOWBroadcast::registerFilter( ESPNOWBroadcast::receive_filter_t filter ) {
    auto old = espnowBroadcastImpl._rxFilter;
    espnowBroadcastImpl._rxFilter = filter;
    return old;
}


#ifdef ESP32

void ESPNOWBroadcastImpl::start() {

    Serial.println("starting ESPNow");

    if ( WiFi.mode(WIFI_STA) ) {
        auto status = WiFi.status();
        if ( status >= WL_DISCONNECTED ) {
            if (esp_wifi_start() == ESP_OK) {
                if (!WiFi.setTxPower(WLED_WIFI_POWER_SETTING)) {
                    auto power = WiFi.getTxPower();
                    Serial.printf("setTxPower(%d) failed. getTX: %d\n", WLED_WIFI_POWER_SETTING, power);
                }
                if (esp_now_init() == ESP_OK) {
                    if (esp_now_register_recv_cb(ESPNOWBroadcastImpl::onESPNowRxCallback) == ESP_OK) {
                        static esp_now_peer_info_t peer = {
                            BROADCAST_ADDR_ARRAY_INITIALIZER,
                            {0},
                            WLED_ESPNOW_WIFI_CHANNEL,
                            WIFI_IF_STA,
                            false,
                            NULL
                            };

                        if (esp_now_add_peer(&peer) == ESP_OK) {
                            ESPNOWBroadcast::STATE starting {ESPNOWBroadcast::STARTING};
                            if (_state.compare_exchange_strong(starting, ESPNOWBroadcast::STARTED)) {
#ifdef ESPNOW_DEBUGGING
                                Serial.println("ESPNOWBroadcast started :)");
#endif
                                return;
                            } else {
#ifdef ESPNOW_DEBUGGING
                                Serial.println("atomic state out of sync");
#endif
                            }
                        } else {
#ifdef ESPNOW_DEBUGGING
                            Serial.println("esp_now_add_peer failed");
#endif
                        }
                    } else {
#ifdef ESPNOW_DEBUGGING
                        Serial.println("esp_now_register_recv_cb failed");
#endif
                    }
                } else {
#ifdef ESPNOW_DEBUGGING
                    Serial.println("esp_now_init_init failed");
#endif
                }
            } else {
#ifdef ESPNOW_DEBUGGING
                Serial.println("esp_wifi_start failed");
#endif
            }
        } else {
#ifdef ESPNOW_DEBUGGING
            Serial.printf("WiFi.status not disconnected - %d\n", status);
#endif
        }
    } else {
#ifdef ESPNOW_DEBUGGING
        Serial.println("WiFi.mode failed");
#endif
    }
    Serial.println("restarting ESPNow");
    setupWiFi();
}

esp_err_t ESPNOWBroadcastImpl::onSystemEvent(void *ctx, system_event_t *event) {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
    onWiFiEvent(ctx, SYSTEM_EVENT, event->event_id, nullptr );
#endif
    return ESP_OK;
}


void ESPNOWBroadcastImpl::onWiFiEvent(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if ( event_base == WIFI_EVENT ) {

#ifdef ESPNOW_DEBUGGING
    #if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
            Serial.printf("WiFiEvent( %d )\n", event_id );
    #else
            Serial.printf("WiFiEvent( %s )\n", WiFi.eventName((arduino_event_id_t)event_id) );
    #endif
#endif
        switch (event_id) {
            case WIFI_EVENT_STA_START: {
                ESPNOWBroadcast::STATE stopped {ESPNOWBroadcast::STOPPED};
                espnowBroadcastImpl._state.compare_exchange_strong(stopped, ESPNOWBroadcast::STARTING);
                break;
            }

            case WIFI_EVENT_STA_STOP:
            case WIFI_EVENT_AP_START: {
                ESPNOWBroadcast::STATE started {ESPNOWBroadcast::STARTED};
                ESPNOWBroadcast::STATE starting {ESPNOWBroadcast::STARTING};
                if (espnowBroadcastImpl._state.compare_exchange_strong(started, ESPNOWBroadcast::STOPPED) ||
                    espnowBroadcastImpl._state.compare_exchange_strong(starting, ESPNOWBroadcast::STOPPED)) {
#ifdef ESPNOW_DEBUGGING
                    Serial.println("WiFi connected: stop broadcasting");
#endif
                    esp_now_unregister_recv_cb();
                    esp_now_deinit();
                }
                break;
            }
        }
    }
}

typedef struct {
    uint16_t frame_head;
    uint16_t duration;
    uint8_t destination_address[6];
    uint8_t source_address[6];
    uint8_t broadcast_address[6];
    uint16_t sequence_control;

    uint8_t category_code;
    uint8_t organization_identifier[3]; // 0x18fe34
    uint8_t random_values[4];
    struct {
        uint8_t element_id;                 // 0xdd
        uint8_t lenght;                     //
        uint8_t organization_identifier[3]; // 0x18fe34
        uint8_t type;                       // 4
        uint8_t version;
        uint8_t body[0];
    } vendor_specific_content;
} __attribute__ ((packed)) espnow_frame_format_t;

void ESPNOWBroadcastImpl::onESPNowRxCallback(const uint8_t *mac, const uint8_t *data, int len) {
    //const espnow_frame_format_t* espnow_data = (espnow_frame_format_t*)(data - sizeof (espnow_frame_format_t));
    const wifi_promiscuous_pkt_t* promiscuous_pkt = (wifi_promiscuous_pkt_t*)(data - sizeof (wifi_pkt_rx_ctrl_t) - sizeof (espnow_frame_format_t));
    int8_t rssi = 0;
    try {
        auto rssi32 = promiscuous_pkt->rx_ctrl.rssi;
        rssi = rssi32 <= -128 ? -127 : rssi32 > 0 ? 0 : rssi32;
    } catch(...) {
        // be safe about accessing memory that isn't directly exposed to the callback
        rssi = 0;
    }

    if (espnowBroadcastImpl._rxFilter) {
        if (!espnowBroadcastImpl._rxFilter(mac, data, len, rssi)) {
            return;
        }
    }

    if(!espnowBroadcastImpl.queuedNetworkRingBuffer.push(mac, data, len, rssi)) {
        Serial.printf("Failed to queue message (%d bytes) to ring buffer.  Dropping message\n", len);
    } else {
#ifdef ESPNOW_CALLBACK_DEBUGGING
        char buf[128];
        sprintf(buf, "Received %d bytes from %x:%x:%x:%x:%x:%x RSSI %d", len,
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
            rssi
            );
        Serial.println(buf);
#endif
    }
}

bool ESPNOWBroadcastImpl::QueuedNetworkRingBuffer::push(const uint8_t* mac, const uint8_t* data, uint8_t len, int8_t rssi) {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
    QueuedNetworkMessage msg[1];
    if (len <= sizeof(msg->data)) {
        memcpy(msg->mac, mac, sizeof(msg->mac));
        memcpy(&(msg->data), data, len);
        msg->len = len;
        msg->rssi = rssi;

        if (pdTRUE == xRingbufferSend(buf, (void**)&msg, sizeof(*msg), 0)) {
            return true;
        }
    }
    return false;
#else
    QueuedNetworkMessage* msg = nullptr;
    if (len <= sizeof(msg->data)) {
        if (pdTRUE == xRingbufferSendAcquire(buf, (void**)&msg, sizeof(*msg), 0)) {
            memcpy(msg->mac, mac, sizeof(msg->mac));
            memcpy(&(msg->data), data, len);
            msg->len = len;
            msg->rssi = rssi;
            xRingbufferSendComplete(buf, msg);
            return true;
        }
    }
    return false;
#endif
}

#endif // ESP32

#endif