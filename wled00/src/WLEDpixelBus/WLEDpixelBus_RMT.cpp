/*-------------------------------------------------------------------------

WLEDpixelBus - RMT output driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

RMT bus works on ESP32, S3, S2, C3, C5, C6
Supports auto-distribution of available RMT memory blocks to reduce interrupt frequency - needs to be refined if ever using RMT input.
All supported targets use the glitch-free high priority interrupt driver by @willmmiles (WLEDPB_USE_RMTHI)
Other targets should only use a single RMT output to avoid glitches due to interrupt latency
Note: S3 would support GDMA for RMT but all others variants do not, it is not implemented

-------------------------------------------------------------------------*/

#include "WLEDpixelBus.h"
#ifdef ARDUINO_ARCH_ESP32
#include "WLEDpixelBus_RMT.h"

#ifdef WLEDPB_USE_RMTHI
#include "esp_rom_gpio.h" // for gpio routing to set inverted signal
#include "soc/rmt_struct.h"
#include "soc/gpio_sig_map.h" // RMT_SIG_OUT0_IDX
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
#include "hal/rmt_ll.h"
#include "esp_private/gpio.h"
#include "esp_private/esp_clk_tree_common.h"
// these macro wrappers require the RCC critical-section, which is not usable from Arduino builds. We call the functions directly (same name)
#undef rmt_ll_enable_bus_clock // macros are defined for some ESP32 variants only, if not defined, this is a no-op
#undef rmt_ll_reset_register
#endif
#endif

namespace WLEDpixelBus {

// Static auto-channel counter for RmtBus
uint8_t RmtBus::expectedChannels = 1;
uint8_t RmtBus::allocatedCount = 0;
uint8_t RmtBus::currentChannelIndex = 0;
uint8_t RmtBus::usedBlocks = 0;

RmtBus::RmtBus(int8_t pin, const LedTiming& timing, uint8_t ledType)
  : _pin(pin)
  , _channel(-1)
  , _initialized(false)
  , _driverInstalled(false)
  , _timing(timing)
{
  // encoder is installed by createBus() from BusProperties
  _ledType = ledType;
}

RmtBus::~RmtBus() {
  end();
}

#ifdef WLEDPB_USE_RMTHI

bool RmtBus::begin() {
  if (_initialized) return true;
  uint8_t blocksToUse = 1;
  uint8_t maxTxChannels = WLED_MAX_RMT_CHANNELS;

  // Auto-channel select with optimized memory block allocation (assumes no RMT RX usage)
  // note on channel allocation: channels are assigned such as to maximize the number of memory blocks
  // available to the channel to minimize the number of interrupts needed for buffer re-fills (less context switching overhead)
  // example: ESP32, 2 channels requested total -> use CH0 with 4 blocks and CH4 with 4 blocks
  // example: ESP32-S3 with 3 channels: use CH0 with 2 block, CH2 with 1 block, and CH3 with 5 blocks
  if (allocatedCount >= expectedChannels || allocatedCount >= maxTxChannels)
    return false;

  // total RMT memory blocks per group equals the total channel count (each channel owns one block,
  // TX and RX channels share the same memory pool)
  const uint8_t totalBlocks = SOC_RMT_CHANNELS_PER_GROUP;

  int left_channels = expectedChannels - allocatedCount - 1;

  if (left_channels == 0) {
    _channel = currentChannelIndex;
    blocksToUse = totalBlocks - usedBlocks;
  } else {
    int k = totalBlocks / expectedChannels;
    int max_k_for_index = maxTxChannels - currentChannelIndex - left_channels;
    if (k > max_k_for_index) k = max_k_for_index;
    if (k < 1) k = 1;

    _channel = currentChannelIndex;
    blocksToUse = k;
  }
  #ifdef RMT_USE_SINGLE_MEM_BLOCK
  blocksToUse = 1;
  #endif

  currentChannelIndex += blocksToUse;
  usedBlocks += blocksToUse;
  allocatedCount++;

  if (_channel >= (int8_t)maxTxChannels) {
    //DEBUG_PRINTF_P(PSTR("[WPB] RMT channel %d >= max %u, FAIL\n"), _channel, maxTxChannels);
    return false;
  }

  //DEBUG_PRINTF_P(PSTR("[WPB] RMT channel %d using %u blocks (total allocated: %u/%u)\n"), _channel, blocksToUse, allocatedCount, maxTxChannels);
  // RMT clock: 40MHz -> 25ns per tick
  const float tickNs = 25.0f;

  auto nsToTicks = [tickNs](uint16_t ns) -> uint16_t {
    uint16_t ticks = (uint16_t)((ns + tickNs / 2) / tickNs);
    return ticks > 0 ? ticks : 1;
  };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
  rmt_symbol_word_t bit0 = {}, bit1 = {};
#else
  rmt_item32_t bit0 = {}, bit1 = {}; // legacy type, same 32-bit layout
#endif
  bit0.level0 = 1; bit0.duration0 = nsToTicks(_timing.t0h_ns);
  bit0.level1 = 0; bit0.duration1 = nsToTicks(_timing.t0l_ns);
  bit1.level0 = 1; bit1.duration0 = nsToTicks(_timing.t1h_ns);
  bit1.level1 = 0; bit1.duration1 = nsToTicks(_timing.t1l_ns);

  esp_err_t err;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5,0,0)
  // The IDF RMT driver is bypassed entirely; configure the peripheral via rmt_ll register access.
  if (allocatedCount == 1) {
    // first bus: enable and reset the peripheral (register reset is global to all channels)
    rmt_ll_enable_bus_clock(0, true);
    rmt_ll_reset_register(0);
    rmt_ll_mem_power_by_pmu(&RMT);
    rmt_ll_enable_mem_access_nonfifo(&RMT, true);
    #if SOC_RMT_SUPPORT_TX_SYNCHRO
    rmt_ll_tx_clear_sync_group(&RMT); // we do not use sync groups, just in case.
    #endif
    esp_clk_tree_enable_src((soc_module_clk_t)RMT_CLK_SRC_DEFAULT, true); // required on C5, works without this on all other ESPs
  }

