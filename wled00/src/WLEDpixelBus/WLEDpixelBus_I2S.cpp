/*-------------------------------------------------------------------------

WLEDpixelBus - parallel I2S/LCD output driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

supports ESP32, ESP32 S2 and ESP32 S3 (via LCD peripheral)
Default is 8 parallel outputs and double DMA buffering but it also supports 16 parallel outputs if needed
For 16 parallel output, triple buffering is required for glitch-free output.
Data is output in 4-step cadence meaning each LED bit is encoded into 4 I2S bits. '0' is 0b1000 and '1' is 0b1110
Encoding is highly optimized for speed as encoding is done "on the fly" while the other buffer is being sent out using DMA.
The RAM usage of the sendout buffer is number of LEDs * bytes per LED + DMA buffer size
3k per DMA buffer works well, enough for 32 RGB LEDs in 8x parallel output or roughly 0.9ms between buffer swaps
Each bus can have individual configuration of color channels but all must share the same timing

-------------------------------------------------------------------------*/

#include "WLEDpixelBus_I2S.h"

#ifdef WLEDPB_I2S_SUPPORT
namespace WLEDpixelBus {

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

#ifdef CONFIG_IDF_TARGET_ESP32S3
I2sBusContext::I2sBusContext(uint8_t /*busNum*/)
  : _dmaChannel(nullptr)
  , _dmaChanId(-1)
#else
I2sBusContext::I2sBusContext(uint8_t busNum)
  : _busNum(busNum)
  , _i2sDev(
#if defined(CONFIG_IDF_TARGET_ESP32)
      (busNum == 0) ? &I2S0 : &I2S1
#else
      &I2S0
#endif
    )
  , _isrHandle(nullptr)
#endif
  , _lastFilled(0)
{
  for (int i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    _dmaDesc[i] = nullptr;
    _dmaBuffer[i] = nullptr;
  }
}

I2sBusContext::~I2sBusContext() {
  deinit();
}

#ifdef CONFIG_IDF_TARGET_ESP32S3 // S3 uses the LCD peripheral, not I2S

bool I2sBusContext::hwInit() {
  // Enable LCD_CAM peripheral
  periph_module_enable(PERIPH_LCD_CAM_MODULE);
  periph_module_reset(PERIPH_LCD_CAM_MODULE);

  // Reset LCD
  LCD_CAM.lcd_user.lcd_reset = 1;
  esp_rom_delay_us(100);
  LCD_CAM.lcd_user.lcd_reset = 0;
  esp_rom_delay_us(100);

  // Calculate clock divider for 4-step cadence (240 MHz LCD base clock)
  double clkm_div = 240000000.0 / (double)calc4StepClockHz(_timing);
  if (clkm_div > LCD_LL_CLK_FRAC_DIV_N_MAX || clkm_div < 2.0) {
    return false;
  }

  uint8_t clkm_div_int = (uint8_t)clkm_div;
  double clkm_frac = clkm_div - clkm_div_int;
  uint8_t divB = 0;
  uint8_t divA = 0;
  if (clkm_frac > 0.001) {
    divA = 63;
    divB = (uint8_t)(clkm_frac * 63.0 + 0.5);
    if (divB >= divA) divB = divA - 1;
  }

  // Configure LCD clock
  LCD_CAM.lcd_clock.clk_en = 1;
  LCD_CAM.lcd_clock.lcd_clk_sel = 2;
  LCD_CAM.lcd_clock.lcd_clkm_div_a = divA;
  LCD_CAM.lcd_clock.lcd_clkm_div_b = divB;
  LCD_CAM.lcd_clock.lcd_clkm_div_num = clkm_div_int;
  LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;
  LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;
  LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1;

  // Configure frame format
  LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0;
  LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0;
  LCD_CAM.lcd_misc.lcd_next_frame_en = 0;
  LCD_CAM.lcd_data_dout_mode.val = 0;
  LCD_CAM.lcd_user.lcd_always_out_en = 1;
  LCD_CAM.lcd_user.lcd_8bits_order = 0;
  LCD_CAM.lcd_user.lcd_bit_order = 0;
#ifdef WLED_PIXELBUS_16PARALLEL
  LCD_CAM.lcd_user.lcd_2byte_en = 1;
#else
  LCD_CAM.lcd_user.lcd_2byte_en = 0;
#endif
  LCD_CAM.lcd_user.lcd_dummy = 1;
  LCD_CAM.lcd_user.lcd_dummy_cyclelen = 0;
  LCD_CAM.lcd_user.lcd_cmd = 0;

  // Allocate GDMA channel
  gdma_channel_alloc_config_t dma_chan_config = {
    .sibling_chan = NULL,
    .direction = GDMA_CHANNEL_DIRECTION_TX,
    .flags = {.reserve_sibling = 0}
  };

  esp_err_t err = gdma_new_channel(&dma_chan_config, &_dmaChannel);
  if (err != ESP_OK) return false;
  int chanId = -1;
  if (gdma_get_channel_id(_dmaChannel, &chanId) != ESP_OK) return false;
  _dmaChanId = (int8_t)chanId;

  err = gdma_connect(_dmaChannel, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
  if (err != ESP_OK) return false;

  gdma_strategy_config_t strategy_config = {
    .owner_check = false,
    .auto_update_desc = false
  };
  gdma_apply_strategy(_dmaChannel, &strategy_config);

  // Register DMA callback
  gdma_tx_event_callbacks_t tx_cbs = {.on_trans_eof = gdmaTxDoneISR};
  gdma_register_tx_event_callbacks(_dmaChannel, &tx_cbs, this);

  return true;
}

void I2sBusContext::hwDeinit() {
  if (_dmaChannel) {
    gdma_disconnect(_dmaChannel);
    gdma_del_channel(_dmaChannel);
    _dmaChannel = nullptr;
  }
  periph_module_disable(PERIPH_LCD_CAM_MODULE);
}

void I2sBusContext::hwStartTransfer() {
  gdma_reset(_dmaChannel);
  LCD_CAM.lcd_user.lcd_dout = 1;
  LCD_CAM.lcd_user.lcd_update = 1;
  LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
  LCD_CAM.lcd_misc.lcd_afifo_reset = 0;
  gdma_start(_dmaChannel, (intptr_t)_dmaDesc[0]);
  esp_rom_delay_us(1);
  LCD_CAM.lcd_user.lcd_start = 1;
}

void IRAM_ATTR I2sBusContext::stopTransferHwIram() {
  LCD_CAM.lcd_user.lcd_start = 0;
  // Register-level GDMA stop: gdma_stop() lives in flash and must not be called from ISR
  if (_dmaChanId >= 0) GDMA.channel[_dmaChanId].out.link.stop = 1;
  _state = DriverState::Idle;
}

void I2sBusContext::channelAttached(int8_t idx, int8_t pin, bool inverted) {
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT);
  esp_rom_gpio_connect_out_signal(pin, LCD_DATA_OUT0_IDX + idx, inverted, false);
  gpio_hal_context_t gpio_hal = { .dev = GPIO_HAL_GET_HW(GPIO_PORT_0) };
  gpio_hal_func_sel(&gpio_hal, GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
  gpio_set_drive_capability((gpio_num_t)pin, GPIO_DRIVE_CAP_3);
}

#else // !CONFIG_IDF_TARGET_ESP32S3

bool I2sBusContext::hwInit() {
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
#else
  _i2sDev->fifo_conf.tx_fifo_mod = 3;
#endif
  _i2sDev->fifo_conf.tx_data_num = 32;  // FIFO threshold

  // note: PCM bypass is not needed for raw parallel output in LCD mode so we do not touch conf1

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
  // clkm_div = base_clock_Hz / (4-step cadence clock * bck_div)
  const uint8_t bckDiv = 4;  // must be >= 2
  const uint32_t clkHz = calc4StepClockHz(_timing); // 4 clock cycles per LED bit period

#if defined(CONFIG_IDF_TARGET_ESP32)
  #ifndef WLED_PIXELBUS_16PARALLEL
  // 8-bit mode: lcd_tx_wrx2_en=1 halves the effective output rate (WR pulses at BCK/2).
  // Use 2x clock constant so the divider is doubled, yielding the correct BCK after the factor-of-2.
  const double baseClockHz = 160000000.0;
  #else
  const double baseClockHz = 80000000.0; // 16-bit mode: APB clock, lcd_tx_wrx2_en=0 has no rate halving
  #endif
#else
  const double baseClockHz = 80000000.0; // S2: 80MHz I2S base clock (wrx2 on S2 does not halve the rate)
#endif

  double clkmdiv = baseClockHz / (double)clkHz / (double)bckDiv;
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

  _i2sDev->sample_rate_conf.val = 0;
  _i2sDev->sample_rate_conf.tx_bck_div_num = bckDiv;
#ifdef WLED_PIXELBUS_16PARALLEL
  _i2sDev->sample_rate_conf.tx_bits_mod = 16; // 16-bit samples for up to 16 parallel channels
#else
  _i2sDev->sample_rate_conf.tx_bits_mod = 8;  // 8-bit samples for up to 8 parallel channels
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
    deinit();
    return false;
  }

  return true;
}

void I2sBusContext::hwDeinit() {
  if (_isrHandle) {
    esp_intr_free(_isrHandle);
    _isrHandle = nullptr;
  }
#if defined(CONFIG_IDF_TARGET_ESP32)
  periph_module_disable(_busNum == 0 ? PERIPH_I2S0_MODULE : PERIPH_I2S1_MODULE);
#else
  periph_module_disable(PERIPH_I2S0_MODULE);
#endif
}

void I2sBusContext::hwStartTransfer() {
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

  _i2sDev->int_clr.val = 0xFFFFFFFF;
  _i2sDev->int_ena.out_eof = 1;       // single descriptor transfer finished
  _i2sDev->int_ena.out_total_eof = 1; // full transfer finished

  _i2sDev->fifo_conf.dscr_en = 1;
  _i2sDev->out_link.start = 0;
  _i2sDev->out_link.addr = (uint32_t)_dmaDesc[0];
  _i2sDev->out_link.start = 1;
  _i2sDev->conf.tx_start = 1;
}

void IRAM_ATTR I2sBusContext::stopTransferHwIram() {
  if (_i2sDev) {
    _i2sDev->int_ena.out_eof = 0;
    _i2sDev->int_ena.out_total_eof = 0;
    _i2sDev->conf.tx_start = 0;
    _i2sDev->out_link.start = 0;
  }
  _state = DriverState::Idle;
}

void I2sBusContext::channelAttached(int8_t idx, int8_t pin, bool inverted) {
  gpio_set_direction((gpio_num_t)pin, GPIO_MODE_OUTPUT); // Configure GPIO
  int sigIdx;
#ifdef WLED_PIXELBUS_16PARALLEL
  #if defined(CONFIG_IDF_TARGET_ESP32)
    sigIdx = (_busNum == 0) ? I2S0O_DATA_OUT8_IDX : I2S1O_DATA_OUT8_IDX;
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    sigIdx = I2S0O_DATA_OUT8_IDX; // 16-bit mode: mapping starts at DATA_OUT8_IDX for the wide 16-bit window
  #else
    sigIdx = I2S0O_DATA_OUT0_IDX;
  #endif
#else
  #if defined(CONFIG_IDF_TARGET_ESP32)
    sigIdx = (_busNum == 0) ? I2S0O_DATA_OUT0_IDX : I2S1O_DATA_OUT0_IDX;
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    sigIdx = I2S0O_DATA_OUT16_IDX; // 8-bit parallel maps to upper bytes on S2
  #else
    sigIdx = I2S0O_DATA_OUT0_IDX;
  #endif
#endif
  sigIdx += idx;
  esp_rom_gpio_connect_out_signal(pin, sigIdx, inverted, false);
}
#endif // !CONFIG_IDF_TARGET_ESP32S3

void I2sBusContext::channelDetached(int8_t /*idx*/, int8_t pin) {
  if (pin >= 0) gpio_reset_pin((gpio_num_t)pin);
}

void I2sBusContext::releaseResources() {
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
}

bool I2sBusContext::_allocDmaBuffers() {
  if (_dmaAllocated) return true;
  _bufferSize = calc4StepBufferSize(_maxSrcBytes, WLEDPB_I2S_DMABYTES,
    WLEDPB_I2S_DMA_BUFFER_COUNT, MIN_DMA_BUFFER_SIZE, DEFAULT_DMA_BUFFER_SIZE);
  if (_bufferSize == 0) return false;

  auto rollback = [&]() {
    for (uint8_t i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; ++i) {
      if (_dmaDesc[i]) heap_caps_free(_dmaDesc[i]);
      if (_dmaBuffer[i]) heap_caps_free(_dmaBuffer[i]);
      _dmaDesc[i] = nullptr;
      _dmaBuffer[i] = nullptr;
    }
    _bufferSize = 0;
  };

  for (uint8_t i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; ++i) {
    _dmaBuffer[i] = (uint8_t*)heap_caps_aligned_alloc(4, _bufferSize, MALLOC_CAP_DMA);
    if (!_dmaBuffer[i]) { rollback(); return false; }
    memset(_dmaBuffer[i], 0, _bufferSize);
    _dmaDesc[i] = (DmaDesc_t*)heap_caps_aligned_alloc(4, sizeof(DmaDesc_t), MALLOC_CAP_DMA);
    if (!_dmaDesc[i]) { rollback(); return false; }
  }

  for (uint8_t i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; ++i) {
    descSetSizeAndLen(_dmaDesc[i], _bufferSize);
    descSetBuf(_dmaDesc[i], _dmaBuffer[i]);
    descSetEof(_dmaDesc[i]);
    descSetOwnerDma(_dmaDesc[i]);
    descSetNext(_dmaDesc[i], _dmaDesc[(i + 1) % WLEDPB_I2S_DMA_BUFFER_COUNT]);
  }
  _dmaAllocated = true;
  //DEBUG_PRINTF_P(PSTR("[I2S] DMA buffers allocated: bufSize=%u x%u\n"), _bufferSize, WLEDPB_I2S_DMA_BUFFER_COUNT);
  return true;
}

// encode4Step: 4-step cadence, converts per-channel byte streams to parallel DMA words.

#ifdef WLED_PIXELBUS_16PARALLEL
// 16-bit parallel encode: branchless gather + scatter, 64 bytes per source byte (16 channels)
void IRAM_ATTR I2sBusContext::encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel) {
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
  encode4Step16<Step4Layout::Standard>(dest, destLen, maxChannel, _channels);
#else
  encode4Step16<Step4Layout::WordSwapped>(dest, destLen, maxChannel, _channels);
#endif
}

