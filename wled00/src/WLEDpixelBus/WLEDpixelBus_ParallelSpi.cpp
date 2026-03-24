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

//==============================================================================
// SPI Parallel Bus Implementation (ESP32-C3)
//==============================================================================
volatile uint32_t spierror = 0;
volatile uint32_t encodecalls = 0;
volatile uint32_t dmacount = 0;

// low level functions available in IDF V5 (not available in IDF V4, this is for future proofint)
static inline void spi_ll_apply_config(spi_dev_t *hw)
{
  hw->cmd.update = 1;
  while (hw->cmd.update);    //waiting config applied
}

static inline void spi_ll_user_start(spi_dev_t *hw)
{
  hw->cmd.usr = 1;
}


// Pin assignments for SPI2 quad mode on C3
// SPI2 signals: FSPID (MOSI/D0), FSPIQ (MISO/D1), FSPIWP (D2), FSPIHD (D3)
static const int SPI_SIGNAL_INDICES[] = { FSPID_OUT_IDX, FSPIQ_OUT_IDX, FSPIWP_OUT_IDX, FSPIHD_OUT_IDX };

// Encoding patterns for SPI quad mode (4-step cadence, LSB first)
// Each lane is one bit position in a nibble, one byte = two clock cycles, 2 bytes = one 4-step bit
static constexpr uint16_t SPI_ZERO_BIT = 0x0001;  // output: [1,0,0,0] = 25% high (0000 0000 0000 0001 in binary, output LSB first)
static constexpr uint16_t SPI_ONE_BIT  = 0x0111;  // output: [1,1,1,0] = 75% high (0000 0001 0001 0001 in binary, output LSB first)

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
  : _txdone(true)
  , _initialized(false)
  //, _hasStarted(false)
  , _currentBuffer(0)
  , _gdmaIsrHandle(nullptr)
  , _spiIsrHandle(nullptr)
  , _hw(&GPSPI2)
  , _channelCount(0)
  , _framePos(0)
  , _numBytes(0)
  , _lastTransmitMs(0)
  , _stagedMask(0)
  , _channelMask(0)
{
  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    _dmaBuffer[i] = nullptr;
  }
  for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
    _channels[i] = {nullptr, -1, nullptr, 0, false};
  }
}

SpiBusContext::~SpiBusContext() {
  deinit();
}

//TODO: rename to isIdle()
bool SpiBusContext::isSpiDone() {
  if (!_txdone) {
    // safety timeout in case of SPI error
    if (millis() - _lastTransmitMs > 100) {
      _txdone = true;
      _hw->cmd.usr = 0;
      forceIdle();
      uint32_t timeout = 10;
      while (_hw->cmd.usr && timeout--) { // wait for SPI to actually stop (should be very fast, just in case)
        delay(1);
      }
      return true;
    }
    return false; // Still waiting for the cooldown
  }
  return true;
}

// Force SPI idle is a dirty hack to guard against some SPI error/race condition that is really hard to track down, it happens randomly and only if UI refresh plus RMT is involved
void SpiBusContext::forceIdle() {
  Serial.println("Timeout - Forcing SPI idle...");
  // print driver state for debugging
  Serial.printf("Channels: %d, FramePos: %d, NumBytes: %d, DMA Count: %d, Encode calls: %d\n", _channelCount, _framePos, _numBytes, dmacount, encodecalls);
  if(spierror) Serial.printf("SPI error: %d\n", spierror);
  _txdone = true;
  _stagedMask = 0;

  // disconnect pins from SPI and set low
  for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
    if (_channels[i].active && _channels[i].pin >= 0) {
      gpio_matrix_out(_channels[i].pin, SIG_GPIO_OUT_IDX, false, false);
      gpio_set_level((gpio_num_t)_channels[i].pin, 0);
    }
  }
  if (_hw) {
    _hw->cmd.usr = 0; // stop SPI user transfer (will output a fast clock, so detach pins from spi before this to avoid glitches)
    _hw->dma_int_clr.val = 0xFFFFFFFF; // clear all SPI interrupt flags
  }
  
  spi_ll_dma_tx_fifo_reset(_hw);
  spi_ll_outfifo_empty_clr(_hw);

  // Stop the DMA
  gdma_dev_t* dma = &GDMA;
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0; // disable interrupt
  gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL); // reset DMA channel
}



