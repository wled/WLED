#include "fastled_fcn.h"

// Code originally from FastLED version 3.6.0. Optimized for WLED use by @dedehai
// Licensed unter MIT license, see LICENSE.txt for details

// convert HSV (16bit hue) to RGB (24bit), optimized for speed (integer types and function arguments were very carefully selected for best performance)
// this does the same as the FastLED hsv2rgb_rainbow function but with 16bit hue and optimizations for use with CRGB as well as CRGBW
// note: this function is used when converting CHSV->CRGB or CHSV32->CRGBW by assignment or constructor, there is no need to call it explicitly
__attribute__((optimize("O2"))) void hsv2rgb_rainbow(uint16_t h, uint8_t s, uint8_t v, uint8_t* rgbdata, bool isRGBW) {
  uint8_t hue = h>>8;
  uint8_t sat = s;
  uint32_t val = v;
  uint32_t offset = h & 0x1FFF; // 0..31
  uint32_t third16 = (offset * 21846); // offset16 = offset * 1/3<<16
  uint8_t third = third16 >> 21; // max = 85
  uint8_t r, g, b; // note: making these 32bit is significantly slower

  if (!(hue & 0x80)) {
    if (!(hue & 0x40)) { // section 0-1
      if (!(hue & 0x20)) {
        r = 255 - third;
        g = third;
        b = 0;
      } else {
        r = 171;
        g = 85 + third;
        b = 0;
      }
    } else { // section 2-3
      if (!(hue & 0x20)) {
        uint8_t twothirds = third16 >> 20; // max=170
        r = 171 - twothirds;
        g = 170 + third;
        b = 0;
      } else {
        r = 0;
        g = 255 - third;
        b = third;
      }
    }
  } else { // section 4-7
    if (!(hue & 0x40)) {
      if (!(hue & 0x20)) {
        r = 0;
        uint8_t twothirds = third16 >> 20; // max=170
        g = 171 - twothirds;
        b = 85 + twothirds;
      } else {
        r = third;
        g = 0;
        b = 255 - third;
      }
    } else {
      if (!(hue & 0x20)) {
        r = 85 + third;
        g = 0;
        b = 171 - third;
      } else {
        r = 170 + third;
        g = 0;
        b = 85 - third;
      }
    }
  }

  // scale down colors if desaturated and add the brightness_floor to r, g, and b.
  if (sat != 255) {
    if (sat == 0) {
      r = 255;
      g = 255;
      b = 255;
    } else {
      //we know sat is < 255 and > 1, lets use that: scale8video is always +1, so drop the conditional
      uint32_t desat = 255 - sat;
      desat = (desat * desat); // scale8_video(desat, desat) but more accurate, dropped the "+1" for speed: visual difference is negligible
      uint8_t brightness_floor = desat >> 8;
      uint32_t satscale = 0xFFFF - desat;
      if (r) r = ((r * satscale) >> 16);
      if (g) g = ((g * satscale) >> 16);
      if (b) b = ((b * satscale) >> 16);

      r += brightness_floor;
      g += brightness_floor;
      b += brightness_floor;
    }
  }

  // scale everything down if value < 255.
  if (val != 255) {
    if (val == 0) {
      r = 0;
      g = 0;
      b = 0;
    } else {
       val = val*val + 512; // = scale8_video(val,val)+2;
      if (r) r = ((r * val) >> 16) + 1;
      if (g) g = ((g * val) >> 16) + 1;
      if (b) b = ((b * val) >> 16) + 1;
    }
  }
  if(isRGBW) {
    rgbdata[0] = b;
    rgbdata[1] = g;
    rgbdata[2] = r;
    //rgbdata[3] = 0; // white
  } else {
    rgbdata[0] = r;
    rgbdata[1] = g;
    rgbdata[2] = b;
  }
}


// black body radiation to RGB
CRGB HeatColor(uint8_t temperature) {
    CRGB heatcolor;
    uint8_t t192 = (((int)temperature * 191) >> 8) + (temperature ? 1 : 0); // scale down, but keep 1 as minimum
    // calculate a value that ramps up from zero to 255 in each 'third' of the scale.
    uint8_t heatramp = t192 & 0x3F; // 0..63
    heatramp <<= 2; // scale up to 0..252
    heatcolor.r = 255;
    heatcolor.b = 0;
    if(t192 & 0x80) { // we're in the hottest third
        heatcolor.g = 255; // full green
        heatcolor.b = heatramp; // ramp up blue
    } else if(t192 & 0x40) { // we're in the middle third
        heatcolor.g = heatramp; // ramp up green
    } else { // we're in the coolest third
        heatcolor.r = heatramp; // ramp up red
        heatcolor.g = 0; // no green
    }
    return heatcolor;
}

