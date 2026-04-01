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
  , _bufferSize(0)
  , _activeBuffer(0)
  , _remainingDataBuffers(0)
  , _timing{0, 0, 0, 0, 0}
  , _clockDiv(1)
  , _isrHandle(nullptr)
  , _channelCount(0)
  , _channelMask(0)
  , _stagedMask(0)
  , _maxDataLen(0)
{
#if defined(WLEDPB_ESP32)
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

bool I2sBusContext::init(const LedTiming& timing, size_t bufferSize) {
  if (_initialized) return true;

  //pinMode(33, OUTPUT); // debug pin for timing analysis

  _timing = timing;
  _bufferSize = (bufferSize + 3) & ~3;  // align to 4 bytes

  // Allocate DMA buffers (4-byte aligned for DMA)
  for (int i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    _dmaBuffer[i] = (uint8_t*)heap_caps_aligned_alloc(4, _bufferSize, MALLOC_CAP_DMA);
    if (!_dmaBuffer[i]) {
      Serial.println("I2S DMA buffer alloc failed");
      deinit();
      return false;
    }
    memset(_dmaBuffer[i], 0, _bufferSize);

    _dmaDesc[i] = (lldesc_t*)heap_caps_aligned_alloc(4, sizeof(lldesc_t), MALLOC_CAP_DMA);
    if (!_dmaDesc[i]) {
      Serial.println("I2S DMA desc alloc failed");
      deinit();
      return false;
    }
  }

  // Setup DMA descriptors - circular chain
  for (int i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    _dmaDesc[i]->size = _bufferSize;
    _dmaDesc[i]->length = _bufferSize;
    _dmaDesc[i]->buf = _dmaBuffer[i];
    _dmaDesc[i]->eof = 1;  // Generate interrupt on completion
    _dmaDesc[i]->sosf = 0;
    _dmaDesc[i]->owner = 1;
    _dmaDesc[i]->qe.stqe_next = _dmaDesc[(i + 1) % WLEDPB_I2S_DMA_BUFFER_COUNT];
  }

  // Enable I2S peripheral
#if defined(WLEDPB_ESP32)
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

  // Configure for 16-bit parallel LCD mode (16 channels)
  _i2sDev->conf2.val = 0;
  _i2sDev->conf2.lcd_en = 1;
  _i2sDev->conf2.lcd_tx_wrx2_en = 0;  // disable 8-bit double-write swap path
  _i2sDev->conf2.lcd_tx_sdx2_en = 0;

  // DMA config
  _i2sDev->lc_conf.val = 0;
  _i2sDev->lc_conf.out_eof_mode = 1;

  // Disable PDM
#if defined(WLEDPB_ESP32)
  _i2sDev->pdm_conf.tx_pdm_en = 0;
  _i2sDev->pdm_conf.pcm2pdm_conv_en = 0;
#endif

  // FIFO configuration
  _i2sDev->fifo_conf.val = 0;
  _i2sDev->fifo_conf.tx_fifo_mod_force_en = 1;
  //_i2sDev->fifo_conf.tx_fifo_mod = 3;  // 0=16bit dual, 1=16bit single, 2=32bit dual, 3=32bit single (32-bit linked for 16-bit samples)
  // For ESP32 Classic, use 16-bit FIFO mode
#if !defined(WLEDPB_ESP32S2)
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
  // bck_div_num must be >= 2 on ESP32 hardware (NeoPixelBus uses 4)
  // step_time = clkm_div * bck_div / base_clock_MHz * 1000 ns
  // clkm_div = step_time_ns * base_clock_MHz / (bck_div * 1000)
  const uint8_t bckDiv = 4;  // must be >= 2, NeoPixelBus uses 4
  uint32_t bitPeriodNs = timing.bitPeriod();

#if defined(WLEDPB_ESP32)
  const double baseClockMhz = 80.0; // ESP32 has 80MHz I2S base clock when not using PLL (APB clock)
#else
  const double baseClockMhz = 80.0; // S2 has 80MHz I2S base clock
#endif

  // NeoPixelBus formula: clkmdiv = nsBitSendTime / bytesPerSample / dmaBitPerDataBit / bck / 1000 * baseClkMhz
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

  Serial.printf("[I2S] Clock: bitPeriod=%uns, clkm_div=%u+%u/%u, bck_div=%u\n", bitPeriodNs, clkmInteger, divB, divA, bckDiv);
  double actualStepNs = (double)clkmInteger * bckDiv / baseClockMhz * 1000.0;
  if (divA > 0) actualStepNs = ((double)clkmInteger + (double)divB / divA) * bckDiv / baseClockMhz * 1000.0;
  Serial.printf("[I2S] Step time: %.1fns (target: %.1fns), bit period: %.1fns\n", actualStepNs, (double)bitPeriodNs / 4.0, actualStepNs * 4.0);

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

  // Sample rate - bck must be >= 2 (NeoPixelBus uses 4)
  _i2sDev->sample_rate_conf.val = 0;
  _i2sDev->sample_rate_conf.tx_bck_div_num = bckDiv;
  _i2sDev->sample_rate_conf.tx_bits_mod = 16;  // 16-bit samples for 16 parallel channels

  // Final reset before ISR install
  _i2sDev->lc_conf.in_rst = 1; _i2sDev->lc_conf.out_rst = 1;
  _i2sDev->lc_conf.ahbm_rst = 1; _i2sDev->lc_conf.ahbm_fifo_rst = 1;
  _i2sDev->lc_conf.in_rst = 0; _i2sDev->lc_conf.out_rst = 0;
  _i2sDev->lc_conf.ahbm_rst = 0; _i2sDev->lc_conf.ahbm_fifo_rst = 0;
  _i2sDev->conf.tx_reset = 1; _i2sDev->conf.tx_fifo_reset = 1;
  _i2sDev->conf.tx_reset = 0; _i2sDev->conf.tx_fifo_reset = 0;

  // Install ISR
  int intSource;
  #if defined(WLEDPB_ESP32)
  intSource = (_busNum == 0) ? ETS_I2S0_INTR_SOURCE : ETS_I2S1_INTR_SOURCE;
  #else
  intSource = ETS_I2S0_INTR_SOURCE;
  #endif

  esp_err_t err = esp_intr_alloc(intSource, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3, dmaISR, this, &_isrHandle);
  if (err != ESP_OK) {
    Serial.printf("I2S ISR alloc failed: %d", err);
    deinit();
    return false;
  }

  _initialized = true;
  Serial.printf("[I2S] Init complete: bus=%u, bufSize=%u\n", _busNum, _bufferSize);
  return true;
}

void I2sBusContext::deinit() {
  // wait for finish sending before deinit (just in case)
  while (!isIdle()) { vTaskDelay(1); }

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

#if defined(WLEDPB_ESP32)
  periph_module_disable(_busNum == 0 ? PERIPH_I2S0_MODULE :  PERIPH_I2S1_MODULE);
#else
  periph_module_disable(PERIPH_I2S0_MODULE);
#endif

  _initialized = false;
}

int8_t I2sBusContext::registerChannel(int8_t pin, I2sBus* bus) {
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

  // Configure GPIO
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);

  // Route I2S output to GPIO
  int sigIdx;
  #if defined(WLEDPB_ESP32)
  //sigIdx = (_busNum == 0) ? I2S0O_DATA_OUT0_IDX : I2S1O_DATA_OUT0_IDX;
  sigIdx = (_busNum == 0) ? I2S0O_DATA_OUT8_IDX : I2S1O_DATA_OUT8_IDX; // 16 bit mode, mapping starts at DATA_OUT8_IDX for the wide 16-bit window
  #elif defined(WLEDPB_ESP32S2)
  // For 16-bit mode on S2, mapping starts at DATA_OUT8_IDX for the wide 16-bit window
  sigIdx = I2S0O_DATA_OUT8_IDX;
  #else
  sigIdx = I2S0O_DATA_OUT0_IDX;
  #endif
  sigIdx += idx;

  gpio_matrix_out(pin, sigIdx, false, false);

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

// 426us to fill 16 channels with 2048 bytes buffer
/*
void IRAM_ATTR I2sBusContext::encode4Step(uint8_t* dest, size_t destLen) {
REG_WRITE(GPIO_OUT1_W1TS_REG, (1u << (33-32))); // pin33 high
  // 4-step cadence encoding for parallel output with 16-bit sample words
  // Each source bit becomes 4 DMA words (one bit per channel in each 16-bit word)
  // Desired output (per bit): [HIGH][data][data][LOW]
  // Buffer is always filled completely (zeros = LOW = reset signal)

  //memset(dest, 0, destLen);
  size_t pos = 0;

  // Pre-calculate max channels to speed up loop
  uint8_t maxCh = 0;
  for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
    if (_channels[ch].active) maxCh = ch + 1;
  }

  // Process each source byte position across all channels
  while (pos + 64 <= destLen) {  // 8 bits * 4 steps * 2 bytes = 64 bytes per source byte
    bool hasData = false;

    for (int ch = 0; ch < maxCh; ch++) {
      if (!_channels[ch].active) continue;
      if (_channels[ch].srcPos >= _channels[ch].srcLen) continue;

      hasData = true;
      uint8_t srcByte = _channels[ch].srcData[_channels[ch].srcPos];
      uint16_t chMask = (1 << ch);

      uint16_t* p = (uint16_t*)(dest + pos);

      #if defined(WLEDPB_ESP32S2)
      // ESP32-S2 does NOT swap half-words (memory layout [step0, step1, step2, step3])
      p[0] |= chMask; if (srcByte & 0x80) { p[1] |= chMask; p[2] |= chMask; } p += 4; // bit 7
      p[0] |= chMask; if (srcByte & 0x40) { p[1] |= chMask; p[2] |= chMask; } p += 4; // bit 6
      p[0] |= chMask; if (srcByte & 0x20) { p[1] |= chMask; p[2] |= chMask; } p += 4; // bit 5
      p[0] |= chMask; if (srcByte & 0x10) { p[1] |= chMask; p[2] |= chMask; } p += 4; // bit 4
      p[0] |= chMask; if (srcByte & 0x08) { p[1] |= chMask; p[2] |= chMask; } p += 4; // bit 3
      p[0] |= chMask; if (srcByte & 0x04) { p[1] |= chMask; p[2] |= chMask; } p += 4; // bit 2
      p[0] |= chMask; if (srcByte & 0x02) { p[1] |= chMask; p[2] |= chMask; } p += 4; // bit 1
      p[0] |= chMask; if (srcByte & 0x01) { p[1] |= chMask; p[2] |= chMask; } p += 4; // bit 0
      #else
      // ESP32 classic: sequence for 16-bit samples [S1, S0, S3, S2]
      p[1] |= chMask; if (srcByte & 0x80) { p[0] |= chMask; p[3] |= chMask; } p += 4; // bit 7
      p[1] |= chMask; if (srcByte & 0x40) { p[0] |= chMask; p[3] |= chMask; } p += 4; // bit 6
      p[1] |= chMask; if (srcByte & 0x20) { p[0] |= chMask; p[3] |= chMask; } p += 4; // bit 5
      p[1] |= chMask; if (srcByte & 0x10) { p[0] |= chMask; p[3] |= chMask; } p += 4; // bit 4
      p[1] |= chMask; if (srcByte & 0x08) { p[0] |= chMask; p[3] |= chMask; } p += 4; // bit 3
      p[1] |= chMask; if (srcByte & 0x04) { p[0] |= chMask; p[3] |= chMask; } p += 4; // bit 2
      p[1] |= chMask; if (srcByte & 0x02) { p[0] |= chMask; p[3] |= chMask; } p += 4; // bit 1
      p[1] |= chMask; if (srcByte & 0x01) { p[0] |= chMask; p[3] |= chMask; } p += 4; // bit 0
      #endif
    }

    if (!hasData) break;

    // Advance all channel positions
    for (int ch = 0; ch < maxCh; ch++) {
      if (_channels[ch].active && _channels[ch].srcPos < _channels[ch].srcLen) {
        _channels[ch].srcPos++;
      }
    }

    pos += 64; // move to next source byte position
  }
  // Rest of buffer remains zero (reset signal) from memset
  REG_WRITE(GPIO_OUT1_W1TC_REG, (1u << (33-32))); // pin33 low
}*/

// 237us to fill 16 channels with 2048 bytes buffer
void IRAM_ATTR I2sBusContext::encode4Step(uint8_t* dest, size_t destLen) {

//REG_WRITE(GPIO_OUT1_W1TS_REG, (1u << (33-32))); // pin33 high
  // Pre-calculate max active channel
  uint8_t maxCh = 0;
  for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
    if (_channels[ch].active) maxCh = ch + 1;
  }

  for (size_t pos = 0; pos + 64 <= destLen; pos += 64) {

    // ── Phase 1: Gather ───────────────────────────────────────────────────────
    // Pure register work — no DMA memory touched.
    // bitMask[i]: which channels have bit (7-i) set in their current src byte
    // alwaysMask: which channels have active data at all (the HIGH step)
    uint16_t alwaysMask = 0;
    uint16_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;   // named regs: compiler keeps in regs
    uint16_t b4 = 0, b5 = 0, b6 = 0, b7 = 0;

    for (int ch = 0; ch < maxCh; ch++) {
      if (!_channels[ch].active) continue;
      if (_channels[ch].srcPos >= _channels[ch].srcLen) continue;

      const uint16_t m = (uint16_t)(1u << ch);
      alwaysMask |= m;

      const uint8_t b = _channels[ch].srcData[_channels[ch].srcPos++]; // advance here

      // Branchless: (0u - bit) is 0x0000 or 0xFFFF
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

    // ── Phase 2: Scatter ─────────────────────────────────────────────────────
    // Fully unrolled. Plain assignment (=) not OR: all channels are already
    // merged into the masks, and the buffer is zero-initialized before this call.
    // 16 x 32-bit stores total, no loop, no branches.
    uint32_t* p = (uint32_t*)(dest + pos);

#if defined(WLEDPB_ESP32S2)
    // S2 layout in memory: [step0, step1, step2, step3]  (no half-word swap)
    // step0 = HIGH (always),  step1 = data,  step2 = data,  step3 = LOW
    // As 32-bit pairs: p[0] = (step1<<16)|step0,  p[1] = (step3<<16)|step2
    //                       = (bN    <<16)|alwaysMask,  p[1] = bN
    #define EMIT(bN, OFF) \
      p[OFF]   = ((uint32_t)(bN) << 16) | alwaysMask; \
      p[OFF+1] = (bN);

#else
    // Classic ESP32 layout in memory: [S1, S0, S3, S2]  (half-words swapped)
    // I2S output order:  S0=HIGH(always), S1=data, S2=data, S3=LOW
    // As 32-bit pairs: p[0] = (S0<<16)|S1 = (alwaysMask<<16)|bN
    //                  p[1] = (S2<<16)|S3 = (bN<<16)|0
    // Precompute the constant high half once:
    const uint32_t AH = (uint32_t)alwaysMask << 16;
    #define EMIT(bN, OFF) \
      p[OFF]   = AH | (bN); \
      p[OFF+1] = (uint32_t)(bN) << 16;
#endif

    EMIT(b0, 0)
    EMIT(b1, 2)
    EMIT(b2, 4)
    EMIT(b3, 6)
    EMIT(b4, 8)
    EMIT(b5, 10)
    EMIT(b6, 12)
    EMIT(b7, 14)
    #undef EMIT
  }
//REG_WRITE(GPIO_OUT1_W1TC_REG, (1u << (33-32))); // pin33 low
}

void I2sBusContext::fillBuffer(uint8_t bufIdx) {
  encode4Step(_dmaBuffer[bufIdx], _bufferSize);
  // desc->length stays at _bufferSize (set in init, never changes)
}

// ----- I2S ISR Tracking -----
static volatile uint32_t s_i2sIsrCount = 0;
static volatile uint32_t s_i2sIsrSending = 0;
static volatile uint32_t s_i2sIsrReset = 0;
static volatile uint32_t s_i2sIsrIdle = 0;

bool I2sBusContext::startTransmit() {
  if (_state != DriverState::Idle) return false;
  if (_channelCount == 0) return false;

  // Only start transmission if ALL active channels have populated data
  if (_stagedMask != _channelMask) return false;
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

  // Fill all buffers initially
  for (int i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    fillBuffer(i);
    _dmaDesc[i]->owner = 1;  // Restore ownership after descriptor init
  }

  _activeBuffer = 0;
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

  // ----- DEBUG BLOCK START -----
  /*
  static uint32_t last_isr = 0;
  uint32_t diff_isr = s_i2sIsrCount - last_isr;
  last_isr = s_i2sIsrCount;
  Serial.printf("[I2S-Tx] startTransmit triggering. ISR count delta since last tx: %u\n", diff_isr);
  Serial.printf("[I2S-Tx] State vars: isrTotal=%u, send=%u, reset=%u, idle=%u\n", 
          s_i2sIsrCount, s_i2sIsrSending, s_i2sIsrReset, s_i2sIsrIdle);
  Serial.printf("[I2S-Tx] HW Regs: conf(0x%08x) conf1(0x%08x) conf2(0x%08x)\n", 
          _i2sDev->conf.val, _i2sDev->conf1.val, _i2sDev->conf2.val);
  Serial.printf("[I2S-Tx] int_ena(0x%08x) int_raw(0x%08x)\n", 
          _i2sDev->int_ena.val, _i2sDev->int_raw.val);
  Serial.printf("[I2S-Tx] out_link(0x%08x) lc_conf(0x%08x)\n", 
          _i2sDev->out_link.val, _i2sDev->lc_conf.val);
      */
  // ----- DEBUG BLOCK END -----

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
  memset(ctx->_dmaBuffer[completedBuf], 0, ctx->_bufferSize); // clear the buffer, will be filled or left blank as reset signal

  if (ctx->_state == DriverState::Sending) {
    // Encode next chunk into the completed buffer
    // encode4Step always fills the full buffer (zeros for any remainder)
    ctx->encode4Step(ctx->_dmaBuffer[completedBuf], ctx->_bufferSize);

    // Check if all source data has been consumed
    bool moreData = false;
    for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
      if (ctx->_channels[ch].active &&
        ctx->_channels[ch].srcPos < ctx->_channels[ch].srcLen) {
        moreData = true;
        break;
      }
    }
    if (!moreData) {
      // Last data chunk was just encoded into completedBuf.
      // DMA is currently playing the next buffer and remaining data buffers are pending.
      ctx->_remainingDataBuffers = WLEDPB_I2S_DMA_BUFFER_COUNT;
      ctx->_state = DriverState::SendingLast;
    }

    // Restore DMA ownership so DMA can use this buffer
    ctx->_dmaDesc[completedBuf]->owner = 1;
  } else if (ctx->_state == DriverState::SendingLast) {
    // One data buffer has just finished; convert it to reset data.
    if (ctx->_remainingDataBuffers > 0) {
      ctx->_remainingDataBuffers--;
    }

    ctx->_dmaDesc[completedBuf]->owner = 1;

    if (ctx->_remainingDataBuffers == 0) {
      // We have cycled through all pending data buffers and have zeroed the last one.
      // The next buffer to play will be reset data. Wait for that one finish then stop.
      ctx->_state = DriverState::WaitingReset;
    } else {
      ctx->_state = DriverState::SendingLast;
    }
  } else {
    // WaitingReset - last data played, zero buffer sent as reset. Stop DMA.
    dev->int_ena.out_eof = 0;
    dev->conf.tx_start = 0;
    dev->out_link.start = 0;
    ctx->_state = DriverState::Idle;
  }
}

