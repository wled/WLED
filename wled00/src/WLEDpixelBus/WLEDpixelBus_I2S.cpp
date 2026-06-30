/*-------------------------------------------------------------------------

WLEDpixelBus - parallel I2S output driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

supports ESP32 and ESP32 S2
Default is 8 parallel outputs and double DMA buffering but it also supports 16 parallel outputs if needed
For 16 parallel output, triple buffering is required for glitch-free output.
Data is output in 4-step cadence meaning each LED bit is encoded into 4 I2S bits. '0' is 0b1000 and '1' is 0b1110
Encoding is highly optimized for speed as encoding is done "on the fly" while the other buffer is being sent out using DMA.
The RAM usage of the sendout buffer is number of LEDs * bytes per LED + DMA buffer size
3k per DMA buffer works well, enough for 32 RGB LEDs in 8x parallel output or roughly 0.9ms between buffer swaps
Each bus can have individual configuration of color channels but all must share the same timing

-------------------------------------------------------------------------*/


#include "WLEDpixelBus.h"
#ifdef WLEDPB_I2S_SUPPORT
#include "WLEDpixelBus_I2S.h"

namespace WLEDpixelBus {

//==============================================================================
// I2S Bus Implementation
//==============================================================================

I2sBusContext* I2sBusContext::_instances[WLEDPB_I2S_BUS_COUNT] = {nullptr};
uint8_t I2sBusContext::_refCount[WLEDPB_I2S_BUS_COUNT] = {0};

I2sBusContext* I2sBusContext::get(uint8_t busNum) {
  if (busNum >= WLEDPB_I2S_BUS_COUNT) return nullptr;

  if (_instances[busNum] == nullptr) {
    _instances[busNum] = new I2sBusContext(busNum);
  }
  if (_instances[busNum] != nullptr)
    _refCount[busNum]++;
  return _instances[busNum];
}

void I2sBusContext::release(uint8_t busNum) {
  if (busNum >= WLEDPB_I2S_BUS_COUNT) return;
  if (_refCount[busNum] == 0) return;

  _refCount[busNum]--;
  if (_refCount[busNum] == 0 && _instances[busNum]) {
    delete _instances[busNum];
    _instances[busNum] = nullptr;
  }
}

I2sBusContext::I2sBusContext(uint8_t busNum)
  : _busNum(busNum)
  , _state(DriverState::Idle)
  , _initialized(false)
  , _maxSrcBytes(0)
  , _bufferSize(0)
  , _dmaAllocated(false)
  , _activeBuffer(0)
  , _remainingDataBuffers(0)
  , _resetBytesLeft(0)
  , _timing{0, 0, 0, 0, 0}
  , _clockDiv(1)
  , _isrHandle(nullptr)
  , _channelCount(0)
  , _channelMask(0)
  , _stagedMask(0)
  , _maxDataLen(0)
{
#if defined(CONFIG_IDF_TARGET_ESP32)
  _i2sDev = (busNum == 0) ? &I2S0 : &I2S1;
#else
  _i2sDev = &I2S0;
#endif

  for (int i = 0; i < WLEDPB_I2S_MAX_CHANNELS; i++) {
    _channels[i] = {nullptr, -1, nullptr, 0, 0, false};
  }

  for (int i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    _dmaDesc[i] = nullptr;
    _dmaBuffer[i] = nullptr;
  }
}

I2sBusContext:: ~I2sBusContext() {
  deinit();
}

bool I2sBusContext::init(const LedTiming& timing) {
  if (_initialized) return true;

  //pinMode(33, OUTPUT); // debug pin for timing analysis

  _timing = timing;
  // DMA buffer allocation is deferred to _allocDmaBuffers(), called from startTransmit() once
  // all channels have registered and _maxSrcBytes reflects the largest bus.

  // Enable I2S peripheral
#if defined(CONFIG_IDF_TARGET_ESP32)
  periph_module_enable(_busNum == 0 ? PERIPH_I2S0_MODULE : PERIPH_I2S1_MODULE);
#else
  periph_module_enable(PERIPH_I2S0_MODULE);
#endif

  // Stop any existing transmission
  _i2sDev->out_link.stop = 1;
  _i2sDev->conf.tx_start = 0;
  _i2sDev->int_ena.val = 0;
  _i2sDev->int_clr.val = 0xFFFFFFFF;
  _i2sDev->fifo_conf.dscr_en = 0;

  // Reset I2S
  _i2sDev->conf.tx_reset = 1;
  _i2sDev->conf.tx_reset = 0;
  _i2sDev->conf.rx_reset = 1;
  _i2sDev->conf.rx_reset = 0;

  // Reset DMA
  _i2sDev->lc_conf.in_rst = 1;
  _i2sDev->lc_conf.in_rst = 0;
  _i2sDev->lc_conf.out_rst = 1;
  _i2sDev->lc_conf.out_rst = 0;
  _i2sDev->lc_conf.ahbm_rst = 1;
  _i2sDev->lc_conf.ahbm_rst = 0;
  _i2sDev->lc_conf.ahbm_fifo_rst = 1;
  _i2sDev->lc_conf.ahbm_fifo_rst = 0;

  // Reset FIFO
  _i2sDev->conf.tx_fifo_reset = 1;
  _i2sDev->conf.tx_fifo_reset = 0;
  _i2sDev->conf.rx_fifo_reset = 1;
  _i2sDev->conf.rx_fifo_reset = 0;

  // Configure for parallel LCD mode
  _i2sDev->conf2.val = 0;
  _i2sDev->conf2.lcd_en = 1;
#ifdef WLED_PIXELBUS_16PARALLEL
  _i2sDev->conf2.lcd_tx_wrx2_en = 0;  // 16-bit mode: disable 8-bit double-write swap path
#else
  _i2sDev->conf2.lcd_tx_wrx2_en = 1;  // 8-bit mode: required for 8-bit parallel output
#endif
  _i2sDev->conf2.lcd_tx_sdx2_en = 0;

  // DMA config
  _i2sDev->lc_conf.val = 0;
  _i2sDev->lc_conf.out_eof_mode = 1;

  // Disable PDM
#if defined(CONFIG_IDF_TARGET_ESP32)
  _i2sDev->pdm_conf.tx_pdm_en = 0;
  _i2sDev->pdm_conf.pcm2pdm_conv_en = 0;
#endif

  // FIFO configuration
  _i2sDev->fifo_conf.val = 0;
  _i2sDev->fifo_conf.tx_fifo_mod_force_en = 1;
  //_i2sDev->fifo_conf.tx_fifo_mod = 3;  // 0=16bit dual, 1=16bit single, 2=32bit dual, 3=32bit single (32-bit linked for 16-bit samples)
  // For ESP32 Classic, use 16-bit FIFO mode
#if !defined(CONFIG_IDF_TARGET_ESP32S2)
  _i2sDev->fifo_conf.tx_fifo_mod = 1;
  //_i2sDev->conf_chan.tx_chan_mod = 0; // Standard mode
#else
  _i2sDev->fifo_conf.tx_fifo_mod = 3;
  //_i2sDev->conf_chan.tx_chan_mod = 1;
#endif
  _i2sDev->fifo_conf.tx_data_num = 32;  // FIFO threshold

  // PCM bypass
  _i2sDev->conf1.val = 0;
  _i2sDev->conf1.tx_stop_en = 0;
  _i2sDev->conf1.tx_pcm_bypass = 1;

  // Channel config
  _i2sDev->conf_chan.val = 0;
  _i2sDev->conf_chan.tx_chan_mod = 1;  // 0=stereo, 1=right-left, 2=left-right, 3=right only, 4=left only

  // I2S conf
  _i2sDev->conf.val = 0;
  _i2sDev->conf.tx_msb_shift = 0;  // No shift in parallel mode
  _i2sDev->conf.tx_right_first = 1;
  #if defined(CONFIG_IDF_TARGET_ESP32S2)
  _i2sDev->conf.tx_dma_equal = 1; // seems required for S2
  #endif

  // Clear timing register
  _i2sDev->timing.val = 0;

  // Calculate clock divider for 4-step cadence
  // bck_div_num must be >= 2 on ESP32 hardware
  // step_time = clkm_div * bck_div / base_clock_MHz * 1000 ns
  // clkm_div = step_time_ns * base_clock_MHz / (bck_div * 1000)
  const uint8_t bckDiv = 4;  // must be >= 2
  uint32_t bitPeriodNs = timing.bitPeriod();

#if defined(CONFIG_IDF_TARGET_ESP32)
  #ifndef WLED_PIXELBUS_16PARALLEL
  // 8-bit mode: lcd_tx_wrx2_en=1 halves the effective output rate (WR pulses at BCK/2).
  // Use 2x clock constant so the divider is doubled, yielding the correct BCK after the factor-of-2.
  const double baseClockMhz = 160.0;
  #else
  const double baseClockMhz = 80.0; // 16-bit mode: APB clock, lcd_tx_wrx2_en=0 has no rate halving
  #endif
#else
  const double baseClockMhz = 80.0; // S2: 80MHz I2S base clock (wrx2 on S2 does not halve the rate)
#endif

  // For parallel 8-bit, bytesPerSample=1, dmaBitPerDataBit=4
  double clkmdiv = (double)bitPeriodNs / 1.0 / 4.0 / (double)bckDiv / 1000.0 * baseClockMhz;
  if (clkmdiv < 2.0) clkmdiv = 2.0;
  if (clkmdiv > 255.0) clkmdiv = 255.0;

  uint8_t clkmInteger = (uint8_t)clkmdiv;
  double clkmFraction = clkmdiv - clkmInteger;

  // Convert fraction to divB/divA (fraction = divB/divA)
  uint8_t divB = 0;
  uint8_t divA = 0;
  if (clkmFraction > 0.001) {
    divA = 63;  // use max denominator for best precision
    divB = (uint8_t)(clkmFraction * 63.0 + 0.5);
    if (divB >= divA) divB = divA - 1;
  }

  _clockDiv = clkmInteger;

  //Serial.printf("[I2S] Clock: bitPeriod=%uns, clkm_div=%u+%u/%u, bck_div=%u\n", bitPeriodNs, clkmInteger, divB, divA, bckDiv);
  double actualStepNs = (double)clkmInteger * bckDiv / baseClockMhz * 1000.0;
  if (divA > 0) actualStepNs = ((double)clkmInteger + (double)divB / divA) * bckDiv / baseClockMhz * 1000.0;
  //Serial.printf("[I2S] Step time: %.1fns (target: %.1fns), bit period: %.1fns\n", actualStepNs, (double)bitPeriodNs / 4.0, actualStepNs * 4.0);

  // Set clock (with fractional divider for accurate timing)
  _i2sDev->clkm_conf.val = 0;

  #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
  _i2sDev->clkm_conf.clk_sel = 2; // APPL = 1 APB = 2
  _i2sDev->clkm_conf.clk_en = 1;  // examples of i2s show this being set if sel is set to 2
  #else
  _i2sDev->clkm_conf.clk_en = 1;
  _i2sDev->clkm_conf.clka_en = 0;
  #endif

  _i2sDev->clkm_conf.clkm_div_a = divA;
  _i2sDev->clkm_conf.clkm_div_b = divB;
  _i2sDev->clkm_conf.clkm_div_num = clkmInteger;

  // Sample rate - bck must be >= 2
  _i2sDev->sample_rate_conf.val = 0;
  _i2sDev->sample_rate_conf.tx_bck_div_num = bckDiv;
#ifdef WLED_PIXELBUS_16PARALLEL
  _i2sDev->sample_rate_conf.tx_bits_mod = 16;  // 16-bit samples for up to 16 parallel channels
#else
  _i2sDev->sample_rate_conf.tx_bits_mod = 8;   // 8-bit samples for up to 8 parallel channels
#endif

  // Final reset before ISR install
  _i2sDev->lc_conf.in_rst = 1;
  _i2sDev->lc_conf.out_rst = 1;
  _i2sDev->lc_conf.ahbm_rst = 1;
  _i2sDev->lc_conf.ahbm_fifo_rst = 1;
  _i2sDev->lc_conf.in_rst = 0;
  _i2sDev->lc_conf.out_rst = 0;
  _i2sDev->lc_conf.ahbm_rst = 0;
  _i2sDev->lc_conf.ahbm_fifo_rst = 0;
  _i2sDev->conf.tx_reset = 1;
  _i2sDev->conf.tx_fifo_reset = 1;
  _i2sDev->conf.tx_reset = 0;
  _i2sDev->conf.tx_fifo_reset = 0;

  // Install ISR
  int intSource;
  #if defined(CONFIG_IDF_TARGET_ESP32)
  intSource = (_busNum == 0) ? ETS_I2S0_INTR_SOURCE : ETS_I2S1_INTR_SOURCE;
  #else
  intSource = ETS_I2S0_INTR_SOURCE;
  #endif

  esp_err_t err = esp_intr_alloc(intSource, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3, dmaISR, this, &_isrHandle); // note: level 3 is the maximum supported without resorting to assembly
  if (err != ESP_OK) {
    //DEBUG_PRINTF("I2S ISR alloc failed: %d", err);
    deinit();
    return false;
  }

  _initialized = true;
  //DEBUG_PRINTF("[I2S] Init complete: bus=%u, bufSize=%u\n", _busNum, _bufferSize);
  return true;
}

void I2sBusContext::deinit() {
  // wait for finish sending before deinit (just in case)
  int timeout = 100;
  while (!isIdle() && timeout--) { vTaskDelay(1); }

  if (_i2sDev) {
    _i2sDev->int_ena.val = 0;      // Disable interrupts first
    _i2sDev->conf.tx_start = 0;
    _i2sDev->out_link.start = 0;
  }

  if (_isrHandle) {
    esp_intr_free(_isrHandle);
    _isrHandle = nullptr;
  }

  for (int i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    if (_dmaBuffer[i]) {
      heap_caps_free(_dmaBuffer[i]);
      _dmaBuffer[i] = nullptr;
    }
    if (_dmaDesc[i]) {
      heap_caps_free(_dmaDesc[i]);
      _dmaDesc[i] = nullptr;
    }
  }

#if defined(CONFIG_IDF_TARGET_ESP32)
  periph_module_disable(_busNum == 0 ? PERIPH_I2S0_MODULE :  PERIPH_I2S1_MODULE);
#else
  periph_module_disable(PERIPH_I2S0_MODULE);
#endif

  _dmaAllocated = false;
  _initialized = false;
}

// Compute the DMA buffer size from _maxSrcBytes (set during registerChannel() by the largest bus),
// then allocate the circular DMA descriptor/buffer chain.
// Called once from startTransmit() after all channels have registered.
bool I2sBusContext::_allocDmaBuffers() {
  // DMA buffer size: spread total pixel data evenly across the circular buffer slots,
  // then clamp to [MIN_DMA_BUFFER_SIZE, DEFAULT_DMA_BUFFER_SIZE]
  _bufferSize = (WLEDPB_I2S_DMABYTES * _maxSrcBytes) / WLEDPB_I2S_DMA_BUFFER_COUNT;
  _bufferSize = (_bufferSize + 3) & ~3;                               // align to 4 bytes
  if (_bufferSize > DEFAULT_DMA_BUFFER_SIZE) _bufferSize = DEFAULT_DMA_BUFFER_SIZE;
  if (_bufferSize < MIN_DMA_BUFFER_SIZE)     _bufferSize = MIN_DMA_BUFFER_SIZE;

  // allocate DMA-capable buffers (4-byte aligned for hardware DMA engine)
  for (int i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    _dmaBuffer[i] = (uint8_t*)heap_caps_aligned_alloc(4, _bufferSize, MALLOC_CAP_DMA);
    if (!_dmaBuffer[i]) return false;
    memset(_dmaBuffer[i], 0, _bufferSize);

    _dmaDesc[i] = (lldesc_t*)heap_caps_aligned_alloc(4, sizeof(lldesc_t), MALLOC_CAP_DMA);
    if (!_dmaDesc[i]) return false;
  }

  // set up DMA descriptors as a circular linked list
  for (int i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    _dmaDesc[i]->size   = _bufferSize;
    _dmaDesc[i]->length = _bufferSize;
    _dmaDesc[i]->buf    = _dmaBuffer[i];
    _dmaDesc[i]->eof    = 1;  // generate EOF interrupt on completion of each buffer
    _dmaDesc[i]->sosf   = 0;
    _dmaDesc[i]->owner  = 1;  // hand ownership to DMA engine
    _dmaDesc[i]->qe.stqe_next = _dmaDesc[(i + 1) % WLEDPB_I2S_DMA_BUFFER_COUNT];
  }

  _dmaAllocated = true;
  //DEBUG_PRINTF_P(PSTR("[I2S] DMA buffers allocated: bufSize=%u x%u\n"), _bufferSize, WLEDPB_I2S_DMA_BUFFER_COUNT);
  return true;
}

int8_t I2sBusContext::registerChannel(int8_t pin, I2sBus* bus, size_t srcBytes, bool inverted) {
  // Find free slot
  int8_t idx = -1;
  for (int i = 0; i < WLEDPB_I2S_MAX_CHANNELS; i++) {
    if (! _channels[i].active) {
      idx = i;
      break;
    }
  }

  if (idx < 0) return -1;

  _channels[idx].bus = bus;
  _channels[idx].pin = pin;
  _channels[idx].active = true;
  _channelCount++;
  _channelMask |= (1 << idx);

  // track the largest source byte count across channels; used in _allocDmaBuffers() to size DMA buffers
  if (srcBytes > _maxSrcBytes) _maxSrcBytes = srcBytes;

  // Configure GPIO
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);

