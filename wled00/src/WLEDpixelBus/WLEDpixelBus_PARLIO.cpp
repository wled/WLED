// WLEDpixelBus_PARLIO.cpp
/*-------------------------------------------------------------------------

WLEDpixelBus - parallel PARLIO output driver implementation

written by Damian Schneider @dedehai 2026

Note: PARLIO driver was generated with heavy help of an AI with lots of refinement and testing (works but needs a thorough review)
For a detailed escription on how it works see header file

TODO: need to do an in-depth review and harden edge cases, maybe also add a watchdog timeout in case things can go wrong
      as we use low level stuff to get around the gaps the "pure API" espressif driver has when using linked DMA lists

      Also C5, H2, C61 and P4 are untested (only tested working on C6)
      P4 does not use the seamless-DMA mode (not supported by API); the fallback path now
      encodes the whole frame into one buffer and sends it with a single
      parlio_tx_unit_transmit() call instead of ping-ponging several small buffers
      (see _allocDmaBuffers()) - still needs testing on real P4 hardware

-------------------------------------------------------------------------*/

#include "WLEDpixelBus_PARLIO.h"

#ifdef WLEDPB_PARLIO_SUPPORT

#if WLEDPB_PARLIO_SEAMLESS_DMA
  #include "hal/parlio_ll.h"      // TX engine low-level control (tx_bytelen, start, clock, idle value)
  #include "hal/gdma_ll.h"        // gdma_ll_tx_restart() - always-inline register write (includes soc/gdma_struct.h for the GDMA instance)
  #include "soc/parl_io_struct.h" // PARL_IO register struct instance
  #define PARLIO_HW (&PARL_IO)    // single PARLIO group/unit on all supported targets
  // C6's parlio_ll.h defines PARLIO_LL_TX_MAX_BYTES_PER_FRAME (0xFFFF), the other targets
  // only define the BITS variant (0x7FFFF) - the byte limit is the same 65535 everywhere
  #ifndef PARLIO_LL_TX_MAX_BYTES_PER_FRAME
    #define PARLIO_LL_TX_MAX_BYTES_PER_FRAME (PARLIO_LL_TX_MAX_BITS_PER_FRAME / 8)
  #endif

  // C5 MP (and future chips with AHB GDMA v2+) expose the DMA controller as AHB_DMA
  // and use ahb_dma_ll_* helpers, while C6/H2 keep the legacy GDMA naming.
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
    _channels[i] = {nullptr, -1, nullptr, 0, 0, false};
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
  // Note: the PARLIO driver only has an INTEGER clock divider from the source clock,
  // so arbitrary custom timings are less accurate than with the fractional I2S divider.
  // Standard timings are fine: 1.25us bit period -> 3.2MHz, exact integer division.
  uint32_t bitPeriodNs = timing.bitPeriod();
  if (bitPeriodNs == 0) return false;
  uint64_t clkHz = (4ULL * 1000000000ULL) / bitPeriodNs;
  if (clkHz > 40000000ULL) return false; // above PARLIO TX max clock on C6/H2
  _outClockHz = (uint32_t)clkHz;

  // NOTE: the PARLIO unit is NOT created here. Pins are fixed at unit creation time and
  // channels register after init(), so creation is deferred to the first startTransmit().

  _initialized = true;
  return true;
}

