/*-------------------------------------------------------------------------
WLEDpixelBus - SPI 2-pin clocked LEDs driver implementation

written by Damian Schneider @dedehai 2026

supports ESP32, S3, S2 and C3 both hardware SPI and BitBanged output
supports hardware brightness on APA102 for improved color resolution
-------------------------------------------------------------------------*/

#include "WLEDpixelBus_SPI.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "soc/gpio_reg.h"
#endif

#define SPI_MAX_CLOCK_HZ 20000000UL // maximum SPI clock supported by all target platforms (ESP8266 max is 20 MHz, ESP32 can do 80 MHz but we clamp to 20 MHz for compatibility with ESP8266 and timing accuracy)

namespace WLEDpixelBus {

// Derive SPI clock Hz from kHz timing set through busconfig (see busmanager)
static uint32_t kHzToClockHz(uint16_t frequencykHz) {
  uint32_t frequencyHz = frequencykHz*1000;
  if (frequencyHz < 1000000)  frequencyHz = 1000000;   // minimum 1 MHz
  if (frequencyHz > SPI_MAX_CLOCK_HZ) frequencyHz = SPI_MAX_CLOCK_HZ;
  return frequencyHz;
}

SpiBus::SpiBus(int8_t dataPin, int8_t clockPin, uint16_t frequencykHz, uint8_t colorOrder, uint8_t numChannels, bool useHardware, uint8_t ledType)
  : _dataPin(dataPin)
  , _clockPin(clockPin)
  , _useHardware(useHardware)
  , _initialized(false)
  , _clockHz(kHzToClockHz(frequencykHz))
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
    SPI.begin(_clockPin, -1, _dataPin, -1); // note: the Arduino core interprets -1 as "no pin" (in older versions it meant "default pin" this bug was fixed)
#else
    SPI.begin();
#endif
    // Frequency and mode are applied per-frame via beginTransaction(); do NOT call the deprecated SPI.setFrequency / SPI.setDataMode here
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
  uint8_t allocBytes = _encoder.getBytesPerPixel();
  if (_ledType == TYPE_APA102 || _ledType == TYPE_P9813) allocBytes++; // need more space for per-pixel header byte, see show()
  if (!allocateEncodeBuffer(_numPixels, allocBytes)) { end(); return false; }
  return true;
}

void SpiBus::end() {
  if (_useHardware) {
    SPI.end();
  }
  #if defined(ARDUINO_ARCH_ESP32)
  if (_dataPin >= 0)  gpio_reset_pin((gpio_num_t)_dataPin);
  if (_clockPin >= 0) gpio_reset_pin((gpio_num_t)_clockPin);
  #else
  pinMode(_dataPin, INPUT);
  pinMode(_clockPin, INPUT);
  #endif
  releaseEncodeBuffer();
  _initialized = false;
}

// Fast inline helpers for bit-bang GPIO register access.
inline void SpiBus::bbSetData(bool high) const {
#if defined(ARDUINO_ARCH_ESP32)
#ifdef ESP_HAS_HIGH_GPIO_BANK
  if (high) {
    if (_dataHigh) REG_WRITE(GPIO_OUT1_W1TS_REG, _dataMask); else REG_WRITE(GPIO_OUT_W1TS_REG, _dataMask); // set
  } else {
    if (_dataHigh) REG_WRITE(GPIO_OUT1_W1TC_REG, _dataMask); else REG_WRITE(GPIO_OUT_W1TC_REG, _dataMask); // clear
  }
#else
  if (high) {
    REG_WRITE(GPIO_OUT_W1TS_REG, _dataMask); // set
  } else {
    REG_WRITE(GPIO_OUT_W1TC_REG, _dataMask); // clear
  }
#endif
#elif defined(ARDUINO_ARCH_ESP8266)
  if (high) GPOS = _dataMask; // set
  else GPOC = _dataMask;      // clear
#endif
}

inline void SpiBus::bbSetClk(bool high) const {
#if defined(ARDUINO_ARCH_ESP32)
#ifdef ESP_HAS_HIGH_GPIO_BANK
  if (high) {
    if (_clkHigh) REG_WRITE(GPIO_OUT1_W1TS_REG, _clkMask); else REG_WRITE(GPIO_OUT_W1TS_REG, _clkMask); // set
  } else {
    if (_clkHigh) REG_WRITE(GPIO_OUT1_W1TC_REG, _clkMask); else REG_WRITE(GPIO_OUT_W1TC_REG, _clkMask); // clear
  }
#else
  if (high) {
    REG_WRITE(GPIO_OUT_W1TS_REG, _clkMask); // set
  } else {
    REG_WRITE(GPIO_OUT_W1TC_REG, _clkMask); // clear
  }
#endif
#elif defined(ARDUINO_ARCH_ESP8266)
  if (high) GPOS = _clkMask; // set
  else GPOC = _clkMask;      // clear
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
  uint32_t n = 0; // default: no start frame (WS2801)
  if (_ledType == TYPE_LPD8806) {
    n = (numPixels + 31) / 32; // LPD8806: start frame is ceil(N/32) zero bytes according to NPB, there is no datasheet info
  } else if (_ledType != TYPE_WS2801) {
    n = 4; // APA102 / LPD6803 / P9813: fixed 4-byte zero start frame
  }
  for (uint32_t i = 0; i < n; i++) sendByte(0x00);
}

