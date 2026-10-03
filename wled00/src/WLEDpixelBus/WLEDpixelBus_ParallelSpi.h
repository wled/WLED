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
#include "esp_private/gdma.h"   // for gdma_channel_handle_t
namespace WLEDpixelBus {

//==============================================================================
// SPI Parallel Bus - ESP32-C3 (uses SPI2 quad mode + GDMA)
//==============================================================================

#define WLEDPB_SPI_MAX_CHANNELS 4   // SPI quad mode = 4 data lines
#define WLEDPB_SPI_DMA_DESC_COUNT 2   // number of DMA buffers, increase to 3 if there are flickering issues

/**
 * SPI driver state machine states.
 */
enum class SpiState : uint8_t {
  Idle = 0,         // Ready for new frame
  Sending = 1       // Data phase active, DMA running
};

/**
 * SPI bus context - manages SPI2 quad mode for parallel LED output on C3
 * Uses GDMA with circular linked-list and ISR-driven buffer refill

 * Error handling:
 *   If outfifo_empty_err fires, the transfer is stopped and recovered immediately.
 *   isIdle() recovers transfers that stop without delivering their completion ISR.
 */
class SpiBusContext {
public:
  static SpiBusContext* get();
  static void release();

  bool init(const LedTiming& timing);
  void deinit();

  int8_t registerChannel(int8_t pin, bool inverted = false);
  void unregisterChannel(int8_t channelIdx);
  uint8_t getChannelCount() const { return _channelCount; }

  bool startTransmit();
  bool isIdle() const; // returns true when idle, also handles error timeout recovery
  void forceIdle() const;  // emergency stop, disconnects pins, resets hardware

  void setChannelData(int8_t channelIdx, const uint8_t* data, size_t len);

private:
  SpiBusContext();
  ~SpiBusContext();
  void IRAM_ATTR encodeSpiChunk(uint8_t bufIdx);
  static bool IRAM_ATTR gdmaISR(gdma_channel_handle_t dma_chan, gdma_event_data_t* event_data, void* user_data);
  static void IRAM_ATTR spiISR(void* arg);
  // State machine
  mutable volatile SpiState _state;
  bool _initialized;
  bool _peripheralsEnabled;
  volatile uint8_t _activeBuffer;   // buffer currently being sent by DMA (like I2S _activeBuffer)
  // DMA
  uint8_t* _dmaBuffer[WLEDPB_SPI_DMA_DESC_COUNT];
  lldesc_t _dmaDesc[WLEDPB_SPI_DMA_DESC_COUNT];
  gdma_channel_handle_t _gdmaChan;
  int _dmaChan;
  intr_handle_t _spiIsrHandle;
  portMUX_TYPE _isrMux;
  spi_dev_t* _hw; // SPI device
  // Source data per channel
  struct ChannelData {
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
  volatile int32_t _bitsLeft;  // bits still to send in chained segments (0 = last segment)
  uint32_t _resetBits;         // reset pulse length in transfer bits, derived from LED timing
  uint32_t _lastTransmitMs;    // millis() when the current transfer started, used by isIdle() watchdog
  mutable uint8_t _stagedMask; // Staging: tracks which channels have provided data for the next frame
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
  bool show() override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "SPI"; }
#endif

private:
  int8_t _pin;
  LedTiming _timing;
  bool _initialized;

  int8_t _channelIdx;
  SpiBusContext* _ctx;
};

} // namespace WLEDpixelBus

#endif // WLEDPB_PARALLEL_SPI_SUPPORT

