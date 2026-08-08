/*
 * ADC manager to handle continous ADC sampling in parallel with single-shot pin reads
 * by @dedehai (2026) licensed under EUPL 1.2 license
 *
 * supports sampling a single pin in continuous ADC mode
 * calling begin() will start sampling an ADC pin at the given sample rate
 * if sample rate is higher than the read interval i.e. sampleRateHz/samplesPerFrame, new samples are discarded
 * if read interval is faster, the read function waits until samples are available or the given timeout elapses
 * any subsequent call for analogRead() or analogReadMilliVolts() will pause the continuous sampling,
 * drain the already sampled data into a buffer, read a pin in one-shot mode, then continue the sampling.
 * replaces analogRead() and analogReadMilliVolts() with managed functions for code compatibility


Note: C6 chip revision 0 and 1 have a hardware bug and the effective ADC resolution is only 8bit, was solved in rev. 2 (around mid 2025)
      https://docs.espressif.com/projects/esp-chip-errata/en/latest/esp32c6/03-errata-description/esp32c6/sar-adc-missing-lower-four-bits.html#sar-adc-loss-of-precision-in-lower-four-bits-of-sar-adc

 TODO:
 - could add the option to use hardware IIR filter, although the lowest coefficient setting of 2 already has a 3dB cutoff around 2kHz (to be confirmed) at 20kHz sample rate
 - IIR filter are supported on all modern ESP32 but probably lacking on ESP32 classic, there we would need to do it in post-processing i.e. when writing the sample buffer
 - need to add a "buffer full" callback? -> is added but no longer needed with the bug being fixed in latest tasmota IDF
 - there is an edge-case issue: when continuous sampling is running, several analog pins are configured and the pin-info page is open it can lead to crashes (some issue with semaphore) -> cannot reproduce now, might be solved with added semaphore timeout
 */
#include "wled.h"

#ifdef ARDUINO_ARCH_ESP32
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)

// prevent macro recursion of arduino overrides
#undef analogRead
#undef analogReadMilliVolts

#define ADCMANAGER_DMA_BLOCKSIZE 128 // DMA buffer block size, IDF driver uses 5 blocks under the hood, there is an ISR call each time a block finishes so dont make it too small
#define ADCMANAGER_READBUFFERSAMPLES 128 // number of samples to read per chunk from the ADC buffer (stack buffer), do not set higher than 128 or stack overflow may occur
#define ADCMANAGER_LOCK_TIMEOUT_MS 10 // timeout for mutex lock, this should be as short as possible but still allow for readSamples() to complete before doing analogRead()

#include <string.h>

struct WLEDAdcManager::ContinuousCtx {
  uint8_t        pin;
  adc_channel_t  channel;
  uint32_t       sampleRate;
  uint16_t       samplesPerFrame;
  adc_continuous_handle_t handle;
  int16_t*       cache;
  uint16_t       cacheSize;
  uint16_t       cacheCount;
};

bool WLEDAdcManager::_pinToChannel(uint8_t pin, adc_channel_t* ch) {
  int8_t c = digitalPinToAnalogChannel(pin);
  if (c < 0 || c >= SOC_ADC_CHANNEL_NUM(0)) return false; // check if channel is withing ADC1 range (SOC_ADC_CHANNEL_NUM(0) is the number of channels on ADC1)
  *ch = (adc_channel_t)c;
  return true;
}


WLEDAdcManager& WLEDAdcManager::instance() {
  static WLEDAdcManager inst;
  return inst;
}

WLEDAdcManager::WLEDAdcManager()
  : _mutex(nullptr)
  , _cali(nullptr)
  , _ctx(nullptr) {
  _mutex = xSemaphoreCreateMutex();
}

WLEDAdcManager::~WLEDAdcManager() {
  end();
  if (_mutex) vSemaphoreDelete(_mutex);
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  if (_cali) adc_cali_delete_scheme_line_fitting(_cali);
#endif
}

