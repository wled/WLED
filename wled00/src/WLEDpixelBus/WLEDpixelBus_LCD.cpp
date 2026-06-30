/*-------------------------------------------------------------------------

WLEDpixelBus - parallel LCD output driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

LCD hardware is available on ESP32 S3 only
Default is 8 parallel outputs and double DMA buffering but it also supports 16 parallel outputs if needed
For 16 parallel output, triple buffering is required for glitch-free output.
Data is output in 4-step cadence meaning each LED bit is encoded into 4 I2S bits. '0' is 0b1000 and '1' is 0b1110
Encoding is highly optimized for speed as encoding is done "on the fly" while the other buffer is being sent out using DMA.
The RAM usage of the sendout buffer is number of LEDs * bytes per LED + DMA buffer size
3k per DMA buffer works well, enough for 32 RGB LEDs in 8x parallel output or roughly 0.9ms between buffer swaps
Each bus can have individual configuration of color channels but all must share the same timing

-------------------------------------------------------------------------*/

#include "WLEDpixelBus.h"
#ifdef WLEDPB_LCD_SUPPORT
#include "WLEDpixelBus_LCD.h"

namespace WLEDpixelBus {

//==============================================================================
// LCD Bus Implementation (ESP32-S3)
//==============================================================================
/*-------------------------------------------------------------------------
WLEDpixelBus - LCD Implementation with Continuous DMA (Glitch-Free)

Key design:
- DMA runs continuously in circular mode (buf0 -> buf1 -> buf0 -> ...)
- Buffers are filled with data or zeros (for reset period)
- Stop only after reset period completes on buffer boundary
- No DMA reconfiguration during transmission
- DMA buffer size is computed from the largest registered bus and allocated
  once in startTransmit() (deferred allocation).
-------------------------------------------------------------------------*/

#include "driver/periph_ctrl.h"
#include "esp_private/gdma.h"
#include "esp_rom_gpio.h"
#include "hal/dma_types.h"
#include "hal/gpio_hal.h"
#include "hal/lcd_ll.h"
#include "soc/lcd_cam_struct.h"
#include "soc/gpio_sig_map.h"

// ============================================
// Configuration
// ============================================

#define LCD_LOG(x...) // Serial.printf(x) // define to debug

LcdBusContext* LcdBusContext::_instance = nullptr;
uint8_t LcdBusContext::_refCount = 0;

LcdBusContext* LcdBusContext::get() {
  if (_instance == nullptr) {
    _instance = new LcdBusContext();
  }
  _refCount++;
  return _instance;
}

void LcdBusContext::release() {
  if (_refCount == 0) return;
  _refCount--;
  if (_refCount == 0 && _instance) {
    delete _instance;
    _instance = nullptr;
  }
}

LcdBusContext::LcdBusContext()
  : _state(DriverState::Idle)
  , _initialized(false)
  , _use16Bit(false)
  , _dmaChannel(nullptr)
  , _timing{0, 0, 0, 0, 0}
  , _bufferSize(0)
  , _channelCount(0)
  , _channelMask(0)
  , _stagedMask(0)
  , _maxDataLen(0)
  , _activeBuffer(0)
  , _maxSrcBytes(0)        // HEADER
  , _resetBytesLeft(0)     // HEADER
  , _dmaAllocated(false)   // HEADER
{
  for (int i = 0; i < WLEDPB_LCD_MAX_CHANNELS; i++) {
    _channels[i] = {nullptr, -1, nullptr, 0, 0, false};
  }
  for (int i = 0; i < WLEDPB_LCD_DMA_BUFFER_COUNT; i++) {
    _dmaDesc[i] = nullptr;
    _dmaBuffer[i] = nullptr;
  }
}

LcdBusContext::~LcdBusContext() {
  deinit();
}

bool LcdBusContext::init(const LedTiming& timing, bool use16Bit) {
  if (_initialized) return true;

  LCD_LOG("Init: deferred alloc, cadence=%d, T0H=%u T1H=%u bitPeriod=%u",
      WLEDPB_LCD_CADENCE_STEPS, timing.t0h_ns, timing.t1h_ns, timing.bitPeriod());

  _timing = timing;
  _use16Bit = use16Bit;

  uint32_t bitPeriodNs = timing.bitPeriod();

  // Enable LCD_CAM peripheral
  periph_module_enable(PERIPH_LCD_CAM_MODULE);
  periph_module_reset(PERIPH_LCD_CAM_MODULE);

  // Reset LCD
  LCD_CAM.lcd_user.lcd_reset = 1;
  esp_rom_delay_us(100);
  LCD_CAM.lcd_user.lcd_reset = 0;
  esp_rom_delay_us(100);

  // Calculate clock divider
  double clkm_div = (double)bitPeriodNs / WLEDPB_LCD_CADENCE_STEPS / 1000.0 * 240.0;

  LCD_LOG("  Bit period: %u ns, clock div: %.2f", bitPeriodNs, clkm_div);

  if (clkm_div > LCD_LL_CLK_FRAC_DIV_N_MAX || clkm_div < 2.0) {
    LCD_LOG("ERROR: Invalid clock divider");
    deinit();
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
  LCD_CAM.lcd_user.lcd_2byte_en = 1; // 16-bit output for up to 16 channels
#else
  LCD_CAM.lcd_user.lcd_2byte_en = 0; // 8-bit output for up to 8 channels
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
  if (err != ESP_OK) {
    LCD_LOG("ERROR:  GDMA alloc failed:  %d", err);
    deinit();
    return false;
  }

  err = gdma_connect(_dmaChannel, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
  if (err != ESP_OK) {
    LCD_LOG("ERROR: GDMA connect failed: %d", err);
    deinit();
    return false;
  }

  gdma_strategy_config_t strategy_config = {
    .owner_check = false,
    .auto_update_desc = false
  };
  gdma_apply_strategy(_dmaChannel, &strategy_config);

  // Register DMA callback
  gdma_tx_event_callbacks_t tx_cbs = {.on_trans_eof = dmaCallback};
  gdma_register_tx_event_callbacks(_dmaChannel, &tx_cbs, this);

  _initialized = true;
  LCD_LOG("Init OK: clkm_div=%u+%u/%u",
      (unsigned)LCD_CAM.lcd_clock.lcd_clkm_div_num,
      (unsigned)LCD_CAM.lcd_clock.lcd_clkm_div_b,
      (unsigned)LCD_CAM.lcd_clock.lcd_clkm_div_a);
  return true;
}

void LcdBusContext::deinit() {
  // Stop transmission
  LCD_CAM.lcd_user.lcd_start = 0;
  _state = DriverState::Idle;

  if (_dmaChannel) {
    gdma_stop(_dmaChannel);
    gdma_disconnect(_dmaChannel);
    gdma_del_channel(_dmaChannel);
    _dmaChannel = nullptr;
  }

  for (int i = 0; i < WLEDPB_LCD_DMA_BUFFER_COUNT; i++) {
    if (_dmaBuffer[i]) {
      heap_caps_free(_dmaBuffer[i]);
      _dmaBuffer[i] = nullptr;
    }
    if (_dmaDesc[i]) {
      heap_caps_free(_dmaDesc[i]);
      _dmaDesc[i] = nullptr;
    }
  }

  if (_initialized) {
    periph_module_disable(PERIPH_LCD_CAM_MODULE);
  }

  _dmaAllocated = false;
  _initialized = false;
}

// Compute the DMA buffer size from _maxSrcBytes (set during registerChannel() by the largest bus),
// then allocate the circular DMA descriptor/buffer chain.
// Called once from startTransmit() after all channels have registered.
bool LcdBusContext::_allocDmaBuffers() {
  if (_dmaBuffer[0] != nullptr) return true; // already allocated

  size_t dmaBytesPerSrc = _use16Bit ? 64 : 32;
  _bufferSize = (dmaBytesPerSrc * _maxSrcBytes) / WLEDPB_LCD_DMA_BUFFER_COUNT;
  _bufferSize = (_bufferSize + 3) & ~3; // align to 4 bytes
  if (_bufferSize > WLEDPB_LCD_DMA_BUFFER_SIZE) _bufferSize = WLEDPB_LCD_DMA_BUFFER_SIZE;
  if (_bufferSize < 1024) _bufferSize = 1024; // MIN_DMA_BUFFER_SIZE

  for (int i = 0; i < WLEDPB_LCD_DMA_BUFFER_COUNT; i++) {
    _dmaBuffer[i] = (uint8_t*)heap_caps_aligned_alloc(4, _bufferSize, MALLOC_CAP_DMA);
    if (!_dmaBuffer[i]) {
      LCD_LOG("ERROR: DMA buffer %d alloc failed", i);
      deinit();
      return false;
    }
    memset(_dmaBuffer[i], 0, _bufferSize);

    _dmaDesc[i] = (dma_descriptor_t*)heap_caps_malloc(sizeof(dma_descriptor_t), MALLOC_CAP_DMA);
    if (!_dmaDesc[i]) {
      LCD_LOG("ERROR: DMA desc %d alloc failed", i);
      deinit();
      return false;
    }
    memset(_dmaDesc[i], 0, sizeof(dma_descriptor_t));
  }

  // Setup DMA descriptors in CIRCULAR mode (restored per-transmission in startTransmit)
  for (int i = 0; i < WLEDPB_LCD_DMA_BUFFER_COUNT; i++) {
    _dmaDesc[i]->dw0.size = _bufferSize;
    _dmaDesc[i]->dw0.length = _bufferSize;
    _dmaDesc[i]->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
    _dmaDesc[i]->dw0.suc_eof = 1;  // Always trigger EOF for ISR
    _dmaDesc[i]->buffer = _dmaBuffer[i];
    _dmaDesc[i]->next = _dmaDesc[(i + 1) % WLEDPB_LCD_DMA_BUFFER_COUNT];  // circular chain
  }

  _dmaAllocated = true;
  LCD_LOG("DMA buffers allocated: bufSize=%u x%u", (unsigned)_bufferSize, WLEDPB_LCD_DMA_BUFFER_COUNT);
  return true;
}

int8_t LcdBusContext::registerChannel(int8_t pin, LcdBus* bus, size_t srcBytes, bool inverted) {
  int8_t idx = -1;
  for (int i = 0; i < WLEDPB_LCD_MAX_CHANNELS; i++) {
    if (!_channels[i].active) {
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

  if (srcBytes > _maxSrcBytes) _maxSrcBytes = srcBytes;

  esp_rom_gpio_connect_out_signal(pin, LCD_DATA_OUT0_IDX + idx, inverted, false);
  gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
  gpio_set_drive_capability((gpio_num_t)pin, GPIO_DRIVE_CAP_3);

  LCD_LOG("Channel %d: pin=%d, mask=0x%04X, srcBytes=%u", idx, pin, _channelMask, (unsigned)srcBytes);
  return idx;
}

void LcdBusContext::unregisterChannel(int8_t channelIdx) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_LCD_MAX_CHANNELS) return;
  if (!_channels[channelIdx].active) return;

  if (_channels[channelIdx].pin >= 0) {
    gpio_reset_pin((gpio_num_t)_channels[channelIdx].pin);
  }

  _channels[channelIdx] = {nullptr, -1, nullptr, 0, 0, false};
  _channelCount--;
  _channelMask &= ~(1 << channelIdx);
}

void LcdBusContext::setChannelData(int8_t channelIdx, const uint8_t* data, size_t len) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_LCD_MAX_CHANNELS) return;

  _channels[channelIdx].srcData = data;
  _channels[channelIdx].srcLen = len;
  _channels[channelIdx].srcPos = 0;

  if (len > _maxDataLen) _maxDataLen = len;

  if (_stagedMask & (1 << channelIdx)) _stagedMask = 0;
  _stagedMask |= (1 << channelIdx);
}

#ifdef WLED_PIXELBUS_16PARALLEL
// 16-bit parallel encode: branchless gather + scatter, 64 bytes per source byte (16 channels)
void IRAM_ATTR LcdBusContext::encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel) {
  for (size_t pos = 0; pos + 64 <= destLen; pos += 64) {
    uint16_t alwaysMask = 0;
    uint16_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;
    uint16_t b4 = 0, b5 = 0, b6 = 0, b7 = 0;

    for (int ch = 0; ch < maxChannel; ch++) {
      if (!_channels[ch].active) continue;
      if (_channels[ch].srcPos >= _channels[ch].srcLen) continue;
      const uint16_t m = (uint16_t)(1u << ch);
      alwaysMask |= m;
      const uint8_t b = _channels[ch].srcData[_channels[ch].srcPos++];
      b0 |= m & (uint16_t)(0u - ((b >> 7) & 1u));
      b1 |= m & (uint16_t)(0u - ((b >> 6) & 1u));
      b2 |= m & (uint16_t)(0u - ((b >> 5) & 1u));
      b3 |= m & (uint16_t)(0u - ((b >> 4) & 1u));
      b4 |= m & (uint16_t)(0u - ((b >> 3) & 1u));
      b5 |= m & (uint16_t)(0u - ((b >> 2) & 1u));
      b6 |= m & (uint16_t)(0u - ((b >> 1) & 1u));
      b7 |= m & (uint16_t)(0u - ((b >> 0) & 1u));
    }
    if (!alwaysMask) break;

    // S3 LCD: no byte swapping.
    uint32_t* p = (uint32_t*)(dest + pos);
    #define EMIT(bN, OFF) \
      p[OFF]   = ((uint32_t)(bN) << 16) | alwaysMask; \
      p[OFF+1] = (uint32_t)(bN);
    EMIT(b0, 0)  EMIT(b1, 2)  EMIT(b2, 4)  EMIT(b3, 6)
    EMIT(b4, 8)  EMIT(b5, 10) EMIT(b6, 12) EMIT(b7, 14)
    #undef EMIT
  }
}