void IRAM_ATTR SpiBusContext::encodeSpiChunk(uint8_t bufIdx) {
encodecalls++;
  uint8_t* dst = _dmaBuffer[bufIdx];
  uint32_t* dst32 = reinterpret_cast<uint32_t*>(dst);
  for (size_t i = 0; i < (WLEDPB_SPI_DMA_BUFFER_SIZE / 4); i++) {
    dst32[i] = 0; // clear buffer (set all lanes low), DMA buffer is 4 bytes aligned
  }
  //memset(dst, 0x00, WLEDPB_SPI_DMA_BUFFER_SIZE); // TODO: memset is not ISR IRAM safe?
/*
  if (!_sending) {
    return;
  }*/

  size_t maxSrcThisChunk = WLEDPB_SPI_DMA_BUFFER_SIZE / 16;  // 16 DMA bytes per source byte
  size_t srcBytesLeft = (_framePos < _numBytes) ? (_numBytes - _framePos) : 0;
  size_t srcThisChunk = (srcBytesLeft < maxSrcThisChunk) ? srcBytesLeft : maxSrcThisChunk;

  if (srcThisChunk == 0) {
    //_framePos = 0;
    return;
  }
  dmacount++;

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

// SPI ISR: handles trans_done (restart SPI bit counter) and outfifo_empty_err
// (FIFO underrun recovery). outfifo_empty_err is temporarily disabled after
// handling to prevent infinite ISR loops; trans_done re-enables it.
void IRAM_ATTR SpiBusContext::spiISR(void* arg) {

  SpiBusContext* ctx = (SpiBusContext*)arg;
  uint32_t status = ctx->_hw->dma_int_st.val;
  spierror = status;

  if (status & SPI_TRANS_DONE_INT_ST) {
    ctx->_txdone = true; // transfer finished
    ctx->_hw->dma_int_clr.val = SPI_TRANS_DONE_INT_ST;  // clear flag
    spierror=0;
    //if(spierror)
    //  spierror++;
    //  digitalWrite(0, LOW);//!!!
      //Serial.print("b");
  }
  else {
  //  Serial.println(status, HEX); // -> prints "2" i.e. SPI_OUTFIFO_EMPTY_ERR_INT_ST
    ctx->_hw->dma_int_clr.val = status;  // Clear all SPI interrupt flags
    // SPI FIFO starved, immediately pull the plug or it will output garbage that causes glithces and white flashes
    for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++){
      if (ctx->_channels[i].active && ctx->_channels[i].pin >= 0) {
          GPIO.func_out_sel_cfg[ctx->_channels[i].pin].func_sel = SIG_GPIO_OUT_IDX; // disconnect from SPI using direct register write (ISR safe)
          GPIO.out_w1tc.out_w1tc = (1 << ctx->_channels[i].pin); // set ouput low (clear) to avoid glitches note: if implementing this for other ESPs: need to also set the high register for pins >31
      }
    }
    // now that pins are disconnected, we can stop the user transfer which is causing the fast clock output that leads to glitches
    ctx->_hw->cmd.usr = 0; // stop SPI user transfer TODO: will this result in a tx done or some other interrupt? -> txdone fires
  //  ctx->_sending = false;
    spierror++; // TODO: does not seem to happen ... ever - even if trans done is not firing.
    ctx->_txdone = true; // set to tx done, need to reset the spi when checking if spi idle
  }

//digitalWrite(0, HIGH); //!!!  note: setting pins inside ISRs can cause crashes

}

