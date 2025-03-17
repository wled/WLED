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
uint8_t scale8( uint8_t i, uint8_t scale );
uint8_t qadd8( uint8_t i, uint8_t j);
uint8_t qsub8( uint8_t i, uint8_t j);
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

void hsv2rgb(const CHSV32& hsv, uint32_t& rgb);
CRGB hsv2rgb(const CHSV& hsv);
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
    inline CHSV( uint8_t ih, uint8_t is, uint8_t iv) __attribute__((always_inline))
        : h(ih), s(is), v(iv)
    {
    }

    /// Allow copy construction
    inline CHSV(const CHSV& rhs) __attribute__((always_inline)) = default;

    /// Allow copy construction
    inline CHSV& operator= (const CHSV& rhs) __attribute__((always_inline)) = default;

    /// Assign new HSV values
    inline CHSV& setHSV(uint8_t ih, uint8_t is, uint8_t iv) __attribute__((always_inline)) {
        h = ih;
        s = is;
        v = iv;
        return *this;
    }
};

// Representation of an RGB pixel (Red, Green, Blue)
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

  inline uint8_t& operator[] (uint8_t x) __attribute__((always_inline)) {
    return raw[x];
  }

  /// Array access operator to index into the CRGB object
  /// @param x the index to retrieve (0-2)
  /// @returns the CRGB::raw value for the given index
  inline const uint8_t& operator[] (uint8_t x) const __attribute__((always_inline))
  {
    return raw[x];
  }

  /// Default constructor
  /// @warning Default values are UNITIALIZED!
  inline CRGB() __attribute__((always_inline)) = default;

  /// Allow construction from red, green, and blue
  /// @param ir input red value
  /// @param ig input green value
  /// @param ib input blue value
  inline CRGB( uint8_t ir, uint8_t ig, uint8_t ib)  __attribute__((always_inline)): r(ir), g(ig), b(ib)
  {
  }

  /// Allow construction from 32-bit (really 24-bit) bit 0xRRGGBB color code
  /// @param colorcode a packed 24 bit color code
  inline CRGB( uint32_t colorcode)  __attribute__((always_inline))
  : r((colorcode >> 16) & 0xFF), g((colorcode >> 8) & 0xFF), b((colorcode >> 0) & 0xFF)
  {
  }
/*
  /// Allow construction from a LEDColorCorrection enum
  /// @param colorcode an LEDColorCorrect enumeration value
  inline CRGB( LEDColorCorrection colorcode) __attribute__((always_inline))
  : r((colorcode >> 16) & 0xFF), g((colorcode >> 8) & 0xFF), b((colorcode >> 0) & 0xFF)
  {

  }

  /// Allow construction from a ColorTemperature enum
  /// @param colorcode an ColorTemperature enumeration value
  inline CRGB( ColorTemperature colorcode) __attribute__((always_inline))
  : r((colorcode >> 16) & 0xFF), g((colorcode >> 8) & 0xFF), b((colorcode >> 0) & 0xFF)
  {

  }
*/
  /// Allow copy construction
  inline CRGB(const CRGB& rhs) __attribute__((always_inline)) = default;

  /// Allow construction from a CHSV color
  inline CRGB(const CHSV& rhs) __attribute__((always_inline))  {
    hsv2rgb_rainbow( rhs, *this);
  }
  /// Allow assignment from hue, saturation, and value
  inline CRGB& setHSV (uint8_t hue, uint8_t sat, uint8_t val) __attribute__((always_inline))
  {
      hsv2rgb_rainbow( CHSV(hue, sat, val), *this);
      return *this;
  }

  /// Allow assignment from just a hue.
  /// Saturation and value (brightness) are set automatically to max.
  /// @param hue color hue
  inline CRGB& setHue (uint8_t hue) __attribute__((always_inline))
  {
      hsv2rgb_rainbow( CHSV(hue, 255, 255), *this);
      return *this;
  }

  /// Allow assignment from HSV color
  inline CRGB& operator= (const CHSV& rhs) __attribute__((always_inline))
  {
      hsv2rgb_rainbow(rhs, *this);
      return *this;
  }
  /// Allow assignment from one RGB struct to another
  inline CRGB& operator= (const CRGB& rhs) __attribute__((always_inline)) = default;

  /// Allow assignment from 32-bit (really 24-bit) 0xRRGGBB color code
  /// @param colorcode a packed 24 bit color code
  inline CRGB& operator= (const uint32_t colorcode) __attribute__((always_inline))
  {
      r = (colorcode >> 16) & 0xFF;
      g = (colorcode >>  8) & 0xFF;
      b = (colorcode >>  0) & 0xFF;
      return *this;
  }

  /// Allow assignment from red, green, and blue
  /// @param nr new red value
  /// @param ng new green value
  /// @param nb new blue value
  inline CRGB& setRGB (uint8_t nr, uint8_t ng, uint8_t nb) __attribute__((always_inline))
  {
      r = nr;
      g = ng;
      b = nb;
      return *this;
  }


  /// Allow assignment from 32-bit (really 24-bit) 0xRRGGBB color code
  /// @param colorcode a packed 24 bit color code
  inline CRGB& setColorCode (uint32_t colorcode) __attribute__((always_inline))
  {
      r = (colorcode >> 16) & 0xFF;
      g = (colorcode >>  8) & 0xFF;
      b = (colorcode >>  0) & 0xFF;
      return *this;
  }


  /// Add one CRGB to another, saturating at 0xFF for each channel
  inline CRGB& operator+= (const CRGB& rhs )
  {
      r = qadd8( r, rhs.r);
      g = qadd8( g, rhs.g);
      b = qadd8( b, rhs.b);
      return *this;
  }

  /// Add a constant to each channel, saturating at 0xFF. 
  /// @note This is NOT an operator+= overload because the compiler
  /// can't usefully decide when it's being passed a 32-bit
  /// constant (e.g. CRGB::Red) and an 8-bit one (CRGB::Blue)
  inline CRGB& addToRGB (uint8_t d )
  {
      r = qadd8( r, d);
      g = qadd8( g, d);
      b = qadd8( b, d);
      return *this;
  }

  /// Subtract one CRGB from another, saturating at 0x00 for each channel
  inline CRGB& operator-= (const CRGB& rhs )
  {
      r = qsub8( r, rhs.r);
      g = qsub8( g, rhs.g);
      b = qsub8( b, rhs.b);
      return *this;
  }

  /// Subtract a constant from each channel, saturating at 0x00. 
  /// @note This is NOT an operator+= overload because the compiler
  /// can't usefully decide when it's being passed a 32-bit
  /// constant (e.g. CRGB::Red) and an 8-bit one (CRGB::Blue)
  inline CRGB& subtractFromRGB(uint8_t d )
  {
      r = qsub8( r, d);
      g = qsub8( g, d);
      b = qsub8( b, d);
      return *this;
  }

  /// Subtract a constant of '1' from each channel, saturating at 0x00
  inline CRGB& operator-- ()  __attribute__((always_inline))
  {
      subtractFromRGB(1);
      return *this;
  }

  /// @copydoc operator--
  inline CRGB operator-- (int )  __attribute__((always_inline))
  {
      CRGB retval(*this);
      --(*this);
      return retval;
  }

  /// Add a constant of '1' from each channel, saturating at 0xFF
  inline CRGB& operator++ ()  __attribute__((always_inline))
  {
      addToRGB(1);
      return *this;
  }

  /// @copydoc operator++
  inline CRGB operator++ (int )  __attribute__((always_inline))
  {
      CRGB retval(*this);
      ++(*this);
      return retval;
  }

  /// Divide each of the channels by a constant
  inline CRGB& operator/= (uint8_t d )
  {
      r /= d;
      g /= d;
      b /= d;
      return *this;
  }

  /// Right shift each of the channels by a constant
  inline CRGB& operator>>= (uint8_t d)
  {
      r >>= d;
      g >>= d;
      b >>= d;
      return *this;
  }

  /// Multiply each of the channels by a constant,
  /// saturating each channel at 0xFF.
  inline CRGB& operator*= (uint8_t d )
  {
      unsigned red = (unsigned)r * (unsigned)d;
      unsigned green = (unsigned)r * (unsigned)d;
      unsigned blue = (unsigned)r * (unsigned)d;
      if( red > 255) red = 255;
      if( green > 255) green = 255;
      if( blue > 255) blue = 255;
      r = red;
      g = green;
      b = blue;
      return *this;
  }

  inline CRGB& nscale8_video(uint8_t scaledown) {
    uint8_t nonzeroscale = (scaledown != 0) ? 1 : 0;
    r = (r == 0) ? 0 : (((int)r * (int)(scaledown) ) >> 8) + nonzeroscale;
    g = (g == 0) ? 0 : (((int)g * (int)(scaledown) ) >> 8) + nonzeroscale;
    b = (b == 0) ? 0 : (((int)b * (int)(scaledown) ) >> 8) + nonzeroscale;
    return *this;
  }

