/*-------------------------------------------------------------------------
WLEDpixelBus - Lightweight LED driver library for WLED

written by Damian Schneider @dedehai 2026

-------------------------------------------------------------------------*/

#pragma once

#include <Arduino.h>
#include <cstring>

#if defined(ARDUINO_ARCH_ESP32)

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "esp_idf_version.h"

// I2S support: targets where the LCD Intel 8080 bus is present (ESP32, ESP32-S2, ESP32-S3)
#if SOC_LCD_I80_BUSES
  #define WLEDPB_I2S_SUPPORT
#endif

// SPI parallel support (C3 - uses SPI quad mode with GDMA)
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define WLEDPB_PARALLEL_SPI_SUPPORT
#endif

// ESP_HAS_HIGH_GPIO_BANK — defined when the target has GPIO > 31 and needs the second GPIO output register bank (GPIO_OUT1_W1TS/TC_REG).
#if SOC_GPIO_PIN_COUNT > 32
#  define ESP_HAS_HIGH_GPIO_BANK 1
#endif

#ifdef WLEDPB_PARALLEL_SPI_SUPPORT
#include "soc/spi_struct.h"
#include "soc/gdma_struct.h"
#include "hal/gdma_ll.h"
#include "soc/gdma_reg.h"
#include "rom/lldesc.h"
#endif

#elif defined(ESP8266)
// nothing to do here
#else
#error "WLEDpixelBus only supports ESP32 and ESP8266 platforms"
#endif

#include "WLEDpixelBus_Timings.h"
#include "WLEDpixelBus_Features.h"
#include "../../colors.h"


