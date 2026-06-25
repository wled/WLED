#include "WLEDpixelBus.h"
#include "WLEDpixelBus_SPI.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "soc/gpio_reg.h"
#endif

namespace WLEDpixelBus {

// Derive SPI clock Hz from timing bitPeriod, clamped to [1 MHz, 20 MHz].
// APA102 {250,250,250,250,0} -> period=500ns -> 2 MHz
// WS2801 {500,500,500,500,1000} -> period=1000ns -> 1 MHz (clamped min)
static uint32_t timingToClockHz(const LedTiming& t) {
  const uint32_t periodNs = t.bitPeriod();
  if (periodNs == 0) return 2000000;
  uint32_t hz = 1000000000UL / periodNs;
  if (hz < 1000000)  hz = 1000000;   // minimum 1 MHz
  if (hz > 20000000) hz = 20000000;  // maximum 20 MHz  // TODO: make this a define and check if ESP8266 can do 40 MHz
  return hz;
}

SpiBus::SpiBus(int8_t dataPin, int8_t clockPin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, bool useHardwareSpi, uint8_t ledType)
  : _dataPin(dataPin), _clockPin(clockPin), _timing(timing),
    _useHardware(useHardwareSpi), _initialized(false), _clockHz(0) {
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
}

SpiBus::~SpiBus() {
  end();
}

bool SpiBus::begin() {
  if (_initialized) return true;

  _clockHz = timingToClockHz(_timing);

  if (_useHardware) {
    // On ESP32 SPI.begin(sck, miso, mosi, ss) must be called with the actual pins so the IO matrix routes the SPI peripheral to the right GPIOs.
    // On ESP8266 the hardware SPI uses fixed pins (MOSI=GPIO13, SCK=GPIO14) so no pin args are needed.
#if defined(ARDUINO_ARCH_ESP32)
    SPI.begin(_clockPin, 127, _dataPin, -1); // note: in arduino core, -1 means "default" not "none", passing 127 as the MISO pin is a workaround to prevent SPI.begin() assign the default pin, see #5670
#else
    SPI.begin();
#endif
    // Frequency and mode are applied per-frame via beginTransaction(); do NOT call the deprecated SPI.setFrequency / SPI.setDataMode here.
  } else {
    // bit banged output
    pinMode(_dataPin, OUTPUT);
    pinMode(_clockPin, OUTPUT);
    // Pre-compute bitmasks so the hot-path bit-bang loop does register writes not the ~10x-slower digitalWrite().
#if defined(ARDUINO_ARCH_ESP32)
    _dataHigh = (_dataPin >= 32);
    _clkHigh  = (_clockPin >= 32);
    _dataMask = 1UL << (_dataHigh ? (_dataPin - 32) : _dataPin);
    _clkMask  = 1UL << (_clkHigh  ? (_clockPin - 32) : _clockPin);
    // Drive both lines LOW initially
#ifdef ESP_HAS_HIGH_GPIO_BANK
    if (_dataHigh) REG_WRITE(GPIO_OUT1_W1TC_REG, _dataMask); else REG_WRITE(GPIO_OUT_W1TC_REG, _dataMask);
    if (_clkHigh)  REG_WRITE(GPIO_OUT1_W1TC_REG, _clkMask);  else REG_WRITE(GPIO_OUT_W1TC_REG, _clkMask);
#else
    REG_WRITE(GPIO_OUT_W1TC_REG, _dataMask);
    REG_WRITE(GPIO_OUT_W1TC_REG, _clkMask);
#endif
#elif defined(ARDUINO_ARCH_ESP8266)
    _dataMask = 1UL << _dataPin;
    _clkMask  = 1UL << _clockPin;
    GPOC = _dataMask;
    GPOC = _clkMask;
#endif
  }

  _initialized = true;
  if (!allocateEncodeBuffer(_numPixels, _encoder.getPixelBytes())) { end(); return false; }
  return true;
}

void SpiBus::end() {
  if (!_initialized) return;
  if (_useHardware) {
    SPI.end();
  }
  if (_dataPin >= 0) gpio_reset_pin((gpio_num_t)_dataPin);
  if (_clockPin >= 0) gpio_reset_pin((gpio_num_t)_clockPin);
  if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; _encodeBufferSize = 0; }
  _initialized = false;
}

