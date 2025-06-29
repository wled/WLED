#include "wled.h"

/*
 * Color conversion & utility methods
 */

/*
 * color blend function, based on FastLED blend function
 * the calculation for each color is: result = (A*(amountOfA) + A + B*(amountOfB) + B) / 256 with amountOfA = 255 - amountOfB
 */
uint32_t color_blend(uint32_t color1, uint32_t color2, uint8_t blend) {
  // min / max blend checking is omitted: calls with 0 or 255 are rare, checking lowers overall performance
  const uint32_t TWO_CHANNEL_MASK = 0x00FF00FF;     // mask for R and B channels or W and G if negated (poorman's SIMD; https://github.com/wled/WLED/pull/4568#discussion_r1986587221)
  uint32_t rb1 =  color1       & TWO_CHANNEL_MASK;  // extract R & B channels from color1
  uint32_t wg1 = (color1 >> 8) & TWO_CHANNEL_MASK;  // extract W & G channels from color1 (shifted for multiplication later)
  uint32_t rb2 =  color2       & TWO_CHANNEL_MASK;  // extract R & B channels from color2
  uint32_t wg2 = (color2 >> 8) & TWO_CHANNEL_MASK;  // extract W & G channels from color2 (shifted for multiplication later)
  uint32_t rb3 = ((((rb1 << 8) | rb2) + (rb2 * blend) - (rb1 * blend)) >> 8) &  TWO_CHANNEL_MASK; // blend red and blue
  uint32_t wg3 = ((((wg1 << 8) | wg2) + (wg2 * blend) - (wg1 * blend)))      & ~TWO_CHANNEL_MASK; // negated mask for white and green
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
  const uint32_t TWO_CHANNEL_MASK = 0x00FF00FF; // mask for R and B channels or W and G if negated
  uint32_t rb = ( c1     & TWO_CHANNEL_MASK) + ( c2     & TWO_CHANNEL_MASK); // mask and add two colors at once
  uint32_t wg = ((c1>>8) & TWO_CHANNEL_MASK) + ((c2>>8) & TWO_CHANNEL_MASK);
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
      const uint32_t scale = (uint32_t(255)<<8) / max; // division of two 8bit (shifted) values does not work -> use bit shifts and multiplaction instead
      rb = ((rb * scale) >> 8) &  TWO_CHANNEL_MASK;
      wg =  (wg * scale)       & ~TWO_CHANNEL_MASK;
    } else wg <<= 8; //shift white and green back to correct position
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
  const uint32_t TWO_CHANNEL_MASK = 0x00FF00FF;
  uint32_t rb = (((c1 & TWO_CHANNEL_MASK) * scale) >> 8) &  TWO_CHANNEL_MASK; // scale red and blue
  uint32_t wg = (((c1 >> 8) & TWO_CHANNEL_MASK) * scale) & ~TWO_CHANNEL_MASK; // scale white and green
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
  if (lo4 && blendType != NOBLEND) {
    if (hi4 == 15) entry = &(pal[0]);
    else ++entry;
    unsigned f2 = (lo4 << 4);
    unsigned f1 = 256 - f2;
    red1   = (red1 * f1 + (unsigned)entry->r * f2) >> 8; // note: using color_blend() is 20% slower
    green1 = (green1 * f1 + (unsigned)entry->g * f2) >> 8;
    blue1  = (blue1 * f1 + (unsigned)entry->b * f2) >> 8;
  }
  if (brightness < 255) { // note: zero checking could be done to return black but that is hardly ever used so it is omitted
    // actually color_fade(c1, brightness)
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

  // apply saturation
  CRGB RGBpalettecolors[4];
  for (int i = 0; i < 4; i++) {
    if (makepastelpalette && palettecolors[i].saturation > 180) {
      palettecolors[i].saturation -= 160; //desaturate all four colors
    }
    RGBpalettecolors[i] = (CRGB)palettecolors[i]; //convert to RGB
    RGBpalettecolors[i] = ((uint32_t)RGBpalettecolors[i]) & 0x00FFFFFFU; //strip alpha from CRGB
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

void loadCustomPalettes() {
  byte tcp[72]; //support gradient palettes with up to 18 entries
  CRGBPalette16 targetPalette;
  customPalettes.clear(); // start fresh
  for (int index = 0; index<10; index++) {
    char fileName[32];
    sprintf_P(fileName, PSTR("/palette%d.json"), index);

    StaticJsonDocument<1536> pDoc; // barely enough to fit 72 numbers
    if (WLED_FS.exists(fileName)) {
      DEBUGFX_PRINTF_P(PSTR("Reading palette from %s\n"), fileName);
      if (readObjectFromFile(fileName, nullptr, &pDoc)) {
        JsonArray pal = pDoc[F("palette")];
        if (!pal.isNull() && pal.size()>3) { // not an empty palette (at least 2 entries)
          memset(tcp, 255, sizeof(tcp));
          if (pal[0].is<int>() && pal[1].is<const char *>()) {
            // we have an array of index & hex strings
            size_t palSize = MIN(pal.size(), 36);
            palSize -= palSize % 2; // make sure size is multiple of 2
            for (size_t i=0, j=0; i<palSize && pal[i].as<int>()<256; i+=2) {
              uint8_t rgbw[] = {0,0,0,0};
              if (colorFromHexString(rgbw, pal[i+1].as<const char *>())) { // will catch non-string entires
                tcp[ j ] = (uint8_t) pal[ i ].as<int>(); // index
                for (size_t c=0; c<3; c++) tcp[j+1+c] = rgbw[c]; // only use RGB component
                DEBUGFX_PRINTF_P(PSTR("%2u -> %3d [%3d,%3d,%3d]\n"), i, int(tcp[j]), int(tcp[j+1]), int(tcp[j+2]), int(tcp[j+3]));
                j += 4;
              }
            }
          } else {
            size_t palSize = MIN(pal.size(), 72);
            palSize -= palSize % 4; // make sure size is multiple of 4
            for (size_t i=0; i<palSize && pal[i].as<int>()<256; i+=4) {
              tcp[ i ] = (uint8_t) pal[ i ].as<int>(); // index
              for (size_t c=0; c<3; c++) tcp[i+1+c] = (uint8_t) pal[i+1+c].as<int>();
              DEBUGFX_PRINTF_P(PSTR("%2u -> %3d [%3d,%3d,%3d]\n"), i, int(tcp[i]), int(tcp[i+1]), int(tcp[i+2]), int(tcp[i+3]));
            }
          }
          customPalettes.push_back(targetPalette.loadDynamicGradientPalette(tcp));
        } else {
          DEBUGFX_PRINTLN(F("Wrong palette format."));
        }
      }
    } else {
      break;
    }
  }
}

void hsv2rgb(const CHSV32& hsv, uint32_t& rgb) // convert HSV (16bit hue) to RGB (32bit with white = 0)
{
  unsigned int remainder, region, p, q, t;
  unsigned int h = hsv.h;
  unsigned int s = hsv.s;
  unsigned int v = hsv.v;
  if (s == 0) {
      rgb = v << 16 | v << 8 | v;
      return;
  }
  region = h / 10923;  // 65536 / 6 = 10923
  remainder = (h - (region * 10923)) * 6;
  p = (v * (255 - s)) >> 8;
  q = (v * (255 - ((s * remainder) >> 16))) >> 8;
  t = (v * (255 - ((s * (65535 - remainder)) >> 16))) >> 8;
  switch (region) {
    case 0:
      rgb = v << 16 | t << 8 | p; break;
    case 1:
      rgb = q << 16 | v << 8 | p; break;
    case 2:
      rgb = p << 16 | v << 8 | t; break;
    case 3:
      rgb = p << 16 | q << 8 | v; break;
    case 4:
      rgb = t << 16 | p << 8 | v; break;
    default:
      rgb = v << 16 | p << 8 | q; break;
  }
}

void rgb2hsv(const uint32_t rgb, CHSV32& hsv) // convert RGB to HSV (16bit hue), much more accurate and faster than fastled version
{
    hsv.raw = 0;
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

void colorHStoRGB(uint16_t hue, byte sat, byte* rgb) { //hue, sat to rgb
  uint32_t crgb;
  hsv2rgb(CHSV32(hue, sat, 255), crgb);
  rgb[0] = byte((crgb) >> 16);
  rgb[1] = byte((crgb) >> 8);
  rgb[2] = byte(crgb);
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

// gamma lookup tables used for color correction (filled on 1st use (cfg.cpp & set.cpp))
uint8_t NeoGammaWLEDMethod::gammaT[256];
uint8_t NeoGammaWLEDMethod::gammaT_inv[256];

// re-calculates & fills gamma tables
void NeoGammaWLEDMethod::calcGammaTable(float gamma)
{
  float gamma_inv = 1.0f / gamma; // inverse gamma
  for (size_t i = 0; i < 256; i++) {
    gammaT[i] = (int)(powf((float)i / 255.0f, gamma) * 255.0f + 0.5f);
    gammaT_inv[i] = (int)(powf((float)i / 255.0f, gamma_inv) * 255.0f + 0.5f);
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

uint32_t IRAM_ATTR_YN NeoGammaWLEDMethod::inverseGamma32(uint32_t color)
{
  if (!gammaCorrectCol) return color;
  uint8_t w = W(color);
  uint8_t r = R(color);
  uint8_t g = G(color);
  uint8_t b = B(color);
  w = gammaT_inv[w];
  r = gammaT_inv[r];
  g = gammaT_inv[g];
  b = gammaT_inv[b];
  return RGBW32(r, g, b, w);
}
