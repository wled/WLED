#pragma once

#include "wled.h"

//
// Inspired by the original v2 usermods
// * usermod_v2_rotaty_encoder_ui
//
// v2 usermod that provides a rotary encoder-based UI.
//
// This usermod allows you to control:
// 
// * Brightness
// * Selected Effect
// * Effect Speed
// * Effect Intensity
// * Palette
//
// Change between modes by pressing a button.
//
// Dependencies
// * This Usermod works best coupled with 
//   FourLineDisplayUsermod.
//
// If FourLineDisplayUsermod is used the folowing options are also enabled
//
// * main color
// * saturation of main color
// * display network (long press buttion)
//

#ifdef USERMOD_MODE_SORT
  #error "Usermod Mode Sort is no longer required. Remove -D USERMOD_MODE_SORT from platformio.ini"
#endif

#ifndef ENCODER_DT_PIN
#define ENCODER_DT_PIN 18
#endif

#ifndef ENCODER_CLK_PIN
#define ENCODER_CLK_PIN 5
#endif

#ifndef ENCODER_SW_PIN
#define ENCODER_SW_PIN 19
#endif

// The last UI state, remove color and saturation option if diplay not active(too many options)
#ifdef USERMOD_FOUR_LINE_DISPLAY
 #define LAST_UI_STATE 8
#else
 #define LAST_UI_STATE 4
#endif

// Number of modes at the start of the list to not sort
#define MODE_SORT_SKIP_COUNT 1

// Which list is being sorted
static char **listBeingSorted;

/**
 * Modes and palettes are stored as strings that
 * end in a quote character. Compare two of them.
 * We are comparing directly within either
 * JSON_mode_names or JSON_palette_names.
 */
static int re_qstringCmp(const void *ap, const void *bp) {
  char *a = listBeingSorted[*((byte *)ap)];
  char *b = listBeingSorted[*((byte *)bp)];
  int i = 0;
  do {
    char aVal = pgm_read_byte_near(a + i);
    if (aVal >= 97 && aVal <= 122) {
      // Lowercase
      aVal -= 32;
    }
    char bVal = pgm_read_byte_near(b + i);
    if (bVal >= 97 && bVal <= 122) {
      // Lowercase
      bVal -= 32;
    }
    // Relly we shouldn't ever get to '\0'
    if (aVal == '"' || bVal == '"' || aVal == '\0' || bVal == '\0') {
      // We're done. one is a substring of the other
      // or something happenend and the quote didn't stop us.
      if (aVal == bVal) {
        // Same value, probably shouldn't happen
        // with this dataset
        return 0;
      }
      else if (aVal == '"' || aVal == '\0') {
        return -1;
      }
      else {
        return 1;
      }
    }
    if (aVal == bVal) {
      // Same characters. Move to the next.
      i++;
      continue;
    }
    // We're done
    if (aVal < bVal) {
      return -1;
    }
    else {
      return 1;
    }
  } while (true);
  // We shouldn't get here.
  return 0;
}


class RotaryEncoderUIUsermod : public Usermod {
private:
  int8_t fadeAmount = 5;              // Amount to change every step (brightness)
  unsigned long loopTime;

  unsigned long buttonPressedTime = 0;
  unsigned long buttonWaitTime = 0;
  bool buttonPressedBefore = false;
  bool buttonLongPressed = false;

  int8_t pinA = ENCODER_DT_PIN;       // DT from encoder
  int8_t pinB = ENCODER_CLK_PIN;      // CLK from encoder
  int8_t pinC = ENCODER_SW_PIN;       // SW from encoder

  unsigned char select_state = 0;     // 0: brightness, 1: effect, 2: effect speed, ...

  uint16_t currentHue1 = 16; // default boot color
  byte currentSat1 = 255;
  
#ifdef USERMOD_FOUR_LINE_DISPLAY
  FourLineDisplayUsermod *display;
#else
  void* display = nullptr;
#endif

  // Pointers the start of the mode names within JSON_mode_names
  char **modes_qstrings = nullptr;

  // Array of mode indexes in alphabetical order.
  byte *modes_alpha_indexes = nullptr;

