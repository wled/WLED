/*-------------------------------------------------------------------------
WLEDpixelBus - high priority RMT interrupts for ESP32
by @willmmiles, 2026
IDF V5 support: rmt_ll compatibility layer, no legacy driver dependency
-------------------------------------------------------------------------*/

#include <Arduino.h>
#include "RmtHIDriver.h"

#if defined(WLEDPB_USE_RMTHI)

#include <algorithm>
#include "soc/soc.h"
#include "soc/rmt_reg.h"
#include "esp_idf_version.h"

#ifdef __riscv
#include "riscv/interrupt.h"
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// ---- IDF V5 compatibility layer ----
// The legacy RMT driver (driver/rmt.h) is deprecated on IDF5; the few legacy types, constants and rmt_ll helpers this driver relies on are re-declared here
#include "soc/rmt_struct.h"
#include "hal/rmt_ll.h"
#include "hal/rmt_types.h"
#include "esp_intr_alloc.h"

#define RMT_CHANNEL_MAX  SOC_RMT_CHANNELS_PER_GROUP
#define RMT_MEM_ITEM_NUM SOC_RMT_MEM_WORDS_PER_CHANNEL

typedef rmt_symbol_word_t rmt_item32_t; // same 32-bit layout as the legacy rmt_item32_t

// RMTMEM: the channel memory view struct moved to the private RMT driver headers in IDF5, but the symbol is still provided by the linker (<target>.peripherals.ld).
extern "C" volatile rmt_item32_t RMTMEM[RMT_CHANNEL_MAX * RMT_MEM_ITEM_NUM];

#define RMTMEM_WORD(chan, idx) RMTMEM[(chan) * RMT_MEM_ITEM_NUM + (idx)]

// the per-event interrupt helpers were removed in IDF5, map them to the generic mask-based functions
static inline void rmt_ll_enable_tx_end_interrupt(rmt_dev_t *dev, uint32_t channel, bool enable)   { rmt_ll_enable_interrupt(dev, RMT_LL_EVENT_TX_DONE(channel), enable); }
static inline void rmt_ll_enable_tx_err_interrupt(rmt_dev_t *dev, uint32_t channel, bool enable)   { rmt_ll_enable_interrupt(dev, RMT_LL_EVENT_TX_ERROR(channel), enable); }
static inline void rmt_ll_enable_tx_thres_interrupt(rmt_dev_t *dev, uint32_t channel, bool enable) { rmt_ll_enable_interrupt(dev, RMT_LL_EVENT_TX_THRES(channel), enable); }
static inline void rmt_ll_clear_tx_end_interrupt(rmt_dev_t *dev, uint32_t channel)   { rmt_ll_clear_interrupt_status(dev, RMT_LL_EVENT_TX_DONE(channel)); }
static inline void rmt_ll_clear_tx_err_interrupt(rmt_dev_t *dev, uint32_t channel)   { rmt_ll_clear_interrupt_status(dev, RMT_LL_EVENT_TX_ERROR(channel)); }
static inline void rmt_ll_clear_tx_thres_interrupt(rmt_dev_t *dev, uint32_t channel) { rmt_ll_clear_interrupt_status(dev, RMT_LL_EVENT_TX_THRES(channel)); }

// renamed in IDF5
static inline void rmt_ll_tx_enable_pingpong(rmt_dev_t *dev, uint32_t channel, bool enable) { rmt_ll_tx_enable_wrap(dev, channel, enable); }

// rmt_ll_tx_stop() is missing in the esp32 (classic) IDF5 hal
#if defined(CONFIG_IDF_TARGET_ESP32)
static inline void rmt_ll_tx_stop(rmt_dev_t *dev, uint32_t channel)
{
  RMTMEM_WORD(channel, 0).val = 0; // append end marker, stops the transmitter
  dev->conf_ch[channel].conf1.tx_start = 0;
  dev->conf_ch[channel].conf1.mem_rd_rst = 1;
  dev->conf_ch[channel].conf1.mem_rd_rst = 0;
}
#endif

