/*-------------------------------------------------------------------------

WLEDpixelBus - parallel SPI output driver implementation

written by Damian Schneider @dedehai 2026

supports ESP32 C3, other chips may work but are untested
uses 4 parallel outputs and double DMA buffering on SPI2
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
#include "esp_rom_gpio.h"
#include "esp_private/gdma.h"
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
static constexpr uint32_t SPI_MIN_CLOCK_HZ = 2000000;
static constexpr uint32_t SPI_MAX_CLOCK_HZ = 5000000;

// Maximum bits per SPI user transfer: 18-bit length register SPI_MS_DATA_BITLEN=262143, we need 16bits per source bit (4x parallel, 4 steps) or 128bits per byte
static constexpr uint32_t SPI_MAX_BITS = (2047 * 128); // 262016 is the max bits that fit in a single SPI transfer (full encoded bytes), multiple transfers can be chained

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
  , _peripheralsEnabled(false)
  , _activeBuffer(0)
  , _gdmaChan(nullptr)
  , _dmaChan(-1)
  , _spiIsrHandle(nullptr)
  , _hw(&GPSPI2)
  , _channelCount(0)
  , _framePos(0)
  , _numBytes(0)
  , _bitsLeft(0)
  , _resetBits(0)
  , _lastTransmitMs(0)
  , _stagedMask(0)
  , _channelMask(0)
{
  _isrMux = portMUX_INITIALIZER_UNLOCKED;
  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    _dmaBuffer[i] = nullptr;
  }
  for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
    _channels[i] = {nullptr, 0, -1, false, false};
  }
}

SpiBusContext::~SpiBusContext() {
  deinit();
}

bool SpiBusContext::isIdle() const {
  if (_state == SpiState::Idle) return true;

  if (_hw->cmd.usr == 0) {
    // give the SPI hardware up to 30us to update its status: hardware clears cmd.usr, ISR "immediately" sets it again (if the gap is longer, LEDs may have latched)
    const uint32_t waitStart = micros();
    while ((uint32_t)(micros() - waitStart) < 30) {
      if (_hw->cmd.usr != 0) return false;       // chained segment restarted, SPI is still running
      if (_state == SpiState::Idle) return true; // trans_done ISR completed normally
    }
    // SPI genuinely stopped without completing: trans_done ISR was lost.
    forceIdle();
    return true;
  }

  // timeout in case the state-machine breaks due to missed ISR (should not happen, this is a safety net)
  if ((uint32_t)(millis() - _lastTransmitMs) > SPI_TRANSFER_TIMEOUT_MS) {
    forceIdle();
    return true;
  }

  return false;
}

// Recovery path for error conditions, cleanly stops DMA, SPI, and disconnects pins to prevent glitches
void SpiBusContext::forceIdle() const {
  portENTER_CRITICAL(&_isrMux); // make sure no ISR will disturb the sequence
  // disconnect pins from SPI and set to idle level
  for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
    if (_channels[i].active && _channels[i].pin >= 0) {
      esp_rom_gpio_connect_out_signal(_channels[i].pin, SIG_GPIO_OUT_IDX, false, false);
      gpio_set_level((gpio_num_t)_channels[i].pin, _channels[i].inverted ? 1 : 0);
    }
  }
  if (_peripheralsEnabled) {
    _hw->cmd.usr = 0;
    _hw->dma_int_ena.val = 0; // disable all SPI interrupts
    _hw->dma_int_clr.val = 0xFFFFFFFF;

    // Reset FIFOs after stopping the SPI user transfer.
    spi_ll_dma_tx_fifo_reset(_hw);
    spi_ll_outfifo_empty_clr(_hw);
  }

  if (_dmaChan >= 0) {
    gdma_dev_t* dma = &GDMA;
    dma->intr[_dmaChan].ena.out_eof = 0;
    gdma_ll_tx_reset_channel(dma, _dmaChan);
  }

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
    return; // all pixel data has been encoded, nothing to do, just send out zeroed buffer as idle state
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
  // Both paths are synchronized with gdmaISR via _isrMux.
  SpiBusContext* ctx = (SpiBusContext*)arg;
  uint32_t status = ctx->_hw->dma_int_st.val;
  ctx->_hw->dma_int_clr.val = status; // Clear all flags immediately

  if (status & SPI_DMA_OUTFIFO_EMPTY_ERR_INT_ST) {
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
    dma->intr[ctx->_dmaChan].ena.out_eof = 0;
    gdma_ll_tx_reset_channel(dma, ctx->_dmaChan);
    spi_ll_dma_tx_fifo_reset(ctx->_hw);
    spi_ll_outfifo_empty_clr(ctx->_hw);
    ctx->_stagedMask = 0;
    ctx->_state = SpiState::Idle;             // recovered: next show() can send immediately
    portEXIT_CRITICAL_ISR(&ctx->_isrMux);
    return;
  }

  if (status & SPI_TRANS_DONE_INT_ST) {
    if (ctx->_bitsLeft > 0) {
      // Chain the next segment: the circular DMA never stopped, re-arm the SPI transfer
      uint32_t bits = (ctx->_bitsLeft > (int32_t)SPI_MAX_BITS) ? SPI_MAX_BITS : (uint32_t)ctx->_bitsLeft;
      ctx->_bitsLeft -= (int32_t)bits;
      spi_ll_set_mosi_bitlen(ctx->_hw, bits);
      spi_ll_apply_config(ctx->_hw); // fast handshake, sub-microsecond
      spi_ll_user_start(ctx->_hw);   // clock resumes ~1-2us after the last bit
      // state stays Sending; encode/DMA are unaffected by segment boundaries
    } else {
      ctx->_state = SpiState::Idle; // last segment finished (includes the reset tail)
    }
  }
}

bool IRAM_ATTR SpiBusContext::gdmaISR(gdma_channel_handle_t dma_chan, gdma_event_data_t* event_data, void* user_data) {
  SpiBusContext* ctx = (SpiBusContext*)user_data;
  gdma_dev_t* dma = &GDMA;
  portENTER_CRITICAL_ISR(&ctx->_isrMux); // make sure we are not disturbed filling the buffer to prevent underruns
  dma->intr[ctx->_dmaChan].clr.out_eof = 1;  // clear interrupt immediately, harmless if driver already cleared it

  // If we're idle, ignore spurious interrupts
  if (ctx->_state == SpiState::Idle) {
    portEXIT_CRITICAL_ISR(&ctx->_isrMux);
    return false;
  }

  uint8_t completedBuf = ctx->_activeBuffer;
  ctx->_activeBuffer = (completedBuf + 1) % WLEDPB_SPI_DMA_DESC_COUNT;
  ctx->encodeSpiChunk(completedBuf); // fill the completed buffer with next chunk of data (or zeroes for reset)
  ctx->_dmaDesc[completedBuf].eof = 1; // Give ownership of the descriptor back to DMA so it can keep feeding SPI  TODO: this may be unnecessary
  ctx->_dmaDesc[completedBuf].owner = 1;
  portEXIT_CRITICAL_ISR(&ctx->_isrMux);
  return false; // no higher-priority task woken
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

  // Enable peripheral clocks and force-reset SPI2.
  // periph_module_enable() uses ref counting and may be a no-op if the
  // peripheral was already enabled. Explicit reset ensures clean state.
  periph_module_enable(PERIPH_SPI2_MODULE);
  periph_module_reset(PERIPH_SPI2_MODULE);
  periph_module_enable(PERIPH_GDMA_MODULE); // GDMA may be shared with other drivers so do not reset. If we call enable, we are also allowed to call disable (calls stack)
  _peripheralsEnabled = true;

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

  // Clock: 4 steps per bit → 4 SPI clock cycles per bit period.
  // targetFreq = 4 / (bitPeriod_ns * 1e-9) = 4,000,000,000 / bitPeriod_ns
  uint32_t targetFreq = calc4StepClockHz(timing);
  if (targetFreq < SPI_MIN_CLOCK_HZ) targetFreq = SPI_MIN_CLOCK_HZ;
  if (targetFreq > SPI_MAX_CLOCK_HZ) targetFreq = SPI_MAX_CLOCK_HZ;

  spi_ll_master_set_clock(_hw, 80000000, targetFreq, 128);

  // reset pulse: zeroes appended after the last data bit. 2 DMA bytes per 4-step bit, transfer length counts 8 bits per DMA byte.
  _resetBits = (uint32_t)calc4StepResetDmaBytes(timing, 2) * 8;

  // Route SPI clock to a dummy pin (needed for DMA to work) -> seems to work fine without this (maybe an IDF V5 issue?)
  //pinMatrixOutAttach(11, FSPICLK_OUT_IDX, false, false);

  //  spi_ll_set_mosi_bitlen(_hw, 16384); // dummy init value, not required (set properly when transfer starts)
  spi_ll_enable_mosi(_hw, true);

  spi_ll_dma_tx_enable(_hw, true);
  spi_ll_dma_tx_fifo_reset(_hw);
  spi_ll_outfifo_empty_clr(_hw);
  spi_ll_apply_config(_hw);

  // Configure GDMA
  gdma_channel_alloc_config_t allocCfg = {};
  allocCfg.direction = GDMA_CHANNEL_DIRECTION_TX;
  esp_err_t err = gdma_new_ahb_channel(&allocCfg, &_gdmaChan); // get a DMA channel
  if (err != ESP_OK) {
    deinit();
    return false; // no free TX channel -> clean failure instead of silent corruption
  }
  gdma_get_channel_id(_gdmaChan, &_dmaChan);

  gdma_dev_t* dma = &GDMA;
  gdma_ll_tx_reset_channel(dma, _dmaChan);

  err = gdma_connect(_gdmaChan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_SPI, 2));
  if (err != ESP_OK) { deinit(); return false; }

  gdma_ll_tx_set_desc_addr(dma, _dmaChan, (uint32_t)&_dmaDesc[0]);

  gdma_tx_event_callbacks_t cbs = {};
  cbs.on_trans_eof = gdmaISR; // signature changes, see below
  err = gdma_register_tx_event_callbacks(_gdmaChan, &cbs, this);
  if (err != ESP_OK) {
    //Serial.printf("[SPI] GDMA ISR alloc failed: %d\n", err);
    deinit();
    return false;
  }
  gdma_ll_tx_reset_channel(dma, _dmaChan);
  gdma_ll_tx_set_desc_addr(dma, _dmaChan, (uint32_t)&_dmaDesc[0]);
  //  gdma_ll_tx_start(dma, _dmaChan); // note: do not start yet, done in startTransmit()

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

  if (_gdmaChan) {
    gdma_del_channel(_gdmaChan); // also tears down the callback/interrupt it installed
    _gdmaChan = nullptr;
  }
  _dmaChan = -1;

  if (_spiIsrHandle) {
    esp_intr_free(_spiIsrHandle);
    _spiIsrHandle = nullptr;
  }

  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    if (_dmaBuffer[i]) {
      heap_caps_free(_dmaBuffer[i]);
      _dmaBuffer[i] = nullptr;
    }
  }

  if (_peripheralsEnabled) {
    periph_module_disable(PERIPH_SPI2_MODULE);
    // do not call disable on the GDMA, we use a legacy driver here, it may interfere with the modern gdma handling
    _peripheralsEnabled = false;
  }
  _initialized = false;
}

int8_t SpiBusContext::registerChannel(int8_t pin, bool inverted) {
  int8_t idx = -1;
  for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
    if (!_channels[i].active) {
      idx = i;
      break;
    }
  }
  if (idx < 0) return -1;

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

  _channels[channelIdx] = {nullptr, 0, -1, false, false};
  _channelCount--;
  _channelMask &= ~(1 << channelIdx);
}

void SpiBusContext::setChannelData(int8_t channelIdx, const uint8_t* data, size_t len) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_SPI_MAX_CHANNELS) return;
  _channels[channelIdx].srcData = data;
  _channels[channelIdx].srcLen = len;

  // Safety: If this channel was already staged, it means we somehow missed triggering startTransmit()
  if (_stagedMask & (1 << channelIdx)) {
    _stagedMask = 0;
  }

  // Mark this channel as staged
  _stagedMask |= (1 << channelIdx);
}

bool SpiBusContext::startTransmit() {
  if (_state != SpiState::Idle) return false; // must be idle to start a new frame, skip frame
  if (_channelCount == 0) return false;

  // Only start transmission once ALL active channels have populated data
  if (_stagedMask != _channelMask) return true; // report success while other buses are still being staged
  _stagedMask = 0; // Reset for next frame

  // Calculate actual data length from staged channels
  size_t newBytes = 0;
  for (int ch = 0; ch < WLEDPB_SPI_MAX_CHANNELS; ch++) {
    if (_channels[ch].active && _channels[ch].srcLen > newBytes) {
      newBytes = _channels[ch].srcLen;
    }
  }
  _numBytes = newBytes;

  // Total bits: 16 DMA bytes per source byte * 8 bits/byte = 128 bits per source byte plus reset: extra zero bits at the end (in the last segment).
  // Note: frames longer than one transfer of SPI_MAX_BITS are chained in the trans_done ISR into multiple SPI user transfers
  uint32_t dataBits = _numBytes * 16 * 8;
  uint32_t totalBits = dataBits + _resetBits;

  uint32_t firstBits = (totalBits > SPI_MAX_BITS) ? SPI_MAX_BITS : totalBits;
  _bitsLeft = (int32_t)(totalBits - firstBits); // remaining bits are chained by the ISR

  // wait for SPI hardware to finish the last transfer (this is another hardware guard for unexpected race conditions, should never happen)
  uint32_t timeout = 10;
  while (_hw->cmd.usr && timeout--) delay(1);
  if (_hw->cmd.usr) forceIdle(); // SPI is still busy after timeout. Something went horribly wrong. Force it idle.

  // init hardware, must not be interrupted, otherwise it breaks for some reason
  portENTER_CRITICAL(&_isrMux);
  _hw->cmd.usr = 0;
  gdma_dev_t* dma = &GDMA;
  dma->intr[_dmaChan].ena.out_eof = 0;
  gdma_ll_tx_reset_channel(dma, _dmaChan);

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
  gdma_ll_tx_set_desc_addr(dma, _dmaChan, (uint32_t)&_dmaDesc[0]);
  gdma_ll_tx_start(dma, _dmaChan);
  dma->intr[_dmaChan].clr.out_eof = 1;
  dma->intr[_dmaChan].ena.out_eof = 1;

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

  _channelIdx = _ctx->registerChannel(_pin, _inverted);
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

void ParallelSpiBus::end() {
  if (!_initialized) return;

  if (_ctx) {
    while (!_ctx->isIdle()) vTaskDelay(1);
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

bool ParallelSpiBus::show() {
  if (!_initialized || !_ctx || !_encodeBuffer) return false;

  // Wait for previous transmission to complete; isIdle() recovers stalled transfers.
  while (!_ctx->isIdle()) vTaskDelay(1);

  _ctx->setChannelData(_channelIdx, _encodeBuffer, _encodeBufferSize);
  return _ctx->startTransmit();
}

bool ParallelSpiBus::canShow() const {
  if (!_ctx) return true;
  return _ctx->isIdle();
}

} // namespace WLEDpixelBus
#endif // WLEDPB_PARALLEL_SPI_SUPPORT