  // Pointers the start of the palette names within JSON_palette_names
  char **palettes_qstrings = nullptr;

  // Array of palette indexes in alphabetical order.
  byte *palettes_alpha_indexes = nullptr;

  unsigned char Enc_A;
  unsigned char Enc_B;
  unsigned char Enc_A_prev = 0;

  bool currentEffectAndPaletteInitialized = false;
  uint8_t effectCurrentIndex = 0;
  uint8_t effectPaletteIndex = 0;
  uint8_t knownMode = 0;
  uint8_t knownPalette = 0;

  uint8_t currentCCT = 128;
  bool isRgbw = false;

  byte presetHigh = 0;
  byte presetLow = 0;

  bool applyToAll = true;

  bool initDone = false;
  bool enabled = true;

  // strings to reduce flash memory usage (used more than twice)
  static const char _name[];
  static const char _enabled[];
  static const char _DT_pin[];
  static const char _CLK_pin[];
  static const char _SW_pin[];
  static const char _presetHigh[];
  static const char _presetLow[];
  static const char _applyToAll[];

  /**
   * Sort the modes and palettes to the index arrays
   * modes_alpha_indexes and palettes_alpha_indexes.
   */
  void sortModesAndPalettes() {
    modes_qstrings = re_findModeStrings(JSON_mode_names, strip.getModeCount());
    modes_alpha_indexes = re_initIndexArray(strip.getModeCount());
    re_sortModes(modes_qstrings, modes_alpha_indexes, strip.getModeCount(), MODE_SORT_SKIP_COUNT);

    palettes_qstrings = re_findModeStrings(JSON_palette_names, strip.getPaletteCount());
    palettes_alpha_indexes = re_initIndexArray(strip.getPaletteCount());

    // How many palette names start with '*' and should not be sorted?
    // (Also skipping the first one, 'Default').
    int skipPaletteCount = 1;
    while (pgm_read_byte_near(palettes_qstrings[skipPaletteCount++]) == '*') ;
    re_sortModes(palettes_qstrings, palettes_alpha_indexes, strip.getPaletteCount(), skipPaletteCount);
  }

  byte *re_initIndexArray(int numModes) {
    byte *indexes = (byte *)malloc(sizeof(byte) * numModes);
    for (byte i = 0; i < numModes; i++) {
      indexes[i] = i;
    }
    return indexes;
  }

  /**
   * Return an array of mode or palette names from the JSON string.
   * They don't end in '\0', they end in '"'. 
   */
  char **re_findModeStrings(const char json[], int numModes) {
    char **modeStrings = (char **)malloc(sizeof(char *) * numModes);
    uint8_t modeIndex = 0;
    bool insideQuotes = false;
    // advance past the mark for markLineNum that may exist.
    char singleJsonSymbol;

    // Find the mode name in JSON
    bool complete = false;
    for (size_t i = 0; i < strlen_P(json); i++) {
      singleJsonSymbol = pgm_read_byte_near(json + i);
      if (singleJsonSymbol == '\0') break;
      switch (singleJsonSymbol) {
        case '"':
          insideQuotes = !insideQuotes;
          if (insideQuotes) {
            // We have a new mode or palette
            modeStrings[modeIndex] = (char *)(json + i + 1);
          }
          break;
        case '[':
          break;
        case ']':
          if (!insideQuotes) complete = true;
          break;
        case ',':
          if (!insideQuotes) modeIndex++;
        default:
          if (!insideQuotes) break;
      }
      if (complete) break;
    }
    return modeStrings;
  }

