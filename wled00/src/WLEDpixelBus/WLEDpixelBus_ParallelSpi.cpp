/*-------------------------------------------------------------------------

WLEDpixelBus - parallel SPI output driver implementation

written by Damian Schneider @dedehai 2026

supports ESP32 C3
uses 4 parallel outputs and double DMA buffering
Data is output in 4-step cadence meaning each LED bit is encoded into 4 bits. '0' is 0b1000 and '1' is 0b1110
Encoding is highly optimized for speed as encoding is done "on the fly" while the other buffer is being sent out using DMA.
The RAM usage of the sendout buffer is number of LEDs * bytes per LED + DMA buffer size
2k per DMA buffer works well, enough for 42 RGB LEDs or roughly 1.2ms between buffer swaps
Each bus can have individual configuration of color channels but all must share the same timing

-------------------------------------------------------------------------*/

#include "WLEDpixelBus.h"
#ifdef WLEDPB_PARALLEL_SPI_SUPPORT
#include "WLEDpixelBus_ParallelSpi.h"

#undef FLAG_ATTR
#define FLAG_ATTR(TYPE)
#include "hal/spi_ll.h"
#include "driver/periph_ctrl.h"
#include "esp_private/gdma.h"
#include "esp_rom_gpio.h"
namespace WLEDpixelBus {

//=============================================
// SPI Parallel Bus Implementation (ESP32-C3)
//============================================


#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
// low level functions not available in IDF V4
static inline void IRAM_ATTR spi_ll_apply_config(spi_dev_t *hw) {
  hw->cmd.update = 1;
  while (hw->cmd.update);    //waiting config applied
}

static inline void IRAM_ATTR spi_ll_user_start(spi_dev_t *hw) {
  hw->cmd.usr = 1;
}
#define GPIO_HW &GPIO
#define gpio_ll_set_func_sel(pin, sig) GPIO_HW->func_out_sel_cfg[pin].func_sel = sig
#else
#include "hal/gpio_ll.h" // need low level GPIO register access to disconnect pin inside the ISR
#define GPIO_HW GPIO_LL_GET_HW(0)
#define gpio_ll_set_func_sel(pin, sig) GPIO_HW->func_out_sel_cfg[pin].out_sel = SIG_GPIO_OUT_IDX;
#endif



// Pin assignments for SPI2 quad mode on C3
// SPI2 signals: FSPID (MOSI/D0), FSPIQ (MISO/D1), FSPIWP (D2), FSPIHD (D3)
static const int SPI_SIGNAL_INDICES[] = { FSPID_OUT_IDX, FSPIQ_OUT_IDX, FSPIWP_OUT_IDX, FSPIHD_OUT_IDX };

// Encoding patterns for SPI quad mode (4-step cadence, LSB first)
// Each lane is one bit position in a nibble, one byte = two clock cycles, 2 bytes = one 4-step bit
static constexpr uint16_t SPI_ZERO_BIT = 0x0001;  // output: [1,0,0,0] = 25% high (0000 0000 0000 0001 in binary, output LSB first)
static constexpr uint16_t SPI_ONE_BIT  = 0x0111;  // output: [1,1,1,0] = 75% high (0000 0001 0001 0001 in binary, output LSB first)

// Reset pulse: ~300us at ~2.6MHz (4-step cadence).
// 300us * 2.6MHz = 780 bits. We use 1024 bits (~400us) to be safe for all LED types. TODO: is this really safe for all led types?
static constexpr uint32_t SPI_RESET_BITS = 1024;

// Maximum bits per SPI user transfer (18-bit length register on C3 SPI_MS_DLEN_REG)
static constexpr uint32_t SPI_MAX_BITS = 262143; // note: 4x parallel, 4 steps -> 16bits per source bit, 2kbyte or 680 RGB LEDs max (tested, confirmed)
// Chained segment size in whole source bytes (128 SPI bits each), just under SPI_MAX_BITS.
// Longer frames are sent as multiple back-to-back transfers, chained in the trans_done ISR.
// The DMA stream is not affected by segment boundaries - only the SPI bit length is.
static constexpr uint32_t SPI_SEG_BITS = 2047 * 128; // 262016 bits = 2047 source bytes
// Maximum chained segments per frame (2 segments ~= 1364 RGB LEDs)
#define WLEDPB_SPI_MAX_SEGMENTS 2

SpiBusContext* SpiBusContext::_instance = nullptr;
uint8_t SpiBusContext::_refCount = 0;

SpiBusContext* SpiBusContext::get() {
  if (_instance == nullptr) {
    _instance = new SpiBusContext();
  }
  _refCount++;
  return _instance;
}

void SpiBusContext::release() {
  if (_refCount == 0) return;
  _refCount--;
  if (_refCount == 0 && _instance) {
    delete _instance;
    _instance = nullptr;
  }
}

SpiBusContext::SpiBusContext()
  : _state(SpiState::Idle)
  , _initialized(false)
  , _activeBuffer(0)
  , _gdmaIsrHandle(nullptr)
  , _spiIsrHandle(nullptr)
  , _hw(&GPSPI2)
  , _channelCount(0)
  , _framePos(0)
  , _numBytes(0)
  , _bitsLeft(0)
  , _lastTransmitMs(0)
  , _stagedMask(0)
  , _channelMask(0)
{
  _isrMux = portMUX_INITIALIZER_UNLOCKED;
  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    _dmaBuffer[i] = nullptr;
  }
  for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
    _channels[i] = {nullptr, nullptr, 0, -1, false, false};
  }
}

