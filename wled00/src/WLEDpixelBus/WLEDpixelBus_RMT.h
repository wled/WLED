#pragma once

#include "WLEDpixelBus.h"
#include "driver/rmt.h"
#include "RmtHiDriver.h"
#include "esp_rom_gpio.h" // for gpio routing to set inverted signal

namespace WLEDpixelBus {

//=======================================
// RMT Bus - Works on all ESP32 variants
//=======================================

class RmtBus : public PixelBus {
public:
  /**
   * Create RMT bus
   * @param pin GPIO pin
   * @param timing LED timing
   * @param order Color order
   * @param channel RMT channel (-1 for auto)
   */
  RmtBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels,
       int8_t channel = -1, uint8_t ledType = 0);
  ~RmtBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels, uint16_t numPixels,
        const CctPixel* cct = nullptr) override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "RMT"; }
#endif

  // Configuration
  void setInverted(bool inv) override {
    _inverted = inv;
  }
  void setTiming(const LedTiming& timing);
  void setColorOrder(uint8_t co);

  // Reset the auto-allocation counter (call before re-creating buses)
  static void setExpectedChannels(uint8_t expected) { s_expectedChannels = (expected > 0) ? expected : 1; }
  static void resetAutoChannel() {
    s_allocatedCount = 0;
    s_currentChannelIndex = 0;
    s_usedBlocks = 0;
  }

private:
  int8_t _pin;
  int8_t _channel;
  bool _inverted;
  bool _initialized;
  LedTiming _timing;
  rmt_channel_t _rmtChannel;
  uint32_t _rmtBit0;
  uint32_t _rmtBit1;
  uint16_t _rmtResetTicks;
  bool _usingRmtHi;

  // _encodeBuffer and _encodeBufferSize are in PixelBus base

  static uint8_t s_expectedChannels;
  static uint8_t s_allocatedCount;
  static uint8_t s_currentChannelIndex;
  static uint8_t s_usedBlocks;
  static uint8_t s_activeChannelMask; // bitmask of initialized channels

  void updateRmtTiming();

  // Per-channel translator context and helpers
  struct RmtContext {
    uint32_t bit0;
    uint32_t bit1;
    uint16_t resetDuration;
  };

  // Static lookup table for ISR speed (max 8 channels)
  static RmtContext s_contexts[8];

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
  static const sample_to_rmt_t s_callbacks[8]; // TODO: could define this above and actually use the correct amount of callbacks
};

} // namespace WLEDpixelBus