/*
    /// fadeLightBy is a synonym for nscale8_video(), as a fade instead of a scale
    /// @param fadefactor the amount to fade, sent to nscale8_video() as (255 - fadefactor)
    inline CRGB& fadeLightBy (uint8_t fadefactor )
    {
        nscale8x3_video( r, g, b, 255 - fadefactor);
        return *this;
    }
*/
    /// Scale down a RGB to N/256ths of its current brightness, using
    /// "plain math" dimming rules. "Plain math" dimming rules means that the low light
    /// levels may dim all the way to 100% black.
    /// @see nscale8x3
    inline CRGB& nscale8 (uint8_t scaledown )
    {
        uint32_t scale_fixed = scaledown + 1;
        r = (((uint32_t)r) * scale_fixed) >> 8;
        g = (((uint32_t)g) * scale_fixed) >> 8;
        b = (((uint32_t)b) * scale_fixed) >> 8;
        return *this;
    }

    /// Scale down a RGB to N/256ths of its current brightness, using
    /// "plain math" dimming rules. "Plain math" dimming rules means that the low light
    /// levels may dim all the way to 100% black.
    /// @see ::scale8
    inline CRGB& nscale8 (const CRGB & scaledown )
    {
        r = ::scale8(r, scaledown.r);
        g = ::scale8(g, scaledown.g);
        b = ::scale8(b, scaledown.b);
        return *this;
    }

    /// Return a CRGB object that is a scaled down version of this object
    inline CRGB scale8 (uint8_t scaledown ) const
    {
        CRGB out = *this;
        uint32_t scale_fixed = scaledown + 1;
        out.r = (((uint32_t)out.r) * scale_fixed) >> 8;
        out.g = (((uint32_t)out.g) * scale_fixed) >> 8;
        out.b = (((uint32_t)out.b) * scale_fixed) >> 8;
        return out;
    }

    /// Return a CRGB object that is a scaled down version of this object
    inline CRGB scale8 (const CRGB & scaledown ) const
    {
        CRGB out;
        out.r = ::scale8(r, scaledown.r);
        out.g = ::scale8(g, scaledown.g);
        out.b = ::scale8(b, scaledown.b);
        return out;
    }

    /// fadeToBlackBy is a synonym for nscale8(), as a fade instead of a scale
    /// @param fadefactor the amount to fade, sent to nscale8() as (255 - fadefactor)
    inline CRGB& fadeToBlackBy (uint8_t fadefactor )
    {
        uint32_t scale_fixed = 256 - fadefactor;
        r = (((uint32_t)r) * scale_fixed) >> 8;
        g = (((uint32_t)g) * scale_fixed) >> 8;
        b = (((uint32_t)b) * scale_fixed) >> 8;
        return *this;
    }

    /// "or" operator brings each channel up to the higher of the two values
    inline CRGB& operator|= (const CRGB& rhs )
    {
        if( rhs.r > r) r = rhs.r;
        if( rhs.g > g) g = rhs.g;
        if( rhs.b > b) b = rhs.b;
        return *this;
    }

    /// @copydoc operator|=
    inline CRGB& operator|= (uint8_t d )
    {
        if( d > r) r = d;
        if( d > g) g = d;
        if( d > b) b = d;
        return *this;
    }

    /// "and" operator brings each channel down to the lower of the two values
    inline CRGB& operator&= (const CRGB& rhs )
    {
        if( rhs.r < r) r = rhs.r;
        if( rhs.g < g) g = rhs.g;
        if( rhs.b < b) b = rhs.b;
        return *this;
    }

    /// @copydoc operator&=
    inline CRGB& operator&= (uint8_t d )
    {
        if( d < r) r = d;
        if( d < g) g = d;
        if( d < b) b = d;
        return *this;
    }

    /// This allows testing a CRGB for zero-ness
    inline explicit operator bool() const __attribute__((always_inline))
    {
        return r || g || b;
    }

    /// Converts a CRGB to a 32-bit color having an alpha of 255.
    inline explicit operator uint32_t() const
    {
        return uint32_t{0xff000000} |
               (uint32_t{r} << 16) |
               (uint32_t{g} << 8) |
               uint32_t{b};
    }

    /// Invert each channel
    inline CRGB operator- () const
    {
        CRGB retval;
        retval.r = 255 - r;
        retval.g = 255 - g;
        retval.b = 255 - b;
        return retval;
    }

    /// Get the average of the R, G, and B values
    inline uint8_t getAverageLight( )  const {
        return ((r + g + b) * 21846) >> 16; // x*21846>>16 is equal to "divide by 3"
    }

    /// Predefined RGB colors
    typedef enum {
        AliceBlue=0xF0F8FF,             ///< @htmlcolorblock{F0F8FF}
        Amethyst=0x9966CC,              ///< @htmlcolorblock{9966CC}
        AntiqueWhite=0xFAEBD7,          ///< @htmlcolorblock{FAEBD7}
        Aqua=0x00FFFF,                  ///< @htmlcolorblock{00FFFF}
        Aquamarine=0x7FFFD4,            ///< @htmlcolorblock{7FFFD4}
        Azure=0xF0FFFF,                 ///< @htmlcolorblock{F0FFFF}
        Beige=0xF5F5DC,                 ///< @htmlcolorblock{F5F5DC}
        Bisque=0xFFE4C4,                ///< @htmlcolorblock{FFE4C4}
        Black=0x000000,                 ///< @htmlcolorblock{000000}
        BlanchedAlmond=0xFFEBCD,        ///< @htmlcolorblock{FFEBCD}
        Blue=0x0000FF,                  ///< @htmlcolorblock{0000FF}
        BlueViolet=0x8A2BE2,            ///< @htmlcolorblock{8A2BE2}
        Brown=0xA52A2A,                 ///< @htmlcolorblock{A52A2A}
        BurlyWood=0xDEB887,             ///< @htmlcolorblock{DEB887}
        CadetBlue=0x5F9EA0,             ///< @htmlcolorblock{5F9EA0}
        Chartreuse=0x7FFF00,            ///< @htmlcolorblock{7FFF00}
        Chocolate=0xD2691E,             ///< @htmlcolorblock{D2691E}
        Coral=0xFF7F50,                 ///< @htmlcolorblock{FF7F50}
        CornflowerBlue=0x6495ED,        ///< @htmlcolorblock{6495ED}
        Cornsilk=0xFFF8DC,              ///< @htmlcolorblock{FFF8DC}
        Crimson=0xDC143C,               ///< @htmlcolorblock{DC143C}
        Cyan=0x00FFFF,                  ///< @htmlcolorblock{00FFFF}
        DarkBlue=0x00008B,              ///< @htmlcolorblock{00008B}
        DarkCyan=0x008B8B,              ///< @htmlcolorblock{008B8B}
        DarkGoldenrod=0xB8860B,         ///< @htmlcolorblock{B8860B}
        DarkGray=0xA9A9A9,              ///< @htmlcolorblock{A9A9A9}
        DarkGrey=0xA9A9A9,              ///< @htmlcolorblock{A9A9A9}
        DarkGreen=0x006400,             ///< @htmlcolorblock{006400}
        DarkKhaki=0xBDB76B,             ///< @htmlcolorblock{BDB76B}
        DarkMagenta=0x8B008B,           ///< @htmlcolorblock{8B008B}
        DarkOliveGreen=0x556B2F,        ///< @htmlcolorblock{556B2F}
        DarkOrange=0xFF8C00,            ///< @htmlcolorblock{FF8C00}
        DarkOrchid=0x9932CC,            ///< @htmlcolorblock{9932CC}
        DarkRed=0x8B0000,               ///< @htmlcolorblock{8B0000}
        DarkSalmon=0xE9967A,            ///< @htmlcolorblock{E9967A}
        DarkSeaGreen=0x8FBC8F,          ///< @htmlcolorblock{8FBC8F}
        DarkSlateBlue=0x483D8B,         ///< @htmlcolorblock{483D8B}
        DarkSlateGray=0x2F4F4F,         ///< @htmlcolorblock{2F4F4F}
        DarkSlateGrey=0x2F4F4F,         ///< @htmlcolorblock{2F4F4F}
        DarkTurquoise=0x00CED1,         ///< @htmlcolorblock{00CED1}
        DarkViolet=0x9400D3,            ///< @htmlcolorblock{9400D3}
        DeepPink=0xFF1493,              ///< @htmlcolorblock{FF1493}
        DeepSkyBlue=0x00BFFF,           ///< @htmlcolorblock{00BFFF}
        DimGray=0x696969,               ///< @htmlcolorblock{696969}
        DimGrey=0x696969,               ///< @htmlcolorblock{696969}
        DodgerBlue=0x1E90FF,            ///< @htmlcolorblock{1E90FF}
        FireBrick=0xB22222,             ///< @htmlcolorblock{B22222}
        FloralWhite=0xFFFAF0,           ///< @htmlcolorblock{FFFAF0}
        ForestGreen=0x228B22,           ///< @htmlcolorblock{228B22}
        Fuchsia=0xFF00FF,               ///< @htmlcolorblock{FF00FF}
        Gainsboro=0xDCDCDC,             ///< @htmlcolorblock{DCDCDC}
        GhostWhite=0xF8F8FF,            ///< @htmlcolorblock{F8F8FF}
        Gold=0xFFD700,                  ///< @htmlcolorblock{FFD700}
        Goldenrod=0xDAA520,             ///< @htmlcolorblock{DAA520}
        Gray=0x808080,                  ///< @htmlcolorblock{808080}
        Grey=0x808080,                  ///< @htmlcolorblock{808080}
        Green=0x008000,                 ///< @htmlcolorblock{008000}
        GreenYellow=0xADFF2F,           ///< @htmlcolorblock{ADFF2F}
        Honeydew=0xF0FFF0,              ///< @htmlcolorblock{F0FFF0}
        HotPink=0xFF69B4,               ///< @htmlcolorblock{FF69B4}
        IndianRed=0xCD5C5C,             ///< @htmlcolorblock{CD5C5C}
        Indigo=0x4B0082,                ///< @htmlcolorblock{4B0082}
        Ivory=0xFFFFF0,                 ///< @htmlcolorblock{FFFFF0}
        Khaki=0xF0E68C,                 ///< @htmlcolorblock{F0E68C}
        Lavender=0xE6E6FA,              ///< @htmlcolorblock{E6E6FA}
        LavenderBlush=0xFFF0F5,         ///< @htmlcolorblock{FFF0F5}
        LawnGreen=0x7CFC00,             ///< @htmlcolorblock{7CFC00}
        LemonChiffon=0xFFFACD,          ///< @htmlcolorblock{FFFACD}
        LightBlue=0xADD8E6,             ///< @htmlcolorblock{ADD8E6}
        LightCoral=0xF08080,            ///< @htmlcolorblock{F08080}
        LightCyan=0xE0FFFF,             ///< @htmlcolorblock{E0FFFF}
        LightGoldenrodYellow=0xFAFAD2,  ///< @htmlcolorblock{FAFAD2}
        LightGreen=0x90EE90,            ///< @htmlcolorblock{90EE90}
        LightGrey=0xD3D3D3,             ///< @htmlcolorblock{D3D3D3}
        LightPink=0xFFB6C1,             ///< @htmlcolorblock{FFB6C1}
        LightSalmon=0xFFA07A,           ///< @htmlcolorblock{FFA07A}
        LightSeaGreen=0x20B2AA,         ///< @htmlcolorblock{20B2AA}
        LightSkyBlue=0x87CEFA,          ///< @htmlcolorblock{87CEFA}
        LightSlateGray=0x778899,        ///< @htmlcolorblock{778899}
        LightSlateGrey=0x778899,        ///< @htmlcolorblock{778899}
        LightSteelBlue=0xB0C4DE,        ///< @htmlcolorblock{B0C4DE}
        LightYellow=0xFFFFE0,           ///< @htmlcolorblock{FFFFE0}
        Lime=0x00FF00,                  ///< @htmlcolorblock{00FF00}
        LimeGreen=0x32CD32,             ///< @htmlcolorblock{32CD32}
        Linen=0xFAF0E6,                 ///< @htmlcolorblock{FAF0E6}
        Magenta=0xFF00FF,               ///< @htmlcolorblock{FF00FF}
        Maroon=0x800000,                ///< @htmlcolorblock{800000}
        MediumAquamarine=0x66CDAA,      ///< @htmlcolorblock{66CDAA}
        MediumBlue=0x0000CD,            ///< @htmlcolorblock{0000CD}
        MediumOrchid=0xBA55D3,          ///< @htmlcolorblock{BA55D3}
        MediumPurple=0x9370DB,          ///< @htmlcolorblock{9370DB}
        MediumSeaGreen=0x3CB371,        ///< @htmlcolorblock{3CB371}
        MediumSlateBlue=0x7B68EE,       ///< @htmlcolorblock{7B68EE}
        MediumSpringGreen=0x00FA9A,     ///< @htmlcolorblock{00FA9A}
        MediumTurquoise=0x48D1CC,       ///< @htmlcolorblock{48D1CC}
        MediumVioletRed=0xC71585,       ///< @htmlcolorblock{C71585}
        MidnightBlue=0x191970,          ///< @htmlcolorblock{191970}
        MintCream=0xF5FFFA,             ///< @htmlcolorblock{F5FFFA}
        MistyRose=0xFFE4E1,             ///< @htmlcolorblock{FFE4E1}
        Moccasin=0xFFE4B5,              ///< @htmlcolorblock{FFE4B5}
        NavajoWhite=0xFFDEAD,           ///< @htmlcolorblock{FFDEAD}
        Navy=0x000080,                  ///< @htmlcolorblock{000080}
        OldLace=0xFDF5E6,               ///< @htmlcolorblock{FDF5E6}
        Olive=0x808000,                 ///< @htmlcolorblock{808000}
        OliveDrab=0x6B8E23,             ///< @htmlcolorblock{6B8E23}
        Orange=0xFFA500,                ///< @htmlcolorblock{FFA500}
        OrangeRed=0xFF4500,             ///< @htmlcolorblock{FF4500}
        Orchid=0xDA70D6,                ///< @htmlcolorblock{DA70D6}
        PaleGoldenrod=0xEEE8AA,         ///< @htmlcolorblock{EEE8AA}
        PaleGreen=0x98FB98,             ///< @htmlcolorblock{98FB98}
        PaleTurquoise=0xAFEEEE,         ///< @htmlcolorblock{AFEEEE}
        PaleVioletRed=0xDB7093,         ///< @htmlcolorblock{DB7093}
        PapayaWhip=0xFFEFD5,            ///< @htmlcolorblock{FFEFD5}
        PeachPuff=0xFFDAB9,             ///< @htmlcolorblock{FFDAB9}
        Peru=0xCD853F,                  ///< @htmlcolorblock{CD853F}
        Pink=0xFFC0CB,                  ///< @htmlcolorblock{FFC0CB}
        Plaid=0xCC5533,                 ///< @htmlcolorblock{CC5533}
        Plum=0xDDA0DD,                  ///< @htmlcolorblock{DDA0DD}
        PowderBlue=0xB0E0E6,            ///< @htmlcolorblock{B0E0E6}
        Purple=0x800080,                ///< @htmlcolorblock{800080}
        Red=0xFF0000,                   ///< @htmlcolorblock{FF0000}
        RosyBrown=0xBC8F8F,             ///< @htmlcolorblock{BC8F8F}
        RoyalBlue=0x4169E1,             ///< @htmlcolorblock{4169E1}
        SaddleBrown=0x8B4513,           ///< @htmlcolorblock{8B4513}
        Salmon=0xFA8072,                ///< @htmlcolorblock{FA8072}
        SandyBrown=0xF4A460,            ///< @htmlcolorblock{F4A460}
        SeaGreen=0x2E8B57,              ///< @htmlcolorblock{2E8B57}
        Seashell=0xFFF5EE,              ///< @htmlcolorblock{FFF5EE}
        Sienna=0xA0522D,                ///< @htmlcolorblock{A0522D}
        Silver=0xC0C0C0,                ///< @htmlcolorblock{C0C0C0}
        SkyBlue=0x87CEEB,               ///< @htmlcolorblock{87CEEB}
        SlateBlue=0x6A5ACD,             ///< @htmlcolorblock{6A5ACD}
        SlateGray=0x708090,             ///< @htmlcolorblock{708090}
        SlateGrey=0x708090,             ///< @htmlcolorblock{708090}
        Snow=0xFFFAFA,                  ///< @htmlcolorblock{FFFAFA}
        SpringGreen=0x00FF7F,           ///< @htmlcolorblock{00FF7F}
        SteelBlue=0x4682B4,             ///< @htmlcolorblock{4682B4}
        Tan=0xD2B48C,                   ///< @htmlcolorblock{D2B48C}
        Teal=0x008080,                  ///< @htmlcolorblock{008080}
        Thistle=0xD8BFD8,               ///< @htmlcolorblock{D8BFD8}
        Tomato=0xFF6347,                ///< @htmlcolorblock{FF6347}
        Turquoise=0x40E0D0,             ///< @htmlcolorblock{40E0D0}
        Violet=0xEE82EE,                ///< @htmlcolorblock{EE82EE}
        Wheat=0xF5DEB3,                 ///< @htmlcolorblock{F5DEB3}
        White=0xFFFFFF,                 ///< @htmlcolorblock{FFFFFF}
        WhiteSmoke=0xF5F5F5,            ///< @htmlcolorblock{F5F5F5}
        Yellow=0xFFFF00,                ///< @htmlcolorblock{FFFF00}
        YellowGreen=0x9ACD32,           ///< @htmlcolorblock{9ACD32}

        // LED RGB color that roughly approximates
        // the color of incandescent fairy lights,
        // assuming that you're using FastLED
        // color correction on your LEDs (recommended).
        FairyLight=0xFFE42D,           ///< @htmlcolorblock{FFE42D}

        // If you are using no color correction, use this
        FairyLightNCC=0xFF9D2A         ///< @htmlcolorblock{FFE42D}

    } HTMLColorCode;
};

