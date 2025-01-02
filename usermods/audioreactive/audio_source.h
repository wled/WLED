#pragma once

/* 
   @title     MoonModules WLED - audioreactive usermod
   @file      audio_source.h
   @repo      https://github.com/MoonModules/WLED, submit changes to this file as PRs to MoonModules/WLED
   @Authors   https://github.com/MoonModules/WLED/commits/mdev/
   @Copyright © 2024 Github MoonModules Commit Authors (contact moonmodules@icloud.com for details)
   @license   Licensed under the EUPL-1.2 or later

*/


#ifdef ARDUINO_ARCH_ESP32
#include <Wire.h>
#include "wled.h"
#include <driver/i2s.h>
#include <driver/adc.h>
#include <soc/i2s_reg.h>  // needed for SPH0465 timing workaround (classic ESP32)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C3)
#include <driver/adc_deprecated.h>
#include <driver/adc_types_deprecated.h>
#endif
// type of i2s_config_t.SampleRate was changed from "int" to "unsigned" in IDF 4.4.x
#define SRate_t uint32_t
#else
#define SRate_t int
#endif

constexpr i2s_port_t AR_I2S_PORT = I2S_NUM_0;       // I2S port to use (do not change!  I2S_NUM_1 possible but this has 
                                                    // strong limitations -> no MCLK routing, no ADC support, no PDM support

//#include <driver/i2s_std.h>
//#include <driver/i2s_pdm.h>
//#include <driver/i2s_tdm.h>
//#include <driver/gpio.h>

// see https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/chip-series-comparison.html#related-documents
// and https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html#overview-of-all-modes
#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(ESP8266) || defined(ESP8265)
  // there are two things in these MCUs that could lead to problems with audio processing:
  // * no floating point hardware (FPU) support - FFT uses float calculations. If done in software, a strong slow-down can be expected (between 8x and 20x)
  // * single core, so FFT task might slow down other things like LED updates
  #if !defined(SOC_I2S_NUM) || (SOC_I2S_NUM < 1)
  #error This audio reactive usermod does not support ESP32-C2 or ESP32-C3.
  #else
  #warning This audio reactive usermod does not support ESP32-C2 and ESP32-C3.
  #endif
#endif

/* ToDo: remove. ES7243 is controlled via compiler defines
   Until this configuration is moved to the webinterface
*/

// if you have problems to get your microphone work on the left channel, uncomment the following line
//#define I2S_USE_RIGHT_CHANNEL    // (experimental) define this to use right channel (digital mics only)

// Uncomment the line below to utilize ADC1 _exclusively_ for I2S sound input.
// benefit: analog mic inputs will be sampled contiously -> better response times and less "glitches"
// WARNING: this option WILL lock-up your device in case that any other analogRead() operation is performed; 
//          for example if you want to read "analog buttons"
//#define I2S_GRAB_ADC1_COMPLETELY // (experimental) continuously sample analog ADC microphone. WARNING will cause analogRead() lock-up

// data type requested from the I2S driver - currently we always use 32bit
//#define I2S_USE_16BIT_SAMPLES   // (experimental) define this to request 16bit - more efficient but possibly less compatible

#if defined(WLED_ENABLE_HUB75MATRIX) && defined(CONFIG_IDF_TARGET_ESP32)
  // this is bitter, but necessary to survive
  #define I2S_USE_16BIT_SAMPLES
#endif

#ifdef I2S_USE_16BIT_SAMPLES
#define I2S_SAMPLE_RESOLUTION I2S_BITS_PER_SAMPLE_16BIT
#define I2S_datatype int16_t
#define I2S_unsigned_datatype uint16_t
#define I2S_data_size I2S_BITS_PER_CHAN_16BIT
#undef  I2S_SAMPLE_DOWNSCALE_TO_16BIT
#else
#define I2S_SAMPLE_RESOLUTION I2S_BITS_PER_SAMPLE_32BIT
//#define I2S_SAMPLE_RESOLUTION I2S_BITS_PER_SAMPLE_24BIT 
#define I2S_datatype int32_t
#define I2S_unsigned_datatype uint32_t
#define I2S_data_size I2S_BITS_PER_CHAN_32BIT
#define I2S_SAMPLE_DOWNSCALE_TO_16BIT
#endif

/* There are several (confusing) options  in IDF 4.4.x:
 * I2S_CHANNEL_FMT_RIGHT_LEFT, I2S_CHANNEL_FMT_ALL_RIGHT and I2S_CHANNEL_FMT_ALL_LEFT stands for stereo mode, which means two channels will transport different data.
 * I2S_CHANNEL_FMT_ONLY_RIGHT and I2S_CHANNEL_FMT_ONLY_LEFT they are mono mode, both channels will only transport same data.
 * I2S_CHANNEL_FMT_MULTIPLE means TDM channels, up to 16 channel will available, and they are stereo as default.
 * if you want to receive two channels, one is the actual data from microphone and another channel is suppose to receive 0, it's different data in two channels, you need to choose I2S_CHANNEL_FMT_RIGHT_LEFT in this case.
*/

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)) && (ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(4, 4, 8)) // should be fixed in IDF 4.4.5, however arduino-esp32 2.0.14 - 2.0.17 did an "I2S rollback" to 4.4.4
// espressif bug: only_left has no sound, left and right are swapped 
// https://github.com/espressif/esp-idf/issues/9635  I2S mic not working since 4.4 (IDFGH-8138)
// https://github.com/espressif/esp-idf/issues/8538  I2S channel selection issue? (IDFGH-6918)
// https://github.com/espressif/esp-idf/issues/6625  I2S: left/right channels are swapped for read (IDFGH-4826)
#ifdef I2S_USE_RIGHT_CHANNEL
#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_LEFT
#define I2S_MIC_CHANNEL_TEXT "right channel only (work-around swapped channel bug in IDF 4.4)."
#define I2S_PDM_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_RIGHT
#define I2S_PDM_MIC_CHANNEL_TEXT "right channel only"
#else
//#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ALL_LEFT
//#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_RIGHT_LEFT
#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_RIGHT
#define I2S_MIC_CHANNEL_TEXT "left channel only (work-around swapped channel bug in IDF 4.4)."
#define I2S_PDM_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_LEFT
#define I2S_PDM_MIC_CHANNEL_TEXT "left channel only."
#endif

#else
// not swapped
#ifdef I2S_USE_RIGHT_CHANNEL
#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_RIGHT
#define I2S_MIC_CHANNEL_TEXT "right channel only."
#else
#define I2S_MIC_CHANNEL I2S_CHANNEL_FMT_ONLY_LEFT
#define I2S_MIC_CHANNEL_TEXT "left channel only."
#endif
#define I2S_PDM_MIC_CHANNEL I2S_MIC_CHANNEL
#define I2S_PDM_MIC_CHANNEL_TEXT I2S_MIC_CHANNEL_TEXT

#endif


// max number of samples for a single i2s_read --> size of global buffer.
#define I2S_SAMPLES_MAX 512  // same as samplesFFT

/* Interface class
   AudioSource serves as base class for all microphone types
   This enables accessing all microphones with one single interface
   which simplifies the caller code
*/
class AudioSource {
  public:
    /* All public methods are virtual, so they can be overridden
       Everything but the destructor is also removed, to make sure each mic
       Implementation provides its version of this function
    */
    virtual ~AudioSource() {};

    /* Initialize
       This function needs to take care of anything that needs to be done
       before samples can be obtained from the microphone.
    */
    virtual void initialize(int8_t = I2S_PIN_NO_CHANGE, int8_t = I2S_PIN_NO_CHANGE, int8_t = I2S_PIN_NO_CHANGE, int8_t = I2S_PIN_NO_CHANGE) = 0;

