/*-------------------------------------------------------------------------

WLEDpixelBus - ESP8266 driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

Supports UART and I2S DMA output as well as parallel bit-banging

-------------------------------------------------------------------------*/

#include "WLEDpixelBus_ESP8266.h"

#ifdef WLEDPB_ESP8266

#include <Arduino.h>
#include <HardwareSerial.h>
#include <uart_register.h>
#include <eagle_soc.h>
#include <esp8266_peri.h>
#include <i2s_reg.h>
#include <slc_register.h>
#include <user_interface.h>
#include <ets_sys.h>

namespace WLEDpixelBus {

//==============================================================================
// ESP8266 UART Bus
//==============================================================================

// Global static tracking for the shared UART ISR
Esp8266UartBus* Esp8266UartBus::s_instances[2] = {nullptr, nullptr};

Esp8266UartBus::Esp8266UartBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType)
  : _pin(pin), _timing(timing), _initialized(false),
    _asyncBuf(nullptr), _asyncBufEnd(nullptr) {
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
}

Esp8266UartBus::~Esp8266UartBus() {
  end();
}

// Shared Interrupt Service Routine (must be in IRAM)
void IRAM_ATTR Esp8266UartBus::UartIsr(void* arg, void* exceptionFrame) {
  for (uint8_t uartNum = 0; uartNum < 2; uartNum++) {
    Esp8266UartBus* instance = s_instances[uartNum];

    // Check if this UART triggered a TX FIFO Empty interrupt
    if (instance && (USIS(uartNum) & (1 << UIFE))) {
      // Logic for bit expansion (Replaces the LUT)
      const uint8_t uartData[4] = {0b110111, 0b000111, 0b110100, 0b000100};

      // Calculate remaining space in the 128-byte hardware FIFO
      uint8_t avail = (128 - ((USS(uartNum) >> USTXC) & 0xff)) / 4;

      while (avail > 0 && instance->_asyncBuf < instance->_asyncBufEnd) {
        uint8_t v = *instance->_asyncBuf++;
        USF(uartNum) = uartData[(v >> 6) & 0x03];
        USF(uartNum) = uartData[(v >> 4) & 0x03];
        USF(uartNum) = uartData[(v >> 2) & 0x03];
        USF(uartNum) = uartData[v & 0x03];
        avail--;
      }

      // If finished, disable interrupt for this UART
      if (instance->_asyncBuf >= instance->_asyncBufEnd) {
        USIE(uartNum) &= ~(1 << UIFE);
      }

      // Clear all interrupt flags for this UART
      USIC(uartNum) = 0xffff;
    }
  }
}

bool Esp8266UartBus::begin() {
  if (_initialized) return true;
  if (_pin != 1 && _pin != 2) return false;
  uint8_t uartNum = (_pin == 2) ? 1 : 0;
  s_instances[uartNum] = this;

  // set LED timing and (re)init the serial
  updateUartTiming();

  ETS_UART_INTR_DISABLE();
  // Attach the shared ISR
  ETS_UART_INTR_ATTACH(UartIsr, nullptr);

  // Set threshold: Interrupt when FIFO drops below 80 bytes
  USC1(uartNum) = (80 << UCFET);
  USIC(uartNum) = 0xffff; // Clear pending
  USIE(uartNum) &= ~((1 << UIFF) | (1 << UIFE)); // Start with interrupts off
  ETS_UART_INTR_ENABLE();

  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getPixelBytes())) { end(); return false; }
  return true;
}

void Esp8266UartBus::end() {
  if (!_initialized) return;
  uint8_t uartNum = (_pin == 2) ? 1 : 0;

  ETS_UART_INTR_DISABLE();
  USIE(uartNum) = 0;
  s_instances[uartNum] = nullptr;

  // If no buses are left, detach ISR
  if (s_instances[0] == nullptr && s_instances[1] == nullptr) {
    ETS_UART_INTR_ATTACH(nullptr, nullptr);
  } else {
    ETS_UART_INTR_ENABLE();
  }

  if (_pin == 2) Serial1.end();
  else Serial.end();
  _initialized = false;
}

