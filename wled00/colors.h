#pragma once
#ifndef WLED_COLORS_H
#define WLED_COLORS_H

/*
 * Color structs and color utility functions
 */
#include <vector>
#include "FastLED.h"

#define ColorFromPalette ColorFromPaletteWLED // override fastled version

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
        uint8_t raw[4];   // Access as an array in the order B, G, R, W
    };

    // Default constructor
    inline CRGBW() __attribute__((always_inline)) = default;

    // Constructor from a 32-bit color (0xWWRRGGBB)
    constexpr CRGBW(uint32_t color) __attribute__((always_inline)) : color32(color) {}

    // Constructor with r, g, b, w values
    constexpr CRGBW(uint8_t red, uint8_t green, uint8_t blue, uint8_t white = 0) __attribute__((always_inline)) : b(blue), g(green), r(red), w(white) {}

    // Constructor from CRGB
    constexpr CRGBW(CRGB rgb) __attribute__((always_inline)) : b(rgb.b), g(rgb.g), r(rgb.r), w(0) {}

    // Access as an array
    inline const uint8_t& operator[] (uint8_t x) const __attribute__((always_inline)) { return raw[x]; }

    // Assignment from 32-bit color
    inline CRGBW& operator=(uint32_t color) __attribute__((always_inline)) { color32 = color; return *this; }

    // Assignment from r, g, b, w
    inline CRGBW& operator=(const CRGB& rgb) __attribute__((always_inline)) { b = rgb.b; g = rgb.g; r = rgb.r; w = 0; return *this; }

    // Conversion operator to uint32_t
    inline operator uint32_t() const __attribute__((always_inline)) {
      return color32;
    }
    /*
    // Conversion operator to CRGB
    inline operator CRGB() const __attribute__((always_inline)) {
      return CRGB(r, g, b);
    }

    CRGBW& scale32 (uint8_t scaledown) // 32bit math
    {
      if (color32 == 0) return *this; // 2 extra instructions, worth it if called a lot on black (which probably is true) adding check if scaledown is zero adds much more overhead as its 8bit
      uint32_t scale = scaledown + 1;
      uint32_t rb = (((color32 & 0x00FF00FF) * scale) >> 8) & 0x00FF00FF; // scale red and blue
      uint32_t wg = (((color32 & 0xFF00FF00) >> 8) * scale) & 0xFF00FF00; // scale white and green
          color32 =  rb | wg;
      return *this;
    }*/

};

struct CHSV32 { // 32bit HSV color with 16bit hue for more accurate conversions
  union {
    struct {
        uint16_t h;  // hue
        uint8_t s;   // saturation
        uint8_t v;   // value
    };
    uint32_t raw;    // 32bit access
  };
  inline CHSV32() __attribute__((always_inline)) = default; // default constructor

