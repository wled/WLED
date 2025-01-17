#pragma once
#if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_FASTPATH) && !defined(WLEDMM_SAVE_FLASH)

#include "wled.h"
/*
 * Color conversion & utility methods - moved here, so the compiler may inline these functions (up to 20% faster)
 */

// WLEDMM make sure that color macros are always defined
#if !defined(RGBW32)
#define RGBW32(r,g,b,w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#endif
#if !defined(W) && !defined(R)
#define R(c) (byte((c) >> 16))
#define G(c) (byte((c) >> 8))
#define B(c) (byte(c))
#define W(c) (byte((c) >> 24))
#endif

#if !defined(FASTLED_VERSION) // pull in FastLED if we don't have it yet (we need the CRGB type)
  #define FASTLED_INTERNAL
  #include <FastLED.h>
#endif

/*
 * color blend function (from colors.cpp)
 */
inline __attribute__((hot,const)) uint32_t color_blend(uint32_t color1, uint32_t color2, uint_fast16_t blend, bool b16=false) {
  if ((color1 == color2) || (blend == 0)) return color1; // WLEDMM
  const uint_fast16_t blendmax = b16 ? 0xFFFF : 0xFF;
  if(blend >= blendmax) return color2;
  const uint_fast8_t shift = b16 ? 16 : 8;

  uint16_t w1 = W(color1); // WLEDMM 16bit to make sure the compiler uses 32bit (not 64bit) for the math
  uint16_t r1 = R(color1);
  uint16_t g1 = G(color1);
  uint16_t b1 = B(color1);

  uint16_t w2 = W(color2);
  uint16_t r2 = R(color2);
  uint16_t g2 = G(color2);
  uint16_t b2 = B(color2);

  if (b16 == false) {
    // WLEDMM based on fastled blend8() - better accuracy for 8bit
    uint8_t w3 = (w1+w2 == 0) ? 0 : (((w1 << 8)|w2) + (w2 * blend) - (w1*blend) ) >> 8;
    uint8_t r3 = (((r1 << 8)|r2) + (r2 * blend) - (r1*blend) ) >> 8;
    uint8_t g3 = (((g1 << 8)|g2) + (g2 * blend) - (g1*blend) ) >> 8;
    uint8_t b3 = (((b1 << 8)|b2) + (b2 * blend) - (b1*blend) ) >> 8;
    return RGBW32(r3, g3, b3, w3);
  } else {
    // old code has lots of "jumps" due to rounding errors
    const uint_fast16_t blend2 = blendmax - blend; // WLEDMM pre-calculate value
    uint32_t w3 = ((w2 * blend) + (w1 * blend2)) >> shift;
    uint32_t r3 = ((r2 * blend) + (r1 * blend2)) >> shift;
    uint32_t g3 = ((g2 * blend) + (g1 * blend2)) >> shift;
    uint32_t b3 = ((b2 * blend) + (b1 * blend2)) >> shift;
    return RGBW32(r3, g3, b3, w3);
  }
}

/*
 * color add function that preserves ratio (from colors.cpp)
 * idea: https://github.com/Aircoookie/WLED/pull/2465 by https://github.com/Proto-molecule
 */

inline __attribute__((hot,const)) uint32_t color_add(uint32_t c1, uint32_t c2, bool fast=false)
{
  if (c2 == 0) return c1;  // WLEDMM shortcut
  if (c1 == 0) return c2;  // WLEDMM shortcut

  if (fast) {
    uint8_t r = R(c1);
    uint8_t g = G(c1);
    uint8_t b = B(c1);
    uint8_t w = W(c1);
    r = qadd8(r, R(c2));
    g = qadd8(g, G(c2));
    b = qadd8(b, B(c2));
    w = qadd8(w, W(c2));
    return RGBW32(r,g,b,w);
  } else {
    uint32_t r = R(c1) + R(c2);
    uint32_t g = G(c1) + G(c2);
    uint32_t b = B(c1) + B(c2);
    uint32_t w = W(c1) + W(c2);
    uint32_t max = r;
    if (g > max) max = g;
    if (b > max) max = b;
    if (w > max) max = w;
    if (max < 256) return RGBW32(r, g, b, w);
    else           return RGBW32(r * 255 / max, g * 255 / max, b * 255 / max, w * 255 / max);
  }
}

/*
 * fades color toward black (from colors.cpp)
 * if using "video" method the resulting color will never become black unless it is already black
 */