  /**
   * Sort either the modes or the palettes using quicksort.
   */
  void re_sortModes(char **modeNames, byte *indexes, int count, int numSkip) {
    listBeingSorted = modeNames;
    qsort(indexes + numSkip, count - numSkip, sizeof(byte), re_qstringCmp);
    listBeingSorted = nullptr;
  }


public:
  /*
   * setup() is called once at boot. WiFi is not yet connected at this point.
   * You can use it to initialize variables, sensors or similar.
   */
  void setup()
  {
    DEBUG_PRINTLN(F("Usermod Rotary Encoder init."));
    PinManagerPinType pins[3] = { { pinA, false }, { pinB, false }, { pinC, false } };
    if (!pinManager.allocateMultiplePins(pins, 3, PinOwner::UM_RotaryEncoderUI)) {
      // BUG: configuring this usermod with conflicting pins
      //      will cause it to de-allocate pins it does not own
      //      (at second config)
      //      This is the exact type of bug solved by pinManager
      //      tracking the owner tags....
      pinA = pinB = pinC = -1;
      enabled = false;
      return;
    }

    pinMode(pinA, INPUT_PULLUP);
    pinMode(pinB, INPUT_PULLUP);
    pinMode(pinC, INPUT_PULLUP);
    loopTime = millis();

    for (uint8_t s = 0; s < busses.getNumBusses(); s++) {
      Bus *bus = busses.getBus(s);
      if (!bus || bus->getLength()==0) break;
      isRgbw |= bus->isRgbw();
    }

    currentCCT = (approximateKelvinFromRGB(RGBW32(col[0], col[1], col[2], col[3])) - 1900) >> 5;

    if (!initDone) sortModesAndPalettes();

#ifdef USERMOD_FOUR_LINE_DISPLAY    
    // This Usermod uses FourLineDisplayUsermod for the best experience.
    // But it's optional. But you want it.
    display = (FourLineDisplayUsermod*) usermods.lookup(USERMOD_ID_FOUR_LINE_DISP);
    if (display != nullptr) {
      display->setMarkLine(1, 0);
    }
#endif

    initDone = true;
    Enc_A = digitalRead(pinA); // Read encoder pins
    Enc_B = digitalRead(pinB);
    Enc_A_prev = Enc_A;
  }

  /*
   * connected() is called every time the WiFi is (re)connected
   * Use it to initialize network interfaces
   */
  void connected()
  {
    //Serial.println("Connected to WiFi!");
  }

