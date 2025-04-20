#ifndef FASTLED_FCN_H
#define FASTLED_FCN_H

#include <stdint.h>
#include <cstring> // for mem operations

// Code originally from FastLED version 3.6.0. Optimized for WLED use by @dedehai
// Licensed unter MIT license, see LICENSE.txt for details

// inline math functions
__attribute__ ((always_inline)) inline uint8_t  scale8(uint8_t i, uint8_t scale ) { return ((int)i * (1 + (int)scale)) >> 8; }
__attribute__ ((always_inline)) inline uint8_t  scale8_video(uint8_t i, uint8_t scale ) { return (((int)i * (int)scale) >> 8) + ((i&&scale)?1:0); }
__attribute__ ((always_inline)) inline uint16_t scale16(uint16_t i, uint16_t scale ) { return ((uint32_t)i * (1 + (uint32_t)scale)) >> 16; }
__attribute__ ((always_inline)) inline uint8_t  qadd8(uint8_t i, uint8_t j) { unsigned t = i + j; return t > 255 ? 255 : t; }
__attribute__ ((always_inline)) inline uint8_t  qsub8(uint8_t i, uint8_t j) { int t = i - j; return t < 0 ? 0 : t; }
__attribute__ ((always_inline)) inline uint8_t  qmul8(uint8_t i, uint8_t j) { unsigned p = (unsigned)i * (unsigned)j; return p > 255 ? 255 : p; }
__attribute__ ((always_inline)) inline int8_t   abs8(int8_t i) { return i < 0 ? -i : i; }
__attribute__ ((always_inline)) inline int8_t   lerp8by8(uint8_t a, uint8_t b, uint8_t frac) { return a + ((((int32_t)b - (int32_t)a) * ((int32_t)frac+1)) >> 8); }

//forward declarations
struct CRGB;
struct CHSV;
class CRGBPalette16;

typedef uint32_t TProgmemRGBPalette16[16];
typedef uint8_t TDynamicRGBGradientPalette_byte;  // Byte of an RGB gradient entry, stored in dynamic (heap) memory
typedef const TDynamicRGBGradientPalette_byte *TDynamicRGBGradientPalette_bytes;  // Pointer to bytes of an RGB gradient, stored in dynamic (heap) memory
typedef TDynamicRGBGradientPalette_bytes TDynamicRGBGradientPalettePtr;  // Alias of ::TDynamicRGBGradientPalette_bytes
typedef const uint8_t TProgmemRGBGradientPalette_byte;
typedef const TProgmemRGBGradientPalette_byte *TProgmemRGBGradientPalette_bytes;
typedef TProgmemRGBGradientPalette_bytes TProgmemRGBGradientPalettePtr;

// color interpolation options for palette
typedef enum {
  NOBLEND=0,            // No interpolation between palette entries
  LINEARBLEND=1,        // Linear interpolation between palette entries, with wrap-around from end to the beginning again
  LINEARBLEND_NOWRAP=2  // Linear interpolation between palette entries, but no wrap-around
} TBlendType;

typedef union {
  struct {
    uint8_t index;  // index of the color entry in the gradient
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };
  uint32_t dword;     // values packed as 32-bit
  uint8_t  bytes[4];  // values as an array
} TRGBGradientPaletteEntryUnion;

// function prototypes
void hsv2rgb_rainbow(uint16_t h, uint8_t s, uint8_t v, uint8_t* rgbdata, bool isRGBW);
CRGB HeatColor(uint8_t temperature); // black body radiation
void fill_solid_RGB(CRGB* colors, uint32_t num, const CRGB& c1) ;
void fill_gradient_RGB(CRGB* colors, uint32_t startpos, CRGB startcolor, uint32_t endpos, CRGB endcolor);
void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2);
void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2, const CRGB& c3);
void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4);
void nblendPaletteTowardPalette(CRGBPalette16& current, CRGBPalette16& target, uint8_t maxChanges);

uint8_t ease8InOutCubic(uint8_t i);
uint16_t ease16InOutCubic(uint16_t i);
uint8_t ease8InOutQuad(uint8_t i);
uint8_t triwave8(uint8_t in);
uint16_t triwave16(uint16_t in);
uint8_t quadwave8(uint8_t in);
uint8_t cubicwave8(uint8_t in);