namespace WLEDpixelBus {

// DMA Buffer Configuration
//==========================
constexpr size_t MIN_DMA_BUFFER_SIZE = 256;
constexpr size_t MAX_DMA_BUFFER_SIZE = 4092; // 12bit in DMA descriptor
#ifdef WLEDPB_PARALLEL_SPI_SUPPORT // C3 only
  constexpr size_t DEFAULT_DMA_BUFFER_SIZE = (1024*2); // must be a multiple of 16 (16 DMA bytes per source byte), clocked out at ~2.6MHz, 4 bits per clock (2k per buffer means about 1ms interrupt intervals with 4 step cadence)
#else
  #if SOC_RMT_TX_CANDIDATES_PER_GROUP > 4 // supports 8 RMT
  constexpr size_t DEFAULT_DMA_BUFFER_SIZE = MAX_DMA_BUFFER_SIZE; // note: 3k is enough except if using 8RMT (ESP32 classic), also requires triple buffering
  #else
  constexpr size_t DEFAULT_DMA_BUFFER_SIZE = (1024*3);
  #endif
#endif
static_assert(DEFAULT_DMA_BUFFER_SIZE <= 4092, "DEFAULT_DMA_BUFFER_SIZE exceeds the maximum possible size");

// packed into ColorEncoder::_pixelFormat: upper nibble = flags
static constexpr uint8_t NCHF_16BIT  = 0x10; // 16-bit per color (UCS8903/8904/SM16825)
static constexpr uint8_t NCHF_CUSTOM = 0x40; // custom channel map: _channelMap[] drives encoding
// 0x20 and 0x80 reserved

// Maximum number of color channels in custom channel map (ColorEncoder::_channelMap[]).
static constexpr uint8_t MAX_CUSTOM_CHANNELS = 6; // note: changing this wont propagate, currently ICs with more than 6 channels are not supported

static constexpr uint8_t MAX_PREFIX_BYTES = 8;
static constexpr uint8_t MAX_SUFFIX_BYTES = 8;
// Chip-specific prefix/suffix bytes
static constexpr uint8_t TM1814_PREFIX_LEN  = 8; // C1+C2 current-config prefix
static constexpr uint8_t TM1914_PREFIX_LEN  = 6; // mode-setting prefix
static constexpr uint8_t SM16825_SUFFIX_LEN = 4; // per-frame configuration suffix
// TM1914 mode-setting prefix: 6 bytes (3 data + 3 inverted)
//static constexpr uint8_t TM1914_PREFIX[6] = { 0xFF, 0xFF, 0xFA, 0x00, 0x00, 0x05 }; // DIN only
//static constexpr uint8_t TM1914_PREFIX[6] = { 0xFF, 0xFF, 0xF5, 0x00, 0x00, 0x0A }; // FDIN only
static constexpr uint8_t TM1914_PREFIX[6] = { 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00 }; // auto switch DIN / FDIN
// SM16825E 32-bit per-frame suffix (appended once after all pixel data):
//   bits 31..7  (25 bits): current gain OUT R,G,B,W,Y — 5 bits each, 0x1F = 310mA, 0x00 = 10.2mA, step ~10.1mA
//   bits  6..5  ( 2 bits): standby enable — 0b00 = normal op (0b10 = standby)
//   bits  4..0  ( 5 bits): reserved — all 1 recommended
//static constexpr uint8_t SM16825_SUFFIX[4] = { 0x08, 0x42, 0x10, 0x9F }; // 20.3mA for all channels as a safe default - TODO: make this configurable, could use the led current from settings
static constexpr uint8_t SM16825_SUFFIX[4] = { 0xFF, 0xFF, 0xFF, 0x9F }; // set max current (not really safe but always was the default)

// Logical channel count thresholds used by the ColorEncoder(uint8_t co, ...) constructor to decide which per-channel wiring rules apply
static constexpr uint8_t CHANNELS_RGBW = 4; // RGBW: W-swap applies, TM1814/TM1815 W-first quirk applies
static constexpr uint8_t CHANNELS_CCT  = 5; // RGB + WW + CW: WW/CW-swap applies

// White-channel swap selector, decoded from the upper nibble of WLED's color-order byte.
enum WSwap : uint8_t {
  WSWAP_NONE = 0,
  WSWAP_B    = 1, // swap W & B
  WSWAP_G    = 2, // swap W & G
  WSWAP_R    = 3, // swap W & R
  WSWAP_WWCW = 4, // swap WW & CW
};

// Custom channel map color source to select which color source feeds a given wire byte.
enum ChannelSource : uint8_t {
  CH_UNUSED = 0,
  CH_R      = 1,
  CH_G      = 2,
  CH_B      = 3,
  CH_W      = 4,
  CH_WW     = 5,
  CH_CW     = 6,
};

// Wire-byte counts per pixel for each pixel format
enum WireBytes : uint8_t {
  WB_RGB    = 3,
  WB_RGBW   = 4,
  WB_CCT    = 5,
  WB_RGB16  = 6,
  WB_RGBW16 = 8,
  WB_CCT16  = 10,
};

// Hardware properties of a one-wire digital bus. Note: clocked 2-pin LEDs have no custom channel support (no known hardware exists that uses that)
struct BusProperties {
  uint8_t  numChannels = 3; // logical channels; is16bit doubles wire bytes
  uint8_t  channelMap[MAX_CUSTOM_CHANNELS] = {2, 1, 3, 0, 0, 0};
  uint8_t  invertMask = 0;
  bool     is16bit = false;
  bool     invertOutput = false;
  LedTiming timing = LedTiming(300, 900, 700, 500, 300);
  uint8_t  prefixLen = 0;
  uint8_t  prefixData[MAX_PREFIX_BYTES] = {};
  uint8_t  suffixLen = 0;
  uint8_t  suffixData[MAX_SUFFIX_BYTES] = {};
};

enum class DriverState : uint8_t {
  Idle = 0,
  Sending = 1
};

using ::CctPixel;

// Pixel color encoder: maps RGBW uint32_t to a per-LED byte stream according to color order (fast) or custom color map (slow, generic)
class ColorEncoder {
public:
  ColorEncoder() : _pixelFormat(0x03), _invertMask(0), _idxR(0), _idxG(1), _idxB(2), _idxW(3), _idxCW(4) { memset(_channelMap, 0, sizeof(_channelMap)); }
  ColorEncoder(uint8_t co, uint8_t numChannels, uint8_t ledType = 0);
  ColorEncoder(const uint8_t channelMap[MAX_CUSTOM_CHANNELS], uint8_t numChannels, uint8_t invertMask, bool is16bit);