SpiBusContext::~SpiBusContext() {
  deinit();
}

bool SpiBusContext::isIdle() const {
  if (_state == SpiState::Idle) return true;

  // If we're in an error state, clean up the SPI state, then we are ready transmit again
  if (_state == SpiState::Error) {
    forceIdle();
    return true;
  }

  if (_hw->cmd.usr == 0) {
    for (volatile int i = 0; i < 200; i++) {
      if (_hw->cmd.usr != 0) return false;       // chained segment restarted, all good
      if (_state == SpiState::Idle) return true; // trans_done ISR completed normally
    }
    // SPI genuinely stopped without completing: trans_done ISR was lost.
    forceIdle();
    return true;
  }

  return false;
}

// Recovery path for error conditions, cleanly stops DMA, SPI, and disconnects pins to prevent glitches
void SpiBusContext::forceIdle() const {
  portENTER_CRITICAL(&_isrMux); // make sure no ISR will disturb the sequence
  // disconnect pins from SPI and set low
  for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
    if (_channels[i].active && _channels[i].pin >= 0) {
      esp_rom_gpio_connect_out_signal(_channels[i].pin, SIG_GPIO_OUT_IDX, false, false);
      gpio_set_level((gpio_num_t)_channels[i].pin, 0);
    }
  }
  if (_hw) {
    _hw->cmd.usr = 0;
    _hw->dma_int_ena.val = 0; // disable all SPI interrupts
    _hw->dma_int_clr.val = 0xFFFFFFFF;
  }

  // Stop DMA
  gdma_dev_t* dma = &GDMA;
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0;
  gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);

  // Reset FIFOs
  spi_ll_dma_tx_fifo_reset(_hw);
  spi_ll_outfifo_empty_clr(_hw);

  _state = SpiState::Idle;
  _stagedMask = 0;
  portEXIT_CRITICAL(&_isrMux);
}

