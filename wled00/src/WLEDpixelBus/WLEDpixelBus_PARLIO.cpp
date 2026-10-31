// WLEDpixelBus_PARLIO.cpp
/*-------------------------------------------------------------------------

WLEDpixelBus - parallel PARLIO output driver implementation

written by Damian Schneider @dedehai 2026

Note: PARLIO driver was generated with heavy help of AI with lots of refinement and testing
For a detailed description of how it works see the header file.

H2, C61 and P4 are untested (only tested working on C5 and C6), P4 does not use the seamless-DMA mode (not supported by API)
P4 encodes the whole frame into one buffer and sends it with parlio_tx_unit_transmit() call - still needs testing on real P4 hardware

-------------------------------------------------------------------------*/

#include "WLEDpixelBus_PARLIO.h"

#ifdef WLEDPB_PARLIO_SUPPORT

#if WLEDPB_PARLIO_SEAMLESS_DMA
  #include "hal/parlio_ll.h"      // TX engine low-level control (tx_bytelen, start, clock, idle value)
  #include "hal/gdma_ll.h"        // gdma_ll_tx_restart() - always-inline register write (includes soc/gdma_struct.h for the GDMA instance)
  #include "soc/parl_io_struct.h" // PARL_IO register struct instance
  //#include "esp_rom_gpio.h"
  #include "soc/parlio_periph.h"  // needed for hardware signal inversion using esp_rom_gpio_connect_out_signal()
  #define PARLIO_HW (&PARL_IO)    // single PARLIO group/unit on all supported targets
  // C6's parlio_ll.h defines PARLIO_LL_TX_MAX_BYTES_PER_FRAME (0xFFFF), the other targets only define the BITS variant (0x7FFFF) - the byte limit is the same 65535 everywhere
  #ifndef PARLIO_LL_TX_MAX_BYTES_PER_FRAME
    #define PARLIO_LL_TX_MAX_BYTES_PER_FRAME (PARLIO_LL_TX_MAX_BITS_PER_FRAME / 8)
  #endif

  // C5 MP (and future chips with AHB GDMA v2+) expose the DMA controller as AHB_DMA and use ahb_dma_ll_* helpers, while C6/H2 keep the legacy GDMA naming
  #if defined(SOC_AHB_GDMA_VERSION) && (SOC_AHB_GDMA_VERSION >= 2)
    #define WLEDPB_PARLIO_GDMA_DEV (&AHB_DMA)
    #define WLEDPB_PARLIO_GDMA_LL_TX_RESTART ahb_dma_ll_tx_restart
  #else
    #define WLEDPB_PARLIO_GDMA_DEV (&GDMA)
    #define WLEDPB_PARLIO_GDMA_LL_TX_RESTART gdma_ll_tx_restart
  #endif
#endif

