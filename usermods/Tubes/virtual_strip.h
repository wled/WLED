#pragma once

#include "util.h"
#include "options.h"
#include "beats.h"
#include "palettes.h"
#include "wled.h"

#define DEFAULT_FADE_SPEED 100
#define MAX_VIRTUAL_LEDS 500

#define DEFAULT_WLED_FX FX_MODE_RAINBOW_CYCLE

class VirtualStrip;
typedef void (*BackgroundFn)(VirtualStrip *strip);

class Background {
  public:
    BackgroundFn animate;
    uint8_t wled_fx_id;
    uint8_t palette_id;
    SyncMode sync=All;
};

typedef enum VirtualStripFade {
  Steady=0,
  FadeIn=1,
  FadeOut=2,
  Dead=99,
} VirtualStripFade;

BeatFrame_24_8 swing(BeatFrame_24_8 frame) {
  uint16_t fr = (frame & 0x3FF); // grab 4 beats
  if (fr < 256)
    fr = ease8InOutApprox(fr) << 2;
  else
    fr = 0x3FF;
  
  return (frame & 0xFC00) + fr;  // recompose it
}

class VirtualStrip {
  // Let WLED do the dimming
  const static uint16_t DEF_BRIGHT = 255;

  public:
    CRGB leds[MAX_VIRTUAL_LEDS];
    uint8_t brightness;

    // Fade in/out
    VirtualStripFade fade;
    uint16_t fader;
    uint8_t fade_speed;

    // Pattern parameters
    Background background;
    uint32_t frame;
    uint8_t beat;
    uint16_t beat16;  // 8 bits of beat and 8 bits of fractional
    uint8_t hue;
    bool beat_pulse;
    int bps = 0;

  VirtualStrip()
  {
    fade = Dead;
  }

  int32_t length() {
    // Try to be the same as the main segment, but not if it's too big
    auto len = strip.getMainSegment().length();
    if (len > MAX_VIRTUAL_LEDS)
      return MAX_VIRTUAL_LEDS;
    return len;
  }

  void load(Background &b, uint8_t fs=DEFAULT_FADE_SPEED)
  {
    background = b;
    fade = FadeIn;
    fader = 0;
    fade_speed = fs;
    brightness = DEF_BRIGHT;
  }

  bool isWled() {
    return background.wled_fx_id != 0;
  }

  void fadeOut(uint8_t fs=DEFAULT_FADE_SPEED)
  {
    if (fade == Dead)
      return;
    fade = FadeOut;
    fade_speed = fs;
  }

  void darken(uint8_t amount=10)
  {
    fadeToBlackBy( leds, length(), amount);
  }

  void fill(CRGB crgb) 
  {
    fill_solid( leds, length(), crgb);
  }

  void update(BeatFrame_24_8 fr, uint8_t bp)
  {
    if (fade == Dead)
      return;
    
    frame = fr;

    switch (this->background.sync) {
      case All:
        break;  

      case SinDrift:
        // Drift slightly
        frame = frame + (beatsin16( 5 ) >> 6);
        break;

      case Swing:
        // Swing the beat
        frame = swing(frame);
        break;

      case SwingDrift:
        // Swing the beat AND drift slightly
        frame = swing(frame) + (beatsin16( 5 ) >> 6);
        break;

      case Pulse:
        // Pulsing from 30 - 210 brightness
        brightness = scale8(beatsin8( 10 ), 180) + 30;
        break;
    }
    hue = (frame >> 4) % 256;
    beat = (frame >> 8) % 16;
    beat_pulse = bp;

    // Animate this virtual strip
    background.animate(this);

    switch (fade) {
      case Steady:
      case Dead:
        break;
        
      case FadeIn:
        if (65535 - fader < fade_speed) {
          fader = 65535;
          fade = Steady;
        } else {
          fader += fade_speed;
        }
        break;
        
      case FadeOut:
        if (fader < fade_speed) {
          fader = 0;
          fade = Dead;
          return;
        } else {
          fader -= fade_speed;
        }
        break;
    }
  }

  CRGB palette_color(uint8_t c, uint8_t offset=0, uint8_t brightness=255) {
    Segment& segment = strip.getMainSegment();
    uint32_t color = segment.color_from_palette(c + offset, false, true, 255, brightness);
    return CRGB(color);
  }

  CRGB hue_color(uint8_t offset=0, uint8_t saturation=255, uint8_t value=192) {
    return CHSV(hue + offset, saturation, value);
  }
 
  uint8_t bpm_sin16( uint16_t lowest=0, uint16_t highest=65535 )
  {
    return scaled16to8(sin16( frame << 7 ) + 32768, lowest, highest);
  }

  uint8_t bpm_cos16( uint16_t lowest=0, uint16_t highest=65535 )
  {
    return scaled16to8(cos16( frame << 7 ) + 32768, lowest, highest);
  }

  CRGB getPixelColor(int32_t pos) {
    return leds[pos % length()];
  }

};
