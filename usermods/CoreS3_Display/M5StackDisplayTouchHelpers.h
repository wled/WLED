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

inline bool isPowerButtonTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_POWER ); }
inline bool isBrightnessDownTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BRIGHTNESS_DOWN ); }
inline bool isBrightnessUpTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BRIGHTNESS_UP ); }
inline bool isEffectPrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_EFFECT_PREV ); }
inline bool isEffectDetailTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_EFFECT_DETAIL ); }
inline bool isEffectNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_EFFECT_NEXT ); }
inline bool isColorButtonTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_COLOR_OPEN ); }
inline bool isPresetOpenButtonTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_OPEN ); }
inline bool isBackButtonTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BACK ); }
inline bool isHueDownTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_HUE_DOWN ); }
inline bool isHueUpTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_HUE_UP ); }
inline bool isSaturationDownTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_SATURATION_DOWN ); }
inline bool isSaturationUpTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_SATURATION_UP ); }
inline bool isSpeedDownTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_SPEED_DOWN ); }
inline bool isSpeedUpTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_SPEED_UP ); }
inline bool isIntensityDownTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_INTENSITY_DOWN ); }
inline bool isIntensityUpTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_INTENSITY_UP ); }
inline bool isPalettePrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PALETTE_PREV ); }
inline bool isPaletteNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PALETTE_NEXT ); }
inline bool isPresetPrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_PREV ); }
inline bool isPresetNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_NEXT ); }
inline bool isPresetManageTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_MANAGE ); }
inline bool isPresetSaveNewTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_SAVE_NEW ); }
inline bool isPresetSaveHoldTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_PRESET_SAVE_HOLD ); }
inline bool isPresetOverwriteOpenTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_OVERWRITE_OPEN ); }
inline bool isPresetOverwritePrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_OVERWRITE_PREV ); }
inline bool isPresetOverwriteNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_OVERWRITE_NEXT ); }
inline bool isPresetOverwriteHoldTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_OVERWRITE_HOLD ); }
inline bool isPresetDeleteOpenTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_DELETE_OPEN ); }
inline bool isPresetDeletePrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_DELETE_PREV ); }
inline bool isPresetDeleteNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_DELETE_NEXT ); }
inline bool isPresetDeleteHoldTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_DELETE_HOLD ); }
inline bool isPresetBootOpenTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BOOT_OPEN ); }
inline bool isPresetBootPrevTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BOOT_PREV ); }
inline bool isPresetBootNextTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BOOT_NEXT ); }
inline bool isPresetBootHoldTouched( int16_t x, int16_t y ) { return pointInsideRect( x, y, M5STACK_TOUCH_BOOT_HOLD ); }

inline bool isTouchTargetPair( M5StackTouchTarget target, M5StackTouchTarget firstTarget, M5StackTouchTarget secondTarget ) {
  return target == firstTarget || target == secondTarget;
}

} // namespace M5StackDisplayTouchHelpers
