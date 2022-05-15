#include "wled.h"
#include "src/dependencies/blynk/Blynk/BlynkHandlers.h"

/*
 * Remote light control with the free Blynk app
 */

uint16_t blHue = 0;
byte blSat = 255;

void initBlynk(const char *auth, const char *host, uint16_t port)
{
  #ifndef WLED_DISABLE_BLYNK
  if (!WLED_CONNECTED) return;
  blynkEnabled = (auth[0] != 0);
  if (blynkEnabled) Blynk.config(auth, host, port);
  #endif
}

void handleBlynk()
{
  #ifndef WLED_DISABLE_BLYNK
  if (WLED_CONNECTED && blynkEnabled)
  Blynk.run();
  #endif
}

void updateBlynk()
{
  #ifndef WLED_DISABLE_BLYNK
  if (!WLED_CONNECTED) return;
  Blynk.virtualWrite(V0, bri);
  //we need a RGB -> HSB convert here
  Blynk.virtualWrite(V3, bri? 1:0);
  Blynk.virtualWrite(V4, effectCurrent);
  Blynk.virtualWrite(V5, effectSpeed);
  Blynk.virtualWrite(V6, effectIntensity);
  Blynk.virtualWrite(V7, nightlightActive);
  Blynk.virtualWrite(V8, notifyDirect);
  #endif
}

#ifndef WLED_DISABLE_BLYNK
BLYNK_WRITE(V0)
{
  bri = param.asInt();//bri
  stateUpdated(CALL_MODE_BLYNK);
}

BLYNK_WRITE(V1)
{
  blHue = param.asInt();//hue
  colorHStoRGB(blHue*10,blSat,col);
  colorUpdated(CALL_MODE_BLYNK);
}

BLYNK_WRITE(V2)
{
  blSat = param.asInt();//sat
  colorHStoRGB(blHue*10,blSat,col);
  colorUpdated(CALL_MODE_BLYNK);
}

BLYNK_WRITE(V3)
{
  bool on = (param.asInt()>0);
  if (!on != !bri) {toggleOnOff(); stateUpdated(CALL_MODE_BLYNK);}
}

BLYNK_WRITE(V4)
{
  effectCurrent = param.asInt()-1;//fx
  colorUpdated(CALL_MODE_BLYNK);
}

BLYNK_WRITE(V5)
{
  effectSpeed = param.asInt();//sx
  colorUpdated(CALL_MODE_BLYNK);
}

BLYNK_WRITE(V6)
{
  effectIntensity = param.asInt();//ix
  colorUpdated(CALL_MODE_BLYNK);
}

BLYNK_WRITE(V7)
{
  nightlightActive = (param.asInt()>0);
}

BLYNK_WRITE(V8)
{
  notifyDirect = (param.asInt()>0); //send notifications
}
#endif