// initilizes the manager and the hardware and starts sampling
bool WLEDAdcManager::begin(uint8_t pin, uint32_t sampleRateHz, uint16_t samplesPerFrame) {
  if (!_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(ADCMANAGER_LOCK_TIMEOUT_MS)) != pdTRUE) return false;

  // tear down any previous session completely
  if (_ctx) {
    _endContinuousADC();
    if (_ctx->cache) { free(_ctx->cache); _ctx->cache = nullptr; }
    free(_ctx);
    _ctx = nullptr;
  }

  adc_channel_t channel;
  if (!_pinToChannel(pin, &channel)) {
    xSemaphoreGive(_mutex);
    return false;
  }

  _ctx = (ContinuousCtx*)calloc(1, sizeof(ContinuousCtx));
  if (!_ctx) {
    xSemaphoreGive(_mutex);
    return false;
  }

  _ctx->pin = pin;
  _ctx->channel = channel;
  _ctx->sampleRate = sampleRateHz;
  _ctx->samplesPerFrame = samplesPerFrame;
  _ctx->cacheSize = samplesPerFrame;
  _ctx->cacheCount = 0;
  _ctx->cache = (int16_t*)calloc(_ctx->cacheSize, sizeof(int16_t));
  if (!_ctx->cache) {
    free(_ctx);
    _ctx = nullptr;
    xSemaphoreGive(_mutex);
    return false;
  }

  bool ok = _initContinuousADC();
  if (!ok) {
    free(_ctx->cache);
    free(_ctx);
    _ctx = nullptr;
  }
  xSemaphoreGive(_mutex);
  return ok;
}

// stop sampling and deinitialize the AdcManager continuous mode (use this if you want to sample a different pin or do not need to sample anymore)
void WLEDAdcManager::end() {
  if (!_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(ADCMANAGER_LOCK_TIMEOUT_MS)) != pdTRUE) return;
  if (_ctx) {
    _endContinuousADC();
    if (_ctx->cache) { free(_ctx->cache); _ctx->cache = nullptr; }
    free(_ctx);
    _ctx = nullptr;
  }
  xSemaphoreGive(_mutex);
}

/*
// buffer overflow callback, we need to watch this as permanent overflow causes stalls in combination with wifi -> it does not it was an IDF bug
volatile bool _overflow = false;
static bool IRAM_ATTR __attribute__((noinline)) _onPoolOvf(adc_continuous_handle_t handle,
                                  const adc_continuous_evt_data_t* edata,
                                  void* user_data) {
  //auto* mgr = static_cast<WLEDAdcManager*>(user_data);
  _overflow = true;   // one word write, ISR-safe, no locks, no copying
  return false;            // nothing to wake
}

void WLEDAdcManager::checkADC() {
  if (_running && _overflow) {
    adc_continuous_stop(_handle);
    adc_continuous_flush_pool(_handle); // flush remaining data, we want fresh samples
    adc_continuous_start(_handle);
    _overflow = false;
  }
}
*/
// initialize the hardware
bool WLEDAdcManager::_initContinuousADC() {
  if (!_ctx) return false;       // begin() not called
  if (_ctx->handle) return true; // already initialized
  size_t frameBytes = (size_t)_ctx->samplesPerFrame * sizeof(adc_digi_output_data_t);
  adc_continuous_handle_cfg_t hcfg = {
    .max_store_buf_size = frameBytes, // hold single frame in buffer, caller needs to drain it fast enough to avoid data loss (can increase to avoid data loss but still need to drain fast enough at one point)
    .conv_frame_size    = ADCMANAGER_DMA_BLOCKSIZE, // use fixed DMA buffer size of 256 bytes (ADC driver creates 5 DMA descriptors with one buffer each, at 20kHz this means an interrupt every 1.4ms
    .flags = { .flush_pool = false }, // do not flush the store buffer on overrun but discard new samples (true means discard oldest, is much slower and can cause issues, do not set true)
  };
  if (adc_continuous_new_handle(&hcfg, &_ctx->handle) != ESP_OK) return false;

  // register buffer overflow callback (sets flag, main loop needs to call checkADC() to clear overflow - this is to prevent wifi stalling due to a now fixed IDF bug causing a lockup)
  //adc_continuous_evt_cbs_t cbs = { .on_conv_done = nullptr, .on_pool_ovf = _onPoolOvf };
  //adc_continuous_register_event_callbacks(_handle, &cbs, this);

  adc_digi_pattern_config_t pat = {
    .atten     = ADC_ATTEN_DB_12,
    .channel   = (uint8_t)_ctx->channel,
    .unit      = ADC_UNIT_1,
    .bit_width = ADC_BITWIDTH_12,
  };
  adc_continuous_config_t cfg = {
    .pattern_num    = 1,
    .adc_pattern    = &pat,
    .sample_freq_hz = _ctx->sampleRate,
    .conv_mode      = ADC_CONV_SINGLE_UNIT_1,
    .format         = WLED_ADC_DIGI_FORMAT,
  };
  // initialize and start sampling
  if (adc_continuous_config(_ctx->handle, &cfg) != ESP_OK || adc_continuous_start(_ctx->handle) != ESP_OK) {
    _endContinuousADC();
    return false;
  }
  return true;
}

