#include "wled.h"

/**
 * @brief Load color and effect values from the strip's main segment into the global state.
 *
 * Delegates to setValuesFromSegment() using strip.getMainSegmentId() to copy the main
 * segment's primary/secondary colors and effect parameters into the global variables.
 */

void setValuesFromMainSeg()          { setValuesFromSegment(strip.getMainSegmentId()); }
/**
 * @brief Load the current global color/effect values from the first selected segment.
 *
 * Copies the primary/secondary colors and effect parameters (mode, speed, intensity, palette)
 * of the strip's first selected segment into the global state by delegating to
 * setValuesFromSegment with the first selected segment index.
 */
void setValuesFromFirstSelectedSeg() { setValuesFromSegment(strip.getFirstSelectedSegId()); }
/**
 * @brief Load a segment's RGBW colors and effect parameters into the global state.
 *
 * Copies the segment's primary (colors[0]) and secondary (colors[1]) RGBW values
 * into the global colPri and colSec arrays and updates effectCurrent, effectSpeed,
 * effectIntensity, and effectPalette to match the segment.
 *
 * @param s Segment index (0-based) to read from.
 */
void setValuesFromSegment(uint8_t s)
{
  Segment& seg = strip.getSegment(s);
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


// applies global legacy values (col, colSec, effectCurrent...)
/**
 * @brief Apply current global effect and color values to all selected segments.
 *
 * Copies the first selected segment as a baseline and updates every segment
 * that is selected (and active) so its effect parameters and colors match
 * the global values. The first selected segment is included and serves as
 * the comparison reference.
 *
 * Effect fields updated:
 * - speed and intensity are written directly to the segment (and set the
 *   global `stateChanged` flag when changed).
 * - palette and mode are applied via the segment's setter methods.
 * - primary color (from colPri) and secondary color (from colSec) are applied
 *   via the segment color setters.
 */
void applyValuesToSelectedSegs()
{
  // copy of first selected segment to tell if value was updated
  unsigned firstSel = strip.getFirstSelectedSegId();
  Segment selsegPrev = strip.getSegment(firstSel);
  for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
    Segment& seg = strip.getSegment(i);
    if (i != firstSel && (!seg.isActive() || !seg.isSelected())) continue;

    if (effectSpeed     != selsegPrev.speed)     {seg.speed     = effectSpeed;     stateChanged = true;}
    if (effectIntensity != selsegPrev.intensity) {seg.intensity = effectIntensity; stateChanged = true;}
    if (effectPalette   != selsegPrev.palette)   {seg.setPalette(effectPalette);}
    if (effectCurrent   != selsegPrev.mode)      {seg.setMode(effectCurrent);}
    uint32_t col0 = RGBW32(colPri[0], colPri[1], colPri[2], colPri[3]);
    uint32_t col1 = RGBW32(colSec[0], colSec[1], colSec[2], colSec[3]);
    if (col0 != selsegPrev.colors[0])            {seg.setColor(0, col0);}
    if (col1 != selsegPrev.colors[1])            {seg.setColor(1, col1);}
  }
}


void toggleOnOff()
{
  if (bri == 0)
  {
    bri = briLast;
    strip.restartRuntime();
  } else
  {
    briLast = bri;
    bri = 0;
  }
  stateChanged = true;
}


/**
 * @brief Scale a brightness value by the global brightness multiplier.
 *
 * Multiplies the input brightness by the global `briMultiplier` (percent) and clamps
 * the result to the 0–255 range.
 *
 * @param in Input brightness (0–255).
 * @return byte Scaled brightness (0–255) after applying `briMultiplier`.
 */
byte scaledBri(byte in)
{
  unsigned val = ((uint16_t)in*briMultiplier)/100;
  if (val > 255) val = 255;
  return (byte)val;
}


/**
 * @brief Apply the current target brightness to the LED strip.
 *
 * Applies the target brightness value `briT` (after scaling via scaledBri) to the hardware
 * brightness by calling strip.setBrightness(...). If realtimeMode is active and
 * arlsForceMaxBri is true, the call is skipped (brightness is left unchanged).
 *
 * @note This function has the side effect of updating the global strip brightness state.
 */