  // Route I2S output to GPIO
  int sigIdx;
#ifdef WLED_PIXELBUS_16PARALLEL
  // 16-bit mode: mapping starts at DATA_OUT8_IDX for the wide 16-bit window
  #if defined(CONFIG_IDF_TARGET_ESP32)
  sigIdx = (_busNum == 0) ? I2S0O_DATA_OUT8_IDX : I2S1O_DATA_OUT8_IDX;
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
  sigIdx = I2S0O_DATA_OUT8_IDX;
  #else
  sigIdx = I2S0O_DATA_OUT0_IDX;
  #endif
#else
  // 8-bit mode: mapping starts at DATA_OUT0_IDX (S2: DATA_OUT16_IDX for upper byte)
  #if defined(CONFIG_IDF_TARGET_ESP32)
  sigIdx = (_busNum == 0) ? I2S0O_DATA_OUT0_IDX : I2S1O_DATA_OUT0_IDX;
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
  sigIdx = I2S0O_DATA_OUT16_IDX; // 8-bit parallel maps to upper bytes on S2
  #else
  sigIdx = I2S0O_DATA_OUT0_IDX;
  #endif
#endif
  sigIdx += idx;

  //gpio_matrix_out(pin, sigIdx, inverted, false);
  esp_rom_gpio_connect_out_signal(pin, sigIdx, inverted, false); // TODO: this is the new command in IDF V5, works in V4 too?

