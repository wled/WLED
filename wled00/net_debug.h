#ifndef WLED_NET_DEBUG_H
#define WLED_NET_DEBUG_H

#include <WString.h>
#include <WiFiUdp.h>

class NetworkDebugPrinter : public Print {
  private:
    WiFiUDP debugUdp;
    IPAddress debugPrintHostIP;
    String buffer;
    const size_t MAX_BUFFER_SIZE = 1024; // Safety limit
    void resolveHostName();
    void flushBuffer();
  public:
    virtual size_t write(uint8_t c);
    virtual size_t write(const uint8_t *buf, size_t s);
};
extern NetworkDebugPrinter NetDebug;

#endif