void Esp8266UartBus::updateUartTiming() {
  uint32_t periodNs = _timing.bitPeriod(); 
  if (periodNs < 200) periodNs = 1250;
  uint32_t baud = 4000000000ULL / periodNs;

  uint8_t uartNum = (_pin == 2) ? 1 : 0;
  if (uartNum == 1) {
    Serial1.begin(baud, SERIAL_6N1, SERIAL_TX_ONLY);
  } else {
    Serial.begin(baud, SERIAL_6N1, SERIAL_TX_ONLY);
  }
  
  const uint32_t fifoResetFlags = (1 << UCTXRST) | (1 << UCRXRST);
  USC0(uartNum) |= fifoResetFlags;
  USC0(uartNum) &= ~(fifoResetFlags);
  USC0(uartNum) &= ~((1 << UCDTRI) | (1 << UCRTSI) | (1 << UCTXI) | (1 << UCDSRI) | (1 << UCCTSI) | (1 << UCRXI)); // clear invert bits
  USC0(uartNum) |= (1 << UCTXI); // invert TX -> idle low   TODO: need to handle inverted LED output for custom bus
}

bool Esp8266UartBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;
  if (!canShow()) return false; // TODO: is this consistent accross all drivers? i.e. return instead of wait?
  // workaround for a bug: after bootup, something changes the pin mux AFTER the bus is initialized, breaking the UART output. it only starts working after a bus re-init. could not find out what causes it.
  // since pinMode() is computationally cheap, it is an acceptable fix
  pinMode(_pin, SPECIAL); 

  uint8_t uartNum = (_pin == 2) ? 1 : 0;
  _asyncBuf = _encodeBuffer;
  _asyncBufEnd = _encodeBuffer + _encodeBufferSize;

  // note: no initial fill required, the ISR will fire and fill the FIFO
  // Enable the "TX FIFO Empty" interrupt to trigger the ISR
  USIE(uartNum) |= (1 << UIFE);

  return true;
}

void Esp8266UartBus::setColorOrder(uint8_t co) {
  _encoder = ColorEncoder(co, _encoder.getColorChannels(), _ledType);
}

bool Esp8266UartBus::canShow() const {
  if (!_initialized) return false;
  // Ready if we have no more data to send
  if (_asyncBuf < _asyncBufEnd) return false;
  // also FIFO must have physically drained
  uint8_t uartNum = (_pin == 2) ? 1 : 0;
  if (((USS(uartNum) >> USTXC) & 0xff) > 0) return false;
  return true;
}


//==============================================================================
// ESP8266 BitBang Bus (parallel output across multiple pins)
//==============================================================================

// Static member definitions
int8_t   Esp8266BitBangBus::s_pins[WLED_MAX_BB_CHANNELS]      = {};
uint16_t Esp8266BitBangBus::s_numPixels[WLED_MAX_BB_CHANNELS] = {};
uint8_t* Esp8266BitBangBus::s_pixelData[WLED_MAX_BB_CHANNELS] = {};
uint8_t  Esp8266BitBangBus::s_channelCount = 0;
uint32_t Esp8266BitBangBus::s_allMask      = 0;
uint8_t  Esp8266BitBangBus::s_stagedCount  = 0;
uint32_t Esp8266BitBangBus::s_t0h          = 0;
uint32_t Esp8266BitBangBus::s_t1h          = 0;
uint32_t Esp8266BitBangBus::s_period       = 0;
uint8_t  Esp8266BitBangBus::s_pixelBytes   = 0;

Esp8266BitBangBus::Esp8266BitBangBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType)
  : _pin(pin), _timing(timing), _initialized(false) {
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
}

Esp8266BitBangBus::~Esp8266BitBangBus() {
  end();
}

bool Esp8266BitBangBus::begin() {
  if (_initialized) return true;
  if (_pin < 0 || _pin > 15) return false; // Only GPIO 0-15 are output-capable on ESP8266
  if (s_channelCount >= WLED_MAX_BB_CHANNELS) return false;

  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);

  // Convert nanosecond timings to CPU cycles.
  // Subtract 1 cycle for the while-loop test overhead (same correction as ESP32).
  const uint32_t cpuMHz = ESP.getCpuFreqMHz(); // 80 or 160

  uint32_t t0h    = (_timing.t0h_ns  * cpuMHz) / 1000u;
  uint32_t t1h    = (_timing.t1h_ns  * cpuMHz) / 1000u;
  t0h             = (t0h    > 1u) ? t0h    - 1u : 0u;
  t1h             = (t1h    > 1u) ? t1h    - 1u : 0u;
  uint32_t period = (_timing.bitPeriod() * cpuMHz) / 1000u;
  period          = (period > 1u) ? period - 1u : 0u;

  // Register in the shared static table
  const uint8_t idx   = s_channelCount;
  s_pins[idx]         = _pin;
  s_allMask          |= (1u << _pin);
  s_channelCount++;

  if (!allocateEncodeBuffer(_numPixels, _encoder.getPixelBytes())) {
    s_channelCount--;
    s_allMask &= ~(1u << _pin);
    return false;
  }

  s_numPixels[idx] = _numPixels;
  s_pixelData[idx] = _pixelData;
  // Timing is shared — same for all channels (enforced by PixelBusAllocator).
  s_t0h        = t0h;
  s_t1h        = t1h;
  s_period     = period;
  s_pixelBytes = _encoder.getPixelBytes();

  _initialized = true;
  return true;
}

