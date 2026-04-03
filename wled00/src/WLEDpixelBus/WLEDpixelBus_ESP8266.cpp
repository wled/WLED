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

Esp8266UartBus::Esp8266UartBus(int8_t pin, const LedTiming& timing, ColorOrder order)
  : _pin(pin), _timing(timing), _order(order), _encoder(order), _initialized(false),
    _asyncBuf(nullptr), _asyncBufEnd(nullptr) {}

Esp8266UartBus::~Esp8266UartBus() {
  end();
}

// Shared Interrupt Service Routine (must be in IRAM)
void IRAM_ATTR Esp8266UartBus::UartIsr(void* arg) {
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
  if (!allocateEncodeBuffer(_numPixels, _encoder.getNumChannels())) { end(); return false; }
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
  USC0(uartNum) &= ~((1 << UCDTRI) | (1 << UCRTSI) | (1 << UCTXI) | (1 << UCDSRI) | (1 << UCCTSI) | (1 << UCRXI));
}

bool Esp8266UartBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;
  if (!canShow()) return false;

  uint8_t uartNum = (_pin == 2) ? 1 : 0;
  _asyncBuf = _encodeBuffer;
  _asyncBufEnd = _encodeBuffer + _encodeBufferSize;

  // Enable the "TX FIFO Empty" interrupt to trigger the ISR
  USIE(uartNum) |= (1 << UIFE);
  
  return true;
}

bool Esp8266UartBus::setPixel(uint16_t pos, uint32_t c, uint8_t ww, uint8_t cw) {
  if (!_encodeBuffer || pos >= _numPixels) return false;
  CctPixel cct{ww, cw};
  _encoder.encode(c, &cct, _encodeBuffer + pos * _encoder.getNumChannels());
  return true;
}

uint32_t Esp8266UartBus::getPixelColor(uint16_t pix) const {
  if (!_encodeBuffer || pix >= _numPixels) return 0;
  return _encoder.decode(_encodeBuffer + pix * _encoder.getNumChannels());
}

void Esp8266UartBus::setColorOrder(ColorOrder order) {
  _order = order;
  _encoder = ColorEncoder(order);
}

bool Esp8266UartBus::canShow() const {
  if (!_initialized) return false;
  // Ready if we have no more data to send
  return (_asyncBuf >= _asyncBufEnd);
}


//==============================================================================
// ESP8266 BitBang Bus
//==============================================================================

Esp8266BitBangBus::Esp8266BitBangBus(int8_t pin, const LedTiming& timing, ColorOrder order)
  : _pin(pin), _timing(timing), _order(order), _encoder(order), _initialized(false) {}

Esp8266BitBangBus::~Esp8266BitBangBus() {
  end();
}

bool Esp8266BitBangBus::begin() {
  if (_initialized) return true;
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
  setTiming(_timing);
  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getNumChannels())) { end(); return false; }
  return true;
}

void Esp8266BitBangBus::end() {
  pinMode(_pin, INPUT);
  _initialized = false;
}

void Esp8266BitBangBus::setTiming(const LedTiming& timing) {
  _timing = timing;
  uint32_t cpuFreq = ESP.getCpuFreqMHz(); // usually 80 or 160
  _t0h = (timing.t0h_ns * cpuFreq) / 1000;
  _t0l = (timing.t0l_ns * cpuFreq) / 1000;
  _t1h = (timing.t1h_ns * cpuFreq) / 1000;
  _t1l = (timing.t1l_ns * cpuFreq) / 1000;
}

bool Esp8266BitBangBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;

  uint8_t numCh = _encoder.getNumChannels();
  uint32_t mask = 1 << _pin;

  os_intr_lock();
  for (uint16_t i = 0; i < _numPixels; i++) {
    const uint8_t* src = _encodeBuffer + i * numCh;
    for (uint8_t b = 0; b < numCh; b++) {
      uint8_t v = src[b];
      for (int bit = 7; bit >= 0; bit--) {
        uint32_t t = ESP.getCycleCount();
        if (v & (1 << bit)) {
          GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, mask);
          while((ESP.getCycleCount() - t) < _t1h);
          GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, mask);
          while((ESP.getCycleCount() - t) < (_t1h + _t1l));
        } else {
          GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, mask);
          while((ESP.getCycleCount() - t) < _t0h);
          GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, mask);
          while((ESP.getCycleCount() - t) < (_t0h + _t0l));
        }
      }
    }
  }
  os_intr_unlock();
  return true;
}

bool Esp8266BitBangBus::setPixel(uint16_t pos, uint32_t c, uint8_t ww, uint8_t cw) {
  if (!_encodeBuffer || pos >= _numPixels) return false;
  CctPixel cct{ww, cw};
  _encoder.encode(c, &cct, _encodeBuffer + pos * _encoder.getNumChannels());
  return true;
}