// Representation of an HSV pixel (hue, saturation, value (aka brightness)).
struct CHSV {
  union {
    struct {
      union {
        uint8_t hue;
        uint8_t h;
      };
      union {
        uint8_t saturation;
        uint8_t sat;
        uint8_t s;
      };
      union {
        uint8_t value;
        uint8_t val;
        uint8_t v;
      };
    };
    uint8_t raw[3]; // order is: hue [0], saturation [1], value [2]
  };

  inline uint8_t& operator[] (uint8_t x) __attribute__((always_inline)) {
    return raw[x];
  }

  inline const uint8_t& operator[] (uint8_t x) const __attribute__((always_inline)) {
    return raw[x];
  }

  // default constructor
  // @warning default values are UNITIALIZED!
  inline CHSV() __attribute__((always_inline)) = default;

  // allow construction from hue, saturation, and value
  inline CHSV(uint8_t ih, uint8_t is, uint8_t iv) __attribute__((always_inline))
  : h(ih), s(is), v(iv) { }

  // allow copy construction
  inline CHSV(const CHSV& rhs) __attribute__((always_inline)) = default;

  // allow copy construction
  inline CHSV& operator= (const CHSV& rhs) __attribute__((always_inline)) = default;

  // assign new HSV values
  inline CHSV& setHSV(uint8_t ih, uint8_t is, uint8_t iv) __attribute__((always_inline)) {
    h = ih;
    s = is;
    v = iv;
    return *this;
  }
};

// representation of an RGB pixel (Red, Green, Blue)
struct CRGB {
  union {
    struct {
      union {
        uint8_t r;
        uint8_t red;
      };
      union {
        uint8_t g;
        uint8_t green;
      };
      union {
        uint8_t b;
        uint8_t blue;
      };
    };
    uint8_t raw[3]; // order is: 0 = red, 1 = green, 2 = blue
  };

  inline uint8_t& operator[] (uint8_t x) __attribute__((always_inline)) { return raw[x]; }

  // array access operator to index into the CRGB object
  inline const uint8_t& operator[] (uint8_t x) const __attribute__((always_inline)) { return raw[x]; }

  // default constructor (uninitialized)
  inline CRGB() __attribute__((always_inline)) = default;

  // allow construction from red, green, and blue
  inline CRGB(uint8_t ir, uint8_t ig, uint8_t ib)  __attribute__((always_inline))
    : r(ir), g(ig), b(ib) { }

  // allow construction from 32-bit (really 24-bit) bit 0xRRGGBB color code
  inline CRGB(uint32_t colorcode)  __attribute__((always_inline))
    : r(uint8_t(colorcode>>16)), g(uint8_t(colorcode>>8)), b(uint8_t(colorcode)) { }

  // allow copy construction
  inline CRGB(const CRGB& rhs) __attribute__((always_inline)) = default;

  // allow construction from a CHSV color
  inline CRGB(const CHSV& rhs) __attribute__((always_inline))  {
    hsv2rgb_rainbow(rhs.h<<8, rhs.s, rhs.v, raw, false);
  }

  // allow assignment from hue, saturation, and value
  inline CRGB& setHSV (uint8_t hue, uint8_t sat, uint8_t val) __attribute__((always_inline)) {
    hsv2rgb_rainbow(hue<<8, sat, val, raw, false); return *this;
  }

  // allow assignment from just a hue, sat and val are set to max
  inline CRGB& setHue (uint8_t hue) __attribute__((always_inline)) {
    hsv2rgb_rainbow(hue<<8, 255, 255, raw, false); return *this;
  }

  // allow assignment from HSV color
  inline CRGB& operator= (const CHSV& rhs) __attribute__((always_inline)) {
    hsv2rgb_rainbow(rhs.h<<8, rhs.s, rhs.v, raw, false); return *this;
  }
  // allow assignment from one RGB struct to another
  inline CRGB& operator= (const CRGB& rhs) __attribute__((always_inline)) = default;

  // allow assignment from 32-bit (really 24-bit) 0xRRGGBB color code
  inline CRGB& operator= (const uint32_t colorcode) __attribute__((always_inline)) {
    r = uint8_t(colorcode>>16);
    g = uint8_t(colorcode>>8);
    b = uint8_t(colorcode);
    return *this;
  }

  // allow assignment from red, green, and blue
  inline CRGB& setRGB (uint8_t nr, uint8_t ng, uint8_t nb) __attribute__((always_inline)) {
    r = nr;
    g = ng;
    b = nb;
    return *this;
  }

