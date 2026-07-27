/*-------------------------------------------------------------------------

WLEDpixelBus - RMT output driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

RMT bus works on ESP32, S3, S2 and C3 (C6/H2 on IDF V5)
Supports auto-distribution of available RMT memory blocks to reduce interrupt frequency - needs to be refined if ever using RMT input
IDF V5: uses the new rmt_tx driver with bytes encoder; the reset/latch gap is enforced in show()
        via a TX-done event callback timestamp instead of being encoded into the waveform
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
#include "esp_idf_version.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
  #include "driver/rmt_tx.h"
  #include "driver/rmt_encoder.h"
#else
  #include "driver/rmt.h"
  #include "RmtHIDriver.h"  // high interrupt priority driver, only on ESP32, S2, S3 using IDF V4
  #include "esp_rom_gpio.h" // for gpio routing to set inverted signal
#endif

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

  bool show() override;
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

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
  // ---- IDF V5: new RMT driver (rmt_tx + bytes encoder) ----
  rmt_channel_handle_t  _rmtChannel = nullptr;
  rmt_encoder_handle_t  _bytesEncoder = nullptr;
  rmt_transmit_config_t _txConfig = {};
  uint32_t _resetUs = 50;
  volatile uint32_t _lastTxEndUs = 0; // written from the TX-done ISR callback

  // TX-done event callback: stamps the end of each frame so show() can enforce the reset/latch gap
  static bool IRAM_ATTR onTxDone(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_ctx);
#else
  // ---- IDF V4: legacy RMT driver ----
  rmt_channel_t _rmtChannel;

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
#endif

  // _encodeBuffer and _encodeBufferSize are in PixelBus base
  static uint8_t expectedChannels; // TODO: make none static? would save a few bytes of ram but use more heap
  static uint8_t allocatedCount;
  static uint8_t currentChannelIndex;
  static uint8_t usedBlocks;
  static uint8_t activeChannelMask; // bitmask of initialized channels
};

} // namespace WLEDpixelBus