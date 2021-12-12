#include "wled.h"

/*
 * LED methods
 */
void setValuesFromMainSeg()
{
  WS2812FX::Segment& seg = strip.getSegment(strip.getMainSegmentId());
  colorFromUint32(seg.colors[0]);
  colorFromUint32(seg.colors[1], true);
  effectCurrent = seg.mode;
  effectSpeed = seg.speed;
  effectIntensity = seg.intensity;
  effectPalette = seg.palette;
}


void resetTimebase()
{
  strip.timebase = 0 - millis();
}


void toggleOnOff()
{
  if (bri == 0)
  {
    bri = briLast;
  } else
  {
    briLast = bri;
    bri = 0;
  }
}


//scales the brightness with the briMultiplier factor
byte scaledBri(byte in)
{
  uint16_t val = ((uint16_t)in*briMultiplier)/100;
  if (val > 255) val = 255;
  return (byte)val;
}


void setAllLeds() {
  strip.setColor(0, col[0], col[1], col[2], col[3]);
  strip.setColor(1, colSec[0], colSec[1], colSec[2], colSec[3]);
  if (!realtimeMode || !arlsForceMaxBri)
  {
    strip.setBrightness(scaledBri(briT));
  }
}


void setLedsStandard()
{
  briOld = bri;
  briT = bri;
  setAllLeds();
}


bool colorChanged()
{
  for (byte i=0; i<4; i++)
  {
    if (col[i] != colIT[i]) return true;
    if (colSec[i] != colSecIT[i]) return true;
  }
  if (bri != briIT) return true;
  return false;
}


void colorUpdated(int callMode)
{
  //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
  //                     6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa 11: ws send only 12: button preset
  if (callMode != CALL_MODE_INIT && 
      callMode != CALL_MODE_DIRECT_CHANGE && 
      callMode != CALL_MODE_NO_NOTIFY &&
			callMode != CALL_MODE_BUTTON_PRESET) strip.applyToAllSelected = true; //if not from JSON api, which directly sets segments

  bool someSel = false;

  if (callMode == CALL_MODE_NOTIFICATION) {
    someSel = (receiveNotificationBrightness || receiveNotificationColor || receiveNotificationEffects);
  }
  
  //Notifier: apply received FX to selected segments only if actually receiving FX
  if (someSel) strip.applyToAllSelected = receiveNotificationEffects;

  bool fxChanged = strip.setEffectConfig(effectCurrent, effectSpeed, effectIntensity, effectPalette) || effectChanged;
  bool colChanged = colorChanged();

  //Notifier: apply received color to selected segments only if actually receiving color
  if (someSel) strip.applyToAllSelected = receiveNotificationColor;

  if (fxChanged || colChanged)
  {
    effectChanged = false;
    if (realtimeTimeout == UINT32_MAX) realtimeTimeout = 0;
    currentPreset = 0; //something changed, so we are no longer in the preset
        
    notify(callMode);
    
    //set flag to update blynk, ws and mqtt
    interfaceUpdateCallMode = callMode;
  } else {
    if (nightlightActive && !nightlightActiveOld && 
        callMode != CALL_MODE_NOTIFICATION && 
        callMode != CALL_MODE_NO_NOTIFY)
    {
      notify(CALL_MODE_NIGHTLIGHT); 
      interfaceUpdateCallMode = CALL_MODE_NIGHTLIGHT;
    }
  }
  
  if (!colChanged) return; //following code is for e.g. initiating transitions
  
  if (callMode != CALL_MODE_NO_NOTIFY && nightlightActive && (nightlightMode == NL_MODE_FADE || nightlightMode == NL_MODE_COLORFADE))
  {
    briNlT = bri;
    nightlightDelayMs -= (millis() - nightlightStartTime);
    nightlightStartTime = millis();
  }
  for (byte i=0; i<4; i++)
  {
    colIT[i] = col[i];
    colSecIT[i] = colSec[i];
  }
  if (briT == 0)
  {
    if (callMode != CALL_MODE_NOTIFICATION) resetTimebase(); //effect start from beginning
  }

  briIT = bri;
  if (bri > 0) briLast = bri;

  //deactivate nightlight if target brightness is reached
  if (bri == nightlightTargetBri && callMode != CALL_MODE_NO_NOTIFY && nightlightMode != NL_MODE_SUN) nightlightActive = false;
  
  if (fadeTransition)
  {
    //set correct delay if not using notification delay
    if (callMode != CALL_MODE_NOTIFICATION && !jsonTransitionOnce) transitionDelayTemp = transitionDelay;
    jsonTransitionOnce = false;
    strip.setTransition(transitionDelayTemp);
    if (transitionDelayTemp == 0) {setLedsStandard(); strip.trigger(); return;}
    
    if (transitionActive)
    {
      briOld = briT;
      tperLast = 0;
    }
    strip.setTransitionMode(true);
    transitionActive = true;
    transitionStartTime = millis();
  } else
  {
    strip.setTransition(0);
    setLedsStandard();
    strip.trigger();
  }
}


