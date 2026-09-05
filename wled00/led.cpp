#include "wled.h"

/*
 * LED methods
 */

 // applies chosen setment properties to legacy values
void setValuesFromSegment(uint8_t s) {
  const Segment& seg = strip.getSegment(s);
  colPri[0] = R(seg.colors[0]);
  colPri[1] = G(seg.colors[0]);
  colPri[2] = B(seg.colors[0]);
  colPri[3] = W(seg.colors[0]);
  colSec[0] = R(seg.colors[1]);
  colSec[1] = G(seg.colors[1]);
  colSec[2] = B(seg.colors[1]);
  colSec[3] = W(seg.colors[1]);
  effectCurrent   = seg.mode;
  effectSpeed     = seg.speed;
  effectIntensity = seg.intensity;
  effectPalette   = seg.palette;
}


// applies global legacy values (colPri, colSec, effectCurrent...) to each selected segment
void applyValuesToSelectedSegs() {
  for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
    Segment& seg = strip.getSegment(i);
    if (!(seg.isActive() && seg.isSelected())) continue;
    if (effectSpeed     != seg.speed)     {seg.speed     = effectSpeed;     stateChanged = true;}
    if (effectIntensity != seg.intensity) {seg.intensity = effectIntensity; stateChanged = true;}
    if (effectPalette   != seg.palette)   {seg.setPalette(effectPalette);}
    if (effectCurrent   != seg.mode)      {seg.setMode(effectCurrent);}
    uint32_t col0 = RGBW32(colPri[0], colPri[1], colPri[2], colPri[3]);
    uint32_t col1 = RGBW32(colSec[0], colSec[1], colSec[2], colSec[3]);
    if (col0 != seg.colors[0])            {seg.setColor(0, col0);}
    if (col1 != seg.colors[1])            {seg.setColor(1, col1);}
  }
}


void toggleOnOff()
{
  briOld = briT; // briT = 0 when off, briT = bri when on or in between while transitioning, store current value so brightness does not jump when toggling on/off during a transition
  if (bri == 0) {
    bri = briLast;
    strip.setPowerFlag(TRANSITION_POWER_ON | TRANSITION_POWER_TRIGGER);
  } else {
    briLast = bri;
    bri = 0;
    strip.setPowerFlag(TRANSITION_POWER_OFF | TRANSITION_POWER_TRIGGER);
  }
  stateChanged = true; // note: if needed, stateUpdated() will start the global on/off transition
}


//scales the brightness with the briMultiplier factor
byte scaledBri(byte in)
{
  unsigned val = ((unsigned)in*briMultiplier)/100;
  if (val > 255) val = 255;
  return (byte)val;
}


//applies global temporary brightness (briT) to strip
void applyBri() {
  if (realtimeOverride || !(realtimeMode && arlsForceMaxBri))
  {
    DEBUG_PRINTF_P(PSTR("Applying strip brightness: %d (%d,%d)\n"), (int)briT, (int)bri, (int)briOld);
    strip.setBrightness(briT);
  }
}


//applies global brightness and sets it as the "current" brightness (no transition)
void applyFinalBri() {
  briOld = bri;
  briT = bri;
  applyBri();
  strip.trigger(); // force one last update
}

