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
#include "radio.h"
#include "debug.h"


class TubesUsermod : public Usermod {
  private:
    BeatController beats;
    Radio radio;
    PatternController controller = PatternController(MAX_REAL_LEDS, &beats, &radio);
    DebugController debug = DebugController(&controller);
    int* master = NULL;  /* master.h deleted */

    void randomize() {
      randomSeed(esp_random());
      random16_add_entropy(esp_random());
    }

  public:
    void setup() {
      randomize();

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