//note: using O2 optimization has little to no effect on FPS
void IRAM_ATTR SpiBusContext::encodeSpiChunk(uint8_t bufIdx) {
  uint8_t* dst = _dmaBuffer[bufIdx];
  uint32_t* dst32 = reinterpret_cast<uint32_t*>(dst);
  for (size_t i = 0; i < (DEFAULT_DMA_BUFFER_SIZE / 4); i++) {
    dst32[i] = 0; // clear buffer (set all lanes low), DMA buffer is 4 bytes aligned. Note: memset is not IRAM safe and may crash
  }

  size_t maxSrcThisChunk = DEFAULT_DMA_BUFFER_SIZE / 16;  // 16 DMA bytes per source byte
  size_t srcBytesLeft = (_framePos < _numBytes) ? (_numBytes - _framePos) : 0;
  size_t srcThisChunk = (srcBytesLeft < maxSrcThisChunk) ? srcBytesLeft : maxSrcThisChunk;

  if (srcThisChunk == 0) {
    // All pixel data has been encoded. Transition to SendingLast state.
    // The next buffer fill will be zeroed (reset pulse).
    if (_state == SpiState::Sending) {
      _state = SpiState::SendingLast;
    }
    return;
  }

  for (uint8_t lane = 0; lane < WLEDPB_SPI_MAX_CHANNELS; lane++) {
    if (!_channels[lane].active || !_channels[lane].srcData) continue;

    size_t srcLen = _channels[lane].srcLen;
    if (_framePos >= srcLen) continue; // Past the end of this lane's data, leave buffer 0 (low/no pulse)

    size_t validBytes = srcLen - _framePos;
    if (validBytes > srcThisChunk) validBytes = srcThisChunk;

    const uint16_t zerobit = SPI_ZERO_BIT << lane;
    const uint16_t onebit  = SPI_ONE_BIT << lane;
    const uint8_t* src = _channels[lane].srcData;
    uint16_t* pOut = reinterpret_cast<uint16_t*>(dst);

    for (size_t i = 0; i < validBytes; i++) {
      uint8_t v = src[_framePos + i];
      *pOut++ |= (v & 0x80) ? onebit : zerobit;
      *pOut++ |= (v & 0x40) ? onebit : zerobit;
      *pOut++ |= (v & 0x20) ? onebit : zerobit;
      *pOut++ |= (v & 0x10) ? onebit : zerobit;
      *pOut++ |= (v & 0x08) ? onebit : zerobit;
      *pOut++ |= (v & 0x04) ? onebit : zerobit;
      *pOut++ |= (v & 0x02) ? onebit : zerobit;
      *pOut++ |= (v & 0x01) ? onebit : zerobit;
    }
  }
  _framePos += srcThisChunk;
}

// SPI ISR: handles trans_done (normal completion) and outfifo_empty_err
void IRAM_ATTR SpiBusContext::spiISR(void* arg) {
// (FIFO underrun recovery). Both paths are synchronized with gdmaISR via _isrMux.
  SpiBusContext* ctx = (SpiBusContext*)arg;
  uint32_t status = ctx->_hw->dma_int_st.val;
  ctx->_hw->dma_int_clr.val = status; // Clear all flags immediately
  if (status & SPI_TRANS_DONE_INT_ST) {
    if (ctx->_bitsLeft > 0) {
      // Chain the next segment. The circular DMA never stopped, so the TX FIFO already
      // holds the next bits: only re-arm the bit length and restart. Do NOT touch the
      // FIFO or DMA here - that would break bit-stream continuity and stall the restart.
      uint32_t bits = (ctx->_bitsLeft > (int32_t)SPI_SEG_BITS) ? SPI_SEG_BITS : (uint32_t)ctx->_bitsLeft;
      ctx->_bitsLeft -= (int32_t)bits;
      spi_ll_set_mosi_bitlen(ctx->_hw, bits);
      spi_ll_apply_config(ctx->_hw); // fast handshake, sub-microsecond
      spi_ll_user_start(ctx->_hw);   // clock resumes ~1-2us after the last bit
      // state stays Sending; encode/DMA are unaffected by segment boundaries
    } else {
      ctx->_state = SpiState::Idle; // last segment finished (includes the reset tail)
    }
  }
  else if (status & SPI_DMA_OUTFIFO_EMPTY_ERR_INT_ST) {
    if (ctx->_state == SpiState::Idle) return; // state machine finished cleanly, ignore
    // SPI FIFO starved (ISR latency too high). The frame is lost, abort and recover immediately
    portENTER_CRITICAL_ISR(&ctx->_isrMux); // note: on C3 this is not really needed as GDMA interrupt has the same priority, keep it just in case
    // disconnect pins from SPI to prevent garbage output (usr=0 outputs a fast clock)
    for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
      if (ctx->_channels[i].active && ctx->_channels[i].pin >= 0) {
        gpio_ll_set_func_sel(ctx->_channels[i].pin, SIG_GPIO_OUT_IDX); // disconnect from SPI using direct register write (ISR safe)
        // set the pin to static level immediately,  note: if implementing this for other ESPs: need to also set the high register for pins >31
        if (ctx->_channels[i].inverted)
          GPIO_HW->out_w1ts.out_w1ts = (1 << ctx->_channels[i].pin); // set ouput high (set) to avoid glitches
        else
          GPIO_HW->out_w1tc.out_w1tc = (1 << ctx->_channels[i].pin); // set ouput low (clear) to avoid glitches
      }
    }
    ctx->_hw->cmd.usr = 0;                    // stop SPI user transfer
    ctx->_hw->dma_int_ena.val = 0;            // startTransmit() re-arms these
    gdma_dev_t* dma = &GDMA;
    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0;
    gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);
    spi_ll_dma_tx_fifo_reset(ctx->_hw);
    spi_ll_outfifo_empty_clr(ctx->_hw);
    ctx->_stagedMask = 0;
    ctx->_state = SpiState::Idle;             // recovered: next show() can send immediately
    portEXIT_CRITICAL_ISR(&ctx->_isrMux);
  }
}

