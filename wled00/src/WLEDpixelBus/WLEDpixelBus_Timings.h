#pragma once

#include <stdint.h>
#include "../../const.h"

namespace WLEDpixelBus {

// Note on timings regarding I2S/LCD/SPI buses (4 step cadence):
// the drivers will always use a 1/4 - 3/4 duty cycle for '0' and '1' bits (e.g. 250ns high + 750ns low for a 1us period)
// it only derives the total period from the given timings for RMT (0hi+0lo+1hi+1lo)/2)
// TODO: a better strategy might be to use 0hi*4 as the period as 0 timing is most critical

/**
 * LED timing parameters in nanoseconds
 */
struct LedTiming {
  uint16_t t0h_ns;    // '0' bit high time
  uint16_t t0l_ns;    // '0' bit low time
  uint16_t t1h_ns;    // '1' bit high time
  uint16_t t1l_ns;    // '1' bit low time
  uint32_t reset_us;  // Reset/latch time in microseconds

  constexpr LedTiming(uint16_t t0h, uint16_t t0l, uint16_t t1h, uint16_t t1l, uint32_t reset)
    : t0h_ns(t0h), t0l_ns(t0l), t1h_ns(t1h), t1l_ns(t1l), reset_us(reset) {}

  // Calculate bit period in ns as the average of all four pulse timings
  constexpr uint32_t bitPeriod() const {
    return (t0h_ns + t0l_ns + t1h_ns + t1l_ns) / 2;
  }
};

/**
 * Scale LED timing parameters by a floating point factor (percent expressed as factor, e.g. 1.2 for +20%)
 * Only t* timings (ns) are scaled; reset_us is preserved.
 */
inline LedTiming scaleTiming(const LedTiming& timing, float factor) {
  auto s = [&](uint32_t v)->uint16_t {
    uint32_t r = (uint32_t)(v * factor + 0.5f);
    if (r < 1) r = 1;
    if (r > 0xFFFF) r = 0xFFFF;
    return (uint16_t)r;
  };
  return LedTiming(s(timing.t0h_ns), s(timing.t0l_ns), s(timing.t1h_ns), s(timing.t1l_ns), timing.reset_us);
}

// LED timing lookup table in flash (PROGMEM on ESP8266, .rodata on ESP32).
//
// WHY PROGMEM: On ESP8266 the linker only sends specific .rodata.* patterns to flash
// (vtables, string literals, RTTI). All other .rodata — including named namespace-scope
// constexpr variables — lands in DRAM. PROGMEM (__attribute__((section(".irom.text"))))
// is the only escape. This table is read once at bus-creation time; the bus stores its own
// copy. Zero permanent DRAM cost.
//
// On ESP32 PROGMEM is a no-op; const data naturally goes to flash via the SPI cache.
//
// Indexed by getTimingIndex() below. The switch returns uint8_t (not a struct) so the
// compiler generates code-based selection, not a DRAM struct-copy table.
//
// Entries with identical timing values are shared: TM1814≡TM1914, UCS8903≡UCS8904,
// APA102≡LPD8806≡P9813≡LPD6803.
static const PROGMEM LedTiming s_ledTimings[] = {
//            t0h   t0l   t1h   t1l  reset_us
/* 0 WS2812 */ { 300,  900,  700,  500,  100 },  // WS2812B (and 1CH_X3, 2CH_X3, WWA)
/* 1 400KHz */ { 800, 1700, 1600,  900,  300 },  // Generic 400Kbps
/* 2 TM1829 */ { 300,  900,  800,  400,  200 },  // TM1829
/* 3 UCS8x03*/ { 400,  850,  800,  450,  500 },  // UCS8903 / UCS8904 (16-bit)
/* 4 APA106 */ { 350, 1350, 1350,  350,   50 },  // APA106 / PL9823
/* 5 TM1x14 */ { 360,  890,  720,  530,  200 },  // TM1914 / TM1814 (same timing)
/* 6 SK6812 */ { 300,  900,  800,  450,  200 },  // SK6812 / SK6812 RGBW
/* 7 TM1815 */ { 740, 1780, 1440, 1060,  200 },  // TM1815
/* 8 FW1906 */ { 400,  850,  800,  450,  300 },  // FW1906 GRBCW
/* 9 WS2805 */ { 300,  790,  790,  300,  300 },  // WS2805 RGBCW
/*10 SM16825*/ { 300,  900,  900,  300,   80 },  // SM16825 (16-bit)
/*11 SPI    */ { 250,  250,  250,  250,    0 },  // APA102 / LPD8806 / P9813 / LPD6803
/*12 WS2801 */ { 500,  500,  500,  500, 1000 },  // WS2801
// TYPE_CUSTOM_BUS (36) timing is passed directly as a LedTiming struct; no fixed entries here.
};

// Maps WLED TYPE_ to an index into s_ledTimings.
// Returns uint8_t — the compiler generates code-based selection, not a 12-byte-per-entry
// DRAM struct table.
static inline uint8_t getTimingIndex(uint8_t wledType) {
  switch (wledType) {
    case TYPE_WS2812_RGB:
    case TYPE_WS2812_WWA:    return  0; // migration: WWA kept for timing selection only
    case TYPE_WS2811_400KHZ: return  1;
    case TYPE_TM1829:        return  2;
    case TYPE_UCS8903:
    case TYPE_UCS8904:       return  3; // identical timing
    case TYPE_APA106:        return  4;
    case TYPE_TM1914:
    case TYPE_TM1814:        return  5; // identical timing
    case TYPE_SK6812_RGBW:   return  6;
    case TYPE_TM1815:        return  7;
    case TYPE_FW1906:        return  8;
    case TYPE_WS2805:        return  9;
    case TYPE_SM16825:       return 10;
    case TYPE_APA102:
    case TYPE_LPD8806:
    case TYPE_P9813:
    case TYPE_LPD6803:       return 11; // TODO: SPI types need testing
    case TYPE_WS2801:        return 12;
    // TYPE_CUSTOM_BUS (36) timing is provided directly as a LedTiming* to create().
    default:                 return  0; // WS2812 fallback
  }
}

// Returns the LED timing for the given WLED bus type, read from flash.
// This is a one-time read at bus-creation; the bus constructor stores its own copy.
inline LedTiming getProtocol(uint8_t wledType) {
  return s_ledTimings[getTimingIndex(wledType)];
}

} // namespace WLEDpixelBus
