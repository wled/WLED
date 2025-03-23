#ifndef WLED_COLORS_H
#define WLED_COLORS_H

// note: some functions/structs have been copied from fastled library, modified and optimized for WLED

#define ColorFromPalette ColorFromPaletteWLED // override fastled version //!!!todo: rename

// 32bit color mangling macros
#define RGBW32(r,g,b,w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#define R(c) (byte((c) >> 16))
#define G(c) (byte((c) >> 8))
#define B(c) (byte(c))
#define W(c) (byte((c) >> 24))

//forward declarations
struct CRGB;
struct CHSV;
struct CRGBW;
struct CHSV32;
class CRGBPalette16;
uint8_t scale8(uint8_t i, uint8_t scale);
uint8_t qadd8(uint8_t i, uint8_t j);
uint8_t qsub8(uint8_t i, uint8_t j);
int8_t abs8(int8_t i);

typedef uint32_t TProgmemRGBPalette16[16];
typedef uint8_t TDynamicRGBGradientPalette_byte;  ///< Byte of an RGB gradient entry, stored in dynamic (heap) memory
typedef const TDynamicRGBGradientPalette_byte *TDynamicRGBGradientPalette_bytes;  ///< Pointer to bytes of an RGB gradient, stored in dynamic (heap) memory
typedef TDynamicRGBGradientPalette_bytes TDynamicRGBGradientPalettePtr;  ///< Alias of ::TDynamicRGBGradientPalette_bytes
typedef const uint8_t TProgmemRGBGradientPalette_byte;
typedef const TProgmemRGBGradientPalette_byte *TProgmemRGBGradientPalette_bytes;
typedef TProgmemRGBGradientPalette_bytes TProgmemRGBGradientPalettePtr;

/// Color interpolation options for palette
typedef enum {
  NOBLEND=0,            ///< No interpolation between palette entries
  LINEARBLEND=1,        ///< Linear interpolation between palette entries, with wrap-around from end to the beginning again
  LINEARBLEND_NOWRAP=2  ///< Linear interpolation between palette entries, but no wrap-around
} TBlendType;

typedef union {
  struct {
    uint8_t index;  ///< index of the color entry in the gradient
    uint8_t r;      ///< CRGB::red channel value of the color entry
    uint8_t g;      ///< CRGB::green channel value of the color entry
    uint8_t b;      ///< CRGB::blue channel value of the color entry
  };
  uint32_t dword;     ///< values as a packed 32-bit double word
  uint8_t  bytes[4];  ///< values as an array
} TRGBGradientPaletteEntryUnion;

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
[[gnu::hot, gnu::pure]] uint32_t ColorFromPaletteWLED(const CRGBPalette16 &pal, unsigned index, uint8_t brightness = (uint8_t)255U, TBlendType blendType = LINEARBLEND);
CRGBPalette16 generateHarmonicRandomPalette(const CRGBPalette16 &basepalette);
CRGBPalette16 generateRandomPalette();

//void hsv2rgb(const CHSV32& hsv, uint32_t& rgb);
void hsv2rgb(const CHSV32& hsv, CRGBW& rgb);
CRGB hsv2rgb(const CHSV& hsv);
//void hsv2rgb_rainbow16(const CHSV32& hsv, CRGBW& rgb);
void hsv2rgb_rainbow16(uint16_t h, uint8_t s, uint8_t v, uint8_t* rgbdata, bool isRGBW = false); 
void hsv2rgb_rainbow(const CHSV& hsv, CRGB& rgb);
void colorHStoRGB(uint16_t hue, byte sat, byte* rgb);
void rgb2hsv(const uint32_t rgbw, CHSV32& hsv);
CHSV rgb2hsv(const CRGB c);
void colorKtoRGB(uint16_t kelvin, byte* rgb);
void colorCTtoRGB(uint16_t mired, byte* rgb); //white spectrum to rgb
CRGB HeatColor(uint8_t temperature); // black body radiation
void colorXYtoRGB(float x, float y, byte* rgb); // only defined if huesync disabled TODO
void colorRGBtoXY(const byte* rgb, float* xy); // only defined if huesync disabled TODO
void colorFromDecOrHexString(byte* rgb, const char* in);
bool colorFromHexString(byte* rgb, const char* in);
uint32_t colorBalanceFromKelvin(uint16_t kelvin, uint32_t rgb);
uint16_t approximateKelvinFromRGB(uint32_t rgb);
void setRandomColor(byte* rgb);
void fill_solid_RGB(CRGB* colors, uint32_t num, const CRGB& c1) ;
void fill_gradient_RGB(CRGB* colors, uint32_t startpos, CRGB startcolor, uint32_t endpos, CRGB endcolor);
void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2);
void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2, const CRGB& c3);
void fill_gradient_RGB(CRGB* colors, uint32_t num, const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4);
void nblendPaletteTowardPalette(CRGBPalette16& current, CRGBPalette16& target, uint8_t maxChanges);