void ParlioBusContext::deinit() {
  int timeout = 100;
  while (!isIdle() && timeout--) { vTaskDelay(1); }

  hwStopTransfer();

#if WLEDPB_PARLIO_SEAMLESS_DMA
  // note: _dmaChan is owned by the PARLIO unit, it is deleted in hwDeinit()
  if (_desc) {
    heap_caps_free(_desc);
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

//==============================================================================
// Hardware abstraction
//==============================================================================

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
    config.data_gpio_nums[i] = (_channels[i].active && _channels[i].pin >= 0)
                             ? (gpio_num_t)_channels[i].pin : GPIO_NUM_NC;
  }
  config.output_clk_freq_hz = _outClockHz;
  config.trans_queue_depth = WLEDPB_PARLIO_DMA_BUFFER_COUNT;
  config.max_transfer_size = _bufferSize;  // note: _allocDmaBuffers() must run before hwInit()
  // the following two fields exist in recent IDF versions, remove them if your IDF does not have them
  config.sample_edge = PARLIO_SAMPLE_EDGE_POS;
  config.bit_pack_order = PARLIO_BIT_PACK_ORDER_MSB;

  esp_err_t err = parlio_new_tx_unit(&config, &_txUnit);
  if (err != ESP_OK) {
    _txUnit = nullptr;
    return false;
  }

  parlio_tx_event_callbacks_t cbs;
  memset(&cbs, 0, sizeof(cbs));
  cbs.on_trans_done = dmaCallback;
  err = parlio_tx_unit_register_event_callbacks(_txUnit, &cbs, this);
  if (err != ESP_OK) {
    parlio_del_tx_unit(_txUnit);
    _txUnit = nullptr;
    return false;
  }

#if WLEDPB_PARLIO_SEAMLESS_DMA
  // descriptor ring (once) + borrow this unit's GDMA channel and register the refill callback
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
    _dmaChan = nullptr; // channel was owned by the unit, now gone
#endif
  }
}

void IRAM_ATTR ParlioBusContext::hwStopTransfer() {
  // only called from deinit() (waits for idle first)
#if WLEDPB_PARLIO_SEAMLESS_DMA
  if (_dmaChan) gdma_stop(_dmaChan);
#endif
  if (_txUnit && _unitEnabled) {
    parlio_tx_unit_disable(_txUnit);
    _unitEnabled = false;
  }
}

#if WLEDPB_PARLIO_SEAMLESS_DMA
//==============================================================================
// Seamless streaming: unit's GDMA channel + our circular descriptor ring
//==============================================================================

