#pragma once

#include "wled.h"
#include <WiFiClient.h>
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  #include <WiFiClientSecure.h>
#endif

namespace departstrip {
namespace net {

class HttpTransport {
public:
  HttpTransport() = default;

  WiFiClient* begin(const String& url, uint32_t timeoutMs, bool& usedSecure);
  void end(bool usedSecure);

private:
  WiFiClient client_;
  String clientHost_;
  uint16_t clientPort_ = 0;
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  WiFiClientSecure clientSecure_;
  String secureHost_;
  uint16_t securePort_ = 0;
  bool secureConfigured_ = false;
#endif
};

} // namespace net
} // namespace departstrip