void IRAM_ATTR SpiBusContext::gdmaISR(void* arg) {
  SpiBusContext* ctx = (SpiBusContext*)arg;
  gdma_dev_t* dma = &GDMA;


  //  Serial.print("*");
  // toggle gpio0
  //    digitalWrite(0, HIGH);

  if (dma->intr[WLEDPB_SPI_GDMA_CHANNEL].st.out_eof) {
    //Clear interrupt immediately
    dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1;
    uint8_t completedBuf = ctx->_currentBuffer;
    ctx->encodeSpiChunk(completedBuf);

    // Give ownership of the descriptor back to DMA so it can keep feeding SPI
    ctx->_dmaDesc[completedBuf].eof = 1; // TODO: it works perfectly fine without setting these flags again, does this help with stability?
    ctx->_dmaDesc[completedBuf].owner = 1; // give ownership back to DMA: it works without this but just make sure its not passed back to CPU

    ctx->_currentBuffer = (completedBuf + 1) % WLEDPB_SPI_DMA_DESC_COUNT; // TODO: reduce again to 2 buffers once it is clear that two are enough (probably is)
  }
  //    digitalWrite(0, LOW);
  //dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.val = dma->intr[WLEDPB_SPI_GDMA_CHANNEL].raw.val; // clear all GDMA interrupt flags for this channel  
}

bool SpiBusContext::init(const LedTiming& timing) {
  if (_initialized) return true;

  //pinMode(0, OUTPUT);
  //digitalWrite(0, LOW); //!!!

  // Allocate DMA buffers
  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    _dmaBuffer[i] = (uint8_t*)heap_caps_aligned_alloc(4, WLEDPB_SPI_DMA_BUFFER_SIZE,
                               MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!_dmaBuffer[i]) {
      Serial.printf("[SPI] DMA buffer %d alloc failed\n", i);
      deinit();
      return false;
    }
    memset(_dmaBuffer[i], 0, WLEDPB_SPI_DMA_BUFFER_SIZE);
  }

  // Setup DMA descriptors - circular linked list
  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    _dmaDesc[i].size = WLEDPB_SPI_DMA_BUFFER_SIZE;
    _dmaDesc[i].length = WLEDPB_SPI_DMA_BUFFER_SIZE;
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

  esp_err_t err = esp_intr_alloc(WLEDPB_SPI_GDMA_INTR_SOURCE, ESP_INTR_FLAG_LEVEL2, gdmaISR, this, &_gdmaIsrHandle); // note: saw no flickering even when using level1
  if (err != ESP_OK) {
    Serial.printf("[SPI] GDMA ISR alloc failed: %d\n", err);
    deinit();
    return false;
  }

  // Install SPI ISR for trans_done and outfifo_empty_err recovery
  _hw->dma_int_clr.val = 0xFFFFFFFF;
  _hw->dma_int_ena.trans_done = 1;
  _hw->dma_int_ena.outfifo_empty_err = 1;
  // _hw->dma_int_ena.val = 0xFFFFFFFF; // REMOVED: Do not enable all interrupts, they trigger false aborts!
  err = esp_intr_alloc(ETS_SPI2_INTR_SOURCE, ESP_INTR_FLAG_LEVEL2, spiISR, this, &_spiIsrHandle);
  if (err != ESP_OK) {
    Serial.printf("[SPI] SPI ISR alloc failed: %d\n", err);
    deinit();
    return false;
  }

  _initialized = true;
  return true;
}

void SpiBusContext::deinit() {
  _txdone = true;

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

int8_t SpiBusContext::registerChannel(int8_t pin, ParallelSpiBus* bus) {
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
  _channelCount++;
  _channelMask |= (1 << idx);

  // Route SPI data signal to GPIO
  pinMode(pin, OUTPUT);
  pinMatrixOutAttach(pin, SPI_SIGNAL_INDICES[idx], false, false);

  return idx;
}

void SpiBusContext::unregisterChannel(int8_t channelIdx) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_SPI_MAX_CHANNELS) return;
  if (!_channels[channelIdx].active) return;

  if (_channels[channelIdx].pin >= 0) {
    gpio_reset_pin((gpio_num_t)_channels[channelIdx].pin);
  }

  _channels[channelIdx] = {nullptr, -1, nullptr, 0, false};
  _channelCount--;
  _channelMask &= ~(1 << channelIdx);
}

void SpiBusContext::setChannelData(int8_t channelIdx, const uint8_t* data, size_t len) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_SPI_MAX_CHANNELS) return;
  _channels[channelIdx].srcData = data;
  _channels[channelIdx].srcLen = len;

  // Mark this channel as staged
  _stagedMask |= (1 << channelIdx);

  if (len > _numBytes) _numBytes = len; // update longest bus length
}