//test only: remove again once settled
void hsv2rgb_rainbow16_ptr(const CHSV32& hsv, uint8_t* rgb);


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

  // Default constructor
  // @warning Default values are UNITIALIZED!
  inline CHSV() __attribute__((always_inline)) = default;

  ///Allow construction from hue, saturation, and value
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
    : r(R(colorcode)), g(G(colorcode)), b(B(colorcode)) { }

  // allow copy construction
  inline CRGB(const CRGB& rhs) __attribute__((always_inline)) = default;

  // allow construction from a CHSV color
  inline CRGB(const CHSV& rhs) __attribute__((always_inline))  {
    hsv2rgb_rainbow(rhs, *this);
  }
  // allow assignment from hue, saturation, and value
  inline CRGB& setHSV (uint8_t hue, uint8_t sat, uint8_t val) __attribute__((always_inline))
  {
    hsv2rgb_rainbow(CHSV(hue, sat, val), *this);
    return *this;
  }

  // Allow assignment from just a hue, sat and val are set to max
  inline CRGB& setHue (uint8_t hue) __attribute__((always_inline))
  {
    hsv2rgb_rainbow(CHSV(hue, 255, 255), *this);
    return *this;
  }

  /// Allow assignment from HSV color
  inline CRGB& operator= (const CHSV& rhs) __attribute__((always_inline))
  {
    hsv2rgb_rainbow(rhs, *this);
    return *this;
  }
  // allow assignment from one RGB struct to another
  inline CRGB& operator= (const CRGB& rhs) __attribute__((always_inline)) = default;

  // allow assignment from 32-bit (really 24-bit) 0xRRGGBB color code
  inline CRGB& operator= (const uint32_t colorcode) __attribute__((always_inline)) {
    r = R(colorcode);
    g = G(colorcode);
    b = B(colorcode);
    return *this;
  }

  /// allow assignment from red, green, and blue
  inline CRGB& setRGB (uint8_t nr, uint8_t ng, uint8_t nb) __attribute__((always_inline)) {
    r = nr;
    g = ng;
    b = nb;
    return *this;
  }
