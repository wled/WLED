#include "wled.h"

/*
 * Physical IO
 */

#define WLED_DEBOUNCE_THRESHOLD      50 // only consider button input of at least 50ms as valid (debouncing)
#define WLED_LONG_PRESS             600 // long press if button is released after held for at least 600ms
#define WLED_DOUBLE_PRESS           350 // double press if another press within 350ms after a short press
#define WLED_LONG_REPEATED_ACTION   300 // how often a repeated action (e.g. dimming) is fired on long press on button IDs >0
#define WLED_LONG_AP               5000 // how long button 0 needs to be held to activate WLED-AP
#define WLED_LONG_FACTORY_RESET   10000 // how long button 0 needs to be held to trigger a factory reset

static const char _mqtt_topic_button[] PROGMEM = "%s/button/%d";  // optimize flash usage

void shortPressAction(uint8_t b)
{
  if (!macroButton[b]) {
    switch (b) {
      case 0: toggleOnOff(); stateUpdated(CALL_MODE_BUTTON); break;
      case 1: ++effectCurrent %= strip.getModeCount(); stateChanged = true; colorUpdated(CALL_MODE_BUTTON); break;
    }
  } else {
    unloadPlaylist(); // applying a preset unloads the playlist
    applyPreset(macroButton[b], CALL_MODE_BUTTON_PRESET);
  }

#ifndef WLED_DISABLE_MQTT
  // publish MQTT message
  if (buttonPublishMqtt && WLED_MQTT_CONNECTED) {
    char subuf[64];
    sprintf_P(subuf, _mqtt_topic_button, mqttDeviceTopic, (int)b);
    mqtt->publish(subuf, 0, false, "short");
  }
#endif
}

void longPressAction(uint8_t b)
{
  if (!macroLongPress[b]) {
    switch (b) {
      case 0: setRandomColor(col); colorUpdated(CALL_MODE_BUTTON); break;
      case 1: bri += 8; stateUpdated(CALL_MODE_BUTTON); buttonPressedTime[b] = millis(); break; // repeatable action
    }
  } else {
    unloadPlaylist(); // applying a preset unloads the playlist
    applyPreset(macroLongPress[b], CALL_MODE_BUTTON_PRESET);
  }

#ifndef WLED_DISABLE_MQTT
  // publish MQTT message
  if (buttonPublishMqtt && WLED_MQTT_CONNECTED) {
    char subuf[64];
    sprintf_P(subuf, _mqtt_topic_button, mqttDeviceTopic, (int)b);
    mqtt->publish(subuf, 0, false, "long");
  }
#endif
}

void doublePressAction(uint8_t b)
{
  if (!macroDoublePress[b]) {
    switch (b) {
      //case 0: toggleOnOff(); colorUpdated(CALL_MODE_BUTTON); break; //instant short press on button 0 if no macro set
      case 1: ++effectPalette %= strip.getPaletteCount(); colorUpdated(CALL_MODE_BUTTON); break;
    }
  } else {
    unloadPlaylist(); // applying a preset unloads the playlist
    applyPreset(macroDoublePress[b], CALL_MODE_BUTTON_PRESET);
  }

#ifndef WLED_DISABLE_MQTT
  // publish MQTT message
  if (buttonPublishMqtt && WLED_MQTT_CONNECTED) {
    char subuf[64];
    sprintf_P(subuf, _mqtt_topic_button, mqttDeviceTopic, (int)b);
    mqtt->publish(subuf, 0, false, "double");
  }
#endif
}

bool isButtonPressed(uint8_t i)
{
  if (btnPin[i]<0) return false;
  uint8_t pin = btnPin[i];

  switch (buttonType[i]) {
    case BTN_TYPE_NONE:
    case BTN_TYPE_RESERVED:
      break;
    case BTN_TYPE_PUSH:
    case BTN_TYPE_SWITCH:
      if (digitalRead(pin) == LOW) return true;
      break;
    case BTN_TYPE_PUSH_ACT_HIGH:
    case BTN_TYPE_PIR_SENSOR:
      if (digitalRead(pin) == HIGH) return true;
      break;
    case BTN_TYPE_TOUCH:
      #if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
      if (touchRead(pin) <= touchThreshold) return true;
      #endif
      break;
  }
  return false;
}

