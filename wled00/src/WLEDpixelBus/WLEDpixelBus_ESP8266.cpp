/*-------------------------------------------------------------------------

WLEDpixelBus - ESP8266 driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

Supports UART and I2S DMA output as well as parallel bit-banging

-------------------------------------------------------------------------*/

#include "WLEDpixelBus_ESP8266.h"

#if defined(ESP8266)

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


// ESP8266 UART Bus
// -----------------

// Global static tracking for the shared UART ISR
Esp8266UartBus* Esp8266UartBus::s_uartInstances[2] = {nullptr, nullptr};

Esp8266UartBus::Esp8266UartBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType)
  : _pin(pin)
  , _timing(timing)
  , _initialized(false)
  , _asyncBuf(nullptr)
  , _asyncBufEnd(nullptr)
  , _txStartMillis(0)
{
  (void)colorOrder; (void)numChannels; // encoder is installed by createBus() from BusProperties
  _ledType = ledType;
}

Esp8266UartBus::~Esp8266UartBus() {
  end();
}

// Shared Interrupt Service Routine (must be in IRAM)
void IRAM_ATTR Esp8266UartBus::uartIsr(void* arg, void* exceptionFrame) {
  for (uint8_t uartNum = 0; uartNum < 2; uartNum++) {
    Esp8266UartBus* instance = s_uartInstances[uartNum];

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
  s_uartInstances[uartNum] = this;

  // set LED timing and (re)init the serial
  updateUartTiming();

  ETS_UART_INTR_DISABLE();
  // Attach the shared ISR
  ETS_UART_INTR_ATTACH(uartIsr, nullptr);

  // Set threshold: Interrupt when FIFO drops below 80 bytes
  USC1(uartNum) = (80 << UCFET);
  USIC(uartNum) = 0xffff; // Clear pending
  USIE(uartNum) &= ~((1 << UIFF) | (1 << UIFE)); // Start with interrupts off
  ETS_UART_INTR_ENABLE();

  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getBytesPerPixel())) { end(); return false; }
  return true;
}

void Esp8266UartBus::end() {
  if (!_initialized) return;
  waitForIdle();
  uint8_t uartNum = (_pin == 2) ? 1 : 0;

  ETS_UART_INTR_DISABLE();
  USIE(uartNum) = 0;
  s_uartInstances[uartNum] = nullptr;

  // If no buses are left, detach ISR
  if (s_uartInstances[0] == nullptr && s_uartInstances[1] == nullptr) {
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
  if (!_inverted) {
    USC0(uartNum) |= (1 << UCTXI); // invert TX -> idle low
  }
}

bool Esp8266UartBus::show() {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;
  waitForIdle();
  // workaround for a bug: after bootup, something changes the pin mux AFTER the bus is initialized, breaking the UART output. it only starts working after a bus re-init. could not find out what causes it.
  // since pinMode() is computationally cheap, it is an acceptable fix
  pinMode(_pin, SPECIAL);

  uint8_t uartNum = (_pin == 2) ? 1 : 0;
  _asyncBuf = _encodeBuffer;
  _asyncBufEnd = _encodeBuffer + _encodeBufferSize;
  _txStartMillis = millis();

  // note: no initial fill required, the ISR will fire and fill the FIFO
  // Enable the "TX FIFO Empty" interrupt to trigger the ISR
  USIE(uartNum) |= (1 << UIFE);

  return true;
}

bool Esp8266UartBus::isIdle() const {
  if (_asyncBuf < _asyncBufEnd) return false;
  const uint8_t uartNum = (_pin == 2) ? 1 : 0;
  return ((USS(uartNum) >> USTXC) & 0xff) == 0;
}

void Esp8266UartBus::abortTransmit() const {
  const uint8_t uartNum = (_pin == 2) ? 1 : 0;
  ETS_UART_INTR_DISABLE();
  USIE(uartNum) &= ~(1 << UIFE);
  _asyncBuf = nullptr;
  _asyncBufEnd = nullptr;
  ETS_UART_INTR_ENABLE();
}

bool Esp8266UartBus::ensureIdle() const {
  if (isIdle()) return true;
  if ((uint32_t)(millis() - _txStartMillis) <= WLEDPB_TRANSFER_TIMEOUT_MS) return false;
  abortTransmit();
  return true;
}

void Esp8266UartBus::waitForIdle() const {
  const uint32_t start = millis();
  while (!ensureIdle() && (uint32_t)(millis() - start) <= WLEDPB_TRANSFER_TIMEOUT_MS) delay(1);
  if (!isIdle()) abortTransmit();
}

bool Esp8266UartBus::canShow() const {
  if (!_initialized) return true; // do not block if init failed! busmanager waits until all buses can show
  return ensureIdle();
}


// ESP8266 DMA Bus (I2S + SLC linked-list DMA)
// --------------------------------------------
Esp8266DmaBus* Esp8266DmaBus::s_dmaInstance = nullptr;

Esp8266DmaBus::Esp8266DmaBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType)
  :  _pin(pin)
  , _timing(timing)
  , _initialized(false)
  , _sending(false)
  , _txStartMillis(0)
  , _bckDiv(1)
  , _clkDiv(1)
  , _dmaDesc(nullptr)
  , _dmaDescCnt(0)
  , _idleBuf(nullptr)
  , _idleBufSize(0)
{
  (void)colorOrder; (void)numChannels; // encoder is installed by createBus() from BusProperties
  _ledType = ledType;
}

