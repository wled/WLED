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

#if ! defined(ARDUINO_ARCH_ESP32)
#error "WLEDpixelBus only supports ESP32 platforms"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "driver/gpio.h"
#include "esp_idf_version.h"

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

namespace WLEDpixelBus {

//==============================================================================
// LED Timing Configuration - Runtime configurable
//==============================================================================

/**
 * LED timing parameters in nanoseconds
 */
struct LedTiming {
    uint16_t t0h_ns;    // '0' bit high time
    uint16_t t0l_ns;    // '0' bit low time
    uint16_t t1h_ns;    // '1' bit high time
    uint16_t t1l_ns;    // '1' bit low time
    uint32_t reset_us;  // Reset/latch time in microseconds

    constexpr LedTiming(uint16_t t0h, uint16_t t0l, uint16_t t1h, uint16_t t1l, uint32_t reset)
        : t0h_ns(t0h), t0l_ns(t0l), t1h_ns(t1h), t1l_ns(t1l), reset_us(reset) {}

    // Calculate bit period in ns
    constexpr uint32_t bitPeriod() const { 
        return (t0h_ns + t0l_ns + t1h_ns + t1l_ns) / 2; 
    }
};

// Predefined timing constants
namespace Timing {
    constexpr LedTiming WS2812  {200, 750, 500, 450, 100};
    constexpr LedTiming WS2811  {300, 950, 900, 350, 300};
    constexpr LedTiming WS2813  {400, 850, 800, 450, 300};
    constexpr LedTiming WS2815  {400, 850, 800, 450, 300};
    constexpr LedTiming WS2805  {300, 790, 790, 300, 300};
    constexpr LedTiming SK6812  {400, 850, 800, 450, 80};
    constexpr LedTiming TM1814  {360, 890, 720, 530, 200};
    constexpr LedTiming TM1829  {300, 900, 800, 400, 200};
    constexpr LedTiming APA106  {350, 1350, 1350, 350, 50};
    constexpr LedTiming UCS1903 {400, 850, 800, 450, 500};
    constexpr LedTiming SM16703 {300, 900, 900, 300, 80};
    constexpr LedTiming Generic800Kbps {400, 850, 800, 450, 300};
    constexpr LedTiming Generic400Kbps {800, 1700, 1600, 900, 300};
}

//==============================================================================
// Color Order Configuration
//==============================================================================

enum class ColorOrder :  uint8_t {
    // RGB variants (3 bytes)
    GRB = 0,
    RGB = 1,
    BRG = 2,
    RBG = 3,
    GBR = 4,
    BGR = 5,
    // RGBW variants (4 bytes)
    GRBW = 10,
    RGBW = 11,
    BRGW = 12,
    RBGW = 13,
    GBRW = 14,
    BGRW = 15,
    WRGB = 16,
    WGRB = 17,
    WBRG = 18,
    WRBG = 19,
    WGBR = 20,
    WBGR = 21,
};

/**
 * Get byte count per pixel for a color order
 */
inline constexpr uint8_t getChannelCount(ColorOrder order) {
    return (static_cast<uint8_t>(order) >= 10) ? 4 : 3;
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
    WaitingReset = 2
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
public:
    virtual ~IBus() = default;

    virtual bool begin() = 0;
    virtual void end() = 0;

    /**
     * Show pixels
     * @param pixels RGBW pixel data (uint32_t per pixel, WLED format)
     * @param numPixels Number of pixels
     * @param cct Optional CCT data (2 bytes per pixel:  WW, CW)
     * @return true if transmission started successfully
     */
    virtual bool show(const uint32_t* pixels, uint16_t numPixels, 
                      const CctPixel* cct = nullptr) = 0;

    virtual bool canShow() const = 0;
    virtual void waitComplete() = 0;
    virtual const char* getType() const = 0;
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
    ColorEncoder(ColorOrder order) : _order(order), _numChannels(getChannelCount(order)) {}

    /**
     * Encode a single pixel to output buffer
     * @param pixel RGBW color value
     * @param cct Optional CCT data (for CCT strips, replaces W channel)
     * @param out Output buffer (must have space for numChannels bytes)
     */
    void encode(uint32_t pixel, const CctPixel* cct, uint8_t* out) const;

    uint8_t getNumChannels() const { return _numChannels; }
    ColorOrder getOrder() const { return _order; }

private:
    ColorOrder _order;
    uint8_t _numChannels;
};

//==============================================================================
// RMT Bus - Works on all ESP32 variants
//==============================================================================

#include "driver/rmt.h"

class RmtBus : public IBus {
public:
    /**
     * Create RMT bus
     * @param pin GPIO pin
     * @param timing LED timing
     * @param order Color order
     * @param channel RMT channel (-1 for auto)
     */
    RmtBus(int8_t pin, const LedTiming& timing, ColorOrder order,
           int8_t channel = -1);
    ~RmtBus() override;