void applyBri() {
  if (!(realtimeMode && arlsForceMaxBri)) {
    //DEBUG_PRINTF_P(PSTR("Applying strip brightness: %d (%d,%d)\n"), (int)briT, (int)bri, (int)briOld);
    strip.setBrightness(scaledBri(briT));
  }
}


/**
 * @brief Apply the current global brightness immediately and trigger a hardware update.
 *
 * Updates the stored previous brightness and the brightness target to the current
 * brightness, applies the brightness (via applyBri()), and triggers the strip to
 * push the change to hardware.
 *
 * @note This performs no transition — the change is applied immediately.
 *
 * Side effects: updates global variables `briOld` and `briT`, calls applyBri(),
 * and calls strip.trigger().
 */
void applyFinalBri() {
  briOld = bri;
  briT = bri;
  applyBri();
  strip.trigger();
}


//called after every state changes, schedules interface updates, handles brightness transition and nightlight activation
/**
 * @brief Handle global state updates after internal changes (brightness, presets, nightlight, transitions).
 *
 * Updates internal bookkeeping and notifiers after a state change without applying segment colors/effects.
 * - Refreshes global values from the first selected segment.
 * - Sends notifications (unless suppressed), UDP broadcasts, and schedules interface (WS/MQTT/Alexa) updates when needed.
 * - Manages nightlight timing/target bookkeeping and deactivation when target reached.
 * - Invokes Usermods on state changes.
 * - Handles fade transitions: starts transition timing, marks transition state, finalizes and applies final brightness when done.
 * - Applies final brightness and triggers the strip when no active fade transition.
 *
 * @param callMode Indicator of why this update is called. Typical values include:
 *                 CALL_MODE_INIT, CALL_MODE_NOTIFICATION, CALL_MODE_NO_NOTIFY, CALL_MODE_NIGHTLIGHT, etc.;
 *                 controls whether notifications/broadcasts and interface updates are sent.
 */
void stateUpdated(byte callMode) {
  //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
  //                     6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa 11: ws send only 12: button preset
  setValuesFromFirstSelectedSeg();

  if (bri != briOld || stateChanged) {
    if (stateChanged) currentPreset = 0; //something changed, so we are no longer in the preset

    if (callMode != CALL_MODE_NOTIFICATION && callMode != CALL_MODE_NO_NOTIFY) notify(callMode);
    if (bri != briOld && nodeBroadcastEnabled) sendSysInfoUDP(); // update on state

    //set flag to update ws and mqtt
    interfaceUpdateCallMode = callMode;
    stateChanged = false;
  } else {
    if (nightlightActive && !nightlightActiveOld && callMode != CALL_MODE_NOTIFICATION && callMode != CALL_MODE_NO_NOTIFY) {
      notify(CALL_MODE_NIGHTLIGHT);
      interfaceUpdateCallMode = CALL_MODE_NIGHTLIGHT;
    }
  }

  if (callMode != CALL_MODE_NO_NOTIFY && nightlightActive && (nightlightMode == NL_MODE_FADE || nightlightMode == NL_MODE_COLORFADE)) {
    briNlT = bri;
    nightlightDelayMs -= (millis() - nightlightStartTime);
    nightlightStartTime = millis();
  }
  if (briT == 0) {
    if (callMode != CALL_MODE_NOTIFICATION) strip.resetTimebase(); //effect start from beginning
  }

  if (bri > 0) briLast = bri;

  //deactivate nightlight if target brightness is reached
  if (bri == nightlightTargetBri && callMode != CALL_MODE_NO_NOTIFY && nightlightMode != NL_MODE_SUN) nightlightActive = false;

  // notify usermods of state change
  UsermodManager::onStateChange(callMode);

  if (fadeTransition) {
    if (strip.getTransition() == 0) {
      jsonTransitionOnce = false;
      transitionActive = false;
      applyFinalBri();
      strip.trigger();
      return;
    }

    if (transitionActive) {
      briOld = briT;
      tperLast = 0;
    } else
      strip.setTransitionMode(true); // force all segments to transition mode
    transitionActive = true;
    transitionStartTime = millis();
  } else {
    applyFinalBri();
  }
}