bool SpiBusContext::startTransmit() {
  if (_channelCount == 0) return false; // TODO: can this ever happen?
  //if (!_txdone) return false; // previous transfer still in progress (should not happen but it might) TODO: does it? -> its guarded

  if (_stagedMask != _channelMask) return true; // not all channels ready yet
  _stagedMask = 0;
  _txdone = false;
  dmacount = 0;

  // debug hack: make sure rmt is not running during critical init
  //rmt_channel_t rmtChannel = (rmt_channel_t)0; // TODO: need to track this if using RMT for other purposes, or better, use a separate timer-based approach for WS2812 reset pulse timing
  //rmt_wait_tx_done(rmtChannel, portMAX_DELAY);
  //rmtChannel= (rmt_channel_t)1;
  //rmt_wait_tx_done(rmtChannel, portMAX_DELAY);
  // CRITICAL SECTION START: Prevent ISRs from interfering with buffer init
  // note: DMA SPI init is VERY timing and sequence sensitive, under load, this sequence is not guaranteed and it leads to stalls and the driver not re-starting correctly
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&mux);
  // Stop SPI and DMA before reconfiguring for new transfer
  //_hw->cmd.usr = 0; // stop SPI user transfer -> this leads to errors, need to let it "cool down by itself"
  while (_hw->cmd.usr); // wait for SPI to actually stop (this step is crucial for stability)
  spi_ll_clear_int_stat(_hw); // the order of these three commands is important
  spi_ll_dma_tx_fifo_reset(_hw);
  spi_ll_outfifo_empty_clr(_hw);

  // TODO: there is no need to calculate this every frame, could store the value at init
  size_t newBytes = 0;
  for (int ch = 0; ch < WLEDPB_SPI_MAX_CHANNELS; ch++) {
    if (_channels[ch].active && _channels[ch].srcLen > newBytes) {
      newBytes = _channels[ch].srcLen;
    }
  }
  //if(spierror) Serial.printf("start: SPI error: %d\n", spierror);
  //Serial.print(".");
  spierror = 0;
  _numBytes = newBytes;

  // Total bits: 16 DMA bytes per source byte × 8 bits/byte = 128 bits per source byte
  // Plus reset: extra zero bits at the end
  uint32_t resetBits = 4096; // TODO: calculate from actual reset pulse, make sure its 4 byte aligned (just in case)  // ~300us reset at ~2.6MHz 
  uint32_t totalBits = (_numBytes * 16 * 8) + resetBits;
  if (totalBits > 262143) totalBits = 262143;  // SPI: max 18 bits for length register (uninterrupted linked-list DMA transfer)  TODO: need to handle this in the UI to not allow more LEDs (restarting a transfer may cause some LED strips to latch, it takes up to 60us)

  spi_ll_set_mosi_bitlen(_hw, totalBits);

  // re-initialize DMA & buffers
  gdma_dev_t* dma = &GDMA;
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 0;
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1;
  gdma_ll_tx_reset_channel(dma, WLEDPB_SPI_GDMA_CHANNEL);
  gdma_ll_tx_connect_to_periph(dma, WLEDPB_SPI_GDMA_CHANNEL, GDMA_TRIG_PERIPH_SPI, SOC_GDMA_TRIG_PERIPH_SPI2);
  gdma_ll_tx_set_desc_addr(dma, WLEDPB_SPI_GDMA_CHANNEL, (uint32_t)&_dmaDesc[0]);

  _framePos = 0;
  _currentBuffer = 0;

  // fill all descriptors
  encodecalls = 0;
  for (int i = 0; i < WLEDPB_SPI_DMA_DESC_COUNT; i++) {
    encodeSpiChunk(i);
    _dmaDesc[i].owner = 1; // make sure DMA owns the descriptor
    _dmaDesc[i].eof = 1; // enable eof flag
  }

  gdma_ll_tx_start(dma, WLEDPB_SPI_GDMA_CHANNEL);
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].clr.out_eof = 1; // clear again, just in case
  dma->intr[WLEDPB_SPI_GDMA_CHANNEL].ena.out_eof = 1; // re enable interrupt

  // re-attach pins to SPI signals
  for (int i = 0; i < WLEDPB_SPI_MAX_CHANNELS; i++) {
    if (_channels[i].active && _channels[i].pin >= 0) {
      pinMatrixOutAttach(_channels[i].pin, SPI_SIGNAL_INDICES[i], false, false); // TODO: should this trick also be used to force outputs low during reset pulse?
    }
  }

  spi_ll_dma_tx_enable(_hw, true);
  spi_ll_apply_config(_hw);  // apply SPI config AFTER starting DMA to make sure they are in sync

  // Short hardware handshake sync: adding a few nops is enough to ensure there is DMA data in the SPI buffer (without this, SPI can immediately quit its duty due to FIFO unterrun)
  //asm volatile("nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"); // might not be needed but just in case

  spi_ll_user_start(_hw); // start SPI user transfer

  _lastTransmitMs = millis();
  portEXIT_CRITICAL(&mux);