    /* Deinitialize
       Release all resources and deactivate any functionality that is used
       by this microphone
    */
    virtual void deinitialize() = 0;

    /* getSamples
       Read num_samples from the microphone, and store them in the provided
       buffer
    */
    virtual void getSamples(float *buffer, uint16_t num_samples) = 0;

    /* check if the audio source driver was initialized successfully */
    virtual bool isInitialized(void) {return(_initialized);}

    /* identify Audiosource type - I2S-ADC or I2S-digital */
    typedef enum{Type_unknown=0, Type_I2SAdc=1, Type_I2SDigital=2} AudioSourceType;
    virtual AudioSourceType getType(void) {return(Type_I2SDigital);}               // default is "I2S digital source" - ADC type overrides this method
 
  protected:
    /* Post-process audio sample - currently on needed for I2SAdcSource*/
    virtual I2S_datatype postProcessSample(I2S_datatype sample_in) {return(sample_in);}   // default method can be overriden by instances (ADC) that need sample postprocessing

    // Private constructor, to make sure it is not callable except from derived classes
    AudioSource(SRate_t sampleRate, int blockSize, float sampleScale, bool i2sMaster) :
      _sampleRate(sampleRate),
      _blockSize(blockSize),
      _initialized(false),
      _i2sMaster(i2sMaster),
      _sampleScale(sampleScale)
    {};

    SRate_t _sampleRate;            // Microphone sampling rate
    int _blockSize;                 // I2S block size
    bool _initialized;              // Gets set to true if initialization is successful
    bool _i2sMaster;                // when false, ESP32 will be in I2S SLAVE mode (for devices that only operate in MASTER mode). Only works in newer IDF >= 4.4.x
    float _sampleScale;             // pre-scaling factor for I2S samples
    I2S_datatype newSampleBuffer[I2S_SAMPLES_MAX+4] = { 0 }; // global buffer for i2s_read
};

/* Basic I2S microphone source
   All functions are marked virtual, so derived classes can replace them
   WARNING: i2sMaster = false is experimental, and most likely will not work
*/
class I2SSource : public AudioSource {
  public:
    I2SSource(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f, bool i2sMaster=true) :
      AudioSource(sampleRate, blockSize, sampleScale, i2sMaster) {
      _config = {
        .mode = i2sMaster ? i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX) : i2s_mode_t(I2S_MODE_SLAVE | I2S_MODE_RX),
        .sample_rate = _sampleRate,
        .bits_per_sample = I2S_SAMPLE_RESOLUTION,  // slave mode: may help to set this to 96000, as the other side (master) controls sample rates
        .channel_format = I2S_MIC_CHANNEL,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        //.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
#ifdef WLEDMM_FASTPATH
  #ifdef WLED_ENABLE_HUB75MATRIX
        .intr_alloc_flags = ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_LEVEL1,    // HUB75 seems to get into trouble if we allocate a higher priority interrupt
        .dma_buf_count = 18,                                            // 100ms buffer (128 * dma_buf_count / sampleRate)
  #else
      #if CONFIG_IDF_TARGET_ESP32 && !defined(BOARD_HAS_PSRAM)          // still need to test on boards with PSRAM
        .intr_alloc_flags = ESP_INTR_FLAG_IRAM|ESP_INTR_FLAG_LEVEL2|ESP_INTR_FLAG_LEVEL3,  // IRAM flag reduces missed samples
      #else
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2|ESP_INTR_FLAG_LEVEL3,  // seems to reduce noise
      #endif
        .dma_buf_count = 24,                                            // 140ms buffer (128 * dma_buf_count / sampleRate)
  #endif
#else
  #ifdef WLED_ENABLE_HUB75MATRIX
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,                      // HUB75 seems to get into trouble if we allocate a higher priority interrupt
  #else
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
  #endif
        .dma_buf_count = 8,
#endif
        .dma_buf_len = _blockSize,
        .use_apll = 0,
        //.fixed_mclk = 0,
        .bits_per_chan = I2S_data_size,
#else
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = _blockSize,
        .use_apll = false
#endif
      };
    }

    virtual void initialize(int8_t i2swsPin = I2S_PIN_NO_CHANGE, int8_t i2ssdPin = I2S_PIN_NO_CHANGE, int8_t i2sckPin = I2S_PIN_NO_CHANGE, int8_t mclkPin = I2S_PIN_NO_CHANGE) {
      DEBUGSR_PRINTLN("I2SSource:: initialize().");
      if (i2swsPin != I2S_PIN_NO_CHANGE && i2ssdPin != I2S_PIN_NO_CHANGE) {
        if (!pinManager.allocatePin(i2swsPin, true, PinOwner::UM_Audioreactive) ||
            !pinManager.allocatePin(i2ssdPin, false, PinOwner::UM_Audioreactive)) { // #206
          ERRORSR_PRINTF("\nAR: Failed to allocate I2S pins: ws=%d, sd=%d\n",  i2swsPin, i2ssdPin); 
          return;
        }
      }

      // i2ssckPin needs special treatment, since it might be unused on PDM mics
      if (i2sckPin != I2S_PIN_NO_CHANGE) {
        if (!pinManager.allocatePin(i2sckPin, true, PinOwner::UM_Audioreactive)) {
          ERRORSR_PRINTF("\nAR: Failed to allocate I2S pins: sck=%d\n",  i2sckPin); 
          return;
        }
      } else {
        #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
          #if !defined(SOC_I2S_SUPPORTS_PDM_RX)
          #warning this MCU does not support PDM microphones
          #endif
        #endif
        #if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
        // This is an I2S PDM microphone, these microphones only use a clock and
        // data line, to make it simpler to debug, use the WS pin as CLK and SD pin as DATA
        // example from espressif: https://github.com/espressif/esp-idf/blob/release/v4.4/examples/peripherals/i2s/i2s_audio_recorder_sdcard/main/i2s_recorder_main.c

        // note to self: PDM has known bugs on S3, and does not work on C3 
        //  * S3: PDM sample rate only at 50% of expected rate: https://github.com/espressif/esp-idf/issues/9893
        //  * S3: I2S PDM has very low amplitude: https://github.com/espressif/esp-idf/issues/8660
        //  * C3: does not support PDM to PCM input. SoC would allow PDM RX, but there is no hardware to directly convert to PCM so it will not work. https://github.com/espressif/esp-idf/issues/8796

        _config.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM); // Change mode to pdm if clock pin not provided. PDM is not supported on ESP32-S2. PDM RX not supported on ESP32-C3
        _config.channel_format =I2S_PDM_MIC_CHANNEL;                             // seems that PDM mono mode always uses left channel.
        _config.use_apll = true;                                                 // experimental - use aPLL clock source to improve sampling quality
        //_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;                     // not needed
        #endif
      }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
      if ((_i2sMaster == false) && (_config.mode & I2S_MODE_SLAVE)) { // I2S slave mode (experimental).
          // Seems we need to drive clocks in slave mode
          _config.use_apll = true;
          _config.fixed_mclk = 512 * int(_config.sample_rate);
      }

      if (mclkPin != I2S_PIN_NO_CHANGE) {
        _config.use_apll = true; // experimental - use aPLL clock source to improve sampling quality, and to avoid glitches.
        // //_config.fixed_mclk = 512 * _sampleRate;
        // //_config.fixed_mclk = 256 * _sampleRate;
      }
      
      #if !defined(SOC_I2S_SUPPORTS_APLL)
        #warning this MCU does not have an APLL high accuracy clock for audio
        // S3: not supported; S2: supported; C3: not supported
        _config.use_apll = false; // APLL not supported on this MCU
      #endif
      #if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
      if (ESP.getChipRevision() == 0) _config.use_apll = false; // APLL is broken on ESP32 revision 0
      #endif
      #if defined(WLED_ENABLE_HUB75MATRIX)
        _config.use_apll = false; // APLL needed for HUB75 DMA driver ?
      #endif
