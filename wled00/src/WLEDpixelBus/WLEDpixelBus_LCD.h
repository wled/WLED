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
#define WLEDPB_LCD_DMA_BUFFER_SIZE 2048 //2048  -> 2048 works well, still no glitches with 512 and an RMT running as well, 1024 seems to work fine, needs more testing (larger buffer might improve FPS)
#endif

// I2S DMA buffer count for circular linked list. For 8-parallel output, double buffering is enough, tripple buffering may be required for 16-parallel output
// TODO: this requires more stress-testing to ensure glitch-free outputs, for 16-parallel maybe even 4 buffers are needed under heavy load
// TODO2: the buffer count and size need to be checked agains memory usage fomulas, they may be incorrect
#ifndef WLEDPB_I2S_DMA_BUFFER_COUNT
  #ifdef WLED_PIXELBUS_16PARALLEL
    #define WLEDPB_I2S_DMA_BUFFER_COUNT 3
  #else
    #define WLEDPB_I2S_DMA_BUFFER_COUNT 2
  #endif
#endif

#ifndef WLEDPB_LCD_CADENCE_STEPS
#define WLEDPB_LCD_CADENCE_STEPS 4  // TODO: 3-step cadence was mostly abandoned, fully remove it? 4 step cadence is generally better to hit timing targets.
#endif

static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE >= 64, "DMA buffer too small");
static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE <= 4092, "DMA buffer too large");
static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE % 4 == 0, "DMA buffer must be multiple of 4");
static_assert(WLEDPB_LCD_CADENCE_STEPS == 3 || WLEDPB_LCD_CADENCE_STEPS == 4, "Cadence must be 3 or 4");

// 16-bit parallel mode supports 16 channels; 8-bit supports 8 channels.
#ifdef WLED_PIXELBUS_16PARALLEL
  #define WLEDPB_LCD_MAX_CHANNELS 16
#else
  #define WLEDPB_LCD_MAX_CHANNELS 8
#endif

class LcdBusContext {
public:
  static LcdBusContext* get();
  static void release();

  bool init(const LedTiming& timing, size_t bufferSize, bool use16Bit = false);
  void deinit();

  int8_t registerChannel(int8_t pin, LcdBus* bus, bool inverted = false);
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
  void printDebugStats();

private:
  LcdBusContext();
  ~LcdBusContext();

  void IRAM_ATTR encode4Step(uint8_t* dest, size_t destLen);
  void fillBuffer(uint8_t bufIdx);
  
  static IRAM_ATTR bool dmaCallback(gdma_channel_handle_t dma_chan,
                     gdma_event_data_t* event_data,
                     void* user_data);

  volatile DriverState _state;
  bool _initialized;
  bool _use16Bit;

  // DMA (circular linked list - never modified during operation)
  gdma_channel_handle_t _dmaChannel;
  dma_descriptor_t* _dmaDesc[WLEDPB_LCD_DMA_BUFFER_COUNT];
  uint8_t* _dmaBuffer[WLEDPB_LCD_DMA_BUFFER_COUNT];
  volatile uint8_t _activeBuffer;
  volatile uint8_t _remainingDataBuffers;

  // Timing
  LedTiming _timing;
  size_t _bufferSize;

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
  LcdBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels,
       size_t bufferSize = DEFAULT_DMA_BUFFER_SIZE, bool use16Bit = false, uint8_t ledType = 0);
  ~LcdBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels, uint16_t numPixels,
        const CctPixel* cct = nullptr) override;
  bool canShow() const override;
  const char* getType() const override { return "LCD"; }

  void setInverted(bool inv) override;
  void setTiming(const LedTiming& timing) { _timing = timing; }
  void setColorOrder(uint8_t co);

private:
  int8_t _pin;
  size_t _bufferSize;
  bool _use16Bit;
  LedTiming _timing;
  bool _inverted = false;
  bool _initialized;

  int8_t _channelIdx;
  LcdBusContext* _ctx;
};

} // namespace WLEDpixelBus

#endif // WLEDPB_LCD_SUPPORT
