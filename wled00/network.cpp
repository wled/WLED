#include "wled.h"
#include "fcn_declare.h"
#include "wled_ethernet.h"


#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
#include "lwip/netif.h"           // AI: required for netif_set_default() and netif_list
#include "lwip/tcpip.h"           // AI: required for LOCK_TCPIP_CORE/UNLOCK_TCPIP_CORE
#include "esp_netif.h"            // AI: required for esp_netif_next() and esp_netif_get_desc()
#include "esp_netif_net_stack.h"  // AI: required for esp_netif_get_netif_impl()
// The following six pins are neither configurable nor
// can they be re-assigned through IOMUX / GPIO matrix.
// See https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-ethernet-kit-v1.1.html#ip101gri-phy-interface
const managed_pin_type esp32_nonconfigurable_ethernet_pins[WLED_ETH_RSVD_PINS_COUNT] = {
    { 21, true  }, // RMII EMAC TX EN  == When high, clocks the data on TXD0 and TXD1 to transmitter
    { 19, true  }, // RMII EMAC TXD0   == First bit of transmitted data
    { 22, true  }, // RMII EMAC TXD1   == Second bit of transmitted data
    { 25, false }, // RMII EMAC RXD0   == First bit of received data
    { 26, false }, // RMII EMAC RXD1   == Second bit of received data
    { 27, true  }, // RMII EMAC CRS_DV == Carrier Sense and RX Data Valid
};

const ethernet_settings ethernetBoards[] = {
  // None
  {
  },

  // WT32-EHT01
  // Please note, from my testing only these pins work for LED outputs:
  //   IO2, IO4, IO12, IO14, IO15
  // These pins do not appear to work from my testing:
  //   IO35, IO36, IO39
  {
    1,                    // eth_address,
    16,                   // eth_power,
    23,                   // eth_mdc,
    18,                   // eth_mdio,
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO0_IN    // eth_clk_mode
  },

  // ESP32-POE
  {
     0,                   // eth_address,
    12,                   // eth_power,
    23,                   // eth_mdc,
    18,                   // eth_mdio,
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO17_OUT  // eth_clk_mode
  },

   // WESP32
  {
    0,			              // eth_address,
    -1,			              // eth_power,
    16,			              // eth_mdc,
    17,			              // eth_mdio,
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO0_IN	  // eth_clk_mode
  },

  // QuinLed-ESP32-Ethernet
  {
    0,			              // eth_address,
    5,			              // eth_power,
    23,			              // eth_mdc,
    18,			              // eth_mdio,
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO17_OUT	// eth_clk_mode
  },

  // TwilightLord-ESP32 Ethernet Shield
  {
    0,			              // eth_address,
    5,			              // eth_power,
    23,			              // eth_mdc,
    18,			              // eth_mdio,
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO17_OUT	// eth_clk_mode
  },

  // ESP3DEUXQuattro
  {
    1,                    // eth_address,
    -1,                   // eth_power,
    23,                   // eth_mdc,
    18,                   // eth_mdio,
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO17_OUT  // eth_clk_mode
  },

  // ESP32-ETHERNET-KIT-VE
  {
    1,                    // eth_address,
    5,                    // eth_power,
    23,                   // eth_mdc,
    18,                   // eth_mdio,
    ETH_PHY_IP101,        // eth_type,
    ETH_CLOCK_GPIO0_IN    // eth_clk_mode
  },

  // QuinLed-Dig-Octa Brainboard-32-8L and LilyGO-T-ETH-POE
  {
    0,			              // eth_address,
    -1,			              // eth_power,
    23,			              // eth_mdc,
    18,			              // eth_mdio,
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO17_OUT	// eth_clk_mode
  },

  // ABC! WLED Controller V43 + Ethernet Shield & compatible
  {
    1,                    // eth_address, 
    5,                    // eth_power, 
    23,                   // eth_mdc, 
    33,                   // eth_mdio, 
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO17_OUT	// eth_clk_mode
  },

  // Serg74-ESP32 Ethernet Shield
  {
    1,                    // eth_address,
    5,                    // eth_power,
    23,                   // eth_mdc,
    18,                   // eth_mdio,
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO17_OUT  // eth_clk_mode
  },

  // ESP32-POE-WROVER
  {
    0,                    // eth_address,
    12,                   // eth_power,
    23,                   // eth_mdc,
    18,                   // eth_mdio,
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO0_OUT   // eth_clk_mode
  },
  
  // LILYGO T-POE Pro
  // https://github.com/Xinyuan-LilyGO/LilyGO-T-ETH-Series/blob/master/schematic/T-POE-PRO.pdf
  {
    0,			              // eth_address,
    5,			              // eth_power,
    23,			              // eth_mdc,
    18,			              // eth_mdio,
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO0_OUT	// eth_clk_mode
  },

 // Gledopto Series With Ethernet
 {
    1,                    // eth_address, 
    5,                    // eth_power, 
    23,                   // eth_mdc, 
    33,                   // eth_mdio, 
    ETH_PHY_LAN8720,      // eth_type,
    ETH_CLOCK_GPIO0_IN	 // eth_clk_mode
  },
 
 // WLED_ETH_QUINLED_V4 (14) - QuinLED Dig-Uno/Quad v4
  {
    0,                   // eth_address
    -1,                  // eth_power
    7,                   // eth_mdc
    8,                   // eth_mdio
    ETH_PHY_LAN8720,     // eth_type
    ETH_CLOCK_GPIO0_IN   // eth_clk_mode
  },
 
 // WLED_ETH_QUINLED_OCTA_V4 (15) - QuinLED Dig-Octa 32-8L v4
  {
    0,                   // eth_address
    -1,                  // eth_power
    23,                  // eth_mdc
    18,                  // eth_mdio
    ETH_PHY_LAN8720,     // eth_type
    ETH_CLOCK_GPIO0_IN   // eth_clk_mode
  },
};