  /*
   * loop() is called continuously. Here you can check for events, read sensors, etc.
   * 
   * Tips:
   * 1. You can use "if (WLED_CONNECTED)" to check for a successful network connection.
   *    Additionally, "if (WLED_MQTT_CONNECTED)" is available to check for a connection to an MQTT broker.
   * 
   * 2. Try to avoid using the delay() function. NEVER use delays longer than 10 milliseconds.
   *    Instead, use a timer check as shown here.
   */
  void loop()
  {
    if (!enabled || strip.isUpdating()) return;
    unsigned long currentTime = millis(); // get the current elapsed time

    // Initialize effectCurrentIndex and effectPaletteIndex to
    // current state. We do it here as (at least) effectCurrent
    // is not yet initialized when setup is called.
    
    if (!currentEffectAndPaletteInitialized) {
      findCurrentEffectAndPalette();
    }

    if (modes_alpha_indexes[effectCurrentIndex] != effectCurrent || palettes_alpha_indexes[effectPaletteIndex] != effectPalette) {
      currentEffectAndPaletteInitialized = false;
    }

    if (currentTime >= (loopTime + 2)) // 2ms since last check of encoder = 500Hz
    {
      loopTime = currentTime; // Updates loopTime

      bool buttonPressed = !digitalRead(pinC); //0=pressed, 1=released
      if (buttonPressed) {
        if (!buttonPressedBefore) buttonPressedTime = currentTime;
        buttonPressedBefore = true;
        if (currentTime-buttonPressedTime > 3000) {
          if (!buttonLongPressed) displayNetworkInfo(); //long press for network info
          buttonLongPressed = true;
        }
      } else if (!buttonPressed && buttonPressedBefore) {
        bool doublePress = buttonWaitTime;
        buttonWaitTime = 0;
        if (!buttonLongPressed) {
          if (doublePress) {
            toggleOnOff();
            lampUdated();
          } else {
            buttonWaitTime = currentTime;
          }
        }
        buttonLongPressed = false;
        buttonPressedBefore = false;
      }
      if (buttonWaitTime && currentTime-buttonWaitTime>350 && !buttonPressedBefore) { //same speed as in button.cpp
        buttonWaitTime = 0;
        char newState = select_state + 1;
        bool changedState = true;
        if (newState > LAST_UI_STATE || (newState == 8 && presetHigh==0 && presetLow == 0)) newState = 0;
        if (display != nullptr) {
          switch (newState) {
            case 0: changedState = changeState(PSTR("Brightness"),      1,   0,  1); break; //1  = sun
            case 1: changedState = changeState(PSTR("Speed"),           1,   4,  2); break; //2  = skip forward
            case 2: changedState = changeState(PSTR("Intensity"),       1,   8,  3); break; //3  = fire
            case 3: changedState = changeState(PSTR("Color Palette"),   2,   0,  4); break; //4  = custom palette
            case 4: changedState = changeState(PSTR("Effect"),          3,   0,  5); break; //5  = puzzle piece
            case 5: changedState = changeState(PSTR("Main Color"),    255, 255,  7); break; //7  = brush
            case 6: changedState = changeState(PSTR("Saturation"),    255, 255,  8); break; //8  = contrast
            case 7: changedState = changeState(PSTR("CCT"),           255, 255, 10); break; //10 = star
            case 8: changedState = changeState(PSTR("Preset"),        255, 255, 11); break; //11 = heart
          }
        }
        if (changedState) select_state = newState;
      }

      Enc_A = digitalRead(pinA); // Read encoder pins
      Enc_B = digitalRead(pinB);
      if ((Enc_A) && (!Enc_A_prev))
      { // A has gone from high to low
        if (Enc_B == LOW)    //changes to LOW so that then encoder registers a change at the very end of a pulse
        { // B is high so clockwise
          switch(select_state) {
            case 0: changeBrightness(true);      break;
            case 1: changeEffectSpeed(true);     break;
            case 2: changeEffectIntensity(true); break;
            case 3: changePalette(true);         break;
            case 4: changeEffect(true);          break;
            case 5: changeHue(true);             break;
            case 6: changeSat(true);             break;
            case 7: changeCCT(true);             break;
            case 8: changePreset(true);          break;
          }
        }
        else if (Enc_B == HIGH)
        { // B is low so counter-clockwise
          switch(select_state) {
            case 0: changeBrightness(false);      break;
            case 1: changeEffectSpeed(false);     break;
            case 2: changeEffectIntensity(false); break;
            case 3: changePalette(false);         break;
            case 4: changeEffect(false);          break;
            case 5: changeHue(false);             break;
            case 6: changeSat(false);             break;
            case 7: changeCCT(false);             break;
            case 8: changePreset(false);          break;
          }
        }
      }
      Enc_A_prev = Enc_A;     // Store value of A for next time
    }
  }

  void displayNetworkInfo() {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
    display->networkOverlay(PSTR("NETWORK INFO"), 10000);
    #endif
  }

  void findCurrentEffectAndPalette() {
    currentEffectAndPaletteInitialized = true;
    for (uint8_t i = 0; i < strip.getModeCount(); i++) {
      if (modes_alpha_indexes[i] == effectCurrent) {
        effectCurrentIndex = i;
        break;
      }
    }

    for (uint8_t i = 0; i < strip.getPaletteCount(); i++) {
      if (palettes_alpha_indexes[i] == effectPalette) {
        effectPaletteIndex = i;
        break;
      }
    }
  }

  boolean changeState(const char *stateName, byte markedLine, byte markedCol, byte glyph) {
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    if (display != nullptr) {
      if (display->wakeDisplay()) {
        // Throw away wake up input
        display->redraw(true);
        return false;
      }
      display->overlay(stateName, 750, glyph);
      display->setMarkLine(markedLine, markedCol);
    }
  #endif
    return true;
  }

  void lampUdated() {
    //call for notifier -> 0: init 1: direct change 2: button 3: notification 4: nightlight 5: other (No notification)
    // 6: fx changed 7: hue 8: preset cycle 9: blynk 10: alexa
    //setValuesFromFirstSelectedSeg(); //to make transition work on main segment (should no longer be required)
    stateUpdated(CALL_MODE_BUTTON);
    updateInterfaces(CALL_MODE_BUTTON);
  }

