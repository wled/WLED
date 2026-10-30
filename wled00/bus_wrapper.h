#pragma once
#ifndef BusWrapper_h
#define BusWrapper_h

#include "src/WLEDpixelBus/WLEDpixelBus.h"
#include "src/WLEDpixelBus/WLEDpixelBus_SPI.h"
#include "bus_config.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "src/WLEDpixelBus/WLEDpixelBus_RMT.h"
#include "src/WLEDpixelBus/WLEDpixelBus_I2S.h"
#include "src/WLEDpixelBus/WLEDpixelBus_ParallelSpi.h"
#include "src/WLEDpixelBus/WLEDpixelBus_PARLIO.h"
#include "src/WLEDpixelBus/WLEDpixelBus_BitBang.h"
#elif defined(ARDUINO_ARCH_ESP8266)
#include "src/WLEDpixelBus/WLEDpixelBus_ESP8266.h"
#include "src/WLEDpixelBus/WLEDpixelBus_BitBang.h"
#endif

//Hardware SPI Pins (ESP8266 only; ESP32 uses bus allocation order to detect HSPI)
#define P_8266_HS_MOSI 13
#define P_8266_HS_CLK  14

// Use single RMT memory block per channel — allows RMT RX channels alongside TX (see WLEDpixelBus_RMT)
//#define RMT_USE_SINGLE_MEM_BLOCK


// Legacy color-order compatibility shim
//
// Backwards compatibility with the NeoPixelBus-based driver: each bus type used a fixed NPB "feature"
// that defined the physical wire color-order on top of which the color order was applied (permutations)
// The features were not used consistently but "by strip defaults". Below table was extracted from legacy bus_wrapper.h using AI.

enum LegacyField : uint8_t { LF_R, LF_G, LF_B, LF_W, LF_CW, LF_PAD };

struct LegacyLayout {
  uint8_t n;            // wire bytes per pixel
  LegacyField f[6];     // color order (WLEDpixelBus has up to 6 colors, TYPE_FW1906 uses all 6 in hardware)
};

// LED Type wire layout as instantiated by the legacy NPB wrapper (except diverging ESP8266 corner casses that were probably bugs).
inline const LegacyLayout& getLegacyChannelLayout(uint8_t busType) {
  static const LegacyLayout grb     = {3, {LF_G, LF_R, LF_B,  LF_PAD, LF_PAD, LF_PAD}}; // WS2812/13, WS2811_400KHZ, LPD8806, LPD6803, TM1914, APA106
  static const LegacyLayout grbw    = {4, {LF_G, LF_R, LF_B,  LF_W,   LF_PAD, LF_PAD}}; // SK6812_RGBW
  static const LegacyLayout grbwwcw = {5, {LF_G, LF_R, LF_B,  LF_W,   LF_CW,  LF_PAD}}; // WS2805 (NeoGrbwwFeature)
  static const LegacyLayout brg     = {3, {LF_B, LF_R, LF_G,  LF_PAD, LF_PAD, LF_PAD}}; // TM1829 (NeoBrgFeature)
  static const LegacyLayout rgb     = {3, {LF_R, LF_G, LF_B,  LF_PAD, LF_PAD, LF_PAD}}; // UCS8903 (16-bit)
  static const LegacyLayout rgbw    = {4, {LF_R, LF_G, LF_B,  LF_W,   LF_PAD, LF_PAD}}; // UCS8904 (16-bit)
  static const LegacyLayout wrgb    = {4, {LF_W, LF_R, LF_G,  LF_B,   LF_PAD, LF_PAD}}; // TM1814/TM1815 (NeoWrgbTm1814Feature)
  static const LegacyLayout grbcwx  = {6, {LF_G, LF_R, LF_B,  LF_CW,  LF_W,   LF_PAD}}; // FW1906 (NeoGrbcwxFeature, 6th byte unused)
  static const LegacyLayout rgbcww  = {5, {LF_R, LF_G, LF_B,  LF_CW,  LF_W,   LF_PAD}}; // SM16825 (NeoRgbcwSm16825eFeature, 16-bit)
  static const LegacyLayout rbg     = {3, {LF_R, LF_B, LF_G,  LF_PAD, LF_PAD, LF_PAD}}; // WS2801 (NeoRbgFeature)
  static const LegacyLayout bgr     = {3, {LF_B, LF_G, LF_R,  LF_PAD, LF_PAD, LF_PAD}}; // APA102 (DotStarBgrFeature), P9813 (P9813BgrFeature)
  switch (busType) {
    case TYPE_TM1829:                 return brg;
    case TYPE_UCS8903:                return rgb;
    case TYPE_UCS8904:                return rgbw;
    case TYPE_TM1814: // fallthrough
    case TYPE_TM1815:                 return wrgb;
    case TYPE_FW1906:                 return grbcwx;
    case TYPE_WS2805:                 return grbwwcw;
    case TYPE_SM16825:                return rgbcww;
    case TYPE_WS2801:                 return rbg;
    case TYPE_APA102: // fallthrough
    case TYPE_P9813:                  return bgr;
    default: // all other one-wire chips used GRB-based NPB features
      if (busTypeHasCCT(busType))   return grbwwcw;
      if (busTypeHasWhite(busType)) return grbw;
      return grb;
  }
}

