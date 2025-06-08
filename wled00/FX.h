#pragma once
/*
  WS2812FX.h - Library for WS2812 LED effects.
  Harm Aldick - 2016
  www.aldick.org

  Copyright (c) 2016  Harm Aldick
  Licensed under the EUPL v. 1.2 or later
  Adapted from code originally licensed under the MIT license

  Modified for WLED

  Segment class/struct (c) 2022 Blaz Kristan (@blazoncek)
*/

#ifndef WS2812FX_h
#define WS2812FX_h

#include <vector>
#include "wled.h"

#ifdef WLED_DEBUG
  // enable additional debug output
  #if defined(WLED_DEBUG_HOST)
    #include "net_debug.h"
    #define DEBUGOUT NetDebug
  #else
    #define DEBUGOUT Serial
  #endif
  #define DEBUGFX_PRINT(x) DEBUGOUT.print(x)
  #define DEBUGFX_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUGFX_PRINTF(x...) DEBUGOUT.printf(x)
  #define DEBUGFX_PRINTF_P(x...) DEBUGOUT.printf_P(x)
#else
  #define DEBUGFX_PRINT(x)
  #define DEBUGFX_PRINTLN(x)
  #define DEBUGFX_PRINTF(x...)
  #define DEBUGFX_PRINTF_P(x...)
#endif

#define FASTLED_INTERNAL //remove annoying pragma messages
#define USE_GET_MILLISECOND_TIMER
#include "FastLED.h"

#define DEFAULT_BRIGHTNESS (uint8_t)127
#define DEFAULT_MODE       (uint8_t)0
#define DEFAULT_SPEED      (uint8_t)128
#define DEFAULT_INTENSITY  (uint8_t)128
#define DEFAULT_COLOR      (uint32_t)0xFFAA00
#define DEFAULT_C1         (uint8_t)128
#define DEFAULT_C2         (uint8_t)128
#define DEFAULT_C3         (uint8_t)16

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

//color mangling macros
#ifndef RGBW32
#define RGBW32(r,g,b,w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#endif

extern bool realtimeRespectLedMaps; // used in getMappedPixelIndex()
extern byte realtimeMode;           // used in getMappedPixelIndex()

/* Not used in all effects yet */
#define WLED_FPS         42
#define FRAMETIME_FIXED  (1000/WLED_FPS)
#define FRAMETIME        strip.getFrameTime()
#if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32S2)
  #define MIN_FRAME_DELAY  2                                              // minimum wait between repaints, to keep other functions like WiFi alive 
#elif defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C3)
  #define MIN_FRAME_DELAY  3                                              // S2/C3 are slower than normal esp32, and only have one core
#else
  #define MIN_FRAME_DELAY  8                                              // 8266 legacy MIN_SHOW_DELAY
#endif
#define FPS_UNLIMITED    0

// FPS calculation (can be defined as compile flag for debugging)
#ifndef FPS_CALC_AVG
#define FPS_CALC_AVG 7 // average FPS calculation over this many frames (moving average)
#endif
#ifndef FPS_MULTIPLIER
#define FPS_MULTIPLIER 1 // dev option: multiplier to get sub-frame FPS without floats
#endif
#define FPS_CALC_SHIFT 7 // bit shift for fixed point math

/* each segment uses 82 bytes of SRAM memory, so if you're application fails because of
  insufficient memory, decreasing MAX_NUM_SEGMENTS may help */
#ifdef ESP8266
  #define MAX_NUM_SEGMENTS  16
  /* How much data bytes all segments combined may allocate */
  #define MAX_SEGMENT_DATA  5120
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  #define MAX_NUM_SEGMENTS  20
  #define MAX_SEGMENT_DATA  (MAX_NUM_SEGMENTS*512)  // 10k by default (S2 is short on free RAM)
#else
  #define MAX_NUM_SEGMENTS  32  // warning: going beyond 32 may consume too much RAM for stable operation
  #define MAX_SEGMENT_DATA  (MAX_NUM_SEGMENTS*1280) // 40k by default
#endif

/* How much data bytes each segment should max allocate to leave enough space for other segments,
  assuming each segment uses the same amount of data. 256 for ESP8266, 640 for ESP32. */
#define FAIR_DATA_PER_SEG (MAX_SEGMENT_DATA / WS2812FX::getMaxSegments())

#define MIN_SHOW_DELAY   (_frametime < 16 ? 8 : 15)

#define NUM_COLORS       3 /* number of colors per segment */
#define SEGMENT          (*strip._currentSegment)
#define SEGENV           (*strip._currentSegment)
#define SEGCOLOR(x)      Segment::getCurrentColor(x)
#define SEGPALETTE       Segment::getCurrentPalette()
#define SEGLEN           Segment::vLength()
#define SEG_W            Segment::vWidth()
#define SEG_H            Segment::vHeight()
#define SPEED_FORMULA_L  (5U + (50U*(255U - SEGMENT.speed))/SEGLEN)

// some common colors
#define RED        (uint32_t)0xFF0000
#define GREEN      (uint32_t)0x00FF00
#define BLUE       (uint32_t)0x0000FF
#define WHITE      (uint32_t)0xFFFFFF
#define BLACK      (uint32_t)0x000000
#define YELLOW     (uint32_t)0xFFFF00
#define CYAN       (uint32_t)0x00FFFF
#define MAGENTA    (uint32_t)0xFF00FF
#define PURPLE     (uint32_t)0x400080
#define ORANGE     (uint32_t)0xFF3000
#define PINK       (uint32_t)0xFF1493
#define GREY       (uint32_t)0x808080
#define GRAY       GREY
#define DARKGREY   (uint32_t)0x333333
#define DARKGRAY   DARKGREY
#define ULTRAWHITE (uint32_t)0xFFFFFFFF
#define DARKSLATEGRAY (uint32_t)0x2F4F4F
#define DARKSLATEGREY DARKSLATEGRAY

// segment options
#define NO_OPTIONS   (uint16_t)0x0000
#define TRANSPOSED   (uint16_t)0x0100 // rotated 90deg & reversed
#define MIRROR_Y_2D  (uint16_t)0x0080
#define REVERSE_Y_2D (uint16_t)0x0040
#define RESET_REQ    (uint16_t)0x0020
#define FROZEN       (uint16_t)0x0010
#define MIRROR       (uint16_t)0x0008
#define SEGMENT_ON   (uint16_t)0x0004
#define REVERSE      (uint16_t)0x0002
#define SELECTED     (uint16_t)0x0001

