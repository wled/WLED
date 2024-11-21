#if !defined(WLEDFX_MATH_H)

#include <Arduino.h> //PI constant
#include <math.h>

//#define WLED_DEBUG_MATH
#undef modd
#define modd(x, y) ((x) - (int)((x) / (y)) * (y))

// 16-bit, integer based Bhaskara I's sine approximation: 16*x*(pi - x) / (5*pi^2 - 4*x*(pi - x))
// input is 16bit unsigned (0-65535), output is 16bit signed (-32767 to +32767)
// optimized integer implementation by @dedehai
inline static int16_t sin16_t(uint16_t theta) {
  int scale = 1;
  if (theta > 0x7FFF) {
    theta = 0xFFFF - theta;
    scale = -1; // second half of the sine function is negative (pi - 2*pi)
  }
  uint32_t precal = theta * (0x7FFF - theta);
  uint64_t numerator = (uint64_t)precal * (4 * 0x7FFF); // 64bit required
  int32_t denominator = 1342095361 - precal; // 1342095361 is 5 * 0x7FFF^2 / 4
  int16_t result = numerator / denominator;
  return result * scale;
}

inline static int16_t cos16_t(uint16_t theta) {
  return sin16_t(theta + 16384); //cos(x) = sin(x+pi/2)
}

#if 0
inline static uint8_t sin8_t(uint8_t theta) {
  int32_t sin16 = sin16_t((uint16_t)theta * 257); // 255 * 257 = 0xFFFF
  sin16 += 0x7FFF; //shift result to range 0-0xFFFF
  //return sin16 >> 8;
  uint32_t usin16 = sin16; // re-interpret as unsigned
  //if (usin16 < 65408) 
    usin16 += 127; // perform rounding (avoid overflow)
  return min(usin16,uint32_t(0xFFFF)) >> 8; // min performs saturation, and prevents overflow
}
#else
inline static uint8_t sin8_t(uint8_t theta) {
  int32_t sin16 = sin16_t((uint16_t)theta * 257); // 255 * 257 = 0xFFFF
  sin16 += 0x7FFF + 128; //shift result to range 0-0xFFFF, +128 for rounding
  return min(sin16, int32_t(0xFFFF)) >> 8; // min performs saturation, and prevents overflow
}
#endif

inline static uint8_t cos8_t(uint8_t theta) {
  return sin8_t(theta + 64); //cos(x) = sin(x+pi/2) // note may roll over - this is no problem here
}

inline static float sin_approx(float theta)
{
  theta = modd(theta, float(TWO_PI)); // modulo: bring to -2pi to 2pi range
  if(theta < 0) theta += float(M_TWOPI); // 0-2pi range
  uint16_t scaled_theta = (uint16_t)(theta * (0xFFFF / float(M_TWOPI)));
  int32_t result = sin16_t(scaled_theta);
  float sin = float(result) / 0x7FFF;
  return sin;
}

inline static float cos_approx(float theta)
{
  return sin_approx(theta + float(M_PI_2));
}

inline static float tan_approx(float x) {
  float c = cos_approx(x);
  if (c==0.0f) return 0;
  float res = sin_approx(x) / c;
  return res;
}

#define ATAN2_CONST_A 0.1963f
#define ATAN2_CONST_B 0.9817f
// atan2_t approximation, with the idea from https://gist.github.com/volkansalma/2972237?permalink_comment_id=3872525#gistcomment-3872525
inline static float atan2_t(float y, float x) {
  float abs_y = fabs(y);
  float abs_x = fabs(x);
  float r = (abs_x - abs_y) / (abs_y + abs_x + 1e-10f); // avoid division by zero by adding a small nubmer
  float angle;
  if(x < 0) {
    r = -r;
    angle = float(M_PI)/2.0f + float(M_PI)/4.f;
  }
  else
    angle = float(M_PI)/2.0f - float(M_PI)/4.f;

  float add = (ATAN2_CONST_A * (r * r) - ATAN2_CONST_B) * r;
	angle += add;
  angle = y < 0 ? -angle : angle;
	return angle;
}


// fastled beatsin: 1:1 replacements to remove the use of fastled sin16()
// Generates a 16-bit sine wave at a given BPM that oscillates within a given range. see fastled for details.
inline static uint16_t beatsin88_t(accum88 beats_per_minute_88, uint16_t lowest = 0, uint16_t highest = 65535, uint32_t timebase = 0, uint16_t phase_offset = 0)
{
    uint16_t beat = beat88( beats_per_minute_88, timebase);
    uint16_t beatsin (sin16_t( beat + phase_offset) + 32768);
    uint16_t rangewidth = highest - lowest;
    uint16_t scaledbeat = scale16( beatsin, rangewidth);
    uint16_t result = lowest + scaledbeat;
    return result;
}

// Generates a 16-bit sine wave at a given BPM that oscillates within a given range. see fastled for details.
inline static uint16_t beatsin16_t(accum88 beats_per_minute, uint16_t lowest = 0, uint16_t highest = 65535, uint32_t timebase = 0, uint16_t phase_offset = 0)
{
    uint16_t beat = beat16( beats_per_minute, timebase);
    uint16_t beatsin = (sin16_t( beat + phase_offset) + 32768);
    uint16_t rangewidth = highest - lowest;
    uint16_t scaledbeat = scale16( beatsin, rangewidth);
    uint16_t result = lowest + scaledbeat;
    return result;
}

// Generates an 8-bit sine wave at a given BPM that oscillates within a given range. see fastled for details.
inline static uint8_t beatsin8_t(accum88 beats_per_minute, uint8_t lowest = 0, uint8_t highest = 255, uint32_t timebase = 0, uint8_t phase_offset = 0)
{
    uint8_t beat = beat8( beats_per_minute, timebase);
    uint8_t beatsin = sin8_t( beat + phase_offset);
    uint8_t rangewidth = highest - lowest;
    uint8_t scaledbeat = scale8( beatsin, rangewidth);
    uint8_t result = lowest + scaledbeat;
    return result;
}

#if !defined(ColorFromPalette) // don't overwrite our own overwrite
// 1:1 replacement of fastled function optimized for ESP, slightly faster, more accurate and uses less flash (~ -200bytes)
static inline CRGB ColorFromPaletteWLED(const CRGBPalette16& pal, unsigned index, uint8_t brightness=255, TBlendType blendType=LINEARBLEND)
{
   if (blendType == LINEARBLEND_NOWRAP) {
     index = (index*240) >> 8; // Blend range is affected by lo4 blend of values, remap to avoid wrapping
  }
    unsigned hi4 = byte(index) >> 4;
    const CRGB* entry = (CRGB*)( (uint8_t*)(&(pal[0])) + (hi4 * sizeof(CRGB)));
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
    return CRGB(red1,green1,blue1);
}
#endif

#define WLEDFX_MATH_H
#endif