// Fast inline helpers for bit-bang GPIO register access.
// Using W1TS/W1TC (write-1-to-set/clear) registers avoids read-modify-write race conditions and is faster than GPIO_OUT read-modify-write.
inline void SpiBus::bbSetData(bool high) const {
#if defined(ARDUINO_ARCH_ESP32)
#ifdef ESP_HAS_HIGH_GPIO_BANK
  if (high) {
    if (_dataHigh) REG_WRITE(GPIO_OUT1_W1TS_REG, _dataMask); else REG_WRITE(GPIO_OUT_W1TS_REG, _dataMask);
  } else {
    if (_dataHigh) REG_WRITE(GPIO_OUT1_W1TC_REG, _dataMask); else REG_WRITE(GPIO_OUT_W1TC_REG, _dataMask);
  }
#else
  if (high) {
    REG_WRITE(GPIO_OUT_W1TS_REG, _dataMask);
  } else {
    REG_WRITE(GPIO_OUT_W1TC_REG, _dataMask);
  }
  // TODO: on the C3 BB output is much faster when using direct CPU access through cpu pin groups (see espressif documentation), could potentially achieve several MHz clockspeed
#endif
#elif defined(ARDUINO_ARCH_ESP8266)
  if (high) GPOS = _dataMask; else GPOC = _dataMask;
#endif
}

inline void SpiBus::bbSetClk(bool high) const {
#if defined(ARDUINO_ARCH_ESP32)
#ifdef ESP_HAS_HIGH_GPIO_BANK
  if (high) {
    if (_clkHigh) REG_WRITE(GPIO_OUT1_W1TS_REG, _clkMask); else REG_WRITE(GPIO_OUT_W1TS_REG, _clkMask);
  } else {
    if (_clkHigh) REG_WRITE(GPIO_OUT1_W1TC_REG, _clkMask); else REG_WRITE(GPIO_OUT_W1TC_REG, _clkMask);
  }
#else
  if (high) {
    REG_WRITE(GPIO_OUT_W1TS_REG, _clkMask);
  } else {
    REG_WRITE(GPIO_OUT_W1TC_REG, _clkMask);
  }
#endif
#elif defined(ARDUINO_ARCH_ESP8266)
  if (high) GPOS = _clkMask; else GPOC = _clkMask;
#endif
}

void SpiBus::sendByte(uint8_t d) {
  if (_useHardware) {
    SPI.transfer(d);
  } else {
    // MSB-first bit-bang, SPI mode 0 (CPOL=0, CPHA=0): data is set up while clock is low, sampled on rising edge.
    for (uint8_t i = 0; i < 8; i++) {
      bbSetData(d & 0x80);
      bbSetClk(true);
      d <<= 1;
      bbSetClk(false);
    }
  }
}

void SpiBus::sendStartFrame(uint16_t numPixels) {
  if (_ledType == TYPE_LPD8806) {
    // LPD8806: start frame is ceil(N/32) zero bytes to clock in the initial latch
    const uint16_t n = (numPixels + 31) / 32;
    for (uint16_t i = 0; i < n; i++) sendByte(0x00);
  } else if (_ledType != TYPE_WS2801) {
    // APA102 / LPD6803 / P9813: fixed 4-byte zero start frame
    // WS2801: no start frame — latch is the reset-time gap between frames
    sendByte(0x00); sendByte(0x00); sendByte(0x00); sendByte(0x00);
  }
}