  // Fast encode — standard 8-bit
  inline void encodeRGB(uint32_t c, uint8_t* out) const {
    out[_idxR] = R(c); out[_idxG] = G(c); out[_idxB] = B(c);
  }
  inline void encodeRGBW(uint32_t c, uint8_t* out) const {
    out[_idxR] = R(c); out[_idxG] = G(c); out[_idxB] = B(c); out[_idxW] = W(c);
  }
  inline void encodeCCT(uint32_t c, const CctPixel& cct, uint8_t* out) const {
    out[_idxR] = R(c); out[_idxG] = G(c); out[_idxB] = B(c);
    out[_idxW] = cct.ww; out[_idxCW] = cct.cw;
  }
  // Fast decode — standard 8-bit
  inline uint32_t decodeRGB(const uint8_t* in) const {
    return RGBW32(in[_idxR], in[_idxG], in[_idxB], 0);
  }
  inline uint32_t decodeRGBW(const uint8_t* in) const {
    return RGBW32(in[_idxR], in[_idxG], in[_idxB], in[_idxW]);
  }
  inline uint32_t decodeCCT(const uint8_t* in) const {
    return RGBW32(in[_idxR], in[_idxG], in[_idxB], in[_idxW]); // WW→W, CW dropped (lossy)
  }

  // Fast encode — 16-bit. Note: only 16bit colors scale during encoding, for 8bit colors, scaling happens in BusDigital
  inline void encodeRGB16(uint32_t c, uint8_t* out, uint8_t bri) const {
    writeU16(out, _idxR, R(c), bri);
    writeU16(out, _idxG, G(c), bri);
    writeU16(out, _idxB, B(c), bri);
  }
  inline void encodeRGBW16(uint32_t c, uint8_t* out, uint8_t bri) const {
    writeU16(out, _idxR, R(c), bri);
    writeU16(out, _idxG, G(c), bri);
    writeU16(out, _idxB, B(c), bri);
    writeU16(out, _idxW, W(c), bri);
  }
  inline void encodeCCT16(uint32_t c, const CctPixel& cct, uint8_t* out, uint8_t bri) const {
    writeU16(out, _idxR,  R(c), bri);
    writeU16(out, _idxG,  G(c), bri);
    writeU16(out, _idxB,  B(c), bri);
    writeU16(out, _idxW,  cct.ww,  bri);
    writeU16(out, _idxCW, cct.cw,  bri);
  }

  // Fast decode — 16-bit
  inline uint32_t decodeRGB16(const uint8_t* in) const {
    return RGBW32(readU16Hi(in, _idxR), readU16Hi(in, _idxG), readU16Hi(in, _idxB), 0);
  }
  inline uint32_t decodeRGBW16(const uint8_t* in) const {
    return RGBW32(readU16Hi(in, _idxR), readU16Hi(in, _idxG), readU16Hi(in, _idxB), readU16Hi(in, _idxW));
  }
  inline uint32_t decodeCCT16(const uint8_t* in) const {
    return RGBW32(readU16Hi(in, _idxR), readU16Hi(in, _idxG), readU16Hi(in, _idxB), readU16Hi(in, _idxW));
  }

  // Generic slow encode/decode — handles custom and inverted color channels
  void     encodeGeneric(uint32_t c, const CctPixel& cct, uint8_t* out, uint8_t bri) const;
  uint32_t decodeGeneric(const uint8_t* in) const;