#endif

      if (_i2sMaster == false) {
        DEBUG_PRINTLN(F("AR: Warning - i2S SLAVE mode is experimental!"));
        if (_config.mode & I2S_MODE_PDM) {
          // APLL does not work in DAC or PDM "Slave Mode": https://github.com/espressif/esp-idf/issues/1244, https://github.com/espressif/esp-idf/issues/2634
          _config.use_apll = false;
          _config.fixed_mclk =  0;
        }
        if ((_config.mode & I2S_MODE_MASTER) != 0) {
          DEBUG_PRINTLN("AR: (oops) I2S SLAVE mode requested but not configured!");
        }
      }

      // Reserve the master clock pin if provided
      _mclkPin = mclkPin;
      if (mclkPin != I2S_PIN_NO_CHANGE) {
        if(!pinManager.allocatePin(mclkPin, true, PinOwner::UM_Audioreactive)) { 
          ERRORSR_PRINTF("\nAR: Failed to allocate I2S pin: MCLK=%d\n",  mclkPin); 
          return;
        } else
        _routeMclk(mclkPin);
      }

      _pinConfig = {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
        .mck_io_num = mclkPin,            // "classic" ESP32 supports setting MCK on GPIO0/GPIO1/GPIO3 only. i2s_set_pin() will fail if wrong mck_io_num is provided.
#endif
        .bck_io_num = i2sckPin,
        .ws_io_num = i2swsPin,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = i2ssdPin
      };

      //DEBUGSR_PRINTF("[AR] I2S: SD=%d, WS=%d, SCK=%d, MCLK=%d\n", i2ssdPin, i2swsPin, i2sckPin, mclkPin);

      esp_err_t err = i2s_driver_install(AR_I2S_PORT, &_config, 0, nullptr);
      if (err != ESP_OK) {
        ERRORSR_PRINTF("AR: Failed to install i2s driver: %d\n", err);
        return;
      }

      DEBUGSR_PRINTF("AR: I2S#0 driver %s aPLL; fixed_mclk=%d.\n", _config.use_apll? "uses":"without", _config.fixed_mclk);
      DEBUGSR_PRINTF("AR: %d bits, Sample scaling factor = %6.4f\n",  _config.bits_per_sample, _sampleScale);
      if(_config.mode & I2S_MODE_MASTER) {
        if (_config.mode & I2S_MODE_PDM) {
          DEBUGSR_PRINTLN(F("AR: I2S#0 driver installed in PDM MASTER mode."));
        } else { 
          DEBUGSR_PRINTLN(F("AR: I2S#0 driver installed in MASTER mode."));
        }
      } else {
        DEBUGSR_PRINTLN(F("AR: I2S#0 driver installed in SLAVE mode."));
      }

      err = i2s_set_pin(AR_I2S_PORT, &_pinConfig);
      if (err != ESP_OK) {
        ERRORSR_PRINTF("AR: Failed to set i2s pin config: %d\n", err);
        i2s_driver_uninstall(AR_I2S_PORT);  // uninstall already-installed driver
        return;
      }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
      err = i2s_set_clk(AR_I2S_PORT, _sampleRate, I2S_SAMPLE_RESOLUTION, I2S_CHANNEL_MONO);  // set bit clocks. Also takes care of MCLK routing if needed.
      if (err != ESP_OK) {
        ERRORSR_PRINTF("AR: Failed to configure i2s clocks: %d\n", err);
        i2s_driver_uninstall(AR_I2S_PORT);  // uninstall already-installed driver
        return;
      }
#endif
      _initialized = true;
    }

    virtual void deinitialize() {
      _initialized = false;
      esp_err_t err = i2s_driver_uninstall(AR_I2S_PORT);
      if (err != ESP_OK) {
        DEBUGSR_PRINTF("Failed to uninstall i2s driver: %d\n", err);
        return;
      }
      if (_pinConfig.ws_io_num   != I2S_PIN_NO_CHANGE) pinManager.deallocatePin(_pinConfig.ws_io_num,   PinOwner::UM_Audioreactive);
      if (_pinConfig.data_in_num != I2S_PIN_NO_CHANGE) pinManager.deallocatePin(_pinConfig.data_in_num, PinOwner::UM_Audioreactive);
      if (_pinConfig.bck_io_num  != I2S_PIN_NO_CHANGE) pinManager.deallocatePin(_pinConfig.bck_io_num,  PinOwner::UM_Audioreactive);
      // Release the master clock pin
      if (_mclkPin != I2S_PIN_NO_CHANGE) pinManager.deallocatePin(_mclkPin, PinOwner::UM_Audioreactive);
    }

    virtual void getSamples(float *buffer, uint16_t num_samples) {
      if (_initialized) {
        esp_err_t err;
        size_t bytes_read = 0;        /* Counter variable to check if we actually got enough data */

        memset(buffer, 0, sizeof(float) * num_samples);  // clear output buffer
        I2S_datatype *newSamples = newSampleBuffer; // use global input buffer
        if (num_samples > I2S_SAMPLES_MAX) num_samples = I2S_SAMPLES_MAX; // protect the buffer from overflow

        err = i2s_read(AR_I2S_PORT, (void *)newSamples, num_samples * sizeof(I2S_datatype), &bytes_read, portMAX_DELAY);
        if (err != ESP_OK) {
          DEBUGSR_PRINTF("Failed to get samples: %d\n", err);
          return;
        }

        // For correct operation, we need to read exactly sizeof(samples) bytes from i2s
        if (bytes_read != (num_samples * sizeof(I2S_datatype))) {
          DEBUGSR_PRINTF("Failed to get enough samples: wanted: %d read: %d\n", num_samples * sizeof(I2S_datatype), bytes_read);
          return;
        }

        // Store samples in sample buffer and update DC offset
        for (int i = 0; i < num_samples; i++) {

          newSamples[i] = postProcessSample(newSamples[i]);  // perform postprocessing (needed for ADC samples)
          
          float currSample = 0.0f;
#ifdef I2S_SAMPLE_DOWNSCALE_TO_16BIT
              currSample = (float) newSamples[i] / 65536.0f;      // 32bit input -> 16bit; keeping lower 16bits as decimal places
#else
              currSample = (float) newSamples[i];                 // 16bit input -> use as-is
#endif
          buffer[i] = currSample;
          buffer[i] *= _sampleScale;                              // scale samples
        }
      }
    }

  protected:
    void _routeMclk(int8_t mclkPin) {
#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
  // MCLK routing by writing registers is not needed any more with IDF > 4.4.0
  #if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0)
    // this way of MCLK routing only works on "classic" ESP32
      /* Enable the mclk routing depending on the selected mclk pin (ESP32: only 0,1,3)
          Only I2S_NUM_0 is supported
      */
      if (mclkPin == GPIO_NUM_0) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
        WRITE_PERI_REG(PIN_CTRL,0xFFF0);
      } else if (mclkPin == GPIO_NUM_1) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD_CLK_OUT3);
        WRITE_PERI_REG(PIN_CTRL, 0xF0F0);
      } else {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0RXD_U, FUNC_U0RXD_CLK_OUT2);
        WRITE_PERI_REG(PIN_CTRL, 0xFF00);
      }
  #endif
#endif
    }

    i2s_config_t _config;
    i2s_pin_config_t _pinConfig;
    int8_t _mclkPin;
};

