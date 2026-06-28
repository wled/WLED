/*-------------------------------------------------------------------------

WLEDpixelBus - parallel SPI output driver implementation

written by Damian Schneider @dedehai 2026

supports ESP32 C3
uses 4 parallel outputs and double DMA buffering
Data is output in 4-step cadence meaning each LED bit is encoded into 4 bits. '0' is 0b1000 and '1' is 0b1110
Encoding is highly optimized for speed as encoding is done "on the fly" while the other buffer is being sent out using DMA.
The RAM usage of the sendout buffer is number of LEDs * bytes per LED + DMA buffer size
2k per DMA buffer works well, enough for 42 RGB LEDs or roughly 1.2ms between buffer swaps
Each bus can have individual configuration of color channels but all must share the same timing

-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"
#ifdef WLEDPB_PARALLEL_SPI_SUPPORT
namespace WLEDpixelBus {

//==============================================================================
// SPI Parallel Bus - ESP32-C3 (uses SPI2 quad mode + GDMA)
//==============================================================================

#define WLEDPB_SPI_MAX_CHANNELS 4   // SPI quad mode = 4 data lines
#define WLEDPB_SPI_DMA_BUFFER_SIZE 2048 // must be a multiple of 16 (16 DMA bytes per source byte), clocked out at ~2.6MHz, 4 bits per clock (2k per buffer means about 1ms interrupt intervals with 4 step cadence)
#define WLEDPB_SPI_DMA_DESC_COUNT 2   // number of DMA buffers, 3 = tripple buffering to avoid SPI starvation under heavy load (TODO: 2 is probably more than enough, currently debugging issues...)
#define WLEDPB_SPI_GDMA_CHANNEL 1 // TODO: how to manage the DMA channels to avoid conflicts with other peripherals? for now we just assume channel 1 is free and used exclusively by this driver
#define WLEDPB_SPI_GDMA_INTR_SOURCE ETS_DMA_CH1_INTR_SOURCE // must match dma channel (otherwise it just loops the two DMA descriptors and will eventually time-out)

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

  int8_t registerChannel(int8_t pin, ParallelSpiBus* bus, bool inverted = false);
  void unregisterChannel(int8_t channelIdx);
  uint8_t getChannelCount() const { return _channelCount; }

  bool startTransmit();
  bool isSpiDone();
  bool isIdle() { return isSpiDone(); }
  void forceIdle();

  void setChannelData(int8_t channelIdx, const uint8_t* data, size_t len);

private:
  SpiBusContext();
  ~SpiBusContext();

  void IRAM_ATTR encodeSpiChunk(uint8_t bufIdx);

  static void IRAM_ATTR gdmaISR(void* arg);
  static void IRAM_ATTR spiISR(void* arg);

  volatile bool _txdone;
  bool _initialized;
  //bool _hasStarted;
  volatile uint8_t _currentBuffer;

  // DMA
  uint8_t* _dmaBuffer[WLEDPB_SPI_DMA_DESC_COUNT];
  lldesc_t _dmaDesc[WLEDPB_SPI_DMA_DESC_COUNT];
  intr_handle_t _gdmaIsrHandle;
  intr_handle_t _spiIsrHandle;

  // SPI device
  spi_dev_t* _hw;

  // Source data per channel
  struct ChannelData {
    ParallelSpiBus* bus;
    const uint8_t* srcData;
    size_t srcLen;
    int8_t pin;
    bool active;
    bool inverted;
  };
  ChannelData _channels[WLEDPB_SPI_MAX_CHANNELS];
  uint8_t _channelCount;
  volatile size_t _framePos;   // current source byte position
  volatile size_t _numBytes;   // total source bytes to send
  mutable uint32_t _lastTransmitMs;

  uint8_t _stagedMask;
  uint8_t _channelMask;

  static SpiBusContext* _instance;
  static uint8_t _refCount;
};

/**
 * SPI parallel output bus (for ESP32-C3)
 */
class ParallelSpiBus : public PixelBus {
public:
  ParallelSpiBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType = 0);
  ~ParallelSpiBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels, uint16_t numPixels,
        const CctPixel* cct = nullptr) override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "SPI"; }
#endif

  bool allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) override;
  void setInverted(bool inv) override;
  void setColorOrder(uint8_t co);

private:
  int8_t _pin;
  LedTiming _timing;
  bool _inverted = false;
  bool _initialized;

  int8_t _channelIdx;
  SpiBusContext* _ctx;
};

} // namespace WLEDpixelBus

#endif // WLEDPB_PARALLEL_SPI_SUPPORT

