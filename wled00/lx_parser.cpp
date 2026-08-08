#include "wled.h"

#ifdef WLED_ENABLE_LOXONE

/*
 * Parser for Loxone formats
 */
bool parseLx(int lxValue, CRGBW &rgbw)
{
  DEBUG_PRINT(F("LX: Lox = "));
  DEBUG_PRINTLN(lxValue);

  bool ok = false;
  float lxRed = 0, lxGreen = 0, lxBlue = 0;

  if (lxValue < 200000000) {
    // Loxone RGB
    ok = true;
    lxRed = round((lxValue % 1000) * 2.55);
    lxGreen = round(((lxValue / 1000) % 1000) * 2.55);
    lxBlue = round(((lxValue / 1000000) % 1000) * 2.55);
  } else if ((lxValue >= 200000000) && (lxValue <= 201006500)) {
    // Loxone Lumitech
    ok = true;
    float tmpBri = floor((lxValue - 200000000) / 10000);
    uint16_t ct = (lxValue - 200000000) - (((uint8_t)tmpBri) * 10000);

    tmpBri *= 2.55f;
    tmpBri = constrain(tmpBri, 0, 255);

    colorKtoRGB(ct, rgbw);
    lxRed = rgbw.r; lxGreen = rgbw.g; lxBlue = rgbw.b;

    lxRed *= (tmpBri/255);
    lxGreen *= (tmpBri/255);
    lxBlue *= (tmpBri/255);
  }

  if (ok) {
    rgbw = 0; // white is unused, make sure it does not contain garbage value
    rgbw.r = (uint8_t) constrain(lxRed, 0, 255);
    rgbw.g = (uint8_t) constrain(lxGreen, 0, 255);
    rgbw.b = (uint8_t) constrain(lxBlue, 0, 255);
    return true;
  }
  return false;
}

void parseLxJson(int lxValue, byte segId, bool secondary)
{
  if (secondary) {
    DEBUG_PRINT(F("LY: Lox secondary = "));
  } else {
    DEBUG_PRINT(F("LX: Lox primary = "));
  }
  DEBUG_PRINTLN(lxValue);
  CRGBW rgbw = 0;
  if (parseLx(lxValue, rgbw)) {
    if (bri == 0) {
      DEBUG_PRINTLN(F("LX: turn on"));
      toggleOnOff();
    }
    bri = 255;
    nightlightActive = false; //always disable nightlight when toggling
    DEBUG_PRINT(F("LX: segment "));
    DEBUG_PRINTLN(segId);
    strip.getSegment(segId).setColor(secondary, rgbw); // legacy values handled as well in json.cpp by stateUpdated()
  }
}

#endif