  // pixel color scaling functions, respect color inverted channels, split 8/16 bit for faster for-loops
  inline void scalePixel8Bit(uint8_t* pixel, uint8_t scale) const {
    const uint8_t logicalChannels = _pixelFormat & 0x0F;
    for (uint8_t i = 0; i < logicalChannels; ++i) {
      const uint8_t inverted = (uint8_t)(0u - ((_invertMask >> i) & 1u));
      uint8_t value = pixel[i] ^ inverted;
      value = (uint8_t)((((uint16_t)value) * scale) >> 8);
      pixel[i] = value ^ inverted;
    }
  }

  inline void scalePixel16Bit(uint8_t* pixel, uint8_t scale) const {
    const uint8_t logicalChannels = (_pixelFormat & 0x0F) / 2;
    for (uint8_t i = 0; i < logicalChannels; ++i) {
      uint8_t* channel = pixel + (size_t)i * 2;
      const uint16_t inverted = (uint16_t)(0u - ((_invertMask >> i) & 1u));
      uint16_t value = (((uint16_t)channel[0] << 8) | channel[1]) ^ inverted;
      value = (uint16_t)((((uint32_t)value) * scale) >> 8);
      value ^= inverted;
      channel[0] = value >> 8;
      channel[1] = value & 0xFF;
    }
  }

  void scalePixel(uint8_t* pixel, uint8_t scale) const {
    if (scale == 255) return;
    scale++; // add 1 for rounding using bitshift
    if (is16Bit()) scalePixel16Bit(pixel, scale); // TODO: we do not really need the functions inlined here but we do in scaleEncodeBuffer(), may need some refinement, also check ESP8266 DMA driver
    else scalePixel8Bit(pixel, scale);
  }

  // Accessors
  uint8_t getPixelFormat()      const { return _pixelFormat; }   // lower nibble=wire bytes, upper=NCHF_* flags
  uint8_t getLogicalChannels()  const { return (_pixelFormat & NCHF_16BIT) ? (_pixelFormat & 0x0F) / 2 : (_pixelFormat & 0x0F); } // logical color channels
  uint8_t getBytesPerPixel()    const { return _pixelFormat & 0x0F; } // wire bytes per pixel
  bool    is16Bit()             const { return (_pixelFormat & NCHF_16BIT) != 0; } // true for UCS8903/8904/SM16825
  bool    hasInvertedChannels() const { return _invertMask != 0; } // bus with at least one inverted color channel
  bool    isCustom()            const { return (_pixelFormat & NCHF_CUSTOM) != 0; } // true for non-permutation custom channel maps

private:
  uint8_t _pixelFormat; // lower nibble = bytes per pixel, upper nibble = NCHF_ flags (16-bit, custom)
  uint8_t _invertMask;  // bitmask: bit i = invert color channel i (applies when hasInvertedChannels())
  uint8_t _idxR, _idxG, _idxB;  // wire byte index for R, G, B
  uint8_t _idxW, _idxCW;        // wire byte index for W (or WW) and CW (CCT types)
  uint8_t _channelMap[MAX_CUSTOM_CHANNELS]; // custom channel map: _channelMap[i] = ChannelSource for wire byte i

  // 16bit pixel helper functions
  static inline void writeU16(uint8_t* out, uint8_t idx, uint8_t val8, uint8_t bri) {
    const uint16_t v = (uint16_t)val8 * bri; // Note: this can "only" reach 99% as 255×255 = 65025. We do not care.
    out[idx*2]   = v >> 8;
    out[idx*2+1] = v & 0xFF;
  }
  static inline uint8_t readU16Hi(const uint8_t* in, uint8_t idx) { return in[idx*2]; }
};


// PixelBus base class
//==============================================================================

class PixelBus {
protected:
  uint8_t* _encodeBuffer = nullptr;   // color data sent to the LEDs (includes prefix and suffix)
  uint8_t* _pixelData = nullptr;      // _encodeBuffer + _prefixLen, cached to avoid per-call addition
  size_t   _encodeBufferSize = 0;     // allocated size in bytes
  uint16_t _numPixels = 0;            // number of encoded pixels (bytes per pixel depends on _encoder)
  uint8_t  _prefixLen = 0;            // byte length of chip prefix at start of _encodeBuffer i.e. sent out first
  uint8_t  _suffixLen = 0;            // byte length of chip suffix appended after pixel data i.e. sent out after pixel data
  uint8_t  _prefixData[MAX_PREFIX_BYTES] = {};
  uint8_t  _suffixData[MAX_SUFFIX_BYTES] = {};
  uint8_t  _ledType   = 0;            // LED chip type (e.g. 31=TM1814); 0 = generic
  bool     _inverted  = false;        // physical output signal inversion (per bus), not to be confused by _invertMask which is per-color channel inversion
  uint8_t  _busBri = 255;             // brightness for color_fade() in setPixelColor() (software scaling, can be on top of hardware scaling)
  uint8_t  _encBri = 255;             // encoder brightness for 16-bit types: uses _encBri * colorchannel
  ColorEncoder _encoder;              // color encoder, set by derived class constructors

public:
  virtual ~PixelBus() { releaseEncodeBuffer(); }

