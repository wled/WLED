#pragma once

#include "virtual_strip.h"
#include "FX.h"

void rainbow(VirtualStrip *strip) 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( strip->leds, strip->length(), strip->hue, 3);
}

void palette_wave(VirtualStrip *strip) 
{
  // FastLED's built-in rainbow generator
  uint8_t hue = strip->hue;
  for (uint8_t i=0; i < strip->length(); i++) {
    CRGB c = strip->palette_color(i, hue);
    nscale8x3(c.r, c.g, c.b, sin8(hue*8));
    strip->leds[i] = c;
    hue++;
  }
}

void particleTest(VirtualStrip *strip)
{
  fill_solid( strip->leds, strip->length(), CRGB::Black);
  fill_solid( strip->leds, 2, strip->palette_color(0, strip->hue));
}

void solidBlack(VirtualStrip *strip)
{
  fill_solid( strip->leds, strip->length(), CRGB::Black);
}

void solidWhite(VirtualStrip *strip) 
{
  fill_solid( strip->leds, strip->length(), CRGB::White);
}

void solidRed(VirtualStrip *strip) 
{
  fill_solid( strip->leds, strip->length(), CRGB::Red);
}

void solidBlue(VirtualStrip *strip) 
{
  fill_solid( strip->leds, strip->length(), CRGB::Blue);
}

void confetti(VirtualStrip *strip) 
{
  strip->darken(2);
  
  int pos = random16(strip->length());
  strip->leds[pos] += strip->palette_color(random8(64), strip->hue);
}

uint16_t random_offset = random16();

void biwave(VirtualStrip *strip)
{
  uint16_t l = strip->frame * 16;
  l = sin16( l + random_offset ) + 32768;

  uint16_t r = strip->frame * 32;
  r = cos16( r + random_offset ) + 32768;

  uint8_t p1 = scaled16to8(l, 0, strip->length()-1);
  uint8_t p2 = scaled16to8(r, 0, strip->length()-1);
  
  if (p2 < p1) {
    uint16_t t = p1;
    p1 = p2;
    p2 = t;
  }

  strip->fill(CRGB::Black);
  for (uint16_t p = p1; p <= p2; p++) {
    strip->leds[p] = strip->palette_color(p*2, strip->hue*3);
  }
}

void tick(VirtualStrip *strip) {
  strip->fill(CRGB::Black);
  strip->leds[strip->beat % 16] = CRGB::White;
}

void sinelon(VirtualStrip *strip) 
{
  // a colored dot sweeping back and forth, with fading trails
  strip->darken(30);

  int pos = scale16(sin16( strip->frame << 5 ) + 32768, strip->length()-1);   // beatsin16 re-implemented
  strip->leds[pos] += strip->hue_color();
}

void bpm_palette(VirtualStrip *strip) 
{
  uint8_t beat = strip->bpm_sin16(64, 255);
  for (int i = 0; i < strip->length(); i++) {
    CRGB c = strip->palette_color(i*2, strip->hue, beat-strip->hue+(i*10));
    strip->leds[i] = c;
  }
}

void bpm(VirtualStrip *strip) 
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  CRGBPalette16 palette = PartyColors_p;

  uint8_t beat = strip->bpm_sin16(64, 255);
  for (int i = 0; i < strip->length(); i++) {
    strip->leds[i] = ColorFromPalette(palette, strip->hue+(i*2), beat-strip->hue+(i*10));
  }
}

void juggle(VirtualStrip *strip) 
{
  // eight colored dots, weaving in and out of sync with each other
  strip->darken(5);

  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    CRGB c = strip->palette_color(dothue + strip->hue);
    // c = CHSV(dothue, 200, 255);
    strip->leds[beatsin16( i+7, 0, strip->length()-1 )] |= c;
    dothue += 32;
  }
}

uint8_t noise[MAX_VIRTUAL_LEDS];

