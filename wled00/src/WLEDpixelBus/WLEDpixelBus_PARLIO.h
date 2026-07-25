// WLEDpixelBus_PARLIO.h
/*-------------------------------------------------------------------------

WLEDpixelBus - parallel PARLIO output driver implementation

written by Damian Schneider @dedehai 2026

supports ESP32-C6, ESP32-H2 and ESP32-C5 via the PARLIO TX peripheral (8 parallel outputs).
ESP32-P4 has a 16-bit wide PARLIO unit, this driver uses 8 bits of it (could be extended).
Requires IDF >= 5.3 (driver/parlio_tx.h). Not available on ESP32, S2, S3, C3, C2.

Architecture mirrors the I2S driver (WLEDpixelBus_I2S): data is streamed from a circular
GDMA descriptor ring that we own, refilled on the fly from per-descriptor EOF interrupts.

Why not the IDF driver's transaction queue (parlio_tx_unit_transmit):
each queued transaction is ended by the driver's EOF ISR with a full pipeline restart -
output clock off, TX engine off, FIFO reset, descriptor remount, GDMA restart, busy-wait.
During that whole sequence the outputs sit at the idle level (LOW); under load this takes
up to ~100us, which is long enough to latch addressable LEDs mid-frame.

Seamless streaming model (WLEDPB_PARLIO_SEAMLESS_DMA, used on C6/H2/C5):
- One hardware transaction per frame. The frame length (data + reset period) is programmed
  into the PARLIO TX byte counter (tx_bytelen); the hardware clocks out exactly that many
  bytes and raises TX EOF wire-accurately at the end of the frame. No descriptor remount,
  no FIFO reset and no TX engine stop ever happens mid-frame -> no output gaps.
- The TX unit's GDMA channel (borrowed via WLEDPB_PARLIO_TX_DMA_CHAN) streams a circular
  descriptor ring (owner check + auto write-back, applied by the driver). Every
  descriptor has suc_eof set, which is safe here because TX EOF on these chips is derived
  from the byte counter, not from DMA EOF. Each completed descriptor raises a GDMA EOF
  interrupt that refills and re-arms it.
- The 16-bit tx_bytelen register limits one hardware transaction to 65535 bytes
  (~680 RGB pixels). Larger frames are chained at TX EOF by simply reprogramming the byte
  counter and re-starting TX: a few register writes (~1-2us at idle level, no FIFO reset,
  no GDMA restart), far below the LED reset/latch threshold.
- End of frame = final TX EOF. The IDF driver's EOF ISR (which we still use) stops the TX
  engine and clock; the lines park at idle_value = _invertMask, i.e. the correct reset
  level (HIGH for inverted buses - the old transaction model parked them LOW).

The IDF driver is still used for unit creation (clock setup, GPIO matrix routing, interrupt
install) and for its EOF ISR, which invokes our on_trans_done callback. We never call
parlio_tx_unit_transmit().

Fallback (WLEDPB_PARLIO_SEAMLESS_DMA == 0, e.g. ESP32-P4 with SOC_PARLIO_TX_SIZE_BY_DMA
or non-AHB GDMA): the original transaction-queue model, one parlio_tx_unit_transmit()
per DMA buffer, re-queued from the on_trans_done callback.

Data is output in 4-step cadence meaning each LED bit is encoded into 4 PARLIO clocks.
'0' is 0b1000 and '1' is 0b1110 (one byte across the 8 data lines per clock).
Encoding is done "on the fly" in the refill path while queued buffers are sent by DMA.
Signal inversion is done in the encoder via a per-channel XOR mask (PARLIO has no hardware
inversion), replicating the GPIO-matrix inversion semantics of the I2S driver:
inverted channels idle HIGH and their reset period is HIGH.

Each bus can have individual configuration of color channels but all must share the same timing.

NOTE: pins are fixed at PARLIO unit creation (unlike I2S where pins are routed lazily via the
GPIO matrix). The unit is therefore created lazily on first startTransmit() and recreated if
channels change afterwards.

NOTE: the seamless path is flash-safe WITHOUT any special sdkconfig options, so it works
on precompiled frameworks (Arduino/Tasmota) where sdkconfig cannot be changed:
- the IDF GDMA TX ISR is linked into IRAM unconditionally (linkerscript, all configs)
- the IDF PARLIO TX ISR is IRAM_ATTR unconditionally
- everything our callbacks call is IRAM_ATTR or always-inline LL code: the DMA outlink
  restart uses a direct gdma_ll_tx_restart() register write (indexed via the channel ID
  obtained from gdma_get_channel_id() at init), not gdma_append() which may live in flash
- pixel data and descriptors live in internal SRAM
If a flash write (e.g. saving settings to NVS) overlaps a frame WITHOUT
CONFIG_GDMA_ISR_IRAM_SAFE=y / CONFIG_PARLIO_ISR_IRAM_SAFE=y, the EOF interrupts are
simply deferred until the flash cache is re-enabled: refills/chunk re-arms run late but
nothing crashes. With those options set (pure-IDF projects) the frame is serviced even
during the flash write. A very long cache-off window can still starve the refill and
latch the LEDs mid-frame - unavoidable on any driver.

NOTE: the PARLIO register struct instance is `PARL_IO` on C6 and H2 (soc/parl_io_struct.h).
If a future target names it differently, adjust the PARLIO_HW macro below.

-------------------------------------------------------------------------*/