// stop sampling and de-initilize the hardware so it can be used by analogRead() but keeps the continuous _ctx configuration
void WLEDAdcManager::_endContinuousADC() {
  if (!_ctx) return;
  if (_ctx->handle) {
    adc_continuous_stop(_ctx->handle);
    adc_continuous_deinit(_ctx->handle);
    _ctx->handle = nullptr;
  }
}

void WLEDAdcManager::_drainToCache() {
  if (!_ctx || !_ctx->handle || !_ctx->cache) return; // safety check
  adc_digi_output_data_t temp[32]; // size of data packets to request, 32 samples at 22kHz is 1.5ms, leftover samples are lost
  while (_ctx->cacheCount < _ctx->cacheSize) {
    uint32_t n = 0;
    // read what is available in the buffer in chunks (no timeout means do not wait for any additional samples)
    if (adc_continuous_read(_ctx->handle, (uint8_t*)temp, sizeof(temp), &n, 0) != ESP_OK || n == 0) break;
    uint16_t cnt = n / sizeof(adc_digi_output_data_t);
    for (uint16_t i = 0; i < cnt && _ctx->cacheCount < _ctx->cacheSize; i++) {
      _ctx->cache[_ctx->cacheCount++] = (int16_t)(temp[i].WLED_ADC_OUT_TYPE.data);
    }
  }
}

// read samples acquired in continuous mode. They are written as 12bit unsigned values into the passed buffer
// tries to read "numSamples" and returns the actual number of samples written into the buffer
// it waits up to timeoutMs total while fetching tmpBfrSize (128) samples at a time, if not enough samples are available, it returns what it got
// to poll the buffer and "just give me what you got" use a timeout of 0. On read error, it restarts the driver so no action needed by caller.
// note: maximum timeout is ADCMANAGER_LOCK_TIMEOUT_MS-1, so make sure to call this function in reasonable intervals if you need all samples (or increase the lock time)
uint16_t WLEDAdcManager::readSamples(int16_t* buffer, uint16_t numSamples, uint32_t timeoutMs) {
  if (!buffer || !numSamples) return 0;
  // note: DMA uses SOC_ADC_DIGI_MAX_BITWIDTH which is 12bits on all checked units TODO: should make sure and handle this to future proof it
  if (!_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(ADCMANAGER_LOCK_TIMEOUT_MS)) != pdTRUE) return 0;
  // check if continuous mode is running AFTER we take the semaphore, otherwise we could have a race with analogRead()
  if (!_ctx || !_ctx->handle) {
    xSemaphoreGive(_mutex);
    return 0;
  }

  const uint32_t maxReadTimeoutMs = ADCMANAGER_LOCK_TIMEOUT_MS - 1;
  const uint32_t readTimeoutMs = timeoutMs < maxReadTimeoutMs ? timeoutMs : maxReadTimeoutMs;
  const uint32_t readStartMs = millis();
  uint16_t out = 0; // number of samples written to the buffer
  // check if any data was cached during an intermediate analogRead()
  if (_ctx->cacheCount) {
    uint16_t copy = _ctx->cacheCount < numSamples ? _ctx->cacheCount : numSamples;
    memcpy(buffer, _ctx->cache, copy * sizeof(int16_t));
    out = copy;
    if (copy < _ctx->cacheCount) {
      memmove(_ctx->cache, _ctx->cache + copy, (_ctx->cacheCount - copy) * sizeof(int16_t));
    }
    _ctx->cacheCount -= copy;
  }

  if (out >= numSamples) {
    xSemaphoreGive(_mutex);
    return out; // already get enough samples from cache
  }

  const int tmpBfrSize = ADCMANAGER_READBUFFERSAMPLES; // use fixed size buffer on stack to read samples from the driver in chunks
  adc_digi_output_data_t temp[tmpBfrSize];

  while (out < numSamples) {
    const uint32_t elapsedMs = millis() - readStartMs;
    if (elapsedMs >= readTimeoutMs) break;

    uint32_t n = 0;
    uint16_t want = (numSamples - out) < tmpBfrSize ? (numSamples - out) : tmpBfrSize;
    size_t wantBytes = want * sizeof(adc_digi_output_data_t);
    uint32_t remainingMs = readTimeoutMs - elapsedMs;
    esp_err_t err = adc_continuous_read(_ctx->handle, (uint8_t*)temp, wantBytes, &n, remainingMs);

    // copy the data into 16bit buffer
    if (n > 0) {
      uint16_t got = n / sizeof(adc_digi_output_data_t);
      for (uint16_t i = 0; i < got; i++) {
        buffer[out++] = (int16_t)(temp[i].WLED_ADC_OUT_TYPE.data);
      }
    }

    if (err == ESP_ERR_TIMEOUT) {
      break; // not enough samples within timeout frame, return what we got
    }
    if (err != ESP_OK) {
      DEBUG_PRINTF_P(PSTR("ADC read error %d, got n=%d restarting"), err, n);
      _endContinuousADC();
      _initContinuousADC();
      break;
    }
    else if (n == 0) break; // should not happen, just in case (if no samples are read, it should not be ESP_OK)
  }

  //adc_continuous_flush_pool(_handle); // flush remaining data -> no need, just let the samples accumulate, uncomment if you need freshest samples only

  xSemaphoreGive(_mutex);
  return out;
}