  void changeBrightness(bool increase) {
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    if (display && display->wakeDisplay()) {
      display->redraw(true);
      // Throw away wake up input
      return;
    }
    display->updateRedrawTime();
  #endif
    bri = max(min((increase ? bri+fadeAmount : bri-fadeAmount), 255), 0);
    lampUdated();
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    display->updateBrightness();
  #endif
  }


  void changeEffect(bool increase) {
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    if (display && display->wakeDisplay()) {
      display->redraw(true);
      // Throw away wake up input
      return;
    }
    display->updateRedrawTime();
  #endif
    effectCurrentIndex = max(min((increase ? effectCurrentIndex+1 : effectCurrentIndex-1), strip.getModeCount()-1), 0);
    effectCurrent = modes_alpha_indexes[effectCurrentIndex];
    stateChanged = true;
    if (applyToAll) {
      for (byte i=0; i<strip.getMaxSegments(); i++) {
        WS2812FX::Segment& seg = strip.getSegment(i);
        if (!seg.isActive()) continue;
        strip.setMode(i, effectCurrent);
      }
    } else {
      //WS2812FX::Segment& seg = strip.getSegment(strip.getMainSegmentId());
      strip.setMode(strip.getMainSegmentId(), effectCurrent);
    }
    lampUdated();
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    display->showCurrentEffectOrPalette(effectCurrent, JSON_mode_names, 3);
  #endif
  }


  void changeEffectSpeed(bool increase) {
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    if (display && display->wakeDisplay()) {
      display->redraw(true);
      // Throw away wake up input
      return;
    }
    display->updateRedrawTime();
  #endif
    effectSpeed = max(min((increase ? effectSpeed+fadeAmount : effectSpeed-fadeAmount), 255), 0);
    stateChanged = true;
    if (applyToAll) {
      for (byte i=0; i<strip.getMaxSegments(); i++) {
        WS2812FX::Segment& seg = strip.getSegment(i);
        if (!seg.isActive()) continue;
        seg.speed = effectSpeed;
      }
    } else {
      WS2812FX::Segment& seg = strip.getSegment(strip.getMainSegmentId());
      seg.speed = effectSpeed;
    }
    lampUdated();
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    display->updateSpeed();
  #endif
  }


  void changeEffectIntensity(bool increase) {
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    if (display && display->wakeDisplay()) {
      display->redraw(true);
      // Throw away wake up input
      return;
    }
    display->updateRedrawTime();
  #endif
    effectIntensity = max(min((increase ? effectIntensity+fadeAmount : effectIntensity-fadeAmount), 255), 0);
    stateChanged = true;
    if (applyToAll) {
      for (byte i=0; i<strip.getMaxSegments(); i++) {
        WS2812FX::Segment& seg = strip.getSegment(i);
        if (!seg.isActive()) continue;
        seg.intensity = effectIntensity;
      }
    } else {
      WS2812FX::Segment& seg = strip.getSegment(strip.getMainSegmentId());
      seg.intensity = effectIntensity;
    }
    lampUdated();
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    display->updateIntensity();
  #endif
  }


  void changePalette(bool increase) {
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    if (display && display->wakeDisplay()) {
      display->redraw(true);
      // Throw away wake up input
      return;
    }
    display->updateRedrawTime();
  #endif
    effectPaletteIndex = max(min((increase ? effectPaletteIndex+1 : effectPaletteIndex-1), strip.getPaletteCount()-1), 0);
    effectPalette = palettes_alpha_indexes[effectPaletteIndex];
    stateChanged = true;
    if (applyToAll) {
      for (byte i=0; i<strip.getMaxSegments(); i++) {
        WS2812FX::Segment& seg = strip.getSegment(i);
        if (!seg.isActive()) continue;
        seg.palette = effectPalette;
      }
    } else {
      WS2812FX::Segment& seg = strip.getSegment(strip.getMainSegmentId());
      seg.palette = effectPalette;
    }
    lampUdated();
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    display->showCurrentEffectOrPalette(effectPalette, JSON_palette_names, 2);
  #endif
  }


