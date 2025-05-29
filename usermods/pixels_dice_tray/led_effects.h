/**
 * The LED effects influenced by dice rolls.
 */
#pragma once

#include "wled.h"

#include "dice_state.h"

// Reuse FX display functions.
extern uint16_t mode_breath();
extern uint16_t mode_blends();
extern uint16_t mode_glitter();
extern uint16_t mode_gravcenter();

static constexpr uint8_t USER_ANY_DIE = 0xFF;
/**
 * Two custom effect parameters are used.
 * c1 - Source Die. Sets which die from [0 - MAX_NUM_DICE) controls this effect.
 *      If this is set to 0xFF, use the latest event regardless of which die it
 *      came from.
 * c2 - Target Roll. Sets the "success" criteria for a roll to >= this value.
 */

/**
 * Return the last die roll based on the custom1 effect setting.
 */
static pixels::RollEvent GetLastRollForSegment() {
  // If an invalid die is selected, fallback to using the most recent roll from
  // any die.
  if (SEGMENT.custom1 >= MAX_NUM_DICE) {
    return GetLastRoll();
  } else {
    return last_die_events[SEGMENT.custom1];
  }
}


/*
 * Alternating pixels running function (copied static function).
 */
// paletteBlend: 0 - wrap when moving, 1 - always wrap, 2 - never wrap, 3 - none (undefined)
#define PALETTE_SOLID_WRAP   (strip.paletteBlend == 1 || strip.paletteBlend == 3)
static uint16_t running_copy(uint32_t color1, uint32_t color2, bool theatre = false) {
  int width = (theatre ? 3 : 1) + (SEGMENT.intensity >> 4);  // window
  uint32_t cycleTime = 50 + (255 - SEGMENT.speed);
  uint32_t it = strip.now / cycleTime;
  bool usePalette = color1 == SEGCOLOR(0);

  for (int i = 0; i < SEGLEN; i++) {
    uint32_t col = color2;
    if (usePalette) color1 = SEGMENT.color_from_palette(i, true, PALETTE_SOLID_WRAP, 0);
    if (theatre) {
      if ((i % width) == SEGENV.aux0) col = color1;
    } else {
      int pos = (i % (width<<1));
      if ((pos < SEGENV.aux0-width) || ((pos >= SEGENV.aux0) && (pos < SEGENV.aux0+width))) col = color1;
    }
    SEGMENT.setPixelColor(i,col);
  }

  if (it != SEGENV.step) {
    SEGENV.aux0 = (SEGENV.aux0 +1) % (theatre ? width : (width<<1));
    SEGENV.step = it;
  }
  return FRAMETIME;
}

static uint16_t simple_roll() {
  auto roll = GetLastRollForSegment();
  if (roll.state != pixels::RollState::ON_FACE) {
    SEGMENT.fill(0);
  } else {
    uint16_t num_segments = float(roll.current_face + 1) / 20.0 * SEGLEN;
    for (int i = 0; i <= num_segments; i++) {
      SEGMENT.setPixelColor(i, SEGCOLOR(0));
    }
    for (int i = num_segments; i < SEGLEN; i++) {
      SEGMENT.setPixelColor(i, SEGCOLOR(1));
    }
  }
  return FRAMETIME;
}
// See https://kno.wled.ge/interfaces/json-api/#effect-metadata
// Name - DieSimple
// Parameters -
//   * Selected Die (custom1)
// Colors - Uses color1 and color2
// Palette - Not used
// Flags - Effect is optimized for use on 1D LED strips.
// Defaults - Selected Die set to 0xFF (USER_ANY_DIE)
static const char _data_FX_MODE_SIMPLE_DIE[] PROGMEM =
    "DieSimple@,,Selected Die;!,!;;1;c1=255";

static uint16_t pulse_roll() {
  auto roll = GetLastRollForSegment();
  if (roll.state != pixels::RollState::ON_FACE) {
    return mode_breath();
  } else {
    uint16_t ret = mode_blends();
    uint16_t num_segments = float(roll.current_face + 1) / 20.0 * SEGLEN;
    for (int i = num_segments; i < SEGLEN; i++) {
      SEGMENT.setPixelColor(i, SEGCOLOR(1));
    }
    return ret;
  }
}
static const char _data_FX_MODE_PULSE_DIE[] PROGMEM =
    "DiePulse@!,!,Selected Die;!,!;!;1;sx=24,pal=50,c1=255";

static uint16_t check_roll() {
  auto roll = GetLastRollForSegment();
  if (roll.state != pixels::RollState::ON_FACE) {
    return running_copy(SEGCOLOR(0), SEGCOLOR(2));
  } else {
    if (roll.current_face + 1 >= SEGMENT.custom2) {
      return mode_glitter();
    } else {
      return mode_gravcenter();
    }
  }
}
static const char _data_FX_MODE_CHECK_DIE[] PROGMEM =
    "DieCheck@!,!,Selected Die,Target Roll;1,2,3;!;1;pal=0,ix=128,m12=2,si=0,c1=255,c2=10";
