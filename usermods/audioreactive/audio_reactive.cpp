
#include "wled.h"

#ifdef ARDUINO_ARCH_ESP32

#include <driver/i2s.h>
#include <driver/adc.h>

#ifdef WLED_ENABLE_DMX
  #error This audio reactive usermod is not compatible with DMX Out.
#endif

#endif

#if defined(ARDUINO_ARCH_ESP32) && (defined(WLED_DEBUG) || defined(SR_DEBUG))
#include <esp_timer.h>
#endif

/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/wled-dev/WLED/wiki/Add-own-functionality
 * 
 * This is an audioreactive v2 usermod.
 * ....
 */

#if !defined(FFTTASK_PRIORITY)
#define FFTTASK_PRIORITY 1 // standard: looptask prio
//#define FFTTASK_PRIORITY 2 // above looptask, below asyc_tcp
//#define FFTTASK_PRIORITY 4 // above asyc_tcp
#endif

// Comment/Uncomment to toggle usb serial debugging
// #define MIC_LOGGER                   // MIC sampling & sound input debugging (serial plotter)
// #define FFT_SAMPLING_LOG             // FFT result debugging
// #define SR_DEBUG                     // generic SR DEBUG messages

#ifdef SR_DEBUG
  #define DEBUGSR_PRINT(x) DEBUGOUT.print(x)
  #define DEBUGSR_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUGSR_PRINTF(x...) DEBUGOUT.printf(x)
#else
  #define DEBUGSR_PRINT(x)
  #define DEBUGSR_PRINTLN(x)
  #define DEBUGSR_PRINTF(x...)
#endif

#if defined(MIC_LOGGER) || defined(FFT_SAMPLING_LOG)
  #define PLOT_PRINT(x) DEBUGOUT.print(x)
  #define PLOT_PRINTLN(x) DEBUGOUT.println(x)
  #define PLOT_PRINTF(x...) DEBUGOUT.printf(x)
#else
  #define PLOT_PRINT(x)
  #define PLOT_PRINTLN(x)
  #define PLOT_PRINTF(x...)
#endif

#define MAX_PALETTES 3

static volatile bool disableSoundProcessing = false;      // if true, sound processing (FFT, filters, AGC) will be suspended. "volatile" as its shared between tasks.
static uint8_t audioSyncEnabled = 0;          // bit field: bit 0 - send, bit 1 - receive (config value)
static bool udpSyncConnected = false;         // UDP connection status -> true if connected to multicast group

#define NUM_GEQ_CHANNELS 16                                           // number of frequency channels. Don't change !!

// audioreactive variables
#ifdef ARDUINO_ARCH_ESP32
static float    micDataReal = 0.0f;             // MicIn data with full 24bit resolution - lowest 8bit after decimal point
static float    multAgc = 1.0f;                 // sample * multAgc = sampleAgc. Our AGC multiplier
static float    sampleAvg = 0.0f;               // Smoothed Average sample - sampleAvg < 1 means "quiet" (simple noise gate)
static float    sampleAgc = 0.0f;               // Smoothed AGC sample
static uint8_t  soundAgc = 0;                   // Automagic gain control: 0 - none, 1 - normal, 2 - vivid, 3 - lazy (config value)
#endif
//static float    volumeSmth = 0.0f;              // either sampleAvg or sampleAgc depending on soundAgc; smoothed sample
static float FFT_MajorPeak = 1.0f;              // FFT: strongest (peak) frequency
static float FFT_Magnitude = 0.0f;              // FFT: volume (magnitude) of peak frequency
static bool samplePeak = false;      // Boolean flag for peak - used in effects. Responding routine may reset this flag. Auto-reset after strip.getFrameTime()
static bool udpSamplePeak = false;   // Boolean flag for peak. Set at the same time as samplePeak, but reset by transmitAudioData
static unsigned long timeOfPeak = 0; // time of last sample peak detection.
static uint8_t fftResult[NUM_GEQ_CHANNELS]= {0};// Our calculated freq. channel result table to be used by effects

// TODO: probably best not used by receive nodes
//static float agcSensitivity = 128;            // AGC sensitivity estimation, based on agc gain (multAgc). calculated by getSensitivity(). range 0..255

// user settable parameters for limitSoundDynamics()
#ifdef UM_AUDIOREACTIVE_DYNAMICS_LIMITER_OFF
static bool limiterOn = false;                 // bool: enable / disable dynamics limiter
#else
static bool limiterOn = true;
#endif
static uint16_t attackTime =  80;             // int: attack time in milliseconds. Default 0.08sec
static uint16_t decayTime = 1400;             // int: decay time in milliseconds.  Default 1.40sec

// peak detection
#ifdef ARDUINO_ARCH_ESP32
static void detectSamplePeak(void);  // peak detection function (needs scaled FFT results in vReal[]) - no used for 8266 receive-only mode
#endif
static void autoResetPeak(void);     // peak auto-reset function
static uint8_t maxVol = 31;          // (was 10) Reasonable value for constant volume for 'peak detector', as it won't always trigger  (deprecated)
static uint8_t binNum = 8;           // Used to select the bin for FFT based beat detection  (deprecated)

#ifdef ARDUINO_ARCH_ESP32

// use audio source class (ESP32 specific)
#include "audio_source.h"
constexpr i2s_port_t I2S_PORT = I2S_NUM_0;       // I2S port to use (do not change !)
constexpr int BLOCK_SIZE = 128;                  // I2S buffer size (samples)

// globals
static uint8_t inputLevel = 128;              // UI slider value
#ifndef SR_SQUELCH
  uint8_t soundSquelch = 10;                  // squelch value for volume reactive routines (config value)
#else
  uint8_t soundSquelch = SR_SQUELCH;          // squelch value for volume reactive routines (config value)
#endif
#ifndef SR_GAIN
  uint8_t sampleGain = 60;                    // sample gain (config value)
#else
  uint8_t sampleGain = SR_GAIN;               // sample gain (config value)
#endif
// user settable options for FFTResult scaling
static uint8_t FFTScalingMode = 3;            // 0 none; 1 optimized logarithmic; 2 optimized linear; 3 optimized square root

// 
// AGC presets
//  Note: in C++, "const" implies "static" - no need to explicitly declare everything as "static const"
// 
#define AGC_NUM_PRESETS 3 // AGC presets:          normal,   vivid,    lazy
const double agcSampleDecay[AGC_NUM_PRESETS]  = { 0.9994f, 0.9985f, 0.9997f}; // decay factor for sampleMax, in case the current sample is below sampleMax
const float agcZoneLow[AGC_NUM_PRESETS]       = {      32,      28,      36}; // low volume emergency zone
const float agcZoneHigh[AGC_NUM_PRESETS]      = {     240,     240,     248}; // high volume emergency zone
const float agcZoneStop[AGC_NUM_PRESETS]      = {     336,     448,     304}; // disable AGC integrator if we get above this level
const float agcTarget0[AGC_NUM_PRESETS]       = {     112,     144,     164}; // first AGC setPoint -> between 40% and 65%
const float agcTarget0Up[AGC_NUM_PRESETS]     = {      88,      64,     116}; // setpoint switching value (a poor man's bang-bang)
const float agcTarget1[AGC_NUM_PRESETS]       = {     220,     224,     216}; // second AGC setPoint -> around 85%
const double agcFollowFast[AGC_NUM_PRESETS]   = { 1/192.f, 1/128.f, 1/256.f}; // quickly follow setpoint - ~0.15 sec
const double agcFollowSlow[AGC_NUM_PRESETS]   = {1/6144.f,1/4096.f,1/8192.f}; // slowly follow setpoint  - ~2-15 secs
const double agcControlKp[AGC_NUM_PRESETS]    = {    0.6f,    1.5f,   0.65f}; // AGC - PI control, proportional gain parameter
const double agcControlKi[AGC_NUM_PRESETS]    = {    1.7f,   1.85f,    1.2f}; // AGC - PI control, integral gain parameter
const float agcSampleSmooth[AGC_NUM_PRESETS]  = {  1/12.f,   1/6.f,  1/16.f}; // smoothing factor for sampleAgc (use rawSampleAgc if you want the non-smoothed value)
// AGC presets end

static AudioSource *audioSource = nullptr;
static bool useBandPassFilter = false;                    // if true, enables a bandpass filter 80Hz-16Khz to remove noise. Applies before FFT.

////////////////////
// Begin FFT Code //
////////////////////

// some prototypes, to ensure consistent interfaces
static float fftAddAvg(int from, int to);   // average of several FFT result bins
void FFTcode(void * parameter);      // audio processing task: read samples, run FFT, fill GEQ channels from FFT results
static void runMicFilter(uint16_t numSamples, float *sampleBuffer);          // pre-filtering of raw samples (band-pass)
static void postProcessFFTResults(bool noiseGateOpen, int numberOfChannels); // post-processing and post-amp of GEQ channels

static TaskHandle_t FFT_Task = nullptr;

// Table of multiplication factors so that we can even out the frequency response.
static float fftResultPink[NUM_GEQ_CHANNELS] = { 1.70f, 1.71f, 1.73f, 1.78f, 1.68f, 1.56f, 1.55f, 1.63f, 1.79f, 1.62f, 1.80f, 2.06f, 2.47f, 3.35f, 6.83f, 9.55f };

// globals and FFT Output variables shared with animations
#if defined(WLED_DEBUG) || defined(SR_DEBUG)
static uint64_t fftTime = 0;
static uint64_t sampleTime = 0;
#endif

// FFT Task variables (filtering and post-processing)
static float   fftCalc[NUM_GEQ_CHANNELS] = {0.0f};                    // Try and normalize fftBin values to a max of 4096, so that 4096/16 = 256.
static float   fftAvg[NUM_GEQ_CHANNELS] = {0.0f};                     // Calculated frequency channel results, with smoothing (used if dynamics limiter is ON)
#ifdef SR_DEBUG
static float   fftResultMax[NUM_GEQ_CHANNELS] = {0.0f};               // A table used for testing to determine how our post-processing is working.
#endif

// audio source parameters and constant
constexpr SRate_t SAMPLE_RATE = 22050;        // Base sample rate in Hz - 22Khz is a standard rate. Physical sample time -> 23ms
//constexpr SRate_t SAMPLE_RATE = 16000;        // 16kHz - use if FFTtask takes more than 20ms. Physical sample time -> 32ms
//constexpr SRate_t SAMPLE_RATE = 20480;        // Base sample rate in Hz - 20Khz is experimental.    Physical sample time -> 25ms
//constexpr SRate_t SAMPLE_RATE = 10240;        // Base sample rate in Hz - previous default.         Physical sample time -> 50ms
#define FFT_MIN_CYCLE 21                      // minimum time before FFT task is repeated. Use with 22Khz sampling
//#define FFT_MIN_CYCLE 30                      // Use with 16Khz sampling
//#define FFT_MIN_CYCLE 23                      // minimum time before FFT task is repeated. Use with 20Khz sampling
//#define FFT_MIN_CYCLE 46                      // minimum time before FFT task is repeated. Use with 10Khz sampling

// FFT Constants
constexpr uint16_t samplesFFT = 512;            // Samples in an FFT batch - This value MUST ALWAYS be a power of 2
constexpr uint16_t samplesFFT_2 = 256;          // meaningfull part of FFT results - only the "lower half" contains useful information.
// the following are observed values, supported by a bit of "educated guessing"
//#define FFT_DOWNSCALE 0.65f                             // 20kHz - downscaling factor for FFT results - "Flat-Top" window @20Khz, old freq channels 
#define FFT_DOWNSCALE 0.46f                             // downscaling factor for FFT results - for "Flat-Top" window @22Khz, new freq channels
#define LOG_256  5.54517744f                            // log(256)

// These are the input and output vectors.  Input vectors receive computed results from FFT.
static float* vReal = nullptr;                  // FFT sample inputs / freq output -  these are our raw result bins
static float* vImag = nullptr;                  // imaginary parts

// Create FFT object
// lib_deps += https://github.com/kosme/arduinoFFT#develop @ 1.9.2
// these options actually cause slow-downs on all esp32 processors, don't use them.
// #define FFT_SPEED_OVER_PRECISION     // enables use of reciprocals (1/x etc) - not faster on ESP32
// #define FFT_SQRT_APPROXIMATION       // enables "quake3" style inverse sqrt  - slower on ESP32
// Below options are forcing ArduinoFFT to use sqrtf() instead of sqrt()
// #define sqrt_internal sqrtf          // see https://github.com/kosme/arduinoFFT/pull/83 - since v2.0.0 this must be done in build_flags

#include <arduinoFFT.h>             // FFT object is created in FFTcode
// Helper functions

// compute average of several FFT result bins
static float fftAddAvg(int from, int to) {
  float result = 0.0f;
  for (int i = from; i <= to; i++) {
    result += vReal[i];
  }
  return result / float(to - from + 1);
}