  void changeHue(bool increase){
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    if (display && display->wakeDisplay()) {
      display->redraw(true);
      // Throw away wake up input
      return;
    }
    display->updateRedrawTime();
  #endif
    currentHue1 = max(min((increase ? currentHue1+fadeAmount : currentHue1-fadeAmount), 255), 0);
    colorHStoRGB(currentHue1*256, currentSat1, col);
    stateChanged = true; 
    if (applyToAll) {
      for (byte i=0; i<strip.getMaxSegments(); i++) {
        WS2812FX::Segment& seg = strip.getSegment(i);
        if (!seg.isActive()) continue;
        seg.colors[0] = RGBW32(col[0], col[1], col[2], col[3]);
      }
    } else {
      WS2812FX::Segment& seg = strip.getSegment(strip.getMainSegmentId());
      seg.colors[0] = RGBW32(col[0], col[1], col[2], col[3]);
    }
    lampUdated();
  }

  void changeSat(bool increase){
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    if (display && display->wakeDisplay()) {
      display->redraw(true);
      // Throw away wake up input
      return;
    }
    display->updateRedrawTime();
  #endif
    currentSat1 = max(min((increase ? currentSat1+fadeAmount : currentSat1-fadeAmount), 255), 0);
    colorHStoRGB(currentHue1*256, currentSat1, col);
    if (applyToAll) {
      for (byte i=0; i<strip.getMaxSegments(); i++) {
        WS2812FX::Segment& seg = strip.getSegment(i);
        if (!seg.isActive()) continue;
        seg.colors[0] = RGBW32(col[0], col[1], col[2], col[3]);
      }
    } else {
      WS2812FX::Segment& seg = strip.getSegment(strip.getMainSegmentId());
      seg.colors[0] = RGBW32(col[0], col[1], col[2], col[3]);
    }
    lampUdated();
  }

  void changePreset(bool increase) {
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    if (display && display->wakeDisplay()) {
      display->redraw(true);
      // Throw away wake up input
      return;
    }
    display->updateRedrawTime();
  #endif
    if (presetHigh && presetLow && presetHigh > presetLow) {
      String apireq = F("win&PL=~");
      if (!increase) apireq += '-';
      apireq += F("&P1=");
      apireq += presetLow;
      apireq += F("&P2=");
      apireq += presetHigh;
      handleSet(nullptr, apireq, false);
      lampUdated();
    }
  }

  void changeCCT(bool increase){
  #ifdef USERMOD_FOUR_LINE_DISPLAY
    if (display && display->wakeDisplay()) {
      display->redraw(true);
      // Throw away wake up input
      return;
    }
    display->updateRedrawTime();
  #endif
    currentCCT = max(min((increase ? currentCCT+fadeAmount : currentCCT-fadeAmount), 255), 0);
//    if (applyToAll) {
      for (byte i=0; i<strip.getMaxSegments(); i++) {
        WS2812FX::Segment& seg = strip.getSegment(i);
        if (!seg.isActive()) continue;
        seg.setCCT(currentCCT, i);
      }
//    } else {
//      WS2812FX::Segment& seg = strip.getSegment(strip.getMainSegmentId());
//      seg.setCCT(currentCCT, strip.getMainSegmentId());
//    }
    lampUdated();
  }

  /*
   * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
   * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
   * Below it is shown how this could be used for e.g. a light sensor
   */
  /*
  void addToJsonInfo(JsonObject& root)
  {
    int reading = 20;
    //this code adds "u":{"Light":[20," lux"]} to the info object
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
    JsonArray lightArr = user.createNestedArray("Light"); //name
    lightArr.add(reading); //value
    lightArr.add(" lux"); //unit
  }
  */

  /*
   * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  /*
  void addToJsonState(JsonObject &root)
  {
    //root["user0"] = userVar0;
  }
  */

  /*
   * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
   * Values in the state object may be modified by connected clients
   */
  /*
  void readFromJsonState(JsonObject &root)
  {
    //userVar0 = root["user0"] | userVar0; //if "user0" key exists in JSON, update, else keep old value
    //if (root["bri"] == 255) Serial.println(F("Don't burn down your garage!"));
  }
  */

