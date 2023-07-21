#include "wled.h"

#ifdef WLED_DEBUG_HOST

size_t NetworkDebugPrinter::write(uint8_t c) {
  if (!WLED_CONNECTED || !netDebugEnabled) return 0;

  if (!debugPrintHostIP && !debugPrintHostIP.fromString(netDebugPrintHost)) {
    #ifdef ESP8266
      WiFi.hostByName(netDebugPrintHost, debugPrintHostIP, 750);
    #else
      #ifdef WLED_USE_ETHERNET
        ETH.hostByName(netDebugPrintHost, debugPrintHostIP);
      #else
        WiFi.hostByName(netDebugPrintHost, debugPrintHostIP);
      #endif
    #endif
  }

  debugUdp.beginPacket(debugPrintHostIP, netDebugPrintPort);
  debugUdp.write(c);
  debugUdp.endPacket();
  return 1;
}

size_t NetworkDebugPrinter::write(const uint8_t *buf, size_t size) {
  if (!WLED_CONNECTED || buf == nullptr || !netDebugEnabled) return 0;

  if (!debugPrintHostIP && !debugPrintHostIP.fromString(netDebugPrintHost)) {
    #ifdef ESP8266
      WiFi.hostByName(netDebugPrintHost, debugPrintHostIP, 750);
    #else
      #ifdef WLED_USE_ETHERNET
        ETH.hostByName(netDebugPrintHost, debugPrintHostIP);
      #else
        WiFi.hostByName(netDebugPrintHost, debugPrintHostIP);
      #endif
    #endif
  }

  debugUdp.beginPacket(debugPrintHostIP, netDebugPrintPort);
  size = debugUdp.write(buf, size);
  debugUdp.endPacket();
  return size;
}

NetworkDebugPrinter NetDebug;

#endif
