/*-------------------------------------------------------------------------
WLEDpixelBus_BitBang — Parallel bit-bang LED output driver for ESP32.

All configured BitBang channels share a single static context.  Each bus
registers its GPIO pin at begin().  When the last registered channel calls
show(), all channels are clocked out in parallel using register-level GPIO
writes and a uint32_t bitmask.

Constraints / design notes:
  • ESP32 only (not ESP8266 — that already has its own BitBangBus).
  • GPIO pin ranges per variant:
      Classic ESP32: 0–39 (low bank 0–31 via GPIO_OUT_W1TS/TC_REG;
                           high bank 32–39 via GPIO_OUT1_W1TS/TC_REG)
      ESP32-S2:      0–46 (same dual-bank split at pin 32)
      ESP32-S3:      0–48 (same dual-bank split at pin 32)
      All others (C3/C6/H2): 0–31 (low bank only)
  • All BitBang channels MUST use the same LED type / timing (enforced by
    PixelBusAllocator when driverType == 2).
  • The ISR lock is released and re-acquired after every output bit so that
    the FreeRTOS scheduler and time-critical ISRs can still run.  If the gap
    exceeds the LED latch (reset) threshold the output is aborted.
  • driverType value: 2  (0=RMT, 1=I2S/LCD/SPI, 2=BitBang)
-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"

#if defined(ARDUINO_ARCH_ESP32)

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
//#include "soc/gpio_struct.h"   // GPIO.out_w1ts / GPIO.out_w1tc -> better use REG_WRITE
#include "soc/gpio_reg.h"
#include "driver/gpio.h"

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

  /** Stage this channel's data.  When all channels are staged, output all in parallel. */
  bool show(const uint32_t* = nullptr, uint16_t = 0,
            const CctPixel* = nullptr) override;

  /** BitBang output is synchronous — always ready. */
  bool canShow() const override { return true; }

#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "BitBang"; }
#endif

  /** Reset the shared static channel registry.
   *  Called by PixelBusAllocator::resetChannelTracking() when all buses are destroyed. */
  static void resetChannels();

private:
  int8_t    _pin         = -1;
  bool      _initialized = false;

  LedTiming _rawTiming;       // saved at construction, converted to cycles in begin()

  // -----------------------------------------------------------------------
  // Shared static state (one context for ALL BitBangBus instances)
  // All timing fields are static because every BitBang bus must use the
  // same LED type (enforced by PixelBusAllocator).
  // -----------------------------------------------------------------------
  static int8_t        s_pins[WLED_MAX_BB_CHANNELS];
  static uint16_t      s_numPixels[WLED_MAX_BB_CHANNELS];  // pixel count per channel
  static uint8_t*      s_pixelData[WLED_MAX_BB_CHANNELS];  // encoded data pointer per channel
  static uint8_t       s_channelCount;
  static uint32_t      s_allMask;      // GPIO bitmask of registered pins 0–31
#ifdef ESP_HAS_HIGH_GPIO_BANK
  static uint32_t      s_allMaskHigh;  // GPIO bitmask of registered pins 32+ (ESP32/S2/S3)
#endif
  static uint8_t       s_stagedCount;  // how many channels have called show() this frame
  static portMUX_TYPE  s_mux;          // critical-section lock used inside outputParallel()
  // Timing in CPU cycles — identical across all channels
  static uint32_t      s_t0h;
  static uint32_t      s_t1h;
  static uint32_t      s_period;
  static uint32_t      s_latchCycles;
  static uint8_t       s_pixelBytes;  // bytes per encoded pixel (derived from LED type)

  // -----------------------------------------------------------------------
  // Core output routine — must run from IRAM for timing accuracy
  // -----------------------------------------------------------------------
  static bool IRAM_ATTR outputParallel();
};

} // namespace WLEDpixelBus

#endif // ARDUINO_ARCH_ESP32
