#include "wled.h"
#include "fcn_declare.h"
//#include "colors.h" // todo: needed? its already included in fcn declare -> seems to compile fine without this include -> remove?
/*
 * Color conversion & utility methods
 */

/*
 * color blend function, based on FastLED blend function
 * the calculation for each color is: result = (A*(amountOfA) + A + B*(amountOfB) + B) / 256 with amountOfA = 255 - amountOfB
 */
uint32_t color_blend(uint32_t color1, uint32_t color2, uint8_t blend) {
  // min / max blend checking is omitted: calls with 0 or 255 are rare, checking lowers overall performance
  uint32_t rb1 = color1 & 0x00FF00FF;
  uint32_t wg1 = (color1>>8) & 0x00FF00FF;
  uint32_t rb2 = color2 & 0x00FF00FF;
  uint32_t wg2 = (color2>>8) & 0x00FF00FF;
  uint32_t rb3 = ((((rb1 << 8) | rb2) + (rb2 * blend) - (rb1 * blend)) >> 8) & 0x00FF00FF;
  uint32_t wg3 = ((((wg1 << 8) | wg2) + (wg2 * blend) - (wg1 * blend))) & 0xFF00FF00;
  return rb3 | wg3;
}

/*
 * color add function that preserves ratio
 * original idea: https://github.com/wled-dev/WLED/pull/2465 by https://github.com/Proto-molecule
 * speed optimisations by @dedehai
 */
uint32_t color_add(uint32_t c1, uint32_t c2, bool preserveCR)
{
  if (c1 == BLACK) return c2;
  if (c2 == BLACK) return c1;
  uint32_t rb = (c1 & 0x00FF00FF) + (c2 & 0x00FF00FF); // mask and add two colors at once
  uint32_t wg = ((c1>>8) & 0x00FF00FF) + ((c2>>8) & 0x00FF00FF);
  uint32_t r = rb >> 16; // extract single color values
  uint32_t b = rb & 0xFFFF;
  uint32_t w = wg >> 16;
  uint32_t g = wg & 0xFFFF;

  if (preserveCR) { // preserve color ratios
    uint32_t max = std::max(r,g); // check for overflow note
    max = std::max(max,b);
    max = std::max(max,w);
    //unsigned max = r; // check for overflow note
    //max = g > max ? g : max;
    //max = b > max ? b : max;
    //max = w > max ? w : max;
    if (max > 255) {
      uint32_t scale = (uint32_t(255)<<8) / max; // division of two 8bit (shifted) values does not work -> use bit shifts and multiplaction instead
      rb = ((rb * scale) >> 8) & 0x00FF00FF; //
      wg = (wg * scale) & 0xFF00FF00;
    } else wg = wg << 8; //shift white and green back to correct position
    return rb | wg;
  } else {
    r = r > 255 ? 255 : r;
    g = g > 255 ? 255 : g;
    b = b > 255 ? 255 : b;
    w = w > 255 ? 255 : w;
    return RGBW32(r,g,b,w);
  }
}

/*
 * fades color toward black
 * if using "video" method the resulting color will never become black unless it is already black
 */

uint32_t color_fade(uint32_t c1, uint8_t amount, bool video)
{
  if (amount == 255) return c1;
  if (c1 == BLACK || amount == 0) return BLACK;
  uint32_t scaledcolor; // color order is: W R G B from MSB to LSB
  uint32_t scale = amount; // 32bit for faster calculation
  uint32_t addRemains = 0;
  if (!video) scale++; // add one for correct scaling using bitshifts
  else { // video scaling: make sure colors do not dim to zero if they started non-zero
    addRemains  = R(c1) ? 0x00010000 : 0;
    addRemains |= G(c1) ? 0x00000100 : 0;
    addRemains |= B(c1) ? 0x00000001 : 0;
    addRemains |= W(c1) ? 0x01000000 : 0;
  }
  uint32_t rb = (((c1 & 0x00FF00FF) * scale) >> 8) & 0x00FF00FF; // scale red and blue
  uint32_t wg = (((c1 & 0xFF00FF00) >> 8) * scale) & 0xFF00FF00; // scale white and green
  scaledcolor = (rb | wg) + addRemains;
  return scaledcolor;
}