// I2sBus implementation

I2sBus::I2sBus(int8_t pin, const LedTiming& timing, ColorOrder order,
         uint8_t busNum, size_t bufferSize)
  : _pin(pin)
  , _busNum(busNum)
  , _bufferSize(bufferSize)
  , _timing(timing)
  , _order(order)
  , _initialized(false)
  , _channelIdx(-1)
  , _ctx(nullptr)
  , _encodeBuffer(nullptr)
  , _encodeBufferSize(0)
{
}

I2sBus::~I2sBus() {
  end();
}

bool I2sBus::begin() {
  if (_initialized) return true;

  _ctx = I2sBusContext::get(_busNum);
  if (!_ctx) return false;

  if (!_ctx->init(_timing, _bufferSize)) {
    I2sBusContext::release(_busNum);
    _ctx = nullptr;
    return false;
  }

  _channelIdx = _ctx->registerChannel(_pin, this);
  if (_channelIdx < 0) {
    Serial.printf("[I2S] registerChannel failed for pin %d\n", _pin);
    I2sBusContext::release(_busNum);
    _ctx = nullptr;
    return false;
  }

  _initialized = true;
  Serial.printf("[I2S] I2sBus::begin() OK: pin=%d, bus=%u, channel=%d\n", _pin, _busNum, _channelIdx);
  return true;
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

bool I2sBus::allocateBuffer(uint16_t numPixels) {
  size_t needed = numPixels * getChannelCount(_order);
  if (_encodeBuffer && _encodeBufferSize >= needed) {
    return true;
  }

  if (_encodeBuffer) {
    heap_caps_free(_encodeBuffer);
  }

  _encodeBuffer = (uint8_t*)heap_caps_malloc(needed, MALLOC_CAP_DMA);
  if (!_encodeBuffer) {
    _encodeBufferSize = 0;
    return false;
  }
  _encodeBufferSize = needed;
  return true;
}

bool I2sBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
  // Always use internal pixel buffer (WLED always calls show() without args)
  if (!_initialized || !_ctx || !_pixelData || _numPixels == 0) return false;

  // Wait for previous transmission to complete
  while (!_ctx->isIdle()) {
    vTaskDelay(1);
  }

  if (!allocateBuffer(_numPixels)) return false;

  // Encode pixels to byte stream
  ColorEncoder encoder(_order);
  uint8_t* dst = _encodeBuffer;
  uint8_t numCh = encoder.getNumChannels();

  for (uint16_t i = 0; i < _numPixels; i++) {
    encoder.encode(_pixelData[i], _cctData ? &_cctData[i] : nullptr, dst);
    dst += numCh;
  }

  // Set data for our channel and start
  _ctx->setChannelData(_channelIdx, _encodeBuffer, _numPixels * numCh);
  return _ctx->startTransmit();
}

bool I2sBus::canShow() const {
  if (!_ctx) return true;
  return _ctx->isIdle();
}

void I2sBus::setColorOrder(ColorOrder order) {
  _order = order;
}


} // namespace WLEDpixelBus
#endif // WLEDPB_I2S_SUPPORT