void IRAM_ATTR SpiBusContext::gdmaISR(void* arg) {
  SpiBusContext* ctx = (SpiBusContext*)arg;
  gdma_dev_t* dma = &GDMA;

  // Check if this is our interrupt
  if (!dma->intr[WLEDPB_SPI_GDMA_CHANNEL].st.out_eof) {
    return;
  }
  // make sure we are not disturbed filling the buffer to prevent underruns
  portENTER_CRITICAL_ISR(&ctx->_isrMux);
  // Clear interrupt immediately
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1;


  // If we're idle or in error, ignore spurious interrupts
  if (ctx->_state == SpiState::Idle || ctx->_state == SpiState::Error) {
    portEXIT_CRITICAL_ISR(&ctx->_isrMux);
    return;
  }

  uint8_t completedBuf = ctx->_activeBuffer;
  ctx->_activeBuffer = (completedBuf + 1) % WLEDPB_SPI_DMA_DESC_COUNT;

  // Fill the completed buffer with next chunk of data (or zeroes for reset)
  ctx->encodeSpiChunk(completedBuf);

  // Give ownership of the descriptor back to DMA so it can keep feeding SPI  TODO: this may be unnecessary
  ctx->_dmaDesc[completedBuf].eof = 1;
  ctx->_dmaDesc[completedBuf].owner = 1;
  portEXIT_CRITICAL_ISR(&ctx->_isrMux);
}