  // color data prefix handling
  void setPrefixLen(uint8_t len) { _prefixLen = len > MAX_PREFIX_BYTES ? MAX_PREFIX_BYTES : len; }
  void setPrefixData(const uint8_t* data, uint8_t len) {
    if (!data) return;
    if (len > MAX_PREFIX_BYTES) len = MAX_PREFIX_BYTES;
    memcpy(_prefixData, data, len);
  }
  virtual void updatePrefix(const uint8_t* data, uint8_t len) {
    if (!_encodeBuffer || len == 0) return;
    if (len > _prefixLen) len = _prefixLen;
    memcpy(_encodeBuffer, data, len);
  }
  bool hasPrefix() const { return _prefixLen > 0; }
  uint8_t getPrefixLen() const { return _prefixLen; }
  // color data suffix handling
  void setSuffixLen(uint8_t len) { _suffixLen = len > MAX_SUFFIX_BYTES ? MAX_SUFFIX_BYTES : len; }
  void setSuffixData(const uint8_t* data, uint8_t len) {
    if (!data) return;
    if (len > MAX_SUFFIX_BYTES) len = MAX_SUFFIX_BYTES;
    memcpy(_suffixData, data, len);
  }
  virtual void updateSuffix(const uint8_t* data, uint8_t len) {
    if (!_pixelData || _suffixLen == 0 || len == 0) return;
    if (len > _suffixLen) len = _suffixLen;
    memcpy(_pixelData + (size_t)_numPixels * _encoder.getBytesPerPixel(), data, len);
  }
  bool hasSuffix() const { return _suffixLen > 0; }
  uint8_t getSuffixLen() const { return _suffixLen; }

  void setBusBri(uint8_t b) { _busBri = b; }
  void setEncBri(uint8_t b) { _encBri = b; }
  inline uint8_t getBusBri() const { return _busBri; }
  // Set APA102 5-bit per-pixel hardware brightness step (0–31), default implementation is a no-op; overridden by SpiBus for TYPE_APA102
  // coarse control via hardware step, fine via color_fade() - is set globally, same value is used for all pixels
  // Note: APA102 would effectively have 13bit per-color resolution we could leverage for smoother dimming
  virtual void setApa102HwBri(uint8_t /*v*/) {}

  virtual void setInverted(bool inv) { _inverted = inv; }
  bool isInverted() const { return _inverted; }
  void setEncoder(const ColorEncoder& enc) { _encoder = enc; } // must be called after construction but before begin().

  virtual bool begin() = 0;
  virtual void end() = 0;
  virtual bool show() = 0; // show() sends the _encodeBuffer to hardware
  virtual bool canShow() const = 0; // polled by busmanager, must never block and never return false permanently
  #ifdef WLED_DEBUG_BUS
  virtual const char* getTypeStr() const = 0;
  #endif

