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
      strip.setTargetFps(60);

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
      // Draw effects layers over whatever WLED is doing.
      this->controller.handleOverlayDraw();
      this->debug.handleOverlayDraw();
    }
};