bool SpiBusContext::init(const LedTiming& timing) {
  if (_initialized) return true;

  // Allocate DMA buffers
  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    _dmaBuffer[i] = (uint8_t*)heap_caps_aligned_alloc(4, DEFAULT_DMA_BUFFER_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!_dmaBuffer[i]) {
      //Serial.printf("[SPI] DMA buffer %d alloc failed\n", i);
      deinit();
      return false;
    }
    memset(_dmaBuffer[i], 0, DEFAULT_DMA_BUFFER_SIZE);
  }

  // Setup DMA descriptors - circular linked list
  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    _dmaDesc[i].size = DEFAULT_DMA_BUFFER_SIZE;
    _dmaDesc[i].length = DEFAULT_DMA_BUFFER_SIZE;
    _dmaDesc[i].owner = 1;
    _dmaDesc[i].sosf = 0;
    _dmaDesc[i].eof = 1;
    _dmaDesc[i].buf = _dmaBuffer[i];
  }
  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    _dmaDesc[i].qe.stqe_next = &_dmaDesc[(i + 1) % WLEDPB_SPI_DMA_DESC_COUNT];
  }

  // Enable peripheral clocks and force-reset (SPI2 + DMA)
  // periph_module_enable() uses ref counting and may be a no-op if the
  // peripheral was already enabled. Explicit reset ensures clean state.
  periph_module_enable(PERIPH_SPI2_MODULE);
  periph_module_reset(PERIPH_SPI2_MODULE);
  periph_module_enable(PERIPH_GDMA_MODULE);
  periph_module_reset(PERIPH_GDMA_MODULE);

  // Configure SPI2 master
  spi_ll_master_init(_hw);
  spi_ll_master_set_mode(_hw, 0);
  spi_ll_set_tx_lsbfirst(_hw, true);

  // skip all SPI phases and jump directly to user mosi phase
  _hw->user.usr_command = 0;
  _hw->user.usr_addr = 0;
  _hw->user.usr_dummy = 0;
  _hw->user.usr_miso = 0;
  _hw->user.usr_mosi = 1;

  // Clear idle output polarities for D2/D3
  _hw->ctrl.q_pol = 0;
  _hw->ctrl.d_pol = 0;
  _hw->ctrl.hold_pol = 0;
  _hw->ctrl.wp_pol = 0;

  spi_line_mode_t linemode = {};
  linemode.data_lines = 4;  // quad mode
  spi_ll_master_set_line_mode(_hw, linemode);

  // Clock: target ~2.6MHz for ~390ns per step, matching user's tested config
  // 4 steps per bit → ~1560ns per bit (within WS2812 tolerance)
  // Clock: 4 steps per bit → 4 SPI clock cycles per bit period.
  // targetFreq = 4 / (bitPeriod_ns * 1e-9) = 4,000,000,000 / bitPeriod_ns
  uint32_t bitPeriodNs = timing.bitPeriod();
  uint32_t targetFreq = 4000000000UL / bitPeriodNs;
  if (targetFreq < 2000000) targetFreq = 2000000;
  if (targetFreq > 5000000) targetFreq = 5000000;

  spi_ll_master_set_clock(_hw, 80000000, targetFreq, 128);

  // Route SPI clock to a dummy pin (needed for DMA to work) -> seems to work fine without this (maybe an IDF V5 issue?)
  //pinMatrixOutAttach(11, FSPICLK_OUT_IDX, false, false);

  //  spi_ll_set_mosi_bitlen(_hw, 16384); // dummy init value, not required (set properly when transfer starts)
  spi_ll_enable_mosi(_hw, true);

  spi_ll_dma_tx_enable(_hw, true);
  spi_ll_dma_tx_fifo_reset(_hw);
  spi_ll_outfifo_empty_clr(_hw);
  spi_ll_apply_config(_hw);

  // Configure GDMA
  gdma_dev_t* dma = &GDMA;
  gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);
  gdma_ll_tx_connect_to_periph(dma, WLEDPB_SPI_GDMA_CHANNEL, GDMA_TRIG_PERIPH_SPI, SOC_GDMA_TRIG_PERIPH_SPI2);
  gdma_ll_tx_set_desc_addr(dma, WLEDPB_SPI_GDMA_CHANNEL, (uint32_t)&_dmaDesc[0]);
  //  gdma_ll_tx_start(dma, WLEDPB_SPI_GDMA_CHANNEL);

  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 1;
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1;

  esp_err_t err = esp_intr_alloc(WLEDPB_SPI_GDMA_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM, gdmaISR, this, &_gdmaIsrHandle); // note: saw no flickering even when using level1
  if (err != ESP_OK) {
    //Serial.printf("[SPI] GDMA ISR alloc failed: %d\n", err);
    deinit();
    return false;
  }

  // Install SPI ISR for trans_done and outfifo_empty_err recovery
  _hw->dma_int_clr.val = 0xFFFFFFFF;
  _hw->dma_int_ena.trans_done = 1;
  _hw->dma_int_ena.outfifo_empty_err = 1;
  // _hw->dma_int_ena.val = 0xFFFFFFFF; // REMOVED: Do not enable all interrupts, they trigger false aborts!
  err = esp_intr_alloc(ETS_SPI2_INTR_SOURCE, ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM, spiISR, this, &_spiIsrHandle);
  if (err != ESP_OK) {
    //Serial.printf("[SPI] SPI ISR alloc failed: %d\n", err);
    deinit();
    return false;
  }

  _initialized = true;
  return true;
}