// CRGB color fill functions (from fastled, used for color palettes)
void fill_solid_RGB(CRGB* colors, uint32_t num, const CRGB& c1) {
  for(uint32_t i = 0; i < num; i++) {
    colors[i] = c1;
  }
}

// fill CRGB array with a color gradient
void fill_gradient_RGB(CRGB* colors, uint32_t startpos, CRGB startcolor, uint32_t endpos, CRGB endcolor) {
  if(endpos < startpos) { // if the points are in the wrong order, flip them
    unsigned t = endpos;
    CRGB tc = endcolor;
    endcolor = startcolor;
    endpos = startpos;
    startpos = t;
    startcolor = tc;
  }
  int rdistance = endcolor.r - startcolor.r;
  int gdistance = endcolor.g - startcolor.g;
  int bdistance = endcolor.b - startcolor.b;

  int divisor = endpos - startpos;
  divisor = divisor == 0 ? 1 : divisor; // prevent division by zero

  int rdelta = (rdistance << 16) / divisor;
  int gdelta = (gdistance << 16) / divisor;
  int bdelta = (bdistance << 16) / divisor;

  int rshifted = startcolor.r << 16;
  int gshifted = startcolor.g << 16;
  int bshifted = startcolor.b << 16;

  for (unsigned i = startpos; i <= endpos; i++) {
    colors[i] = CRGB(rshifted >> 16, gshifted >> 16, bshifted >> 16);
    rshifted += rdelta;
    gshifted += gdelta;
    bshifted += bdelta;
  }
}

void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2) {
  uint32_t last = num - 1;
  fill_gradient_RGB(colors, 0, c1, last, c2);
}

void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2, const CRGB& c3) {
  uint32_t half = (num / 2);
  uint32_t last = num - 1;
  fill_gradient_RGB(colors,    0, c1, half, c2);
  fill_gradient_RGB(colors, half, c2, last, c3);
}

void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4) {
  uint32_t onethird = (num / 3);
  uint32_t twothirds = ((num * 2) / 3);
  uint32_t last = num - 1;
  fill_gradient_RGB(colors,         0, c1,  onethird, c2);
  fill_gradient_RGB(colors,  onethird, c2, twothirds, c3);
  fill_gradient_RGB(colors, twothirds, c3,      last, c4);
}

// palette blending
void nblendPaletteTowardPalette(CRGBPalette16& current, CRGBPalette16& target, uint8_t maxChanges) {
  uint8_t* p1;
  uint8_t* p2;
  uint32_t changes = 0;
  p1 = (uint8_t*)current.entries;
  p2 = (uint8_t*)target.entries;
  const uint32_t totalChannels = sizeof(CRGBPalette16);
  for (uint32_t i = 0; i < totalChannels; ++i) {
    if (p1[i] == p2[i]) continue; // if the values are equal, no changes are needed
    if (p1[i] < p2[i]) { ++p1[i]; ++changes; } // if the current value is less than the target, increase it by one
    if (p1[i] > p2[i]) { // if the current value is greater than the target, increase it by one (or two if it's still greater).
      --p1[i]; ++changes;
      if (p1[i] > p2[i])
        --p1[i];
    }
    if(changes >= maxChanges)
      break;
  }
}

// cubic ease function (S-curve: 3x^2 - 2x^3)
// 8-bit
uint8_t ease8InOutCubic(uint8_t i) {
  uint32_t ii = ((uint32_t)i * i) >> 8;
  uint32_t factor = (3 << 8) - (((uint32_t)i << 1)); // 3 - 2i
  return (ii * factor) >> 8;
}
// 16-bit
uint16_t ease16InOutCubic(uint16_t i) {
  uint32_t ii = ((uint32_t)i * i) >> 16;
  uint32_t factor = (3 << 16) - (((uint32_t)i << 1)); // 3 - 2i
  return (ii * factor) >> 16;
}

// quadradic ease function (S-curve: x^2)
uint8_t ease8InOutQuad(uint8_t i)
{
  uint32_t j = i;
  if (j & 0x80) j = 255 - j; // mirror if > 127
  uint32_t jj = (j * j) >> 7;
  return (i & 0x80) ? (255 - jj) : jj;
}

// triangular wave generator
uint8_t triwave8(uint8_t in) {
  if (in & 0x80) in = 255 - in;
  return in << 1;
}

uint16_t triwave16(uint16_t in) {
  if (in < 0x8000) return in *2;
  return 0xFFFF - (in - 0x8000)*2;
}

// quadratic waveform generator. Spends just a little more time at the limits than "sine" does.
uint8_t quadwave8(uint8_t in) {
  return ease8InOutQuad(triwave8(in));
}

// cubic waveform generator. Spends visibly more time at the limits than "sine" does.
uint8_t cubicwave8(uint8_t in) {
  return ease8InOutCubic(triwave8(in));
}
