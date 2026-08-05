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

 TODO:
 - could add the option to use hardware IIR filter, although the lowest coefficient setting of 2 already has a 3dB cutoff around 2kHz (to be confirmed) at 20kHz sample rate
 - IIR filter are supported on all modern ESP32 but probably lacking on ESP32 classic, there we would need to do it in post-processing i.e. when writing the sample buffer
 - need to add a "buffer full" callback? -> is added but no longer needed with the bug being fixed in latest tasmota IDF
 */

//#include "wled_adcmanager.h"
#include "wled.h"

// prevent macro recursion of arduino overrides
#undef analogRead
#if defined(ARDUINO_ARCH_ESP32)
#undef analogReadMilliVolts
#endif

#ifdef ARDUINO_ARCH_ESP32

#define ADCMANAGER_DMA_BLOCKSIZE 128 // DMA buffer block size, IDF driver uses 5 blocks under the hood, there is an ISR call each time a block finishes so dont make it too small
#define ADCMANAGER_READBUFFERSIZE (128 * sizeof(adc_digi_output_data_t)) // size of stack buffer for reading samples from the driver

#include <string.h>

static bool _isADC1(uint8_t pin, int8_t ch) {
#if defined(CONFIG_IDF_TARGET_ESP32)
  (void)ch; return (pin >= 32 && pin <= 39);
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  return (ch >= 0 && ch <= 9);
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  return (ch >= 0 && ch <= 9);
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  return (ch >= 0 && ch <= 4);
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  (void)ch; return (pin <= 5);
#else
  (void)pin; (void)ch; return true;
#endif
}

bool WLEDAdcManager::_pinToChannel(uint8_t pin, adc_channel_t* ch) {
  int8_t c = digitalPinToAnalogChannel(pin);
  if (c < 0 || !_isADC1(pin, c)) return false;
  *ch = (adc_channel_t)c;
  return true;
}

WLEDAdcManager& WLEDAdcManager::instance() {
  static WLEDAdcManager inst;
  return inst;
}

WLEDAdcManager::WLEDAdcManager()
  : _pin(0xFF)
  , _channel(ADC_CHANNEL_0)
  , _sampleRate(0)
  , _samplesPerFrame(0)
  , _handle(nullptr)
  , _running(false)
  , _cache(nullptr)
  , _cacheSize(0)
  , _cacheCount(0)
  , _cali(nullptr) {
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
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _endContinuousADC();
  _cacheCount = 0;

  if (!_pinToChannel(pin, &_channel)) {
    xSemaphoreGive(_mutex);
    return false;
  }
  _pin = pin;
  _sampleRate = sampleRateHz;
  _samplesPerFrame = samplesPerFrame;

  if (_cache) free(_cache);
  _cacheSize = samplesPerFrame;
  _cache = (int16_t*)calloc(_cacheSize, sizeof(int16_t));

  bool ok = _initContinuousADC();
  xSemaphoreGive(_mutex);
  return ok;
}

// stop sampling and de-initilize the hardware so it can be used by analogRead() or to sample a different pin
void WLEDAdcManager::end() {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _endContinuousADC();
  _cacheCount = 0;
  if (_cache) { free(_cache); _cache = nullptr; _cacheSize = 0; }
  _pin = 0xFF;
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
  if (_handle) return true; // already initialized
  size_t frameBytes = (size_t)_samplesPerFrame * sizeof(adc_digi_output_data_t);
  adc_continuous_handle_cfg_t hcfg = {
    .max_store_buf_size = frameBytes * 1, // hold two frames in buffer, caller needs to drain it fast enough to avoid data loss
    .conv_frame_size    = ADCMANAGER_DMA_BLOCKSIZE, // use fixed DMA buffer size of 256 bytes (ADC driver creates 5 DMA descriptors with one buffer each, at 20kHz this means an interrupt every 1.4ms
    .flags = { .flush_pool = false }, // do not flush the store buffer on overrun but discard new samples (true means discard oldest, is much slower and can cause issues, do not set true)
  };
  if (adc_continuous_new_handle(&hcfg, &_handle) != ESP_OK) return false;

  // register buffer overflow callback (sets flag, main loop needs to call checkADC() to clear overflow - this is to prevent wifi stalling due to a now fixed IDF bug causing a lockup)
  //adc_continuous_evt_cbs_t cbs = { .on_conv_done = nullptr, .on_pool_ovf = _onPoolOvf };
  //adc_continuous_register_event_callbacks(_handle, &cbs, this);

  adc_digi_pattern_config_t pat = {
    .atten     = ADC_ATTEN_DB_12,
    .channel   = (uint8_t)_channel,
    .unit      = ADC_UNIT_1,
    .bit_width = ADC_BITWIDTH_12,
  };
  adc_continuous_config_t cfg = {
    .pattern_num    = 1,
    .adc_pattern    = &pat,
    .sample_freq_hz = _sampleRate,
    .conv_mode      = ADC_CONV_SINGLE_UNIT_1,
    .format         = WLED_ADC_DIGI_FORMAT,
  };
  // initialize and start sampling
  if (adc_continuous_config(_handle, &cfg) != ESP_OK || adc_continuous_start(_handle) != ESP_OK) {
    _endContinuousADC();
    return false;
  }
  _running = true;
  return true;
}

