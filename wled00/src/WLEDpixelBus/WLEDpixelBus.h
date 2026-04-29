/*-------------------------------------------------------------------------
WLEDpixelBus - Lightweight LED driver library for WLED

by @dedehai, 2026

Features:
- Runtime LED timing configuration
- Double-buffered DMA with interrupt-driven refilling (4-step cadence)
- Support for ESP32, ESP32-S2, ESP32-S3, ESP32-C3
- RMT, I2S parallel, and LCD parallel output methods
- RGBW uint32_t pixel buffer format (WLED native)
- Separate CCT (WW/CW) buffer support
- IDF v4.x compatible

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
#include "driver/rmt.h"

// Platform detection
#if defined(CONFIG_IDF_TARGET_ESP32)
  #define WLEDPB_ESP32
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  #define WLEDPB_ESP32S2
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define WLEDPB_ESP32S3
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  #define WLEDPB_ESP32C3
#endif

// I2S support (ESP32 and S2 only for parallel mode)
#if defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2)
  #define WLEDPB_I2S_SUPPORT
#endif

// LCD support (S3 only)
#if defined(WLEDPB_ESP32S3)
  #define WLEDPB_LCD_SUPPORT
#endif

// SPI parallel support (C3 - uses SPI quad mode with GDMA)
#if defined(WLEDPB_ESP32C3)
  #define WLEDPB_PARALLEL_SPI_SUPPORT 
#endif

#ifdef WLEDPB_PARALLEL_SPI_SUPPORT
#include "soc/spi_struct.h"
#include "soc/gdma_struct.h"
#include "hal/gdma_ll.h"
#include "soc/gdma_reg.h"
#include "rom/lldesc.h"
#endif

#elif defined(ARDUINO_ARCH_ESP8266)

#define WLEDPB_ESP8266

#else
#error "WLEDpixelBus only supports ESP32 and ESP8266 platforms"
#endif

#include "WLEDpixelBus_Timings.h"
#include "WLEDpixelBus_Features.h"

namespace WLEDpixelBus {


//==============================================================================
// WLED Pixel Format - uint32_t RGBW
//==============================================================================

/**
 * Extract color components from WLED's uint32_t format
 * Format: 0xWWRRGGBB (W in high byte, B in low byte)
 */
inline uint8_t getR(uint32_t color) { return (color >> 16) & 0xFF; }
inline uint8_t getG(uint32_t color) { return (color >> 8) & 0xFF; }
inline uint8_t getB(uint32_t color) { return color & 0xFF; }
inline uint8_t getW(uint32_t color) { return (color >> 24) & 0xFF; }

/**
 * Create uint32_t color from components
 */