void SpiBusContext::deinit() {
  // Ensure we're in a clean state before freeing resources
  forceIdle();

  // Stop SPI and DMA before freeing resources
  if (_hw) {
    _hw->cmd.usr = 0;  // Stop SPI transfer
  }

  gdma_dev_t* dma = &GDMA;
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0;  // Disable interrupt
  gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);

  if (_hw) {
    _hw->dma_int_ena.trans_done = 0;  // Disable SPI trans_done interrupt
  }

  if (_spiIsrHandle) {
    esp_intr_free(_spiIsrHandle);
    _spiIsrHandle = nullptr;
  }

  if (_gdmaIsrHandle) {
    esp_intr_free(_gdmaIsrHandle);
    _gdmaIsrHandle = nullptr;
  }

  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    if (_dmaBuffer[i]) {
      heap_caps_free(_dmaBuffer[i]);
      _dmaBuffer[i] = nullptr;
    }
  }

  periph_module_disable(PERIPH_SPI2_MODULE);
  periph_module_disable(PERIPH_GDMA_MODULE);
  _initialized = false;
}

int8_t SpiBusContext::registerChannel(int8_t pin, ParallelSpiBus* bus, bool inverted) {
  int8_t idx = -1;
  for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
    if (!_channels[i].active) {
      idx = i;
      break;
    }
  }
  if (idx < 0) return -1;

  _channels[idx].bus = bus;
  _channels[idx].pin = pin;
  _channels[idx].active = true;
  _channels[idx].inverted = inverted;
  _channelCount++;
  _channelMask |= (1 << idx);

  // Route SPI data signal to GPIO
  pinMode(pin, OUTPUT);
  esp_rom_gpio_connect_out_signal(pin, SPI_SIGNAL_INDICES[idx], inverted, false);

  return idx;
}

void SpiBusContext::unregisterChannel(int8_t channelIdx) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_SPI_MAX_CHANNELS) return;
  if (!_channels[channelIdx].active) return;

  if (_channels[channelIdx].pin >= 0) {
    gpio_reset_pin((gpio_num_t)_channels[channelIdx].pin);
  }

  _channels[channelIdx] = {nullptr, nullptr, 0, -1, false, false};
  _channelCount--;
  _channelMask &= ~(1 << channelIdx);
}

void SpiBusContext::setChannelData(int8_t channelIdx, const uint8_t* data, size_t len) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_SPI_MAX_CHANNELS) return;
  _channels[channelIdx].srcData = data;
  _channels[channelIdx].srcLen = len;
  // Mark this channel as staged
  _stagedMask |= (1 << channelIdx);
}