    bool begin() override;
    void end() override;

    bool show(const uint32_t* pixels, uint16_t numPixels,
              const CctPixel* cct = nullptr) override;
    bool canShow() const override;
    void waitComplete() override;
    const char* getType() const override { return "RMT"; }

    // Configuration
    void setInverted(bool inv) { _inverted = inv; }
    void setTiming(const LedTiming& timing);
    void setColorOrder(ColorOrder order);

private:
    int8_t _pin;
    int8_t _channel;
    LedTiming _timing;
    ColorOrder _order;
    bool _inverted;
    bool _initialized;
    
    rmt_channel_t _rmtChannel;
    uint32_t _rmtBit0;
    uint32_t _rmtBit1;
    uint16_t _rmtResetTicks;

    // Encode buffer
    uint8_t* _encodeBuffer;
    size_t _encodeBufferSize;

    void updateRmtTiming();
    bool allocateBuffer(uint16_t numPixels);

    // Static translate callback
    static void IRAM_ATTR translateCB(const void* src, rmt_item32_t* dest,
                                       size_t src_size, size_t wanted_num,
                                       size_t* translated_size, size_t* item_num);
};

//==============================================================================
// I2S Parallel Bus - ESP32 and ESP32-S2
//==============================================================================

#ifdef WLEDPB_I2S_SUPPORT

#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "rom/lldesc.h"
#include "esp_intr_alloc.h"

#if defined(WLEDPB_ESP32)
    #define WLEDPB_I2S_BUS_COUNT 2
    #define WLEDPB_I2S_MAX_CHANNELS 16
#else
    #define WLEDPB_I2S_BUS_COUNT 1
    #define WLEDPB_I2S_MAX_CHANNELS 8
#endif

/**
 * I2S bus context - manages shared I2S peripheral for parallel output
 * Uses double-buffered DMA with ISR-driven buffer refill
 */
class I2sBusContext {
public: 
    static I2sBusContext* get(uint8_t busNum);
    static void release(uint8_t busNum);

    bool init(const LedTiming& timing, size_t bufferSize);
    void deinit();

    // Channel management
    int8_t registerChannel(int8_t pin, I2sBus* bus);
    void unregisterChannel(int8_t channelIdx);
    uint8_t getChannelCount() const { return _channelCount; }

    // Transmission
    bool startTransmit();
    bool isIdle() const { return _state == DriverState::Idle; }

    // Data access for channels
    void setChannelData(int8_t channelIdx, const uint8_t* data, size_t len);

private:
    I2sBusContext(uint8_t busNum);
    ~I2sBusContext();

    void fillBuffer(uint8_t bufIdx);
    static void IRAM_ATTR dmaISR(void* arg);

    uint8_t _busNum;
    i2s_dev_t* _i2sDev;
    volatile DriverState _state;
    bool _initialized;

