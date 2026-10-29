// WLEDpixelBus_PARLIO.h
/*-------------------------------------------------------------------------

WLEDpixelBus - parallel PARLIO output driver implementation

written by Damian Schneider @dedehai 2026

Note: PARLIO driver was generated with heavy help of an AI with lots of refinement and testing (works but needs a thorough review)

Supports ESP32-C6, ESP32-H2 and ESP32-C5 via the PARLIO TX peripheral (8 parallel outputs).
Each bus can have individual configuration of color channels but all must share the same timing.
ESP32-P4 has a 16-bit wide PARLIO unit, this driver uses 8 bits of it (could be extended).
Requires IDF >= 5.3 (driver/parlio_tx.h). Not available on ESP32, S2, S3, C3, C2.

Architecture mirrors the I2S driver (WLEDpixelBus_I2S): data is streamed from a circular
GDMA descriptor ring that we own, refilled on the fly from per-descriptor EOF interrupts.
This seamless transmission avoids the ~100us pauses that the stock IDF driver has.
The IDF driver is still used for unit creation (clock setup, GPIO matrix routing, interrupt install)

Fallback (WLEDPB_PARLIO_SEAMLESS_DMA == 0, e.g. ESP32-P4 with SOC_PARLIO_TX_SIZE_BY_DMA or non-AHB GDMA):
the whole frame (data + reset period) is encoded into a single buffer up front and uses the IDF driver to send it.

-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"
#ifdef ARDUINO_ARCH_ESP32
#include <esp_idf_version.h>
#include "soc/soc_caps.h"
#endif

#ifdef SOC_PARLIO_SUPPORTED
#define WLEDPB_PARLIO_SUPPORT

#include "driver/parlio_tx.h"
#include "driver/gpio.h"

#if defined(SOC_AHB_GDMA_SUPPORTED) && SOC_AHB_GDMA_SUPPORTED && !(defined(SOC_PARLIO_TX_SIZE_BY_DMA) && SOC_PARLIO_TX_SIZE_BY_DMA) && !defined(CONFIG_IDF_TARGET_ESP32P4) && ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
  #define WLEDPB_PARLIO_SEAMLESS_DMA 1
#else
  #define WLEDPB_PARLIO_SEAMLESS_DMA 0 // fall back to "single buffer transfer" mode (uses more RAM)
#endif

#if WLEDPB_PARLIO_SEAMLESS_DMA
  #include "hal/dma_types.h"     // dma_descriptor_t, DMA_DESCRIPTOR_BUFFER_*
  #include "esp_private/gdma.h"  // gdma_start/stop/append, gdma_register_tx_event_callbacks

// Mirror of the beginning of the IDF driver's private struct parlio_tx_unit_t (parlio_tx.c, parlio_priv.h since v5.5)
// up to the dma_chan field which we need to reconfigure the DMA. In V6 it changed again but not before dma_chan, so the mirror is still valid.
typedef struct {
  int unit_id;
  int dir;
  void* group;
  size_t data_width;
  gpio_num_t data_gpio_nums[SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH];
  gpio_num_t valid_gpio_num;
  gpio_num_t clk_out_gpio_num;
  gpio_num_t clk_in_gpio_num;
  void* intr;
  void* pm_lock;
  gdma_channel_handle_t dma_chan;
} WledpbParlioTxUnitHead;

#define WLEDPB_PARLIO_TX_DMA_CHAN(unit) (((const WledpbParlioTxUnitHead*)(unit))->dma_chan)
#endif

namespace WLEDpixelBus {

#define WLEDPB_PARLIO_BUS_COUNT 1 // only one PARLIO TX unit per chip (note: busNum in constructor is always 0 for now, could remove that parameter)

#ifndef WLEDPB_PARLIO_DMA_BUFFER_COUNT
  #define WLEDPB_PARLIO_DMA_BUFFER_COUNT 3 // 3 buffers in a ring configuration seems to work well even when using RMT
#endif


#define WLEDPB_PARLIO_MAX_CHANNELS 8  // PARLIO TX on C6/H2/C5 supports up to 8 data lines TODO: use SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH ?
#define WLEDPB_PARLIO_DMABYTES 32     // 32 bytes per pixel byte (4 clocks per bit, 1 byte per clock)
#define WLEDPB_PARLIO_XFER_DONE_FLAG 3 // flag to indicate end of transfer, must NOT be a multiple of 4

// PARLIO bus context - manages shared PARLIO unit for parallel output
class ParlioBusContext {
public:
  static ParlioBusContext* get(uint8_t busNum);
  static void release(uint8_t busNum);

  bool init(const LedTiming& timing);
  void deinit();

  int8_t registerChannel(int8_t pin, size_t srcBytes, bool inverted = false);
  void unregisterChannel(int8_t channelIdx);
  uint8_t getChannelCount() const { return _channelCount; }

  bool startTransmit();
  bool isIdle() const { return _state == DriverState::Idle; }
  bool ensureIdle(); // non-blocking completion check with timeout recovery
  void waitForIdle(); // bounded wait; aborts a stalled transfer before returning
  void abortTransmit(); // watchdog recovery: stop unit and DMA, return to idle

  void setChannelData(int8_t channelIdx, const uint8_t* data, size_t len);

private:
  ParlioBusContext(uint8_t busNum);
  ~ParlioBusContext();

  void IRAM_ATTR fillBuffer(uint8_t bufIdx);
  bool _allocDmaBuffers(); // allocate/reallocate DMA buffers sized for the largest registered channel
  void IRAM_ATTR encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel); // 4-step cadence encoding, applies inversion via XOR
  uint32_t IRAM_ATTR _calcResetBytes() const; // bytes of reset period appended at the end of a frame (called from ISR context)

  static IRAM_ATTR bool txDoneCallback(parlio_tx_unit_handle_t tx_unit, const parlio_tx_done_event_data_t* edata, void* user_ctx);

#if WLEDPB_PARLIO_SEAMLESS_DMA
  bool _initDmaRing();                        // descriptor ring (once) + borrow the unit's GDMA channel + refill callback
  void IRAM_ATTR _armDescriptor(uint8_t idx); // hand a filled buffer's descriptor to the DMA
  void IRAM_ATTR _processChunkEof();          // PARLIO TX EOF: chain next byte-count chunk or finish frame
  static IRAM_ATTR bool gdmaEofCallback(gdma_channel_handle_t dma_chan, gdma_event_data_t* edata, void* user_ctx);
#else
  bool IRAM_ATTR _queueBuffer(uint8_t bufIdx); // queue the (single) full-frame buffer as one PARLIO transaction
#endif

  bool hwInit();          // creates the PARLIO TX unit with the currently registered pins
  void hwDeinit();
  bool hwStartTransfer(); // enables the unit and starts the frame
  void IRAM_ATTR hwStopTransfer();

  parlio_tx_unit_handle_t _txUnit;
  bool _unitEnabled;
  bool _unitStale;        // channels changed after unit creation, recreate on next transmit

  volatile DriverState _state;
  volatile uint32_t _txStartMillis; // millis() at frame start, used by the stall watchdog in show()/canShow()
  bool _initialized;

  uint8_t* _dmaBuffer[WLEDPB_PARLIO_DMA_BUFFER_COUNT];
  volatile size_t _txLen[WLEDPB_PARLIO_DMA_BUFFER_COUNT];        // bytes per buffer
  volatile bool _endOfFrame[WLEDPB_PARLIO_DMA_BUFFER_COUNT];     // last buffer of the frame
  size_t _bufferSize;      // actual allocated DMA buffer size (per buffer)
  size_t _maxSrcBytes;     // max source (encoded pixel) bytes across all registered channels; drives DMA sizing
  bool _dmaAllocated;
  volatile uint16_t _resetBytesLeft;

#if WLEDPB_PARLIO_SEAMLESS_DMA
  // GDMA channel borrowed from the PARLIO unit (owned and deleted by the IDF driver) + our circular descriptor ring
  gdma_channel_handle_t _dmaChan;
  int8_t _dmaChanIdx; // hardware channel index of _dmaChan for direct LL register access (-1 = unknown)
  dma_descriptor_t* _desc;
  volatile uint8_t _fillHead;        // ring index of the next descriptor to refill
  volatile uint32_t _chunkBytesLeft; // frame bytes not yet covered by a programmed tx_bytelen chunk
#endif

  LedTiming _timing;
  uint32_t _outClockHz;    // PARLIO output clock = 4 clocks per LED bit

  struct ChannelData {
    int8_t pin;
    const uint8_t* srcData;
    size_t srcLen;
    size_t srcPos;
    bool active;
  };
  ChannelData _channels[WLEDPB_PARLIO_MAX_CHANNELS];
  uint8_t _channelCount;
  uint16_t _channelMask;
  uint16_t _stagedMask;
  uint8_t _invertMask;     // bit per channel: XOR mask for inverted outputs
  size_t _maxDataLen;

  static ParlioBusContext* _instances[WLEDPB_PARLIO_BUS_COUNT];
  static uint8_t _refCount[WLEDPB_PARLIO_BUS_COUNT];
};

// PARLIO parallel output bus

class ParlioBus : public PixelBus {
public:
  ParlioBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t busNum = 0, uint8_t ledType = 0, size_t numPixels = 0);
  ~ParlioBus() override;

  bool begin() override;
  void end() override;

  bool show() override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "PARLIO"; }
#endif

private:
  int8_t _pin;
  LedTiming _timing;
  bool _initialized;
  uint8_t _busNum;
  int8_t _channelIdx;
  ParlioBusContext* _ctx;
};

} // namespace WLEDpixelBus
#endif // WLEDPB_PARLIO_SUPPORT