__attribute__((always_inline)) inline CRGB operator+(const CRGB& p1, const CRGB& p2) {
    return CRGB( qadd8( p1.r, p2.r), qadd8( p1.g, p2.g), qadd8( p1.b, p2.b));
}

__attribute__((always_inline)) inline CRGB operator-(const CRGB& p1, const CRGB& p2) {
    return CRGB( qsub8( p1.r, p2.r), qsub8( p1.g, p2.g), qsub8( p1.b, p2.b));
}

__attribute__((always_inline)) inline bool operator== (const CRGB& lhs, const CRGB& rhs) {
    return (lhs.r == rhs.r) && (lhs.g == rhs.g) && (lhs.b == rhs.b);
}

__attribute__((always_inline)) inline bool operator!= (const CRGB& lhs, const CRGB& rhs) {
    return !(lhs == rhs);
}

/*
/// HSV color palette with 16 discrete values
class CHSVPalette16 {
public:
    CHSV entries[16];  ///< the color entries that make up the palette

    /// @copydoc CHSV::CHSV()
    CHSVPalette16() {};

    /// Create palette from 16 CHSV values
    CHSVPalette16( const CHSV& c00,const CHSV& c01,const CHSV& c02,const CHSV& c03,
                    const CHSV& c04,const CHSV& c05,const CHSV& c06,const CHSV& c07,
                    const CHSV& c08,const CHSV& c09,const CHSV& c10,const CHSV& c11,
                    const CHSV& c12,const CHSV& c13,const CHSV& c14,const CHSV& c15 )
    {
        entries[0]=c00; entries[1]=c01; entries[2]=c02; entries[3]=c03;
        entries[4]=c04; entries[5]=c05; entries[6]=c06; entries[7]=c07;
        entries[8]=c08; entries[9]=c09; entries[10]=c10; entries[11]=c11;
        entries[12]=c12; entries[13]=c13; entries[14]=c14; entries[15]=c15;
    };

    /// Copy constructor
    CHSVPalette16( const CHSVPalette16& rhs)
    {
        memmove( (void *) &(entries[0]), &(rhs.entries[0]), sizeof( entries));
    }

    /// @copydoc CHSVPalette16(const CHSVPalette16& rhs)
    CHSVPalette16& operator=( const CHSVPalette16& rhs)
    {
        memmove( (void *) &(entries[0]), &(rhs.entries[0]), sizeof( entries));
        return *this;
    }

    /// Create palette from palette stored in PROGMEM
    CHSVPalette16( const TProgmemHSVPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; ++i) {
            CRGB xyz   =  FL_PGM_READ_DWORD_NEAR( rhs + i);
            entries[i].hue = xyz.red;
            entries[i].sat = xyz.green;
            entries[i].val = xyz.blue;
        }
    }

    /// @copydoc CHSVPalette16(const TProgmemHSVPalette16&)
    CHSVPalette16& operator=( const TProgmemHSVPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; ++i) {
            CRGB xyz   =  FL_PGM_READ_DWORD_NEAR( rhs + i);
            entries[i].hue = xyz.red;
            entries[i].sat = xyz.green;
            entries[i].val = xyz.blue;
        }
        return *this;
    }

    /// Array access operator to index into the gradient entries
    /// @param x the index to retrieve
    /// @returns reference to an entry in the palette's color array
    /// @note This does not perform any interpolation like ColorFromPalette(),
    /// it accesses the underlying entries that make up the gradient. Beware
    /// of bounds issues!
    inline CHSV& operator[] (uint8_t x) __attribute__((always_inline))
    {
        return entries[x];
    }

    /// @copydoc operator[]
    inline const CHSV& operator[] (uint8_t x) const __attribute__((always_inline))
    {
        return entries[x];
    }

    /// @copydoc operator[]
    inline CHSV& operator[] (int x) __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }

    /// @copydoc operator[]
    inline const CHSV& operator[] (int x) const __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }

    /// Get the underlying pointer to the CHSV entries making up the palette
    operator CHSV*()
    {
        return &(entries[0]);
    }

    /// Check if two palettes have the same color entries
    bool operator==( const CHSVPalette16 &rhs) const
    {
        const uint8_t* p = (const uint8_t*)(&(this->entries[0]));
        const uint8_t* q = (const uint8_t*)(&(rhs.entries[0]));
        if( p == q) return true;
        for( uint8_t i = 0; i < (sizeof( entries)); ++i) {
            if( *p != *q) return false;
            ++p;
            ++q;
        }
        return true;
    }

    /// Check if two palettes do not have the same color entries
    bool operator!=( const CHSVPalette16 &rhs) const
    {
        return !( *this == rhs);
    }

    /// Create palette filled with one color
    /// @param c1 the color to fill the palette with
    CHSVPalette16( const CHSV& c1)
    {
        fill_solid( &(entries[0]), 16, c1);
    }

    /// Create palette with a gradient from one color to another
    /// @param c1 the starting color for the gradient
    /// @param c2 the end color for the gradient
    CHSVPalette16( const CHSV& c1, const CHSV& c2)
    {
        fill_gradient( &(entries[0]), 16, c1, c2);
    }

    /// Create palette with three-color gradient
    /// @param c1 the starting color for the gradient
    /// @param c2 the middle color for the gradient
    /// @param c3 the end color for the gradient
    CHSVPalette16( const CHSV& c1, const CHSV& c2, const CHSV& c3)
    {
        fill_gradient( &(entries[0]), 16, c1, c2, c3);
    }

    /// Create palette with four-color gradient
    /// @param c1 the starting color for the gradient
    /// @param c2 the first middle color for the gradient
    /// @param c3 the second middle color for the gradient
    /// @param c4 the end color for the gradient
    CHSVPalette16( const CHSV& c1, const CHSV& c2, const CHSV& c3, const CHSV& c4)
    {
        fill_gradient( &(entries[0]), 16, c1, c2, c3, c4);
    }

};
*/