  // The group clock source is shared by all channels. All buses use
  // the same 80 MHz source and 40 MHz channel resolution.
  rmt_ll_set_group_clock_src(&RMT, _channel, RMT_CLK_SRC_DEFAULT, 1, 1, 0);
  rmt_ll_enable_group_clock(&RMT, true);
  rmt_ll_tx_set_channel_clock_div(&RMT, _channel, 2);
  rmt_ll_tx_reset_channels_clock_div(&RMT, 1 << _channel);

  #if SOC_RMT_TX_CANDIDATES_PER_GROUP < SOC_RMT_CHANNELS_PER_GROUP
  // claim the memory blocks of the RX channels for TX use (layout is TX then RX, we only need to claim the RX ones)
  for (int i = SOC_RMT_TX_CANDIDATES_PER_GROUP; i < SOC_RMT_CHANNELS_PER_GROUP; i++) {
    rmt_ll_rx_set_mem_owner(&RMT, i, RMT_LL_MEM_OWNER_SW);
  }
  #endif
#else
  // legacy driver performs the initial clock/line setup (IDF V4), RmtHiDriver takes over from there
  rmt_config_t config = {};

  config.rmt_mode = RMT_MODE_TX;
  config.channel = (rmt_channel_t)_channel;
  config.gpio_num = (gpio_num_t)_pin;
  config.mem_block_num = blocksToUse;
  config.clk_div = 2;  // 40MHz

  config.tx_config.loop_en = false;
  config.tx_config.carrier_en = false;
  config.tx_config.idle_output_en = true;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;

  err = rmt_config(&config);
  if (err != ESP_OK) {
    return false;
  }

  // Register hack for memory blocks normally assigned to RX (S3 / C3): set owner to TX
  // TODO: need this for ESP32 as well?
#ifndef RMT_USE_SINGLE_MEM_BLOCK
  #if defined(CONFIG_IDF_TARGET_ESP32S3)
  for (int i = 4; i < 8; i++) {
    rmt_set_memory_owner((rmt_channel_t)i, RMT_MEM_OWNER_TX);
  }
  #elif defined(CONFIG_IDF_TARGET_ESP32C3)
  for (int i = 2; i < 4; i++) {
    rmt_set_memory_owner((rmt_channel_t)i, RMT_MEM_OWNER_TX);
  }
  #endif
#endif
#endif

  const uint32_t resetUs = (_timing.reset_us > 0) ? _timing.reset_us : 300;
  err = RmtHiDriver::Install(_channel, bit0.val, bit1.val, (resetUs * 1000 + 12) / 25, blocksToUse);
  if (err != ESP_OK) {
    return false;
  }
  _driverInstalled = true;

  // route the pin, hardware signal inversion via GPIO matrix if _inverted (must be set before begin())
  gpio_func_sel((gpio_num_t)_pin, PIN_FUNC_GPIO);
  esp_rom_gpio_connect_out_signal(_pin, RMT_SIG_OUT0_IDX + _channel, _inverted, false);

  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getBytesPerPixel())) { end(); return false; }
  return true;
}

void RmtBus::end() {
  if (_driverInstalled) {
    RmtHiDriver::Uninstall(_channel);
    _driverInstalled = false;
  }
  if (_initialized && _pin >= 0) gpio_reset_pin((gpio_num_t)_pin);
  releaseEncodeBuffer();
  _initialized = false;
  _channel = -1;
}

bool RmtBus::show() {
  // Encoding is done per-pixel in setPixelColor(); _encodeBuffer is ready to ship.
  // the high priority driver waits for tx done internally and sends the reset/latch gap in-band
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;
  return RmtHiDriver::Write(_channel, _encodeBuffer, _encodeBufferSize) == ESP_OK;
}

bool RmtBus::canShow() const {
  if (!_initialized) return true; // do not block if init failed! busmanager waits until all buses can show
  return (ESP_OK == RmtHiDriver::WaitForTxDone(_channel, 0)); // 0 timeout means "poll and return immediately"
}

#else // !WLEDPB_USE_RMTHI

// Stub for targets without RmtHiDriver support (P4 uses a newer RMT IP, C61 has no RMT):
// RMT bus is unavailable, begin() fails so a different output driver can be selected.
bool RmtBus::begin() { return false; }
void RmtBus::end() {}
bool RmtBus::show() { return false; }
bool RmtBus::canShow() const { return true; }

#endif // WLEDPB_USE_RMTHI

} // namespace WLEDpixelBus
#endif // ARDUINO_ARCH_ESP32
