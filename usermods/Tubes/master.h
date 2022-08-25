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

    PatternController *controller;
    Button button[5];

  Master(PatternController *controller) {
    this->controller = controller;
  }

  void setup() {
    this->button[0].setup(BUTTON_PIN_1);
    this->button[1].setup(BUTTON_PIN_2);
    this->button[2].setup(BUTTON_PIN_3);
    this->button[3].setup(BUTTON_PIN_4);
    this->button[4].setup(BUTTON_PIN_5);
    Serial.println((char *)F("Master: ok"));
  }

  void update() {
    for (uint8_t i=0; i < 5; i++) {
      if (button[i].triggered()) {
        if (button[i].pressed())
          this->onButtonPress(i);
        else
          this->onButtonRelease(i);
      }
    }

    if (this->taps && this->perTapTimer.ended()) {
      if (this->taps == 2) {
        this->ok();
      } else {
        this->fail();
      }
      this->taps = 0;
    }
  }

  void handleOverlayDraw() {
    this->updateStatus(this->controller);
  }

  void ok() {
    addFlash(CRGB::Green);
  }

  void fail() {
    addFlash(CRGB::Red);
  }

  void onButtonPress(uint8_t button) {
    if (button == 0)
      return;

    if (button == 4) {
      Serial.println((char *)F("Skip >>"));
      this->controller->force_next();
      this->ok();
      return;
    }

    if (button == 3) {
      this->tap();
      return;
    }

    Serial.print((char *)F("Pressed "));
    Serial.println(button);
  }

  void onButtonRelease(uint8_t button) {
#ifdef EXTRA_STUFF
    if (button == 2) {
      if (this->palette_mode)
        this->controller->_load_palette(this->palette_id);
      this->palette_mode = false;
    }
#endif

    if (button == 3) {
      if (this->taps == 0)
        return;
      this->tap();
      return;
    }

    Serial.print((char *)F("Released "));
    Serial.println(button);
  }

  void tap() {
    Serial.println((char *)F("tap"));
    if (!this->taps) {
      this->tapTimer.start(0);
    }
    this->perTapTimer.start(1000);

    uint32_t time = this->tapTimer.since_mark();
    this->tapTime[this->taps++] = time;

    uint32_t bpm = 0;
    if (this->taps > 4) {
      // Can study this later to make BPM detection better

      bpm = 60000*256*(this->taps-1) / time;  // 120 beats per min = 500ms per beat
      if (bpm < 70*256)
        bpm *= 2;
      else if (bpm > 140*256)
        bpm /= 2;
    }
    
    if (this->taps == 16) {
      Serial.println("OK! taps");
      this->taps = 0;
      auto frac = bpm % 256;

      // Slight snap to beat
      if (frac < 128)
        bpm -= frac / 2;
      else if (frac > 128)
        bpm += (256-frac) / 2;
      
      this->controller->set_tapped_bpm(bpm);
      this->ok();
    } else if (this->taps >= 2) {
      this->controller->set_tapped_bpm(this->controller->current_state.bpm, taps-1);
    }
  }

  void updateStatus(PatternController *controller) {
    if (this->taps) {
      this->displayProgress(this->taps);
    } else if (this->palette_mode) {
      this->displayPalette(this->background);
    } else {
      uint8_t beat_pos = (controller->current_state.beat_frame >> 8) % 16;
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