bool SpiBusContext::startTransmit() {
  if (_state != SpiState::Idle) return false; // must be idle to start a new frame, skip frame
  if (_channelCount == 0) return false;

  // Only start transmission if ALL active channels have populated data
  if (_stagedMask != _channelMask) return false; // not all channels staged, something went wrong, skip frame
  _stagedMask = 0; // Reset for next frame

  // Calculate actual data length from staged channels
  size_t newBytes = 0;
  for (int ch = 0; ch < WLEDPB_SPI_MAX_CHANNELS; ch++) {
    if (_channels[ch].active && _channels[ch].srcLen > newBytes) {
      newBytes = _channels[ch].srcLen;
    }
  }
  _numBytes = newBytes;
// clamp to chain capacity (tail pixels simply keep their previous values)
  const size_t maxBytes = 2047 * WLEDPB_SPI_MAX_SEGMENTS;
  if (_numBytes > maxBytes) _numBytes = maxBytes;

  // Total bits: 16 DMA bytes per source byte * 8 bits/byte = 128 bits per source byte
  // Plus reset: extra zero bits at the end (in the last segment).
  // Frames longer than one transfer are chained in the trans_done ISR.
  uint32_t dataBits = _numBytes * 16 * 8;
  uint32_t totalBits = dataBits + SPI_RESET_BITS;

  uint32_t firstBits = (totalBits > SPI_SEG_BITS) ? SPI_SEG_BITS : totalBits;
  _bitsLeft = (int32_t)(totalBits - firstBits); // remaining bits are chained by the ISR

  // Wait for SPI to be idle
  uint32_t timeout = 100;
  while (_hw->cmd.usr && timeout--) {
    delay(1);
  }
  if (_hw->cmd.usr) {
    forceIdle(); // SPI is still busy after timeout. Force it idle.
  }

  // init hardware, must not be interrupted, otherwise it breaks for some reason
  portENTER_CRITICAL(&_isrMux);
  _hw->cmd.usr = 0;
  gdma_dev_t* dma = &GDMA;
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0;
  gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);

  spi_ll_clear_int_stat(_hw);
  _hw->dma_int_ena.trans_done = 1; // re-enable interrupts in case they got disabled due to error
  _hw->dma_int_ena.outfifo_empty_err = 1;
  spi_ll_dma_tx_fifo_reset(_hw);
  spi_ll_outfifo_empty_clr(_hw);

  spi_ll_set_mosi_bitlen(_hw, firstBits);

  // Re-initialize DMA descriptors and encode initial buffers
  _framePos = 0;
  _activeBuffer = 0;
  _state = SpiState::Sending;

  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    // Restore circular linked list
    _dmaDesc[i].qe.stqe_next = &_dmaDesc[(i + 1) % WLEDPB_SPI_DMA_DESC_COUNT];
    _dmaDesc[i].size = DEFAULT_DMA_BUFFER_SIZE;
    _dmaDesc[i].length = DEFAULT_DMA_BUFFER_SIZE;
    _dmaDesc[i].owner = 1;
    _dmaDesc[i].eof = 1;
    encodeSpiChunk(i);
  }

  // Phase 4: Brief critical section to start DMA and SPI atomically.
  gdma_ll_tx_set_desc_addr(dma, WLEDPB_SPI_GDMA_CHANNEL, (uint32_t)&_dmaDesc[0]);
  gdma_ll_tx_start(dma, WLEDPB_SPI_GDMA_CHANNEL);
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1;
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 1;

  // Re-attach pins to SPI signals
  for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
    if (_channels[i].active && _channels[i].pin >= 0) {
      pinMatrixOutAttach(_channels[i].pin, SPI_SIGNAL_INDICES[i], _channels[i].inverted, false);
    }
  }
  spi_ll_dma_tx_enable(_hw, true);
  spi_ll_apply_config(_hw);  // apply SPI config AFTER starting DMA to make sure they are in sync

  // Short hardware handshake sync: adding a few nops is enough to ensure there is DMA data in the SPI buffer (without this, SPI can immediately quit its duty due to FIFO unterrun)
  //asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"); // might not be needed but just in case
  spi_ll_user_start(_hw); // start SPI user transfer
  _lastTransmitMs = millis();
  portEXIT_CRITICAL(&_isrMux);
  return true;
}
/*
void SpiBusContext::hwStopTransfer() {
  if (_hw) {
    _hw->cmd.usr = 0;
    _hw->dma_int_ena.val = 0; // Disable all SPI interrupts
  }
  gdma_dev_t* dma = &GDMA;
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0;
  gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);
}

void SpiBusContext::hwResetFifo() {
  if (_hw) {
    spi_ll_dma_tx_fifo_reset(_hw);
    spi_ll_outfifo_empty_clr(_hw);
  }
}
*/
///////////////////////////////////
// ParallelSpiBus implementation //
///////////////////////////////////
ParallelSpiBus::ParallelSpiBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType)
  : _pin(pin)
  , _timing(timing)
  , _initialized(false)
  , _channelIdx(-1)
  , _ctx(nullptr)
{
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
}