void Esp8266BitBangBus::end() {
  if (_initialized) {
    uint8_t slot = s_channelCount; // sentinel
    for (uint8_t i = 0; i < s_channelCount; i++) {
      if (s_pins[i] == _pin) { slot = i; break; }
    }
    if (slot < s_channelCount) {
      for (uint8_t i = slot; i + 1 < s_channelCount; i++) {
        s_pins[i]      = s_pins[i + 1];
        s_numPixels[i] = s_numPixels[i + 1];
        s_pixelData[i] = s_pixelData[i + 1];
      }
      s_channelCount--;
      s_allMask = 0;
      for (uint8_t i = 0; i < s_channelCount; i++) s_allMask |= (1u << s_pins[i]);
      s_stagedCount = 0;
    }
  }
  pinMode(_pin, INPUT);
  _initialized = false;

  if (_encodeBuffer) {
    free(_encodeBuffer);
    _encodeBuffer = nullptr;
    _pixelData    = nullptr;
  }
}

void Esp8266BitBangBus::setTiming(const LedTiming& timing) {
  _timing = timing;
  // Recompute cycle counts if already initialized
  if (_initialized) {
    const uint32_t cpuMHz = ESP.getCpuFreqMHz();
    uint32_t t0h    = (_timing.t0h_ns  * cpuMHz) / 1000u;
    uint32_t t1h    = (_timing.t1h_ns  * cpuMHz) / 1000u;
    t0h             = (t0h    > 1u) ? t0h    - 1u : 0u;
    t1h             = (t1h    > 1u) ? t1h    - 1u : 0u;
    uint32_t period = (_timing.bitPeriod() * cpuMHz) / 1000u;
    period          = (period > 1u) ? period - 1u : 0u;
    s_t0h   = t0h;
    s_t1h   = t1h;
    s_period = period;
  }
}

// Stage this channel's data. When all channels have staged, output all in parallel.
IRAM_ATTR bool Esp8266BitBangBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_pixelData) return false;

  s_stagedCount++;
  if (s_stagedCount < s_channelCount) {
    return true; // not all channels ready yet
  }

  // Last channel staged — output all channels in parallel
  s_stagedCount = 0;
  return outputParallel();
}

void Esp8266BitBangBus::setColorOrder(uint8_t co) {
  _encoder = ColorEncoder(co, _encoder.getColorChannels(), _ledType);
}

// ---------------------------------------------------------------------------
// resetChannels() — called by PixelBusAllocator::resetChannelTracking()
// ---------------------------------------------------------------------------
void Esp8266BitBangBus::resetChannels() {
  s_channelCount = 0;
  s_allMask      = 0;
  s_stagedCount  = 0;
  s_t0h          = 0;
  s_t1h          = 0;
  s_period       = 0;
  s_pixelBytes   = 0;
  memset(s_pins,      0, sizeof(s_pins));
  memset(s_numPixels, 0, sizeof(s_numPixels));
  memset(s_pixelData, 0, sizeof(s_pixelData));
}

