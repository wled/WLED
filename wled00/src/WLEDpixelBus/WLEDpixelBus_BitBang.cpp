/*-------------------------------------------------------------------------
WLEDpixelBus — Parallel bit-bang LED output driver

written by Damian Schneider @dedehai 2026

works on all ESP32 variants and ESP8266

Uses software bit-bang timing and outputs data on all set-up pins in parallel.
Interrupts are are re-enabled after each LED data is written, if the return time is too long, output is aborted
The frame is then sent again to avoid effect tearing, this is repeated a few times before giving up
If reset period is set to zero, interrupts are disabled and the sendout is blocking (flicker free but conflicts with anything that requires timed ISRs, like IR receiver)
Each bus can have individual configuration of color channels but all must share the same timing
-------------------------------------------------------------------------*/

#include "WLEDpixelBus_BitBang.h"
#if defined(ARDUINO_ARCH_ESP32)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  #include "esp_private/esp_clk.h"
#else
  #include "clk.h"    // esp_clk_cpu_freq()
#endif


#include "esp_cpu.h"
#elif defined(ESP8266)
#include <Arduino.h>
#define REG_WRITE(addr, val) GPIO_REG_WRITE(addr, val)
#define GPIO_OUT_W1TS_REG    GPIO_OUT_W1TS_ADDRESS
#define GPIO_OUT_W1TC_REG    GPIO_OUT_W1TC_ADDRESS
#endif

namespace WLEDpixelBus {
// BB state, shared among all buses
BitBangBus::BBstate* BitBangBus::_bitBangState = nullptr;

// critical section macros
#if defined(ESP8266)
    #define WLEDPB_BB_ENTERCRITICAL() os_intr_lock()
    #define WLEDPB_BB_EXITCRITICAL()  os_intr_unlock()
#else
    #define WLEDPB_BB_ENTERCRITICAL() portENTER_CRITICAL(&_bitBangState->mux)
    #define WLEDPB_BB_EXITCRITICAL()  portEXIT_CRITICAL(&_bitBangState->mux)
#endif


// getCycleCount() reads the CPU cycle counter for precise timing
#if defined(ESP8266)
  static inline uint32_t getCycleCount() {
    uint32_t ccount;
    __asm__ __volatile__("rsr %0,ccount" : "=a"(ccount));
    return ccount;
  }
  static constexpr uint32_t LOOPTEST_CYCLES = 16;
#else

  static inline __attribute__((always_inline)) uint32_t getCycleCount() {
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    return esp_cpu_get_cycle_count();
  #else
    return esp_cpu_get_ccount();
  #endif
  }
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)
  static constexpr uint32_t LOOPTEST_CYCLES = 8;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  static constexpr uint32_t LOOPTEST_CYCLES = 10;
#else // ESP32 and S2
  static constexpr uint32_t LOOPTEST_CYCLES = 12; // note: LOOPTEST_CYCLES values need fine-tuning
#endif
#endif

// Constructor / Destructor
BitBangBus::BitBangBus(int8_t pin, const LedTiming& timing, uint8_t ledType)
  : _pin(pin)
  , _timing(timing)
  , _initialized(false)
{
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

  if (!_bitBangState) {
    _bitBangState = (BBstate*)calloc(1, sizeof(BBstate)); // Allocate shared state struct on first channel
    if (!_bitBangState) return false;
  }
  if (_bitBangState->channelCount >= WLED_MAX_BB_CHANNELS) return false;

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

  // Subtract the while-loop test overhead (one extra iteration) to compensate for the latency between the condition passing and the actual GPIO write.
  const uint32_t lo = LOOPTEST_CYCLES;

  uint32_t t0h    = (_timing.t0h_ns  * cpuMHz) / 1000u;
  uint32_t t1h    = (_timing.t1h_ns  * cpuMHz) / 1000u;
  t0h             = (t0h    > lo) ? t0h    - lo : 0u;
  t1h             = (t1h    > lo) ? t1h    - lo : 0u;
  uint32_t period = (((uint32_t)_timing.t0h_ns + _timing.t0l_ns) * cpuMHz) / 1000u; // derive the period from the "0" bit
  period          = (period > lo) ? period - lo : 0u;
 // max time allowed before we assume a latch, use half the reset period.
 // Note: there are strips that latch after just ~10us maybe even less, users can customize this period and set it to 10us if there are issues.
  //      if the reset period is set to 0, interrupts are disabled
  uint32_t resetus = _timing.reset_us < 10 ? 10 : _timing.reset_us; // note: below 10us the output will starve due to short interrupts
  const uint32_t latchCycles = resetus * cpuMHz;
  if (_timing.reset_us > 0) _bitBangState->allowInterrupts = true;
  else _bitBangState->allowInterrupts = false; // disable interrupt polling if set to zero

  // Register in the shared static table
  const uint8_t idx  = _bitBangState->channelCount;
  _bitBangState->pins[idx]        = _pin;
#ifdef ESP_HAS_HIGH_GPIO_BANK
  if (_pin >= 32) _bitBangState->allMaskHigh |= (1u << (_pin - 32));
  else            _bitBangState->allMask     |= (1u << _pin);
#else
  _bitBangState->allMask         |= (1u << _pin);
#endif
  _bitBangState->channelCount++;

  // Allocate the per-pixel encode buffer (via PixelBus helper)
  if (!allocateEncodeBuffer(_numPixels, _encoder.getBytesPerPixel())) {
    _bitBangState->channelCount--;
#ifdef ESP_HAS_HIGH_GPIO_BANK
    if (_pin >= 32) _bitBangState->allMaskHigh &= ~(1u << (_pin - 32));
    else            _bitBangState->allMask     &= ~(1u << _pin);
#else
    _bitBangState->allMask &= ~(1u << _pin);
#endif
    return false;
  }
#if defined(ARDUINO_ARCH_ESP32)
  esp_rom_gpio_connect_out_signal(_pin, SIG_GPIO_OUT_IDX, _inverted, false); // route output pin, inverts signal in hardware if needed
#endif

  // Publish per-channel data pointers into the shared static arrays.
  // Timing is set here (same for all channels; overwriting with same values is harmless).
  _bitBangState->encodeBuffer[idx] = _encodeBuffer;
  _bitBangState->encodeBufferSize[idx] = _encodeBufferSize;
  _bitBangState->t0h             = t0h;
  _bitBangState->t1h             = t1h;
  _bitBangState->period          = period;
  _bitBangState->latchCycles     = latchCycles;

  _initialized = true;
  return true;
}

