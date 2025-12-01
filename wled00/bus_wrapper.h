#pragma once
#ifndef BusWrapper_h
#define BusWrapper_h

//#define NPB_CONF_4STEP_CADENCE
#include "NeoPixelBus.h"
#ifdef ARDUINO_ARCH_ESP8266
#include <NeoEsp8266DmaMethodFix.h>
#endif

//Hardware SPI Pins
#define P_8266_HS_MOSI 13
#define P_8266_HS_CLK  14
#define P_32_HS_MOSI   13
#define P_32_HS_CLK    14
#define P_32_VS_MOSI   23
#define P_32_VS_CLK    18

//The dirty list of possible bus types. Quite a lot...
#define I_NONE 0
//ESP8266 RGB
#define I_8266_U0_NEO_3 1
#define I_8266_U1_NEO_3 2
#define I_8266_DM_NEO_3 3
#define I_8266_BB_NEO_3 4
//RGBW
#define I_8266_U0_NEO_4 5
#define I_8266_U1_NEO_4 6
#define I_8266_DM_NEO_4 7
#define I_8266_BB_NEO_4 8
//400Kbps
#define I_8266_U0_400_3 9
#define I_8266_U1_400_3 10
#define I_8266_DM_400_3 11
#define I_8266_BB_400_3 12
//TM1814 (RGBW)
#define I_8266_U0_TM1_4 13
#define I_8266_U1_TM1_4 14
#define I_8266_DM_TM1_4 15
#define I_8266_BB_TM1_4 16
//TM1829 (RGB)
#define I_8266_U0_TM2_3 17
#define I_8266_U1_TM2_3 18
#define I_8266_DM_TM2_3 19
#define I_8266_BB_TM2_3 20
//UCS8903 (RGB)
#define I_8266_U0_UCS_3 21
#define I_8266_U1_UCS_3 22
#define I_8266_DM_UCS_3 23
#define I_8266_BB_UCS_3 24
//UCS8904 (RGBW)
#define I_8266_U0_UCS_4 25
#define I_8266_U1_UCS_4 26
#define I_8266_DM_UCS_4 27
#define I_8266_BB_UCS_4 28
//FW1906 GRBCW
#define I_8266_U0_FW6_5 29
#define I_8266_U1_FW6_5 30
#define I_8266_DM_FW6_5 31
#define I_8266_BB_FW6_5 32
//ESP8266 APA106
#define I_8266_U0_APA106_3 33
#define I_8266_U1_APA106_3 34
#define I_8266_DM_APA106_3 35
#define I_8266_BB_APA106_3 36
//WS2805 (RGBCW)
#define I_8266_U0_2805_5 37
#define I_8266_U1_2805_5 38
#define I_8266_DM_2805_5 39
#define I_8266_BB_2805_5 40
//TM1914 (RGB)
#define I_8266_U0_TM1914_3 41
#define I_8266_U1_TM1914_3 42
#define I_8266_DM_TM1914_3 43
#define I_8266_BB_TM1914_3 44
//SM16825 (RGBCW)
#define I_8266_U0_SM16825_5 45
#define I_8266_U1_SM16825_5 46
#define I_8266_DM_SM16825_5 47
#define I_8266_BB_SM16825_5 48

/*** ESP32 Neopixel methods ***/
//RGB
#define I_32_RN_NEO_3 1
#define I_32_I2_NEO_3 2
//RGBW
#define I_32_RN_NEO_4 5
#define I_32_I2_NEO_4 6
//400Kbps
#define I_32_RN_400_3 9
#define I_32_I2_400_3 10
//TM1814 (RGBW)
#define I_32_RN_TM1_4 13
#define I_32_I2_TM1_4 14
//TM1829 (RGB)
#define I_32_RN_TM2_3 17
#define I_32_I2_TM2_3 18
//UCS8903 (RGB)
#define I_32_RN_UCS_3 21
#define I_32_I2_UCS_3 22
//UCS8904 (RGBW)
#define I_32_RN_UCS_4 25
#define I_32_I2_UCS_4 26
//FW1906 GRBCW
#define I_32_RN_FW6_5 29
#define I_32_I2_FW6_5 30
//APA106
#define I_32_RN_APA106_3 33
#define I_32_I2_APA106_3 34
//WS2805 (RGBCW)
#define I_32_RN_2805_5 37
#define I_32_I2_2805_5 38
//TM1914 (RGB)
#define I_32_RN_TM1914_3 41
#define I_32_I2_TM1914_3 42
//SM16825 (RGBCW)
#define I_32_RN_SM16825_5 45
#define I_32_I2_SM16825_5 46

//APA102
#define I_HS_DOT_3 101 //hardware SPI
#define I_SS_DOT_3 102 //soft SPI

//LPD8806
#define I_HS_LPD_3 103
#define I_SS_LPD_3 104

//WS2801
#define I_HS_WS1_3 105
#define I_SS_WS1_3 106

//P9813
#define I_HS_P98_3 107
#define I_SS_P98_3 108

//LPD6803
#define I_HS_LPO_3 109
#define I_SS_LPO_3 110


// In the following NeoGammaNullMethod can be replaced with NeoGammaWLEDMethod to perform Gamma correction implicitly
// unfortunately that may apply Gamma correction to pre-calculated palettes which is undesired

/*** ESP8266 Neopixel methods ***/
#ifdef ESP8266
//RGB
#define B_8266_U0_NEO_3 NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart0Ws2813Method> //3 chan, esp8266, gpio1
#define B_8266_U1_NEO_3 NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1Ws2813Method> //3 chan, esp8266, gpio2
#define B_8266_DM_NEO_3 NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma800KbpsMethod>  //3 chan, esp8266, gpio3
#define B_8266_BB_NEO_3 NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang800KbpsMethod> //3 chan, esp8266, bb (any pin but 16)
//RGBW
#define B_8266_U0_NEO_4 NeoPixelBus<NeoGrbwFeature, NeoEsp8266Uart0Ws2813Method>   //4 chan, esp8266, gpio1
#define B_8266_U1_NEO_4 NeoPixelBus<NeoGrbwFeature, NeoEsp8266Uart1Ws2813Method>   //4 chan, esp8266, gpio2
#define B_8266_DM_NEO_4 NeoPixelBus<NeoGrbwFeature, NeoEsp8266Dma800KbpsMethod>    //4 chan, esp8266, gpio3
#define B_8266_BB_NEO_4 NeoPixelBus<NeoGrbwFeature, NeoEsp8266BitBang800KbpsMethod> //4 chan, esp8266, bb (any pin)
//400Kbps
#define B_8266_U0_400_3 NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart0400KbpsMethod>   //3 chan, esp8266, gpio1
#define B_8266_U1_400_3 NeoPixelBus<NeoGrbFeature, NeoEsp8266Uart1400KbpsMethod>   //3 chan, esp8266, gpio2
#define B_8266_DM_400_3 NeoPixelBus<NeoGrbFeature, NeoEsp8266Dma400KbpsMethod>     //3 chan, esp8266, gpio3
#define B_8266_BB_400_3 NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBang400KbpsMethod> //3 chan, esp8266, bb (any pin)
//TM1814 (RGBW)
#define B_8266_U0_TM1_4 NeoPixelBus<NeoWrgbTm1814Feature, NeoEsp8266Uart0Tm1814Method>
#define B_8266_U1_TM1_4 NeoPixelBus<NeoWrgbTm1814Feature, NeoEsp8266Uart1Tm1814Method>
#define B_8266_DM_TM1_4 NeoPixelBus<NeoWrgbTm1814Feature, NeoEsp8266DmaTm1814Method>
#define B_8266_BB_TM1_4 NeoPixelBus<NeoWrgbTm1814Feature, NeoEsp8266BitBangTm1814Method>
//TM1829 (RGB)
#define B_8266_U0_TM2_3 NeoPixelBus<NeoBrgFeature, NeoEsp8266Uart0Tm1829Method>
#define B_8266_U1_TM2_3 NeoPixelBus<NeoBrgFeature, NeoEsp8266Uart1Tm1829Method>
#define B_8266_DM_TM2_3 NeoPixelBus<NeoBrgFeature, NeoEsp8266DmaTm1829Method>
#define B_8266_BB_TM2_3 NeoPixelBus<NeoBrgFeature, NeoEsp8266BitBangTm1829Method>
//UCS8903
#define B_8266_U0_UCS_3 NeoPixelBus<NeoRgbUcs8903Feature, NeoEsp8266Uart0Ws2813Method> //3 chan, esp8266, gpio1
#define B_8266_U1_UCS_3 NeoPixelBus<NeoRgbUcs8903Feature, NeoEsp8266Uart1Ws2813Method> //3 chan, esp8266, gpio2
#define B_8266_DM_UCS_3 NeoPixelBus<NeoRgbUcs8903Feature, NeoEsp8266Dma800KbpsMethod>  //3 chan, esp8266, gpio3
#define B_8266_BB_UCS_3 NeoPixelBus<NeoRgbUcs8903Feature, NeoEsp8266BitBang800KbpsMethod> //3 chan, esp8266, bb (any pin but 16)
//UCS8904 RGBW
#define B_8266_U0_UCS_4 NeoPixelBus<NeoRgbwUcs8904Feature, NeoEsp8266Uart0Ws2813Method>   //4 chan, esp8266, gpio1
#define B_8266_U1_UCS_4 NeoPixelBus<NeoRgbwUcs8904Feature, NeoEsp8266Uart1Ws2813Method>   //4 chan, esp8266, gpio2
#define B_8266_DM_UCS_4 NeoPixelBus<NeoRgbwUcs8904Feature, NeoEsp8266Dma800KbpsMethod>    //4 chan, esp8266, gpio3
#define B_8266_BB_UCS_4 NeoPixelBus<NeoRgbwUcs8904Feature, NeoEsp8266BitBang800KbpsMethod> //4 chan, esp8266, bb (any pin)
//APA106
#define B_8266_U0_APA106_3 NeoPixelBus<NeoRbgFeature, NeoEsp8266Uart0Apa106Method> //3 chan, esp8266, gpio1
#define B_8266_U1_APA106_3 NeoPixelBus<NeoRbgFeature, NeoEsp8266Uart1Apa106Method> //3 chan, esp8266, gpio2
#define B_8266_DM_APA106_3 NeoPixelBus<NeoGrbFeature, NeoEsp8266DmaApa106Method>  //3 chan, esp8266, gpio3
#define B_8266_BB_APA106_3 NeoPixelBus<NeoGrbFeature, NeoEsp8266BitBangApa106Method> //3 chan, esp8266, bb (any pin but 16)
//FW1906 GRBCW
#define B_8266_U0_FW6_5 NeoPixelBus<NeoGrbcwxFeature, NeoEsp8266Uart0Ws2813Method>   //esp8266, gpio1
#define B_8266_U1_FW6_5 NeoPixelBus<NeoGrbcwxFeature, NeoEsp8266Uart1Ws2813Method>   //esp8266, gpio2
#define B_8266_DM_FW6_5 NeoPixelBus<NeoGrbcwxFeature, NeoEsp8266Dma800KbpsMethod>   //esp8266, gpio3
#define B_8266_BB_FW6_5 NeoPixelBus<NeoGrbcwxFeature, NeoEsp8266BitBang800KbpsMethod>   //esp8266, bb
//WS2805 GRBCW
#define B_8266_U0_2805_5 NeoPixelBus<NeoGrbwwFeature, NeoEsp8266Uart0Ws2805Method> //esp8266, gpio1
#define B_8266_U1_2805_5 NeoPixelBus<NeoGrbwwFeature, NeoEsp8266Uart1Ws2805Method> //esp8266, gpio2
#define B_8266_DM_2805_5 NeoPixelBus<NeoGrbwwFeature, NeoEsp8266DmaWs2805Method> //esp8266, gpio3
#define B_8266_BB_2805_5 NeoPixelBus<NeoGrbwwFeature, NeoEsp8266BitBangWs2805Method> //esp8266, bb
//TM1914 (RGB)
#define B_8266_U0_TM1914_3 NeoPixelBus<NeoRgbTm1914Feature, NeoEsp8266Uart0Tm1914Method>
#define B_8266_U1_TM1914_3 NeoPixelBus<NeoRgbTm1914Feature, NeoEsp8266Uart1Tm1914Method>
#define B_8266_DM_TM1914_3 NeoPixelBus<NeoRgbTm1914Feature, NeoEsp8266DmaTm1914Method>
#define B_8266_BB_TM1914_3 NeoPixelBus<NeoRgbTm1914Feature, NeoEsp8266BitBangTm1914Method>
//Sm16825 (RGBWC)
#define B_8266_U0_SM16825_5 NeoPixelBus<NeoRgbwcSm16825eFeature, NeoEsp8266Uart0Ws2813Method>
#define B_8266_U1_SM16825_5 NeoPixelBus<NeoRgbwcSm16825eFeature, NeoEsp8266Uart1Ws2813Method>
#define B_8266_DM_SM16825_5 NeoPixelBus<NeoRgbwcSm16825eFeature, NeoEsp8266Dma800KbpsMethod>
#define B_8266_BB_SM16825_5 NeoPixelBus<NeoRgbwcSm16825eFeature, NeoEsp8266BitBangWs2813Method>
#endif

/*** ESP32 Neopixel methods ***/
#ifdef ARDUINO_ARCH_ESP32
// C3: I2S0 and I2S1 methods not supported (has one I2S bus)
// S2: I2S0 methods supported (single & parallel), I2S1 methods not supported (has one I2S bus)
// S3: I2S0 methods not supported, I2S1 supports LCD parallel methods (has two I2S buses)
// https://github.com/Makuna/NeoPixelBus/blob/b32f719e95ef3c35c46da5c99538017ef925c026/src/internal/Esp32_i2s.h#L4
// https://github.com/Makuna/NeoPixelBus/blob/b32f719e95ef3c35c46da5c99538017ef925c026/src/internal/NeoEsp32RmtMethod.h#L857
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // S3 will always use LCD parallel output
  typedef X8Ws2812xMethod X1Ws2812xMethod;
  typedef X8Sk6812Method X1Sk6812Method;
  typedef X8400KbpsMethod X1400KbpsMethod;
  typedef X8800KbpsMethod X1800KbpsMethod;
  typedef X8Tm1814Method X1Tm1814Method;
  typedef X8Tm1829Method X1Tm1829Method;
  typedef X8Apa106Method X1Apa106Method;
  typedef X8Ws2805Method X1Ws2805Method;
  typedef X8Tm1914Method X1Tm1914Method;
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  // S2 will use I2S0
  typedef NeoEsp32I2s0Ws2812xMethod X1Ws2812xMethod;
  typedef NeoEsp32I2s0Sk6812Method X1Sk6812Method;
  typedef NeoEsp32I2s0400KbpsMethod X1400KbpsMethod;
  typedef NeoEsp32I2s0800KbpsMethod X1800KbpsMethod;
  typedef NeoEsp32I2s0Tm1814Method X1Tm1814Method;
  typedef NeoEsp32I2s0Tm1829Method X1Tm1829Method;
  typedef NeoEsp32I2s0Apa106Method X1Apa106Method;
  typedef NeoEsp32I2s0Ws2805Method X1Ws2805Method;
  typedef NeoEsp32I2s0Tm1914Method X1Tm1914Method;
#elif !defined(CONFIG_IDF_TARGET_ESP32C3)
  // regular ESP32 will use I2S1
  typedef NeoEsp32I2s1Ws2812xMethod X1Ws2812xMethod;
  typedef NeoEsp32I2s1Sk6812Method X1Sk6812Method;
  typedef NeoEsp32I2s1400KbpsMethod X1400KbpsMethod;
  typedef NeoEsp32I2s1800KbpsMethod X1800KbpsMethod;
  typedef NeoEsp32I2s1Tm1814Method X1Tm1814Method;
  typedef NeoEsp32I2s1Tm1829Method X1Tm1829Method;
  typedef NeoEsp32I2s1Apa106Method X1Apa106Method;
  typedef NeoEsp32I2s1Ws2805Method X1Ws2805Method;
  typedef NeoEsp32I2s1Tm1914Method X1Tm1914Method;