//
// FFT main task
//
void FFTcode(void * parameter)
{
  DEBUGSR_PRINT("FFT started on core: "); DEBUGSR_PRINTLN(xPortGetCoreID());

  // allocate FFT buffers on first call
  if (vReal == nullptr) vReal = (float*) calloc(sizeof(float), samplesFFT);
  if (vImag == nullptr) vImag = (float*) calloc(sizeof(float), samplesFFT);
  if ((vReal == nullptr) || (vImag == nullptr)) {
    // something went wrong
    if (vReal) free(vReal); vReal = nullptr;
    if (vImag) free(vImag); vImag = nullptr;
    return;
  }
  // Create FFT object with weighing factor storage
  ArduinoFFT<float> FFT = ArduinoFFT<float>( vReal, vImag, samplesFFT, SAMPLE_RATE, true);

  // see https://www.freertos.org/vtaskdelayuntil.html
  const TickType_t xFrequency = FFT_MIN_CYCLE * portTICK_PERIOD_MS;  

  TickType_t xLastWakeTime = xTaskGetTickCount();
  for(;;) {
    delay(1);           // DO NOT DELETE THIS LINE! It is needed to give the IDLE(0) task enough time and to keep the watchdog happy.
                        // taskYIELD(), yield(), vTaskDelay() and esp_task_wdt_feed() didn't seem to work.

    // Don't run FFT computing code if we're in Receive mode or in realtime mode
    if (disableSoundProcessing || (audioSyncEnabled & 0x02)) {
      vTaskDelayUntil( &xLastWakeTime, xFrequency);        // release CPU, and let I2S fill its buffers
      continue;
    }

#if defined(WLED_DEBUG) || defined(SR_DEBUG)
    uint64_t start = esp_timer_get_time();
    bool haveDoneFFT = false; // indicates if second measurement (FFT time) is valid
#endif

    // get a fresh batch of samples from I2S
    if (audioSource) audioSource->getSamples(vReal, samplesFFT);
    memset(vImag, 0, samplesFFT * sizeof(float));   // set imaginary parts to 0

#if defined(WLED_DEBUG) || defined(SR_DEBUG)
    if (start < esp_timer_get_time()) { // filter out overflows
      uint64_t sampleTimeInMillis = (esp_timer_get_time() - start +5ULL) / 10ULL; // "+5" to ensure proper rounding
      sampleTime = (sampleTimeInMillis*3 + sampleTime*7)/10; // smooth
    }
    start = esp_timer_get_time(); // start measuring FFT time
#endif

    xLastWakeTime = xTaskGetTickCount();       // update "last unblocked time" for vTaskDelay

    // band pass filter - can reduce noise floor by a factor of 50
    // downside: frequencies below 100Hz will be ignored
    if (useBandPassFilter) runMicFilter(samplesFFT, vReal);

    // find highest sample in the batch
    float maxSample = 0.0f;                         // max sample from FFT batch
    for (int i=0; i < samplesFFT; i++) {
	    // pick our  our current mic sample - we take the max value from all samples that go into FFT
	    if ((vReal[i] <= (INT16_MAX - 1024)) && (vReal[i] >= (INT16_MIN + 1024)))  //skip extreme values - normally these are artefacts
        if (fabsf((float)vReal[i]) > maxSample) maxSample = fabsf((float)vReal[i]);
    }
    // release highest sample to volume reactive effects early - not strictly necessary here - could also be done at the end of the function
    // early release allows the filters (getSample() and agcAvg()) to work with fresh values - we will have matching gain and noise gate values when we want to process the FFT results.
    micDataReal = maxSample;

#ifdef SR_DEBUG
    if (true) {  // this allows measure FFT runtimes, as it disables the "only when needed" optimization 
#else
    if (sampleAvg > 0.25f) { // noise gate open means that FFT results will be used. Don't run FFT if results are not needed.
#endif

      // run FFT (takes 3-5ms on ESP32, ~12ms on ESP32-S2)
      FFT.dcRemoval();                                            // remove DC offset
      FFT.windowing( FFTWindow::Flat_top, FFTDirection::Forward); // Weigh data using "Flat Top" function - better amplitude accuracy
      //FFT.windowing(FFTWindow::Blackman_Harris, FFTDirection::Forward);  // Weigh data using "Blackman- Harris" window - sharp peaks due to excellent sideband rejection
      FFT.compute( FFTDirection::Forward );                       // Compute FFT
      FFT.complexToMagnitude();                                   // Compute magnitudes
      vReal[0] = 0;   // The remaining DC offset on the signal produces a strong spike on position 0 that should be eliminated to avoid issues.

      FFT.majorPeak(&FFT_MajorPeak, &FFT_Magnitude);                // let the effects know which freq was most dominant
      FFT_MajorPeak = constrain(FFT_MajorPeak, 1.0f, 11025.0f);   // restrict value to range expected by effects

#if defined(WLED_DEBUG) || defined(SR_DEBUG)
      haveDoneFFT = true;
#endif

    } else { // noise gate closed - only clear results as FFT was skipped. MIC samples are still valid when we do this.
      memset(vReal, 0, samplesFFT * sizeof(float));
      FFT_MajorPeak = 1;
      FFT_Magnitude = 0.001;
    }

    for (int i = 0; i < samplesFFT; i++) {
      float t = fabsf(vReal[i]);                      // just to be sure - values in fft bins should be positive any way
      vReal[i] = t / 16.0f;                           // Reduce magnitude. Want end result to be scaled linear and ~4096 max.
    } // for()

    // mapping of FFT result bins to frequency channels
    if (fabsf(sampleAvg) > 0.5f) { // noise gate open
#if 0
    /* This FFT post processing is a DIY endeavour. What we really need is someone with sound engineering expertise to do a great job here AND most importantly, that the animations look GREAT as a result.
    *
    * Andrew's updated mapping of 256 bins down to the 16 result bins with Sample Freq = 10240, samplesFFT = 512 and some overlap.
    * Based on testing, the lowest/Start frequency is 60 Hz (with bin 3) and a highest/End frequency of 5120 Hz in bin 255.
    * Now, Take the 60Hz and multiply by 1.320367784 to get the next frequency and so on until the end. Then determine the bins.
    * End frequency = Start frequency * multiplier ^ 16
    * Multiplier = (End frequency/ Start frequency) ^ 1/16
    * Multiplier = 1.320367784
    */                                    //  Range
      fftCalc[ 0] = fftAddAvg(2,4);       // 60 - 100
      fftCalc[ 1] = fftAddAvg(4,5);       // 80 - 120
      fftCalc[ 2] = fftAddAvg(5,7);       // 100 - 160
      fftCalc[ 3] = fftAddAvg(7,9);       // 140 - 200
      fftCalc[ 4] = fftAddAvg(9,12);      // 180 - 260
      fftCalc[ 5] = fftAddAvg(12,16);     // 240 - 340
      fftCalc[ 6] = fftAddAvg(16,21);     // 320 - 440
      fftCalc[ 7] = fftAddAvg(21,29);     // 420 - 600
      fftCalc[ 8] = fftAddAvg(29,37);     // 580 - 760
      fftCalc[ 9] = fftAddAvg(37,48);     // 740 - 980
      fftCalc[10] = fftAddAvg(48,64);     // 960 - 1300
      fftCalc[11] = fftAddAvg(64,84);     // 1280 - 1700
      fftCalc[12] = fftAddAvg(84,111);    // 1680 - 2240
      fftCalc[13] = fftAddAvg(111,147);   // 2220 - 2960
      fftCalc[14] = fftAddAvg(147,194);   // 2940 - 3900
      fftCalc[15] = fftAddAvg(194,250);   // 3880 - 5000 // avoid the last 5 bins, which are usually inaccurate
#else
      /* new mapping, optimized for 22050 Hz by softhack007 */
                                                    // bins frequency  range
      if (useBandPassFilter) {
        // skip frequencies below 100hz
        fftCalc[ 0] = 0.8f * fftAddAvg(3,4);
        fftCalc[ 1] = 0.9f * fftAddAvg(4,5);
        fftCalc[ 2] = fftAddAvg(5,6);
        fftCalc[ 3] = fftAddAvg(6,7);
        // don't use the last bins from 206 to 255. 
        fftCalc[15] = fftAddAvg(165,205) * 0.75f;   // 40 7106 - 8828 high             -- with some damping
      } else {
        fftCalc[ 0] = fftAddAvg(1,2);               // 1    43 - 86   sub-bass
        fftCalc[ 1] = fftAddAvg(2,3);               // 1    86 - 129  bass
        fftCalc[ 2] = fftAddAvg(3,5);               // 2   129 - 216  bass
        fftCalc[ 3] = fftAddAvg(5,7);               // 2   216 - 301  bass + midrange
        // don't use the last bins from 216 to 255. They are usually contaminated by aliasing (aka noise) 
        fftCalc[15] = fftAddAvg(165,215) * 0.70f;   // 50 7106 - 9259 high             -- with some damping
      }
      fftCalc[ 4] = fftAddAvg(7,10);                // 3   301 - 430  midrange
      fftCalc[ 5] = fftAddAvg(10,13);               // 3   430 - 560  midrange
      fftCalc[ 6] = fftAddAvg(13,19);               // 5   560 - 818  midrange
      fftCalc[ 7] = fftAddAvg(19,26);               // 7   818 - 1120 midrange -- 1Khz should always be the center !
      fftCalc[ 8] = fftAddAvg(26,33);               // 7  1120 - 1421 midrange
      fftCalc[ 9] = fftAddAvg(33,44);               // 9  1421 - 1895 midrange
      fftCalc[10] = fftAddAvg(44,56);               // 12 1895 - 2412 midrange + high mid
      fftCalc[11] = fftAddAvg(56,70);               // 14 2412 - 3015 high mid
      fftCalc[12] = fftAddAvg(70,86);               // 16 3015 - 3704 high mid
      fftCalc[13] = fftAddAvg(86,104);              // 18 3704 - 4479 high mid
      fftCalc[14] = fftAddAvg(104,165) * 0.88f;     // 61 4479 - 7106 high mid + high  -- with slight damping
#endif
    } else {  // noise gate closed - just decay old values
      for (int i=0; i < NUM_GEQ_CHANNELS; i++) {
        fftCalc[i] *= 0.85f;  // decay to zero
        if (fftCalc[i] < 4.0f) fftCalc[i] = 0.0f;
      }
    }

    // post-processing of frequency channels (pink noise adjustment, AGC, smoothing, scaling)
    postProcessFFTResults((fabsf(sampleAvg) > 0.25f)? true : false , NUM_GEQ_CHANNELS);

#if defined(WLED_DEBUG) || defined(SR_DEBUG)
    if (haveDoneFFT && (start < esp_timer_get_time())) { // filter out overflows
      uint64_t fftTimeInMillis = ((esp_timer_get_time() - start) +5ULL) / 10ULL; // "+5" to ensure proper rounding
      fftTime  = (fftTimeInMillis*3 + fftTime*7)/10; // smooth
    }
#endif
    // run peak detection
    autoResetPeak();
    detectSamplePeak();
    
    #if !defined(I2S_GRAB_ADC1_COMPLETELY)    
    if ((audioSource == nullptr) || (audioSource->getType() != AudioSource::Type_I2SAdc))  // the "delay trick" does not help for analog ADC
    #endif
      vTaskDelayUntil( &xLastWakeTime, xFrequency);        // release CPU, and let I2S fill its buffers

  } // for(;;)ever
} // FFTcode() task end


///////////////////////////
// Pre / Postprocessing  //
///////////////////////////

static void runMicFilter(uint16_t numSamples, float *sampleBuffer)          // pre-filtering of raw samples (band-pass)
{
  // low frequency cutoff parameter - see https://dsp.stackexchange.com/questions/40462/exponential-moving-average-cut-off-frequency
  //constexpr float alpha = 0.04f;   // 150Hz
  //constexpr float alpha = 0.03f;   // 110Hz
  constexpr float alpha = 0.0225f; // 80hz
  //constexpr float alpha = 0.01693f;// 60hz
  // high frequency cutoff  parameter
  //constexpr float beta1 = 0.75f;   // 11Khz
  //constexpr float beta1 = 0.82f;   // 15Khz
  //constexpr float beta1 = 0.8285f; // 18Khz
  constexpr float beta1 = 0.85f;  // 20Khz

  constexpr float beta2 = (1.0f - beta1) / 2.0f;
  static float last_vals[2] = { 0.0f }; // FIR high freq cutoff filter
  static float lowfilt = 0.0f;          // IIR low frequency cutoff filter

  for (int i=0; i < numSamples; i++) {
        // FIR lowpass, to remove high frequency noise
        float highFilteredSample;
        if (i < (numSamples-1)) highFilteredSample = beta1*sampleBuffer[i] + beta2*last_vals[0] + beta2*sampleBuffer[i+1];  // smooth out spikes
        else highFilteredSample = beta1*sampleBuffer[i] + beta2*last_vals[0]  + beta2*last_vals[1];                  // special handling for last sample in array
        last_vals[1] = last_vals[0];
        last_vals[0] = sampleBuffer[i];
        sampleBuffer[i] = highFilteredSample;
        // IIR highpass, to remove low frequency noise
        lowfilt += alpha * (sampleBuffer[i] - lowfilt);
        sampleBuffer[i] = sampleBuffer[i] - lowfilt;
  }
}

static void postProcessFFTResults(bool noiseGateOpen, int numberOfChannels) // post-processing and post-amp of GEQ channels
{
    for (int i=0; i < numberOfChannels; i++) {

      if (noiseGateOpen) { // noise gate open
        // Adjustment for frequency curves.
        fftCalc[i] *= fftResultPink[i];
        if (FFTScalingMode > 0) fftCalc[i] *= FFT_DOWNSCALE;  // adjustment related to FFT windowing function
        // Manual linear adjustment of gain using sampleGain adjustment for different input types.
        fftCalc[i] *= soundAgc ? multAgc : ((float)sampleGain/40.0f * (float)inputLevel/128.0f + 1.0f/16.0f); //apply gain, with inputLevel adjustment
        if(fftCalc[i] < 0) fftCalc[i] = 0;
      }

      // smooth results - rise fast, fall slower
      if(fftCalc[i] > fftAvg[i])   // rise fast 
        fftAvg[i] = fftCalc[i] *0.75f + 0.25f*fftAvg[i];  // will need approx 2 cycles (50ms) for converging against fftCalc[i]
      else {                       // fall slow
        if (decayTime < 1000) fftAvg[i] = fftCalc[i]*0.22f + 0.78f*fftAvg[i];       // approx  5 cycles (225ms) for falling to zero
        else if (decayTime < 2000) fftAvg[i] = fftCalc[i]*0.17f + 0.83f*fftAvg[i];  // default - approx  9 cycles (225ms) for falling to zero
        else if (decayTime < 3000) fftAvg[i] = fftCalc[i]*0.14f + 0.86f*fftAvg[i];  // approx 14 cycles (350ms) for falling to zero
        else fftAvg[i] = fftCalc[i]*0.1f  + 0.9f*fftAvg[i];                         // approx 20 cycles (500ms) for falling to zero
      }
      // constrain internal vars - just to be sure
      fftCalc[i] = constrain(fftCalc[i], 0.0f, 1023.0f);
      fftAvg[i] = constrain(fftAvg[i], 0.0f, 1023.0f);

      float currentResult;
      if(limiterOn == true)
        currentResult = fftAvg[i];
      else
        currentResult = fftCalc[i];

      switch (FFTScalingMode) {
        case 1:
            // Logarithmic scaling
            currentResult *= 0.42f;                      // 42 is the answer ;-)
            currentResult -= 8.0f;                       // this skips the lowest row, giving some room for peaks
            if (currentResult > 1.0f) currentResult = logf(currentResult); // log to base "e", which is the fastest log() function
            else currentResult = 0.0f;                   // special handling, because log(1) = 0; log(0) = undefined
            currentResult *= 0.85f + (float(i)/18.0f);  // extra up-scaling for high frequencies
            currentResult = mapf(currentResult, 0, LOG_256, 0, 255); // map [log(1) ... log(255)] to [0 ... 255]
        break;
        case 2:
            // Linear scaling
            currentResult *= 0.30f;                     // needs a bit more damping, get stay below 255
            currentResult -= 4.0f;                       // giving a bit more room for peaks
            if (currentResult < 1.0f) currentResult = 0.0f;
            currentResult *= 0.85f + (float(i)/1.8f);   // extra up-scaling for high frequencies
        break;
        case 3:
            // square root scaling
            currentResult *= 0.38f;
            currentResult -= 6.0f;
            if (currentResult > 1.0f) currentResult = sqrtf(currentResult);
            else currentResult = 0.0f;                   // special handling, because sqrt(0) = undefined
            currentResult *= 0.85f + (float(i)/4.5f);   // extra up-scaling for high frequencies
            currentResult = mapf(currentResult, 0.0, 16.0, 0.0, 255.0); // map [sqrt(1) ... sqrt(256)] to [0 ... 255]
        break;

        case 0:
        default:
            // no scaling - leave freq bins as-is
            currentResult -= 4; // just a bit more room for peaks
        break;
      }

      // Now, let's dump it all into fftResult. Need to do this, otherwise other routines might grab fftResult values prematurely.
      if (soundAgc > 0) {  // apply extra "GEQ Gain" if set by user
        float post_gain = (float)inputLevel/128.0f;
        if (post_gain < 1.0f) post_gain = ((post_gain -1.0f) * 0.8f) +1.0f;
        currentResult *= post_gain;
      }
      fftResult[i] = constrain((int)currentResult, 0, 255);
    }
}
////////////////////
// Peak detection //
////////////////////

// peak detection is called from FFT task when vReal[] contains valid FFT results
static void detectSamplePeak(void) {
  bool havePeak = false;
  // softhack007: this code continuously triggers while amplitude in the selected bin is above a certain threshold. So it does not detect peaks - it detects high activity in a frequency bin.
  // Poor man's beat detection by seeing if sample > Average + some value.
  // This goes through ALL of the 255 bins - but ignores stupid settings
  // Then we got a peak, else we don't. The peak has to time out on its own in order to support UDP sound sync.
  if ((sampleAvg > 1) && (maxVol > 0) && (binNum > 4) && (vReal[binNum] > maxVol) && ((millis() - timeOfPeak) > 100)) {
    havePeak = true;
  }

  if (havePeak) {
    samplePeak    = true;
    timeOfPeak    = millis();
    udpSamplePeak = true;
  }
}

#endif

static void autoResetPeak(void) {
  uint16_t peakDelay = max(uint16_t(50), strip.getFrameTime());
  if (millis() - timeOfPeak > peakDelay) {          // Auto-reset of samplePeak after at least one complete frame has passed.
    samplePeak = false;
    if (audioSyncEnabled == 0) udpSamplePeak = false;  // this is normally reset by transmitAudioData
  }
}


////////////////////
// usermod class  //
////////////////////

//class name. Use something descriptive and leave the ": public Usermod" part :)
class AudioReactive : public Usermod {

  private:
#ifdef ARDUINO_ARCH_ESP32

    #ifndef AUDIOPIN
    int8_t audioPin = -1;
    #else
    int8_t audioPin = AUDIOPIN;
    #endif
    #ifndef SR_DMTYPE // I2S mic type
    uint8_t dmType = 1; // 0=none/disabled/analog; 1=generic I2S
    #define SR_DMTYPE 1 // default type = I2S
    #else
    uint8_t dmType = SR_DMTYPE;
    #endif
    #ifndef I2S_SDPIN // aka DOUT
    int8_t i2ssdPin = 32;
    #else
    int8_t i2ssdPin = I2S_SDPIN;
    #endif
    #ifndef I2S_WSPIN // aka LRCL
    int8_t i2swsPin = 15;
    #else
    int8_t i2swsPin = I2S_WSPIN;
    #endif
    #ifndef I2S_CKPIN // aka BCLK
    int8_t i2sckPin = 14; /*PDM: set to I2S_PIN_NO_CHANGE*/
    #else
    int8_t i2sckPin = I2S_CKPIN;
    #endif
    #ifndef MCLK_PIN
    int8_t mclkPin = I2S_PIN_NO_CHANGE;  /* ESP32: only -1, 0, 1, 3 allowed*/
    #else
    int8_t mclkPin = MCLK_PIN;
    #endif
#endif

    // new "V2" audiosync struct - 44 Bytes
    struct __attribute__ ((packed)) audioSyncPacket {  // "packed" ensures that there are no additional gaps
      char    header[6];      //  06 Bytes  offset 0
      uint8_t reserved1[2];   //  02 Bytes, offset 6  - gap required by the compiler - not used yet 
      float   sampleRaw;      //  04 Bytes  offset 8  - either "sampleRaw" or "rawSampleAgc" depending on soundAgc setting
      float   sampleSmth;     //  04 Bytes  offset 12 - either "sampleAvg" or "sampleAgc" depending on soundAgc setting
      uint8_t samplePeak;     //  01 Bytes  offset 16 - 0 no peak; >=1 peak detected. In future, this will also provide peak Magnitude
      uint8_t reserved2;      //  01 Bytes  offset 17 - for future extensions - not used yet
      uint8_t fftResult[16];  //  16 Bytes  offset 18
      uint16_t reserved3;     //  02 Bytes, offset 34 - gap required by the compiler - not used yet
      float  FFT_Magnitude;   //  04 Bytes  offset 36
      float  FFT_MajorPeak;   //  04 Bytes  offset 40
    };

    // old "V1" audiosync struct - 83 Bytes payload, 88 bytes total (with padding added by compiler) - for backwards compatibility
    struct audioSyncPacket_v1 {
      char header[6];         //  06 Bytes
      uint8_t myVals[32];     //  32 Bytes
      int sampleAgc;          //  04 Bytes
      int sampleRaw;          //  04 Bytes
      float sampleAvg;        //  04 Bytes
      bool samplePeak;        //  01 Bytes
      uint8_t fftResult[16];  //  16 Bytes
      double FFT_Magnitude;   //  08 Bytes
      double FFT_MajorPeak;   //  08 Bytes
    };

    #define UDPSOUND_MAX_PACKET 88 // max packet size for audiosync

    // set your config variables to their boot default value (this can also be done in readFromConfig() or a constructor if you prefer)
    #ifdef UM_AUDIOREACTIVE_ENABLE
    bool     enabled = true;
    #else
    bool     enabled = false;
    #endif

    bool     initDone = false;
    bool     addPalettes = false;
    int8_t   palettes = 0;

    // variables  for UDP sound sync
    WiFiUDP fftUdp;               // UDP object for sound sync (from WiFi UDP, not Async UDP!) 
    unsigned long lastTime = 0;   // last time of running UDP Microphone Sync
    const uint16_t delayMs = 10;  // I don't want to sample too often and overload WLED
    uint16_t audioSyncPort= 11988;// default port for UDP sound sync

    bool updateIsRunning = false; // true during OTA.

#ifdef ARDUINO_ARCH_ESP32
    // used for AGC
    int      last_soundAgc = -1;   // used to detect AGC mode change (for resetting AGC internal error buffers)
    double   control_integrated = 0.0;   // persistent across calls to agcAvg(); "integrator control" = accumulated error


    // variables used by getSample() and agcAvg()
    int16_t  micIn = 0;           // Current sample starts with negative values and large values, which is why it's 16 bit signed
    double   sampleMax = 0.0;     // Max sample over a few seconds. Needed for AGC controller.
    double   micLev = 0.0;        // Used to convert returned value to have '0' as minimum. A leveller
    float    expAdjF = 0.0f;      // Used for exponential filter.
    float    sampleReal = 0.0f;	  // "sampleRaw" as float, to provide bits that are lost otherwise (before amplification by sampleGain or inputLevel). Needed for AGC.
    int16_t  sampleRaw = 0;       // Current sample. Must only be updated ONCE!!! (amplified mic value by sampleGain and inputLevel)
    int16_t  rawSampleAgc = 0;    // not smoothed AGC sample
#endif

    // variables used in effects
    float   volumeSmth = 0.0f;    // either sampleAvg or sampleAgc depending on soundAgc; smoothed sample
    int16_t  volumeRaw = 0;       // either sampleRaw or rawSampleAgc depending on soundAgc
    float my_magnitude =0.0f;     // FFT_Magnitude, scaled by multAgc

    // used to feed "Info" Page
    unsigned long last_UDPTime = 0;    // time of last valid UDP sound sync datapacket
    int receivedFormat = 0;            // last received UDP sound sync format - 0=none, 1=v1 (0.13.x), 2=v2 (0.14.x)
    float maxSample5sec = 0.0f;        // max sample (after AGC) in last 5 seconds 
    unsigned long sampleMaxTimer = 0;  // last time maxSample5sec was reset
    #define CYCLE_SAMPLEMAX 3500       // time window for merasuring

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _config[];
    static const char _dynamics[];
    static const char _frequency[];
    static const char _inputLvl[];
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
    static const char _analogmic[];
#endif
    static const char _digitalmic[];
    static const char _addPalettes[];
    static const char UDP_SYNC_HEADER[];
    static const char UDP_SYNC_HEADER_v1[];

    // private methods
    void removeAudioPalettes(void);
    void createAudioPalettes(void);
    CRGB getCRGBForBand(int x, int pal);
    void fillAudioPalettes(void);

    ////////////////////
    // Debug support  //
    ////////////////////
    void logAudio()
    {
      if (disableSoundProcessing && (!udpSyncConnected || ((audioSyncEnabled & 0x02) == 0))) return;   // no audio availeable
    #ifdef MIC_LOGGER
      // Debugging functions for audio input and sound processing. Comment out the values you want to see
      PLOT_PRINT("micReal:");     PLOT_PRINT(micDataReal); PLOT_PRINT("\t");
      PLOT_PRINT("volumeSmth:");  PLOT_PRINT(volumeSmth);  PLOT_PRINT("\t");
      //PLOT_PRINT("volumeRaw:");   PLOT_PRINT(volumeRaw);   PLOT_PRINT("\t");
      PLOT_PRINT("DC_Level:");    PLOT_PRINT(micLev);      PLOT_PRINT("\t");
      //PLOT_PRINT("sampleAgc:");   PLOT_PRINT(sampleAgc);   PLOT_PRINT("\t");
      //PLOT_PRINT("sampleAvg:");   PLOT_PRINT(sampleAvg);   PLOT_PRINT("\t");
      //PLOT_PRINT("sampleReal:");  PLOT_PRINT(sampleReal);  PLOT_PRINT("\t");
    #ifdef ARDUINO_ARCH_ESP32
      //PLOT_PRINT("micIn:");       PLOT_PRINT(micIn);       PLOT_PRINT("\t");
      //PLOT_PRINT("sample:");      PLOT_PRINT(sample);      PLOT_PRINT("\t");
      //PLOT_PRINT("sampleMax:");   PLOT_PRINT(sampleMax);   PLOT_PRINT("\t");
      //PLOT_PRINT("samplePeak:");  PLOT_PRINT((samplePeak!=0) ? 128:0);   PLOT_PRINT("\t");
      //PLOT_PRINT("multAgc:");     PLOT_PRINT(multAgc, 4);  PLOT_PRINT("\t");
    #endif
      PLOT_PRINTLN();
    #endif

    #ifdef FFT_SAMPLING_LOG
      #if 0
        for(int i=0; i<NUM_GEQ_CHANNELS; i++) {
          PLOT_PRINT(fftResult[i]);
          PLOT_PRINT("\t");
        }
        PLOT_PRINTLN();
      #endif

      // OPTIONS are in the following format: Description \n Option
      //
      // Set true if wanting to see all the bands in their own vertical space on the Serial Plotter, false if wanting to see values in Serial Monitor
      const bool mapValuesToPlotterSpace = false;
      // Set true to apply an auto-gain like setting to to the data (this hasn't been tested recently)
      const bool scaleValuesFromCurrentMaxVal = false;
      // prints the max value seen in the current data
      const bool printMaxVal = false;
      // prints the min value seen in the current data
      const bool printMinVal = false;
      // if !scaleValuesFromCurrentMaxVal, we scale values from [0..defaultScalingFromHighValue] to [0..scalingToHighValue], lower this if you want to see smaller values easier
      const int defaultScalingFromHighValue = 256;
      // Print values to terminal in range of [0..scalingToHighValue] if !mapValuesToPlotterSpace, or [(i)*scalingToHighValue..(i+1)*scalingToHighValue] if mapValuesToPlotterSpace
      const int scalingToHighValue = 256;
      // set higher if using scaleValuesFromCurrentMaxVal and you want a small value that's also the current maxVal to look small on the plotter (can't be 0 to avoid divide by zero error)
      const int minimumMaxVal = 1;

      int maxVal = minimumMaxVal;
      int minVal = 0;
      for(int i = 0; i < NUM_GEQ_CHANNELS; i++) {
        if(fftResult[i] > maxVal) maxVal = fftResult[i];
        if(fftResult[i] < minVal) minVal = fftResult[i];
      }
      for(int i = 0; i < NUM_GEQ_CHANNELS; i++) {
        PLOT_PRINT(i); PLOT_PRINT(":");
        PLOT_PRINTF("%04ld ", map(fftResult[i], 0, (scaleValuesFromCurrentMaxVal ? maxVal : defaultScalingFromHighValue), (mapValuesToPlotterSpace*i*scalingToHighValue)+0, (mapValuesToPlotterSpace*i*scalingToHighValue)+scalingToHighValue-1));
      }
      if(printMaxVal) {
        PLOT_PRINTF("maxVal:%04d ", maxVal + (mapValuesToPlotterSpace ? 16*256 : 0));
      }
      if(printMinVal) {
        PLOT_PRINTF("%04d:minVal ", minVal);  // printed with value first, then label, so negative values can be seen in Serial Monitor but don't throw off y axis in Serial Plotter
      }
      if(mapValuesToPlotterSpace)
        PLOT_PRINTF("max:%04d ", (printMaxVal ? 17 : 16)*256); // print line above the maximum value we expect to see on the plotter to avoid autoscaling y axis
      else {
        PLOT_PRINTF("max:%04d ", 256);
      }
      PLOT_PRINTLN();
    #endif // FFT_SAMPLING_LOG
    } // logAudio()


#ifdef ARDUINO_ARCH_ESP32
    //////////////////////
    // Audio Processing //
    //////////////////////

    /*
    * A "PI controller" multiplier to automatically adjust sound sensitivity.
    * 
    * A few tricks are implemented so that sampleAgc does't only utilize 0% and 100%:
    * 0. don't amplify anything below squelch (but keep previous gain)
    * 1. gain input = maximum signal observed in the last 5-10 seconds
    * 2. we use two setpoints, one at ~60%, and one at ~80% of the maximum signal
    * 3. the amplification depends on signal level:
    *    a) normal zone - very slow adjustment
    *    b) emergency zone (<10% or >90%) - very fast adjustment
    */
    void agcAvg(unsigned long the_time)
    {
      const int AGC_preset = (soundAgc > 0)? (soundAgc-1): 0; // make sure the _compiler_ knows this value will not change while we are inside the function

      float lastMultAgc = multAgc;      // last multiplier used
      float multAgcTemp = multAgc;      // new multiplier
      float tmpAgc = sampleReal * multAgc;        // what-if amplified signal

      float control_error;                        // "control error" input for PI control

      if (last_soundAgc != soundAgc)
        control_integrated = 0.0;                // new preset - reset integrator

      // For PI controller, we need to have a constant "frequency"
      // so let's make sure that the control loop is not running at insane speed
      static unsigned long last_time = 0;
      unsigned long time_now = millis();
      if ((the_time > 0) && (the_time < time_now)) time_now = the_time;  // allow caller to override my clock

      if (time_now - last_time > 2)  {
        last_time = time_now;

        if((fabsf(sampleReal) < 2.0f) || (sampleMax < 1.0)) {
          // MIC signal is "squelched" - deliver silence
          tmpAgc = 0;
          // we need to "spin down" the intgrated error buffer
          if (fabs(control_integrated) < 0.01)  control_integrated  = 0.0;
          else                                  control_integrated *= 0.91;
        } else {
          // compute new setpoint
          if (tmpAgc <= agcTarget0Up[AGC_preset])
            multAgcTemp = agcTarget0[AGC_preset] / sampleMax;   // Make the multiplier so that sampleMax * multiplier = first setpoint
          else
            multAgcTemp = agcTarget1[AGC_preset] / sampleMax;   // Make the multiplier so that sampleMax * multiplier = second setpoint
        }
        // limit amplification
        if (multAgcTemp > 32.0f)      multAgcTemp = 32.0f;
        if (multAgcTemp < 1.0f/64.0f) multAgcTemp = 1.0f/64.0f;

        // compute error terms
        control_error = multAgcTemp - lastMultAgc;
        
        if (((multAgcTemp > 0.085f) && (multAgcTemp < 6.5f))    //integrator anti-windup by clamping
            && (multAgc*sampleMax < agcZoneStop[AGC_preset]))   //integrator ceiling (>140% of max)
          control_integrated += control_error * 0.002 * 0.25;   // 2ms = integration time; 0.25 for damping
        else
          control_integrated *= 0.9;                            // spin down that beasty integrator

        // apply PI Control 
        tmpAgc = sampleReal * lastMultAgc;                      // check "zone" of the signal using previous gain
        if ((tmpAgc > agcZoneHigh[AGC_preset]) || (tmpAgc < soundSquelch + agcZoneLow[AGC_preset])) {  // upper/lower energy zone
          multAgcTemp = lastMultAgc + agcFollowFast[AGC_preset] * agcControlKp[AGC_preset] * control_error;
          multAgcTemp += agcFollowFast[AGC_preset] * agcControlKi[AGC_preset] * control_integrated;
        } else {                                                                         // "normal zone"
          multAgcTemp = lastMultAgc + agcFollowSlow[AGC_preset] * agcControlKp[AGC_preset] * control_error;
          multAgcTemp += agcFollowSlow[AGC_preset] * agcControlKi[AGC_preset] * control_integrated;
        }

        // limit amplification again - PI controller sometimes "overshoots"
        //multAgcTemp = constrain(multAgcTemp, 0.015625f, 32.0f); // 1/64 < multAgcTemp < 32
        if (multAgcTemp > 32.0f)      multAgcTemp = 32.0f;
        if (multAgcTemp < 1.0f/64.0f) multAgcTemp = 1.0f/64.0f;
      }

      // NOW finally amplify the signal
      tmpAgc = sampleReal * multAgcTemp;                  // apply gain to signal
      if (fabsf(sampleReal) < 2.0f) tmpAgc = 0.0f;        // apply squelch threshold
      //tmpAgc = constrain(tmpAgc, 0, 255);
      if (tmpAgc > 255) tmpAgc = 255.0f;                  // limit to 8bit
      if (tmpAgc < 1)   tmpAgc = 0.0f;                    // just to be sure

      // update global vars ONCE - multAgc, sampleAGC, rawSampleAgc
      multAgc = multAgcTemp;
      rawSampleAgc = 0.8f * tmpAgc + 0.2f * (float)rawSampleAgc;
      // update smoothed AGC sample
      if (fabsf(tmpAgc) < 1.0f) 
        sampleAgc =  0.5f * tmpAgc + 0.5f * sampleAgc;    // fast path to zero
      else
        sampleAgc += agcSampleSmooth[AGC_preset] * (tmpAgc - sampleAgc); // smooth path

      sampleAgc = fabsf(sampleAgc);                                      // // make sure we have a positive value
      last_soundAgc = soundAgc;
    } // agcAvg()

    // post-processing and filtering of MIC sample (micDataReal) from FFTcode()
    void getSample()
    {
      float    sampleAdj;           // Gain adjusted sample value
      float    tmpSample;           // An interim sample variable used for calculations.
      const float weighting = 0.2f; // Exponential filter weighting. Will be adjustable in a future release.
      const int   AGC_preset = (soundAgc > 0)? (soundAgc-1): 0; // make sure the _compiler_ knows this value will not change while we are inside the function

      #ifdef WLED_DISABLE_SOUND
        micIn = perlin8(millis(), millis());          // Simulated analog read
        micDataReal = micIn;
      #else
        #ifdef ARDUINO_ARCH_ESP32
        micIn = int(micDataReal);      // micDataSm = ((micData * 3) + micData)/4;
        #else
        // this is the minimal code for reading analog mic input on 8266.
        // warning!! Absolutely experimental code. Audio on 8266 is still not working. Expects a million follow-on problems. 
        static unsigned long lastAnalogTime = 0;
        static float lastAnalogValue = 0.0f;
        if (millis() - lastAnalogTime > 20) {
            micDataReal = analogRead(A0); // read one sample with 10bit resolution. This is a dirty hack, supporting volumereactive effects only.
            lastAnalogTime = millis();
            lastAnalogValue = micDataReal;
            yield();
        } else micDataReal = lastAnalogValue;
        micIn = int(micDataReal);
        #endif
      #endif

      micLev += (micDataReal-micLev) / 12288.0f;
      if(micIn < micLev) micLev = ((micLev * 31.0f) + micDataReal) / 32.0f; // align MicLev to lowest input signal

      micIn -= micLev;                                  // Let's center it to 0 now
      // Using an exponential filter to smooth out the signal. We'll add controls for this in a future release.
      float micInNoDC = fabsf(micDataReal - micLev);
      expAdjF = (weighting * micInNoDC + (1.0f-weighting) * expAdjF);
      expAdjF = fabsf(expAdjF);                         // Now (!) take the absolute value

      expAdjF = (expAdjF <= soundSquelch) ? 0: expAdjF; // simple noise gate
      if ((soundSquelch == 0) && (expAdjF < 0.25f)) expAdjF = 0; // do something meaningfull when "squelch = 0"

      tmpSample = expAdjF;
      micIn = abs(micIn);                               // And get the absolute value of each sample

      sampleAdj = tmpSample * sampleGain / 40.0f * inputLevel/128.0f + tmpSample / 16.0f; // Adjust the gain. with inputLevel adjustment
      sampleReal = tmpSample;

      sampleAdj = fmax(fmin(sampleAdj, 255), 0);        // Question: why are we limiting the value to 8 bits ???
      sampleRaw = (int16_t)sampleAdj;                   // ONLY update sample ONCE!!!!

      // keep "peak" sample, but decay value if current sample is below peak
      if ((sampleMax < sampleReal) && (sampleReal > 0.5f)) {
        sampleMax = sampleMax + 0.5f * (sampleReal - sampleMax);  // new peak - with some filtering
        // another simple way to detect samplePeak - cannot detect beats, but reacts on peak volume
        if (((binNum < 12) || ((maxVol < 1))) && (millis() - timeOfPeak > 80) && (sampleAvg > 1)) {
          samplePeak    = true;
          timeOfPeak    = millis();
          udpSamplePeak = true;
        }
      } else {
        if ((multAgc*sampleMax > agcZoneStop[AGC_preset]) && (soundAgc > 0))
          sampleMax += 0.5f * (sampleReal - sampleMax);        // over AGC Zone - get back quickly
        else
          sampleMax *= agcSampleDecay[AGC_preset];             // signal to zero --> 5-8sec
      }
      if (sampleMax < 0.5f) sampleMax = 0.0f;

      sampleAvg = ((sampleAvg * 15.0f) + sampleAdj) / 16.0f;   // Smooth it out over the last 16 samples.
      sampleAvg = fabsf(sampleAvg);                            // make sure we have a positive value
    } // getSample()

#endif

    /* Limits the dynamics of volumeSmth (= sampleAvg or sampleAgc). 
     * does not affect FFTResult[] or volumeRaw ( = sample or rawSampleAgc) 
    */
    // effects: Gravimeter, Gravcenter, Gravcentric, Noisefire, Plasmoid, Freqpixels, Freqwave, Gravfreq, (2D Swirl, 2D Waverly)
    void limitSampleDynamics(void) {
      const float bigChange = 196;                  // just a representative number - a large, expected sample value
      static unsigned long last_time = 0;
      static float last_volumeSmth = 0.0f;

      if (limiterOn == false) return;

      long delta_time = millis() - last_time;
      delta_time = constrain(delta_time , 1, 1000); // below 1ms -> 1ms; above 1sec -> sily lil hick-up
      float deltaSample = volumeSmth - last_volumeSmth;

      if (attackTime > 0) {                         // user has defined attack time > 0
        float maxAttack =   bigChange * float(delta_time) / float(attackTime);
        if (deltaSample > maxAttack) deltaSample = maxAttack;
      }
      if (decayTime > 0) {                          // user has defined decay time > 0
        float maxDecay  = - bigChange * float(delta_time) / float(decayTime);
        if (deltaSample < maxDecay) deltaSample = maxDecay;
      }

      volumeSmth = last_volumeSmth + deltaSample; 

      last_volumeSmth = volumeSmth;
      last_time = millis();
    }


    //////////////////////
    // UDP Sound Sync   //
    //////////////////////

    // try to establish UDP sound sync connection
    void connectUDPSoundSync(void) {
      // This function tries to establish a UDP sync connection if needed
      // necessary as we also want to transmit in "AP Mode", but the standard "connected()" callback only reacts on STA connection
      static unsigned long last_connection_attempt = 0;

      if ((audioSyncPort <= 0) || ((audioSyncEnabled & 0x03) == 0)) return;  // Sound Sync not enabled
      if (udpSyncConnected) return;                                          // already connected
      if (!(apActive || interfacesInited)) return;                           // neither AP nor other connections availeable
      if (millis() - last_connection_attempt < 15000) return;                // only try once in 15 seconds
      if (updateIsRunning) return; 

      // if we arrive here, we need a UDP connection but don't have one
      last_connection_attempt = millis();
      connected(); // try to start UDP
    }

#ifdef ARDUINO_ARCH_ESP32
    void transmitAudioData()
    {
      if (!udpSyncConnected) return;
      //DEBUGSR_PRINTLN("Transmitting UDP Mic Packet");

      audioSyncPacket transmitData;
      memset(reinterpret_cast<void *>(&transmitData), 0, sizeof(transmitData)); // make sure that the packet - including "invisible" padding bytes added by the compiler - is fully initialized

      strncpy_P(transmitData.header, PSTR(UDP_SYNC_HEADER), 6);
      // transmit samples that were not modified by limitSampleDynamics()
      transmitData.sampleRaw   = (soundAgc) ? rawSampleAgc: sampleRaw;
      transmitData.sampleSmth  = (soundAgc) ? sampleAgc   : sampleAvg;
      transmitData.samplePeak  = udpSamplePeak ? 1:0;
      udpSamplePeak            = false;           // Reset udpSamplePeak after we've transmitted it

      for (int i = 0; i < NUM_GEQ_CHANNELS; i++) {
        transmitData.fftResult[i] = (uint8_t)constrain(fftResult[i], 0, 254);
      }

      transmitData.FFT_Magnitude = my_magnitude;
      transmitData.FFT_MajorPeak = FFT_MajorPeak;

      if (fftUdp.beginMulticastPacket() != 0) { // beginMulticastPacket returns 0 in case of error
        fftUdp.write(reinterpret_cast<uint8_t *>(&transmitData), sizeof(transmitData));
        fftUdp.endPacket();
      }
      return;
    } // transmitAudioData()

#endif

    static bool isValidUdpSyncVersion(const char *header) {
      return strncmp_P(header, UDP_SYNC_HEADER, 6) == 0;
    }
    static bool isValidUdpSyncVersion_v1(const char *header) {
      return strncmp_P(header, UDP_SYNC_HEADER_v1, 6) == 0;
    }

    void decodeAudioData(int packetSize, uint8_t *fftBuff) {
      audioSyncPacket receivedPacket;
      memset(&receivedPacket, 0, sizeof(receivedPacket));                                  // start clean
      memcpy(&receivedPacket, fftBuff, min((unsigned)packetSize, (unsigned)sizeof(receivedPacket))); // don't violate alignment - thanks @willmmiles#

      // update samples for effects
      volumeSmth   = fmaxf(receivedPacket.sampleSmth, 0.0f);
      volumeRaw    = fmaxf(receivedPacket.sampleRaw, 0.0f);
#ifdef ARDUINO_ARCH_ESP32
      // update internal samples
      sampleRaw    = volumeRaw;
      sampleAvg    = volumeSmth;
      rawSampleAgc = volumeRaw;
      sampleAgc    = volumeSmth;
      multAgc      = 1.0f;   
#endif
      // Only change samplePeak IF it's currently false.
      // If it's true already, then the animation still needs to respond.
      autoResetPeak();
      if (!samplePeak) {
            samplePeak = receivedPacket.samplePeak >0 ? true:false;
            if (samplePeak) timeOfPeak = millis();
            //userVar1 = samplePeak;
      }
      //These values are only computed by ESP32
      for (int i = 0; i < NUM_GEQ_CHANNELS; i++) fftResult[i] = receivedPacket.fftResult[i];
      my_magnitude  = fmaxf(receivedPacket.FFT_Magnitude, 0.0f);
      FFT_Magnitude = my_magnitude;
      FFT_MajorPeak = constrain(receivedPacket.FFT_MajorPeak, 1.0f, 11025.0f);  // restrict value to range expected by effects
    }

    void decodeAudioData_v1(int packetSize, uint8_t *fftBuff) {
      audioSyncPacket_v1 *receivedPacket = reinterpret_cast<audioSyncPacket_v1*>(fftBuff);
      // update samples for effects
      volumeSmth   = fmaxf(receivedPacket->sampleAgc, 0.0f);
      volumeRaw    = volumeSmth;   // V1 format does not have "raw" AGC sample
#ifdef ARDUINO_ARCH_ESP32
      // update internal samples
      sampleRaw    = fmaxf(receivedPacket->sampleRaw, 0.0f);
      sampleAvg    = fmaxf(receivedPacket->sampleAvg, 0.0f);;
      sampleAgc    = volumeSmth;
      rawSampleAgc = volumeRaw;
      multAgc      = 1.0f;
#endif 
      // Only change samplePeak IF it's currently false.
      // If it's true already, then the animation still needs to respond.
      autoResetPeak();
      if (!samplePeak) {
            samplePeak = receivedPacket->samplePeak >0 ? true:false;
            if (samplePeak) timeOfPeak = millis();
            //userVar1 = samplePeak;
      }
      //These values are only available on the ESP32
      for (int i = 0; i < NUM_GEQ_CHANNELS; i++) fftResult[i] = receivedPacket->fftResult[i];
      my_magnitude  = fmaxf(receivedPacket->FFT_Magnitude, 0.0);
      FFT_Magnitude = my_magnitude;
      FFT_MajorPeak = constrain(receivedPacket->FFT_MajorPeak, 1.0, 11025.0);  // restrict value to range expected by effects
    }

    bool receiveAudioData()   // check & process new data. return TRUE in case that new audio data was received. 
    {
      if (!udpSyncConnected) return false;
      bool haveFreshData = false;

      size_t packetSize = fftUdp.parsePacket();
#ifdef ARDUINO_ARCH_ESP32
      if ((packetSize > 0) && ((packetSize < 5) || (packetSize > UDPSOUND_MAX_PACKET))) fftUdp.flush(); // discard invalid packets (too small or too big) - only works on esp32
#endif
      if ((packetSize > 5) && (packetSize <= UDPSOUND_MAX_PACKET)) {
        //DEBUGSR_PRINTLN("Received UDP Sync Packet");
        uint8_t fftBuff[UDPSOUND_MAX_PACKET+1] = { 0 }; // fixed-size buffer for receiving (stack), to avoid heap fragmentation caused by variable sized arrays
        fftUdp.read(fftBuff, packetSize);

        // VERIFY THAT THIS IS A COMPATIBLE PACKET
        if (packetSize == sizeof(audioSyncPacket) && (isValidUdpSyncVersion((const char *)fftBuff))) {
          decodeAudioData(packetSize, fftBuff);
          //DEBUGSR_PRINTLN("Finished parsing UDP Sync Packet v2");
          haveFreshData = true;
          receivedFormat = 2;
        } else {
          if (packetSize == sizeof(audioSyncPacket_v1) && (isValidUdpSyncVersion_v1((const char *)fftBuff))) {
            decodeAudioData_v1(packetSize, fftBuff);
            //DEBUGSR_PRINTLN("Finished parsing UDP Sync Packet v1");
            haveFreshData = true;
            receivedFormat = 1;
          } else receivedFormat = 0; // unknown format
        }
      }
      return haveFreshData;
    }


    //////////////////////
    // usermod functions//
    //////////////////////

  public:
    //Functions called by WLED or other usermods

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     * It is called *AFTER* readFromConfig()
     */
    void setup() override
    {
      disableSoundProcessing = true; // just to be sure
      if (!initDone) {
        // usermod exchangeable data
        // we will assign all usermod exportable data here as pointers to original variables or arrays and allocate memory for pointers
        um_data = new um_data_t;
        um_data->u_size = 8;
        um_data->u_type = new um_types_t[um_data->u_size];
        um_data->u_data = new void*[um_data->u_size];
        um_data->u_data[0] = &volumeSmth;      //*used (New)
        um_data->u_type[0] = UMT_FLOAT;
        um_data->u_data[1] = &volumeRaw;      // used (New)
        um_data->u_type[1] = UMT_UINT16;
        um_data->u_data[2] = fftResult;        //*used (Blurz, DJ Light, Noisemove, GEQ_base, 2D Funky Plank, Akemi)
        um_data->u_type[2] = UMT_BYTE_ARR;
        um_data->u_data[3] = &samplePeak;      //*used (Puddlepeak, Ripplepeak, Waterfall)
        um_data->u_type[3] = UMT_BYTE;
        um_data->u_data[4] = &FFT_MajorPeak;   //*used (Ripplepeak, Freqmap, Freqmatrix, Freqpixels, Freqwave, Gravfreq, Rocktaves, Waterfall)
        um_data->u_type[4] = UMT_FLOAT;
        um_data->u_data[5] = &my_magnitude;   // used (New)
        um_data->u_type[5] = UMT_FLOAT;
        um_data->u_data[6] = &maxVol;          // assigned in effect function from UI element!!! (Puddlepeak, Ripplepeak, Waterfall)
        um_data->u_type[6] = UMT_BYTE;
        um_data->u_data[7] = &binNum;          // assigned in effect function from UI element!!! (Puddlepeak, Ripplepeak, Waterfall)
        um_data->u_type[7] = UMT_BYTE;
      }


#ifdef ARDUINO_ARCH_ESP32

      // Reset I2S peripheral for good measure
      i2s_driver_uninstall(I2S_NUM_0);   // E (696) I2S: i2s_driver_uninstall(2006): I2S port 0 has not installed
      #if !defined(CONFIG_IDF_TARGET_ESP32C3)
        delay(100);
        periph_module_reset(PERIPH_I2S0_MODULE);   // not possible on -C3
      #endif
      delay(100);         // Give that poor microphone some time to setup.

      useBandPassFilter = false;

      #if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
        if ((i2sckPin == I2S_PIN_NO_CHANGE) && (i2ssdPin >= 0) && (i2swsPin >= 0) && ((dmType == 1) || (dmType == 4)) ) dmType = 5;   // dummy user support: SCK == -1 --means--> PDM microphone
      #endif

      switch (dmType) {
      #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
        // stub cases for not-yet-supported I2S modes on other ESP32 chips
        case 0:  //ADC analog
        #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3)
        case 5:  //PDM Microphone
        #endif
      #endif
        case 1:
          DEBUGSR_PRINT(F("AR: Generic I2S Microphone - ")); DEBUGSR_PRINTLN(F(I2S_MIC_CHANNEL_TEXT));
          audioSource = new I2SSource(SAMPLE_RATE, BLOCK_SIZE);
          delay(100);
          if (audioSource) audioSource->initialize(i2swsPin, i2ssdPin, i2sckPin);
          break;
        case 2:
          DEBUGSR_PRINTLN(F("AR: ES7243 Microphone (right channel only)."));
          audioSource = new ES7243(SAMPLE_RATE, BLOCK_SIZE);
          delay(100);
          if (audioSource) audioSource->initialize(i2swsPin, i2ssdPin, i2sckPin, mclkPin);
          break;
        case 3:
          DEBUGSR_PRINT(F("AR: SPH0645 Microphone - ")); DEBUGSR_PRINTLN(F(I2S_MIC_CHANNEL_TEXT));
          audioSource = new SPH0654(SAMPLE_RATE, BLOCK_SIZE);
          delay(100);
          audioSource->initialize(i2swsPin, i2ssdPin, i2sckPin);
          break;
        case 4:
          DEBUGSR_PRINT(F("AR: Generic I2S Microphone with Master Clock - ")); DEBUGSR_PRINTLN(F(I2S_MIC_CHANNEL_TEXT));
          audioSource = new I2SSource(SAMPLE_RATE, BLOCK_SIZE, 1.0f/24.0f);
          delay(100);
          if (audioSource) audioSource->initialize(i2swsPin, i2ssdPin, i2sckPin, mclkPin);
          break;
        #if  !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
        case 5:
          DEBUGSR_PRINT(F("AR: I2S PDM Microphone - ")); DEBUGSR_PRINTLN(F(I2S_PDM_MIC_CHANNEL_TEXT));
          audioSource = new I2SSource(SAMPLE_RATE, BLOCK_SIZE, 1.0f/4.0f);
          useBandPassFilter = true;  // this reduces the noise floor on SPM1423 from 5% Vpp (~380) down to 0.05% Vpp (~5)
          delay(100);
          if (audioSource) audioSource->initialize(i2swsPin, i2ssdPin);
          break;
        #endif
        case 6:
          DEBUGSR_PRINTLN(F("AR: ES8388 Source"));
          audioSource = new ES8388Source(SAMPLE_RATE, BLOCK_SIZE);
          delay(100);
          if (audioSource) audioSource->initialize(i2swsPin, i2ssdPin, i2sckPin, mclkPin);
          break;

        #if  !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
        // ADC over I2S is only possible on "classic" ESP32
        case 0:
        default:
          DEBUGSR_PRINTLN(F("AR: Analog Microphone (left channel only)."));
          audioSource = new I2SAdcSource(SAMPLE_RATE, BLOCK_SIZE);
          delay(100);
          useBandPassFilter = true;  // PDM bandpass filter seems to help for bad quality analog
          if (audioSource) audioSource->initialize(audioPin);
          break;
        #endif
      }
      delay(250); // give microphone enough time to initialise

      if (!audioSource) enabled = false;                 // audio failed to initialise
#endif
      if (enabled) onUpdateBegin(false);                 // create FFT task, and initialize network


#ifdef ARDUINO_ARCH_ESP32
      if (FFT_Task == nullptr) enabled = false;          // FFT task creation failed
      if((!audioSource) || (!audioSource->isInitialized())) {  // audio source failed to initialize. Still stay "enabled", as there might be input arriving via UDP Sound Sync 
      #ifdef WLED_DEBUG
        DEBUG_PRINTLN(F("AR: Failed to initialize sound input driver. Please check input PIN settings."));
      #else
        DEBUGSR_PRINTLN(F("AR: Failed to initialize sound input driver. Please check input PIN settings."));
      #endif
        disableSoundProcessing = true;
      }
#endif
      if (enabled) disableSoundProcessing = false;       // all good - enable audio processing
      if (enabled) connectUDPSoundSync();
      if (enabled && addPalettes) createAudioPalettes();
      initDone = true;
    }


    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() override
    {
      if (udpSyncConnected) {   // clean-up: if open, close old UDP sync connection
        udpSyncConnected = false;
        fftUdp.stop();
      }
      
      if (audioSyncPort > 0 && (audioSyncEnabled & 0x03)) {
      #ifdef ARDUINO_ARCH_ESP32
        udpSyncConnected = fftUdp.beginMulticast(IPAddress(239, 0, 0, 1), audioSyncPort);
      #else
        udpSyncConnected = fftUdp.beginMulticast(WiFi.localIP(), IPAddress(239, 0, 0, 1), audioSyncPort);
      #endif
      }
    }


    /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.
     * 
     * Tips:
     * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
     *    Additionally, "if (WLED_MQTT_CONNECTED)" is available to check for a connection to an MQTT broker.
     * 
     * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
     *    Instead, use a timer check as shown here.
     */
    void loop() override
    {
      static unsigned long lastUMRun = millis();

      if (!enabled) {
        disableSoundProcessing = true;   // keep processing suspended (FFT task)
        lastUMRun = millis();            // update time keeping
        return;
      }
      // We cannot wait indefinitely before processing audio data
      if (strip.isUpdating() && (millis() - lastUMRun < 2)) return;   // be nice, but not too nice

      // suspend local sound processing when "real time mode" is active (E131, UDP, ADALIGHT, ARTNET)
      if (  (realtimeOverride == REALTIME_OVERRIDE_NONE)  // please add other overrides here if needed
          &&( (realtimeMode == REALTIME_MODE_GENERIC)
            ||(realtimeMode == REALTIME_MODE_E131)
            ||(realtimeMode == REALTIME_MODE_UDP)
            ||(realtimeMode == REALTIME_MODE_ADALIGHT)
            ||(realtimeMode == REALTIME_MODE_ARTNET) ) )  // please add other modes here if needed
      {
        #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_DEBUG)
        if ((disableSoundProcessing == false) && (audioSyncEnabled == 0)) {  // we just switched to "disabled"
          DEBUG_PRINTLN(F("[AR userLoop]  realtime mode active - audio processing suspended."));
          DEBUG_PRINTF_P(PSTR("               RealtimeMode = %d; RealtimeOverride = %d\n"), int(realtimeMode), int(realtimeOverride));
        }
        #endif
        disableSoundProcessing = true;
      } else {
        #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_DEBUG)
        if ((disableSoundProcessing == true) && (audioSyncEnabled == 0) && audioSource->isInitialized()) {    // we just switched to "enabled"
          DEBUG_PRINTLN(F("[AR userLoop]  realtime mode ended - audio processing resumed."));
          DEBUG_PRINTF_P(PSTR("               RealtimeMode = %d; RealtimeOverride = %d\n"), int(realtimeMode), int(realtimeOverride));
        }
        #endif
        if ((disableSoundProcessing == true) && (audioSyncEnabled == 0)) lastUMRun = millis();  // just left "realtime mode" - update timekeeping
        disableSoundProcessing = false;
      }

      if (audioSyncEnabled & 0x02) disableSoundProcessing = true;   // make sure everything is disabled IF in audio Receive mode
      if (audioSyncEnabled & 0x01) disableSoundProcessing = false;  // keep running audio IF we're in audio Transmit mode
#ifdef ARDUINO_ARCH_ESP32
      if (!audioSource->isInitialized()) disableSoundProcessing = true;  // no audio source


      // Only run the sampling code IF we're not in Receive mode or realtime mode
      if (!(audioSyncEnabled & 0x02) && !disableSoundProcessing) {
        if (soundAgc > AGC_NUM_PRESETS) soundAgc = 0; // make sure that AGC preset is valid (to avoid array bounds violation)

        unsigned long t_now = millis();      // remember current time
        int userloopDelay = int(t_now - lastUMRun);
        if (lastUMRun == 0) userloopDelay=0; // startup - don't have valid data from last run.

        #ifdef WLED_DEBUG
          // complain when audio userloop has been delayed for long time. Currently we need userloop running between 500 and 1500 times per second. 
          // softhack007 disabled temporarily - avoid serial console spam with MANY leds and low FPS
          //if ((userloopDelay > 65) && !disableSoundProcessing && (audioSyncEnabled == 0)) {
          //  DEBUG_PRINTF_P(PSTR("[AR userLoop] hiccup detected -> was inactive for last %d millis!\n"), userloopDelay);
          //}
        #endif

        // run filters, and repeat in case of loop delays (hick-up compensation)
        if (userloopDelay <2) userloopDelay = 0;      // minor glitch, no problem
        if (userloopDelay >200) userloopDelay = 200;  // limit number of filter re-runs  
        do {
          getSample();                        // run microphone sampling filters
          agcAvg(t_now - userloopDelay);      // Calculated the PI adjusted value as sampleAvg
          userloopDelay -= 2;                 // advance "simulated time" by 2ms
        } while (userloopDelay > 0);
        lastUMRun = t_now;                    // update time keeping

        // update samples for effects (raw, smooth) 
        volumeSmth = (soundAgc) ? sampleAgc   : sampleAvg;
        volumeRaw  = (soundAgc) ? rawSampleAgc: sampleRaw;
        // update FFTMagnitude, taking into account AGC amplification
        my_magnitude = FFT_Magnitude; // / 16.0f, 8.0f, 4.0f done in effects
        if (soundAgc) my_magnitude *= multAgc;
        if (volumeSmth < 1 ) my_magnitude = 0.001f;  // noise gate closed - mute

        limitSampleDynamics();
      }  // if (!disableSoundProcessing)
#endif

      autoResetPeak();          // auto-reset sample peak after strip minShowDelay
      if (!udpSyncConnected) udpSamplePeak = false;  // reset UDP samplePeak while UDP is unconnected

      connectUDPSoundSync();  // ensure we have a connection - if needed

      // UDP Microphone Sync  - receive mode
      if ((audioSyncEnabled & 0x02) && udpSyncConnected) {
          // Only run the audio listener code if we're in Receive mode
          static float syncVolumeSmth = 0;
          bool have_new_sample = false;
          if (millis() - lastTime > delayMs) {
            have_new_sample = receiveAudioData();
            if (have_new_sample) last_UDPTime = millis();
#ifdef ARDUINO_ARCH_ESP32
            else fftUdp.flush(); // Flush udp input buffers if we haven't read it - avoids hickups in receive mode. Does not work on 8266.
#endif
            lastTime = millis();
          }
          if (have_new_sample) syncVolumeSmth = volumeSmth;   // remember received sample
          else volumeSmth = syncVolumeSmth;                   // restore originally received sample for next run of dynamics limiter
          limitSampleDynamics();                              // run dynamics limiter on received volumeSmth, to hide jumps and hickups
      }

      #if defined(MIC_LOGGER) || defined(MIC_SAMPLING_LOG) || defined(FFT_SAMPLING_LOG)
      static unsigned long lastMicLoggerTime = 0;
      if (millis()-lastMicLoggerTime > 20) {
        lastMicLoggerTime = millis();
        logAudio();
      }
      #endif

      // Info Page: keep max sample from last 5 seconds
#ifdef ARDUINO_ARCH_ESP32
      if ((millis() -  sampleMaxTimer) > CYCLE_SAMPLEMAX) {
        sampleMaxTimer = millis();
        maxSample5sec = (0.15f * maxSample5sec) + 0.85f *((soundAgc) ? sampleAgc : sampleAvg); // reset, and start with some smoothing
        if (sampleAvg < 1) maxSample5sec = 0; // noise gate 
      } else {
         if ((sampleAvg >= 1)) maxSample5sec = fmaxf(maxSample5sec, (soundAgc) ? rawSampleAgc : sampleRaw); // follow maximum volume
      }
#else  // similar functionality for 8266 receive only - use VolumeSmth instead of raw sample data
      if ((millis() -  sampleMaxTimer) > CYCLE_SAMPLEMAX) {
        sampleMaxTimer = millis();
        maxSample5sec = (0.15 * maxSample5sec) + 0.85 * volumeSmth; // reset, and start with some smoothing
        if (volumeSmth < 1.0f) maxSample5sec = 0; // noise gate
        if (maxSample5sec < 0.0f) maxSample5sec = 0; // avoid negative values
      } else {
         if (volumeSmth >= 1.0f) maxSample5sec = fmaxf(maxSample5sec, volumeRaw); // follow maximum volume
      }
#endif

#ifdef ARDUINO_ARCH_ESP32
      //UDP Microphone Sync  - transmit mode
      if ((audioSyncEnabled & 0x01) && (millis() - lastTime > 20)) {
        // Only run the transmit code IF we're in Transmit mode
        transmitAudioData();
        lastTime = millis();
      }
#endif

      fillAudioPalettes();
    }


    bool getUMData(um_data_t **data) override
    {
      if (!data || !enabled) return false; // no pointer provided by caller or not enabled -> exit
      *data = um_data;
      return true;
    }

#ifdef ARDUINO_ARCH_ESP32
    void onUpdateBegin(bool init) override
    {
#ifdef WLED_DEBUG
      fftTime = sampleTime = 0;
#endif
      // gracefully suspend FFT task (if running)
      disableSoundProcessing = true;

      // reset sound data
      micDataReal = 0.0f;
      volumeRaw = 0; volumeSmth = 0;
      sampleAgc = 0; sampleAvg = 0;
      sampleRaw = 0; rawSampleAgc = 0;
      my_magnitude = 0; FFT_Magnitude = 0; FFT_MajorPeak = 1;
      multAgc = 1;
      // reset FFT data
      memset(fftCalc, 0, sizeof(fftCalc)); 
      memset(fftAvg, 0, sizeof(fftAvg)); 
      memset(fftResult, 0, sizeof(fftResult)); 
      for(int i=(init?0:1); i<NUM_GEQ_CHANNELS; i+=2) fftResult[i] = 16; // make a tiny pattern
      inputLevel = 128;                                    // reset level slider to default
      autoResetPeak();

      if (init && FFT_Task) {
        delay(25);                // give some time for I2S driver to finish sampling before we suspend it
        vTaskSuspend(FFT_Task);   // update is about to begin, disable task to prevent crash
        if (udpSyncConnected) {   // close UDP sync connection (if open)
          udpSyncConnected = false;
          fftUdp.stop();
        }
      } else {
        // update has failed or create task requested
        if (FFT_Task) {
          vTaskResume(FFT_Task);
          connected(); // resume UDP
        } else
          xTaskCreateUniversal(               // xTaskCreateUniversal also works on -S2 and -C3 with single core
            FFTcode,                          // Function to implement the task
            "FFT",                            // Name of the task
            3592,                             // Stack size in words // 3592 leaves 800-1024 bytes of task stack free
            NULL,                             // Task input parameter
            FFTTASK_PRIORITY,                 // Priority of the task
            &FFT_Task                         // Task handle
            , 0                               // Core where the task should run
          );
      }
      micDataReal = 0.0f;                     // just to be sure
      if (enabled) disableSoundProcessing = false;
      updateIsRunning = init;
    }

#else // reduced function for 8266
    void onUpdateBegin(bool init)
    {
      // gracefully suspend audio (if running)
      disableSoundProcessing = true;
      // reset sound data
      volumeRaw = 0; volumeSmth = 0;
      for(int i=(init?0:1); i<NUM_GEQ_CHANNELS; i+=2) fftResult[i] = 16; // make a tiny pattern
      autoResetPeak();
      if (init) {
        if (udpSyncConnected) {   // close UDP sync connection (if open)
          udpSyncConnected = false;
          fftUdp.stop();
          DEBUGSR_PRINTLN(F("AR onUpdateBegin(true): UDP connection closed."));
          receivedFormat = 0;
        }
      }
      if (enabled) disableSoundProcessing = init; // init = true means that OTA is just starting --> don't process audio
      updateIsRunning = init;
    }
#endif

#ifdef ARDUINO_ARCH_ESP32
    /**
     * handleButton() can be used to override default button behaviour. Returning true
     * will prevent button working in a default way.
     */
    bool handleButton(uint8_t b) override {
      yield();
      // crude way of determining if audio input is analog
      // better would be for AudioSource to implement getType()
      if (enabled
          && dmType == 0 && audioPin>=0
          && (buttonType[b] == BTN_TYPE_ANALOG || buttonType[b] == BTN_TYPE_ANALOG_INVERTED)
         ) {
        return true;
      }
      return false;
    }

#endif
    ////////////////////////////
    // Settings and Info Page //
    ////////////////////////////

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject& root) override
    {
#ifdef ARDUINO_ARCH_ESP32
      char myStringBuffer[16]; // buffer for snprintf() - not used yet on 8266
#endif
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray infoArr = user.createNestedArray(FPSTR(_name));

      String uiDomString = F("<button class=\"btn btn-xs\" onclick=\"requestJson({");
      uiDomString += FPSTR(_name);
      uiDomString += F(":{");
      uiDomString += FPSTR(_enabled);
      uiDomString += enabled ? F(":false}});\">") : F(":true}});\">");
      uiDomString += F("<i class=\"icons");
      uiDomString += enabled ? F(" on") : F(" off");
      uiDomString += F("\">&#xe08f;</i>");
      uiDomString += F("</button>");
      infoArr.add(uiDomString);

      if (enabled) {
#ifdef ARDUINO_ARCH_ESP32
        // Input Level Slider
        if (disableSoundProcessing == false) {                                 // only show slider when audio processing is running
          if (soundAgc > 0) {
            infoArr = user.createNestedArray(F("GEQ Input Level"));           // if AGC is on, this slider only affects fftResult[] frequencies
          } else {
            infoArr = user.createNestedArray(F("Audio Input Level"));
          }
          uiDomString = F("<div class=\"slider\"><div class=\"sliderwrap il\"><input class=\"noslide\" onchange=\"requestJson({");
          uiDomString += FPSTR(_name);
          uiDomString += F(":{");
          uiDomString += FPSTR(_inputLvl);
          uiDomString += F(":parseInt(this.value)}});\" oninput=\"updateTrail(this);\" max=255 min=0 type=\"range\" value=");
          uiDomString += inputLevel;
          uiDomString += F(" /><div class=\"sliderdisplay\"></div></div></div>"); //<output class=\"sliderbubble\"></output>
          infoArr.add(uiDomString);
        } 
#endif
        // The following can be used for troubleshooting user errors and is so not enclosed in #ifdef WLED_DEBUG

        // current Audio input
        infoArr = user.createNestedArray(F("Audio Source"));
        if (audioSyncEnabled & 0x02) {
          // UDP sound sync - receive mode
          infoArr.add(F("UDP sound sync"));
          if (udpSyncConnected) {
            if (millis() - last_UDPTime < 2500)
              infoArr.add(F(" - receiving"));
            else
              infoArr.add(F(" - idle"));
          } else {
            infoArr.add(F(" - no connection"));
          }
#ifndef ARDUINO_ARCH_ESP32  // substitute for 8266
        } else {
          infoArr.add(F("sound sync Off"));
        }
#else  // ESP32 only
        } else {
          // Analog or I2S digital input
          if (audioSource && (audioSource->isInitialized())) {
            // audio source successfully configured
            if (audioSource->getType() == AudioSource::Type_I2SAdc) {
              infoArr.add(F("ADC analog"));
            } else {
              infoArr.add(F("I2S digital"));
            }
            // input level or "silence"
            if (maxSample5sec > 1.0f) {
              float my_usage = 100.0f * (maxSample5sec / 255.0f);
              snprintf_P(myStringBuffer, 15, PSTR(" - peak %3d%%"), int(my_usage));
              infoArr.add(myStringBuffer);
            } else {
              infoArr.add(F(" - quiet"));
            }
          } else {
            // error during audio source setup
            infoArr.add(F("not initialized"));
            infoArr.add(F(" - check pin settings"));
          }
        }

        // Sound processing (FFT and input filters)
        infoArr = user.createNestedArray(F("Sound Processing"));
        if (audioSource && (disableSoundProcessing == false)) {
          infoArr.add(F("running"));
        } else {
          infoArr.add(F("suspended"));
        }

        // AGC or manual Gain
        if ((soundAgc==0) && (disableSoundProcessing == false) && !(audioSyncEnabled & 0x02)) {
          infoArr = user.createNestedArray(F("Manual Gain"));
          float myGain = ((float)sampleGain/40.0f * (float)inputLevel/128.0f) + 1.0f/16.0f;     // non-AGC gain from presets
          infoArr.add(roundf(myGain*100.0f) / 100.0f);
          infoArr.add("x");
        }
        if (soundAgc && (disableSoundProcessing == false) && !(audioSyncEnabled & 0x02)) {
          infoArr = user.createNestedArray(F("AGC Gain"));
          infoArr.add(roundf(multAgc*100.0f) / 100.0f);
          infoArr.add("x");
        }
#endif
        // UDP Sound Sync status
        infoArr = user.createNestedArray(F("UDP Sound Sync"));
        if (audioSyncEnabled) {
          if (audioSyncEnabled & 0x01) {
            infoArr.add(F("send mode"));
            if ((udpSyncConnected) && (millis() - lastTime < 2500)) infoArr.add(F(" v2"));
          } else if (audioSyncEnabled & 0x02) {
              infoArr.add(F("receive mode"));
          }
        } else
          infoArr.add("off");
        if (audioSyncEnabled && !udpSyncConnected) infoArr.add(" <i>(unconnected)</i>");
        if (audioSyncEnabled && udpSyncConnected && (millis() - last_UDPTime < 2500)) {
            if (receivedFormat == 1) infoArr.add(F(" v1"));
            if (receivedFormat == 2) infoArr.add(F(" v2"));
        }

        #if defined(WLED_DEBUG) || defined(SR_DEBUG)
        #ifdef ARDUINO_ARCH_ESP32
        infoArr = user.createNestedArray(F("Sampling time"));
        infoArr.add(float(sampleTime)/100.0f);
        infoArr.add(" ms");

        infoArr = user.createNestedArray(F("FFT time"));
        infoArr.add(float(fftTime)/100.0f);
        if ((fftTime/100) >= FFT_MIN_CYCLE) // FFT time over budget -> I2S buffer will overflow 
          infoArr.add("<b style=\"color:red;\">! ms</b>");
        else if ((fftTime/80 + sampleTime/80) >= FFT_MIN_CYCLE) // FFT time >75% of budget -> risk of instability
          infoArr.add("<b style=\"color:orange;\"> ms!</b>");
        else
          infoArr.add(" ms");

        DEBUGSR_PRINTF("AR Sampling time: %5.2f ms\n", float(sampleTime)/100.0f);
        DEBUGSR_PRINTF("AR FFT time     : %5.2f ms\n", float(fftTime)/100.0f);
        #endif
        #endif
      }
    }


    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root) override
    {
      if (!initDone) return;  // prevent crash on boot applyPreset()
      JsonObject usermod = root[FPSTR(_name)];
      if (usermod.isNull()) {
        usermod = root.createNestedObject(FPSTR(_name));
      }
      usermod["on"] = enabled;
    }


    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void readFromJsonState(JsonObject& root) override
    {
      if (!initDone) return;  // prevent crash on boot applyPreset()
      bool prevEnabled = enabled;
      JsonObject usermod = root[FPSTR(_name)];
      if (!usermod.isNull()) {
        if (usermod[FPSTR(_enabled)].is<bool>()) {
          enabled = usermod[FPSTR(_enabled)].as<bool>();
          if (prevEnabled != enabled) onUpdateBegin(!enabled);
          if (addPalettes) {
            // add/remove custom/audioreactive palettes
            if (prevEnabled && !enabled) removeAudioPalettes();
            if (!prevEnabled && enabled) createAudioPalettes();
          }
        }
#ifdef ARDUINO_ARCH_ESP32
        if (usermod[FPSTR(_inputLvl)].is<int>()) {
          inputLevel = min(255,max(0,usermod[FPSTR(_inputLvl)].as<int>()));
        }
#endif
      }
      if (root.containsKey(F("rmcpal")) && root[F("rmcpal")].as<bool>()) {
        // handle removal of custom palettes from JSON call so we don't break things
        removeAudioPalettes();
      }
    }

    void onStateChange(uint8_t callMode) override {
      if (initDone && enabled && addPalettes && palettes==0 && strip.customPalettes.size()<10) {
        // if palettes were removed during JSON call re-add them
        createAudioPalettes();
      }
    }

    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current state, use serializeConfig() in your loop().
     * 
     * CAUTION: serializeConfig() will initiate a filesystem write operation.
     * It might cause the LEDs to stutter and will cause flash wear if called too often.
     * Use it sparingly and always in the loop, never in network callbacks!
     * 
     * addToConfig() will make your settings editable through the Usermod Settings page automatically.
     *
     * Usermod Settings Overview:
     * - Numeric values are treated as floats in the browser.
     *   - If the numeric value entered into the browser contains a decimal point, it will be parsed as a C float
     *     before being returned to the Usermod.  The float data type has only 6-7 decimal digits of precision, and
     *     doubles are not supported, numbers will be rounded to the nearest float value when being parsed.
     *     The range accepted by the input field is +/- 1.175494351e-38 to +/- 3.402823466e+38.
     *   - If the numeric value entered into the browser doesn't contain a decimal point, it will be parsed as a
     *     C int32_t (range: -2147483648 to 2147483647) before being returned to the usermod.
     *     Overflows or underflows are truncated to the max/min value for an int32_t, and again truncated to the type
     *     used in the Usermod when reading the value from ArduinoJson.
     * - Pin values can be treated differently from an integer value by using the key name "pin"
     *   - "pin" can contain a single or array of integer values
     *   - On the Usermod Settings page there is simple checking for pin conflicts and warnings for special pins
     *     - Red color indicates a conflict.  Yellow color indicates a pin with a warning (e.g. an input-only pin)
     *   - Tip: use int8_t to store the pin value in the Usermod, so a -1 value (pin not set) can be used
     *
     * See usermod_v2_auto_save.h for an example that saves Flash space by reusing ArduinoJson key name strings
     * 
     * If you need a dedicated settings page with custom layout for your Usermod, that takes a lot more work.  
     * You will have to add the setting to the HTML, xml.cpp and set.cpp manually.
     * See the WLED Soundreactive fork (code and wiki) for reference.  https://github.com/atuline/WLED
     * 
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root) override
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_addPalettes)] = addPalettes;