// sanity checks for ethernet config table and WLED_ETH_DEFAULT
static_assert((sizeof(ethernetBoards)/sizeof(ethernetBoards[0])) == WLED_NUM_ETH_TYPES, "WLED_NUM_ETH_TYPES does not match size of ethernetBoards[] table.");
#ifdef WLED_ETH_DEFAULT
  static_assert(((WLED_ETH_DEFAULT) >= WLED_ETH_NONE) && ((WLED_ETH_DEFAULT) < WLED_NUM_ETH_TYPES), "WLED_ETH_DEFAULT is out of range.");
#endif

bool initEthernet()
{
  static bool successfullyConfiguredEthernet = false;

  if (successfullyConfiguredEthernet) {
    // DEBUG_PRINTLN(F("initE: ETH already successfully configured, ignoring"));
    return false;
  }
  if (ethernetType == WLED_ETH_NONE) {
    return false;
  }
  if (ethernetType >= WLED_NUM_ETH_TYPES) {
    DEBUG_PRINTF_P(PSTR("initE: Ignoring attempt for invalid ethernetType (%d)\n"), ethernetType);
    return false;
  }

  DEBUG_PRINTF_P(PSTR("initE: Attempting ETH config: %d\n"), ethernetType);

  // Ethernet initialization should only succeed once -- else reboot required
  ethernet_settings es = ethernetBoards[ethernetType];
  managed_pin_type pinsToAllocate[10] = {
    // first six pins are non-configurable
    esp32_nonconfigurable_ethernet_pins[0],
    esp32_nonconfigurable_ethernet_pins[1],
    esp32_nonconfigurable_ethernet_pins[2],
    esp32_nonconfigurable_ethernet_pins[3],
    esp32_nonconfigurable_ethernet_pins[4],
    esp32_nonconfigurable_ethernet_pins[5],
    { (int8_t)es.eth_mdc,   true },  // [6] = MDC  is output and mandatory
    { (int8_t)es.eth_mdio,  true },  // [7] = MDIO is bidirectional and mandatory
    { (int8_t)es.eth_power, true },  // [8] = optional pin, not all boards use
    { ((int8_t)0xFE),       false }, // [9] = replaced with eth_clk_mode, mandatory
  };
  // update the clock pin....
  if (es.eth_clk_mode == ETH_CLOCK_GPIO0_IN) {
    pinsToAllocate[9].pin = 0;
    pinsToAllocate[9].isOutput = false;
  } else if (es.eth_clk_mode == ETH_CLOCK_GPIO0_OUT) {
    pinsToAllocate[9].pin = 0;
    pinsToAllocate[9].isOutput = true;
  } else if (es.eth_clk_mode == ETH_CLOCK_GPIO16_OUT) {
    pinsToAllocate[9].pin = 16;
    pinsToAllocate[9].isOutput = true;
  } else if (es.eth_clk_mode == ETH_CLOCK_GPIO17_OUT) {
    pinsToAllocate[9].pin = 17;
    pinsToAllocate[9].isOutput = true;
  } else {
    DEBUG_PRINTF_P(PSTR("initE: Failing due to invalid eth_clk_mode (%d)\n"), es.eth_clk_mode);
    return false;
  }

  if (!PinManager::allocateMultiplePins(pinsToAllocate, 10, PinOwner::Ethernet)) {
    DEBUG_PRINTLN(F("initE: Failed to allocate ethernet pins"));
    return false;
  }

  /*
  For LAN8720 the most correct way is to perform clean reset each time before init
  applying LOW to power or nRST pin for at least 100 us (please refer to datasheet, page 59)
  ESP_IDF > V4 implements it (150 us, lan87xx_reset_hw(esp_eth_phy_t *phy) function in 
  /components/esp_eth/src/esp_eth_phy_lan87xx.c, line 280)
  but ESP_IDF < V4 does not. Lets do it:
  [not always needed, might be relevant in some EMI situations at startup and for hot resets]
  */
  #if ESP_IDF_VERSION_MAJOR==3
  if(es.eth_power>0 && es.eth_type==ETH_PHY_LAN8720) {
    pinMode(es.eth_power, OUTPUT);
    digitalWrite(es.eth_power, 0);
    delayMicroseconds(150);
    digitalWrite(es.eth_power, 1);
    delayMicroseconds(10);
  }
  #endif

  if (!ETH.begin(
                (uint8_t) es.eth_address,
                (int)     es.eth_power,
                (int)     es.eth_mdc,
                (int)     es.eth_mdio,
                (eth_phy_type_t)   es.eth_type,
                (eth_clock_mode_t) es.eth_clk_mode
                )) {
    DEBUG_PRINTLN(F("initE: ETH.begin() failed"));
    // de-allocate the allocated pins
    for (managed_pin_type mpt : pinsToAllocate) {
      PinManager::deallocatePin(mpt.pin, PinOwner::Ethernet);
    }
    return false;
  }

  // https://github.com/wled/WLED/issues/5247
  // AI: apply ethernet static IP configuration using the new dedicated
  // ethernet IP variables (ethStaticIP, ethStaticGW, ethStaticSN) rather than
  // sharing the first WiFi network's static IP config as was previously done.
  // ethStaticIP of 0.0.0.0 means use DHCP for ethernet.
  // Gateway of 0.0.0.0 is valid — means no default route via ethernet,
  // lwIP will only install a subnet route for the ethernet interface.
  if ((uint32_t)ethStaticIP != 0x00000000) {
    // AI: always pass the configured gateway to ETH.config().
    // Default route selection between interfaces is handled by netif_set_default()
    // in setPrimaryNetworkInterface(). Gateway of 0.0.0.0 is explicitly supported
    // for users who want ethernet as a stub interface with no onward routing.
    ETH.config(ethStaticIP, ethStaticGW, ethStaticSN, dnsAddress);
    DEBUG_PRINTF_P(PSTR("initE: Static IP configured. IP=%d.%d.%d.%d GW=%d.%d.%d.%d PNI=%s\n"),
      ethStaticIP[0], ethStaticIP[1], ethStaticIP[2], ethStaticIP[3],
      ethStaticGW[0], ethStaticGW[1], ethStaticGW[2], ethStaticGW[3],
      ethPrimaryInterface ? "ETH" : "WiFi");
  } else {
    // AI: no static IP configured, use DHCP for ethernet
    ETH.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    DEBUG_PRINTLN(F("initE: DHCP configured for ethernet"));
  }

  successfullyConfiguredEthernet = true;
  DEBUG_PRINTLN(F("initE: *** Ethernet successfully configured! ***"));
  return true;
}