#pragma once

#include "WLEDpixelBus.h"
#include <esp_idf_version.h>
#include "soc/soc_caps.h"

#if defined(SOC_PARLIO_SUPPORTED) && SOC_PARLIO_SUPPORTED
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    #define WLEDPB_PARLIO_SUPPORT 1
  #endif
#endif

#ifdef WLEDPB_PARLIO_SUPPORT

#include "driver/parlio_tx.h"
#include "driver/gpio.h"

// Seamless single-transaction streaming is possible when the chip has AHB GDMA and the
// PARLIO TX end-of-frame is derived from the byte counter (not from DMA EOF). Limited to
// IDF 5.x: the driver's internals changed on 6.x (struct layout, driver-owned GDMA
// callbacks), the fallback below is used there until verified.
#if defined(SOC_AHB_GDMA_SUPPORTED) && SOC_AHB_GDMA_SUPPORTED && \
    !(defined(SOC_PARLIO_TX_SIZE_BY_DMA) && SOC_PARLIO_TX_SIZE_BY_DMA) && \
    ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(6, 0, 0)
  #define WLEDPB_PARLIO_SEAMLESS_DMA 1
#else
  #define WLEDPB_PARLIO_SEAMLESS_DMA 0
#endif

#if WLEDPB_PARLIO_SEAMLESS_DMA
  #include "hal/dma_types.h"     // dma_descriptor_t, DMA_DESCRIPTOR_BUFFER_*
  #include "esp_private/gdma.h"  // gdma_start/stop/append, gdma_register_tx_event_callbacks

// Mirror of the head of the IDF driver's private struct parlio_tx_unit_t (parlio_tx.c,
// parlio_priv.h since v5.5). Lets us reach the TX unit's GDMA channel: that channel is
// already connected to the PARLIO trigger by the driver and has the owner-check +
// auto-write-back strategy applied - and the driver never registers GDMA callbacks on
// it (v5.3-v5.5), so we can use it for our descriptor ring instead of allocating a
// second channel. Note: GDMA allows only ONE channel connected to a peripheral trigger;
// a second gdma_connect() to PARLIO fails with ESP_ERR_INVALID_STATE (and the driver's
// own connect failing instead would break parlio_del_tx_unit's cleanup path).
// Layout verified identical for IDF v5.3, v5.4, v5.5. Only fields up to dma_chan must
// match; if a future IDF changes this, the handle reads wrong and GDMA calls fail
// safely (no output) - update the mirror then.
typedef struct {
  int unit_id;                  // parlio_unit_t.unit_id
  int dir;                      // parlio_unit_t.dir (parlio_dir_t)
  void* group;                  // parlio_unit_t.group
  size_t data_width;
  void* intr;                   // intr_handle_t
  void* pm_lock;                // esp_pm_lock_handle_t
  gdma_channel_handle_t dma_chan;
} WledpbParlioTxUnitHead;
#define WLEDPB_PARLIO_TX_DMA_CHAN(unit) (((const WledpbParlioTxUnitHead*)(unit))->dma_chan)
#endif