// ---------------------------------------------------------------------------
// outputParallel() — the hot path, must live in IRAM.
//
// Per-bit sequence (identical to ESP32 BitBangBus, single GPIO bank):
//   1. Compute zeroMask for the current bit across all channels.
//   2. Wait for the full bit period since the previous HIGH edge.
//   3. Set all output pins HIGH simultaneously.
//   4. After T0H cycles: pull '0' outputs LOW.
//   5. After T1H cycles: pull all remaining outputs LOW.
//
// Interrupts are locked for the full duration using os_intr_lock() /
// os_intr_unlock() (the SDK equivalent of portENTER/EXIT_CRITICAL on ESP8266).
// The total lock duration equals the previous sequential approach for a single
// channel; for N channels it is identical (all clocked in parallel, same time).
// ---------------------------------------------------------------------------
bool IRAM_ATTR Esp8266BitBangBus::outputParallel() {
  if (s_channelCount == 0) return true;

  const uint32_t t0h        = s_t0h;
  const uint32_t t1h        = s_t1h;
  const uint32_t period     = s_period;
  const uint32_t setAllMask = s_allMask;
  const uint8_t  pixelBytes = s_pixelBytes;
  const uint8_t  nCh        = s_channelCount;

  // Find maximum pixel count across all channels
  uint16_t maxPixels = 0;
  for (uint8_t ch = 0; ch < nCh; ch++) {
    if (s_numPixels[ch] > maxPixels) maxPixels = s_numPixels[ch];
  }
  if (maxPixels == 0) return true;

  const uint32_t totalBits = (uint32_t)maxPixels * pixelBytes * 8u;

  // Pre-compute per-channel pin masks and total byte extents
  uint32_t chanPinMask[nCh];
  uint32_t chanTotalBytes[nCh];
  for (uint8_t ch = 0; ch < nCh; ch++) {
    chanPinMask[ch]    = 1u << s_pins[ch];
    chanTotalBytes[ch] = (uint32_t)s_numPixels[ch] * pixelBytes;
  }

  // Inline lambda equivalent: compute bitmask of channels outputting '0' for this bit
  // (channels past their data end also output '0').
  // MSB-first within each byte.
  // TODO: there may be room to speed this up, it is quite slow and takes like 2us
  auto computeZeroMask = [&](uint32_t bitIndex) -> uint32_t {
    const uint32_t byteIndex = bitIndex >> 3;
    const uint8_t bitMask = 0x80u >> (bitIndex & 7u);
    uint32_t zm = 0;
    for (uint8_t ch = 0; ch < nCh; ch++) {
      if (byteIndex >= chanTotalBytes[ch] || !(s_pixelData[ch][byteIndex] & bitMask)) {
        zm |= chanPinMask[ch];
      }
    }
    return zm;
  };

  // Period reference: initialise as already expired so the first pulse fires immediately.
  uint32_t cyclesStart = ESP.getCycleCount() - period;

  os_intr_lock();

  for (uint32_t bitIndex = 0; bitIndex < totalBits; bitIndex++) {

    // ── Step 1: Compute zero mask for this bit ────────────────────────────
    uint32_t zeroMask = computeZeroMask(bitIndex);

    // ── Step 2: Wait for the full bit period since the last HIGH edge ──────
    while ((ESP.getCycleCount() - cyclesStart) < period);

    // ── Step 3: Set all outputs HIGH simultaneously ───────────────────────
    GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, setAllMask);
    cyclesStart = ESP.getCycleCount();

    // ── Step 4: After T0H — pull '0' outputs LOW ─────────────────────────
    while ((ESP.getCycleCount() - cyclesStart) < t0h);
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, zeroMask);

    // ── Step 5: After T1H — pull all remaining outputs LOW ───────────────
    while ((ESP.getCycleCount() - cyclesStart) < t1h);
    GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, setAllMask);
  }

  os_intr_unlock();
  return true;
}


//==============================================================================
// ESP8266 DMA Bus (I2S + SLC linked-list DMA)
//==============================================================================
//
// Architecture overview:
//   The SLC (streaming linked-list controller) drives I2S TX continuously.
//   Two "state" descriptors loop on the shared idle buffer (all zeros → LOW).
//   On show(), state[1].next is patched to the first pixel descriptor so the
//   DMA seamlessly transitions: LOW idle → pixel data → reset zeros → LOW idle.
//   An EOF ISR fires at the end of the last pixel descriptor and restores the
//   idle loop without ever stopping I2S, so GPIO3 is ALWAYS driven and never
//   floats HIGH.  The "high pulse before first LED" problem is eliminated.
//
// Descriptor layout (during show):
//   state[0] → state[1] → data[0] → ... → data[N-1,EOF] → reset[0..M-1] → state[0]
// Descriptor layout (idle):
//   state[0] → state[1] → state[0]  (infinite loop, outputs zeros)
//

// ISR singleton
Esp8266DmaBus* Esp8266DmaBus::s_this = nullptr;

Esp8266DmaBus::Esp8266DmaBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType)
  : _pin(pin), _timing(timing),
    _initialized(false), _sending(false),
    _dmaDesc(nullptr), _dmaDescCnt(0),
    _idleBuf(nullptr), _idleBufSize(0) {
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
}

Esp8266DmaBus::~Esp8266DmaBus() {
  end();
}

