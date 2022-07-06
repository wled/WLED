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

    void randomize(long seed) {
      for (int i = 0; i < seed % 16; i++) {
        randomSeed(random(INT_MAX));
      }
      random16_add_entropy( random(INT_MAX) );  
    }

  public:
    void setup() {
      // Start timing
      globalTimer.setup();
      beats.setup();
      controller.setup(0);
      debug.setup();
    }

    void loop()
    {
      EVERY_N_MILLISECONDS(1000) {
        randomize(random(INT_MAX));
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
Fillnoise - maybe
Fireworks
Fireworks Starburst
Flow
Gradient - maybe
Juggle - maybe
Lake
Meteor Smooth - maybe
Noise 2
Noise 4
Pacifica
Palette - maybe
Phased - maybe
Plasma
Ripple
Running Dual
Saw - maybe
Sinelon Dual - maybe
Tetrix - maybe
Twinklecat
Twinkleup

*/