/*-------------------------------------------------------------------------

WLEDpixelBus - parallel I2S/LCD output driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

supports ESP32, ESP32 S2 and ESP32 S3 (via LCD peripheral)
Default is 8 parallel outputs and double DMA buffering but it also supports 16 parallel outputs if needed
For 16 parallel output, triple buffering is required for glitch-free output.
Data is output in 4-step cadence meaning each LED bit is encoded into 4 I2S bits. '0' is 0b1000 and '1' is 0b1110
Encoding is highly optimized for speed as encoding is done "on the fly" while the other buffer is being sent out using DMA.
The RAM usage of the sendout buffer is number of LEDs * bytes per LED + DMA buffer size
3k per DMA buffer works well, enough for 32 RGB LEDs in 8x parallel output or roughly 0.9ms between buffer swaps
Each bus can have individual configuration of color channels but all must share the same timing

-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"
#ifdef WLEDPB_I2S_SUPPORT
namespace WLEDpixelBus {

//==============================================================================
// I2S Parallel Bus - ESP32, ESP32-S2, ESP32-S3 (LCD)
//==============================================================================

#include "driver/periph_ctrl.h"
#include "esp_rom_gpio.h"

#ifdef CONFIG_IDF_TARGET_ESP32S3
  #include "esp_private/gdma.h"
  #include "hal/dma_types.h"
  #include "hal/gpio_hal.h"
  #include "hal/lcd_ll.h"
  #include "soc/lcd_cam_struct.h"
  #include "soc/gpio_sig_map.h"
#else
  #include "soc/i2s_struct.h"
  #include "soc/i2s_reg.h"
  #include "rom/lldesc.h"
  #include "esp_intr_alloc.h"
#endif

// SOC_LCD_I80_BUSES: number of I2S peripherals that support the LCD Intel 8080
// TODO: support both buses on ESP32? (currently only bus 1 is used for LED output, one is for AR)
// mode. 2 on ESP32 (I2S0 + I2S1), 1 on ESP32-S2 (I2S0 only), 1 on ESP32-S3 (LCD_CAM).
#define WLEDPB_I2S_BUS_COUNT SOC_LCD_I80_BUSES

// note: 4-step cadence with 16 parallel outs requires 8 bytes per source bit or 192bytes per RGB LED, 1k buffer can hold ~5 LEDs, ISR will fire every 144us

// I2S DMA buffer count for circular linked list. For 8-parallel output, double buffering is enough, tripple buffering is required for 16-parallel output.
// TODO: this requires more stress-testing to ensure glitch-free outputs, for 16-parallel maybe even 4 buffers are needed under heavy load
// TODO2: the buffer count and size need to be checked agains memory usage fomulas, they may be incorrect
#ifndef WLEDPB_I2S_DMA_BUFFER_COUNT
  #ifdef WLED_PIXELBUS_16PARALLEL
    #define WLEDPB_I2S_DMA_BUFFER_COUNT 3 // need 3 buffers in 16x parallel mode
  #else
    #if SOC_RMT_TX_CANDIDATES_PER_GROUP > 4 // supports 8 RMT (ESP32 only)
    #define WLEDPB_I2S_DMA_BUFFER_COUNT 3
    #else
    #define WLEDPB_I2S_DMA_BUFFER_COUNT 2 // 2 buffers is enough if not constantly interrupted by RMT
    #endif
  #endif
#endif

// 16-bit parallel mode supports 16 channels; 8-bit supports 8 channels.
#ifdef WLED_PIXELBUS_16PARALLEL
  #define WLEDPB_I2S_MAX_CHANNELS 16
  #define WLEDPB_I2S_DMABYTES 64     // 64 bytes per pixel byte (4 clocks per bit, 2 bytes per clock)
#else
  #define WLEDPB_I2S_MAX_CHANNELS 8
  #define WLEDPB_I2S_DMABYTES 32     // 32 bytes per pixel byte (4 clocks per bit, 1 byte per clock)
#endif
#define WLEDPB_I2S_XFER_DONE_FLAG 3  // flag to indicate end of transfer, must NOT be a multiple of 4
/**
 * I2S bus context - manages shared I2S/LCD peripheral for parallel output
 * Uses circular DMA buffers with ISR-driven buffer refill
 */
class I2sBusContext {
public:
  static I2sBusContext* get(uint8_t busNum);
  static void release(uint8_t busNum);

  bool init(const LedTiming& timing);
  void deinit();

  // Channel management
  int8_t registerChannel(int8_t pin, I2sBus* bus, size_t srcBytes, bool inverted = false);
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

  void IRAM_ATTR fillBuffer(uint8_t bufIdx);
  bool _allocDmaBuffers(); // allocate/reallocate DMA buffers sized for the largest registered channel
  void IRAM_ATTR encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel); // Encoding (4-step cadence)

#ifdef CONFIG_IDF_TARGET_ESP32S3
  static IRAM_ATTR bool dmaCallback(gdma_channel_handle_t dma_chan, gdma_event_data_t* event_data, void* user_data);