// a one-shot read takes about 0.7-2.5ms if continuous reading is active, depending on chip type (ESP32 is slower, newer ones are faster), 0.2ms otherwise
bool WLEDAdcManager::_oneshotRead(adc_channel_t ch, int* outRaw) {
  adc_oneshot_unit_handle_t h;
  adc_oneshot_unit_init_cfg_t icfg = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE };
  if (adc_oneshot_new_unit(&icfg, &h) != ESP_OK) return false;

  adc_oneshot_chan_cfg_t ccfg = {
    .atten = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12 };
  adc_oneshot_config_channel(h, ch, &ccfg);

  bool ok = (adc_oneshot_read(h, ch, outRaw) == ESP_OK);
  adc_oneshot_del_unit(h);
  if (ok) {
    // The raw result is in SOC_ADC_RTC_MAX_BITWIDTH bits, independent of .bitwidth set above, we use 12bit in WLED
    #if (SOC_ADC_RTC_MAX_BITWIDTH > 12)
      *outRaw >>= (SOC_ADC_RTC_MAX_BITWIDTH - 12);
    #elif (SOC_ADC_RTC_MAX_BITWIDTH < 12)
      *outRaw <<= (12 - SOC_ADC_RTC_MAX_BITWIDTH);
    #endif
  }
  return ok;
}

int WLEDAdcManager::analogRead(uint8_t pin) {
  int raw = 0;
  adc_channel_t ch;
  if (!_pinToChannel(pin, &ch)) return 0;

  if (!_mutex || xSemaphoreTake(_mutex, pdMS_TO_TICKS(ADCMANAGER_LOCK_TIMEOUT_MS)) != pdTRUE) return 0;
  if (_ctx) { // continuous sampling is used
    _drainToCache();
    _endContinuousADC();  // stop sampling and free the ADC hardware if in use
    _oneshotRead(ch, &raw);
    _initContinuousADC(); // re-init and start sampling again
  } else {
    _oneshotRead(ch, &raw);
  }
  xSemaphoreGive(_mutex);
  return raw;
}

bool WLEDAdcManager::_initCali() {
  if (_cali) return true;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  adc_cali_curve_fitting_config_t cfg = {
    .unit_id  = ADC_UNIT_1,
    #if SOC_ADC_CALIB_CHAN_COMPENS_SUPPORTED
    //.chan     = adc_channel_t(channel); // per channel calibration is not implemented (C5, C6, P4 support it). trading complexity for a few mV of inaccuracy here.
    #endif
    .atten    = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
  };
  if (adc_cali_create_scheme_curve_fitting(&cfg, &_cali) == ESP_OK) return true;
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  adc_cali_line_fitting_config_t cfg = {
    .unit_id  = ADC_UNIT_1,
    .atten    = ADC_ATTEN_DB_12,
    .bitwidth = ADC_BITWIDTH_12,
  };
  if (adc_cali_create_scheme_line_fitting(&cfg, &_cali) == ESP_OK) return true;
#endif
  return false;
}

#if SOC_ADC_DIG_IIR_FILTER_SUPPORTED
#endif

int WLEDAdcManager::analogReadMilliVolts(uint8_t pin) {
  int raw = analogRead(pin); // returns 0 on an invalid pin or error
  int mv = 0;
  if ((_cali || _initCali()) && adc_cali_raw_to_voltage(_cali, raw, &mv) == ESP_OK) return mv;
  return (raw * 3300) / 4095; // fallback to linear conversion if calibration fails
}
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#endif // ARDUINO_ARCH_ESP32