// channel status word: holds the RMT_STATE_CHx transmitter state field (see soc/rmt_reg.h)
__attribute__((always_inline))
static inline uint32_t rmt_ll_tx_get_channel_status(rmt_dev_t *dev, uint32_t channel) {
#if defined(CONFIG_IDF_TARGET_ESP32)
  return dev->status_ch[channel];
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  return dev->tx_status[channel].state;
#elif defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
  // C5, C6 and H2 share the same RMT register layout; only the state field may be returned here
  // (the RISC-V _RmtStatusIsTransmitting() tests status != 0, and chnstatus.val also contains
  // the memory address counters, which stay non-zero when idle)
  return dev->chnstatus[channel].state_chn;
#else // S2/S3: the state field lives in the chnstatus register
  return dev->chnstatus[channel].val;
#endif
}

#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#include "hal/rmt_ll.h"
#define RMTMEM_WORD(chan, idx) RMTMEM.chan[chan].data32[idx]
#else
/* Shims for older ESP-IDF v3; we can safely assume original ESP32 */
#include "soc/rmt_struct.h"

#define RMTMEM_WORD(chan, idx) RMTMEM.chan[chan].data32[idx]

// Selected RMT API functions borrowed from ESP-IDF v4.4.8
// components/hal/esp32/include/hal/rmt_ll.h
// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

__attribute__((always_inline))
static inline void rmt_ll_tx_reset_pointer(rmt_dev_t *dev, uint32_t channel)
{
  dev->conf_ch[channel].conf1.mem_rd_rst = 1;
  dev->conf_ch[channel].conf1.mem_rd_rst = 0;
}

__attribute__((always_inline))
static inline void rmt_ll_tx_start(rmt_dev_t *dev, uint32_t channel)
{
  dev->conf_ch[channel].conf1.tx_start = 1;
}

__attribute__((always_inline))
static inline void rmt_ll_tx_stop(rmt_dev_t *dev, uint32_t channel)
{
  RMTMEM.chan[channel].data32[0].val = 0;
  dev->conf_ch[channel].conf1.tx_start = 0;
  dev->conf_ch[channel].conf1.mem_rd_rst = 1;
  dev->conf_ch[channel].conf1.mem_rd_rst = 0;
}

__attribute__((always_inline))
static inline void rmt_ll_tx_enable_pingpong(rmt_dev_t *dev, uint32_t channel, bool enable)
{
  dev->apb_conf.mem_tx_wrap_en = enable;
}

__attribute__((always_inline))
static inline void rmt_ll_tx_enable_loop(rmt_dev_t *dev, uint32_t channel, bool enable)
{
  dev->conf_ch[channel].conf1.tx_conti_mode = enable;
}

__attribute__((always_inline))
static inline uint32_t rmt_ll_tx_get_channel_status(rmt_dev_t *dev, uint32_t channel)
{
  return dev->status_ch[channel];
}

__attribute__((always_inline))
static inline void rmt_ll_tx_set_limit(rmt_dev_t *dev, uint32_t channel, uint32_t limit)
{
  dev->tx_lim_ch[channel].limit = limit;
}

__attribute__((always_inline))
static inline void rmt_ll_enable_interrupt(rmt_dev_t *dev, uint32_t mask, bool enable)
{
  if (enable) {
    dev->int_ena.val |= mask;
  } else {
    dev->int_ena.val &= ~mask;
  }
}

__attribute__((always_inline))
static inline void rmt_ll_enable_tx_end_interrupt(rmt_dev_t *dev, uint32_t channel, bool enable)
{
  dev->int_ena.val &= ~(1 << (channel * 3));
  dev->int_ena.val |= (enable << (channel * 3));
}

__attribute__((always_inline))
static inline void rmt_ll_enable_tx_err_interrupt(rmt_dev_t *dev, uint32_t channel, bool enable)
{
  dev->int_ena.val &= ~(1 << (channel * 3 + 2));
  dev->int_ena.val |= (enable << (channel * 3 + 2));
}