/* ES7243 Microphone
   This is an I2S microphone that requires initialization over
   I2C before I2S data can be received
*/
class ES7243 : public I2SSource {
  private:
    // I2C initialization functions for ES7243
    void _es7243I2cBegin() {
      Wire.setClock(100000);
    }

    void _es7243I2cWrite(uint8_t reg, uint8_t val) {
      #ifndef ES7243_ADDR
        #define ES7243_ADDR 0x13   // default address
      #endif
      Wire.beginTransmission(ES7243_ADDR);
      Wire.write((uint8_t)reg);
      Wire.write((uint8_t)val);
      uint8_t i2cErr = Wire.endTransmission();  // i2cErr == 0 means OK
      if (i2cErr != 0) {
        DEBUGSR_PRINTF("AR: ES7243 I2C write failed with error=%d  (addr=0x%X, reg 0x%X, val 0x%X).\n", i2cErr, ES7243_ADDR, reg, val);
      }
    }

    void _es7243InitAdc() {
      _es7243I2cBegin();
      _es7243I2cWrite(0x00, 0x01);
      _es7243I2cWrite(0x06, 0x00);
      _es7243I2cWrite(0x05, 0x1B);
      _es7243I2cWrite(0x01, 0x00); // 0x00 for 24 bit to match INMP441 - not sure if this needs adjustment to get 16bit samples from I2S
      _es7243I2cWrite(0x08, 0x43);
      _es7243I2cWrite(0x05, 0x13);
    }

public:
    ES7243(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f, bool i2sMaster=true) :
      I2SSource(sampleRate, blockSize, sampleScale, i2sMaster) {
      _config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    };

    void initialize(int8_t i2swsPin, int8_t i2ssdPin, int8_t i2sckPin, int8_t mclkPin) {
      DEBUGSR_PRINTLN("ES7243:: initialize();");

      // if ((i2sckPin < 0) || (mclkPin < 0)) { // WLEDMM not sure if this check is needed here, too
      //   ERRORSR_PRINTF("\nAR: invalid I2S pin: SCK=%d, MCLK=%d\n", i2sckPin, mclkPin);
      //   return;
      // }
      if ((i2c_sda < 0) || (i2c_scl < 0)) {  // check that global I2C pins are not "undefined"
        ERRORSR_PRINTF("\nAR: invalid ES7243 global I2C pins: SDA=%d, SCL=%d\n", i2c_sda, i2c_scl); 
        return;
      }
      if (!pinManager.joinWire(i2c_sda, i2c_scl)) {    // WLEDMM specific: start I2C with globally defined pins
        ERRORSR_PRINTF("\nAR: failed to join I2C bus with SDA=%d, SCL=%d\n", i2c_sda, i2c_scl); 
        return;
      }

      // First route mclk, then configure ADC over I2C, then configure I2S
      _es7243InitAdc();
      I2SSource::initialize(i2swsPin, i2ssdPin, i2sckPin, mclkPin);
    }

    void deinitialize() {
      I2SSource::deinitialize();
    }
};

/* ES8388 Sound Module
   This is an I2S sound processing unit that requires initialization over
   I2C before I2S data can be received. 
*/
class ES8388Source : public I2SSource {
  private:
    // I2C initialization functions for ES8388
    void _es8388I2cBegin() {
      Wire.setClock(100000);
    }

    void _es8388I2cWrite(uint8_t reg, uint8_t val) {
      #ifndef ES8388_ADDR
        #define ES8388_ADDR 0x10   // default address
      #endif
      Wire.beginTransmission(ES8388_ADDR);
      Wire.write((uint8_t)reg);
      Wire.write((uint8_t)val);
      uint8_t i2cErr = Wire.endTransmission();  // i2cErr == 0 means OK
      if (i2cErr != 0) {
        DEBUGSR_PRINTF("AR: ES8388 I2C write failed with error=%d  (addr=0x%X, reg 0x%X, val 0x%X).\n", i2cErr, ES8388_ADDR, reg, val);
      }
    }

    void _es8388InitAdc() {
      // https://dl.radxa.com/rock2/docs/hw/ds/ES8388%20user%20Guide.pdf Section 10.1
      // http://www.everest-semi.com/pdf/ES8388%20DS.pdf Better spec sheet, more clear. 
      // https://docs.google.com/spreadsheets/d/1CN3MvhkcPVESuxKyx1xRYqfUit5hOdsG45St9BCUm-g/edit#gid=0 generally
      // Sets ADC to around what AudioReactive expects, and loops line-in to line-out/headphone for monitoring.
      // Registries are decimal, settings are binary as that's how everything is listed in the docs
      // ...which makes it easier to reference the docs.
      //
      _es8388I2cBegin(); 
      _es8388I2cWrite( 8,0b00000000); // I2S to slave
      _es8388I2cWrite( 2,0b11110011); // Power down DEM and STM
      _es8388I2cWrite(43,0b10000000); // Set same LRCK
      _es8388I2cWrite( 0,0b00000101); // Set chip to Play & Record Mode
      _es8388I2cWrite(13,0b00000010); // Set MCLK/LRCK ratio to 256
      _es8388I2cWrite( 1,0b01000000); // Power up analog and lbias
      _es8388I2cWrite( 3,0b00000000); // Power up ADC, Analog Input, and Mic Bias
      _es8388I2cWrite( 4,0b11111100); // Power down DAC, Turn on LOUT1 and ROUT1 and LOUT2 and ROUT2 power
      _es8388I2cWrite( 2,0b01000000); // Power up DEM and STM and undocumented bit for "turn on line-out amp"

      // #define use_es8388_mic

    #ifdef use_es8388_mic
      // The mics *and* line-in are BOTH connected to LIN2/RIN2 on the AudioKit
      // so there's no way to completely eliminate the mics. It's also hella noisy. 
      // Line-in works OK on the AudioKit, generally speaking, as the mics really need
      // amplification to be noticeable in a quiet room. If you're in a very loud room, 
      // the mics on the AudioKit WILL pick up sound even in line-in mode. 
      // TL;DR: Don't use the AudioKit for anything, use the LyraT. 
      //
      // The LyraT does a reasonable job with mic input as configured below.

      // Pick one of these. If you have to use the mics, use a LyraT over an AudioKit if you can:
      _es8388I2cWrite(10,0b00000000); // Use Lin1/Rin1 for ADC input (mic on LyraT)
      //_es8388I2cWrite(10,0b01010000); // Use Lin2/Rin2 for ADC input (mic *and* line-in on AudioKit)
      
      _es8388I2cWrite( 9,0b10001000); // Select Analog Input PGA Gain for ADC to +24dB (L+R)
      _es8388I2cWrite(16,0b00000000); // Set ADC digital volume attenuation to 0dB (left)
      _es8388I2cWrite(17,0b00000000); // Set ADC digital volume attenuation to 0dB (right)
      _es8388I2cWrite(38,0b00011011); // Mixer - route LIN1/RIN1 to output after mic gain

      _es8388I2cWrite(39,0b01000000); // Mixer - route LIN to mixL, +6dB gain
      _es8388I2cWrite(42,0b01000000); // Mixer - route RIN to mixR, +6dB gain
      _es8388I2cWrite(46,0b00100001); // LOUT1VOL - 0b00100001 = +4.5dB
      _es8388I2cWrite(47,0b00100001); // ROUT1VOL - 0b00100001 = +4.5dB
      _es8388I2cWrite(48,0b00100001); // LOUT2VOL - 0b00100001 = +4.5dB
      _es8388I2cWrite(49,0b00100001); // ROUT2VOL - 0b00100001 = +4.5dB

      // Music ALC - the mics like Auto Level Control
      // You can also use this for line-in, but it's not really needed.
      //
      _es8388I2cWrite(18,0b11111000); // ALC: stereo, max gain +35.5dB, min gain -12dB 
      _es8388I2cWrite(19,0b00110000); // ALC: target -1.5dB, 0ms hold time
      _es8388I2cWrite(20,0b10100110); // ALC: gain ramp up = 420ms/93ms, gain ramp down = check manual for calc
      _es8388I2cWrite(21,0b00000110); // ALC: use "ALC" mode, no zero-cross, window 96 samples
      _es8388I2cWrite(22,0b01011001); // ALC: noise gate threshold, PGA gain constant, noise gate enabled 
    #else
      _es8388I2cWrite(10,0b01010000); // Use Lin2/Rin2 for ADC input ("line-in")
      _es8388I2cWrite( 9,0b00000000); // Select Analog Input PGA Gain for ADC to 0dB (L+R)
      _es8388I2cWrite(16,0b01000000); // Set ADC digital volume attenuation to -32dB (left)
      _es8388I2cWrite(17,0b01000000); // Set ADC digital volume attenuation to -32dB (right)
      _es8388I2cWrite(38,0b00001001); // Mixer - route LIN2/RIN2 to output

      _es8388I2cWrite(39,0b01010000); // Mixer - route LIN to mixL, 0dB gain
      _es8388I2cWrite(42,0b01010000); // Mixer - route RIN to mixR, 0dB gain
      _es8388I2cWrite(46,0b00011011); // LOUT1VOL - 0b00011110 = +0dB, 0b00011011 = LyraT balance fix
      _es8388I2cWrite(47,0b00011110); // ROUT1VOL - 0b00011110 = +0dB
      _es8388I2cWrite(48,0b00011110); // LOUT2VOL - 0b00011110 = +0dB
      _es8388I2cWrite(49,0b00011110); // ROUT2VOL - 0b00011110 = +0dB
    #endif

    }