ParallelSpiBus::~ParallelSpiBus() {
  end();
}

bool ParallelSpiBus::begin() {
  if (_initialized) return true;

  _ctx = SpiBusContext::get();
  if (!_ctx) return false;

  if (!_ctx->init(_timing)) {
    SpiBusContext::release();
    _ctx = nullptr;
    return false;
  }

  _channelIdx = _ctx->registerChannel(_pin, this, _inverted);
  if (_channelIdx < 0) {
    //Serial.printf("[SPI] registerChannel failed for pin %d\n", _pin);
    SpiBusContext::release();
    _ctx = nullptr;
    return false;
  }

  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getPixelBytes())) { end(); return false; }
  return true;
}

// invert output signal, must be set before begin()
void ParallelSpiBus::setInverted(bool inv) {
  _inverted = inv;
}

void ParallelSpiBus::end() {
  if (!_initialized) return;

  if (_ctx) {
    uint32_t startWait = millis();
    while (!_ctx->isIdle()) {
      if (millis() - startWait > 200) {
        break; // Timeout: proceed with cleanup anyway
      }
      vTaskDelay(1);
    }
    _ctx->unregisterChannel(_channelIdx);
    SpiBusContext::release();
    _ctx = nullptr;
  }

  if (_encodeBuffer) {
    heap_caps_free(_encodeBuffer);
    _encodeBuffer = nullptr;
    _encodeBufferSize = 0;
  }

  _initialized = false;
}

bool ParallelSpiBus::allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) {
  const size_t pixelBytes = padPixelBytesForSuffix((size_t)numPixels * numChannels, _ledType);
  size_t needed = _prefixLen + pixelBytes + _suffixLen;
  if (_encodeBuffer && _encodeBufferSize >= needed) return true;
  if (_encodeBuffer) { heap_caps_free(_encodeBuffer); _encodeBuffer = nullptr; }
  if (needed == 0) return true;
  _encodeBuffer = (uint8_t*)heap_caps_malloc(needed, MALLOC_CAP_INTERNAL);
  if (!_encodeBuffer) { _encodeBufferSize = 0; return false; }
  memset(_encodeBuffer, 0, needed);
  _encodeBufferSize = needed;
  _pixelData  = _encodeBuffer + _prefixLen;
  if (_suffixLen == sizeof(SM16825_SUFFIX) && _ledType == TYPE_SM16825)
    memcpy(_pixelData + pixelBytes, SM16825_SUFFIX, sizeof(SM16825_SUFFIX));
  return true;
}

bool ParallelSpiBus::show() {
  if (!_initialized || !_ctx || !_encodeBuffer) return false;

  // Wait for previous transmission to complete with timeout (should not happen, BusManager already waits for canShow())
  uint32_t waitStart = millis();
  while (!_ctx->isIdle()) {
    if (millis() - waitStart > 200) {
      return false; // Timeout: don't start a new frame on a stuck driver
    }
    vTaskDelay(1);
  }

  _ctx->setChannelData(_channelIdx, _encodeBuffer, _encodeBufferSize);
  return _ctx->startTransmit();
}

bool ParallelSpiBus::canShow() const {
  if (!_ctx) return true;
  return _ctx->isIdle();
}

void ParallelSpiBus::setColorOrder(uint8_t co) {
  _encoder = ColorEncoder(co, _encoder.getColorChannels(), _ledType);
}

} // namespace WLEDpixelBus
#endif // WLEDPB_PARALLEL_SPI_SUPPORT