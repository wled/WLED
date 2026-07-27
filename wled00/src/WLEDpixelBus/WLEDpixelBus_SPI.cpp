/*-------------------------------------------------------------------------
WLEDpixelBus - SPI 2-pin clocked LEDs driver implementation

supports ESP32, S3, S2 and C3 both hardware SPI and BitBanged output
supports hardware brightness on APA102 for improved color resolution

written by Damian Schneider @dedehai 2026

-------------------------------------------------------------------------*/

#include "WLEDpixelBus_SPI.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "soc/gpio_reg.h"
#endif

#define SPI_MAX_CLOCK_HZ 20000000UL // maximum SPI clock supported by all target platforms (ESP8266 max is 20 MHz, ESP32 can do 80 MHz but we clamp to 20 MHz for compatibility with ESP8266 and timing accuracy)

namespace WLEDpixelBus {

// Derive SPI clock Hz from kHz timing set through busconfig (see busmanager)
static uint32_t timingToClockHz(uint16_t frequencykHz) {
  uint32_t frequencyHz = frequencykHz*1000;
  if (frequencyHz < 1000000)  frequencyHz = 1000000;   // minimum 1 MHz
  if (frequencyHz > SPI_MAX_CLOCK_HZ) frequencyHz = SPI_MAX_CLOCK_HZ;
  return frequencyHz;
}

SpiBus::SpiBus(int8_t dataPin, int8_t clockPin, uint16_t frequencykHz, uint8_t colorOrder, uint8_t numChannels, bool useHardwareSpi, uint8_t ledType)
  : _dataPin(dataPin)
  , _clockPin(clockPin)
  , _useHardware(useHardwareSpi)
  , _initialized(false)
  , _clockHz(timingToClockHz(frequencykHz))
{
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
}

SpiBus::~SpiBus() {
  end();
}

bool SpiBus::begin() {
  if (_initialized) return true;

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
  uint8_t allocBytes = _encoder.getPixelBytes();
  if (_ledType == TYPE_APA102 || _ledType == TYPE_P9813) allocBytes++; // need more space for per-pixel header byte, see show()
  if (!allocateEncodeBuffer(_numPixels, allocBytes)) { end(); return false; }
  return true;
}

void SpiBus::end() {
  if (!_initialized) return;
  if (_useHardware) {
    SPI.end();
  }
  #if defined(ARDUINO_ARCH_ESP32)
  if (_dataPin >= 0) gpio_reset_pin((gpio_num_t)_dataPin);
  if (_clockPin >= 0) gpio_reset_pin((gpio_num_t)_clockPin);
  #else
  pinMode(_dataPin, INPUT);
  pinMode(_clockPin, INPUT);
  #endif
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
    for (uint32_t i = 0; i < 4; i++) sendByte(0x00);
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
    const uint32_t n = (numPixels + 15) / 16;
    for (uint32_t i = 0; i < n; i++) sendByte(0x00);
  } else if (_ledType == TYPE_LPD6803) {
    const uint32_t n = (numPixels + 7) / 8;
    for (uint32_t i = 0; i < n; i++) sendByte(0x00);
  } else if (_ledType == TYPE_LPD8806) {
    const uint32_t n = (numPixels + 31) / 32;
    for (uint32_t i = 0; i < n; i++) sendByte(0xFF);
  } else if (_ledType == TYPE_P9813) {
    for (uint32_t i = 0; i < 4; i++) sendByte(0x00);
  }
  // TYPE_WS2801: nothing to send
}

bool SpiBus::show() {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;

  const uint8_t pixelBytes = _encoder.getPixelBytes();

  if (_useHardware) {
    SPI.beginTransaction(SPISettings(_clockHz, MSBFIRST, SPI_MODE0)); // beginTransaction applies frequency, bit order, and SPI mode
  }

  if (_ledType == TYPE_APA102 || _ledType == TYPE_P9813) {
    // These chips need a per-pixel header byte (brightness/flag) in front of the RGB data
    // Expand the compact encoded buffer in-place, starting from the last pixel (no data is overwritten)
    // APA102 wire format: [0xE0|bri, B, G, R], P9813 wire format:  [flag, B, G, R]
    // note: this "extension" is intentionally not done when encoding the buffer so we can keep the encoding branch-free and fast for all types.
    const uint8_t wireBytes = pixelBytes + 1;
    for (uint32_t i = _numPixels; i > 0; i--) {
      uint8_t* src = _encodeBuffer + (size_t)(i - 1) * pixelBytes;
      uint8_t* dst = _encodeBuffer + (size_t)(i - 1) * wireBytes;
      for (int8_t b = pixelBytes - 1; b >= 0; b--) dst[b + 1] = src[b];
      if (_ledType == TYPE_APA102) {
        dst[0] = 0xE0 | _apa102HwBri;
      } else { // TYPE_P9813
        const uint8_t b = dst[1], g = dst[2], r = dst[3];
        dst[0] = 0xC0 | ((~b & 0xC0) >> 2) | ((~g & 0xC0) >> 4) | ((~r & 0xC0) >> 6);
      }
    }
    sendStartFrame(_numPixels);
    uint8_t* src = _encodeBuffer;
    if (_useHardware) {
      SPI.writeBytes(src, (size_t)_numPixels * wireBytes);
    } else {
      const size_t total = (size_t)_numPixels * wireBytes;
      for (size_t i = 0; i < total; i++) sendByte(src[i]);
    }
    #ifdef WLED_DISABLE_GLOBAL_PIXELBUFFER
    // Restore the compact layout so getPixelColor() and the next frame's getPixelColor() see the buffer in its normal encoded format.
    // When the global _pixels[] buffer is disabled, this is necessary for all subsequent getPixelColor() calls. Still way faster than not doing full buffer encoding.
    src = _encodeBuffer + 1; // skip header
    for (uint32_t i = 0; i < _numPixels; ++i) {
      uint8_t* dst = _encodeBuffer + (size_t)i * pixelBytes;
      for (uint8_t b = 0; b < pixelBytes; b++) dst[b] = src[b];
      src += wireBytes; // move to next pixel
    }
    #endif
  } else {
    // WS2801 / LPD8806 / LPD6803: raw encoded bytes, no per-pixel framing byte
    uint8_t* src = _encodeBuffer;
    if (_useHardware) {
      SPI.writeBytes(src, pixelBytes*_numPixels);
    }
    else {
      for (uint16_t i = 0; i < _numPixels; i++) {
        for (uint8_t ch = 0; ch < pixelBytes; ch++) sendByte(src[ch]);
        src += pixelBytes;
      }
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