// 1:1 replacement of fastled function optimized for ESP, slightly faster, more accurate and uses less flash (~ -200bytes)
uint32_t ColorFromPaletteWLED(const CRGBPalette16& pal, unsigned index, uint8_t brightness, TBlendType blendType)
{
  if (blendType == LINEARBLEND_NOWRAP) {
    index = (index * 0xF0) >> 8; // Blend range is affected by lo4 blend of values, remap to avoid wrapping
  }
  unsigned hi4 = byte(index) >> 4;
  unsigned lo4 = (index & 0x0F);
  const CRGB* entry = (CRGB*)&(pal[0]) + hi4;
  unsigned red1   = entry->r;
  unsigned green1 = entry->g;
  unsigned blue1  = entry->b;
  if(lo4 && blendType != NOBLEND) {
    if (hi4 == 15) entry = &(pal[0]);
    else ++entry;
    unsigned f2 = (lo4 << 4);
    unsigned f1 = 256 - f2;
    red1   = (red1 * f1 + (unsigned)entry->r * f2) >> 8; // note: using color_blend() is 20% slower
    green1 = (green1 * f1 + (unsigned)entry->g * f2) >> 8;
    blue1  = (blue1 * f1 + (unsigned)entry->b * f2) >> 8;
  }
  if (brightness < 255) { // note: zero checking could be done to return black but that is hardly ever used so it is omitted
    uint32_t scale = brightness + 1; // adjust for rounding (bitshift)
    red1   = (red1 * scale) >> 8; // note: using color_fade() is 30% slower
    green1 = (green1 * scale) >> 8;
    blue1  = (blue1 * scale) >> 8;
  }
  return RGBW32(red1,green1,blue1,0);
}

void setRandomColor(byte* rgb)
{
  lastRandomIndex = get_random_wheel_index(lastRandomIndex);
  colorHStoRGB(lastRandomIndex*256,255,rgb);
}

/*
 * generates a random palette based on harmonic color theory
 * takes a base palette as the input, it will choose one color of the base palette and keep it
 */
CRGBPalette16 generateHarmonicRandomPalette(const CRGBPalette16 &basepalette)
{
  CHSV palettecolors[4]; // array of colors for the new palette
  uint8_t keepcolorposition = hw_random8(4); // color position of current random palette to keep
  palettecolors[keepcolorposition] = rgb2hsv(basepalette.entries[keepcolorposition*5]); // read one of the base colors of the current palette
  palettecolors[keepcolorposition].hue += hw_random8(10)-5; // +/- 5 randomness of base color
  // generate 4 saturation and brightness value numbers
  // only one saturation is allowed to be below 200 creating mostly vibrant colors
  // only one brightness value number is allowed below 200, creating mostly bright palettes

  for (int i = 0; i < 3; i++) { // generate three high values
    palettecolors[i].saturation = hw_random8(200,255);
    palettecolors[i].value = hw_random8(220,255);
  }
  // allow one to be lower
  palettecolors[3].saturation = hw_random8(20,255);
  palettecolors[3].value = hw_random8(80,255);

  // shuffle the arrays
  for (int i = 3; i > 0; i--) {
    std::swap(palettecolors[i].saturation, palettecolors[hw_random8(i + 1)].saturation);
    std::swap(palettecolors[i].value, palettecolors[hw_random8(i + 1)].value);
  }

  // now generate three new hues based off of the hue of the chosen current color
  uint8_t basehue = palettecolors[keepcolorposition].hue;
  uint8_t harmonics[3]; // hues that are harmonic but still a little random
  uint8_t type = hw_random8(5); // choose a harmony type

  switch (type) {
    case 0: // analogous
      harmonics[0] = basehue + hw_random8(30, 50);
      harmonics[1] = basehue + hw_random8(10, 30);
      harmonics[2] = basehue - hw_random8(10, 30);
      break;

    case 1: // triadic
      harmonics[0] = basehue + 113 + hw_random8(15);
      harmonics[1] = basehue + 233 + hw_random8(15);
      harmonics[2] = basehue -   7 + hw_random8(15);
      break;

    case 2: // split-complementary
      harmonics[0] = basehue + 145 + hw_random8(10);
      harmonics[1] = basehue + 205 + hw_random8(10);
      harmonics[2] = basehue -   5 + hw_random8(10);
      break;

    case 3: // square
      harmonics[0] = basehue +  85 + hw_random8(10);
      harmonics[1] = basehue + 175 + hw_random8(10);
      harmonics[2] = basehue + 265 + hw_random8(10);
     break;

    case 4: // tetradic
      harmonics[0] = basehue +  80 + hw_random8(20);
      harmonics[1] = basehue + 170 + hw_random8(20);
      harmonics[2] = basehue -  15 + hw_random8(30);
     break;
  }

  if (hw_random8() < 128) {
    // 50:50 chance of shuffling hues or keep the color order
    for (int i = 2; i > 0; i--) {
      std::swap(harmonics[i], harmonics[hw_random8(i + 1)]);
    }
  }

  // now set the hues
  int j = 0;
  for (int i = 0; i < 4; i++) {
    if (i==keepcolorposition) continue; // skip the base color
    palettecolors[i].hue = harmonics[j];
    j++;
  }

  bool makepastelpalette = false;
  if (hw_random8() < 25) { // ~10% chance of desaturated 'pastel' colors
    makepastelpalette = true;
  }

  // apply saturation & gamma correction
  CRGB RGBpalettecolors[4];
  for (int i = 0; i < 4; i++) {
    if (makepastelpalette && palettecolors[i].saturation > 180) {
      palettecolors[i].saturation -= 160; //desaturate all four colors
    }
    RGBpalettecolors[i] = (CRGB)palettecolors[i]; //convert to RGB
    RGBpalettecolors[i] = gamma32(((uint32_t)RGBpalettecolors[i]) & 0x00FFFFFFU); //strip alpha from CRGB
  }

  return CRGBPalette16(RGBpalettecolors[0],
                       RGBpalettecolors[1],
                       RGBpalettecolors[2],
                       RGBpalettecolors[3]);
}