/// RGB color palette with 16 discrete values
class CRGBPalette16 {
public:
    CRGB entries[16];
    CRGBPalette16() {
        memset(entries, 0, sizeof(entries)); // default constructor: set all to black
    }

    // Create palette from 16 CRGB values
    CRGBPalette16( const CRGB& c00,const CRGB& c01,const CRGB& c02,const CRGB& c03,
                   const CRGB& c04,const CRGB& c05,const CRGB& c06,const CRGB& c07,
                   const CRGB& c08,const CRGB& c09,const CRGB& c10,const CRGB& c11,
                   const CRGB& c12,const CRGB& c13,const CRGB& c14,const CRGB& c15 ) 
    {
        entries[0]=c00; entries[1]=c01; entries[2]=c02; entries[3]=c03;
        entries[4]=c04; entries[5]=c05; entries[6]=c06; entries[7]=c07;
        entries[8]=c08; entries[9]=c09; entries[10]=c10; entries[11]=c11;
        entries[12]=c12; entries[13]=c13; entries[14]=c14; entries[15]=c15;
    };

    /// Copy constructor
    CRGBPalette16( const CRGBPalette16& rhs)
    {
        memmove( (void *) &(entries[0]), &(rhs.entries[0]), sizeof( entries));
    }
    /// Create palette from array of CRGB colors
    CRGBPalette16( const CRGB rhs[16])
    {
        memmove( (void *) &(entries[0]), &(rhs[0]), sizeof( entries));
    }
    /// @copydoc CRGBPalette16(const CRGBPalette16&)
    CRGBPalette16& operator=( const CRGBPalette16& rhs)
    {
        memmove( (void *) &(entries[0]), &(rhs.entries[0]), sizeof( entries));
        return *this;
    }
    /// Create palette from array of CRGB colors
    CRGBPalette16& operator=( const CRGB rhs[16])
    {
        memmove( (void *) &(entries[0]), &(rhs[0]), sizeof( entries));
        return *this;
    }
/*
    /// Create palette from CHSV palette
    CRGBPalette16( const CHSVPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; ++i) {
            entries[i] = rhs.entries[i]; // implicit HSV-to-RGB conversion
        }
    }*/

/*
    /// Create palette from array of CHSV colors
    CRGBPalette16( const CHSV rhs[16])
    {
        for( uint8_t i = 0; i < 16; ++i) {
            entries[i] = rhs[i]; // implicit HSV-to-RGB conversion
        }
    }*/