  public:
    ES8388Source(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f, bool i2sMaster=true) :
      I2SSource(sampleRate, blockSize, sampleScale, i2sMaster) {
      _config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    };

    void initialize(int8_t i2swsPin, int8_t i2ssdPin, int8_t i2sckPin, int8_t mclkPin) {
      DEBUGSR_PRINTLN("ES8388Source:: initialize();");

      // if ((i2sckPin < 0) || (mclkPin < 0)) { // WLEDMM not sure if this check is needed here, too
      //    ERRORSR_PRINTF("\nAR: invalid I2S ES8388 pin: SCK=%d, MCLK=%d\n", i2sckPin, mclkPin); 
      //    return;
      // }
      // BUG: "use global I2C pins" are valid as -1, and -1 is seen as invalid here.
      // Workaround: Set I2C pins here, which will also set them globally.
      // Bug also exists in ES7243.
       if ((i2c_sda < 0) || (i2c_scl < 0)) {  // check that global I2C pins are not "undefined"
        ERRORSR_PRINTF("\nAR: invalid ES8388 global I2C pins: SDA=%d, SCL=%d\n", i2c_sda, i2c_scl); 
        return;
      }
      if (!pinManager.joinWire(i2c_sda, i2c_scl)) {    // WLEDMM specific: start I2C with globally defined pins
        ERRORSR_PRINTF("\nAR: failed to join I2C bus with SDA=%d, SCL=%d\n", i2c_sda, i2c_scl); 
        return;
      }

      // First route mclk, then configure ADC over I2C, then configure I2S
      _es8388InitAdc();
      I2SSource::initialize(i2swsPin, i2ssdPin, i2sckPin, mclkPin);
    }

    void deinitialize() {
      I2SSource::deinitialize();
    }

};

/* ES8311 Sound Module
   This is an I2S sound processing unit that requires initialization over
   I2C before I2S data can be received. 
*/
class ES8311Source : public I2SSource {
  private:
    // I2C initialization functions for es8311
    void _es8311I2cBegin() {
      Wire.setClock(100000);
    }

    void _es8311I2cWrite(uint8_t reg, uint8_t val) {
      #ifndef ES8311_ADDR
        #define ES8311_ADDR 0x18   // default address is... foggy
      #endif
      Wire.beginTransmission(ES8311_ADDR);
      Wire.write((uint8_t)reg);
      Wire.write((uint8_t)val);
      uint8_t i2cErr = Wire.endTransmission();  // i2cErr == 0 means OK
      if (i2cErr != 0) {
        DEBUGSR_PRINTF("AR: ES8311 I2C write failed with error=%d  (addr=0x%X, reg 0x%X, val 0x%X).\n", i2cErr, ES8311_ADDR, reg, val);
      }
    }

    void _es8311InitAdc() {
      // 
      // Currently only tested with the ESP32-P4 boards with the onboard mic.
      // Datasheet with I2C commands: https://dl.xkwy2018.com/downloads/RK3588/01_Official%20Release/04_Product%20Line%20Branch_NVR/02_Key%20Device%20Specifications/ES8311%20DS.pdf
      //
      _es8311I2cBegin(); 
      _es8311I2cWrite(0x00, 0b00011111); // RESET, default value
      _es8311I2cWrite(0x45, 0b00000000); // GP, default value
      _es8311I2cWrite(0x01, 0b00111010); // CLOCK MANAGER was 0b00110000 trying 0b00111010 (MCLK enable?)

      _es8311I2cWrite(0x02, 0b00000000); // 22050hz calculated
      _es8311I2cWrite(0x05, 0b00000000); // 22050hz calculated
      _es8311I2cWrite(0x03, 0b00010000); // 22050hz calculated
      _es8311I2cWrite(0x04, 0b00010000); // 22050hz calculated
      _es8311I2cWrite(0x07, 0b00000000); // 22050hz calculated
      _es8311I2cWrite(0x08, 0b11111111); // 22050hz calculated
      _es8311I2cWrite(0x06, 0b11100011); // 22050hz calculated

      _es8311I2cWrite(0x16, 0b00100100); // ADC was 0b00000011 trying 0b00100100 was good
      _es8311I2cWrite(0x0B, 0b00000000); // SYSTEM at default
      _es8311I2cWrite(0x0C, 0b00100000); // SYSTEM was 0b00001111 trying 0b00100000
      _es8311I2cWrite(0x10, 0b00010011); // SYSTEM was 0b00011111 trying 0b00010011
      _es8311I2cWrite(0x11, 0b01111100); // SYSTEM was 0b01111111 trying 0b01111100
      _es8311I2cWrite(0x00, 0b11000000); // *** RESET (again - seems important?)
      _es8311I2cWrite(0x01, 0b00111010); // *** CLOCK MANAGER was 0b00111111 trying 0b00111010 (again?? seems important)
      _es8311I2cWrite(0x14, 0b00010000); // *** SYSTEM was 0b00011010 trying 0b00010000 (PGA gain)
      _es8311I2cWrite(0x0A, 0b00001000); // *** SDP OUT, was 0b00001100 trying 0b00001000 (I2S 32-bit)
      _es8311I2cWrite(0x0E, 0b00000010); // *** SYSTEM was 0b00000010 trying 0b00000010
      _es8311I2cWrite(0x0F, 0b01000100); // SYSTEM was 0b01000100
      _es8311I2cWrite(0x15, 0b00010000); // ADC soft ramp (disabled 0000xxxx)
      _es8311I2cWrite(0x1B, 0b00000101); // ADC soft-mute was 0b00000101
      _es8311I2cWrite(0x1C, 0b01100101); // ADC EQ and offset freeze at 0b01100101 (bad at 0b00101100)
      _es8311I2cWrite(0x17, 0b10111111); // ADC volume was 0b11111111 trying ADC volume 0b10111111 = 0db (maxgain)
      _es8311I2cWrite(0x18, 0b10000001); // ADC ALC enabled and AutoMute disabled.
      // _es8311I2cWrite(0x19, 0b11110100); // ADC ALC max and min - not sure how best to use this, default seems fine
    }