// Compose a legacy wire layout with color order into a WLEDpixelBus channel map (which puts colors on the wire in the order of channelMap[], see color encoders)
inline uint8_t legacyColorOrderToChannelMap(uint8_t busType, uint8_t colorOrder, uint8_t* channelMap) {
  // permutations of how "colorOrder" was applied, 0 is "GRB", i.e. no permuation but the underlying bus layout was GRB for common types (but not all see getLegacyChannelLayout())
  static constexpr uint8_t kLegacyPerm[6][3] = {
    {LF_R, LF_G, LF_B}, // 0 = "GRB" (default)
    {LF_G, LF_R, LF_B}, // 1 = "RGB"
    {LF_R, LF_B, LF_G}, // 2 = "BRG"
    {LF_B, LF_R, LF_G}, // 3 = "RBG"
    {LF_G, LF_B, LF_R}, // 4 = "BGR"
    {LF_B, LF_G, LF_R}, // 5 = "GBR"
  };
  const LegacyLayout& layout = getLegacyChannelLayout(busType);

  // Logical content carried by each field after the permutation + white swap.
  uint8_t content[LF_PAD + 1];
  const uint8_t perm = colorOrder & 0x0F;
  const uint8_t* p = kLegacyPerm[perm <= COL_ORDER_MAX ? perm : 0];
  content[LF_R] = p[0]; content[LF_G] = p[1]; content[LF_B] = p[2];
  content[LF_W] = LF_W; content[LF_CW] = LF_CW; content[LF_PAD] = LF_PAD;

  const bool isCct = layout.n >= 5; // 5+ channel layouts carry WW/CW
  const uint8_t wSwap = colorOrder >> 4;
  if (!isCct) {
    // RGBW: swap contents of W with B/G/R (upper nibble 1/2/3)
    if (wSwap >= WLEDpixelBus::WSWAP_B && wSwap <= WLEDpixelBus::WSWAP_R) {
      const uint8_t other = (wSwap == WLEDpixelBus::WSWAP_B) ? LF_B : (wSwap == WLEDpixelBus::WSWAP_G) ? LF_G : LF_R;
      std::swap(content[LF_W], content[other]);
    }
  } else if (wSwap == WLEDpixelBus::WSWAP_WWCW) {
    std::swap(content[LF_W], content[LF_CW]); // RGBCCT: swap WW & CW contents
  }

  for (uint8_t i = 0; i < WLEDpixelBus::MAX_CUSTOM_CHANNELS; i++) {
    if (i >= layout.n) { channelMap[i] = WLEDpixelBus::CH_UNUSED; continue; }
    switch (content[layout.f[i]]) {
      case LF_R:  channelMap[i] = WLEDpixelBus::CH_R;  break;
      case LF_G:  channelMap[i] = WLEDpixelBus::CH_G;  break;
      case LF_B:  channelMap[i] = WLEDpixelBus::CH_B;  break;
      case LF_W:  channelMap[i] = isCct ? WLEDpixelBus::CH_WW : WLEDpixelBus::CH_W; break;
      case LF_CW: channelMap[i] = WLEDpixelBus::CH_CW; break;
      default:    channelMap[i] = WLEDpixelBus::CH_UNUSED; break; // LF_PAD
    }
  }
  return layout.n;
}

// Legacy LED types that do not have a color-order permutation. Their wire map is fixed.
inline bool legacyHasFixedChannelMap(uint8_t ledType) {
  return ledType == TYPE_WS2812_1CH_X3 || ledType == TYPE_WS2812_WWA;
}

