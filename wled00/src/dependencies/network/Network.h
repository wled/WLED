#ifdef ESP8266
  #include <ESP8266WiFi.h>
#else // ESP32
  #include <WiFi.h>
  #include <ETH.h>
#endif

#ifndef Network_h
#define Network_h

class WLEDNetworkClass
{
public:
  IPAddress localIP();
  IPAddress subnetMask();
  IPAddress gatewayIP();
  void localMAC(uint8_t* MAC);
  bool isConnected();
  bool isEthernet();

#if defined(ARDUINO_ARCH_ESP32) && defined(LWIP_IPV6) && ESP_IDF_VERSION_MAJOR >= 5
  void enableIPv6();
  bool hasLinkLocalIPv6();
  bool hasGlobalIPv6();
  IPAddress localIPv6LinkLocal();
  IPAddress localIPv6Global();
#endif
};

extern WLEDNetworkClass WLEDNetwork;

#endif