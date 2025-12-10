#ifndef WLED_NET_DEBUG_H
#define WLED_NET_DEBUG_H

#include <WString.h>
#include <WiFiUdp.h>

class NetworkDebugPrinter : public Print {
  private:
    WiFiUDP debugUdp; // needs to be here otherwise UDP messages get truncated upon destruction
    IPAddress debugPrintHostIP;
  public:
    virtual size_t write(uint8_t c);
    virtual size_t write(const uint8_t *buf, size_t s);
};

// use it on your linux/macOS with: nc -p 7868 -u -l -s <network ip>
extern NetworkDebugPrinter NetDebug;

#endif