void updateInterfaces(uint8_t callMode)
{
  sendDataWs();
  if (callMode == CALL_MODE_WS_SEND) {
    lastInterfaceUpdate = millis();
    return;
  }
  
  #ifndef WLED_DISABLE_ALEXA
  if (espalexaDevice != nullptr && callMode != CALL_MODE_ALEXA) {
    espalexaDevice->setValue(bri);
    espalexaDevice->setColor(col[0], col[1], col[2]);
  }
  #endif
  #ifndef WLED_DISABLE_BLYNK
  if (callMode != CALL_MODE_BLYNK && 
      callMode != CALL_MODE_NO_NOTIFY) updateBlynk();
  #endif
  doPublishMqtt = true;
  lastInterfaceUpdate = millis();
}


void handleTransitions()
{
  //handle still pending interface update
  if (interfaceUpdateCallMode && millis() - lastInterfaceUpdate > 2000)
  {
    updateInterfaces(interfaceUpdateCallMode);
    interfaceUpdateCallMode = 0; //disable
  }
  if (doPublishMqtt) publishMqtt();
  
  if (transitionActive && transitionDelayTemp > 0)
  {
    float tper = (millis() - transitionStartTime)/(float)transitionDelayTemp;
    if (tper >= 1.0)
    {
      strip.setTransitionMode(false);
      transitionActive = false;
      tperLast = 0;
      setLedsStandard();
      return;
    }
    if (tper - tperLast < 0.004) return;
    tperLast = tper;
    briT    = briOld   +((bri    - briOld   )*tper);
    
    setAllLeds();
  }
}


void handleNightlight()
{
  if (nightlightActive)
  {
    if (!nightlightActiveOld) //init
    {
      nightlightStartTime = millis();
      nightlightDelayMs = (int)(nightlightDelayMins*60000);
      nightlightActiveOld = true;
      briNlT = bri;
      for (byte i=0; i<4; i++) colNlT[i] = col[i]; // remember starting color
      if (nightlightMode == NL_MODE_SUN)
      {
        //save current
        colNlT[0] = effectCurrent;
        colNlT[1] = effectSpeed;
        colNlT[2] = effectPalette;

        strip.setMode(strip.getMainSegmentId(), FX_MODE_STATIC); //make sure seg runtime is reset if left in sunrise mode
        effectCurrent = FX_MODE_SUNRISE;
        effectSpeed = nightlightDelayMins;
        effectPalette = 0;
        if (effectSpeed > 60) effectSpeed = 60; //currently limited to 60 minutes
        if (bri) effectSpeed += 60; //sunset if currently on
        briNlT = !bri; //true == sunrise, false == sunset
        if (!bri) bri = briLast;
        colorUpdated(CALL_MODE_NO_NOTIFY);
      }
    }
    float nper = (millis() - nightlightStartTime)/((float)nightlightDelayMs);
    if (nightlightMode == NL_MODE_FADE || nightlightMode == NL_MODE_COLORFADE)
    {
      bri = briNlT + ((nightlightTargetBri - briNlT)*nper);
      if (nightlightMode == NL_MODE_COLORFADE)                                         // color fading only is enabled with "NF=2"
      {
        for (byte i=0; i<4; i++) col[i] = colNlT[i]+ ((colSec[i] - colNlT[i])*nper);   // fading from actual color to secondary color
      }
      colorUpdated(CALL_MODE_NO_NOTIFY);
    }
    if (nper >= 1) //nightlight duration over
    {
      nightlightActive = false;
      if (nightlightMode == NL_MODE_SET)
      {
        bri = nightlightTargetBri;
        colorUpdated(CALL_MODE_NO_NOTIFY);
      }
      if (bri == 0) briLast = briNlT;
      if (nightlightMode == NL_MODE_SUN)
      {
        if (!briNlT) { //turn off if sunset
          effectCurrent = colNlT[0];
          effectSpeed = colNlT[1];
          effectPalette = colNlT[2];
          toggleOnOff();
          setLedsStandard();
        }
      }
      #ifndef WLED_DISABLE_BLYNK
      updateBlynk();
      #endif
      if (macroNl > 0)
        applyPreset(macroNl);
      nightlightActiveOld = false;
    }
  } else if (nightlightActiveOld) //early de-init
  {
    if (nightlightMode == NL_MODE_SUN) { //restore previous effect
      effectCurrent = colNlT[0];
      effectSpeed = colNlT[1];
      effectPalette = colNlT[2];
      colorUpdated(CALL_MODE_NO_NOTIFY);
    }
    nightlightActiveOld = false;
  }
}

//utility for FastLED to use our custom timer
uint32_t get_millisecond_timer()
{
  return strip.now;
}