CRGBPalette16 generateRandomPalette()  // generate fully random palette
{
  return CRGBPalette16(CHSV(hw_random8(), hw_random8(160, 255), hw_random8(128, 255)),
                       CHSV(hw_random8(), hw_random8(160, 255), hw_random8(128, 255)),
                       CHSV(hw_random8(), hw_random8(160, 255), hw_random8(128, 255)),
                       CHSV(hw_random8(), hw_random8(160, 255), hw_random8(128, 255)));
}

// convert HSV (16bit hue) to RGB (32bit with white = 0), optimized for speed
//__attribute__((optimize("O3"))) 
void hsv2rgb(const CHSV32& hsv, CRGBW& rgb) {
  unsigned p, q, t;
  unsigned region = ((unsigned)hsv.h * 6) >> 16; // h / (65536 / 6)
  unsigned remainder = (hsv.h - (region * 10923)) * 6; // 10923 = (65536 / 6)
  //unsigned region = (unsigned)hsv.h / 10923;  // 65536 / 6 = 10923
  //unsigned  remainder = ((unsigned)hsv.h - (region * 10923)) * 6;

  // check for zero saturation
  if (hsv.s == 0) {
    rgb.r = rgb.g = rgb.b = hsv.v;
    return;
  }

  p = (hsv.v * (255 - hsv.s)) >> 8;
  q = (hsv.v * (255 - ((hsv.s * remainder) >> 16))) >> 8;
  t = (hsv.v * (255 - ((hsv.s * (65535 - remainder)) >> 16))) >> 8;
  switch (region) {
    case 0:
      rgb.r = hsv.v;
      rgb.g = t;
      rgb.b = p;
      break;
    case 1:
      rgb.r = q;
      rgb.g = hsv.v;
      rgb.b = p;
      break;
    case 2:
      rgb.r = p;
      rgb.g = hsv.v;
      rgb.b = t;
      break;
    case 3:
      rgb.r = p;
      rgb.g = q;
      rgb.b = hsv.v;
      break;
    case 4:
      rgb.r = t;
      rgb.g = p;
      rgb.b = hsv.v;
      break;
    default:
      rgb.r = hsv.v;
      rgb.g = p;
      rgb.b = q;
      break;
  }
}

inline CRGB hsv2rgb(const CHSV& hsv) {  // CHSV to CRGB
  CHSV32 hsv32(hsv);
  CRGBW rgb;
  hsv2rgb(hsv32, rgb);
  return CRGB(rgb);
}