    // DMA double buffer
    lldesc_t* _dmaDesc[2];
    uint8_t* _dmaBuffer[2];
    size_t _bufferSize;
    volatile uint8_t _activeBuffer;

    // Timing
    LedTiming _timing;
    uint32_t _clockDiv;

    // ISR handle
    intr_handle_t _isrHandle;

    // Channel data
    struct ChannelData {
        I2sBus* bus;
        int8_t pin;
        const uint8_t* srcData;
        size_t srcLen;
        size_t srcPos;
        bool active;
    };
    ChannelData _channels[WLEDPB_I2S_MAX_CHANNELS];
    uint8_t _channelCount;
    uint16_t _channelMask;
    size_t _maxDataLen;

    // Encoding (4-step cadence)
    void encode4Step(uint8_t* dest, size_t destLen, size_t* bytesWritten);

    // Singleton instances
    static I2sBusContext* _instances[WLEDPB_I2S_BUS_COUNT];
    static uint8_t _refCount[WLEDPB_I2S_BUS_COUNT];
};

/**
 * I2S parallel output bus
 */
class I2sBus : public IBus {
public:
    /**
     * Create I2S bus
     * @param pin GPIO pin
     * @param timing LED timing
     * @param order Color order
     * @param busNum I2S bus number (0 or 1 on ESP32, 0 on S2)
     * @param bufferSize DMA buffer size
     */
    I2sBus(int8_t pin, const LedTiming& timing, ColorOrder order,
           uint8_t busNum = 0, size_t bufferSize = DEFAULT_DMA_BUFFER_SIZE);
    ~I2sBus() override;

    bool begin() override;
    void end() override;

    bool show(const uint32_t* pixels, uint16_t numPixels,
              const CctPixel* cct = nullptr) override;
    bool canShow() const override;
    void waitComplete() override;
    const char* getType() const override { return "I2S"; }

    void setTiming(const LedTiming& timing) { _timing = timing; }
    void setColorOrder(ColorOrder order);

    // Access for context
    const uint8_t* getEncodedData() const { return _encodeBuffer; }
    size_t getEncodedLen() const { return _encodedLen; }

private:
    int8_t _pin;
    uint8_t _busNum;
    size_t _bufferSize;
    LedTiming _timing;
    ColorOrder _order;
    bool _initialized;

    int8_t _channelIdx;
    I2sBusContext* _ctx;

    uint8_t* _encodeBuffer;
    size_t _encodeBufferSize;
    size_t _encodedLen;

    bool allocateBuffer(uint16_t numPixels);
};

#endif // WLEDPB_I2S_SUPPORT

//==============================================================================
// LCD Parallel Bus - ESP32-S3 only
//==============================================================================
#ifdef WLEDPB_LCD_SUPPORT

#include "driver/periph_ctrl.h"
#include "esp_private/gdma.h"
#include "esp_rom_gpio.h"
#include "hal/dma_types.h"
#include "hal/gpio_hal.h"
#include "hal/lcd_ll.h"
#include "soc/lcd_cam_struct.h"
#include "soc/gpio_sig_map.h"

#ifndef WLEDPB_LCD_DMA_BUFFER_SIZE
#define WLEDPB_LCD_DMA_BUFFER_SIZE 2048
#endif

#ifndef WLEDPB_LCD_CADENCE_STEPS
#define WLEDPB_LCD_CADENCE_STEPS 4
#endif

#ifndef WLEDPB_LCD_DEBUG
#define WLEDPB_LCD_DEBUG 1
#endif

#define WLEDPB_LCD_MAX_CHANNELS 8

// Transmission state machine
enum class TxState :  uint8_t {
    Idle = 0,           // Not transmitting
    SendingData,        // Sending pixel data
    SendingReset,       // Sending zeros for reset period
    StopPending         // Will stop on next EOF
};

class LcdBusContext {
public:
    static LcdBusContext* get();
    static void release();

