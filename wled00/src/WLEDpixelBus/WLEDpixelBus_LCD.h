/*-------------------------------------------------------------------------

WLEDpixelBus - parallel LCD output driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

LCD hardware is available on ESP32 S3 only
Default is 8 parallel outputs and double DMA buffering but it also supports 16 parallel outputs if needed
For 16 parallel output, triple buffering is required for glitch-free output.
Data is output in 4-step cadence meaning each LED bit is encoded into 4 I2S bits. '0' is 0b1000 and '1' is 0b1110
Encoding is highly optimized for speed as encoding is done "on the fly" while the other buffer is being sent out using DMA.
The RAM usage of the sendout buffer is number of LEDs * bytes per LED + DMA buffer size
3k per DMA buffer works well, enough for 32 RGB LEDs in 8x parallel output or roughly 0.9ms between buffer swaps
Each bus can have individual configuration of color channels but all must share the same timing

-------------------------------------------------------------------------*/

#pragma once
#ifdef WLEDPB_LCD_SUPPORT
#include "WLEDpixelBus.h"

namespace WLEDpixelBus {

//==============================================================================
// LCD Parallel Bus - ESP32-S3 only
//==============================================================================

#include "driver/periph_ctrl.h"
#include "esp_private/gdma.h"
#include "esp_rom_gpio.h"
#include "hal/dma_types.h"
#include "hal/gpio_hal.h"
#include "hal/lcd_ll.h"
#include "soc/lcd_cam_struct.h"
#include "soc/gpio_sig_map.h"

#ifndef WLEDPB_LCD_DMA_BUFFER_SIZE
#define WLEDPB_LCD_DMA_BUFFER_SIZE (3*1024) // 2048 works as well, still no glitches with 512 and an RMT running as well, 1024 seems to work fine, needs more testing (larger buffer might improve FPS)
#endif

// I2S DMA buffer count for circular linked list. For 8-parallel output, double buffering is enough, tripple buffering may be required for 16-parallel output
// TODO: this requires more stress-testing to ensure glitch-free outputs, for 16-parallel maybe even 4 buffers are needed under heavy load
// TODO2: the buffer count and size need to be checked agains memory usage fomulas, they may be incorrect
#ifndef WLEDPB_LCD_DMA_BUFFER_COUNT
  #ifdef WLED_PIXELBUS_16PARALLEL
    #define WLEDPB_LCD_DMA_BUFFER_COUNT 3
  #else
    #define WLEDPB_LCD_DMA_BUFFER_COUNT 2
  #endif
#endif

#ifndef WLEDPB_LCD_CADENCE_STEPS
#define WLEDPB_LCD_CADENCE_STEPS 4  // TODO: 3-step cadence was mostly abandoned, fully remove it? 4 step cadence is generally better to hit timing targets.
#endif

// 16-bit parallel mode supports 16 channels; 8-bit supports 8 channels.
#ifdef WLED_PIXELBUS_16PARALLEL
  #define WLEDPB_LCD_MAX_CHANNELS 16
#else
  #define WLEDPB_LCD_MAX_CHANNELS 8
#endif


static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE >= 64, "DMA buffer too small");
static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE <= 4092, "DMA buffer too large");
static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE % 4 == 0, "DMA buffer must be multiple of 4");
static_assert(WLEDPB_LCD_CADENCE_STEPS == 3 || WLEDPB_LCD_CADENCE_STEPS == 4, "Cadence must be 3 or 4");
// SOC_LCD_I80_BUS_WIDTH is the physical output width of the LCD I80 peripheral.
// On ESP32-S3 this is 16, so 16-parallel is the hardware maximum.
static_assert(WLEDPB_LCD_MAX_CHANNELS <= SOC_LCD_I80_BUS_WIDTH,
  "WLEDPB_LCD_MAX_CHANNELS exceeds hardware LCD bus width (SOC_LCD_I80_BUS_WIDTH)");

class LcdBusContext {
public:
  static LcdBusContext* get();
  static void release();

  bool init(const LedTiming& timing, bool use16Bit = false);
  void deinit();

  int8_t registerChannel(int8_t pin, LcdBus* bus, size_t srcBytes, bool inverted = false);
  void unregisterChannel(int8_t channelIdx);
  uint8_t getChannelCount() const { return _channelCount; }

  bool startTransmit();
  bool isIdle() const { return _state == DriverState::Idle; }

  void forceIdle() {
    LCD_CAM.lcd_user.lcd_start = 0;
    if (_dmaChannel) gdma_stop(_dmaChannel);
    _state = DriverState::Idle;
  }

  void setChannelData(int8_t channelIdx, const uint8_t* data, size_t len);

private:
  LcdBusContext();
  ~LcdBusContext();

  void IRAM_ATTR encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel);
  void fillBuffer(uint8_t bufIdx);
  bool _allocDmaBuffers(); // allocate/reallocate DMA buffers sized for the largest registered channel
  static IRAM_ATTR bool dmaCallback(gdma_channel_handle_t dma_chan, gdma_event_data_t* event_data, void* user_data);

  volatile DriverState _state;
  bool _initialized;
  bool _use16Bit;

  // DMA (circular linked list - never modified during operation)
  gdma_channel_handle_t _dmaChannel;
  dma_descriptor_t* _dmaDesc[WLEDPB_LCD_DMA_BUFFER_COUNT];
  uint8_t* _dmaBuffer[WLEDPB_LCD_DMA_BUFFER_COUNT];

  // Timing
  LedTiming _timing;
  size_t _bufferSize;
  size_t _maxSrcBytes;     // max source (encoded pixel) bytes across all registered channels; drives DMA sizing
  bool _dmaAllocated;      // true when DMA buffers are allocated and reflect current _maxSrcBytes
  volatile uint8_t _activeBuffer;
  volatile uint8_t _remainingDataBuffers;
  volatile uint16_t _resetBytesLeft;

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
  uint16_t _channelMask;
  uint16_t _stagedMask;
  size_t _maxDataLen;


  static LcdBusContext* _instance;
  static uint8_t _refCount;
  friend class LcdBus;
};

class LcdBus :  public PixelBus {
public:
  LcdBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, size_t numPixels,  bool use16Bit = false, uint8_t ledType = 0);
  ~LcdBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels, uint16_t numPixels,
        const CctPixel* cct = nullptr) override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "LCD"; }
#endif

  void setInverted(bool inv) override;
  void setColorOrder(uint8_t co);

private:
  int8_t _pin;
  bool _use16Bit;
  LedTiming _timing;
  bool _inverted = false;
  bool _initialized;

  int8_t _channelIdx;
  LcdBusContext* _ctx;
};

} // namespace WLEDpixelBus

#endif // WLEDPB_LCD_SUPPORT
