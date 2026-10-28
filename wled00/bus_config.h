#pragma once
#ifndef BusConfig_h
#define BusConfig_h

#include "const.h"
#include "src/WLEDpixelBus/WLEDpixelBus.h" // MAX_CUSTOM_CHANNELS, CH_*, LedTiming

// Bus configuration structures and helpers (formerly part of bus_manager)
//==========================================================================

constexpr bool busTypeHasRGB(uint8_t type) {
  return !((type >= TYPE_WS2812_1CH_X3 && type <= TYPE_WS2812_WWA) || type == TYPE_ANALOG_1CH || type == TYPE_ANALOG_2CH || type == TYPE_ONOFF);
}
constexpr bool busTypeHasWhite(uint8_t type) {
  return  (type >= TYPE_WS2812_1CH_X3 && type <= TYPE_WS2812_WWA) ||
          type == TYPE_SK6812_RGBW || type == TYPE_TM1814 || type == TYPE_TM1815 || type == TYPE_UCS8904 ||
          type == TYPE_FW1906 || type == TYPE_WS2805 || type == TYPE_SM16825 ||        // digital types with white channel
          (type > TYPE_ONOFF && type <= TYPE_ANALOG_5CH && type != TYPE_ANALOG_3CH) || // analog types with white channel
          type == TYPE_NET_DDP_RGBW || type == TYPE_NET_ARTNET_RGBW;                   // network types with white channel
}
constexpr bool busTypeHasCCT(uint8_t type) {
  return  type == TYPE_WS2812_WWA    || type == TYPE_SM16825 ||
          type == TYPE_ANALOG_2CH    || type == TYPE_ANALOG_5CH ||
          type == TYPE_FW1906        || type == TYPE_WS2805;
}
constexpr bool busTypeIsValid(uint8_t type)     { return (type >= TYPE_DIGITAL_MIN && type < 128); }
constexpr bool busTypeIs2Pin(uint8_t type)      { return (type >= TYPE_2PIN_MIN && type <= TYPE_2PIN_MAX); }
constexpr bool busTypeIsDigital(uint8_t type)   { return (type >= TYPE_DIGITAL_MIN && type <= TYPE_DIGITAL_MAX) || busTypeIs2Pin(type); }
constexpr bool busTypeIsOnOff(uint8_t type)     { return (type == TYPE_ONOFF); }
constexpr bool busTypeIsPWM(uint8_t type)       { return (type >= TYPE_ANALOG_MIN && type <= TYPE_ANALOG_MAX); }
constexpr bool busTypeIsVirtual(uint8_t type)   { return (type >= TYPE_VIRTUAL_MIN && type <= TYPE_VIRTUAL_MAX); }
constexpr bool busTypeIsHub75(uint8_t type)     { return (type >= TYPE_HUB75MATRIX_MIN && type <= TYPE_HUB75MATRIX_MAX); }
constexpr bool busTypeIs16bit(uint8_t type)     { return type == TYPE_UCS8903 || type == TYPE_UCS8904 || type == TYPE_SM16825; }
constexpr bool busTypeMustRefresh(uint8_t type) { return type == TYPE_TM1814 || type == TYPE_TM1815; }
constexpr int  busTypeNumPWMPins(uint8_t type)  { return (type - 40); }
// credit @PaoloTK; for HUB75 the 5 slots store config params (panelW, panelH, chain, rows, cols), not GPIO pins
constexpr size_t busTypeGetNumberOfPins(uint8_t type) {
  return busTypeIsVirtual(type) ? 4 : busTypeIsPWM(type) ? busTypeNumPWMPins(type) : busTypeIsHub75(type) ? 5 : busTypeIs2Pin(type) + 1;
}
constexpr size_t busTypeGetNumberOfChannels(uint8_t type) {
  return (busTypeHasWhite(type) + 3*busTypeHasRGB(type) + busTypeHasCCT(type));
}