#define FX_MODE_STATIC                   0
#define FX_MODE_BLINK                    1
#define FX_MODE_BREATH                   2
#define FX_MODE_COLOR_WIPE               3
#define FX_MODE_COLOR_WIPE_RANDOM        4
#define FX_MODE_RANDOM_COLOR             5
#define FX_MODE_COLOR_SWEEP              6
#define FX_MODE_DYNAMIC                  7
#define FX_MODE_RAINBOW                  8
#define FX_MODE_RAINBOW_CYCLE            9
#define FX_MODE_SCAN                    10
#define FX_MODE_DUAL_SCAN               11  // candidate for removal (use Scan)
#define FX_MODE_FADE                    12
#define FX_MODE_THEATER_CHASE           13
#define FX_MODE_THEATER_CHASE_RAINBOW   14  // candidate for removal (use Theater)
#define FX_MODE_RUNNING_LIGHTS          15
#define FX_MODE_SAW                     16
#define FX_MODE_TWINKLE                 17
#define FX_MODE_DISSOLVE                18
#define FX_MODE_DISSOLVE_RANDOM         19  // candidate for removal (use Dissolve with with check 3)
#define FX_MODE_SPARKLE                 20
#define FX_MODE_FLASH_SPARKLE           21
#define FX_MODE_HYPER_SPARKLE           22
#define FX_MODE_STROBE                  23
#define FX_MODE_STROBE_RAINBOW          24
#define FX_MODE_MULTI_STROBE            25
#define FX_MODE_BLINK_RAINBOW           26
#define FX_MODE_ANDROID                 27
#define FX_MODE_CHASE_COLOR             28
#define FX_MODE_CHASE_RANDOM            29
#define FX_MODE_CHASE_RAINBOW           30
#define FX_MODE_CHASE_FLASH             31
#define FX_MODE_CHASE_FLASH_RANDOM      32
#define FX_MODE_CHASE_RAINBOW_WHITE     33
#define FX_MODE_COLORFUL                34
#define FX_MODE_TRAFFIC_LIGHT           35
#define FX_MODE_COLOR_SWEEP_RANDOM      36
#define FX_MODE_RUNNING_COLOR           37  // candidate for removal (use Theater)
#define FX_MODE_AURORA                  38
#define FX_MODE_RUNNING_RANDOM          39
#define FX_MODE_LARSON_SCANNER          40
#define FX_MODE_COMET                   41
#define FX_MODE_FIREWORKS               42
#define FX_MODE_RAIN                    43
#define FX_MODE_TETRIX                  44  //was Merry Christmas prior to 0.12.0 (use "Chase 2" with Red/Green)
#define FX_MODE_FIRE_FLICKER            45
#define FX_MODE_GRADIENT                46
#define FX_MODE_LOADING                 47
#define FX_MODE_ROLLINGBALLS            48  //was Police before 0.14
#define FX_MODE_FAIRY                   49  //was Police All prior to 0.13.0-b6 (use "Two Dots" with Red/Blue and full intensity)
#define FX_MODE_TWO_DOTS                50
#define FX_MODE_FAIRYTWINKLE            51  //was Two Areas prior to 0.13.0-b6 (use "Two Dots" with full intensity)
#define FX_MODE_RUNNING_DUAL            52  // candidate for removal (use Running)
#define FX_MODE_IMAGE                   53
#define FX_MODE_TRICOLOR_CHASE          54
#define FX_MODE_TRICOLOR_WIPE           55
#define FX_MODE_TRICOLOR_FADE           56
#define FX_MODE_LIGHTNING               57
#define FX_MODE_ICU                     58
#define FX_MODE_MULTI_COMET             59
#define FX_MODE_DUAL_LARSON_SCANNER     60  // candidate for removal (use Scanner with with check 1)
#define FX_MODE_RANDOM_CHASE            61
#define FX_MODE_OSCILLATE               62
#define FX_MODE_PRIDE_2015              63
#define FX_MODE_JUGGLE                  64
#define FX_MODE_PALETTE                 65
#define FX_MODE_FIRE_2012               66
#define FX_MODE_COLORWAVES              67
#define FX_MODE_BPM                     68
#define FX_MODE_FILLNOISE8              69
#define FX_MODE_NOISE16_1               70
#define FX_MODE_NOISE16_2               71
#define FX_MODE_NOISE16_3               72
#define FX_MODE_NOISE16_4               73
#define FX_MODE_COLORTWINKLE            74
#define FX_MODE_LAKE                    75
#define FX_MODE_METEOR                  76
//#define FX_MODE_METEOR_SMOOTH           77  // replaced by Meteor
#define FX_MODE_RAILWAY                 78
#define FX_MODE_RIPPLE                  79
#define FX_MODE_TWINKLEFOX              80
#define FX_MODE_TWINKLECAT              81
#define FX_MODE_HALLOWEEN_EYES          82
#define FX_MODE_STATIC_PATTERN          83
#define FX_MODE_TRI_STATIC_PATTERN      84
#define FX_MODE_SPOTS                   85
#define FX_MODE_SPOTS_FADE              86
#define FX_MODE_GLITTER                 87
#define FX_MODE_CANDLE                  88
#define FX_MODE_STARBURST               89
#define FX_MODE_EXPLODING_FIREWORKS     90
#define FX_MODE_BOUNCINGBALLS           91
#define FX_MODE_SINELON                 92
#define FX_MODE_SINELON_DUAL            93  // candidate for removal (use sinelon)
#define FX_MODE_SINELON_RAINBOW         94  // candidate for removal (use sinelon)
#define FX_MODE_POPCORN                 95
#define FX_MODE_DRIP                    96
#define FX_MODE_PLASMA                  97
#define FX_MODE_PERCENT                 98
#define FX_MODE_RIPPLE_RAINBOW          99  // candidate for removal (use ripple)
#define FX_MODE_HEARTBEAT              100
#define FX_MODE_PACIFICA               101
#define FX_MODE_CANDLE_MULTI           102  // candidate for removal (use candle with multi select)
#define FX_MODE_SOLID_GLITTER          103  // candidate for removal (use glitter)
#define FX_MODE_SUNRISE                104
#define FX_MODE_PHASED                 105
#define FX_MODE_TWINKLEUP              106
#define FX_MODE_NOISEPAL               107
#define FX_MODE_SINEWAVE               108
#define FX_MODE_PHASEDNOISE            109
#define FX_MODE_FLOW                   110
#define FX_MODE_CHUNCHUN               111
#define FX_MODE_DANCING_SHADOWS        112
#define FX_MODE_WASHING_MACHINE        113
#define FX_MODE_2DPLASMAROTOZOOM       114 // was Candy Cane prior to 0.14 (use Chase 2)
#define FX_MODE_BLENDS                 115
#define FX_MODE_TV_SIMULATOR           116
#define FX_MODE_DYNAMIC_SMOOTH         117 // candidate for removal (check3 in dynamic)

// new 0.14 2D effects
#define FX_MODE_2DSPACESHIPS           118 //gap fill
#define FX_MODE_2DCRAZYBEES            119 //gap fill
#define FX_MODE_2DGHOSTRIDER           120 //gap fill
#define FX_MODE_2DBLOBS                121 //gap fill
#define FX_MODE_2DSCROLLTEXT           122 //gap fill
#define FX_MODE_2DDRIFTROSE            123 //gap fill
#define FX_MODE_2DDISTORTIONWAVES      124 //gap fill
#define FX_MODE_2DSOAP                 125 //gap fill
#define FX_MODE_2DOCTOPUS              126 //gap fill
#define FX_MODE_2DWAVINGCELL           127 //gap fill