  return idx;
}

void I2sBusContext::unregisterChannel(int8_t channelIdx) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_I2S_MAX_CHANNELS) return;
  if (!_channels[channelIdx].active) return;

  if (_channels[channelIdx].pin >= 0) {
    gpio_reset_pin((gpio_num_t)_channels[channelIdx].pin);
  }

  _channels[channelIdx] = {nullptr, -1, nullptr, 0, 0, false};
  _channelCount--;
  _channelMask &= ~(1 << channelIdx);
}

void I2sBusContext::setChannelData(int8_t channelIdx, const uint8_t* data, size_t len) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_I2S_MAX_CHANNELS) return;

  _channels[channelIdx].srcData = data;
  _channels[channelIdx].srcLen = len;
  _channels[channelIdx].srcPos = 0;

  if (len > _maxDataLen) {
    _maxDataLen = len;
  }

  // Safety: If this channel was already staged, it means we somehow missed triggering startTransmit()
  if (_stagedMask & (1 << channelIdx)) {
    _stagedMask = 0; 
  }
  _stagedMask |= (1 << channelIdx);
}

// encode4Step: 4-step cadence, converts per-channel byte streams to parallel DMA words.

#ifdef WLED_PIXELBUS_16PARALLEL
// 16-bit parallel encode: branchless gather + scatter, 64 bytes per source byte (16 channels)
// Returns the number of bytes actually written into dest (may be < destLen when data runs out).
void IRAM_ATTR I2sBusContext::encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel) {
  for (size_t pos = 0; pos + 64 <= destLen; pos += 64) {
    // alwaysMask: channels with active data (HIGH step); bN: channels with bit N set
    uint16_t alwaysMask = 0;
    uint16_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;   // named regs: compiler should keep in regs
    uint16_t b4 = 0, b5 = 0, b6 = 0, b7 = 0;

    for (int ch = 0; ch < maxChannel; ch++) {
      if (!_channels[ch].active) continue;
      if (_channels[ch].srcPos >= _channels[ch].srcLen) continue;
      const uint16_t m = (uint16_t)(1u << ch);
      alwaysMask |= m;
      const uint8_t b = _channels[ch].srcData[_channels[ch].srcPos++];
      // extract bits, unrolled for speed
      b0 |= m & (uint16_t)(0u - ((b >> 7) & 1u));
      b1 |= m & (uint16_t)(0u - ((b >> 6) & 1u));
      b2 |= m & (uint16_t)(0u - ((b >> 5) & 1u));
      b3 |= m & (uint16_t)(0u - ((b >> 4) & 1u));
      b4 |= m & (uint16_t)(0u - ((b >> 3) & 1u));
      b5 |= m & (uint16_t)(0u - ((b >> 2) & 1u));
      b6 |= m & (uint16_t)(0u - ((b >> 1) & 1u));
      b7 |= m & (uint16_t)(0u - ((b >> 0) & 1u));
    }

    if (!alwaysMask) break;  // no active channels produced data

    // 16 x 32-bit stores, fully unrolled.
    uint32_t* p = (uint32_t*)(dest + pos);
#if defined(CONFIG_IDF_TARGET_ESP32S2)
    // S2 layout: [step0, step1, step2, step3] (no half-word swap)
    // step0=HIGH, step1=data, step2=data, step3=LOW (or 0b1000, 0b1110)
    // 32-bit pair: p[0]=(bN<<16)|alwaysMask,  p[1]=(0<<16)|bN
    #define EMIT(bN, OFF) \
      p[OFF]   = ((uint32_t)(bN) << 16) | alwaysMask; \
      p[OFF+1] = (bN);
#else
    // Classic ESP32 layout: [S1, S0, S3, S2] (half-words swapped)
    // Output order: S0=HIGH, S1=data, S2=data, S3=LOW
    // 32-bit pair: p[0]=(alwaysMask<<16)|bN,  p[1]=(bN<<16)|0
    const uint32_t AH = (uint32_t)alwaysMask << 16;
    #define EMIT(bN, OFF) \
      p[OFF]   = AH | (bN); \
      p[OFF+1] = (uint32_t)(bN) << 16;
#endif
    EMIT(b0, 0)  EMIT(b1, 2)  EMIT(b2, 4)  EMIT(b3, 6)
    EMIT(b4, 8)  EMIT(b5, 10) EMIT(b6, 12) EMIT(b7, 14)
    #undef EMIT
  }
}