  // allow assignment from 32-bit (really 24-bit) 0xRRGGBB color code
  inline CRGB& setColorCode (uint32_t colorcode) __attribute__((always_inline)) {
    r = uint8_t(colorcode>>16);
    g = uint8_t(colorcode>>8);
    b = uint8_t(colorcode);
    return *this;
  }

  // add one CRGB to another, saturating at 0xFF for each channel
  inline CRGB& operator+= (const CRGB& rhs) {
    r = qadd8(r, rhs.r);
    g = qadd8(g, rhs.g);
    b = qadd8(b, rhs.b);
    return *this;
  }

  // add a constant to each channel, saturating at 0xFF
  inline CRGB& addToRGB (uint8_t d) {
    r = qadd8(r, d);
    g = qadd8(g, d);
    b = qadd8(b, d);
    return *this;
  }

  // subtract one CRGB from another, saturating at 0x00 for each channel
  inline CRGB& operator-= (const CRGB& rhs) {
    r = qsub8(r, rhs.r);
    g = qsub8(g, rhs.g);
    b = qsub8(b, rhs.b);
    return *this;
  }

  // subtract a constant from each channel, saturating at 0x00
  inline CRGB& subtractFromRGB(uint8_t d) {
    r = qsub8(r, d);
    g = qsub8(g, d);
    b = qsub8(b, d);
    return *this;
  }

  // subtract a constant of '1' from each channel, saturating at 0x00
  inline CRGB& operator-- () __attribute__((always_inline)) {
    subtractFromRGB(1);
    return *this;
  }

  // operator--
  inline CRGB operator-- (int) __attribute__((always_inline)) {
    CRGB retval(*this);
    --(*this);
    return retval;
  }

  // add a constant of '1' to each channel, saturating at 0xFF
  inline CRGB& operator++ () __attribute__((always_inline)) {
    addToRGB(1);
    return *this;
  }

  // operator++
  inline CRGB operator++ (int) __attribute__((always_inline)) {
    CRGB retval(*this);
    ++(*this);
    return retval;
  }

  // divide each of the channels by a constant
  inline CRGB& operator/= (uint8_t d) {
    r /= d;
    g /= d;
    b /= d;
    return *this;
  }

  // right shift each of the channels by a constant
  inline CRGB& operator>>= (uint8_t d) {
    r >>= d;
    g >>= d;
    b >>= d;
    return *this;
  }

  // multiply each of the channels by a constant, saturating each channel at 0xFF.
  inline CRGB& operator*= (uint8_t d) {
    r = qmul8(r, d);
    g = qmul8(g, d);
    b = qmul8(b, d);
    return *this;
  }

  // scale down a RGB to N/256ths of its current brightness (will not scale all the way to black)
  inline CRGB& nscale8_video(uint8_t scaledown) {
    uint8_t nonzeroscale = (scaledown != 0) ? 1 : 0;
    r = (r == 0) ? 0 : (((int)r * (int)(scaledown)) >> 8) + nonzeroscale;
    g = (g == 0) ? 0 : (((int)g * (int)(scaledown)) >> 8) + nonzeroscale;
    b = (b == 0) ? 0 : (((int)b * (int)(scaledown)) >> 8) + nonzeroscale;
    return *this;
  }

  // scale down a RGB to N/256ths of its current brightness (can scale to black)
  inline CRGB& nscale8(uint8_t scaledown) {
    uint32_t scale_fixed = scaledown + 1;
    r = (((uint32_t)r) * scale_fixed) >> 8;
    g = (((uint32_t)g) * scale_fixed) >> 8;
    b = (((uint32_t)b) * scale_fixed) >> 8;
    return *this;
  }

  inline CRGB& nscale8(const CRGB& scaledown) {
    r = ::scale8(r, scaledown.r);
    g = ::scale8(g, scaledown.g);
    b = ::scale8(b, scaledown.b);
    return *this;
  }

  // return a CRGB object that is a scaled down version of this object
  inline CRGB scale8(uint8_t scaledown) const {
    CRGB out = *this;
    uint32_t scale_fixed = scaledown + 1;
    out.r = (((uint32_t)out.r) * scale_fixed) >> 8;
    out.g = (((uint32_t)out.g) * scale_fixed) >> 8;
    out.b = (((uint32_t)out.b) * scale_fixed) >> 8;
    return out;
  }

