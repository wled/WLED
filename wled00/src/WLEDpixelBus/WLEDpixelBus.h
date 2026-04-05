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
// LED Timing Configuration - Runtime configurable
//==============================================================================

/**
 * LED timing parameters in nanoseconds
 */


//==============================================================================
// Color Order Configuration
//==============================================================================

enum class ColorOrder :  uint8_t {
  // RGB variants (3 bytes)
  GRB = 0,
  RGB = 1,
  BRG = 2,
  RBG = 3,
  BGR = 4,
  GBR = 5,
  // RGBW variants (4 bytes)
  GRBW = 10,
  RGBW = 11,
  BRGW = 12,
  RBGW = 13,
  BGRW = 14,
  GBRW = 15,
  WRGB = 16,
  WGRB = 17,
  WBRG = 18,
  WRBG = 19,
  WGBR = 20,
  WBGR = 21,
  // RGBCWCW variants (5 bytes)
  GRBWC = 30,
  RGBWC = 31,
  BRGWC = 32,
  RBGWC = 33,
  BGRWC = 34,
  GBRWC = 35,
};

/**
 * Get byte count per pixel for a color order
 */
inline constexpr uint8_t getChannelCount(ColorOrder order) {
  return (static_cast<uint8_t>(order) >= 30) ? 5 : ((static_cast<uint8_t>(order) >= 10) ? 4 : 3);
}

/**
 * Check if color order includes white channel
 */
inline constexpr bool hasWhiteChannel(ColorOrder order) {
  return static_cast<uint8_t>(order) >= 10;
}

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
struct CctPixel {
  uint8_t ww;  // Warm white
  uint8_t cw;  // Cool white
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

/**
 * Pixel encoder: maps RGBW uint32 to per-LED byte stream according to color order.
 * Three specialized encode/decode functions have zero conditionals; dispatch is done
 * once per call in the caller based on numChannels.
 */
class ColorEncoder {
public:
  ColorEncoder() : _numChannels(3), _logCh(3), _idxR(0), _idxG(1), _idxB(2), _idxW(0xFF), _idxWW(0xFF), _idxCW(0xFF) {}
  ColorEncoder(uint8_t co, uint8_t numChannels, uint8_t ledType = 0);

  // --- Branch-free encode functions (dispatch on numChannels before calling) ---
  inline void encodeRGB(uint32_t pixel, uint8_t* out) const {
    out[_idxR] = getR(pixel);
    out[_idxG] = getG(pixel);
    out[_idxB] = getB(pixel);
  }
  inline void encodeRGBW(uint32_t pixel, uint8_t* out) const {
    out[_idxR] = getR(pixel);
    out[_idxG] = getG(pixel);
    out[_idxB] = getB(pixel);
    out[_idxW] = getW(pixel);
  }
  inline void encodeCCT(uint32_t pixel, const CctPixel& cct, uint8_t* out) const {
    out[_idxR]  = getR(pixel);
    out[_idxG]  = getG(pixel);
    out[_idxB]  = getB(pixel);
    out[_idxWW] = cct.ww;
    out[_idxCW] = cct.cw;
  }

  // --- Branch-free decode functions (dispatch on numChannels before calling) ---
  inline uint32_t decodeRGB(const uint8_t* in) const {
    return makeColor(in[_idxR], in[_idxG], in[_idxB]);
  }
  inline uint32_t decodeRGBW(const uint8_t* in) const {
    return makeColor(in[_idxR], in[_idxG], in[_idxB], in[_idxW]);
  }
  inline uint32_t decodeCCT(const uint8_t* in) const {
    return makeColor(in[_idxR], in[_idxG], in[_idxB], in[_idxWW]);
  }

  // --- 16-bit encode/decode (UCS8903/UCS8904/SM16825 — numChannels=6/8/10) ---
  // Wire format: (value, 0) — value in high byte, zero in low byte (NeoPixelBus convention).
  inline void encodeRGB16(uint32_t pixel, uint8_t* out) const {
    out[_idxR*2] = getR(pixel); out[_idxR*2+1] = 0;
    out[_idxG*2] = getG(pixel); out[_idxG*2+1] = 0;
    out[_idxB*2] = getB(pixel); out[_idxB*2+1] = 0;
  }
  inline void encodeRGBW16(uint32_t pixel, uint8_t* out) const {
    out[_idxR*2] = getR(pixel); out[_idxR*2+1] = 0;
    out[_idxG*2] = getG(pixel); out[_idxG*2+1] = 0;
    out[_idxB*2] = getB(pixel); out[_idxB*2+1] = 0;
    out[_idxW*2] = getW(pixel); out[_idxW*2+1] = 0;
  }
  inline void encodeCCT16(uint32_t pixel, const CctPixel& cct, uint8_t* out) const {
    out[_idxR*2]  = getR(pixel); out[_idxR*2+1]  = 0;
    out[_idxG*2]  = getG(pixel); out[_idxG*2+1]  = 0;
    out[_idxB*2]  = getB(pixel); out[_idxB*2+1]  = 0;
    out[_idxWW*2] = cct.ww;      out[_idxWW*2+1] = 0;
    out[_idxCW*2] = cct.cw;      out[_idxCW*2+1] = 0;
  }
  inline uint32_t decodeRGB16(const uint8_t* in) const {
    return makeColor(in[_idxR*2], in[_idxG*2], in[_idxB*2]);
  }
  inline uint32_t decodeRGBW16(const uint8_t* in) const {
    return makeColor(in[_idxR*2], in[_idxG*2], in[_idxB*2], in[_idxW*2]);
  }
  inline uint32_t decodeCCT16(const uint8_t* in) const {
    return makeColor(in[_idxR*2], in[_idxG*2], in[_idxB*2], in[_idxWW*2]);
  }