#else // WLED_PIXELBUS_16PARALLEL

// 8-bit parallel encode: branchless gather + scatter, 32 bytes per source byte (8 channels)
void IRAM_ATTR LcdBusContext::encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel) {
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

    uint32_t* p = (uint32_t*)(dest + pos);
    #define EMIT8(bN, OFF) \
      p[OFF] = (uint32_t)(alwaysMask) | ((uint32_t)(bN) << 8) | ((uint32_t)(bN) << 16);
    EMIT8(b0, 0)  EMIT8(b1, 1)  EMIT8(b2, 2)  EMIT8(b3, 3)
    EMIT8(b4, 4)  EMIT8(b5, 5)  EMIT8(b6, 6)  EMIT8(b7, 7)
    #undef EMIT8
  }
}

#endif // WLED_PIXELBUS_16PARALLEL

void IRAM_ATTR LcdBusContext::fillBuffer(uint8_t bufIdx) {
  memset(_dmaBuffer[bufIdx], 0, _bufferSize);

  if (_resetBytesLeft > 0) {
    _dmaDesc[bufIdx]->dw0.length = _resetBytesLeft;
    _dmaDesc[bufIdx]->next = nullptr; // stop the engine after this one
    _resetBytesLeft = 3; // flag end of frame, don't queue any more buffers
    return;
  }

  uint32_t bytesToEncode = 0;
  uint8_t maxCh = 0;
  for (int ch = 0; ch < WLEDPB_LCD_MAX_CHANNELS; ch++) {
    if (_channels[ch].active) {
      maxCh = ch + 1;
      uint32_t channelBytesLeft = _channels[ch].srcLen - _channels[ch].srcPos;
      if (channelBytesLeft > bytesToEncode) bytesToEncode = channelBytesLeft;
    }
  }

  size_t dmaBytesPerSrc = _use16Bit ? 64 : 32;
  uint32_t translatedbytes = bytesToEncode * dmaBytesPerSrc;
  translatedbytes = translatedbytes > _bufferSize ? _bufferSize : translatedbytes;
  encode4Step(_dmaBuffer[bufIdx], translatedbytes, maxCh);

  if (translatedbytes < _bufferSize) {
    // Data ran out before the buffer was full (i.e. we are done), compute the minimum reset period we must send as zero cycles
    uint32_t resetNs = _timing.reset_us * 1000;
    uint32_t bitPeriodNs = _timing.bitPeriod() + 1; // +1 to ensure no division by zero and slightly over-estimate the reset cycle
    uint32_t zeroCycles = resetNs / bitPeriodNs;
    size_t resetBytes = zeroCycles * (dmaBytesPerSrc / 8); // one cycle is 4 clocks, on each clock one/two buffer byte(s) sent out in parallel

    size_t newLen = translatedbytes + resetBytes;
    if (newLen > _bufferSize) {
      _resetBytesLeft = newLen - _bufferSize; // reset pulse does not fit into this buffer frame, send another one
      _dmaDesc[bufIdx]->dw0.length = _bufferSize;
    } else {
      _dmaDesc[bufIdx]->dw0.length = newLen; // send the rest (zeroes) as a reset
      _resetBytesLeft = 3; // flag end of frame, don't queue any more buffers or it will mess up the DMA
      _dmaDesc[bufIdx]->next = nullptr;  // reset fit into this buffer, end transfer after this is sent
    }
  }
}