__attribute__((always_inline))
static inline void rmt_ll_enable_tx_thres_interrupt(rmt_dev_t *dev, uint32_t channel, bool enable)
{
  dev->int_ena.val &= ~(1 << (channel + 24));
  dev->int_ena.val |= (enable << (channel + 24));
}

__attribute__((always_inline))
static inline void rmt_ll_clear_tx_end_interrupt(rmt_dev_t *dev, uint32_t channel)
{
  dev->int_clr.val = (1 << (channel * 3));
}

__attribute__((always_inline))
static inline void rmt_ll_clear_tx_err_interrupt(rmt_dev_t *dev, uint32_t channel)
{
  dev->int_clr.val = (1 << (channel * 3 + 2));
}

__attribute__((always_inline))
static inline void rmt_ll_clear_tx_thres_interrupt(rmt_dev_t *dev, uint32_t channel)
{
  dev->int_clr.val = (1 << (channel + 24));
}

__attribute__((always_inline))
static inline uint32_t rmt_ll_get_tx_thres_interrupt_status(rmt_dev_t *dev)
{
  uint32_t status =  dev->int_st.val;
  return (status & 0xFF000000) >> 24; // TODO: this is probably wrong for C5 and C6, needs investigation
}

__attribute__((always_inline))
static inline void rmt_ll_tx_set_mem_blocks(rmt_dev_t *dev, uint32_t channel, uint8_t block_num)
{
    dev->conf_ch[channel].conf0.mem_size = block_num;
}

#endif


// *********************************
// Select method for binding interrupt
//
// - If the Bluetooth driver has registered a high-level interrupt, piggyback on that API
// - If we're on a modern core, allocate the interrupt with the API (old cores are bugged)
// - Otherwise use the low-level hardware API to manually bind the interrupt


#if defined(CONFIG_BTDM_CTRL_HLI) && !defined(__riscv)
// Espressif's bluetooth driver offers a helpful sharing layer; bring in the interrupt management calls
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_cpu.h"      // esp_cpu_get_core_id()
#include "rom/ets_sys.h"  // intr_matrix_set()
#else
#include "hal/interrupt_controller_hal.h"
#endif
extern "C" esp_err_t hli_intr_register(intr_handler_t handler, void* arg, uint32_t intr_reg, uint32_t intr_mask);

#else /* !CONFIG_BTDM_CTRL_HLI*/

// Declare the our high-priority ISR handler
extern "C" void ld_include_hli_vectors_rmt();   // an object with an address, but no space

#if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
#include "soc/periph_defs.h"
#endif

// Select level flag
#if defined(__riscv)
// RISCV chips don't block interrupts while scheduling; all we need to do is be higher than the WiFi ISR
#define INT_LEVEL_FLAG ESP_INTR_FLAG_LEVEL3 // on RISC-V, higher-level interrupts are not RTOS-safe  TODO: may need revisiting
#elif defined(CONFIG_ESP_SYSTEM_CHECK_INT_LEVEL_5)
#define INT_LEVEL_FLAG ESP_INTR_FLAG_LEVEL4
#else
#define INT_LEVEL_FLAG ESP_INTR_FLAG_LEVEL5
#endif

// ESP-IDF v3 cannot enable high priority interrupts through the API at all;
// and ESP-IDF v4 on XTensa cannot enable Level 5 due to incorrect interrupt descriptor tables
#if !defined(__XTENSA__) || (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)) || ((ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0) && CONFIG_ESP_SYSTEM_CHECK_INT_LEVEL_5))
#define NEOESP32_RMT_CAN_USE_INTR_ALLOC

// XTensa cores require the assembly bridge
#ifdef __XTENSA__
#define HI_IRQ_HANDLER nullptr
#define HI_IRQ_HANDLER_ARG ld_include_hli_vectors_rmt
#else
#define HI_IRQ_HANDLER NeoEsp32RmtMethodIsr
#define HI_IRQ_HANDLER_ARG nullptr
#endif

#else
/* !CONFIG_BTDM_CTRL_HLI && !NEOESP32_RMT_CAN_USE_INTR_ALLOC */
// This is the index of the LV5 interrupt vector - see interrupt descriptor table in idf components/hal/esp32/interrupt_descriptor_table.c
#define ESP32_LV5_IRQ_INDEX 26

