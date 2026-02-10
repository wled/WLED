/*
  WLED board capabilities: uses CONFIG_IDF_TARGET_... to extract board capability flags.
  You can still use CONFIG_IDF_TARGET_ in the sourc code; this file provides shortcuts for repeating capability checks.
*/
#pragma once

#ifndef WLED_BOARDS_H
#define WLED_BOARDS_H
#include "NodeStruct.h"  // to get generic NODE_TYPE_IDs

/*
 * Structure: the Below part uses isolated "#if defined()" instead of a chain of if ... elif ... else
 * so that the constant WLED_BOARD will be set exactly once unless there are conflicting build_flags. 
 * In case that several blocks are active, This causes a compiler error to easily spot the problem
 * "error: redefinition of 'constexpr const unsigned int WLED_BOARD'"
 */

/* TODO: add
    WLED_HAVE_TOUCH (for button.cpp)
    WLED_HAVE_I2S0_LEDS (for bus_wrapper.h)
    WLED_HAVE_I2S1_LEDS (for bus_wrapper.h)
    (find a name) // On ESP32-C3/C5/C6 only the first 2 RMT channels are usable for transmitting (bus_wrapper.h)
    (find a name) // ESP32, S3, P4 can use SparkFunDMX (wled.h, dmx_output.cpp)
    WLED_ALLOW_LOLIN_WIFI_FIX // (wled.h)
 */


#if defined(ESP8266)
// Capabilities of good-old 8266
  // has no FPU
  // bitshift with rounding is faster than integer division
  // no byte-accessible fast RTC memory (newer esp32 variants only)
  // no parallel I2S LEDs driver
constexpr unsigned WLED_BOARD = NODE_TYPE_ID_ESP8266;

  // sanity check for esp32
  #if defined(ARDUINO_ARCH_ESP32) || defined(ESP32)
    #error "ARDUINO_ARCH_ESP32 or ESP32 is defined together with ESP8266. Please fix your buildenv."
  #endif
#endif

#if CONFIG_IDF_TARGET_ESP32
// Capabilities of classic ESP32 and classic PICO-D4/D2/D3
  #define WLED_HAVE_FAST_FLOAT  1     // has an FPU for fast floating point (single precision)
  #define WLED_HAVE_FAST_int_DIVIDE 1 // integer division is fast (no need to use bitshifts with rounding instead of integer division
  // no byte-accessible fast RTC memory (newer esp32 variants only)
  #define WLED_HAVE_IRAM_32BIT_HEAP   // only classic ESP32 has "32bit accessible only" aka IRAM type heap

  #define WLED_HAS_PARALLEL_I2S  1    // classic esp32 has I2S parallel leds driver (NeoPixelBus)

  constexpr unsigned WLED_BOARD = NODE_TYPE_ID_ESP32;
  // sanity checks
  #if (SOC_CPU_CORES_NUM < 2)
    #error "ESP32 single-core is not supported."
  #endif
#endif

#if CONFIG_IDF_TARGET_ESP32S3
// Capabilities of ESP32-S3
  #define WLED_HAVE_FAST_FLOAT  1     // has an FPU for fast floating point (single precision)
  #define WLED_HAVE_FAST_int_DIVIDE 1 // integer division is fast (no need to use bitshifts with rounding instead of integer division
  #define WLED_HAVE_RTC_MEMORY_HEAP   // has byte-accessible fast RTC memory that can be used as heap
  // no 4byte-accessible IRAM heap

  #define WLED_HAS_PARALLEL_I2S  1    // esp32-S3 supports I2S parallel leds driver (NeoPixelBus)

  constexpr unsigned WLED_BOARD = NODE_TYPE_ID_ESP32S3;
  // sanity checks
  #if (SOC_CPU_CORES_NUM < 2)
    #error "ESP32-S3 single-core is not supported."
  #endif
#endif

#if CONFIG_IDF_TARGET_ESP32S2
// Capabilities of ESP32-S2
  // has no FPU
  #define WLED_HAVE_FAST_int_DIVIDE 1 // integer division is fast (no need to use bitshifts with rounding instead of integer division
  #define WLED_HAVE_RTC_MEMORY_HEAP   // has byte-accessible fast RTC memory that can be used as heap
  // no 4byte-accessible IRAM heap

  #define WLED_HAS_PARALLEL_I2S 1    // esp32-S3 supports I2S parallel leds driver (NeoPixelBus)

  constexpr unsigned WLED_BOARD = NODE_TYPE_ID_ESP32S3;
#endif

#if CONFIG_IDF_TARGET_ESP32C3
// Capabilities of ESP32-C3
  // has no FPU
  // bitshift with rounding is faster than integer division
  #define WLED_HAVE_RTC_MEMORY_HEAP   // has byte-accessible fast RTC memory that can be used as heap
  // no 4byte-accessible IRAM heap
  // no parallel I2S LEDs driver

  constexpr unsigned WLED_BOARD = NODE_TYPE_ID_ESP32C3;
#endif

#if CONFIG_IDF_TARGET_ESP32C5
// Capabilities of ESP32-C5
  // has no FPU
  // bitshift with rounding is faster than integer division
  #define WLED_HAVE_RTC_MEMORY_HEAP   // has byte-accessible fast RTC memory that can be used as heap
  // no 4byte-accessible IRAM heap
  // no parallel I2S LEDs driver

  constexpr unsigned WLED_BOARD = NODE_TYPE_ID_ESP32C5;
#endif

#if CONFIG_IDF_TARGET_ESP32C6
// Capabilities of ESP32-C6
  // has no FPU
  // bitshift with rounding is faster than integer division
  #define WLED_HAVE_RTC_MEMORY_HEAP   // has byte-accessible fast RTC memory that can be used as heap
  // no 4byte-accessible IRAM heap
  // no parallel I2S LEDs driver

  constexpr unsigned WLED_BOARD = NODE_TYPE_ID_ESP32C6;
#endif

#if CONFIG_IDF_TARGET_ESP32C61
// Capabilities of ESP32-C61
  // has no FPU
  // bitshift with rounding is faster than integer division
  #define WLED_HAVE_RTC_MEMORY_HEAP   // has byte-accessible fast RTC memory that can be used as heap
  // no 4byte-accessible IRAM heap
  // no parallel I2S LEDs driver

  constexpr unsigned WLED_BOARD = NODE_TYPE_ID_ESP32C61;
#endif

#if CONFIG_IDF_TARGET_ESP32P4
// Capabilities of ESP32-P4
  #define WLED_HAVE_FAST_FLOAT  1     // has an FPU for fast floating point (single precision)
  // TBC: is bitshift with rounding is faster than integer division ?
  #define WLED_HAVE_FAST_int_DIVIDE 1 // integer division is fast (no need to use bitshifts with rounding instead of integer division
  #define WLED_HAVE_RTC_MEMORY_HEAP   // TBC: does it have byte-accessible fast RTC memory that can be used as heap ?
  // no 4byte-accessible IRAM heap
  // no parallel I2S LEDs driver

  constexpr unsigned WLED_BOARD = NODE_TYPE_ID_ESP32P4;

  // sanity checks
  #if (SOC_CPU_CORES_NUM < 2)
    #error "ESP32-P4 single-core is not supported."
  #endif
#endif


// sanity check: the constexpr assignment below will fail when WLED_BOARD is not set by the previous blocks
constexpr unsigned wled_boards_sanity_check = 0 + WLED_BOARD;

#endif // WLED_BOARDS_H