namespace WLEDpixelBus {

ParlioBusContext* ParlioBusContext::_instances[WLEDPB_PARLIO_BUS_COUNT] = {nullptr};
uint8_t ParlioBusContext::_refCount[WLEDPB_PARLIO_BUS_COUNT] = {0};

ParlioBusContext* ParlioBusContext::get(uint8_t busNum) {
  if (busNum >= WLEDPB_PARLIO_BUS_COUNT) return nullptr;

  if (_instances[busNum] == nullptr) {
    _instances[busNum] = new ParlioBusContext(busNum);
  }
  if (_instances[busNum] != nullptr)
    _refCount[busNum]++;
  return _instances[busNum];
}

void ParlioBusContext::release(uint8_t busNum) {
  if (busNum >= WLEDPB_PARLIO_BUS_COUNT) return;
  if (_refCount[busNum] == 0) return;

  _refCount[busNum]--;
  if (_refCount[busNum] == 0 && _instances[busNum]) {
    delete _instances[busNum];
    _instances[busNum] = nullptr;
  }
}

ParlioBusContext::ParlioBusContext(uint8_t /*busNum*/)
  : _txUnit(nullptr)
  , _unitEnabled(false)
  , _unitStale(false)
  , _state(DriverState::Idle)
  , _txStartMillis(0)
  , _initialized(false)
  , _bufferSize(0)
  , _maxSrcBytes(0)
  , _dmaAllocated(false)
  , _resetBytesLeft(0)
#if WLEDPB_PARLIO_SEAMLESS_DMA
  , _dmaChan(nullptr)
  , _dmaChanIdx(-1)
  , _desc(nullptr)
  , _fillHead(0)
  , _chunkBytesLeft(0)
#endif
  , _timing{0, 0, 0, 0, 0}
  , _outClockHz(0)
  , _channelCount(0)
  , _channelMask(0)
  , _stagedMask(0)
  , _invertMask(0)
  , _maxDataLen(0)
{
  for (int i = 0; i < WLEDPB_PARLIO_MAX_CHANNELS; i++) {
    _channels[i] = {-1, nullptr, 0, 0, false};
  }

  for (int i = 0; i < WLEDPB_PARLIO_DMA_BUFFER_COUNT; i++) {
    _dmaBuffer[i] = nullptr;
    _txLen[i] = 0;
    _endOfFrame[i] = false;
  }
}

ParlioBusContext::~ParlioBusContext() {
  deinit();
}

bool ParlioBusContext::init(const LedTiming& timing) {
  if (_initialized) return true;

  _timing = timing;

  // PARLIO output clock: 4 clocks per LED bit (4-step cadence).
  // Note: the PARLIO driver only has an INTEGER clock divider from the source clock (increases in steps of about 16ns)
  uint32_t clkHz = calc4StepClockHz(timing);
  if (clkHz > 40000000UL) clkHz = 40000000UL; // limit output clock (40MHz max)  TODO: check if this is the limit for all ESPs
  _outClockHz = clkHz;

  // Note: the PARLIO unit is NOT created here. Pins are fixed at unit creation time and
  // channels register after init(), so creation is deferred to the first startTransmit().

  _initialized = true;
  return true;
}

void ParlioBusContext::deinit() {
  waitForIdle();

  hwStopTransfer();

#if WLEDPB_PARLIO_SEAMLESS_DMA
  if (_desc) {
    heap_caps_free(_desc); // note: _dmaChan is owned by the PARLIO unit, it is deleted in hwDeinit()
    _desc = nullptr;
  }
#endif

  for (int i = 0; i < WLEDPB_PARLIO_DMA_BUFFER_COUNT; i++) {
    if (_dmaBuffer[i]) {
      heap_caps_free(_dmaBuffer[i]);
      _dmaBuffer[i] = nullptr;
    }
  }

  hwDeinit();

  _dmaAllocated = false;
  _initialized = false;
}

bool ParlioBusContext::hwInit() {
  // data GPIOs are fixed at creation; use GPIO_NUM_NC for unused data lines
  parlio_tx_unit_config_t config;
  memset(&config, 0, sizeof(config));
  config.clk_src = PARLIO_CLK_SRC_DEFAULT; // do NOT select XTAL explicitly, known C6 issue
  config.data_width = WLEDPB_PARLIO_MAX_CHANNELS;
  config.clk_in_gpio_num = GPIO_NUM_NC;    // not used, TX only
  config.valid_gpio_num = GPIO_NUM_NC;     // valid signal would occupy the MSB data line
  config.clk_out_gpio_num = GPIO_NUM_NC;   // no external clock needed
  for (int i = 0; i < WLEDPB_PARLIO_MAX_CHANNELS; i++) {
    config.data_gpio_nums[i] = (_channels[i].active && _channels[i].pin >= 0) ? (gpio_num_t)_channels[i].pin : GPIO_NUM_NC;
  }
  config.output_clk_freq_hz = _outClockHz;
  config.trans_queue_depth = WLEDPB_PARLIO_DMA_BUFFER_COUNT;
  config.max_transfer_size = _bufferSize;  // note: _allocDmaBuffers() must run before hwInit()
  config.sample_edge = PARLIO_SAMPLE_EDGE_POS;
  config.bit_pack_order = PARLIO_BIT_PACK_ORDER_MSB;

  esp_err_t err = parlio_new_tx_unit(&config, &_txUnit);
  if (err != ESP_OK) {
    _txUnit = nullptr;
    return false;
  }

  for (int i = 0; i < WLEDPB_PARLIO_MAX_CHANNELS; i++) {
    if (_channels[i].active && _channels[i].pin >= 0 && (_invertMask & (1 << i))) {
      esp_rom_gpio_connect_out_signal(_channels[i].pin, parlio_periph_signals.groups[0].tx_units[0].data_sigs[i], true, false);  // invert the output using hardware
    }
  }

  parlio_tx_event_callbacks_t cbs;
  memset(&cbs, 0, sizeof(cbs));
  cbs.on_trans_done = txDoneCallback;
  err = parlio_tx_unit_register_event_callbacks(_txUnit, &cbs, this); 
  if (err != ESP_OK) {
    parlio_del_tx_unit(_txUnit);
    _txUnit = nullptr;
    return false;
  }

#if WLEDPB_PARLIO_SEAMLESS_DMA
  if (!_initDmaRing()) {
    parlio_del_tx_unit(_txUnit);
    _txUnit = nullptr;
    return false;
  }
#endif

  _unitStale = false;
  _unitEnabled = false;
  return true;
}

void ParlioBusContext::hwDeinit() {
  if (_txUnit) {
    if (_unitEnabled) {
      parlio_tx_unit_disable(_txUnit);
      _unitEnabled = false;
    }
    parlio_del_tx_unit(_txUnit);
    _txUnit = nullptr;
#if WLEDPB_PARLIO_SEAMLESS_DMA
    _dmaChan = nullptr; // channel was owned by the unit
#endif
  }
}

// note: only called from deinit() (waits for idle first) and abortTransmit() (watchdog recovery)
void IRAM_ATTR ParlioBusContext::hwStopTransfer() {
#if WLEDPB_PARLIO_SEAMLESS_DMA
  if (_dmaChan) gdma_stop(_dmaChan);
#endif
  if (_txUnit && _unitEnabled) {
    parlio_tx_unit_disable(_txUnit);
    _unitEnabled = false;
  }
}

// Watchdog recovery for a stalled transmission: stop the unit/DMA and return to idle, hardware is re-enabled by hwStartTransfer()
void ParlioBusContext::abortTransmit() {
  hwStopTransfer();
  _state = DriverState::Idle;
  _stagedMask = 0;
  _resetBytesLeft = 0;
#if WLEDPB_PARLIO_SEAMLESS_DMA
  _chunkBytesLeft = 0;
#endif
}

bool ParlioBusContext::ensureIdle() {
  if (isIdle()) return true;
  if ((uint32_t)(millis() - _txStartMillis) <= WLEDPB_TRANSFER_TIMEOUT_MS) return false;
  abortTransmit();
  return true;
}

void ParlioBusContext::waitForIdle() {
  const uint32_t start = millis();
  while (!ensureIdle() && (uint32_t)(millis() - start) <= WLEDPB_TRANSFER_TIMEOUT_MS) {
    vTaskDelay(1);
  }
  if (!isIdle()) abortTransmit();
}

#if WLEDPB_PARLIO_SEAMLESS_DMA

// Seamless streaming: unit's GDMA channel + our circular descriptor ring
bool ParlioBusContext::_initDmaRing() {
  if (!_desc) {
    _desc = (dma_descriptor_t*)heap_caps_aligned_alloc(4, WLEDPB_PARLIO_DMA_BUFFER_COUNT * sizeof(dma_descriptor_t),
                                                       MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (!_desc) return false;
    memset(_desc, 0, WLEDPB_PARLIO_DMA_BUFFER_COUNT * sizeof(dma_descriptor_t));
    for (int i = 0; i < WLEDPB_PARLIO_DMA_BUFFER_COUNT; i++) {
      _desc[i].dw0.suc_eof = 1; // per-descriptor EOF interrupt (refill heartbeat); PARLIO ignores it (EOF is by byte counter)
      _desc[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_CPU;
      _desc[i].buffer = _dmaBuffer[i];
      _desc[i].next = (i == WLEDPB_PARLIO_DMA_BUFFER_COUNT - 1) ? &_desc[0] : &_desc[i + 1];
    }
  }

  // borrow the unit's GDMA channel (see WledpbParlioTxUnitHead): already connected to the PARLIO trigger
  _dmaChan = WLEDPB_PARLIO_TX_DMA_CHAN(_txUnit);

  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  // IDF 5.5 configures the channel with owner_check=false / auto_update_desc=false, we need that changed for our seamless ring refill
  gdma_strategy_config_t strategy = {
    .owner_check = true,
    .auto_update_desc = true,
    .eof_till_data_popped = false,
  };
  if (gdma_apply_strategy(_dmaChan, &strategy) != ESP_OK) {
    _dmaChan = nullptr;
    return false;
  }
  #endif
  if (!_dmaChan) return false;

  _dmaChanIdx = -1;
  int chanIdx = -1;
  if (gdma_get_channel_id(_dmaChan, &chanIdx) == ESP_OK &&
      chanIdx >= 0 && chanIdx < SOC_GDMA_PAIRS_PER_GROUP_MAX) {
    _dmaChanIdx = (int8_t)chanIdx;
  }

  // the driver (v5.3-v5.5) never registers GDMA callbacks on this channel, they are ours
  gdma_tx_event_callbacks_t cbs = {};
  cbs.on_trans_eof = gdmaEofCallback;
  if (gdma_register_tx_event_callbacks(_dmaChan, &cbs, this) != ESP_OK) {
    _dmaChan = nullptr;
    return false;
  }
  return true;
}

void IRAM_ATTR ParlioBusContext::_armDescriptor(uint8_t idx) {
  const uint32_t len = (uint32_t)_txLen[idx];
  _desc[idx].dw0.size = len;
  _desc[idx].dw0.length = len;
  _desc[idx].buffer = _dmaBuffer[idx];
  _desc[idx].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA; // owner write last: hands the descriptor to the DMA
}

IRAM_ATTR bool ParlioBusContext::gdmaEofCallback(gdma_channel_handle_t /*dma_chan*/, gdma_event_data_t* /*edata*/, void* user_ctx) {
  ParlioBusContext* ctx = (ParlioBusContext*)user_ctx;
  if (ctx->_state != DriverState::Sending) return false; // stale wake-up between frames

  // refill every descriptor the DMA has handed back to the CPU
  while (ctx->_state == DriverState::Sending) {
    const uint8_t idx = ctx->_fillHead;
    if (ctx->_desc[idx].dw0.owner != DMA_DESCRIPTOR_BUFFER_OWNER_CPU) break; // not consumed yet
    if (ctx->_resetBytesLeft == WLEDPB_PARLIO_XFER_DONE_FLAG) break;         // final buffer already queued, leave DMA stalled behind it
    ctx->fillBuffer(idx);
    ctx->_armDescriptor(idx);
    ctx->_fillHead = (uint8_t)((idx + 1) % WLEDPB_PARLIO_DMA_BUFFER_COUNT);
  }

  // If the DMA caught up with the CPU and paused on a CPU-owned descriptor, restart the outlink
  if (ctx->_dmaChanIdx >= 0) {
    WLEDPB_PARLIO_GDMA_LL_TX_RESTART(WLEDPB_PARLIO_GDMA_DEV, (uint32_t)ctx->_dmaChanIdx);
  } else {
    gdma_append(ctx->_dmaChan); // channel index unknown: last resort
  }
  return false;
}

void IRAM_ATTR ParlioBusContext::_processChunkEof() {
  // Invoked from the IDF driver's PARLIO TX EOF ISR (via on_trans_done)
  // at this point, tx_bytelen bytes have been clocked out. The driver ISR has already stopped the TX engine and clock
  if (_state != DriverState::Sending) return; // stale TX EOF from a previous frame
  if (_chunkBytesLeft == 0) {
    _state = DriverState::Idle; // whole frame clocked out, engine stays off until the next frame
    return;
  }

  // Frame longer than the 16-bit tx_bytelen register: seamlessly chain the next chunk
  const uint32_t chunk = _chunkBytesLeft > PARLIO_LL_TX_MAX_BYTES_PER_FRAME ? PARLIO_LL_TX_MAX_BYTES_PER_FRAME : _chunkBytesLeft;
  _chunkBytesLeft -= chunk;
  parlio_ll_tx_set_trans_bit_len(PARLIO_HW, chunk * 8);
  uint32_t wait = 100000; // FIFO still holds DMA'd data; bounded wait for safety
  while (!parlio_ll_tx_is_ready(PARLIO_HW) && --wait) {}
  parlio_ll_tx_start(PARLIO_HW, true);
  parlio_ll_tx_enable_clock(PARLIO_HW, true);
}

#else

// Fallback: single full-frame buffer, one transaction per frame
bool IRAM_ATTR ParlioBusContext::_queueBuffer(uint8_t bufIdx) {
  parlio_transmit_config_t trans_config = {
    .idle_value = 0 // GPIO matrix inversion applies to the idle level too
  };
  // note: payload size is in BITS, not bytes
  return parlio_tx_unit_transmit(_txUnit, _dmaBuffer[bufIdx], _txLen[bufIdx] * 8, &trans_config) == ESP_OK;
}
#endif // WLEDPB_PARLIO_SEAMLESS_DMA

bool ParlioBusContext::_allocDmaBuffers() {
  if (_dmaAllocated) return true;
  auto rollback = [&]() {
    for (uint8_t i = 0; i < WLEDPB_PARLIO_DMA_BUFFER_COUNT; ++i) {
      if (_dmaBuffer[i]) heap_caps_free(_dmaBuffer[i]);
      _dmaBuffer[i] = nullptr;
    }
    _bufferSize = 0;
  };

#if WLEDPB_PARLIO_SEAMLESS_DMA
  _bufferSize = calc4StepBufferSize(_maxSrcBytes, WLEDPB_PARLIO_DMABYTES,
    WLEDPB_PARLIO_DMA_BUFFER_COUNT, MIN_DMA_BUFFER_SIZE, DEFAULT_DMA_BUFFER_SIZE);
  if (_bufferSize == 0) return false;
  for (uint8_t i = 0; i < WLEDPB_PARLIO_DMA_BUFFER_COUNT; ++i) {
    _dmaBuffer[i] = (uint8_t*)heap_caps_aligned_alloc(4, _bufferSize, MALLOC_CAP_DMA);
    if (!_dmaBuffer[i]) { rollback(); return false; }
    memset(_dmaBuffer[i], 0, _bufferSize);
  }
#else
  // The transaction fallback needs one gap-free buffer for the complete frame.
  _bufferSize = (size_t)WLEDPB_PARLIO_DMABYTES * _maxSrcBytes + _calcResetBytes();
  _bufferSize = (_bufferSize + 3) & ~size_t(3);
  if (_bufferSize == 0) return false;
  _dmaBuffer[0] = (uint8_t*)heap_caps_aligned_alloc(4, _bufferSize, MALLOC_CAP_DMA);
  if (!_dmaBuffer[0]) { rollback(); return false; }
  memset(_dmaBuffer[0], 0, _bufferSize);
#endif
  _dmaAllocated = true;
  return true;
}

int8_t ParlioBusContext::registerChannel(int8_t pin, size_t srcBytes, bool inverted) {
  int8_t idx = -1;
  for (int i = 0; i < WLEDPB_PARLIO_MAX_CHANNELS; i++) {
    if (!_channels[i].active) {
      idx = i; // found free slot
      break;
    }
  }

  if (idx < 0) return -1;

  _channels[idx].pin = pin;
  _channels[idx].active = true;
  _channelCount++;
  _channelMask |= (1 << idx);
  if (inverted) _invertMask |= (1 << idx);

  // track the largest source byte count across channels; used in _allocDmaBuffers() to size DMA buffers
  if (srcBytes > _maxSrcBytes) _maxSrcBytes = srcBytes;

  // pins are fixed at PARLIO unit creation: no pin routing here, just mark the unit for recreation
  if (_txUnit) _unitStale = true;

  return idx;
}

void ParlioBusContext::unregisterChannel(int8_t channelIdx) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_PARLIO_MAX_CHANNELS) return;
  if (!_channels[channelIdx].active) return;

  _channels[channelIdx] = {-1, nullptr, 0, 0, false};
  _channelCount--;
  _channelMask &= ~(1 << channelIdx);
  _invertMask &= ~(1 << channelIdx);

  if (_txUnit) _unitStale = true; // recreate unit without this pin on next transmit
}

void ParlioBusContext::setChannelData(int8_t channelIdx, const uint8_t* data, size_t len) {
  if (channelIdx < 0 || channelIdx >= WLEDPB_PARLIO_MAX_CHANNELS) return;

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

// encode4Step: 4-step cadence, converts per-channel byte streams to parallel DMA words
// step0=HIGH, step1=data, step2=data, step3=LOW ('0' is 0b1000, '1' is 0b1110).
// 8-bit parallel encode: branchless gather + scatter, 32 bytes per source byte (8 channels)
// TODO: identical to I2S version, need to consolidate
void IRAM_ATTR ParlioBusContext::encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel) {
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
    if (!alwaysMask) break; // no active channels produced data

    uint32_t* p = (uint32_t*)(dest + pos);
    // S2/S3: no swap, layout [S0,S1,S2,S3] = [HIGH,data,data,LOW]
    #define EMIT8(bN, OFF) \
      p[OFF] = (uint32_t)(alwaysMask) | ((uint32_t)(bN) << 8) | ((uint32_t)(bN) << 16);

    EMIT8(b0, 0)  EMIT8(b1, 1)  EMIT8(b2, 2)  EMIT8(b3, 3)
    EMIT8(b4, 4)  EMIT8(b5, 5)  EMIT8(b6, 6)  EMIT8(b7, 7)
    #undef EMIT8
  }
}

