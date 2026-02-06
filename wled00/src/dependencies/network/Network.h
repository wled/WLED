#ifdef ESP8266
  #include <ESP8266WiFi.h>
#else // ESP32
  #include <WiFi.h>
  #include <ETH.h>
#endif

#ifndef Network_h
#define Network_h

// Arduino-ESP32 v3.x (IDF 5+) defines its own 'Network' global as NetworkManager.
// Rename WLED's Network to avoid the conflict. The #define ensures all existing
// references to 'Network.localIP()' etc. transparently use the renamed instance.
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
  #define Network WLEDNetwork
#endif

class NetworkClass
{
public:
  IPAddress localIP();
  IPAddress subnetMask();
  IPAddress gatewayIP();
  void localMAC(uint8_t* MAC);
  bool isConnected();
  bool isEthernet();
};

extern NetworkClass Network;

#endif