#define R(c) (byte((c) >> 16))
#define G(c) (byte((c) >> 8))
#define B(c) (byte(c))

  /// Allow assignment from 32-bit (really 24-bit) 0xRRGGBB color code
  inline CRGB& setColorCode (uint32_t colorcode) __attribute__((always_inline)) {
    r = R(colorcode);
    g = G(colorcode);
    b = B(colorcode);
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
    unsigned red = (unsigned)r * (unsigned)d;
    unsigned green = (unsigned)r * (unsigned)d;
    unsigned blue = (unsigned)r * (unsigned)d;
    if(red > 255) red = 255;
    if(green > 255) green = 255;
    if(blue > 255) blue = 255;
    r = red;
    g = green;
    b = blue;
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

  /// Return a CRGB object that is a scaled down version of this object
  inline CRGB scale8(uint8_t scaledown) const {
    CRGB out = *this;
    uint32_t scale_fixed = scaledown + 1;
    out.r = (((uint32_t)out.r) * scale_fixed) >> 8;
    out.g = (((uint32_t)out.g) * scale_fixed) >> 8;
    out.b = (((uint32_t)out.b) * scale_fixed) >> 8;
    return out;
  }

  /// Return a CRGB object that is a scaled down version of this object
  inline CRGB scale8(const CRGB& scaledown) const {
    CRGB out;
    out.r = ::scale8(r, scaledown.r);
    out.g = ::scale8(g, scaledown.g);
    out.b = ::scale8(b, scaledown.b);
    return out;
  }

  /// fadeToBlackBy is a synonym for nscale8(), as a fade instead of a scale
  /// @param fadefactor the amount to fade, sent to nscale8() as (255 - fadefactor)
  inline CRGB& fadeToBlackBy(uint8_t fadefactor) {
    uint32_t scale_fixed = 256 - fadefactor;
    r = (((uint32_t)r) * scale_fixed) >> 8;
    g = (((uint32_t)g) * scale_fixed) >> 8;
    b = (((uint32_t)b) * scale_fixed) >> 8;
    return *this;
  }

  /// "or" operator brings each channel up to the higher of the two values
  inline CRGB& operator|=(const CRGB& rhs) {
    if (rhs.r > r) r = rhs.r;
    if (rhs.g > g) g = rhs.g;
    if (rhs.b > b) b = rhs.b;
    return *this;
  }

  /// @copydoc operator|=
  inline CRGB& operator|=(uint8_t d) {
    if (d > r) r = d;
    if (d > g) g = d;
    if (d > b) b = d;
    return *this;
  }

  /// "and" operator brings each channel down to the lower of the two values
  inline CRGB& operator&=(const CRGB& rhs) {
    if (rhs.r < r) r = rhs.r;
    if (rhs.g < g) g = rhs.g;
    if (rhs.b < b) b = rhs.b;
    return *this;
  }

  /// @copydoc operator&=
  inline CRGB& operator&=(uint8_t d) {
    if (d < r) r = d;
    if (d < g) g = d;
    if (d < b) b = d;
    return *this;
  }

  /// This allows testing a CRGB for zero-ness
  inline explicit operator bool() const __attribute__((always_inline)) {
    return r || g || b;
  }

  /// Converts a CRGB to a 32-bit color with white = 0
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
    for (int i = 0; i < (sizeof(entries)); ++i) {
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
    uint32_t count = 0;
    do {
      u.dword = *(const uint32_t*)(progent + count);
      ++count;
    } while (u.index != 255);

    int32_t lastSlotUsed = -1;

    u.dword = *(const uint32_t*)(progent);
    CRGB rgbstart(u.r, u.g, u.b);

    uint32_t indexstart = 0;
    uint32_t istart8 = 0;
    uint32_t iend8 = 0;
    while (indexstart < 255) {
      ++progent;
      u.dword = *(const uint32_t*)(progent);
      uint32_t indexend = u.index;
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
    uint16_t count = 0;
    do {
      u = *(ent + count);
      ++count;
    } while (u.index != 255);

    int8_t lastSlotUsed = -1;

    u = *ent;
    CRGB rgbstart(u.r, u.g, u.b);

    int indexstart = 0;
    uint8_t istart8 = 0;
    uint8_t iend8 = 0;
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

    // Access as an array
    inline const uint8_t& operator[](uint8_t x) const __attribute__((always_inline)) { return raw[x]; }

    // Assignment from 32-bit color
    inline CRGBW& operator=(uint32_t color) __attribute__((always_inline)) { color32 = color; return *this; }

    // Assignment from r, g, b, w
    inline CRGBW& operator=(const CRGB& rgb) __attribute__((always_inline)) { b = rgb.b; g = rgb.g; r = rgb.r; w = 0; return *this; }

    // Conversion operator to uint32_t
    inline operator uint32_t() const __attribute__((always_inline)) {
      return color32;
    }

    // get the average of the R, G, B and W values
    uint8_t getAverageLight() const {
      return (r + g + b + w) >> 2;
    }
  };

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
  inline CHSV32(const uint32_t rgb) __attribute__((always_inline)) {
    rgb2hsv(rgb, *this);
  }
  inline CHSV32& operator= (const uint32_t& rgb) __attribute__((always_inline)) { // assignment from 32bit rgb color (white channel is ignored)
      rgb2hsv(rgb, *this);
      return *this;
  }
};

#endif