#include "wled.h"
#include "fcn_declare.h"
#include "wled_ethernet.h"


#if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_ETHERNET)
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
    0,                    // eth_address,
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
  }
};

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
    DEBUG_PRINTLN(F("initC: ETH.begin() failed"));
    // de-allocate the allocated pins
    for (managed_pin_type mpt : pinsToAllocate) {
      PinManager::deallocatePin(mpt.pin, PinOwner::Ethernet);
    }
    return false;
  }

  successfullyConfiguredEthernet = true;
  DEBUG_PRINTLN(F("initC: *** Ethernet successfully configured! ***"));
  return true;
}
#endif


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
    DEBUG_PRINTF_P(PSTR("WiFi: Defaulf SSID (%s) used.\n"), multiWiFi[0].clientSSID);
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
  #define ARDUINO_EVENT_ETH_DISCONNECTED        SYSTEM_EVENT_ETH_DISCONNECTED
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
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
      // also triggered when connected to selected SSID
      DEBUG_PRINTLN(F("WiFi-E: SSID scan completed."));
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
      if (!apActive) {
        WiFi.disconnect(true); // disable WiFi entirely
      }
      if (multiWiFi[0].staticIP != (uint32_t)0x00000000 && multiWiFi[0].staticGW != (uint32_t)0x00000000) {
        ETH.config(multiWiFi[0].staticIP, multiWiFi[0].staticGW, multiWiFi[0].staticSN, dnsAddress);
      } else {
        ETH.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
      }
      // convert the "serverDescription" into a valid DNS hostname (alphanumeric)
      char hostname[64];
      prepareHostname(hostname);
      ETH.setHostname(hostname);
      showWelcomePage = false;
      break;
      }
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      DEBUG_PRINTLN(F("ETH-E: Disconnected"));
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

