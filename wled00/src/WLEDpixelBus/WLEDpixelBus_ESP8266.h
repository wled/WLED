/*-------------------------------------------------------------------------

WLEDpixelBus - ESP8266 driver implementation

written by Damian Schneider @dedehai 2026

I would like to thank Michael C. Miller (@Makuna), NeoPixelBus helped me figure out the proper hardware initialisation.

Supports UART and I2S DMA output as well as parallel bit-banging

-------------------------------------------------------------------------*/
#pragma once

#include "WLEDpixelBus.h"

#ifdef WLEDPB_ESP8266

namespace WLEDpixelBus {

//==============================================================================
// ESP8266 UART Bus (Asynchronous via UART1/UART0)
//==============================================================================

class Esp8266UartBus : public PixelBus {
public:
  Esp8266UartBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType = 0);
  ~Esp8266UartBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels = nullptr, uint16_t numPixels = 0, const CctPixel* cct = nullptr) override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "ESP8266_UART"; }
#endif

  void setTiming(const LedTiming& timing) { _timing = timing; }
  void setColorOrder(uint8_t co);

  static void UartIsr(void* arg, void* exceptionFrame);
  static Esp8266UartBus* s_instances[2];

private:
  int8_t _pin;
  LedTiming _timing;
  bool _initialized;
  volatile uint8_t* _asyncBuf    = nullptr;
  volatile uint8_t* _asyncBufEnd = nullptr;

  void updateUartTiming();
};

//==============================================================================
// ESP8266 DMA Bus (Via I2S + SLC linked-list DMA)
//==============================================================================

// SLC DMA descriptor (matches SDK slc_queue_item layout)
struct SlcQueueItem {
  uint32_t blocksize  : 12;
  uint32_t datalen    : 12;
  uint32_t unused     :  5;
  uint32_t sub_sof    :  1;
  uint32_t eof        :  1;
  uint32_t owner      :  1;
  uint8_t* buf_ptr;
  struct SlcQueueItem* next_link_ptr;
};

class Esp8266DmaBus : public PixelBus {
public:
  Esp8266DmaBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType = 0);
  ~Esp8266DmaBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels = nullptr, uint16_t numPixels = 0, const CctPixel* cct = nullptr) override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "ESP8266_DMA"; }
#endif

  IRAM_ATTR bool setPixel(uint16_t pos, uint32_t c, uint16_t wwcw) override;
  IRAM_ATTR uint32_t getPixelColor(uint16_t pix) const override;
  bool allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) override;
  void updateSuffix(const uint8_t* data, uint8_t len) override;
  void scaleAll(uint8_t scale) override;

  void setTiming(const LedTiming& timing) { _timing = timing; }
  void setColorOrder(uint8_t co);

  static Esp8266DmaBus* s_this;

private:
  static const uint16_t c_maxDmaBlockSize = 4095;
  static const uint8_t  c_stateBlockCount = 2;
  static const uint16_t c_idleBufSize     = 256; // size of idle/reset zero buffer

  int8_t _pin; // Only GPIO3 supported for I2S DMA on ESP8266
  LedTiming _timing;
  bool _initialized;
  volatile bool _sending;

  // SLC DMA linked-list
  SlcQueueItem* _dmaDesc;    // allocated array of all descriptors
  uint16_t      _dmaDescCnt; // total count of descriptors
  uint8_t*      _idleBuf;    // zero-filled buffer shared by state + reset descriptors
  size_t        _idleBufSize;

  void   buildDescriptorChain();
  void   startI2s(uint8_t bckDiv, uint8_t clkDiv);
  void   stopI2s();
  static void IRAM_ATTR slcIsr();

  // DmaItemInit helper
  static void dmaItemInit(SlcQueueItem* item, uint8_t* data, size_t sz, SlcQueueItem* next) {
    item->owner = 1; item->eof = 0; item->sub_sof = 0; item->unused = 0;
    item->datalen = sz; item->blocksize = sz;
    item->buf_ptr = data; item->next_link_ptr = next;
  }
};

//==============================================================================
// ESP8266 BitBang Bus (supports parallel output across multiple pins)
//==============================================================================

class Esp8266BitBangBus : public PixelBus {
public:
  Esp8266BitBangBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType = 0);
  ~Esp8266BitBangBus() override;

  bool begin() override;
  void end() override;

  // Stage this channel's data. When all channels have staged, output all in parallel.
  bool show(const uint32_t* pixels = nullptr, uint16_t numPixels = 0, const CctPixel* cct = nullptr) override;
  // BitBang output is synchronous — always ready once initialized.
  bool canShow() const override { return _initialized; }
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "ESP8266_BB"; }
#endif

  void setTiming(const LedTiming& timing);
  void setColorOrder(uint8_t co);

  // Reset the shared static channel registry (called when all buses are destroyed).
  static void resetChannels();

private:
  int8_t _pin;
  LedTiming _timing;
  bool _initialized;

  // -----------------------------------------------------------------------
  // Shared static state (one context for ALL Esp8266BitBangBus instances)
  // All timing fields are static because every BitBang bus must use the
  // same LED type (enforced by PixelBusAllocator).
  // ESP8266 has only one 32-bit GPIO output register (pins 0–15 usable as output).
  // -----------------------------------------------------------------------
  static int8_t    s_pins[WLED_MAX_BB_CHANNELS];
  static uint16_t  s_numPixels[WLED_MAX_BB_CHANNELS];
  static uint8_t*  s_pixelData[WLED_MAX_BB_CHANNELS];
  static uint8_t   s_channelCount;
  static uint32_t  s_allMask;       // GPIO bitmask of all registered output pins
  static uint8_t   s_stagedCount;   // how many channels have called show() this frame
  // Timing in CPU cycles — identical across all channels
  static uint32_t  s_t0h;
  static uint32_t  s_t1h;
  static uint32_t  s_period;
  static uint8_t   s_pixelBytes;    // bytes per encoded pixel

  // Core output routine — must run from IRAM for timing accuracy
  static bool IRAM_ATTR outputParallel();
};

} // namespace WLEDpixelBus

#endif // WLEDPB_ESP8266