#endif

// RMT driver selection
#if !defined(WLED_USE_SHARED_RMT)  && !defined(__riscv)
#include <NeoEsp32RmtHIMethod.h>
#define NeoEsp32RmtMethod(x) NeoEsp32RmtHIN ## x ## Method
#else
#define NeoEsp32RmtMethod(x) NeoEsp32RmtN ## x ## Method
#endif

//RGB
#define B_32_RN_NEO_3 NeoPixelBus<NeoGrbFeature, NeoEsp32RmtMethod(Ws2812x)> // ESP32, S2, S3, C3
//#define B_32_IN_NEO_3 NeoPixelBus<NeoGrbFeature, NeoEsp32I2sNWs2812xMethod> // ESP32 (dynamic I2S selection)
#define B_32_I2_NEO_3 NeoPixelBus<NeoGrbFeature, X1Ws2812xMethod> // ESP32, S2, S3 (automatic I2S selection, see typedef above)
#define B_32_IP_NEO_3 NeoPixelBus<NeoGrbFeature, X8Ws2812xMethod> // parallel I2S (ESP32, S2, S3)
//RGBW
#define B_32_RN_NEO_4 NeoPixelBus<NeoGrbwFeature, NeoEsp32RmtMethod(Sk6812)>
#define B_32_I2_NEO_4 NeoPixelBus<NeoGrbwFeature, X1Sk6812Method>
#define B_32_IP_NEO_4 NeoPixelBus<NeoGrbwFeature, X8Sk6812Method> // parallel I2S
//400Kbps
#define B_32_RN_400_3 NeoPixelBus<NeoGrbFeature, NeoEsp32RmtMethod(400Kbps)>
#define B_32_I2_400_3 NeoPixelBus<NeoGrbFeature, X1400KbpsMethod>
#define B_32_IP_400_3 NeoPixelBus<NeoGrbFeature, X8400KbpsMethod> // parallel I2S
//TM1814 (RGBW)
#define B_32_RN_TM1_4 NeoPixelBus<NeoWrgbTm1814Feature, NeoEsp32RmtMethod(Tm1814)>
#define B_32_I2_TM1_4 NeoPixelBus<NeoWrgbTm1814Feature, X1Tm1814Method>
#define B_32_IP_TM1_4 NeoPixelBus<NeoWrgbTm1814Feature, X8Tm1814Method> // parallel I2S
//TM1829 (RGB)
#define B_32_RN_TM2_3 NeoPixelBus<NeoBrgFeature, NeoEsp32RmtMethod(Tm1829)>
#define B_32_I2_TM2_3 NeoPixelBus<NeoBrgFeature, X1Tm1829Method>
#define B_32_IP_TM2_3 NeoPixelBus<NeoBrgFeature, X8Tm1829Method> // parallel I2S
//UCS8903
#define B_32_RN_UCS_3 NeoPixelBus<NeoRgbUcs8903Feature, NeoEsp32RmtMethod(Ws2812x)>
#define B_32_I2_UCS_3 NeoPixelBus<NeoRgbUcs8903Feature, X1800KbpsMethod>
#define B_32_IP_UCS_3 NeoPixelBus<NeoRgbUcs8903Feature, X8800KbpsMethod> // parallel I2S
//UCS8904
#define B_32_RN_UCS_4 NeoPixelBus<NeoRgbwUcs8904Feature, NeoEsp32RmtMethod(Ws2812x)>
#define B_32_I2_UCS_4 NeoPixelBus<NeoRgbwUcs8904Feature, X1800KbpsMethod>
#define B_32_IP_UCS_4 NeoPixelBus<NeoRgbwUcs8904Feature, X8800KbpsMethod>// parallel I2S
//APA106
#define B_32_RN_APA106_3 NeoPixelBus<NeoGrbFeature, NeoEsp32RmtMethod(Apa106)>
#define B_32_I2_APA106_3 NeoPixelBus<NeoGrbFeature, X1Apa106Method>
#define B_32_IP_APA106_3 NeoPixelBus<NeoGrbFeature, X8Apa106Method> // parallel I2S
//FW1906 GRBCW
#define B_32_RN_FW6_5 NeoPixelBus<NeoGrbcwxFeature, NeoEsp32RmtMethod(Ws2812x)>
#define B_32_I2_FW6_5 NeoPixelBus<NeoGrbcwxFeature, X1800KbpsMethod>
#define B_32_IP_FW6_5 NeoPixelBus<NeoGrbcwxFeature, X8800KbpsMethod> // parallel I2S
//WS2805 RGBWC
#define B_32_RN_2805_5 NeoPixelBus<NeoGrbwwFeature, NeoEsp32RmtMethod(Ws2805)>
#define B_32_I2_2805_5 NeoPixelBus<NeoGrbwwFeature, X1Ws2805Method>
#define B_32_IP_2805_5 NeoPixelBus<NeoGrbwwFeature, X8Ws2805Method> // parallel I2S
//TM1914 (RGB)
#define B_32_RN_TM1914_3 NeoPixelBus<NeoGrbTm1914Feature, NeoEsp32RmtMethod(Tm1914)>
#define B_32_I2_TM1914_3 NeoPixelBus<NeoGrbTm1914Feature, X1Tm1914Method>
#define B_32_IP_TM1914_3 NeoPixelBus<NeoGrbTm1914Feature, X8Tm1914Method> // parallel I2S
//Sm16825 (RGBWC)
#define B_32_RN_SM16825_5 NeoPixelBus<NeoRgbcwSm16825eFeature, NeoEsp32RmtMethod(Ws2812x)>
#define B_32_I2_SM16825_5 NeoPixelBus<NeoRgbcwSm16825eFeature, X1Ws2812xMethod>
#define B_32_IP_SM16825_5 NeoPixelBus<NeoRgbcwSm16825eFeature, X8Ws2812xMethod> // parallel I2S
#endif

//APA102
#ifdef WLED_USE_ETHERNET
// fix for #2542 (by @BlackBird77)
#define B_HS_DOT_3 NeoPixelBus<DotStarBgrFeature, DotStarEsp32HspiHzMethod> //hardware HSPI (was DotStarEsp32DmaHspi5MhzMethod in NPB @ 2.6.9)
#else
#define B_HS_DOT_3 NeoPixelBus<DotStarBgrFeature, DotStarSpiHzMethod> //hardware VSPI
#endif
#define B_SS_DOT_3 NeoPixelBus<DotStarBgrFeature, DotStarMethod>    //soft SPI

//LPD8806
#define B_HS_LPD_3 NeoPixelBus<Lpd8806GrbFeature, Lpd8806SpiHzMethod>
#define B_SS_LPD_3 NeoPixelBus<Lpd8806GrbFeature, Lpd8806Method>

//LPD6803
#define B_HS_LPO_3 NeoPixelBus<Lpd6803GrbFeature, Lpd6803SpiHzMethod>
#define B_SS_LPO_3 NeoPixelBus<Lpd6803GrbFeature, Lpd6803Method>

//WS2801
#ifdef WLED_USE_ETHERNET
#define B_HS_WS1_3 NeoPixelBus<NeoRbgFeature, Ws2801MethodBase<TwoWireHspiImple<SpiSpeedHz>>>
#else
#define B_HS_WS1_3 NeoPixelBus<NeoRbgFeature, Ws2801SpiHzMethod>
#endif
#define B_SS_WS1_3 NeoPixelBus<NeoRbgFeature, Ws2801Method>

//P9813
#define B_HS_P98_3 NeoPixelBus<P9813BgrFeature, P9813SpiHzMethod>
#define B_SS_P98_3 NeoPixelBus<P9813BgrFeature, P9813Method>

// 48bit & 64bit to 24bit & 32bit RGB(W) conversion
#define toRGBW32(c) (RGBW32((c>>40)&0xFF, (c>>24)&0xFF, (c>>8)&0xFF, (c>>56)&0xFF))
#define RGBW32(r,g,b,w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))

//handles pointer type conversion for all possible bus types
class PolyBus {
  private:
    static bool _useParallelI2S;

  public:
    static inline void setParallelI2S1Output(bool b = true) { _useParallelI2S = b; }
    static inline bool isParallelI2S1Output(void) { return _useParallelI2S; }

  // initialize SPI bus speed for DotStar methods
  template <class T>
  static void beginDotStar(void* busPtr, int8_t sck, int8_t miso, int8_t mosi, int8_t ss, uint16_t clock_kHz /* 0 == use default */) {
    T dotStar_strip = static_cast<T>(busPtr);
    #ifdef ESP8266
    dotStar_strip->Begin();
    #else
    if (sck == -1 && mosi == -1) dotStar_strip->Begin();
    else                         dotStar_strip->Begin(sck, miso, mosi, ss);
    #endif
    if (clock_kHz) dotStar_strip->SetMethodSettings(NeoSpiSettings((uint32_t)clock_kHz*1000));
  }

  // Begin & initialize the PixelSettings for TM1814 strips.
  template <class T>
  static void beginTM1814(void* busPtr) {
    T tm1814_strip = static_cast<T>(busPtr);
    tm1814_strip->Begin();
    // Max current for each LED (22.5 mA).
    tm1814_strip->SetPixelSettings(NeoTm1814Settings(/*R*/225, /*G*/225, /*B*/225, /*W*/225));
  }

  template <class T>
  static void beginTM1914(void* busPtr) {
    T tm1914_strip = static_cast<T>(busPtr);
    tm1914_strip->Begin();
    tm1914_strip->SetPixelSettings(NeoTm1914Settings());  //NeoTm1914_Mode_DinFdinAutoSwitch, NeoTm1914_Mode_DinOnly, NeoTm1914_Mode_FdinOnly 
  }