void handleSwitch(uint8_t b)
{
  // isButtonPressed() handles inverted/noninverted logic
  if (buttonPressedBefore[b] != isButtonPressed(b)) {
    buttonPressedTime[b] = millis();
    buttonPressedBefore[b] = !buttonPressedBefore[b];
  }

  if (buttonLongPressed[b] == buttonPressedBefore[b]) return;

  if (millis() - buttonPressedTime[b] > WLED_DEBOUNCE_THRESHOLD) { //fire edge event only after 50ms without change (debounce)
    if (!buttonPressedBefore[b]) { // on -> off
      if (macroButton[b]) applyPreset(macroButton[b], CALL_MODE_BUTTON_PRESET);
      else { //turn on
        if (!bri) {toggleOnOff(); stateUpdated(CALL_MODE_BUTTON);}
      }
    } else {  // off -> on
      if (macroLongPress[b]) applyPreset(macroLongPress[b], CALL_MODE_BUTTON_PRESET);
      else { //turn off
        if (bri) {toggleOnOff(); stateUpdated(CALL_MODE_BUTTON);}
      }
    }

#ifndef WLED_DISABLE_MQTT
    // publish MQTT message
    if (buttonPublishMqtt && WLED_MQTT_CONNECTED) {
      char subuf[64];
      if (buttonType[b] == BTN_TYPE_PIR_SENSOR) sprintf_P(subuf, PSTR("%s/motion/%d"), mqttDeviceTopic, (int)b);
      else sprintf_P(subuf, _mqtt_topic_button, mqttDeviceTopic, (int)b);
      mqtt->publish(subuf, 0, false, !buttonPressedBefore[b] ? "off" : "on");
    }
#endif

    buttonLongPressed[b] = buttonPressedBefore[b]; //save the last "long term" switch state
  }
}

#define ANALOG_BTN_READ_CYCLE 250   // min time between two analog reading cycles
#define STRIP_WAIT_TIME 6           // max wait time in case of strip.isUpdating()
#define POT_SMOOTHING 0.25f         // smoothing factor for raw potentiometer readings
#define POT_SENSITIVITY 4           // changes below this amount are noise (POT scratching, or ADC noise)

void handleAnalog(uint8_t b)
{
  static uint8_t oldRead[WLED_MAX_BUTTONS] = {0};
  static float filteredReading[WLED_MAX_BUTTONS] = {0.0f};
  uint16_t rawReading;    // raw value from analogRead, scaled to 12bit

  #ifdef ESP8266
  rawReading = analogRead(A0) << 2;   // convert 10bit read to 12bit
  #else
  if ((btnPin[b] < 0) || (digitalPinToAnalogChannel(btnPin[b]) < 0)) return; // pin must support analog ADC - newer esp32 frameworks throw lots of warnings otherwise
  rawReading = analogRead(btnPin[b]); // collect at full 12bit resolution
  #endif
  yield();                            // keep WiFi task running - analog read may take several millis on ESP8266

  filteredReading[b] += POT_SMOOTHING * ((float(rawReading) / 16.0f) - filteredReading[b]); // filter raw input, and scale to [0..255]
  uint16_t aRead = max(min(int(filteredReading[b]), 255), 0);                               // squash into 8bit
  if(aRead <= POT_SENSITIVITY) aRead = 0;                                                   // make sure that 0 and 255 are used
  if(aRead >= 255-POT_SENSITIVITY) aRead = 255;

  if (buttonType[b] == BTN_TYPE_ANALOG_INVERTED) aRead = 255 - aRead;

  // remove noise & reduce frequency of UI updates
  if (abs(int(aRead) - int(oldRead[b])) <= POT_SENSITIVITY) return;  // no significant change in reading

  // Unomment the next lines if you still see flickering related to potentiometer
  // This waits until strip finishes updating (why: strip was not updating at the start of handleButton() but may have started during analogRead()?)
  //unsigned long wait_started = millis();
  //while(strip.isUpdating() && (millis() - wait_started < STRIP_WAIT_TIME)) {
  //  delay(1);
  //}
  //if (strip.isUpdating()) return; // give up

  oldRead[b] = aRead;

  // if no macro for "short press" and "long press" is defined use brightness control
  if (!macroButton[b] && !macroLongPress[b]) {
    // if "double press" macro defines which option to change
    if (macroDoublePress[b] >= 250) {
      // global brightness
      if (aRead == 0) {
        briLast = bri;
        bri = 0;
      } else {
        bri = aRead;
      }
    } else if (macroDoublePress[b] == 249) {
      // effect speed
      effectSpeed = aRead;
    } else if (macroDoublePress[b] == 248) {
      // effect intensity
      effectIntensity = aRead;
    } else if (macroDoublePress[b] == 247) {
      // selected palette
      effectPalette = map(aRead, 0, 252, 0, strip.getPaletteCount()-1);
      effectPalette = constrain(effectPalette, 0, strip.getPaletteCount()-1);  // map is allowed to "overshoot", so we need to contrain the result
    } else if (macroDoublePress[b] == 200) {
      // primary color, hue, full saturation
      colorHStoRGB(aRead*256,255,col);
    } else {
      // otherwise use "double press" for segment selection
      Segment& seg = strip.getSegment(macroDoublePress[b]);
      if (aRead == 0) {
        seg.setOption(SEG_OPTION_ON, false); // off (use transition)
      } else {
        seg.setOpacity(aRead);
        seg.setOption(SEG_OPTION_ON, true); // on (use transition)
      }
      // this will notify clients of update (websockets,mqtt,etc)
      updateInterfaces(CALL_MODE_BUTTON);
    }
  } else {
    //TODO:
    // we can either trigger a preset depending on the level (between short and long entries)
    // or use it for RGBW direct control
  }
  colorUpdated(CALL_MODE_BUTTON);
}