void SpiBus::sendEndFrame(uint16_t numPixels) {
  // APA102: ceil(N/16) zero bytes.  TODO: NPB seems to send zero bytes, datasheet states four 0xFF bytes is the end frame
  //   Each APA102 delays the clock by one half-cycle; N LEDs need N/2 extra clock pulses to ensure the last pixel latches. One byte = 8 clocks,
  //   so ceil(N/16) bytes provide the required ceil(N/2) pulses. The APA102 datasheet's "4 zero bytes" claim is only valid for N ≤ 64.
  // LPD6803: ceil(N/8) zero bytes (one clock per pixel required).
  // LPD8806: ceil(N/32) 0xFF bytes (high level = latch for MSB-set pixel data).
  // P9813: fixed 4 zero bytes.
  // WS2801: nothing — latch is a timing gap, not a byte sequence.
  if (_ledType == TYPE_APA102) {
    const uint16_t n = (numPixels + 15) / 16;
    for (uint16_t i = 0; i < n; i++) sendByte(0x00);
  } else if (_ledType == TYPE_LPD6803) {
    const uint16_t n = (numPixels + 7) / 8;
    for (uint16_t i = 0; i < n; i++) sendByte(0x00);
  } else if (_ledType == TYPE_LPD8806) {
    const uint16_t n = (numPixels + 31) / 32;
    for (uint16_t i = 0; i < n; i++) sendByte(0xFF);
  } else if (_ledType == TYPE_P9813) {
    sendByte(0x00); sendByte(0x00); sendByte(0x00); sendByte(0x00);
  }
  // TYPE_WS2801: nothing to send
}

bool SpiBus::show(const uint32_t* /*pixels*/, uint16_t /*numPixels*/, const CctPixel* /*cct*/) {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;

  const uint8_t pixelBytes = _encoder.getPixelBytes();

  if (_useHardware) {
    // beginTransaction applies frequency, bit order, and SPI mode atomically.
    // This is mandatory on ESP32 — settings applied outside a transaction are ignored by the hardware.
    SPI.beginTransaction(SPISettings(_clockHz, MSBFIRST, SPI_MODE0));
  }

  sendStartFrame(_numPixels);

  if (_ledType == TYPE_APA102) {
    // APA102 per-pixel wire format: [0xE0|brightness5bit, byte0, byte1, byte2]
    // 0xE0|0x1F == 0xFF == full hardware brightness (5-bit field, 0x1F = max)
    for (uint16_t i = 0; i < _numPixels; i++) {
      sendByte(0xE0 | _apa102HwBri);
      const uint8_t* src = _encodeBuffer + (size_t)i * pixelBytes;
      for (uint8_t ch = 0; ch < pixelBytes; ch++) sendByte(src[ch]);
    }
  } else if (_ledType == TYPE_P9813) {
    // P9813 per-pixel wire format: [flag, B, G, R]
    // flag = 0xC0 | (~B[7:6]>>2) | (~G[7:6]>>4) | (~R[7:6]>>6)
    // _encodeBuffer bytes are already in wire order (BGR = indices 0,1,2 when
    // color order is configured as BGR, which is the P9813 native order).
    for (uint16_t i = 0; i < _numPixels; i++) {
      const uint8_t* src = _encodeBuffer + (size_t)i * pixelBytes;
      const uint8_t b = src[0], g = src[1], r = src[2];
      const uint8_t flag = 0xC0 | ((~b & 0xC0) >> 2) | ((~g & 0xC0) >> 4) | ((~r & 0xC0) >> 6);
      sendByte(flag); sendByte(b); sendByte(g); sendByte(r);
    }
  } else {
    // WS2801 / LPD8806 / LPD6803: raw encoded bytes, no per-pixel framing byte
    for (uint16_t i = 0; i < _numPixels; i++) {
      const uint8_t* src = _encodeBuffer + (size_t)i * pixelBytes;
      for (uint8_t ch = 0; ch < pixelBytes; ch++) sendByte(src[ch]);
    }
  }

  sendEndFrame(_numPixels);

  if (_useHardware) {
    SPI.endTransaction();
  }

  return true;
}

void SpiBus::setColorOrder(uint8_t co) {
  _encoder = ColorEncoder(co, _encoder.getColorChannels(), _ledType);
}

bool SpiBus::canShow() const {
  return true;
}

} // namespace WLEDpixelBus