// WLED-SR effects (SR compatible IDs !!!)
#define FX_MODE_PIXELS                 128
#define FX_MODE_PIXELWAVE              129
#define FX_MODE_JUGGLES                130
#define FX_MODE_MATRIPIX               131
#define FX_MODE_GRAVIMETER             132
#define FX_MODE_PLASMOID               133
#define FX_MODE_PUDDLES                134
#define FX_MODE_MIDNOISE               135
#define FX_MODE_NOISEMETER             136
#define FX_MODE_FREQWAVE               137
#define FX_MODE_FREQMATRIX             138
#define FX_MODE_2DGEQ                  139
#define FX_MODE_WATERFALL              140
#define FX_MODE_FREQPIXELS             141
#define FX_MODE_BINMAP                 142
#define FX_MODE_NOISEFIRE              143
#define FX_MODE_PUDDLEPEAK             144
#define FX_MODE_NOISEMOVE              145
#define FX_MODE_2DNOISE                146
#define FX_MODE_PERLINMOVE             147
#define FX_MODE_RIPPLEPEAK             148
#define FX_MODE_2DFIRENOISE            149
#define FX_MODE_2DSQUAREDSWIRL         150
// #define FX_MODE_2DFIRE2012             151
#define FX_MODE_2DDNA                  152
#define FX_MODE_2DMATRIX               153
#define FX_MODE_2DMETABALLS            154
#define FX_MODE_FREQMAP                155
#define FX_MODE_GRAVCENTER             156
#define FX_MODE_GRAVCENTRIC            157
#define FX_MODE_GRAVFREQ               158
#define FX_MODE_DJLIGHT                159
#define FX_MODE_2DFUNKYPLANK           160
//#define FX_MODE_2DCENTERBARS           161
#define FX_MODE_2DPULSER               162
#define FX_MODE_BLURZ                  163
#define FX_MODE_2DDRIFT                164
#define FX_MODE_2DWAVERLY              165
#define FX_MODE_2DSUNRADIATION         166
#define FX_MODE_2DCOLOREDBURSTS        167
#define FX_MODE_2DJULIA                168
// #define FX_MODE_2DPOOLNOISE            169 //have been removed in WLED SR in the past because of low mem but should be added back
// #define FX_MODE_2DTWISTER              170 //have been removed in WLED SR in the past because of low mem but should be added back
// #define FX_MODE_2DCAELEMENTATY         171 //have been removed in WLED SR in the past because of low mem but should be added back
#define FX_MODE_2DGAMEOFLIFE           172
#define FX_MODE_2DTARTAN               173
#define FX_MODE_2DPOLARLIGHTS          174
#define FX_MODE_2DSWIRL                175
#define FX_MODE_2DLISSAJOUS            176
#define FX_MODE_2DFRIZZLES             177
#define FX_MODE_2DPLASMABALL           178
#define FX_MODE_FLOWSTRIPE             179
#define FX_MODE_2DHIPHOTIC             180
#define FX_MODE_2DSINDOTS              181
#define FX_MODE_2DDNASPIRAL            182
#define FX_MODE_2DBLACKHOLE            183
#define FX_MODE_WAVESINS               184
#define FX_MODE_ROCKTAVES              185
#define FX_MODE_2DAKEMI                186

#define FX_MODE_PARTICLEVOLCANO        187
#define FX_MODE_PARTICLEFIRE           188
#define FX_MODE_PARTICLEFIREWORKS      189
#define FX_MODE_PARTICLEVORTEX         190
#define FX_MODE_PARTICLEPERLIN         191
#define FX_MODE_PARTICLEPIT            192
#define FX_MODE_PARTICLEBOX            193
#define FX_MODE_PARTICLEATTRACTOR      194
#define FX_MODE_PARTICLEIMPACT         195
#define FX_MODE_PARTICLEWATERFALL      196
#define FX_MODE_PARTICLESPRAY          197
#define FX_MODE_PARTICLESGEQ           198
#define FX_MODE_PARTICLECENTERGEQ      199
#define FX_MODE_PARTICLEGHOSTRIDER     200
#define FX_MODE_PARTICLEBLOBS          201
#define FX_MODE_PSDRIP                 202
#define FX_MODE_PSPINBALL              203
#define FX_MODE_PSDANCINGSHADOWS       204
#define FX_MODE_PSFIREWORKS1D          205
#define FX_MODE_PSSPARKLER             206
#define FX_MODE_PSHOURGLASS            207
#define FX_MODE_PS1DSPRAY              208
#define FX_MODE_PSBALANCE              209
#define FX_MODE_PSCHASE                210
#define FX_MODE_PSSTARBURST            211
#define FX_MODE_PS1DGEQ                212
#define FX_MODE_PSFIRE1D               213
#define FX_MODE_PS1DSONICSTREAM        214
#define FX_MODE_PS1DSONICBOOM          215
#define FX_MODE_PS1DSPRINGY            216
#define FX_MODE_PARTICLEGALAXY         217
#define MODE_COUNT                     218


#define BLEND_STYLE_FADE            0x00  // universal
#define BLEND_STYLE_FAIRY_DUST      0x01  // universal
#define BLEND_STYLE_SWIPE_RIGHT     0x02  // 1D or 2D
#define BLEND_STYLE_SWIPE_LEFT      0x03  // 1D or 2D
#define BLEND_STYLE_OUTSIDE_IN      0x04  // 1D or 2D
#define BLEND_STYLE_INSIDE_OUT      0x05  // 1D or 2D
#define BLEND_STYLE_SWIPE_UP        0x06  // 2D
#define BLEND_STYLE_SWIPE_DOWN      0x07  // 2D
#define BLEND_STYLE_OPEN_H          0x08  // 2D
#define BLEND_STYLE_OPEN_V          0x09  // 2D
#define BLEND_STYLE_SWIPE_TL        0x0A  // 2D
#define BLEND_STYLE_SWIPE_TR        0x0B  // 2D
#define BLEND_STYLE_SWIPE_BR        0x0C  // 2D
#define BLEND_STYLE_SWIPE_BL        0x0D  // 2D
#define BLEND_STYLE_CIRCULAR_OUT    0x0E  // 2D
#define BLEND_STYLE_CIRCULAR_IN     0x0F  // 2D
// as there are many push variants to optimise if statements they are groupped together
#define BLEND_STYLE_PUSH_RIGHT      0x10  // 1D or 2D (& 0b00010000)
#define BLEND_STYLE_PUSH_LEFT       0x11  // 1D or 2D (& 0b00010000)
#define BLEND_STYLE_PUSH_UP         0x12  // 2D (& 0b00010000)
#define BLEND_STYLE_PUSH_DOWN       0x13  // 2D (& 0b00010000)
#define BLEND_STYLE_PUSH_TL         0x14  // 2D (& 0b00010000)
#define BLEND_STYLE_PUSH_TR         0x15  // 2D (& 0b00010000)
#define BLEND_STYLE_PUSH_BR         0x16  // 2D (& 0b00010000)
#define BLEND_STYLE_PUSH_BL         0x17  // 2D (& 0b00010000)
#define BLEND_STYLE_PUSH_MASK       0x10
#define BLEND_STYLE_COUNT           18


typedef enum mapping1D2D {
  M12_Pixels = 0,
  M12_pBar = 1,
  M12_pArc = 2,
  M12_pCorner = 3,
  M12_sPinwheel = 4
} mapping1D2D_t;

class WS2812FX;

