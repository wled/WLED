#include "wled.h"

#ifndef WLED_DISABLE_INFRARED
#include "ir_codes.h"

/*
 * Infrared sensor support for several generic RGB remotes and custom JSON remote
 */

IRrecv* irrecv;
decode_results results;
unsigned long irCheckedTime = 0;
uint32_t lastValidCode = 0;
byte lastRepeatableAction = ACTION_NONE;
uint8_t lastRepeatableValue = 0;
uint16_t irTimesRepeated = 0;
uint8_t lastIR6ColourIdx = 0;


// brightnessSteps: a static array of brightness levels following a geometric
// progression.  Can be generated from the following Python, adjusting the
// arbitrary 4.5 value to taste:
//
// def values(level):
//     while level >= 5:
//         yield int(level)
//         level -= level / 4.5
// result = [v for v in reversed(list(values(255)))]
// print("%d values: %s" % (len(result), result))
//
// It would be hard to maintain repeatable steps if calculating this on the fly.
const uint8_t brightnessSteps[] = {
  5, 7, 9, 12, 16, 20, 26, 34, 43, 56, 72, 93, 119, 154, 198, 255
};
const size_t numBrightnessSteps = sizeof(brightnessSteps) / sizeof(uint8_t);

// increment `bri` to the next `brightnessSteps` value
static void incBrightness()
{
  // dumb incremental search is efficient enough for so few items
  for (unsigned index = 0; index < numBrightnessSteps; ++index)
  {
    if (brightnessSteps[index] > bri)
    {
      bri = brightnessSteps[index];
      lastRepeatableAction = ACTION_BRIGHT_UP;
      break;
    }
  }
}

// decrement `bri` to the next `brightnessSteps` value
static void decBrightness()
{
  // dumb incremental search is efficient enough for so few items
  for (int index = numBrightnessSteps - 1; index >= 0; --index)
  {
    if (brightnessSteps[index] < bri)
    {
      bri = brightnessSteps[index];
      lastRepeatableAction = ACTION_BRIGHT_DOWN;
      break;
    }
  }
}

static void presetFallback(uint8_t presetID, uint8_t effectID, uint8_t paletteID)
{
  applyPresetWithFallback(presetID, CALL_MODE_BUTTON_PRESET, effectID, paletteID);
}

static byte relativeChange(byte property, int8_t amount, byte lowerBoundary = 0, byte higherBoundary = 0xFF)
{
  int16_t new_val = (int16_t) property + amount;
  if (lowerBoundary >= higherBoundary) return property;
  if (new_val > higherBoundary) new_val = higherBoundary;
  if (new_val < lowerBoundary)  new_val = lowerBoundary;
  return (byte)constrain(new_val, 0, 255);
}

static void changeEffect(uint8_t fx)
{
  if (irApplyToAllSelected) {
    for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
      Segment& seg = strip.getSegment(i);
      if (!seg.isActive() || !seg.isSelected()) continue;
      seg.setMode(fx);
    }
    setValuesFromFirstSelectedSeg();
  } else {
    strip.getSegment(strip.getMainSegmentId()).setMode(fx);
    setValuesFromMainSeg();
  }
  stateChanged = true;
}

static void changePalette(uint8_t pal)
{
  if (irApplyToAllSelected) {
    for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
      Segment& seg = strip.getSegment(i);
      if (!seg.isActive() || !seg.isSelected()) continue;
      seg.setPalette(pal);
    }
    setValuesFromFirstSelectedSeg();
  } else {
    strip.getMainSegment().palette = pal;
    setValuesFromMainSeg();
  }
  stateChanged = true;
}

static void changeEffectSpeed(int8_t amount)
{
  if (effectCurrent != 0) {
    int16_t new_val = (int16_t) effectSpeed + amount;
    effectSpeed = (byte)constrain(new_val,0,255);
    if (irApplyToAllSelected) {
      for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
        Segment& seg = strip.getSegment(i);
        if (!seg.isActive() || !seg.isSelected()) continue;
        seg.speed = effectSpeed;
      }
      setValuesFromFirstSelectedSeg();
    } else {
      strip.getMainSegment().speed = effectSpeed;
      setValuesFromMainSeg();
    }
  } else { // if Effect == "solid Color", change the hue of the primary color
    Segment& sseg = irApplyToAllSelected ? strip.getFirstSelectedSeg() : strip.getMainSegment();
    CRGB fastled_col = CRGB(sseg.colors[0]);
    CHSV prim_hsv = rgb2hsv(fastled_col);
    int16_t new_val = (int16_t)prim_hsv.h + amount;
    if (new_val > 255) new_val -= 255;  // roll-over if  bigger than 255
    if (new_val < 0) new_val += 255;    // roll-over if smaller than 0
    prim_hsv.h = (byte)new_val;
    hsv2rgb_rainbow(prim_hsv, fastled_col);
    if (irApplyToAllSelected) {
      for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
        Segment& seg = strip.getSegment(i);
        if (!seg.isActive() || !seg.isSelected()) continue;
        seg.colors[0] = RGBW32(fastled_col.red, fastled_col.green, fastled_col.blue, W(sseg.colors[0]));
      }
      setValuesFromFirstSelectedSeg();
    } else {
      strip.getMainSegment().colors[0] = RGBW32(fastled_col.red, fastled_col.green, fastled_col.blue, W(sseg.colors[0]));
      setValuesFromMainSeg();
    }
  }
  stateChanged = true;

  if(amount > 0) lastRepeatableAction = ACTION_SPEED_UP;
  if(amount < 0) lastRepeatableAction = ACTION_SPEED_DOWN;
  lastRepeatableValue = amount;
}