void BitBangBus::end() {
  if (_initialized) {
    // Find our slot by scanning _bitBangState->pins (no stored index needed).
    uint8_t slot = _bitBangState->channelCount; // sentinel: not found
    for (uint8_t i = 0; i < _bitBangState->channelCount; i++) {
      if (_bitBangState->pins[i] == _pin) { slot = i; break; }
    }
    if (slot < _bitBangState->channelCount) {
      // Shift remaining entries down to fill the gap.
      for (uint8_t i = slot; i + 1 < _bitBangState->channelCount; i++) {
        _bitBangState->pins[i] = _bitBangState->pins[i + 1];
        _bitBangState->encodeBuffer[i] = _bitBangState->encodeBuffer[i + 1];
        _bitBangState->encodeBufferSize[i] = _bitBangState->encodeBufferSize[i + 1];
      }
      _bitBangState->channelCount--;
      _bitBangState->allMask = 0;
#ifdef ESP_HAS_HIGH_GPIO_BANK
      _bitBangState->allMaskHigh = 0;
      for (uint8_t i = 0; i < _bitBangState->channelCount; i++) {
        if (_bitBangState->pins[i] >= 32) _bitBangState->allMaskHigh |= (1u << (_bitBangState->pins[i] - 32));
        else                  _bitBangState->allMask     |= (1u << _bitBangState->pins[i]);
      }
#else
      for (uint8_t i = 0; i < _bitBangState->channelCount; i++) _bitBangState->allMask |= (1u << _bitBangState->pins[i]);
#endif
      _bitBangState->stagedCount = 0;
    }
  }
  _initialized = false;

#if defined(ARDUINO_ARCH_ESP32)
  if (_pin >= 0) gpio_reset_pin((gpio_num_t)_pin); // reset all pin settings, including inversion
#elif defined(ESP8266)
  if (_pin >= 0) pinMode(_pin, INPUT);
#endif

  // Release the encode buffer via base class helper.
  releaseEncodeBuffer();

  // If no channels left, free the shared state struct
  if (_bitBangState && _bitBangState->channelCount == 0) {
    free(_bitBangState);
    _bitBangState = nullptr;
  }
}

// show() stages this channel's data.  When the last channel stages, run outputParallel()
bool BitBangBus::show() {
  if (!_initialized || !_encodeBuffer) return false;

  _bitBangState->stagedCount++;
  if (_bitBangState->stagedCount < _bitBangState->channelCount) {
    return true;  // not all channels have been staged, we need all of them triggered before sending
  }

  _bitBangState->stagedCount = 0;   // reset for next frame
  for (uint8_t retry = 0; retry < 3; ++retry) {
    if (outputParallel()) return true;
    // Let every strip latch the partial frame before restarting from pixel zero.
    const uint32_t resetStart = getCycleCount();
    while ((getCycleCount() - resetStart) < _bitBangState->latchCycles) yield();
  }
  return false;
}

// resetChannelTracking() — called by PixelBusAllocator::resetChannelTracking()
void BitBangBus::resetChannelTracking() {
  if (_bitBangState) memset(_bitBangState, 0, sizeof(BBstate));
}

