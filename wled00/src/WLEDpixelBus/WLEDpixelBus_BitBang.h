/*-------------------------------------------------------------------------
WLEDpixelBus — Parallel bit-bang LED output driver

written by Damian Schneider @dedehai 2026

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

// Note: maximum number of parallel BitBang channels WLED_MAX_BB_CHANNELS is defined in const.h

namespace WLEDpixelBus {
class BitBangBus : public PixelBus {
public:
  BitBangBus(int8_t pin, const LedTiming& timing, uint8_t ledType = 0);
  ~BitBangBus() override;

  bool begin() override;
  void end()   override;

  bool show() override; // Stage this channel's data.  When all channels are staged, output all in parallel.
  bool canShow() const override { return true; } // BitBang output is synchronous — always ready  TODO: on multi-core systems with tasks running on different cores, this is not true (but currently there is no seperate task)

#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "BitBang"; }
#endif

  static void resetChannelTracking(); // reset shared channel tracking, called by PixelBusAllocator

private:
  int8_t    _pin         = -1;
  LedTiming _timing;
  bool      _initialized = false;

  // Shared state (one context for ALL BitBangBus instances), all timings must be the same (enforced by PixelBusAllocator).
  struct BBstate {
    const uint8_t* encodeBuffer[WLED_MAX_BB_CHANNELS]; // encoded color data to be sent out
    size_t        encodeBufferSize[WLED_MAX_BB_CHANNELS];
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
    uint8_t       channelCount;
    uint8_t       stagedCount;  // how many channels have called show() this frame
    bool          allowInterrupts; // if enabled, interrupts are allowed to run between LEDs, if not the output is fully blocking
  };
  static BBstate* _bitBangState;

  static bool IRAM_ATTR outputParallel();
};

} // namespace WLEDpixelBus
