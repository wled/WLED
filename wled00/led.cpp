#include "wled.h"

/*
 * LED methods
 */

// Runtime state private to this file - previously WLED_GLOBAL, a leftover from
// when all state lived in one big extern block regardless of who used it.
static bool          transitionActive = false;
static unsigned long transitionStartTime;
static unsigned long lastNlUpdate;
static byte           briNlT = 0;              // current nightlight brightness
static byte           colNlT[] = { 0, 0, 0, 0 }; // current nightlight color

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
  briOld16 = briT16; // adopt current brightness so toggling during a transition does not jump
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
uint16_t scaledBri16(uint16_t in)
{
  uint32_t val = ((uint32_t)in * briMultiplier) / 100;
  if (val > (255U << 8)) val = 255U << 8;
  return (uint16_t)val;
}


//applies global temporary brightness (briT16) to strip
void applyBri() {
  if (realtimeOverride || !(realtimeMode && arlsForceMaxBri))
    strip.setBrightness16(briT16);
}


//applies global brightness and sets it as the "current" brightness (no transition)
void applyFinalBri() {
  briOld16 = (uint16_t)bri << 8;
  briT16 = briOld16;
  applyBri();
  strip.trigger(); // force one last update
}

// local function to handle global brightness transition, called from stateUpdated(). Note: power flags are set in toggleOnOff()
void handleBriChange() {
  //DEBUG_PRINTF_P(PSTR("state update: briT16: %d bri: %d briOld16: %d, isPoweron: %d , isPoweroff %d, trigger: %d\n"), (int)briT16, (int)bri, (int)briOld16, (int)strip.isPoweringOn(), (int)strip.isPoweringOff(), (int)strip.isPowerTrigger());
  if (strip.getTransition() == 0) {
    jsonTransitionOnce = false;
    transitionActive = false;
    strip.clearPowerFlag(0xFF); // clear all power flags
    applyFinalBri();
  } else {
    uint32_t now = millis();

    if ((!strip.isPoweringOn() && bri > 0 && (briOld16 == 0 || strip.isPoweringOff()))) {
      // Brightness was increased from zero; treat as power on if we're not already
      strip.setPowerFlag(TRANSITION_POWER_ON | TRANSITION_POWER_TRIGGER);
    } else if (!strip.isPoweringOff() && bri == 0 && (briOld16 > 0 || strip.isPoweringOn())) {
      // Brightness was decreased to zero; treat as power off if we're not already
      strip.setPowerFlag(TRANSITION_POWER_OFF | TRANSITION_POWER_TRIGGER);
    }

    // Was there a power state change?
    if (strip.isPowerTrigger()) {
      if (strip.isPoweringOff() && strip.isPoweringOn()) {
        // if both flags are set, the power state was reversed during transition.  We invert the transition.
        // note: segments do the same, timing to finish the transition matches (more or less), segment blending is held in spatial transition until global transition finishes.
        int duration = strip.getTransition();
        int elapsed  = constrain((int)(now - transitionStartTime), 0, duration); // duration may have changed mid-flight
        int progress = duration - elapsed;
        const int32_t targetBri = (uint16_t)bri << 8;
        int32_t delta = (bri > 0 || blendingStyle == TRANSITION_FADE) ? (int32_t)briT16 - targetBri : 0; // spatial power-off doesn't fade
        if (delta) {
          int32_t room = (delta > 0) ? (255U << 8) - targetBri : targetBri; // headroom for the fade start on briT's side of bri
          int32_t absDelta = delta > 0 ? delta : -delta;
          elapsed = max(elapsed, (int)((duration * (int64_t)absDelta + room - 1) / room)); // target moved: stretch so the start stays in range
        }
        if (elapsed) briOld16 = targetBri + (int32_t)((int64_t)delta * duration / elapsed); // extrapolate from bri back through briT
        transitionStartTime = now - (duration - elapsed); // invert progress
        strip.clearPowerFlag((bri > 0) ? TRANSITION_POWER_OFF : TRANSITION_POWER_ON); // bri carries the target state
      }
      else
      {
        // Starting a power transition
        if (strip.isPoweringOn()) {
          // global power on
          strip.setTransitionMode(false); // stop any transition that is going on while in off mode and start clean (a segment power on prior to global on will continue otherwise)
          strip.restartRuntime();         // and restart any running effect when powering on
          if (blendingStyle != TRANSITION_FADE) applyFinalBri(); // set brightness immediately, otherwise it will fade-in -> this does not yet work. need to set to bri old? or bri last?
        }
        briOld16 = briT16;  // Adopt the current state of any existing fade transition
        transitionStartTime = now;  // (Re)start global transition timer
      }

      if (blendingStyle != TRANSITION_FADE) { // Note: fading is omitted if segments run a spatial power off transition, see handleTransitions()
        strip.setTransitionMode(true); // force all segments to a spatial on/off transition, segments handle transition inversion (on during off or off during on)
      }

      strip.clearPowerFlag(TRANSITION_POWER_TRIGGER);
    } else {
      if (((uint16_t)bri << 8) == briOld16) return; // no change found

      // Purely a brightness change.
      // Strictly speaking someone might have set brightness 0->0 so we have to ignore that case during power off.
      if (!strip.isPoweringOff()) {
        // Any existing transition re-starts from where we are
        briOld16 = briT16;
        transitionStartTime = now;
      }
    }

    // In all cases that arrive here, there is an active transition now
    transitionActive = true;
  }
}

//called after every state changes, schedules interface updates, handles brightness transition and nightlight activation
//unlike colorUpdated(), does NOT apply any colors or FX to segments
void stateUpdated(byte callMode) {
  //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
  //                     6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa 11: ws send only 12: button preset
  setValuesFromFirstSelectedSeg();  // a much better approach would be to use main segment: setValuesFromMainSeg()

  if (((uint16_t)bri << 8) != briOld16 || stateChanged) {
    if (stateChanged) currentPreset = 0; //something changed, so we are no longer in the preset

    if (callMode != CALL_MODE_NOTIFICATION && callMode != CALL_MODE_NO_NOTIFY) notify(callMode);
    if (((uint16_t)bri << 8) != briOld16 && nodeBroadcastEnabled) sendSysInfoUDP(); // update on state

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
  if (briT16 == 0) {
    if (callMode != CALL_MODE_NOTIFICATION) strip.resetTimebase(); //effect start from beginning
  }

  if (bri > 0) briLast = bri;

  //deactivate nightlight if target brightness is reached
  if (bri == nightlightTargetBri && callMode != CALL_MODE_NO_NOTIFY && nightlightMode != NL_MODE_SUN) nightlightActive = false;

  // notify usermods of state change
  UsermodManager::onStateChange(callMode);

  handleBriChange(); // check if a global brightness changed and start/update transition if needed

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
    // fade global brightness from briOld16 to bri, skip if powering off using spatial transition (avoid fading in parallel)
    // note: power on starts at the target brightness, but a later brightness change can still fade
    if (!strip.isPoweringOff() || blendingStyle == TRANSITION_FADE) {
      uint16_t briTO = briT16;
      int32_t deltaBri = (int32_t)((uint16_t)bri << 8) - (int32_t)briOld16;
      briT16 = (uint16_t)((int32_t)briOld16 + ((int64_t)deltaBri * progress / duration));
      if (briTO != briT16) applyBri();
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
