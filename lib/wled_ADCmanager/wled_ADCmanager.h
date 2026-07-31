/*
 * ADC manager to handle continous ADC sampling in parallel with single-shot pin reads
 * by @dedehai (2026) licensed under EUPL 1.2 license
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int wled_adc_analog_read(uint8_t pin);
int wled_adc_analog_read_mv(uint8_t pin);

#ifdef __cplusplus
}

#ifdef ARDUINO_ARCH_ESP32

#include <Arduino.h>
#include <driver/gpio.h>
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
  #define WLED_ADC_DIGI_FORMAT ADC_DIGI_OUTPUT_FORMAT_TYPE1
  #define WLED_ADC_OUT_TYPE    type1
#else
  #define WLED_ADC_DIGI_FORMAT ADC_DIGI_OUTPUT_FORMAT_TYPE2
  #define WLED_ADC_OUT_TYPE    type2
#endif

class WLEDAdcManager {
public:
  static WLEDAdcManager& instance();

  bool begin(uint8_t pin, uint32_t sampleRateHz, uint16_t samplesPerFrame);
  void end();

  bool readSamples(int16_t* buffer, uint16_t numSamples, uint32_t timeoutMs = 100);

  int analogRead(uint8_t pin);
  int analogReadMilliVolts(uint8_t pin);

  bool isRunning() const { return _running; }

private:
  WLEDAdcManager();
  ~WLEDAdcManager();
  WLEDAdcManager(const WLEDAdcManager&) = delete;
  WLEDAdcManager& operator=(const WLEDAdcManager&) = delete;

  static bool _pinToChannel(uint8_t pin, adc_channel_t* ch);

  bool _createHandle();
  void _destroyHandle();
  void _drainToCache();
  bool _oneshotRead(adc_channel_t ch, int* outRaw);
  bool _initCali();

  SemaphoreHandle_t _mutex;

  uint8_t        _pin;
  adc_channel_t  _channel;
  uint32_t       _sampleRate;
  uint16_t       _samplesPerFrame;
  adc_continuous_handle_t _handle;
  bool           _running;

  int16_t*       _cache;
  uint16_t       _cacheSize;
  uint16_t       _cacheCount;

  adc_cali_handle_t _cali;
};

#endif // ARDUINO_ARCH_ESP32

// override of native Arduino functions for compatibility with external usermods
#undef analogRead
#define analogRead(pin) wled_adc_analog_read(pin)
#if defined(ARDUINO_ARCH_ESP32)
#undef analogReadMilliVolts
#define analogReadMilliVolts(pin) wled_adc_analog_read_mv(pin)
#else
#define analogReadMilliVolts(pin) wled_adc_analog_read_mv(pin)
#endif

#endif // __cplusplus