bool LcdBusContext::startTransmit() {
  if (_state != DriverState::Idle) return false;
  if (_channelCount == 0) return false;

  // Only start transmission if ALL active channels have populated data
  if (_stagedMask != _channelMask) return true;
  _stagedMask = 0; // Reset for next frame

  _maxDataLen = 0;
  for (int ch = 0; ch < WLEDPB_LCD_MAX_CHANNELS; ch++) {
    if (_channels[ch].active) {
      _channels[ch].srcPos = 0;
      if (_channels[ch].srcLen > _maxDataLen) {
        _maxDataLen = _channels[ch].srcLen;
      }
    }
  }
  if (_maxDataLen == 0) return false;

  _resetBytesLeft = 0;

  // allocate or reallocate DMA buffers if needed — deferred from init() so all channels
  // can register first and _maxSrcBytes reflects the bus with the most pixels
  if (!_dmaAllocated) {
    if (!_allocDmaBuffers()) return false;
  }

  // Fill all buffers initially and restore circular chain
  for (int i = 0; i < WLEDPB_LCD_DMA_BUFFER_COUNT; i++) {
    _dmaDesc[i]->next = _dmaDesc[(i + 1) % WLEDPB_LCD_DMA_BUFFER_COUNT]; // restore circular buffer chain
    _dmaDesc[i]->dw0.length = _bufferSize;
    fillBuffer(i);
    _dmaDesc[i]->dw0.suc_eof = 1;    // enable eof, just in case
    _dmaDesc[i]->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;  // hand ownership over to DMA after descriptor init
  }

  _activeBuffer = 0; // start with first buffer
  _state = DriverState::Sending;

  // Start DMA (circular mode - descriptors already linked)
  gdma_reset(_dmaChannel);

  LCD_CAM.lcd_user.lcd_dout = 1;
  LCD_CAM.lcd_user.lcd_update = 1;
  LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
  LCD_CAM.lcd_misc.lcd_afifo_reset = 0;

  gdma_start(_dmaChannel, (intptr_t)_dmaDesc[0]);
  esp_rom_delay_us(1);
  LCD_CAM.lcd_user.lcd_start = 1;

  return true;
}