inline __attribute__((hot,const)) uint32_t color_fade(uint32_t c1, uint8_t amount, bool video=false)
{
  if (amount == 255) return c1; // WLEDMM small optimization - plus it avoids over-fading in "video" mode
  if (amount == 0) return 0; // WLEDMM shortcut

  uint32_t scaledcolor = 0; // color order is: W R G B from MSB to LSB
  uint16_t w = W(c1);    // WLEDMM 16bit to make sure the compiler uses 32bit (not 64bit) for the math
  uint16_t r = R(c1);
  uint16_t g = G(c1);
  uint16_t b = B(c1);
  if (video)  {
    uint16_t scale = amount; // 32bit for faster calculation
    // bugfix: doing "+1" after shifting is obviously wrong
    // optimization: ((r && scale) ? 1 : 0) can be simplified to "if (r > 0) +1" ; if we arive here, then scale != 0 and scale < 255
    if (w>0) scaledcolor |= (((w * scale) >> 8) +1) << 24;  // WLEDMM small speedup when no white channel
    if (r>0) scaledcolor |= (((r * scale) >> 8) +1) << 16;
    if (g>0) scaledcolor |= (((g * scale) >> 8) +1) << 8;
    if (b>0) scaledcolor |=  ((b * scale) >> 8) +1;
    return scaledcolor;
  }
  else  {
    uint16_t scale = 1 + amount;
    if (w>0) scaledcolor |= ((w * scale) >> 8) << 24;                              // WLEDMM small speedup when no white channel
    scaledcolor |= ((r * scale) >> 8) << 16;
    scaledcolor |= (g * scale) & 0x0000FF00;                                       // WLEDMM faster than right-left shift "" >>8 ) <<8"
    scaledcolor |= (b * scale) >> 8;
    return scaledcolor;
  }
}

//scales the brightness with the briMultiplier factor (from led.cpp)
extern uint_fast16_t briMultiplier;  // defined in wled.h
inline __attribute__((hot,const)) byte scaledBri(byte in)  // WLEDMM added IRAM_ATTR_YN
{
  if (briMultiplier == 100) return(in); // WLEDMM shortcut
  uint_fast16_t val = ((uint_fast16_t)in*(uint_fast16_t)briMultiplier)/100; // WLEDMM
  if (val > 255) val = 255;
  return (byte)val;
}

//
// overwrite FastLed colorFromPalette with an optimized version created by dedehai (https://github.com/Aircoookie/WLED/pull/4138)
// 
// 1:1 replacement of fastled function optimized for ESP, slightly faster, more accurate and uses less flash (~ -200bytes)
// WLEDMM: converted to inline
#undef ColorFromPalette // overwrite any existing override
inline __attribute__((hot)) CRGB ColorFromPaletteWLED(const CRGBPalette16& pal, unsigned index, uint8_t brightness=255, TBlendType blendType=LINEARBLEND)
{
  if (blendType == LINEARBLEND_NOWRAP) {
    index = (index*240) >> 8; // Blend range is affected by lo4 blend of values, remap to avoid wrapping
  }
  unsigned hi4 = byte(index) >> 4;
  const CRGB* entry = (CRGB*)((uint8_t*)(&(pal[0])) + (hi4 * sizeof(CRGB)));
  unsigned red1   = entry->r;
  unsigned green1 = entry->g;
  unsigned blue1  = entry->b;
  if (blendType != NOBLEND) {
    if (hi4 == 15) entry = &(pal[0]);
    else ++entry;
    unsigned f2 = ((index & 0x0F) << 4) + 1; // +1 so we scale by 256 as a max value, then result can just be shifted by 8
    unsigned f1 = (257 - f2); // f2 is 1 minimum, so this is 256 max
    red1   = (red1 * f1 + (unsigned)entry->r * f2) >> 8;
    green1 = (green1 * f1 + (unsigned)entry->g * f2) >> 8;
    blue1  = (blue1 * f1 + (unsigned)entry->b * f2) >> 8;
  }
  if (brightness < 255) { // note: zero checking could be done to return black but that is hardly ever used so it is omitted
    uint32_t scale = brightness + 1; // adjust for rounding (bitshift)
    red1   = (red1 * scale) >> 8;
    green1 = (green1 * scale) >> 8;
    blue1  = (blue1 * scale) >> 8;
  }
  return RGBW32(red1,green1,blue1,0);
}
#define ColorFromPalette ColorFromPaletteWLED // override fastled function

#endif
