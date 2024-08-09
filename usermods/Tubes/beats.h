#pragma once

#include "timer.h"

#define DEFAULT_BPM  120

typedef uint32_t BeatFrame_24_8;  // 24:8 bitwise float

// Regulates the beat counter, running patterns at 256 "fracs" per beat
class BeatController {
  public:
    accum88 bpm = 0;
    BeatFrame_24_8 frac;
    uint32_t accum = 0;
    uint32_t micros_per_frac;

  void setup()
  {
    // Starts in phrase 1
    sync(DEFAULT_BPM << 8, 0);
  }

  void update()
  {
    // Maintains an accumulator with 14 bits of precision
    accum += globalTimer.delta_micros << 8;
    while (accum > micros_per_frac) {
      frac++;
      accum -= micros_per_frac;
    }
  }

  void sync(accum88 b, BeatFrame_24_8 f) {
    if (b < 40<<8) {
      // Reject BPMs that are too low.
      return;
    }
    
    accum88 last_bpm = bpm;
    bpm = b;
    frac = f;
    accum = 0;

    micros_per_frac = (uint32_t)(15360000000.0 / (float)bpm);

    if (last_bpm != bpm)
      print_bpm();
  }
  
  void set_bpm(accum88 b) {
    sync(b, frac);
  }

  void adjust_bpm(saccum78 b) {
    sync(bpm + b, frac);
  }

  void start_phrase() {
    frac &= -0xFFF;
    accum = 0;
  }

  void print_bpm() {
    Serial.print(bpm >> 8);
    uint8_t frac = scale8(100, bpm & 0xFF);
    Serial.print(F("."));
    if (frac < 10)
      Serial.print(F("0"));
    Serial.print(frac);
    Serial.println(F("bpm"));
  }

};
