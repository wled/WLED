#pragma once

#include "wled.h"

#include "util.h"
#include "options.h"

// #define USERADIO

#include "FX.h"

#include "beats.h"
#include "virtual_strip.h"
#include "led_strip.h"
#include "master.h"

#include "controller.h"
#include "debug.h"


#define MASTER_PIN 25
#define LEGACY_PIN 32  // DigUno Q4


class TubesUsermod : public Usermod {
  private:
    BeatController beats;
    PatternController controller = PatternController(MAX_REAL_LEDS, &beats);
    DebugController debug = DebugController(&controller);
    Master *master = nullptr;
    bool isLegacy = false;

    void randomize() {
      randomSeed(esp_random());
      random16_set_seed(random(0, 65535));
      random16_add_entropy(esp_random());
    }

  public:
    void setup() {
      pinMode(MASTER_PIN, INPUT_PULLUP);
      pinMode(LEGACY_PIN, INPUT_PULLUP);
      if (digitalRead(MASTER_PIN) == LOW) {
      }
      isLegacy = (digitalRead(MASTER_PIN) == LOW);
      randomize();

      // Override some behaviors on all Tubes
      bootPreset = 0;  // Try to prevent initial playlists from starting
      fadeTransition = true;  // Fade palette transitions
      transitionDelay = 8000;   // Fade them for a long time
      strip.setTargetFps(60);
      strip.setCCT(100);

      // Start timing
      globalTimer.setup();
      beats.setup();
      controller.setup();
      if (controller.isMaster()) {
        master = new Master(&controller);
        master->setup();
      }
      debug.setup();
    }

    void loop()
    {
      EVERY_N_MILLISECONDS(10000) {
        randomize();
      }

      if (master) 
        master->update();
      beats.update();
      controller.update();
      debug.update();

      // Draw after everything else is done
      controller.led_strip->update();
    }

    void handleOverlayDraw()
    {
      // Draw effects layers over whatever WLED is doing.
      this->controller.handleOverlayDraw();
      if (master) 
        master->handleOverlayDraw();
      this->debug.handleOverlayDraw();
    }
};
