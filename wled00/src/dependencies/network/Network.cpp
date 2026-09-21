#include "Network.h"

IPAddress WLEDNetworkClass::localIP()
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

IPAddress WLEDNetworkClass::subnetMask()
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

IPAddress WLEDNetworkClass::gatewayIP()
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

// directed broadcast address of the active interface (host bits set to one).
// Kept here so that all callers share one implementation - including whatever
// this needs to become once IPv6 is supported.
IPAddress WLEDNetworkClass::broadcastIP()
{
  IPAddress ip = localIP();
  if (ip[0] == 0) {
    return IPAddress(255, 255, 255, 255); // not connected yet: limited broadcast
  }
  return IPAddress(uint32_t(ip) | ~uint32_t(subnetMask()));
}

void WLEDNetworkClass::localMAC(uint8_t* MAC)
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

bool WLEDNetworkClass::isConnected()
{
  return (WiFi.localIP()[0] != 0 && WiFi.status() == WL_CONNECTED) || isEthernet();
}

bool WLEDNetworkClass::isEthernet()
{
#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
  return (ETH.localIP()[0] != 0) && ETH.linkUp();
#endif
  return false;
}

WLEDNetworkClass WLEDNetwork;