#else
// 8-bit parallel encode: branchless gather + scatter, 32 bytes per source byte (8 channels)
void IRAM_ATTR I2sBusContext::encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel) {
  for (size_t pos = 0; pos + 32 <= destLen; pos += 32) {
    uint8_t alwaysMask = 0;
    uint8_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;
    uint8_t b4 = 0, b5 = 0, b6 = 0, b7 = 0;

    for (int ch = 0; ch < maxChannel; ch++) {
      if (!_channels[ch].active) continue;
      if (_channels[ch].srcPos >= _channels[ch].srcLen) continue;
      const uint8_t m = (uint8_t)(1u << ch);
      alwaysMask |= m;
      const uint8_t b = _channels[ch].srcData[_channels[ch].srcPos++];
      b0 |= m & (uint8_t)(0u - ((b >> 7) & 1u));
      b1 |= m & (uint8_t)(0u - ((b >> 6) & 1u));
      b2 |= m & (uint8_t)(0u - ((b >> 5) & 1u));
      b3 |= m & (uint8_t)(0u - ((b >> 4) & 1u));
      b4 |= m & (uint8_t)(0u - ((b >> 3) & 1u));
      b5 |= m & (uint8_t)(0u - ((b >> 2) & 1u));
      b6 |= m & (uint8_t)(0u - ((b >> 1) & 1u));
      b7 |= m & (uint8_t)(0u - ((b >> 0) & 1u));
    }
    if (!alwaysMask) break;

    // 8-bit LCD mode with lcd_tx_wrx2_en=1 swaps bytes within 16-bit half-words:
    //   Memory [b0,b1,b2,b3] outputs as [b2,b3,b0,b1] (half-word swap within 32-bit word)
    // Output cadence: [HIGH][data][data][LOW] -> steps [S0,S1,S2,S3]
    // Memory write order for ESP32 (half-word swapped): [S2,S3,S0,S1]
    //   => as 32-bit: p[n] = (S0<<16)|S2 | upper, but since S3=LOW=0 and S1=data:
    //   => p[0] = (S0<<24)|(S2<<16)|(S1<<8)|S3 in byte view
    //   As single uint32_t (LE): byte0=S2=data, byte1=S3=0, byte2=S0=HIGH, byte3=S1=data
    //   = bN | 0 | alwaysMask | bN => as uint32: bN | (alwaysMask<<16) | ((uint32_t)bN<<24)
    //
    // For S2 (no half-word swap): layout [S0,S1,S2,S3]
    //   = alwaysMask | (bN<<8) | (bN<<16) | 0
    //   As uint32: alwaysMask | ((uint32_t)bN<<8) | ((uint32_t)bN<<16)
    uint32_t* p = (uint32_t*)(dest + pos);
#if defined(CONFIG_IDF_TARGET_ESP32S2)
    // S2: no swap, layout [S0,S1,S2,S3] = [HIGH,data,data,LOW]
    #define EMIT8(bN, OFF) \
      p[OFF] = (uint32_t)(alwaysMask) | ((uint32_t)(bN) << 8) | ((uint32_t)(bN) << 16);
#else
    // Classic ESP32: half-word swap, memory [S2,S3,S0,S1] = [data,0,HIGH,data]
    const uint32_t AH8 = (uint32_t)alwaysMask << 16;
    #define EMIT8(bN, OFF) \
      p[OFF] = AH8 | (uint32_t)(bN) | ((uint32_t)(bN) << 24);
#endif
    EMIT8(b0, 0)  EMIT8(b1, 1)  EMIT8(b2, 2)  EMIT8(b3, 3)
    EMIT8(b4, 4)  EMIT8(b5, 5)  EMIT8(b6, 6)  EMIT8(b7, 7)
    #undef EMIT8
  }
}

