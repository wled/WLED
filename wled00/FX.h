/*
  WS2812FX.h - Library for WS2812 LED effects.
  Harm Aldick - 2016
  www.aldick.org
  LICENSE
  The MIT License (MIT)
  Copyright (c) 2016  Harm Aldick
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  Modified for WLED
*/

#ifndef WS2812FX_h
#define WS2812FX_h

#include "const.h"

#define FASTLED_INTERNAL //remove annoying pragma messages
#define USE_GET_MILLISECOND_TIMER
#include "FastLED.h"

#define DEFAULT_BRIGHTNESS (uint8_t)127
#define DEFAULT_MODE       (uint8_t)0
#define DEFAULT_SPEED      (uint8_t)128
#define DEFAULT_INTENSITY  (uint8_t)128
#define DEFAULT_COLOR      (uint32_t)0xFFAA00

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

/* Not used in all effects yet */
#define WLED_FPS         42
#define FRAMETIME_FIXED  (1000/WLED_FPS)
#define FRAMETIME        _frametime

// External buffer
#define EXTERNAL_BUFFER_SIZE  150

/* each segment uses 52 bytes of SRAM memory, so if you're application fails because of
  insufficient memory, decreasing MAX_NUM_SEGMENTS may help */
#ifdef ESP8266
  #define MAX_NUM_SEGMENTS    16
  /* How many color transitions can run at once */
  #define MAX_NUM_TRANSITIONS  8
  /* How much data bytes all segments combined may allocate */
  #define MAX_SEGMENT_DATA  4096
#else
  #ifndef MAX_NUM_SEGMENTS
    #define MAX_NUM_SEGMENTS  32
  #endif
  #define MAX_NUM_TRANSITIONS 24
  #define MAX_SEGMENT_DATA  20480
#endif

/* How much data bytes each segment should max allocate to leave enough space for other segments,
  assuming each segment uses the same amount of data. 256 for ESP8266, 640 for ESP32. */
#define FAIR_DATA_PER_SEG (MAX_SEGMENT_DATA / MAX_NUM_SEGMENTS)

#define MIN_SHOW_DELAY   (_frametime < 16 ? 8 : 15)

#define NUM_COLORS       3 /* number of colors per segment */
#define SEGMENT          _segments[_segment_index]
#define SEGCOLOR(x)      _colors_t[x]
#define SEGENV           _segment_runtimes[_segment_index]
#define SEGLEN           _virtualSegmentLength
#define SEGACT           SEGMENT.stop
#define SPEED_FORMULA_L  5U + (50U*(255U - SEGMENT.speed))/SEGLEN

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
#define ULTRAWHITE (uint32_t)0xFFFFFFFF

// options
// bit    7: segment is in transition mode
// bits 4-6: TBD
// bit    3: mirror effect within segment
// bit    2: segment is on
// bit    1: reverse segment
// bit    0: segment is selected
#define NO_OPTIONS   (uint8_t)0x00
#define TRANSITIONAL (uint8_t)0x80
#define MIRROR       (uint8_t)0x08
#define SEGMENT_ON   (uint8_t)0x04
#define REVERSE      (uint8_t)0x02
#define SELECTED     (uint8_t)0x01
#define IS_TRANSITIONAL ((SEGMENT.options & TRANSITIONAL) == TRANSITIONAL)
#define IS_MIRROR       ((SEGMENT.options & MIRROR      ) == MIRROR      )
#define IS_SEGMENT_ON   ((SEGMENT.options & SEGMENT_ON  ) == SEGMENT_ON  )
#define IS_REVERSE      ((SEGMENT.options & REVERSE     ) == REVERSE     )
#define IS_SELECTED     ((SEGMENT.options & SELECTED    ) == SELECTED    )

#define MODE_COUNT  119

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
#define FX_MODE_DUAL_SCAN               11
#define FX_MODE_FADE                    12
#define FX_MODE_THEATER_CHASE           13
#define FX_MODE_THEATER_CHASE_RAINBOW   14
#define FX_MODE_RUNNING_LIGHTS          15
#define FX_MODE_SAW                     16
#define FX_MODE_TWINKLE                 17
#define FX_MODE_DISSOLVE                18
#define FX_MODE_DISSOLVE_RANDOM         19
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
#define FX_MODE_RUNNING_COLOR           37
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
#define FX_MODE_POLICE                  48  // candidate for removal (after below three)
#define FX_MODE_FAIRY                   49  //was Police All prior to 0.13.0-b6 (use "Two Dots" with Red/Blue and full intensity)
#define FX_MODE_TWO_DOTS                50
#define FX_MODE_FAIRYTWINKLE            51  //was Two Areas prior to 0.13.0-b6 (use "Two Dots" with full intensity)
#define FX_MODE_RUNNING_DUAL            52
#define FX_MODE_HALLOWEEN               53  // candidate for removal
#define FX_MODE_TRICOLOR_CHASE          54
#define FX_MODE_TRICOLOR_WIPE           55
#define FX_MODE_TRICOLOR_FADE           56
#define FX_MODE_LIGHTNING               57
#define FX_MODE_ICU                     58
#define FX_MODE_MULTI_COMET             59
#define FX_MODE_DUAL_LARSON_SCANNER     60
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
#define FX_MODE_METEOR_SMOOTH           77
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
#define FX_MODE_SINELON_DUAL            93
#define FX_MODE_SINELON_RAINBOW         94
#define FX_MODE_POPCORN                 95
#define FX_MODE_DRIP                    96
#define FX_MODE_PLASMA                  97
#define FX_MODE_PERCENT                 98
#define FX_MODE_RIPPLE_RAINBOW          99
#define FX_MODE_HEARTBEAT              100
#define FX_MODE_PACIFICA               101
#define FX_MODE_CANDLE_MULTI           102
#define FX_MODE_SOLID_GLITTER          103
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
#define FX_MODE_CANDY_CANE             114  // candidate for removal
#define FX_MODE_BLENDS                 115
#define FX_MODE_TV_SIMULATOR           116
#define FX_MODE_DYNAMIC_SMOOTH         117
#define FX_MODE_EXTERNAL               118