// ---------------------------------------------------------------------------
// allocateEncodeBuffer
//   Pixel buffer only — no lead-in, no appended reset.
//   Idle/reset are handled by _idleBuf + descriptor chain.
// ---------------------------------------------------------------------------
bool Esp8266DmaBus::allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) {
  // 4-step I2S encoding: each source byte → 4 encoded bytes
  const size_t pixelBytes = (size_t)numPixels * numChannels * 4;
  size_t needed = _prefixLen + pixelBytes + _suffixLen * 4;
  if (_encodeBuffer && _encodeBufferSize >= needed) return true;
  if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; }
  if (needed == 0) return true;
  _encodeBuffer = (uint8_t*)malloc(needed);
  if (!_encodeBuffer) { _encodeBufferSize = 0; return false; }
  memset(_encodeBuffer, 0, needed);
  _encodeBufferSize = needed;
  _pixelData = _encodeBuffer + _prefixLen;
  if (_suffixLen == sizeof(SM16825_SUFFIX) && _ledType == TYPE_SM16825) {
    uint32_t* dst = (uint32_t*)(_pixelData + pixelBytes);
    for (uint8_t i = 0; i < (uint8_t)sizeof(SM16825_SUFFIX); i++) {
      uint32_t word = 0;
      uint8_t v = SM16825_SUFFIX[i];
      for (int bit = 7; bit >= 0; bit--) { word <<= 4; word |= (v & (1 << bit)) ? 0xEu : 0x8u; }
      dst[i] = word;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// buildDescriptorChain
//   Builds the SLC descriptor linked list according to the layout above.
//   Must be called (a) after allocateEncodeBuffer and (b) after _idleBuf
//   has been set up.  Descriptor memory must already be allocated.
// ---------------------------------------------------------------------------
void Esp8266DmaBus::buildDescriptorChain() {
  // --- Layout constants ---
  // reset duration: use timing.reset_us, minimum 300 µs for stubborn LEDs.
  uint32_t bitPeriod = _timing.bitPeriod();
  if (bitPeriod == 0) bitPeriod = 1250;
  uint32_t resetUs = (_timing.reset_us > 300) ? _timing.reset_us : 300;
  // encoded bytes produced per LED bit period in 4-step cadence = 4 bits / 8 bits * 4 bytes = 0.5 bytes
  // bytes per µs at bitPeriod ns/bit: bytesPerUs = 4 (i2s bytes/led-bit) / (bitPeriod/1000 µs/led-bit)
  //   = 4000.0 / bitPeriod  bytes/µs
  size_t resetBytes = (size_t)((4000.0f / (float)bitPeriod) * (float)resetUs + 0.5f);
  // round up to 4-byte boundary
  resetBytes = (resetBytes + 3) & ~3u;
  if (resetBytes < 4) resetBytes = 4;

  // --- Count descriptors ---
  size_t pixelBytes   = _encodeBufferSize;
  size_t dataBlocks   = (pixelBytes > 0) ? ((pixelBytes + c_maxDmaBlockSize - 1) / c_maxDmaBlockSize) : 1;
  size_t resetBlocks  = (resetBytes + c_idleBufSize - 1) / c_idleBufSize;
  _dmaDescCnt = (uint16_t)(c_stateBlockCount + dataBlocks + resetBlocks);

  // Free previous descriptor allocation
  if (_dmaDesc) { free(_dmaDesc); _dmaDesc = nullptr; }
  _dmaDesc = (SlcQueueItem*)malloc(_dmaDescCnt * sizeof(SlcQueueItem));
  if (!_dmaDesc) { _dmaDescCnt = 0; return; }

  uint16_t idx = 0;

  // --- State descriptors: loop on idle buf (4 bytes each to keep it tiny) ---
  dmaItemInit(&_dmaDesc[0], _idleBuf, 4, &_dmaDesc[1]);
  dmaItemInit(&_dmaDesc[1], _idleBuf, 4, &_dmaDesc[0]); // default: idle loop

  idx = c_stateBlockCount;

  // --- Pixel data descriptors ---
  uint8_t* ptr  = _encodeBuffer;
  size_t   left = pixelBytes;
  size_t   firstDataIdx = idx;
  while (left > 0) {
    size_t chunk = (left > c_maxDmaBlockSize) ? c_maxDmaBlockSize : left;
    dmaItemInit(&_dmaDesc[idx], ptr, chunk, &_dmaDesc[idx + 1]);
    ptr  += chunk;
    left -= chunk;
    idx++;
  }
  // Mark the last data descriptor as EOF → ISR fires here
  _dmaDesc[idx - 1].eof = 1;

  // --- Reset (idle) descriptors ---
  size_t firstResetIdx = idx;
  size_t resetLeft = resetBytes;
  while (resetLeft > 0) {
    size_t chunk = (resetLeft > c_idleBufSize) ? c_idleBufSize : resetLeft;
    dmaItemInit(&_dmaDesc[idx], _idleBuf, (uint16_t)chunk, &_dmaDesc[idx + 1]);
    resetLeft -= chunk;
    idx++;
  }
  // Last reset descriptor loops back to state[0]
  _dmaDesc[idx - 1].next_link_ptr = &_dmaDesc[0];
  (void)firstDataIdx; (void)firstResetIdx; // used only for clarity
}

// ---------------------------------------------------------------------------
// slcIsr  — fires on SLCIRXEOF (end of last pixel descriptor)
//   Restore state[1] → state[0] idle loop. Mark as no longer sending.
// ---------------------------------------------------------------------------
void IRAM_ATTR Esp8266DmaBus::slcIsr() {
  ETS_SLC_INTR_DISABLE();
  uint32_t status = SLCIS;
  SLCIC = 0xFFFFFFFF;
  if ((status & SLCIRXEOF) && s_this) {
    // Re-close the idle loop so state[0]→state[1]→state[0]
    s_this->_dmaDesc[1].next_link_ptr = &s_this->_dmaDesc[0];
    s_this->_sending = false;
  }
  ETS_SLC_INTR_ENABLE();
}

// ---------------------------------------------------------------------------
// startI2s  — configure SLC + I2S registers and kick off continuous DMA
// ---------------------------------------------------------------------------

// TODO: just like in uart, the output pin matrix is somehow overwritten, maybe by pinmanager?
void Esp8266DmaBus::startI2s(uint8_t bckDiv, uint8_t clkDiv) {
  ETS_SLC_INTR_DISABLE(); // disable ISR while configuring (just in case)
  // Reset SLC
  SLCC0 |= SLCRXLR | SLCTXLR;
  SLCC0 &= ~(SLCRXLR | SLCTXLR);
  SLCIC = 0xFFFFFFFF;

  // Configure SLC in DMA mode 1
  SLCC0 &= ~(SLCMM << SLCM);
  SLCC0 |= (1 << SLCM);
  SLCRXDC |= SLCBINR | SLCBTNR;
  SLCRXDC &= ~(SLCBRXFE | SLCBRXEM | SLCBRXFM);

  // TXLINK: needs a valid descriptor (we reuse the last one; TX not actually used)
  SLCTXL &= ~(SLCTXLAM << SLCTXLA);
  SLCTXL |= (uint32_t)(&_dmaDesc[_dmaDescCnt - 1]) << SLCTXLA;

  // RXLINK: start from state[0] (idle loop)
  SLCRXL &= ~(SLCRXLAM << SLCRXLA);
  SLCRXL |= (uint32_t)(&_dmaDesc[0]) << SLCRXLA;

  // Attach ISR
  ETS_SLC_INTR_ATTACH(slcIsr, nullptr);
  SLCIE = SLCIRXEOF;
  ETS_SLC_INTR_ENABLE();

  // Start SLC
  SLCTXL |= SLCTXLS;
  SLCRXL |= SLCRXLS;

  // Enable I2S clock
  I2S_CLK_ENABLE();
  I2SIC = 0x3F;
  I2SIE = 0;

  // Reset I2S
  I2SC &= ~(I2SRST);
  I2SC |= I2SRST;
  I2SC &= ~(I2SRST);

  // Set RX/TX FIFO_MOD=0 (16-bit stereo) and re-enable DMA.
  I2SFC &= ~(I2SDE | (I2STXFMM << I2STXFM) | (I2SRXFMM << I2SRXFM));
  I2SFC |= I2SDE; // re-enable DMA
  // Set RX/TX CHAN_MOD=0 (stereo, normal).
  I2SCC &= ~((I2STXCMM << I2STXCM) | (I2SRXCMM << I2SRXCM));

  // I2S config: right-first, MSB-right, slave mode off
  I2SC &= ~(I2STSM | I2SRSM | (I2SBMM << I2SBM) | (I2SBDM << I2SBD) | (I2SCDM << I2SCD));
  I2SC |= I2SRF | I2SMR | I2SRSM | I2SRMS | ((uint32_t)bckDiv << I2SBD) | ((uint32_t)clkDiv << I2SCD);

  // Start I2S TX
  I2SC |= I2STXS;
}

// ---------------------------------------------------------------------------
// stopI2s  — disable SLC + I2S
// ---------------------------------------------------------------------------
void Esp8266DmaBus::stopI2s() {
  ETS_SLC_INTR_DISABLE();
  SLCIC = 0xFFFFFFFF; // clear pending interrupt flags
  I2SC &= ~(I2STXS | I2SRXS);
  I2SC &= ~I2SRST;
  I2SC |= I2SRST;
  I2SC &= ~I2SRST;
}

// ---------------------------------------------------------------------------
// begin
// ---------------------------------------------------------------------------
bool Esp8266DmaBus::begin() {
  if (_initialized) return true;
  if (_pin != 3) return false;

  // Hold GPIO3 LOW before any I2S activity (prevents startup glitch)
  pinMode(3, OUTPUT);
  digitalWrite(3, LOW);

  // Compute I2S clock divisors for 4-step cadence
  // I2S base clock = 160 MHz (even with 80 MHz CPU freq, tested)
  // I2S clock is baseclk / bckDiv / clkDiv. We want to get as close as possible to 4 / bitPeriod
  uint8_t best_clkdiv = 1;
  uint8_t best_baseclkdiv = 1;
  uint64_t best_error = UINT64_MAX;
  // target I2S bit clock = 4 / bitPeriod_ns * 1e9  Hz
  // divisor = I2SBASEFREQ / rate = I2SBASEFREQ * bitPeriod / 4e9
  uint32_t bitPeriod = _timing.bitPeriod(); // in nanoseconds, for example 1250=1.25us for WS2812
  uint64_t target = (uint64_t)I2SBASEFREQ * bitPeriod;

  for (uint8_t bck = 2; bck <= 64; bck += 2) {
      uint64_t den = (uint64_t)4000000000ULL * bck; // denominator of clkdiv = I2SBASEFREQ * bitPeriod / (4e9 * baseclkdiv)

      uint64_t clk = (target + den / 2) / den;
      if (clk < 1 || clk > 63) continue;

      uint64_t actual = clk * den;
      uint64_t error = (actual > target) ? (actual - target) : (target - actual);

      if (error < best_error) {
          best_error = error;
          best_clkdiv = clk;
          best_baseclkdiv = bck;
      }
  }

  uint8_t bckDiv = best_baseclkdiv;
  uint8_t clkDiv = best_clkdiv;

  // Allocate idle/reset zero buffer
  _idleBufSize = c_idleBufSize;
  _idleBuf = (uint8_t*)malloc(_idleBufSize);
  if (!_idleBuf) return false;
  memset(_idleBuf, 0, _idleBufSize);

  // Allocate pixel encode buffer and build initial descriptor chain
  if (!allocateEncodeBuffer(_numPixels, _encoder.getPixelBytes())) { end(); return false; }
  buildDescriptorChain();
  if (!_dmaDesc) { end(); return false; }

  s_this = this;
  _sending = false;

  // Switch GPIO3 MUX to I2S (transitions LOW→LOW because I2S starts in idle)
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_I2SO_DATA);

  startI2s(bckDiv, clkDiv);

  _initialized = true;
  return true;
}

// ---------------------------------------------------------------------------
// end
// ---------------------------------------------------------------------------
void Esp8266DmaBus::end() {
  if (!_initialized && !_idleBuf && !_dmaDesc) return;
  stopI2s();
  ETS_SLC_INTR_ATTACH(nullptr, nullptr);
  s_this = nullptr;
  if (_dmaDesc)  { free(_dmaDesc);  _dmaDesc  = nullptr; _dmaDescCnt = 0; }
  if (_idleBuf)  { free(_idleBuf);  _idleBuf  = nullptr; _idleBufSize = 0; }
  if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; _encodeBufferSize = 0; }
  pinMode(_pin, INPUT);
  _initialized = false;
  _sending = false;
}