static void changeEffectIntensity(int8_t amount)
{
  if (effectCurrent != 0) {
    int16_t new_val = (int16_t) effectIntensity + amount;
    effectIntensity = (byte)constrain(new_val,0,255);
    if (irApplyToAllSelected) {
      for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
        Segment& seg = strip.getSegment(i);
        if (!seg.isActive() || !seg.isSelected()) continue;
        seg.intensity = effectIntensity;
      }
      setValuesFromFirstSelectedSeg();
    } else {
      strip.getMainSegment().speed = effectIntensity;
      setValuesFromMainSeg();
    }
  } else { // if Effect == "solid Color", change the saturation of the primary color
    Segment& sseg = irApplyToAllSelected ? strip.getFirstSelectedSeg() : strip.getMainSegment();
    CRGB fastled_col = CRGB(sseg.colors[0]);
    CHSV prim_hsv = rgb2hsv(fastled_col);
    int16_t new_val = (int16_t) prim_hsv.s + amount;
    prim_hsv.s = (byte)constrain(new_val,0,255);  // constrain to 0-255
    hsv2rgb_rainbow(prim_hsv, fastled_col);
    if (irApplyToAllSelected) {
      for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
        Segment& seg = strip.getSegment(i);
        if (!seg.isActive() || !seg.isSelected()) continue;
        seg.colors[0] = RGBW32(fastled_col.red, fastled_col.green, fastled_col.blue, W(sseg.colors[0]));
      }
      setValuesFromFirstSelectedSeg();
    } else {
      strip.getMainSegment().colors[0] = RGBW32(fastled_col.red, fastled_col.green, fastled_col.blue, W(sseg.colors[0]));
      setValuesFromMainSeg();
    }
  }
  stateChanged = true;

  if(amount > 0) lastRepeatableAction = ACTION_INTENSITY_UP;
  if(amount < 0) lastRepeatableAction = ACTION_INTENSITY_DOWN;
  lastRepeatableValue = amount;
}

static void changeColor(uint32_t c, int16_t cct=-1)
{
  if (irApplyToAllSelected) {
    // main segment may not be selected!
    for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
      Segment& seg = strip.getSegment(i);
      if (!seg.isActive() || !seg.isSelected()) continue;
      byte capabilities = seg.getLightCapabilities();
      uint32_t mask = 0;
      bool isRGB   = GET_BIT(capabilities, 0);  // is segment RGB capable
      bool hasW    = GET_BIT(capabilities, 1);  // do we have white/CCT channel
      bool isCCT   = GET_BIT(capabilities, 2);  // is segment CCT capable
      bool wSlider = GET_BIT(capabilities, 3);  // is white auto calculated (white slider NOT shown in UI)
      if (isRGB) mask |= 0x00FFFFFF; // RGB
      if (hasW)  mask |= 0xFF000000; // white
      if (hasW && !wSlider && (c & 0xFF000000)) { // segment has white channel & white channel is auto calculated & white specified
        seg.setColor(0, c | 0xFFFFFF); // for accurate/brighter mode we fake white (since button may not set white color to 0xFFFFFF)
      } else if (c & mask) seg.setColor(0, c & mask); // only apply if not black
      if (isCCT && cct >= 0) seg.setCCT(cct);
    }
    setValuesFromFirstSelectedSeg();
  } else {
    byte i = strip.getMainSegmentId();
    Segment& seg = strip.getSegment(i);
    byte capabilities = seg.getLightCapabilities();
    uint32_t mask = 0;
    bool isRGB   = GET_BIT(capabilities, 0);  // is segment RGB capable
    bool hasW    = GET_BIT(capabilities, 1);  // do we have white/CCT channel
    bool isCCT   = GET_BIT(capabilities, 2);  // is segment CCT capable
    bool wSlider = GET_BIT(capabilities, 3);  // is white auto calculated (white slider NOT shown in UI)
    if (isRGB) mask |= 0x00FFFFFF; // RGB
    if (hasW)  mask |= 0xFF000000; // white
    if (hasW && !wSlider && (c & 0xFF000000)) { // segment has white channel & white channel is auto calculated & white specified
      seg.setColor(0, c | 0xFFFFFF); // for accurate/brighter mode we fake white (since button may not set white color to 0xFFFFFF)
    } else if (c & mask) seg.setColor(0, c & mask); // only apply if not black
    if (isCCT && cct >= 0) seg.setCCT(cct);
    setValuesFromMainSeg();
  }
  stateChanged = true;
}