class WS2812FX {
  typedef uint16_t (WS2812FX::*mode_ptr)(void);

  // pre show callback
  typedef void (*show_callback) (void);

  static WS2812FX* instance;
  
  // segment parameters
  public:
    typedef struct Segment { // 31 (32 in memory) bytes
      uint16_t start;
      uint16_t stop; //segment invalid if stop == 0
      uint16_t offset;
      uint8_t  speed;
      uint8_t  intensity;
      uint8_t  palette;
      uint8_t  mode;
      uint8_t  options; //bit pattern: msb first: transitional needspixelstate tbd tbd (paused) on reverse selected
      uint8_t  grouping, spacing;
      uint8_t  opacity;
      uint32_t colors[NUM_COLORS];
      uint8_t  cct; //0==1900K, 255==10091K
      uint8_t  _capabilities;
      char *name;
      bool setColor(uint8_t slot, uint32_t c, uint8_t segn) { //returns true if changed
        if (slot >= NUM_COLORS || segn >= MAX_NUM_SEGMENTS) return false;
        if (c == colors[slot]) return false;
        uint8_t b = (slot == 1) ? cct : opacity;
        ColorTransition::startTransition(b, colors[slot], instance->_transitionDur, segn, slot);
        colors[slot] = c; return true;
      }
      void setCCT(uint16_t k, uint8_t segn) {
        if (segn >= MAX_NUM_SEGMENTS) return;
        if (k > 255) { //kelvin value, convert to 0-255
          if (k < 1900)  k = 1900;
          if (k > 10091) k = 10091;
          k = (k - 1900) >> 5;
        }
        if (cct == k) return;
        ColorTransition::startTransition(cct, colors[1], instance->_transitionDur, segn, 1);
        cct = k;
      }
      void setOpacity(uint8_t o, uint8_t segn) {
        if (segn >= MAX_NUM_SEGMENTS) return;
        if (opacity == o) return;
        ColorTransition::startTransition(opacity, colors[0], instance->_transitionDur, segn, 0);
        opacity = o;
      }
      void setOption(uint8_t n, bool val, uint8_t segn = 255)
      {
        bool prevOn = false;
        if (n == SEG_OPTION_ON) {
          prevOn = getOption(SEG_OPTION_ON);
          if (!val && prevOn) { //fade off
            ColorTransition::startTransition(opacity, colors[0], instance->_transitionDur, segn, 0);
          }
        }

        if (val) {
          options |= 0x01 << n;
        } else
        {
          options &= ~(0x01 << n);
        }

        if (n == SEG_OPTION_ON && val && !prevOn) { //fade on
          ColorTransition::startTransition(0, colors[0], instance->_transitionDur, segn, 0);
        }
      }
      bool getOption(uint8_t n)
      {
        return ((options >> n) & 0x01);
      }
      inline bool isSelected()
      {
        return getOption(0);
      }
      inline bool isActive()
      {
        return stop > start;
      }
      inline uint16_t length()
      {
        return stop - start;
      }
      inline uint16_t groupLength()
      {
        return grouping + spacing;
      }
      uint16_t virtualLength()
      {
        uint16_t groupLen = groupLength();
        uint16_t vLength = (length() + groupLen - 1) / groupLen;
        if (options & MIRROR)
          vLength = (vLength + 1) /2;  // divide by 2 if mirror, leave at least a single LED
        return vLength;
      }
      uint8_t differs(Segment& b);
      inline uint8_t getLightCapabilities() {return _capabilities;}
      void refreshLightCapabilities();
    } segment;

  // segment runtime parameters
    typedef struct Segment_runtime { // 28 bytes
      unsigned long next_time;  // millis() of next update
      uint32_t step;  // custom "step" var
      uint32_t call;  // call counter
      uint16_t aux0;  // custom var
      uint16_t aux1;  // custom var
      byte* data = nullptr;
      bool allocateData(uint16_t len){
        if (data && _dataLen == len) return true; //already allocated
        deallocateData();
        if (WS2812FX::instance->_usedSegmentData + len > MAX_SEGMENT_DATA) return false; //not enough memory
        // if possible use SPI RAM on ESP32
        #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_PSRAM)
        if (psramFound())
          data = (byte*) ps_malloc(len);
        else
        #endif
          data = (byte*) malloc(len);
        if (!data) return false; //allocation failed
        WS2812FX::instance->_usedSegmentData += len;
        _dataLen = len;
        memset(data, 0, len);
        return true;
      }
      void deallocateData(){
        free(data);
        data = nullptr;
        WS2812FX::instance->_usedSegmentData -= _dataLen;
        _dataLen = 0;
      }

      /** 
       * If reset of this segment was request, clears runtime
       * settings of this segment.
       * Must not be called while an effect mode function is running
       * because it could access the data buffer and this method 
       * may free that data buffer.
       */
      void resetIfRequired() {
        if (_requiresReset) {
          next_time = 0; step = 0; call = 0; aux0 = 0; aux1 = 0; 
          deallocateData();
          _requiresReset = false;
        }
      }

      /** 
       * Flags that before the next effect is calculated,
       * the internal segment state should be reset. 
       * Call resetIfRequired before calling the next effect function.
       * Safe to call from interrupts and network requests.
       */
      inline void markForReset() { _requiresReset = true; }
      private:
        uint16_t _dataLen = 0;
        bool _requiresReset = false;
    } segment_runtime;

