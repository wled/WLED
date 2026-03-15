#pragma once

#include "WLEDpixelBus.h"
#ifdef WLEDPB_SPI_SUPPORT
namespace WLEDpixelBus {

//==============================================================================
// SPI Parallel Bus - ESP32-C3 (uses SPI2 quad mode + GDMA)
//==============================================================================


#define WLEDPB_SPI_MAX_CHANNELS 4   // SPI quad mode = 4 data lines
#define WLEDPB_SPI_DMA_BUFFER_SIZE 1024 // must be a multiple of 16 (16 DMA bytes per source byte), clocked out at ~2.6MHz, 4 bits per clock (1k per buffer means about 0.5ms interrupt intervals)
#define WLEDPB_SPI_GDMA_CHANNEL 0

class ParallelSpiBus;

/**
 * SPI bus context - manages SPI2 quad mode for parallel LED output on C3
 * Uses GDMA with circular linked-list and ISR-driven buffer refill
 */
class SpiBusContext {
public:
    static SpiBusContext* get();
    static void release();

    bool init(const LedTiming& timing);
    void deinit();

    int8_t registerChannel(int8_t pin, ParallelSpiBus* bus);
    void unregisterChannel(int8_t channelIdx);
    uint8_t getChannelCount() const { return _channelCount; }

    bool startTransmit();
    bool isIdle() const { return !_sending; }
    bool isSpiDone();
    void forceIdle();

    void setChannelData(int8_t channelIdx, const uint8_t* data, size_t len);

private:
    SpiBusContext();
    ~SpiBusContext();

    void encodeSpiChunk(uint8_t bufIdx);

    static void IRAM_ATTR gdmaISR(void* arg);
    static void IRAM_ATTR spiISR(void* arg);

    volatile bool _sending;
    bool _initialized;
    bool _hasStarted;
    volatile uint8_t _currentBuffer;

    // DMA
    uint8_t* _dmaBuffer[2];
    lldesc_t _dmaDesc[2];
    intr_handle_t _isrHandle;
    intr_handle_t _spiIsrHandle;

    // SPI device
    spi_dev_t* _hw;

    // Source data per channel
    struct ChannelData {
        ParallelSpiBus* bus;
        int8_t pin;
        const uint8_t* srcData;
        size_t srcLen;
        bool active;
    };
    ChannelData _channels[WLEDPB_SPI_MAX_CHANNELS];
    uint8_t _channelCount;
    size_t _framePos;   // current source byte position
    size_t _numBytes;   // total source bytes to send
    mutable uint32_t _lastTransmitMs;

    uint8_t _stagedMask;
    uint8_t _channelMask;

    static SpiBusContext* _instance;
    static uint8_t _refCount;
};

/**
 * SPI parallel output bus (for ESP32-C3)
 */
class ParallelSpiBus : public IBus {
public:
    ParallelSpiBus(int8_t pin, const LedTiming& timing, ColorOrder order);
    ~ParallelSpiBus() override;

    bool begin() override;
    void end() override;

    bool show(const uint32_t* pixels, uint16_t numPixels,
              const CctPixel* cct = nullptr) override;
    bool canShow() const override;
    void waitComplete() override;
    const char* getType() const override { return "SPI"; }

    void setTiming(const LedTiming& timing) { _timing = timing; }
    void setColorOrder(ColorOrder order);

private:
    bool allocateBuffer(uint16_t numPixels);

    int8_t _pin;
    LedTiming _timing;
    ColorOrder _order;
    bool _initialized;

    int8_t _channelIdx;
    SpiBusContext* _ctx;

    uint8_t* _encodeBuffer;
    size_t _encodeBufferSize;
};

} // namespace WLEDpixelBus

#endif // WLEDPB_SPI_SUPPORT
