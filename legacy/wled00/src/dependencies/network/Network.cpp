#include "Network.h"

IPAddress NetworkClass::localIP()
{
  IPAddress localIP;
#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
  localIP = ETH.localIP();
  if (localIP[0] != 0) {
    return localIP;
  }
#endif
  localIP = WiFi.localIP();
  if (localIP[0] != 0) {
    return localIP;
  }

  return INADDR_NONE;
}

IPAddress NetworkClass::subnetMask()
{
#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
  if (ETH.localIP()[0] != 0) {
    return ETH.subnetMask();
  }
#endif
  if (WiFi.localIP()[0] != 0) {
    return WiFi.subnetMask();
  }
  return IPAddress(255, 255, 255, 0);
}

IPAddress NetworkClass::gatewayIP()
{
#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
  if (ETH.localIP()[0] != 0) {
      return ETH.gatewayIP();
  }
#endif
  if (WiFi.localIP()[0] != 0) {
      return WiFi.gatewayIP();
  }
  return INADDR_NONE;
}

void NetworkClass::localMAC(uint8_t* MAC)
{
#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
  // ETH.macAddress(MAC); // Does not work because of missing ETHClass:: in ETH.ccp

  // Start work around
  String macString = ETH.macAddress();
  char macChar[18];
  char * octetEnd = macChar;

  strlcpy(macChar, macString.c_str(), 18);

  for (uint8_t i = 0; i < 6; i++) {
    MAC[i] = (uint8_t)strtol(octetEnd, &octetEnd, 16);
    octetEnd++;
  }
  // End work around

  for (uint8_t i = 0; i < 6; i++) {
    if (MAC[i] != 0x00) {
      return;
    }
  }
#endif
  WiFi.macAddress(MAC);
  return;
}

bool NetworkClass::isConnected()
{
#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
  return (WiFi.localIP()[0] != 0 && WiFi.status() == WL_CONNECTED) || ETH.localIP()[0] != 0;
#else
  return (WiFi.localIP()[0] != 0 && WiFi.status() == WL_CONNECTED);
#endif
}

bool NetworkClass::isEthernet()
{
#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
  return (ETH.localIP()[0] != 0);
#endif
  return false;
}

NetworkClass Network;