    typedef struct ColorTransition { // 12 bytes
      uint32_t colorOld = 0;
      uint32_t transitionStart;
      uint16_t transitionDur;
      uint8_t segment = 0xFF; //lower 6 bits: the segment this transition is for (255 indicates transition not in use/available) upper 2 bits: color channel
      uint8_t briOld = 0;
      static void startTransition(uint8_t oldBri, uint32_t oldCol, uint16_t dur, uint8_t segn, uint8_t slot) {
        if (segn >= MAX_NUM_SEGMENTS || slot >= NUM_COLORS || dur == 0) return;
        if (instance->_brightness == 0) return; //do not need transitions if master bri is off
        if (!instance->_segments[segn].getOption(SEG_OPTION_ON)) return; //not if segment is off either
        uint8_t tIndex = 0xFF; //none found
        uint16_t tProgression = 0;
        uint8_t s = segn + (slot << 6); //merge slot and segment into one byte

        for (uint8_t i = 0; i < MAX_NUM_TRANSITIONS; i++) {
          uint8_t tSeg = instance->transitions[i].segment;
          //see if this segment + color already has a running transition
          if (tSeg == s) {
            tIndex = i; break;
          }
          if (tSeg == 0xFF) { //free transition
            tIndex = i; tProgression = 0xFFFF;
          }
        }

        if (tIndex == 0xFF) { //no slot found yet
          for (uint8_t i = 0; i < MAX_NUM_TRANSITIONS; i++) {
            //find most progressed transition to overwrite
            uint16_t prog = instance->transitions[i].progress();
            if (prog > tProgression) {
              tIndex = i; tProgression = prog;
            }
          }
        }

        ColorTransition& t = instance->transitions[tIndex];
        if (t.segment == s) //this is an active transition on the same segment+color
        {
          bool wasTurningOff = (oldBri == 0);
          t.briOld = t.currentBri(wasTurningOff, slot);
          t.colorOld = t.currentColor(oldCol);
        } else {
          t.briOld = oldBri;
          t.colorOld = oldCol;
          uint8_t prevSeg = t.segment & 0x3F;
          if (prevSeg < MAX_NUM_SEGMENTS) instance->_segments[prevSeg].setOption(SEG_OPTION_TRANSITIONAL, false);
        }
        t.transitionDur = dur;
        t.transitionStart = millis();
        t.segment = s;
        instance->_segments[segn].setOption(SEG_OPTION_TRANSITIONAL, true);
        //refresh immediately, required for Solid mode
        if (instance->_segment_runtimes[segn].next_time > t.transitionStart + 22) instance->_segment_runtimes[segn].next_time = t.transitionStart;
      }
      uint16_t progress(bool allowEnd = false) { //transition progression between 0-65535
        uint32_t timeNow = millis();
        if (timeNow - transitionStart > transitionDur) {
          if (allowEnd) {
            uint8_t segn = segment & 0x3F;
            if (segn < MAX_NUM_SEGMENTS) instance->_segments[segn].setOption(SEG_OPTION_TRANSITIONAL, false);
            segment = 0xFF;
          }
          return 0xFFFF;
        }
        uint32_t elapsed = timeNow - transitionStart;
        uint32_t prog = elapsed * 0xFFFF / transitionDur;
        return (prog > 0xFFFF) ? 0xFFFF : prog;
      }
      uint32_t currentColor(uint32_t colorNew) {
        return instance->color_blend(colorOld, colorNew, progress(true), true);
      }
      uint8_t currentBri(bool turningOff = false, uint8_t slot = 0) {
        uint8_t segn = segment & 0x3F;
        if (segn >= MAX_NUM_SEGMENTS) return 0;
        uint8_t briNew = instance->_segments[segn].opacity;
        if (slot == 0) {
          if (!instance->_segments[segn].getOption(SEG_OPTION_ON) || turningOff) briNew = 0;
        } else { //transition slot 1 brightness for CCT transition
          briNew = instance->_segments[segn].cct;
        }
        uint32_t prog = progress() + 1;
        return ((briNew * prog) + (briOld * (0x10000 - prog))) >> 16;
      }
    } color_transition;

