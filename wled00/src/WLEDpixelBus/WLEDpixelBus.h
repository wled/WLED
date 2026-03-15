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
  #define WLEDPB_SPI_SUPPORT
#endif

#ifdef WLEDPB_SPI_SUPPORT
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
#include "WLEDpixelBus_SPI.h"

#if defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32S3) || defined(WLEDPB_ESP32C3)
#include "WLEDpixelBus_RMT.h"
#include "WLEDpixelBus_I2S.h"
#include "WLEDpixelBus_LCD.h"
#include "WLEDpixelBus_ParallelSpi.h"
#elif defined(WLEDPB_ESP8266)
#include "WLEDpixelBus_ESP8266.h"
#endif

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

constexpr size_t DEFAULT_DMA_BUFFER_SIZE = 2048;
constexpr size_t MIN_DMA_BUFFER_SIZE = 256;
constexpr size_t MAX_DMA_BUFFER_SIZE = 4092;

//==============================================================================
// Base Bus Interface
//==============================================================================

class IBus {
protected:
  uint32_t* _pixelData = nullptr;
  CctPixel* _cctData = nullptr;
  uint16_t _numPixels = 0;
  uint8_t _brightness = 255;

public:
  virtual ~IBus() {
    if (_pixelData) free(_pixelData);
    if (_cctData) free(_cctData);
  }

  virtual bool begin() = 0;
  virtual void end() = 0;

  /**
   * Show pixels
   * @param pixels RGBW pixel data (uint32_t per pixel, WLED format)
   * @param numPixels Number of pixels
   * @param cct Optional CCT data (2 bytes per pixel:  WW, CW)
   * @return true if transmission started successfully
   */
  virtual bool show(const uint32_t* pixels = nullptr, uint16_t numPixels = 0,
            const CctPixel* cct = nullptr) = 0;

  virtual bool canShow() const = 0;
  virtual void waitComplete() = 0;
  virtual const char* getType() const = 0;

  virtual bool allocatePixelBuffer(uint16_t numPixels, bool hasCCT = false) {
    if (_pixelData) {
      if (_numPixels == numPixels) return true;
      free(_pixelData);
      _pixelData = nullptr;
    }
    if (_cctData) {
      free(_cctData);
      _cctData = nullptr;
    }
    _numPixels = numPixels;
    if (numPixels == 0) return true;
    // TODO: use WLED alloc functions that are min-heap safe
    _pixelData = (uint32_t*)malloc(numPixels * sizeof(uint32_t));
    if (!_pixelData) return false;
    memset(_pixelData, 0, numPixels * sizeof(uint32_t));

    if (hasCCT) {
      _cctData = (CctPixel*)malloc(numPixels * sizeof(CctPixel));
      if (!_cctData) {
        free(_pixelData);
        _pixelData = nullptr;
        return false;
      }
      if (_cctData) memset(_cctData, 0, numPixels * sizeof(CctPixel));
    }

    return true;
  }

  // TODO: can be removed, pixels are now written to the buffer directly in busDigital::setPixelColor()
  //  virtual void IRAM_ATTR setPixelColor(uint16_t pix, uint32_t c, const CctPixel* cp = nullptr) {
  //      if (pix >= _numPixels) return; // TODO: can this be removed safely? busmanager already checks? its in the hot path.
  //      _pixelData[pix] = c;
  //      if (cp && _cctData) _cctData[pix] = *cp; // TODO: make set cct a seperate function? might speed things up by removing this check
  //  }

  virtual uint32_t getPixelColor(uint16_t pix) const {
    if (pix >= _numPixels) return 0;
    return _pixelData[pix];
  }
  
  virtual uint16_t getNumPixels() const {
    return _numPixels;
  }

  virtual void setBrightness(uint8_t b) {
    _brightness = b;
  }
  
  virtual uint32_t* getPixelData() { return _pixelData; }
  virtual CctPixel* getCctData() { return _cctData; }
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
IBus* createBus(BusType type, int8_t pin, const LedTiming& timing, 
        ColorOrder order, size_t bufferSize = DEFAULT_DMA_BUFFER_SIZE,
        int8_t channel = -1);

/**
 * Get recommended bus type for current platform
 */
BusType getRecommendedBusType();

/**
 * Estimate exact memory footprint of a bus.
 * Accounts for encode buffers, driver overhead, and shared DMA contexts.
 */
 // TODO: check if this is accurate and busmanager does not double-account for things calculated here
static inline size_t estimateMemory(BusType type, uint16_t numPixels, uint8_t channelCount, size_t dmaBufferSize = DEFAULT_DMA_BUFFER_SIZE) {
  size_t mem = 0;

  // Bus instance overhead
  mem += 128; // Approximate C++ object size + IBus data structures
  // TODO: check if this is correct
  switch (type) {
    case BusType::UART:
      // UART encode buffer: 4 encoded bytes per LED byte (no DMA)
      mem += numPixels * channelCount * 4;
      break;

    case BusType::BitBang:
      mem += numPixels * channelCount;  // single buffer
      break;

    case BusType::DMA:
      // I2S DMA on ESP8266: same 4-step encoding as UART but written via DMA
      mem += numPixels * channelCount * 4;
      break;

    default:
      // Encode buffer (RMT, I2S, LCD, SPI, etc.)
      mem += numPixels * channelCount;

      // DMA buffers and descriptors for I2S/LCD
      // They are double-buffered and mapped via heap_caps_malloc
      if (type == BusType::I2S || type == BusType::LCD || type == BusType::Auto) {
        mem += dmaBufferSize * 2;
        // Include minimal descriptor memory estimates (~64 bytes per context)
        mem += 64;
      }
      break;
  }

  return mem;
}

} // namespace WLEDpixelBus