#ifdef ARDUINO_ARCH_ESP32
    #if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
      JsonObject amic = top.createNestedObject(FPSTR(_analogmic));
      amic["pin"] = audioPin;
    #endif

      JsonObject dmic = top.createNestedObject(FPSTR(_digitalmic));
      dmic["type"] = dmType;
      JsonArray pinArray = dmic.createNestedArray("pin");
      pinArray.add(i2ssdPin);
      pinArray.add(i2swsPin);
      pinArray.add(i2sckPin);
      pinArray.add(mclkPin);

      JsonObject cfg = top.createNestedObject(FPSTR(_config));
      cfg[F("squelch")] = soundSquelch;
      cfg[F("gain")] = sampleGain;
      cfg[F("AGC")] = soundAgc;

      JsonObject freqScale = top.createNestedObject(FPSTR(_frequency));
      freqScale[F("scale")] = FFTScalingMode;
#endif

      JsonObject dynLim = top.createNestedObject(FPSTR(_dynamics));
      dynLim[F("limiter")] = limiterOn;
      dynLim[F("rise")] = attackTime;
      dynLim[F("fall")] = decayTime;

      JsonObject sync = top.createNestedObject("sync");
      sync["port"] = audioSyncPort;
      sync["mode"] = audioSyncEnabled;
    }


    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens immediately after boot, or after saving on the Usermod Settings page)
     * 
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     * 
     * Return true in case the config values returned from Usermod Settings were complete, or false if you'd like WLED to save your defaults to disk (so any missing values are editable in Usermod Settings)
     * 
     * getJsonValue() returns false if the value is missing, or copies the value into the variable provided and returns true if the value is present
     * The configComplete variable is true only if the "exampleUsermod" object and all values are present.  If any values are missing, WLED will know to call addToConfig() to save them
     * 
     * This function is guaranteed to be called on boot, but could also be called every time settings are updated
     */
    bool readFromConfig(JsonObject& root) override
    {
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();
      bool oldEnabled = enabled;
      bool oldAddPalettes = addPalettes;

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top[FPSTR(_addPalettes)], addPalettes);