  // return a CRGB object that is a scaled down version of this object
  inline CRGB scale8(const CRGB& scaledown) const {
    CRGB out;
    out.r = ::scale8(r, scaledown.r);
    out.g = ::scale8(g, scaledown.g);
    out.b = ::scale8(b, scaledown.b);
    return out;
  }

  // fadeToBlackBy is a synonym for nscale8(), as a fade instead of a scale
  inline CRGB& fadeToBlackBy(uint8_t fadefactor) {
    uint32_t scale_fixed = 256 - fadefactor;
    r = (((uint32_t)r) * scale_fixed) >> 8;
    g = (((uint32_t)g) * scale_fixed) >> 8;
    b = (((uint32_t)b) * scale_fixed) >> 8;
    return *this;
  }

  // "or" operator brings each channel up to the higher of the two values
  inline CRGB& operator|=(const CRGB& rhs) {
    if (rhs.r > r) r = rhs.r;
    if (rhs.g > g) g = rhs.g;
    if (rhs.b > b) b = rhs.b;
    return *this;
  }

  inline CRGB& operator|=(uint8_t d) {
    if (d > r) r = d;
    if (d > g) g = d;
    if (d > b) b = d;
    return *this;
  }

  // "and" operator brings each channel down to the lower of the two values
  inline CRGB& operator&=(const CRGB& rhs) {
    if (rhs.r < r) r = rhs.r;
    if (rhs.g < g) g = rhs.g;
    if (rhs.b < b) b = rhs.b;
    return *this;
  }

  inline CRGB& operator&=(uint8_t d) {
    if (d < r) r = d;
    if (d < g) g = d;
    if (d < b) b = d;
    return *this;
  }

  // this allows testing a CRGB for zero-ness
  inline explicit operator bool() const __attribute__((always_inline)) {
    return r || g || b;
  }

  // converts a CRGB to a 32-bit color with white = 0
  inline explicit operator uint32_t() const {
    return (uint32_t{r} << 16) |
           (uint32_t{g} << 8)  |
            uint32_t{b};
  }

  // invert each channel
  inline CRGB operator-() const {
    CRGB retval;
    retval.r = 255 - r;
    retval.g = 255 - g;
    retval.b = 255 - b;
    return retval;
  }

  // get the average of the R, G, and B values
  inline uint8_t getAverageLight() const {
    return ((r + g + b) * 21846) >> 16; // x*21846>>16 is equal to "divide by 3"
  }