bool ParlioBusContext::_initDmaRing() {
  // descriptor ring in DMA-capable internal RAM, statically closed into a circle:
  // the hardware follows the next pointers on its own, so the DMA never stops mid-frame.
  // Allocated once (survives unit recreation); the channel is re-borrowed per unit.
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

  // borrow the unit's GDMA channel (see WledpbParlioTxUnitHead): already connected to the
  // PARLIO trigger, with owner-check + auto-write-back strategy applied by the driver
  _dmaChan = WLEDPB_PARLIO_TX_DMA_CHAN(_txUnit);

  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  // 5.5 configures the channel with owner_check=false / auto_update_desc=false, we need that changed for our seamless ring refill
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

  // Hardware channel index for direct LL register access from the ISR. On AHB GDMA
  // (C6/H2/C5) the channel ID equals the register struct channel index. Queried via the
  // public (esp_private) API so no struct mirroring is needed. If it fails we fall back
  // to gdma_append() in the ISR (fine as long as CONFIG_GDMA_CTRL_FUNC_IN_IRAM=y or the
  // callback never runs with the flash cache off).
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

IRAM_ATTR bool ParlioBusContext::gdmaEofCallback(gdma_channel_handle_t /*dma_chan*/,
                                                 gdma_event_data_t* /*edata*/,
                                                 void* user_ctx) {
  ParlioBusContext* ctx = (ParlioBusContext*)user_ctx;
  if (ctx->_state != DriverState::Sending) return false; // stale wake-up between frames

  // Walk the ring from the fill head and refill every descriptor the DMA has handed back
  // to the CPU: auto write-back clears the owner bit exactly when a descriptor has been
  // consumed (descriptors never armed this frame are CPU-owned as well and simply get
  // filled next). Stops at the first DMA-owned descriptor. This makes the callback
  // idempotent: coalesced interrupts are caught up, and a stale wake-up between frames
  // either finds nothing to do or accidentally does the correct work.
  while (ctx->_state == DriverState::Sending) {
    const uint8_t idx = ctx->_fillHead;
    if (ctx->_desc[idx].dw0.owner != DMA_DESCRIPTOR_BUFFER_OWNER_CPU) break; // not consumed yet
    if (ctx->_resetBytesLeft == WLEDPB_PARLIO_XFER_DONE_FLAG) break;         // final buffer already queued, leave DMA stalled behind it
    ctx->fillBuffer(idx);
    ctx->_armDescriptor(idx);
    ctx->_fillHead = (uint8_t)((idx + 1) % WLEDPB_PARLIO_DMA_BUFFER_COUNT);
  }

  // If the DMA caught up with the CPU and paused on a CPU-owned descriptor, restart the
  // outlink. Use the always-inline LL register write instead of gdma_append(): gdma_append()
  // lives in flash unless CONFIG_GDMA_CTRL_FUNC_IN_IRAM=y (not the default, and unchangeable
  // on precompiled Arduino/Tasmota frameworks), while the GDMA ISR itself is IRAM-resident
  // and IRAM-flagged in those builds - calling flash from it panics with "Cache error"
  // during NVS writes. The LL TX restart helper compiles to a single MMIO write.
  if (ctx->_dmaChanIdx >= 0) {
    WLEDPB_PARLIO_GDMA_LL_TX_RESTART(WLEDPB_PARLIO_GDMA_DEV, (uint32_t)ctx->_dmaChanIdx);
  } else {
    gdma_append(ctx->_dmaChan); // channel index unknown: last resort
  }
  return false;
}

void IRAM_ATTR ParlioBusContext::_processChunkEof() {
  // Invoked from the IDF driver's PARLIO TX EOF ISR (via on_trans_done). TX EOF is
  // wire-accurate: tx_bytelen bytes have been clocked out. The driver ISR has already
  // stopped the TX engine and clock when we get here.
  if (_state != DriverState::Sending) return; // stale TX EOF from a previous frame
  if (_chunkBytesLeft == 0) {
    _state = DriverState::Idle; // whole frame clocked out, engine stays off until the next frame
    return;
  }

  // Frame longer than the 16-bit tx_bytelen register: seamlessly chain the next chunk.
  // The DMA ring kept feeding the FIFO across this boundary, so unlike the IDF driver's
  // transaction model there is no FIFO reset, no descriptor remount and no GDMA restart
  // here - just a few register writes. The outputs sit at the idle level only for the
  // ISR latency (~1-2us), far below the LED reset/latch threshold.
  const uint32_t chunk = _chunkBytesLeft > PARLIO_LL_TX_MAX_BYTES_PER_FRAME ? PARLIO_LL_TX_MAX_BYTES_PER_FRAME : _chunkBytesLeft;
  _chunkBytesLeft -= chunk;
  parlio_ll_tx_set_trans_bit_len(PARLIO_HW, chunk * 8);
  uint32_t wait = 100000; // FIFO still holds DMA'd data; bounded wait for safety
  while (!parlio_ll_tx_is_ready(PARLIO_HW) && --wait) {}
  parlio_ll_tx_start(PARLIO_HW, true);
  parlio_ll_tx_enable_clock(PARLIO_HW, true);
}

#else
//==============================================================================
// Fallback: single full-frame buffer, one transaction per frame
//==============================================================================

bool IRAM_ATTR ParlioBusContext::_queueBuffer(uint8_t bufIdx) {
  parlio_transmit_config_t trans_config = {
    .idle_value = _invertMask // hold lines at the reset level after the transaction (remove field if your IDF lacks it)
  };
  // note: payload size is in BITS, not bytes
  return parlio_tx_unit_transmit(_txUnit, _dmaBuffer[bufIdx], _txLen[bufIdx] * 8, &trans_config) == ESP_OK;
}
#endif // WLEDPB_PARLIO_SEAMLESS_DMA

//==============================================================================
// Buffer management & encoding
//==============================================================================

bool ParlioBusContext::_allocDmaBuffers() {
  if (_dmaBuffer[0] != nullptr) return true;

#if WLEDPB_PARLIO_SEAMLESS_DMA
  _bufferSize = (WLEDPB_PARLIO_DMABYTES * _maxSrcBytes) / WLEDPB_PARLIO_DMA_BUFFER_COUNT;
  _bufferSize = (_bufferSize + 3) & ~3;                               // align to 4 bytes
  if (_bufferSize > DEFAULT_DMA_BUFFER_SIZE) _bufferSize = DEFAULT_DMA_BUFFER_SIZE;
  // GDMA descriptors have 12-bit size/length fields (max 4095, 4-byte aligned -> 4092)
  if (_bufferSize > DMA_DESCRIPTOR_BUFFER_MAX_SIZE_4B_ALIGNED) _bufferSize = DMA_DESCRIPTOR_BUFFER_MAX_SIZE_4B_ALIGNED;
  if (_bufferSize < MIN_DMA_BUFFER_SIZE)     _bufferSize = MIN_DMA_BUFFER_SIZE;

  // allocate the descriptor-ring buffers (4-byte aligned for the GDMA engine)
  for (int i = 0; i < WLEDPB_PARLIO_DMA_BUFFER_COUNT; i++) {
    _dmaBuffer[i] = (uint8_t*)heap_caps_aligned_alloc(4, _bufferSize, MALLOC_CAP_DMA);
    if (!_dmaBuffer[i]) return false;
    memset(_dmaBuffer[i], 0, _bufferSize);
  }
#else
  // Fallback (P4): there is no byte-counter EOF to chain off, so don't stream several
  // small buffers through the transaction queue - each transaction boundary there is a
  // full pipeline restart (engine/clock off, FIFO reset), the exact cost the seamless
  // path exists to avoid on C6/H2/C5. Instead encode the whole frame (pixel data + the
  // trailing reset period) into one buffer up front and hand it to
  // parlio_tx_unit_transmit() in a single call: the IDF driver builds its own linked DMA
  // descriptor chain to cover it (sized via max_transfer_size in hwInit(), below), so the
  // transfer streams gap-free start to finish and completes exactly once, at the true end
  // of the frame. With the buffer sized to hold data+reset, fillBuffer() naturally takes
  // the "last buffer of the frame" path on the first (only) call, so hwStartTransfer()
  // queues exactly one transaction and the transmit-done callback just marks the frame
  // complete (no refill/re-queue logic in the fallback at all).
  // NOTE: this trades the small rotating buffers for one buffer sized for the entire
  // frame - for long strips this can be a lot of DMA-capable RAM (32x the source bytes).
  // If that doesn't fit in internal RAM, consider MALLOC_CAP_SPIRAM here (verify your IDF
  // version/target's GDMA can reach PSRAM before relying on it).
  _bufferSize = (size_t)WLEDPB_PARLIO_DMABYTES * _maxSrcBytes + _calcResetBytes();
  _bufferSize = (_bufferSize + 3) & ~3; // align to 4 bytes

  _dmaBuffer[0] = (uint8_t*)heap_caps_aligned_alloc(4, _bufferSize, MALLOC_CAP_DMA);
  if (!_dmaBuffer[0]) return false;
  memset(_dmaBuffer[0], 0, _bufferSize);
#endif

  _dmaAllocated = true;
  return true;
}

int8_t ParlioBusContext::registerChannel(int8_t pin, ParlioBus* bus, size_t srcBytes, bool inverted) {
  // Find free slot
  int8_t idx = -1;
  for (int i = 0; i < WLEDPB_PARLIO_MAX_CHANNELS; i++) {
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

  _channels[channelIdx] = {nullptr, -1, nullptr, 0, 0, false};
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

// encode4Step: 4-step cadence, converts per-channel byte streams to parallel DMA words.
// PARLIO outputs one byte across the 8 data lines per clock, in linear memory order:
// step0=HIGH, step1=data, step2=data, step3=LOW ('0' is 0b1000, '1' is 0b1110).
// Inversion is applied by XORing every step with _invertMask, replicating GPIO-matrix
// inversion: inverted channels emit the complementary waveform (and a HIGH reset period,
// since fillBuffer pre-fills the buffer with _invertMask instead of 0).
void IRAM_ATTR ParlioBusContext::encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel) {
  const uint8_t inv = _invertMask;
  for (size_t pos = 0; pos + 32 <= destLen; pos += 32) {
    // alwaysMask: channels with active data (HIGH step); bN: channels with bit N set
    uint8_t alwaysMask = 0;
    uint8_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;
    uint8_t b4 = 0, b5 = 0, b6 = 0, b7 = 0;

    for (int ch = 0; ch < maxChannel; ch++) {
      if (!_channels[ch].active) continue;
      if (_channels[ch].srcPos >= _channels[ch].srcLen) continue;
      const uint8_t m = (uint8_t)(1u << ch);
      alwaysMask |= m;
      const uint8_t b = _channels[ch].srcData[_channels[ch].srcPos++];
      // extract bits, unrolled for speed
      b0 |= m & (uint8_t)(0u - ((b >> 7) & 1u));
      b1 |= m & (uint8_t)(0u - ((b >> 6) & 1u));
      b2 |= m & (uint8_t)(0u - ((b >> 5) & 1u));
      b3 |= m & (uint8_t)(0u - ((b >> 4) & 1u));
      b4 |= m & (uint8_t)(0u - ((b >> 3) & 1u));
      b5 |= m & (uint8_t)(0u - ((b >> 2) & 1u));
      b6 |= m & (uint8_t)(0u - ((b >> 1) & 1u));
      b7 |= m & (uint8_t)(0u - ((b >> 0) & 1u));
    }
    if (!alwaysMask) break;  // no active channels produced data

    uint32_t* p = (uint32_t*)(dest + pos);
    // little-endian 32-bit store = memory bytes [step0, step1, step2, step3]
    const uint32_t s0 = (uint32_t)(alwaysMask ^ inv);
    const uint32_t s3 = (uint32_t)inv << 24;
    #define EMIT(bN, OFF) { \
      const uint32_t d = (uint32_t)((bN) ^ inv); \
      p[OFF] = s0 | (d << 8) | (d << 16) | s3; \
    }
    EMIT(b0, 0)  EMIT(b1, 1)  EMIT(b2, 2)  EMIT(b3, 3)
    EMIT(b4, 4)  EMIT(b5, 5)  EMIT(b6, 6)  EMIT(b7, 7)
    #undef EMIT
  }
}

// bytes of reset period appended at the end of a frame
uint32_t IRAM_ATTR ParlioBusContext::_calcResetBytes() const {
  uint32_t resetNs = _timing.reset_us * 1000;
  uint32_t bitPeriodNs = _timing.bitPeriod() + 1; // +1 to ensure no division by zero and slightly over-estimate the reset cycle
  uint32_t zeroCycles = resetNs / bitPeriodNs;
  return zeroCycles * (WLEDPB_PARLIO_DMABYTES / 8); // one LED bit cycle is 4 clocks, 1 buffer byte per clock
}

void IRAM_ATTR ParlioBusContext::fillBuffer(uint8_t bufIdx) {
  // pre-fill with _invertMask instead of 0: this is the "all lines idle" level, i.e. the reset
  // level (LOW for normal channels, HIGH for inverted channels, matching matrix inversion)
  memset(_dmaBuffer[bufIdx], _invertMask, _bufferSize);
  _endOfFrame[bufIdx] = false;

  if (_resetBytesLeft > 0) {
    // reset pulse continuation: this buffer is pure reset level. Long reset periods span
    // several buffers; only the last one is flagged as end of frame. Never let _txLen
    // exceed _bufferSize: the DMA would read past the buffer (can fault on C6's PMA).
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

  uint32_t translatedbytes = bytesToEncode * WLEDPB_PARLIO_DMABYTES;
  translatedbytes = translatedbytes > _bufferSize ? _bufferSize : translatedbytes;
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

  if (!_dmaAllocated) {
    if (!_allocDmaBuffers()) return false;
  }

  // (re)create the PARLIO unit: lazily on first transmit, or when channels changed
  if (!_txUnit || _unitStale) {
    hwDeinit();
    if (!hwInit()) return false;
  }

#if !WLEDPB_PARLIO_SEAMLESS_DMA
  _state = DriverState::Sending; // fallback: EOF callbacks do not gate on the state
#endif
  // seamless: _state is set to Sending at the end of hwStartTransfer(), after the ring
  // is fully built and the engine is running - until then EOF callbacks stay disabled

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
  // note: _state is still Idle here, so a stale GDMA/PARLIO EOF interrupt pending from
  // the previous frame is ignored by the callbacks while we rebuild (see gdmaEofCallback)
  gdma_stop(_dmaChan);
  _fillHead = 0;
  for (int i = 0; i < WLEDPB_PARLIO_DMA_BUFFER_COUNT; i++) {
    _desc[i].dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_CPU;
    fillBuffer(i);
    _armDescriptor(i);
    _fillHead = (uint8_t)((i + 1) % WLEDPB_PARLIO_DMA_BUFFER_COUNT);
    if (_endOfFrame[i]) break; // frame fits into the first buffers, rest of the ring stays disarmed
  }

  // mirror the IDF driver's transaction start sequence, minus the descriptor remount:
  // the ring stays linked, so the DMA streams across buffers without any CPU involvement
  parlio_ll_tx_set_idle_data_value(PARLIO_HW, _invertMask); // park lines at the reset level when idle
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
  // open the refill window only now: the first EOF can raise one buffer-duration from
  // here at the earliest, and the ring is fully built
  _state = DriverState::Sending;
  return true;
#else
  // Single-buffer fallback: fill buffer 0 with the whole frame (data + reset period,
  // the buffer is sized to hold both) and send it as one transaction. The IDF driver
  // builds the DMA descriptor chain for it and raises transmit-done once, at the end.
  fillBuffer(0);
  return _queueBuffer(0);
#endif
}

//==============================================================================
// ISR / transmit-done callback
//==============================================================================

IRAM_ATTR bool ParlioBusContext::dmaCallback(parlio_tx_unit_handle_t tx_unit,
                                             const parlio_tx_done_event_data_t* edata,
                                             void* user_ctx) {
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

// ============================================
// ParlioBus implementation
// ============================================

ParlioBus::ParlioBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t busNum, uint8_t ledType, size_t numPixels)
  : _pin(pin)
  , _timing(timing)
  , _inverted(false)
  , _initialized(false)
  , _busNum(busNum)
  , _channelIdx(-1)
  , _ctx(nullptr)
{
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
  _numPixels = numPixels; // stored so begin() can report srcBytes to the shared ParlioBusContext for DMA sizing
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
  const size_t srcBytes = (size_t)_numPixels * _encoder.getPixelBytes();
  _channelIdx = _ctx->registerChannel(_pin, this, srcBytes, _inverted);
  if (_channelIdx < 0) {
    ParlioBusContext::release(_busNum);
    _ctx = nullptr;
    return false;
  }

  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getPixelBytes())) { end(); return false; }
  return true;
}