// True when a custom channel map matches the permutation supported by ColorOrderMap.
inline bool legacyChannelMapMatchesColorOrder(const WLEDpixelBus::BusProperties& properties, uint8_t colorOrder, uint8_t ledType) {
  if (legacyHasFixedChannelMap(ledType) || properties.numChannels < 3 || properties.numChannels > WLEDpixelBus::MAX_CUSTOM_CHANNELS) return false;
  uint8_t expected[WLEDpixelBus::MAX_CUSTOM_CHANNELS];
  const uint8_t n = legacyColorOrderToChannelMap(ledType, colorOrder, expected);
  if (n != properties.numChannels) return false;
  for (uint8_t i = 0; i < n; i++) {
    if (properties.channelMap[i] != expected[i]) return false;
  }
  return true;
}

// Wire position of each logical channel (R,G,B,W/WW,CW) for a color order.
inline void getLegacyColorOrderPositions(uint8_t colorOrder, uint8_t numChannels, uint8_t ledType, uint8_t positions[5]) {
  uint8_t map[WLEDpixelBus::MAX_CUSTOM_CHANNELS];
  legacyColorOrderToChannelMap(ledType, colorOrder, map);
  positions[0] = 0; positions[1] = 1; positions[2] = 2; positions[3] = 3; positions[4] = 4;
  for (uint8_t i = 0; i < numChannels; i++) {
    switch (map[i]) {
      case WLEDpixelBus::CH_R:  positions[0] = i; break;
      case WLEDpixelBus::CH_G:  positions[1] = i; break;
      case WLEDpixelBus::CH_B:  positions[2] = i; break;
      case WLEDpixelBus::CH_W:
      case WLEDpixelBus::CH_WW: positions[3] = i; break;
      case WLEDpixelBus::CH_CW: positions[4] = i; break;
      default: break;
    }
  }
}

// Translate between legacy color orders while preserving the chip's physical wire layout.
inline void translateLegacyColorOrder(uint32_t &c, CctPixel& cct, uint8_t sourceOrder, uint8_t targetOrder, uint8_t numChannels, uint8_t ledType) {
  if (sourceOrder == targetOrder) return;

  uint8_t sourcePositions[5], targetPositions[5];
  getLegacyColorOrderPositions(sourceOrder, numChannels, ledType, sourcePositions);
  getLegacyColorOrderPositions(targetOrder, numChannels, ledType, targetPositions);
  uint8_t sourceByWirePosition[5] = {};
  sourceByWirePosition[sourcePositions[0]] = R(c);
  sourceByWirePosition[sourcePositions[1]] = G(c);
  sourceByWirePosition[sourcePositions[2]] = B(c);
  if (numChannels >= 4)
    sourceByWirePosition[sourcePositions[3]] = numChannels == 5 ? cct.ww : W(c);
  if (numChannels == 5)
    sourceByWirePosition[sourcePositions[4]] = cct.cw;

  const uint8_t targetR = sourceByWirePosition[targetPositions[0]];
  const uint8_t targetG = sourceByWirePosition[targetPositions[1]];
  const uint8_t targetB = sourceByWirePosition[targetPositions[2]];
  const uint8_t targetW = numChannels >= 4 ? sourceByWirePosition[targetPositions[3]] : W(c);
  const uint8_t targetCW = numChannels == 5 ? sourceByWirePosition[targetPositions[4]] : 0;
  // for 5ch (CCT) also write WW into W(c): the encoder ignores W(c) and uses wwcw, but
  // getPixelColor() can only recover the WW channel through W(c) (CW stays lost, decode is lossy)
  c = RGBW32(targetR, targetG, targetB, targetW);
  if (numChannels == 5) {
    cct.ww = targetW;
    cct.cw = targetCW;
  }
}