// ---------------------------------------------------------------------------
// setPixel / getPixelColor / scaleAll
//   Pixel data starts at offset 0 in _encodeBuffer (no lead-in needed;
//   idle state ensures GPIO3 is LOW before the first pixel bit arrives).
// ---------------------------------------------------------------------------
IRAM_ATTR bool Esp8266DmaBus::setPixel(uint16_t pos, uint32_t c, uint16_t wwcw) {
  const uint8_t pixelBytes = _encoder.getPixelBytes();
  uint8_t src[12];
  const CctPixel cct{wwcw};
  switch (_encoder.getPixelFormat()) {
    case 3:                    _encoder.encodeRGB(c, src);                    break;
    case 4:                    _encoder.encodeRGBW(c, src);                   break;
    case 5:                    _encoder.encodeCCT(c, cct, src);               break;
    case (3*2) | NCHF_16BIT:   _encoder.encodeRGB16(c, src, _encBri);         break;
    case (4*2) | NCHF_16BIT:   _encoder.encodeRGBW16(c, src, _encBri);        break;
    case (5*2) | NCHF_16BIT:   _encoder.encodeCCT16(c, cct, src, _encBri);    break;
    default:                   _encoder.encodeGeneric(c, cct, src, _encBri);  break;
  }
  uint32_t* dst = (uint32_t*)(_pixelData + (size_t)pos * pixelBytes * 4);
  for (uint8_t b = 0; b < pixelBytes; b++) {
    uint32_t word = 0;
    uint8_t v = src[b];
    for (int bit = 7; bit >= 0; bit--) {
      word <<= 4;
      word |= (v & (1 << bit)) ? 0xEu : 0x8u;
    }
    dst[b] = word;
  }
  return true;
}