//Serial.print("-");
  return true;
}

// SpiBus implementation

ParallelSpiBus::ParallelSpiBus(int8_t pin, const LedTiming& timing, ColorOrder order)
  : _pin(pin)
  , _timing(timing)
  , _order(order)
  , _initialized(false)
  , _channelIdx(-1)
  , _ctx(nullptr)
  , _encodeBuffer(nullptr)
  , _encodeBufferSize(0)
{
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

  _channelIdx = _ctx->registerChannel(_pin, this);
  if (_channelIdx < 0) {
    Serial.printf("[SPI] registerChannel failed for pin %d\n", _pin);
    SpiBusContext::release();
    _ctx = nullptr;
    return false;
  }

  _initialized = true;
  return true;
}

void ParallelSpiBus::end() {
  if (!_initialized) return;

  if (_ctx) {
    uint32_t startWait = millis();
    while (!_ctx->isIdle()) {
      if (millis() - startWait > 100) {
        break;
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

bool ParallelSpiBus::allocateBuffer(uint16_t numPixels) {
  size_t needed = numPixels * getChannelCount(_order);
  if (_encodeBuffer && _encodeBufferSize >= needed) return true;

  if (_encodeBuffer) heap_caps_free(_encodeBuffer);

  _encodeBuffer = (uint8_t*)heap_caps_malloc(needed, MALLOC_CAP_INTERNAL);
  if (!_encodeBuffer) {
    _encodeBufferSize = 0;
    return false;
  }
  _encodeBufferSize = needed;
  return true;
}

// TODO: this actually only uses internal buffer, could get rid of the parameters that are now unused.
bool ParallelSpiBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
  if (!_initialized || !_ctx) return false;

  // If a previous SPI transfer is still in progress, defer this update to avoid
  // overlapping transmissions. Multi-channel parallel SPI keeps all lanes in sync
  // from one final startTransmit() call once all channels have been staged.
  if (!_ctx->isSpiDone()) return false;

  if (!allocateBuffer(_numPixels)) return false; // TODO: naming is confusing, it does not allocate each time, should this be moved to init()?

  ColorEncoder encoder(_order);
  uint8_t* dst = _encodeBuffer;
  uint8_t numCh = encoder.getNumChannels();

  for (uint16_t i = 0; i < _numPixels; i++) {
    encoder.encode(_pixelData[i], _cctData ? &_cctData[i] : nullptr, dst);
    dst += numCh;
  }

  _ctx->setChannelData(_channelIdx, _encodeBuffer, _numPixels * numCh);

  return _ctx->startTransmit();
}

bool ParallelSpiBus::canShow() const {
  if (!_ctx) return true;
  return _ctx->isSpiDone();
}

void ParallelSpiBus::waitComplete() {
  while (_ctx && !_ctx->isIdle()) {
    vTaskDelay(1);
  }
}

void ParallelSpiBus::setColorOrder(ColorOrder order) {
  _order = order;
}


} // namespace WLEDpixelBus
#endif // WLEDPB_PARALLEL_SPI_SUPPORT