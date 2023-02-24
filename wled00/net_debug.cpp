#include "wled.h"

#ifdef WLED_DEBUG_HOST //WLEDMM: this looks unnecesarry as .h is not included anyway if no netdebug

size_t NetworkDebugPrinter::write(uint8_t c) {
  if (!WLED_CONNECTED || !netDebugEnabled) return 0;

  //WLEDMM: init IP moved to initInterfaces
  
  debugUdp.beginPacket(netDebugPrintIP, netDebugPrintPort); //WLEDMM: using netDebugPrintIP instead of debugPrintHostIP
  debugUdp.write(c);
  debugUdp.endPacket();
  return 1;
}

size_t NetworkDebugPrinter::write(const uint8_t *buf, size_t size) {
  if (!WLED_CONNECTED || buf == nullptr || !netDebugEnabled) return 0;

  //WLEDMM: init IP moved to initInterfaces

  debugUdp.beginPacket(netDebugPrintIP, netDebugPrintPort); //WLEDMM: using netDebugPrintIP instead of debugPrintHostIP
  size = debugUdp.write(buf, size);
  debugUdp.endPacket();
  return size;
}

NetworkDebugPrinter NetDebug;

#endif