// AI: below section was generated by an AI
// AI: setPrimaryNetworkInterface() sets the lwIP default network interface
// based on the user's ethPrimaryInterface selection.
// Uses esp-netif API (esp_netif_next / esp_netif_get_desc) to identify
// interfaces by their official description strings ("sta" for WiFi STA,
// "eth" for Ethernet) rather than fragile lwIP netif name prefixes.
// Description strings are official esp-netif defaults defined in
// esp_netif_defaults.h — stable across IDF versions.
// Falls back to any available interface if the preferred one is not ready.
void setPrimaryNetworkInterface() {
  const char *targetDesc = ethPrimaryInterface ? "eth" : "sta";

  struct netif *target = nullptr;
  struct netif *fallback = nullptr;
  const char *selectedDesc = nullptr;
  const char *fallbackDesc = nullptr;

  // AI: acquire lwIP TCP/IP core lock before calling netif_set_default()
  LOCK_TCPIP_CORE();

  // AI: iterate esp_netif handles using official API, identify interface
  // type via description string, get lwIP netif pointer via impl handle
  esp_netif_t *esp_netif = esp_netif_next(NULL);
  while (esp_netif != NULL) {
    const char *desc = esp_netif_get_desc(esp_netif);
    struct netif *netif_impl = (struct netif *)esp_netif_get_netif_impl(esp_netif);
    if (netif_impl && netif_is_up(netif_impl) &&
        netif_impl->ip_addr.u_addr.ip4.addr != 0) {
      if (desc && strcmp(desc, targetDesc) == 0) {
        target = netif_impl;
        selectedDesc = desc;
        break;
      }
      if (!fallback) { fallback = netif_impl; fallbackDesc = desc; }
    }
    esp_netif = esp_netif_next(esp_netif);
  }

  // AI: fall back to any ready interface if preferred is unavailable
  // prevents outbound traffic being pinned to a dead default netif
  if (!target && fallback) {
    target = fallback;
    selectedDesc = fallbackDesc;
    DEBUG_PRINTLN(F("setPNI: Preferred interface unavailable, using fallback"));
  }

  if (target != nullptr) {
    netif_set_default(target);
    DEBUG_PRINTF_P(PSTR("setPNI: Primary netif set via desc='%s' (%d.%d.%d.%d)\n"),
      selectedDesc ? selectedDesc : "unknown",
      ip4_addr1(&target->ip_addr.u_addr.ip4),
      ip4_addr2(&target->ip_addr.u_addr.ip4),
      ip4_addr3(&target->ip_addr.u_addr.ip4),
      ip4_addr4(&target->ip_addr.u_addr.ip4));
  } else {
    DEBUG_PRINTLN(F("setPNI: No ready interface found, will retry on next IP event"));
  }

  UNLOCK_TCPIP_CORE();
}
#endif
// AI: end