#endif  /* NEOESP32_RMT_CAN_USE_INTR_ALLOC */
#endif  /* CONFIG_BTDM_CTRL_HLI */


// RMT driver implementation
struct NeoEsp32RmtHIChannelState {
  uint32_t rmtBit0, rmtBit1;
  uint32_t resetDuration;

  const byte* txDataStart;    // data array
  const byte* txDataEnd;      // one past end
  const byte* txDataCurrent;  // current location
  size_t rmtOffset;
  size_t batchSize;           // memory blocks assigned
};

// Global variables
#if defined(NEOESP32_RMT_CAN_USE_INTR_ALLOC)
static intr_handle_t isrHandle = nullptr;
#endif

static NeoEsp32RmtHIChannelState** driverState = nullptr;

// Fill the RMT buffer memory
// This is implemented using many arguments instead of passing the structure object to ensure we do only one lookup
// All the arguments are passed in registers, so they don't need to be looked up again
static void IRAM_ATTR RmtFillBuffer(uint8_t channel, const byte** src_ptr, const byte* end, uint32_t bit0, uint32_t bit1, size_t* offset_ptr, size_t reserve, size_t batchSize) {
  // We assume that (rmtToWrite % 8) == 0
  size_t rmtToWrite = batchSize - reserve;
  rmt_item32_t* dest = (rmt_item32_t*) &RMTMEM_WORD(channel, *offset_ptr + reserve); // write directly in to RMT memory
  const byte* psrc = *src_ptr;

  *offset_ptr ^= batchSize;

  if (psrc != end) {
    while (rmtToWrite > 0) {
      uint8_t data = *psrc;
      // unrolled loop for speed: do not depend on compiler to do it for us
      dest[0].val = (data & 0x80) ? bit1 : bit0;
      dest[1].val = (data & 0x40) ? bit1 : bit0;
      dest[2].val = (data & 0x20) ? bit1 : bit0;
      dest[3].val = (data & 0x10) ? bit1 : bit0;
      dest[4].val = (data & 0x08) ? bit1 : bit0;
      dest[5].val = (data & 0x04) ? bit1 : bit0;
      dest[6].val = (data & 0x02) ? bit1 : bit0;
      dest[7].val = (data & 0x01) ? bit1 : bit0;
      dest += 8; // Advance destination pointer by 8 items at once
      rmtToWrite -= 8;
      psrc++;
      if (psrc == end) {
        break;
      }
    }

    *src_ptr = psrc;
  }

  if (rmtToWrite > 0) {
    // Add end event (zero durations = EOF marker); the output keeps the level of bit0's second phase (low)
    // note: the bitfield types differ between IDF4 (uint32_t) and IDF5 (uint16_t)
    rmt_item32_t bit0Word;
    bit0Word.val = bit0;
    rmt_item32_t endMark = {};
    endMark.duration0 = 0;
    endMark.level0 = bit0Word.level1;
    endMark.duration1 = 0;
    endMark.level1 = bit0Word.level1;
    *dest = endMark;
  }
}