uint32_t Esp8266BitBangBus::getPixelColor(uint16_t pix) const {
  if (!_encodeBuffer || pix >= _numPixels) return 0;
  return _encoder.decode(_encodeBuffer + pix * _encoder.getNumChannels());
}

void Esp8266BitBangBus::setColorOrder(ColorOrder order) {
  _order = order;
  _encoder = ColorEncoder(order);
}

bool Esp8266BitBangBus::canShow() const {
  return _initialized;
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

Esp8266DmaBus::Esp8266DmaBus(int8_t pin, const LedTiming& timing, ColorOrder order)
  : _pin(pin), _timing(timing), _order(order), _encoder(order),
    _initialized(false), _sending(false),
    _dmaDesc(nullptr), _dmaDescCnt(0),
    _idleBuf(nullptr), _idleBufSize(0) {}

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
  size_t needed = (size_t)numPixels * numChannels * 4;
  if (_encodeBuffer && _encodeBufferSize >= needed) return true;
  if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; }
  if (needed == 0) return true;
  _encodeBuffer = (uint8_t*)malloc(needed);
  if (!_encodeBuffer) { _encodeBufferSize = 0; return false; }
  memset(_encodeBuffer, 0, needed);
  _encodeBufferSize = needed;
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
void Esp8266DmaBus::startI2s(uint8_t bckDiv, uint8_t clkDiv) {
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
  // I2S base clock = 160 MHz / 2 = 80 MHz
  // target I2S bit clock = 4 / bitPeriod_ns * 1e9  Hz
  // divisor = I2SBASEFREQ / rate = I2SBASEFREQ * bitPeriod / 4e9
  uint32_t bitPeriod = _timing.bitPeriod();
  if (bitPeriod == 0) bitPeriod = 1250;
  float divisor = 80000000.0f * (float)bitPeriod / 4000000000.0f;
  // Choose bck_div = 4 (minimum even value; NeoPixelBus uses 4)
  uint8_t bckDiv = 4;
  uint8_t clkDiv = (uint8_t)(divisor / bckDiv + 0.5f);
  if (clkDiv < 1) clkDiv = 1;
  if (clkDiv > 63) clkDiv = 63;

  // Allocate idle/reset zero buffer
  _idleBufSize = c_idleBufSize;
  _idleBuf = (uint8_t*)malloc(_idleBufSize);
  if (!_idleBuf) return false;
  memset(_idleBuf, 0, _idleBufSize);

  // Allocate pixel encode buffer and build initial descriptor chain
  if (!allocateEncodeBuffer(_numPixels, _encoder.getNumChannels())) { end(); return false; }
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
bool Esp8266DmaBus::setPixel(uint16_t pos, uint32_t c, uint8_t ww, uint8_t cw) {
  if (!_encodeBuffer || pos >= _numPixels) return false;
  uint8_t numCh = _encoder.getNumChannels();
  uint8_t src[5];
  CctPixel cct{ww, cw};
  _encoder.encode(c, &cct, src);
  uint32_t* dst = (uint32_t*)(_encodeBuffer + (size_t)pos * numCh * 4);
  for (uint8_t b = 0; b < numCh; b++) {
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

uint32_t Esp8266DmaBus::getPixelColor(uint16_t pix) const {
  if (!_encodeBuffer || pix >= _numPixels) return 0;
  uint8_t numCh = _encoder.getNumChannels();
  const uint32_t* src = (const uint32_t*)(_encodeBuffer + (size_t)pix * numCh * 4);
  uint8_t decoded[5];
  for (uint8_t b = 0; b < numCh; b++) {
    uint32_t word = src[b];
    uint8_t v = 0;
    for (int nib = 7; nib >= 0; nib--) {
      v <<= 1;
      if (((word >> (nib * 4)) & 0xF) == 0xE) v |= 1;
    }
    decoded[b] = v;
  }
  return _encoder.decode(decoded);
}

// Since the colors are already 4-step encoded, we need to decode first, scale then re-encode.
void Esp8266DmaBus::scaleAll(uint8_t scale) {
  if (!_encodeBuffer || scale == 255) return;
  uint8_t numCh = _encoder.getNumChannels();
  uint32_t* buf = (uint32_t*)_encodeBuffer;
  size_t numWords = (size_t)_numPixels * numCh;
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

void Esp8266DmaBus::setColorOrder(ColorOrder order) {
  _order = order;
  _encoder = ColorEncoder(order);
}

// ---------------------------------------------------------------------------
// show
//   Patches state[1] to break out of the idle loop into the pixel data,
//   then returns immediately. The ISR restores the idle loop when done.
// ---------------------------------------------------------------------------
bool Esp8266DmaBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;
  if (_sending) return false; // previous frame still running

  _sending = true;
  // Break the idle loop: state[1] now points to first pixel descriptor
  _dmaDesc[1].next_link_ptr = &_dmaDesc[c_stateBlockCount];
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