#else
// 8-bit parallel encode: branchless gather + scatter, 32 bytes per source byte (8 channels)
// uses the shared 4-step encoder, only the scatter layout differs per target
void IRAM_ATTR I2sBusContext::encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel) {
#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
  encode4Step8<uint8_t, Step4Layout::Standard>(dest, destLen, maxChannel, _channels);
#else
  // Classic ESP32: half-word swap, memory [S2,S3,S0,S1] = [data,0,HIGH,data]
  encode4Step8<uint8_t, Step4Layout::WordSwapped>(dest, destLen, maxChannel, _channels);
#endif
}
#endif // WLED_PIXELBUS_16PARALLEL

void IRAM_ATTR __attribute__((noinline)) I2sBusContext::fillBuffer(uint8_t bufIdx) {
  uint32_t* buf32 = (uint32_t*)_dmaBuffer[bufIdx];
  const uint32_t* end = buf32 + (_bufferSize >> 2);
  while (buf32 < end) *buf32++ = 0; // clear buffer (set all lanes low), DMA buffer is 4 bytes aligned. Note: memset is not IRAM safe and may crash

  if (_resetBytesLeft > 0 && _resetBytesLeft != WLEDPB_I2S_XFER_DONE_FLAG) {
    if (_resetBytesLeft <= _bufferSize) {
      descSetLength(_dmaDesc[bufIdx], _resetBytesLeft);
      descSetNext(_dmaDesc[bufIdx], nullptr);
      _resetBytesLeft = WLEDPB_I2S_XFER_DONE_FLAG;
    } else {
      descSetLength(_dmaDesc[bufIdx], _bufferSize);
      _resetBytesLeft -= _bufferSize;
    }
    return;
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

  size_t translatedBytes = bytesToEncode * WLEDPB_I2S_DMABYTES;
  if (translatedBytes > _bufferSize) translatedBytes = _bufferSize;
  translatedBytes -= translatedBytes % WLEDPB_I2S_DMABYTES;
  descSetLength(_dmaDesc[bufIdx], translatedBytes);
  encode4Step(_dmaBuffer[bufIdx], translatedBytes, maxCh);

  if (translatedBytes < _bufferSize) {
    // Data ran out before the buffer was full (i.e. we are done), compute the minimum reset period we must send as zero cycles
    size_t resetBytes = calc4StepResetDmaBytes(_timing, WLEDPB_I2S_DMABYTES / 8);

    size_t newLen = translatedBytes + resetBytes;
    if (newLen > _bufferSize) {
      _resetBytesLeft = newLen - _bufferSize; // reset pulse does not fit into this buffer frame, send another one (see above)
      descSetLength(_dmaDesc[bufIdx], _bufferSize);
    }
    else {
      descSetLength(_dmaDesc[bufIdx], newLen);     // send the rest (zeroes) as a reset
      _resetBytesLeft = WLEDPB_I2S_XFER_DONE_FLAG; // flag end of frame, don't queue any more buffers or it will mess up the DMA
      descSetNext(_dmaDesc[bufIdx], nullptr);      // reset fit into this buffer, end transfer after this is sent
    }
  }
}

bool I2sBusContext::startTransmit() {
  if (_state != DriverState::Idle) return false;
  if (_channelCount == 0) return false;

  // Only start transmission if ALL active channels have populated data
  if (_stagedMask != _channelMask) return true;
  _stagedMask = 0; // Reset for next frame

  for (int ch = 0; ch < WLEDPB_I2S_MAX_CHANNELS; ch++) {
    if (_channels[ch].active) {
      _channels[ch].srcPos = 0;
    }
  }

  _resetBytesLeft = 0;

  if (!_dmaAllocated) {
    if (!_allocDmaBuffers()) return false;
  }

  // Fill all buffers initially
  for (int i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    descSetNext(_dmaDesc[i], _dmaDesc[(i + 1) % WLEDPB_I2S_DMA_BUFFER_COUNT]); // restore circular buffer chain
    descSetLength(_dmaDesc[i], _bufferSize);
    fillBuffer(i);
    descSetEof(_dmaDesc[i]);       // enable eof, just in case
    descSetOwnerDma(_dmaDesc[i]);  // hand ownership over to DMA after descriptor init
  }

  _lastFilled = WLEDPB_I2S_DMA_BUFFER_COUNT - 1; // all buffers were just pre-filled, next to refill is buffer 0
  _state = DriverState::Sending;
  _txStartMillis = millis(); // watchdog start

  hwStartTransfer();

  return true;
}

#ifdef CONFIG_IDF_TARGET_ESP32S3
IRAM_ATTR bool I2sBusContext::gdmaTxDoneISR(gdma_channel_handle_t dma_chan, gdma_event_data_t* event_data, void* user_data) {
  (void)event_data;
  I2sBusContext* ctx = (I2sBusContext*)user_data;
  if (dma_chan != ctx->_dmaChannel || ctx->_dmaChanId < 0) return false; // sanity check, just in case

  uint32_t eofAddr = GDMA.channel[ctx->_dmaChanId].out.eof_des_addr; // descriptor that just finished

  int8_t completedBuf = -1;
  for (uint8_t i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    if ((uint32_t)ctx->_dmaDesc[i] == eofAddr) { completedBuf = i; break; }
  }

  ctx->_processEof(completedBuf);
  return false;
}
#else
void IRAM_ATTR I2sBusContext::dmaISR(void* arg) {
  I2sBusContext* ctx = (I2sBusContext*)arg;
  i2s_dev_t* dev = ctx->_i2sDev;
  uint32_t status = dev->int_st.val;
  dev->int_clr.val = status;

  if (status & I2S_OUT_TOTAL_EOF_INT_ST) { // link terminated: frame fully sent
    ctx->stopTransferHwIram();
    return;
  }
  if (!(status & I2S_OUT_EOF_INT_ST)) return;

  // which descriptor actually produced this EOF (resyncs even if EOFs were coalesced)
  uint32_t eofAddr = dev->out_eof_des_addr;
  int8_t completedBuf = -1;
  for (uint8_t i = 0; i < WLEDPB_I2S_DMA_BUFFER_COUNT; i++) {
    if ((uint32_t)ctx->_dmaDesc[i] == eofAddr) { completedBuf = i; break; }
  }

  ctx->_processEof(completedBuf);
}
#endif

// Refill buffer(s) upon EOF. The completed descriptor is safe to reuse, the following descriptor may still be in flight.
void IRAM_ATTR I2sBusContext::_processEof(int8_t completedBuf) {
  if (completedBuf < 0) return;

  // end of frame? checks the just finished descriptor for the terminating nullptr
  if (descGetNext(_dmaDesc[completedBuf]) == nullptr) {
    stopTransferHwIram();
    return;
  }

#if WLEDPB_I2S_DMA_BUFFER_COUNT > 2
  if (_lastFilled == (uint8_t)completedBuf) return; // duplicate eof, nothing pending (never refill the in-flight buffer)
  // refill every completed-but-unprocessed buffer, oldest first, to catch up if a previous eof was missed.
  uint8_t b = (_lastFilled + 1) % WLEDPB_I2S_DMA_BUFFER_COUNT;
  while (_resetBytesLeft != WLEDPB_I2S_XFER_DONE_FLAG) { // dont fill if reset pulse was queued
    fillBuffer(b); // fill buffer, handles reset pulse and end of transfer
    _lastFilled = b;
    if (b == (uint8_t)completedBuf) break;
    b = (b + 1) % WLEDPB_I2S_DMA_BUFFER_COUNT;
  }
#else
  // double buffering: only the completed buffer is safe to refill, the other one is in-flight
  if (_resetBytesLeft != WLEDPB_I2S_XFER_DONE_FLAG) {
    fillBuffer(completedBuf); // fill buffer, handles reset pulse and end of transfer
    _lastFilled = (uint8_t)completedBuf;
  }
#endif
}

// I2sBus implementation
// the bus handling (begin/end/show/canShow) is provided by ParallelBusBase
I2sBus::I2sBus(int8_t pin, const LedTiming& timing, uint8_t busNum, uint8_t ledType)
  : ParallelBusBase<I2sBusContext>(pin, timing, busNum)
{
  // encoder is installed by createBus() from BusProperties
  _ledType = ledType;
}

} // namespace WLEDpixelBus
#endif // WLEDPB_I2S_SUPPORT