#else
  static void IRAM_ATTR dmaISR(void* arg);
#endif
  void IRAM_ATTR _processEof();

  // Hardware abstraction
  bool hwInit(const LedTiming& timing);
  void hwDeinit();
  void hwStartTransfer();
  void IRAM_ATTR hwStopTransfer();
  void hwRoutePin(int8_t pin, int8_t idx, bool inverted);

  // DMA descriptor abstraction
#ifdef CONFIG_IDF_TARGET_ESP32S3
  using DmaDesc_t = dma_descriptor_t;
  static inline void descSetBuf(DmaDesc_t* d, uint8_t* b) { d->buffer = b; }
  static inline void descSetSizeAndLen(DmaDesc_t* d, size_t len) { d->dw0.size = len; d->dw0.length = len; }
  static inline void descSetLength(DmaDesc_t* d, size_t len) { d->dw0.length = len; }
  static inline void descSetNext(DmaDesc_t* d, DmaDesc_t* n) { d->next = n; }
  static inline DmaDesc_t* descGetNext(DmaDesc_t* d) { return d->next; }
  static inline void descSetEof(DmaDesc_t* d) { d->dw0.suc_eof = 1; }
  static inline void descSetOwnerDma(DmaDesc_t* d) { d->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA; }
  static inline bool descIsOwnerDma(DmaDesc_t* d) { return d->dw0.owner == DMA_DESCRIPTOR_BUFFER_OWNER_DMA; }
#else
  using DmaDesc_t = lldesc_t;
  static inline void descSetBuf(DmaDesc_t* d, uint8_t* b) { d->buf = b; }
  static inline void descSetSizeAndLen(DmaDesc_t* d, size_t len) { d->size = len; d->length = len; }
  static inline void descSetLength(DmaDesc_t* d, size_t len) { d->length = len; }
  static inline void descSetNext(DmaDesc_t* d, DmaDesc_t* n) { d->qe.stqe_next = n; }
  static inline DmaDesc_t* descGetNext(DmaDesc_t* d) { return d->qe.stqe_next; }
  static inline void descSetEof(DmaDesc_t* d) { d->eof = 1; }
  static inline void descSetOwnerDma(DmaDesc_t* d) { d->owner = 1; }
  static inline bool descIsOwnerDma(DmaDesc_t* d) { return d->owner == 1; }
#endif

#ifdef CONFIG_IDF_TARGET_ESP32S3
  gdma_channel_handle_t _dmaChannel;
#else
  uint8_t _busNum;
  i2s_dev_t* _i2sDev;
  intr_handle_t _isrHandle;
#endif

  volatile DriverState _state;
  bool _initialized;

  // DMA circular buffer chain
  DmaDesc_t* _dmaDesc[WLEDPB_I2S_DMA_BUFFER_COUNT];
  uint8_t* _dmaBuffer[WLEDPB_I2S_DMA_BUFFER_COUNT];
  size_t _bufferSize;      // actual allocated DMA buffer size (per buffer)
  size_t _maxSrcBytes;     // max source (encoded pixel) bytes across all registered channels; drives DMA sizing
  bool _dmaAllocated;      // true when DMA buffers are allocated and reflect current _maxSrcBytes
  volatile uint8_t _activeBuffer;
  volatile uint8_t _remainingDataBuffers;
  volatile uint16_t _resetBytesLeft;

  // Timing
  LedTiming _timing;
  uint32_t _clockDiv;

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
  uint16_t _stagedMask;
  size_t _maxDataLen;

  // Singleton instances
  static I2sBusContext* _instances[WLEDPB_I2S_BUS_COUNT];
  static uint8_t _refCount[WLEDPB_I2S_BUS_COUNT];
};

/**
 * I2S parallel output bus
 */
class I2sBus : public PixelBus {
public:
  /**
   * Create I2S bus
   * @param pin GPIO pin
   * @param timing LED timing
   * @param colorOrder Color order
   * @param numChannels Bytes per pixel
   * @param busNum I2S bus number (0 or 1 on ESP32, 0 on S2/S3)
   * @param ledType LED chip type constant
   * @param numPixels Number of pixels; stored for DMA buffer sizing in I2sBusContext
   */
  I2sBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t busNum = 1, uint8_t ledType = 0, size_t numPixels = 0);
  ~I2sBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct = nullptr) override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "I2S"; }
#endif

  // Override to use DMA-capable allocator for I2S
  bool allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) override;

  void setInverted(bool inv) override;
  void setColorOrder(uint8_t co);

private:
  int8_t _pin;
  uint8_t _busNum;
  LedTiming _timing;
  bool _inverted = false;
  bool _initialized;

  int8_t _channelIdx;
  I2sBusContext* _ctx;

  // _encodeBuffer and _encodeBufferSize are in PixelBus base (allocated DMA-capable via allocateEncodeBuffer override)
  bool allocateBuffer(uint16_t numPixels);  // legacy, calls allocateEncodeBuffer
};

} // namespace WLEDpixelBus
#endif // WLEDPB_I2S_SUPPORT