//by https://github.com/tzapu/WiFiManager/blob/master/WiFiManager.cpp
int getSignalQuality(int rssi)
{
    int quality = 0;

    if (rssi <= -100)
    {
        quality = 0;
    }
    else if (rssi >= -50)
    {
        quality = 100;
    }
    else
    {
        quality = 2 * (rssi + 100);
    }
    return quality;
}


void fillMAC2Str(char *str, const uint8_t *mac) {
  sprintf_P(str, PSTR("%02x%02x%02x%02x%02x%02x"), MAC2STR(mac));
  byte nul = 0;
  for (int i = 0; i < 6; i++) nul |= *mac++;  // do we have 0
  if (!nul) str[0] = '\0';                    // empty string
}

void fillStr2MAC(uint8_t *mac, const char *str) {
  for (int i = 0; i < 6; i++) *mac++ = 0;     // clear
  if (!str) return;                           // null string
  uint64_t MAC = strtoull(str, nullptr, 16);
  for (int i = 0; i < 6; i++) { *--mac = MAC & 0xFF; MAC >>= 8; }
}


// performs asynchronous scan for available networks (which may take couple of seconds to finish)
// returns configured WiFi ID with the strongest signal (or default if no configured networks available)
int findWiFi(bool doScan) {
  if (multiWiFi.size() <= 1) {
    DEBUG_PRINTF_P(PSTR("WiFi: Default SSID (%s) used.\n"), multiWiFi[0].clientSSID);
    return 0;
  }

  int status = WiFi.scanComplete(); // complete scan may take as much as several seconds (usually <6s with not very crowded air)

  if (doScan || status == WIFI_SCAN_FAILED) {
    DEBUG_PRINTF_P(PSTR("WiFi: Scan started. @ %lus\n"), millis()/1000);
    WiFi.scanNetworks(true);  // start scanning in asynchronous mode (will delete old scan)
  } else if (status >= 0) {   // status contains number of found networks (including duplicate SSIDs with different BSSID)
    DEBUG_PRINTF_P(PSTR("WiFi: Found %d SSIDs. @ %lus\n"), status, millis()/1000);
    int rssi = -9999;
    int selected = selectedWiFi;
    for (int o = 0; o < status; o++) {
      DEBUG_PRINTF_P(PSTR(" SSID: %s (BSSID: %s) RSSI: %ddB\n"), WiFi.SSID(o).c_str(), WiFi.BSSIDstr(o).c_str(), WiFi.RSSI(o));
      for (unsigned n = 0; n < multiWiFi.size(); n++)
        if (!strcmp(WiFi.SSID(o).c_str(), multiWiFi[n].clientSSID)) {
          bool foundBSSID = memcmp(multiWiFi[n].bssid, WiFi.BSSID(o), 6) == 0;
          // find the WiFi with the strongest signal (but keep priority of entry if signal difference is not big)
          if (foundBSSID || (n < selected && WiFi.RSSI(o) > rssi-10) || WiFi.RSSI(o) > rssi) {
            rssi = foundBSSID ? 0 : WiFi.RSSI(o); // RSSI is only ever negative
            selected = n;
          }
          break;
        }
    }
    DEBUG_PRINTF_P(PSTR("WiFi: Selected SSID: %s RSSI: %ddB\n"), multiWiFi[selected].clientSSID, rssi);
    return selected;
  }
  //DEBUG_PRINT(F("WiFi scan running."));
  return status; // scan is still running or there was an error
}


