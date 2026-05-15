#pragma once

// ESPnow WLED: Lightweight ESP-NOW driver. Only the API surface actually used by WLED  is implemented.

#ifndef WLED_DISABLE_ESPNOW
#ifdef ESP8266
  #include <espnow.h>
#else
  #include <esp_now.h>
#endif
// Broadcast MAC address constant
static const uint8_t ESPNOW_BROADCAST_ADDRESS[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Maximum number of ESP-NOW frames that may be in-flight (queued in the driver), capped to prevent memory exhaustion
#define ESPNOW_MAX_INFLIGHT 16

#ifdef ESP8266
  // Map ESP32 wifi_interface_t names to ESP8266 SDK interface
  #ifndef WIFI_IF_STA
    #define WIFI_IF_STA STATION_IF
  #endif
  #ifndef WIFI_IF_AP
    #define WIFI_IF_AP  SOFTAP_IF
  #endif
#endif

class WledEspNow {
public:
  // Callback types
  typedef void (*sent_cb_t)(uint8_t *address, uint8_t status);
  typedef void (*rcvd_cb_t)(uint8_t *address, uint8_t *data, uint8_t len, signed int rssi, bool broadcast);

  // Register callbacks before calling begin().
  void onDataSent(sent_cb_t cb) { _sentCB = cb; }
  void onDataRcvd(rcvd_cb_t cb) { _rcvdCB = cb; }

  // Start ESP-NOW on the given WiFi interface. Note: on ESP8266 the interface is currently not used, it is only needed to determine ROLE which is fixed to COMBO
  bool begin(uint8_t channel, uint8_t iface);

  // Start ESP-NOW in STA mode, inheriting the current WiFi channel.
  bool begin();

  // Stop ESP-NOW and release SDK resources.
  void stop();

  // Send data as a broadcast. addr is accepted for API compatibility but is always ignored — only the broadcast peer is used. Returns 0 on success.
  uint8_t send(const uint8_t *addr, const uint8_t *data, uint8_t len);

#ifdef ARDUINO_ARCH_ESP32
  // Narrow the AP-interface WiFi bandwidth for coexistence with ESP8266 peers used with iface: WIFI_IF_AP; bw: WIFI_BW_HT20 in wled.cpp
  void setWiFiBandwidth(uint8_t iface, uint8_t bw);
#endif

  // Accessible from file-static SDK callbacks in espnow.cpp:
  sent_cb_t _sentCB = nullptr;
  rcvd_cb_t _rcvdCB = nullptr;
  volatile int8_t _inFlight {0}; // frames queued in driver, not yet confirmed

private:
  bool _running = false;
};

/*
// start AI code, unreviewed, untested
// --- WledEspNowBroadcast -----------------------------------------------
// Thin facade matching the ESPNOWBroadcast interface from WLEDtubes so the
// Tubes usermod can be ported upstream with minimal modifications.
// Delegates actual sending to espNow; state is derived from statusESPNow.

// Allow override of callback array size before including this header.
#ifndef WLED_ESPNOW_MAX_REGISTERED_CALLBACKS
  #ifndef WLED_MAX_USERMODS
    // const.h is not included yet at this point; provide a safe default that
    // will be overridden by the actual WLED_MAX_USERMODS value once const.h
    // defines it.  The array is fixed-size, so use the larger ESP32 default.
    #define WLED_MAX_USERMODS 6
  #endif
  #define WLED_ESPNOW_MAX_REGISTERED_CALLBACKS (WLED_MAX_USERMODS + 1)
#endif

class WledEspNowBroadcast {
public:
  enum STATE { STOPPED = 0, STARTING, STARTED, MAX };

  // Broadcast a raw message (delegates to espNow.send).
  bool send(const uint8_t *msg, size_t len);

  // Receive callback type: (sender_mac, data, len, rssi)
  typedef void (*receive_callback_t)(const uint8_t *sender,
                                     const uint8_t *data,
                                     uint8_t len, int8_t rssi);
  bool registerCallback(receive_callback_t cb);
  bool removeCallback(receive_callback_t cb);

  // Optional filter invoked from within the ESP-NOW recv handler before the
  // registered callbacks.  Return false to discard the packet.
  // Only a single filter is active at a time; returns the previous filter.
  typedef bool (*receive_filter_t)(const uint8_t *sender,
                                   const uint8_t *data,
                                   uint8_t len, int8_t rssi);
  receive_filter_t registerFilter(receive_filter_t filter = nullptr);

  // Return the current broadcast state derived from the WLED statusESPNow flag.
  STATE getState() const;
  bool  isStarted() const { return getState() == STARTED; }

  // Dispatch received data to all registered callbacks (after filter).
  // Called from the ESP-NOW recv handler in espnow.cpp.
  void dispatch(const uint8_t *mac, const uint8_t *data,
                uint8_t len, int8_t rssi);

private:
  receive_callback_t _callbacks[WLED_ESPNOW_MAX_REGISTERED_CALLBACKS] = {nullptr};
  receive_filter_t   _filter = nullptr;
};
// end AI code, unreviewed, untested
*/
// Global instances (defined in espnow.cpp)
extern WledEspNow           espNow;
//extern WledEspNowBroadcast  espnowBroadcast;

#endif // WLED_DISABLE_ESPNOW
