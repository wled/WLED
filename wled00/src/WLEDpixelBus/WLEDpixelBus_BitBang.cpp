/*-------------------------------------------------------------------------
WLEDpixelBus_BitBang — Parallel bit-bang LED output driver for ESP32.
See WLEDpixelBus_BitBang.h for architecture notes.
-------------------------------------------------------------------------*/

#include "WLEDpixelBus_BitBang.h"

#if defined(ARDUINO_ARCH_ESP32)

#include "esp_clk.h"    // esp_clk_cpu_freq()

namespace WLEDpixelBus {

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------
int8_t       BitBangBus::s_pins[WLED_MAX_BB_CHANNELS] = {};
uint16_t     BitBangBus::s_numPixels[WLED_MAX_BB_CHANNELS] = {};
uint8_t*     BitBangBus::s_pixelData[WLED_MAX_BB_CHANNELS] = {};
uint8_t      BitBangBus::s_channelCount  = 0;
uint32_t     BitBangBus::s_allMask       = 0;
#ifdef ESP_HAS_HIGH_GPIO_BANK
uint32_t     BitBangBus::s_allMaskHigh   = 0;
#endif
uint8_t      BitBangBus::s_stagedCount   = 0;
portMUX_TYPE BitBangBus::s_mux           = portMUX_INITIALIZER_UNLOCKED;
uint32_t     BitBangBus::s_t0h           = 0;
uint32_t     BitBangBus::s_t1h           = 0;
uint32_t     BitBangBus::s_period        = 0;
uint32_t     BitBangBus::s_latchCycles   = 0;
uint8_t      BitBangBus::s_pixelBytes    = 0;

// ---------------------------------------------------------------------------
// getCycleCount() — read the CPU cycle counter.
// Must be IRAM_ATTR because it is called from outputParallel() which is IRAM.
// ---------------------------------------------------------------------------
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
    defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)
  // RISC-V: machine cycle counter CSR 0x7e2 (vendor extension, same as NeoPixelBus)
  static inline uint32_t IRAM_ATTR getCycleCount() {
    uint32_t ccount;
    __asm__ __volatile__("csrr %0, 0x7e2" : "=r"(ccount));
    return ccount;
  }
  static constexpr uint32_t LOOPTEST_CYCLES = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  // Xtensa LX7 — same rsr instruction, tighter pipeline
  static inline uint32_t IRAM_ATTR getCycleCount() {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount" : "=a"(ccount));
    return ccount;
  }
  static constexpr uint32_t LOOPTEST_CYCLES = 2;
#else
  // Xtensa LX6 — ESP32, ESP32-S2
  static inline uint32_t IRAM_ATTR getCycleCount() {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount" : "=a"(ccount));
    return ccount;
  }
  static constexpr uint32_t LOOPTEST_CYCLES = 4;
#endif

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
BitBangBus::BitBangBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder,
                       uint8_t numChannels, uint8_t ledType)
  : _pin(pin), _rawTiming(timing), _initialized(false)
{
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
}

BitBangBus::~BitBangBus() {
  end();
}

// ---------------------------------------------------------------------------
// begin()
// ---------------------------------------------------------------------------
bool BitBangBus::begin() {
  if (_initialized) return true;

  // GPIO pin range check using IDF macros from soc/soc_caps.h:
  // SOC_GPIO_PIN_COUNT — total GPIO count for this target (40/ESP32, 47/S2, 49/S3, 22/C3, ...).
  // SOC_GPIO_VALID_OUTPUT_GPIO_MASK — 64-bit bitmask of GPIOs usable as outputs
  //   (excludes input-only pins such as GPIO 34-39 on ESP32).
  // High bank (GPIO_OUT1) is needed for pins > 31 on ESP32/S2/S3.
  if (_pin < 0 || _pin >= SOC_GPIO_PIN_COUNT ||
      !((SOC_GPIO_VALID_OUTPUT_GPIO_MASK >> _pin) & 1ULL)) return false;

  // Configure GPIO as output, drive LOW (idle state for non-inverted LEDs)
  gpio_set_direction((gpio_num_t)_pin, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)_pin, 0);

  // Convert nanosecond timings to CPU cycles
  const uint32_t cpuMHz = esp_clk_cpu_freq() / 1000000u;

  // Subtract the while-loop test overhead (one extra iteration) to compensate
  // for the latency between the condition passing and the actual GPIO write.
  const uint32_t lo = LOOPTEST_CYCLES;

  uint32_t t0h    = (_rawTiming.t0h_ns  * cpuMHz) / 1000u;
  uint32_t t1h    = (_rawTiming.t1h_ns  * cpuMHz) / 1000u;
  t0h             = (t0h    > lo) ? t0h    - lo : 0u;
  t1h             = (t1h    > lo) ? t1h    - lo : 0u;
  uint32_t period = (_rawTiming.bitPeriod() * cpuMHz) / 1000u;
  period          = (period > lo) ? period - lo : 0u;
  const uint32_t latchCycles = (uint32_t)_rawTiming.reset_us * cpuMHz;

  // Register in the shared static table
  if (s_channelCount >= WLED_MAX_BB_CHANNELS) return false;
  const uint8_t idx  = s_channelCount;
  s_pins[idx]        = _pin;