IRAM_ATTR bool LcdBusContext::dmaCallback(gdma_channel_handle_t dma_chan,
                      gdma_event_data_t* event_data,
                      void* user_data) {
  LcdBusContext* ctx = (LcdBusContext*)user_data;

  // The completed buffer just finished playing; DMA is now on the next buffer
  uint8_t completedBuf = ctx->_activeBuffer;
  ctx->_activeBuffer = (ctx->_activeBuffer + 1) % WLEDPB_LCD_DMA_BUFFER_COUNT;

  if (ctx->_dmaDesc[completedBuf]->next == nullptr) { // this was the last buffer
    LCD_CAM.lcd_user.lcd_start = 0;
    gdma_stop(ctx->_dmaChannel);
    ctx->_state = DriverState::Idle;
    return false;
  }

  if (ctx->_resetBytesLeft != 3) {
    ctx->fillBuffer(completedBuf); // fill buffer, handle reset pulse and end of frame
    ctx->_dmaDesc[completedBuf]->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA; // owner is reset upon eof and must be handed back to DMA
  }

  return false; // Do not yield OS for this DMA streaming interrupt
}

// ============================================
// LcdBus Implementation
// ============================================

LcdBus::LcdBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, size_t numPixels, bool use16Bit, uint8_t ledType)
  : _pin(pin)
  , _use16Bit(use16Bit)
  , _timing(timing)
  , _initialized(false)
  , _channelIdx(-1)
  , _ctx(nullptr)
{
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
}

