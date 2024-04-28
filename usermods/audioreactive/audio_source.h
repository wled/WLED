#pragma once
#ifdef ARDUINO_ARCH_ESP32
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

//#include <driver/i2s_std.h>
//#include <driver/i2s_pdm.h>
//#include <driver/i2s_tdm.h>
//#include <driver/gpio.h>

// see https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/hw-reference/chip-series-comparison.html#related-documents
// and https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html#overview-of-all-modes
#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(ESP8266) || defined(ESP8265)
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

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)) && (ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(4, 4, 4))
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
    AudioSource(SRate_t sampleRate, int blockSize, float sampleScale) :
      _sampleRate(sampleRate),
      _blockSize(blockSize),
      _initialized(false),
      _sampleScale(sampleScale)
    {};

    SRate_t _sampleRate;            // Microphone sampling rate
    int _blockSize;                 // I2S block size
    bool _initialized;              // Gets set to true if initialization is successful
    float _sampleScale;             // pre-scaling factor for I2S samples
};

/* Basic I2S microphone source
   All functions are marked virtual, so derived classes can replace them
*/
class I2SSource : public AudioSource {
  public:
    I2SSource(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f) :
      AudioSource(sampleRate, blockSize, sampleScale) {
      _config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = _sampleRate,
        .bits_per_sample = I2S_SAMPLE_RESOLUTION,
        .channel_format = I2S_MIC_CHANNEL,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        //.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
        .dma_buf_count = 8,
        .dma_buf_len = _blockSize,
        .use_apll = 0,
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
          DEBUGSR_PRINTF("\nAR: Failed to allocate I2S pins: ws=%d, sd=%d\n",  i2swsPin, i2ssdPin); 
          return;
        }
      }

      // i2ssckPin needs special treatment, since it might be unused on PDM mics
      if (i2sckPin != I2S_PIN_NO_CHANGE) {
        if (!pinManager.allocatePin(i2sckPin, true, PinOwner::UM_Audioreactive)) {
          DEBUGSR_PRINTF("\nAR: Failed to allocate I2S pins: sck=%d\n",  i2sckPin); 
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
        #endif
      }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
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
#endif

      // Reserve the master clock pin if provided
      _mclkPin = mclkPin;
      if (mclkPin != I2S_PIN_NO_CHANGE) {
        if(!pinManager.allocatePin(mclkPin, true, PinOwner::UM_Audioreactive)) { 
          DEBUGSR_PRINTF("\nAR: Failed to allocate I2S pin: MCLK=%d\n",  mclkPin); 
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

      esp_err_t err = i2s_driver_install(I2S_NUM_0, &_config, 0, nullptr);
      if (err != ESP_OK) {
        DEBUGSR_PRINTF("AR: Failed to install i2s driver: %d\n", err);
        return;
      }

      DEBUGSR_PRINTF("AR: I2S#0 driver %s aPLL; fixed_mclk=%d.\n", _config.use_apll? "uses":"without", _config.fixed_mclk);
      DEBUGSR_PRINTF("AR: %d bits, Sample scaling factor = %6.4f\n",  _config.bits_per_sample, _sampleScale);
      if (_config.mode & I2S_MODE_PDM) {
          DEBUGSR_PRINTLN(F("AR: I2S#0 driver installed in PDM MASTER mode."));
      } else { 
          DEBUGSR_PRINTLN(F("AR: I2S#0 driver installed in MASTER mode."));
      }

      err = i2s_set_pin(I2S_NUM_0, &_pinConfig);
      if (err != ESP_OK) {
        DEBUGSR_PRINTF("AR: Failed to set i2s pin config: %d\n", err);
        i2s_driver_uninstall(I2S_NUM_0);  // uninstall already-installed driver
        return;
      }

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
      err = i2s_set_clk(I2S_NUM_0, _sampleRate, I2S_SAMPLE_RESOLUTION, I2S_CHANNEL_MONO);  // set bit clocks. Also takes care of MCLK routing if needed.
      if (err != ESP_OK) {
        DEBUGSR_PRINTF("AR: Failed to configure i2s clocks: %d\n", err);
        i2s_driver_uninstall(I2S_NUM_0);  // uninstall already-installed driver
        return;
      }
#endif
      _initialized = true;
    }

    virtual void deinitialize() {
      _initialized = false;
      esp_err_t err = i2s_driver_uninstall(I2S_NUM_0);
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
        I2S_datatype newSamples[num_samples]; /* Intermediary sample storage */

        err = i2s_read(I2S_NUM_0, (void *)newSamples, sizeof(newSamples), &bytes_read, portMAX_DELAY);
        if (err != ESP_OK) {
          DEBUGSR_PRINTF("Failed to get samples: %d\n", err);
          return;
        }

        // For correct operation, we need to read exactly sizeof(samples) bytes from i2s
        if (bytes_read != sizeof(newSamples)) {
          DEBUGSR_PRINTF("Failed to get enough samples: wanted: %d read: %d\n", sizeof(newSamples), bytes_read);
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
      _es7243I2cWrite(0x00, 0x01);
      _es7243I2cWrite(0x06, 0x00);
      _es7243I2cWrite(0x05, 0x1B);
      _es7243I2cWrite(0x01, 0x00); // 0x00 for 24 bit to match INMP441 - not sure if this needs adjustment to get 16bit samples from I2S
      _es7243I2cWrite(0x08, 0x43);
      _es7243I2cWrite(0x05, 0x13);
    }

public:
    ES7243(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f) :
      I2SSource(sampleRate, blockSize, sampleScale) {
      _config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
    };

    void initialize(int8_t i2swsPin, int8_t i2ssdPin, int8_t i2sckPin, int8_t mclkPin) {
      DEBUGSR_PRINTLN("ES7243:: initialize();");
      if ((i2sckPin < 0) || (mclkPin < 0)) {
        DEBUGSR_PRINTF("\nAR: invalid I2S pin: SCK=%d, MCLK=%d\n", i2sckPin, mclkPin); 
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

    void _es8388I2cWrite(uint8_t reg, uint8_t val) {
#ifndef ES8388_ADDR
      Wire.beginTransmission(0x10);
      #define ES8388_ADDR 0x10   // default address
#else
      Wire.beginTransmission(ES8388_ADDR);
#endif
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
      I2SSource(sampleRate, blockSize, sampleScale) {
      _config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    };

    void initialize(int8_t i2swsPin, int8_t i2ssdPin, int8_t i2sckPin, int8_t mclkPin) {
      DEBUGSR_PRINTLN("ES8388Source:: initialize();");
      if ((i2sckPin < 0) || (mclkPin < 0)) {
        DEBUGSR_PRINTF("\nAR: invalid I2S pin: SCK=%d, MCLK=%d\n", i2sckPin, mclkPin); 
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

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 2, 0)
#if !defined(SOC_I2S_SUPPORTS_ADC) && !defined(SOC_I2S_SUPPORTS_ADC_DAC)
  #warning this MCU does not support analog sound input
#endif
#endif

#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
// ADC over I2S is only availeable in "classic" ESP32

/* ADC over I2S Microphone
   This microphone is an ADC pin sampled via the I2S interval
   This allows to use the I2S API to obtain ADC samples with high sample rates
   without the need of manual timing of the samples
*/
class I2SAdcSource : public I2SSource {
  public:
    I2SAdcSource(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f) :
      I2SSource(sampleRate, blockSize, sampleScale) {
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
         DEBUGSR_PRINTF("failed to allocate GPIO for audio analog input: %d\n", audioPin);
        return;
      }
      _audioPin = audioPin;

      // Determine Analog channel. Only Channels on ADC1 are supported
      int8_t channel = digitalPinToAnalogChannel(_audioPin);
      if (channel > 9) {
        DEBUGSR_PRINTF("Incompatible GPIO used for analog audio input: %d\n", _audioPin);
        return;
      } else {
        adc_gpio_init(ADC_UNIT_1, adc_channel_t(channel));
        _myADCchannel = channel;
      }

      // Install Driver
      esp_err_t err = i2s_driver_install(I2S_NUM_0, &_config, 0, nullptr);
      if (err != ESP_OK) {
        DEBUGSR_PRINTF("Failed to install i2s driver: %d\n", err);
        return;
      }

      adc1_config_width(ADC_WIDTH_BIT_12);   // ensure that ADC runs with 12bit resolution

      // Enable I2S mode of ADC
      err = i2s_set_adc_mode(ADC_UNIT_1, adc1_channel_t(channel));
      if (err != ESP_OK) {
        DEBUGSR_PRINTF("Failed to set i2s adc mode: %d\n", err);
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
    SPH0654(SRate_t sampleRate, int blockSize, float sampleScale = 1.0f) :
      I2SSource(sampleRate, blockSize, sampleScale)
    {}

    void initialize(int8_t i2swsPin, int8_t i2ssdPin, int8_t i2sckPin, int8_t = I2S_PIN_NO_CHANGE) {
      DEBUGSR_PRINTLN("SPH0654:: initialize();");
      I2SSource::initialize(i2swsPin, i2ssdPin, i2sckPin);
#if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
// these registers are only existing in "classic" ESP32
      REG_SET_BIT(I2S_TIMING_REG(I2S_NUM_0), BIT(9));
      REG_SET_BIT(I2S_CONF_REG(I2S_NUM_0), I2S_RX_MSB_SHIFT);
#else
      #warning FIX ME! Please.
#endif
    }
};
#endif