// Driver preference for digital LED buses: stored in BusConfig::driverType and BusDigital::_driverType
enum BusDriverType : uint8_t {
  BUSDRV_RMT     = 0, // RMT peripheral (default on most ESP32 variants)
  BUSDRV_PARHW   = 1, // parallel output (chip-dependent): ESP32/S2/S3 use I2S-LCD, C3 uses parallel SPI, C6/H2/C5/P4 use PARLIO
  BUSDRV_BITBANG = 2, // parallel bit-bang GPIO (ESP32 only)
};

// Channel layout & protocol timing for digital LED buses
// WW/CW are not treated differently if not both are set (see #5654 for reference).
struct BusChannelLayout {
  uint8_t  numChannels = 0;       // wire channels per pixel; 0 = not resolved yet
  uint8_t  channelMap[WLEDpixelBus::MAX_CUSTOM_CHANNELS] = {WLEDpixelBus::CH_G, WLEDpixelBus::CH_R, WLEDpixelBus::CH_B, WLEDpixelBus::CH_UNUSED, WLEDpixelBus::CH_UNUSED, WLEDpixelBus::CH_UNUSED}; // GRB default
  uint8_t  invertMask  = 0;       // bitmask: bit i = invert channel i output level
  bool     is16bit      = false;  // 2 wire bytes per channel (UCS8903/04, SM16825)
  bool     invertOutput = false;  // invert the hardware output signal polarity
  bool     custom       = false;  // user-entered override active (replaces CustomBusConfig::active())
  uint16_t t0h = 300;  // '0' bit high time, ns
  uint16_t t0l = 900;  // '0' bit low  time, ns
  uint16_t t1h = 700;  // '1' bit high time, ns
  uint16_t t1l = 500;  // '1' bit low  time, ns
  uint16_t trst = 300; // reset/latch time, µs
};

// Transient bus configuration, filled by JSON/HTTP parsing and consumed by the
// Bus constructors (busConfigs vector is cleared after finalizeInit).
struct BusConfig {
  // identity & geometry
  uint8_t  type      = TYPE_NONE; // 7-bit bus type; the off-refresh flag lives in refreshReq (bit-7 hack ends at the JSON/form boundary)
  uint8_t  pins[OUTPUT_MAX_PINS] = {255, 255, 255, 255, 255};
  uint16_t start     = 0;
  uint16_t count     = 1;
  // color
  uint8_t  colorOrder = COL_ORDER_GRB; // lower nibble: RGB permutation, upper nibble: W/WWCW swap
  uint8_t  skipAmount = 0;
  BusChannelLayout channels; // wire layout: resolved from type+colorOrder, or user override when channels.custom
  uint8_t  autoWhite  = RGBW_MODE_MANUAL_ONLY;
  // driver & electrical
  uint8_t  driverType      = BUSDRV_RMT;
  uint8_t  busSpeedFactor  = 100;  // percent (100 = default timings)
  uint8_t  milliAmpsPerLed = LED_MILLIAMPS_DEFAULT;
  uint16_t milliAmpsMax    = ABL_MILLIAMPS_DEFAULT;
  uint16_t frequency       = 0;    // kHz (2-pin) / Hz (PWM)
  // flags
  bool     reversed   = false;
  bool     refreshReq = false;    // requires refresh while off
  String   text;                  // network hostname / custom text (config-time only)

  BusConfig() = default;
  // Slim ctor for programmatic construction (default buses, usermods); all other fields keep their defaults.
  BusConfig(uint8_t busType, uint8_t* ppins, uint16_t pstart, uint16_t len = 1)
  : type(busType)
  , start(pstart)
  , count(std::max(len, (uint16_t)1))
  {
    const size_t nPins = busTypeGetNumberOfPins(type);
    for (size_t i = 0; i < nPins; i++) pins[i] = ppins[i];
  }

  // validates start and length and extends total if needed
  bool adjustBounds(uint16_t& total) {
    if (!count) count = 1;
    if (!busTypeIsVirtual(type) && count > MAX_LEDS_PER_BUS) count = MAX_LEDS_PER_BUS;
    if (start >= MAX_LEDS) return false;
    // limit length of strip if it would exceed total permissible LEDs
    if (start + count > MAX_LEDS) count = MAX_LEDS - start;
    // extend total count accordingly
    if (start + count > total) total = start + count;
    return true;
  }
};

#endif