  static void begin(void* busPtr, uint8_t busType, uint8_t* pins, uint16_t clock_kHz /* only used by DotStar */) {
    switch (busType) {
      case I_NONE: break;
    #ifdef ESP8266
      case I_8266_U0_NEO_3: (static_cast<B_8266_U0_NEO_3*>(busPtr))->Begin(); break;
      case I_8266_U1_NEO_3: (static_cast<B_8266_U1_NEO_3*>(busPtr))->Begin(); break;
      case I_8266_DM_NEO_3: (static_cast<B_8266_DM_NEO_3*>(busPtr))->Begin(); break;
      case I_8266_BB_NEO_3: (static_cast<B_8266_BB_NEO_3*>(busPtr))->Begin(); break;
      case I_8266_U0_NEO_4: (static_cast<B_8266_U0_NEO_4*>(busPtr))->Begin(); break;
      case I_8266_U1_NEO_4: (static_cast<B_8266_U1_NEO_4*>(busPtr))->Begin(); break;
      case I_8266_DM_NEO_4: (static_cast<B_8266_DM_NEO_4*>(busPtr))->Begin(); break;
      case I_8266_BB_NEO_4: (static_cast<B_8266_BB_NEO_4*>(busPtr))->Begin(); break;
      case I_8266_U0_400_3: (static_cast<B_8266_U0_400_3*>(busPtr))->Begin(); break;
      case I_8266_U1_400_3: (static_cast<B_8266_U1_400_3*>(busPtr))->Begin(); break;
      case I_8266_DM_400_3: (static_cast<B_8266_DM_400_3*>(busPtr))->Begin(); break;
      case I_8266_BB_400_3: (static_cast<B_8266_BB_400_3*>(busPtr))->Begin(); break;
      case I_8266_U0_TM1_4: beginTM1814<B_8266_U0_TM1_4*>(busPtr); break;
      case I_8266_U1_TM1_4: beginTM1814<B_8266_U1_TM1_4*>(busPtr); break;
      case I_8266_DM_TM1_4: beginTM1814<B_8266_DM_TM1_4*>(busPtr); break;
      case I_8266_BB_TM1_4: beginTM1814<B_8266_BB_TM1_4*>(busPtr); break;
      case I_8266_U0_TM2_3: (static_cast<B_8266_U0_TM2_3*>(busPtr))->Begin(); break;
      case I_8266_U1_TM2_3: (static_cast<B_8266_U1_TM2_3*>(busPtr))->Begin(); break;
      case I_8266_DM_TM2_3: (static_cast<B_8266_DM_TM2_3*>(busPtr))->Begin(); break;
      case I_8266_BB_TM2_3: (static_cast<B_8266_BB_TM2_3*>(busPtr))->Begin(); break;
      case I_HS_DOT_3: beginDotStar<B_HS_DOT_3*>(busPtr, -1, -1, -1, -1, clock_kHz); break;
      case I_HS_LPD_3: beginDotStar<B_HS_LPD_3*>(busPtr, -1, -1, -1, -1, clock_kHz); break;
      case I_HS_LPO_3: beginDotStar<B_HS_LPO_3*>(busPtr, -1, -1, -1, -1, clock_kHz); break;
      case I_HS_WS1_3: beginDotStar<B_HS_WS1_3*>(busPtr, -1, -1, -1, -1, clock_kHz); break;
      case I_HS_P98_3: beginDotStar<B_HS_P98_3*>(busPtr, -1, -1, -1, -1, clock_kHz); break;
      case I_8266_U0_UCS_3: (static_cast<B_8266_U0_UCS_3*>(busPtr))->Begin(); break;
      case I_8266_U1_UCS_3: (static_cast<B_8266_U1_UCS_3*>(busPtr))->Begin(); break;
      case I_8266_DM_UCS_3: (static_cast<B_8266_DM_UCS_3*>(busPtr))->Begin(); break;
      case I_8266_BB_UCS_3: (static_cast<B_8266_BB_UCS_3*>(busPtr))->Begin(); break;
      case I_8266_U0_UCS_4: (static_cast<B_8266_U0_UCS_4*>(busPtr))->Begin(); break;
      case I_8266_U1_UCS_4: (static_cast<B_8266_U1_UCS_4*>(busPtr))->Begin(); break;
      case I_8266_DM_UCS_4: (static_cast<B_8266_DM_UCS_4*>(busPtr))->Begin(); break;
      case I_8266_BB_UCS_4: (static_cast<B_8266_BB_UCS_4*>(busPtr))->Begin(); break;
      case I_8266_U0_APA106_3: (static_cast<B_8266_U0_APA106_3*>(busPtr))->Begin(); break;
      case I_8266_U1_APA106_3: (static_cast<B_8266_U1_APA106_3*>(busPtr))->Begin(); break;
      case I_8266_DM_APA106_3: (static_cast<B_8266_DM_APA106_3*>(busPtr))->Begin(); break;
      case I_8266_BB_APA106_3: (static_cast<B_8266_BB_APA106_3*>(busPtr))->Begin(); break;
      case I_8266_U0_FW6_5: (static_cast<B_8266_U0_FW6_5*>(busPtr))->Begin(); break;
      case I_8266_U1_FW6_5: (static_cast<B_8266_U1_FW6_5*>(busPtr))->Begin(); break;
      case I_8266_DM_FW6_5: (static_cast<B_8266_DM_FW6_5*>(busPtr))->Begin(); break;
      case I_8266_BB_FW6_5: (static_cast<B_8266_BB_FW6_5*>(busPtr))->Begin(); break;
      case I_8266_U0_2805_5: (static_cast<B_8266_U0_2805_5*>(busPtr))->Begin(); break;
      case I_8266_U1_2805_5: (static_cast<B_8266_U1_2805_5*>(busPtr))->Begin(); break;
      case I_8266_DM_2805_5: (static_cast<B_8266_DM_2805_5*>(busPtr))->Begin(); break;
      case I_8266_BB_2805_5: (static_cast<B_8266_BB_2805_5*>(busPtr))->Begin(); break;
      case I_8266_U0_TM1914_3: beginTM1914<B_8266_U0_TM1914_3*>(busPtr); break;
      case I_8266_U1_TM1914_3: beginTM1914<B_8266_U1_TM1914_3*>(busPtr); break;
      case I_8266_DM_TM1914_3: beginTM1914<B_8266_DM_TM1914_3*>(busPtr); break;
      case I_8266_BB_TM1914_3: beginTM1914<B_8266_BB_TM1914_3*>(busPtr); break;
      case I_8266_U0_SM16825_5: (static_cast<B_8266_U0_SM16825_5*>(busPtr))->Begin(); break;
      case I_8266_U1_SM16825_5: (static_cast<B_8266_U1_SM16825_5*>(busPtr))->Begin(); break;
      case I_8266_DM_SM16825_5: (static_cast<B_8266_DM_SM16825_5*>(busPtr))->Begin(); break;
      case I_8266_BB_SM16825_5: (static_cast<B_8266_BB_SM16825_5*>(busPtr))->Begin(); break;
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      // RMT buses
      case I_32_RN_NEO_3: (static_cast<B_32_RN_NEO_3*>(busPtr))->Begin(); break;
      case I_32_RN_NEO_4: (static_cast<B_32_RN_NEO_4*>(busPtr))->Begin(); break;
      case I_32_RN_400_3: (static_cast<B_32_RN_400_3*>(busPtr))->Begin(); break;
      case I_32_RN_TM1_4: beginTM1814<B_32_RN_TM1_4*>(busPtr); break;
      case I_32_RN_TM2_3: (static_cast<B_32_RN_TM2_3*>(busPtr))->Begin(); break;
      case I_32_RN_UCS_3: (static_cast<B_32_RN_UCS_3*>(busPtr))->Begin(); break;
      case I_32_RN_UCS_4: (static_cast<B_32_RN_UCS_4*>(busPtr))->Begin(); break;
      case I_32_RN_FW6_5: (static_cast<B_32_RN_FW6_5*>(busPtr))->Begin(); break;
      case I_32_RN_APA106_3: (static_cast<B_32_RN_APA106_3*>(busPtr))->Begin(); break;
      case I_32_RN_2805_5: (static_cast<B_32_RN_2805_5*>(busPtr))->Begin(); break;
      case I_32_RN_TM1914_3: beginTM1914<B_32_RN_TM1914_3*>(busPtr); break;
      case I_32_RN_SM16825_5: (static_cast<B_32_RN_SM16825_5*>(busPtr))->Begin(); break;
      // I2S1 bus or parellel buses
      #ifndef CONFIG_IDF_TARGET_ESP32C3
      case I_32_I2_NEO_3: if (_useParallelI2S) (static_cast<B_32_IP_NEO_3*>(busPtr))->Begin(); else (static_cast<B_32_I2_NEO_3*>(busPtr))->Begin(); break;
      case I_32_I2_NEO_4: if (_useParallelI2S) (static_cast<B_32_IP_NEO_4*>(busPtr))->Begin(); else (static_cast<B_32_I2_NEO_4*>(busPtr))->Begin(); break;
      case I_32_I2_400_3: if (_useParallelI2S) (static_cast<B_32_IP_400_3*>(busPtr))->Begin(); else (static_cast<B_32_I2_400_3*>(busPtr))->Begin(); break;
      case I_32_I2_TM1_4: if (_useParallelI2S) beginTM1814<B_32_IP_TM1_4*>(busPtr); else beginTM1814<B_32_I2_TM1_4*>(busPtr); break;
      case I_32_I2_TM2_3: if (_useParallelI2S) (static_cast<B_32_IP_TM2_3*>(busPtr))->Begin(); else (static_cast<B_32_I2_TM2_3*>(busPtr))->Begin(); break;
      case I_32_I2_UCS_3: if (_useParallelI2S) (static_cast<B_32_IP_UCS_3*>(busPtr))->Begin(); else (static_cast<B_32_I2_UCS_3*>(busPtr))->Begin(); break;
      case I_32_I2_UCS_4: if (_useParallelI2S) (static_cast<B_32_IP_UCS_4*>(busPtr))->Begin(); else (static_cast<B_32_I2_UCS_4*>(busPtr))->Begin(); break;
      case I_32_I2_FW6_5: if (_useParallelI2S) (static_cast<B_32_IP_FW6_5*>(busPtr))->Begin(); else (static_cast<B_32_I2_FW6_5*>(busPtr))->Begin(); break;
      case I_32_I2_APA106_3: if (_useParallelI2S) (static_cast<B_32_IP_APA106_3*>(busPtr))->Begin(); else (static_cast<B_32_I2_APA106_3*>(busPtr))->Begin(); break;
      case I_32_I2_2805_5: if (_useParallelI2S) (static_cast<B_32_IP_2805_5*>(busPtr))->Begin(); else (static_cast<B_32_I2_2805_5*>(busPtr))->Begin(); break;
      case I_32_I2_TM1914_3: if (_useParallelI2S) beginTM1914<B_32_IP_TM1914_3*>(busPtr); else beginTM1914<B_32_I2_TM1914_3*>(busPtr); break;
      case I_32_I2_SM16825_5: if (_useParallelI2S) (static_cast<B_32_IP_SM16825_5*>(busPtr))->Begin(); else (static_cast<B_32_I2_SM16825_5*>(busPtr))->Begin(); break;
      #endif
      // ESP32 can (and should, to avoid inadvertantly driving the chip select signal) specify the pins used for SPI, but only in begin()
      case I_HS_DOT_3: beginDotStar<B_HS_DOT_3*>(busPtr, pins[1], -1, pins[0], -1, clock_kHz); break;
      case I_HS_LPD_3: beginDotStar<B_HS_LPD_3*>(busPtr, pins[1], -1, pins[0], -1, clock_kHz); break;
      case I_HS_LPO_3: beginDotStar<B_HS_LPO_3*>(busPtr, pins[1], -1, pins[0], -1, clock_kHz); break;
      case I_HS_WS1_3: beginDotStar<B_HS_WS1_3*>(busPtr, pins[1], -1, pins[0], -1, clock_kHz); break;
      case I_HS_P98_3: beginDotStar<B_HS_P98_3*>(busPtr, pins[1], -1, pins[0], -1, clock_kHz); break;
    #endif
      case I_SS_DOT_3: (static_cast<B_SS_DOT_3*>(busPtr))->Begin(); break;
      case I_SS_LPD_3: (static_cast<B_SS_LPD_3*>(busPtr))->Begin(); break;
      case I_SS_LPO_3: (static_cast<B_SS_LPO_3*>(busPtr))->Begin(); break;
      case I_SS_WS1_3: (static_cast<B_SS_WS1_3*>(busPtr))->Begin(); break;
      case I_SS_P98_3: (static_cast<B_SS_P98_3*>(busPtr))->Begin(); break;
    }
  }

