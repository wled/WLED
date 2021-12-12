#include "wled.h"

/*
 * Infrared sensor support for generic 24/40/44 key RGB remotes
 */

#if defined(WLED_DISABLE_INFRARED)
void handleIR(){}
#else

IRrecv* irrecv;
//change pin in NpbWrapper.h

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
const byte brightnessSteps[] = {
  5, 7, 9, 12, 16, 20, 26, 34, 43, 56, 72, 93, 119, 154, 198, 255
};
const size_t numBrightnessSteps = sizeof(brightnessSteps) / sizeof(uint8_t);

// increment `bri` to the next `brightnessSteps` value
void incBrightness()
{
  // dumb incremental search is efficient enough for so few items
  for (uint8_t index = 0; index < numBrightnessSteps; ++index)
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
void decBrightness()
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

// apply preset or fallback to a effect and palette if it doesn't exist
void presetFallback(uint8_t presetID, uint8_t effectID, uint8_t paletteID) 
{
  byte prevError = errorFlag;
  if (!applyPreset(presetID, CALL_MODE_BUTTON_PRESET)) { 
    effectCurrent = effectID;      
    effectPalette = paletteID;
    errorFlag = prevError; //clear error 12 from non-existent preset
  }
}

//Add what your custom IR codes should trigger here. Guide: https://github.com/Aircoookie/WLED/wiki/Infrared-Control
//IR codes themselves can be defined directly after "case" or in "ir_codes.h"
bool decodeIRCustom(uint32_t code)
{
  switch (code)
  {
    //just examples, feel free to modify or remove
    case IRCUSTOM_ONOFF : toggleOnOff(); break;
    case IRCUSTOM_MACRO1 : applyPreset(1, CALL_MODE_BUTTON_PRESET); break;

    default: return false;
  }
  if (code != IRCUSTOM_MACRO1) colorUpdated(CALL_MODE_BUTTON); //don't update color again if we apply macro, it already does it
  return true;
}

void relativeChange(byte* property, int8_t amount, byte lowerBoundary, byte higherBoundary)
{
  int16_t new_val = (int16_t) *property + amount;
  if (new_val > higherBoundary) new_val = higherBoundary;
  else if (new_val < lowerBoundary) new_val = lowerBoundary;
  *property = (byte)constrain(new_val,0.1,255.1);
}

void changeEffectSpeed(int8_t amount)
{
  if (effectCurrent != 0) {
    int16_t new_val = (int16_t) effectSpeed + amount;
    effectSpeed = (byte)constrain(new_val,0.1,255.1);
  } else {                              // if Effect == "solid Color", change the hue of the primary color
    CRGB fastled_col;
    fastled_col.red =   col[0];
    fastled_col.green = col[1];
    fastled_col.blue =  col[2];
    CHSV prim_hsv = rgb2hsv_approximate(fastled_col);
    int16_t new_val = (int16_t) prim_hsv.h + amount;
    if (new_val > 255) new_val -= 255;  // roll-over if  bigger than 255
    if (new_val < 0) new_val += 255;    // roll-over if smaller than 0
    prim_hsv.h = (byte)new_val;
    hsv2rgb_rainbow(prim_hsv, fastled_col);
    col[0] = fastled_col.red; 
    col[1] = fastled_col.green; 
    col[2] = fastled_col.blue;
  }

  if(amount > 0) lastRepeatableAction = ACTION_SPEED_UP;
  if(amount < 0) lastRepeatableAction = ACTION_SPEED_DOWN;
  lastRepeatableValue = amount;
}

void changeEffectIntensity(int8_t amount)
{
  if (effectCurrent != 0) {
    int16_t new_val = (int16_t) effectIntensity + amount;
    effectIntensity = (byte)constrain(new_val,0.1,255.1);
  } else {                                            // if Effect == "solid Color", change the saturation of the primary color
    CRGB fastled_col;
    fastled_col.red =   col[0];
    fastled_col.green = col[1];
    fastled_col.blue =  col[2];
    CHSV prim_hsv = rgb2hsv_approximate(fastled_col);
    int16_t new_val = (int16_t) prim_hsv.s + amount;
    prim_hsv.s = (byte)constrain(new_val,0.1,255.1);  // constrain to 0-255
    hsv2rgb_rainbow(prim_hsv, fastled_col);
    col[0] = fastled_col.red; 
    col[1] = fastled_col.green; 
    col[2] = fastled_col.blue;
  }

  if(amount > 0) lastRepeatableAction = ACTION_INTENSITY_UP;
  if(amount < 0) lastRepeatableAction = ACTION_INTENSITY_DOWN;
  lastRepeatableValue = amount;
}

void decodeIR(uint32_t code)
{
  if (code == 0xFFFFFFFF) //repeated code, continue brightness up/down
  {
    irTimesRepeated++;
    applyRepeatActions();
    return;
  }
  lastValidCode = 0; irTimesRepeated = 0;
  if (decodeIRCustom(code)) return;
  if (irEnabled == 8) { // any remote configurable with ir.json file
    decodeIRJson(code);
    colorUpdated(CALL_MODE_BUTTON);
    return;
  }
  if (code > 0xFFFFFF) return; //invalid code
  switch (irEnabled) {
    case 1: 
      if (code > 0xF80000) {
        decodeIR24OLD(code);            // white 24-key remote (old) - it sends 0xFF0000 values
      } else {
        decodeIR24(code);               // 24-key remote - 0xF70000 to 0xF80000
      }
      break;
    case 2: decodeIR24CT(code);  break;  // white 24-key remote with CW, WW, CT+ and CT- keys
    case 3: decodeIR40(code);    break;  // blue  40-key remote with 25%, 50%, 75% and 100% keys
    case 4: decodeIR44(code);    break;  // white 44-key remote with color-up/down keys and DIY1 to 6 keys 
    case 5: decodeIR21(code);    break;  // white 21-key remote  
    case 6: decodeIR6(code);     break;  // black 6-key learning remote defaults: "CH" controls brightness,
                                          // "VOL +" controls effect, "VOL -" controls colour/palette, "MUTE" 
                                          // sets bright plain white
    case 7: decodeIR9(code);    break;
    //case 8: return; // ir.json file, handled above switch statement
    default: return;
  }

  if (nightlightActive && bri == 0) nightlightActive = false;
  colorUpdated(CALL_MODE_BUTTON); //for notifier, IR is considered a button input
}

void applyRepeatActions(){
  
    if (lastRepeatableAction == ACTION_BRIGHT_UP)
    { 
      incBrightness(); colorUpdated(CALL_MODE_BUTTON);
    }
    else if (lastRepeatableAction == ACTION_BRIGHT_DOWN )
    {
      decBrightness(); colorUpdated(CALL_MODE_BUTTON);
    }

    if (lastRepeatableAction == ACTION_SPEED_UP)
    { 
      changeEffectSpeed(lastRepeatableValue); colorUpdated(CALL_MODE_BUTTON);
    }
    else if (lastRepeatableAction == ACTION_SPEED_DOWN )
    {
      changeEffectSpeed(lastRepeatableValue); colorUpdated(CALL_MODE_BUTTON);
    }

    if (lastRepeatableAction == ACTION_INTENSITY_UP)
    { 
      changeEffectIntensity(lastRepeatableValue); colorUpdated(CALL_MODE_BUTTON);
    }
    else if (lastRepeatableAction == ACTION_INTENSITY_DOWN )
    {
      changeEffectIntensity(lastRepeatableValue); colorUpdated(CALL_MODE_BUTTON);
    }

    if (lastValidCode == IR40_WPLUS)
    { 
      relativeChangeWhite(10); colorUpdated(CALL_MODE_BUTTON);
    }
    else if (lastValidCode == IR40_WMINUS)
    {
      relativeChangeWhite(-10, 5); colorUpdated(CALL_MODE_BUTTON);
    }
    else if ((lastValidCode == IR24_ON || lastValidCode == IR40_ON) && irTimesRepeated > 7 )
    {
      nightlightActive = true;
      nightlightStartTime = millis();
      colorUpdated(CALL_MODE_BUTTON);
    }
    else if (irEnabled == 8) 
    {
      decodeIRJson(lastValidCode);
    }
}

void decodeIR24(uint32_t code)
{
  switch (code) {
    case IR24_BRIGHTER  : incBrightness();                  break;
    case IR24_DARKER    : decBrightness();                  break;
    case IR24_OFF    : if (bri > 0) briLast = bri; bri = 0; break;
    case IR24_ON        : bri = briLast;                    break;
    case IR24_RED       : colorFromUint32(COLOR_RED);       break;
    case IR24_REDDISH   : colorFromUint32(COLOR_REDDISH);   break;
    case IR24_ORANGE    : colorFromUint32(COLOR_ORANGE);    break;
    case IR24_YELLOWISH : colorFromUint32(COLOR_YELLOWISH); break;
    case IR24_YELLOW    : colorFromUint32(COLOR_YELLOW);    break;
    case IR24_GREEN     : colorFromUint32(COLOR_GREEN);     break;
    case IR24_GREENISH  : colorFromUint32(COLOR_GREENISH);  break;
    case IR24_TURQUOISE : colorFromUint32(COLOR_TURQUOISE); break;
    case IR24_CYAN      : colorFromUint32(COLOR_CYAN);      break;
    case IR24_AQUA      : colorFromUint32(COLOR_AQUA);      break;
    case IR24_BLUE      : colorFromUint32(COLOR_BLUE);      break;
    case IR24_DEEPBLUE  : colorFromUint32(COLOR_DEEPBLUE);  break;
    case IR24_PURPLE    : colorFromUint32(COLOR_PURPLE);    break;
    case IR24_MAGENTA   : colorFromUint32(COLOR_MAGENTA);   break;
    case IR24_PINK      : colorFromUint32(COLOR_PINK);      break;
    case IR24_WHITE     : colorFromUint32(COLOR_WHITE);        effectCurrent = 0;  break;
    case IR24_FLASH     : presetFallback(1, FX_MODE_COLORTWINKLE, effectPalette);  break;
    case IR24_STROBE    : presetFallback(2, FX_MODE_RAINBOW_CYCLE, effectPalette); break;
    case IR24_FADE      : presetFallback(3, FX_MODE_BREATH, effectPalette);        break;
    case IR24_SMOOTH    : presetFallback(4, FX_MODE_RAINBOW, effectPalette);       break;
    default: return;
  }
  lastValidCode = code;
}

void decodeIR24OLD(uint32_t code)
{
  switch (code) {
    case IR24_OLD_BRIGHTER  : incBrightness();                     break;
    case IR24_OLD_DARKER    : decBrightness();                     break;
    case IR24_OLD_OFF       : if (bri > 0) briLast = bri; bri = 0; break;
    case IR24_OLD_ON        : bri = briLast;                       break;
    case IR24_OLD_RED       : colorFromUint32(COLOR_RED);          break;
    case IR24_OLD_REDDISH   : colorFromUint32(COLOR_REDDISH);      break;
    case IR24_OLD_ORANGE    : colorFromUint32(COLOR_ORANGE);       break;
    case IR24_OLD_YELLOWISH : colorFromUint32(COLOR_YELLOWISH);    break;
    case IR24_OLD_YELLOW    : colorFromUint32(COLOR_YELLOW);       break;
    case IR24_OLD_GREEN     : colorFromUint32(COLOR_GREEN);        break;
    case IR24_OLD_GREENISH  : colorFromUint32(COLOR_GREENISH);     break;
    case IR24_OLD_TURQUOISE : colorFromUint32(COLOR_TURQUOISE);    break;
    case IR24_OLD_CYAN      : colorFromUint32(COLOR_CYAN);         break;
    case IR24_OLD_AQUA      : colorFromUint32(COLOR_AQUA);         break;
    case IR24_OLD_BLUE      : colorFromUint32(COLOR_BLUE);         break;
    case IR24_OLD_DEEPBLUE  : colorFromUint32(COLOR_DEEPBLUE);     break;
    case IR24_OLD_PURPLE    : colorFromUint32(COLOR_PURPLE);       break;
    case IR24_OLD_MAGENTA   : colorFromUint32(COLOR_MAGENTA);      break;
    case IR24_OLD_PINK      : colorFromUint32(COLOR_PINK);         break;
    case IR24_OLD_WHITE     : colorFromUint32(COLOR_WHITE); effectCurrent = 0; break;
    case IR24_OLD_FLASH     : presetFallback(1, FX_MODE_COLORTWINKLE, 0);      break;
    case IR24_OLD_STROBE    : presetFallback(2, FX_MODE_RAINBOW_CYCLE, 0);     break;
    case IR24_OLD_FADE      : presetFallback(3, FX_MODE_BREATH, 0);            break;
    case IR24_OLD_SMOOTH    : presetFallback(4, FX_MODE_RAINBOW, 0);           break;
    default: return;
  }
  lastValidCode = code;
}

void decodeIR24CT(uint32_t code)
{
  switch (code) {
    case IR24_CT_BRIGHTER   : incBrightness();                     break;
    case IR24_CT_DARKER     : decBrightness();                     break;
    case IR24_CT_OFF        : if (bri > 0) briLast = bri; bri = 0; break;
    case IR24_CT_ON         : bri = briLast;                       break;
    case IR24_CT_RED        : colorFromUint32(COLOR_RED);          break;
    case IR24_CT_REDDISH    : colorFromUint32(COLOR_REDDISH);      break;
    case IR24_CT_ORANGE     : colorFromUint32(COLOR_ORANGE);       break;
    case IR24_CT_YELLOWISH  : colorFromUint32(COLOR_YELLOWISH);    break;
    case IR24_CT_YELLOW     : colorFromUint32(COLOR_YELLOW);       break;
    case IR24_CT_GREEN      : colorFromUint32(COLOR_GREEN);        break;
    case IR24_CT_GREENISH   : colorFromUint32(COLOR_GREENISH);     break;
    case IR24_CT_TURQUOISE  : colorFromUint32(COLOR_TURQUOISE);    break;
    case IR24_CT_CYAN       : colorFromUint32(COLOR_CYAN);         break;
    case IR24_CT_AQUA       : colorFromUint32(COLOR_AQUA);         break;
    case IR24_CT_BLUE       : colorFromUint32(COLOR_BLUE);         break;
    case IR24_CT_DEEPBLUE   : colorFromUint32(COLOR_DEEPBLUE);     break;
    case IR24_CT_PURPLE     : colorFromUint32(COLOR_PURPLE);       break;
    case IR24_CT_MAGENTA    : colorFromUint32(COLOR_MAGENTA);      break;
    case IR24_CT_PINK       : colorFromUint32(COLOR_PINK);         break;
    case IR24_CT_COLDWHITE  : colorFromUint32(COLOR2_COLDWHITE);    effectCurrent = 0;  break;
    case IR24_CT_WARMWHITE  : colorFromUint32(COLOR2_WARMWHITE);    effectCurrent = 0;  break;
    case IR24_CT_CTPLUS     : colorFromUint32(COLOR2_COLDWHITE2);   effectCurrent = 0;  break;
    case IR24_CT_CTMINUS    : colorFromUint32(COLOR2_WARMWHITE2);   effectCurrent = 0;  break;
    case IR24_CT_MEMORY   : {
      if (col[3] > 0) col[3] = 0; 
      else colorFromUint32(COLOR2_NEUTRALWHITE); effectCurrent = 0; }                   break;
    default: return; 
  }
  lastValidCode = code;
}

void decodeIR40(uint32_t code)
{
  switch (code) {
    case IR40_BPLUS        : incBrightness();                                            break;
    case IR40_BMINUS       : decBrightness();                                            break;
    case IR40_OFF          : if (bri > 0) briLast = bri; bri = 0;                        break;
    case IR40_ON           : bri = briLast;                                              break;
    case IR40_RED          : colorFromUint24(COLOR_RED);                                 break;
    case IR40_REDDISH      : colorFromUint24(COLOR_REDDISH);                             break;
    case IR40_ORANGE       : colorFromUint24(COLOR_ORANGE);                              break;
    case IR40_YELLOWISH    : colorFromUint24(COLOR_YELLOWISH);                           break;
    case IR40_YELLOW       : colorFromUint24(COLOR_YELLOW);                              break;
    case IR40_GREEN        : colorFromUint24(COLOR_GREEN);                               break;
    case IR40_GREENISH     : colorFromUint24(COLOR_GREENISH);                            break;
    case IR40_TURQUOISE    : colorFromUint24(COLOR_TURQUOISE);                           break;
    case IR40_CYAN         : colorFromUint24(COLOR_CYAN);                                break;
    case IR40_AQUA         : colorFromUint24(COLOR_AQUA);                                break;
    case IR40_BLUE         : colorFromUint24(COLOR_BLUE);                                break;
    case IR40_DEEPBLUE     : colorFromUint24(COLOR_DEEPBLUE);                            break;
    case IR40_PURPLE       : colorFromUint24(COLOR_PURPLE);                              break;
    case IR40_MAGENTA      : colorFromUint24(COLOR_MAGENTA);                             break;
    case IR40_PINK         : colorFromUint24(COLOR_PINK);                                break;
    case IR40_WARMWHITE2   : {
      if (strip.isRgbw) {        colorFromUint32(COLOR2_WARMWHITE2);   effectCurrent = 0; }    
      else                  colorFromUint24(COLOR_WARMWHITE2);                       }   break;
    case IR40_WARMWHITE    : {
      if (strip.isRgbw) {        colorFromUint32(COLOR2_WARMWHITE);    effectCurrent = 0; }    
      else                  colorFromUint24(COLOR_WARMWHITE);                        }   break;
    case IR40_WHITE        : {
      if (strip.isRgbw) {        colorFromUint32(COLOR2_NEUTRALWHITE); effectCurrent = 0; }    
      else                  colorFromUint24(COLOR_NEUTRALWHITE);                     }   break;
    case IR40_COLDWHITE    : {
      if (strip.isRgbw) {        colorFromUint32(COLOR2_COLDWHITE);    effectCurrent = 0; }   
      else                  colorFromUint24(COLOR_COLDWHITE);                        }   break;
    case IR40_COLDWHITE2    : {
      if (strip.isRgbw) {        colorFromUint32(COLOR2_COLDWHITE2);   effectCurrent = 0; }   
      else                  colorFromUint24(COLOR_COLDWHITE2);                       }   break;
    case IR40_WPLUS        : relativeChangeWhite(10);                                    break;
    case IR40_WMINUS       : relativeChangeWhite(-10, 5);                                break;
    case IR40_WOFF         : whiteLast = col[3]; col[3] = 0;                             break;
    case IR40_WON          : col[3] = whiteLast;                                         break;
    case IR40_W25          : bri = 63;                                                   break;
    case IR40_W50          : bri = 127;                                                  break;
    case IR40_W75          : bri = 191;                                                  break;
    case IR40_W100         : bri = 255;                                                  break;
    case IR40_QUICK        : changeEffectSpeed( 16);                                     break;
    case IR40_SLOW         : changeEffectSpeed(-16);                                     break;
    case IR40_JUMP7        : changeEffectIntensity( 16);                                 break;
    case IR40_AUTO         : changeEffectIntensity(-16);                                 break;
    case IR40_JUMP3        : presetFallback(1, FX_MODE_STATIC,       0); break;
    case IR40_FADE3        : presetFallback(2, FX_MODE_BREATH,       0); break;
    case IR40_FADE7        : presetFallback(3, FX_MODE_FIRE_FLICKER, 0); break;
    case IR40_FLASH        : presetFallback(4, FX_MODE_RAINBOW,      0); break;
  }
  lastValidCode = code;
}

void decodeIR44(uint32_t code)
{
  switch (code) {
    case IR44_BPLUS       : incBrightness();                                            break;
    case IR44_BMINUS      : decBrightness();                                            break;
    case IR44_OFF         : if (bri > 0) briLast = bri; bri = 0;                        break;
    case IR44_ON          : bri = briLast;                                              break;
    case IR44_RED         : colorFromUint24(COLOR_RED);                                 break;
    case IR44_REDDISH     : colorFromUint24(COLOR_REDDISH);                             break;
    case IR44_ORANGE      : colorFromUint24(COLOR_ORANGE);                              break;
    case IR44_YELLOWISH   : colorFromUint24(COLOR_YELLOWISH);                           break;
    case IR44_YELLOW      : colorFromUint24(COLOR_YELLOW);                              break;
    case IR44_GREEN       : colorFromUint24(COLOR_GREEN);                               break;
    case IR44_GREENISH    : colorFromUint24(COLOR_GREENISH);                            break;
    case IR44_TURQUOISE   : colorFromUint24(COLOR_TURQUOISE);                           break;
    case IR44_CYAN        : colorFromUint24(COLOR_CYAN);                                break;
    case IR44_AQUA        : colorFromUint24(COLOR_AQUA);                                break;
    case IR44_BLUE        : colorFromUint24(COLOR_BLUE);                                break;
    case IR44_DEEPBLUE    : colorFromUint24(COLOR_DEEPBLUE);                            break;
    case IR44_PURPLE      : colorFromUint24(COLOR_PURPLE);                              break;
    case IR44_MAGENTA     : colorFromUint24(COLOR_MAGENTA);                             break;
    case IR44_PINK        : colorFromUint24(COLOR_PINK);                                break;
    case IR44_WHITE       : {
      if (strip.isRgbw) {
        if (col[3] > 0) col[3] = 0; 
        else {              colorFromUint32(COLOR2_NEUTRALWHITE); effectCurrent = 0; }
      } else                colorFromUint24(COLOR_NEUTRALWHITE);                     }  break;
    case IR44_WARMWHITE2  : {
      if (strip.isRgbw) {        colorFromUint32(COLOR2_WARMWHITE2);   effectCurrent = 0; }    
      else                  colorFromUint24(COLOR_WARMWHITE2);                       }  break;
    case IR44_WARMWHITE   : {
      if (strip.isRgbw) {        colorFromUint32(COLOR2_WARMWHITE);    effectCurrent = 0; }    
      else                  colorFromUint24(COLOR_WARMWHITE);                        }  break;
    case IR44_COLDWHITE   : {
      if (strip.isRgbw) {        colorFromUint32(COLOR2_COLDWHITE);    effectCurrent = 0; }   
      else                  colorFromUint24(COLOR_COLDWHITE);                        }  break;
    case IR44_COLDWHITE2  : {
      if (strip.isRgbw) {        colorFromUint32(COLOR2_COLDWHITE2);   effectCurrent = 0; }    
      else                  colorFromUint24(COLOR_COLDWHITE2);                       }  break;
    case IR44_REDPLUS     : relativeChange(&effectCurrent,  1, 0, MODE_COUNT);          break;
    case IR44_REDMINUS    : relativeChange(&effectCurrent, -1, 0);                      break;
    case IR44_GREENPLUS   : relativeChange(&effectPalette,  1, 0, strip.getPaletteCount() -1);     break;
    case IR44_GREENMINUS  : relativeChange(&effectPalette, -1, 0);                      break;
    case IR44_BLUEPLUS    : changeEffectIntensity( 16);                                 break;
    case IR44_BLUEMINUS   : changeEffectIntensity(-16);                                 break;
    case IR44_QUICK       : changeEffectSpeed( 16);                                     break;
    case IR44_SLOW        : changeEffectSpeed(-16);                                     break;
    case IR44_DIY1        : presetFallback(1, FX_MODE_STATIC,        0); break;
    case IR44_DIY2        : presetFallback(2, FX_MODE_BREATH,        0); break;
    case IR44_DIY3        : presetFallback(3, FX_MODE_FIRE_FLICKER,  0); break;
    case IR44_DIY4        : presetFallback(4, FX_MODE_RAINBOW,       0); break;
    case IR44_DIY5        : presetFallback(5, FX_MODE_METEOR_SMOOTH, 0); break;
    case IR44_DIY6        : presetFallback(6, FX_MODE_RAIN,          0); break;
    case IR44_AUTO        : effectCurrent = FX_MODE_STATIC;                             break;
    case IR44_FLASH       : effectCurrent = FX_MODE_PALETTE;                            break;
    case IR44_JUMP3       : bri = 63;                                                   break;
    case IR44_JUMP7       : bri = 127;                                                  break;
    case IR44_FADE3       : bri = 191;                                                  break;
    case IR44_FADE7       : bri = 255;                                                  break;
  }
  lastValidCode = code;
}

void decodeIR21(uint32_t code)
{
    switch (code) {
    case IR21_BRIGHTER:  incBrightness();                  break;
    case IR21_DARKER:    decBrightness();                  break;
    case IR21_OFF:    if (bri > 0) briLast = bri; bri = 0; break;
    case IR21_ON:        bri = briLast;                    break;
    case IR21_RED:       colorFromUint32(COLOR_RED);       break;
    case IR21_REDDISH:   colorFromUint32(COLOR_REDDISH);   break;
    case IR21_ORANGE:    colorFromUint32(COLOR_ORANGE);    break;
    case IR21_YELLOWISH: colorFromUint32(COLOR_YELLOWISH); break;
    case IR21_GREEN:     colorFromUint32(COLOR_GREEN);     break;
    case IR21_GREENISH:  colorFromUint32(COLOR_GREENISH);  break;
    case IR21_TURQUOISE: colorFromUint32(COLOR_TURQUOISE); break;
    case IR21_CYAN:      colorFromUint32(COLOR_CYAN);      break;
    case IR21_BLUE:      colorFromUint32(COLOR_BLUE);      break;
    case IR21_DEEPBLUE:  colorFromUint32(COLOR_DEEPBLUE);  break;
    case IR21_PURPLE:    colorFromUint32(COLOR_PURPLE);    break;
    case IR21_PINK:      colorFromUint32(COLOR_PINK);      break;
    case IR21_WHITE:     colorFromUint32(COLOR_WHITE);           effectCurrent = 0;  break;
    case IR21_FLASH:     presetFallback(1, FX_MODE_COLORTWINKLE,  0); break;
    case IR21_STROBE:    presetFallback(2, FX_MODE_RAINBOW_CYCLE, 0); break;
    case IR21_FADE:      presetFallback(3, FX_MODE_BREATH,        0); break;
    case IR21_SMOOTH:    presetFallback(4, FX_MODE_RAINBOW,       0); break;
    default: return;
    }
    lastValidCode = code;
}

void decodeIR6(uint32_t code)
{
  switch (code) {
    case IR6_POWER: toggleOnOff();                                          break;
    case IR6_CHANNEL_UP: incBrightness();                                   break;
    case IR6_CHANNEL_DOWN: decBrightness();                                 break;
    case IR6_VOLUME_UP:   relativeChange(&effectCurrent, 1, 0, MODE_COUNT); break;  // next effect
    case IR6_VOLUME_DOWN:                                                           // next palette
      relativeChange(&effectPalette, 1, 0, strip.getPaletteCount() -1); 
      switch(lastIR6ColourIdx) {
        case 0: colorFromUint32(COLOR_RED);       break;
        case 1: colorFromUint32(COLOR_REDDISH);   break;
        case 2: colorFromUint32(COLOR_ORANGE);    break;
        case 3: colorFromUint32(COLOR_YELLOWISH); break;
        case 4: colorFromUint32(COLOR_GREEN);     break;
        case 5: colorFromUint32(COLOR_GREENISH);  break;
        case 6: colorFromUint32(COLOR_TURQUOISE); break;
        case 7: colorFromUint32(COLOR_CYAN);      break;
        case 8: colorFromUint32(COLOR_BLUE);      break;
        case 9: colorFromUint32(COLOR_DEEPBLUE);  break;
        case 10:colorFromUint32(COLOR_PURPLE);    break;
        case 11:colorFromUint32(COLOR_PINK);      break;
        case 12:colorFromUint32(COLOR_WHITE);     break;
        default:                                  break;
      }
      lastIR6ColourIdx++;
      if(lastIR6ColourIdx > 12) lastIR6ColourIdx = 0;                      break;
    case IR6_MUTE: effectCurrent = 0; effectPalette = 0; colorFromUint32(COLOR_WHITE); bri=255; break;
  }
  lastValidCode = code;
}

void decodeIR9(uint32_t code)
{
  switch (code) {
    case IR9_POWER      : toggleOnOff();  break;
    case IR9_A          : presetFallback(1, FX_MODE_COLORTWINKLE, effectPalette);  break;
    case IR9_B          : presetFallback(2, FX_MODE_RAINBOW_CYCLE, effectPalette); break;
    case IR9_C          : presetFallback(3, FX_MODE_BREATH, effectPalette);        break;
    case IR9_UP         : incBrightness();                                            break;
    case IR9_DOWN       : decBrightness();                                            break;
    //case IR9_UP         : changeEffectIntensity(16);         break;
    //case IR9_DOWN       : changeEffectIntensity(-16);     break;
    case IR9_LEFT       : changeEffectSpeed(-16);                                     break;
    case IR9_RIGHT      : changeEffectSpeed(16);                                      break;
    case IR9_SELECT     : relativeChange(&effectCurrent, 1, 0, MODE_COUNT);           break;
    default: return;
  }
  lastValidCode = code;
}


/*
This allows users to customize IR actions without the need to edit C code and compile.
From the https://github.com/Aircoookie/WLED/wiki/Infrared-Control page, download the starter 
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
void decodeIRJson(uint32_t code) 
{
  char objKey[10];
  String cmdStr;
  JsonObject fdo;
  JsonObject jsonCmdObj;

  #ifdef WLED_USE_DYNAMIC_JSON
  DynamicJsonDocument doc(JSON_BUFFER_SIZE);
  #else
  if (!requestJSONBufferLock(13)) return;
  #endif

  sprintf_P(objKey, PSTR("\"0x%lX\":"), (unsigned long)code);

  // attempt to read command from ir.json
  // this may fail for two reasons: ir.json does not exist or IR code not found
  // if the IR code is not found readObjectFromFile() will clean() doc JSON document
  // so we can differentiate between the two
  readObjectFromFile("/ir.json", objKey, &doc);
  fdo = doc.as<JsonObject>();
  lastValidCode = 0;
  if (fdo.isNull()) {
    //the received code does not exist
    if (!WLED_FS.exists("/ir.json")) errorFlag = ERR_FS_IRLOAD; //warn if IR file itself doesn't exist
    releaseJSONBufferLock();
    return;
  }

  cmdStr = fdo["cmd"].as<String>();
  jsonCmdObj = fdo["cmd"]; //object

  if (!cmdStr.isEmpty()) 
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
        uint8_t p1 = fdo["PL"] ? fdo["PL"] : 1;
        uint8_t p2 = fdo["FX"] ? fdo["FX"] : random8(MODE_COUNT);
        uint8_t p3 = fdo["FP"] ? fdo["FP"] : 0;
        presetFallback(p1, p2, p3);
      }
    } else {
      // HTTP API command
      if (cmdStr.indexOf("~") || fdo["rpt"]) 
      {
        // repeatable action
        lastValidCode = code;
      }
      if (effectCurrent == 0 && cmdStr.indexOf("FP=") > -1) {
        // setting palette but it wont show because effect is solid
        effectCurrent = FX_MODE_GRADIENT;
      }
      if (!cmdStr.startsWith("win&")) {
        cmdStr = "win&" + cmdStr;
      }
      handleSet(nullptr, cmdStr, false);
    }
    colorUpdated(CALL_MODE_BUTTON);
  } else if (!jsonCmdObj.isNull()) {
    // command is JSON object
    deserializeState(jsonCmdObj, CALL_MODE_BUTTON_PRESET);
  }
  releaseJSONBufferLock();
}

void initIR()
{
  if (irEnabled > 0)
  {
    irrecv = new IRrecv(irPin);
    irrecv->enableIRIn();
  }
}

void handleIR()
{
  if (irEnabled > 0 && millis() - irCheckedTime > 120)
  {
    irCheckedTime = millis();
    if (irEnabled > 0)
    {
      if (irrecv == NULL)
      { 
        initIR(); return;
      }
      
      if (irrecv->decode(&results))
      {
        if (results.value != 0) // only print results if anything is received ( != 0 )
        {
					if (!pinManager.isPinAllocated(1)) //GPIO 1 - Serial TX pin
          	Serial.printf_P(PSTR("IR recv: 0x%lX\n"), (unsigned long)results.value);
        }
        decodeIR(results.value);
        irrecv->resume();
      }
    } else if (irrecv != NULL)
    {
      irrecv->disableIRIn();
      delete irrecv; irrecv = NULL;
    }
  }
}

#endif