    WS2812FX() {
      WS2812FX::instance = this;
      //assign each member of the _mode[] array to its respective function reference 
      _mode[FX_MODE_STATIC]                  = &WS2812FX::mode_static;
      _mode[FX_MODE_BLINK]                   = &WS2812FX::mode_blink;
      _mode[FX_MODE_COLOR_WIPE]              = &WS2812FX::mode_color_wipe;
      _mode[FX_MODE_COLOR_WIPE_RANDOM]       = &WS2812FX::mode_color_wipe_random;
      _mode[FX_MODE_RANDOM_COLOR]            = &WS2812FX::mode_random_color;
      _mode[FX_MODE_COLOR_SWEEP]             = &WS2812FX::mode_color_sweep;
      _mode[FX_MODE_DYNAMIC]                 = &WS2812FX::mode_dynamic;
      _mode[FX_MODE_RAINBOW]                 = &WS2812FX::mode_rainbow;
      _mode[FX_MODE_RAINBOW_CYCLE]           = &WS2812FX::mode_rainbow_cycle;
      _mode[FX_MODE_SCAN]                    = &WS2812FX::mode_scan;
      _mode[FX_MODE_DUAL_SCAN]               = &WS2812FX::mode_dual_scan;
      _mode[FX_MODE_FADE]                    = &WS2812FX::mode_fade;
      _mode[FX_MODE_THEATER_CHASE]           = &WS2812FX::mode_theater_chase;
      _mode[FX_MODE_THEATER_CHASE_RAINBOW]   = &WS2812FX::mode_theater_chase_rainbow;
      _mode[FX_MODE_SAW]                     = &WS2812FX::mode_saw;
      _mode[FX_MODE_TWINKLE]                 = &WS2812FX::mode_twinkle;
      _mode[FX_MODE_DISSOLVE]                = &WS2812FX::mode_dissolve;
      _mode[FX_MODE_DISSOLVE_RANDOM]         = &WS2812FX::mode_dissolve_random;
      _mode[FX_MODE_SPARKLE]                 = &WS2812FX::mode_sparkle;
      _mode[FX_MODE_FLASH_SPARKLE]           = &WS2812FX::mode_flash_sparkle;
      _mode[FX_MODE_HYPER_SPARKLE]           = &WS2812FX::mode_hyper_sparkle;
      _mode[FX_MODE_STROBE]                  = &WS2812FX::mode_strobe;
      _mode[FX_MODE_STROBE_RAINBOW]          = &WS2812FX::mode_strobe_rainbow;
      _mode[FX_MODE_MULTI_STROBE]            = &WS2812FX::mode_multi_strobe;
      _mode[FX_MODE_BLINK_RAINBOW]           = &WS2812FX::mode_blink_rainbow;
      _mode[FX_MODE_ANDROID]                 = &WS2812FX::mode_android;
      _mode[FX_MODE_CHASE_COLOR]             = &WS2812FX::mode_chase_color;
      _mode[FX_MODE_CHASE_RANDOM]            = &WS2812FX::mode_chase_random;
      _mode[FX_MODE_CHASE_RAINBOW]           = &WS2812FX::mode_chase_rainbow;
      _mode[FX_MODE_CHASE_FLASH]             = &WS2812FX::mode_chase_flash;
      _mode[FX_MODE_CHASE_FLASH_RANDOM]      = &WS2812FX::mode_chase_flash_random;
      _mode[FX_MODE_CHASE_RAINBOW_WHITE]     = &WS2812FX::mode_chase_rainbow_white;
      _mode[FX_MODE_COLORFUL]                = &WS2812FX::mode_colorful;
      _mode[FX_MODE_TRAFFIC_LIGHT]           = &WS2812FX::mode_traffic_light;
      _mode[FX_MODE_COLOR_SWEEP_RANDOM]      = &WS2812FX::mode_color_sweep_random;
      _mode[FX_MODE_RUNNING_COLOR]           = &WS2812FX::mode_running_color;
      _mode[FX_MODE_AURORA]                  = &WS2812FX::mode_aurora;
      _mode[FX_MODE_RUNNING_RANDOM]          = &WS2812FX::mode_running_random;
      _mode[FX_MODE_LARSON_SCANNER]          = &WS2812FX::mode_larson_scanner;
      _mode[FX_MODE_COMET]                   = &WS2812FX::mode_comet;
      _mode[FX_MODE_FIREWORKS]               = &WS2812FX::mode_fireworks;
      _mode[FX_MODE_RAIN]                    = &WS2812FX::mode_rain;
      _mode[FX_MODE_TETRIX]                  = &WS2812FX::mode_tetrix;
      _mode[FX_MODE_FIRE_FLICKER]            = &WS2812FX::mode_fire_flicker;
      _mode[FX_MODE_GRADIENT]                = &WS2812FX::mode_gradient;
      _mode[FX_MODE_LOADING]                 = &WS2812FX::mode_loading;
      _mode[FX_MODE_POLICE]                  = &WS2812FX::mode_police;
      _mode[FX_MODE_FAIRY]                   = &WS2812FX::mode_fairy;
      _mode[FX_MODE_TWO_DOTS]                = &WS2812FX::mode_two_dots;
      _mode[FX_MODE_FAIRYTWINKLE]            = &WS2812FX::mode_fairytwinkle;
      _mode[FX_MODE_RUNNING_DUAL]            = &WS2812FX::mode_running_dual;
      _mode[FX_MODE_HALLOWEEN]               = &WS2812FX::mode_halloween;
      _mode[FX_MODE_TRICOLOR_CHASE]          = &WS2812FX::mode_tricolor_chase;
      _mode[FX_MODE_TRICOLOR_WIPE]           = &WS2812FX::mode_tricolor_wipe;
      _mode[FX_MODE_TRICOLOR_FADE]           = &WS2812FX::mode_tricolor_fade;
      _mode[FX_MODE_BREATH]                  = &WS2812FX::mode_breath;
      _mode[FX_MODE_RUNNING_LIGHTS]          = &WS2812FX::mode_running_lights;
      _mode[FX_MODE_LIGHTNING]               = &WS2812FX::mode_lightning;
      _mode[FX_MODE_ICU]                     = &WS2812FX::mode_icu;
      _mode[FX_MODE_MULTI_COMET]             = &WS2812FX::mode_multi_comet;
      _mode[FX_MODE_DUAL_LARSON_SCANNER]     = &WS2812FX::mode_dual_larson_scanner;
      _mode[FX_MODE_RANDOM_CHASE]            = &WS2812FX::mode_random_chase;
      _mode[FX_MODE_OSCILLATE]               = &WS2812FX::mode_oscillate;
      _mode[FX_MODE_FIRE_2012]               = &WS2812FX::mode_fire_2012;
      _mode[FX_MODE_PRIDE_2015]              = &WS2812FX::mode_pride_2015;
      _mode[FX_MODE_BPM]                     = &WS2812FX::mode_bpm;
      _mode[FX_MODE_JUGGLE]                  = &WS2812FX::mode_juggle;
      _mode[FX_MODE_PALETTE]                 = &WS2812FX::mode_palette;
      _mode[FX_MODE_COLORWAVES]              = &WS2812FX::mode_colorwaves;
      _mode[FX_MODE_FILLNOISE8]              = &WS2812FX::mode_fillnoise8;
      _mode[FX_MODE_NOISE16_1]               = &WS2812FX::mode_noise16_1;
      _mode[FX_MODE_NOISE16_2]               = &WS2812FX::mode_noise16_2;
      _mode[FX_MODE_NOISE16_3]               = &WS2812FX::mode_noise16_3;
      _mode[FX_MODE_NOISE16_4]               = &WS2812FX::mode_noise16_4;
      _mode[FX_MODE_COLORTWINKLE]            = &WS2812FX::mode_colortwinkle;
      _mode[FX_MODE_LAKE]                    = &WS2812FX::mode_lake;
      _mode[FX_MODE_METEOR]                  = &WS2812FX::mode_meteor;
      _mode[FX_MODE_METEOR_SMOOTH]           = &WS2812FX::mode_meteor_smooth;
      _mode[FX_MODE_RAILWAY]                 = &WS2812FX::mode_railway;
      _mode[FX_MODE_RIPPLE]                  = &WS2812FX::mode_ripple;
      _mode[FX_MODE_TWINKLEFOX]              = &WS2812FX::mode_twinklefox;
      _mode[FX_MODE_TWINKLECAT]              = &WS2812FX::mode_twinklecat;
      _mode[FX_MODE_HALLOWEEN_EYES]          = &WS2812FX::mode_halloween_eyes;
      _mode[FX_MODE_STATIC_PATTERN]          = &WS2812FX::mode_static_pattern;
      _mode[FX_MODE_TRI_STATIC_PATTERN]      = &WS2812FX::mode_tri_static_pattern;
      _mode[FX_MODE_SPOTS]                   = &WS2812FX::mode_spots;
      _mode[FX_MODE_SPOTS_FADE]              = &WS2812FX::mode_spots_fade;
      _mode[FX_MODE_GLITTER]                 = &WS2812FX::mode_glitter;
      _mode[FX_MODE_CANDLE]                  = &WS2812FX::mode_candle;
      _mode[FX_MODE_STARBURST]               = &WS2812FX::mode_starburst;
      _mode[FX_MODE_EXPLODING_FIREWORKS]     = &WS2812FX::mode_exploding_fireworks;
      _mode[FX_MODE_BOUNCINGBALLS]           = &WS2812FX::mode_bouncing_balls;
      _mode[FX_MODE_SINELON]                 = &WS2812FX::mode_sinelon;
      _mode[FX_MODE_SINELON_DUAL]            = &WS2812FX::mode_sinelon_dual;
      _mode[FX_MODE_SINELON_RAINBOW]         = &WS2812FX::mode_sinelon_rainbow;
      _mode[FX_MODE_POPCORN]                 = &WS2812FX::mode_popcorn;
      _mode[FX_MODE_DRIP]                    = &WS2812FX::mode_drip;
      _mode[FX_MODE_PLASMA]                  = &WS2812FX::mode_plasma;
      _mode[FX_MODE_PERCENT]                 = &WS2812FX::mode_percent;
      _mode[FX_MODE_RIPPLE_RAINBOW]          = &WS2812FX::mode_ripple_rainbow;
      _mode[FX_MODE_HEARTBEAT]               = &WS2812FX::mode_heartbeat;
      _mode[FX_MODE_PACIFICA]                = &WS2812FX::mode_pacifica;
      _mode[FX_MODE_CANDLE_MULTI]            = &WS2812FX::mode_candle_multi;
      _mode[FX_MODE_SOLID_GLITTER]           = &WS2812FX::mode_solid_glitter;
      _mode[FX_MODE_SUNRISE]                 = &WS2812FX::mode_sunrise;
      _mode[FX_MODE_PHASED]                  = &WS2812FX::mode_phased;
      _mode[FX_MODE_TWINKLEUP]               = &WS2812FX::mode_twinkleup;
      _mode[FX_MODE_NOISEPAL]                = &WS2812FX::mode_noisepal;
      _mode[FX_MODE_SINEWAVE]                = &WS2812FX::mode_sinewave;
      _mode[FX_MODE_PHASEDNOISE]             = &WS2812FX::mode_phased_noise;
      _mode[FX_MODE_FLOW]                    = &WS2812FX::mode_flow;
      _mode[FX_MODE_CHUNCHUN]                = &WS2812FX::mode_chunchun;
      _mode[FX_MODE_DANCING_SHADOWS]         = &WS2812FX::mode_dancing_shadows;
      _mode[FX_MODE_WASHING_MACHINE]         = &WS2812FX::mode_washing_machine;
      _mode[FX_MODE_CANDY_CANE]              = &WS2812FX::mode_candy_cane;
      _mode[FX_MODE_BLENDS]                  = &WS2812FX::mode_blends;
      _mode[FX_MODE_TV_SIMULATOR]            = &WS2812FX::mode_tv_simulator;
      _mode[FX_MODE_DYNAMIC_SMOOTH]          = &WS2812FX::mode_dynamic_smooth;
      _mode[FX_MODE_EXTERNAL]                = &WS2812FX::mode_external;

      _brightness = DEFAULT_BRIGHTNESS;
      currentPalette = CRGBPalette16(CRGB::Black);
      targetPalette = CloudColors_p;
      ablMilliampsMax = ABL_MILLIAMPS_DEFAULT;
      currentMilliamps = 0;
      timebase = 0;
      resetSegments();
    }