Esp8266DmaBus::~Esp8266DmaBus() {
  end();
}

bool Esp8266DmaBus::allocateEncodeBuffer(uint16_t numPixels, uint8_t bytesPerPixel) {
  // 4-step I2S encoding: each source byte → 4 encoded bytes
  const size_t sourcePixelBytes = (size_t)numPixels * bytesPerPixel;
  const size_t prefixBytes = (size_t)_prefixLen * 4;
  const size_t pixelBytes = sourcePixelBytes * 4;
  const size_t needed = prefixBytes + pixelBytes + (size_t)_suffixLen * 4;
  if (_encodeBuffer && _encodeBufferSize >= needed) return true;
  releaseEncodeBuffer();
  if (needed == 0) return true;
  _encodeBuffer = (uint8_t*)malloc(needed);
  if (!_encodeBuffer) { _encodeBufferSize = 0; return false; }
  memset(_encodeBuffer, 0, needed);
  _encodeBufferSize = needed;
  _pixelData = _encodeBuffer + prefixBytes;
  updatePrefix(_prefixData, _prefixLen);
  if (_suffixLen > 0) {
    uint32_t* dst = (uint32_t*)(_pixelData + pixelBytes);
    for (uint8_t i = 0; i < _suffixLen; i++) {
      uint32_t word = 0;
      uint8_t v = _suffixData[i];
      for (int bit = 7; bit >= 0; bit--) {
        word <<= 4;
        if (_inverted) word |= (v & (1 << bit)) ? 0x1u : 0x7u;
        else           word |= (v & (1 << bit)) ? 0xEu : 0x8u;
      }
      dst[i] = word;
    }
  }
  return true;
}