void WLEDAdcManager::_endContinuousADC() {
  if (_handle) {
    adc_continuous_stop(_handle);
    adc_continuous_deinit(_handle);
    _handle = nullptr;
  }
  _running = false;
}

void WLEDAdcManager::_drainToCache() {
  if (!_handle || !_cache) return; // note: checking _handle is redundant but also does not hurt (caller checks _running)
  adc_digi_output_data_t temp[32]; // size of data packets to request, 32 samples at 22kHz is 1.5ms, leftover samples are lost
  _cacheCount = 0;
  while (_cacheCount < _cacheSize) {
    uint32_t n = 0;
    // read what is available in the buffer in chunks (no timeout means do not wait for any additional samples)
    if (adc_continuous_read(_handle, (uint8_t*)temp, sizeof(temp), &n, 0) != ESP_OK || n == 0) break;
    uint16_t cnt = n / sizeof(adc_digi_output_data_t);
    for (uint16_t i = 0; i < cnt && _cacheCount < _cacheSize; i++) {
      _cache[_cacheCount++] = (int16_t)(temp[i].WLED_ADC_OUT_TYPE.data);
    }
  }
}

// read samples acquired in continuous mode. They are written as 12bit unsigned values into the passed buffer
// tries to read "numSamples" and returns the actual number of samples written into the buffer
// it waits up to timeoutMs per fetch of tmpBfrSize (128) samples, if not enough samples are available, it returns what it got
// to poll the buffer and "just give me what you got" use a timeout of 0. On read error, it restarts the driver so no action needed by caller.
uint16_t WLEDAdcManager::readSamples(int16_t* buffer, uint16_t numSamples, uint32_t timeoutMs) {
  if (!_running || !buffer || !numSamples) return false;
  // note: DMA uses SOC_ADC_DIGI_MAX_BITWIDTH which is 12bits on all checked units TODO: should make sure and handle this to future proof it
  xSemaphoreTake(_mutex, portMAX_DELAY);

  uint16_t out = 0; // number of samples written to the buffer
  // check if any data was cached during an intermediate analogRead()
  if (_cacheCount) {
    uint16_t copy = _cacheCount < numSamples ? _cacheCount : numSamples;
    memcpy(buffer, _cache, copy * sizeof(int16_t));
    out = copy;
    if (copy < _cacheCount) {
      memmove(_cache, _cache + copy, (_cacheCount - copy) * sizeof(int16_t));
    }
    _cacheCount -= copy;
  }

  if (out >= numSamples) {
    xSemaphoreGive(_mutex);
    return out; // already get enough samples from cache
  }

  const int tmpBfrSize = ADCMANAGER_READBUFFERSIZE; // use fixed size buffer on stack to read samples from the driver in chunks
  adc_digi_output_data_t temp[tmpBfrSize];

  while (out < numSamples) {
    uint32_t n = 0;
    uint16_t want = (numSamples - out) < tmpBfrSize ? (numSamples - out) : tmpBfrSize;
    size_t wantBytes = want * sizeof(adc_digi_output_data_t);
    esp_err_t err = adc_continuous_read(_handle, (uint8_t*)temp, wantBytes, &n, pdMS_TO_TICKS(timeoutMs));

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

// a one-shot read takes about 0.8-2.5ms if continuous reading is active, 0.2ms otherwise
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

  xSemaphoreTake(_mutex, portMAX_DELAY);
  if (_running) {
    _drainToCache();
    _endContinuousADC();
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
  int result_mv = 0;
  adc_channel_t ch;
  if (!_pinToChannel(pin, &ch)) return 0;
  int raw = analogRead(pin);
  if (!_cali && !_initCali()) return (raw * 3300) / 4095;

  int mv = 0;
  return (adc_cali_raw_to_voltage(_cali, raw, &mv) == ESP_OK) ? mv : (raw * 3300) / 4095;
}
#endif // ARDUINO_ARCH_ESP32
