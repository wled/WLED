/*
 * ADC manager to handle continous ADC sampling in parallel with single-shot pin reads
 * by @dedehai (2026) licensed under EUPL 1.2 license
 */

#pragma once

#ifdef ARDUINO_ARCH_ESP32
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)

#include <stdint.h>
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
  bool isRunning() const { return _ctx != nullptr; }

  uint16_t readSamples(int16_t* buffer, uint16_t numSamples, uint32_t timeoutMs = 100);

  int analogRead(uint8_t pin);
  int analogReadMilliVolts(uint8_t pin);
  //void checkADC(); // check ADC status, reset if overflow happened (watchdog function, needs to be called frequently if used, i.e. put this in main loop)

private:
WLEDAdcManager();
  ~WLEDAdcManager();
  WLEDAdcManager(const WLEDAdcManager&) = delete;
  WLEDAdcManager& operator=(const WLEDAdcManager&) = delete;

  static bool _pinToChannel(uint8_t pin, adc_channel_t* ch);

  bool _initContinuousADC();
  void _endContinuousADC();
  void _drainToCache();
  bool _oneshotRead(adc_channel_t ch, int* outRaw);
  bool _initCali();

  struct ContinuousCtx;          // forward declaration
  SemaphoreHandle_t _mutex;
  adc_cali_handle_t _cali;
  ContinuousCtx*    _ctx;        // nullptr when continuous mode is idle
};

// override of native Arduino functions for compatibility with external usermods

#undef analogRead
#define analogRead(pin) WLEDAdcManager::instance().analogRead(pin)
#undef analogReadMilliVolts
#define analogReadMilliVolts(pin) WLEDAdcManager::instance().analogReadMilliVolts(pin)
// ESP8266: do not override analogRead
// we could add analogReadMilliVolts() which would just return (int)(analogRead(pin) / 1023.0f);
#endif // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
#endif // ARDUINO_ARCH_ESP32
