/*-------------------------------------------------------------------------

WLEDpixelBus - RMT output driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

RMT bus works on ESP32, S3, S2 and C3
Supports auto-distribution of available RMT memory blocks to reduce interrupt frequency - needs to be refined if ever using RMT input
The glitch-free high priority interrupt implementation by @willmmiles is not available on the C3 and not at all in IDF V5

-------------------------------------------------------------------------*/

#pragma once

#if SOC_RMT_TX_CANDIDATES_PER_GROUP > 4
# define WPB_RMT_CHANNELS 8
#elif SOC_RMT_TX_CANDIDATES_PER_GROUP > 2
# define WPB_RMT_CHANNELS 4
#else
# define WPB_RMT_CHANNELS 2
#endif

#include "WLEDpixelBus.h"
#include "driver/rmt.h"
#include "RmtHIDriver.h"  // high interrupt priority driver, only on ESP32, S2, S3 using IDF V4
#include "esp_rom_gpio.h" // for gpio routing to set inverted signal

namespace WLEDpixelBus {

//=======================================
// RMT Bus
//=======================================

class RmtBus : public PixelBus {
public:
  /**
   * Create RMT bus
   * @param pin GPIO pin
   * @param timing LED timing
   * @param order Color order
   */
  RmtBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType = 0);
  ~RmtBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels, uint16_t numPixels,
        const CctPixel* cct = nullptr) override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "RMT"; }
#endif

  void setInverted(bool inv) override;
  void setColorOrder(uint8_t co);

  // Reset the auto-allocation counter (call before re-creating buses)
  static void setExpectedChannels(uint8_t expected) { expectedChannels = (expected > 0) ? expected : 1; }
  static void resetAutoChannel() {
    allocatedCount = 0;
    currentChannelIndex = 0;
    usedBlocks = 0;
  }

private:
  int8_t _pin;
  int8_t _channel;
  bool _inverted;
  bool _initialized;
  LedTiming _timing;
  rmt_channel_t _rmtChannel;

  // _encodeBuffer and _encodeBufferSize are in PixelBus base
  static uint8_t expectedChannels; // TODO: make none static
  static uint8_t allocatedCount;
  static uint8_t currentChannelIndex;
  static uint8_t usedBlocks;
  static uint8_t activeChannelMask; // bitmask of initialized channels

  void updateRmtTiming();

  // Per-channel translator context and helpers
  struct RmtContext {
    uint32_t bit0;
    uint32_t bit1;
    uint16_t resetDuration;
  };

  // Static lookup table for timing speeds
  static RmtContext contexts[WPB_RMT_CHANNELS];

  // Explicit wrappers: implemented in .cpp file to ensure they are placed in IRAM
  static void IRAM_ATTR translator_ch0(const void* src, rmt_item32_t* dest, size_t s, size_t w, size_t* ts, size_t* in);
  static void IRAM_ATTR translator_ch1(const void* src, rmt_item32_t* dest, size_t s, size_t w, size_t* ts, size_t* in);
  #if SOC_RMT_TX_CANDIDATES_PER_GROUP > 2
  static void IRAM_ATTR translator_ch2(const void* src, rmt_item32_t* dest, size_t s, size_t w, size_t* ts, size_t* in);
  static void IRAM_ATTR translator_ch3(const void* src, rmt_item32_t* dest, size_t s, size_t w, size_t* ts, size_t* in);
  #endif
  #if SOC_RMT_TX_CANDIDATES_PER_GROUP > 4
  static void IRAM_ATTR translator_ch4(const void* src, rmt_item32_t* dest, size_t s, size_t w, size_t* ts, size_t* in);
  static void IRAM_ATTR translator_ch5(const void* src, rmt_item32_t* dest, size_t s, size_t w, size_t* ts, size_t* in);
  static void IRAM_ATTR translator_ch6(const void* src, rmt_item32_t* dest, size_t s, size_t w, size_t* ts, size_t* in);
  static void IRAM_ATTR translator_ch7(const void* src, rmt_item32_t* dest, size_t s, size_t w, size_t* ts, size_t* in);
  #endif
  // Actual translator implementation (defined in .cpp)
  static void IRAM_ATTR translateInternal(uint8_t channel, const void* src, rmt_item32_t* dest,
                                          size_t src_size, size_t wanted_num,
                                          size_t* translated_size, size_t* item_num);

  // Jump table of callbacks (defined in .cpp). Use 8 entries to match max RMT channels.
  static const sample_to_rmt_t callbacks[WPB_RMT_CHANNELS];
};

} // namespace WLEDpixelBus

