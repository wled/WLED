#include "http_transport.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "esp_heap_caps.h"
#endif

namespace {
bool parseEndpoint(const String& url, bool isHttps, String& hostOut, uint16_t& portOut) {
  int schemeIdx = url.indexOf(F("://"));
  int hostStart = (schemeIdx >= 0) ? schemeIdx + 3 : 0;
  int pathIdx = url.indexOf('/', hostStart);
  String authority = (pathIdx >= 0) ? url.substring(hostStart, pathIdx)
                                    : url.substring(hostStart);
  authority.trim();
  if (!authority.length()) return false;

  int atIdx = authority.lastIndexOf('@');
  if (atIdx >= 0) authority = authority.substring(atIdx + 1);

  int colonIdx = authority.lastIndexOf(':');
  if (colonIdx >= 0) {
    hostOut = authority.substring(0, colonIdx);
    String portStr = authority.substring(colonIdx + 1);
    uint32_t parsed = (uint32_t)portStr.toInt();
    if (parsed == 0 || parsed > 65535) parsed = isHttps ? 443u : 80u;
    portOut = static_cast<uint16_t>(parsed);
  } else {
    hostOut = authority;
    portOut = isHttps ? 443 : 80;
  }

  hostOut.trim();
  if (!hostOut.length()) return false;
  return true;
}
} // namespace

namespace departstrip {
namespace net {

WiFiClient* HttpTransport::begin(const String& url, uint32_t timeoutMs, bool& usedSecure) {
  bool isHttps = url.startsWith(F("https://")) || url.startsWith(F("HTTPS://"));
  usedSecure = false;

  String host;
  uint16_t port = isHttps ? 443 : 80;
  bool haveEndpoint = parseEndpoint(url, isHttps, host, port);

#if defined(WLED_DEBUG)
  size_t largest = 0;
  size_t freeHeap = 0;
#if defined(ARDUINO_ARCH_ESP32)
  largest = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
  freeHeap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
#else
  largest = ESP.getMaxAllocHeap();
  freeHeap = ESP.getFreeHeap();
#endif
  DEBUG_PRINTF("DepartStrip: HttpTransport::begin: %s host=%s port=%u largest=%u free=%u\n",
               isHttps ? "https" : "http",
               haveEndpoint ? host.c_str() : "(unknown)",
               (unsigned)port,
               (unsigned)largest,
               (unsigned)freeHeap);
#endif

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  if (isHttps) {
    usedSecure = true;
    if (!secureConfigured_) {
      clientSecure_.setInsecure();
      clientSecure_.setNoDelay(true);
      secureConfigured_ = true;
    }
    if (haveEndpoint) {
      if (!secureHost_.length() || host != secureHost_ || port != securePort_) {
        if (clientSecure_.connected()) clientSecure_.stop();
        secureHost_ = host;
        securePort_ = port;
      }
    } else {
      secureHost_.clear();
      securePort_ = 0;
    }
    clientSecure_.setTimeout(timeoutMs);
    clientSecure_.setNoDelay(true);
    return &clientSecure_;
  }
#else
  (void)isHttps;
  (void)haveEndpoint;
  (void)host;
  (void)port;
#endif

  bool needReset = !client_.connected();
  if (haveEndpoint) {
    if (!clientHost_.length() || host != clientHost_ || port != clientPort_) {
      needReset = true;
    }
  }
  if (needReset) {
    client_.stop();
    client_ = WiFiClient();
    client_.setNoDelay(true);
    clientHost_ = haveEndpoint ? host : String();
    clientPort_ = haveEndpoint ? port : 0;
  }
  client_.setTimeout(timeoutMs);
  client_.setNoDelay(true);
  return &client_;
}

void HttpTransport::end(bool usedSecure) {
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  if (usedSecure) {
    if (!clientSecure_.connected()) {
      clientSecure_.stop();
      secureHost_.clear();
      securePort_ = 0;
    }
    return;
  }
#else
  (void)usedSecure;
#endif
  if (!client_.connected()) {
    client_.stop();
    client_ = WiFiClient();
    clientHost_.clear();
    clientPort_ = 0;
  }
}

} // namespace net
} // namespace departstrip