  // Encode one pixel into _encodeBuffer at pos, c and ww/cw are already brightness-scaled for 8bit pixels (done by BusDigital)
  // for 16-bit LED types the encoder multiplies each channel by _encBri for better resolution
  // Note: _encodeBuffer != nullptr and pos < _numPixels must be checked by caller (speed optimisation)
  // Note: using O2 optimization seems to make it slower
  // TODO: on ESP32, do not put this in IRAM on C3 it works in IRAM, need to test if there is any speed benefit from IRAM (note: this may have all changed in IDF V5)
  virtual bool setPixelColor(uint16_t pos, uint32_t c, uint16_t wwcw) {
    const uint8_t pixelFormat = _encoder.getPixelFormat();
    uint8_t* out = _pixelData + (size_t)pos * _encoder.getBytesPerPixel();
    const CctPixel cct{wwcw};
    switch (pixelFormat) {
      case 3:                    _encoder.encodeRGB(c, out);                   break; // 3ch RGB
      case 4:                    _encoder.encodeRGBW(c, out);                  break; // 4ch RGBW
      case 5:                    _encoder.encodeCCT(c, cct, out);              break; // 5ch CCT
      case (3*2) | NCHF_16BIT:   _encoder.encodeRGB16(c, out, _encBri);        break; // 16-bit RGB (6 bytes per pixel)
      case (4*2) | NCHF_16BIT:   _encoder.encodeRGBW16(c, out, _encBri);       break; // 16-bit RGBW (8bytes per pixel)
      case (5*2) | NCHF_16BIT:   _encoder.encodeCCT16(c, cct, out, _encBri);   break; // 16-bit CCT (10 bytes per pixel)
      default:                   _encoder.encodeGeneric(c, cct, out, _encBri); break; // inverted / special cases up to 6 channels per physical pixel
    }
    return true;
  }

  // Decode one pixel back from _encodeBuffer (lossy, used if no global buffer is available)
  // Note: buses with non-linear encoding (e.g. Esp8266DmaBus 4-step) must override this.
  virtual uint32_t getPixelColor(uint16_t pix) const {
    const uint8_t pixelFormat = _encoder.getPixelFormat();
    const uint8_t* in = _pixelData + (size_t)pix * _encoder.getBytesPerPixel();
    switch (pixelFormat) {
      case 3:                    return _encoder.decodeRGB(in);
      case 4:                    return _encoder.decodeRGBW(in);
      case 5:                    return _encoder.decodeCCT(in);
      case (3*2) | NCHF_16BIT:   return _encoder.decodeRGB16(in);
      case (4*2) | NCHF_16BIT:   return _encoder.decodeRGBW16(in);
      case (5*2) | NCHF_16BIT:   return _encoder.decodeCCT16(in);
      default:                   return _encoder.decodeGeneric(in);
    }
  }

  // Set every logical pixel to black, respecting color channel inversion
  virtual void clearEncodeBuffer() {
    if (!_pixelData || _numPixels == 0) return;
    const size_t bytesPerPixel = _encoder.getBytesPerPixel();
    const size_t pixelBytes = (size_t)_numPixels * bytesPerPixel;

    // fast path if no inverted channels: just set entire color buffer to 0
    if (!_encoder.hasInvertedChannels()) {
      memset(_pixelData, 0, pixelBytes);
      return;
    }
    // slow path if there is at least one inverted color channel: fill each pixel with black
    uint8_t blackPixel[MAX_CUSTOM_CHANNELS * 2] = {};
    _encoder.scalePixel(blackPixel, 0);

    // Mixed channel inversion needs a repeating per-pixel fill pattern.
    for (uint16_t i = 0; i < _numPixels; ++i) {
      memcpy(_pixelData + (size_t)i * bytesPerPixel, blackPixel, bytesPerPixel);
    }
  }