// bytes of reset period appended at the end of a frame
uint32_t IRAM_ATTR ParlioBusContext::_calcResetBytes() const {
  // one LED bit cycle is 4 clocks, 1 buffer byte per clock
  return (uint32_t)calc4StepResetDmaBytes(_timing, WLEDPB_PARLIO_DMABYTES / 8);
}

void IRAM_ATTR __attribute__((noinline)) ParlioBusContext::fillBuffer(uint8_t bufIdx) {
  uint32_t* buf32 = (uint32_t*)_dmaBuffer[bufIdx];
  const uint32_t* end = buf32 + (_bufferSize >> 2);
  while (buf32 < end) *buf32++ = 0; // clear buffer (set all lanes low), DMA buffer is 4 bytes aligned. Note: memset is not IRAM safe and may crash

  _endOfFrame[bufIdx] = false;

  if (_resetBytesLeft > 0 && _resetBytesLeft != WLEDPB_PARLIO_XFER_DONE_FLAG) {
    // reset pulse continuation: this buffer is pure reset level. Long reset periods span several buffers; only the last one is flagged as end of frame. Never let _txLen
    if (_resetBytesLeft <= _bufferSize) {
      _txLen[bufIdx] = _resetBytesLeft;
      _resetBytesLeft = WLEDPB_PARLIO_XFER_DONE_FLAG; // flag end of frame, don't queue any more buffers
      _endOfFrame[bufIdx] = true;
    } else {
      _txLen[bufIdx] = _bufferSize;
      _resetBytesLeft -= _bufferSize; // more pure-reset buffers to come (stays a multiple of 4)
    }
    return; // nothing to encode, keep lines at reset level
  }

  uint32_t bytesToEncode = 0;
  uint8_t maxCh = 0;
  for (int ch = 0; ch < WLEDPB_PARLIO_MAX_CHANNELS; ch++) {
    if (_channels[ch].active) {
      maxCh = ch + 1;
      uint32_t channelBytesLeft = _channels[ch].srcLen - _channels[ch].srcPos;
      if (channelBytesLeft > bytesToEncode) bytesToEncode = channelBytesLeft;
    }
  }

  size_t translatedbytes = bytesToEncode * WLEDPB_PARLIO_DMABYTES;
  if (translatedbytes > _bufferSize) translatedbytes = _bufferSize;
  translatedbytes -= translatedbytes % WLEDPB_PARLIO_DMABYTES;
  encode4Step(_dmaBuffer[bufIdx], translatedbytes, maxCh);
  _txLen[bufIdx] = translatedbytes;

  if (translatedbytes < _bufferSize) {
    // Data ran out before the buffer was full (i.e. we are done), compute the minimum reset period we must send
    size_t resetBytes = _calcResetBytes();

    size_t newLen = translatedbytes + resetBytes;
    if (newLen > _bufferSize) {
      _resetBytesLeft = newLen - _bufferSize; // reset pulse does not fit into this buffer frame, send another one (see above)
      _txLen[bufIdx] = _bufferSize;
    }
    else {
      _txLen[bufIdx] = newLen;                        // send the rest (reset level) as a reset
      _endOfFrame[bufIdx] = true;
      _resetBytesLeft = WLEDPB_PARLIO_XFER_DONE_FLAG; // flag end of frame, don't queue any more buffers
    }
  }
}