  public:
    ES8311Source(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f, bool i2sMaster=true) :
      I2SSource(sampleRate, blockSize, sampleScale, i2sMaster) {
      _config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    };

    void initialize(int8_t i2swsPin, int8_t i2ssdPin, int8_t i2sckPin, int8_t mclkPin) {
      DEBUGSR_PRINTLN("es8311Source:: initialize();");

      // if ((i2sckPin < 0) || (mclkPin < 0)) { // WLEDMM not sure if this check is needed here, too
      //    ERRORSR_PRINTF("\nAR: invalid I2S es8311 pin: SCK=%d, MCLK=%d\n", i2sckPin, mclkPin); 
      //    return;
      // }
      // BUG: "use global I2C pins" are valid as -1, and -1 is seen as invalid here.
      // Workaround: Set I2C pins here, which will also set them globally.
      // Bug also exists in ES7243.
       if ((i2c_sda < 0) || (i2c_scl < 0)) {  // check that global I2C pins are not "undefined"
        ERRORSR_PRINTF("\nAR: invalid es8311 global I2C pins: SDA=%d, SCL=%d\n", i2c_sda, i2c_scl); 
        return;
      }
      if (!pinManager.joinWire(i2c_sda, i2c_scl)) {    // WLEDMM specific: start I2C with globally defined pins
        ERRORSR_PRINTF("\nAR: failed to join I2C bus with SDA=%d, SCL=%d\n", i2c_sda, i2c_scl); 
        return;
      }

      // First route mclk, then configure ADC over I2C, then configure I2S
      _es8311InitAdc();
      I2SSource::initialize(i2swsPin, i2ssdPin, i2sckPin, mclkPin);
    }

    void deinitialize() {
      I2SSource::deinitialize();
    }

};

class WM8978Source : public I2SSource {
  private:
    // I2C initialization functions for WM8978
    void _wm8978I2cBegin() {
      Wire.setClock(400000);
    }

    void _wm8978I2cWrite(uint8_t reg, uint16_t val) {
      #ifndef WM8978_ADDR
        #define WM8978_ADDR 0x1A
      #endif
      char buf[2];
      buf[0] = (reg << 1) | ((val >> 8) & 0X01);
      buf[1] = val & 0XFF;
      Wire.beginTransmission(WM8978_ADDR);
      Wire.write((const uint8_t*)buf, 2);
      uint8_t i2cErr = Wire.endTransmission();  // i2cErr == 0 means OK
      if (i2cErr != 0) {
        DEBUGSR_PRINTF("AR: WM8978 I2C write failed with error=%d  (addr=0x%X, reg 0x%X, val 0x%X).\n", i2cErr, WM8978_ADDR, reg, val);
      }
    }

    void _wm8978InitAdc() {
      // https://www.mouser.com/datasheet/2/76/WM8978_v4.5-1141768.pdf
      // Sets ADC to around what AudioReactive expects, and loops line-in to line-out/headphone for monitoring.
      // Registries are decimal, settings are 9-bit binary as that's how everything is listed in the docs
      // ...which makes it easier to reference the docs.
      //
      _wm8978I2cBegin(); 

      _wm8978I2cWrite( 0,0b000000000); // Reset all settings
      _wm8978I2cWrite( 1,0b000111110); // Power Management 1 - power off most things, but enable mic bias and I/O tie-off to help mitigate mic leakage.
      _wm8978I2cWrite( 2,0b110111111); // Power Management 2 - enable output and amp stages (amps may lift signal but it works better on the ADCs)
      _wm8978I2cWrite( 3,0b000001100); // Power Management 3 - enable L&R output mixers

      #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
      _wm8978I2cWrite( 4,0b001010000); // Audio Interface - standard I2S, 24-bit
      #else
      _wm8978I2cWrite( 4,0b001001000); // Audio Interface - left-justified I2S, 24-bit
      #endif
      
      _wm8978I2cWrite( 6,0b000000000); // Clock generation control - use external mclk
      _wm8978I2cWrite( 7,0b000000100); // Sets sample rate to ~24kHz (only used for internal calculations, not I2S)
      _wm8978I2cWrite(14,0b010001000); // 128x ADC oversampling - high pass filter disabled as it kills the bass response
      _wm8978I2cWrite(43,0b000110000); // Mute signal paths we don't use
      _wm8978I2cWrite(44,0b100000000); // Disconnect microphones
      _wm8978I2cWrite(45,0b111000000); // Mute signal paths we don't use
      _wm8978I2cWrite(46,0b111000000); // Mute signal paths we don't use
      _wm8978I2cWrite(47,0b001000000); // 0dB gain on left line-in
      _wm8978I2cWrite(48,0b001000000); // 0dB gain on right line-in
      _wm8978I2cWrite(49,0b000000011); // Mixer thermal shutdown enable and unused IOs to 30kΩ
      _wm8978I2cWrite(50,0b000010110); // Output mixer enable only left bypass at 0dB gain
      _wm8978I2cWrite(51,0b000010110); // Output mixer enable only right bypass at 0dB gain
      _wm8978I2cWrite(52,0b110111001); // Left line-out enabled at 0dB gain
      _wm8978I2cWrite(53,0b110111001); // Right line-out enabled at 0db gain
      _wm8978I2cWrite(54,0b111000000); // Mute left speaker output
      _wm8978I2cWrite(55,0b111000000); // Mute right speaker output

    }

  public:
    WM8978Source(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f, bool i2sMaster=true) :
      I2SSource(sampleRate, blockSize, sampleScale, i2sMaster) {
      _config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    };

    void initialize(int8_t i2swsPin, int8_t i2ssdPin, int8_t i2sckPin, int8_t mclkPin) {
      DEBUGSR_PRINTLN("WM8978Source:: initialize();");

      // if ((i2sckPin < 0) || (mclkPin < 0)) { // WLEDMM not sure if this check is needed here, too
      //    ERRORSR_PRINTF("\nAR: invalid I2S WM8978 pin: SCK=%d, MCLK=%d\n", i2sckPin, mclkPin); 
      //    return;
      // }
      // BUG: "use global I2C pins" are valid as -1, and -1 is seen as invalid here.
      // Workaround: Set I2C pins here, which will also set them globally.
      // Bug also exists in ES7243.
       if ((i2c_sda < 0) || (i2c_scl < 0)) {  // check that global I2C pins are not "undefined"
        ERRORSR_PRINTF("\nAR: invalid WM8978 global I2C pins: SDA=%d, SCL=%d\n", i2c_sda, i2c_scl); 
        return;
      }
      if (!pinManager.joinWire(i2c_sda, i2c_scl)) {    // WLEDMM specific: start I2C with globally defined pins
        ERRORSR_PRINTF("\nAR: failed to join I2C bus with SDA=%d, SCL=%d\n", i2c_sda, i2c_scl); 
        return;
      }

      // First route mclk, then configure ADC over I2C, then configure I2S
      _wm8978InitAdc();
      I2SSource::initialize(i2swsPin, i2ssdPin, i2sckPin, mclkPin);
    }

    void deinitialize() {
      I2SSource::deinitialize();
    }

};

class AC101Source : public I2SSource {
  private:
    // I2C initialization functions for WM8978
    void _ac101I2cBegin() {
      Wire.setClock(400000);
    }