#ifdef ESP_HAS_HIGH_GPIO_BANK
  if (_pin >= 32) s_allMaskHigh |= (1u << (_pin - 32));
  else            s_allMask     |= (1u << _pin);
#else
  s_allMask         |= (1u << _pin);
#endif
  s_channelCount++;

  // Allocate the per-pixel encode buffer (via PixelBus helper)
  if (!allocateEncodeBuffer(_numPixels, _encoder.getPixelBytes())) {
    s_channelCount--;
#ifdef ESP_HAS_HIGH_GPIO_BANK
    if (_pin >= 32) s_allMaskHigh &= ~(1u << (_pin - 32));
    else            s_allMask     &= ~(1u << _pin);
#else
    s_allMask &= ~(1u << _pin);
#endif
    return false;
  }

  // Publish per-channel data pointers into the shared static arrays.
  // Timing is set here (same for all channels; overwriting with same values is harmless).
  s_numPixels[idx]  = _numPixels;
  s_pixelData[idx]  = _pixelData;
  s_t0h             = t0h;
  s_t1h             = t1h;
  s_period          = period;
  s_latchCycles     = latchCycles;
  s_pixelBytes      = _encoder.getPixelBytes();

  _initialized = true;
  return true;
}

// ---------------------------------------------------------------------------
// end()
// ---------------------------------------------------------------------------
void BitBangBus::end() {
  if (_initialized) {
    // Find our slot by scanning s_pins (no stored index needed).
    uint8_t slot = s_channelCount; // sentinel: not found
    for (uint8_t i = 0; i < s_channelCount; i++) {
      if (s_pins[i] == _pin) { slot = i; break; }
    }
    if (slot < s_channelCount) {
      // Shift remaining entries down to fill the gap.
      for (uint8_t i = slot; i + 1 < s_channelCount; i++) {
        s_pins[i]       = s_pins[i + 1];
        s_numPixels[i]  = s_numPixels[i + 1];
        s_pixelData[i]  = s_pixelData[i + 1];
      }
      s_channelCount--;
      s_allMask = 0;
#ifdef ESP_HAS_HIGH_GPIO_BANK
      s_allMaskHigh = 0;
      for (uint8_t i = 0; i < s_channelCount; i++) {
        if (s_pins[i] >= 32) s_allMaskHigh |= (1u << (s_pins[i] - 32));
        else                  s_allMask     |= (1u << s_pins[i]);
      }
#else
      for (uint8_t i = 0; i < s_channelCount; i++) s_allMask |= (1u << s_pins[i]);
#endif
      s_stagedCount = 0;
    }
  }
  _initialized = false;

  if (_pin >= 0) gpio_reset_pin((gpio_num_t)_pin);

  // Release the encode buffer via base class helper.
  if (_encodeBuffer) {
    free(_encodeBuffer);
    _encodeBuffer = nullptr;
    _pixelData    = nullptr;
  }
}

// ---------------------------------------------------------------------------
// show()
// Stage this channel's data.  When the last channel stages, run outputParallel().
// ---------------------------------------------------------------------------
bool BitBangBus::show(const uint32_t*, uint16_t, const CctPixel*) {
  if (!_initialized || !_pixelData) return false;

  s_stagedCount++;
  if (s_stagedCount < s_channelCount) {
    return true;  // not all channels ready yet
  }

  // Last channel staged — output all channels in parallel
  s_stagedCount = 0;
  return outputParallel();
}