bool ParlioBusContext::startTransmit() {
  if (_state != DriverState::Idle) return false;
  if (_channelCount == 0) return false;

  // Only start transmission if ALL active channels have populated data
  if (_stagedMask != _channelMask) return true;
  _stagedMask = 0; // Reset for next frame

  _maxDataLen = 0;
  for (int ch = 0; ch < WLEDPB_PARLIO_MAX_CHANNELS; ch++) {
    if (_channels[ch].active) {
      _channels[ch].srcPos = 0;
      if (_channels[ch].srcLen > _maxDataLen) {
        _maxDataLen = _channels[ch].srcLen;
      }
    }
  }

  _resetBytesLeft = 0;
  _txStartMillis = millis();

#if !WLEDPB_PARLIO_SEAMLESS_DMA
  // The fallback transmits one buffer
  const size_t requiredBytes = (_maxDataLen * WLEDPB_PARLIO_DMABYTES + _calcResetBytes() + 3) & ~size_t(3);
  if (_dmaAllocated && requiredBytes > _bufferSize) {
    hwDeinit();
    heap_caps_free(_dmaBuffer[0]);
    _dmaBuffer[0] = nullptr;
    _bufferSize = 0;
    _dmaAllocated = false;
  }
#endif
  if (!_dmaAllocated && !_allocDmaBuffers()) return false;

  // (re)create the PARLIO unit on first transmit, or when channels changed
  if (!_txUnit || _unitStale) {
    hwDeinit();
    if (!hwInit()) return false;
  }

#if !WLEDPB_PARLIO_SEAMLESS_DMA
  _state = DriverState::Sending; // fallback: EOF callbacks do not gate on the state
#endif
  // seamless: _state is set to Sending at the end of hwStartTransfer(), after the ring/ is fully built and the engine is running - until then EOF callbacks stay disabled

  if (!hwStartTransfer()) {
    _state = DriverState::Idle;
    return false;
  }

  return true;
}

