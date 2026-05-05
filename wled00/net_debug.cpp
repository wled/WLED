#include "wled.h"

#ifdef WLED_ENABLE_NET_DEBUG

size_t NetworkDebugPrinter::write(uint8_t c) {
  if (!WLED_CONNECTED || !netDebugEnabled) return 0;

  debugUdp.beginPacket(netDebugPrintIP, netDebugPrintPort);
  debugUdp.write(c);
  debugUdp.endPacket();
  return 1;
}

size_t NetworkDebugPrinter::write(const uint8_t *buf, size_t size) {
  if (!WLED_CONNECTED || buf == nullptr || !netDebugEnabled) return 0;

  debugUdp.beginPacket(netDebugPrintIP, netDebugPrintPort);
  size = debugUdp.write(buf, size);
  debugUdp.endPacket();
  return size;
}

NetworkDebugPrinter NetDebug;

#endif
