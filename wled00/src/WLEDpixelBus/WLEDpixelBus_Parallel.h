/*-------------------------------------------------------------------------

WLEDpixelBus - shared base classes and helpers for the parallel output drivers

written by Damian Schneider @dedehai 2026

The parallel output drivers (I2S/LCD, PARLIO and parallel SPI) all use the same
architecture: one shared peripheral context owns a ring of DMA buffers and one
"channel" per output pin, and every bus of the same kind shares that context.

This file holds everything the three of them have in common:

  * ParallelChannel     - the per-pin record (was a per-driver "ChannelData" with
                           three different field orders)
  * ParallelBusContext  - channel registry, staging mask, transfer state machine
                           and the stall watchdog
  * ParallelBusBase     - the PixelBus half (begin/end/show/canShow)
  * the 4-step cadence encoder and the DMA buffer sizing helpers

Everything peripheral specific stays in the driver: GPIO routing, the DMA
descriptor layout, the interrupt handlers and the actual transfer start.

-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"

namespace WLEDpixelBus {

// One output pin of a parallel bus.
// Initialize with ParallelChannel{} and set fields explicitly; do not rely on
// the field order.
struct ParallelChannel {
  int8_t pin = -1;
  const uint8_t* srcData = nullptr;
  size_t srcLen = 0;
  size_t srcPos = 0;
  bool active = false;
  bool inverted = false;
};

// Base class for the parallel output contexts.
//
// MaxChannels is a template parameter so that the channel array is sized
// exactly as the individual driver needs it (8/16 for I2S, 8 for PARLIO,
// 4 for parallel SPI) - no RAM is wasted and no runtime lookup is needed.
template <uint8_t MaxChannels>
class ParallelBusContext {
public:
  virtual ~ParallelBusContext() {}

  int8_t registerChannel(int8_t pin, size_t srcBytes, bool inverted = false);
  void   unregisterChannel(int8_t channelIdx);
  uint8_t getChannelCount() const { return _channelCount; }
  void setChannelData(int8_t channelIdx, const uint8_t* data, size_t len);

  bool isIdle() const { return _state == DriverState::Idle; }
  virtual bool ensureIdle();   // non-blocking completion check with timeout recovery
  void waitForIdle();           // bounded wait; aborts a stalled transfer before returning
  void abortTransmit();         // abort on timeout (just in case to avoid driver getting stuck)

  bool init(const LedTiming& timing);
  void deinit();

protected:
  // --- hooks the driver has to provide ------------------------------------
  virtual bool hwInit() = 0;           // set up the peripheral, _timing is already valid
  virtual void hwDeinit() = 0;         // release the peripheral
  virtual void releaseResources() {}   // free the driver owned DMA buffers/descriptors
  virtual void stopTransferHw() {}      // stop an in-flight transfer
  virtual void abortTransferHw() { stopTransferHw(); } // stop an in-flight transfer
  virtual void channelAttached(int8_t idx, int8_t pin, bool inverted) = 0; // route the pin
  virtual void channelDetached(int8_t idx, int8_t pin) {}                  // release the pin
  virtual void onChannelDataStaged(int8_t idx, size_t len) {}              // driver bookkeeping

  // --- shared state --------------------------------------------------------
  ParallelChannel _channels[MaxChannels];
  uint8_t  _channelCount = 0;
  uint16_t _channelMask = 0;
  uint16_t _stagedMask = 0;
  size_t   _maxSrcBytes = 0;    // largest source byte count across all channels, drives DMA sizing
  size_t   _bufferSize = 0;     // actual allocated DMA buffer size (per buffer)
  bool     _dmaAllocated = false;
  bool     _initialized = false;
  volatile DriverState _state = DriverState::Idle;
  volatile uint32_t _txStartMillis = 0;  // millis() when the current transfer started, watchdog
  volatile size_t _resetBytesLeft = 0;
  LedTiming _timing = LedTiming(300, 900, 700, 500, 300); // overwritten by init()
};

// ----------------------------------------------------------------------------
// ParallelBusContext implementation
// ----------------------------------------------------------------------------

template <uint8_t MaxChannels>
int8_t ParallelBusContext<MaxChannels>::registerChannel(int8_t pin, size_t srcBytes, bool inverted) {
  // find a free slot
  int8_t idx = -1;
  for (uint8_t i = 0; i < MaxChannels; i++) {
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
  _channelMask |= (1u << idx);

  // track the largest source byte count across channels; used to size the DMA buffers
  if (srcBytes > _maxSrcBytes) _maxSrcBytes = srcBytes;

  channelAttached(idx, pin, inverted); // driver specific pin routing

  return idx;
}

template <uint8_t MaxChannels>
void ParallelBusContext<MaxChannels>::unregisterChannel(int8_t channelIdx) {
  if (channelIdx < 0 || channelIdx >= MaxChannels) return;
  if (!_channels[channelIdx].active) return;

  channelDetached(channelIdx, _channels[channelIdx].pin); // driver specific pin release

  _channels[channelIdx] = ParallelChannel{};
  _channelCount--;
  _channelMask &= ~(1u << channelIdx);
  _stagedMask &= ~(1u << channelIdx);
}

template <uint8_t MaxChannels>
void ParallelBusContext<MaxChannels>::setChannelData(int8_t channelIdx, const uint8_t* data, size_t len) {
  if (channelIdx < 0 || channelIdx >= MaxChannels) return;

  _channels[channelIdx].srcData = data;
  _channels[channelIdx].srcLen = len;
  _channels[channelIdx].srcPos = 0;

  onChannelDataStaged(channelIdx, len); // driver specific bookkeeping

   // Safety: If this channel was already staged, it means we somehow missed triggering startTransmit()
  if (_stagedMask & (1u << channelIdx)) _stagedMask = 0; // reset the stage mask to resync (should not happen)
  _stagedMask |= (1u << channelIdx);
}

template <uint8_t MaxChannels>
bool ParallelBusContext<MaxChannels>::ensureIdle() {
  if (isIdle()) return true;
  if ((uint32_t)(millis() - _txStartMillis) <= WLEDPB_TRANSFER_TIMEOUT_MS) return false;
  abortTransmit();
  return true;
}

template <uint8_t MaxChannels>
void ParallelBusContext<MaxChannels>::waitForIdle() {
  const uint32_t start = millis();
  while (!ensureIdle() && (uint32_t)(millis() - start) <= WLEDPB_TRANSFER_TIMEOUT_MS) {
    vTaskDelay(1);
  }
  if (!isIdle()) abortTransmit();
}

template <uint8_t MaxChannels>
void ParallelBusContext<MaxChannels>::abortTransmit() {
  abortTransferHw();   // driver specific: stop the DMA and release the pins
  _state = DriverState::Idle;
  _stagedMask = 0;
  _resetBytesLeft = 0;
}

template <uint8_t MaxChannels>
bool ParallelBusContext<MaxChannels>::init(const LedTiming& timing) {
  if (_initialized) return true;

  _timing = timing;
  if (!hwInit()) {
    deinit();
    return false;
  }
  _initialized = true;
  return true;
}

template <uint8_t MaxChannels>
void ParallelBusContext<MaxChannels>::deinit() {
  waitForIdle();
  abortTransmit();     // stops the hardware and releases the pins
  releaseResources();  // frees the driver owned DMA buffers/descriptors
  hwDeinit();          // releases the peripheral
  _dmaAllocated = false;
  _initialized = false;
}

// ----------------------------------------------------------------------------
// ParallelBusBase - the PixelBus half, shared by all parallel drivers
// ----------------------------------------------------------------------------

template <class Ctx>
class ParallelBusBase : public PixelBus {
public:
  ~ParallelBusBase() override { end(); }

  bool begin() override {
    if (_initialized) return true;

    _ctx = Ctx::get(_busNum);
    if (!_ctx) return false;

    if (!_ctx->init(_timing)) {
      Ctx::release(_busNum);
      _ctx = nullptr;
      return false;
    }

    // pass our encoded byte count so the context can size the DMA buffers for the largest bus
    const size_t srcBytes = (size_t)_numPixels * _encoder.getBytesPerPixel() + _prefixLen + _suffixLen;
    _channelIdx = _ctx->registerChannel(_pin, srcBytes, _inverted);
    if (_channelIdx < 0) {
      Ctx::release(_busNum);
      _ctx = nullptr;
      return false;
    }

    _initialized = true;
    if (!allocateEncodeBuffer(_numPixels, _encoder.getBytesPerPixel())) { end(); return false; }
    return true;
  }

  void end() override {
    if (_ctx) {
      _ctx->waitForIdle();
      _ctx->unregisterChannel(_channelIdx);
      Ctx::release(_busNum);
      _ctx = nullptr;
    }
    releaseEncodeBuffer();
    _initialized = false;
  }

  bool show() override {
    if (!_initialized || !_ctx || !_encodeBuffer || _numPixels == 0) return false;
    _ctx->waitForIdle();
    _ctx->setChannelData(_channelIdx, _encodeBuffer, _encodeBufferSize);
    return _ctx->startTransmit();
  }

  bool canShow() const override {
    if (!_initialized) return true; // do not block if init failed! busmanager waits until all buses can show
    return _ctx && _ctx->ensureIdle();
  }

protected:
  // note: a mem-initializer may not name a base class member, so the derived
  // classes pass their configuration through this constructor
  ParallelBusBase(int8_t pin, const LedTiming& timing, uint8_t busNum)
    : _pin(pin)
    , _timing(timing)
    , _busNum(busNum)
  {}

  int8_t _pin = -1;
  LedTiming _timing = LedTiming(300, 900, 700, 500, 300); // overwritten by the constructor
  bool _initialized = false;
  uint8_t _busNum = 0;
  int8_t _channelIdx = -1;
  Ctx* _ctx = nullptr;
};

// ----------------------------------------------------------------------------
// 4-step cadence helpers for the parallel buses (I2S, PARLIO, parallel SPI)
// ----------------------------------------------------------------------------

// Compute target clock frequency in Hz for 4-step cadence (4 clock cycles per bit period).
inline uint32_t calc4StepClockHz(const LedTiming& timing) {
  uint32_t period = timing.bitPeriod();
  if (period == 0) period = 1250;
  return 4000000000UL / period;
}

// Compute number of DMA bytes for the reset period appended to a frame.
// dmaBytesPerLedBit = bytes consumed in DMA for one full 4-step LED bit i.e. one bitPeriod (called from ISR context)
inline __attribute__((always_inline)) size_t calc4StepResetDmaBytes(const LedTiming& timing, uint8_t dmaBytesPerLedBit) {
  uint32_t resetNs = timing.reset_us * 1000;
  uint32_t bitPeriodNs = timing.bitPeriod() + 1; // +1 to prevent div by zero
  uint32_t zeroCycles = resetNs / bitPeriodNs;
  return (size_t)zeroCycles * dmaBytesPerLedBit + 4; // add 4 bytes to ensure non zero and for safety
}

// Compute a DMA buffer size aligned to 4 bytes, clamped to bounds and source-byte groups
inline size_t calc4StepBufferSize(size_t maxSrcBytes, uint8_t dmaBytesPerSrcByte, uint8_t bufCount, size_t minSize, size_t maxSize) {
  if (bufCount == 0) bufCount = 1;
  size_t sz = (dmaBytesPerSrcByte * maxSrcBytes) / bufCount;
  sz = (sz + 3) & ~3; // 4-byte align
  if (sz > maxSize) sz = maxSize;
  if (sz < minSize) sz = minSize;
  // note: dmaBytesPerSrcByte must be a multiple of 4 for proper alignment (true for current 4-step encoding: its 16, 32 or 64 bytes)
  sz -= sz % dmaBytesPerSrcByte; // align to src byte groups (see xxx_DMABYTES)
  return sz;
}

// 4-step cadence encoder: expands each source byte into 4 DMA "steps" of 8 channels.
// '0' is 0b1000 (1/4 high), '1' is 0b1110 (3/4 high).
//
// Layout:
//   Standard     - no swap, memory [S0,S1,S2,S3] = [HIGH,data,data,LOW]
//   WordSwapped  - half-word swap, memory [S2,S3,S0,S1] = [data,0,HIGH,data]
enum class Step4Layout : uint8_t { Standard, WordSwapped };

template <typename MASK, Step4Layout Layout>
inline __attribute__((always_inline)) void encode4Step8(uint8_t* dest, size_t destLen, uint8_t maxChannel, ParallelChannel* channels) {
  for (size_t pos = 0; pos + 32 <= destLen; pos += 32) {
    MASK alwaysMask = 0;
    MASK b0 = 0, b1 = 0, b2 = 0, b3 = 0;
    MASK b4 = 0, b5 = 0, b6 = 0, b7 = 0;

    for (uint8_t ch = 0; ch < maxChannel; ch++) {
      if (!channels[ch].active) continue;
      if (channels[ch].srcPos >= channels[ch].srcLen) continue;
      const MASK m = (MASK)(1u << ch);
      alwaysMask |= m;
      const uint8_t b = channels[ch].srcData[channels[ch].srcPos++];
      b0 |= m & (MASK)(0u - ((b >> 7) & 1u));
      b1 |= m & (MASK)(0u - ((b >> 6) & 1u));
      b2 |= m & (MASK)(0u - ((b >> 5) & 1u));
      b3 |= m & (MASK)(0u - ((b >> 4) & 1u));
      b4 |= m & (MASK)(0u - ((b >> 3) & 1u));
      b5 |= m & (MASK)(0u - ((b >> 2) & 1u));
      b6 |= m & (MASK)(0u - ((b >> 1) & 1u));
      b7 |= m & (MASK)(0u - ((b >> 0) & 1u));
    }
    if (!alwaysMask) break; // no active channels produced data

    uint32_t* p = (uint32_t*)(dest + pos);
    if constexpr (Layout == Step4Layout::Standard) {
      // no swap, memory [S0,S1,S2,S3] = [HIGH,data,data,LOW]
      p[0] = (uint32_t)(alwaysMask) | ((uint32_t)(b0) << 8) | ((uint32_t)(b0) << 16);
      p[1] = (uint32_t)(alwaysMask) | ((uint32_t)(b1) << 8) | ((uint32_t)(b1) << 16);
      p[2] = (uint32_t)(alwaysMask) | ((uint32_t)(b2) << 8) | ((uint32_t)(b2) << 16);
      p[3] = (uint32_t)(alwaysMask) | ((uint32_t)(b3) << 8) | ((uint32_t)(b3) << 16);
      p[4] = (uint32_t)(alwaysMask) | ((uint32_t)(b4) << 8) | ((uint32_t)(b4) << 16);
      p[5] = (uint32_t)(alwaysMask) | ((uint32_t)(b5) << 8) | ((uint32_t)(b5) << 16);
      p[6] = (uint32_t)(alwaysMask) | ((uint32_t)(b6) << 8) | ((uint32_t)(b6) << 16);
      p[7] = (uint32_t)(alwaysMask) | ((uint32_t)(b7) << 8) | ((uint32_t)(b7) << 16);
    } else {
      // half-word swap, memory [S2,S3,S0,S1] = [data,0,HIGH,data] (ESP32 classic)
      const uint32_t AH = (uint32_t)alwaysMask << 16;
      p[0] = AH | (uint32_t)(b0) | ((uint32_t)(b0) << 24);
      p[1] = AH | (uint32_t)(b1) | ((uint32_t)(b1) << 24);
      p[2] = AH | (uint32_t)(b2) | ((uint32_t)(b2) << 24);
      p[3] = AH | (uint32_t)(b3) | ((uint32_t)(b3) << 24);
      p[4] = AH | (uint32_t)(b4) | ((uint32_t)(b4) << 24);
      p[5] = AH | (uint32_t)(b5) | ((uint32_t)(b5) << 24);
      p[6] = AH | (uint32_t)(b6) | ((uint32_t)(b6) << 24);
      p[7] = AH | (uint32_t)(b7) | ((uint32_t)(b7) << 24);
    }
  }
}

// Encode 16 parallel channels using the target-specific 4-step DMA word layout
template <Step4Layout Layout>
inline __attribute__((always_inline)) void encode4Step16(uint8_t* dest, size_t destLen, uint8_t maxChannel, ParallelChannel* channels) {
  for (size_t pos = 0; pos + 64 <= destLen; pos += 64) {
    uint16_t alwaysMask = 0;
    uint16_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;
    uint16_t b4 = 0, b5 = 0, b6 = 0, b7 = 0;

    for (unsigned ch = 0; ch < maxChannel; ch++) {
      ParallelChannel& c = channels[ch];
      if (!c.active || c.srcPos >= c.srcLen) continue;
      const uint16_t m = (uint16_t)(1u << ch);
      alwaysMask |= m;
      const uint8_t b = c.srcData[c.srcPos++];
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

    uint32_t* p = (uint32_t*)(dest + pos);
    if constexpr (Layout == Step4Layout::Standard) {
      // no swap: word 0 = [S0=HIGH, S1=data], word 1 = [S2=data, S3=LOW]
      p[0]  = ((uint32_t)b0 << 16) | (uint32_t)alwaysMask;  p[1]  = (uint32_t)b0;
      p[2]  = ((uint32_t)b1 << 16) | (uint32_t)alwaysMask;  p[3]  = (uint32_t)b1;
      p[4]  = ((uint32_t)b2 << 16) | (uint32_t)alwaysMask;  p[5]  = (uint32_t)b2;
      p[6]  = ((uint32_t)b3 << 16) | (uint32_t)alwaysMask;  p[7]  = (uint32_t)b3;
      p[8]  = ((uint32_t)b4 << 16) | (uint32_t)alwaysMask;  p[9]  = (uint32_t)b4;
      p[10] = ((uint32_t)b5 << 16) | (uint32_t)alwaysMask;  p[11] = (uint32_t)b5;
      p[12] = ((uint32_t)b6 << 16) | (uint32_t)alwaysMask;  p[13] = (uint32_t)b6;
      p[14] = ((uint32_t)b7 << 16) | (uint32_t)alwaysMask;  p[15] = (uint32_t)b7;
    } else {
      // half-word swap (classic ESP32): word 0 = [HIGH in upper, data in lower], word 1 = [data in upper, 0]
      const uint32_t ah = (uint32_t)alwaysMask << 16;
      p[0]  = ah | (uint32_t)b0;  p[1]  = (uint32_t)b0 << 16;
      p[2]  = ah | (uint32_t)b1;  p[3]  = (uint32_t)b1 << 16;
      p[4]  = ah | (uint32_t)b2;  p[5]  = (uint32_t)b2 << 16;
      p[6]  = ah | (uint32_t)b3;  p[7]  = (uint32_t)b3 << 16;
      p[8]  = ah | (uint32_t)b4;  p[9]  = (uint32_t)b4 << 16;
      p[10] = ah | (uint32_t)b5;  p[11] = (uint32_t)b5 << 16;
      p[12] = ah | (uint32_t)b6;  p[13] = (uint32_t)b6 << 16;
      p[14] = ah | (uint32_t)b7;  p[15] = (uint32_t)b7 << 16;
    }
  }
}

// Encode source bytes as 4-bit quad-SPI symbols; dest must be zeroed for lane ORing.
template <uint16_t ZeroBit, uint16_t OneBit>
inline __attribute__((always_inline)) void encode4Step4(uint16_t* dest, size_t srcOffset, size_t srcBytes, uint8_t maxChannel, ParallelChannel* channels) {
  for (unsigned lane = 0; lane < maxChannel; lane++) {
    const ParallelChannel& c = channels[lane];
    if (!c.active || !c.srcData || srcOffset >= c.srcLen) continue;

    size_t validBytes = c.srcLen - srcOffset;
    if (validBytes > srcBytes) validBytes = srcBytes;

    const uint16_t zeroBit = (uint16_t)(ZeroBit << lane);
    const uint16_t oneBit  = (uint16_t)(OneBit  << lane);
    const uint8_t* src = c.srcData + srcOffset;
    uint16_t* pOut = dest;

    for (size_t i = 0; i < validBytes; i++) {
      const uint8_t b = src[i];
      *pOut++ |= (b & 0x80) ? oneBit : zeroBit;
      *pOut++ |= (b & 0x40) ? oneBit : zeroBit;
      *pOut++ |= (b & 0x20) ? oneBit : zeroBit;
      *pOut++ |= (b & 0x10) ? oneBit : zeroBit;
      *pOut++ |= (b & 0x08) ? oneBit : zeroBit;
      *pOut++ |= (b & 0x04) ? oneBit : zeroBit;
      *pOut++ |= (b & 0x02) ? oneBit : zeroBit;
      *pOut++ |= (b & 0x01) ? oneBit : zeroBit;
    }
  }
}

} // namespace WLEDpixelBus
