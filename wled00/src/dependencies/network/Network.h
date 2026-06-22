#ifdef ESP8266
  #include <ESP8266WiFi.h>
#else // ESP32
  #include <WiFi.h>
  #include <ETH.h>
  #include <memory>
#endif

#ifndef Network_h
#define Network_h

#ifdef ARDUINO_ARCH_ESP32
class AsyncDNS;
extern std::shared_ptr<AsyncDNS> DNSlookup;
#endif

class WLEDNetworkClass
{
public:
  IPAddress localIP();
  IPAddress subnetMask();
  IPAddress gatewayIP();
  void localMAC(uint8_t* MAC);
  bool isConnected();
  bool isEthernet();
};

extern WLEDNetworkClass WLEDNetwork;

// WLED historically used a helper named Network. arduino-esp32 3.x also
// provides a global NetworkManager named Network, so keep WLED's helper on a
// unique symbol and map existing WLED call sites to it after framework headers
// above have been included.
#define Network WLEDNetwork

#endif