    /*
    /// @copydoc CRGBPalette16(const CHSVPalette16&)
    CRGBPalette16& operator=( const CHSVPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; ++i) {
    		entries[i] = rhs.entries[i]; // implicit HSV-to-RGB conversion
        }
        return *this;
    }


    /// Create palette from array of CHSV colors
    CRGBPalette16& operator=( const CHSV rhs[16])
    {
        for( uint8_t i = 0; i < 16; ++i) {
            entries[i] = rhs[i]; // implicit HSV-to-RGB conversion
        }
        return *this;
    }*/


    /// Create palette from palette stored in PROGMEM
    CRGBPalette16( const TProgmemRGBPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; ++i) {
            entries[i] =  *(const uint32_t*)(rhs + i);
        }
    }
    /// @copydoc CRGBPalette16(const TProgmemRGBPalette16&)
    CRGBPalette16& operator=( const TProgmemRGBPalette16& rhs)
    {
        for( uint8_t i = 0; i < 16; ++i) {
            entries[i] =  *(const uint32_t*)( rhs + i);
        }
        return *this;
    }

    /// @copydoc CHSVPalette16::operator==
    bool operator==( const CRGBPalette16 &rhs) const
    {
        const uint8_t* p = (const uint8_t*)(&(this->entries[0]));
        const uint8_t* q = (const uint8_t*)(&(rhs.entries[0]));
        if( p == q) return true;
        for( uint8_t i = 0; i < (sizeof( entries)); ++i) {
            if( *p != *q) return false;
            ++p;
            ++q;
        }
        return true;
    }
    /// @copydoc CHSVPalette16::operator!=
    bool operator!=( const CRGBPalette16 &rhs) const
    {
        return !( *this == rhs);
    }
    /// @copydoc CHSVPalette16::operator[]
    inline CRGB& operator[] (uint8_t x) __attribute__((always_inline))
    {
        return entries[x];
    }
    /// @copydoc CHSVPalette16::operator[]
    inline const CRGB& operator[] (uint8_t x) const __attribute__((always_inline))
    {
        return entries[x];
    }

    /// @copydoc CHSVPalette16::operator[]
    inline CRGB& operator[] (int x) __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }
    /// @copydoc CHSVPalette16::operator[]
    inline const CRGB& operator[] (int x) const __attribute__((always_inline))
    {
        return entries[(uint8_t)x];
    }

    /// Get the underlying pointer to the CHSV entries making up the palette
    operator CRGB*()
    {
        return &(entries[0]);
    }
    
