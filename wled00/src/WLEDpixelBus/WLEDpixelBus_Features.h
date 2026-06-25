/*-------------------------------------------------------------------------

WLEDpixelBus - special features

written by Damian Schneider @dedehai 2026

prefix data (TM1914), suffix data (SM16825) and brigthness to LED hardware current mapping (TM1814, TM1815 and APA102)

-------------------------------------------------------------------------*/

#pragma once

#include "../../const.h"

// SM16825E 32-bit per-frame suffix (appended once after all pixel data):
//   bits 31..7  (25 bits): current gain OUT R,G,B,W,Y — 5 bits each, 0x1F = 310mA, 0x00 = 10.2mA, step ~10.1mA
//   bits  6..5  ( 2 bits): standby enable — 0b00 = normal op (0b10 = standby)
//   bits  4..0  ( 5 bits): reserved — all 1 recommended
// static constexpr uint8_t SM16825_SUFFIX[4] = { 0xFF, 0xFF, 0xFF, 0x9F }; // set max current (not really safe)
static constexpr uint8_t SM16825_SUFFIX[4] = { 0x08, 0x42, 0x10, 0x9F }; // 20.3mA for all channels as a safe default - TODO: make this configurable or use a safe default? also add standby mode support if off?

// mapBrightnessToCurrentStep() is used by BusDigital for current-based dimming of chips
// with discrete current levels (e.g. TM1814/TM1815).

#include <stdint.h>

// TM1914 mode-setting prefix: 6 bytes (3 data + 3 inverted).
// DIN/FDIN auto-switch mode (0xFF). Written once after begin(); never changes at runtime.
// Other modes: DIN-only 0xFA, FDIN-only 0xF5.
//static constexpr uint8_t TM1914_PREFIX[6] = { 0xFF, 0xFF, 0xFA, 0x00, 0x00, 0x05 }; DIN only
//static constexpr uint8_t TM1914_PREFIX[6] = { 0xFF, 0xFF, 0xF5, 0x00, 0x00, 0x0A }; FDIN only
static constexpr uint8_t TM1914_PREFIX[6] = { 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00 };


namespace WLEDpixelBus {
/**
 * Map a WLED brightness value (0..255) to a hardware current step and a residual
 * color scale, maximising effective resolution for chips with discrete current levels.
 *
 * The strategy: pick the smallest current step whose brightness is >= the target,
 * then scale pixel colors down to close the gap. Current handles coarse dimming;
 * color scale provides sub-step interpolation without wasting hardware range.
 *
 * All arithmetic is integer (Q16.8 fixed-point)
 *
 * @param brightness  Target brightness 0..255.
 * @param numSteps    Number of discrete current levels the chip supports (e.g. 64).
 * @param minBri      Brightness equivalent of the minimum current step (e.g. 44 for
 *                    TM1814: floor(6.5/38 * 255)). Below this floor, step 0 is used
 *                    and color scaling brings brightness down further.
 * @param stepOut     Output: current step to program into the chip (0..numSteps-1).
 * @param scaleOut    Output: color scale to apply to pixel data (0..255; 255 = no change).
 */
inline void mapBrightnessToCurrentStep(uint8_t brightness, uint8_t numSteps, uint8_t minBri, uint8_t& stepOut, uint8_t& scaleOut) {
  if (brightness == 0 || numSteps == 0) {
    stepOut = 0; scaleOut = 0;
    return;
  }

  const uint8_t maxStep = numSteps - 1;
  // Q16.8 step size: (255 - minBri) / maxStep
  const uint32_t range   = 255 - minBri;
  const uint32_t stepFP  = (range << 8) / maxStep;  // Q16.8

  if (brightness <= minBri) {
    // Below minimum current floor: use step 0, scale colors down proportionally.
    stepOut  = 0;
    scaleOut = ((uint16_t)brightness * 255) / minBri;
    return;
  }

  // Compute ceiling step: smallest step whose brightness >= target.
  const uint32_t diffFP = (uint32_t)(brightness - minBri) << 8;  // Q16.8
  uint32_t step = (diffFP + stepFP - 1) / stepFP;                 // ceiling division
  if (step > maxStep) step = maxStep;

  // Actual brightness at this step (integer floor, guaranteed >= brightness).
  const uint32_t curBri = minBri + (step * range) / maxStep;

  stepOut  = (uint8_t)step;
  scaleOut = (uint8_t)(((uint16_t)brightness * 255) / curBri);  // always <= 255
}

} // namespace WLEDpixelBus