  typedef enum {
    AliceBlue=0xF0F8FF,
    Amethyst=0x9966CC,
    AntiqueWhite=0xFAEBD7,
    Aqua=0x00FFFF,
    Aquamarine=0x7FFFD4,
    Azure=0xF0FFFF,
    Beige=0xF5F5DC,
    Bisque=0xFFE4C4,
    Black=0x000000,
    BlanchedAlmond=0xFFEBCD,
    Blue=0x0000FF,
    BlueViolet=0x8A2BE2,
    Brown=0xA52A2A,
    BurlyWood=0xDEB887,
    CadetBlue=0x5F9EA0,
    Chartreuse=0x7FFF00,
    Chocolate=0xD2691E,
    Coral=0xFF7F50,
    CornflowerBlue=0x6495ED,
    Cornsilk=0xFFF8DC,
    Crimson=0xDC143C,
    Cyan=0x00FFFF,
    DarkBlue=0x00008B,
    DarkCyan=0x008B8B,
    DarkGoldenrod=0xB8860B,
    DarkGray=0xA9A9A9,
    DarkGrey=0xA9A9A9,
    DarkGreen=0x006400,
    DarkKhaki=0xBDB76B,
    DarkMagenta=0x8B008B,
    DarkOliveGreen=0x556B2F,
    DarkOrange=0xFF8C00,
    DarkOrchid=0x9932CC,
    DarkRed=0x8B0000,
    DarkSalmon=0xE9967A,
    DarkSeaGreen=0x8FBC8F,
    DarkSlateBlue=0x483D8B,
    DarkSlateGray=0x2F4F4F,
    DarkSlateGrey=0x2F4F4F,
    DarkTurquoise=0x00CED1,
    DarkViolet=0x9400D3,
    DeepPink=0xFF1493,
    DeepSkyBlue=0x00BFFF,
    DimGray=0x696969,
    DimGrey=0x696969,
    DodgerBlue=0x1E90FF,
    FireBrick=0xB22222,
    FloralWhite=0xFFFAF0,
    ForestGreen=0x228B22,
    Fuchsia=0xFF00FF,
    Gainsboro=0xDCDCDC,
    GhostWhite=0xF8F8FF,
    Gold=0xFFD700,
    Goldenrod=0xDAA520,
    Gray=0x808080,
    Grey=0x808080,
    Green=0x008000,
    GreenYellow=0xADFF2F,
    Honeydew=0xF0FFF0,
    HotPink=0xFF69B4,
    IndianRed=0xCD5C5C,
    Indigo=0x4B0082,
    Ivory=0xFFFFF0,
    Khaki=0xF0E68C,
    Lavender=0xE6E6FA,
    LavenderBlush=0xFFF0F5,
    LawnGreen=0x7CFC00,
    LemonChiffon=0xFFFACD,
    LightBlue=0xADD8E6,
    LightCoral=0xF08080,
    LightCyan=0xE0FFFF,
    LightGoldenrodYellow=0xFAFAD2,
    LightGreen=0x90EE90,
    LightGrey=0xD3D3D3,
    LightPink=0xFFB6C1,
    LightSalmon=0xFFA07A,
    LightSeaGreen=0x20B2AA,
    LightSkyBlue=0x87CEFA,
    LightSlateGray=0x778899,
    LightSlateGrey=0x778899,
    LightSteelBlue=0xB0C4DE,
    LightYellow=0xFFFFE0,
    Lime=0x00FF00,
    LimeGreen=0x32CD32,
    Linen=0xFAF0E6,
    Magenta=0xFF00FF,
    Maroon=0x800000,
    MediumAquamarine=0x66CDAA,
    MediumBlue=0x0000CD,
    MediumOrchid=0xBA55D3,
    MediumPurple=0x9370DB,
    MediumSeaGreen=0x3CB371,
    MediumSlateBlue=0x7B68EE,
    MediumSpringGreen=0x00FA9A,
    MediumTurquoise=0x48D1CC,
    MediumVioletRed=0xC71585,
    MidnightBlue=0x191970,
    MintCream=0xF5FFFA,
    MistyRose=0xFFE4E1,
    Moccasin=0xFFE4B5,
    NavajoWhite=0xFFDEAD,
    Navy=0x000080,
    OldLace=0xFDF5E6,
    Olive=0x808000,
    OliveDrab=0x6B8E23,
    Orange=0xFFA500,
    OrangeRed=0xFF4500,
    Orchid=0xDA70D6,
    PaleGoldenrod=0xEEE8AA,
    PaleGreen=0x98FB98,
    PaleTurquoise=0xAFEEEE,
    PaleVioletRed=0xDB7093,
    PapayaWhip=0xFFEFD5,
    PeachPuff=0xFFDAB9,
    Peru=0xCD853F,
    Pink=0xFFC0CB,
    Plaid=0xCC5533,
    Plum=0xDDA0DD,
    PowderBlue=0xB0E0E6,
    Purple=0x800080,
    Red=0xFF0000,
    RosyBrown=0xBC8F8F,
    RoyalBlue=0x4169E1,
    SaddleBrown=0x8B4513,
    Salmon=0xFA8072,
    SandyBrown=0xF4A460,
    SeaGreen=0x2E8B57,
    Seashell=0xFFF5EE,
    Sienna=0xA0522D,
    Silver=0xC0C0C0,
    SkyBlue=0x87CEEB,
    SlateBlue=0x6A5ACD,
    SlateGray=0x708090,
    SlateGrey=0x708090,
    Snow=0xFFFAFA,
    SpringGreen=0x00FF7F,
    SteelBlue=0x4682B4,
    Tan=0xD2B48C,
    Teal=0x008080,
    Thistle=0xD8BFD8,
    Tomato=0xFF6347,
    Turquoise=0x40E0D0,
    Violet=0xEE82EE,
    Wheat=0xF5DEB3,
    White=0xFFFFFF,
    WhiteSmoke=0xF5F5F5,
    Yellow=0xFFFF00,
    YellowGreen=0x9ACD32,
    FairyLight=0xFFE42D,    // LED RGB color that roughly approximates the color of incandescent fairy lights
    FairyLightNCC=0xFF9D2A  // if using no color correction, use this
  } HTMLColorCode;
};