// segment, 76 bytes
class Segment {
  public:
    uint32_t colors[NUM_COLORS];
    uint16_t start;   // start index / start X coordinate 2D (left)
    uint16_t stop;    // stop index / stop X coordinate 2D (right); segment is invalid if stop == 0
    uint16_t startY;  // start Y coodrinate 2D (top); there should be no more than 255 rows
    uint16_t stopY;   // stop Y coordinate 2D (bottom); there should be no more than 255 rows
    uint16_t offset;  // offset for 1D effects (effect will wrap around)
    union {
      mutable uint16_t options; //bit pattern: msb first: [transposed mirrorY reverseY] transitional (tbd) paused needspixelstate mirrored on reverse selected
      struct {
        mutable bool selected : 1;  //     0 : selected
        bool    reverse       : 1;  //     1 : reversed
        mutable bool on       : 1;  //     2 : is On
        bool    mirror        : 1;  //     3 : mirrored
        mutable bool freeze   : 1;  //     4 : paused/frozen
        mutable bool reset    : 1;  //     5 : indicates that Segment runtime requires reset
        bool    reverse_y     : 1;  //     6 : reversed Y (2D)
        bool    mirror_y      : 1;  //     7 : mirrored Y (2D)
        bool    transpose     : 1;  //     8 : transposed (2D, swapped X & Y)
        uint8_t map1D2D       : 3;  //  9-11 : mapping for 1D effect on 2D (0-use as strip, 1-expand vertically, 2-circular/arc, 3-rectangular/corner, ...)
        uint8_t soundSim      : 2;  // 12-13 : 0-3 sound simulation types ("soft" & "hard" or "on"/"off")
        mutable uint8_t set   : 2;  // 14-15 : 0-3 UI segment sets/groups
      };
    };
    uint8_t  grouping, spacing;
    uint8_t  opacity,  cct;       // 0==1900K, 255==10091K
    // effect data
    uint8_t  mode;
    uint8_t  palette;
    uint8_t  speed;
    uint8_t  intensity;
    uint8_t  custom1,  custom2;   // custom FX parameters/sliders
    struct {
      uint8_t custom3 : 5;        // reduced range slider (0-31)
      bool    check1  : 1;        // checkmark 1
      bool    check2  : 1;        // checkmark 2
      bool    check3  : 1;        // checkmark 3
      //uint8_t blendMode : 4;      // segment blending modes: top, bottom, add, subtract, difference, multiply, divide, lighten, darken, screen, overlay, hardlight, softlight, dodge, burn
    };
    uint8_t   blendMode;          // segment blending modes: top, bottom, add, subtract, difference, multiply, divide, lighten, darken, screen, overlay, hardlight, softlight, dodge, burn
    char     *name;               // segment name

    // runtime data
    mutable unsigned long next_time;  // millis() of next update
    mutable uint32_t step;  // custom "step" var
    mutable uint32_t call;  // call counter
    mutable uint16_t aux0;  // custom var
    mutable uint16_t aux1;  // custom var
    byte     *data; // effect data pointer

    static uint16_t maxWidth, maxHeight;  // these define matrix width & height (max. segment dimensions)

  private:
    uint32_t *pixels;                 // pixel data
    unsigned _dataLen;
    uint8_t  _default_palette;        // palette number that gets assigned to pal0
    union {
      mutable uint8_t _capabilities;  // determines segment capabilities in terms of what is available: RGB, W, CCT, manual W, etc.
      struct {
        bool    _isRGB    : 1;
        bool    _hasW     : 1;
        bool    _isCCT    : 1;
        bool    _manualW  : 1;
      };
    };

    // static variables are use to speed up effect calculations by stashing common pre-calculated values
    static unsigned      _usedSegmentData;    // amount of data used by all segments
    static unsigned      _vLength;            // 1D dimension used for current effect
    static unsigned      _vWidth, _vHeight;   // 2D dimensions used for current effect
    static uint32_t      _currentColors[NUM_COLORS]; // colors used for current effect (faster access from effect functions)
    static CRGBPalette16 _currentPalette;     // palette used for current effect (includes transition, used in color_from_palette())
    static CRGBPalette16 _randomPalette;      // actual random palette
    static CRGBPalette16 _newRandomPalette;   // target random palette
    static uint16_t      _lastPaletteChange;  // last random palette change time (in seconds)
    static uint16_t      _nextPaletteBlend;   // next due time for random palette morph (in millis())
    static bool          _modeBlend;          // mode/effect blending semaphore
    // clipping rectangle used for blending
    static uint16_t      _clipStart, _clipStop;
    static uint8_t       _clipStartY, _clipStopY;

    // transition data, holds values during transition (76 bytes/28 bytes)
    struct Transition {
      Segment      *_oldSegment;          // previous segment environment (may be nullptr if effect did not change)
      unsigned long _start;               // must accommodate millis()
      uint32_t      _colors[NUM_COLORS];  // current colors
      #ifndef WLED_SAVE_RAM
      CRGBPalette16 _palT;                // temporary palette (slowly being morphed from old to new)
      #endif
      uint16_t      _dur;                 // duration of transition in ms
      uint16_t      _progress;            // transition progress (0-65535); pre-calculated from _start & _dur in updateTransitionProgress()
      uint8_t       _prevPaletteBlends;   // number of previous palette blends (there are max 255 blends possible)
      uint8_t       _palette, _bri, _cct; // palette ID, brightness and CCT at the start of transition (brightness will be 0 if segment was off)
      Transition(uint16_t dur=750)
      : _oldSegment(nullptr)
      , _start(millis())
      , _colors{0,0,0}
      #ifndef WLED_SAVE_RAM
      , _palT(CRGBPalette16(CRGB::Black))
      #endif
      , _dur(dur)
      , _progress(0)
      , _prevPaletteBlends(0)
      , _palette(0)
      , _bri(0)
      , _cct(0)
      {}
      ~Transition() {
        //DEBUGFX_PRINTF_P(PSTR("-- Destroying transition: %p\n"), this);
        if (_oldSegment) delete _oldSegment;
      }
    } *_t;

  protected:

    inline static unsigned getUsedSegmentData()            { return Segment::_usedSegmentData; }
    inline static void     addUsedSegmentData(int len)     { Segment::_usedSegmentData += len; }

    inline uint32_t *getPixels() const                              { return pixels; }
    inline void     setPixelColorRaw(unsigned i, uint32_t c) const  { pixels[i] = c; }
    inline uint32_t getPixelColorRaw(unsigned i) const              { return pixels[i]; };
  #ifndef WLED_DISABLE_2D
    inline void     setPixelColorXYRaw(unsigned x, unsigned y, uint32_t c) const  { auto XY = [](unsigned X, unsigned Y){ return X + Y*Segment::vWidth(); }; pixels[XY(x,y)] = c; }
    inline uint32_t getPixelColorXYRaw(unsigned x, unsigned y) const              { auto XY = [](unsigned X, unsigned Y){ return X + Y*Segment::vWidth(); }; return pixels[XY(x,y)]; };
  #endif
    void resetIfRequired();         // sets all SEGENV variables to 0 and clears data buffer
    CRGBPalette16 &loadPalette(CRGBPalette16 &tgt, uint8_t pal);

    // transition functions
    void stopTransition();                  // ends transition mode by destroying transition structure (does nothing if not in transition)
    void updateTransitionProgress() const;  // sets transition progress (0-65535) based on time passed since transition start
    inline void handleTransition() {
      updateTransitionProgress();
      if (isInTransition() && progress() == 0xFFFFU) stopTransition();
    }
    inline uint16_t progress() const          { return isInTransition() ? _t->_progress : 0xFFFFU; } // relies on handleTransition()/updateTransitionProgress() to update progression variable
    inline Segment *getOldSegment() const     { return isInTransition() ? _t->_oldSegment : nullptr; }

    inline static void modeBlend(bool blend)  { Segment::_modeBlend = blend; }
    inline static void setClippingRect(int startX, int stopX, int startY = 0, int stopY = 1) { _clipStart = startX; _clipStop = stopX; _clipStartY = startY; _clipStopY = stopY; };
    inline static bool isPreviousMode()       { return Segment::_modeBlend; }    // needed for determining CCT/opacity during non-BLEND_STYLE_FADE transition

    static void handleRandomPalette();

  public:

