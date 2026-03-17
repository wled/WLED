#include "WLEDpixelBus.h"
#ifdef ARDUINO_ARCH_ESP32
#include "WLEDpixelBus_RMT.h"

namespace WLEDpixelBus {

//==============================================================================
// RMT Bus Implementation
//==============================================================================

// Per-channel context table - stored in DRAM for ISR access
DRAM_ATTR RmtBus::RmtContext RmtBus::s_contexts[8] = {};

// Explicit IRAM tranlator callback wrappers for each channel (ensures the function is placed in IRAM which is dropped when using templates)
// TODO: only define the ones actually needed based on available RMT channels, is there an IDF define for this?
void IRAM_ATTR RmtBus::translator_ch0(const void* s, rmt_item32_t* d, size_t ss, size_t w, size_t* ts, size_t* in) { translateInternal(0, s, d, ss, w, ts, in); }
void IRAM_ATTR RmtBus::translator_ch1(const void* s, rmt_item32_t* d, size_t ss, size_t w, size_t* ts, size_t* in) { translateInternal(1, s, d, ss, w, ts, in); }
#if SOC_RMT_TX_CANDIDATES_PER_GROUP > 2
void IRAM_ATTR RmtBus::translator_ch2(const void* s, rmt_item32_t* d, size_t ss, size_t w, size_t* ts, size_t* in) { translateInternal(2, s, d, ss, w, ts, in); }
void IRAM_ATTR RmtBus::translator_ch3(const void* s, rmt_item32_t* d, size_t ss, size_t w, size_t* ts, size_t* in) { translateInternal(3, s, d, ss, w, ts, in); }
#endif
#if SOC_RMT_TX_CANDIDATES_PER_GROUP > 4
void IRAM_ATTR RmtBus::translator_ch4(const void* s, rmt_item32_t* d, size_t ss, size_t w, size_t* ts, size_t* in) { translateInternal(4, s, d, ss, w, ts, in); }
void IRAM_ATTR RmtBus::translator_ch5(const void* s, rmt_item32_t* d, size_t ss, size_t w, size_t* ts, size_t* in) { translateInternal(5, s, d, ss, w, ts, in); }
void IRAM_ATTR RmtBus::translator_ch6(const void* s, rmt_item32_t* d, size_t ss, size_t w, size_t* ts, size_t* in) { translateInternal(6, s, d, ss, w, ts, in); }
void IRAM_ATTR RmtBus::translator_ch7(const void* s, rmt_item32_t* d, size_t ss, size_t w, size_t* ts, size_t* in) { translateInternal(7, s, d, ss, w, ts, in); }
#endif
// Jump table stored in DRAM so ISR code can quickly find the correct wrapper
DRAM_ATTR const sample_to_rmt_t RmtBus::s_callbacks[8] = {
  RmtBus::translator_ch0, RmtBus::translator_ch1
#if SOC_RMT_TX_CANDIDATES_PER_GROUP > 2
  ,RmtBus::translator_ch2, RmtBus::translator_ch3
#endif
#if SOC_RMT_TX_CANDIDATES_PER_GROUP > 4
  ,RmtBus::translator_ch4, RmtBus::translator_ch5,
  RmtBus::translator_ch6, RmtBus::translator_ch7
#endif
};

// Static auto-channel counter for RmtBus
uint8_t RmtBus::s_expectedChannels = 1;
uint8_t RmtBus::s_allocatedCount = 0;
uint8_t RmtBus::s_currentChannelIndex = 0;
uint8_t RmtBus::s_usedBlocks = 0;
uint8_t RmtBus::s_activeChannelMask = 0;

RmtBus::RmtBus(int8_t pin, const LedTiming& timing, ColorOrder order, int8_t channel)
  : _pin(pin)
  , _channel(channel)
  , _timing(timing)
  , _order(order)
  , _inverted(false)
  , _initialized(false)
  , _rmtChannel(RMT_CHANNEL_0)
  , _rmtBit0(0)
  , _rmtBit1(0)
  , _rmtResetTicks(0)
  , _encodeBuffer(nullptr)
  , _encodeBufferSize(0)
{
}

RmtBus::~RmtBus() {
  end();
}

void RmtBus::updateRmtTiming() {
  // RMT clock:  80MHz with div=2 -> 40MHz -> 25ns per tick
  const uint8_t clockDiv = 2;
  const float tickNs = 25.0f;

  auto nsToTicks = [tickNs](uint16_t ns) -> uint16_t {
    uint16_t ticks = (uint16_t)((ns + tickNs / 2) / tickNs);
    return ticks > 0 ? ticks : 1;
  };

  uint16_t t0h = nsToTicks(_timing.t0h_ns);
  uint16_t t0l = nsToTicks(_timing.t0l_ns);
  uint16_t t1h = nsToTicks(_timing.t1h_ns);
  uint16_t t1l = nsToTicks(_timing.t1l_ns);

  Serial.printf("[WPB] RMT timing (ns): t0h=%u t0l=%u t1h=%u t1l=%u reset_us=%u\n", _timing.t0h_ns, _timing.t0l_ns, _timing.t1h_ns, _timing.t1l_ns, _timing.reset_us);

  rmt_item32_t bit0, bit1;

  if (_inverted) {
    bit0.level0 = 0; bit0.duration0 = t0h;
    bit0.level1 = 1; bit0.duration1 = t0l;
    bit1.level0 = 0; bit1.duration0 = t1h;
    bit1.level1 = 1; bit1.duration1 = t1l;
  } else {
    bit0.level0 = 1; bit0.duration0 = t0h;
    bit0.level1 = 0; bit0.duration1 = t0l;
    bit1.level0 = 1; bit1.duration0 = t1h;
    bit1.level1 = 0; bit1.duration1 = t1l;
  }

  _rmtBit0 = bit0.val;
  _rmtBit1 = bit1.val;
  _rmtResetTicks = nsToTicks(_timing.reset_us * 1000);

  // If already initialized, update the shared DRAM context for this channel
  if (_initialized) {
    s_contexts[(int)_rmtChannel].bit0 = _rmtBit0;
    s_contexts[(int)_rmtChannel].bit1 = _rmtBit1;
    s_contexts[(int)_rmtChannel].resetDuration = _rmtResetTicks;
  }
}

bool RmtBus::begin() {
  if (_initialized) return true;

  uint8_t blocksToUse = 1;
  uint8_t maxTxChannels = getRmtMaxChannels();

  // Auto-channel select with optimized memory block allocation (assumes no RMT RX usage)
  // note on channel allocation: channels are assigned such as to maximize the number of memory blocks
  // available to the channel to minimize the number of interrupts needed for buffer re-fills (less context switching overhead)
  // C3 and S3 have less channels than blocks, so the last channel can always use additional blocks
  // example: ESP32, 2 channels requested total -> use CH0 with 4 blocks and CH4 with 4 blocks
  // example: ESP32-S3 with 3 channels: use CH0 with 2 block, CH2 with 1 blocks, and CH3 with 5 blocks
  if (_channel < 0) {
    if (s_allocatedCount >= s_expectedChannels || s_allocatedCount >= maxTxChannels) {
      return false;
    }

    uint8_t totalBlocks;
#if defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S3)
    totalBlocks = 8; // ESP32 and S3 have 8 blocks of RMT memory
#elif defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32C3) // note: C6 RMT hardware is the same as C3
    totalBlocks = 4; // other supported ESP32 variants have 4 blocks
#else
    totalBlocks = 4; // default to 4 if unknown, should be safe
#endif



    int left_channels = s_expectedChannels - s_allocatedCount - 1;

    if (left_channels == 0) {
      _channel = s_currentChannelIndex;
      blocksToUse = totalBlocks - s_usedBlocks;
    } else {
      int k = totalBlocks / s_expectedChannels;
      int max_k_for_index = maxTxChannels - s_currentChannelIndex - left_channels;
      if (k > max_k_for_index) k = max_k_for_index;
      if (k < 1) k = 1;

      _channel = s_currentChannelIndex;
      blocksToUse = k;
    }

    s_currentChannelIndex += blocksToUse;
    s_usedBlocks += blocksToUse;
    s_allocatedCount++;
  }

  if (_channel >= (int8_t)maxTxChannels) {
    Serial.printf("[WPB] RMT channel %d >= max %u, FAIL\n", _channel, maxTxChannels);
    return false;
  }
  _rmtChannel = (rmt_channel_t)_channel;

  Serial.printf("[WPB] RMT channel %d using %u blocks (total allocated: %u/%u)\n", _channel, blocksToUse, s_allocatedCount, maxTxChannels);

  updateRmtTiming();

  // Store initial timing into the shared DRAM context for this channel
  s_contexts[(int)_rmtChannel].bit0 = _rmtBit0;
  s_contexts[(int)_rmtChannel].bit1 = _rmtBit1;
  s_contexts[(int)_rmtChannel].resetDuration = _rmtResetTicks;

  rmt_config_t config = {};

  config.rmt_mode = RMT_MODE_TX;
  config.channel = _rmtChannel;
  config.gpio_num = (gpio_num_t)_pin;
  config.mem_block_num = blocksToUse;
  config.clk_div = 2;  // 40MHz

  config.tx_config.loop_en = false;
  config.tx_config.carrier_en = false;
  config.tx_config.idle_output_en = true;
  config.tx_config.idle_level = _inverted ? RMT_IDLE_LEVEL_HIGH : RMT_IDLE_LEVEL_LOW;


  esp_err_t err = rmt_config(&config);
  if (err != ESP_OK) {
    return false;
  }

  // install driver at high priority  TODO: need to raise priority just like in RMT high priority driver
  err = rmt_driver_install(_rmtChannel, 0, (ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3));
  if (err != ESP_OK) {
    return false;
  }

  // Register hack for memory blocks normally assigned to RX  TODO: is this also needed for S2 / classic ESP32?
#if defined(WLEDPB_ESP32S3) || defined(WLEDPB_ESP32C3)
  #if defined(WLEDPB_ESP32S3)
  for (int i = 4; i < 8; i++) {
    rmt_set_memory_owner((rmt_channel_t)i, RMT_MEM_OWNER_TX);
  }
  #elif defined(WLEDPB_ESP32C3)
  for (int i = 2; i < 4; i++) {
    rmt_set_memory_owner((rmt_channel_t)i, RMT_MEM_OWNER_TX);
  }
  #endif
#endif

  err = rmt_translator_init(_rmtChannel, s_callbacks[(int)_rmtChannel]);
  if (err != ESP_OK) {
    rmt_driver_uninstall(_rmtChannel);
    return false;
  }

  _initialized = true;
  s_activeChannelMask |= (1 << _channel);
  return true;
}

void RmtBus::end() {
  if (!_initialized) return;

  s_activeChannelMask &= ~(1 << _channel);
  rmt_driver_uninstall(_rmtChannel);

  if (_encodeBuffer) {
    free(_encodeBuffer);
    _encodeBuffer = nullptr;
    _encodeBufferSize = 0;
  }

  _initialized = false;
}

bool RmtBus::allocateBuffer(uint16_t numPixels) {
  size_t needed = numPixels * getChannelCount(_order);
  if (_encodeBuffer && _encodeBufferSize >= needed) {
    return true;
  }

  if (_encodeBuffer) {
    free(_encodeBuffer);
  }

  _encodeBuffer = (uint8_t*)malloc(needed);
  if (!_encodeBuffer) {
    _encodeBufferSize = 0;
    return false;
  }
  _encodeBufferSize = needed;
  return true;
}

bool RmtBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
  if (!pixels) {
    pixels = _pixelData;
    numPixels = _numPixels;
    cct = _cctData;
  }
  if (numPixels == 0) numPixels = _numPixels;
  if (!cct) cct = _cctData;

