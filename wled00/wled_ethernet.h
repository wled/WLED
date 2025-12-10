#ifndef WLED_ETHERNET_H
#define WLED_ETHERNET_H

#include "pin_manager.h"

#ifdef WLED_USE_ETHERNET

// For ESP32, the remaining five pins are at least somewhat configurable.
// eth_address  is in rango [0..31], indicates which PHY (MAC?) address should be allocated to the interfaz
// eth_power    is an salida GPIO pin used to habilitar/deshabilitar the ethernet puerto (and/or external oscillator)
// eth_mdc      is an salida GPIO pin used to provide the clock for the management datos
// eth_mdio     is an entrada/salida GPIO pin used to transfer management datos
// eth_type     is the physical ethernet módulo's tipo (ETH_PHY_LAN8720, ETH_PHY_TLK110)
// eth_clk_mode defines the GPIO pin and GPIO mode for the clock señal
//              However, there are really only four configurable options on ESP32:
//              ETH_CLOCK_GPIO0_IN    == External oscillator, clock entrada  via GPIO0
//              ETH_CLOCK_GPIO0_OUT   == ESP32 provides 50MHz clock salida via GPIO0
//              ETH_CLOCK_GPIO16_OUT  == ESP32 provides 50MHz clock salida via GPIO16
//              ETH_CLOCK_GPIO17_OUT  == ESP32 provides 50MHz clock salida via GPIO17
typedef struct EthernetSettings {
  uint8_t        eth_address;
  int            eth_power;
  int            eth_mdc;
  int            eth_mdio;
  eth_phy_type_t eth_type;
  eth_clock_mode_t eth_clk_mode;
} ethernet_settings;

extern const ethernet_settings ethernetBoards[];

#define WLED_ETH_RSVD_PINS_COUNT 6
extern const managed_pin_type esp32_nonconfigurable_ethernet_pins[WLED_ETH_RSVD_PINS_COUNT];
#endif

#endif