//called after every state changes, schedules interface updates, handles brightness transition and nightlight activation
//unlike colorUpdated(), does NOT apply any colors or FX to segments
void stateUpdated(byte callMode) {
  //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
  //                     6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa 11: ws send only 12: button preset
  setValuesFromFirstSelectedSeg();  // a much better approach would be to use main segment: setValuesFromMainSeg()

  if (bri != briOld || stateChanged) {
    if (stateChanged) currentPreset = 0; //something changed, so we are no longer in the preset

    if (callMode != CALL_MODE_NOTIFICATION && callMode != CALL_MODE_NO_NOTIFY) notify(callMode);
    if (bri != briOld && nodeBroadcastEnabled) sendSysInfoUDP(); // update on state

    //set flag to update ws and mqtt
    interfaceUpdateCallMode = callMode;
  } else {
    if (nightlightActive && !nightlightActiveOld && callMode != CALL_MODE_NOTIFICATION && callMode != CALL_MODE_NO_NOTIFY) {
      notify(CALL_MODE_NIGHTLIGHT);
      interfaceUpdateCallMode = CALL_MODE_NIGHTLIGHT;
    }
  }

  unsigned long now = millis();
  if (callMode != CALL_MODE_NO_NOTIFY && nightlightActive && (nightlightMode == NL_MODE_FADE || nightlightMode == NL_MODE_COLORFADE)) {
    briNlT = bri;
    nightlightDelayMs -= (now - nightlightStartTime);
    nightlightStartTime = now;
  }
  if (briT == 0) {
    if (callMode != CALL_MODE_NOTIFICATION) strip.resetTimebase(); //effect start from beginning
  }

  if (bri > 0) briLast = bri;

  //deactivate nightlight if target brightness is reached
  if (bri == nightlightTargetBri && callMode != CALL_MODE_NO_NOTIFY && nightlightMode != NL_MODE_SUN) nightlightActive = false;

  // notify usermods of state change
  UsermodManager::onStateChange(callMode);

  // global brightness transition handling. Note: power flags are set in toggleOnOff()
  DEBUG_PRINTF_P(PSTR("***********state update: briT: %d bri: %d briOld: %d, isPoweron: %d , isPoweroff %d, trigger: %d\n"), (int)briT, (int)bri, (int)briOld, (int)strip.isPoweringOn(), (int)strip.isPoweringOff(), (int)strip.isPowerTrigger());

  if (strip.getTransition() == 0) {
    jsonTransitionOnce = false;
    transitionActive = false;
    applyFinalBri();
  } else {
    if (strip.isPoweringOff() && strip.isPoweringOn() && blendingStyle != TRANSITION_FADE) {
      // if both flags are set, the power state was reversed during transition, invert the transition time to keep "overall brightness" i.e number of lit LEDs
      // note: segments do the same, timing to finish the transition matches (more or less), segment blending is held in spatial transition until global transition finishes.
      int progress = now - transitionStartTime;
      int duration = strip.getTransition();
      transitionStartTime = now - (duration - progress); // invert transition progress
      if (bri > 0) strip.clearPowerFlag(TRANSITION_POWER_OFF);
      else strip.clearPowerFlag(TRANSITION_POWER_ON);
    }
    else if (strip.isPoweringOn() && strip.isPowerTrigger() || (bri > 0 && briOld == 0)) {
      // global power on from off state either through power button or brightness change
      strip.setPowerFlag(TRANSITION_POWER_ON | TRANSITION_POWER_TRIGGER); // if powering on by brightness change, set power flag to inite spatial transition (if set)
      strip.setTransitionMode(false); // stop any transition that is going on while in off mode and start clean (a segment power on prior to global on will continue otherwise)
      strip.restartRuntime();         // and restart any running effect when powering on
      if (blendingStyle != TRANSITION_FADE) applyFinalBri();; // set brightness immediately, otherwise it will fade-in -> this does not yet work. need to set to bri old? or bri last?
    }
    //TODO: do we need to set briT = briOld when powering off? since we can now fade in parallel, just let it continue?

    if (strip.isPoweringOff() && bri > 0) {
      // powering off but brightness was changed -> switch to powering on, update is handled below
      strip.clearPowerFlag(TRANSITION_POWER_OFF);
      strip.setPowerFlag(TRANSITION_POWER_ON | TRANSITION_POWER_TRIGGER);
      Serial.println("state: brightness change during power off transition, toggling to on transition");
    }

    // if brightness changed, start a new global transition but do not reset the timer if powering off (unless powering back on i.e. triggered)
    // Note: fading is omitted if segments run a spatial power off transition, see handleTransitions()
    if ((bri != briOld && !strip.isPoweringOff()) || strip.isPowerTrigger()) {
      if (transitionActive) {
        briOld = briT; // capture transition value: starts brightness fade from current value
      }
      transitionActive = true;
      transitionStartTime = now; // note: this only affects brightness fade, spatial transition continues as it is handled on segment level
      Serial.println("state: starting global transition, briT: " + String(briT) + " bri: " + String(bri) + " briOld: " + String(briOld));
    }
    if (blendingStyle != TRANSITION_FADE && (strip.isPoweringOn() || strip.isPoweringOff()) && strip.isPowerTrigger()) {
      Serial.println("state: global on/off transition detected, forcing all segments to transition mode");
      strip.setTransitionMode(true); // force all segments to a spatial on/off transition, segments handle transition inversion (on during off or off during on)
    }
    strip.clearPowerFlag(TRANSITION_POWER_TRIGGER);
  }

  stateChanged = false;
}