#endif // WLED_PIXELBUS_16PARALLEL

void IRAM_ATTR I2sBusContext::fillBuffer(uint8_t bufIdx) {
  // clear the buffer
  memset(_dmaBuffer[bufIdx], 0, _bufferSize); // clear the buffer, will be filled or left blank as reset signal (cant be skipped, some channels may have less data)

  if (_resetBytesLeft > 0) {
    _dmaDesc[bufIdx]->length = _resetBytesLeft;
    _dmaDesc[bufIdx]->qe.stqe_next = nullptr; // stop the engine after this one  note: this assumes the reset period fits within one buffer, so don't set buffers too small i.e. at least 1k
    _resetBytesLeft = 3; // flag end of frame, don't queue any more buffers (is a multiple of 4 if set "naturally" below)
    return; // nothing to encode, this is a reset pulse, keep output low (all zeroes)
  }
  uint32_t bytesToEncode = 0;
  uint8_t maxCh = 0;
  for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
    if (_channels[ch].active) {
      maxCh = ch + 1;
      uint32_t channelBytesLeft = _channels[ch].srcLen - _channels[ch].srcPos;
      if (channelBytesLeft > bytesToEncode) bytesToEncode = channelBytesLeft;
    }
  }

  uint32_t translatedbytes = bytesToEncode * WLEDPB_I2S_DMABYTES;
  translatedbytes = translatedbytes > _bufferSize ? _bufferSize : translatedbytes;
  encode4Step(_dmaBuffer[bufIdx], translatedbytes, maxCh);

  if (translatedbytes < _bufferSize) {
    // Data ran out before the buffer was full (i.e. we are done), compute the minimum reset period we must send as zero cycles
    uint32_t resetNs = _timing.reset_us * 1000;
    uint32_t bitPeriodNs = _timing.bitPeriod() + 1; // +1 to ensure no division by zero and slightly over-estimate the reset cycle
    uint32_t zeroCycles = resetNs / bitPeriodNs;
    size_t resetBytes = zeroCycles * (WLEDPB_I2S_DMABYTES / 8); // one cycle is 4 clocks, on each clock two/one buffer byte(s) sent out in parallel

    size_t newLen = translatedbytes + resetBytes;
    if (newLen > _bufferSize) {
      _resetBytesLeft = newLen - _bufferSize; // reset pulse does not fit into this buffer frame, send another one (see above)
      _dmaDesc[bufIdx]->length = _bufferSize;
    }
    else {
      _dmaDesc[bufIdx]->length = newLen; // send the rest (zeroes) as a reset
      _resetBytesLeft = 3; // flag end of frame, don't queue any more buffers or it will mess up the DMA
      _dmaDesc[bufIdx]->qe.stqe_next = nullptr;  // reset fit into this buffer, end transfer after this is sent
    }
  }
}