  static void* create(uint8_t busType, uint8_t* pins, uint16_t len, uint8_t channel) {
    // NOTE: "channel" is only used on ESP32 (and its variants) for RMT channel allocation

    #if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
    if (_useParallelI2S && (channel >= 8)) {
        // Parallel I2S channels are to be used first, so subtract 8 to get the RMT channel number
        channel -= 8;
    }
    #endif

    #if defined(ARDUINO_ARCH_ESP32) && !(defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3))
    // since 0.15.0-b3 I2S1 is favoured for classic ESP32 and moved to position 0 (channel 0) so we need to subtract 1 for correct RMT allocation
    if (!_useParallelI2S && channel > 0) channel--; // accommodate I2S1 which is used as 1st bus on classic ESP32
    #endif

    void* busPtr = nullptr;
    switch (busType) {
      case I_NONE: break;
    #ifdef ESP8266
      case I_8266_U0_NEO_3: busPtr = new B_8266_U0_NEO_3(len, pins[0]); break;
      case I_8266_U1_NEO_3: busPtr = new B_8266_U1_NEO_3(len, pins[0]); break;
      case I_8266_DM_NEO_3: busPtr = new B_8266_DM_NEO_3(len, pins[0]); break;
      case I_8266_BB_NEO_3: busPtr = new B_8266_BB_NEO_3(len, pins[0]); break;
      case I_8266_U0_NEO_4: busPtr = new B_8266_U0_NEO_4(len, pins[0]); break;
      case I_8266_U1_NEO_4: busPtr = new B_8266_U1_NEO_4(len, pins[0]); break;
      case I_8266_DM_NEO_4: busPtr = new B_8266_DM_NEO_4(len, pins[0]); break;
      case I_8266_BB_NEO_4: busPtr = new B_8266_BB_NEO_4(len, pins[0]); break;
      case I_8266_U0_400_3: busPtr = new B_8266_U0_400_3(len, pins[0]); break;
      case I_8266_U1_400_3: busPtr = new B_8266_U1_400_3(len, pins[0]); break;
      case I_8266_DM_400_3: busPtr = new B_8266_DM_400_3(len, pins[0]); break;
      case I_8266_BB_400_3: busPtr = new B_8266_BB_400_3(len, pins[0]); break;
      case I_8266_U0_TM1_4: busPtr = new B_8266_U0_TM1_4(len, pins[0]); break;
      case I_8266_U1_TM1_4: busPtr = new B_8266_U1_TM1_4(len, pins[0]); break;
      case I_8266_DM_TM1_4: busPtr = new B_8266_DM_TM1_4(len, pins[0]); break;
      case I_8266_BB_TM1_4: busPtr = new B_8266_BB_TM1_4(len, pins[0]); break;
      case I_8266_U0_TM2_3: busPtr = new B_8266_U0_TM2_3(len, pins[0]); break;
      case I_8266_U1_TM2_3: busPtr = new B_8266_U1_TM2_3(len, pins[0]); break;
      case I_8266_DM_TM2_3: busPtr = new B_8266_DM_TM2_3(len, pins[0]); break;
      case I_8266_BB_TM2_3: busPtr = new B_8266_BB_TM2_3(len, pins[0]); break;
      case I_8266_U0_UCS_3: busPtr = new B_8266_U0_UCS_3(len, pins[0]); break;
      case I_8266_U1_UCS_3: busPtr = new B_8266_U1_UCS_3(len, pins[0]); break;
      case I_8266_DM_UCS_3: busPtr = new B_8266_DM_UCS_3(len, pins[0]); break;
      case I_8266_BB_UCS_3: busPtr = new B_8266_BB_UCS_3(len, pins[0]); break;
      case I_8266_U0_UCS_4: busPtr = new B_8266_U0_UCS_4(len, pins[0]); break;
      case I_8266_U1_UCS_4: busPtr = new B_8266_U1_UCS_4(len, pins[0]); break;
      case I_8266_DM_UCS_4: busPtr = new B_8266_DM_UCS_4(len, pins[0]); break;
      case I_8266_BB_UCS_4: busPtr = new B_8266_BB_UCS_4(len, pins[0]); break;
      case I_8266_U0_APA106_3: busPtr = new B_8266_U0_APA106_3(len, pins[0]); break;
      case I_8266_U1_APA106_3: busPtr = new B_8266_U1_APA106_3(len, pins[0]); break;
      case I_8266_DM_APA106_3: busPtr = new B_8266_DM_APA106_3(len, pins[0]); break;
      case I_8266_BB_APA106_3: busPtr = new B_8266_BB_APA106_3(len, pins[0]); break;
      case I_8266_U0_FW6_5: busPtr = new B_8266_U0_FW6_5(len, pins[0]); break;
      case I_8266_U1_FW6_5: busPtr = new B_8266_U1_FW6_5(len, pins[0]); break;
      case I_8266_DM_FW6_5: busPtr = new B_8266_DM_FW6_5(len, pins[0]); break;
      case I_8266_BB_FW6_5: busPtr = new B_8266_BB_FW6_5(len, pins[0]); break;
      case I_8266_U0_2805_5: busPtr = new B_8266_U0_2805_5(len, pins[0]); break;
      case I_8266_U1_2805_5: busPtr = new B_8266_U1_2805_5(len, pins[0]); break;
      case I_8266_DM_2805_5: busPtr = new B_8266_DM_2805_5(len, pins[0]); break;
      case I_8266_BB_2805_5: busPtr = new B_8266_BB_2805_5(len, pins[0]); break;
      case I_8266_U0_TM1914_3: busPtr = new B_8266_U0_TM1914_3(len, pins[0]); break;
      case I_8266_U1_TM1914_3: busPtr = new B_8266_U1_TM1914_3(len, pins[0]); break;
      case I_8266_DM_TM1914_3: busPtr = new B_8266_DM_TM1914_3(len, pins[0]); break;
      case I_8266_BB_TM1914_3: busPtr = new B_8266_BB_TM1914_3(len, pins[0]); break;
      case I_8266_U0_SM16825_5: busPtr = new B_8266_U0_SM16825_5(len, pins[0]); break;
      case I_8266_U1_SM16825_5: busPtr = new B_8266_U1_SM16825_5(len, pins[0]); break;
      case I_8266_DM_SM16825_5: busPtr = new B_8266_DM_SM16825_5(len, pins[0]); break;
      case I_8266_BB_SM16825_5: busPtr = new B_8266_BB_SM16825_5(len, pins[0]); break;
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      // RMT buses
      case I_32_RN_NEO_3: busPtr = new B_32_RN_NEO_3(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_NEO_4: busPtr = new B_32_RN_NEO_4(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_400_3: busPtr = new B_32_RN_400_3(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_TM1_4: busPtr = new B_32_RN_TM1_4(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_TM2_3: busPtr = new B_32_RN_TM2_3(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_UCS_3: busPtr = new B_32_RN_UCS_3(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_UCS_4: busPtr = new B_32_RN_UCS_4(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_APA106_3: busPtr = new B_32_RN_APA106_3(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_FW6_5: busPtr = new B_32_RN_FW6_5(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_2805_5: busPtr = new B_32_RN_2805_5(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_TM1914_3: busPtr = new B_32_RN_TM1914_3(len, pins[0], (NeoBusChannel)channel); break;
      case I_32_RN_SM16825_5: busPtr = new B_32_RN_SM16825_5(len, pins[0], (NeoBusChannel)channel); break;
      // I2S1 bus or paralell buses
      #ifndef CONFIG_IDF_TARGET_ESP32C3
      case I_32_I2_NEO_3: if (_useParallelI2S) busPtr = new B_32_IP_NEO_3(len, pins[0]); else busPtr = new B_32_I2_NEO_3(len, pins[0]); break;
      case I_32_I2_NEO_4: if (_useParallelI2S) busPtr = new B_32_IP_NEO_4(len, pins[0]); else busPtr = new B_32_I2_NEO_4(len, pins[0]); break;
      case I_32_I2_400_3: if (_useParallelI2S) busPtr = new B_32_IP_400_3(len, pins[0]); else busPtr = new B_32_I2_400_3(len, pins[0]); break;
      case I_32_I2_TM1_4: if (_useParallelI2S) busPtr = new B_32_IP_TM1_4(len, pins[0]); else busPtr = new B_32_I2_TM1_4(len, pins[0]); break;
      case I_32_I2_TM2_3: if (_useParallelI2S) busPtr = new B_32_IP_TM2_3(len, pins[0]); else busPtr = new B_32_I2_TM2_3(len, pins[0]); break;
      case I_32_I2_UCS_3: if (_useParallelI2S) busPtr = new B_32_IP_UCS_3(len, pins[0]); else busPtr = new B_32_I2_UCS_3(len, pins[0]); break;
      case I_32_I2_UCS_4: if (_useParallelI2S) busPtr = new B_32_IP_UCS_4(len, pins[0]); else busPtr = new B_32_I2_UCS_4(len, pins[0]); break;
      case I_32_I2_APA106_3: if (_useParallelI2S) busPtr = new B_32_IP_APA106_3(len, pins[0]); else busPtr = new B_32_I2_APA106_3(len, pins[0]); break;
      case I_32_I2_FW6_5: if (_useParallelI2S) busPtr = new B_32_IP_FW6_5(len, pins[0]); else busPtr = new B_32_I2_FW6_5(len, pins[0]); break;
      case I_32_I2_2805_5: if (_useParallelI2S) busPtr = new B_32_IP_2805_5(len, pins[0]); else busPtr = new B_32_I2_2805_5(len, pins[0]); break;
      case I_32_I2_TM1914_3: if (_useParallelI2S) busPtr = new B_32_IP_TM1914_3(len, pins[0]); else busPtr = new B_32_I2_TM1914_3(len, pins[0]); break;
      case I_32_I2_SM16825_5: if (_useParallelI2S) busPtr = new B_32_IP_SM16825_5(len, pins[0]); else busPtr = new B_32_I2_SM16825_5(len, pins[0]); break;
      #endif
    #endif
      // for 2-wire: pins[1] is clk, pins[0] is dat.  begin expects (len, clk, dat)
      case I_HS_DOT_3: busPtr = new B_HS_DOT_3(len, pins[1], pins[0]); break;
      case I_SS_DOT_3: busPtr = new B_SS_DOT_3(len, pins[1], pins[0]); break;
      case I_HS_LPD_3: busPtr = new B_HS_LPD_3(len, pins[1], pins[0]); break;
      case I_SS_LPD_3: busPtr = new B_SS_LPD_3(len, pins[1], pins[0]); break;
      case I_HS_LPO_3: busPtr = new B_HS_LPO_3(len, pins[1], pins[0]); break;
      case I_SS_LPO_3: busPtr = new B_SS_LPO_3(len, pins[1], pins[0]); break;
      case I_HS_WS1_3: busPtr = new B_HS_WS1_3(len, pins[1], pins[0]); break;
      case I_SS_WS1_3: busPtr = new B_SS_WS1_3(len, pins[1], pins[0]); break;
      case I_HS_P98_3: busPtr = new B_HS_P98_3(len, pins[1], pins[0]); break;
      case I_SS_P98_3: busPtr = new B_SS_P98_3(len, pins[1], pins[0]); break;
    }

    return busPtr;
  }

  static void show(void* busPtr, uint8_t busType, bool consistent = true) {
    switch (busType) {
      case I_NONE: break;
    #ifdef ESP8266
      case I_8266_U0_NEO_3: (static_cast<B_8266_U0_NEO_3*>(busPtr))->Show(consistent); break;
      case I_8266_U1_NEO_3: (static_cast<B_8266_U1_NEO_3*>(busPtr))->Show(consistent); break;
      case I_8266_DM_NEO_3: (static_cast<B_8266_DM_NEO_3*>(busPtr))->Show(consistent); break;
      case I_8266_BB_NEO_3: (static_cast<B_8266_BB_NEO_3*>(busPtr))->Show(consistent); break;
      case I_8266_U0_NEO_4: (static_cast<B_8266_U0_NEO_4*>(busPtr))->Show(consistent); break;
      case I_8266_U1_NEO_4: (static_cast<B_8266_U1_NEO_4*>(busPtr))->Show(consistent); break;
      case I_8266_DM_NEO_4: (static_cast<B_8266_DM_NEO_4*>(busPtr))->Show(consistent); break;
      case I_8266_BB_NEO_4: (static_cast<B_8266_BB_NEO_4*>(busPtr))->Show(consistent); break;
      case I_8266_U0_400_3: (static_cast<B_8266_U0_400_3*>(busPtr))->Show(consistent); break;
      case I_8266_U1_400_3: (static_cast<B_8266_U1_400_3*>(busPtr))->Show(consistent); break;
      case I_8266_DM_400_3: (static_cast<B_8266_DM_400_3*>(busPtr))->Show(consistent); break;
      case I_8266_BB_400_3: (static_cast<B_8266_BB_400_3*>(busPtr))->Show(consistent); break;
      case I_8266_U0_TM1_4: (static_cast<B_8266_U0_TM1_4*>(busPtr))->Show(consistent); break;
      case I_8266_U1_TM1_4: (static_cast<B_8266_U1_TM1_4*>(busPtr))->Show(consistent); break;
      case I_8266_DM_TM1_4: (static_cast<B_8266_DM_TM1_4*>(busPtr))->Show(consistent); break;
      case I_8266_BB_TM1_4: (static_cast<B_8266_BB_TM1_4*>(busPtr))->Show(consistent); break;
      case I_8266_U0_TM2_3: (static_cast<B_8266_U0_TM2_3*>(busPtr))->Show(consistent); break;
      case I_8266_U1_TM2_3: (static_cast<B_8266_U1_TM2_3*>(busPtr))->Show(consistent); break;
      case I_8266_DM_TM2_3: (static_cast<B_8266_DM_TM2_3*>(busPtr))->Show(consistent); break;
      case I_8266_BB_TM2_3: (static_cast<B_8266_BB_TM2_3*>(busPtr))->Show(consistent); break;
      case I_8266_U0_UCS_3: (static_cast<B_8266_U0_UCS_3*>(busPtr))->Show(consistent); break;
      case I_8266_U1_UCS_3: (static_cast<B_8266_U1_UCS_3*>(busPtr))->Show(consistent); break;
      case I_8266_DM_UCS_3: (static_cast<B_8266_DM_UCS_3*>(busPtr))->Show(consistent); break;
      case I_8266_BB_UCS_3: (static_cast<B_8266_BB_UCS_3*>(busPtr))->Show(consistent); break;
      case I_8266_U0_UCS_4: (static_cast<B_8266_U0_UCS_4*>(busPtr))->Show(consistent); break;
      case I_8266_U1_UCS_4: (static_cast<B_8266_U1_UCS_4*>(busPtr))->Show(consistent); break;
      case I_8266_DM_UCS_4: (static_cast<B_8266_DM_UCS_4*>(busPtr))->Show(consistent); break;
      case I_8266_BB_UCS_4: (static_cast<B_8266_BB_UCS_4*>(busPtr))->Show(consistent); break;
      case I_8266_U0_APA106_3: (static_cast<B_8266_U0_APA106_3*>(busPtr))->Show(consistent); break;
      case I_8266_U1_APA106_3: (static_cast<B_8266_U1_APA106_3*>(busPtr))->Show(consistent); break;
      case I_8266_DM_APA106_3: (static_cast<B_8266_DM_APA106_3*>(busPtr))->Show(consistent); break;
      case I_8266_BB_APA106_3: (static_cast<B_8266_BB_APA106_3*>(busPtr))->Show(consistent); break;
      case I_8266_U0_FW6_5: (static_cast<B_8266_U0_FW6_5*>(busPtr))->Show(consistent); break;
      case I_8266_U1_FW6_5: (static_cast<B_8266_U1_FW6_5*>(busPtr))->Show(consistent); break;
      case I_8266_DM_FW6_5: (static_cast<B_8266_DM_FW6_5*>(busPtr))->Show(consistent); break;
      case I_8266_BB_FW6_5: (static_cast<B_8266_BB_FW6_5*>(busPtr))->Show(consistent); break;
      case I_8266_U0_2805_5: (static_cast<B_8266_U0_2805_5*>(busPtr))->Show(consistent); break;
      case I_8266_U1_2805_5: (static_cast<B_8266_U1_2805_5*>(busPtr))->Show(consistent); break;
      case I_8266_DM_2805_5: (static_cast<B_8266_DM_2805_5*>(busPtr))->Show(consistent); break;
      case I_8266_BB_2805_5: (static_cast<B_8266_BB_2805_5*>(busPtr))->Show(consistent); break;
      case I_8266_U0_TM1914_3: (static_cast<B_8266_U0_TM1914_3*>(busPtr))->Show(consistent); break;
      case I_8266_U1_TM1914_3: (static_cast<B_8266_U1_TM1914_3*>(busPtr))->Show(consistent); break;
      case I_8266_DM_TM1914_3: (static_cast<B_8266_DM_TM1914_3*>(busPtr))->Show(consistent); break;
      case I_8266_BB_TM1914_3: (static_cast<B_8266_BB_TM1914_3*>(busPtr))->Show(consistent); break;
      case I_8266_U0_SM16825_5: (static_cast<B_8266_U0_SM16825_5*>(busPtr))->Show(consistent); break;
      case I_8266_U1_SM16825_5: (static_cast<B_8266_U1_SM16825_5*>(busPtr))->Show(consistent); break;
      case I_8266_DM_SM16825_5: (static_cast<B_8266_DM_SM16825_5*>(busPtr))->Show(consistent); break;
      case I_8266_BB_SM16825_5: (static_cast<B_8266_BB_SM16825_5*>(busPtr))->Show(consistent); break;
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      // RMT buses
      case I_32_RN_NEO_3: (static_cast<B_32_RN_NEO_3*>(busPtr))->Show(consistent); break;
      case I_32_RN_NEO_4: (static_cast<B_32_RN_NEO_4*>(busPtr))->Show(consistent); break;
      case I_32_RN_400_3: (static_cast<B_32_RN_400_3*>(busPtr))->Show(consistent); break;
      case I_32_RN_TM1_4: (static_cast<B_32_RN_TM1_4*>(busPtr))->Show(consistent); break;
      case I_32_RN_TM2_3: (static_cast<B_32_RN_TM2_3*>(busPtr))->Show(consistent); break;
      case I_32_RN_UCS_3: (static_cast<B_32_RN_UCS_3*>(busPtr))->Show(consistent); break;
      case I_32_RN_UCS_4: (static_cast<B_32_RN_UCS_4*>(busPtr))->Show(consistent); break;
      case I_32_RN_APA106_3: (static_cast<B_32_RN_APA106_3*>(busPtr))->Show(consistent); break;
      case I_32_RN_FW6_5: (static_cast<B_32_RN_FW6_5*>(busPtr))->Show(consistent); break;
      case I_32_RN_2805_5: (static_cast<B_32_RN_2805_5*>(busPtr))->Show(consistent); break;
      case I_32_RN_TM1914_3: (static_cast<B_32_RN_TM1914_3*>(busPtr))->Show(consistent); break;
      case I_32_RN_SM16825_5: (static_cast<B_32_RN_SM16825_5*>(busPtr))->Show(consistent); break;
      // I2S1 bus or paralell buses
      #ifndef CONFIG_IDF_TARGET_ESP32C3
      case I_32_I2_NEO_3: if (_useParallelI2S) (static_cast<B_32_IP_NEO_3*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_NEO_3*>(busPtr))->Show(consistent); break;
      case I_32_I2_NEO_4: if (_useParallelI2S) (static_cast<B_32_IP_NEO_4*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_NEO_4*>(busPtr))->Show(consistent); break;
      case I_32_I2_400_3: if (_useParallelI2S) (static_cast<B_32_IP_400_3*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_400_3*>(busPtr))->Show(consistent); break;
      case I_32_I2_TM1_4: if (_useParallelI2S) (static_cast<B_32_IP_TM1_4*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_TM1_4*>(busPtr))->Show(consistent); break;
      case I_32_I2_TM2_3: if (_useParallelI2S) (static_cast<B_32_IP_TM2_3*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_TM2_3*>(busPtr))->Show(consistent); break;
      case I_32_I2_UCS_3: if (_useParallelI2S) (static_cast<B_32_IP_UCS_3*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_UCS_3*>(busPtr))->Show(consistent); break;
      case I_32_I2_UCS_4: if (_useParallelI2S) (static_cast<B_32_IP_UCS_4*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_UCS_4*>(busPtr))->Show(consistent); break;
      case I_32_I2_APA106_3: if (_useParallelI2S) (static_cast<B_32_IP_APA106_3*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_APA106_3*>(busPtr))->Show(consistent); break;
      case I_32_I2_FW6_5: if (_useParallelI2S) (static_cast<B_32_IP_FW6_5*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_FW6_5*>(busPtr))->Show(consistent); break;
      case I_32_I2_2805_5: if (_useParallelI2S) (static_cast<B_32_IP_2805_5*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_2805_5*>(busPtr))->Show(consistent); break;
      case I_32_I2_TM1914_3: if (_useParallelI2S) (static_cast<B_32_IP_TM1914_3*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_TM1914_3*>(busPtr))->Show(consistent); break;
      case I_32_I2_SM16825_5: if (_useParallelI2S) (static_cast<B_32_IP_SM16825_5*>(busPtr))->Show(consistent); else (static_cast<B_32_I2_SM16825_5*>(busPtr))->Show(consistent); break;
      #endif
    #endif
      case I_HS_DOT_3: (static_cast<B_HS_DOT_3*>(busPtr))->Show(consistent); break;
      case I_SS_DOT_3: (static_cast<B_SS_DOT_3*>(busPtr))->Show(consistent); break;
      case I_HS_LPD_3: (static_cast<B_HS_LPD_3*>(busPtr))->Show(consistent); break;
      case I_SS_LPD_3: (static_cast<B_SS_LPD_3*>(busPtr))->Show(consistent); break;
      case I_HS_LPO_3: (static_cast<B_HS_LPO_3*>(busPtr))->Show(consistent); break;
      case I_SS_LPO_3: (static_cast<B_SS_LPO_3*>(busPtr))->Show(consistent); break;
      case I_HS_WS1_3: (static_cast<B_HS_WS1_3*>(busPtr))->Show(consistent); break;
      case I_SS_WS1_3: (static_cast<B_SS_WS1_3*>(busPtr))->Show(consistent); break;
      case I_HS_P98_3: (static_cast<B_HS_P98_3*>(busPtr))->Show(consistent); break;
      case I_SS_P98_3: (static_cast<B_SS_P98_3*>(busPtr))->Show(consistent); break;
    }
  }

  static bool canShow(void* busPtr, uint8_t busType) {
    switch (busType) {
      case I_NONE: return true;
    #ifdef ESP8266
      case I_8266_U0_NEO_3: return (static_cast<B_8266_U0_NEO_3*>(busPtr))->CanShow(); break;
      case I_8266_U1_NEO_3: return (static_cast<B_8266_U1_NEO_3*>(busPtr))->CanShow(); break;
      case I_8266_DM_NEO_3: return (static_cast<B_8266_DM_NEO_3*>(busPtr))->CanShow(); break;
      case I_8266_BB_NEO_3: return (static_cast<B_8266_BB_NEO_3*>(busPtr))->CanShow(); break;
      case I_8266_U0_NEO_4: return (static_cast<B_8266_U0_NEO_4*>(busPtr))->CanShow(); break;
      case I_8266_U1_NEO_4: return (static_cast<B_8266_U1_NEO_4*>(busPtr))->CanShow(); break;
      case I_8266_DM_NEO_4: return (static_cast<B_8266_DM_NEO_4*>(busPtr))->CanShow(); break;
      case I_8266_BB_NEO_4: return (static_cast<B_8266_BB_NEO_4*>(busPtr))->CanShow(); break;
      case I_8266_U0_400_3: return (static_cast<B_8266_U0_400_3*>(busPtr))->CanShow(); break;
      case I_8266_U1_400_3: return (static_cast<B_8266_U1_400_3*>(busPtr))->CanShow(); break;
      case I_8266_DM_400_3: return (static_cast<B_8266_DM_400_3*>(busPtr))->CanShow(); break;
      case I_8266_BB_400_3: return (static_cast<B_8266_BB_400_3*>(busPtr))->CanShow(); break;
      case I_8266_U0_TM1_4: return (static_cast<B_8266_U0_TM1_4*>(busPtr))->CanShow(); break;
      case I_8266_U1_TM1_4: return (static_cast<B_8266_U1_TM1_4*>(busPtr))->CanShow(); break;
      case I_8266_DM_TM1_4: return (static_cast<B_8266_DM_TM1_4*>(busPtr))->CanShow(); break;
      case I_8266_BB_TM1_4: return (static_cast<B_8266_BB_TM1_4*>(busPtr))->CanShow(); break;
      case I_8266_U0_TM2_3: return (static_cast<B_8266_U0_TM2_3*>(busPtr))->CanShow(); break;
      case I_8266_U1_TM2_3: return (static_cast<B_8266_U1_TM2_3*>(busPtr))->CanShow(); break;
      case I_8266_DM_TM2_3: return (static_cast<B_8266_DM_TM2_3*>(busPtr))->CanShow(); break;
      case I_8266_BB_TM2_3: return (static_cast<B_8266_BB_TM2_3*>(busPtr))->CanShow(); break;
      case I_8266_U0_UCS_3: return (static_cast<B_8266_U0_UCS_3*>(busPtr))->CanShow(); break;
      case I_8266_U1_UCS_3: return (static_cast<B_8266_U1_UCS_3*>(busPtr))->CanShow(); break;
      case I_8266_DM_UCS_3: return (static_cast<B_8266_DM_UCS_3*>(busPtr))->CanShow(); break;
      case I_8266_BB_UCS_3: return (static_cast<B_8266_BB_UCS_3*>(busPtr))->CanShow(); break;
      case I_8266_U0_UCS_4: return (static_cast<B_8266_U0_UCS_4*>(busPtr))->CanShow(); break;
      case I_8266_U1_UCS_4: return (static_cast<B_8266_U1_UCS_4*>(busPtr))->CanShow(); break;
      case I_8266_DM_UCS_4: return (static_cast<B_8266_DM_UCS_4*>(busPtr))->CanShow(); break;
      case I_8266_BB_UCS_4: return (static_cast<B_8266_BB_UCS_4*>(busPtr))->CanShow(); break;
      case I_8266_U0_APA106_3: return (static_cast<B_8266_U0_APA106_3*>(busPtr))->CanShow(); break;
      case I_8266_U1_APA106_3: return (static_cast<B_8266_U1_APA106_3*>(busPtr))->CanShow(); break;
      case I_8266_DM_APA106_3: return (static_cast<B_8266_DM_APA106_3*>(busPtr))->CanShow(); break;
      case I_8266_BB_APA106_3: return (static_cast<B_8266_BB_APA106_3*>(busPtr))->CanShow(); break;
      case I_8266_U0_FW6_5: return (static_cast<B_8266_U0_FW6_5*>(busPtr))->CanShow(); break;
      case I_8266_U1_FW6_5: return (static_cast<B_8266_U1_FW6_5*>(busPtr))->CanShow(); break;
      case I_8266_DM_FW6_5: return (static_cast<B_8266_DM_FW6_5*>(busPtr))->CanShow(); break;
      case I_8266_BB_FW6_5: return (static_cast<B_8266_BB_FW6_5*>(busPtr))->CanShow(); break;
      case I_8266_U0_2805_5: return (static_cast<B_8266_U0_2805_5*>(busPtr))->CanShow(); break;
      case I_8266_U1_2805_5: return (static_cast<B_8266_U1_2805_5*>(busPtr))->CanShow(); break;
      case I_8266_DM_2805_5: return (static_cast<B_8266_DM_2805_5*>(busPtr))->CanShow(); break;
      case I_8266_BB_2805_5: return (static_cast<B_8266_BB_2805_5*>(busPtr))->CanShow(); break;
      case I_8266_U0_TM1914_3: return (static_cast<B_8266_U0_TM1914_3*>(busPtr))->CanShow(); break;
      case I_8266_U1_TM1914_3: return (static_cast<B_8266_U1_TM1914_3*>(busPtr))->CanShow(); break;
      case I_8266_DM_TM1914_3: return (static_cast<B_8266_DM_TM1914_3*>(busPtr))->CanShow(); break;
      case I_8266_BB_TM1914_3: return (static_cast<B_8266_BB_TM1914_3*>(busPtr))->CanShow(); break;
      case I_8266_U0_SM16825_5: return (static_cast<B_8266_U0_SM16825_5*>(busPtr))->CanShow(); break;
      case I_8266_U1_SM16825_5: return (static_cast<B_8266_U1_SM16825_5*>(busPtr))->CanShow(); break;
      case I_8266_DM_SM16825_5: return (static_cast<B_8266_DM_SM16825_5*>(busPtr))->CanShow(); break;
      case I_8266_BB_SM16825_5: return (static_cast<B_8266_BB_SM16825_5*>(busPtr))->CanShow(); break;
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      // RMT buses
      case I_32_RN_NEO_3: return (static_cast<B_32_RN_NEO_3*>(busPtr))->CanShow(); break;
      case I_32_RN_NEO_4: return (static_cast<B_32_RN_NEO_4*>(busPtr))->CanShow(); break;
      case I_32_RN_400_3: return (static_cast<B_32_RN_400_3*>(busPtr))->CanShow(); break;
      case I_32_RN_TM1_4: return (static_cast<B_32_RN_TM1_4*>(busPtr))->CanShow(); break;
      case I_32_RN_TM2_3: return (static_cast<B_32_RN_TM2_3*>(busPtr))->CanShow(); break;
      case I_32_RN_UCS_3: return (static_cast<B_32_RN_UCS_3*>(busPtr))->CanShow(); break;
      case I_32_RN_UCS_4: return (static_cast<B_32_RN_UCS_4*>(busPtr))->CanShow(); break;
      case I_32_RN_APA106_3: return (static_cast<B_32_RN_APA106_3*>(busPtr))->CanShow(); break;
      case I_32_RN_FW6_5: return (static_cast<B_32_RN_FW6_5*>(busPtr))->CanShow(); break;
      case I_32_RN_2805_5: return (static_cast<B_32_RN_2805_5*>(busPtr))->CanShow(); break;
      case I_32_RN_TM1914_3: return (static_cast<B_32_RN_TM1914_3*>(busPtr))->CanShow(); break;
      case I_32_RN_SM16825_5: return (static_cast<B_32_RN_SM16825_5*>(busPtr))->CanShow(); break;
      // I2S1 bus or paralell buses
      #ifndef CONFIG_IDF_TARGET_ESP32C3
      case I_32_I2_NEO_3: if (_useParallelI2S) return (static_cast<B_32_IP_NEO_3*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_NEO_3*>(busPtr))->CanShow(); break;
      case I_32_I2_NEO_4: if (_useParallelI2S) return (static_cast<B_32_IP_NEO_4*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_NEO_4*>(busPtr))->CanShow(); break;
      case I_32_I2_400_3: if (_useParallelI2S) return (static_cast<B_32_IP_400_3*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_400_3*>(busPtr))->CanShow(); break;
      case I_32_I2_TM1_4: if (_useParallelI2S) return (static_cast<B_32_IP_TM1_4*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_TM1_4*>(busPtr))->CanShow(); break;
      case I_32_I2_TM2_3: if (_useParallelI2S) return (static_cast<B_32_IP_TM2_3*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_TM2_3*>(busPtr))->CanShow(); break;
      case I_32_I2_UCS_3: if (_useParallelI2S) return (static_cast<B_32_IP_UCS_3*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_UCS_3*>(busPtr))->CanShow(); break;
      case I_32_I2_UCS_4: if (_useParallelI2S) return (static_cast<B_32_IP_UCS_4*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_UCS_4*>(busPtr))->CanShow(); break;
      case I_32_I2_APA106_3: if (_useParallelI2S) return (static_cast<B_32_IP_APA106_3*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_APA106_3*>(busPtr))->CanShow(); break;
      case I_32_I2_FW6_5: if (_useParallelI2S) return (static_cast<B_32_IP_FW6_5*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_FW6_5*>(busPtr))->CanShow(); break;
      case I_32_I2_2805_5: if (_useParallelI2S) return (static_cast<B_32_IP_2805_5*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_2805_5*>(busPtr))->CanShow(); break;
      case I_32_I2_TM1914_3: if (_useParallelI2S) return (static_cast<B_32_IP_TM1914_3*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_TM1914_3*>(busPtr))->CanShow(); break;
      case I_32_I2_SM16825_5: if (_useParallelI2S) return (static_cast<B_32_IP_SM16825_5*>(busPtr))->CanShow(); else return (static_cast<B_32_I2_SM16825_5*>(busPtr))->CanShow(); break;
      #endif
    #endif
      case I_HS_DOT_3: return (static_cast<B_HS_DOT_3*>(busPtr))->CanShow(); break;
      case I_SS_DOT_3: return (static_cast<B_SS_DOT_3*>(busPtr))->CanShow(); break;
      case I_HS_LPD_3: return (static_cast<B_HS_LPD_3*>(busPtr))->CanShow(); break;
      case I_SS_LPD_3: return (static_cast<B_SS_LPD_3*>(busPtr))->CanShow(); break;
      case I_HS_LPO_3: return (static_cast<B_HS_LPO_3*>(busPtr))->CanShow(); break;
      case I_SS_LPO_3: return (static_cast<B_SS_LPO_3*>(busPtr))->CanShow(); break;
      case I_HS_WS1_3: return (static_cast<B_HS_WS1_3*>(busPtr))->CanShow(); break;
      case I_SS_WS1_3: return (static_cast<B_SS_WS1_3*>(busPtr))->CanShow(); break;
      case I_HS_P98_3: return (static_cast<B_HS_P98_3*>(busPtr))->CanShow(); break;
      case I_SS_P98_3: return (static_cast<B_SS_P98_3*>(busPtr))->CanShow(); break;
    }
    return true;
  }

  [[gnu::hot]] static void setPixelColor(void* busPtr, uint8_t busType, uint16_t pix, uint32_t c, uint8_t co, uint16_t wwcw = 0) {
    uint8_t r = c >> 16;
    uint8_t g = c >> 8;
    uint8_t b = c >> 0;
    uint8_t w = c >> 24;
    RgbwColor col;
    uint8_t cctWW = wwcw & 0xFF, cctCW = (wwcw>>8) & 0xFF;

    // reorder channels to selected order
    switch (co & 0x0F) {
      default: col.G = g; col.R = r; col.B = b; break; //0 = GRB, default
      case  1: col.G = r; col.R = g; col.B = b; break; //1 = RGB, common for WS2811
      case  2: col.G = b; col.R = r; col.B = g; break; //2 = BRG
      case  3: col.G = r; col.R = b; col.B = g; break; //3 = RBG
      case  4: col.G = b; col.R = g; col.B = r; break; //4 = BGR
      case  5: col.G = g; col.R = b; col.B = r; break; //5 = GBR
    }
    // upper nibble contains W swap information
    switch (co >> 4) {
      default: col.W = w;                break; // no swapping
      case  1: col.W = col.B; col.B = w; break; // swap W & B
      case  2: col.W = col.G; col.G = w; break; // swap W & G
      case  3: col.W = col.R; col.R = w; break; // swap W & R
      case  4: std::swap(cctWW, cctCW);  break; // swap WW & CW
    }

    switch (busType) {
      case I_NONE: break;
    #ifdef ESP8266
      case I_8266_U0_NEO_3: (static_cast<B_8266_U0_NEO_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_U1_NEO_3: (static_cast<B_8266_U1_NEO_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_DM_NEO_3: (static_cast<B_8266_DM_NEO_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_BB_NEO_3: (static_cast<B_8266_BB_NEO_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_U0_NEO_4: (static_cast<B_8266_U0_NEO_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_8266_U1_NEO_4: (static_cast<B_8266_U1_NEO_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_8266_DM_NEO_4: (static_cast<B_8266_DM_NEO_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_8266_BB_NEO_4: (static_cast<B_8266_BB_NEO_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_8266_U0_400_3: (static_cast<B_8266_U0_400_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_U1_400_3: (static_cast<B_8266_U1_400_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_DM_400_3: (static_cast<B_8266_DM_400_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_BB_400_3: (static_cast<B_8266_BB_400_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_U0_TM1_4: (static_cast<B_8266_U0_TM1_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_8266_U1_TM1_4: (static_cast<B_8266_U1_TM1_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_8266_DM_TM1_4: (static_cast<B_8266_DM_TM1_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_8266_BB_TM1_4: (static_cast<B_8266_BB_TM1_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_8266_U0_TM2_3: (static_cast<B_8266_U0_TM2_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_U1_TM2_3: (static_cast<B_8266_U1_TM2_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_DM_TM2_3: (static_cast<B_8266_DM_TM2_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_BB_TM2_3: (static_cast<B_8266_BB_TM2_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_U0_UCS_3: (static_cast<B_8266_U0_UCS_3*>(busPtr))->SetPixelColor(pix, Rgb48Color(RgbColor(col))); break;
      case I_8266_U1_UCS_3: (static_cast<B_8266_U1_UCS_3*>(busPtr))->SetPixelColor(pix, Rgb48Color(RgbColor(col))); break;
      case I_8266_DM_UCS_3: (static_cast<B_8266_DM_UCS_3*>(busPtr))->SetPixelColor(pix, Rgb48Color(RgbColor(col))); break;
      case I_8266_BB_UCS_3: (static_cast<B_8266_BB_UCS_3*>(busPtr))->SetPixelColor(pix, Rgb48Color(RgbColor(col))); break;
      case I_8266_U0_UCS_4: (static_cast<B_8266_U0_UCS_4*>(busPtr))->SetPixelColor(pix, Rgbw64Color(col)); break;
      case I_8266_U1_UCS_4: (static_cast<B_8266_U1_UCS_4*>(busPtr))->SetPixelColor(pix, Rgbw64Color(col)); break;
      case I_8266_DM_UCS_4: (static_cast<B_8266_DM_UCS_4*>(busPtr))->SetPixelColor(pix, Rgbw64Color(col)); break;
      case I_8266_BB_UCS_4: (static_cast<B_8266_BB_UCS_4*>(busPtr))->SetPixelColor(pix, Rgbw64Color(col)); break;
      case I_8266_U0_APA106_3: (static_cast<B_8266_U0_APA106_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_U1_APA106_3: (static_cast<B_8266_U1_APA106_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_DM_APA106_3: (static_cast<B_8266_DM_APA106_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_BB_APA106_3: (static_cast<B_8266_BB_APA106_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_U0_FW6_5: (static_cast<B_8266_U0_FW6_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_8266_U1_FW6_5: (static_cast<B_8266_U1_FW6_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_8266_DM_FW6_5: (static_cast<B_8266_DM_FW6_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_8266_BB_FW6_5: (static_cast<B_8266_BB_FW6_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_8266_U0_2805_5: (static_cast<B_8266_U0_2805_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_8266_U1_2805_5: (static_cast<B_8266_U1_2805_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_8266_DM_2805_5: (static_cast<B_8266_DM_2805_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_8266_BB_2805_5: (static_cast<B_8266_BB_2805_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_8266_U0_TM1914_3: (static_cast<B_8266_U0_TM1914_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_U1_TM1914_3: (static_cast<B_8266_U1_TM1914_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_DM_TM1914_3: (static_cast<B_8266_DM_TM1914_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_BB_TM1914_3: (static_cast<B_8266_BB_TM1914_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_8266_U0_SM16825_5: (static_cast<B_8266_U0_SM16825_5*>(busPtr))->SetPixelColor(pix, Rgbww80Color(col.R*257, col.G*257, col.B*257, cctWW*257, cctCW*257)); break;
      case I_8266_U1_SM16825_5: (static_cast<B_8266_U1_SM16825_5*>(busPtr))->SetPixelColor(pix, Rgbww80Color(col.R*257, col.G*257, col.B*257, cctWW*257, cctCW*257)); break;
      case I_8266_DM_SM16825_5: (static_cast<B_8266_DM_SM16825_5*>(busPtr))->SetPixelColor(pix, Rgbww80Color(col.R*257, col.G*257, col.B*257, cctWW*257, cctCW*257)); break;
      case I_8266_BB_SM16825_5: (static_cast<B_8266_BB_SM16825_5*>(busPtr))->SetPixelColor(pix, Rgbww80Color(col.R*257, col.G*257, col.B*257, cctWW*257, cctCW*257)); break;
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      // RMT buses
      case I_32_RN_NEO_3: (static_cast<B_32_RN_NEO_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_32_RN_NEO_4: (static_cast<B_32_RN_NEO_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_32_RN_400_3: (static_cast<B_32_RN_400_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_32_RN_TM1_4: (static_cast<B_32_RN_TM1_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_32_RN_TM2_3: (static_cast<B_32_RN_TM2_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_32_RN_UCS_3: (static_cast<B_32_RN_UCS_3*>(busPtr))->SetPixelColor(pix, Rgb48Color(RgbColor(col))); break;
      case I_32_RN_UCS_4: (static_cast<B_32_RN_UCS_4*>(busPtr))->SetPixelColor(pix, Rgbw64Color(col)); break;
      case I_32_RN_APA106_3: (static_cast<B_32_RN_APA106_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_32_RN_FW6_5: (static_cast<B_32_RN_FW6_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_32_RN_2805_5: (static_cast<B_32_RN_2805_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_32_RN_TM1914_3: (static_cast<B_32_RN_TM1914_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_32_RN_SM16825_5: (static_cast<B_32_RN_SM16825_5*>(busPtr))->SetPixelColor(pix, Rgbww80Color(col.R*257, col.G*257, col.B*257, cctWW*257, cctCW*257)); break;
      // I2S1 bus or paralell buses
      #ifndef CONFIG_IDF_TARGET_ESP32C3
      case I_32_I2_NEO_3: if (_useParallelI2S) (static_cast<B_32_IP_NEO_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); else (static_cast<B_32_I2_NEO_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_32_I2_NEO_4: if (_useParallelI2S) (static_cast<B_32_IP_NEO_4*>(busPtr))->SetPixelColor(pix, col); else (static_cast<B_32_I2_NEO_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_32_I2_400_3: if (_useParallelI2S) (static_cast<B_32_IP_400_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); else (static_cast<B_32_I2_400_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_32_I2_TM1_4: if (_useParallelI2S) (static_cast<B_32_IP_TM1_4*>(busPtr))->SetPixelColor(pix, col); else (static_cast<B_32_I2_TM1_4*>(busPtr))->SetPixelColor(pix, col); break;
      case I_32_I2_TM2_3: if (_useParallelI2S) (static_cast<B_32_IP_TM2_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); else (static_cast<B_32_I2_TM2_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_32_I2_UCS_3: if (_useParallelI2S) (static_cast<B_32_IP_UCS_3*>(busPtr))->SetPixelColor(pix, Rgb48Color(RgbColor(col))); else (static_cast<B_32_I2_UCS_3*>(busPtr))->SetPixelColor(pix, Rgb48Color(RgbColor(col))); break;
      case I_32_I2_UCS_4: if (_useParallelI2S) (static_cast<B_32_IP_UCS_4*>(busPtr))->SetPixelColor(pix, Rgbw64Color(col)); else (static_cast<B_32_I2_UCS_4*>(busPtr))->SetPixelColor(pix, Rgbw64Color(col)); break;
      case I_32_I2_APA106_3: if (_useParallelI2S) (static_cast<B_32_IP_APA106_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); else (static_cast<B_32_I2_APA106_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_32_I2_FW6_5: if (_useParallelI2S) (static_cast<B_32_IP_FW6_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); else (static_cast<B_32_I2_FW6_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_32_I2_2805_5: if (_useParallelI2S) (static_cast<B_32_IP_2805_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); else (static_cast<B_32_I2_2805_5*>(busPtr))->SetPixelColor(pix, RgbwwColor(col.R, col.G, col.B, cctWW, cctCW)); break;
      case I_32_I2_TM1914_3: if (_useParallelI2S) (static_cast<B_32_IP_TM1914_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); else (static_cast<B_32_I2_TM1914_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_32_I2_SM16825_5: if (_useParallelI2S) (static_cast<B_32_IP_SM16825_5*>(busPtr))->SetPixelColor(pix, Rgbww80Color(col.R*257, col.G*257, col.B*257, cctWW*257, cctCW*257)); else (static_cast<B_32_I2_SM16825_5*>(busPtr))->SetPixelColor(pix, Rgbww80Color(col.R*257, col.G*257, col.B*257, cctWW*257, cctCW*257)); break;
      #endif
    #endif
      case I_HS_DOT_3: (static_cast<B_HS_DOT_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_SS_DOT_3: (static_cast<B_SS_DOT_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_HS_LPD_3: (static_cast<B_HS_LPD_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_SS_LPD_3: (static_cast<B_SS_LPD_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_HS_LPO_3: (static_cast<B_HS_LPO_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_SS_LPO_3: (static_cast<B_SS_LPO_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_HS_WS1_3: (static_cast<B_HS_WS1_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_SS_WS1_3: (static_cast<B_SS_WS1_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_HS_P98_3: (static_cast<B_HS_P98_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
      case I_SS_P98_3: (static_cast<B_SS_P98_3*>(busPtr))->SetPixelColor(pix, RgbColor(col)); break;
    }
  }

  [[gnu::hot]] static uint32_t getPixelColor(void* busPtr, uint8_t busType, uint16_t pix, uint8_t co) {
    RgbwColor col(0,0,0,0);
    switch (busType) {
      case I_NONE: break;
    #ifdef ESP8266
      case I_8266_U0_NEO_3: col = (static_cast<B_8266_U0_NEO_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U1_NEO_3: col = (static_cast<B_8266_U1_NEO_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_DM_NEO_3: col = (static_cast<B_8266_DM_NEO_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_BB_NEO_3: col = (static_cast<B_8266_BB_NEO_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U0_NEO_4: col = (static_cast<B_8266_U0_NEO_4*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U1_NEO_4: col = (static_cast<B_8266_U1_NEO_4*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_DM_NEO_4: col = (static_cast<B_8266_DM_NEO_4*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_BB_NEO_4: col = (static_cast<B_8266_BB_NEO_4*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U0_400_3: col = (static_cast<B_8266_U0_400_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U1_400_3: col = (static_cast<B_8266_U1_400_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_DM_400_3: col = (static_cast<B_8266_DM_400_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_BB_400_3: col = (static_cast<B_8266_BB_400_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U0_TM1_4: col = (static_cast<B_8266_U0_TM1_4*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U1_TM1_4: col = (static_cast<B_8266_U1_TM1_4*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_DM_TM1_4: col = (static_cast<B_8266_DM_TM1_4*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_BB_TM1_4: col = (static_cast<B_8266_BB_TM1_4*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U0_TM2_3: col = (static_cast<B_8266_U0_TM2_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U1_TM2_3: col = (static_cast<B_8266_U1_TM2_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_DM_TM2_3: col = (static_cast<B_8266_DM_TM2_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_BB_TM2_3: col = (static_cast<B_8266_BB_TM2_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U0_UCS_3: { Rgb48Color c = (static_cast<B_8266_U0_UCS_3*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R>>8,c.G>>8,c.B>>8,0); } break;
      case I_8266_U1_UCS_3: { Rgb48Color c = (static_cast<B_8266_U1_UCS_3*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R>>8,c.G>>8,c.B>>8,0); } break;
      case I_8266_DM_UCS_3: { Rgb48Color c = (static_cast<B_8266_DM_UCS_3*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R>>8,c.G>>8,c.B>>8,0); } break;
      case I_8266_BB_UCS_3: { Rgb48Color c = (static_cast<B_8266_BB_UCS_3*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R>>8,c.G>>8,c.B>>8,0); } break;
      case I_8266_U0_UCS_4: { Rgbw64Color c = (static_cast<B_8266_U0_UCS_4*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R>>8,c.G>>8,c.B>>8,c.W>>8); } break;
      case I_8266_U1_UCS_4: { Rgbw64Color c = (static_cast<B_8266_U1_UCS_4*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R>>8,c.G>>8,c.B>>8,c.W>>8); } break;
      case I_8266_DM_UCS_4: { Rgbw64Color c = (static_cast<B_8266_DM_UCS_4*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R>>8,c.G>>8,c.B>>8,c.W>>8); } break;
      case I_8266_BB_UCS_4: { Rgbw64Color c = (static_cast<B_8266_BB_UCS_4*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R>>8,c.G>>8,c.B>>8,c.W>>8); } break;
      case I_8266_U0_APA106_3: col = (static_cast<B_8266_U0_APA106_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U1_APA106_3: col = (static_cast<B_8266_U1_APA106_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_DM_APA106_3: col = (static_cast<B_8266_DM_APA106_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_BB_APA106_3: col = (static_cast<B_8266_BB_APA106_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U0_FW6_5: { RgbwwColor c = (static_cast<B_8266_U0_FW6_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_U1_FW6_5: { RgbwwColor c = (static_cast<B_8266_U1_FW6_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_DM_FW6_5: { RgbwwColor c = (static_cast<B_8266_DM_FW6_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_BB_FW6_5: { RgbwwColor c = (static_cast<B_8266_BB_FW6_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_U0_2805_5: { RgbwwColor c = (static_cast<B_8266_U0_2805_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_U1_2805_5: { RgbwwColor c = (static_cast<B_8266_U1_2805_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_DM_2805_5: { RgbwwColor c = (static_cast<B_8266_DM_2805_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_BB_2805_5: { RgbwwColor c = (static_cast<B_8266_BB_2805_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_U0_TM1914_3: col = (static_cast<B_8266_U0_TM1914_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U1_TM1914_3: col = (static_cast<B_8266_U1_TM1914_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_DM_TM1914_3: col = (static_cast<B_8266_DM_TM1914_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_BB_TM1914_3: col = (static_cast<B_8266_BB_TM1914_3*>(busPtr))->GetPixelColor(pix); break;
      case I_8266_U0_SM16825_5: { Rgbww80Color c = (static_cast<B_8266_U0_SM16825_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_U1_SM16825_5: { Rgbww80Color c = (static_cast<B_8266_U1_SM16825_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_DM_SM16825_5: { Rgbww80Color c = (static_cast<B_8266_DM_SM16825_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_8266_BB_SM16825_5: { Rgbww80Color c = (static_cast<B_8266_BB_SM16825_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      // RMT buses
      case I_32_RN_NEO_3: col = (static_cast<B_32_RN_NEO_3*>(busPtr))->GetPixelColor(pix); break;
      case I_32_RN_NEO_4: col = (static_cast<B_32_RN_NEO_4*>(busPtr))->GetPixelColor(pix); break;
      case I_32_RN_400_3: col = (static_cast<B_32_RN_400_3*>(busPtr))->GetPixelColor(pix); break;
      case I_32_RN_TM1_4: col = (static_cast<B_32_RN_TM1_4*>(busPtr))->GetPixelColor(pix); break;
      case I_32_RN_TM2_3: col = (static_cast<B_32_RN_TM2_3*>(busPtr))->GetPixelColor(pix); break;
      case I_32_RN_UCS_3: { Rgb48Color c = (static_cast<B_32_RN_UCS_3*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R>>8,c.G>>8,c.B>>8,0); } break;
      case I_32_RN_UCS_4: { Rgbw64Color c = (static_cast<B_32_RN_UCS_4*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R>>8,c.G>>8,c.B>>8,c.W>>8); } break;
      case I_32_RN_APA106_3: col = (static_cast<B_32_RN_APA106_3*>(busPtr))->GetPixelColor(pix); break;
      case I_32_RN_FW6_5: { RgbwwColor c = (static_cast<B_32_RN_FW6_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_32_RN_2805_5: { RgbwwColor c = (static_cast<B_32_RN_2805_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_32_RN_TM1914_3: col = (static_cast<B_32_RN_TM1914_3*>(busPtr))->GetPixelColor(pix); break;
      case I_32_RN_SM16825_5: { Rgbww80Color c = (static_cast<B_32_RN_SM16825_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R/257,c.G/257,c.B/257,max(c.WW,c.CW)/257); } break; // will not return original W
      // I2S1 bus or paralell buses
      #ifndef CONFIG_IDF_TARGET_ESP32C3
      case I_32_I2_NEO_3: col = (_useParallelI2S) ? (static_cast<B_32_IP_NEO_3*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_NEO_3*>(busPtr))->GetPixelColor(pix); break;
      case I_32_I2_NEO_4: col = (_useParallelI2S) ? (static_cast<B_32_IP_NEO_4*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_NEO_4*>(busPtr))->GetPixelColor(pix); break;
      case I_32_I2_400_3: col = (_useParallelI2S) ? (static_cast<B_32_IP_400_3*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_400_3*>(busPtr))->GetPixelColor(pix); break;
      case I_32_I2_TM1_4: col = (_useParallelI2S) ? (static_cast<B_32_IP_TM1_4*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_TM1_4*>(busPtr))->GetPixelColor(pix); break;
      case I_32_I2_TM2_3: col = (_useParallelI2S) ? (static_cast<B_32_IP_TM2_3*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_TM2_3*>(busPtr))->GetPixelColor(pix); break;
      case I_32_I2_UCS_3: { Rgb48Color c = (_useParallelI2S) ? (static_cast<B_32_IP_UCS_3*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_UCS_3*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R/257,c.G/257,c.B/257,0); } break;
      case I_32_I2_UCS_4: { Rgbw64Color c = (_useParallelI2S) ? (static_cast<B_32_IP_UCS_4*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_UCS_4*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R/257,c.G/257,c.B/257,c.W/257); } break;
      case I_32_I2_APA106_3: col = (_useParallelI2S) ? (static_cast<B_32_IP_APA106_3*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_APA106_3*>(busPtr))->GetPixelColor(pix); break;
      case I_32_I2_FW6_5: { RgbwwColor c = (_useParallelI2S) ? (static_cast<B_32_IP_FW6_5*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_FW6_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_32_I2_2805_5: { RgbwwColor c = (_useParallelI2S) ? (static_cast<B_32_IP_2805_5*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_2805_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R,c.G,c.B,max(c.WW,c.CW)); } break; // will not return original W
      case I_32_I2_TM1914_3: col = (_useParallelI2S) ? (static_cast<B_32_IP_TM1914_3*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_TM1914_3*>(busPtr))->GetPixelColor(pix); break;
      case I_32_I2_SM16825_5: { Rgbww80Color c = (_useParallelI2S) ? (static_cast<B_32_IP_SM16825_5*>(busPtr))->GetPixelColor(pix) : (static_cast<B_32_I2_SM16825_5*>(busPtr))->GetPixelColor(pix); col = RGBW32(c.R/257,c.G/257,c.B/257,max(c.WW,c.CW)/257); } break; // will not return original W
      #endif
    #endif
      case I_HS_DOT_3: col = (static_cast<B_HS_DOT_3*>(busPtr))->GetPixelColor(pix); break;
      case I_SS_DOT_3: col = (static_cast<B_SS_DOT_3*>(busPtr))->GetPixelColor(pix); break;
      case I_HS_LPD_3: col = (static_cast<B_HS_LPD_3*>(busPtr))->GetPixelColor(pix); break;
      case I_SS_LPD_3: col = (static_cast<B_SS_LPD_3*>(busPtr))->GetPixelColor(pix); break;
      case I_HS_LPO_3: col = (static_cast<B_HS_LPO_3*>(busPtr))->GetPixelColor(pix); break;
      case I_SS_LPO_3: col = (static_cast<B_SS_LPO_3*>(busPtr))->GetPixelColor(pix); break;
      case I_HS_WS1_3: col = (static_cast<B_HS_WS1_3*>(busPtr))->GetPixelColor(pix); break;
      case I_SS_WS1_3: col = (static_cast<B_SS_WS1_3*>(busPtr))->GetPixelColor(pix); break;
      case I_HS_P98_3: col = (static_cast<B_HS_P98_3*>(busPtr))->GetPixelColor(pix); break;
      case I_SS_P98_3: col = (static_cast<B_SS_P98_3*>(busPtr))->GetPixelColor(pix); break;
    }

    // upper nibble contains W swap information
    uint8_t w = col.W;
    switch (co >> 4) {
      case 1: col.W = col.B; col.B = w; break; // swap W & B
      case 2: col.W = col.G; col.G = w; break; // swap W & G
      case 3: col.W = col.R; col.R = w; break; // swap W & R
    }
    switch (co & 0x0F) {
      //                    W               G              R               B
      default: return ((col.W << 24) | (col.G << 8) | (col.R << 16) | (col.B)); //0 = GRB, default
      case  1: return ((col.W << 24) | (col.R << 8) | (col.G << 16) | (col.B)); //1 = RGB, common for WS2811
      case  2: return ((col.W << 24) | (col.B << 8) | (col.R << 16) | (col.G)); //2 = BRG
      case  3: return ((col.W << 24) | (col.B << 8) | (col.G << 16) | (col.R)); //3 = RBG
      case  4: return ((col.W << 24) | (col.R << 8) | (col.B << 16) | (col.G)); //4 = BGR
      case  5: return ((col.W << 24) | (col.G << 8) | (col.B << 16) | (col.R)); //5 = GBR
    }
    return 0;
  }

  static void cleanup(void* busPtr, uint8_t busType) {
    if (busPtr == nullptr) return;
    switch (busType) {
      case I_NONE: break;
    #ifdef ESP8266
      case I_8266_U0_NEO_3: delete (static_cast<B_8266_U0_NEO_3*>(busPtr)); break;
      case I_8266_U1_NEO_3: delete (static_cast<B_8266_U1_NEO_3*>(busPtr)); break;
      case I_8266_DM_NEO_3: delete (static_cast<B_8266_DM_NEO_3*>(busPtr)); break;
      case I_8266_BB_NEO_3: delete (static_cast<B_8266_BB_NEO_3*>(busPtr)); break;
      case I_8266_U0_NEO_4: delete (static_cast<B_8266_U0_NEO_4*>(busPtr)); break;
      case I_8266_U1_NEO_4: delete (static_cast<B_8266_U1_NEO_4*>(busPtr)); break;
      case I_8266_DM_NEO_4: delete (static_cast<B_8266_DM_NEO_4*>(busPtr)); break;
      case I_8266_BB_NEO_4: delete (static_cast<B_8266_BB_NEO_4*>(busPtr)); break;
      case I_8266_U0_400_3: delete (static_cast<B_8266_U0_400_3*>(busPtr)); break;
      case I_8266_U1_400_3: delete (static_cast<B_8266_U1_400_3*>(busPtr)); break;
      case I_8266_DM_400_3: delete (static_cast<B_8266_DM_400_3*>(busPtr)); break;
      case I_8266_BB_400_3: delete (static_cast<B_8266_BB_400_3*>(busPtr)); break;
      case I_8266_U0_TM1_4: delete (static_cast<B_8266_U0_TM1_4*>(busPtr)); break;
      case I_8266_U1_TM1_4: delete (static_cast<B_8266_U1_TM1_4*>(busPtr)); break;
      case I_8266_DM_TM1_4: delete (static_cast<B_8266_DM_TM1_4*>(busPtr)); break;
      case I_8266_BB_TM1_4: delete (static_cast<B_8266_BB_TM1_4*>(busPtr)); break;
      case I_8266_U0_TM2_3: delete (static_cast<B_8266_U0_TM2_3*>(busPtr)); break;
      case I_8266_U1_TM2_3: delete (static_cast<B_8266_U1_TM2_3*>(busPtr)); break;
      case I_8266_DM_TM2_3: delete (static_cast<B_8266_DM_TM2_3*>(busPtr)); break;
      case I_8266_BB_TM2_3: delete (static_cast<B_8266_BB_TM2_3*>(busPtr)); break;
      case I_8266_U0_UCS_3: delete (static_cast<B_8266_U0_UCS_3*>(busPtr)); break;
      case I_8266_U1_UCS_3: delete (static_cast<B_8266_U1_UCS_3*>(busPtr)); break;
      case I_8266_DM_UCS_3: delete (static_cast<B_8266_DM_UCS_3*>(busPtr)); break;
      case I_8266_BB_UCS_3: delete (static_cast<B_8266_BB_UCS_3*>(busPtr)); break;
      case I_8266_U0_UCS_4: delete (static_cast<B_8266_U0_UCS_4*>(busPtr)); break;
      case I_8266_U1_UCS_4: delete (static_cast<B_8266_U1_UCS_4*>(busPtr)); break;
      case I_8266_DM_UCS_4: delete (static_cast<B_8266_DM_UCS_4*>(busPtr)); break;
      case I_8266_BB_UCS_4: delete (static_cast<B_8266_BB_UCS_4*>(busPtr)); break;
      case I_8266_U0_APA106_3: delete (static_cast<B_8266_U0_APA106_3*>(busPtr)); break;
      case I_8266_U1_APA106_3: delete (static_cast<B_8266_U1_APA106_3*>(busPtr)); break;
      case I_8266_DM_APA106_3: delete (static_cast<B_8266_DM_APA106_3*>(busPtr)); break;
      case I_8266_BB_APA106_3: delete (static_cast<B_8266_BB_APA106_3*>(busPtr)); break;
      case I_8266_U0_FW6_5: delete (static_cast<B_8266_U0_FW6_5*>(busPtr)); break;
      case I_8266_U1_FW6_5: delete (static_cast<B_8266_U1_FW6_5*>(busPtr)); break;
      case I_8266_DM_FW6_5: delete (static_cast<B_8266_DM_FW6_5*>(busPtr)); break;
      case I_8266_BB_FW6_5: delete (static_cast<B_8266_BB_FW6_5*>(busPtr)); break;
      case I_8266_U0_2805_5: delete (static_cast<B_8266_U0_2805_5*>(busPtr)); break;
      case I_8266_U1_2805_5: delete (static_cast<B_8266_U1_2805_5*>(busPtr)); break;
      case I_8266_DM_2805_5: delete (static_cast<B_8266_DM_2805_5*>(busPtr)); break;
      case I_8266_BB_2805_5: delete (static_cast<B_8266_BB_2805_5*>(busPtr)); break;
      case I_8266_U0_TM1914_3: delete (static_cast<B_8266_U0_TM1914_3*>(busPtr)); break;
      case I_8266_U1_TM1914_3: delete (static_cast<B_8266_U1_TM1914_3*>(busPtr)); break;
      case I_8266_DM_TM1914_3: delete (static_cast<B_8266_DM_TM1914_3*>(busPtr)); break;
      case I_8266_BB_TM1914_3: delete (static_cast<B_8266_BB_TM1914_3*>(busPtr)); break;
      case I_8266_U0_SM16825_5: delete (static_cast<B_8266_U0_SM16825_5*>(busPtr)); break;
      case I_8266_U1_SM16825_5: delete (static_cast<B_8266_U1_SM16825_5*>(busPtr)); break;
      case I_8266_DM_SM16825_5: delete (static_cast<B_8266_DM_SM16825_5*>(busPtr)); break;
      case I_8266_BB_SM16825_5: delete (static_cast<B_8266_BB_SM16825_5*>(busPtr)); break;
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      // RMT buses
      case I_32_RN_NEO_3: delete (static_cast<B_32_RN_NEO_3*>(busPtr)); break;
      case I_32_RN_NEO_4: delete (static_cast<B_32_RN_NEO_4*>(busPtr)); break;
      case I_32_RN_400_3: delete (static_cast<B_32_RN_400_3*>(busPtr)); break;
      case I_32_RN_TM1_4: delete (static_cast<B_32_RN_TM1_4*>(busPtr)); break;
      case I_32_RN_TM2_3: delete (static_cast<B_32_RN_TM2_3*>(busPtr)); break;
      case I_32_RN_UCS_3: delete (static_cast<B_32_RN_UCS_3*>(busPtr)); break;
      case I_32_RN_UCS_4: delete (static_cast<B_32_RN_UCS_4*>(busPtr)); break;
      case I_32_RN_APA106_3: delete (static_cast<B_32_RN_APA106_3*>(busPtr)); break;
      case I_32_RN_FW6_5: delete (static_cast<B_32_RN_FW6_5*>(busPtr)); break;
      case I_32_RN_2805_5: delete (static_cast<B_32_RN_2805_5*>(busPtr)); break;
      case I_32_RN_TM1914_3: delete (static_cast<B_32_RN_TM1914_3*>(busPtr)); break;
      case I_32_RN_SM16825_5: delete (static_cast<B_32_RN_SM16825_5*>(busPtr)); break;
      // I2S1 bus or paralell buses
      #ifndef CONFIG_IDF_TARGET_ESP32C3
      case I_32_I2_NEO_3: if (_useParallelI2S) delete (static_cast<B_32_IP_NEO_3*>(busPtr)); else delete (static_cast<B_32_I2_NEO_3*>(busPtr)); break;
      case I_32_I2_NEO_4: if (_useParallelI2S) delete (static_cast<B_32_IP_NEO_4*>(busPtr)); else delete (static_cast<B_32_I2_NEO_4*>(busPtr)); break;
      case I_32_I2_400_3: if (_useParallelI2S) delete (static_cast<B_32_IP_400_3*>(busPtr)); else delete (static_cast<B_32_I2_400_3*>(busPtr)); break;
      case I_32_I2_TM1_4: if (_useParallelI2S) delete (static_cast<B_32_IP_TM1_4*>(busPtr)); else delete (static_cast<B_32_I2_TM1_4*>(busPtr)); break;
      case I_32_I2_TM2_3: if (_useParallelI2S) delete (static_cast<B_32_IP_TM2_3*>(busPtr)); else delete (static_cast<B_32_I2_TM2_3*>(busPtr)); break;
      case I_32_I2_UCS_3: if (_useParallelI2S) delete (static_cast<B_32_IP_UCS_3*>(busPtr)); else delete (static_cast<B_32_I2_UCS_3*>(busPtr)); break;
      case I_32_I2_UCS_4: if (_useParallelI2S) delete (static_cast<B_32_IP_UCS_4*>(busPtr)); else delete (static_cast<B_32_I2_UCS_4*>(busPtr)); break;
      case I_32_I2_APA106_3: if (_useParallelI2S) delete (static_cast<B_32_IP_APA106_3*>(busPtr)); else delete (static_cast<B_32_I2_APA106_3*>(busPtr)); break;
      case I_32_I2_FW6_5: if (_useParallelI2S) delete (static_cast<B_32_IP_FW6_5*>(busPtr)); else delete (static_cast<B_32_I2_FW6_5*>(busPtr)); break;
      case I_32_I2_2805_5: if (_useParallelI2S) delete (static_cast<B_32_IP_2805_5*>(busPtr)); else delete (static_cast<B_32_I2_2805_5*>(busPtr)); break;
      case I_32_I2_TM1914_3: if (_useParallelI2S) delete (static_cast<B_32_IP_TM1914_3*>(busPtr)); else delete (static_cast<B_32_I2_TM1914_3*>(busPtr)); break;
      case I_32_I2_SM16825_5: if (_useParallelI2S) delete (static_cast<B_32_IP_SM16825_5*>(busPtr)); else delete (static_cast<B_32_I2_SM16825_5*>(busPtr)); break;
      #endif
    #endif
      case I_HS_DOT_3: delete (static_cast<B_HS_DOT_3*>(busPtr)); break;
      case I_SS_DOT_3: delete (static_cast<B_SS_DOT_3*>(busPtr)); break;
      case I_HS_LPD_3: delete (static_cast<B_HS_LPD_3*>(busPtr)); break;
      case I_SS_LPD_3: delete (static_cast<B_SS_LPD_3*>(busPtr)); break;
      case I_HS_LPO_3: delete (static_cast<B_HS_LPO_3*>(busPtr)); break;
      case I_SS_LPO_3: delete (static_cast<B_SS_LPO_3*>(busPtr)); break;
      case I_HS_WS1_3: delete (static_cast<B_HS_WS1_3*>(busPtr)); break;
      case I_SS_WS1_3: delete (static_cast<B_SS_WS1_3*>(busPtr)); break;
      case I_HS_P98_3: delete (static_cast<B_HS_P98_3*>(busPtr)); break;
      case I_SS_P98_3: delete (static_cast<B_SS_P98_3*>(busPtr)); break;
    }
  }

  static unsigned getDataSize(void* busPtr, uint8_t busType) {
    unsigned size = 0;
    switch (busType) {
      case I_NONE: break;
    #ifdef ESP8266
      case I_8266_U0_NEO_3: size = (static_cast<B_8266_U0_NEO_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_NEO_3: size = (static_cast<B_8266_U1_NEO_3*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_NEO_3: size = (static_cast<B_8266_DM_NEO_3*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_NEO_3: size = (static_cast<B_8266_BB_NEO_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_NEO_4: size = (static_cast<B_8266_U0_NEO_4*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_NEO_4: size = (static_cast<B_8266_U1_NEO_4*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_NEO_4: size = (static_cast<B_8266_DM_NEO_4*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_NEO_4: size = (static_cast<B_8266_BB_NEO_4*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_400_3: size = (static_cast<B_8266_U0_400_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_400_3: size = (static_cast<B_8266_U1_400_3*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_400_3: size = (static_cast<B_8266_DM_400_3*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_400_3: size = (static_cast<B_8266_BB_400_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_TM1_4: size = (static_cast<B_8266_U0_TM1_4*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_TM1_4: size = (static_cast<B_8266_U1_TM1_4*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_TM1_4: size = (static_cast<B_8266_DM_TM1_4*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_TM1_4: size = (static_cast<B_8266_BB_TM1_4*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_TM2_3: size = (static_cast<B_8266_U0_TM2_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_TM2_3: size = (static_cast<B_8266_U1_TM2_3*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_TM2_3: size = (static_cast<B_8266_DM_TM2_3*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_TM2_3: size = (static_cast<B_8266_BB_TM2_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_UCS_3: size = (static_cast<B_8266_U0_UCS_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_UCS_3: size = (static_cast<B_8266_U1_UCS_3*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_UCS_3: size = (static_cast<B_8266_DM_UCS_3*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_UCS_3: size = (static_cast<B_8266_BB_UCS_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_UCS_4: size = (static_cast<B_8266_U0_UCS_4*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_UCS_4: size = (static_cast<B_8266_U1_UCS_4*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_UCS_4: size = (static_cast<B_8266_DM_UCS_4*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_UCS_4: size = (static_cast<B_8266_BB_UCS_4*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_APA106_3: size = (static_cast<B_8266_U0_APA106_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_APA106_3: size = (static_cast<B_8266_U1_APA106_3*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_APA106_3: size = (static_cast<B_8266_DM_APA106_3*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_APA106_3: size = (static_cast<B_8266_BB_APA106_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_FW6_5: size = (static_cast<B_8266_U0_FW6_5*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_FW6_5: size = (static_cast<B_8266_U1_FW6_5*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_FW6_5: size = (static_cast<B_8266_DM_FW6_5*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_FW6_5: size = (static_cast<B_8266_BB_FW6_5*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_2805_5: size = (static_cast<B_8266_U0_2805_5*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_2805_5: size = (static_cast<B_8266_U1_2805_5*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_2805_5: size = (static_cast<B_8266_DM_2805_5*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_2805_5: size = (static_cast<B_8266_BB_2805_5*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_TM1914_3: size = (static_cast<B_8266_U0_TM1914_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_TM1914_3: size = (static_cast<B_8266_U1_TM1914_3*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_TM1914_3: size = (static_cast<B_8266_DM_TM1914_3*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_TM1914_3: size = (static_cast<B_8266_BB_TM1914_3*>(busPtr))->PixelsSize(); break;
      case I_8266_U0_SM16825_5: size = (static_cast<B_8266_U0_SM16825_5*>(busPtr))->PixelsSize(); break;
      case I_8266_U1_SM16825_5: size = (static_cast<B_8266_U1_SM16825_5*>(busPtr))->PixelsSize(); break;
      case I_8266_DM_SM16825_5: size = (static_cast<B_8266_DM_SM16825_5*>(busPtr))->PixelsSize()*5; break;
      case I_8266_BB_SM16825_5: size = (static_cast<B_8266_BB_SM16825_5*>(busPtr))->PixelsSize(); break;
    #endif
    #ifdef ARDUINO_ARCH_ESP32
      // RMT buses (front + back + small system managed RMT)
      case I_32_RN_NEO_3: size = (static_cast<B_32_RN_NEO_3*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_NEO_4: size = (static_cast<B_32_RN_NEO_4*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_400_3: size = (static_cast<B_32_RN_400_3*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_TM1_4: size = (static_cast<B_32_RN_TM1_4*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_TM2_3: size = (static_cast<B_32_RN_TM2_3*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_UCS_3: size = (static_cast<B_32_RN_UCS_3*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_UCS_4: size = (static_cast<B_32_RN_UCS_4*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_APA106_3: size = (static_cast<B_32_RN_APA106_3*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_FW6_5: size = (static_cast<B_32_RN_FW6_5*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_2805_5: size = (static_cast<B_32_RN_2805_5*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_TM1914_3: size = (static_cast<B_32_RN_TM1914_3*>(busPtr))->PixelsSize()*2; break;
      case I_32_RN_SM16825_5: size = (static_cast<B_32_RN_SM16825_5*>(busPtr))->PixelsSize()*2; break;
      // I2S1 bus or paralell buses (front + DMA; DMA = front * cadence, aligned to 4 bytes)
      #ifndef CONFIG_IDF_TARGET_ESP32C3
      case I_32_I2_NEO_3: size = (_useParallelI2S) ? (static_cast<B_32_IP_NEO_3*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_NEO_3*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_NEO_4: size = (_useParallelI2S) ? (static_cast<B_32_IP_NEO_4*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_NEO_4*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_400_3: size = (_useParallelI2S) ? (static_cast<B_32_IP_400_3*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_400_3*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_TM1_4: size = (_useParallelI2S) ? (static_cast<B_32_IP_TM1_4*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_TM1_4*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_TM2_3: size = (_useParallelI2S) ? (static_cast<B_32_IP_TM2_3*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_TM2_3*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_UCS_3: size = (_useParallelI2S) ? (static_cast<B_32_IP_UCS_3*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_UCS_3*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_UCS_4: size = (_useParallelI2S) ? (static_cast<B_32_IP_UCS_4*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_UCS_4*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_APA106_3: size = (_useParallelI2S) ? (static_cast<B_32_IP_APA106_3*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_APA106_3*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_FW6_5: size = (_useParallelI2S) ? (static_cast<B_32_IP_FW6_5*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_FW6_5*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_2805_5: size = (_useParallelI2S) ? (static_cast<B_32_IP_2805_5*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_2805_5*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_TM1914_3: size = (_useParallelI2S) ? (static_cast<B_32_IP_TM1914_3*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_TM1914_3*>(busPtr))->PixelsSize()*4; break;
      case I_32_I2_SM16825_5: size = (_useParallelI2S) ? (static_cast<B_32_IP_SM16825_5*>(busPtr))->PixelsSize()*4 : (static_cast<B_32_I2_SM16825_5*>(busPtr))->PixelsSize()*4; break;
      #endif
    #endif
      case I_HS_DOT_3: size = (static_cast<B_HS_DOT_3*>(busPtr))->PixelsSize()*2; break;
      case I_SS_DOT_3: size = (static_cast<B_SS_DOT_3*>(busPtr))->PixelsSize()*2; break;
      case I_HS_LPD_3: size = (static_cast<B_HS_LPD_3*>(busPtr))->PixelsSize()*2; break;
      case I_SS_LPD_3: size = (static_cast<B_SS_LPD_3*>(busPtr))->PixelsSize()*2; break;
      case I_HS_LPO_3: size = (static_cast<B_HS_LPO_3*>(busPtr))->PixelsSize()*2; break;
      case I_SS_LPO_3: size = (static_cast<B_SS_LPO_3*>(busPtr))->PixelsSize()*2; break;
      case I_HS_WS1_3: size = (static_cast<B_HS_WS1_3*>(busPtr))->PixelsSize()*2; break;
      case I_SS_WS1_3: size = (static_cast<B_SS_WS1_3*>(busPtr))->PixelsSize()*2; break;
      case I_HS_P98_3: size = (static_cast<B_HS_P98_3*>(busPtr))->PixelsSize()*2; break;
      case I_SS_P98_3: size = (static_cast<B_SS_P98_3*>(busPtr))->PixelsSize()*2; break;
    }
    return size;
  }

  static unsigned memUsage(unsigned count, unsigned busType) {
    unsigned size = count*3;  // let's assume 3 channels, we will add count or 2*count below for 4 channels or 5 channels
    switch (busType) {
      case I_NONE: size = 0; break;
    #ifdef ESP8266
      // UART methods have front + back buffers + small UART
      case I_8266_U0_NEO_4    : // fallthrough
      case I_8266_U1_NEO_4    : // fallthrough
      case I_8266_BB_NEO_4    : // fallthrough
      case I_8266_U0_TM1_4    : // fallthrough
      case I_8266_U1_TM1_4    : // fallthrough
      case I_8266_BB_TM1_4    : size = (size + count);       break; // 4 channels
      case I_8266_U0_UCS_3    : // fallthrough
      case I_8266_U1_UCS_3    : // fallthrough
      case I_8266_BB_UCS_3    : size *= 2;                   break; // 16 bit
      case I_8266_U0_UCS_4    : // fallthrough
      case I_8266_U1_UCS_4    : // fallthrough
      case I_8266_BB_UCS_4    : size = (size + count)*2;     break; // 16 bit 4 channels
      case I_8266_U0_FW6_5    : // fallthrough
      case I_8266_U1_FW6_5    : // fallthrough
      case I_8266_BB_FW6_5    : // fallthrough
      case I_8266_U0_2805_5   : // fallthrough
      case I_8266_U1_2805_5   : // fallthrough
      case I_8266_BB_2805_5   : size = (size + 2*count);     break; // 5 channels
      case I_8266_U0_SM16825_5: // fallthrough
      case I_8266_U1_SM16825_5: // fallthrough
      case I_8266_BB_SM16825_5: size = (size + 2*count)*2;   break; // 16 bit 5 channels
      // DMA methods have front + DMA buffer = ((1+(3+1)) * channels; exact value is a bit of mistery - needs a dig into NPB)
      case I_8266_DM_NEO_3    : // fallthrough
      case I_8266_DM_400_3    : // fallthrough
      case I_8266_DM_TM2_3    : // fallthrough
      case I_8266_DM_APA106_3 : // fallthrough
      case I_8266_DM_TM1914_3 : size *= 5;                   break;
      case I_8266_DM_NEO_4    : // fallthrough
      case I_8266_DM_TM1_4    : size = (size + count)*5;     break;
      case I_8266_DM_UCS_3    : size *= 2*5;                 break;
      case I_8266_DM_UCS_4    : size = (size + count)*2*5;   break;
      case I_8266_DM_FW6_5    : // fallthrough
      case I_8266_DM_2805_5   : size = (size + 2*count)*5;   break;
      case I_8266_DM_SM16825_5: size = (size + 2*count)*2*5; break;
    #else
      // RMT buses (1x front and 1x back buffer, does not include small RMT buffer)
      case I_32_RN_NEO_4    : // fallthrough
      case I_32_RN_TM1_4    : size = (size + count)*2;     break; // 4 channels
      case I_32_RN_UCS_3    : size *= 2*2;                 break; // 16bit
      case I_32_RN_UCS_4    : size = (size + count)*2*2;   break; // 16bit, 4 channels
      case I_32_RN_FW6_5    : // fallthrough
      case I_32_RN_2805_5   : size = (size + 2*count)*2;   break; // 5 channels
      case I_32_RN_SM16825_5: size = (size + 2*count)*2*2; break; // 16bit, 5 channels
      // I2S1 bus or paralell I2S1 buses (1x front, does not include DMA buffer which is front*cadence, a bit(?) more for LCD)
      #ifndef CONFIG_IDF_TARGET_ESP32C3
      case I_32_I2_NEO_3    : // fallthrough
      case I_32_I2_400_3    : // fallthrough
      case I_32_I2_TM2_3    : // fallthrough
      case I_32_I2_APA106_3 :                              break; // do nothing, I2S uses single buffer + DMA buffer
      case I_32_I2_NEO_4    : // fallthrough
      case I_32_I2_TM1_4    : size = (size + count);       break; // 4 channels
      case I_32_I2_UCS_3    : size *= 2;                   break; // 16 bit
      case I_32_I2_UCS_4    : size = (size + count)*2;     break; // 16 bit, 4 channels
      case I_32_I2_FW6_5    : // fallthrough
      case I_32_I2_2805_5   : size = (size + 2*count);     break; // 5 channels
      case I_32_I2_SM16825_5: size = (size + 2*count)*2;   break; // 16 bit, 5 channels
      #endif
      default               : size *= 2;                   break; // everything else uses 2 buffers
    #endif
    }
    return size;
  }

  //gives back the internal type index (I_XX_XXX_X above) for the input
  static uint8_t getI(uint8_t busType, const uint8_t* pins, uint8_t num = 0) {
    if (!Bus::isDigital(busType)) return I_NONE;
    if (Bus::is2Pin(busType)) { //SPI LED chips
      bool isHSPI = false;
      #ifdef ESP8266
      if (pins[0] == P_8266_HS_MOSI && pins[1] == P_8266_HS_CLK) isHSPI = true;
      #else
      // temporary hack to limit use of hardware SPI to a single SPI peripheral (HSPI): only allow ESP32 hardware serial on segment 0
      // SPI global variable is normally linked to VSPI on ESP32 (or FSPI C3, S3)
      if (!num) isHSPI = true;
      #endif
      uint8_t t = I_NONE;
      switch (busType) {
        case TYPE_APA102:  t = I_SS_DOT_3; break;
        case TYPE_LPD8806: t = I_SS_LPD_3; break;
        case TYPE_LPD6803: t = I_SS_LPO_3; break;
        case TYPE_WS2801:  t = I_SS_WS1_3; break;
        case TYPE_P9813:   t = I_SS_P98_3; break;
        default: t=I_NONE;
      }
      if (t > I_NONE && isHSPI) t--; //hardware SPI has one smaller ID than software
      return t;
    } else {
      #ifdef ESP8266
      uint8_t offset = pins[0] -1; //for driver: 0 = uart0, 1 = uart1, 2 = dma, 3 = bitbang
      if (offset > 3) offset = 3;
      switch (busType) {
        case TYPE_WS2812_1CH_X3:
        case TYPE_WS2812_2CH_X3:
        case TYPE_WS2812_RGB:
        case TYPE_WS2812_WWA:
          return I_8266_U0_NEO_3 + offset;
        case TYPE_SK6812_RGBW:
          return I_8266_U0_NEO_4 + offset;
        case TYPE_WS2811_400KHZ:
          return I_8266_U0_400_3 + offset;
        case TYPE_TM1814:
          return I_8266_U0_TM1_4 + offset;
        case TYPE_TM1829:
          return I_8266_U0_TM2_3 + offset;
        case TYPE_UCS8903:
          return I_8266_U0_UCS_3 + offset;
        case TYPE_UCS8904:
          return I_8266_U0_UCS_4 + offset;
        case TYPE_APA106:
          return I_8266_U0_APA106_3 + offset;
        case TYPE_FW1906:
          return I_8266_U0_FW6_5 + offset;
        case TYPE_WS2805:
          return I_8266_U0_2805_5 + offset;
        case TYPE_TM1914:
          return I_8266_U0_TM1914_3 + offset;
        case TYPE_SM16825:
          return I_8266_U0_SM16825_5 + offset;
      }
      #else //ESP32
      uint8_t offset = 0; // 0 = RMT (num 1-8), 1 = I2S1 [I2S0 is used by Audioreactive]
      #if defined(CONFIG_IDF_TARGET_ESP32S2)
      // ESP32-S2 only has 4 RMT channels
      if (_useParallelI2S) {
        if (num > 11) return I_NONE;
        if (num < 8) offset = 1;    // use x8 parallel I2S0 channels followed by RMT
                                    // Note: conflicts with AudioReactive if enabled
      } else {
        if (num > 4) return I_NONE;
        if (num > 3) offset = 1;  // only one I2S0 (use last to allow Audioreactive)
      }
      #elif defined(CONFIG_IDF_TARGET_ESP32C3)
      // On ESP32-C3 only the first 2 RMT channels are usable for transmitting
      if (num > 1) return I_NONE;
      //if (num > 1) offset = 1; // I2S not supported yet (only 1 I2S)
      #elif defined(CONFIG_IDF_TARGET_ESP32S3)
      // On ESP32-S3 only the first 4 RMT channels are usable for transmitting
      if (_useParallelI2S) {
        if (num > 11) return I_NONE;
        if (num < 8) offset = 1;    // use x8 parallel I2S LCD channels, followed by RMT
      } else {
        if (num > 3) return I_NONE; // do not use single I2S (as it is not supported)
      }
      #else
      // standard ESP32 has 8 RMT and x1/x8 I2S1 channels
      if (_useParallelI2S) {
        if (num > 15) return I_NONE;
        if (num < 8) offset = 1;  // 8 I2S followed by 8 RMT
      } else {
        if (num > 9) return I_NONE;
        if (num == 0) offset = 1; // prefer I2S1 for 1st bus (less flickering but more RAM needed)
      }
      #endif
      switch (busType) {
        case TYPE_WS2812_1CH_X3:
        case TYPE_WS2812_2CH_X3:
        case TYPE_WS2812_RGB:
        case TYPE_WS2812_WWA:
          return I_32_RN_NEO_3 + offset;
        case TYPE_SK6812_RGBW:
          return I_32_RN_NEO_4 + offset;
        case TYPE_WS2811_400KHZ:
          return I_32_RN_400_3 + offset;
        case TYPE_TM1814:
          return I_32_RN_TM1_4 + offset;
        case TYPE_TM1829:
          return I_32_RN_TM2_3 + offset;
        case TYPE_UCS8903:
          return I_32_RN_UCS_3 + offset;
        case TYPE_UCS8904:
          return I_32_RN_UCS_4 + offset;
        case TYPE_APA106:
          return I_32_RN_APA106_3 + offset;
        case TYPE_FW1906:
          return I_32_RN_FW6_5 + offset;
        case TYPE_WS2805:
          return I_32_RN_2805_5 + offset;
        case TYPE_TM1914:
          return I_32_RN_TM1914_3 + offset;
        case TYPE_SM16825:
          return I_32_RN_SM16825_5 + offset;
      }
      #endif
    }
    return I_NONE;
  }
};
#endif