#ifdef ARDUINO_ARCH_ESP32
    #if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
      configComplete &= getJsonValue(top[FPSTR(_analogmic)]["pin"], audioPin);
    #else
      audioPin = -1; // MCU does not support analog mic
    #endif

      configComplete &= getJsonValue(top[FPSTR(_digitalmic)]["type"],   dmType);
    #if  defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S3)
      if (dmType == 0) dmType = SR_DMTYPE;   // MCU does not support analog
      #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3)
      if (dmType == 5) dmType = SR_DMTYPE;   // MCU does not support PDM
      #endif
    #endif

      configComplete &= getJsonValue(top[FPSTR(_digitalmic)]["pin"][0], i2ssdPin);
      configComplete &= getJsonValue(top[FPSTR(_digitalmic)]["pin"][1], i2swsPin);
      configComplete &= getJsonValue(top[FPSTR(_digitalmic)]["pin"][2], i2sckPin);
      configComplete &= getJsonValue(top[FPSTR(_digitalmic)]["pin"][3], mclkPin);

      configComplete &= getJsonValue(top[FPSTR(_config)][F("squelch")], soundSquelch);
      configComplete &= getJsonValue(top[FPSTR(_config)][F("gain")],    sampleGain);
      configComplete &= getJsonValue(top[FPSTR(_config)][F("AGC")],     soundAgc);

      configComplete &= getJsonValue(top[FPSTR(_frequency)][F("scale")], FFTScalingMode);

      configComplete &= getJsonValue(top[FPSTR(_dynamics)][F("limiter")], limiterOn);
      configComplete &= getJsonValue(top[FPSTR(_dynamics)][F("rise")],  attackTime);
      configComplete &= getJsonValue(top[FPSTR(_dynamics)][F("fall")],  decayTime);
