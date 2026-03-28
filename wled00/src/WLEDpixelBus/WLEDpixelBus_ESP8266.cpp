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
  : _pin(pin), _timing(timing), _order(order), _initialized(false), 
    _asyncBuf(nullptr), _asyncBufEnd(nullptr),
    _encodeBuffer(nullptr), _encodeBufferSize(0) {}

Esp8266UartBus::~Esp8266UartBus() {
  end();
  if (_encodeBuffer) free(_encodeBuffer);
}

// 1x Buffer allocation (only storing raw bytes, not UART bit-patterns)
bool Esp8266UartBus::allocateBuffer(size_t rawDataLen) {
  if (_encodeBufferSize >= rawDataLen) return true;
  if (_encodeBuffer) free(_encodeBuffer);
  _encodeBuffer = (uint8_t*)malloc(rawDataLen);
  if (!_encodeBuffer) return false;
  _encodeBufferSize = rawDataLen;
  return true;
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

bool Esp8266UartBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
  if (!pixels) { pixels = _pixelData; numPixels = _numPixels; cct = _cctData; }
  if (!_initialized || !pixels || numPixels == 0) return false;
  if (!canShow()) return false; // Ensure previous transfer is done

  size_t bpp = (_order >= ColorOrder::RGBWC) ? 5 : ((_order >= ColorOrder::RGBW) ? 4 : 3);
  size_t rawLen = numPixels * bpp;
  
  if (!allocateBuffer(rawLen)) return false;

  // Encode pixels to a flat raw byte buffer (1x size)
  ColorEncoder encoder(_order);
  uint8_t* out = _encodeBuffer;
  for (size_t i = 0; i < numPixels; i++) {
    encoder.encode(pixels[i], cct ? &cct[i] : nullptr, out);
    out += bpp;
  }

  uint8_t uartNum = (_pin == 2) ? 1 : 0;
  _asyncBuf = _encodeBuffer;
  _asyncBufEnd = _encodeBuffer + rawLen;

  // Enable the "TX FIFO Empty" interrupt to trigger the ISR
  USIE(uartNum) |= (1 << UIFE);
  
  return true;
}

bool Esp8266UartBus::canShow() const {
  if (!_initialized) return false;
  // Ready if we have no more data to send
  return (_asyncBuf >= _asyncBufEnd);
}

void Esp8266UartBus::waitComplete() {
  while (!canShow()) { yield(); }
}


//==============================================================================
// ESP8266 BitBang Bus
//==============================================================================

Esp8266BitBangBus::Esp8266BitBangBus(int8_t pin, const LedTiming& timing, ColorOrder order)
  : _pin(pin), _timing(timing), _order(order), _initialized(false) {}

Esp8266BitBangBus::~Esp8266BitBangBus() {
  end();
}

bool Esp8266BitBangBus::begin() {
  if (_initialized) return true;
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
  setTiming(_timing);
  _initialized = true;
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

bool Esp8266BitBangBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
  if (!pixels) { pixels = _pixelData; numPixels = _numPixels; cct = _cctData; }
  if (!_initialized || !pixels || numPixels == 0) return false;

  size_t bpp = (_order >= ColorOrder::RGBWC) ? 5 : ((_order >= ColorOrder::RGBW) ? 4 : 3);
  ColorEncoder encoder(_order);
  uint32_t mask = 1 << _pin;
  uint8_t pData[5];

  os_intr_lock();
  for (size_t i = 0; i < numPixels; i++) {
    encoder.encode(pixels[i], cct ? &cct[i] : nullptr, pData);
    for (size_t b = 0; b < bpp; b++) {
      uint8_t v = pData[b];
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

bool Esp8266BitBangBus::canShow() const {
  return _initialized;
}

void Esp8266BitBangBus::waitComplete() {
  // Blocking show, always complete upon return
}


//==============================================================================
// ESP8266 DMA Bus 
//==============================================================================

Esp8266DmaBus::Esp8266DmaBus(int8_t pin, const LedTiming& timing, ColorOrder order)
  : _pin(pin), _timing(timing), _order(order), _initialized(false), _encodeBuffer(nullptr), _encodeBufferSize(0) {}

Esp8266DmaBus::~Esp8266DmaBus() {
  end();
  if (_encodeBuffer) {
    free(_encodeBuffer);
    _encodeBuffer = nullptr;
  }
}

bool Esp8266DmaBus::allocateBuffer(size_t len) {
  if (_encodeBufferSize >= len) return true;
  if (_encodeBuffer) free(_encodeBuffer);
  _encodeBuffer = (uint8_t*)malloc(len);
  if (!_encodeBuffer) return false;
  _encodeBufferSize = len;
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
  return true;
}

void Esp8266DmaBus::end() {
  if (!_initialized) return;
  i2s_end();
  pinMode(_pin, INPUT);
  _initialized = false;
}

bool Esp8266DmaBus::show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct) {
  if (!pixels) { pixels = _pixelData; numPixels = _numPixels; cct = _cctData; }
  if (!_initialized || !pixels || numPixels == 0) return false;

  size_t bpp = (_order >= ColorOrder::RGBWC) ? 5 : ((_order >= ColorOrder::RGBW) ? 4 : 3);
  
  // 4 step cadence per LED bit. 
  // 1 LED bit = 4 I2S bits (half-byte / nibble).
  // 1 LED byte (8 bits) = 32 I2S bits (4 bytes).
  size_t outLen = numPixels * bpp * 4 + 40; // + 40 zero bytes for reset (latch)
  if (!allocateBuffer(outLen)) return false;

  memset(_encodeBuffer, 0, outLen);

  ColorEncoder encoder(_order);
  uint32_t* out32 = (uint32_t*)_encodeBuffer;
  uint8_t pData[5];

  // Using inverted 4-step cadence:
  // Normally:
  // 1 = 0b1110 (0xE)
  // 0 = 0b1000 (0x8)
  for (size_t i = 0; i < numPixels; i++) {
    encoder.encode(pixels[i], cct ? &cct[i] : nullptr, pData);
    for (size_t b = 0; b < bpp; b++) {
      uint32_t i2sData = 0;
      uint8_t v = pData[b];
      for (int bit = 7; bit >= 0; bit--) {
        i2sData <<= 4;
        if (v & (1 << bit)) {
          i2sData |= 0xE; // 1110
        } else {
          i2sData |= 0x8; // 1000
        }
      }
      *out32++ = i2sData;
    }
  }

  // Write using Core I2S: It processes full `uint32` buffer natively over I2S
  uint32_t* buf32 = (uint32_t*)_encodeBuffer;
  for (size_t i = 0; i < outLen / 4; i++) {
    i2s_write_sample(buf32[i]);
  }

  return true;
}

bool Esp8266DmaBus::canShow() const {
  return _initialized && !i2s_is_full();
}

void Esp8266DmaBus::waitComplete() {
  // Not strictly supported blocking via I2S, wait for FIFO room
  while (i2s_is_full()) { yield(); }
}

} // namespace WLEDpixelBus

#endif // WLEDPB_ESP8266