    Segment(uint16_t sStart=0, uint16_t sStop=30, uint16_t sStartY = 0, uint16_t sStopY = 1)
    : colors{DEFAULT_COLOR,BLACK,BLACK}
    , start(sStart)
    , stop(sStop > sStart ? sStop : sStart+1) // minimum length is 1
    , startY(sStartY)
    , stopY(sStopY > sStartY ? sStopY : sStartY+1) // minimum height is 1
    , offset(0)
    , options(SELECTED | SEGMENT_ON)
    , grouping(1)
    , spacing(0)
    , opacity(255)
    , cct(127)
    , mode(DEFAULT_MODE)
    , palette(0)
    , speed(DEFAULT_SPEED)
    , intensity(DEFAULT_INTENSITY)
    , custom1(DEFAULT_C1)
    , custom2(DEFAULT_C2)
    , custom3(DEFAULT_C3)
    , check1(false)
    , check2(false)
    , check3(false)
    , blendMode(0)
    , name(nullptr)
    , next_time(0)
    , step(0)
    , call(0)
    , aux0(0)
    , aux1(0)
    , data(nullptr)
    , _dataLen(0)
    , _default_palette(6)
    , _capabilities(0)
    , _t(nullptr)
    {
      DEBUGFX_PRINTF_P(PSTR("-- Creating segment: %p [%d,%d:%d,%d]\n"), this, (int)start, (int)stop, (int)startY, (int)stopY);
      // allocate render buffer (always entire segment)
      pixels = static_cast<uint32_t*>(d_calloc(sizeof(uint32_t), length())); // error handling is also done in isActive()
      if (!pixels) {
        DEBUGFX_PRINTLN(F("!!! Not enough RAM for pixel buffer !!!"));
        extern byte errorFlag;
        errorFlag = ERR_NORAM_PX;
        stop = 0; // mark segment as inactive/invalid
      }
    }

    Segment(const Segment &orig); // copy constructor
    Segment(Segment &&orig) noexcept; // move constructor

    ~Segment() {
      #ifdef WLED_DEBUG
      DEBUGFX_PRINTF_P(PSTR("-- Destroying segment: %p [%d,%d:%d,%d]"), this, (int)start, (int)stop, (int)startY, (int)stopY);
      if (name) DEBUGFX_PRINTF_P(PSTR(" %s (%p)"), name, name);
      if (data) DEBUGFX_PRINTF_P(PSTR(" %u->(%p)"), _dataLen, data);
      DEBUGFX_PRINTF_P(PSTR(" T[%p]"), _t);
      DEBUGFX_PRINTLN();
      #endif
      clearName();
      deallocateData();
      d_free(pixels);
    }

    Segment& operator= (const Segment &orig); // copy assignment
    Segment& operator= (Segment &&orig) noexcept; // move assignment

#ifdef WLED_DEBUG
    size_t getSize() const { return sizeof(Segment) + (data?_dataLen:0) + (name?strlen(name):0) + (_t?sizeof(Transition):0) + (pixels?length()*sizeof(uint32_t):0); }
#endif

    inline bool     getOption(uint8_t n)   const { return ((options >> n) & 0x01); }
    inline bool     isSelected()           const { return selected; }
    inline bool     isInTransition()       const { return _t != nullptr; }
    inline bool     isActive()             const { return stop > start && pixels; }
    inline bool     hasRGB()               const { return _isRGB; }
    inline bool     hasWhite()             const { return _hasW; }
    inline bool     isCCT()                const { return _isCCT; }
    inline uint16_t width()                const { return stop > start ? (stop - start) : 0; }// segment width in physical pixels (length if 1D)
    inline uint16_t height()               const { return stopY - startY; }                   // segment height (if 2D) in physical pixels (it *is* always >=1)
    inline uint16_t length()               const { return width() * height(); }               // segment length (count) in physical pixels
    inline uint16_t groupLength()          const { return grouping + spacing; }
    inline uint8_t  getLightCapabilities() const { return _capabilities; }
    inline void     deactivate()                 { setGeometry(0,0); }
    inline Segment &clearName()                  { d_free(name); name = nullptr; return *this; }
    inline Segment &setName(const String &name)  { return setName(name.c_str()); }

    inline static unsigned vLength()                       { return Segment::_vLength; }
    inline static unsigned vWidth()                        { return Segment::_vWidth; }
    inline static unsigned vHeight()                       { return Segment::_vHeight; }
    inline static uint32_t getCurrentColor(unsigned i)     { return Segment::_currentColors[i<NUM_COLORS?i:0]; }
    inline static const CRGBPalette16 &getCurrentPalette() { return Segment::_currentPalette; }

    inline void setDrawDimensions() const { Segment::_vWidth = virtualWidth(); Segment::_vHeight = virtualHeight(); Segment::_vLength = virtualLength(); }

    void    beginDraw(uint16_t prog = 0xFFFFU);         // set up parameters for current effect
    void    setGeometry(uint16_t i1, uint16_t i2, uint8_t grp=1, uint8_t spc=0, uint16_t ofs=UINT16_MAX, uint16_t i1Y=0, uint16_t i2Y=1, uint8_t m12=0);
    Segment &setColor(uint8_t slot, uint32_t c);
    Segment &setCCT(uint16_t k);
    Segment &setOpacity(uint8_t o);
    Segment &setOption(uint8_t n, bool val);
    Segment &setMode(uint8_t fx, bool loadDefaults = false);
    Segment &setPalette(uint8_t pal);
    Segment &setName(const char* name);
    void    refreshLightCapabilities() const;

    // runtime data functions
    inline uint16_t dataSize() const { return _dataLen; }
    bool allocateData(size_t len);  // allocates effect data buffer in heap and clears it
    void deallocateData();          // deallocates (frees) effect data buffer from heap
    /**
      * Flags that before the next effect is calculated,
      * the internal segment state should be reset.
      * Call resetIfRequired before calling the next effect function.
      * Safe to call from interrupts and network requests.
      */
    inline Segment &markForReset() { reset = true; return *this; }  // setOption(SEG_OPTION_RESET, true)

    void startTransition(uint16_t dur, bool segmentCopy = true);    // transition has to start before actual segment values change
    uint8_t  currentCCT() const; // current segment's CCT (blended while in transition)
    uint8_t  currentBri() const; // current segment's opacity/brightness (blended while in transition)