    void _ac101I2cWrite(uint8_t reg_addr, uint16_t val) {
      #ifndef AC101_ADDR
        #define AC101_ADDR 0x1A
      #endif
      char send_buff[3];
      send_buff[0] = reg_addr;
      send_buff[1] = uint8_t((val >> 8) & 0xff);
      send_buff[2] = uint8_t(val & 0xff);
      Wire.beginTransmission(AC101_ADDR);
      Wire.write((const uint8_t*)send_buff, 3);
      uint8_t i2cErr = Wire.endTransmission();  // i2cErr == 0 means OK
      if (i2cErr != 0) {
        DEBUGSR_PRINTF("AR: AC101 I2C write failed with error=%d  (addr=0x%X, reg 0x%X, val 0x%X).\n", i2cErr, AC101_ADDR, reg_addr, val);
      }
    }

    void _ac101InitAdc() {
      // https://files.seeedstudio.com/wiki/ReSpeaker_6-Mics_Circular_Array_kit_for_Raspberry_Pi/reg/AC101_User_Manual_v1.1.pdf
      // This supports mostly the older AI Thinkier AudioKit A1S that has an AC101 chip
      // Newer versions use the ES3833 chip - which we also support.

      _ac101I2cBegin();

      #define CHIP_AUDIO_RS     0x00
      #define SYSCLK_CTRL       0x03
      #define MOD_CLK_ENA       0x04
      #define MOD_RST_CTRL      0x05
      #define I2S_SR_CTRL       0x06
      #define I2S1LCK_CTRL      0x10
      #define I2S1_SDOUT_CTRL   0x11
      #define I2S1_MXR_SRC      0x13
      #define ADC_DIG_CTRL      0x40
      #define ADC_APC_CTRL      0x50
      #define ADC_SRC           0x51
      #define ADC_SRCBST_CTRL   0x52
      #define OMIXER_DACA_CTRL  0x53
      #define OMIXER_SR         0x54
      #define HPOUT_CTRL        0x56

      _ac101I2cWrite(CHIP_AUDIO_RS, 0x123); // I think anything written here is a reset as 0x123 is kinda suss.

      delay(100);

      _ac101I2cWrite(SYSCLK_CTRL,       0b0000100000001000); // System Clock is I2S MCLK
      _ac101I2cWrite(MOD_CLK_ENA,       0b1000000000001000); // I2S and ADC Clock Enable
      _ac101I2cWrite(MOD_RST_CTRL,      0b1000000000001000); // I2S and ADC Clock Enable
      _ac101I2cWrite(I2S_SR_CTRL,       0b0100000000000000); // set to 22050hz just in case
      _ac101I2cWrite(I2S1LCK_CTRL,      0b1000000000110000); // set I2S slave mode, 24-bit word size
      _ac101I2cWrite(I2S1_SDOUT_CTRL,   0b1100000000000000); // I2S enable Left/Right channels
      _ac101I2cWrite(I2S1_MXR_SRC,      0b0010001000000000); // I2S digital Mixer, ADC L/R data
      _ac101I2cWrite(ADC_SRCBST_CTRL,   0b0000000000000100); // mute all boosts. last 3 bits are reserved/default
      _ac101I2cWrite(OMIXER_SR,         0b0000010000001000); // Line L/R to output mixer
      _ac101I2cWrite(ADC_SRC,           0b0000010000001000); // Line L/R to ADC
      _ac101I2cWrite(ADC_DIG_CTRL,      0b1000000000000000); // Enable ADC
      _ac101I2cWrite(ADC_APC_CTRL,      0b1011100100000000); // ADC L/R enabled, 0dB gain
      _ac101I2cWrite(OMIXER_DACA_CTRL,  0b0011111110000000); // L/R Analog Output Mixer enabled, headphone DC offset default
      _ac101I2cWrite(HPOUT_CTRL,        0b1111101111110001); // Headphone out from Analog Mixer stage, no reduction in volume

    }

  public:
    AC101Source(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f, bool i2sMaster=true) :
      I2SSource(sampleRate, blockSize, sampleScale, i2sMaster) {
      _config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    };

    void initialize(int8_t i2swsPin, int8_t i2ssdPin, int8_t i2sckPin, int8_t mclkPin) {
      DEBUGSR_PRINTLN("AC101Source:: initialize();");

      // if ((i2sckPin < 0) || (mclkPin < 0)) { // WLEDMM not sure if this check is needed here, too
      //    ERRORSR_PRINTF("\nAR: invalid I2S WM8978 pin: SCK=%d, MCLK=%d\n", i2sckPin, mclkPin); 
      //    return;
      // }
      // BUG: "use global I2C pins" are valid as -1, and -1 is seen as invalid here.
      // Workaround: Set I2C pins here, which will also set them globally.
      // Bug also exists in ES7243.
       if ((i2c_sda < 0) || (i2c_scl < 0)) {  // check that global I2C pins are not "undefined"
        ERRORSR_PRINTF("\nAR: invalid AC101 global I2C pins: SDA=%d, SCL=%d\n", i2c_sda, i2c_scl); 
        return;
      }
      if (!pinManager.joinWire(i2c_sda, i2c_scl)) {    // WLEDMM specific: start I2C with globally defined pins
        ERRORSR_PRINTF("\nAR: failed to join I2C bus with SDA=%d, SCL=%d\n", i2c_sda, i2c_scl); 
        return;
      }

      // First route mclk, then configure ADC over I2C, then configure I2S
      _ac101InitAdc();
      I2SSource::initialize(i2swsPin, i2ssdPin, i2sckPin, mclkPin);
    }

    void deinitialize() {
      I2SSource::deinitialize();
    }

};

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
#if !defined(SOC_I2S_SUPPORTS_ADC) && !defined(SOC_I2S_SUPPORTS_ADC_DAC)
  #warning this MCU does not support analog sound input
#endif
#endif

#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
// ADC over I2S is only available in "classic" ESP32