bool isWiFiConfigured() {
  return multiWiFi.size() > 1 || (strlen(multiWiFi[0].clientSSID) >= 1 && strcmp_P(multiWiFi[0].clientSSID, PSTR(DEFAULT_CLIENT_SSID)) != 0);
}

#if defined(ESP8266)
  #define ARDUINO_EVENT_WIFI_AP_STADISCONNECTED WIFI_EVENT_SOFTAPMODE_STADISCONNECTED
  #define ARDUINO_EVENT_WIFI_AP_STACONNECTED    WIFI_EVENT_SOFTAPMODE_STACONNECTED
  #define ARDUINO_EVENT_WIFI_STA_GOT_IP         WIFI_EVENT_STAMODE_GOT_IP
  #define ARDUINO_EVENT_WIFI_STA_CONNECTED      WIFI_EVENT_STAMODE_CONNECTED
  #define ARDUINO_EVENT_WIFI_STA_DISCONNECTED   WIFI_EVENT_STAMODE_DISCONNECTED
#elif defined(ARDUINO_ARCH_ESP32) && !defined(ESP_ARDUINO_VERSION_MAJOR) //ESP_IDF_VERSION_MAJOR==3
  // not strictly IDF v3 but Arduino core related
  #define ARDUINO_EVENT_WIFI_AP_STADISCONNECTED SYSTEM_EVENT_AP_STADISCONNECTED
  #define ARDUINO_EVENT_WIFI_AP_STACONNECTED    SYSTEM_EVENT_AP_STACONNECTED
  #define ARDUINO_EVENT_WIFI_STA_GOT_IP         SYSTEM_EVENT_STA_GOT_IP
  #define ARDUINO_EVENT_WIFI_STA_CONNECTED      SYSTEM_EVENT_STA_CONNECTED
  #define ARDUINO_EVENT_WIFI_STA_DISCONNECTED   SYSTEM_EVENT_STA_DISCONNECTED
  #define ARDUINO_EVENT_WIFI_AP_START           SYSTEM_EVENT_AP_START
  #define ARDUINO_EVENT_WIFI_AP_STOP            SYSTEM_EVENT_AP_STOP
  #define ARDUINO_EVENT_WIFI_SCAN_DONE          SYSTEM_EVENT_SCAN_DONE
  #define ARDUINO_EVENT_ETH_START               SYSTEM_EVENT_ETH_START
  #define ARDUINO_EVENT_ETH_CONNECTED           SYSTEM_EVENT_ETH_CONNECTED
  #define ARDUINO_EVENT_ETH_GOT_IP              SYSTEM_EVENT_ETH_GOT_IP  // AI: added for DHCP ethernet IP assignment event
  #define ARDUINO_EVENT_ETH_DISCONNECTED        SYSTEM_EVENT_ETH_DISCONNECTED