    // 1D strip
    uint16_t virtualLength() const;
    [[gnu::hot]] void setPixelColor(int n, uint32_t c) const; // set relative pixel within segment with color
    inline void setPixelColor(unsigned n, uint32_t c) const                    { setPixelColor(int(n), c); }
    inline void setPixelColor(int n, byte r, byte g, byte b, byte w = 0) const { setPixelColor(n, RGBW32(r,g,b,w)); }
    inline void setPixelColor(int n, CRGB c) const                             { setPixelColor(n, RGBW32(c.r,c.g,c.b,0)); }
    void setRawPixelColor(int i, uint32_t col) const                           { if (i >= 0 && i < length()) setPixelColorRaw(i,col); }
    #ifdef WLED_USE_AA_PIXELS
    void setPixelColor(float i, uint32_t c, bool aa = true) const;
    inline void setPixelColor(float i, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0, bool aa = true) const { setPixelColor(i, RGBW32(r,g,b,w), aa); }
    inline void setPixelColor(float i, CRGB c, bool aa = true) const                                         { setPixelColor(i, RGBW32(c.r,c.g,c.b,0), aa); }
    #endif
    [[gnu::hot]] bool isPixelClipped(int i) const;
    [[gnu::hot]] uint32_t getPixelColor(int i) const;
    // 1D support functions (some implement 2D as well)
    void blur(uint8_t, bool smear = false) const;
    void clear() const { fill(BLACK); } // clear segment
    void fill(uint32_t c) const;
    void fade_out(uint8_t r) const;
    void fadeToSecondaryBy(uint8_t fadeBy) const;
    void fadeToBlackBy(uint8_t fadeBy) const;
    inline void blendPixelColor(int n, uint32_t color, uint8_t blend) const        { setPixelColor(n, color_blend(getPixelColor(n), color, blend)); }
    inline void blendPixelColor(int n, CRGB c, uint8_t blend) const                { blendPixelColor(n, RGBW32(c.r,c.g,c.b,0), blend); }
    inline void addPixelColor(int n, uint32_t color, bool preserveCR = true) const { setPixelColor(n, color_add(getPixelColor(n), color, preserveCR)); }
    inline void addPixelColor(int n, byte r, byte g, byte b, byte w = 0, bool preserveCR = true) const
                                                                                   { addPixelColor(n, RGBW32(r,g,b,w), preserveCR); }
    inline void addPixelColor(int n, CRGB c, bool preserveCR = true) const         { addPixelColor(n, RGBW32(c.r,c.g,c.b,0), preserveCR); }
    inline void fadePixelColor(uint16_t n, uint8_t fade) const                     { setPixelColor(n, color_fade(getPixelColor(n), fade, true)); }
    [[gnu::hot]] uint32_t color_from_palette(uint16_t, bool mapping, bool moving, uint8_t mcol, uint8_t pbri = 255) const;
    [[gnu::hot]] uint32_t color_wheel(uint8_t pos) const;
    // 2D matrix
    unsigned virtualWidth()  const;       // segment width in virtual pixels (accounts for groupping and spacing)
    unsigned virtualHeight() const;       // segment height in virtual pixels (accounts for groupping and spacing)
    inline unsigned nrOfVStrips() const { // returns number of virtual vertical strips in 2D matrix (used to expand 1D effects into 2D)
    #ifndef WLED_DISABLE_2D
      return (is2D() &&  map1D2D == M12_pBar) ? virtualWidth() : 1;
    #else
      return 1;
    #endif
    }
  #ifndef WLED_DISABLE_2D
    inline bool is2D() const                                                            { return (width()>1 && height()>1); }
    [[gnu::hot]] void setPixelColorXY(int x, int y, uint32_t c) const; // set relative pixel within segment with color
    inline void setPixelColorXY(unsigned x, unsigned y, uint32_t c) const               { setPixelColorXY(int(x), int(y), c); }
    inline void setPixelColorXY(int x, int y, byte r, byte g, byte b, byte w = 0) const { setPixelColorXY(x, y, RGBW32(r,g,b,w)); }
    inline void setPixelColorXY(int x, int y, CRGB c) const                             { setPixelColorXY(x, y, RGBW32(c.r,c.g,c.b,0)); }
    inline void setPixelColorXY(unsigned x, unsigned y, CRGB c) const                   { setPixelColorXY(int(x), int(y), RGBW32(c.r,c.g,c.b,0)); }
    #ifdef WLED_USE_AA_PIXELS
    void setPixelColorXY(float x, float y, uint32_t c, bool aa = true) const;
    inline void setPixelColorXY(float x, float y, byte r, byte g, byte b, byte w = 0, bool aa = true) const { setPixelColorXY(x, y, RGBW32(r,g,b,w), aa); }
    inline void setPixelColorXY(float x, float y, CRGB c, bool aa = true) const                             { setPixelColorXY(x, y, RGBW32(c.r,c.g,c.b,0), aa); }
    #endif
    [[gnu::hot]] bool isPixelXYClipped(int x, int y) const;
    [[gnu::hot]] uint32_t getPixelColorXY(int x, int y) const;
    // 2D support functions
    inline void blendPixelColorXY(uint16_t x, uint16_t y, uint32_t color, uint8_t blend) const { setPixelColorXY(x, y, color_blend(getPixelColorXY(x,y), color, blend)); }
    inline void blendPixelColorXY(uint16_t x, uint16_t y, CRGB c, uint8_t blend) const         { blendPixelColorXY(x, y, RGBW32(c.r,c.g,c.b,0), blend); }
    inline void addPixelColorXY(int x, int y, uint32_t color, bool preserveCR = true) const    { setPixelColorXY(x, y, color_add(getPixelColorXY(x,y), color, preserveCR)); }
    inline void addPixelColorXY(int x, int y, byte r, byte g, byte b, byte w = 0, bool preserveCR = true)
                                                                                               { addPixelColorXY(x, y, RGBW32(r,g,b,w), preserveCR); }
    inline void addPixelColorXY(int x, int y, CRGB c, bool preserveCR = true) const            { addPixelColorXY(x, y, RGBW32(c.r,c.g,c.b,0), preserveCR); }
    inline void fadePixelColorXY(uint16_t x, uint16_t y, uint8_t fade) const                   { setPixelColorXY(x, y, color_fade(getPixelColorXY(x,y), fade, true)); }
    inline void blurCols(fract8 blur_amount, bool smear = false) const                         { blur2D(0, blur_amount, smear); } // blur all columns (50% faster than full 2D blur)
    inline void blurRows(fract8 blur_amount, bool smear = false) const                         { blur2D(blur_amount, 0, smear); } // blur all rows (50% faster than full 2D blur)
    //void box_blur(unsigned r = 1U, bool smear = false); // 2D box blur
    void blur2D(uint8_t blur_x, uint8_t blur_y, bool smear = false) const;
    void moveX(int delta, bool wrap = false) const;
    void moveY(int delta, bool wrap = false) const;
    void move(unsigned dir, unsigned delta, bool wrap = false) const;
    void drawCircle(uint16_t cx, uint16_t cy, uint8_t radius, uint32_t c, bool soft = false) const;
    void fillCircle(uint16_t cx, uint16_t cy, uint8_t radius, uint32_t c, bool soft = false) const;
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t c, bool soft = false) const;
    void drawCharacter(unsigned char chr, int16_t x, int16_t y, uint8_t w, uint8_t h, uint32_t color, uint32_t col2 = 0, int8_t rotate = 0) const;
    void wu_pixel(uint32_t x, uint32_t y, CRGB c) const;
    inline void drawCircle(uint16_t cx, uint16_t cy, uint8_t radius, CRGB c, bool soft = false) const { drawCircle(cx, cy, radius, RGBW32(c.r,c.g,c.b,0), soft); }
    inline void fillCircle(uint16_t cx, uint16_t cy, uint8_t radius, CRGB c, bool soft = false) const { fillCircle(cx, cy, radius, RGBW32(c.r,c.g,c.b,0), soft); }
    inline void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, CRGB c, bool soft = false) const { drawLine(x0, y0, x1, y1, RGBW32(c.r,c.g,c.b,0), soft); } // automatic inline
    inline void drawCharacter(unsigned char chr, int16_t x, int16_t y, uint8_t w, uint8_t h, CRGB c, CRGB c2 = CRGB::Black, int8_t rotate = 0) const { drawCharacter(chr, x, y, w, h, RGBW32(c.r,c.g,c.b,0), RGBW32(c2.r,c2.g,c2.b,0), rotate); } // automatic inline
    inline void fill_solid(CRGB c) const { fill(RGBW32(c.r,c.g,c.b,0)); }
  #else
    inline constexpr bool is2D() const                                            { return false; }
    inline void setPixelColorXY(int x, int y, uint32_t c) const                   { setPixelColor(x, c); }
    inline void setPixelColorXY(unsigned x, unsigned y, uint32_t c) const         { setPixelColor(int(x), c); }
    inline void setPixelColorXY(int x, int y, byte r, byte g, byte b, byte w = 0) const { setPixelColor(x, RGBW32(r,g,b,w)); }
    inline void setPixelColorXY(int x, int y, CRGB c) const                       { setPixelColor(x, RGBW32(c.r,c.g,c.b,0)); }
    inline void setPixelColorXY(unsigned x, unsigned y, CRGB c) const             { setPixelColor(int(x), RGBW32(c.r,c.g,c.b,0)); }
    #ifdef WLED_USE_AA_PIXELS
    inline void setPixelColorXY(float x, float y, uint32_t c, bool aa = true) const { setPixelColor(x, c, aa); }
    inline void setPixelColorXY(float x, float y, byte r, byte g, byte b, byte w = 0, bool aa = true) { setPixelColor(x, RGBW32(r,g,b,w), aa); }
    inline void setPixelColorXY(float x, float y, CRGB c, bool aa = true) const     { setPixelColor(x, RGBW32(c.r,c.g,c.b,0), aa); }
    #endif
    inline bool isPixelXYClipped(int x, int y) const                                       { return isPixelClipped(x); }
    inline uint32_t getPixelColorXY(int x, int y) const                                    { return getPixelColor(x); }
    inline void blendPixelColorXY(uint16_t x, uint16_t y, uint32_t c, uint8_t blend) const { blendPixelColor(x, c, blend); }
    inline void blendPixelColorXY(uint16_t x, uint16_t y, CRGB c, uint8_t blend) const     { blendPixelColor(x, RGBW32(c.r,c.g,c.b,0), blend); }
    inline void addPixelColorXY(int x, int y, uint32_t color, bool saturate = false) const { addPixelColor(x, color, saturate); }
    inline void addPixelColorXY(int x, int y, byte r, byte g, byte b, byte w = 0, bool saturate = false) const { addPixelColor(x, RGBW32(r,g,b,w), saturate); }
    inline void addPixelColorXY(int x, int y, CRGB c, bool saturate = false) const         { addPixelColor(x, RGBW32(c.r,c.g,c.b,0), saturate); }
    inline void fadePixelColorXY(uint16_t x, uint16_t y, uint8_t fade) const               { fadePixelColor(x, fade); }
    //inline void box_blur(unsigned i, bool vertical, fract8 blur_amount) {}
    inline void blur2D(uint8_t blur_x, uint8_t blur_y, bool smear = false) {}
    inline void blurCols(fract8 blur_amount, bool smear = false) { blur(blur_amount, smear); } // blur all columns (50% faster than full 2D blur)
    inline void blurRows(fract8 blur_amount, bool smear = false) {}
    inline void moveX(int delta, bool wrap = false) {}
    inline void moveY(int delta, bool wrap = false) {}
    inline void move(uint8_t dir, uint8_t delta, bool wrap = false) {}
    inline void drawCircle(uint16_t cx, uint16_t cy, uint8_t radius, uint32_t c, bool soft = false) {}
    inline void drawCircle(uint16_t cx, uint16_t cy, uint8_t radius, CRGB c, bool soft = false) {}
    inline void fillCircle(uint16_t cx, uint16_t cy, uint8_t radius, uint32_t c, bool soft = false) {}
    inline void fillCircle(uint16_t cx, uint16_t cy, uint8_t radius, CRGB c, bool soft = false) {}
    inline void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t c, bool soft = false) {}
    inline void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, CRGB c, bool soft = false) {}
    inline void drawCharacter(unsigned char chr, int16_t x, int16_t y, uint8_t w, uint8_t h, uint32_t color, uint32_t = 0, int8_t = 0) {}
    inline void drawCharacter(unsigned char chr, int16_t x, int16_t y, uint8_t w, uint8_t h, CRGB c, CRGB c2, int8_t rotate = 0) {}
    inline void wu_pixel(uint32_t x, uint32_t y, CRGB c) {}
  #endif
  friend class WS2812FX;
};