  if (!_initialized || !pixels || numPixels == 0) {
    return false;
  }

  // Wait for previous transmission on THIS channel to complete
  rmt_wait_tx_done((rmt_channel_t)_rmtChannel, portMAX_DELAY);

  if (!allocateBuffer(numPixels)) return false;

  // Encode pixels to byte stream
  ColorEncoder encoder(_order);
  uint8_t* dst = _encodeBuffer;
  uint8_t numCh = encoder.getNumChannels();

  for (uint16_t i = 0; i < numPixels; i++) {
    encoder.encode(pixels[i], cct ?  &cct[i] : nullptr, dst);
    dst += numCh;
  }

  // Update per-channel context for translator (ensure latest timings are visible to ISR)
  s_contexts[(int)_rmtChannel].bit0 = _rmtBit0;
  s_contexts[(int)_rmtChannel].bit1 = _rmtBit1;
  s_contexts[(int)_rmtChannel].resetDuration = _rmtResetTicks;

  // Start transmission
  size_t dataLen = numPixels * numCh;
  esp_err_t err = rmt_write_sample(_rmtChannel, _encodeBuffer, dataLen, false);

  return err == ESP_OK;
}

bool RmtBus::canShow() const {
  if (!_initialized) return true;
  // Use rmt_wait_tx_done with 0 timeout to check if TX is done (matching NeoPixelBus)
  return (ESP_OK == rmt_wait_tx_done(_rmtChannel, 0));
}