/* ADC over I2S Microphone
   This microphone is an ADC pin sampled via the I2S interval
   This allows to use the I2S API to obtain ADC samples with high sample rates
   without the need of manual timing of the samples
*/
class I2SAdcSource : public I2SSource {
  public:
    I2SAdcSource(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f) :
      I2SSource(sampleRate, blockSize, sampleScale, true) {
      _config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
        .sample_rate = _sampleRate,
        .bits_per_sample = I2S_SAMPLE_RESOLUTION,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
#else
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
#endif
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = _blockSize,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0        
      };
    }

    /* identify Audiosource type - I2S-ADC*/
    AudioSourceType getType(void) {return(Type_I2SAdc);}

    void initialize(int8_t audioPin, int8_t = I2S_PIN_NO_CHANGE, int8_t = I2S_PIN_NO_CHANGE, int8_t = I2S_PIN_NO_CHANGE) {
      DEBUGSR_PRINTLN("I2SAdcSource:: initialize().");
      _myADCchannel = 0x0F;
      if(!pinManager.allocatePin(audioPin, false, PinOwner::UM_Audioreactive)) {
         ERRORSR_PRINTF("failed to allocate GPIO for audio analog input: %d\n", audioPin);
        return;
      }
      _audioPin = audioPin;

      // Determine Analog channel. Only Channels on ADC1 are supported
      int8_t channel = digitalPinToAnalogChannel(_audioPin);
      if ((channel < 0) || (channel > 9)) {  // channel == -1 means "not an ADC pin"
        USER_PRINTF("AR: Incompatible GPIO used for analog audio input: %d\n", _audioPin);
        return;
      } else {
        adc_gpio_init(ADC_UNIT_1, adc_channel_t(channel));
        _myADCchannel = channel;
      }

      // Install Driver
      esp_err_t err = i2s_driver_install(I2S_NUM_0, &_config, 0, nullptr);
      if (err != ESP_OK) {
        ERRORSR_PRINTF("Failed to install i2s driver: %d\n", err);
        return;
      }

      // adc1_config_width(ADC_WIDTH_BIT_12);   // ensure that ADC runs with 12bit resolution - should not be needed, because i2s_set_adc_mode does that any way

      // Enable I2S mode of ADC
      err = i2s_set_adc_mode(ADC_UNIT_1, adc1_channel_t(channel));
      if (err != ESP_OK) {
        USER_PRINTF("AR: Failed to set i2s adc mode: %d\n", err);
        return;
      }

      // see example in https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/I2S/HiFreq_ADC/HiFreq_ADC.ino
      adc1_config_channel_atten(adc1_channel_t(channel), ADC_ATTEN_DB_11);   // configure ADC input amplification

      #if defined(I2S_GRAB_ADC1_COMPLETELY)
      // according to docs from espressif, the ADC needs to be started explicitly
      // fingers crossed
        err = i2s_adc_enable(I2S_NUM_0);
        if (err != ESP_OK) {
            DEBUGSR_PRINTF("Failed to enable i2s adc: %d\n", err);
            //return;
        }
      #else
        // bugfix: do not disable ADC initially - its already disabled after driver install.
        //err = i2s_adc_disable(I2S_NUM_0);
		    // //err = i2s_stop(I2S_NUM_0);
        //if (err != ESP_OK) {
        //    DEBUGSR_PRINTF("Failed to initially disable i2s adc: %d\n", err);
        //}
      #endif

      _initialized = true;
    }


    I2S_datatype postProcessSample(I2S_datatype sample_in) {
      static I2S_datatype lastADCsample = 0;          // last good sample
      static unsigned int broken_samples_counter = 0; // number of consecutive broken (and fixed) ADC samples
      I2S_datatype sample_out = 0;

      // bring sample down down to 16bit unsigned
      I2S_unsigned_datatype rawData = * reinterpret_cast<I2S_unsigned_datatype *> (&sample_in); // C++ acrobatics to get sample as "unsigned"
      #ifndef I2S_USE_16BIT_SAMPLES
        rawData = (rawData >> 16) & 0xFFFF;                       // scale input down from 32bit -> 16bit
        I2S_datatype lastGoodSample = lastADCsample / 16384 ;     // prepare "last good sample" accordingly (26bit-> 12bit with correct sign handling)
      #else
        rawData = rawData & 0xFFFF;                               // input is already in 16bit, just mask off possible junk
        I2S_datatype lastGoodSample = lastADCsample * 4;          // prepare "last good sample" accordingly (10bit-> 12bit)
      #endif

      // decode ADC sample data fields
      uint16_t the_channel = (rawData >> 12) & 0x000F;           // upper 4 bit = ADC channel
      uint16_t the_sample  =  rawData & 0x0FFF;                  // lower 12bit -> ADC sample (unsigned)
      I2S_datatype finalSample = (int(the_sample) - 2048);       // convert unsigned sample to signed (centered at 0);

      if ((the_channel != _myADCchannel) && (_myADCchannel != 0x0F)) { // 0x0F means "don't know what my channel is" 
        // fix bad sample
        finalSample = lastGoodSample;                             // replace with last good ADC sample
        broken_samples_counter ++;
        if (broken_samples_counter > 256) _myADCchannel = 0x0F;   // too  many bad samples in a row -> disable sample corrections
        //Serial.print("\n!ADC rogue sample 0x"); Serial.print(rawData, HEX); Serial.print("\tchannel:");Serial.println(the_channel);
      } else broken_samples_counter = 0;                          // good sample - reset counter

      // back to original resolution
      #ifndef I2S_USE_16BIT_SAMPLES
        finalSample = finalSample << 16;                          // scale up from 16bit -> 32bit;
      #endif

      finalSample = finalSample / 4;                              // mimic old analog driver behaviour (12bit -> 10bit)
      sample_out = (3 * finalSample + lastADCsample) / 4;         // apply low-pass filter (2-tap FIR)
      //sample_out = (finalSample + lastADCsample) / 2;             // apply stronger low-pass filter (2-tap FIR)

      lastADCsample = sample_out;                                 // update ADC last sample
      return(sample_out);
    }


    void getSamples(float *buffer, uint16_t num_samples) {
      /* Enable ADC. This has to be enabled and disabled directly before and
       * after sampling, otherwise Wifi dies
       */
      if (_initialized) {
        #if !defined(I2S_GRAB_ADC1_COMPLETELY)
          // old code - works for me without enable/disable, at least on ESP32.
          //esp_err_t err = i2s_start(I2S_NUM_0);
          esp_err_t err = i2s_adc_enable(I2S_NUM_0);
          if (err != ESP_OK) {
            DEBUGSR_PRINTF("Failed to enable i2s adc: %d\n", err);
            return;
          }
        #endif

        I2SSource::getSamples(buffer, num_samples);

        #if !defined(I2S_GRAB_ADC1_COMPLETELY)
          // old code - works for me without enable/disable, at least on ESP32.
          err = i2s_adc_disable(I2S_NUM_0);  //i2s_adc_disable() may cause crash with IDF 4.4 (https://github.com/espressif/arduino-esp32/issues/6832)
          //err = i2s_stop(I2S_NUM_0);
          if (err != ESP_OK) {
            DEBUGSR_PRINTF("Failed to disable i2s adc: %d\n", err);
            return;
          }
        #endif
      }
    }

    void deinitialize() {
      pinManager.deallocatePin(_audioPin, PinOwner::UM_Audioreactive);
      _initialized = false;
      _myADCchannel = 0x0F;
      
      esp_err_t err;
      #if defined(I2S_GRAB_ADC1_COMPLETELY)
        // according to docs from espressif, the ADC needs to be stopped explicitly
        // fingers crossed
        err = i2s_adc_disable(I2S_NUM_0);
        if (err != ESP_OK) {
          DEBUGSR_PRINTF("Failed to disable i2s adc: %d\n", err);
        }
      #endif

      i2s_stop(I2S_NUM_0);
      err = i2s_driver_uninstall(I2S_NUM_0);
      if (err != ESP_OK) {
        DEBUGSR_PRINTF("Failed to uninstall i2s driver: %d\n", err);
        return;
      }
    }

  private:
    int8_t _audioPin;
    int8_t _myADCchannel = 0x0F;       // current ADC channel for analog input. 0x0F means "undefined"
};
#endif

/* SPH0645 Microphone
   This is an I2S microphone with some timing quirks that need
   special consideration.
*/

// https://github.com/espressif/esp-idf/issues/7192  SPH0645 i2s microphone issue when migrate from legacy esp-idf version (IDFGH-5453)
// a user recommended this: Try to set .communication_format to I2S_COMM_FORMAT_STAND_I2S and call i2s_set_clk() after i2s_set_pin().
class SPH0654 : public I2SSource {
  public:
    SPH0654(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f, bool i2sMaster=true) :
      I2SSource(sampleRate, blockSize, sampleScale, i2sMaster)
    {}

    void initialize(int8_t i2swsPin, int8_t i2ssdPin, int8_t i2sckPin, int8_t = I2S_PIN_NO_CHANGE) {
      DEBUGSR_PRINTLN("SPH0654:: initialize();");
      I2SSource::initialize(i2swsPin, i2ssdPin, i2sckPin);
#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
// these registers are only existing in "classic" ESP32
      REG_SET_BIT(I2S_TIMING_REG(AR_I2S_PORT), BIT(9));
      REG_SET_BIT(I2S_CONF_REG(AR_I2S_PORT), I2S_RX_MSB_SHIFT);
#else
      #warning FIX ME! Please.
#endif
    }
};
#endif