    void
      finalizeInit(),
      service(void),
      blur(uint8_t),
      fill(uint32_t),
      fade_out(uint8_t r),
      setMode(uint8_t segid, uint8_t m),
      setColor(uint8_t slot, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0),
      setColor(uint8_t slot, uint32_t c),
      setCCT(uint16_t k),
      setBrightness(uint8_t b, bool direct = false),
      setRange(uint16_t i, uint16_t i2, uint32_t col),
      setShowCallback(show_callback cb),
      setTransition(uint16_t t),
      setTransitionMode(bool t),
      calcGammaTable(float),
      trigger(void),
      setSegment(uint8_t n, uint16_t start, uint16_t stop, uint8_t grouping = 0, uint8_t spacing = 0, uint16_t offset = UINT16_MAX),
      setMainSegmentId(uint8_t n),
      restartRuntime(),
      resetSegments(),
      makeAutoSegments(bool forceReset = false),
      fixInvalidSegments(),
      setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b, uint8_t w = 0),
      show(void),
			setTargetFps(uint8_t fps),
      deserializeMap(uint8_t n=0);

    inline void setPixelColor(uint16_t n, uint32_t c) {setPixelColor(n, byte(c>>16), byte(c>>8), byte(c), byte(c>>24));}

    bool
      gammaCorrectBri = false,
      gammaCorrectCol = true,
      checkSegmentAlignment(void),
      hasRGBWBus(void),
      hasCCTBus(void),
      // return true if the strip is being sent pixel updates
      isUpdating(void);

    uint8_t
      paletteFade = 0,
      paletteBlend = 0,
      milliampsPerLed = 55,
      autoWhiteMode = RGBW_MODE_DUAL,
      cctBlending = 0,
      getBrightness(void),
      getModeCount(void),
      getPaletteCount(void),
      getMaxSegments(void),
      getActiveSegmentsNum(void),
      getFirstSelectedSegId(void),
      getMainSegmentId(void),
      getLastActiveSegmentId(void),
      getTargetFps(void),
      setPixelSegment(uint8_t n),
      gamma8(uint8_t),
      gamma8_cal(uint8_t, float),
      get_random_wheel_index(uint8_t);

    inline uint8_t sin_gap(uint16_t in) {
      if (in & 0x100) return 0;
      return sin8(in + 192); // correct phase shift of sine so that it starts and stops at 0
    }

    int8_t
      tristate_square8(uint8_t x, uint8_t pulsewidth, uint8_t attdec);

    uint16_t
      ablMilliampsMax,
      currentMilliamps,
      triwave16(uint16_t),
      getLengthTotal(void),
      getLengthPhysical(void),
      getFps();

    uint32_t
      now,
      timebase,
      color_wheel(uint8_t),
      color_from_palette(uint16_t, bool mapping, bool wrap, uint8_t mcol, uint8_t pbri = 255),
      color_blend(uint32_t,uint32_t,uint16_t,bool b16=false),
      currentColor(uint32_t colorNew, uint8_t tNr),
      gamma32(uint32_t),
      getLastShow(void),
      getPixelColor(uint16_t);

    WS2812FX::Segment
      &getSegment(uint8_t n),
      &getFirstSelectedSeg(void),
      &getMainSegment(void);

    WS2812FX::Segment*
      getSegments(void);

    // builtin modes
    uint16_t
      mode_static(void),
      mode_blink(void),
      mode_blink_rainbow(void),
      mode_strobe(void),
      mode_strobe_rainbow(void),
      mode_color_wipe(void),
      mode_color_sweep(void),
      mode_color_wipe_random(void),
      mode_color_sweep_random(void),
      mode_random_color(void),
      mode_dynamic(void),
      mode_breath(void),
      mode_fade(void),
      mode_scan(void),
      mode_dual_scan(void),
      mode_theater_chase(void),
      mode_theater_chase_rainbow(void),
      mode_rainbow(void),
      mode_rainbow_cycle(void),
      mode_running_lights(void),
      mode_saw(void),
      mode_twinkle(void),
      mode_dissolve(void),
      mode_dissolve_random(void),
      mode_sparkle(void),
      mode_flash_sparkle(void),
      mode_hyper_sparkle(void),
      mode_multi_strobe(void),
      mode_android(void),
      mode_chase_color(void),
      mode_chase_random(void),
      mode_chase_rainbow(void),
      mode_chase_flash(void),
      mode_chase_flash_random(void),
      mode_chase_rainbow_white(void),
      mode_colorful(void),
      mode_traffic_light(void),
      mode_running_color(void),
      mode_aurora(void),
      mode_running_random(void),
      mode_larson_scanner(void),
      mode_comet(void),
      mode_fireworks(void),
      mode_rain(void),
      mode_tetrix(void),
      mode_halloween(void),
      mode_fire_flicker(void),
      mode_gradient(void),
      mode_loading(void),
      mode_police(void),
      mode_fairy(void),
      mode_two_dots(void),
      mode_fairytwinkle(void),
      mode_running_dual(void),
      mode_bicolor_chase(void),
      mode_tricolor_chase(void),
      mode_tricolor_wipe(void),
      mode_tricolor_fade(void),
      mode_lightning(void),
      mode_icu(void),
      mode_multi_comet(void),
      mode_dual_larson_scanner(void),
      mode_random_chase(void),
      mode_oscillate(void),
      mode_fire_2012(void),
      mode_pride_2015(void),
      mode_bpm(void),
      mode_juggle(void),
      mode_palette(void),
      mode_colorwaves(void),
      mode_fillnoise8(void),
      mode_noise16_1(void),
      mode_noise16_2(void),
      mode_noise16_3(void),
      mode_noise16_4(void),
      mode_colortwinkle(void),
      mode_lake(void),
      mode_meteor(void),
      mode_meteor_smooth(void),
      mode_railway(void),
      mode_ripple(void),
      mode_twinklefox(void),
      mode_twinklecat(void),
      mode_halloween_eyes(void),
      mode_static_pattern(void),
      mode_tri_static_pattern(void),
      mode_spots(void),
      mode_spots_fade(void),
      mode_glitter(void),
      mode_candle(void),
      mode_starburst(void),
      mode_exploding_fireworks(void),
      mode_bouncing_balls(void),
      mode_sinelon(void),
      mode_sinelon_dual(void),
      mode_sinelon_rainbow(void),
      mode_popcorn(void),
      mode_drip(void),
      mode_plasma(void),
      mode_percent(void),
      mode_ripple_rainbow(void),
      mode_heartbeat(void),
      mode_pacifica(void),
      mode_candle_multi(void),
      mode_solid_glitter(void),
      mode_sunrise(void),
      mode_phased(void),
      mode_twinkleup(void),
      mode_noisepal(void),
      mode_sinewave(void),
      mode_phased_noise(void),
      mode_flow(void),
      mode_chunchun(void),
      mode_dancing_shadows(void),
      mode_washing_machine(void),
      mode_candy_cane(void),
      mode_blends(void),
      mode_tv_simulator(void),
      mode_dynamic_smooth(void),
      mode_external(void);

  private:
    uint32_t crgb_to_col(CRGB fastled);
    CRGB col_to_crgb(uint32_t);
    CRGBPalette16 currentPalette;
    CRGBPalette16 targetPalette;

    uint16_t _length, _virtualSegmentLength;
    uint16_t _rand16seed;
    uint8_t _brightness;
    uint16_t _usedSegmentData = 0;
    uint16_t _transitionDur = 750;

		uint8_t _targetFps = 42;
		uint16_t _frametime = (1000/42);
    uint16_t _cumulativeFps = 2;

    bool
      _isOffRefreshRequired = false, //periodic refresh is required for the strip to remain off.
      _hasWhiteChannel = false,
      _triggered;

    mode_ptr _mode[MODE_COUNT]; // SRAM footprint: 4 bytes per element

    show_callback _callback = nullptr;

    // mode helper functions
    uint16_t
      blink(uint32_t, uint32_t, bool strobe, bool),
      candle(bool),
      color_wipe(bool, bool),
      dynamic(bool),
      scan(bool),
      running_base(bool,bool),
      larson_scanner(bool),
      sinelon_base(bool,bool),
      dissolve(uint32_t),
      chase(uint32_t, uint32_t, uint32_t, bool),
      gradient_base(bool),
      ripple_base(bool),
      police_base(uint32_t, uint32_t),
      running(uint32_t, uint32_t, bool theatre=false),
      tricolor_chase(uint32_t, uint32_t),
      twinklefox_base(bool),
      spots_base(uint16_t),
      phased_base(uint8_t);

    CRGB twinklefox_one_twinkle(uint32_t ms, uint8_t salt, bool cat);
    CRGB pacifica_one_layer(uint16_t i, CRGBPalette16& p, uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff);

    void
      blendPixelColor(uint16_t n, uint32_t color, uint8_t blend),
      startTransition(uint8_t oldBri, uint32_t oldCol, uint16_t dur, uint8_t segn, uint8_t slot),
      estimateCurrentAndLimitBri(void),
      load_gradient_palette(uint8_t),
      handle_palette(void);

    uint16_t* customMappingTable = nullptr;
    uint16_t  customMappingSize  = 0;
    
    uint32_t _lastPaletteChange = 0;
    uint32_t _lastShow = 0;

    uint32_t _colors_t[3];
    uint8_t _bri_t;
    bool _no_rgb = false;
    
    uint8_t _segment_index = 0;
    uint8_t _segment_index_palette_last = 99;
    uint8_t _mainSegment;

    segment _segments[MAX_NUM_SEGMENTS] = { // SRAM footprint: 24 bytes per element
      // start, stop, offset, speed, intensity, palette, mode, options, grouping, spacing, opacity (unused), color[], capabilities
      {0, 7, 0, DEFAULT_SPEED, 128, 0, DEFAULT_MODE, NO_OPTIONS, 1, 0, 255, {DEFAULT_COLOR}, 0}
    };
    segment_runtime _segment_runtimes[MAX_NUM_SEGMENTS]; // SRAM footprint: 28 bytes per element
    friend class Segment_runtime;

    ColorTransition transitions[MAX_NUM_TRANSITIONS]; //12 bytes per element
    friend class ColorTransition;

    uint16_t
      transitionProgress(uint8_t tNr);
  
    CRGB external_buffer[EXTERNAL_BUFFER_SIZE]; // 4 bytes per element

  public:
    static CRGB* get_external_buffer() {
      return instance->external_buffer;
    }
    static CRGB get_palette_crgb(uint16_t c, uint8_t brightness=255U) {
      uint32_t color = instance->color_from_palette(c, false, true, 255, brightness);
      return instance->col_to_crgb(color);
    }
    static void load_palette(uint8_t palette_id) {
      for (uint8_t i = 0; i < instance->getMaxSegments(); i++) {
        WS2812FX::Segment& seg = instance->getSegment(i);
        seg.palette = palette_id;
      }
    }
    static void load_pattern(uint8_t pattern_id) {
      for (uint8_t i = 0; i < instance->getMaxSegments(); i++) {
        instance->setMode(i, pattern_id);
      }
    }
    static WS2812FX* get_strip() {
      return instance;
    }

    inline bool hasWhiteChannel(void) {return _hasWhiteChannel;}
    inline bool isOffRefreshRequired(void) {return _isOffRefreshRequired;}
};