  /**
   * addToConfig() (called from set.cpp) stores persistent properties to cfg.json
   */
  void addToConfig(JsonObject &root) {
    // we add JSON object: {"Rotary-Encoder":{"DT-pin":12,"CLK-pin":14,"SW-pin":13}}
    JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
    top[FPSTR(_enabled)] = enabled;
    top[FPSTR(_DT_pin)]  = pinA;
    top[FPSTR(_CLK_pin)] = pinB;
    top[FPSTR(_SW_pin)]  = pinC;
    top[FPSTR(_presetLow)]  = presetLow;
    top[FPSTR(_presetHigh)] = presetHigh;
    top[FPSTR(_applyToAll)] = applyToAll;
    DEBUG_PRINTLN(F("Rotary Encoder config saved."));
  }

  /**
   * readFromConfig() is called before setup() to populate properties from values stored in cfg.json
   *
   * The function should return true if configuration was successfully loaded or false if there was no configuration.
   */
  bool readFromConfig(JsonObject &root) {
    // we look for JSON object: {"Rotary-Encoder":{"DT-pin":12,"CLK-pin":14,"SW-pin":13}}
    JsonObject top = root[FPSTR(_name)];
    if (top.isNull()) {
      DEBUG_PRINT(FPSTR(_name));
      DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
      return false;
    }
    int8_t newDTpin  = top[FPSTR(_DT_pin)]  | pinA;
    int8_t newCLKpin = top[FPSTR(_CLK_pin)] | pinB;
    int8_t newSWpin  = top[FPSTR(_SW_pin)]  | pinC;

    presetHigh = top[FPSTR(_presetHigh)] | presetHigh;
    presetLow  = top[FPSTR(_presetLow)]  | presetLow;
    presetHigh = MIN(250,MAX(0,presetHigh));
    presetLow  = MIN(250,MAX(0,presetLow));

    enabled    = top[FPSTR(_enabled)] | enabled;
    applyToAll = top[FPSTR(_applyToAll)] | applyToAll;

    DEBUG_PRINT(FPSTR(_name));
    if (!initDone) {
      // first run: reading from cfg.json
      pinA = newDTpin;
      pinB = newCLKpin;
      pinC = newSWpin;
      DEBUG_PRINTLN(F(" config loaded."));
    } else {
      DEBUG_PRINTLN(F(" config (re)loaded."));
      // changing parameters from settings page
      if (pinA!=newDTpin || pinB!=newCLKpin || pinC!=newSWpin) {
        pinManager.deallocatePin(pinA, PinOwner::UM_RotaryEncoderUI);
        pinManager.deallocatePin(pinB, PinOwner::UM_RotaryEncoderUI);
        pinManager.deallocatePin(pinC, PinOwner::UM_RotaryEncoderUI);
        pinA = newDTpin;
        pinB = newCLKpin;
        pinC = newSWpin;
        if (pinA<0 || pinB<0 || pinC<0) {
          enabled = false;
          return true;
        }
        setup();
      }
    }
    // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
    return !top[FPSTR(_applyToAll)].isNull();
  }

  /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
  uint16_t getId()
  {
    return USERMOD_ID_ROTARY_ENC_UI;
  }
};

// strings to reduce flash memory usage (used more than twice)
const char RotaryEncoderUIUsermod::_name[]       PROGMEM = "Rotary-Encoder";
const char RotaryEncoderUIUsermod::_enabled[]    PROGMEM = "enabled";
const char RotaryEncoderUIUsermod::_DT_pin[]     PROGMEM = "DT-pin";
const char RotaryEncoderUIUsermod::_CLK_pin[]    PROGMEM = "CLK-pin";
const char RotaryEncoderUIUsermod::_SW_pin[]     PROGMEM = "SW-pin";
const char RotaryEncoderUIUsermod::_presetHigh[] PROGMEM = "preset-high";
const char RotaryEncoderUIUsermod::_presetLow[]  PROGMEM = "preset-low";
const char RotaryEncoderUIUsermod::_applyToAll[] PROGMEM = "apply-2-all-seg";
