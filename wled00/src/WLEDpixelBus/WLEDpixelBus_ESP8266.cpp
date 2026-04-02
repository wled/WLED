#include "WLEDpixelBus_ESP8266.h"

#ifdef WLEDPB_ESP8266

#include <Arduino.h>
#include <HardwareSerial.h>
#include <uart_register.h>
#include <eagle_soc.h>
#include <i2s_reg.h>
#include <slc_register.h>
#include <user_interface.h>

#ifdef ARDUINO_ESP8266_MAJOR
#include <core_esp8266_i2s.h>
#else
#include <i2s.h>
#endif

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
// ESP8266 DMA Bus 
//==============================================================================

Esp8266DmaBus::Esp8266DmaBus(int8_t pin, const LedTiming& timing, ColorOrder order)
  : _pin(pin), _timing(timing), _order(order), _encoder(order), _initialized(false) {}

Esp8266DmaBus::~Esp8266DmaBus() {
  end();
}

bool Esp8266DmaBus::allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) {
  // 4-step I2S encoding: each source byte → 4 bytes; 40 extra bytes for reset latch
  size_t needed = (size_t)numPixels * numChannels * 4 + 40;
  if (_encodeBuffer && _encodeBufferSize >= needed) return true;
  if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; }
  if (needed == 0) return true;
  _encodeBuffer = (uint8_t*)malloc(needed);
  if (!_encodeBuffer) { _encodeBufferSize = 0; return false; }
  memset(_encodeBuffer, 0, needed);
  _encodeBufferSize = needed;
  return true;
}

void Esp8266DmaBus::updateI2sTiming() {
  // Setup using Native ESP8266 Core I2S
  uint32_t bitPeriod = _timing.bitPeriod();
  if (bitPeriod == 0) bitPeriod = 1250;
  
  // We map 4 I2S bits to 1 LED bit (4-step cadence).
  // Each LED bit needs 4 I2S periods, thus our desired I2S clock is 4x the LED bit rate.
  // periodNs is time per LED bit. I2S bit clock period = bitPeriod / 4.
  // I2S bits/sec = 1,000,000,000 / (bitPeriod / 4) = 4,000,000,000 / bitPeriod.
  // Using core `i2s_begin` which sets a clock and routing:
  // Actually, `i2s_begin` expects a sample rate for 32-bit (stereo 16+16) frames.
  // Sample rate = (I2S bits/sec) / 32
  uint32_t sampleRate = (4000000000ULL / bitPeriod) / 32;
  i2s_set_rate(sampleRate);
}

bool Esp8266DmaBus::begin() {
  if (_initialized) return true;
  if (_pin != 3) return false; // ESP8266 I2S RX is GPIO3 for NeoPixels
  
  // Begin core I2S subsystem without driving clocks on GPIO15/GPIO2!
  i2s_rxtxdrive_begin(false, true, false, false);

  // To make GPIO3 act as I2S Data Out instead of I2S IN, configure registers manually:
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, 1); // Select I2SO_DATA on GPIO3

  updateI2sTiming();

  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getNumChannels())) { end(); return false; }
  return true;
}

void Esp8266DmaBus::end() {
  if (!_initialized) return;
  i2s_end();
  if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; _encodeBufferSize = 0; }
  pinMode(_pin, INPUT);
  _initialized = false;
}

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

// since the colors are laready 4-step encoded, we need to decode first, scale then reencode.
void Esp8266DmaBus::scaleAll(uint8_t scale) {
  if (!_encodeBuffer || scale == 255) return;
  uint8_t numCh = _encoder.getNumChannels();
  uint32_t* buf = (uint32_t*)_encodeBuffer;
  size_t numWords = (size_t)_numPixels * numCh; // the reset bytes at the end are zero, scaling 0 stays 0
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

bool Esp8266DmaBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;

  // Write the already-4-step-encoded buffer via Core I2S
  const uint32_t* buf32 = (const uint32_t*)_encodeBuffer;
  size_t numWords = _encodeBufferSize / 4;
  for (size_t i = 0; i < numWords; i++) {
    i2s_write_sample(buf32[i]);
  }
  return true;
}

bool Esp8266DmaBus::canShow() const {
  return _initialized && !i2s_is_full();
}

} // namespace WLEDpixelBus

#endif // WLEDPB_ESP8266