#endif
      configComplete &= getJsonValue(top["sync"]["port"], audioSyncPort);
      configComplete &= getJsonValue(top["sync"]["mode"], audioSyncEnabled);

      if (initDone) {
        // add/remove custom/audioreactive palettes
        if ((oldAddPalettes && !addPalettes) || (oldAddPalettes && !enabled)) removeAudioPalettes();
        if ((addPalettes && !oldAddPalettes && enabled) || (addPalettes && !oldEnabled && enabled)) createAudioPalettes();
      } // else setup() will create palettes
      return configComplete;
    }


    void appendConfigData(Print& uiScript) override
    {
      uiScript.print(F("ux='AudioReactive';"));         // ux = shortcut for Audioreactive - fingers crossed that "ux" isn't already used as JS var, html post parameter or css style
#ifdef ARDUINO_ARCH_ESP32
      uiScript.print(F("uxp=ux+':digitalmic:pin[]';")); // uxp = shortcut for AudioReactive:digitalmic:pin[]
      uiScript.print(F("dd=addDropdown(ux,'digitalmic:type');"));
    #if  !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
      uiScript.print(F("addOption(dd,'Generic Analog',0);"));
    #endif
      uiScript.print(F("addOption(dd,'Generic I2S',1);"));
      uiScript.print(F("addOption(dd,'ES7243',2);"));
      uiScript.print(F("addOption(dd,'SPH0654',3);"));
      uiScript.print(F("addOption(dd,'Generic I2S with Mclk',4);"));
    #if  !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3)
      uiScript.print(F("addOption(dd,'Generic I2S PDM',5);"));
    #endif
    uiScript.print(F("addOption(dd,'ES8388',6);"));
    
      uiScript.print(F("dd=addDropdown(ux,'config:AGC');"));
      uiScript.print(F("addOption(dd,'Off',0);"));
      uiScript.print(F("addOption(dd,'Normal',1);"));
      uiScript.print(F("addOption(dd,'Vivid',2);"));
      uiScript.print(F("addOption(dd,'Lazy',3);"));

      uiScript.print(F("dd=addDropdown(ux,'dynamics:limiter');"));
      uiScript.print(F("addOption(dd,'Off',0);"));
      uiScript.print(F("addOption(dd,'On',1);"));
      uiScript.print(F("addInfo(ux+':dynamics:limiter',0,' On ');"));  // 0 is field type, 1 is actual field
      uiScript.print(F("addInfo(ux+':dynamics:rise',1,'ms <i>(&#x266A; effects only)</i>');"));
      uiScript.print(F("addInfo(ux+':dynamics:fall',1,'ms <i>(&#x266A; effects only)</i>');"));

      uiScript.print(F("dd=addDropdown(ux,'frequency:scale');"));
      uiScript.print(F("addOption(dd,'None',0);"));
      uiScript.print(F("addOption(dd,'Linear (Amplitude)',2);"));
      uiScript.print(F("addOption(dd,'Square Root (Energy)',3);"));
      uiScript.print(F("addOption(dd,'Logarithmic (Loudness)',1);"));
#endif

      uiScript.print(F("dd=addDropdown(ux,'sync:mode');"));
      uiScript.print(F("addOption(dd,'Off',0);"));
#ifdef ARDUINO_ARCH_ESP32
      uiScript.print(F("addOption(dd,'Send',1);"));
#endif
      uiScript.print(F("addOption(dd,'Receive',2);"));
#ifdef ARDUINO_ARCH_ESP32
      uiScript.print(F("addInfo(ux+':digitalmic:type',1,'<i>requires reboot!</i>');"));  // 0 is field type, 1 is actual field
      uiScript.print(F("addInfo(uxp,0,'<i>sd/data/dout</i>','I2S SD');"));
      uiScript.print(F("addInfo(uxp,1,'<i>ws/clk/lrck</i>','I2S WS');"));
      uiScript.print(F("addInfo(uxp,2,'<i>sck/bclk</i>','I2S SCK');"));
      #if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
        uiScript.print(F("addInfo(uxp,3,'<i>only use -1, 0, 1 or 3</i>','I2S MCLK');"));
      #else
        uiScript.print(F("addInfo(uxp,3,'<i>master clock</i>','I2S MCLK');"));
      #endif
#endif
    }


    /*
     * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
     * Commonly used for custom clocks (Cronixie, 7 segment)
     */
    //void handleOverlayDraw() override
    //{
      //strip.setPixelColor(0, RGBW32(0,0,0,0)) // set the first pixel to black
    //}

   
    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId() override
    {
      return USERMOD_ID_AUDIOREACTIVE;
    }
};

