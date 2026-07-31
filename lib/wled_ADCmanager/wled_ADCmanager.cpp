/*
 * ADC manager to handle continous ADC sampling in parallel with single-shot pin reads
 * by @dedehai (2026) licensed under EUPL 1.2 license
 */

#include "wled_adcmanager.h"

// prevent macro recursion of arduino overrides
#undef analogRead
#if defined(ARDUINO_ARCH_ESP32)
#undef analogReadMilliVolts
#endif

#ifdef ARDUINO_ARCH_ESP32

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
  : _pin(0xFF), _channel(ADC_CHANNEL_0), _sampleRate(0), _samplesPerFrame(0), _handle(nullptr), _running(false), _cache(nullptr), _cacheSize(0), _cacheCount(0), _cali(nullptr) {
  _mutex = xSemaphoreCreateMutex();
}

WLEDAdcManager::~WLEDAdcManager() {
  end();
  if (_mutex) vSemaphoreDelete(_mutex);
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  if (_cali) adc_cali_delete_scheme_line_fitting(_cali);
#endif
}

bool WLEDAdcManager::begin(uint8_t pin, uint32_t sampleRateHz, uint16_t samplesPerFrame) {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _destroyHandle();
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

  bool ok = _createHandle();
  xSemaphoreGive(_mutex);
  return ok;
}

void WLEDAdcManager::end() {
  xSemaphoreTake(_mutex, portMAX_DELAY);
  _destroyHandle();
  _cacheCount = 0;
  if (_cache) { free(_cache); _cache = nullptr; _cacheSize = 0; }
  _pin = 0xFF;
  xSemaphoreGive(_mutex);
}

bool WLEDAdcManager::_createHandle() {
  if (_handle) return true;
  size_t frameBytes = (size_t)_samplesPerFrame * sizeof(adc_digi_output_data_t);
  adc_continuous_handle_cfg_t hcfg = {
    .max_store_buf_size = frameBytes * 4,
    .conv_frame_size    = frameBytes,
    .flags = { .flush_pool = true },
  };
  if (adc_continuous_new_handle(&hcfg, &_handle) != ESP_OK) return false;

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
  if (adc_continuous_config(_handle, &cfg) != ESP_OK ||
    adc_continuous_start(_handle) != ESP_OK) {
    _destroyHandle();
    return false;
  }
  _running = true;
  return true;
}

void WLEDAdcManager::_destroyHandle() {
  if (_handle) {
    adc_continuous_stop(_handle);
    adc_continuous_deinit(_handle);
    _handle = nullptr;
  }
  _running = false;
}

void WLEDAdcManager::_drainToCache() {
  if (!_handle || !_cache) return;
  adc_continuous_stop(_handle);
  adc_digi_output_data_t temp[64];
  _cacheCount = 0;
  while (_cacheCount < _cacheSize) {
    uint32_t n = 0;
    if (adc_continuous_read(_handle, (uint8_t*)temp, sizeof(temp), &n, 0) != ESP_OK || n == 0) break;
    uint16_t cnt = n / sizeof(adc_digi_output_data_t);
    for (uint16_t i = 0; i < cnt && _cacheCount < _cacheSize; i++) {
      _cache[_cacheCount++] = (int16_t)((int)(temp[i].WLED_ADC_OUT_TYPE.data) - 2048);
    }
  }
}

bool WLEDAdcManager::readSamples(int16_t* buffer, uint16_t numSamples, uint32_t timeoutMs) {
  if (!buffer || !numSamples) return false;
  xSemaphoreTake(_mutex, portMAX_DELAY);

  uint16_t out = 0;

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
    return true;
  }
  if (!_handle) {
    xSemaphoreGive(_mutex);
    return false;
  }

  adc_continuous_start(_handle);
  adc_digi_output_data_t temp[64];

  while (out < numSamples) {
    uint32_t n = 0;
    uint16_t want = (numSamples - out) < 64 ? (numSamples - out) : 64;
    size_t wantBytes = want * sizeof(adc_digi_output_data_t);

    if (adc_continuous_read(_handle, (uint8_t*)temp, wantBytes, &n, pdMS_TO_TICKS(timeoutMs)) != ESP_OK || n == 0) {
      adc_continuous_stop(_handle);
      xSemaphoreGive(_mutex);
      return false;
    }
    uint16_t got = n / sizeof(adc_digi_output_data_t);
    for (uint16_t i = 0; i < got; i++) {
      buffer[out++] = (int16_t)((int)(temp[i].WLED_ADC_OUT_TYPE.data) - 2048);
    }
  }

  adc_continuous_stop(_handle);
  xSemaphoreGive(_mutex);
  return true;
}

bool WLEDAdcManager::_oneshotRead(adc_channel_t ch, int* outRaw) {
  adc_oneshot_unit_handle_t h;
  adc_oneshot_unit_init_cfg_t icfg = { .unit_id = ADC_UNIT_1, .ulp_mode = ADC_ULP_MODE_DISABLE };
  if (adc_oneshot_new_unit(&icfg, &h) != ESP_OK) return false;

  adc_oneshot_chan_cfg_t ccfg = { .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12 };
  adc_oneshot_config_channel(h, ch, &ccfg);

  bool ok = (adc_oneshot_read(h, ch, outRaw) == ESP_OK);
  adc_oneshot_del_unit(h);
  return ok;
}

int WLEDAdcManager::analogRead(uint8_t pin) {
  Serial.println("ADCread");
  int raw = 0;
  adc_channel_t ch;
  if (!_pinToChannel(pin, &ch)) return 0;

  xSemaphoreTake(_mutex, portMAX_DELAY);
  if (_running) {
    _drainToCache();
    _destroyHandle();
    _oneshotRead(ch, &raw);
    _createHandle();
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

int WLEDAdcManager::analogReadMilliVolts(uint8_t pin) {
  int raw = analogRead(pin);
  if (!_cali && !_initCali()) return (raw * 3300) / 4095;

  int mv = 0;
  return (adc_cali_raw_to_voltage(_cali, raw, &mv) == ESP_OK) ? mv : (raw * 3300) / 4095;
}

#endif // ARDUINO_ARCH_ESP32

//  C wrappers
extern "C" {

int wled_adc_analog_read(uint8_t pin) {
#ifdef ARDUINO_ARCH_ESP32
  return WLEDAdcManager::instance().analogRead(pin);
#else
  eturn analogRead(pin);
#endif
}

int wled_adc_analog_read_mv(uint8_t pin) {
#ifdef ARDUINO_ARCH_ESP32
  return WLEDAdcManager::instance().analogReadMilliVolts(pin);
#else
  eturn (int)(analogRead(pin) / 1023.0f);
#endif
}

}