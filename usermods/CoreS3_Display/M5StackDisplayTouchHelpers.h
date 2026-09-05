#pragma once

#include "M5StackDisplayUI.h"
#include "M5StackDisplayTouchState.h"

// Stateless helpers shared by the M5Stack controller Touch layer.
//
// These functions do not own runtime state, access hardware, draw UI,
// or modify WLED state. Their bodies are moved from the hardware-verified
// CoreS3_Display implementation without algorithm changes.
namespace M5StackDisplayTouchHelpers {

inline void resetRepeatTouch( M5StackRepeatTouchState& state ) {
  state.pressStart = 0;
  state.lastRepeat = 0;
  state.longPressActive = false;
}
inline void beginRepeatTouch( M5StackRepeatTouchState& state, unsigned long now ) {
  state.pressStart = now;
  state.lastRepeat = now;
  state.longPressActive = false;
}
inline bool serviceRepeatTouch( M5StackRepeatTouchState& state, unsigned long now, unsigned long longPressMs, unsigned long repeatMs ) {
  if ( !state.longPressActive && now - state.pressStart >= longPressMs ) {
    state.longPressActive = true;
    state.lastRepeat = now;
    return true;
  }

  if ( state.longPressActive && now - state.lastRepeat >= repeatMs ) {
    state.lastRepeat = now;
    return true;
  }

  return false;
}
inline bool pointInsideRect( int16_t px, int16_t py, const M5StackTouchRect& rect ) {
  return ( px >= rect.x && px < rect.x + rect.w && py >= rect.y && py < rect.y + rect.h );
}

bool isPowerButtonTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_POWER ); }
bool isBrightnessDownTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BRIGHTNESS_DOWN ); }
bool isBrightnessUpTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BRIGHTNESS_UP ); }
bool isEffectPrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_EFFECT_PREV ); }
bool isEffectDetailTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_EFFECT_DETAIL ); }
bool isEffectNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_EFFECT_NEXT ); }
bool isColorButtonTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_COLOR_OPEN ); }
bool isPresetOpenButtonTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_OPEN ); }
bool isBackButtonTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BACK ); }
bool isHueDownTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_HUE_DOWN ); }
bool isHueUpTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_HUE_UP ); }
bool isSaturationDownTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_SATURATION_DOWN ); }
bool isSaturationUpTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_SATURATION_UP ); }
bool isSpeedDownTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_SPEED_DOWN ); }
bool isSpeedUpTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_SPEED_UP ); }
bool isIntensityDownTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_INTENSITY_DOWN ); }
bool isIntensityUpTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_INTENSITY_UP ); }
bool isPalettePrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PALETTE_PREV ); }
bool isPaletteNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PALETTE_NEXT ); }
bool isPresetPrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_PREV ); }
bool isPresetNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_NEXT ); }
bool isPresetManageTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_MANAGE ); }
bool isPresetSaveNewTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_SAVE_NEW ); }
bool isPresetSaveHoldTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_SAVE_HOLD ); }
bool isPresetOverwriteOpenTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_OVERWRITE_OPEN ); }
bool isPresetOverwritePrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_OVERWRITE_PREV ); }
bool isPresetOverwriteNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_OVERWRITE_NEXT ); }
bool isPresetOverwriteHoldTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_OVERWRITE_HOLD ); }
bool isPresetDeleteOpenTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_DELETE_OPEN ); }
bool isPresetDeletePrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_DELETE_PREV ); }
bool isPresetDeleteNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_DELETE_NEXT ); }
bool isPresetDeleteHoldTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_DELETE_HOLD ); }
bool isPresetBootOpenTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BOOT_OPEN ); }
bool isPresetBootPrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BOOT_PREV ); }
bool isPresetBootNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BOOT_NEXT ); }
bool isPresetBootHoldTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BOOT_HOLD ); }
inline bool isTouchTargetPair( M5StackTouchTarget target, M5StackTouchTarget firstTarget, M5StackTouchTarget secondTarget ) {
  return target == firstTarget || target == secondTarget;
}
} // namespace M5StackDisplayTouchHelpers
