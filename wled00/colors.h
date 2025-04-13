#ifndef WLED_COLORS_H
#define WLED_COLORS_H
#include "src/dependencies/fastled/fastled_fcn.h"
/*
  Note on color types and conversions:
  - WLED uses 32bit colors (RGBW), if possible, use CRGBW instead of CRGB for better performance (no conversion in setPixelColor)
  - use CRGB if RAM usage is of concern (i.e. for larger color arrays)
  - direct conversion (assignment or construction) from CHSV/CHSV32 to CRGB/CRGBW use the "rainbow" method (nicer colors, see fastled documentation)
  - converting CRGB(W) to HSV32 color is quite accurate but still not 100% (but much more accurate than fastled's "hsv2rgb_approximate" function)
  - when converting CRGB(W) to HSV32 and back, "hsv2rgb_spectrum" preserves the colors better than the _rainbow version
  - to manipulate an RGB color in HSV space, use the adjust_color function or the CRGBW.adjust_hue method

  Some functions in this file are derived from FastLED (https://github.com/FastLED/FastLED) licensed under the MIT license.
  See /src/dependencies/fastled/LICENSE.txt for details.
*/

// 32bit color mangling macros
#define RGBW32(r,g,b,w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#define R(c) (byte((c) >> 16))
#define G(c) (byte((c) >> 8))
#define B(c) (byte(c))
#define W(c) (byte((c) >> 24))

struct CRGBW;
struct CHSV32;

// similar to NeoPixelBus NeoGammaTableMethod but allows dynamic changes (superseded by NPB::NeoGammaDynamicTableMethod)
class NeoGammaWLEDMethod {
  public:
    [[gnu::hot]] static uint8_t Correct(uint8_t value);         // apply Gamma to single channel
    [[gnu::hot]] static uint32_t Correct32(uint32_t color);     // apply Gamma to RGBW32 color (WLED specific, not used by NPB)
    static void calcGammaTable(float gamma);                              // re-calculates & fills gamma table
    static inline uint8_t rawGamma8(uint8_t val) { return gammaT[val]; }  // get value from Gamma table (WLED specific, not used by NPB)
  private:
    static uint8_t gammaT[];
};
#define gamma32(c) NeoGammaWLEDMethod::Correct32(c)
#define gamma8(c)  NeoGammaWLEDMethod::rawGamma8(c)
[[gnu::hot, gnu::pure]] uint32_t color_blend(uint32_t c1, uint32_t c2 , uint8_t blend);
inline uint32_t color_blend16(uint32_t c1, uint32_t c2, uint16_t b) { return color_blend(c1, c2, b >> 8); };
[[gnu::hot, gnu::pure]] uint32_t color_add(uint32_t, uint32_t, bool preserveCR = false);
[[gnu::hot, gnu::pure]] uint32_t color_fade(uint32_t c1, uint8_t amount, bool video=false);
void adjust_color(CRGBW& rgb, int32_t hueShift, int32_t valueChange, int32_t satChange);
[[gnu::hot, gnu::pure]] uint32_t ColorFromPalette(const CRGBPalette16 &pal, unsigned index, uint8_t brightness = (uint8_t)255U, TBlendType blendType = LINEARBLEND);
CRGBPalette16 generateHarmonicRandomPalette(const CRGBPalette16 &basepalette);
CRGBPalette16 generateRandomPalette();


void hsv2rgb_spectrum(const CHSV32& hsv, CRGBW& rgb);
void hsv2rgb_spectrum(const CHSV& hsv, CRGB& rgb);
void rgb2hsv(const CRGBW& rgb, CHSV32& hsv);
CHSV rgb2hsv(const CRGB c);
void colorHStoRGB(uint16_t hue, byte sat, byte* rgb);
void colorKtoRGB(uint16_t kelvin, byte* rgb);
void colorCTtoRGB(uint16_t mired, byte* rgb); //white spectrum to rgb
void colorXYtoRGB(float x, float y, byte* rgb); // only defined if huesync disabled TODO
void colorRGBtoXY(const byte* rgb, float* xy); // only defined if huesync disabled TODO
void colorFromDecOrHexString(byte* rgb, const char* in);
bool colorFromHexString(byte* rgb, const char* in);
uint32_t colorBalanceFromKelvin(uint16_t kelvin, uint32_t rgb);
uint16_t approximateKelvinFromRGB(uint32_t rgb);
void setRandomColor(byte* rgb);

struct CHSV32 { // 32bit HSV color with 16bit hue for more accurate conversions
  union {
    struct {
        uint16_t h;  // hue
        uint8_t s;   // saturation
        uint8_t v;   // value
    };
    uint32_t hsv32;    // 32bit access
  };
  inline CHSV32() __attribute__((always_inline)) = default; // default constructor