void AudioReactive::removeAudioPalettes(void) {
  DEBUG_PRINTLN(F("Removing audio palettes."));
  while (palettes>0) {
    strip.customPalettes.pop_back();
    DEBUG_PRINTLN(palettes);
    palettes--;
  }
  DEBUG_PRINT(F("Total # of palettes: ")); DEBUG_PRINTLN(strip.customPalettes.size());
}

void AudioReactive::createAudioPalettes(void) {
  DEBUG_PRINT(F("Total # of palettes: ")); DEBUG_PRINTLN(strip.customPalettes.size());
  if (palettes) return;
  DEBUG_PRINTLN(F("Adding audio palettes."));
  for (int i=0; i<MAX_PALETTES; i++)
    if (strip.customPalettes.size() < 10) {
      strip.customPalettes.push_back(CRGBPalette16(CRGB(BLACK)));
      palettes++;
      DEBUG_PRINTLN(palettes);
    } else break;
}

// credit @netmindz ar palette, adapted for usermod @blazoncek
CRGB AudioReactive::getCRGBForBand(int x, int pal) {
  CRGB value;
  CHSV hsv;
  int b;
  switch (pal) {
    case 2:
      b = map(x, 0, 255, 0, NUM_GEQ_CHANNELS/2); // convert palette position to lower half of freq band
      hsv = CHSV(fftResult[b], 255, x);
      hsv2rgb_rainbow(hsv, value);  // convert to R,G,B
      break;
    case 1:
      b = map(x, 1, 255, 0, 10); // convert palette position to lower half of freq band
      hsv = CHSV(fftResult[b], 255, map(fftResult[b], 0, 255, 30, 255));  // pick hue
      hsv2rgb_rainbow(hsv, value);  // convert to R,G,B
      break;
    default:
      if (x == 1) {
        value = CRGB(fftResult[10]/2, fftResult[4]/2, fftResult[0]/2);
      } else if(x == 255) {
        value = CRGB(fftResult[10]/2, fftResult[0]/2, fftResult[4]/2);
      } else {
        value = CRGB(fftResult[0]/2, fftResult[4]/2, fftResult[10]/2);
      }
      break;
  }
  return value;
}