    /// Allow construction from hue, saturation, and value
    /// @param ih input hue
    /// @param is input saturation
    /// @param iv input value
  inline CHSV32(uint16_t ih, uint8_t is, uint8_t iv) __attribute__((always_inline)) // constructor from 16bit h, s, v
        : h(ih), s(is), v(iv) {}
  inline CHSV32(uint8_t ih, uint8_t is, uint8_t iv) __attribute__((always_inline)) // constructor from 8bit h, s, v
        : h((uint16_t)ih << 8), s(is), v(iv) {}
  inline CHSV32(const CHSV& chsv) __attribute__((always_inline))  // constructor from CHSV
    : h((uint16_t)chsv.h << 8), s(chsv.s), v(chsv.v) {}
  inline operator CHSV() const { return CHSV((uint8_t)(h >> 8), s, v); } // typecast to CHSV
};
extern bool gammaCorrectCol;
// similar to NeoPixelBus NeoGammaTableMethod but allows dynamic changes (superseded by NPB::NeoGammaDynamicTableMethod)
class NeoGammaWLEDMethod {
  public:
    [[gnu::hot]] static uint8_t Correct(uint8_t value);             // apply Gamma to single channel
    [[gnu::hot]] static uint32_t inverseGamma32(uint32_t color);    // apply inverse Gamma to RGBW32 color
    static void calcGammaTable(float gamma);                        // re-calculates & fills gamma tables
    static inline uint8_t rawGamma8(uint8_t val) { return gammaT[val]; }  // get value from Gamma table (WLED specific, not used by NPB)
    static inline uint8_t rawInverseGamma8(uint8_t val) { return gammaT_inv[val]; }  // get value from inverse Gamma table (WLED specific, not used by NPB)
    static inline uint32_t Correct32(uint32_t color) { // apply Gamma to RGBW32 color (WLED specific, not used by NPB)
      if (!gammaCorrectCol) return color; // no gamma correction
      uint8_t  w = byte(color>>24), r = byte(color>>16), g = byte(color>>8), b = byte(color); // extract r, g, b, w channels
      w = gammaT[w]; r = gammaT[r]; g = gammaT[g]; b = gammaT[b];
      return (uint32_t(w) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }
  private:
    static uint8_t gammaT[];
    static uint8_t gammaT_inv[];
};
#define gamma32(c) NeoGammaWLEDMethod::Correct32(c)
#define gamma8(c)  NeoGammaWLEDMethod::rawGamma8(c)
#define gamma32inv(c) NeoGammaWLEDMethod::inverseGamma32(c)
#define gamma8inv(c)  NeoGammaWLEDMethod::rawInverseGamma8(c)
[[gnu::hot, gnu::pure]] uint32_t color_blend(uint32_t c1, uint32_t c2 , uint8_t blend);
inline uint32_t color_blend16(uint32_t c1, uint32_t c2, uint16_t b) { return color_blend(c1, c2, b >> 8); };
[[gnu::hot, gnu::pure]] uint32_t color_add(uint32_t, uint32_t, bool preserveCR = false);
[[gnu::hot, gnu::pure]] uint32_t color_fade(uint32_t c1, uint8_t amount, bool video = false);
[[gnu::hot, gnu::pure]] uint32_t adjust_color(uint32_t rgb, uint32_t hueShift, uint32_t lighten, uint32_t brighten);
[[gnu::hot, gnu::pure]] uint32_t ColorFromPaletteWLED(const CRGBPalette16 &pal, unsigned index, uint8_t brightness = (uint8_t)255U, TBlendType blendType = LINEARBLEND);
CRGBPalette16 generateHarmonicRandomPalette(const CRGBPalette16 &basepalette);
CRGBPalette16 generateRandomPalette();
void loadCustomPalettes();
extern std::vector<CRGBPalette16> customPalettes;
inline size_t getPaletteCount() { return FIXED_PALETTE_COUNT + customPalettes.size(); }
inline uint32_t colorFromRgbw(byte* rgbw) { return uint32_t((byte(rgbw[3]) << 24) | (byte(rgbw[0]) << 16) | (byte(rgbw[1]) << 8) | (byte(rgbw[2]))); }
void hsv2rgb(const CHSV32& hsv, uint32_t& rgb);
void colorHStoRGB(uint16_t hue, byte sat, byte* rgb);
void rgb2hsv(const uint32_t rgb, CHSV32& hsv);
inline CHSV rgb2hsv(const CRGB c) { CHSV32 hsv; rgb2hsv((uint32_t((byte(c.r) << 16) | (byte(c.g) << 8) | (byte(c.b)))), hsv); return CHSV(hsv); } // CRGB to hsv
void colorKtoRGB(uint16_t kelvin, byte* rgb);
void colorCTtoRGB(uint16_t mired, byte* rgb); //white spectrum to rgb
void colorXYtoRGB(float x, float y, byte* rgb); // only defined if huesync disabled TODO
void colorRGBtoXY(const byte* rgb, float* xy); // only defined if huesync disabled TODO
void colorFromDecOrHexString(byte* rgb, const char* in);
bool colorFromHexString(byte* rgb, const char* in);
uint32_t colorBalanceFromKelvin(uint16_t kelvin, uint32_t rgb);
uint16_t approximateKelvinFromRGB(uint32_t rgb);
void setRandomColor(byte* rgb);

// fast scaling function for colors, performs color*scale/256 for all four channels, speed over accuracy
// note: inlining uses less code than actual function calls
static inline uint32_t fast_color_scale(const uint32_t c, const uint8_t scale) {
  uint32_t rb = (((c     & 0x00FF00FF) * scale) >> 8) &  0x00FF00FF;
  uint32_t wg = (((c>>8) & 0x00FF00FF) * scale)       & ~0x00FF00FF;
  return rb | wg;
}

// palettes
extern const TProgmemRGBPalette16* const fastledPalettes[];
extern const uint8_t* const gGradientPalettes[];
#endif