void updateInterfaces(uint8_t callMode) {
  if (!interfaceUpdateCallMode || millis() - lastInterfaceUpdate < INTERFACE_UPDATE_COOLDOWN) return;

  sendDataWs();
  lastInterfaceUpdate = millis();
  interfaceUpdateCallMode = CALL_MODE_INIT; //disable further updates

  if (callMode == CALL_MODE_WS_SEND) return;

  #ifndef WLED_DISABLE_ALEXA
  if (espalexaDevice != nullptr && callMode != CALL_MODE_ALEXA) {
    espalexaDevice->setValue(bri);
    espalexaDevice->setColor(colPri[0], colPri[1], colPri[2]);
  }
  #endif
  #ifndef WLED_DISABLE_MQTT
  publishMqtt();
  #endif
}

// handle global transitions, for more details on transitions see Segment::startTransition()
void handleTransitions() {
  //handle still pending interface update
  updateInterfaces(interfaceUpdateCallMode);

  // note: the !stateChanged is a workaround: bri is updated async, this code can run before stateUpdated() is called, causing a jump in the fade
  if (transitionActive && strip.getTransition() > 0 && !stateChanged) {
    //Serial.printf("GT: %d, bri: %d, briOld: %d\n", (int)briT, (int)bri, (int)briOld);
    int progress = millis() - transitionStartTime;
    int duration = strip.getTransition();
    // finalize once the transition time has elapsed
    if (progress >= duration) {
      strip.clearPowerFlag(0xFF); // if transition ends, reset all global flags
      // restore (global) transition time if not called from UDP notifier or single/temporary transition from JSON (also playlist)
      if (jsonTransitionOnce) strip.setTransition(transitionDelay);
      transitionActive = false;
      jsonTransitionOnce = false;
      applyFinalBri();
      return;
    }
    // fade global brightness from briOld to bri, skip if powering off using spatial transition (avoid fading in parallel)
    // note: power on sets briOld = bri so it wont fade but still allows global brightness change during that transition
    if (!strip.isPoweringOff() || blendingStyle == TRANSITION_FADE) {
      byte briTO = briT;
      int deltaBri = (int)bri - (int)briOld;
      briT = briOld + (deltaBri * progress / duration);
      if (briTO != briT) applyBri();
    }
  }
}


// legacy method, applies values from col, effectCurrent, ... to selected segments
void colorUpdated(byte callMode) {
  applyValuesToSelectedSegs();
  stateUpdated(callMode);
}


void handleNightlight() {
  unsigned long now = millis();
  if (now < 100 && lastNlUpdate > 0) lastNlUpdate = 0; // take care of millis() rollover
  if (now - lastNlUpdate < 100) return; // allow only 10 NL updates per second
  lastNlUpdate = now;

  if (nightlightActive)
  {
    if (!nightlightActiveOld) //init
    {
      nightlightStartTime = millis();
      nightlightDelayMs = (unsigned)(nightlightDelayMins*60000);
      nightlightActiveOld = true;
      briNlT = bri;
      for (unsigned i=0; i<4; i++) colNlT[i] = colPri[i]; // remember starting color
      if (nightlightMode == NL_MODE_SUN)
      {
        //save current
        colNlT[0] = effectCurrent;
        colNlT[1] = effectSpeed;
        colNlT[2] = effectPalette;

        strip.getFirstSelectedSeg().setMode(FX_MODE_STATIC); // make sure seg runtime is reset if it was in sunrise mode
        effectCurrent = FX_MODE_SUNRISE;            // colorUpdated() will take care of assigning that to all selected segments
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
        for (unsigned i=0; i<4; i++) colPri[i] = colNlT[i]+ ((colSec[i] - colNlT[i])*nper);   // fading from actual color to secondary color
      }
      uint16_t transitionduration = strip.getTransition();
      strip.setTransition(0); // temporary disable transition and set color & brightness directly, (hacky fix for #5620)
      colorUpdated(CALL_MODE_NO_NOTIFY);
      strip.setTransition(transitionduration); // restore transition time to previous value. Note: this needs proper fixing by disabling transitions completely in nightlight mode, reference implementation https://github.com/blazoncek/WLED/commit/c01a6b774969b652c30e383073958302042fd1f9
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
          applyFinalBri();
        }
      }

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
uint32_t get_millisecond_timer() {
  return strip.now;
}