void handleButton()
{
  static unsigned long lastRead = 0UL;
  static unsigned long lastRun = 0UL;
  unsigned long now = millis();

  if (strip.isUpdating() && (now - lastRun < 400)) return; // don't interfere with strip update (unless strip is updating continuously, e.g. very long strips)
  lastRun = now;

  for (uint8_t b=0; b<WLED_MAX_BUTTONS; b++) {
    #ifdef ESP8266
    if ((btnPin[b]<0 && !(buttonType[b] == BTN_TYPE_ANALOG || buttonType[b] == BTN_TYPE_ANALOG_INVERTED)) || buttonType[b] == BTN_TYPE_NONE) continue;
    #else
    if (btnPin[b]<0 || buttonType[b] == BTN_TYPE_NONE) continue;
    #endif

    if (usermods.handleButton(b)) continue; // did usermod handle buttons

    if (buttonType[b] == BTN_TYPE_ANALOG || buttonType[b] == BTN_TYPE_ANALOG_INVERTED) { // button is not a button but a potentiometer
      if (now - lastRead > ANALOG_BTN_READ_CYCLE) {
        handleAnalog(b);
        lastRead = now;
      }
      continue;
    }

    // button is not momentary, but switch. This is only suitable on pins whose on-boot state does not matter (NOT gpio0)
    if (buttonType[b] == BTN_TYPE_SWITCH || buttonType[b] == BTN_TYPE_PIR_SENSOR) {
      handleSwitch(b);
      continue;
    }

    // momentary button logic
    if (isButtonPressed(b)) { // pressed

      // if all macros are the same, fire action immediately on rising edge
      if (macroButton[b] && macroButton[b] == macroLongPress[b] && macroButton[b] == macroDoublePress[b]) {
        if (!buttonPressedBefore[b])
          shortPressAction(b);
        buttonPressedBefore[b] = true;
        buttonPressedTime[b] = now; // continually update (for debouncing to work in release handler)
        continue;
      }

      if (!buttonPressedBefore[b]) buttonPressedTime[b] = now;
      buttonPressedBefore[b] = true;

      if (now - buttonPressedTime[b] > WLED_LONG_PRESS) { //long press
        if (!buttonLongPressed[b]) longPressAction(b);
        else if (b) { //repeatable action (~3 times per s) on button > 0
          longPressAction(b);
          buttonPressedTime[b] = now - WLED_LONG_REPEATED_ACTION; //333ms
        }
        buttonLongPressed[b] = true;
      }

    } else if (!isButtonPressed(b) && buttonPressedBefore[b]) { //released
      long dur = now - buttonPressedTime[b];

      // released after rising-edge short press action
      if (macroButton[b] && macroButton[b] == macroLongPress[b] && macroButton[b] == macroDoublePress[b]) {
        if (dur > WLED_DEBOUNCE_THRESHOLD) buttonPressedBefore[b] = false; // debounce, blocks button for 50 ms once it has been released
        continue;
      }

      if (dur < WLED_DEBOUNCE_THRESHOLD) {buttonPressedBefore[b] = false; continue;} // too short "press", debounce
      bool doublePress = buttonWaitTime[b]; //did we have a short press before?
      buttonWaitTime[b] = 0;

      if (b == 0 && dur > WLED_LONG_AP) { // long press on button 0 (when released)
        if (dur > WLED_LONG_FACTORY_RESET) { // factory reset if pressed > 10 seconds
          WLED_FS.format();
          #ifdef WLED_ADD_EEPROM_SUPPORT
          clearEEPROM();
          #endif
          doReboot = true;
        } else {
          WLED::instance().initAP(true);
        }
      } else if (!buttonLongPressed[b]) { //short press
        //NOTE: this interferes with double click handling in usermods so usermod needs to implement full button handling
        if (b != 1 && !macroDoublePress[b]) { //don't wait for double press on buttons without a default action if no double press macro set
          shortPressAction(b);
        } else { //double press if less than 350 ms between current press and previous short press release (buttonWaitTime!=0)
          if (doublePress) {
            doublePressAction(b);
          } else {
            buttonWaitTime[b] = now;
          }
        }
      }
      buttonPressedBefore[b] = false;
      buttonLongPressed[b] = false;
    }

    //if 350ms elapsed since last short press release it is a short press
    if (buttonWaitTime[b] && now - buttonWaitTime[b] > WLED_DOUBLE_PRESS && !buttonPressedBefore[b]) {
      buttonWaitTime[b] = 0;
      shortPressAction(b);
    }
  }
}