#endif

#if defined(ARDUINO_ARCH_ESP32) && defined(LWIP_IPV6)
#include "lwip/raw.h"
#include "lwip/icmp6.h"
// This is a terrible workaround for a terrible bug: on ESP32 platforms, unsolicited IPv6 router
// advertisements will cause LwIP to overwrite the IPv4 DNS servers with the IPv6 DNS servers
// mentioned in the RA packet.  As a workaround, we just blackhole those packets using the raw
// callback, since we don't yet support IPv6.
//
// This may have been improved in IDF v5 -- Espressif has added a feature to store DNS servers
// on a per interface basis in their LwIP fork.
//
// References:
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/lwip.html (see the "Note" block under "Adapted APIs" -- though it very much undersells the problem.)
// https://github.com/espressif/arduino-esp32/discussions/9988 - links to older discussions
static u8_t blockRouterAdvertisements(void* arg, struct raw_pcb* pcb, struct pbuf* p, const ip_addr_t* addr) {
  // ICMPv6 type is the first byte of the payload, so we skip the header
  if (p->len > 0 && (pbuf_get_at(p, sizeof(struct ip6_hdr)) == ICMP6_TYPE_RA)) {
    pbuf_free(p);
    return 1; // claim the packet — lwIP will not pass it further
  }
  return 0; // not consumed, pass it on
}

void installIPv6RABlocker() {
  struct raw_pcb* ra_blocker = raw_new_ip_type(IPADDR_TYPE_V6, IP6_NEXTH_ICMP6);
  raw_recv(ra_blocker, blockRouterAdvertisements, NULL);
}
#endif

