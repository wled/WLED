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
  virtual ~RestJsonClient() = default;

  // Returns pointer to internal document on success, nullptr on failure.
  DynamicJsonDocument* getJson(const char* url);

  void resetRateLimit();

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
};