// Resolve the wire channel layout for a bus type + color order. No-op for custom (user-entered) layouts.
// Handles the fixed-map types WS2812_1CH_X3 / WS2812_WWA for config backwards compatibilty (they could now be handled as custom bus layouts)
inline void resolveChannelLayout(BusChannelLayout& ch, uint8_t busType, uint8_t colorOrder) {
  if (ch.custom) return;
  if (busType == TYPE_WS2812_1CH_X3) {
    ch.numChannels = 3;
    ch.channelMap[0] = ch.channelMap[1] = ch.channelMap[2] = WLEDpixelBus::CH_W;
  } else if (busType == TYPE_WS2812_WWA) {
    ch.numChannels = 3;
    ch.channelMap[0] = WLEDpixelBus::CH_CW;
    ch.channelMap[1] = WLEDpixelBus::CH_WW;
    ch.channelMap[2] = WLEDpixelBus::CH_WW; // separate amber channel is not yet supported
  } else {
    ch.numChannels = legacyColorOrderToChannelMap(busType, colorOrder, ch.channelMap);
  }
  ch.invertMask = 0;
  ch.is16bit = busTypeIs16bit(busType);
  ch.invertOutput = busType == TYPE_TM1814 || busType == TYPE_TM1815 || busType == TYPE_TM1914;
  const WLEDpixelBus::LedTiming timing = WLEDpixelBus::getLedTiming(busType);
  ch.t0h = timing.t0h_ns; ch.t0l = timing.t0l_ns;
  ch.t1h = timing.t1h_ns; ch.t1l = timing.t1l_ns;
  ch.trst = timing.reset_us;
}

// Resolve bc.channels from type + colorOrder (no-op when bc.channels.custom).
// 2-pin (clocked) buses have a fixed wire format, so custom channel maps are cleared for them.
inline void resolveBusConfigLayout(BusConfig& bc) {
  if (busTypeIs2Pin(bc.type)) bc.channels.custom = false;
  resolveChannelLayout(bc.channels, bc.type, bc.colorOrder);
}

// Type-derived hardware properties: prefix/suffix bytes and output inversion (special chip types only)
inline void applyTypeFraming(WLEDpixelBus::BusProperties& props, uint8_t busType) {
  if (busType == TYPE_TM1814 || busType == TYPE_TM1815) {
    props.prefixLen = WLEDpixelBus::TM1814_PREFIX_LEN; // prefix data is calculated per frame and contains brightness scaling
    props.invertOutput = true;
  } else if (busType == TYPE_TM1914) {
    props.prefixLen = WLEDpixelBus::TM1914_PREFIX_LEN;
    memcpy(props.prefixData, WLEDpixelBus::TM1914_PREFIX, sizeof(WLEDpixelBus::TM1914_PREFIX));
    props.invertOutput = true;
  } else if (busType == TYPE_SM16825) {
    props.suffixLen = WLEDpixelBus::SM16825_SUFFIX_LEN;
    memcpy(props.suffixData, WLEDpixelBus::SM16825_SUFFIX, sizeof(WLEDpixelBus::SM16825_SUFFIX));
  }
}

// Assemble the complete one-wire wire descriptor from a resolved channel layout (this is then used to create the actual bus)
// Note: all (or most) of the above legacy handling could be removed if the full bus properties were stored in config (and also handed down from the UI)
inline WLEDpixelBus::BusProperties makeBusProperties(const BusChannelLayout& layout, uint8_t protocolType) {
  WLEDpixelBus::BusProperties props;
  props.numChannels = layout.numChannels;
  memcpy(props.channelMap, layout.channelMap, sizeof(props.channelMap));
  props.invertMask = layout.invertMask;
  props.is16bit = layout.is16bit;
  props.invertOutput = layout.invertOutput;
  props.timing = WLEDpixelBus::LedTiming(layout.t0h, layout.t0l, layout.t1h, layout.t1l, layout.trst);
  applyTypeFraming(props, protocolType);
  return props;
}

class PixelBusAllocator {
  private:
  #ifndef ESP8266
    static uint8_t _rmtChannelsAssigned;
    static uint8_t _parHwChannelsAssigned;    // parallel output channels: I2S/LCD (ESP32/S2/S3), parallel SPI (C3), PARLIO (C6/H2/C5/P4)
    static uint8_t _parHwBusType; // Track first parallel bus type to enforce parallel timing
    static uint8_t _bitBangChannelsAssigned;
    static uint8_t _bitBangBusType;    // Track first BitBang type to enforce parallel timing
    static uint8_t _hardwareSPIused;   // number of hardware SPI's used, currently only one SPI output is supported. On C3, parallel SPI output takes priority
  #else
    static uint8_t _bitBangBusType;    // Track first ESP8266 BitBang type to enforce parallel timing
  #endif
  public:

  // Shared-clock drivers (parallel HW, bit-bang) lock every channel to the first bus type.
  static uint8_t checkParallelType(uint8_t busType, uint8_t driverType) {
    #ifndef ESP8266
    if (driverType == BUSDRV_PARHW && _parHwBusType != 0) return _parHwBusType;
    #endif
    if (driverType == BUSDRV_BITBANG && _bitBangBusType != 0) return _bitBangBusType;
    return busType;
  }

