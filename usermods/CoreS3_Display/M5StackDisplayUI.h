#pragma once

#include <stdint.h>

// Common M5Stack controller UI geometry.
//
// The 320 x 240 hit areas below are the hardware-verified CoreS3
// baseline. Another M5Stack profile may reuse them only after
// real-hardware validation.

// ===========================================================
// Shared 320 x 240 UI touch rectangles
//
// These hit areas are part of the common M5Stack controller UI.
// Their numeric values remain the hardware-verified CoreS3 baseline
// and can be reused by another profile only after hardware validation.
// ===========================================================

struct M5StackTouchRect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

static constexpr M5StackTouchRect M5STACK_TOUCH_POWER              = {   8,   8,  44, 44 };
// MAIN header: Recovery AP hold applies only to the network-status area.
// The right-side battery indicator is display-only and intentionally has
// no touch target.
static constexpr M5StackTouchRect M5STACK_TOUCH_WIFI_RECOVERY      = {  60,  28, 178, 28 };
static constexpr M5StackTouchRect M5STACK_TOUCH_BRIGHTNESS_DOWN    = {  16,  82,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_BRIGHTNESS_UP      = { 240,  82,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_EFFECT_PREV        = {  16, 138,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_EFFECT_DETAIL      = {  88, 138, 144, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_EFFECT_NEXT        = { 240, 138,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_COLOR_OPEN         = {   8, 180, 152, 60 };
static constexpr M5StackTouchRect M5STACK_TOUCH_PRESET_OPEN        = { 160, 180, 152, 60 };
static constexpr M5StackTouchRect M5STACK_TOUCH_BACK               = { 268,   8,  44, 44 };
static constexpr M5StackTouchRect M5STACK_TOUCH_COLOR_SLOT_1       = {  24,  62,  88, 54 };
static constexpr M5StackTouchRect M5STACK_TOUCH_COLOR_SLOT_2       = { 116,  62,  88, 54 };
static constexpr M5StackTouchRect M5STACK_TOUCH_COLOR_SLOT_3       = { 208,  62,  88, 54 };
static constexpr M5StackTouchRect M5STACK_TOUCH_HUE_DOWN           = {  16, 151,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_HUE_UP             = { 240, 151,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_SATURATION_DOWN    = {  16, 204,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_SATURATION_UP      = { 240, 204,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_SPEED_DOWN         = {  16,  82,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_SPEED_UP           = { 240,  82,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_INTENSITY_DOWN     = {  16, 140,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_INTENSITY_UP       = { 240, 140,  64, 34 };
static constexpr M5StackTouchRect M5STACK_TOUCH_PALETTE_PREV       = {   8, 188,  80, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_PALETTE_NEXT       = { 232, 188,  80, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_PRESET_PREV        = {   8, 188,  80, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_PRESET_NEXT        = { 232, 188,  80, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_PRESET_MANAGE      = {  88, 188, 144, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_PRESET_SAVE_NEW    = {  48,  60, 224, 42 };
static constexpr M5StackTouchRect M5STACK_TOUCH_PRESET_SAVE_HOLD   = {  48, 170, 224, 66 };
static constexpr M5StackTouchRect M5STACK_TOUCH_OVERWRITE_OPEN     = {  48, 102, 224, 42 };
static constexpr M5StackTouchRect M5STACK_TOUCH_OVERWRITE_PREV     = {   8, 116,  80, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_OVERWRITE_NEXT     = { 232, 116,  80, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_OVERWRITE_HOLD     = {  48, 184, 224, 56 };
static constexpr M5StackTouchRect M5STACK_TOUCH_DELETE_OPEN        = {  48, 144, 224, 42 };
static constexpr M5StackTouchRect M5STACK_TOUCH_DELETE_PREV        = {   8, 116,  80, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_DELETE_NEXT        = { 232, 116,  80, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_DELETE_HOLD        = {  48, 184, 224, 56 };
static constexpr M5StackTouchRect M5STACK_TOUCH_BOOT_OPEN          = {  48, 186, 224, 48 };
static constexpr M5StackTouchRect M5STACK_TOUCH_BOOT_PREV          = {   8, 116,  80, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_BOOT_NEXT          = { 232, 116,  80, 52 };
static constexpr M5StackTouchRect M5STACK_TOUCH_BOOT_HOLD          = {  48, 184, 224, 56 };