  // allow construction from hue, saturation, and value
  inline CHSV32(uint16_t ih, uint8_t is, uint8_t iv) __attribute__((always_inline)) // constructor from 16bit h, s, v
        : h(ih), s(is), v(iv) {}

  inline CHSV32(uint8_t ih, uint8_t is, uint8_t iv) __attribute__((always_inline)) // constructor from 8bit h, s, v
        : h((uint16_t)ih << 8), s(is), v(iv) {}

  inline CHSV32(const CHSV& chsv) __attribute__((always_inline))  // constructor from CHSV
    : h((uint16_t)chsv.h << 8), s(chsv.s), v(chsv.v) {}

  inline operator CHSV() const { return CHSV((uint8_t)(h >> 8), s, v); } // typecast to CHSV

  // construction from a 32bit rgb color (white channel is ignored)
  inline CHSV32(const CRGBW& rgb) __attribute__((always_inline));

  inline CHSV32& operator= (const CRGBW& rgb) __attribute__((always_inline)); // assignment from 32bit rgb color (white channel is ignored)
};

// CRGBW can be used to manipulate 32bit colors faster. However: if it is passed to functions, it adds overhead compared to a uint32_t color
// use with caution and pay attention to flash size. Usually converting a uint32_t to CRGBW to extract r, g, b, w values is slower than using bitshifts
// it can be useful to avoid back and forth conversions between uint32_t and fastled CRGB
struct CRGBW {
  union {
    uint32_t color32; // Access as a 32-bit value (0xWWRRGGBB)
    struct {
      uint8_t b;
      uint8_t g;
      uint8_t r;
      uint8_t w;
    };
    uint8_t raw[4];   // Access as an array in the order B, G, R, W (matches 32 bit colors)
  };

  // Default constructor
  inline CRGBW() __attribute__((always_inline)) = default;

  // Constructor from a 32-bit color (0xWWRRGGBB)
  constexpr CRGBW(uint32_t color) __attribute__((always_inline)) : color32(color) {}

  // Constructor with r, g, b, w values
  constexpr CRGBW(uint8_t red, uint8_t green, uint8_t blue, uint8_t white = 0) __attribute__((always_inline)) : b(blue), g(green), r(red), w(white) {}

  // Constructor from CRGB
  constexpr CRGBW(CRGB rgb) __attribute__((always_inline)) : b(rgb.b), g(rgb.g), r(rgb.r), w(0) {}

  // Constructor from CHSV32
  inline CRGBW(CHSV32 hsv) __attribute__((always_inline)) { hsv2rgb_rainbow(hsv.h, hsv.s, hsv.v, raw, true); }

  // Constructor from CHSV
  inline CRGBW(CHSV hsv) __attribute__((always_inline)) { hsv2rgb_rainbow(hsv.h<<8, hsv.s, hsv.v, raw, true); }

  // Access as an array
  inline const uint8_t& operator[](uint8_t x) const __attribute__((always_inline)) { return raw[x]; }

  // Assignment from 32-bit color
  inline CRGBW& operator=(uint32_t color) __attribute__((always_inline)) { color32 = color; return *this; }

  // Assignment from CHSV32
  inline CRGBW& operator=(CHSV32 hsv) __attribute__((always_inline)) { hsv2rgb_rainbow(hsv.h, hsv.s, hsv.v, raw, true); return *this; }

  // Assignment from CHSV
  inline CRGBW& operator=(CHSV hsv) __attribute__((always_inline)) { hsv2rgb_rainbow(hsv.h<<8, hsv.s, hsv.v, raw, true); return *this; }

  // Assignment from r, g, b, w
  inline CRGBW& operator=(const CRGB& rgb) __attribute__((always_inline)) { b = rgb.b; g = rgb.g; r = rgb.r; w = 0; return *this; }

  // Conversion operator to uint32_t
  inline operator uint32_t() const __attribute__((always_inline)) {
    return color32;
  }

  // adjust hue: input range is 256 for full color cycle, input can be negative
  inline void adjust_hue(int hueshift) __attribute__((always_inline)) {
    CHSV32 hsv = *this;
    hsv.h += hueshift << 8;
    hsv2rgb_spectrum(hsv, *this);
  }

  // get the average of the R, G, B and W values
  uint8_t getAverageLight() const {
    return (r + g + b + w) >> 2;
  }
};

inline CHSV32::CHSV32(const CRGBW& rgb) {
  rgb2hsv(rgb, *this);
}

inline CHSV32& CHSV32::operator= (const CRGBW& rgb) { // assignment from 32bit rgb color (white channel is ignored)
  rgb2hsv(rgb, *this);
  return *this;
}


#endif