/**
 * @brief Send pending interface updates (WebSocket, Alexa, MQTT) when allowed.
 *
 * Checks whether interface updates are enabled and the cooldown has elapsed; if so,
 * sends WebSocket data, records the update time, and disables further updates until
 * re-enabled by the caller. Unless invoked specifically for WebSocket-only sending,
 * also pushes state to Alexa and publishes MQTT if those integrations are enabled.
 *
 * @param callMode Caller context; if set to CALL_MODE_WS_SEND the function will
 * only perform the WebSocket send and return without updating Alexa or MQTT.
 *
 * Side effects:
 * - Calls sendDataWs() and updates lastInterfaceUpdate and interfaceUpdateCallMode.
 * - May call Alexa and MQTT publish functions depending on build flags and arguments.
 */
void updateInterfaces(uint8_t callMode)
{
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


void handleTransitions()
{
  //handle still pending interface update
  updateInterfaces(interfaceUpdateCallMode);

  if (transitionActive && strip.getTransition() > 0) {
    float tper = (millis() - transitionStartTime)/(float)strip.getTransition();
    if (tper >= 1.0f) {
      strip.setTransitionMode(false); // stop all transitions
      // restore (global) transition time if not called from UDP notifier or single/temporary transition from JSON (also playlist)
      if (jsonTransitionOnce) strip.setTransition(transitionDelay);
      transitionActive = false;
      jsonTransitionOnce = false;
      tperLast = 0;
      applyFinalBri();
      return;
    }
    if (tper - tperLast < 0.004f) return; // less than 1 bit change (1/255)
    tperLast = tper;
    briT = briOld + ((bri - briOld) * tper);

    applyBri();
  }
}


/**
 * @brief Apply current global color/effect values to selected segments and propagate the state change.
 *
 * Copies the global color and effect settings into all selected (and active) segments, then runs the post-update
 * housekeeping and interface/notification logic represented by stateUpdated.
 *
 * @param callMode Code indicating the origin/context of the change (used by stateUpdated to decide which interfaces
 *                 and notifications to invoke). */
void colorUpdated(byte callMode) {
  applyValuesToSelectedSegs();
  stateUpdated(callMode);
}


/**
 * @brief Manage the nightlight feature (fade, color fade, sun simulation, and set modes).
 *
 * This routine is the periodic handler that advances or terminates an active nightlight cycle.
 * It is rate-limited to roughly 10 updates per second and handles millis() rollover.
 *
 * Behavior by mode:
 * - NL_MODE_SET: immediately applies the target brightness (when the duration ends).
 * - NL_MODE_FADE: linearly interpolates brightness from the saved start value to the target over the configured delay.
 * - NL_MODE_COLORFADE: like FADE but additionally cross-fades primary color from the saved start color toward the secondary color.
 * - NL_MODE_SUN: simulates sunrise/sunset by switching the segment to a sunrise effect for the configured duration; restores prior effect and toggles the strip off when a sunset completes.
 *
 * Side effects and interactions:
 * - Updates global brightness and primary color (bri, colPri[]), and may update briLast.
 * - On activation, captures the starting brightness and primary color(s) into briNlT and colNlT[].
 * - In SUN mode, saves and restores effect state (effectCurrent, effectSpeed, effectPalette), sets the segment runtime to a sunrise mode, and may call toggleOnOff().
 * - Calls colorUpdated() to propagate color/segment changes, applyFinalBri() when appropriate, and applyPreset(macroNl) when a macro is configured at the end of the nightlight.
 * - Clears nightlightActive and nightlightActiveOld flags when the cycle finishes or is cancelled.
 *
 * This function does not return a value.
 */
void handleNightlight()
{
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

        strip.setMode(strip.getFirstSelectedSegId(), FX_MODE_STATIC); // make sure seg runtime is reset if it was in sunrise mode
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
        for (unsigned i=0; i<4; i++) colPri[i] = colNlT[i]+ ((colSec[i] - colNlT[i])*nper);   // fading from actual color to secondary color
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
uint32_t get_millisecond_timer()
{
  return strip.now;
}
