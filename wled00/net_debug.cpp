#include "wled.h"

#ifdef WLED_DEBUG_HOST

void NetworkDebugPrinter::resolveHostName() {
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
}

void NetworkDebugPrinter::flushBuffer() {
  if (buffer.length() == 0) return;
  
  resolveHostName();
  
  debugUdp.beginPacket(debugPrintHostIP, netDebugPrintPort);
  debugUdp.printf("%s %s", serverDescription, buffer.c_str());
  debugUdp.endPacket();
  buffer = "";
}

size_t NetworkDebugPrinter::write(uint8_t c) {
  if (!WLED_CONNECTED || !netDebugEnabled) return 0;
  
  buffer += (char)c;
  
  // Flush on newline or if buffer gets too large
  if (c == '\n' || buffer.length() >= MAX_BUFFER_SIZE) {
    flushBuffer();
  }
  
  return 1;
}

size_t NetworkDebugPrinter::write(const uint8_t *buf, size_t size) {
  if (!WLED_CONNECTED || buf == nullptr || !netDebugEnabled) return 0;
  
  // Check if we can find a newline in the buffer
  bool hasNewline = false;
  size_t lastNewlinePos = 0;
  
  for (size_t i = 0; i < size; i++) {
    buffer += (char)buf[i];
    if (buf[i] == '\n') {
      hasNewline = true;
      lastNewlinePos = i;
      flushBuffer();
    }
  }
  
  // If buffer is large but no newline was found, flush anyway
  if (!hasNewline && buffer.length() >= MAX_BUFFER_SIZE) {
    flushBuffer();
  }
  
  return size;
}

NetworkDebugPrinter NetDebug;

#endif