static void changeWhite(int8_t amount, int16_t cct=-1)
{
  Segment& seg = irApplyToAllSelected ? strip.getFirstSelectedSeg() : strip.getMainSegment();
  byte r = R(seg.colors[0]);
  byte g = G(seg.colors[0]);
  byte b = B(seg.colors[0]);
  byte w = relativeChange(W(seg.colors[0]), amount, 5);
  changeColor(RGBW32(r, g, b, w), cct);
}

static void decodeIR24(uint32_t code)
{
  switch (code) {
    case IR24_BRIGHTER  : incBrightness();                                         break;
    case IR24_DARKER    : decBrightness();                                         break;
    case IR24_OFF    : if (bri > 0) briLast = bri; bri = 0;                        break;
    case IR24_ON        : bri = briLast;                                           break;
    case IR24_RED       : changeColor(COLOR_RED);                                  break;
    case IR24_REDDISH   : changeColor(COLOR_REDDISH);                              break;
    case IR24_ORANGE    : changeColor(COLOR_ORANGE);                               break;
    case IR24_YELLOWISH : changeColor(COLOR_YELLOWISH);                            break;
    case IR24_YELLOW    : changeColor(COLOR_YELLOW);                               break;
    case IR24_GREEN     : changeColor(COLOR_GREEN);                                break;
    case IR24_GREENISH  : changeColor(COLOR_GREENISH);                             break;
    case IR24_TURQUOISE : changeColor(COLOR_TURQUOISE);                            break;
    case IR24_CYAN      : changeColor(COLOR_CYAN);                                 break;
    case IR24_AQUA      : changeColor(COLOR_AQUA);                                 break;
    case IR24_BLUE      : changeColor(COLOR_BLUE);                                 break;
    case IR24_DEEPBLUE  : changeColor(COLOR_DEEPBLUE);                             break;
    case IR24_PURPLE    : changeColor(COLOR_PURPLE);                               break;
    case IR24_MAGENTA   : changeColor(COLOR_MAGENTA);                              break;
    case IR24_PINK      : changeColor(COLOR_PINK);                                 break;
    case IR24_WHITE     : changeColor(COLOR_WHITE); changeEffect(FX_MODE_STATIC);  break;
    case IR24_FLASH     : presetFallback(1, FX_MODE_COLORTWINKLE, effectPalette);  break;
    case IR24_STROBE    : presetFallback(2, FX_MODE_RAINBOW_CYCLE, effectPalette); break;
    case IR24_FADE      : presetFallback(3, FX_MODE_BREATH, effectPalette);        break;
    case IR24_SMOOTH    : presetFallback(4, FX_MODE_RAINBOW, effectPalette);       break;
    default: return;
  }
  lastValidCode = code;
}

static void decodeIR24OLD(uint32_t code)
{
  switch (code) {
    case IR24_OLD_BRIGHTER  : incBrightness();                                        break;
    case IR24_OLD_DARKER    : decBrightness();                                        break;
    case IR24_OLD_OFF       : if (bri > 0) briLast = bri; bri = 0;                    break;
    case IR24_OLD_ON        : bri = briLast;                                          break;
    case IR24_OLD_RED       : changeColor(COLOR_RED);                                 break;
    case IR24_OLD_REDDISH   : changeColor(COLOR_REDDISH);                             break;
    case IR24_OLD_ORANGE    : changeColor(COLOR_ORANGE);                              break;
    case IR24_OLD_YELLOWISH : changeColor(COLOR_YELLOWISH);                           break;
    case IR24_OLD_YELLOW    : changeColor(COLOR_YELLOW);                              break;
    case IR24_OLD_GREEN     : changeColor(COLOR_GREEN);                               break;
    case IR24_OLD_GREENISH  : changeColor(COLOR_GREENISH);                            break;
    case IR24_OLD_TURQUOISE : changeColor(COLOR_TURQUOISE);                           break;
    case IR24_OLD_CYAN      : changeColor(COLOR_CYAN);                                break;
    case IR24_OLD_AQUA      : changeColor(COLOR_AQUA);                                break;
    case IR24_OLD_BLUE      : changeColor(COLOR_BLUE);                                break;
    case IR24_OLD_DEEPBLUE  : changeColor(COLOR_DEEPBLUE);                            break;
    case IR24_OLD_PURPLE    : changeColor(COLOR_PURPLE);                              break;
    case IR24_OLD_MAGENTA   : changeColor(COLOR_MAGENTA);                             break;
    case IR24_OLD_PINK      : changeColor(COLOR_PINK);                                break;
    case IR24_OLD_WHITE     : changeColor(COLOR_WHITE); changeEffect(FX_MODE_STATIC); break;
    case IR24_OLD_FLASH     : presetFallback(1, FX_MODE_COLORTWINKLE, 0);             break;
    case IR24_OLD_STROBE    : presetFallback(2, FX_MODE_RAINBOW_CYCLE, 0);            break;
    case IR24_OLD_FADE      : presetFallback(3, FX_MODE_BREATH, 0);                   break;
    case IR24_OLD_SMOOTH    : presetFallback(4, FX_MODE_RAINBOW, 0);                  break;
    default: return;
  }
  lastValidCode = code;
}

