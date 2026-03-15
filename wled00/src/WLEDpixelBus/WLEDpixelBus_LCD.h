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

#ifndef WLEDPB_LCD_CADENCE_STEPS
#define WLEDPB_LCD_CADENCE_STEPS 4
#endif

#if WLEDPB_LCD_DEBUG
  #define LCD_LOG(fmt, ...) Serial.printf("[LCD] " fmt "\n", ##__VA_ARGS__)
#else
  #define LCD_LOG(fmt, ...)
#endif

static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE >= 64, "DMA buffer too small");
static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE <= 4092, "DMA buffer too large");
static_assert(WLEDPB_LCD_DMA_BUFFER_SIZE % 4 == 0, "DMA buffer must be multiple of 4");
static_assert(WLEDPB_LCD_CADENCE_STEPS == 3 || WLEDPB_LCD_CADENCE_STEPS == 4, "Cadence must be 3 or 4");

#define WLEDPB_LCD_MAX_CHANNELS 16

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
  dma_descriptor_t* _dmaDesc[2];
  uint8_t* _dmaBuffer[2];
  volatile uint8_t _activeBuffer;

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

} // namespace WLEDpixelBus

#endif // WLEDPB_LCD_SUPPORT