//handle Ethernet connection event
void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      // AP client disconnected
      if (--apClients == 0 && isWiFiConfigured()) forceReconnect = true; // no clients reconnect WiFi if awailable
      DEBUG_PRINTF_P(PSTR("WiFi-E: AP Client Disconnected (%d) @ %lus.\n"), (int)apClients, millis()/1000);
      break;
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      // AP client connected
      apClients++;
      DEBUG_PRINTF_P(PSTR("WiFi-E: AP Client Connected (%d) @ %lus.\n"), (int)apClients, millis()/1000);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      DEBUG_PRINT(F("WiFi-E: IP address: ")); DEBUG_PRINTLN(Network.localIP());
      // AI: re-evaluate primary network interface when WiFi gets its IP
      // handles both static IP and DHCP scenarios for WiFi interface
      #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
      setPrimaryNetworkInterface();
      #endif
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      // followed by IDLE and SCAN_DONE
      DEBUG_PRINTF_P(PSTR("WiFi-E: Connected! @ %lus\n"), millis()/1000);
      wasConnected = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (wasConnected && interfacesInited) {
        DEBUG_PRINTF_P(PSTR("WiFi-E: Disconnected! @ %lus\n"), millis()/1000);
        if (interfacesInited && multiWiFi.size() > 1 && WiFi.scanComplete() >= 0) {
          findWiFi(true); // reinit WiFi scan
          forceReconnect = true;
        }
        interfacesInited = false;
      }
      break;
  #ifdef ARDUINO_ARCH_ESP32
    case ARDUINO_EVENT_WIFI_READY:
      DEBUG_PRINTLN(F("WiFi-E: driver ready."));
      break;
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
      // also triggered when connected to selected SSID
      DEBUG_PRINTLN(F("WiFi-E: SSID scan completed."));
      break;
    case ARDUINO_EVENT_WIFI_STA_START:
      DEBUG_PRINTLN(F("WiFi-E: STA Started"));
      break;
    case ARDUINO_EVENT_WIFI_STA_STOP:
      DEBUG_PRINTLN(F("WiFi-E: STA Stopped"));
      break;
    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
      DEBUG_PRINTLN(F("WiFi-E: STA authentication mode change."));
      break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
      DEBUG_PRINTLN(F("WiFi-E: IP address lost."));
      break;

    case ARDUINO_EVENT_WIFI_AP_START:
      DEBUG_PRINTLN(F("WiFi-E: AP Started"));
      break;
    case ARDUINO_EVENT_WIFI_AP_STOP:
      DEBUG_PRINTLN(F("WiFi-E: AP Stopped"));
      break;
    #if defined(WLED_USE_ETHERNET)
    case ARDUINO_EVENT_ETH_START:
      DEBUG_PRINTLN(F("ETH-E: Started"));
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      {
      DEBUG_PRINTLN(F("ETH-E: Connected"));
      // AI: WiFi is intentionally kept active when ethernet connects.
      // Previously WiFi was disabled here to prevent routing conflicts, but
      // with dual-interface support, netif_set_default() handles routing
      // preference between interfaces. Disabling WiFi here would defeat the
      // purpose of the feature entirely.
      char hostname[64] = {'\0'};
      getWLEDhostname(hostname, sizeof(hostname), true);
      ETH.setHostname(hostname);
      // AI: attempt to set default gateway interface on ethernet connect
      setPrimaryNetworkInterface();
      showWelcomePage = false;
      break;
      }
    case ARDUINO_EVENT_ETH_GOT_IP:
      // AI: ethernet DHCP IP assigned — now safe to set default netif
      // this event is the reliable trigger for DHCP ethernet configuration
      DEBUG_PRINT(F("ETH-E: Got IP: ")); DEBUG_PRINTLN(ETH.localIP());
      setPrimaryNetworkInterface();
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      DEBUG_PRINTLN(F("ETH-E: Disconnected"));
      // AI: re-evaluate primary network interface on ethernet disconnect
      // ensures fallback to WiFi if ethernet was the primary interface
      setPrimaryNetworkInterface();
      // This doesn't really affect ethernet per se,
      // as it's only configured once.  Rather, it
      // may be necessary to reconnect the WiFi when
      // ethernet disconnects, as a way to provide
      // alternative access to the device.
      if (interfacesInited && WiFi.scanComplete() >= 0) findWiFi(true); // reinit WiFi scan
      forceReconnect = true;
      break;
    #endif
  #endif
    default:
      DEBUG_PRINTF_P(PSTR("WiFi-E: Event %d\n"), (int)event);
      break;
  }
}