void SpiBus::sendEndFrame(uint16_t numPixels) {
  uint8_t val = 0; // default end frame byte
  uint32_t n = 0;  // default: no end frame (WS2801: nothing to send, clk=low for 500us is the latch trigger)
  if (_ledType == TYPE_APA102) {
    n = 1 + numPixels / 16;    // APA102/SK9822: datasheet states 4 0xFF bytes, pololu.com states it is unreliable and better send at least (numPixels-1)/2 zero bits
  } else if (_ledType == TYPE_LPD6803) {
    n = (numPixels + 7) / 8;   // LPD6803: one clock per pixel required, data low
  } else if (_ledType == TYPE_LPD8806) {
    n = (numPixels + 31) / 32; // LPD8806: stop frame is ceil(N/32) 0xFF bytes according to NPB, there is no datasheet info
    val = 0xFF;
  } else if (_ledType == TYPE_P9813) {
    n = 4;                     // P9813: fixed 4 zero bytes.
  }
  for (uint32_t i = 0; i < n; i++) sendByte(val);
}

bool SpiBus::show() {
  if (!_initialized || !_encodeBuffer || _numPixels == 0) return false;

  const uint8_t pixelBytes = _encoder.getBytesPerPixel();

  if (_useHardware) {
    SPI.beginTransaction(SPISettings(_clockHz, MSBFIRST, SPI_MODE0)); // beginTransaction applies frequency, bit order, and SPI mode
  }

  if (_ledType == TYPE_APA102 || _ledType == TYPE_P9813) {
    // These chips need a per-pixel header byte (brightness/flag) in front of the RGB data
    // Expand the compact encoded buffer in-place, starting from the last pixel so no data is overwritten
    // Native wire formats: APA102: [0xE0|bri, B, G, R], P9813: [flag, B, G, R]
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
        dst[0] = 0xC0 | ((~b & 0xC0) >> 2) | ((~g & 0xC0) >> 4) | ((~r & 0xC0) >> 6); // flag byte is [11 B' G' R'], where B', G', R' are the inverted top 2 bits of each color channel
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
    // Always restore the compact layout: readback, clear, scaling, and the next
    // frame must never depend on whether the global pixel buffer is enabled.
    src = _encodeBuffer + 1;
    for (uint32_t i = 0; i < _numPixels; ++i) {
      uint8_t* dst = _encodeBuffer + (size_t)i * pixelBytes;
      for (uint8_t b = 0; b < pixelBytes; b++) dst[b] = src[b];
      src += wireBytes;
    }
  } else {
    // WS2801 uses the compact encoded stream directly
    // LPD6803 and LPD8806 require protocol-specific payload conversion in addition to their frames
    // Note: custom color channels are not supported for clocked LED strips, only color ordering
    sendStartFrame(_numPixels);
    const uint8_t* src = _encodeBuffer;
    if (_ledType != TYPE_LPD6803 && _ledType != TYPE_LPD8806) {
      if (_useHardware) {
        SPI.writeBytes(src, (size_t)pixelBytes * _numPixels);
      } else {
        for (size_t i = 0; i < (size_t)pixelBytes * _numPixels; ++i) sendByte(src[i]);
      }
    } else {
      for (uint16_t i = 0; i < _numPixels; ++i, src += pixelBytes) {
        uint8_t wire[3];
        size_t wireLen;
        if (_ledType == TYPE_LPD6803) {
          // 16bit per pixel: RGB555, high byte first, highest bit is always set
          const uint16_t color555 = 0x8000u | ((uint16_t)(src[0] & 0xF8) << 7) | ((uint16_t)(src[1] & 0xF8) << 2) | ((uint16_t)(src[2] & 0xF8) >> 3);
          wire[0] = color555 >> 8;
          wire[1] = color555 & 0xFF;
          wireLen = 2;
        } else {
          // each chip has 6 channels, 7-bit colors, highest bit is always set. We send 3 bytes at a time. TODO: this may need refinement if there are strips with non-RGB encoding
          wire[0] = (src[0] >> 1) | 0x80;
          wire[1] = (src[1] >> 1) | 0x80;
          wire[2] = (src[2] >> 1) | 0x80;
          wireLen = 3;
        }
        if (_useHardware) SPI.writeBytes(wire, wireLen);
        else for (size_t b = 0; b < wireLen; ++b) sendByte(wire[b]);
      }
    }
  }

  sendEndFrame(_numPixels);

  if (_useHardware) {
    SPI.endTransaction();
  }

  return true;
}

bool SpiBus::canShow() const {
  return true; // show() is synchronous so we are always ready to call it again
}

} // namespace WLEDpixelBus