void hsv2rgb_rainbow16(uint16_t h, uint8_t s, uint8_t v, uint8_t* rgbdata, bool isRGBW) {
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
  //rgb.r = r;
  //rgb.g = g;
  //rgb.b = b;
}

/*
// rainbow spectrum, adapted from fastled
// note: integer types are carefully chosen to maximize performance
void hsv2rgb_rainbow16(const CHSV32& hsv, CRGBW& rgb) { 

  uint8_t hue = hsv.h>>8;
  uint8_t sat = hsv.s;
  uint32_t val = hsv.v;
  uint8_t offset = hsv.h & 0x1FFF; // 0..31
  uint32_t third16 = ((int)offset * 21846); // offset16 = offset * 1/3<<16
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

  rgb.r = r;
  rgb.g = g;
  rgb.b = b;
}
*/

// note: code duplication is for speed, using conversion functions, makes it much slower (about half the speed)
// in order to remove the duplication without much speed impact: add a conversion function, but remove all looped calls to it and raplace those with the 16bit version.
// all attempts using pointers, references or inline functions did not result in a speedup



#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )
 
 
#define K255 255
#define K171 171
#define K170 170
#define K85  85
 
void hsv2rgb_rainbow( const CHSV& hsv, CRGB& rgb)
{
    // Yellow has a higher inherent brightness than
    // any other color; 'pure' yellow is perceived to
    // be 93% as bright as white.  In order to make
    // yellow appear the correct relative brightness,
    // it has to be rendered brighter than all other
    // colors.
    // Level Y1 is a moderate boost, the default.
    // Level Y2 is a strong boost.
    const uint8_t Y1 = 1;
    const uint8_t Y2 = 0;
    
    // G2: Whether to divide all greens by two.
    // Depends GREATLY on your particular LEDs
    const uint8_t G2 = 0;
    
    // Gscale: what to scale green down by.
    // Depends GREATLY on your particular LEDs
    const uint8_t Gscale = 0;
    
    
    uint8_t hue = hsv.hue;
    uint8_t sat = hsv.sat;
    uint8_t val = hsv.val;
    
    uint8_t offset = hue & 0x1F; // 0..31
    
    // offset8 = offset * 8
    uint8_t offset8 = offset;
    {
#if defined(__AVR__)
        // Left to its own devices, gcc turns "x <<= 3" into a loop
        // It's much faster and smaller to just do three single-bit shifts
        // So this business is to force that.
        offset8 <<= 1;
        asm volatile("");
        offset8 <<= 1;
        asm volatile("");
        offset8 <<= 1;
#else
        // On ARM and other non-AVR platforms, we just shift 3.
        offset8 <<= 3;
#endif
    }
    
    uint8_t third = scale8( offset8, (256 / 3)); // max = 85
    
    uint8_t r, g, b;
    
    if( ! (hue & 0x80) ) {
        // 0XX
        if( ! (hue & 0x40) ) {
            // 00X
            //section 0-1
            if( ! (hue & 0x20) ) {
                // 000
                //case 0: // R -> O
                r = K255 - third;
                g = third;
                b = 0;
                FORCE_REFERENCE(b);
            } else {
                // 001
                //case 1: // O -> Y
                if( Y1 ) {
                    r = K171;
                    g = K85 + third ;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
                if( Y2 ) {
                    r = K170 + third;
                    //uint8_t twothirds = (third << 1);
                    uint8_t twothirds = scale8( offset8, ((256 * 2) / 3)); // max=170
                    g = K85 + twothirds;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
            }
        } else {
            //01X
            // section 2-3
            if( !  (hue & 0x20) ) {
                // 010
                //case 2: // Y -> G
                if( Y1 ) {
                    //uint8_t twothirds = (third << 1);
                    uint8_t twothirds = scale8( offset8, ((256 * 2) / 3)); // max=170
                    r = K171 - twothirds;
                    g = K170 + third;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
                if( Y2 ) {
                    r = K255 - offset8;
                    g = K255;
                    b = 0;
                    FORCE_REFERENCE(b);
                }
            } else {
                // 011
                // case 3: // G -> A
                r = 0;
                FORCE_REFERENCE(r);
                g = K255 - third;
                b = third;
            }
        }
    } else {
        // section 4-7
        // 1XX
        if( ! (hue & 0x40) ) {
            // 10X
            if( ! ( hue & 0x20) ) {
                // 100
                //case 4: // A -> B
                r = 0;
                FORCE_REFERENCE(r);
                //uint8_t twothirds = (third << 1);
                uint8_t twothirds = scale8( offset8, ((256 * 2) / 3)); // max=170
                g = K171 - twothirds; //K170?
                b = K85  + twothirds;
                
            } else {
                // 101
                //case 5: // B -> P
                r = third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K255 - third;
                
            }
        } else {
            if( !  (hue & 0x20)  ) {
                // 110
                //case 6: // P -- K
                r = K85 + third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K171 - third;
                
            } else {
                // 111
                //case 7: // K -> R
                r = K170 + third;
                g = 0;
                FORCE_REFERENCE(g);
                b = K85 - third;
                
            }
        }
    }
    
    // This is one of the good places to scale the green down,
    // although the client can scale green down as well.
    if( G2 ) g = g >> 1;
    if( Gscale ) g = scale8_video( g, Gscale);
    
    // Scale down colors if we're desaturated at all
    // and add the brightness_floor to r, g, and b.
    if( sat != 255 ) {
        if( sat == 0) {
            r = 255; b = 255; g = 255;
        } else {
            uint8_t desat = 255 - sat;
            desat = scale8_video( desat, desat);
 
            uint8_t satscale = 255 - desat;
            //satscale = sat; // uncomment to revert to pre-2021 saturation behavior
 

            if( r ) r = scale8( r, satscale) + 1;
            if( g ) g = scale8( g, satscale) + 1;
            if( b ) b = scale8( b, satscale) + 1;

            uint8_t brightness_floor = desat;
            r += brightness_floor;
            g += brightness_floor;
            b += brightness_floor;
        }
    }
    
    // Now scale everything down if we're at value < 255.
    if( val != 255 ) {
        
        val = scale8_video( val, val);
        if( val == 0 ) {
            r=0; g=0; b=0;
        } else {
            // nscale8x3_video( r, g, b, val);

            if( r ) r = scale8( r, val) + 1;
            if( g ) g = scale8( g, val) + 1;
            if( b ) b = scale8( b, val) + 1;
        }
    }
    
    // Here we have the old AVR "missing std X+n" problem again
    // It turns out that fixing it winds up costing more than
    // not fixing it.
    // To paraphrase Dr Bronner, profile! profile! profile!
    //asm volatile(  ""  :  :  : "r26", "r27" );
    //asm volatile (" movw r30, r26 \n" : : : "r30", "r31");
    rgb.r = r;
    rgb.g = g;
    rgb.b = b;
}
/*
// rainbow spectrum, adapted from fastled
// note: integer types are carefully chosen to maximize performance
void hsv2rgb_rainbow(const CHSV& hsv, CRGB& rgb) {
  
  // slower version using conversion functions
  //CHSV32 hsv32(hsv);
  //CRGBW rgbw;
  //hsv2rgb_rainbow16(hsv32, rgbw);
  //rgb = CRGB(rgbw);
  
  uint8_t hue = hsv.hue;
  uint8_t sat = hsv.sat;
  uint32_t val = hsv.val;
  uint8_t offset = hue & 0x1F; // 0..31
  uint32_t third16 = ((int)offset * 21846); // offset16 = offset * 1/3<<16
  uint8_t third = third16 >> 13; // max = 85
  uint8_t r, g, b;

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
        uint8_t twothirds = third16 >> 12; // max=170
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
        uint8_t twothirds = third16 >> 12; // max=170
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

  rgb.r = r;
  rgb.g = g;
  rgb.b = b;
}
*/
void rgb2hsv(const uint32_t rgb, CHSV32& hsv) // convert RGB to HSV (16bit hue), much more accurate and faster than fastled version
{
    hsv.hsv32 = 0;
    int32_t r = (rgb>>16)&0xFF;
    int32_t g = (rgb>>8)&0xFF;
    int32_t b = rgb&0xFF;
    int32_t minval, maxval, delta;
    minval = min(r, g);
    minval = min(minval, b);
    maxval = max(r, g);
    maxval = max(maxval, b);
    if (maxval == 0)  return; // black
    hsv.v = maxval;
    delta = maxval - minval;
    hsv.s = (255 * delta) / maxval;
    if (hsv.s == 0)  return; // gray value
    if (maxval == r) hsv.h = (10923 * (g - b)) / delta;
    else if (maxval == g)  hsv.h = 21845 + (10923 * (b - r)) / delta;
    else hsv.h = 43690 + (10923 * (r - g)) / delta;
}

inline CHSV rgb2hsv(const CRGB c) {  // CRGB to CHSV
  CHSV32 hsv;
  rgb2hsv((uint32_t((byte(c.r) << 16) | (byte(c.g) << 8) | (byte(c.b)))), hsv);
  return CHSV(hsv);
}

void colorHStoRGB(uint16_t hue, byte sat, byte* rgb) { //hue, sat to rgb
  CRGBW crgb;
  hsv2rgb(CHSV32(hue, sat, 255), crgb);
  rgb[0] = crgb.r;
  rgb[1] = crgb.g;
  rgb[2] = crgb.b;
}

//get RGB values from color temperature in K (https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html)
void colorKtoRGB(uint16_t kelvin, byte* rgb) //white spectrum to rgb, calc
{
  int r = 0, g = 0, b = 0;
  float temp = kelvin / 100.0f;
  if (temp <= 66.0f) {
    r = 255;
    g = roundf(99.4708025861f * logf(temp) - 161.1195681661f);
    if (temp <= 19.0f) {
      b = 0;
    } else {
      b = roundf(138.5177312231f * logf((temp - 10.0f)) - 305.0447927307f);
    }
  } else {
    r = roundf(329.698727446f * powf((temp - 60.0f), -0.1332047592f));
    g = roundf(288.1221695283f * powf((temp - 60.0f), -0.0755148492f));
    b = 255;
  }
  //g += 12; //mod by Aircoookie, a bit less accurate but visibly less pinkish
  rgb[0] = (uint8_t) constrain(r, 0, 255);
  rgb[1] = (uint8_t) constrain(g, 0, 255);
  rgb[2] = (uint8_t) constrain(b, 0, 255);
  rgb[3] = 0;
}

void colorCTtoRGB(uint16_t mired, byte* rgb) //white spectrum to rgb, bins
{
  //this is only an approximation using WS2812B with gamma correction enabled
  if (mired > 475) {
    rgb[0]=255;rgb[1]=199;rgb[2]=92;//500
  } else if (mired > 425) {
    rgb[0]=255;rgb[1]=213;rgb[2]=118;//450
  } else if (mired > 375) {
    rgb[0]=255;rgb[1]=216;rgb[2]=118;//400
  } else if (mired > 325) {
    rgb[0]=255;rgb[1]=234;rgb[2]=140;//350
  } else if (mired > 275) {
    rgb[0]=255;rgb[1]=243;rgb[2]=160;//300
  } else if (mired > 225) {
    rgb[0]=250;rgb[1]=255;rgb[2]=188;//250
  } else if (mired > 175) {
    rgb[0]=247;rgb[1]=255;rgb[2]=215;//200
  } else {
    rgb[0]=237;rgb[1]=255;rgb[2]=239;//150
  }
}

// black body radiation to RGB (from fastled)
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

#ifndef WLED_DISABLE_HUESYNC
void colorXYtoRGB(float x, float y, byte* rgb) //coordinates to rgb (https://www.developers.meethue.com/documentation/color-conversions-rgb-xy)
{
  float z = 1.0f - x - y;
  float X = (1.0f / y) * x;
  float Z = (1.0f / y) * z;
  float r = (int)255*(X * 1.656492f - 0.354851f - Z * 0.255038f);
  float g = (int)255*(-X * 0.707196f + 1.655397f + Z * 0.036152f);
  float b = (int)255*(X * 0.051713f - 0.121364f + Z * 1.011530f);
  if (r > b && r > g && r > 1.0f) {
    // red is too big
    g = g / r;
    b = b / r;
    r = 1.0f;
  } else if (g > b && g > r && g > 1.0f) {
    // green is too big
    r = r / g;
    b = b / g;
    g = 1.0f;
  } else if (b > r && b > g && b > 1.0f) {
    // blue is too big
    r = r / b;
    g = g / b;
    b = 1.0f;
  }
  // Apply gamma correction
  r = r <= 0.0031308f ? 12.92f * r : (1.0f + 0.055f) * powf(r, (1.0f / 2.4f)) - 0.055f;
  g = g <= 0.0031308f ? 12.92f * g : (1.0f + 0.055f) * powf(g, (1.0f / 2.4f)) - 0.055f;
  b = b <= 0.0031308f ? 12.92f * b : (1.0f + 0.055f) * powf(b, (1.0f / 2.4f)) - 0.055f;

  if (r > b && r > g) {
    // red is biggest
    if (r > 1.0f) {
      g = g / r;
      b = b / r;
      r = 1.0f;
    }
  } else if (g > b && g > r) {
    // green is biggest
    if (g > 1.0f) {
      r = r / g;
      b = b / g;
      g = 1.0f;
    }
  } else if (b > r && b > g) {
    // blue is biggest
    if (b > 1.0f) {
      r = r / b;
      g = g / b;
      b = 1.0f;
    }
  }
  rgb[0] = byte(255.0f*r);
  rgb[1] = byte(255.0f*g);
  rgb[2] = byte(255.0f*b);
}

void colorRGBtoXY(const byte* rgb, float* xy) //rgb to coordinates (https://www.developers.meethue.com/documentation/color-conversions-rgb-xy)
{
  float X = rgb[0] * 0.664511f + rgb[1] * 0.154324f + rgb[2] * 0.162028f;
  float Y = rgb[0] * 0.283881f + rgb[1] * 0.668433f + rgb[2] * 0.047685f;
  float Z = rgb[0] * 0.000088f + rgb[1] * 0.072310f + rgb[2] * 0.986039f;
  xy[0] = X / (X + Y + Z);
  xy[1] = Y / (X + Y + Z);
}
#endif // WLED_DISABLE_HUESYNC

//RRGGBB / WWRRGGBB order for hex
void colorFromDecOrHexString(byte* rgb, const char* in)
{
  if (in[0] == 0) return;
  char first = in[0];
  uint32_t c = 0;

  if (first == '#' || first == 'h' || first == 'H') //is HEX encoded
  {
    c = strtoul(in +1, NULL, 16);
  } else
  {
    c = strtoul(in, NULL, 10);
  }

  rgb[0] = R(c);
  rgb[1] = G(c);
  rgb[2] = B(c);
  rgb[3] = W(c);
}

//contrary to the colorFromDecOrHexString() function, this uses the more standard RRGGBB / RRGGBBWW order
bool colorFromHexString(byte* rgb, const char* in) {
  if (in == nullptr) return false;
  size_t inputSize = strnlen(in, 9);
  if (inputSize != 6 && inputSize != 8) return false;

  uint32_t c = strtoul(in, NULL, 16);

  if (inputSize == 6) {
    rgb[0] = (c >> 16);
    rgb[1] = (c >>  8);
    rgb[2] =  c       ;
  } else {
    rgb[0] = (c >> 24);
    rgb[1] = (c >> 16);
    rgb[2] = (c >>  8);
    rgb[3] =  c       ;
  }
  return true;
}

static inline float minf(float v, float w)
{
  if (w > v) return v;
  return w;
}

static inline float maxf(float v, float w)
{
  if (w > v) return w;
  return v;
}

// adjust RGB values based on color temperature in K (range [2800-10200]) (https://en.wikipedia.org/wiki/Color_balance)
// called from bus manager when color correction is enabled!
uint32_t colorBalanceFromKelvin(uint16_t kelvin, uint32_t rgb)
{
  //remember so that slow colorKtoRGB() doesn't have to run for every setPixelColor()
  static byte correctionRGB[4] = {0,0,0,0};
  static uint16_t lastKelvin = 0;
  if (lastKelvin != kelvin) colorKtoRGB(kelvin, correctionRGB);  // convert Kelvin to RGB
  lastKelvin = kelvin;
  byte rgbw[4];
  rgbw[0] = ((uint16_t) correctionRGB[0] * R(rgb)) /255; // correct R
  rgbw[1] = ((uint16_t) correctionRGB[1] * G(rgb)) /255; // correct G
  rgbw[2] = ((uint16_t) correctionRGB[2] * B(rgb)) /255; // correct B
  rgbw[3] =                                W(rgb);
  return RGBW32(rgbw[0],rgbw[1],rgbw[2],rgbw[3]);
}

//approximates a Kelvin color temperature from an RGB color.
//this does no check for the "whiteness" of the color,
//so should be used combined with a saturation check (as done by auto-white)
//values from http://www.vendian.org/mncharity/dir3/blackbody/UnstableURLs/bbr_color.html (10deg)
//equation spreadsheet at https://bit.ly/30RkHaN
//accuracy +-50K from 1900K up to 8000K
//minimum returned: 1900K, maximum returned: 10091K (range of 8192)
uint16_t approximateKelvinFromRGB(uint32_t rgb) {
  //if not either red or blue is 255, color is dimmed. Scale up
  uint8_t r = R(rgb), b = B(rgb);
  if (r == b) return 6550; //red == blue at about 6600K (also can't go further if both R and B are 0)

  if (r > b) {
    //scale blue up as if red was at 255
    uint16_t scale = 0xFFFF / r; //get scale factor (range 257-65535)
    b = ((uint16_t)b * scale) >> 8;
    //For all temps K<6600 R is bigger than B (for full bri colors R=255)
    //-> Use 9 linear approximations for blackbody radiation blue values from 2000-6600K (blue is always 0 below 2000K)
    if (b < 33)  return 1900 + b       *6;
    if (b < 72)  return 2100 + (b-33)  *10;
    if (b < 101) return 2492 + (b-72)  *14;
    if (b < 132) return 2900 + (b-101) *16;
    if (b < 159) return 3398 + (b-132) *19;
    if (b < 186) return 3906 + (b-159) *22;
    if (b < 210) return 4500 + (b-186) *25;
    if (b < 230) return 5100 + (b-210) *30;
                 return 5700 + (b-230) *34;
  } else {
    //scale red up as if blue was at 255
    uint16_t scale = 0xFFFF / b; //get scale factor (range 257-65535)
    r = ((uint16_t)r * scale) >> 8;
    //For all temps K>6600 B is bigger than R (for full bri colors B=255)
    //-> Use 2 linear approximations for blackbody radiation red values from 6600-10091K (blue is always 0 below 2000K)
    if (r > 225) return 6600 + (254-r) *50;
    uint16_t k = 8080 + (225-r) *86;
    return (k > 10091) ? 10091 : k;
  }
}

// gamma lookup table used for color correction (filled on 1st use (cfg.cpp & set.cpp))
uint8_t NeoGammaWLEDMethod::gammaT[256];

// re-calculates & fills gamma table
void NeoGammaWLEDMethod::calcGammaTable(float gamma)
{
  for (size_t i = 0; i < 256; i++) {
    gammaT[i] = (int)(powf((float)i / 255.0f, gamma) * 255.0f + 0.5f);
  }
}

uint8_t IRAM_ATTR_YN NeoGammaWLEDMethod::Correct(uint8_t value)
{
  if (!gammaCorrectCol) return value;
  return gammaT[value];
}

// used for color gamma correction
uint32_t IRAM_ATTR_YN NeoGammaWLEDMethod::Correct32(uint32_t color)
{
  if (!gammaCorrectCol) return color;
  uint8_t w = W(color);
  uint8_t r = R(color);
  uint8_t g = G(color);
  uint8_t b = B(color);
  w = gammaT[w];
  r = gammaT[r];
  g = gammaT[g];
  b = gammaT[b];
  return RGBW32(r, g, b, w);
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
      uint32_t t = endpos;
      CRGB tc = endcolor;
      endcolor = startcolor;
      endpos = startpos;
      startpos = t;
      startcolor = tc;
  }
  int32_t rdistance = endcolor.r - startcolor.r;
  int32_t gdistance = endcolor.g - startcolor.g;
  int32_t bdistance = endcolor.b - startcolor.b;

  int32_t divisor = endpos - startpos;
  divisor = divisor == 0 ? 1 : divisor; // prevent division by zero

  int32_t rdelta = (rdistance << 16) / divisor;
  int32_t gdelta = (gdistance << 16) / divisor;
  int32_t bdelta = (bdistance << 16) / divisor;

  int32_t rshifted = startcolor.r << 16;
  int32_t gshifted = startcolor.g << 16;
  int32_t bshifted = startcolor.b << 16;

  for (int32_t i = startpos; i <= endpos; i++) {
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