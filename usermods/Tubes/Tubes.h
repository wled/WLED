#pragma once

#include "wled.h"

#include "util.h"
#include "options.h"

// #define MASTERCONTROL

#define MASTER_PIN    6

// #define USERADIO

#include "FX.h"

#include "beats.h"
#include "virtual_strip.h"
#include "led_strip.h"

#include "controller.h"
#include "debug.h"


class TubesUsermod : public Usermod {
  private:
    BeatController beats;
    PatternController controller = PatternController(MAX_REAL_LEDS, &beats);
    DebugController debug = DebugController(&controller);
    int* master = NULL;  /* master.h deleted */

    void randomize() {
      randomSeed(esp_random());
      random16_set_seed(random(0, 65535));
      random16_add_entropy(esp_random());
    }

  public:
    void setup() {
      randomize();

      // Override some behaviors on all Tubes
      bootPreset = 0;  // Try to prevent initial playlists from starting
      fadeTransition = true;  // Fade palette transitions
      transitionDelay = 8000;   // Fade them for a long time
      WS2812FX::load_pattern(DEFAULT_WLED_FX);  // Crossfade with FLOW

      // Start timing
      globalTimer.setup();
      beats.setup();
      controller.setup(0);
      debug.setup();
    }

    void loop()
    {
      EVERY_N_MILLISECONDS(10000) {
        randomize();
      }

      beats.update();
      controller.update();
      debug.update();

      // Draw after everything else is done
      controller.led_strip->update(master != NULL); // ~25us
    }

    void handleOverlayDraw()
    {
      // Perform a cross-fade between current WLED mode and the external buffer

      // uint8_t segment_id = strip.getMainSegmentId();
      uint16_t length = strip.getLengthTotal();
      auto external_buffer = strip.get_external_buffer();

      uint8_t fade = sin8(millis() / 40); // amount that Tubes overwrites WLED, 0-255
      
      if (fade > 0) {
        for (int i = 0, p = 0; i < length; i++, p++) {
          if (p >= EXTERNAL_BUFFER_SIZE) {
            p = 0;
          }

          CRGB color1 = strip.getPixelColor(i);
          CRGB color2 = external_buffer[p];

          uint8_t r = blend8(color1.r, color2.r, fade);
          uint8_t g = blend8(color1.g, color2.g, fade);
          uint8_t b = blend8(color1.b, color2.b, fade);

          strip.setPixelColor(i, CRGB(r,g,b));
        }
      }

      // Draw effects layers over whatever WLED is doing.
      WS2812FX* leds = &strip;
      controller.effects->draw(leds);
    }
};




/*
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

*/