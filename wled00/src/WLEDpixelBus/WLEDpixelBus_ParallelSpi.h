/*-------------------------------------------------------------------------

WLEDpixelBus - parallel SPI output driver implementation

written by Damian Schneider @dedehai 2026

currently only supports ESP32 C3 (other ESPs have I2S, LCD or PARLIO)
uses 4 parallel outputs and double DMA buffering
Data is output in 4-step cadence meaning each LED bit is encoded into 4 bits. '0' is 0b1000 and '1' is 0b1110
Encoding is highly optimized for speed as encoding is done "on the fly" while the other buffer is being sent out using DMA.
The RAM usage of the sendout buffer is number of LEDs * bytes per LED + DMA buffer size
2k per DMA buffer works well, enough for 42 RGB LEDs or roughly 1.2ms between buffer swaps
Each bus can have individual configuration of color channels but all must share the same timing

-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"
#ifdef WLEDPB_PARALLELSPI_SUPPORT
#include "WLEDpixelBus_Parallel.h" // shared base classes for the parallel drivers
#include "esp_private/gdma.h"   // for gdma_channel_handle_t
namespace WLEDpixelBus {

#define WLEDPB_PARALLELSPI_MAX_CHANNELS 4    // SPI quad mode = 4 data lines
#define WLEDPB_PARALLELSPI_DMA_BUFFER_COUNT 2  // number of DMA buffers, increase to 3 if there are flickering issues
#define WLEDPB_PARALLELSPI_DMABYTES 16       // 16 DMA bytes per source byte in quad-SPI 4-step encoding

 // Parallel-SPI bus context - manages SPI2 quad mode for parallel LED output
 // Uses GDMA with circular linked-list and ISR-driven buffer refill
class ParallelSpiBusContext : public ParallelBusContext<WLEDPB_PARALLELSPI_MAX_CHANNELS> {
public:
  static ParallelSpiBusContext* get(uint8_t busNum = 0); // note: the bus number is ignored
  static void release(uint8_t busNum = 0);

  bool startTransmit();
  bool ensureIdle() override; // non-blocking completion check with timeout recovery

private:
  ParallelSpiBusContext();
  ~ParallelSpiBusContext();
  void IRAM_ATTR fillBuffer(uint8_t bufIdx);
  bool _allocDmaBuffers();
  static bool IRAM_ATTR gdmaISR(gdma_channel_handle_t dma_chan, gdma_event_data_t* event_data, void* user_data);
  static void IRAM_ATTR spiISR(void* arg);

  bool hwInit() override;
  void hwDeinit() override;
  void releaseResources() override;
  void abortTransferHw() override; // emergency stop, disconnects pins, resets hardware
  void channelAttached(int8_t idx, int8_t pin, bool inverted) override;
  void channelDetached(int8_t idx, int8_t pin) override;

  bool _peripheralsEnabled;
  volatile uint8_t _activeBuffer;   // buffer currently being sent by DMA (like I2S _activeBuffer)
  // DMA
  uint8_t* _dmaBuffer[WLEDPB_PARALLELSPI_DMA_BUFFER_COUNT];
  lldesc_t _dmaDesc[WLEDPB_PARALLELSPI_DMA_BUFFER_COUNT];
  gdma_channel_handle_t _gdmaChan;
  int _dmaChanId;
  intr_handle_t _spiIsrHandle;
  portMUX_TYPE _isrMux;
  spi_dev_t* _hw; // SPI device
  volatile size_t _framePos;   // current source byte position
  volatile size_t _numBytes;   // total source bytes to send
  volatile int32_t _bitsLeft;  // bits still to send in chained segments (0 = last segment)
  uint32_t _resetBits;         // reset pulse length in transfer bits, derived from LED timing

  static ParallelSpiBusContext* _instance;
  static uint8_t _refCount;
};


 // SPI parallel output bus
class ParallelSpiBus : public ParallelBusBase<ParallelSpiBusContext> {
public:
  ParallelSpiBus(int8_t pin, const LedTiming& timing, uint8_t ledType = 0);
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "PARALLEL_SPI"; }
#endif
};

} // namespace WLEDpixelBus

#endif // WLEDPB_PARALLELSPI_SUPPORT