static void decodeIR24CT(uint32_t code)
{
  switch (code) {
    case IR24_CT_BRIGHTER   : incBrightness();                     break;
    case IR24_CT_DARKER     : decBrightness();                     break;
    case IR24_CT_OFF        : if (bri > 0) briLast = bri; bri = 0; break;
    case IR24_CT_ON         : bri = briLast;                       break;
    case IR24_CT_RED        : changeColor(COLOR_RED);              break;
    case IR24_CT_REDDISH    : changeColor(COLOR_REDDISH);          break;
    case IR24_CT_ORANGE     : changeColor(COLOR_ORANGE);           break;
    case IR24_CT_YELLOWISH  : changeColor(COLOR_YELLOWISH);        break;
    case IR24_CT_YELLOW     : changeColor(COLOR_YELLOW);           break;
    case IR24_CT_GREEN      : changeColor(COLOR_GREEN);            break;
    case IR24_CT_GREENISH   : changeColor(COLOR_GREENISH);         break;
    case IR24_CT_TURQUOISE  : changeColor(COLOR_TURQUOISE);        break;
    case IR24_CT_CYAN       : changeColor(COLOR_CYAN);             break;
    case IR24_CT_AQUA       : changeColor(COLOR_AQUA);             break;
    case IR24_CT_BLUE       : changeColor(COLOR_BLUE);             break;
    case IR24_CT_DEEPBLUE   : changeColor(COLOR_DEEPBLUE);         break;
    case IR24_CT_PURPLE     : changeColor(COLOR_PURPLE);           break;
    case IR24_CT_MAGENTA    : changeColor(COLOR_MAGENTA);          break;
    case IR24_CT_PINK       : changeColor(COLOR_PINK);             break;
    case IR24_CT_COLDWHITE  : changeColor(COLOR_COLDWHITE2,                                             255); changeEffect(FX_MODE_STATIC); break;
    case IR24_CT_WARMWHITE  : changeColor(COLOR_WARMWHITE2,                                               0); changeEffect(FX_MODE_STATIC); break;
    case IR24_CT_CTPLUS     : changeColor(COLOR_COLDWHITE, strip.getSegment(strip.getMainSegmentId()).cct+1); changeEffect(FX_MODE_STATIC); break;
    case IR24_CT_CTMINUS    : changeColor(COLOR_WARMWHITE, strip.getSegment(strip.getMainSegmentId()).cct-1); changeEffect(FX_MODE_STATIC); break;
    case IR24_CT_MEMORY     : changeColor(COLOR_NEUTRALWHITE,                                           127); changeEffect(FX_MODE_STATIC); break;
    default: return;
  }
  lastValidCode = code;
}

static void decodeIR40(uint32_t code)
{
  Segment& seg = irApplyToAllSelected ? strip.getFirstSelectedSeg() : strip.getMainSegment();
  byte r = R(seg.colors[0]);
  byte g = G(seg.colors[0]);
  byte b = B(seg.colors[0]);
  byte w = W(seg.colors[0]);
  switch (code) {
    case IR40_BPLUS        : incBrightness();                            break;
    case IR40_BMINUS       : decBrightness();                            break;
    case IR40_OFF          : if (bri > 0) briLast = bri; bri = 0;        break;
    case IR40_ON           : bri = briLast;                              break;
    case IR40_RED          : changeColor(COLOR_RED);                     break;
    case IR40_REDDISH      : changeColor(COLOR_REDDISH);                 break;
    case IR40_ORANGE       : changeColor(COLOR_ORANGE);                  break;
    case IR40_YELLOWISH    : changeColor(COLOR_YELLOWISH);               break;
    case IR40_YELLOW       : changeColor(COLOR_YELLOW);                  break;
    case IR40_GREEN        : changeColor(COLOR_GREEN);                   break;
    case IR40_GREENISH     : changeColor(COLOR_GREENISH);                break;
    case IR40_TURQUOISE    : changeColor(COLOR_TURQUOISE);               break;
    case IR40_CYAN         : changeColor(COLOR_CYAN);                    break;
    case IR40_AQUA         : changeColor(COLOR_AQUA);                    break;
    case IR40_BLUE         : changeColor(COLOR_BLUE);                    break;
    case IR40_DEEPBLUE     : changeColor(COLOR_DEEPBLUE);                break;
    case IR40_PURPLE       : changeColor(COLOR_PURPLE);                  break;
    case IR40_MAGENTA      : changeColor(COLOR_MAGENTA);                 break;
    case IR40_PINK         : changeColor(COLOR_PINK);                    break;
    case IR40_WARMWHITE2   : changeColor(COLOR_WARMWHITE2,     0); changeEffect(FX_MODE_STATIC); break;
    case IR40_WARMWHITE    : changeColor(COLOR_WARMWHITE,     63); changeEffect(FX_MODE_STATIC); break;
    case IR40_WHITE        : changeColor(COLOR_NEUTRALWHITE, 127); changeEffect(FX_MODE_STATIC); break;
    case IR40_COLDWHITE    : changeColor(COLOR_COLDWHITE,    191); changeEffect(FX_MODE_STATIC); break;
    case IR40_COLDWHITE2   : changeColor(COLOR_COLDWHITE2,   255); changeEffect(FX_MODE_STATIC); break;
    case IR40_WPLUS        : changeWhite(10);                            break;
    case IR40_WMINUS       : changeWhite(-10);                           break;
    case IR40_WOFF         : if (w) whiteLast = w; changeColor(RGBW32(r, g, b, 0));              break;
    case IR40_WON          : changeColor(RGBW32(r, g, b, whiteLast));    break;
    case IR40_W25          : bri = 63;                                   break;
    case IR40_W50          : bri = 127;                                  break;
    case IR40_W75          : bri = 191;                                  break;
    case IR40_W100         : bri = 255;                                  break;
    case IR40_QUICK        : changeEffectSpeed( 16);                     break;
    case IR40_SLOW         : changeEffectSpeed(-16);                     break;
    case IR40_JUMP7        : changeEffectIntensity( 16);                 break;
    case IR40_AUTO         : changeEffectIntensity(-16);                 break;
    case IR40_JUMP3        : presetFallback(1, FX_MODE_STATIC,       0); break;
    case IR40_FADE3        : presetFallback(2, FX_MODE_BREATH,       0); break;
    case IR40_FADE7        : presetFallback(3, FX_MODE_FIRE_FLICKER, 0); break;
    case IR40_FLASH        : presetFallback(4, FX_MODE_RAINBOW,      0); break;
    default: return;
  }
  lastValidCode = code;
}