// main "strip" class (104 bytes)
class WS2812FX {
  typedef uint16_t (*mode_ptr)(); // pointer to mode function
  typedef void (*show_callback)(); // pre show callback
  typedef struct ModeData {
    uint8_t     _id;   // mode (effect) id
    mode_ptr    _fcn;  // mode (effect) function
    const char *_data; // mode (effect) name and its UI control data
    ModeData(uint8_t id, uint16_t (*fcn)(void), const char *data) : _id(id), _fcn(fcn), _data(data) {}
  } mode_data_t;

  public:

    WS2812FX() :
      paletteBlend(0),
      now(millis()),
      timebase(0),
      isMatrix(false),
#ifdef WLED_AUTOSEGMENTS
      autoSegments(true),
#else
      autoSegments(false),
#endif
      correctWB(false),
      cctFromRgb(false),
      // true private variables
      _pixels(nullptr),
      _suspend(false),
      _brightness(DEFAULT_BRIGHTNESS),
      _length(DEFAULT_LED_COUNT),
      _transitionDur(750),
      _frametime(FRAMETIME_FIXED),
      _cumulativeFps(WLED_FPS << FPS_CALC_SHIFT),
      _targetFps(WLED_FPS),
      _isServicing(false),
      _isOffRefreshRequired(false),
      _hasWhiteChannel(false),
      _triggered(false),
      _segment_index(0),
      _mainSegment(0),
      _modeCount(MODE_COUNT),
      _callback(nullptr),
      customMappingTable(nullptr),
      customMappingSize(0),
      _lastShow(0),
      _lastServiceShow(0)
    {
      _mode.reserve(_modeCount);     // allocate memory to prevent initial fragmentation (does not increase size())
      _modeData.reserve(_modeCount); // allocate memory to prevent initial fragmentation (does not increase size())
      if (_mode.capacity() <= 1 || _modeData.capacity() <= 1) _modeCount = 1; // memory allocation failed only show Solid
      else setupEffectData();
    }

    ~WS2812FX() {
      d_free(_pixels);
      d_free(customMappingTable);
      _mode.clear();
      _modeData.clear();
      _segments.clear();
#ifndef WLED_DISABLE_2D
      panel.clear();
#endif
    }

    void
#ifdef WLED_DEBUG
      printSize(),                                // prints memory usage for strip components
#endif
      finalizeInit(),                             // initialises strip components
      service(),                                  // executes effect functions when due and calls strip.show()
      setCCT(uint16_t k),                         // sets global CCT (either in relative 0-255 value or in K)
      setBrightness(uint8_t b, bool direct = false),    // sets strip brightness
      setRange(uint16_t i, uint16_t i2, uint32_t col),  // used for clock overlay
      purgeSegments(),                            // removes inactive segments from RAM (may incure penalty and memory fragmentation but reduces vector footprint)
      setMainSegmentId(unsigned n = 0),
      resetSegments(),                            // marks all segments for reset
      makeAutoSegments(bool forceReset = false),  // will create segments based on configured outputs
      fixInvalidSegments(),                       // fixes incorrect segment configuration
      blendSegment(const Segment &topSegment) const,    // blends topSegment into pixels
      show(),                                     // initiates LED output
      setTargetFps(unsigned fps),
      setupEffectData(),                          // add default effects to the list; defined in FX.cpp
      waitForIt();                                // wait until frame is over (service() has finished or time for 1 frame has passed)

    void setRealtimePixelColor(unsigned i, uint32_t c);
    inline void setPixelColor(unsigned n, uint32_t c) const   { if (n < getLengthTotal()) _pixels[n] = c; }  // paints absolute strip pixel with index n and color c
    inline void resetTimebase()                               { timebase = 0UL - millis(); }
    inline void setPixelColor(unsigned n, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0) const
                                                              { setPixelColor(n, RGBW32(r,g,b,w)); }
    inline void setPixelColor(unsigned n, CRGB c) const       { setPixelColor(n, c.red, c.green, c.blue); }
    inline void fill(uint32_t c) const                        { for (size_t i = 0; i < getLengthTotal(); i++) setPixelColor(i, c); } // fill whole strip with color (inline)
    inline void trigger()                                     { _triggered = true; }  // Forces the next frame to be computed on all active segments.
    inline void setShowCallback(show_callback cb)             { _callback = cb; }
    inline void setTransition(uint16_t t)                     { _transitionDur = t; } // sets transition time (in ms)
    inline void appendSegment(uint16_t sStart=0, uint16_t sStop=30, uint16_t sStartY = 0, uint16_t sStopY = 1)
                                                              { if (_segments.size() < getMaxSegments()) _segments.emplace_back(sStart,sStop,sStartY,sStopY); }
    inline void suspend()                                     { _suspend = true; }    // will suspend (and canacel) strip.service() execution
    inline void resume()                                      { _suspend = false; }   // will resume strip.service() execution