__attribute__((always_inline)) inline CRGB operator+(const CRGB& p1, const CRGB& p2) {
  return CRGB(qadd8(p1.r, p2.r), qadd8(p1.g, p2.g), qadd8(p1.b, p2.b));
}

__attribute__((always_inline)) inline CRGB operator-(const CRGB& p1, const CRGB& p2) {
  return CRGB(qsub8(p1.r, p2.r), qsub8(p1.g, p2.g), qsub8(p1.b, p2.b));
}

__attribute__((always_inline)) inline bool operator== (const CRGB& lhs, const CRGB& rhs) {
  return (lhs.r == rhs.r) && (lhs.g == rhs.g) && (lhs.b == rhs.b);
}

__attribute__((always_inline)) inline bool operator!= (const CRGB& lhs, const CRGB& rhs) {
  return !(lhs == rhs);
}

// RGB color palette with 16 discrete values
class CRGBPalette16 {
public:
  CRGB entries[16];
  CRGBPalette16() {
    memset(entries, 0, sizeof(entries)); // default constructor: set all to black
  }

  // Create palette from 16 CRGB values
  CRGBPalette16(const CRGB& c00, const CRGB& c01, const CRGB& c02, const CRGB& c03,
                const CRGB& c04, const CRGB& c05, const CRGB& c06, const CRGB& c07,
                const CRGB& c08, const CRGB& c09, const CRGB& c10, const CRGB& c11,
                const CRGB& c12, const CRGB& c13, const CRGB& c14, const CRGB& c15) {
    entries[0]  = c00; entries[1]  = c01; entries[2]  = c02; entries[3]  = c03;
    entries[4]  = c04; entries[5]  = c05; entries[6]  = c06; entries[7]  = c07;
    entries[8]  = c08; entries[9]  = c09; entries[10] = c10; entries[11] = c11;
    entries[12] = c12; entries[13] = c13; entries[14] = c14; entries[15] = c15;
  };

  // Copy constructor
  CRGBPalette16(const CRGBPalette16& rhs) {
    memmove((void*)&(entries[0]), &(rhs.entries[0]), sizeof(entries));
  }

  // Create palette from array of CRGB colors
  CRGBPalette16(const CRGB rhs[16]) {
    memmove((void*)&(entries[0]), &(rhs[0]), sizeof(entries));
  }

  // Copy assignment operator
  CRGBPalette16& operator=(const CRGBPalette16& rhs) {
    memmove((void*)&(entries[0]), &(rhs.entries[0]), sizeof(entries));
    return *this;
  }

  // Create palette from array of CRGB colors
  CRGBPalette16& operator=(const CRGB rhs[16]) {
    memmove((void*)&(entries[0]), &(rhs[0]), sizeof(entries));
    return *this;
  }

  // Create palette from palette stored in PROGMEM
  CRGBPalette16(const TProgmemRGBPalette16& rhs) {
    for (int i = 0; i < 16; ++i) {
      entries[i] = *(const uint32_t*)(rhs + i);
    }
  }

  // Copy assignment operator for PROGMEM palette
  CRGBPalette16& operator=(const TProgmemRGBPalette16& rhs) {
    for (int i = 0; i < 16; ++i) {
      entries[i] = *(const uint32_t*)(rhs + i);
    }
    return *this;
  }

  // Equality operator
  bool operator==(const CRGBPalette16& rhs) const {
    const uint8_t* p = (const uint8_t*)(&(this->entries[0]));
    const uint8_t* q = (const uint8_t*)(&(rhs.entries[0]));
    if (p == q) return true;
    for (unsigned i = 0; i < (sizeof(entries)); ++i) {
      if (*p != *q) return false;
      ++p;
      ++q;
    }
    return true;
  }

  // Inequality operator
  bool operator!=(const CRGBPalette16& rhs) const {
    return !(*this == rhs);
  }

  // Array subscript operator
  inline CRGB& operator[](uint8_t x) __attribute__((always_inline)) {
    return entries[x];
  }

  // Array subscript operator (const)
  inline const CRGB& operator[](uint8_t x) const __attribute__((always_inline)) {
    return entries[x];
  }