  static void resetChannelTracking() {
    #ifndef ESP8266
    _rmtChannelsAssigned = 0;
    _parHwChannelsAssigned = 0;
    _parHwBusType = 0; // TYPE_NONE
    _bitBangChannelsAssigned = 0;
    _bitBangBusType = 0; // TYPE_NONE
    _hardwareSPIused = 0;
    WLEDpixelBus::RmtBus::resetChannelTracking();
    #if (WLED_MAX_BB_CHANNELS > 0)
    WLEDpixelBus::BitBangBus::resetChannelTracking();
    #endif
    #else
    _bitBangBusType = 0; // TYPE_NONE
    WLEDpixelBus::BitBangBus::resetChannelTracking();
    #endif
  }

  // RMT driver sizes its channel pool from this count, so it must be set before the RMT buses are constructed
  static void setExpectedRmtChannelCount(uint8_t count) {
    #ifndef ESP8266
    WLEDpixelBus::RmtBus::setExpectedChannelCount(count);
    #endif
  }

  static bool allocateHardware(uint8_t busType, const uint8_t* pins, uint8_t& driverType) {
    if (!busTypeIsDigital(busType)) return false;

    if (busTypeIs2Pin(busType)) {
      // TODO: could check if an SPI is still available and set _hardwareSPIused to 1 to prevent hardware collision
      // note: SPI is intentionally not reserved here as only one is supported and first come first serve is used in create()
      return true; // for now, allow as many SPI buses as the UI allows. First one uses hardware SPI if available (on C3, if a parallel SPI output is used it takes priority)
    }

    #ifndef ESP8266
    // Driver fallback order: requested driver -> parallel bus (I2S/LCD/SPI/PARLIO) -> BitBang.
    // If the requested driver has no free channel (or is unsupported on this target), the next
    // available driver is used instead.
    if (driverType == BUSDRV_RMT && _rmtChannelsAssigned < WLED_MAX_RMT_CHANNELS) {
      _rmtChannelsAssigned++;
      return true;
    }
    if (driverType != BUSDRV_BITBANG && _parHwChannelsAssigned < WLED_MAX_PARHW_CHANNELS) {
      // BUSDRV_PARHW maps to the chip's parallel output peripheral: I2S on ESP32/S2, LCD on S3, parallel SPI on C3, PARLIO on C6/H2/C5/P4, all use 4-step cadence
      driverType = BUSDRV_PARHW;
      if (_parHwChannelsAssigned == 0) {
        _parHwBusType = busType; // lock LED type to first parallel channel
        #ifdef CONFIG_IDF_TARGET_ESP32C3
        _hardwareSPIused++; // reserve SPI: C3 uses parallel SPI output for "I2S" and takes priority over 2pin buses
        #endif
      }
      _parHwChannelsAssigned++;
      return true;
    }
    // Last resort (or explicitly requested): parallel BitBang, all channels share one timing
    if (_bitBangChannelsAssigned < WLED_MAX_BB_CHANNELS) {
      driverType = BUSDRV_BITBANG;
      if (_bitBangBusType == 0) {
        _bitBangBusType = busType; // lock LED type to first BitBang channel
      } else if (_bitBangBusType != busType) {
        return false; // mismatched LED type — all BitBang channels must share timing
      }
      _bitBangChannelsAssigned++;
      return true;
    }
    return false; // No channels available
    #else
    // ESP8266: assign driverType based on pin number so BusManager::show() can sequence correctly.
    // GPIO1/2 → UART (async, fire-and-forget ISR)
    // GPIO3   → DMA  (async, I2S SLC DMA)
    // others  → BitBang (interrupt-blocking — must run before async buses)
    if (pins[0] == 1 || pins[0] == 2) {
      driverType = BUSDRV_RMT; // reuse BUSDRV_RMT as "async UART" sentinel on ESP8266
    } else if (pins[0] == 3) {
      driverType = BUSDRV_PARHW; // async DMA
    } else {
      driverType = BUSDRV_BITBANG;
      // Enforce single LED type for parallel timing
      if (_bitBangBusType == 0) {
        _bitBangBusType = busType;
      } else if (_bitBangBusType != busType) {
        return false; // mismatched LED type — all ESP8266 BitBang channels must share timing
      }
    }
    #endif

    return true;
  }

// One-wire buses require properties (built from the resolved LED type). Clocked 2-pin buses ignore them.
static WLEDpixelBus::PixelBus* create(uint8_t busType, uint8_t* pins, uint16_t len, uint8_t colorOrder, uint8_t driverType = BUSDRV_RMT, uint8_t busSpeedFactor = 100, uint16_t frequencykHz = 0, const WLEDpixelBus::BusProperties* properties = nullptr) {
    if (!busTypeIsDigital(busType)) return nullptr;

    // Clocked chips are not part of the one-wire property path and do not share the parallel-type lock.
    if (busTypeIs2Pin(busType)) {
      bool isHSPI = false;
      #ifdef ESP8266
      if (pins[0] == P_8266_HS_MOSI && pins[1] == P_8266_HS_CLK) isHSPI = true;
      #else
      if (_hardwareSPIused == 0) {
        isHSPI = true;
        _hardwareSPIused++; // claim hardware SPI (currently only one is supported), on C3 this can also be claimed by parallel SPI (done so in allocateHardware)
      }
      #endif
      const uint8_t numChannels = (uint8_t)busTypeGetNumberOfChannels(busType);
      WLEDpixelBus::SpiBus* bus = new WLEDpixelBus::SpiBus(pins[0], pins[1], frequencykHz, colorOrder, numChannels, isHSPI, busType); // TODO: move this into createbus function?
      // Override the generic co-based encoder with the legacy chip-specific wire layout
      // (WS2801 = R,B,G; APA102/P9813 = B,G,R; LPD8806/LPD6803 = G,R,B).
      uint8_t channelMap[WLEDpixelBus::MAX_CUSTOM_CHANNELS];
      const uint8_t wireCh = legacyColorOrderToChannelMap(busType, colorOrder, channelMap);
      bus->setEncoder(WLEDpixelBus::ColorEncoder(channelMap, wireCh, 0, false));
      return bus;
    }

    const uint8_t requestedType = busType;
    busType = checkParallelType(busType, driverType); // all parallel output buses must share the same type

    WLEDpixelBus::BusDriver driver = WLEDpixelBus::BusDriver::RMT; // always overwritten below; initialised to avoid unused-variable warning

    #ifdef ESP8266
    if (pins[0] == 1 || pins[0] == 2) driver = WLEDpixelBus::BusDriver::UART; // GPIO1=TX0, GPIO2=TX1, TX0 is used for debug if enabled
    else if (pins[0] == 3) driver = WLEDpixelBus::BusDriver::DMA; // DMA method uses a lot of RAM!
    else driver = WLEDpixelBus::BusDriver::BitBang;
    #elif !defined(CONFIG_IDF_TARGET_ESP32C61)
    switch (driverType) {
      case BUSDRV_RMT:
        driver = WLEDpixelBus::BusDriver::RMT;
        break;
      case BUSDRV_PARHW:
        #if defined(CONFIG_IDF_TARGET_ESP32C3)  //TODO: should use hardware capabilities instead of MCU types
        driver = WLEDpixelBus::BusDriver::SPI; // parallel SPI on C3
        #elif defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)
        driver = WLEDpixelBus::BusDriver::PARLIO; // PARLIO on C5, C6, H2, P4
        #else
        driver = WLEDpixelBus::BusDriver::I2S; // ESP32 & S2: use parallel I2S, S3: use LCD parallel out
        #endif
        break;
      default:
        driver = WLEDpixelBus::BusDriver::BitBang;
        break;
    }
    #else
    // C61 only supports BB for now (might be able to use parallel SPI), it has no RMT, no I2S, no PARLIO hardware
    driver = WLEDpixelBus::BusDriver::BitBang;
    #endif

    if (!properties) return nullptr; // one-wire buses are created from an explicit property set
    WLEDpixelBus::BusProperties wire = *properties;
    // Caller should already have used checkParallelType(). If it did not, keep the shared clock consistent.
    if (busType != requestedType) wire.timing = WLEDpixelBus::getLedTiming(busType);
    if (busSpeedFactor != 100) {
      float factor = (float)busSpeedFactor / 100.0f;
      wire.timing = WLEDpixelBus::scaleTiming(wire.timing, factor);
    }

    return WLEDpixelBus::createBus(driver, pins[0], wire, busType, len);
  }
};
#endif
