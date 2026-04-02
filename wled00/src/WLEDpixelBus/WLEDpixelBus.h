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
// Base Pixel Bus Interface
//==============================================================================

class PixelBus {
protected:
  uint8_t* _encodeBuffer = nullptr;   // encoded pixel data ready for hardware transmission
  size_t   _encodeBufferSize = 0;     // allocated size in bytes
  uint16_t _numPixels = 0;

public:
  virtual ~PixelBus() {
    // Subclass end() should free _encodeBuffer with the correct allocator and
    // set it to nullptr before the base destructor runs. This is a safety net.
    if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; }
  }

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
   * @param pos  pixel index (0-based, hardware index including skip)
   * @param c    RGBW color, already brightness-scaled by BusDigital
   * @param ww   warm-white value (from CCT calculation), 0 for non-CCT buses
   * @param cw   cool-white value (from CCT calculation), 0 for non-CCT buses
   * @return false if pos out of range or buffer not yet allocated
   */
  virtual bool setPixel(uint16_t pos, uint32_t c, uint8_t ww, uint8_t cw) = 0;

  /**
   * Decode one pixel back from _encodeBuffer (used for read-modify-write and getPixelColor).
   * Returns RGBW32 color. WW/CW are NOT encoded separately so CCT round-trips are lossy.
   */
  virtual uint32_t getPixelColor(uint16_t pix) const = 0;

  /**
   * Zero the encode buffer (set all pixels to black).
   * Derived classes may override if their encoding is non-trivial (e.g. I2S 4-step).
   * For all standard RGB/RGBW protocols, all-zero bytes encode as black.
   */
  virtual void clearEncodeBuffer() {
    if (_encodeBuffer && _encodeBufferSize > 0) memset(_encodeBuffer, 0, _encodeBufferSize);
  }

  /**
   * Proportionally scale all encoded channel bytes by scale/256 (video scale).
   * Used by BusDigital::applyBriLimit() for ABL. No-op if scale == 255.
   * Note: buses with non-linear encoding (e.g. Esp8266DmaBus 4-step) must override this.
   */
  virtual void scaleAll(uint8_t scale) {
    if (scale == 255 || !_encodeBuffer || _encodeBufferSize == 0) return;
    for (size_t i = 0; i < _encodeBufferSize; i++) {
      _encodeBuffer[i] = ((uint16_t)(_encodeBuffer[i] + 1) * scale) >> 8;
    }
  }

  /**
   * Allocate encode buffer. Called from begin() after hardware init.
   * Default uses plain malloc; DMA buses override to use heap_caps_malloc.
   * @param numPixels  hardware pixel count (may include skipped pixels)
   * @param numChannels bytes per pixel in the encoded stream
   */
  virtual bool allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) {
    size_t needed = (size_t)numPixels * numChannels;
    if (_encodeBuffer && _encodeBufferSize >= needed) return true;
    if (_encodeBuffer) { free(_encodeBuffer); _encodeBuffer = nullptr; }
    if (needed == 0) return true;
    _encodeBuffer = (uint8_t*)malloc(needed);
    if (!_encodeBuffer) { _encodeBufferSize = 0; return false; }
    memset(_encodeBuffer, 0, needed);
    _encodeBufferSize = needed;
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
// Color Encoding Helpers
//==============================================================================

/**
 * Encode pixel to byte stream according to color order
 */
class ColorEncoder {
public:
  ColorEncoder(ColorOrder order);

  /**
   * Encode a single pixel to output buffer
   * @param pixel RGBW color value
   * @param cct Optional CCT data (for CCT strips, replaces W channel)
   * @param out Output buffer (must have space for numChannels bytes)
   */
  inline void encode(uint32_t pixel, const CctPixel* cct, uint8_t* out) const;

  /**
   * Decode a previously encoded pixel back to RGBW uint32.
   * Lossy for CCT buses (WW/CW are not individually recoverable).
   * @param in  pointer to encoded bytes for one pixel (getNumChannels() bytes)
   */
  inline uint32_t decode(const uint8_t* in) const;

  uint8_t getNumChannels() const { return _numChannels; }
  ColorOrder getOrder() const { return _order; }

private:
  ColorOrder _order;
  uint8_t _numChannels;
  uint8_t _idxR;
  uint8_t _idxG;
  uint8_t _idxB;
  uint8_t _idxW;
  uint8_t _idxWW;
  uint8_t _idxCW;
};

inline void ColorEncoder::encode(uint32_t pixel, const CctPixel* cct, uint8_t* out) const {
  uint8_t r = getR(pixel);
  uint8_t g = getG(pixel);
  uint8_t b = getB(pixel);
  uint8_t w = getW(pixel);

  out[_idxR] = r;
  out[_idxG] = g;
  out[_idxB] = b;
  if (_idxW != 0xFF) out[_idxW] = w;

  if (_idxWW != 0xFF) {
    out[_idxWW] = cct ? cct->ww : w; // TODO: handle ww/cw more clever, there is not really a need to have this explicit, could save an if for RGB case
    out[_idxCW] = cct ? cct->cw : 0;
  }
}

/**
 * Decode a previously encoded pixel back to RGBW uint32. Lossy for CCT (WW/CW not recovered).
 * @param in  pointer to encoded bytes for one pixel (must be getNumChannels() bytes)
 */
inline uint32_t ColorEncoder::decode(const uint8_t* in) const {
  uint8_t r = (_idxR != 0xFF) ? in[_idxR] : 0;
  uint8_t g = (_idxG != 0xFF) ? in[_idxG] : 0;
  uint8_t b = (_idxB != 0xFF) ? in[_idxB] : 0;
  // W: prefer W channel; for CCT buses use WW channel as the W approximation
  uint8_t w = 0;
  if (_idxW  != 0xFF) w = in[_idxW];
  else if (_idxWW != 0xFF) w = in[_idxWW];
  return makeColor(r, g, b, w);
}


//==============================================================================
// Bus Factory - Create appropriate bus for platform
//==============================================================================

enum class BusType :  uint8_t {
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
PixelBus* createBus(BusType type, int8_t pin, const LedTiming& timing, 
        ColorOrder order, size_t bufferSize = DEFAULT_DMA_BUFFER_SIZE,
        int8_t channel = -1);

/**
 * Get recommended bus type for current platform
 */
BusType getRecommendedBusType();

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