//10 names per line
const char JSON_mode_names[] PROGMEM = R"=====([
"Solid","Blink","Breathe","Wipe","Wipe Random","Random Colors","Sweep","Dynamic","Colorloop","Rainbow",
"Scan","Scan Dual","Fade","Theater","Theater Rainbow","Running","Saw","Twinkle","Dissolve","Dissolve Rnd",
"Sparkle","Sparkle Dark","Sparkle+","Strobe","Strobe Rainbow","Strobe Mega","Blink Rainbow","Android","Chase","Chase Random",
"Chase Rainbow","Chase Flash","Chase Flash Rnd","Rainbow Runner","Colorful","Traffic Light","Sweep Random","Chase 2","Aurora","Stream",
"Scanner","Lighthouse","Fireworks","Rain","Tetrix","Fire Flicker","Gradient","Loading","Police","Fairy",
"Two Dots","Fairytwinkle","Running Dual","Halloween","Chase 3","Tri Wipe","Tri Fade","Lightning","ICU","Multi Comet",
"Scanner Dual","Stream 2","Oscillate","Pride 2015","Juggle","Palette","Fire 2012","Colorwaves","Bpm","Fill Noise",
"Noise 1","Noise 2","Noise 3","Noise 4","Colortwinkles","Lake","Meteor","Meteor Smooth","Railway","Ripple",
"Twinklefox","Twinklecat","Halloween Eyes","Solid Pattern","Solid Pattern Tri","Spots","Spots Fade","Glitter","Candle","Fireworks Starburst",
"Fireworks 1D","Bouncing Balls","Sinelon","Sinelon Dual","Sinelon Rainbow","Popcorn","Drip","Plasma","Percent","Ripple Rainbow",
"Heartbeat","Pacifica","Candle Multi", "Solid Glitter","Sunrise","Phased","Twinkleup","Noise Pal", "Sine","Phased Noise",
"Flow","Chunchun","Dancing Shadows","Washing Machine","Candy Cane","Blends","TV Simulator","Dynamic Smooth", "External!"
])=====";


const char JSON_palette_names[] PROGMEM = R"=====([
"Default","* Random Cycle","* Color 1","* Colors 1&2","* Color Gradient","* Colors Only","Party","Cloud","Lava","Ocean",
"Forest","Rainbow","Rainbow Bands","Sunset","Rivendell","Breeze","Red & Blue","Yellowout","Analogous","Splash",
"Pastel","Sunset 2","Beech","Vintage","Departure","Landscape","Beach","Sherbet","Hult","Hult 64",
"Drywet","Jul","Grintage","Rewhi","Tertiary","Fire","Icefire","Cyane","Light Pink","Autumn",
"Magenta","Magred","Yelmag","Yelblu","Orange & Teal","Tiamat","April Night","Orangery","C9","Sakura",
"Aurora","Atlantica","C9 2","C9 New","Temperature","Aurora 2","Retro Clown","Candy","Toxy Reaf","Fairy Reaf",
"Semi Blue","Pink Candy","Red Reaf","Aqua Flash","Yelblu Hot","Lite Light","Red Flash","Blink Red","Red Shift","Red Tide",
"Candy2"
])=====";

#endif