void fillnoise8(uint32_t frame, uint8_t num_leds) {
  uint16_t scale = 17;
  uint8_t dataSmoothing = 240;

  auto max_leds = sizeof(noise)/sizeof(noise[0]);
  if (num_leds > max_leds) {
    num_leds = max_leds;
  }

  for (int i = 0; i < num_leds; i++) {
    uint8_t data = inoise8(i * scale, frame>>2);

    // The range of the inoise8 function is roughly 16-238.
    // These two operations expand those values out to roughly 0..255
    data = qsub8(data,16);
    data = qadd8(data,scale8(data,39));

    uint8_t olddata = noise[i];
    uint8_t newdata = scale8( olddata, dataSmoothing) + scale8( data, 256 - dataSmoothing);
    noise[i] = newdata;
  }
}

void drawNoise(VirtualStrip *strip)
{
  // generate noise data
  fillnoise8(strip->frame >> 2, strip->length());

  for(int i = 0; i < strip->length(); i++) {
    CRGB color = strip->palette_color(noise[i], strip->hue);
    strip->leds[i] = color;
  }
}

void draw_wled_fx(VirtualStrip *strip) {
}

typedef struct {
  uint8_t wled_fx_id;
  BackgroundFn backgroundFn;
  ControlParameters control;
} PatternDef;