// invert output signal, must be set before begin()
void ParlioBus::setInverted(bool inv) {
  _inverted = inv;
}

void ParlioBus::end() {
  if (!_initialized) return;

  if (_ctx) {
    // Wait for any active transmission to complete before cleanup
    while (!_ctx->isIdle()) vTaskDelay(1);
    _ctx->unregisterChannel(_channelIdx);
    ParlioBusContext::release(_busNum);
    _ctx = nullptr;
  }

  if (_encodeBuffer) {
    heap_caps_free(_encodeBuffer);
    _encodeBuffer = nullptr;
    _encodeBufferSize = 0;
  }

  _initialized = false;
}

bool ParlioBus::allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) {
  const size_t pixelBytes = padPixelBytesForSuffix((size_t)numPixels * numChannels, _ledType);
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

bool ParlioBus::show() {
  if (!_initialized || !_ctx || !_encodeBuffer || _numPixels == 0) return false;

  // Wait for previous transmission to complete
  while (!_ctx->isIdle()) {
    vTaskDelay(1);
  }

  // Send already-encoded buffer directly
  _ctx->setChannelData(_channelIdx, _encodeBuffer, _encodeBufferSize);
  return _ctx->startTransmit();
}

bool ParlioBus::canShow() const {
  if (!_ctx) return true;
  return _ctx->isIdle();
}

void ParlioBus::setColorOrder(uint8_t co) {
  _encoder = ColorEncoder(co, _encoder.getColorChannels(), _ledType);
}

} // namespace WLEDpixelBus
#endif // WLEDPB_PARLIO_SUPPORT