static void decodeIR44(uint32_t code)
{
  switch (code) {
    case IR44_BPLUS       : incBrightness();                             break;
    case IR44_BMINUS      : decBrightness();                             break;
    case IR44_OFF         : if (bri > 0) briLast = bri; bri = 0;         break;
    case IR44_ON          : bri = briLast;                               break;
    case IR44_RED         : changeColor(COLOR_RED);                      break;
    case IR44_REDDISH     : changeColor(COLOR_REDDISH);                  break;
    case IR44_ORANGE      : changeColor(COLOR_ORANGE);                   break;
    case IR44_YELLOWISH   : changeColor(COLOR_YELLOWISH);                break;
    case IR44_YELLOW      : changeColor(COLOR_YELLOW);                   break;
    case IR44_GREEN       : changeColor(COLOR_GREEN);                    break;
    case IR44_GREENISH    : changeColor(COLOR_GREENISH);                 break;
    case IR44_TURQUOISE   : changeColor(COLOR_TURQUOISE);                break;
    case IR44_CYAN        : changeColor(COLOR_CYAN);                     break;
    case IR44_AQUA        : changeColor(COLOR_AQUA);                     break;
    case IR44_BLUE        : changeColor(COLOR_BLUE);                     break;
    case IR44_DEEPBLUE    : changeColor(COLOR_DEEPBLUE);                 break;
    case IR44_PURPLE      : changeColor(COLOR_PURPLE);                   break;
    case IR44_MAGENTA     : changeColor(COLOR_MAGENTA);                  break;
    case IR44_PINK        : changeColor(COLOR_PINK);                     break;
    case IR44_WHITE       : changeColor(COLOR_NEUTRALWHITE, 127); changeEffect(FX_MODE_STATIC);  break;
    case IR44_WARMWHITE2  : changeColor(COLOR_WARMWHITE2,     0); changeEffect(FX_MODE_STATIC);  break;
    case IR44_WARMWHITE   : changeColor(COLOR_WARMWHITE,     63); changeEffect(FX_MODE_STATIC);  break;
    case IR44_COLDWHITE   : changeColor(COLOR_COLDWHITE,    191); changeEffect(FX_MODE_STATIC);  break;
    case IR44_COLDWHITE2  : changeColor(COLOR_COLDWHITE2,   255); changeEffect(FX_MODE_STATIC);  break;
    case IR44_REDPLUS     : changeEffect(relativeChange(effectCurrent,  1, 0, strip.getModeCount() -1));               break;
    case IR44_REDMINUS    : changeEffect(relativeChange(effectCurrent, -1, 0, strip.getModeCount() -1));               break;
    case IR44_GREENPLUS   : changePalette(relativeChange(effectPalette,  1, 0, strip.getPaletteCount() -1)); break;
    case IR44_GREENMINUS  : changePalette(relativeChange(effectPalette, -1, 0, strip.getPaletteCount() -1)); break;
    case IR44_BLUEPLUS    : changeEffectIntensity( 16);                  break;
    case IR44_BLUEMINUS   : changeEffectIntensity(-16);                  break;
    case IR44_QUICK       : changeEffectSpeed( 16);                      break;
    case IR44_SLOW        : changeEffectSpeed(-16);                      break;
    case IR44_DIY1        : presetFallback(1, FX_MODE_STATIC,        0); break;
    case IR44_DIY2        : presetFallback(2, FX_MODE_BREATH,        0); break;
    case IR44_DIY3        : presetFallback(3, FX_MODE_FIRE_FLICKER,  0); break;
    case IR44_DIY4        : presetFallback(4, FX_MODE_RAINBOW,       0); break;
    case IR44_DIY5        : presetFallback(5, FX_MODE_METEOR, 0);        break;
    case IR44_DIY6        : presetFallback(6, FX_MODE_RAIN,          0); break;
    case IR44_AUTO        : changeEffect(FX_MODE_STATIC);                break;
    case IR44_FLASH       : changeEffect(FX_MODE_PALETTE);               break;
    case IR44_JUMP3       : bri = 63;                                    break;
    case IR44_JUMP7       : bri = 127;                                   break;
    case IR44_FADE3       : bri = 191;                                   break;
    case IR44_FADE7       : bri = 255;                                   break;
    default: return;
  }
  lastValidCode = code;
}