inline uint32_t makeColor(uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) {
  return ((uint32_t)w << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

//==============================================================================
// CCT (Warm White / Cool White) Support
//==============================================================================

/**
 * CCT data for a single pixel
 * Passed as separate buffer from main RGBW data
 */
 /*
struct CctPixel {
  uint8_t ww;  // Warm white
  uint8_t cw;  // Cool white
};*/
struct CctPixel {
  union {
      uint16_t wwcw; // Access as a 16-bit value (0xWWCW), default when setting 16-bit CCT types
      struct {
        uint8_t ww;  // Warm white
        uint8_t cw;  // Cool white
      };
  };
};

//==============================================================================
// Driver State
//==============================================================================

enum class DriverState : uint8_t {
  Idle = 0,
  Sending = 1,
  SendingLast = 2,   // Last data buffer was filled; wait for current buffer to finish so last-data buffer plays
  WaitingReset = 3   // Last data buffer played; zero buffer playing as reset signal
};

//==============================================================================
// DMA Buffer Configuration
//==============================================================================


constexpr size_t DEFAULT_DMA_BUFFER_SIZE = (1024*3);
constexpr size_t MIN_DMA_BUFFER_SIZE = 256;
constexpr size_t MAX_DMA_BUFFER_SIZE = 4092;

//==============================================================================
// Color Encoding Helpers
//==============================================================================

// Upper-nibble flags packed into ColorEncoder::_pixelFormat.
// Lower nibble = logical channel count.  Upper nibble = combination of these flags.
static constexpr uint8_t NCHF_16BIT  = 0x10; // 16-bit chip (UCS8903/8904/SM16825): wire bytes = logCh * 2
static constexpr uint8_t NCHF_INVERT = 0x20; // one or more channels polarity-inverted (_invertMask applies)
static constexpr uint8_t NCHF_CUSTOM = 0x40; // custom channel map (TYPE_CUSTOM_BUS): _channelMap[] drives encoding
// 0x80 reserved

/**
 * Pixel encoder: maps RGBW uint32_t to a per-LED byte stream according to color order.
 *
 * _pixelFormat packs two things:
 *   bits[3:0]  wire bytes per pixel (= logical channels for 8-bit; logical channels * 2 for 16-bit)
 *   bits[7:4]  NCHF_* flags (16BIT | INVERT | CUSTOM)
 *
 * setPixel / getPixelColor switch on _pixelFormat directly → single branch-free dispatch.
 *   0x03/04/05          fast 8-bit RGB / RGBW / CCT paths (no inversion)
 *   3|NCHF_16BIT etc.   fast 16-bit paths (no inversion)
 *   all other values    encodeGeneric / decodeGeneric (inverted, custom, special chips)
 *
 * getPixelBytes() returns wire bytes: logCh * 2 for 16-bit types, logCh otherwise.
 */
class ColorEncoder {
public:
  ColorEncoder() : _pixelFormat(0x03), _invertMask(0),
                   _idxR(0), _idxG(1), _idxB(2),
                   _idxW(3), _idxCW(4) { memset(_channelMap, 0, sizeof(_channelMap)); }
  ColorEncoder(uint8_t co, uint8_t numChannels, uint8_t ledType = 0);
  // Custom channel map constructor for TYPE_CUSTOM_BUS
  // channelMap[i]: 0=Unused, 1=R, 2=G, 3=B, 4=W, 5=WW, 6=CW
  ColorEncoder(const uint8_t channelMap[6], uint8_t numChannels, uint8_t invertMask, bool is16bit);

  // -------------------------------------------------------------------------
  // Fast encode — standard 8-bit types (no invert)
  // -------------------------------------------------------------------------

  inline void encodeRGB(uint32_t c, uint8_t* out) const {
    out[_idxR] = getR(c); out[_idxG] = getG(c); out[_idxB] = getB(c);
  }
  inline void encodeRGBW(uint32_t c, uint8_t* out) const {
    out[_idxR] = getR(c); out[_idxG] = getG(c); out[_idxB] = getB(c); out[_idxW] = getW(c);
  }
  inline void encodeCCT(uint32_t c, const CctPixel& cct, uint8_t* out) const {
    out[_idxR] = getR(c); out[_idxG] = getG(c); out[_idxB] = getB(c);
    out[_idxW] = cct.ww; out[_idxCW] = cct.cw;
  }

  // -------------------------------------------------------------------------
  // Fast encode — 16-bit types (UCS8903 / UCS8904 / SM16825)
  // -------------------------------------------------------------------------

  inline void encodeRGB16(uint32_t c, uint8_t* out, uint8_t bri) const {
    writeU16(out, _idxR, getR(c), bri);
    writeU16(out, _idxG, getG(c), bri);
    writeU16(out, _idxB, getB(c), bri);
  }
  inline void encodeRGBW16(uint32_t c, uint8_t* out, uint8_t bri) const {
    writeU16(out, _idxR, getR(c), bri);
    writeU16(out, _idxG, getG(c), bri);
    writeU16(out, _idxB, getB(c), bri);
    writeU16(out, _idxW, getW(c), bri);
  }
  inline void encodeCCT16(uint32_t c, const CctPixel& cct, uint8_t* out, uint8_t bri) const {
    writeU16(out, _idxR,  getR(c), bri);
    writeU16(out, _idxG,  getG(c), bri);
    writeU16(out, _idxB,  getB(c), bri);
    writeU16(out, _idxW,  cct.ww,  bri);
    writeU16(out, _idxCW, cct.cw,  bri);
  }

  // -------------------------------------------------------------------------
  // Fast decode — standard 8-bit types (no invert)
  // -------------------------------------------------------------------------

  inline uint32_t decodeRGB(const uint8_t* in) const {
    return makeColor(in[_idxR], in[_idxG], in[_idxB]);
  }
  inline uint32_t decodeRGBW(const uint8_t* in) const {
    return makeColor(in[_idxR], in[_idxG], in[_idxB], in[_idxW]);
  }
  inline uint32_t decodeCCT(const uint8_t* in) const {
    return makeColor(in[_idxR], in[_idxG], in[_idxB], in[_idxW]); // WW→W, CW dropped (lossy)
  }

  // -------------------------------------------------------------------------
  // Fast decode — 16-bit types
  // -------------------------------------------------------------------------

  inline uint32_t decodeRGB16(const uint8_t* in) const {
    return makeColor(readU16Hi(in, _idxR), readU16Hi(in, _idxG), readU16Hi(in, _idxB));
  }
  inline uint32_t decodeRGBW16(const uint8_t* in) const {
    return makeColor(readU16Hi(in, _idxR), readU16Hi(in, _idxG), readU16Hi(in, _idxB), readU16Hi(in, _idxW));
  }
  inline uint32_t decodeCCT16(const uint8_t* in) const {
    return makeColor(readU16Hi(in, _idxR), readU16Hi(in, _idxG), readU16Hi(in, _idxB), readU16Hi(in, _idxW));
  }

  // -------------------------------------------------------------------------
  // Generic slow encode/decode — handles NCHF_INVERT, NCHF_SPEC1, NCHF_SPEC2
  // (and any 16-bit + invert combination); defined in WLEDpixelBus.cpp
  // -------------------------------------------------------------------------

  void     encodeGeneric(uint32_t c, const CctPixel& cct, uint8_t* out, uint8_t bri) const;
  uint32_t decodeGeneric(const uint8_t* in) const;

  // Accessors
  uint8_t getPixelFormat()     const { return _pixelFormat; }   // packed dispatch key: lower nibble=wire bytes, upper=NCHF_*
  uint8_t getColorChannels()   const { return (_pixelFormat & NCHF_16BIT) ? (_pixelFormat & 0x0F) / 2 : (_pixelFormat & 0x0F); } // logical color channels
  uint8_t getPixelBytes()      const { return _pixelFormat & 0x0F; } // wire bytes per pixel (branch-free)
  bool    is16bit()            const { return (_pixelFormat & NCHF_16BIT) != 0; } // true for UCS8903/8904/SM16825

private:
  uint8_t _pixelFormat; // lower nibble = bytes per pixel, upper nibble = NCHF_ flags (invert, 16-bit, custom)
  uint8_t _invertMask;  // bitmask: bit i = invert color i (applies when NCHF_INVERT or NCHF_CUSTOM set)
  uint8_t _idxR, _idxG, _idxB;  // wire byte index for R, G, B
  uint8_t _idxW, _idxCW;       // wire byte index for W (or WW) and CW (CCT types)
  uint8_t _channelMap[6]; // custom channel map (TYPE_CUSTOM_BUS): _channelMap[i] = color source for wire byte i
                          // MUST remain last — keeps _idxR/_idxG/_idxB at same offsets as before, preserving IRAM code size

  // Helpers
  static inline void writeU16(uint8_t* out, uint8_t idx, uint8_t val8, uint8_t bri) {
    const uint16_t v = (uint16_t)val8 * bri;
    out[idx*2]   = v >> 8;
    out[idx*2+1] = v & 0xFF;
  }
  static inline uint8_t readU16Hi(const uint8_t* in, uint8_t idx) { return in[idx*2]; }
};

//==============================================================================
// Base Pixel Bus Interface
//==============================================================================

class PixelBus {
protected:
  uint8_t* _encodeBuffer = nullptr;   // encoded pixel data ready for hardware transmission
  size_t   _encodeBufferSize = 0;     // allocated size in bytes
  uint16_t _numPixels = 0;
  uint8_t  _prefixLen = 0;            // byte length of chip prefix at start of _encodeBuffer
  ColorEncoder _encoder;              // color encoder, set by derived class constructors
  uint8_t  _ledType   = 0;            // LED chip type (e.g. 31=TM1814); 0 = generic
  uint8_t* _pixelData = nullptr;      // _encodeBuffer + _prefixLen, cached to avoid per-call addition
  uint8_t  _suffixLen = 0;            // byte length of chip suffix appended after pixel data
  uint8_t  _busBri = 255;  // brightness for color_fade() in setPixelColor(): _bri for 8-bit types,
                           // fine residual for TM1814/TM1815, 255 (no-op) for 16-bit types
  uint8_t  _encBri = 255;  // encoder brightness for 16-bit types (SM16825/UCS8903/UCS8904):
                           // applied as channel*_encBri for full 16-bit wire precision; 255 for 8-bit

public:
  virtual ~PixelBus() {
    // Subclass end() should free _encodeBuffer with the correct allocator and
    // set it to nullptr before the base destructor runs. This is a safety net.
    if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; }
  }

  /**
   * Reserve prefix bytes before pixel data. Must be called BEFORE begin().
   * allocateEncodeBuffer() will zero the prefix region; updatePrefix() fills it per-frame.
   * @param len  Number of prefix bytes to reserve
   */
  void setPrefixLen(uint8_t len) {
    _prefixLen = len;
  }

  /**
   * Overwrite the prefix bytes in _encodeBuffer at runtime (e.g. per-frame for current control).
   * Must be called AFTER begin() (i.e. after allocateEncodeBuffer()). No-op if buffer not ready.
   * @param data  New prefix bytes
   * @param len   Must be <= _prefixLen (will be clamped)
   */
  void updatePrefix(const uint8_t* data, uint8_t len) {
    if (!_encodeBuffer || len == 0) return;
    if (len > _prefixLen) len = _prefixLen;
    memcpy(_encodeBuffer, data, len);
  }

  /**
   * Set bus-level brightness applied during pixel encoding for all LED types.
   * For 8-bit types: applied as video-scale fade on the full uint32_t pixel (hue-preserving).
   * For 16-bit types (SM16825, UCS8903, UCS8904): applied as `channel * bri` → full 16-bit value.
   * For TM1814/TM1815: set to the fine residual scale after hardware current-step selection.
   * @param b  brightness 0–255
   */
  void setBusBri(uint8_t b) { _busBri = b; } // TODO: brightness scaling/parameters may need some refinement or moving
  void setEncBri(uint8_t b) { _encBri = b; }
  inline uint8_t getBusBri() const { return _busBri; }

  /**
   * Set the APA102 5-bit per-pixel hardware brightness step (0–31).
   * Default implementation is a no-op; overridden by SpiBus for TYPE_APA102.
   * Called by BusDigital::setBrightness() as part of the two-stage brightness scheme:
   * coarse control via hardware current step, fine control via color_fade() residual.
   */
  virtual void setApa102HwBri(uint8_t /*v*/) {}

  /**
   * physical output signal inversion (polarity).
   * must be implemented on bus driver level
   */
  virtual void setInverted(bool /*inv*/) { }

  /**
   * Replace the color encoder (e.g. for TYPE_CUSTOM_BUS after bus creation).
   * Must be called after construction but before begin().
   */
  void setEncoder(const ColorEncoder& enc) { _encoder = enc; }

  bool hasPrefix() const { return _prefixLen > 0; }
  uint8_t getPrefixLen() const { return _prefixLen; }

  /**
   * Reserve suffix bytes after pixel data. Must be called BEFORE begin().
   * allocateEncodeBuffer() initialises the suffix region; updateSuffix() overwrites it.
   * @param len  Number of suffix bytes to reserve
   */
  void setSuffixLen(uint8_t len) {
    _suffixLen = len;
  }

  /**
   * Overwrite the suffix bytes in _encodeBuffer.
   * Must be called AFTER begin() (i.e. after allocateEncodeBuffer()). No-op if buffer not ready.
   * @param data  New suffix bytes
   * @param len   Must be <= _suffixLen (will be clamped)
   */
  virtual void updateSuffix(const uint8_t* data, uint8_t len) {
    if (!_pixelData || _suffixLen == 0 || len == 0) return;
    if (len > _suffixLen) len = _suffixLen;
    memcpy(_pixelData + (size_t)_numPixels * _encoder.getPixelBytes(), data, len);
  }

  bool hasSuffix() const { return _suffixLen > 0; }
  uint8_t getSuffixLen() const { return _suffixLen; }

  virtual bool begin() = 0;
  virtual void end() = 0;

  /**
   * Show pixels — sends the pre-encoded _encodeBuffer to hardware.
   * The pixel/numPixels/cct parameters are ignored; encoding is done
   * per-pixel by setPixel() before show() is called.
   */
  virtual bool show(const uint32_t* pixels = nullptr, uint16_t numPixels = 0,
            const CctPixel* cct = nullptr) = 0;

  virtual bool canShow() const = 0;
  virtual const char* getType() const = 0;

  /**
   * Encode one pixel into _encodeBuffer at pos.
   * c and ww/cw must already be brightness-scaled by the caller (BusDigital::setPixelColor
   * applies color_fade() via _busBri before calling here). For 16-bit LED types the encoder
   * multiplies each channel by _encBri for full 16-bit wire precision.
   * @param pos  pixel index (0-based, hardware index including skip)
   * @param c    RGBW color (brightness-scaled for 8-bit types; raw for 16-bit)
   * @param wwcw  warm-white/cool-white combined (CCT calculation result, brightness-scaled)
   */
  // Preconditions (guaranteed by BusDigital calling path):
  //   _encodeBuffer != nullptr  (_valid == true implies begin() succeeded)
  //   pos < _numPixels          (setNumPixels = lenToCreate + _skip, pix bounded by both)
  // note: using O2 optimization seems to make it slower
  // TODO: on ESP32, do not put this in IRAM on C3 it works in IRAM
  virtual bool setPixel(uint16_t pos, uint32_t c, uint16_t wwcw) {
    const uint8_t pixelFormat = _encoder.getPixelFormat();
    uint8_t* out = _pixelData + (size_t)pos * _encoder.getPixelBytes();
    const CctPixel cct{wwcw};
    switch (pixelFormat) {
      case 3:                    _encoder.encodeRGB(c, out);                   break; // 3ch RGB
      case 4:                    _encoder.encodeRGBW(c, out);                  break; // 4ch RGBW
      case 5:                    _encoder.encodeCCT(c, cct, out);              break; // 5ch CCT
      case (3*2) | NCHF_16BIT:   _encoder.encodeRGB16(c, out, _encBri);        break; // 16-bit RGB
      case (4*2) | NCHF_16BIT:   _encoder.encodeRGBW16(c, out, _encBri);       break; // 16-bit RGBW
      case (5*2) | NCHF_16BIT:   _encoder.encodeCCT16(c, cct, out, _encBri);   break; // 16-bit CCT
      default:                   _encoder.encodeGeneric(c, cct, out, _encBri); break; // inverted / special cases
    }
    return true;
  }

  /**
   * Decode one pixel back from _encodeBuffer (used for read-modify-write and getPixelColor).
   * Returns RGBW32 color. WW/CW are NOT encoded separately so CCT round-trips are lossy.
   * Override only if the bus uses non-linear encoding (e.g. Esp8266DmaBus 4-step).
   */
   // TODO: on ESP32, do not put this in IRAM
  virtual uint32_t getPixelColor(uint16_t pix) const {
    const uint8_t pixelFormat = _encoder.getPixelFormat();
    const uint8_t* in = _pixelData + (size_t)pix * _encoder.getPixelBytes();
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

  /**
   * Zero the encode buffer (set all pixels to black), preserving the prefix.
   * Derived classes may override if their encoding is non-trivial (e.g. I2S 4-step).
   * For all standard RGB/RGBW protocols, all-zero bytes encode as black.
   */
  virtual void clearEncodeBuffer() {
    if (_pixelData && _numPixels > 0) {
      const size_t pixelBytes = (size_t)_numPixels * _encoder.getPixelBytes();
      memset(_pixelData, 0, pixelBytes);
    }
  }

  /**
   * Proportionally scale all encoded channel bytes by scale/256 (video scale).
   * Used by BusDigital::applyBriLimit() for ABL. No-op if scale == 255.
   * Note: buses with non-linear encoding (e.g. Esp8266DmaBus 4-step) must override this.
   */
  virtual void scaleAll(uint8_t scale) {
    if (scale == 255 || !_pixelData || _numPixels == 0) return;
    const size_t pixelBytes = (size_t)_numPixels * _encoder.getPixelBytes();
    for (size_t i = 0; i < pixelBytes; i++) {
      _pixelData[i] = ((uint16_t)(_pixelData[i] + 1) * scale) >> 8;
    }
  }

  /**
   * Allocate encode buffer. Called from begin() after hardware init.
   * Default uses plain malloc; DMA buses override to use heap_caps_malloc.
   * @param numPixels  hardware pixel count (may include skipped pixels)
   * @param numChannels bytes per pixel in the encoded stream
   */
  virtual bool allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) {
    const size_t pixelBytes = (size_t)numPixels * numChannels;
    size_t needed = _prefixLen + pixelBytes + _suffixLen;
    if (_encodeBuffer && _encodeBufferSize >= needed) return true;
    if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; }
    if (needed == 0) return true;
    _encodeBuffer = (uint8_t*)malloc(needed);
    if (!_encodeBuffer) { _encodeBufferSize = 0; return false; }
    memset(_encodeBuffer, 0, needed);
    _encodeBufferSize = needed;
    _pixelData = _encodeBuffer + _prefixLen;
    if (_suffixLen == sizeof(SM16825_SUFFIX) && _ledType == TYPE_SM16825)
      memcpy(_pixelData + pixelBytes, SM16825_SUFFIX, sizeof(SM16825_SUFFIX));
    return true;
  }

  size_t   getEncodeBufferSize() const { return _encodeBufferSize; }
  virtual uint16_t getNumPixels() const { return _numPixels; }
  void setNumPixels(uint16_t n) { _numPixels = n; }
};