namespace WLEDpixelBus {

//==============================================================================
// PARLIO Parallel Bus - ESP32-C6, ESP32-H2, ESP32-C5 (8 lines), ESP32-P4 (subset)
//==============================================================================

// only one PARLIO TX unit per chip
#define WLEDPB_PARLIO_BUS_COUNT 1

// DMA buffer count. In seamless mode each buffer is one descriptor in the ring; refill
// happens from the GDMA EOF interrupt (LOWMED priority), so 3-4 buffers of a few KB give
// ample refill slack (one buffer duration is ~1ms per 4KB at 3.2MHz output clock).
#ifndef WLEDPB_PARLIO_DMA_BUFFER_COUNT
  #define WLEDPB_PARLIO_DMA_BUFFER_COUNT 3
#endif

// PARLIO TX on C6/H2/C5 supports up to 8 data lines (SOC_PARLIO_TX_UNIT_MAX_DATA_WIDTH)
#define WLEDPB_PARLIO_MAX_CHANNELS 8
#define WLEDPB_PARLIO_DMABYTES 32     // 32 bytes per pixel byte (4 clocks per bit, 1 byte per clock)
#define WLEDPB_PARLIO_XFER_DONE_FLAG 3 // flag to indicate end of transfer, must NOT be a multiple of 4

class ParlioBus;

/**
 * PARLIO bus context - manages the shared PARLIO TX unit for parallel output
 * Seamless mode: one transaction per frame over our own GDMA descriptor ring.
 * Fallback mode: IDF transaction queue with callback-driven buffer refill.
 */
class ParlioBusContext {
public:
  static ParlioBusContext* get(uint8_t busNum);
  static void release(uint8_t busNum);

  bool init(const LedTiming& timing);
  void deinit();

  // Channel management
  int8_t registerChannel(int8_t pin, ParlioBus* bus, size_t srcBytes, bool inverted = false);
  void unregisterChannel(int8_t channelIdx);
  uint8_t getChannelCount() const { return _channelCount; }

  // Transmission
  bool startTransmit();
  bool isIdle() const { return _state == DriverState::Idle; }

  // Data access for channels
  void setChannelData(int8_t channelIdx, const uint8_t* data, size_t len);

private:
  ParlioBusContext(uint8_t busNum);
  ~ParlioBusContext();

  void IRAM_ATTR fillBuffer(uint8_t bufIdx);
  bool _allocDmaBuffers(); // allocate/reallocate DMA buffers sized for the largest registered channel
  void IRAM_ATTR encode4Step(uint8_t* dest, size_t destLen, uint8_t maxChannel); // 4-step cadence encoding, applies inversion via XOR
  uint32_t IRAM_ATTR _calcResetBytes() const; // bytes of reset period appended at the end of a frame (called from ISR context)

  static IRAM_ATTR bool dmaCallback(parlio_tx_unit_handle_t tx_unit, const parlio_tx_done_event_data_t* edata, void* user_ctx);

#if WLEDPB_PARLIO_SEAMLESS_DMA
  bool _initDmaRing();                        // descriptor ring (once) + borrow the unit's GDMA channel + refill callback
  void IRAM_ATTR _armDescriptor(uint8_t idx); // hand a filled buffer's descriptor to the DMA
  void IRAM_ATTR _processChunkEof();          // PARLIO TX EOF: chain next byte-count chunk or finish frame
  static IRAM_ATTR bool gdmaEofCallback(gdma_channel_handle_t dma_chan, gdma_event_data_t* edata, void* user_ctx);
#else
  void IRAM_ATTR _processEof();
  bool IRAM_ATTR _queueBuffer(uint8_t bufIdx); // queue one buffer as a PARLIO transaction
#endif