static void decodeIR21(uint32_t code)
{
    switch (code) {
      case IR21_BRIGHTER:  incBrightness();                                        break;
      case IR21_DARKER:    decBrightness();                                        break;
      case IR21_OFF:       if (bri > 0) briLast = bri; bri = 0;                    break;
      case IR21_ON:        bri = briLast;                                          break;
      case IR21_RED:       changeColor(COLOR_RED);                                 break;
      case IR21_REDDISH:   changeColor(COLOR_REDDISH);                             break;
      case IR21_ORANGE:    changeColor(COLOR_ORANGE);                              break;
      case IR21_YELLOWISH: changeColor(COLOR_YELLOWISH);                           break;
      case IR21_GREEN:     changeColor(COLOR_GREEN);                               break;
      case IR21_GREENISH:  changeColor(COLOR_GREENISH);                            break;
      case IR21_TURQUOISE: changeColor(COLOR_TURQUOISE);                           break;
      case IR21_CYAN:      changeColor(COLOR_CYAN);                                break;
      case IR21_BLUE:      changeColor(COLOR_BLUE);                                break;
      case IR21_DEEPBLUE:  changeColor(COLOR_DEEPBLUE);                            break;
      case IR21_PURPLE:    changeColor(COLOR_PURPLE);                              break;
      case IR21_PINK:      changeColor(COLOR_PINK);                                break;
      case IR21_WHITE:     changeColor(COLOR_WHITE); changeEffect(FX_MODE_STATIC); break;
      case IR21_FLASH:     presetFallback(1, FX_MODE_COLORTWINKLE,  0);            break;
      case IR21_STROBE:    presetFallback(2, FX_MODE_RAINBOW_CYCLE, 0);            break;
      case IR21_FADE:      presetFallback(3, FX_MODE_BREATH,        0);            break;
      case IR21_SMOOTH:    presetFallback(4, FX_MODE_RAINBOW,       0);            break;
      default: return;
    }
    lastValidCode = code;
}

static void decodeIR6(uint32_t code)
{
  switch (code) {
    case IR6_POWER:        toggleOnOff();                                                    break;
    case IR6_CHANNEL_UP:   incBrightness();                                                  break;
    case IR6_CHANNEL_DOWN: decBrightness();                                                  break;
    case IR6_VOLUME_UP:    changeEffect(relativeChange(effectCurrent, 1, 0, strip.getModeCount() -1)); break;
    case IR6_VOLUME_DOWN:  changePalette(relativeChange(effectPalette, 1, 0, strip.getPaletteCount() -1));
      switch(lastIR6ColourIdx) {
        case 0: changeColor(COLOR_RED);       break;
        case 1: changeColor(COLOR_REDDISH);   break;
        case 2: changeColor(COLOR_ORANGE);    break;
        case 3: changeColor(COLOR_YELLOWISH); break;
        case 4: changeColor(COLOR_GREEN);     break;
        case 5: changeColor(COLOR_GREENISH);  break;
        case 6: changeColor(COLOR_TURQUOISE); break;
        case 7: changeColor(COLOR_CYAN);      break;
        case 8: changeColor(COLOR_BLUE);      break;
        case 9: changeColor(COLOR_DEEPBLUE);  break;
        case 10:changeColor(COLOR_PURPLE);    break;
        case 11:changeColor(COLOR_PINK);      break;
        case 12:changeColor(COLOR_WHITE);     break;
        default:                              break;
      }
      lastIR6ColourIdx++;
      if(lastIR6ColourIdx > 12) lastIR6ColourIdx = 0;
      break;
    case IR6_MUTE: changeEffect(FX_MODE_STATIC); changePalette(0); changeColor(COLOR_WHITE); bri=255; break;
    default: return;
  }
  lastValidCode = code;
}