void Esp8266DmaBus::buildDescriptorChain() {
  uint32_t bitPeriod = _timing.bitPeriod();
  if (bitPeriod == 0) bitPeriod = 1250;
  uint32_t resetUs = (_timing.reset_us > 300) ? _timing.reset_us : 300;
  size_t resetBytes = (size_t)((4000.0f / (float)bitPeriod) * (float)resetUs + 0.5f);
  resetBytes = (resetBytes + 3) & ~3u; // round up to 4-byte boundary
  if (resetBytes < 4) resetBytes = 4;

  size_t pixelBytes   = _encodeBufferSize;
  size_t dataBlocks   = (pixelBytes > 0) ? ((pixelBytes + c_maxDmaBlockSize - 1) / c_maxDmaBlockSize) : 1;
  size_t resetBlocks  = (resetBytes + c_idleBufSize - 1) / c_idleBufSize;
  _dmaDescCnt = (uint16_t)(c_stateBlockCount + dataBlocks + resetBlocks);

  // Free previous descriptor allocation
  if (_dmaDesc) { free(_dmaDesc); _dmaDesc = nullptr; }
  _dmaDesc = (SlcQueueItem*)malloc(_dmaDescCnt * sizeof(SlcQueueItem));
  if (!_dmaDesc) { _dmaDescCnt = 0; return; }

  uint16_t idx = 0;

  // loop on idle buf (4 bytes each to keep it tiny)
  dmaItemInit(&_dmaDesc[0], _idleBuf, 4, &_dmaDesc[1]);
  dmaItemInit(&_dmaDesc[1], _idleBuf, 4, &_dmaDesc[0]); // default: idle loop

  idx = c_stateBlockCount;

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
  _dmaDesc[idx - 1].eof = 1;   // Mark the last data descriptor as EOF: ISR fires here

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


// slcIsr fires on SLCIRXEOF (end of last pixel descriptor)
void IRAM_ATTR Esp8266DmaBus::slcIsr() {
  ETS_SLC_INTR_DISABLE();
  uint32_t status = SLCIS;
  SLCIC = 0xFFFFFFFF;
  if ((status & SLCIRXEOF) && s_dmaInstance) {
    // Re-close the idle loop so state[0]→state[1]→state[0]
    s_dmaInstance->_dmaDesc[1].next_link_ptr = &s_dmaInstance->_dmaDesc[0];
    s_dmaInstance->_sending = false;
  }
  ETS_SLC_INTR_ENABLE();
}

// TODO: just like in uart, the output pin matrix is somehow overwritten, needs investigation and maybe a workaround like in UART
void Esp8266DmaBus::startI2s(uint8_t bckDiv, uint8_t clkDiv) const {
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

void Esp8266DmaBus::stopI2s() const {
  ETS_SLC_INTR_DISABLE();
  SLCIE = 0;
  SLCRXL &= ~SLCRXLS; // stop both links before their descriptors can be freed
  SLCTXL &= ~SLCTXLS;
  SLCIC = 0xFFFFFFFF;
  _sending = false;
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

  // Hold GPIO3 appropriately before any I2S activity (prevents startup glitch)
  pinMode(3, OUTPUT);
  digitalWrite(3, _inverted ? HIGH : LOW);

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

  _bckDiv = best_baseclkdiv;
  _clkDiv = best_clkdiv;

  // Allocate idle/reset zero buffer
  _idleBufSize = c_idleBufSize;
  _idleBuf = (uint8_t*)malloc(_idleBufSize);
  if (!_idleBuf) return false;
  memset(_idleBuf, _inverted ? 0xFF : 0x00, _idleBufSize);

  // Allocate pixel encode buffer and build initial descriptor chain
  if (!allocateEncodeBuffer(_numPixels, _encoder.getBytesPerPixel())) { end(); return false; }
  buildDescriptorChain();
  if (!_dmaDesc) { end(); return false; }

  s_dmaInstance = this;
  _sending = false;

  // Switch GPIO3 MUX to I2S (transitions LOW→LOW because I2S starts in idle)
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_I2SO_DATA);

  startI2s(_bckDiv, _clkDiv);

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
  s_dmaInstance = nullptr;
  if (_dmaDesc)  { free(_dmaDesc);  _dmaDesc  = nullptr; _dmaDescCnt = 0; }
  if (_idleBuf)  { free(_idleBuf);  _idleBuf  = nullptr; _idleBufSize = 0; }
  releaseEncodeBuffer();
  pinMode(_pin, INPUT);
  _initialized = false;
  _sending = false;
}

// ---------------------------------------------------------------------------
// setPixel / getPixelColor / encoded-buffer scaling
//   Pixel data starts at offset 0 in _encodeBuffer (no lead-in needed;
//   idle state ensures GPIO3 is LOW before the first pixel bit arrives).
// ---------------------------------------------------------------------------
void IRAM_ATTR Esp8266DmaBus::encodeBytes(const uint8_t* src, uint32_t* dst) const {
  const uint8_t bytes = _encoder.getBytesPerPixel();
  for (uint8_t b = 0; b < bytes; ++b) {
    uint32_t word = 0;
    const uint8_t value = src[b];
    for (int bit = 7; bit >= 0; --bit) {
      word <<= 4;
      if (_inverted) word |= (value & (1 << bit)) ? 0x1u : 0x7u;
      else           word |= (value & (1 << bit)) ? 0xEu : 0x8u;
    }
    dst[b] = word;
  }
}

void IRAM_ATTR Esp8266DmaBus::decodeBytes(const uint32_t* src, uint8_t* dst) const {
  const uint8_t bytes = _encoder.getBytesPerPixel();
  for (uint8_t b = 0; b < bytes; ++b) {
    const uint32_t word = src[b];
    uint8_t value = 0;
    for (int nib = 7; nib >= 0; --nib) {
      value <<= 1;
      const uint8_t step = (word >> (nib * 4)) & 0xF;
      if ((!_inverted && step == 0xE) || (_inverted && step == 0x1)) value |= 1;
    }
    dst[b] = value;
  }
}

IRAM_ATTR bool Esp8266DmaBus::setPixelColor(uint16_t pos, uint32_t c, uint16_t wwcw) {
  const uint8_t pixelBytes = _encoder.getBytesPerPixel();
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
  encodeBytes(src, dst);
  return true;
}

IRAM_ATTR uint32_t Esp8266DmaBus::getPixelColor(uint16_t pix) const {
  const uint8_t pixelBytes = _encoder.getBytesPerPixel();
  const uint32_t* src = (const uint32_t*)(_pixelData + (size_t)pix * pixelBytes * 4);
  uint8_t decoded[12];
  decodeBytes(src, decoded);
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

void Esp8266DmaBus::scaleEncodeBuffer(uint8_t scale) {
  if (!_pixelData || scale == 255) return;
  const uint8_t pixelBytes = _encoder.getBytesPerPixel();
  for (uint16_t pixel = 0; pixel < _numPixels; ++pixel) {
    uint32_t* encoded = (uint32_t*)(_pixelData + (size_t)pixel * pixelBytes * 4);
    uint8_t decoded[12];
    decodeBytes(encoded, decoded);
    _encoder.scalePixel(decoded, scale);
    encodeBytes(decoded, encoded);
  }
}

void Esp8266DmaBus::clearEncodeBuffer() {
  scaleEncodeBuffer(0); // TODO: this needs optimisation, it is slow
}

void Esp8266DmaBus::updatePrefix(const uint8_t* data, uint8_t len) {
  if (!_encodeBuffer || !data || _prefixLen == 0 || len == 0) return;
  if (len > _prefixLen) len = _prefixLen;
  uint32_t* dst = (uint32_t*)_encodeBuffer;
  for (uint8_t i = 0; i < len; i++) {
    uint32_t word = 0;
    uint8_t v = data[i];
    for (int bit = 7; bit >= 0; bit--) {
      word <<= 4;
      if (_inverted) word |= (v & (1 << bit)) ? 0x1u : 0x7u;
      else           word |= (v & (1 << bit)) ? 0xEu : 0x8u;
    }
    dst[i] = word;
  }
}

void Esp8266DmaBus::updateSuffix(const uint8_t* data, uint8_t len) {
  if (!_pixelData || !data || _suffixLen == 0 || len == 0) return;
  if (len > _suffixLen) len = _suffixLen;
  const size_t pixelBytes = (size_t)_numPixels * _encoder.getBytesPerPixel();
  uint32_t* dst = (uint32_t*)(_pixelData + pixelBytes * 4);
  for (uint8_t i = 0; i < len; i++) {
    uint32_t word = 0;
    uint8_t v = data[i];
    for (int bit = 7; bit >= 0; bit--) {
      word <<= 4;
      if (_inverted) word |= (v & (1 << bit)) ? 0x1u : 0x7u;
      else           word |= (v & (1 << bit)) ? 0xEu : 0x8u;
    }
    dst[i] = word;
  }
}

// ---------------------------------------------------------------------------
// show
//   Patches state[1] to break out of the idle loop into the pixel data,
//   then returns immediately. The ISR restores the idle loop when done.
// ---------------------------------------------------------------------------
bool Esp8266DmaBus::show() {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;
  waitForIdle();

  // Break the idle loop: state[1] now points to first pixel descriptor
  _dmaDesc[1].next_link_ptr = &_dmaDesc[c_stateBlockCount];
  _sending = true;
  _txStartMillis = millis();

  return true;
}

void Esp8266DmaBus::abortTransmit() const {
  _dmaDesc[1].next_link_ptr = &_dmaDesc[0];
  stopI2s();
  startI2s(_bckDiv, _clkDiv); // restart the idle loop with valid descriptors
  _sending = false;
}

bool Esp8266DmaBus::ensureIdle() const {
  if (!_sending) return true;
  if ((uint32_t)(millis() - _txStartMillis) <= WLEDPB_TRANSFER_TIMEOUT_MS) return false;
  abortTransmit();
  return true;
}

void Esp8266DmaBus::waitForIdle() const {
  const uint32_t start = millis();
  while (!ensureIdle() && (uint32_t)(millis() - start) <= WLEDPB_TRANSFER_TIMEOUT_MS) delay(1);
  if (_sending) abortTransmit();
}

bool Esp8266DmaBus::canShow() const {
  if (!_initialized) return true; // do not block if init failed! busmanager waits until all buses can show
  return ensureIdle();
}

} // namespace WLEDpixelBus

#endif // ESP8266