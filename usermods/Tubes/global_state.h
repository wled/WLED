#pragma once

#include "wled.h"
#include "beats.h"
#include "effects.h"


class TubeState {
  public:
    // Global clock: frames are defined as 1/64th of a beat
    accum88 bpm = 0;            // BPM in high 8 bits, fraction in low 8 bits 
    BeatFrame_24_8 beat_frame = 0;  // current beat (24 bits) and fractional beat (8bits)
  
    uint16_t pattern_phrase;
    uint8_t pattern_id;
    uint8_t pattern_sync_id;
  
    uint16_t palette_phrase;
    uint8_t palette_id;
  
    uint16_t effect_phrase;
    EffectParameters effect_params;

  void print() {
    uint16_t phrase = this->beat_frame >> 12;
    Serial.print(F("["));
    Serial.print(phrase);
    Serial.print(F("."));
    Serial.print((this->beat_frame >> 8) % 16);
    Serial.print(F(" P"));
    Serial.print(this->pattern_id);
    Serial.print(F(","));
    Serial.print(this->pattern_sync_id);
    Serial.print(F(" C"));
    Serial.print(this->palette_id);
    Serial.print(F(" E"));
    Serial.print(this->effect_params.effect);
    Serial.print(F(","));
    Serial.print(this->effect_params.pen);
    Serial.print(F(","));
    Serial.print(this->effect_params.beat);
    Serial.print(F(","));
    Serial.print(this->effect_params.chance);
    Serial.print(F(" "));
    Serial.print(this->bpm >> 8);
    uint8_t frac = scale8(100, this->bpm & 0xFF);
    Serial.print(F("."));
    if (frac < 10)
      Serial.print(F("0"));
    Serial.print(frac);
    Serial.print(F("bpm]"));  
  }

};

typedef uint8_t CommandId;

const static CommandId COMMAND_OPTIONS = 0x10;
const static CommandId COMMAND_UPDATE = 0x20;
const static CommandId COMMAND_RESET = 0xF0;