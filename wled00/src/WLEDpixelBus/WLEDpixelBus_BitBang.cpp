/*-------------------------------------------------------------------------
WLEDpixelBus — Parallel bit-bang LED output driver

written by Damian Schneider @dedehai 2026

works on all ESP32 variants

Interrupts are fully disabled during output to avoid any glitches
Each bus can have individual configuration of color channels but all must share the same timing

-------------------------------------------------------------------------*/

#include "WLEDpixelBus_BitBang.h"
#if defined(ARDUINO_ARCH_ESP32)
#include "esp_clk.h"    // esp_clk_cpu_freq()
#elif defined(ESP8266)
#include <Arduino.h>
#define REG_WRITE(addr, val) GPIO_REG_WRITE(addr, val)
#define GPIO_OUT_W1TS_REG    GPIO_OUT_W1TS_ADDRESS
#define GPIO_OUT_W1TC_REG    GPIO_OUT_W1TC_ADDRESS
#endif

namespace WLEDpixelBus {
// BB state, shared among all buses
BitBangBus::BBstate* BitBangBus::_BBs = nullptr;

// ---------------------------------------------------------------------------
// getCycleCount() — read the CPU cycle counter.
// Must be IRAM_ATTR because it is called from outputParallel() which is IRAM.
// ---------------------------------------------------------------------------
#if defined(ESP8266)
  static inline uint32_t IRAM_ATTR getCycleCount() {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount" : "=a"(ccount));
    return ccount;
  }
  static constexpr uint32_t LOOPTEST_CYCLES = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || \
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
BitBangBus::BitBangBus(int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, uint8_t ledType)
  : _pin(pin), _rawTiming(timing), _initialized(false)
{
  _encoder = ColorEncoder(colorOrder, numChannels, ledType);
  _ledType = ledType;
}

BitBangBus::~BitBangBus() {
  end();
}

bool BitBangBus::begin() {
  if (_initialized) return true;

  // GPIO pin range check
#if defined(ARDUINO_ARCH_ESP32)
  if (_pin < 0 || _pin >= SOC_GPIO_PIN_COUNT || !((SOC_GPIO_VALID_OUTPUT_GPIO_MASK >> _pin) & 1ULL)) return false;
#elif defined(ESP8266)
  if (_pin < 0 || _pin > 15) return false;
#endif

  if (!_BBs) {
    _BBs = (BBstate*)calloc(1, sizeof(BBstate)); // Allocate shared state struct on first channel
    if (!_BBs) return false;
  }
  if (_BBs->channelCount >= WLED_MAX_BB_CHANNELS) return false;

  // Configure GPIO as output, drive LOW (idle state for non-inverted LEDs)
#if defined(ARDUINO_ARCH_ESP32)
  gpio_set_direction((gpio_num_t)_pin, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)_pin, 0);
#elif defined(ESP8266)
  pinMode(_pin, OUTPUT);
  digitalWrite(_pin, LOW);
#endif

  // Convert nanosecond timings to CPU cycles
#if defined(ARDUINO_ARCH_ESP32)
  const uint32_t cpuMHz = esp_clk_cpu_freq() / 1000000u;
#elif defined(ESP8266)
  const uint32_t cpuMHz = ESP.getCpuFreqMHz();
#endif

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
  const uint8_t idx  = _BBs->channelCount;
  _BBs->pins[idx]        = _pin;
#ifdef ESP_HAS_HIGH_GPIO_BANK
  if (_pin >= 32) _BBs->allMaskHigh |= (1u << (_pin - 32));
  else            _BBs->allMask     |= (1u << _pin);
#else
  _BBs->allMask         |= (1u << _pin);
#endif
  _BBs->channelCount++;

  // Allocate the per-pixel encode buffer (via PixelBus helper)
  if (!allocateEncodeBuffer(_numPixels, _encoder.getPixelBytes())) {
    _BBs->channelCount--;
#ifdef ESP_HAS_HIGH_GPIO_BANK
    if (_pin >= 32) _BBs->allMaskHigh &= ~(1u << (_pin - 32));
    else            _BBs->allMask     &= ~(1u << _pin);
#else
    _BBs->allMask &= ~(1u << _pin);
#endif
    return false;
  }
#if defined(ARDUINO_ARCH_ESP32)
  esp_rom_gpio_connect_out_signal(_pin, SIG_GPIO_OUT_IDX, _inverted, false); // route output pin, inverts signal in hardware if needed
#endif