IRAM_ATTR uint32_t Esp8266DmaBus::getPixelColor(uint16_t pix) const {
  const uint8_t pixelBytes = _encoder.getPixelBytes();
  const uint32_t* src = (const uint32_t*)(_pixelData + (size_t)pix * pixelBytes * 4);
  uint8_t decoded[12];
  for (uint8_t b = 0; b < pixelBytes; b++) {
    uint32_t word = src[b];
    uint8_t v = 0;
    for (int nib = 7; nib >= 0; nib--) {
      v <<= 1;
      if (((word >> (nib * 4)) & 0xF) == 0xE) v |= 1;
    }
    decoded[b] = v;
  }
  switch (_encoder.getPixelFormat()) {
    case 3:                    return _encoder.decodeRGB(decoded);
    case 4:                    return _encoder.decodeRGBW(decoded);
    case 5:                    return _encoder.decodeCCT(decoded);
    case (3*2) | NCHF_16BIT:   return _encoder.decodeRGB16(decoded);
    case (4*2) | NCHF_16BIT:   return _encoder.decodeRGBW16(decoded);
    case (5*2) | NCHF_16BIT:   return _encoder.decodeCCT16(decoded);
    default:                   return _encoder.decodeGeneric(decoded);
  }
}

// Since the colors are already 4-step encoded, we need to decode first, scale then re-encode.
void Esp8266DmaBus::scaleAll(uint8_t scale) {
  if (!_pixelData || scale == 255) return;
  uint8_t pixelBytes = _encoder.getPixelBytes();
  uint32_t* buf = (uint32_t*)_pixelData;
  size_t numWords = (size_t)_numPixels * pixelBytes;
  for (size_t w = 0; w < numWords; w++) {
    uint32_t word = buf[w];
    uint8_t v = 0;
    for (int nib = 7; nib >= 0; nib--) {
      v <<= 1;
      if (((word >> (nib * 4)) & 0xF) == 0xE) v |= 1;
    }
    v = ((uint16_t)(v + 1) * scale) >> 8;
    uint32_t newWord = 0;
    for (int bit = 7; bit >= 0; bit--) {
      newWord <<= 4;
      newWord |= (v & (1 << bit)) ? 0xEu : 0x8u;
    }
    buf[w] = newWord;
  }
}