bool I2sBusContext::startTransmit() {
  if (_state != DriverState::Idle) return false;
  if (_channelCount == 0) return false;

  // Only start transmission if ALL active channels have populated data
  if (_stagedMask != _channelMask) return true;
  _stagedMask = 0; // Reset for next frame

  _maxDataLen = 0;
  for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
    if (_channels[ch].active) {
      _channels[ch].srcPos = 0;
      if (_channels[ch].srcLen > _maxDataLen) {
        _maxDataLen = _channels[ch].srcLen;
      }
    }
  }

  _resetBytesLeft = 0;

  // allocate or reallocate DMA buffers if needed — deferred from init() so all channels
  // can register first and _maxSrcBytes reflects the bus with the most pixels
  if (!_dmaAllocated) {
    if (!_allocDmaBuffers()) return false;
  }

  // Fill all buffers initially
  for (int i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    _dmaDesc[i]->qe.stqe_next = _dmaDesc[(i + 1) % WLEDPB_I2S_DMA_BUFFER_COUNT]; // restore circular buffer chain
    _dmaDesc[i]->length = _bufferSize;
    fillBuffer(i);
    _dmaDesc[i]->eof = 1;    // enable eof, just in case
    _dmaDesc[i]->owner = 1;  // hand ownership over to DMA after descriptor init
  }

  _activeBuffer = 0; // start with first buffer
  _remainingDataBuffers = WLEDPB_I2S_DMA_BUFFER_COUNT;
  _state = DriverState::Sending;

  // Reset DMA and FIFO before starting
  _i2sDev->lc_conf.in_rst = 1;
  _i2sDev->lc_conf.out_rst = 1;
  _i2sDev->lc_conf.ahbm_rst = 1;
  _i2sDev->lc_conf.ahbm_fifo_rst = 1;
  _i2sDev->lc_conf.in_rst = 0;
  _i2sDev->lc_conf.out_rst = 0;
  _i2sDev->lc_conf.ahbm_rst = 0;
  _i2sDev->lc_conf.ahbm_fifo_rst = 0;
  _i2sDev->conf.tx_reset = 1;
  _i2sDev->conf.tx_fifo_reset = 1;
  _i2sDev->conf.tx_reset = 0;
  _i2sDev->conf.tx_fifo_reset = 0;

  // Clear and enable interrupts
  _i2sDev->int_clr.val = 0xFFFFFFFF;
  _i2sDev->int_ena.out_eof = 1;

  // Enable DMA and start
  _i2sDev->fifo_conf.dscr_en = 1;
  _i2sDev->out_link.start = 0;
  _i2sDev->out_link.addr = (uint32_t)_dmaDesc[0];
  _i2sDev->out_link.start = 1;
  _i2sDev->conf.tx_start = 1;

  return true;
}