// outputParallel() is the hot path and MUST remain in IRAM. Completed channels emit zero bits.
// ISRs are enabled after every LED, an over-long gap aborts the frame (and retries)
// ---------------------------------------------------------------------------
bool IRAM_ATTR __attribute__((noinline)) BitBangBus::outputParallel() {
  if (!_bitBangState || _bitBangState->channelCount == 0) return true;
  const uint32_t t0h = _bitBangState->t0h;
  const uint32_t t1h = _bitBangState->t1h;
  const uint32_t period = _bitBangState->period;
  const uint32_t latchCycles = _bitBangState->latchCycles;
  const uint8_t nCh = _bitBangState->channelCount;
  const bool allowInterrupts = _bitBangState->allowInterrupts;
#ifdef ESP_HAS_HIGH_GPIO_BANK
  const uint32_t setOutputMaskLow = _bitBangState->allMask;
  const uint32_t setOutputMaskHigh = _bitBangState->allMaskHigh;
#else
  const uint32_t setOutputMask = _bitBangState->allMask;
#endif

  size_t maxBytes = 0;
  uint32_t chanPinMask[nCh];
#ifdef ESP_HAS_HIGH_GPIO_BANK
  uint32_t chanPinMaskHigh[nCh];
#endif
  for (uint8_t ch = 0; ch < nCh; ++ch) {
    if (_bitBangState->encodeBufferSize[ch] > maxBytes) maxBytes = _bitBangState->encodeBufferSize[ch];
#ifdef ESP_HAS_HIGH_GPIO_BANK
    if (_bitBangState->pins[ch] >= 32) {
      chanPinMask[ch] = 0;
      chanPinMaskHigh[ch] = 1u << (_bitBangState->pins[ch] - 32);
    } else {
      chanPinMask[ch] = 1u << _bitBangState->pins[ch];
      chanPinMaskHigh[ch] = 0;
    }
#else
    chanPinMask[ch] = 1u << _bitBangState->pins[ch];
#endif
  }
  if (maxBytes == 0) return true;
  const uint32_t totalBits = maxBytes * 8u;
  uint32_t cyclesStart = getCycleCount() - period;
  WLEDPB_BB_ENTERCRITICAL();

  for (uint32_t bitIndex = 0; bitIndex < totalBits; ++bitIndex) {
    const size_t byteIndex = bitIndex >> 3;
    const uint8_t bitMask = 0x80u >> (bitIndex & 7u);
#ifdef ESP_HAS_HIGH_GPIO_BANK
    uint32_t zeroMaskLow = 0, zeroMaskHigh = 0;
    for (uint8_t ch = 0; ch < nCh; ++ch) {
      if (byteIndex >= _bitBangState->encodeBufferSize[ch] || !(_bitBangState->encodeBuffer[ch][byteIndex] & bitMask)) {
        zeroMaskLow |= chanPinMask[ch];
        zeroMaskHigh |= chanPinMaskHigh[ch];
      }
    }
#else
    uint32_t zeroMask = 0;
    for (uint8_t ch = 0; ch < nCh; ++ch) {
      if (byteIndex >= _bitBangState->encodeBufferSize[ch] || !(_bitBangState->encodeBuffer[ch][byteIndex] & bitMask)) {
        zeroMask |= chanPinMask[ch];
      }
    }
#endif

    while ((getCycleCount() - cyclesStart) < period) {}
#ifdef ESP_HAS_HIGH_GPIO_BANK
    REG_WRITE(GPIO_OUT_W1TS_REG, setOutputMaskLow);
    REG_WRITE(GPIO_OUT1_W1TS_REG, setOutputMaskHigh);
#else
    REG_WRITE(GPIO_OUT_W1TS_REG, setOutputMask);
#endif
    cyclesStart = getCycleCount();
    while ((getCycleCount() - cyclesStart) < t0h) {}
#ifdef ESP_HAS_HIGH_GPIO_BANK
    REG_WRITE(GPIO_OUT_W1TC_REG, zeroMaskLow);
    REG_WRITE(GPIO_OUT1_W1TC_REG, zeroMaskHigh);
#else
    REG_WRITE(GPIO_OUT_W1TC_REG, zeroMask);
#endif
    while ((getCycleCount() - cyclesStart) < t1h) {}
#ifdef ESP_HAS_HIGH_GPIO_BANK
    REG_WRITE(GPIO_OUT_W1TC_REG, setOutputMaskLow);
    REG_WRITE(GPIO_OUT1_W1TC_REG, setOutputMaskHigh);
#else
    REG_WRITE(GPIO_OUT_W1TC_REG, setOutputMask);
#endif

    if (allowInterrupts && ((bitIndex + 1) % 24u == 0)) {
      const uint32_t idleStart = getCycleCount();
      WLEDPB_BB_EXITCRITICAL();
      WLEDPB_BB_ENTERCRITICAL();
      if ((getCycleCount() - idleStart) > latchCycles) {
        WLEDPB_BB_EXITCRITICAL();
        return false;
      }
    }
  }
  WLEDPB_BB_EXITCRITICAL();
  return true;
}
} // namespace WLEDpixelBus