  // Publish per-channel data pointers into the shared static arrays.
  // Timing is set here (same for all channels; overwriting with same values is harmless).
  _BBs->numPixels[idx]  = _numPixels;
  _BBs->pixelData[idx]  = _pixelData;
  _BBs->t0h             = t0h;
  _BBs->t1h             = t1h;
  _BBs->period          = period;
  _BBs->latchCycles     = latchCycles;
  _BBs->pixelBytes      = _encoder.getPixelBytes();

  _initialized = true;
  return true;
}

// invert output signal, must be set before begin()
void BitBangBus::setInverted(bool inv) {
  _inverted = inv;
}

void BitBangBus::end() {
  if (_initialized) {
    // Find our slot by scanning _BBs->pins (no stored index needed).
    uint8_t slot = _BBs->channelCount; // sentinel: not found
    for (uint8_t i = 0; i < _BBs->channelCount; i++) {
      if (_BBs->pins[i] == _pin) { slot = i; break; }
    }
    if (slot < _BBs->channelCount) {
      // Shift remaining entries down to fill the gap.
      for (uint8_t i = slot; i + 1 < _BBs->channelCount; i++) {
        _BBs->pins[i]       = _BBs->pins[i + 1];
        _BBs->numPixels[i]  = _BBs->numPixels[i + 1];
        _BBs->pixelData[i]  = _BBs->pixelData[i + 1];
      }
      _BBs->channelCount--;
      _BBs->allMask = 0;
#ifdef ESP_HAS_HIGH_GPIO_BANK
      _BBs->allMaskHigh = 0;
      for (uint8_t i = 0; i < _BBs->channelCount; i++) {
        if (_BBs->pins[i] >= 32) _BBs->allMaskHigh |= (1u << (_BBs->pins[i] - 32));
        else                  _BBs->allMask     |= (1u << _BBs->pins[i]);
      }
#else
      for (uint8_t i = 0; i < _BBs->channelCount; i++) _BBs->allMask |= (1u << _BBs->pins[i]);
#endif
      _BBs->stagedCount = 0;
    }
  }
  _initialized = false;

#if defined(ARDUINO_ARCH_ESP32)
  if (_pin >= 0) gpio_reset_pin((gpio_num_t)_pin); // reset all pin settings, including inversion
#elif defined(ESP8266)
  if (_pin >= 0) pinMode(_pin, INPUT);
#endif

  // Release the encode buffer via base class helper.
  if (_encodeBuffer) {
    free(_encodeBuffer);
    _encodeBuffer = nullptr;
    _pixelData    = nullptr;
  }

  // If no channels left, free the shared state struct
  if (_BBs && _BBs->channelCount == 0) {
    free(_BBs);
    _BBs = nullptr;
  }
}

// ---------------------------------------------------------------------------
// show()
// Stage this channel's data.  When the last channel stages, run outputParallel().
// ---------------------------------------------------------------------------
bool BitBangBus::show(const uint32_t*, uint16_t, const CctPixel*) {
  if (!_initialized || !_pixelData) return false;

  _BBs->stagedCount++;
  if (_BBs->stagedCount < _BBs->channelCount) {
    return true;  // not all channels ready yet
  }

  _BBs->stagedCount = 0;   // reset for next frame
  return outputParallel(); // send data to LEDs (blocking)

}