void IRAM_ATTR I2sBusContext::dmaISR(void* arg) {
  I2sBusContext* ctx = (I2sBusContext*)arg;
  i2s_dev_t* dev = ctx->_i2sDev;

  uint32_t status = dev->int_st.val;
  dev->int_clr.val = status;

  if (!(status & I2S_OUT_EOF_INT_ST)) return;

  // The completed buffer just finished playing; DMA is now on the next buffer
  uint8_t completedBuf = ctx->_activeBuffer;
  ctx->_activeBuffer = (ctx->_activeBuffer + 1) % WLEDPB_I2S_DMA_BUFFER_COUNT;
  if (ctx->_dmaDesc[completedBuf]->qe.stqe_next == nullptr) { // this was the last buffer
    //ctx->_state = DriverState::WaitingReset; // if resetBytesLeft is not a multiple of 4 this flags end of transfer
    dev->int_ena.out_eof = 0; // disable interrupt
    dev->conf.tx_start = 0;
    dev->out_link.start = 0;
    ctx->_state = DriverState::Idle;
    return;
  } else if ((ctx->_resetBytesLeft != 3)) { // 3 means end of transfer (i.e. reset pulse) was encoded on last fillBuffer call
    ctx->fillBuffer(completedBuf); // fill buffer, handle reset pulse and end of
    ctx->_dmaDesc[completedBuf]->owner = 1; // owner is reset upon eof and must be handed back to DMA, do not hand it over if bus is stopped or it breaks the state-machine
    return;
  }
}