    bool init(const LedTiming& timing, size_t bufferSize, bool use16Bit = false);
    void deinit();

    int8_t registerChannel(int8_t pin, LcdBus* bus);
    void unregisterChannel(int8_t channelIdx);
    uint8_t getChannelCount() const { return _channelCount; }

    bool startTransmit();
    bool isIdle() const { return _state == DriverState:: Idle; }
    
    void forceIdle() {
        LCD_CAM.lcd_user.lcd_start = 0;
        _state = DriverState::Idle;
        _txState = TxState:: Idle;
    }

    void setChannelData(int8_t channelIdx, const uint8_t* data, size_t len);
    void printDebugStats();

private:
    LcdBusContext();
    ~LcdBusContext();

    size_t encodeIntoBuffer(uint8_t* dest, size_t destLen);
    bool hasMoreData() const;
    void fillBufferForState(uint8_t bufIdx);
    
    static IRAM_ATTR bool dmaCallback(gdma_channel_handle_t dma_chan,
                                       gdma_event_data_t* event_data,
                                       void* user_data);

    volatile DriverState _state;
    volatile TxState _txState;
    bool _initialized;
    bool _use16Bit;

    // DMA (circular linked list - never modified during operation)
    gdma_channel_handle_t _dmaChannel;
    dma_descriptor_t* _dmaDesc[2];
    uint8_t* _dmaBuffer[2];
    volatile uint8_t _activeBuffer;

    // Timing
    LedTiming _timing;
    size_t _resetBytesPerBuffer;
    volatile size_t _resetBytesRemaining;

    // Channels
    struct ChannelData {
        LcdBus* bus;
        int8_t pin;
        const uint8_t* srcData;
        size_t srcLen;
        size_t srcPos;
        bool active;
    };
    ChannelData _channels[WLEDPB_LCD_MAX_CHANNELS];
    uint8_t _channelCount;
    uint8_t _channelMask;
    size_t _maxDataLen;

    static LcdBusContext* _instance;
    static uint8_t _refCount;
    
    friend class LcdBus;
};

class LcdBus :  public IBus {
public: 
    LcdBus(int8_t pin, const LedTiming& timing, ColorOrder order,
           size_t bufferSize = DEFAULT_DMA_BUFFER_SIZE, bool use16Bit = false);
    ~LcdBus() override;

    bool begin() override;
    void end() override;

    bool show(const uint32_t* pixels, uint16_t numPixels,
              const CctPixel* cct = nullptr) override;
    bool canShow() const override;
    void waitComplete() override;
    const char* getType() const override { return "LCD"; }

    void setTiming(const LedTiming& timing) { _timing = timing; }
    void setColorOrder(ColorOrder order);

private:
    bool allocateBuffer(uint16_t numPixels);

    int8_t _pin;
    size_t _bufferSize;
    bool _use16Bit;
    LedTiming _timing;
    ColorOrder _order;
    bool _initialized;

    int8_t _channelIdx;
    LcdBusContext* _ctx;

    uint8_t* _encodeBuffer;
    size_t _encodeBufferSize;
    size_t _encodedLen;
};

#endif // WLEDPB_LCD_SUPPORT

//==============================================================================
// Bus Factory - Create appropriate bus for platform
//==============================================================================

enum class BusType :  uint8_t {
    RMT = 0,
    I2S = 1,
    LCD = 2,
    Auto = 255
};

/**
 * Create a bus instance
 * @param type Bus type (Auto will select best for platform)
 * @param pin GPIO pin
 * @param timing LED timing
 * @param order Color order
 * @return Bus instance (caller owns, delete when done)
 */
IBus* createBus(BusType type, int8_t pin, const LedTiming& timing, 
                ColorOrder order, size_t bufferSize = DEFAULT_DMA_BUFFER_SIZE);

/**
 * Get recommended bus type for current platform
 */
BusType getRecommendedBusType();

} // namespace WLEDpixelBus