static void decodeIR9(uint32_t code)
{
  switch (code) {
    case IR9_POWER      : toggleOnOff();                                                    break;
    case IR9_A          : presetFallback(1, FX_MODE_COLORTWINKLE, effectPalette);           break;
    case IR9_B          : presetFallback(2, FX_MODE_RAINBOW_CYCLE, effectPalette);          break;
    case IR9_C          : presetFallback(3, FX_MODE_BREATH, effectPalette);                 break;
    case IR9_UP         : incBrightness();                                                  break;
    case IR9_DOWN       : decBrightness();                                                  break;
    case IR9_LEFT       : changeEffectSpeed(-16);                                           break;
    case IR9_RIGHT      : changeEffectSpeed(16);                                            break;
    case IR9_SELECT     : changeEffect(relativeChange(effectCurrent, 1, 0, strip.getModeCount() -1)); break;
    default: return;
  }
  lastValidCode = code;
}


/*
This allows users to customize IR actions without the need to edit C code and compile.
From the https://github.com/wled-dev/WLED/wiki/Infrared-Control page, download the starter
ir.json file that corresponds to the number of buttons on your remote.
Many of the remotes with the same number of buttons emit the same codes, but will have
different labels or colors. Once you edit the ir.json file, upload it to your controller
using the /edit page.

Each key should be the hex encoded IR code. The "cmd" property should be the HTTP API
or JSON API command to execute on button press. If the command contains a relative change (SI=~16),
it will register as a repeatable command. If the command doesn't contain a "~" but is repeatable, add "rpt" property
set to true. Other properties are ignored but having labels and positions can assist with editing
the json file.

Sample:
{
  "0xFF629D": {"cmd": "T=2", "rpt": true, "label": "Toggle on/off"},  // HTTP command
  "0xFF9867": {"cmd": "A=~16", "label": "Inc brightness"},            // HTTP command with incrementing
  "0xFF38C7": {"cmd": {"bri": 10}, "label": "Dim to 10"},             // JSON command
  "0xFF22DD": {"cmd": "!presetFallback", "PL": 1, "FX": 16, "FP": 6,  // Custom command
               "label": "Preset 1, fallback to Saw - Party if not found"},
}
*/
static void decodeIRJson(uint32_t code)
{
  char objKey[10];
  char fileName[16];
  String cmdStr;
  JsonObject fdo;
  JsonObject jsonCmdObj;

  if (!requestJSONBufferLock(13)) return;

  sprintf_P(objKey, PSTR("\"0x%lX\":"), (unsigned long)code);
  strcpy_P(fileName, PSTR("/ir.json")); // for FS.exists()

  // attempt to read command from ir.json
  // this may fail for two reasons: ir.json does not exist or IR code not found
  // if the IR code is not found readObjectFromFile() will clean() doc JSON document
  // so we can differentiate between the two
  readObjectFromFile(fileName, objKey, pDoc);
  fdo = pDoc->as<JsonObject>();
  lastValidCode = 0;
  if (fdo.isNull()) {
    //the received code does not exist
    if (!WLED_FS.exists(fileName)) errorFlag = ERR_FS_IRLOAD; //warn if IR file itself doesn't exist
    releaseJSONBufferLock();
    return;
  }

  cmdStr = fdo["cmd"].as<String>();
  jsonCmdObj = fdo["cmd"]; //object

  if (jsonCmdObj.isNull())  // we could also use: fdo["cmd"].is<String>()
  {
    if (cmdStr.startsWith("!")) {
      // call limited set of C functions
      if (cmdStr.startsWith(F("!incBri"))) {
        lastValidCode = code;
        incBrightness();
      } else if (cmdStr.startsWith(F("!decBri"))) {
        lastValidCode = code;
        decBrightness();
      } else if (cmdStr.startsWith(F("!presetF"))) { //!presetFallback
        uint8_t p1 = fdo["PL"] | 1;
        uint8_t p2 = fdo["FX"] | hw_random8(strip.getModeCount() -1);
        uint8_t p3 = fdo["FP"] | 0;
        presetFallback(p1, p2, p3);
      }
    } else {
      // HTTP API command
      String apireq = "win"; apireq += '&';                        // reduce flash string usage
      if (cmdStr.indexOf("~") > 0 || fdo["rpt"]) lastValidCode = code; // repeatable action
      if (!cmdStr.startsWith(apireq)) cmdStr = apireq + cmdStr;    // if no "win&" prefix
      if (!irApplyToAllSelected && cmdStr.indexOf(F("SS="))<0) {
        char tmp[10];
        sprintf_P(tmp, PSTR("&SS=%d"), strip.getMainSegmentId());
        cmdStr += tmp;
      }
      fdo.clear();                                                 // clear JSON buffer (it is no longer needed)
      handleSet(nullptr, cmdStr, false);                           // no stateUpdated() call here
    }
  } else {
    // command is JSON object
    if (jsonCmdObj[F("psave")].isNull()) {
      if (irApplyToAllSelected && jsonCmdObj["seg"].is<JsonArray>()) {
        JsonObject seg = jsonCmdObj["seg"][0];                    // take 1st segment from array and use it to apply to all selected segments
        seg.remove("id");                                         // remove segment ID if it exists
        jsonCmdObj["seg"] = seg;                                  // replace array with object
      }
      deserializeState(jsonCmdObj, CALL_MODE_BUTTON_PRESET);      // **will call stateUpdated() with correct CALL_MODE**
    } else {
      uint8_t psave = jsonCmdObj[F("psave")].as<int>();
      char pname[33];
      sprintf_P(pname, PSTR("IR Preset %d"), psave);
      fdo.clear();
      if (psave > 0 && psave < 251) savePreset(psave, pname, fdo);
    }
  }
  releaseJSONBufferLock();
}