  // Array subscript operator
  inline CRGB& operator[](int x) __attribute__((always_inline)) {
    return entries[(uint8_t)x];
  }

  // Array subscript operator (const)
  inline const CRGB& operator[](int x) const __attribute__((always_inline)) {
    return entries[(uint8_t)x];
  }

  // Get the underlying pointer to the CRGB entries making up the palette
  operator CRGB*() {
    return &(entries[0]);
  }

  // Create palette from a single CRGB color
  CRGBPalette16(const CRGB& c1) {
    fill_solid_RGB(&(entries[0]), 16, c1);
  }

  // Create palette from two CRGB colors
  CRGBPalette16(const CRGB& c1, const CRGB& c2) {
    fill_gradient_RGB(&(entries[0]), 16, c1, c2);
  }

  // Create palette from three CRGB colors
  CRGBPalette16(const CRGB& c1, const CRGB& c2, const CRGB& c3) {
    fill_gradient_RGB(&(entries[0]), 16, c1, c2, c3);
  }

  // Create palette from four CRGB colors
  CRGBPalette16(const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4) {
    fill_gradient_RGB(&(entries[0]), 16, c1, c2, c3, c4);
  }

  // Creates a palette from a gradient palette in PROGMEM.
  //
  // Gradient palettes are loaded into CRGBPalettes in such a way
  // that, if possible, every color represented in the gradient palette
  // is also represented in the CRGBPalette, this may not preserve original
  // color spacing, but will try to not omit small color bands.

  CRGBPalette16(TProgmemRGBGradientPalette_bytes progpal) {
    *this = progpal;
  }

  CRGBPalette16& operator=(TProgmemRGBGradientPalette_bytes progpal) {
    TRGBGradientPaletteEntryUnion* progent = (TRGBGradientPaletteEntryUnion*)(progpal);
    TRGBGradientPaletteEntryUnion u;

    // Count entries
    int count = 0;
    do {
      u.dword = *(const uint32_t*)(progent + count);
      ++count;
    } while (u.index != 255);

    int lastSlotUsed = -1;

    u.dword = *(const uint32_t*)(progent);
    CRGB rgbstart(u.r, u.g, u.b);

    int indexstart = 0;
    int istart8 = 0;
    int iend8 = 0;
    while (indexstart < 255) {
      ++progent;
      u.dword = *(const uint32_t*)(progent);
      int indexend = u.index;
      CRGB rgbend(u.r, u.g, u.b);
      istart8 = indexstart / 16;
      iend8 = indexend / 16;
      if (count < 16) {
        if ((istart8 <= lastSlotUsed) && (lastSlotUsed < 15)) {
          istart8 = lastSlotUsed + 1;
          if (iend8 < istart8) {
            iend8 = istart8;
          }
        }
        lastSlotUsed = iend8;
      }
      fill_gradient_RGB(&(entries[0]), istart8, rgbstart, iend8, rgbend);
      indexstart = indexend;
      rgbstart = rgbend;
    }
    return *this;
  }

  // Creates a palette from a gradient palette in dynamic (heap) memory.
  CRGBPalette16& loadDynamicGradientPalette(TDynamicRGBGradientPalette_bytes gpal) {
    TRGBGradientPaletteEntryUnion* ent = (TRGBGradientPaletteEntryUnion*)(gpal);
    TRGBGradientPaletteEntryUnion u;

    // Count entries
    unsigned count = 0;
    do {
      u = *(ent + count);
      ++count;
    } while (u.index != 255);

    int lastSlotUsed = -1;

    u = *ent;
    CRGB rgbstart(u.r, u.g, u.b);

    int indexstart = 0;
    int istart8 = 0;
    int iend8 = 0;
    while (indexstart < 255) {
      ++ent;
      u = *ent;
      int indexend = u.index;
      CRGB rgbend(u.r, u.g, u.b);
      istart8 = indexstart / 16;
      iend8 = indexend / 16;
      if (count < 16) {
        if ((istart8 <= lastSlotUsed) && (lastSlotUsed < 15)) {
          istart8 = lastSlotUsed + 1;
          if (iend8 < istart8) {
            iend8 = istart8;
          }
        }
        lastSlotUsed = iend8;
      }
      fill_gradient_RGB(&(entries[0]), istart8, rgbstart, iend8, rgbend);
      indexstart = indexend;
      rgbstart = rgbend;
    }
    return *this;
  }
};

#endif