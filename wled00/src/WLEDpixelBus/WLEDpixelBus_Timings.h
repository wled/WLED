#pragma once

#include <stdint.h>

namespace WLEDpixelBus {

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

    // Calculate bit period in ns
    constexpr uint32_t bitPeriod() const { 
        return (t0h_ns + t0l_ns + t1h_ns + t1l_ns) / 2; 
    }
};

// Predefined timing constants
namespace Timing {
    // ---- Standard 1-wire LEDs (800KHz family) ----
    constexpr LedTiming WS2812  {300, 900, 800, 450, 100};   // WS2812B
    constexpr LedTiming WS2811  {300, 950, 900, 350, 300};   // WS2811 (12V)
    constexpr LedTiming WS2813  {400, 850, 800, 450, 300};   // WS2813 (backup data)
    constexpr LedTiming WS2815  {400, 850, 800, 450, 300};   // WS2815 (12V, 255mA)
    constexpr LedTiming WS2805  {300, 790, 790, 300, 300};   // WS2805 RGBCW (5ch)
    constexpr LedTiming SK6812  {400, 850, 800, 450, 80};    // SK6812 / SK6812 RGBW

    // ---- Titan Micro LEDs ----
    constexpr LedTiming TM1814  {360, 890, 720, 530, 200};   // TM1814 RGBW, requires prefix
    constexpr LedTiming TM1829  {300, 900, 800, 400, 200};   // TM1829 (inverted logic)
    constexpr LedTiming TM1914  {360, 890, 720, 530, 200};   // TM1914 RGB, requires prefix

    // ---- Other 1-wire LEDs ----
    constexpr LedTiming APA106  {350, 1350, 1350, 350, 50};  // APA106 / PL9823
    constexpr LedTiming UCS8903 {400, 850, 800, 450, 500};   // UCS8903 RGB (16bit)
    constexpr LedTiming UCS8904 {400, 850, 800, 450, 500};   // UCS8904 RGBW (16bit)
    constexpr LedTiming SM16703 {300, 900, 900, 300, 80};    // SM16703
    constexpr LedTiming SM16825 {300, 900, 900, 300, 80};    // SM16825 RGBCW (16bit, 5ch)
    constexpr LedTiming FW1906  {400, 850, 800, 450, 300};   // FW1906 GRBCW (5ch)

    // ---- 400 KHz LEDs ----
    constexpr LedTiming Generic800Kbps {400, 850, 800, 450, 300};
    constexpr LedTiming Generic400Kbps {800, 1700, 1600, 900, 300};

    // ---- 2-wire (SPI) LEDs - timing represents SPI clock speed ----
    // For SPI LEDs the timing struct is repurposed: bitPeriod() gives the SPI clock period
    // Default SPI clock ~2MHz (500ns period)
    constexpr LedTiming APA102  {250, 250, 250, 250, 0};     // APA102 / DotStar (up to ~20MHz)
    constexpr LedTiming WS2801  {500, 500, 500, 500, 1000};  // WS2801 (max ~2MHz, 500us latch)
    constexpr LedTiming LPD8806 {250, 250, 250, 250, 0};     // LPD8806 (latch = 0-bits)
    constexpr LedTiming LPD6803 {250, 250, 250, 250, 0};     // LPD6803
    constexpr LedTiming P9813   {250, 250, 250, 250, 0};     // P9813 (Total Control Lighting)
}

// ---- I2S / LCD Clock Calculation Helpers ----

/**
 * Calculate the I2S/LCD clock divider from LED timing and cadence steps
 * @param timing LED timing parameters
 * @param cadenceSteps Number of cadence steps (3 or 4)
 * @param baseClockMHz Base clock in MHz (160 for ESP32, 80 for S2, 240 for S3 LCD)
 * @return Clock divider value
 */
inline uint32_t calcClockDiv(const LedTiming& timing, uint8_t cadenceSteps, uint32_t baseClockMHz) {
    uint32_t bitPeriodNs = timing.bitPeriod();
    uint32_t stepPeriodNs = bitPeriodNs / cadenceSteps;
    uint32_t div = (baseClockMHz * stepPeriodNs) / 1000;
    return (div < 2) ? 2 : (div > 255) ? 255 : div;
}

/**
 * Calculate reset bytes needed for I2S/LCD DMA
 * @param timing LED timing parameters
 * @param cadenceSteps Number of cadence steps
 * @return Number of zero-bytes needed for reset period
 */
inline uint32_t calcResetBytes(const LedTiming& timing, uint8_t cadenceSteps) {
    uint32_t bitPeriodNs = timing.bitPeriod();
    uint32_t clockPeriodNs = bitPeriodNs / cadenceSteps;
    uint32_t bytesPerUs = (clockPeriodNs > 0) ? (1000 / clockPeriodNs) : 1;
    return timing.reset_us * bytesPerUs;
}

} // namespace WLEDpixelBus