// I2sBus implementation

I2sBus::I2sBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t busNum, uint8_t ledType, size_t numPixels)
  : _pin(pin)
  , _busNum(busNum)
  , _timing(timing)
  , _initialized(false)
  , _channelIdx(-1)
  , _ctx(nullptr)
{
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
  _numPixels = numPixels; // stored so begin() can report srcBytes to the shared I2sBusContext for DMA sizing
}

I2sBus::~I2sBus() {
  end();
}

bool I2sBus::begin() {
  if (_initialized) return true;

  _ctx = I2sBusContext::get(_busNum);
  if (!_ctx) return false;

  if (!_ctx->init(_timing)) {
    I2sBusContext::release(_busNum);
    _ctx = nullptr;
    return false;
  }

  // pass our encoded byte count so the context can size DMA buffers for the largest bus
  const size_t srcBytes = (size_t)_numPixels * _encoder.getPixelBytes();
  _channelIdx = _ctx->registerChannel(_pin, this, srcBytes, _inverted);
  if (_channelIdx < 0) {
    //DEBUG_PRINTF_P(PSTR("[I2S] registerChannel failed for pin %d\n"), _pin);
    I2sBusContext::release(_busNum);
    _ctx = nullptr;
    return false;
  }

  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getPixelBytes())) { end(); return false; }
  //DEBUG_PRINTF_P(PSTR("[I2S] I2sBus::begin() OK: pin=%d, bus=%u, channel=%d\n"), _pin, _busNum, _channelIdx);
  return true;
}

// invert output signal, must be set before begin()
void I2sBus::setInverted(bool inv) {
  _inverted = inv;
}

void I2sBus::end() {
  if (!_initialized) return;

  if (_ctx) {
    // Wait for any active transmission to complete before cleanup
    while (!_ctx->isIdle()) vTaskDelay(1);
    _ctx->unregisterChannel(_channelIdx);
    I2sBusContext::release(_busNum);
    _ctx = nullptr;
  }

  if (_encodeBuffer) {
    heap_caps_free(_encodeBuffer);
    _encodeBuffer = nullptr;
    _encodeBufferSize = 0;
  }

  _initialized = false;
}

bool I2sBus::allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) {
  const size_t pixelBytes = (size_t)numPixels * numChannels;
  size_t needed = _prefixLen + pixelBytes + _suffixLen;
  if (_encodeBuffer && _encodeBufferSize >= needed) return true;
  if (_encodeBuffer) { heap_caps_free(_encodeBuffer); _encodeBuffer = nullptr; }
  if (needed == 0) return true;
  _encodeBuffer = (uint8_t*)heap_caps_malloc(needed, MALLOC_CAP_DMA);
  if (!_encodeBuffer) { _encodeBufferSize = 0; return false; }
  memset(_encodeBuffer, 0, needed);
  _encodeBufferSize = needed;
  _pixelData  = _encodeBuffer + _prefixLen;
  if (_suffixLen == sizeof(SM16825_SUFFIX) && _ledType == TYPE_SM16825)
    memcpy(_pixelData + pixelBytes, SM16825_SUFFIX, sizeof(SM16825_SUFFIX));
  return true;
}

bool I2sBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_ctx || !_encodeBuffer || _numPixels == 0) return false;

  // Wait for previous transmission to complete
  while (!_ctx->isIdle()) {
    vTaskDelay(1);
  }

  // Send already-encoded buffer directly
  _ctx->setChannelData(_channelIdx, _encodeBuffer, _encodeBufferSize);
  return _ctx->startTransmit();
}

bool I2sBus::canShow() const {
  if (!_ctx) return true;
  return _ctx->isIdle();
}

void I2sBus::setColorOrder(uint8_t co) {
  _encoder = ColorEncoder(co, _encoder.getColorChannels(), _ledType);
}


} // namespace WLEDpixelBus
#endif // WLEDPB_I2S_SUPPORT