    void restartRuntime();
    void setTransitionMode(bool t);

    bool checkSegmentAlignment() const;
    bool hasRGBWBus() const;
    bool hasCCTBus() const;
    bool deserializeMap(unsigned n = 0);

    inline bool isUpdating() const           { return !BusManager::canAllShow(); } // return true if the strip is being sent pixel updates
    inline bool isServicing() const          { return _isServicing; }           // returns true if strip.service() is executing
    inline bool hasWhiteChannel() const      { return _hasWhiteChannel; }       // returns true if strip contains separate white chanel
    inline bool isOffRefreshRequired() const { return _isOffRefreshRequired; }  // returns true if strip requires regular updates (i.e. TM1814 chipset)
    inline bool isSuspended() const          { return _suspend; }               // returns true if strip.service() execution is suspended
    inline bool needsUpdate() const          { return _triggered; }             // returns true if strip received a trigger() request

    uint8_t paletteBlend;
    uint8_t getActiveSegmentsNum() const;
    uint8_t getFirstSelectedSegId() const;
    uint8_t getLastActiveSegmentId() const;
    uint8_t getActiveSegsLightCapabilities(bool selectedOnly = false) const;
    uint8_t addEffect(uint8_t id, mode_ptr mode_fn, const char *mode_name);         // add effect to the list; defined in FX.cpp;

    inline uint8_t getBrightness() const    { return _brightness; }       // returns current strip brightness
    inline static constexpr unsigned getMaxSegments() { return MAX_NUM_SEGMENTS; }  // returns maximum number of supported segments (fixed value)
    inline uint8_t getSegmentsNum() const   { return _segments.size(); }  // returns currently present segments
    inline uint8_t getCurrSegmentId() const { return _segment_index; }    // returns current segment index (only valid while strip.isServicing())
    inline uint8_t getMainSegmentId() const { return _mainSegment; }      // returns main segment index
    inline uint8_t getTargetFps() const     { return _targetFps; }        // returns rough FPS value for las 2s interval
    inline uint8_t getModeCount() const     { return _modeCount; }        // returns number of registered modes/effects

    uint16_t getLengthPhysical() const;
    uint16_t getLengthTotal() const; // will include virtual/nonexistent pixels in matrix

    inline uint16_t getFps() const          { return (millis() - _lastShow > 2000) ? 0 : (FPS_MULTIPLIER * _cumulativeFps) >> FPS_CALC_SHIFT; } // Returns the refresh rate of the LED strip (_cumulativeFps is stored in fixed point)
    inline uint16_t getFrameTime() const    { return _frametime; }        // returns amount of time a frame should take (in ms)
    inline uint16_t getMinShowDelay() const { return MIN_FRAME_DELAY; }   // returns minimum amount of time strip.service() can be delayed (constant)
    inline uint16_t getLength() const       { return _length; }           // returns actual amount of LEDs on a strip (2D matrix may have less LEDs than W*H)
    inline uint16_t getTransition() const   { return _transitionDur; }    // returns currently set transition time (in ms)
    inline uint16_t getMappedPixelIndex(uint16_t index) const {           // convert logical address to physical
      if (index < customMappingSize && (realtimeMode == REALTIME_MODE_INACTIVE || realtimeRespectLedMaps)) index = customMappingTable[index];
      return index;
    };

    unsigned long now, timebase;
    inline uint32_t getPixelColor(unsigned n) const { return (n < getLengthTotal()) ? _pixels[n] : 0; } // returns color of pixel n
    inline uint32_t getLastShow() const             { return _lastShow; }                 // returns millis() timestamp of last strip.show() call

    const char *getModeData(unsigned id = 0) const  { return (id && id < _modeCount) ? _modeData[id] : PSTR("Solid"); }
    inline const char **getModeDataSrc()            { return &(_modeData[0]); }           // vectors use arrays for underlying data

    Segment&        getSegment(unsigned id);
    inline Segment& getFirstSelectedSeg() { return _segments[getFirstSelectedSegId()]; }  // returns reference to first segment that is "selected"
    inline Segment& getMainSegment()      { return _segments[getMainSegmentId()]; }       // returns reference to main segment
    inline Segment* getSegments()         { return &(_segments[0]); }                     // returns pointer to segment vector structure (warning: use carefully)

  // 2D support (panels)

#ifndef WLED_DISABLE_2D
    struct Panel {
      uint16_t xOffset; // x offset relative to the top left of matrix in LEDs
      uint16_t yOffset; // y offset relative to the top left of matrix in LEDs
      uint8_t  width;   // width of the panel
      uint8_t  height;  // height of the panel
      union {
        uint8_t options;
        struct {
          bool bottomStart : 1; // starts at bottom?
          bool rightStart  : 1; // starts on right?
          bool vertical    : 1; // is vertical?
          bool serpentine  : 1; // is serpentine?
        };
      };
      Panel()
      : xOffset(0)
      , yOffset(0)
      , width(8)
      , height(8)
      , options(0)
      {}
    };
    std::vector<Panel> panel;
#endif

    void setUpMatrix();     // sets up automatic matrix ledmap from panel configuration

    inline void     setPixelColorXY(unsigned x, unsigned y, uint32_t c) const { setPixelColor(y * Segment::maxWidth + x, c); }
    inline void     setPixelColorXY(unsigned x, unsigned y, byte r, byte g, byte b, byte w = 0) const { setPixelColorXY(x, y, RGBW32(r,g,b,w)); }
    inline void     setPixelColorXY(unsigned x, unsigned y, CRGB c) const     { setPixelColorXY(x, y, RGBW32(c.r,c.g,c.b,0)); }
    inline uint32_t getPixelColorXY(unsigned x, unsigned y) const             { return getPixelColor(y * Segment::maxWidth + x); }

  // end 2D support

    bool isMatrix;
    struct {
      bool autoSegments : 1;
      bool correctWB    : 1;
      bool cctFromRgb   : 1;
    };

    Segment *_currentSegment;

  private:
    uint32_t *_pixels;
    std::vector<Segment> _segments;

    volatile bool _suspend;

    uint8_t  _brightness;
    uint16_t _length;
    uint16_t _transitionDur;

    uint16_t _frametime;
    uint16_t _cumulativeFps;
    uint8_t  _targetFps;

    // will require only 1 byte
    struct {
      bool _isServicing          : 1;
      bool _isOffRefreshRequired : 1; //periodic refresh is required for the strip to remain off.
      bool _hasWhiteChannel      : 1;
      bool _triggered            : 1;
    };

    uint8_t _segment_index;
    uint8_t _mainSegment;

    uint8_t                  _modeCount;
    std::vector<mode_ptr>    _mode;     // SRAM footprint: 4 bytes per element
    std::vector<const char*> _modeData; // mode (effect) name and its slider control data array

    show_callback _callback;

    uint16_t* customMappingTable;
    uint16_t  customMappingSize;

    unsigned long _lastShow;
    unsigned long _lastServiceShow;

    friend class Segment;
};

extern const char JSON_mode_names[];
extern const char JSON_palette_names[];

#endif