void AudioReactive::fillAudioPalettes() {
  if (!palettes) return;
  size_t lastCustPalette = strip.customPalettes.size();
  if (int(lastCustPalette) >= palettes) lastCustPalette -= palettes;
  for (int pal=0; pal<palettes; pal++) {
    uint8_t tcp[16];  // Needs to be 4 times however many colors are being used.
                      // 3 colors = 12, 4 colors = 16, etc.

    tcp[0] = 0;  // anchor of first color - must be zero
    tcp[1] = 0;
    tcp[2] = 0;
    tcp[3] = 0;
    
    CRGB rgb = getCRGBForBand(1, pal);
    tcp[4] = 1;  // anchor of first color
    tcp[5] = rgb.r;
    tcp[6] = rgb.g;
    tcp[7] = rgb.b;
    
    rgb = getCRGBForBand(128, pal);
    tcp[8] = 128;
    tcp[9] = rgb.r;
    tcp[10] = rgb.g;
    tcp[11] = rgb.b;
    
    rgb = getCRGBForBand(255, pal);
    tcp[12] = 255;  // anchor of last color - must be 255
    tcp[13] = rgb.r;
    tcp[14] = rgb.g;
    tcp[15] = rgb.b;

    strip.customPalettes[lastCustPalette+pal].loadDynamicGradientPalette(tcp);
  }
}

// strings to reduce flash memory usage (used more than twice)
const char AudioReactive::_name[]       PROGMEM = "AudioReactive";
const char AudioReactive::_enabled[]    PROGMEM = "enabled";
const char AudioReactive::_config[]     PROGMEM = "config";
const char AudioReactive::_dynamics[]   PROGMEM = "dynamics";
const char AudioReactive::_frequency[]  PROGMEM = "frequency";
const char AudioReactive::_inputLvl[]   PROGMEM = "inputLevel";
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S3)
const char AudioReactive::_analogmic[]  PROGMEM = "analogmic";
#endif
const char AudioReactive::_digitalmic[] PROGMEM = "digitalmic";
const char AudioReactive::_addPalettes[]       PROGMEM = "add-palettes";
const char AudioReactive::UDP_SYNC_HEADER[]    PROGMEM = "00002"; // new sync header version, as format no longer compatible with previous structure
const char AudioReactive::UDP_SYNC_HEADER_v1[] PROGMEM = "00001"; // old sync header version - need to add backwards-compatibility feature

static AudioReactive ar_module;
REGISTER_USERMOD(ar_module);
