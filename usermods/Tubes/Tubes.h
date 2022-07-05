#pragma once

#include "wled.h"

#include "util.h"
#include "options.h"

// #define MASTERCONTROL

#define MASTER_PIN 6
#define NUM_LEDS 64

#define USERADIO

#include "FX.h"

#include "beats.h"
#include "virtual_strip.h"

// #include "controller.h"
// #include "radio.h"
// #include "debug.h"


class TubesUsermod : public Usermod {
  private:
    BeatController beats;
    // Radio radio;
    // PatternController controller(NUM_LEDS, &beats, &radio);
    // DebugController debug(&controller);

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
      // controller.setup(0);
      // debug.setup();
    }

    void loop()
    {
      EVERY_N_MILLISECONDS(1000) {
        randomize(random(INT_MAX));
      }

      beats.update(); // ~30us
      // controller.update(); // radio: 0-3000us   patterns: 0-3000us   lcd: ~50000us
      // debug.update(); // ~25us

      // Draw after everything else is done
      // controller.led_strip->update(master != NULL); // ~25us

      CRGB *external_buffer = WS2812FX::get_external_buffer();
      for (int i = 0; i < 10; i++) {
        external_buffer[i] = CRGB::White;
      }
    }
};