LcdBus::~LcdBus() {
  end();
}

bool LcdBus::begin() {
  if (_initialized) return true;

  _ctx = LcdBusContext::get();
  if (!_ctx) return false;

  if (!_ctx->init(_timing, _use16Bit)) {
    LcdBusContext::release();
    _ctx = nullptr;
    return false;
  }

  // pass our encoded byte count so the context can size DMA buffers for the largest bus
  const size_t srcBytes = (size_t)_numPixels * _encoder.getPixelBytes();
  _channelIdx = _ctx->registerChannel(_pin, this, srcBytes, _inverted);
  if (_channelIdx < 0) {
    LcdBusContext::release();
    _ctx = nullptr;
    return false;
  }

  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getPixelBytes())) {
    end();
    return false;
  }
  LCD_LOG("LcdBus: ch=%d pin=%d", _channelIdx, _pin);
  return true;
}

// invert output signal, must be set before begin()
void LcdBus::setInverted(bool inv) {
  _inverted = inv;
}

void LcdBus::end() {
  if (!_initialized) return;

  if (_ctx) {
    // Wait for any active transmission to complete before cleanup
    while (!_ctx->isIdle()) vTaskDelay(1);
    _ctx->unregisterChannel(_channelIdx);
    LcdBusContext::release();
    _ctx = nullptr;
  }

  if (_encodeBuffer) {
    heap_caps_free(_encodeBuffer);
    _encodeBuffer = nullptr;
    _encodeBufferSize = 0;
  }

  _initialized = false;
}

bool LcdBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_ctx || !_encodeBuffer || _numPixels == 0) return false;

  // Wait for previous transmission to complete
  uint32_t start = millis();
  while (!_ctx->isIdle()) {
    if (millis() - start > 1000) {
      _ctx->forceIdle();
      break;
    }
    yield();
  }

  // Send already-encoded buffer directly
  _ctx->setChannelData(_channelIdx, _encodeBuffer, _encodeBufferSize);
  return _ctx->startTransmit();
}

bool LcdBus::canShow() const {
  if (!_ctx) return true;
  return _ctx->isIdle();
}

void LcdBus::setColorOrder(uint8_t co) {
  _encoder = ColorEncoder(co, _encoder.getColorChannels(), _ledType);
}

} // namespace WLEDpixelBus
#endif