//==============================================================================
// Forward Declarations
//==============================================================================

class RmtBus;

#ifdef WLEDPB_I2S_SUPPORT
class I2sBus;
class I2sBusContext;
#endif

#ifdef WLEDPB_LCD_SUPPORT
class LcdBus;
class LcdBusContext;
#endif

//==============================================================================
// Bus Factory - Create appropriate bus for platform
//==============================================================================

enum class BusDriver : uint8_t {
  RMT = 0,
  I2S = 1,
  LCD = 2,
  SPI = 3,
  UART = 4,
  DMA = 5,
  BitBang = 6,
  Auto = 255
};

/**
 * Get the maximum number of RMT TX channels for the current platform
 */
constexpr uint8_t getRmtMaxChannels() {
#if defined(WLEDPB_ESP32)
  return 8;   // ESP32 original: 8 RMT channels
#elif defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32S3)
  return 4;   // ESP32-S2/S3: 4 RMT TX channels
#elif defined(WLEDPB_ESP32C3)
  return 2;   // ESP32-C3: 2 RMT TX channels
#else
  return 0;
#endif
}

/**
 * Create a bus instance
 * @param type Bus type (Auto will select best for platform)
 * @param pin GPIO pin
 * @param timing LED timing
 * @param order Color order
 * @param bufferSize DMA buffer size (for I2S/LCD)
 * @param channel RMT channel to use (-1 for auto-allocate)
 * @return Bus instance (caller owns, delete when done)
 */
PixelBus* createBus(BusDriver type, int8_t pin, const LedTiming& timing,
  uint8_t colorOrder, uint8_t numChannels, size_t bufferSize = DEFAULT_DMA_BUFFER_SIZE,
  int8_t channel = -1, uint8_t ledType = 0);

/**
 * Get recommended bus type for current platform
 */
BusDriver getRecommendedBusDriver();

} // namespace WLEDpixelBus

#include "WLEDpixelBus_SPI.h"

#if defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32S3) || defined(WLEDPB_ESP32C3)
#include "WLEDpixelBus_RMT.h"
#include "WLEDpixelBus_I2S.h"
#include "WLEDpixelBus_LCD.h"
#include "WLEDpixelBus_ParallelSpi.h"
#elif defined(WLEDPB_ESP8266)
#include "WLEDpixelBus_ESP8266.h"
#endif

