#include "http_transport.h"

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

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)
  if (isHttps) {
    usedSecure = true;
    bool needReset = !clientSecure_.connected();
    if (haveEndpoint) {
      if (!secureHost_.length() || host != secureHost_ || port != securePort_) {
        needReset = true;
      }
    }
    if (needReset) {
      clientSecure_.stop();
      clientSecure_ = WiFiClientSecure();
      clientSecure_.setInsecure();
      clientSecure_.setNoDelay(true);
      secureHost_ = haveEndpoint ? host : String();
      securePort_ = haveEndpoint ? port : 0;
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
      clientSecure_ = WiFiClientSecure();
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