// ---------------------------------------------------------------------------
// resetChannels() — called by PixelBusAllocator::resetChannelTracking()
// ---------------------------------------------------------------------------
void BitBangBus::resetChannels() {
  s_channelCount = 0;
  s_allMask      = 0;
#ifdef ESP_HAS_HIGH_GPIO_BANK
  s_allMaskHigh  = 0;
#endif
  s_stagedCount  = 0;
  s_t0h          = 0;
  s_t1h          = 0;
  s_period       = 0;
  s_latchCycles  = 0;
  s_pixelBytes   = 0;
  memset(s_pins,       0, sizeof(s_pins));
  memset(s_numPixels,  0, sizeof(s_numPixels));
  memset(s_pixelData,  0, sizeof(s_pixelData));
}

// ---------------------------------------------------------------------------
// outputParallel() — the hot path, must live in IRAM.
//
// Per-bit sequence:
//   1. Compute zeroMask for the current bit (channels outputting a '0', or past
//      their data end, contribute their pin to the mask).
//   2. At each pixel boundary (after the first), release the ISR lock so the
//      FreeRTOS scheduler and time-critical ISRs can run.  Re-acquire and check
//      whether the idle gap exceeded the LED latch threshold; abort if so.
//   3. Wait for the full bit period since the previous HIGH edge.
//   4. Set all output pins HIGH simultaneously (setOutputMask).
//   5. After T0H cycles: pull the '0' outputs LOW  (GPIO.out_w1tc = zeroMask).
//   6. After T1H cycles: pull all remaining outputs LOW (GPIO.out_w1tc = setOutputMask).
//
// All channels are pulsed for maxPixels × pixelBytes × 8 bits.  Channels that
// have fewer pixels than maxPixels simply output '0' bits once their data ends.
// ---------------------------------------------------------------------------
bool IRAM_ATTR BitBangBus::outputParallel() {
  if (s_channelCount == 0) return true;

  const uint32_t t0h          = s_t0h;
  const uint32_t t1h          = s_t1h;
  const uint32_t period       = s_period;
  const uint32_t latchCycles  = s_latchCycles;
  const uint8_t  pixelBytes   = s_pixelBytes;
  const uint8_t  nCh          = s_channelCount;
  // GPIO output masks — split into low bank (pins 0–31, all variants) and
  // high bank (pins 32+, ESP32/S2/S3 only via GPIO_OUT1_W1TS/TC_REG).
#ifdef ESP_HAS_HIGH_GPIO_BANK
  const uint32_t setOutputMaskLow  = s_allMask;      // pins 0–31
  const uint32_t setOutputMaskHigh = s_allMaskHigh;  // pins 32+
#else
  const uint32_t setOutputMask = s_allMask;  // GPIO bitmask of all active output pins (0–31)
#endif

  // Find the maximum pixel count across all channels (drives total loop length).
  uint16_t maxPixels = 0;
  for (uint8_t ch = 0; ch < nCh; ch++) {
    if (s_numPixels[ch] > maxPixels) maxPixels = s_numPixels[ch];
  }
  if (maxPixels == 0) return true;

  const uint32_t totalBits    = (uint32_t)maxPixels * pixelBytes * 8u;
  const uint32_t bitsPerPixel = (uint32_t)pixelBytes * 8u;

  // Per-channel pin masks and data extents — pre-computed for fast inner-loop access.
  // On classic ESP32, pins ≥32 go into the high-bank mask; others into the low-bank mask.
  uint32_t chanPinMask[nCh];
#ifdef ESP_HAS_HIGH_GPIO_BANK
  uint32_t chanPinMaskHigh[nCh];
#endif
  uint32_t chanTotalBytes[nCh];
  for (uint8_t ch = 0; ch < nCh; ch++) {
#ifdef ESP_HAS_HIGH_GPIO_BANK
    if (s_pins[ch] >= 32) {
      chanPinMask[ch]     = 0;
      chanPinMaskHigh[ch] = 1u << (s_pins[ch] - 32);
    } else {
      chanPinMask[ch]     = 1u << s_pins[ch];
      chanPinMaskHigh[ch] = 0;
    }
#else
    chanPinMask[ch]    = 1u << s_pins[ch];
#endif
    chanTotalBytes[ch] = (uint32_t)s_numPixels[ch] * pixelBytes;
  }

  // Returns the GPIO mask(s) of all channels that should output a logical '0' for
  // the given bit index.  Channels that have exhausted their pixel data also
  // output '0'.  Bit order is MSB-first within each byte.
#ifdef ESP_HAS_HIGH_GPIO_BANK
  auto computeZeroMasks = [&](uint32_t bitIndex, uint32_t& zmLow, uint32_t& zmHigh) {
    const uint32_t byteIndex = bitIndex >> 3;
    const uint8_t  bitPos    = 7u - (uint8_t)(bitIndex & 7u);  // MSB first
    zmLow = 0; zmHigh = 0;
    for (uint8_t ch = 0; ch < nCh; ch++) {
      if (byteIndex >= chanTotalBytes[ch] ||
          !(s_pixelData[ch][byteIndex] & (1u << bitPos))) {
        zmLow  |= chanPinMask[ch];
        zmHigh |= chanPinMaskHigh[ch];
      }
    }
  };
#else
  auto computeZeroMask = [&](uint32_t bitIndex) -> uint32_t {
    const uint32_t byteIndex = bitIndex >> 3;
    const uint8_t  bitPos    = 7u - (uint8_t)(bitIndex & 7u);  // MSB first
    uint32_t zm = 0;
    for (uint8_t ch = 0; ch < nCh; ch++) {
      if (byteIndex >= chanTotalBytes[ch] ||
          !(s_pixelData[ch][byteIndex] & (1u << bitPos))) {
        zm |= chanPinMask[ch];
      }
    }
    return zm;
  };
#endif

  // Period reference: initialise as already expired so the first pulse fires immediately.
  uint32_t cyclesStart = getCycleCount() - period;

  portENTER_CRITICAL(&s_mux);

  for (uint32_t bitIndex = 0; bitIndex < totalBits; bitIndex++) {

    // ── Step 1: Compute zero mask(s) for this bit ────────────────────────
#ifdef ESP_HAS_HIGH_GPIO_BANK
    uint32_t zeroMaskLow = 0, zeroMaskHigh = 0;
    computeZeroMasks(bitIndex, zeroMaskLow, zeroMaskHigh);
#else
    uint32_t zeroMask = computeZeroMask(bitIndex);
#endif

    // ── Step 2: Pixel boundary — release ISR lock, check latch ───────────
    // On every pixel boundary (except the very first bit) give ISRs a chance
    // to run, then verify the idle gap has not caused an accidental LED latch.
    /*
    if (bitIndex > 0 && (bitIndex % bitsPerPixel) == 0) {
      portEXIT_CRITICAL(&s_mux);
      portENTER_CRITICAL(&s_mux);
      if (latchCycles > 0 && (getCycleCount() - cyclesStart) > latchCycles) {
        portEXIT_CRITICAL(&s_mux);
        return false;  // ISR latency caused accidental LED latch — frame aborted
      }
    }*/

    // ── Step 3: Wait for the full bit period since the last HIGH edge ──────
    while ((getCycleCount() - cyclesStart) < period);

    // ── Step 4: Set all outputs HIGH simultaneously ───────────────────────
#ifdef ESP_HAS_HIGH_GPIO_BANK
    REG_WRITE(GPIO_OUT_W1TS_REG,  setOutputMaskLow);
    REG_WRITE(GPIO_OUT1_W1TS_REG, setOutputMaskHigh);
#else
    REG_WRITE(GPIO_OUT_W1TS_REG, setOutputMask);
#endif
    cyclesStart = getCycleCount();

    // ── Step 5: After T0H — pull '0' outputs LOW ─────────────────────────
    while ((getCycleCount() - cyclesStart) < t0h);
#ifdef ESP_HAS_HIGH_GPIO_BANK
    REG_WRITE(GPIO_OUT_W1TC_REG,  zeroMaskLow);
    REG_WRITE(GPIO_OUT1_W1TC_REG, zeroMaskHigh);
#else
    REG_WRITE(GPIO_OUT_W1TC_REG, zeroMask);
#endif

    // ── Step 6: After T1H — pull all remaining outputs LOW ───────────────
    while ((getCycleCount() - cyclesStart) < t1h);
#ifdef ESP_HAS_HIGH_GPIO_BANK
    REG_WRITE(GPIO_OUT_W1TC_REG,  setOutputMaskLow);
    REG_WRITE(GPIO_OUT1_W1TC_REG, setOutputMaskHigh);
#else
    REG_WRITE(GPIO_OUT_W1TC_REG, setOutputMask);
#endif
  }

  portEXIT_CRITICAL(&s_mux);
  return true;
}

} // namespace WLEDpixelBus

#endif // ARDUINO_ARCH_ESP32
