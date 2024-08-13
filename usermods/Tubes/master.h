#pragma once

#include "timer.h"
#include "controller.h"
#include "led_strip.h"

#define X_AXIS_PIN 20
#define Y_AXIS_PIN 21

#define BUTTON_PIN_1   70  // SELECT?
#define BUTTON_PIN_2   71  // SKIP
#define BUTTON_PIN_3   72  // SET COLOR
#define BUTTON_PIN_4   23  // TAP
#define BUTTON_PIN_5   25  // "NEXT!"


class Master {
  public:
    uint8_t taps=0;
    Timer tapTimer;
    Timer perTapTimer;
    uint16_t tapTime[16];

    Background background;
    uint8_t palette_mode = false;
    uint8_t palette_id = 0;

    PatternController& controller;
    Button button[5];

  Master(PatternController& c) : controller(c) { };

  void setup() {
    button[0].setup(BUTTON_PIN_1);
    button[1].setup(BUTTON_PIN_2);
    button[2].setup(BUTTON_PIN_3);
    button[3].setup(BUTTON_PIN_4);
    button[4].setup(BUTTON_PIN_5);
    Serial.println((char *)F("Master: ok"));
  }

  void update() {
    for (uint8_t i=0; i < 5; i++) {
      if (button[i].triggered()) {
        if (button[i].pressed())
          onButtonPress(i);
        else
          onButtonRelease(i);
      }
    }

    if (taps && perTapTimer.ended()) {
      if (taps == 2) {
        ok();
      } else {
        fail();
      }
      taps = 0;
    }
  }

  void handleOverlayDraw() {
    updateStatus(controller);
  }

  void ok() {
    addFlash(CRGB::Green);
  }

  void fail() {
    addFlash(CRGB::Red);
  }

  void onButtonPress(uint8_t b) {
    if (b == 0)
      return;

    if (b == 4) {
      Serial.println((char *)F("Skip >>"));
      controller.force_next();
      ok();
      return;
    }

    if (b == 3) {
      tap();
      return;
    }

    Serial.print((char *)F("Pressed "));
    Serial.println(b);
  }

  void onButtonRelease(uint8_t b) {
#ifdef EXTRA_STUFF
    if (b == 2) {
      if (palette_mode)
        controller._load_palette(palette_id);
      palette_mode = false;
    }
#endif

    if (b == 3) {
      if (taps == 0)
        return;
      tap();
      return;
    }

    Serial.print((char *)F("Released "));
    Serial.println(b);
  }

  void tap() {
    if (!taps) {
      tapTimer.start(0);
    }
    perTapTimer.start(1500);

    uint32_t time = tapTimer.since_mark();
    tapTime[taps++] = time;

    uint32_t bpm = 0;
    if (taps > 4) {
      // Can study this later to make BPM detection better

      // Should be 60000; fudge a bit to adjust to real-world timings
      bpm = 60220*256*(taps-1) / time;  // 120 beats per min = 500ms per beat
      if (bpm < 70*256)
        bpm *= 2;
      else if (bpm > 140*256)
        bpm /= 2;
    }

    Serial.printf("tap %d: ", taps);
    Serial.print(bpm >> 8);
    uint8_t f = scale8(100, bpm & 0xFF);
    Serial.print(".");
    if (f < 10)
      Serial.print("0");
    Serial.println(f);

    if (taps == 16) {
      Serial.println("OK! taps");
      taps = 0;
      auto frac = bpm % 256;

      // Slight snap to beat
      if (frac < 128)
        bpm -= frac / 2;
      else if (frac > 128)
        bpm += (256-frac) / 2;
      
      controller.set_tapped_bpm(bpm);
      ok();
    } else if (taps >= 2) {
      controller.set_tapped_bpm(controller.current_state.bpm, taps-1);
    }
  }

  void updateStatus(const PatternController& controller) {
    if (taps) {
      displayProgress(taps);
    } else if (palette_mode) {
      displayPalette(background);
    } else {
      uint8_t beat_pos = (controller.current_state.beat_frame >> 8) % 16;
      strip.setPixelColor(15 - beat_pos, CRGB::White);
    }
  }

  void displayProgress(uint8_t progress) {
    for (int i = 0; i < 16; i++) {
      if (i < progress % 16) {
        strip.setPixelColor(15 - i, CRGB(128,128,128));
      } else {
        strip.setPixelColor(15 - i, CRGB::Black);
      }
    }
  }

  void displayPalette(Background &background) {
    for (int i = 0; i < 16; i++) {
      Segment& segment = strip.getMainSegment();
      auto color = CRGB(segment.color_from_palette(i * 16, false, true, 255));
      strip.setPixelColor(i, color);
    }
  }

};