  uint8_t getNumChannels() const { return _numChannels; }
  uint8_t getLogicalChannels() const { return _logCh; }

private:
  uint8_t _numChannels;
  uint8_t _logCh;    // logical channel count (before 16-bit doubling)
  uint8_t _idxR;
  uint8_t _idxG;
  uint8_t _idxB;
  uint8_t _idxW;
  uint8_t _idxWW;
  uint8_t _idxCW;
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
   * physical output signal inversion (polarity).
   * must be implemented on bus driver level
   */
  virtual void setInverted(bool /*inv*/) { }

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
    memcpy(_pixelData + (size_t)_numPixels * _encoder.getNumChannels(), data, len);
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
   * Encode one pre-processed pixel into _encodeBuffer at position pos.
   * Dispatches to branch-free encodeRGB/encodeRGBW/encodeCCT based on channel count.
   * Override only if the bus uses non-linear encoding (e.g. Esp8266DmaBus 4-step).
   * @param pos  pixel index (0-based, hardware index including skip)
   * @param c    RGBW color, already brightness-scaled by BusDigital
   * @param ww   warm-white value (from CCT calculation), 0 for non-CCT buses
   * @param cw   cool-white value (from CCT calculation), 0 for non-CCT buses
   * @return false if pos out of range or buffer not yet allocated
   */
  // Preconditions (guaranteed by BusDigital calling path):
  //   _encodeBuffer != nullptr  (_valid == true implies begin() succeeded)
  //   pos < _numPixels          (setNumPixels = lenToCreate + _skip, pix bounded by both)
  virtual IRAM_ATTR bool setPixel(uint16_t pos, uint32_t c, uint8_t ww, uint8_t cw) {
    const uint8_t nch = _encoder.getNumChannels();
    uint8_t* out = _pixelData + (size_t)pos * nch;
    switch (nch) {
      case 3:  _encoder.encodeRGB(c, out);            break;
      case 4:  _encoder.encodeRGBW(c, out);           break;
      case 6:  _encoder.encodeRGB16(c, out);          break;
      case 8:  _encoder.encodeRGBW16(c, out);         break;
      case 10: { CctPixel cct{ww, cw}; _encoder.encodeCCT16(c, cct, out); break; }
      default: { CctPixel cct{ww, cw}; _encoder.encodeCCT(c, cct, out);   break; }
    }
    return true;
  }

  /**
   * Decode one pixel back from _encodeBuffer (used for read-modify-write and getPixelColor).
   * Returns RGBW32 color. WW/CW are NOT encoded separately so CCT round-trips are lossy.
   * Override only if the bus uses non-linear encoding (e.g. Esp8266DmaBus 4-step).
   */
  virtual IRAM_ATTR uint32_t getPixelColor(uint16_t pix) const {
    const uint8_t nch = _encoder.getNumChannels();
    const uint8_t* in = _pixelData + (size_t)pix * nch;
    switch (nch) {
      case 3:  return _encoder.decodeRGB(in);
      case 4:  return _encoder.decodeRGBW(in);
      case 6:  return _encoder.decodeRGB16(in);
      case 8:  return _encoder.decodeRGBW16(in);
      case 10: return _encoder.decodeCCT16(in);
      default: return _encoder.decodeCCT(in);
    }
  }

  /**
   * Zero the encode buffer (set all pixels to black), preserving the prefix.
   * Derived classes may override if their encoding is non-trivial (e.g. I2S 4-step).
   * For all standard RGB/RGBW protocols, all-zero bytes encode as black.
   */
  virtual void clearEncodeBuffer() {
    if (_pixelData && _numPixels > 0) {
      const size_t pixelBytes = (size_t)_numPixels * _encoder.getNumChannels();
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
    const size_t pixelBytes = (size_t)_numPixels * _encoder.getNumChannels();
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
    if (_suffixLen == sizeof(SM16825_SUFFIX) && _ledType == WLEDPB_TYPE_SM16825)
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