static void IRAM_ATTR RmtStartWrite(uint8_t channel, NeoEsp32RmtHIChannelState& state) {

  // Reset context state
  state.rmtOffset = 0;

  // Fill the first part of the buffer with a reset event
  // FUTURE: we could do timing analysis with the last interrupt on this channel
  // Use 8 words to stay aligned with the buffer fill logic
  // note: the bitfield types differ between IDF4 (uint32_t) and IDF5 (uint16_t)

  rmt_item32_t bit0Word;
  bit0Word.val = state.rmtBit0;
  rmt_item32_t fill = {};
  fill.duration0 = 100;
  fill.level0 = bit0Word.level1;
  fill.duration1 = 100;
  fill.level1 = bit0Word.level1;
  rmt_item32_t* dest = (rmt_item32_t*) &RMTMEM_WORD(channel, 0);
  for (auto i = 0; i < 7; ++i) dest[i] = fill;
  fill.duration1 = state.resetDuration > 1400 ? (state.resetDuration - 1400) : 100;
  dest[7] = fill;

  // Fill the remaining buffer with real data
  RmtFillBuffer(channel, &state.txDataCurrent, state.txDataEnd, state.rmtBit0, state.rmtBit1, &state.rmtOffset, 8, state.batchSize);
  RmtFillBuffer(channel, &state.txDataCurrent, state.txDataEnd, state.rmtBit0, state.rmtBit1, &state.rmtOffset, 0, state.batchSize);

  // Start operation
  rmt_ll_clear_tx_thres_interrupt(&RMT, channel);
  #if defined(CONFIG_IDF_TARGET_ESP32C5)
  RMT.chnconf0[channel].tx_stop_chn = 0; // C5: tx_stop_chn has been observed to remain asserted after  rmt_ll_tx_stop(), preventing subsequent transmissions.
  #endif
  rmt_ll_tx_reset_pointer(&RMT, channel);
  rmt_ll_tx_start(&RMT, channel);

}

extern "C" void IRAM_ATTR NeoEsp32RmtMethodIsr(void *arg) {
  // Tx threshold interrupt
  uint32_t status = rmt_ll_get_tx_thres_interrupt_status(&RMT);
  while (status) {
    uint8_t channel = __builtin_ffs(status) - 1;
    if (driverState[channel]) {
      // Normal case
      NeoEsp32RmtHIChannelState& state = *driverState[channel];
      RmtFillBuffer(channel, &state.txDataCurrent, state.txDataEnd, state.rmtBit0, state.rmtBit1, &state.rmtOffset, 0, state.batchSize);
    } else {
      // Danger - another driver got invoked?
      rmt_ll_tx_stop(&RMT, channel);
    }
    rmt_ll_clear_tx_thres_interrupt(&RMT, channel);
    status = rmt_ll_get_tx_thres_interrupt_status(&RMT);
  }
};

// Wrapper around the register analysis defines
// For all currently supported chips, this is constant for all channels; but this is not true of *all* ESP32
static inline bool _RmtStatusIsTransmitting(uint8_t channel, uint32_t status) {
#if defined(__riscv)
  return status != 0;
#else
  uint32_t v;
  switch(channel) {
#ifdef RMT_STATE_CH0
    case 0: v = (status >> RMT_STATE_CH0_S) & RMT_STATE_CH0_V; break;
#endif
#ifdef RMT_STATE_CH1
    case 1: v = (status >> RMT_STATE_CH1_S) & RMT_STATE_CH1_V; break;
#endif
#ifdef RMT_STATE_CH2
    case 2: v = (status >> RMT_STATE_CH2_S) & RMT_STATE_CH2_V; break;
#endif
#ifdef RMT_STATE_CH3
    case 3: v = (status >> RMT_STATE_CH3_S) & RMT_STATE_CH3_V; break;
#endif
#ifdef RMT_STATE_CH4
    case 4: v = (status >> RMT_STATE_CH4_S) & RMT_STATE_CH4_V; break;
#endif
#ifdef RMT_STATE_CH5
    case 5: v = (status >> RMT_STATE_CH5_S) & RMT_STATE_CH5_V; break;
#endif
#ifdef RMT_STATE_CH6
    case 6: v = (status >> RMT_STATE_CH6_S) & RMT_STATE_CH6_V; break;
#endif
#ifdef RMT_STATE_CH7
    case 7: v = (status >> RMT_STATE_CH7_S) & RMT_STATE_CH7_V; break;
#endif
    default: v = 0;
  }

  return v != 0;
#endif
}