void Esp8266DmaBus::updateSuffix(const uint8_t* data, uint8_t len) {
  if (!_pixelData || _suffixLen == 0 || len == 0) return;
  if (len > _suffixLen) len = _suffixLen;
  const size_t pixelWords = (size_t)_numPixels * _encoder.getPixelBytes();
  uint32_t* dst = (uint32_t*)(_pixelData + pixelWords * 4);
  for (uint8_t i = 0; i < len; i++) {
    uint32_t word = 0;
    uint8_t v = data[i];
    for (int bit = 7; bit >= 0; bit--) { word <<= 4; word |= (v & (1 << bit)) ? 0xEu : 0x8u; }
    dst[i] = word;
  }
}

void Esp8266DmaBus::setColorOrder(uint8_t co) {
  _encoder = ColorEncoder(co, _encoder.getColorChannels(), _ledType);
}

// ---------------------------------------------------------------------------
// show
//   Patches state[1] to break out of the idle loop into the pixel data,
//   then returns immediately. The ISR restores the idle loop when done.
// ---------------------------------------------------------------------------
bool Esp8266DmaBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;
  if (_sending) return false; // previous frame still running

  // Break the idle loop: state[1] now points to first pixel descriptor
  _dmaDesc[1].next_link_ptr = &_dmaDesc[c_stateBlockCount];
  _sending = true;

  return true;
}

// ---------------------------------------------------------------------------
// canShow  — ready when the previous frame's ISR has restored idle loop
// ---------------------------------------------------------------------------
bool Esp8266DmaBus::canShow() const {
  return _initialized && !_sending;
}

} // namespace WLEDpixelBus

#endif // WLEDPB_ESP8266
