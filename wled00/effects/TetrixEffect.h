#pragma once

#include "../FX.h"
#include "Effect.h"
#include "BufferedEffect.h"

#include <type_traits> //TODO remove

class TetrixEffect : public BaseEffect<TetrixEffect, BufferedEffect> {
private:
    struct Tetris {
        float    pos{};
        float    speed{};
        uint8_t  col{};   // color index
        uint16_t brick{}; // brick size in pixels
        uint16_t stack{0u}; // stack size in pixels
        uint32_t step{}; // 2D-fication of SEGENV.step (state)
    };

    using Self = TetrixEffect;
    using Base = BaseEffect<TetrixEffect, BufferedEffect>;

public:
    explicit TetrixEffect(const EffectInformation& ei) : Base{ei, true} {}

    static constexpr EffectInformation effectInformation {
        "Tetrix@!,Width,,,,One color;!,!;!;;sx=0,ix=0,pal=11,m12=1",
        FX_MODE_TETRIX,
        0u,
        &Self::makeEffect,
        &Self::nextFrame,
        &Self::nextRow,
        &Self::getPixelColor,
    };

    void nextFrameImpl() {
      Base::nextFrameImpl();

      unsigned strips = SEGMENT.nrOfVStrips(); // allow running on virtual strips (columns in 2D segment)
      drops.resize(strips);
      if (drops.size() != strips) {
        drops.clear();
        return;
      }

      for (unsigned stripNr=0; stripNr<strips; stripNr++)
        runStrip(stripNr, &drops[stripNr]);
    }

private:
    // virtualStrip idea by @ewowi (Ewoud Wijma)
    // requires virtual strip # to be embedded into upper 16 bits of index in setPixelcolor()
    // the following functions will not work on virtual strips: fill(), fade_out(), fadeToBlack(), blur()
    void runStrip(size_t stripNr, Tetris *drop) {
      // initialize dropping on first call or segment full
      if (SEGENV.call == 0) {
        drop->stack = 0;                  // reset brick stack size
        drop->step = strip.now + 2000;     // start by fading out strip
        if (SEGMENT.check1) drop->col = 0;// use only one color from palette
      }

      if (drop->step == 0) {              // init brick
        // speed calculation: a single brick should reach bottom of strip in X seconds
        // if the speed is set to 1 this should take 5s and at 255 it should take 0.25s
        // as this is dependant on SEGLEN it should be taken into account and the fact that effect runs every FRAMETIME s
        int speed = SEGMENT.speed ? SEGMENT.speed : hw_random8(1,255);
        speed = map(speed, 1, 255, 5000, 250); // time taken for full (SEGLEN) drop
        drop->speed = float(SEGLEN * FRAMETIME) / float(speed); // set speed
        drop->pos   = SEGLEN;             // start at end of segment (no need to subtract 1)
        if (!SEGMENT.check1) drop->col = hw_random8(0,15)<<4;   // limit color choices so there is enough HUE gap
        drop->step  = 1;                  // drop state (0 init, 1 forming, 2 falling)
        drop->brick = (SEGMENT.intensity ? (SEGMENT.intensity>>5)+1 : hw_random8(1,5)) * (1+(SEGLEN>>6));  // size of brick
      }

      if (drop->step == 1) {              // forming
        if (hw_random8()>>6) {               // random drop
          drop->step = 2;                 // fall
        }
      }
      if (drop->step == 2) {              // falling
        if (drop->pos > drop->stack) {    // fall until top of stack
          drop->pos -= drop->speed;       // may add gravity as: speed += gravity
          if (int(drop->pos) < int(drop->stack)) drop->pos = drop->stack;
          for (unsigned i = unsigned(drop->pos); i < SEGLEN; i++) {
            uint32_t col = i < unsigned(drop->pos)+drop->brick ? SEGMENT.color_from_palette(drop->col, false, false, 0) : SEGCOLOR(1);
            buffer.setPixelColor(indexToVStrip(i, stripNr), col);
          }
        } else {                          // we hit bottom
          drop->step = 0;                 // proceed with next brick, go back to init
          drop->stack += drop->brick;     // increase the stack size
          if (drop->stack >= SEGLEN) drop->step = strip.now + 2000; // fade out stack
        }
      }

      if (drop->step > 2) {               // fade strip
        drop->brick = 0;                  // reset brick size (no more growing)
        if (drop->step > strip.now) {
          // allow fading of virtual strip
          for (unsigned i = 0; i < SEGLEN; i++) buffer.blendPixelColor(indexToVStrip(i, stripNr), SEGCOLOR(1), 25); // 10% blend
        } else {
          drop->stack = 0;                // reset brick stack size
          drop->step = 0;                 // proceed with next brick
          if (SEGMENT.check1) drop->col += 8;   // gradually increase palette index
        }
      }
    }

private:
    std::vector<Tetris> drops;
};