// If enabled, RMT idle level is set to HIGH when off
// to prevent leakage current when using an N-channel MOSFET to toggle LED power
#ifdef ESP32_DATA_IDLE_HIGH
void esp32RMTInvertIdle()
{
  bool idle_out;
  for (uint8_t u = 0; u < busses.getNumBusses(); u++)
  {
    if (u > 7) return; // only 8 RMT channels, TODO: ESP32 variants have less RMT channels
    Bus *bus = busses.getBus(u);
    if (!bus || bus->getLength()==0 || !IS_DIGITAL(bus->getType()) || IS_2PIN(bus->getType())) continue;
    //assumes that bus number to rmt channel mapping stays 1:1
    rmt_channel_t ch = static_cast<rmt_channel_t>(u);
    rmt_idle_level_t lvl;
    rmt_get_idle_level(ch, &idle_out, &lvl);
    if (lvl == RMT_IDLE_LEVEL_HIGH) lvl = RMT_IDLE_LEVEL_LOW;
    else if (lvl == RMT_IDLE_LEVEL_LOW) lvl = RMT_IDLE_LEVEL_HIGH;
    else continue;
    rmt_set_idle_level(ch, idle_out, lvl);
  }
}
#endif

void handleIO()
{
  handleButton();

  //set relay when LEDs turn on
  if (strip.getBrightness())
  {
    lastOnTime = millis();
    if (offMode)
    {
      #ifdef ESP32_DATA_IDLE_HIGH
      esp32RMTInvertIdle();
      #endif
      if (rlyPin>=0) {
        pinMode(rlyPin, OUTPUT);
        digitalWrite(rlyPin, rlyMde);
      }
      offMode = false;
    }
  } else if (millis() - lastOnTime > 600)
  {
    if (!offMode) {
      #ifdef ESP8266
      // turn off built-in LED if strip is turned off
      // this will break digital bus so will need to be reinitialised on On
      PinOwner ledPinOwner = pinManager.getPinOwner(LED_BUILTIN);
      if (!strip.isOffRefreshRequired() && (ledPinOwner == PinOwner::None || ledPinOwner == PinOwner::BusDigital)) {
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, HIGH);
      }
      #endif
      #ifdef ESP32_DATA_IDLE_HIGH
      esp32RMTInvertIdle();
      #endif
      if (rlyPin>=0) {
        pinMode(rlyPin, OUTPUT);
        digitalWrite(rlyPin, !rlyMde);
      }
    }
    offMode = true;
  }
}