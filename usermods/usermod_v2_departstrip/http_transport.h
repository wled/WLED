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
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  WiFiClientSecure clientSecure_;
#endif
};

} // namespace net
} // namespace departstrip