void RmtBus::waitComplete() {
  if (_initialized) {
    rmt_wait_tx_done(_rmtChannel, portMAX_DELAY);
  }
}

void RmtBus::setTiming(const LedTiming& timing) {
  Serial.printf("[WPB] RMT setTiming called: t0h=%u t0l=%u t1h=%u t1l=%u reset_us=%u\n", timing.t0h_ns, timing.t0l_ns, timing.t1h_ns, timing.t1l_ns, timing.reset_us);
  _timing = timing;
  if (_initialized) {
    updateRmtTiming();
  }
}

void RmtBus::setColorOrder(ColorOrder order) {
  _order = order;
}

void IRAM_ATTR RmtBus::translateInternal(uint8_t channel, const void* src, rmt_item32_t* dest,
                                         size_t src_size, size_t wanted_num,
                                         size_t* translated_size, size_t* item_num) {
  if (src == nullptr || dest == nullptr) {
    *translated_size = 0;
    *item_num = 0;
    return;
  }

  const uint8_t* psrc = (const uint8_t*)src;
  rmt_item32_t* pdest = dest;
  
  // Cache instance timings in registers for maximum ISR speed
  const uint32_t bit0 = s_contexts[channel].bit0; 
  const uint32_t bit1 = s_contexts[channel].bit1;
  const uint16_t resetDuration = s_contexts[channel].resetDuration;

  // Calculate how many full bytes we can translate based on RMT buffer space (wanted_num)
  size_t bytes_to_process = src_size;
  size_t items_limit = wanted_num / 8;
  if (bytes_to_process > items_limit) bytes_to_process = items_limit;

  // Process all but the last byte in a tight loop without reset checks -> no branching in the loop for maximum speed
  size_t i = 0;
  size_t fast_limit = (bytes_to_process > 0) ? (bytes_to_process - 1) : 0;

  for (; i < fast_limit; i++) {
    const uint8_t data = psrc[i];

    // using loop unrolling makes this faster avoiding bit shifts, loop overhead and lets the compiler optimize more
    pdest[0].val = (data & 0x80) ? bit1 : bit0;
    pdest[1].val = (data & 0x40) ? bit1 : bit0;
    pdest[2].val = (data & 0x20) ? bit1 : bit0;
    pdest[3].val = (data & 0x10) ? bit1 : bit0;
    pdest[4].val = (data & 0x08) ? bit1 : bit0;
    pdest[5].val = (data & 0x04) ? bit1 : bit0;
    pdest[6].val = (data & 0x02) ? bit1 : bit0;
    pdest[7].val = (data & 0x01) ? bit1 : bit0;

    pdest += 8;
  }

  // process the final byte separately to handle the reset duration (otherwise a check for "last byte" is needed in the loop above)
  // i.e. prioritize speed over code simplicity as this ISR runs per RMT channel and at very high frequency (hundreds of times per second per channel)
  if (i < src_size && (i * 8) < wanted_num) {
    const uint8_t data = psrc[i];
    pdest[0].val = (data & 0x80) ? bit1 : bit0;
    pdest[1].val = (data & 0x40) ? bit1 : bit0;
    pdest[2].val = (data & 0x20) ? bit1 : bit0;
    pdest[3].val = (data & 0x10) ? bit1 : bit0;
    pdest[4].val = (data & 0x08) ? bit1 : bit0;
    pdest[5].val = (data & 0x04) ? bit1 : bit0;
    pdest[6].val = (data & 0x02) ? bit1 : bit0;
    pdest[7].val = (data & 0x01) ? bit1 : bit0;

    i++;
    // If this byte is the end of the source data, inject the Reset Signal
    if (i == src_size) {
      pdest[7].duration1 = resetDuration;
    }
  }

  *translated_size = i;
  *item_num = i * 8;
}

} // namespace WLEDpixelBus
#endif
