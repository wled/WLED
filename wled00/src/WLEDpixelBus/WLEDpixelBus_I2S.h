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
       uint8_t busNum = 1, size_t bufferSize = DEFAULT_DMA_BUFFER_SIZE);
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

  // Memory estimation: per-bus encode buffer + shared I2S DMA context (on first bus)
  static size_t estimateMemory(uint16_t numPixels, uint8_t channelCount, bool isFirstBus = true) {
    size_t mem = numPixels * channelCount;  // per-bus encode buffer
    if (isFirstBus) mem += DEFAULT_DMA_BUFFER_SIZE * 2 + 64;  // shared: 2× DMA buffers + 2× descriptors
    return mem;
  }

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

  bool allocateBuffer(uint16_t numPixels);
};

} // namespace WLEDpixelBus
#endif // WLEDPB_I2S_SUPPORT