static void applyRepeatActions()
{
  if (irEnabled == 8) {
    decodeIRJson(lastValidCode);
    stateUpdated(CALL_MODE_BUTTON_PRESET);
    return;
  } else switch (lastRepeatableAction) {
    case ACTION_BRIGHT_UP :      incBrightness();                            stateUpdated(CALL_MODE_BUTTON); return;
    case ACTION_BRIGHT_DOWN :    decBrightness();                            stateUpdated(CALL_MODE_BUTTON); return;
    case ACTION_SPEED_UP :       changeEffectSpeed(lastRepeatableValue);     stateUpdated(CALL_MODE_BUTTON); return;
    case ACTION_SPEED_DOWN :     changeEffectSpeed(lastRepeatableValue);     stateUpdated(CALL_MODE_BUTTON); return;
    case ACTION_INTENSITY_UP :   changeEffectIntensity(lastRepeatableValue); stateUpdated(CALL_MODE_BUTTON); return;
    case ACTION_INTENSITY_DOWN : changeEffectIntensity(lastRepeatableValue); stateUpdated(CALL_MODE_BUTTON); return;
    default: break;
  }
  if (lastValidCode == IR40_WPLUS) {
    changeWhite(10);
    stateUpdated(CALL_MODE_BUTTON);
  } else if (lastValidCode == IR40_WMINUS) {
    changeWhite(-10);
    stateUpdated(CALL_MODE_BUTTON);
  } else if ((lastValidCode == IR24_ON || lastValidCode == IR40_ON) && irTimesRepeated > 7 ) {
    nightlightActive = true;
    nightlightStartTime = millis();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

static void decodeIR(uint32_t code)
{
  if (code == 0xFFFFFFFF) {
    //repeated code, continue brightness up/down
    irTimesRepeated++;
    applyRepeatActions();
    return;
  }
  lastValidCode = 0; irTimesRepeated = 0;
  lastRepeatableAction = ACTION_NONE;

  if (irEnabled == 8) { // any remote configurable with ir.json file
    decodeIRJson(code);
    stateUpdated(CALL_MODE_BUTTON_PRESET);
    return;
  }
  if (code > 0xFFFFFF) return; //invalid code

  switch (irEnabled) {
    case 1:
      if (code > 0xF80000) decodeIR24OLD(code); // white 24-key remote (old) - it sends 0xFF0000 values
      else                 decodeIR24(code);    // 24-key remote - 0xF70000 to 0xF80000
      break;
    case 2: decodeIR24CT(code); break; // white 24-key remote with CW, WW, CT+ and CT- keys
    case 3: decodeIR40(code);   break; // blue  40-key remote with 25%, 50%, 75% and 100% keys
    case 4: decodeIR44(code);   break; // white 44-key remote with color-up/down keys and DIY1 to 6 keys
    case 5: decodeIR21(code);   break; // white 21-key remote
    case 6: decodeIR6(code);    break; // black 6-key learning remote defaults: "CH" controls brightness,
                                       // "VOL +" controls effect, "VOL -" controls colour/palette, "MUTE"
                                       // sets bright plain white
    case 7: decodeIR9(code);    break;
    //case 8: return; // ir.json file, handled above switch statement
  }

  if (nightlightActive && bri == 0) nightlightActive = false;
  stateUpdated(CALL_MODE_BUTTON); //for notifier, IR is considered a button input
}

void initIR()
{
  if (irEnabled > 0) {
    irrecv = new IRrecv(irPin);
    if (irrecv) irrecv->enableIRIn();
  } else irrecv = nullptr;
}

void deInitIR()
{
  if (irrecv) {
    irrecv->disableIRIn();
    delete irrecv;
  }
  irrecv = nullptr;
}

void handleIR()
{
  unsigned long currentTime = millis();
  unsigned timeDiff = currentTime - irCheckedTime;
  if (timeDiff > 120 && irEnabled > 0 && irrecv) {
    if (strip.isUpdating() && timeDiff < 240) return;  // be nice, but not too nice
    irCheckedTime = currentTime;
    if (irrecv->decode(&results)) {
      if (results.value != 0 && serialCanTX) { // only print results if anything is received ( != 0 )
        Serial.printf_P(PSTR("IR recv: 0x%lX\n"), (unsigned long)results.value);
      }
      decodeIR(results.value);
      irrecv->resume();
    }
  }
}

#endif
