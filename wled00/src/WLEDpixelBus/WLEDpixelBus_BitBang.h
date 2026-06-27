/*-------------------------------------------------------------------------
WLEDpixelBus — Parallel bit-bang LED output driver

written by Damian Schneider @dedehai 2026

works on all ESP32 variants

Interrupts are fully disabled during output to avoid any glitches
Each bus can have individual configuration of color channels but all must share the same timing

-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
//#include "soc/gpio_struct.h"   // GPIO.out_w1ts / GPIO.out_w1tc -> better use REG_WRITE
#include "soc/gpio_reg.h"
#include "driver/gpio.h"
#elif defined(ESP8266)
#include <Arduino.h>
#include <eagle_soc.h>
#include <esp8266_peri.h>
#include <ets_sys.h>
#endif

namespace WLEDpixelBus {

// Note: maximum number of parallel BitBang channels WLED_MAX_BB_CHANNELS is defined in const.h

class BitBangBus : public PixelBus {
public:
  /**
   * Construct a BitBang bus for one GPIO pin.
   * @param pin         GPIO pin number (must be set in SOC_GPIO_VALID_OUTPUT_GPIO_MASK for this target)
   * @param timing      LED protocol timing (from WLEDpixelBus_Timings)
   * @param colorOrder  WLED colour-order byte
   * @param numChannels Number of colour channels per LED (3 or 4)
   * @param ledType     LED chip type constant (TYPE_*)
   */
  BitBangBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder,
             uint8_t numChannels, uint8_t ledType = 0);
  ~BitBangBus() override;

  bool begin() override;
  void end()   override;
  void setInverted(bool inv) override; // invert output signal
  bool show(const uint32_t* = nullptr, uint16_t = 0, const CctPixel* = nullptr) override; // Stage this channel's data.  When all channels are staged, output all in parallel.
  bool canShow() const override { return true; } // BitBang output is synchronous — always ready  TODO: on multi-core systems with tasks running on different cores, this is not true (but it currently is)

#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "BitBang"; }
#endif

  // Reset the shared static channel registryc, alled by PixelBusAllocator::resetChannelTracking() when all buses are destroyed
  static void resetChannels();

private:
  int8_t    _pin         = -1;
  bool      _inverted    = false; // invert output signal
  bool      _initialized = false;

  LedTiming _rawTiming;       // saved at construction, converted to cycles in begin()

  // -----------------------------------------------------------------------
  // Shared state (one context for ALL BitBangBus instances)
  // All timing fields must use the same LED type (enforced by PixelBusAllocator).
  // -----------------------------------------------------------------------
  struct BBstate {
    uint8_t*      pixelData[WLED_MAX_BB_CHANNELS];  // encoded data pointer per channel
    uint32_t      t0h; // Timing in CPU cycles — identical across all channels
    uint32_t      t1h;
    uint32_t      period;
    uint32_t      latchCycles;
    uint32_t      allMask;      // GPIO bitmask of registered pins 0–31
    #ifdef ESP_HAS_HIGH_GPIO_BANK
    uint32_t      allMaskHigh;  // GPIO bitmask of registered pins 32+ (ESP32/S2/S3)
    #endif
    #if defined(ARDUINO_ARCH_ESP32)
    portMUX_TYPE  mux; // critical-section lock used inside outputParallel()
    #endif
    int8_t        pins[WLED_MAX_BB_CHANNELS];
    uint16_t      numPixels[WLED_MAX_BB_CHANNELS];  // pixel count per channel
    uint8_t       channelCount;
    uint8_t       stagedCount;  // how many channels have called show() this frame
    uint8_t       pixelBytes;  // bytes per encoded pixel (derived from LED type)
  };
  static BBstate* _BBs;

  // -----------------------------------------------------------------------
  // Core output routine — must run from IRAM for timing accuracy
  // -----------------------------------------------------------------------
  static bool IRAM_ATTR outputParallel();
};

} // namespace WLEDpixelBus
