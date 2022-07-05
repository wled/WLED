#pragma once

#include "wled.h"

uint8_t scaled16to8( uint16_t v, uint16_t lowest=0, uint16_t highest=65535) {
  uint16_t rangewidth = highest - lowest;
  uint16_t scaledbeat = scale16( v, rangewidth );
  uint16_t result = lowest + scaledbeat;
  return result;
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

#define __ESP32__
#define USTD_OPTION_FS_FORCE_NO_FS
#include <ustd_array.h>