  // Proportionally scale all encoded colors, used by BusDigital::applyBriLimit() for ABL
  // Note: buses with non-linear encoding (e.g. Esp8266DmaBus 4-step) must override this.
  virtual void scaleEncodeBuffer(uint8_t scale) {
    if (scale == 255 || !_pixelData) return;
    scale++; // add 1 for rounding using bitshift
    const size_t bytesPerPixel = _encoder.getBytesPerPixel();
    if (!_encoder.is16Bit()) {
      // fast path: 8bit colors, no inversion: just scale each byte
      if (!_encoder.hasInvertedChannels()) {
        const size_t pixelBytes = (size_t)_numPixels * bytesPerPixel;
        for (size_t i = 0; i < pixelBytes; i++) {
          _pixelData[i] = ((uint16_t)(_pixelData[i]) * scale) >> 8;
        }
        return;
      }
      // slow path: respect inverted color channels
      for (uint16_t i = 0; i < _numPixels; ++i) {
        _encoder.scalePixel8Bit(_pixelData + (size_t)i * bytesPerPixel, scale); // TODO: could be optimised for speed
      }
    } else {
      // no fast path for 16bit, could be optimised for speed if needed
      for (uint16_t i = 0; i < _numPixels; ++i) {
        _encoder.scalePixel16Bit(_pixelData + (size_t)i * bytesPerPixel, scale); // TODO: could be optimised for speed
      }
    }
  }

  // Allocate encode buffer (data sent to the LEDs)
  virtual bool allocateEncodeBuffer(uint16_t numPixels, uint8_t bytesPerPixel) {
    const size_t pixelBytes = (size_t)numPixels * bytesPerPixel;
    const size_t needed = _prefixLen + pixelBytes + _suffixLen;
    if (_encodeBuffer && _encodeBufferSize >= needed) return true;
    releaseEncodeBuffer();
    if (needed == 0) return true;
    #if defined(ARDUINO_ARCH_ESP32)
    _encodeBuffer = (uint8_t*)heap_caps_malloc(needed, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    #else
    _encodeBuffer = (uint8_t*)malloc(needed);
    #endif
    if (!_encodeBuffer) return false;
    memset(_encodeBuffer, 0, needed);
    _encodeBufferSize = needed;
    _pixelData = _encodeBuffer + _prefixLen;
    if (_prefixLen > 0) memcpy(_encodeBuffer, _prefixData, _prefixLen);
    if (_suffixLen > 0) memcpy(_pixelData + pixelBytes, _suffixData, _suffixLen);
    return true;
  }

  void releaseEncodeBuffer() {
    if (_encodeBuffer) {
      #if defined(ARDUINO_ARCH_ESP32)
      heap_caps_free(_encodeBuffer);
      #else
      free(_encodeBuffer);
      #endif
    }
    _encodeBuffer = nullptr;
    _pixelData = nullptr;
    _encodeBufferSize = 0;
  }

  size_t   getEncodeBufferSize() const { return _encodeBufferSize; }
  virtual uint16_t getNumPixels() const { return _numPixels; }
  void setNumPixels(uint16_t n) { _numPixels = n; }
};

enum class BusDriver : uint8_t {
  RMT     = 0,
  I2S     = 1, // I2S on ESP32 and S2, LCD on S3
  SPI     = 2, // parallel SPI output (C3)
  UART    = 3,
  DMA     = 4,
  BitBang = 5,
  PARLIO  = 6  // PARLIO TX parallel output (C6, H2, C5, P4 — IDF >= 5.3)
};

PixelBus* createBus(BusDriver driver, int8_t pin, const BusProperties& properties, uint8_t ledType = 0, size_t numPixels = 0);

} // namespace WLEDpixelBus

#include "WLEDpixelBus_SPI.h" // clocked LEDs, supported on all platforms

#if defined(ARDUINO_ARCH_ESP32)
#include "WLEDpixelBus_RMT.h"
#include "WLEDpixelBus_I2S.h"
#include "WLEDpixelBus_ParallelSpi.h"
#include "WLEDpixelBus_PARLIO.h"
#include "WLEDpixelBus_BitBang.h"
#elif defined(ESP8266)
#include "WLEDpixelBus_ESP8266.h"
#include "WLEDpixelBus_BitBang.h"
#endif