esp_err_t RmtHiDriver::Install(uint8_t channel, uint32_t rmtBit0, uint32_t rmtBit1, uint32_t reset, uint8_t blocksToUse) {
  // Validate channel number
  if (channel >= RMT_CHANNEL_MAX) {
  return ESP_ERR_INVALID_ARG;
  }

  esp_err_t err = ESP_OK;
  if (!driverState) {
  // First time init
  driverState = reinterpret_cast<NeoEsp32RmtHIChannelState**>(heap_caps_calloc(RMT_CHANNEL_MAX, sizeof(NeoEsp32RmtHIChannelState*), MALLOC_CAP_INTERNAL));
  if (!driverState) return ESP_ERR_NO_MEM;
  // Ensure all interrupts are cleared before binding
  RMT.int_ena.val = 0;
  RMT.int_clr.val = 0xFFFFFFFF;

  // Bind interrupt handler
#if defined(CONFIG_BTDM_CTRL_HLI)
  // Bluetooth driver has taken the empty high-priority interrupt.  Fortunately, it allows us to
  // hook up another handler.
  err = hli_intr_register(NeoEsp32RmtMethodIsr, nullptr, (uintptr_t) &RMT.int_st, 0xFF000000);
  // 25 is the magic number of the bluetooth ISR on ESP32 - see soc/soc.h.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  intr_matrix_set(esp_cpu_get_core_id(), ETS_RMT_INTR_SOURCE, 25);
  ESP_INTR_ENABLE(25);
#else
  intr_matrix_set(cpu_hal_get_core_id(), ETS_RMT_INTR_SOURCE, 25);
  intr_cntrl_ll_enable_interrupts(1<<25);
#endif
#elif defined(NEOESP32_RMT_CAN_USE_INTR_ALLOC)
  // Use the platform code to allocate the interrupt
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0) && defined(__XTENSA__)
  // IDF5 requires handler and arg to be NULL for high level (assembly vectored) interrupts;
  // reference the bridge's guard symbol so the linker keeps the .S file
  static volatile const void*  __attribute__((used)) pleaseLinkAssembly = (void*) ld_include_hli_vectors_rmt;
  err = esp_intr_alloc(ETS_RMT_INTR_SOURCE, INT_LEVEL_FLAG | ESP_INTR_FLAG_IRAM, nullptr, nullptr, &isrHandle);
#else
  // If we need the additional assembly bridge, we pass it as the "arg" to the IDF so it gets linked in
  err = esp_intr_alloc(ETS_RMT_INTR_SOURCE, INT_LEVEL_FLAG | ESP_INTR_FLAG_IRAM, HI_IRQ_HANDLER, (void*) HI_IRQ_HANDLER_ARG, &isrHandle);
#endif
  //err = ESP_ERR_NOT_FINISHED;
#else
  // Broken IDF API does not allow us to reserve the interrupt; do it manually
  static volatile const void*  __attribute__((used)) pleaseLinkAssembly = (void*) ld_include_hli_vectors_rmt;
  intr_matrix_set(xPortGetCoreID(), ETS_RMT_INTR_SOURCE, ESP32_LV5_IRQ_INDEX);
  ESP_INTR_ENABLE(ESP32_LV5_IRQ_INDEX);
#endif

  if (err != ESP_OK) {
    heap_caps_free(driverState);
    driverState = nullptr;
    return err;
    }
  }

  if (driverState[channel] != nullptr) {
  return ESP_ERR_INVALID_STATE;   // already in use
  }

  NeoEsp32RmtHIChannelState* state = reinterpret_cast<NeoEsp32RmtHIChannelState*>(heap_caps_calloc(1, sizeof(NeoEsp32RmtHIChannelState), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (state == nullptr) {
  return ESP_ERR_NO_MEM;
  }

  // Store timing information
  state->rmtBit0 = rmtBit0;
  state->rmtBit1 = rmtBit1;
  state->resetDuration = reset;

  // Calculate batch size based on number of blocks (64 items per block)
  state->batchSize = (RMT_MEM_ITEM_NUM * blocksToUse) / 2;

  // Initialize hardware
  rmt_ll_tx_stop(&RMT, channel);
  #if defined(CONFIG_IDF_TARGET_ESP32C5)
  RMT.chnconf0[channel].tx_stop_chn = 0; // The peripheral/channel reset has already made TX idle. Avoid leaving tx_stop_chn asserted on C5.
  #else
  //rmt_ll_tx_stop(&RMT, channel);
  #endif
  rmt_ll_tx_reset_pointer(&RMT, channel);
  rmt_ll_tx_set_mem_blocks(&RMT, channel, blocksToUse);
  rmt_ll_enable_tx_err_interrupt(&RMT, channel, false);
  rmt_ll_enable_tx_end_interrupt(&RMT, channel, false);
  rmt_ll_enable_tx_thres_interrupt(&RMT, channel, false);
  rmt_ll_clear_tx_err_interrupt(&RMT, channel);
  rmt_ll_clear_tx_end_interrupt(&RMT, channel);
  rmt_ll_clear_tx_thres_interrupt(&RMT, channel);
  rmt_ll_tx_enable_loop(&RMT, channel, false);
  rmt_ll_tx_enable_pingpong(&RMT, channel, true);
  rmt_ll_tx_set_limit(&RMT, channel, state->batchSize);
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  rmt_ll_tx_enable_carrier_modulation(&RMT, channel, false); // disable carrier modulation, its enabled by default in V5
  rmt_ll_tx_fix_idle_level(&RMT, channel, 0, true);          // just in case, set idle low (TODO: need to invert if inverted output)
#endif

  driverState[channel] = state;

  rmt_ll_enable_tx_thres_interrupt(&RMT, channel, true);

  return err;
}

esp_err_t  RmtHiDriver::Uninstall(uint8_t channel) {
  if ((channel >= RMT_CHANNEL_MAX) || !driverState || !driverState[channel]) return ESP_ERR_INVALID_ARG;

  NeoEsp32RmtHIChannelState* state = driverState[channel];

  WaitForTxDone(channel, 10000 / portTICK_PERIOD_MS);

  // Done or not, we're out of here
  rmt_ll_tx_stop(&RMT, channel);
  rmt_ll_enable_tx_thres_interrupt(&RMT, channel, false);
  driverState[channel] = nullptr;
  heap_caps_free(state);

#if !defined(CONFIG_BTDM_CTRL_HLI)  /* Cannot unbind from bluetooth ISR */
  // Turn off the driver ISR and release global state if none are left
  for (uint8_t channelIndex = 0; channelIndex < RMT_CHANNEL_MAX; ++channelIndex) {
  if (driverState[channelIndex]) return ESP_OK; // done
  }

#if defined(NEOESP32_RMT_CAN_USE_INTR_ALLOC)
  esp_intr_free(isrHandle);
#else
  ESP_INTR_DISABLE(ESP32_LV5_IRQ_INDEX);
#endif

  heap_caps_free(driverState);
  driverState = nullptr;
#endif /* !defined(CONFIG_BTDM_CTRL_HLI) */

  return ESP_OK;
}

esp_err_t  RmtHiDriver::Write(uint8_t channel, const uint8_t *src, size_t srcSize) {
  if ((channel >= RMT_CHANNEL_MAX) || !driverState || !driverState[channel]) return ESP_ERR_INVALID_ARG;

  NeoEsp32RmtHIChannelState& state = *driverState[channel];
  esp_err_t result = WaitForTxDone(channel, 10000 / portTICK_PERIOD_MS);

  if (result == ESP_OK) {
  state.txDataStart = src;
  state.txDataCurrent = src;
  state.txDataEnd = src + srcSize;
  RmtStartWrite(channel, state);
  }
  return result;
}

esp_err_t RmtHiDriver::WaitForTxDone(uint8_t channel, TickType_t wait_time) {
  if ((channel >= RMT_CHANNEL_MAX) || !driverState || !driverState[channel]) return ESP_ERR_INVALID_ARG;

  NeoEsp32RmtHIChannelState& state = *driverState[channel];
  // yield-wait until wait_time
  esp_err_t rv = ESP_OK;
  uint32_t status;
  while(1) {
  status = rmt_ll_tx_get_channel_status(&RMT, channel);
  if (!_RmtStatusIsTransmitting(channel, status)) break;
  if (wait_time == 0) { rv = ESP_ERR_TIMEOUT; break; };

  TickType_t sleep = std::min(wait_time, (TickType_t) 5);
  vTaskDelay(sleep);
  wait_time -= sleep;
  };

  return rv;
}

#endif