  // Hardware abstraction
  bool hwInit();        // creates the PARLIO TX unit with the currently registered pins
  void hwDeinit();
  bool hwStartTransfer(); // enables the unit and starts the frame
  void IRAM_ATTR hwStopTransfer();

  parlio_tx_unit_handle_t _txUnit;
  bool _unitEnabled;
  bool _unitStale;        // channels changed after unit creation, recreate on next transmit

  volatile DriverState _state;
  bool _initialized;

  // DMA buffers
  uint8_t* _dmaBuffer[WLEDPB_PARLIO_DMA_BUFFER_COUNT];
  volatile size_t _txLen[WLEDPB_PARLIO_DMA_BUFFER_COUNT];        // bytes per buffer
  volatile bool _endOfFrame[WLEDPB_PARLIO_DMA_BUFFER_COUNT];     // last buffer of the frame
  size_t _bufferSize;      // actual allocated DMA buffer size (per buffer)
  size_t _maxSrcBytes;     // max source (encoded pixel) bytes across all registered channels; drives DMA sizing
  bool _dmaAllocated;
  volatile uint16_t _resetBytesLeft;

#if WLEDPB_PARLIO_SEAMLESS_DMA
  // GDMA channel borrowed from the PARLIO unit (owned and deleted by the IDF driver)
  // + our circular descriptor ring
  gdma_channel_handle_t _dmaChan;
  int8_t _dmaChanIdx; // hardware channel index of _dmaChan for direct LL register access (-1 = unknown)
  dma_descriptor_t* _desc;
  volatile uint8_t _fillHead;        // ring index of the next descriptor to refill
  volatile uint32_t _chunkBytesLeft; // frame bytes not yet covered by a programmed tx_bytelen chunk
#else
  volatile uint8_t _activeBuffer;
#endif

  // Timing
  LedTiming _timing;
  uint32_t _outClockHz;    // PARLIO output clock = 4 clocks per LED bit

  // Channel data
  struct ChannelData {
    ParlioBus* bus;
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

  // Singleton instances
  static ParlioBusContext* _instances[WLEDPB_PARLIO_BUS_COUNT];
  static uint8_t _refCount[WLEDPB_PARLIO_BUS_COUNT];
};

/**
 * PARLIO parallel output bus
 */
class ParlioBus : public PixelBus {
public:
  /**
   * Create PARLIO bus
   * @param pin GPIO pin
   * @param timing LED timing
   * @param colorOrder Color order
   * @param numChannels Bytes per pixel
   * @param busNum PARLIO bus number (only bus 0 exists, one PARLIO TX unit per chip)
   * @param ledType LED chip type constant
   * @param numPixels Number of pixels; stored for DMA buffer sizing in ParlioBusContext
   */
  ParlioBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t busNum = 0, uint8_t ledType = 0, size_t numPixels = 0);
  ~ParlioBus() override;

  bool begin() override;
  void end() override;

  bool show(const uint32_t* pixels, uint16_t numPixels, const CctPixel* cct = nullptr) override;
  bool canShow() const override;
#ifdef WLED_DEBUG_BUS
  const char* getTypeStr() const override { return "PARLIO"; }
#endif

  void setInverted(bool inv) override;
  void setColorOrder(uint8_t co);

  // Override to use DMA-capable allocator for PARLIO
  bool allocateEncodeBuffer(uint16_t numPixels, uint8_t numChannels) override;

private:
  int8_t _pin;
  LedTiming _timing;
  bool _inverted;
  bool _initialized;
  uint8_t _busNum;
  int8_t _channelIdx;
  ParlioBusContext* _ctx;
};

} // namespace WLEDpixelBus
#endif // WLEDPB_PARLIO_SUPPORT