// ---------------------------------------------------------------------------
// resetChannels() — called by PixelBusAllocator::resetChannelTracking()
// ---------------------------------------------------------------------------
void BitBangBus::resetChannels() {
  if (_BBs) memset(_BBs, 0, sizeof(BBstate));
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
  if (!_BBs || _BBs->channelCount == 0) return true;

  uint32_t starttime = micros();

  const uint32_t t0h          = _BBs->t0h;
  const uint32_t t1h          = _BBs->t1h;
  const uint32_t period       = _BBs->period;
  const uint32_t latchCycles  = _BBs->latchCycles;
  const uint8_t  pixelBytes   = _BBs->pixelBytes;
  const uint8_t  nCh          = _BBs->channelCount;
  // GPIO output masks — split into low bank (pins 0–31, all variants) and
  // high bank (pins 32+, ESP32/S2/S3 only via GPIO_OUT1_W1TS/TC_REG).
#ifdef ESP_HAS_HIGH_GPIO_BANK
  const uint32_t setOutputMaskLow  = _BBs->allMask;      // pins 0–31
  const uint32_t setOutputMaskHigh = _BBs->allMaskHigh;  // pins 32+
#else
  const uint32_t setOutputMask = _BBs->allMask;  // GPIO bitmask of all active output pins (0–31)
#endif

  // Find the maximum pixel count across all channels (drives total loop length).
  uint16_t maxPixels = 0;
  for (uint8_t ch = 0; ch < nCh; ch++) {
    if (_BBs->numPixels[ch] > maxPixels) maxPixels = _BBs->numPixels[ch];
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
    if (_BBs->pins[ch] >= 32) {
      chanPinMask[ch]     = 0;
      chanPinMaskHigh[ch] = 1u << (_BBs->pins[ch] - 32);
    } else {
      chanPinMask[ch]     = 1u << _BBs->pins[ch];
      chanPinMaskHigh[ch] = 0;
    }
#else
    chanPinMask[ch]    = 1u << _BBs->pins[ch];
#endif
    chanTotalBytes[ch] = (uint32_t)_BBs->numPixels[ch] * pixelBytes;
  }

  // Returns the GPIO mask(s) of all channels that should output a logical '0' for
  // the given bit index.  Channels that have exhausted their pixel data also
  // output '0'.  Bit order is MSB-first within each byte.
#ifdef ESP_HAS_HIGH_GPIO_BANK
  auto computeZeroMasks = [&](uint32_t bitIndex, uint32_t& zmLow, uint32_t& zmHigh) {
    const uint32_t byteIndex = bitIndex >> 3;
    const uint8_t  bitMask = 0x80u >> (bitIndex & 7u);
    zmLow = 0; zmHigh = 0;
    for (uint8_t ch = 0; ch < nCh; ch++) {
      if (byteIndex >= chanTotalBytes[ch] || !(_BBs->pixelData[ch][byteIndex] & bitMask)) {
        zmLow  |= chanPinMask[ch];
        zmHigh |= chanPinMaskHigh[ch];
      }
    }
  };
#else
  auto computeZeroMask = [&](uint32_t bitIndex) -> uint32_t {
    const uint32_t byteIndex = bitIndex >> 3;
    const uint8_t bitMask = 0x80u >> (bitIndex & 7u);
    uint32_t zm = 0;
    for (uint8_t ch = 0; ch < nCh; ch++) {
      if (byteIndex >= chanTotalBytes[ch] || !(_BBs->pixelData[ch][byteIndex] & bitMask)) {
        zm |= chanPinMask[ch];
      }
    }
    return zm;
  };
#endif

  // Period reference: initialise as already expired so the first pulse fires immediately.
  uint32_t cyclesStart = getCycleCount() - period;

#if defined(ARDUINO_ARCH_ESP32)
  portENTER_CRITICAL(&_BBs->mux);
#elif defined(ESP8266)
  os_intr_lock();
#endif

  for (uint32_t bitIndex = 0; bitIndex < totalBits; bitIndex++) {

    // Compute zero mask(s) for this bit
#ifdef ESP_HAS_HIGH_GPIO_BANK
    uint32_t zeroMaskLow = 0, zeroMaskHigh = 0;
    computeZeroMasks(bitIndex, zeroMaskLow, zeroMaskHigh);
#else
    uint32_t zeroMask = computeZeroMask(bitIndex);
#endif

    // Wait for the full bit period since the last HIGH edge
    while ((getCycleCount() - cyclesStart) < period);

    // Set all outputs HIGH simultaneously
#ifdef ESP_HAS_HIGH_GPIO_BANK
    REG_WRITE(GPIO_OUT_W1TS_REG,  setOutputMaskLow);
    REG_WRITE(GPIO_OUT1_W1TS_REG, setOutputMaskHigh);
#else
    REG_WRITE(GPIO_OUT_W1TS_REG, setOutputMask);
#endif
    cyclesStart = getCycleCount();

    // After T0H — pull '0' outputs LOW
    while ((getCycleCount() - cyclesStart) < t0h);
#ifdef ESP_HAS_HIGH_GPIO_BANK
    REG_WRITE(GPIO_OUT_W1TC_REG,  zeroMaskLow);
    REG_WRITE(GPIO_OUT1_W1TC_REG, zeroMaskHigh);
#else
    REG_WRITE(GPIO_OUT_W1TC_REG, zeroMask);
#endif

    // After T1H — pull all remaining outputs LOW
    while ((getCycleCount() - cyclesStart) < t1h);
#ifdef ESP_HAS_HIGH_GPIO_BANK
    REG_WRITE(GPIO_OUT_W1TC_REG,  setOutputMaskLow);
    REG_WRITE(GPIO_OUT1_W1TC_REG, setOutputMaskHigh);
#else
    REG_WRITE(GPIO_OUT_W1TC_REG, setOutputMask);
#endif
  }

#if defined(ARDUINO_ARCH_ESP32)
  portEXIT_CRITICAL(&_BBs->mux);
#elif defined(ESP8266)
  os_intr_unlock();
#endif
  
  uint32_t endtime = micros();
  Serial.printf("took %d us\n", endtime - starttime);

  return true;
}


} // namespace WLEDpixelBus