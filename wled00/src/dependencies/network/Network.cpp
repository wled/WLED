#include "Network.h"
#ifdef ARDUINO_ARCH_ESP32
#include "esp_netif.h"
#endif

// Forward declaration of the external WiFi manager flag (set by e.g. MatterUsermod)
extern bool externalWiFiManager;

IPAddress WLEDNetworkClass::localIP()
{
  IPAddress localIP;
#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
  localIP = ETH.localIP();
  if (localIP[0] != 0) {
    return localIP;
  }
#endif
#ifdef ARDUINO_ARCH_ESP32
  if (externalWiFiManager) {
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta) {
      esp_netif_ip_info_t ip_info;
      if (esp_netif_get_ip_info(sta, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        return IPAddress(ip_info.ip.addr);
      }
    }
    return INADDR_NONE;
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
#ifdef ARDUINO_ARCH_ESP32
  if (externalWiFiManager) {
    // Arduino's WiFi class may not track the connection when an external stack
    // (e.g. Matter) manages the netif.  Query esp_netif directly instead.
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta) {
      esp_netif_ip_info_t ip_info;
      if (esp_netif_get_ip_info(sta, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
        return true;
      }
    }
    return false;
  }
#endif
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