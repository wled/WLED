#pragma once

// Lightweight REST client that reuses a fixed JSON buffer to avoid
// heap fragmentation caused by repeated allocations.

#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "wled.h"

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266HTTPClient.h>
#else
#include <HTTPClient.h>
#endif

class RestJsonClient {
public:
  RestJsonClient();
  // Optionally construct with a specific socket timeout (ms).
  explicit RestJsonClient(uint32_t socketTimeoutMs);
  virtual ~RestJsonClient() = default;

  // Non-copyable, non-movable to avoid duplicating HTTPClient and large JSON buffer.
  RestJsonClient(const RestJsonClient&) = delete;
  RestJsonClient& operator=(const RestJsonClient&) = delete;
  RestJsonClient(RestJsonClient&&) = delete;
  RestJsonClient& operator=(RestJsonClient&&) = delete;

  // Returns pointer to internal document on success, nullptr on failure.
  DynamicJsonDocument* getJson(const char* url);

  void resetRateLimit();

  // Configure/read the underlying socket (Stream) timeout in milliseconds.
  // This is applied to the WiFiClient/WiFiClientSecure before HTTPClient.begin().
  void setSocketTimeoutMs(uint32_t ms) { socketTimeoutMs_ = ms; }
  uint32_t socketTimeoutMs() const { return socketTimeoutMs_; }

protected:
  static constexpr unsigned RATE_LIMIT_MS = 10u * 1000u; // 10 seconds
#if defined(ARDUINO_ARCH_ESP8266)
  static constexpr size_t MAX_JSON_SIZE = 16 * 1024;     // 16kB on 8266
#else
  static constexpr size_t MAX_JSON_SIZE = 32 * 1024;     // 32kB on ESP32
#endif
  
private:
  HTTPClient http_;
  unsigned long lastFetchMs_;
  DynamicJsonDocument doc_;
  static constexpr uint32_t DEFAULT_SOCKET_TIMEOUT_MS = 7000u; // 7 seconds
  uint32_t socketTimeoutMs_ = DEFAULT_SOCKET_TIMEOUT_MS;
};