/*
    /// @copydoc CHSVPalette16::CHSVPalette16(const CHSV&)
    CRGBPalette16( const CHSV& c1)
    {
        fill_solid( &(entries[0]), 16, c1);
    }
    
    /// @copydoc CHSVPalette16::CHSVPalette16(const CHSV&, const CHSV&)
    CRGBPalette16( const CHSV& c1, const CHSV& c2)
    {
        fill_gradient( &(entries[0]), 16, c1, c2);
    }
    /// @copydoc CHSVPalette16::CHSVPalette16(const CHSV&, const CHSV&, const CHSV&)
    CRGBPalette16( const CHSV& c1, const CHSV& c2, const CHSV& c3)
    {
        fill_gradient( &(entries[0]), 16, c1, c2, c3);
    }
    /// @copydoc CHSVPalette16::CHSVPalette16(const CHSV&, const CHSV&, const CHSV&, const CHSV&)
    CRGBPalette16( const CHSV& c1, const CHSV& c2, const CHSV& c3, const CHSV& c4)
    {
        fill_gradient( &(entries[0]), 16, c1, c2, c3, c4);
    }
*/
    /// @copydoc CHSVPalette16::CHSVPalette16(const CHSV&)
    CRGBPalette16( const CRGB& c1)
    {
        fill_solid_RGB(&(entries[0]), 16, c1);
    }
    /// @copydoc CHSVPalette16::CHSVPalette16(const CHSV&, const CHSV&)
    CRGBPalette16( const CRGB& c1, const CRGB& c2)
    {
        fill_gradient_RGB(&(entries[0]), 16, c1, c2);
    }
    /// @copydoc CHSVPalette16::CHSVPalette16(const CHSV&, const CHSV&, const CHSV&)
    CRGBPalette16( const CRGB& c1, const CRGB& c2, const CRGB& c3)
    {
        fill_gradient_RGB(&(entries[0]), 16, c1, c2, c3);
    }
    /// @copydoc CHSVPalette16::CHSVPalette16(const CHSV&, const CHSV&, const CHSV&, const CHSV&)
    CRGBPalette16( const CRGB& c1, const CRGB& c2, const CRGB& c3, const CRGB& c4)
    {
        fill_gradient_RGB(&(entries[0]), 16, c1, c2, c3, c4);
    }

    /// Creates a palette from a gradient palette in PROGMEM. 
    ///
    /// Gradient palettes are loaded into CRGBPalettes in such a way
    /// that, if possible, every color represented in the gradient palette
    /// is also represented in the CRGBPalette.  
    ///
    /// For example, consider a gradient palette that is all black except
    /// for a single, one-element-wide (1/256th!) spike of red in the middle:
    ///   @code
    ///     0,   0,0,0
    ///   124,   0,0,0
    ///   125, 255,0,0  // one 1/256th-palette-wide red stripe
    ///   126,   0,0,0
    ///   255,   0,0,0
    ///   @endcode
    /// A naive conversion of this 256-element palette to a 16-element palette
    /// might accidentally completely eliminate the red spike, rendering the
    /// palette completely black. 
    ///
    /// However, the conversions provided here would attempt to include a
    /// the red stripe in the output, more-or-less as faithfully as possible.
    /// So in this case, the resulting CRGBPalette16 palette would have a red
    /// stripe in the middle which was 1/16th of a palette wide -- the
    /// narrowest possible in a CRGBPalette16. 
    ///
    /// This means that the relative width of stripes in a CRGBPalette16
    /// will be, by definition, different from the widths in the gradient
    /// palette.  This code attempts to preserve "all the colors", rather than
    /// the exact stripe widths at the expense of dropping some colors.

    CRGBPalette16( TProgmemRGBGradientPalette_bytes progpal )
    {
        *this = progpal;
    }
    /// @copydoc CRGBPalette16(TProgmemRGBGradientPalette_bytes)
    CRGBPalette16& operator=( TProgmemRGBGradientPalette_bytes progpal )
    {
        TRGBGradientPaletteEntryUnion* progent = (TRGBGradientPaletteEntryUnion*)(progpal);
        TRGBGradientPaletteEntryUnion u;

        // Count entries
        uint16_t count = 0;
        do {
            u.dword = *(const uint32_t*)(progent + count);
            ++count;
        } while ( u.index != 255);

        int8_t lastSlotUsed = -1;

        u.dword = *(const uint32_t*)( progent);
        CRGB rgbstart( u.r, u.g, u.b);

        int indexstart = 0;
        uint8_t istart8 = 0;
        uint8_t iend8 = 0;
        while( indexstart < 255) {
            ++progent;
            u.dword = *(const uint32_t*)( progent);
            int indexend  = u.index;
            CRGB rgbend( u.r, u.g, u.b);
            istart8 = indexstart / 16;
            iend8   = indexend   / 16;
            if( count < 16) {
                if( (istart8 <= lastSlotUsed) && (lastSlotUsed < 15)) {
                    istart8 = lastSlotUsed + 1;
                    if( iend8 < istart8) {
                        iend8 = istart8;
                    }
                }
                lastSlotUsed = iend8;
            }
            //fill_gradient_RGB( &(entries[0]), istart8, rgbstart, iend8, rgbend); //!!! todo: implement fill gradient function
            indexstart = indexend;
            rgbstart = rgbend;
        }
        return *this;
    }
    
    /// Creates a palette from a gradient palette in dynamic (heap) memory. 
    /// @copydetails CRGBPalette16::CRGBPalette16(TProgmemRGBGradientPalette_bytes)
    CRGBPalette16& loadDynamicGradientPalette( TDynamicRGBGradientPalette_bytes gpal )
    {
        TRGBGradientPaletteEntryUnion* ent = (TRGBGradientPaletteEntryUnion*)(gpal);
        TRGBGradientPaletteEntryUnion u;

        // Count entries
        uint16_t count = 0;
        do {
            u = *(ent + count);
            ++count;
        } while ( u.index != 255);

        int8_t lastSlotUsed = -1;


        u = *ent;
        CRGB rgbstart( u.r, u.g, u.b);

        int indexstart = 0;
        uint8_t istart8 = 0;
        uint8_t iend8 = 0;
        while( indexstart < 255) {
            ++ent;
            u = *ent;
            int indexend  = u.index;
            CRGB rgbend( u.r, u.g, u.b);
            istart8 = indexstart / 16;
            iend8   = indexend   / 16;
            if( count < 16) {
                if( (istart8 <= lastSlotUsed) && (lastSlotUsed < 15)) {
                    istart8 = lastSlotUsed + 1;
                    if( iend8 < istart8) {
                        iend8 = istart8;
                    }
                }
                lastSlotUsed = iend8;
            }
            //fill_gradient_RGB( &(entries[0]), istart8, rgbstart, iend8, rgbend); //!!! todo: implement fill gradient function
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
    inline const uint8_t& operator[] (uint8_t x) const __attribute__((always_inline)) { return raw[x]; }

    // Assignment from 32-bit color
    inline CRGBW& operator=(uint32_t color) __attribute__((always_inline)) { color32 = color; return *this; }

    // Assignment from r, g, b, w
    inline CRGBW& operator=(const CRGB& rgb) __attribute__((always_inline)) { b = rgb.b; g = rgb.g; r = rgb.r; w = 0; return *this; }

    // Conversion operator to uint32_t
    inline operator uint32_t() const __attribute__((always_inline)) {
      return color32;
    }

    //inline bool operator==(const CRGBW& clr) const __attribute__((always_inline)) {
    //    return color32 == clr.color32;
    //}

    //inline bool operator==(const uint32_t clr) const __attribute__((always_inline)) {
    //    return color32 == clr;
    //}
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

    /// Get the average of the R, G, B and W values
    uint8_t getAverageLight( ) const {
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
    uint32_t raw;    // 32bit access
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