bool ParlioBusContext::hwStartTransfer() {
  if (!_unitEnabled) {
    if (parlio_tx_unit_enable(_txUnit) != ESP_OK) return false;
    _unitEnabled = true;
  }

#if WLEDPB_PARLIO_SEAMLESS_DMA
  // one hardware transaction for the whole frame (data + reset period)
  const uint32_t totalBytes = (uint32_t)_maxDataLen * WLEDPB_PARLIO_DMABYTES + _calcResetBytes();
  const uint32_t firstChunk = totalBytes > PARLIO_LL_TX_MAX_BYTES_PER_FRAME ? PARLIO_LL_TX_MAX_BYTES_PER_FRAME : totalBytes;
  _chunkBytesLeft = totalBytes - firstChunk;

  // reset the ring: DMA stopped, all descriptors back to CPU, refill from scratch.
  // note: _state is still Idle here, so a stale pending GDMA/PARLIO EOF interrupt is ignored by the callbacks while we rebuild (see gdmaEofCallback)
  gdma_stop(_dmaChan);
  _fillHead = 0;
  for (int i = 0; i < WLEDPB_PARLIO_DMA_BUFFER_COUNT; i++) {
    _desc[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_CPU;
    fillBuffer(i);
    _armDescriptor(i);
    _fillHead = (uint8_t)((i + 1) % WLEDPB_PARLIO_DMA_BUFFER_COUNT);
    if (_endOfFrame[i]) break; // frame fits into the first buffers, rest of the ring stays disarmed
  }

  // mirror the IDF driver's transaction start sequence, minus the descriptor remount (the ring stays linked)
  parlio_ll_tx_set_idle_data_value(PARLIO_HW, 0); // GPIO matrix inversion applies to the idle level too
  parlio_ll_tx_reset_fifo(PARLIO_HW);
  parlio_ll_tx_reset_clock(PARLIO_HW);
  parlio_ll_tx_set_trans_bit_len(PARLIO_HW, firstChunk * 8);
  gdma_start(_dmaChan, (intptr_t)&_desc[0]);
  // wait until the first DMA data reached the TX FIFO (same handshake as the IDF driver)
  uint32_t wait = 1000000;
  while (!parlio_ll_tx_is_ready(PARLIO_HW) && --wait) {}
  if (!wait) return false;
  parlio_ll_tx_start(PARLIO_HW, true);
  parlio_ll_tx_enable_clock(PARLIO_HW, true);
  // open the refill window only now: the first EOF can raise one buffer-duration from here at the earliest, and the ring is fully built
  _state = DriverState::Sending;
  return true;
#else
  // Single-buffer fallback: fill buffer 0 with the whole frame
  fillBuffer(0);
  return _queueBuffer(0);
#endif
}

IRAM_ATTR bool ParlioBusContext::txDoneCallback(parlio_tx_unit_handle_t tx_unit, const parlio_tx_done_event_data_t* edata, void* user_ctx) {
  (void)tx_unit;
  (void)edata;
  ParlioBusContext* ctx = (ParlioBusContext*)user_ctx;
#if WLEDPB_PARLIO_SEAMLESS_DMA
  ctx->_processChunkEof();
#else
  ctx->_state = DriverState::Idle; // single transaction per frame: transmit-done = frame complete
#endif
  return false;
}

// ParlioBus implementation
// ============================================

ParlioBus::ParlioBus(int8_t pin, const LedTiming& timing, uint8_t busNum, uint8_t ledType)
  : _pin(pin)
  , _timing(timing)
  , _initialized(false)
  , _busNum(busNum)
  , _channelIdx(-1)
  , _ctx(nullptr)
{
  // encoder is installed by createBus() from BusProperties
  _ledType = ledType;
}

ParlioBus::~ParlioBus() {
  end();
}

bool ParlioBus::begin() {
  if (_initialized) return true;

  _ctx = ParlioBusContext::get(_busNum);
  if (!_ctx) return false;

  if (!_ctx->init(_timing)) {
    ParlioBusContext::release(_busNum);
    _ctx = nullptr;
    return false;
  }

  // pass our encoded byte count so the context can size DMA buffers for the largest bus
  const size_t srcBytes = (size_t)_numPixels * _encoder.getBytesPerPixel() + _prefixLen + _suffixLen;
  _channelIdx = _ctx->registerChannel(_pin, srcBytes, _inverted);
  if (_channelIdx < 0) {
    ParlioBusContext::release(_busNum);
    _ctx = nullptr;
    return false;
  }

  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getBytesPerPixel())) { end(); return false; }
  return true;
}

void ParlioBus::end() {
  if (_ctx) {
    _ctx->waitForIdle();
    _ctx->unregisterChannel(_channelIdx);
    ParlioBusContext::release(_busNum);
    _ctx = nullptr;
  }
  releaseEncodeBuffer();
  _initialized = false;
}

bool ParlioBus::show() {
  if (!_initialized || !_ctx || !_encodeBuffer || _numPixels == 0) return false;
  _ctx->waitForIdle();
  _ctx->setChannelData(_channelIdx, _encodeBuffer, _encodeBufferSize);
  return _ctx->startTransmit();
}

bool ParlioBus::canShow() const {
  if (!_initialized) return true; // do not block if init failed! busmanager waits until all buses can show
  return _ctx && _ctx->ensureIdle();
}

} // namespace WLEDpixelBus
#endif // WLEDPB_PARLIO_SUPPORT