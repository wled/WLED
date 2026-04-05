#pragma once

#include "WLEDpixelBus.h"
#ifdef WLEDPB_I2S_SUPPORT
namespace WLEDpixelBus {

//==============================================================================
// I2S Parallel Bus - ESP32 and ESP32-S2
//==============================================================================

#include "soc/i2s_struct.h"
#include "soc/i2s_reg.h"
#include "driver/periph_ctrl.h"
#include "rom/lldesc.h"
#include "esp_intr_alloc.h"

#if defined(WLEDPB_ESP32)
  #define WLEDPB_I2S_BUS_COUNT 2 // TODO: support both buses on ESP32? (currently only bus 1 is used for LED output, one is for AR)
#else // S2 (C3 & S3 do not support parallel I2S output, S3 uses LCD peripheral instead, C3 has parallel SPI as an alternative)
  #define WLEDPB_I2S_BUS_COUNT 1
#endif

// note: 4-step cadence with 16 parallel outs requires 8 bytes per source bit or 192bytes per LED, 1k buffer can hold ~5 LEDs, ISR will fire every 144us
// TODO: 16 parallel channels does not make much sense to use as a default. should enable both 8 and 16 parallel channels.
//
// on ESP32, 8*768 works with 8RMT, 8*512 does not. 6*768 also flickers, 6*1024 works
// with improved DMA ISR: works with 3x2k buffer


// I2S DMA buffer count for circular linked list. Default is double-buffering (2); can be increased for deeper pipelining.
#ifndef WLEDPB_I2S_DMA_BUFFER_COUNT
  #define WLEDPB_I2S_DMA_BUFFER_COUNT 3
#endif

#define WLEDPB_I2S_MAX_CHANNELS 16 // if using both I2S, 32 would be possible but that is probably way overkill to implement, 16 I2S + 8RMT is good enough

/**
 * I2S bus context - manages shared I2S peripheral for parallel output
 * Uses circular DMA buffers with ISR-driven buffer refill
 */
class I2sBusContext {
public: 
  static I2sBusContext* get(uint8_t busNum);
  static void release(uint8_t busNum);

  bool init(const LedTiming& timing, size_t bufferSize);
  void deinit();

  // Channel management
  int8_t registerChannel(int8_t pin, I2sBus* bus, bool inverted = false);
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

  // DMA circular buffer chain
  lldesc_t* _dmaDesc[WLEDPB_I2S_DMA_BUFFER_COUNT];
  uint8_t* _dmaBuffer[WLEDPB_I2S_DMA_BUFFER_COUNT];
  size_t _bufferSize;
  volatile uint8_t _activeBuffer;
  volatile uint8_t _remainingDataBuffers;

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
  uint16_t _stagedMask;
  size_t _maxDataLen;

  // Encoding (4-step cadence)
  void IRAM_ATTR encode4Step(uint8_t* dest, size_t destLen);

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
   * @param order Color order
   * @param busNum I2S bus number (0 or 1 on ESP32, 0 on S2)
   * @param bufferSize DMA buffer size
   */
  I2sBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels,
       uint8_t busNum = 1, size_t bufferSize = DEFAULT_DMA_BUFFER_SIZE, uint8_t ledType = 0);
  ~I2sBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels, uint16_t numPixels,
        const CctPixel* cct = nullptr) override;
  bool canShow() const override;
  const char* getType() const override { return "I2S"; }

  // Override to use DMA-capable allocator for I2S
  bool allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) override;

  void setInverted(bool inv) override;
  void setTiming(const LedTiming& timing) { _timing = timing; }
  void setColorOrder(uint8_t co);

private:
  int8_t _pin;
  uint8_t _busNum;
  size_t _bufferSize;
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