// List of patterns to cycle through.  Each is defined as a separate function below.
static const uint8_t numInternalPatterns = 24;
PatternDef gPatterns[] = { 
  {0, drawNoise, {ShortDuration}},
  {0, drawNoise, {ShortDuration}},
  {0, drawNoise, {MediumDuration}},
  {0, drawNoise, {MediumDuration}},
  {0, drawNoise, {MediumDuration}},
  {0, drawNoise, {LongDuration}},
  {0, drawNoise, {LongDuration}},
  {0, confetti, {ShortDuration}},
  {0, confetti, {MediumDuration}},
  {0, juggle, {ShortDuration}},

  {0, drawNoise, {ShortDuration}},
  {0, drawNoise, {ShortDuration}},
  {0, drawNoise, {MediumDuration}},
  {0, drawNoise, {MediumDuration}},
  {0, drawNoise, {MediumDuration}},
  {0, drawNoise, {LongDuration}},
  {0, drawNoise, {LongDuration}},
  {0, confetti, {ShortDuration}},
  {0, confetti, {MediumDuration}},
  {0, juggle, {ShortDuration}},

  {0, palette_wave, {ShortDuration, Boring}},
  {0, palette_wave, {MediumDuration, Boring}},
  {0, bpm_palette, {ShortDuration}},
  {0, bpm_palette, {MediumDuration, HighEnergy}},
  {FX_MODE_FADE, draw_wled_fx, {ShortDuration, Boring}}, // 12
  {FX_MODE_CHASE_RAINBOW, draw_wled_fx, {MediumDuration, HighEnergy}}, // 30
      // Make it HighEnergy? or find out why it's sometimes flashy
  {FX_MODE_AURORA, draw_wled_fx, {MediumDuration, Boring}}, // 38
      // TODO: Aurora is too dark?
  {FX_MODE_GRADIENT, draw_wled_fx,{ShortDuration}}, // 46
  {FX_MODE_FAIRYTWINKLE, draw_wled_fx, {LongDuration}}, // 51
  {FX_MODE_RUNNING_DUAL, draw_wled_fx, {ExtraShortDuration, Boring}},// 52

  {FX_MODE_DUAL_LARSON_SCANNER, draw_wled_fx, {MediumDuration}}, // 60
  {FX_MODE_JUGGLE, draw_wled_fx, {MediumDuration}}, // 64
  {FX_MODE_PALETTE, draw_wled_fx, {ShortDuration}},// 65
  {FX_MODE_FIRE_2012, draw_wled_fx, {MediumDuration}}, // 66
  {FX_MODE_BPM, draw_wled_fx, {MediumDuration}}, // 68
      // TODO: needs to be slowed down
  {FX_MODE_FILLNOISE8, draw_wled_fx, {LongDuration}}, // 69
  {FX_MODE_NOISE16_2, draw_wled_fx, {MediumDuration}}, // 71
  {FX_MODE_NOISE16_3, draw_wled_fx, {ShortDuration}}, // 72
  {FX_MODE_NOISE16_3, draw_wled_fx, {LongDuration, MediumEnergy}}, // 72
      // TODO: Noise3 needs to be slowed down, it's a bit spastic
  {FX_MODE_COLORTWINKLE, draw_wled_fx, {MediumDuration}}, // 74

  {FX_MODE_LAKE, draw_wled_fx, {ShortDuration}}, // 75
  {FX_MODE_LAKE, draw_wled_fx, {MediumDuration}}, // 75
  {FX_MODE_LAKE, draw_wled_fx, {LongDuration}}, // 75
  {FX_MODE_METEOR_SMOOTH, draw_wled_fx, {MediumDuration}}, // 77
  {FX_MODE_STARBURST, draw_wled_fx, {ExtraShortDuration, HighEnergy}}, // 89
  {FX_MODE_EXPLODING_FIREWORKS, draw_wled_fx, {ExtraShortDuration}},// 90
      // TODO: Must be set to only fire from one side
  {FX_MODE_SINELON_DUAL, draw_wled_fx, {MediumDuration}}, // 93
  {FX_MODE_POPCORN, draw_wled_fx, {ShortDuration, MediumEnergy}}, // 95
  {FX_MODE_PLASMA, draw_wled_fx, {ShortDuration}}, // 97
  {FX_MODE_PLASMA, draw_wled_fx, {LongDuration}}, // 97

  {FX_MODE_PACIFICA, draw_wled_fx, {ShortDuration}}, // 101
  {FX_MODE_PACIFICA, draw_wled_fx, {LongDuration}}, // 101
  {FX_MODE_TWINKLEUP, draw_wled_fx, {LongDuration}}, // 106
  {FX_MODE_NOISEPAL, draw_wled_fx, {LongDuration}}, // 107
  {FX_MODE_PHASEDNOISE, draw_wled_fx, {MediumDuration}}, // 109
  {FX_MODE_FLOW, draw_wled_fx, {ShortDuration}}, // 110
  {FX_MODE_FLOW, draw_wled_fx, {ShortDuration}}, // 110
  {FX_MODE_FLOW, draw_wled_fx, {ShortDuration}}, // 110
  {FX_MODE_FLOW, draw_wled_fx, {MediumDuration}}, // 110
  {FX_MODE_FLOW, draw_wled_fx, {MediumDuration}}, // 110

  {FX_MODE_FLOW, draw_wled_fx, {LongDuration}}, // 110
  {FX_MODE_FLOW, draw_wled_fx, {ExtraLongDuration}}, // 110
  {FX_MODE_FIRE_2012, draw_wled_fx, {ShortDuration}}, // 66
  {FX_MODE_FIRE_2012, draw_wled_fx, {MediumDuration}}, // 66
  {FX_MODE_PHASEDNOISE, draw_wled_fx, {ShortDuration}}, // 109

};

/*
*/
const uint8_t gPatternCount = ARRAY_SIZE(gPatterns);

/*

WLED OK not great:
4  - Wipe random 
8 - Colorloop
15 - Running
18 - Dissolve
27 - Android
36 - Sweep Random
41 - Lighthouse
44 - Tetrix
50 - Two Dots
56 - Tri Fade
67 - Color Waves --- maybe too spastic
70 - Noise 1
73 - Noise 4
80 - Twinklefox
104 - Sunrise
108 - Sine Wave
115 - Blends
154 - Plasmoid
157 - Noisemeter

LIST OF GOOD PATTERNS

Aurora
Dynamic Smooth
Blends
Colortwinkles
Fireworks
Fireworks Starburst
Flow
Lake
Noise 2
Noise 4
Pacifica
Plasma
Ripple
Running Dual
Twinklecat
Twinkleup

AUDIOREACTIVE
Midnoise
GravCenter

MAYBE GOOD PATTERNS
Fillnoise
Gradient
Juggle
Meteor Smooth
Palette
Phased
Saw
Sinelon Dual
Tetrix


Colors to fix:
72 - replace with something else
35 - drops framerate
62 - drops framerate
61 - drops framerate

*/