#include "wled.h"

//
// I2C Rotary Encoder UI Usermod
//
// Functionally equivalent to the GPIO Rotary Encoder UI usermod
// (usermod_v2_rotary_encoder_ui_ALT), but uses an I2C rotary encoder
// instead of a 5-pin GPIO encoder.
//
// Designed for the M5Stack Encoder Unit (UNIT-ENCODER, SKU: U135).
// I2C address: 0x40 (default). No encoder GPIO pins are needed —
// configure I2C SDA/SCL globally in the WLED Hardware settings page.
//
// M5Stack Encoder Unit register map:
//   0x00 : Mode (0=Pulse, 1=AB) — write 0x00 to initialise
//   0x10 : Encoder count low byte
//   0x11 : Encoder count high byte  (int16_t, little-endian)
//   0x20 : Button (0=Released, 1=Pressed)
//
// Controls (same as GPIO variant):
//   Rotate  : adjust the currently selected parameter
//   Press   : cycle through parameter modes
//   2×Press : toggle on/off
//   Hold 3s : show network info overlay (requires FourLineDisplay)
//

#ifdef USERMOD_FOUR_LINE_DISPLAY
#include "usermod_v2_four_line_display.h"
#endif

#ifdef USERMOD_MODE_SORT
  #error "Usermod Mode Sort is no longer required. Remove -D USERMOD_MODE_SORT from platformio.ini"
#endif

#ifndef I2C_ENCODER_POLL_MS
#define I2C_ENCODER_POLL_MS 10   // poll every 10 ms (100 Hz)
#endif

// With FourLineDisplay: Brightness/Speed/Intensity/Palette/Effect/Hue/Sat/CCT/Preset/Cx3
// Without            : Brightness/Speed/Intensity/Palette/Effect only
#ifdef USERMOD_FOUR_LINE_DISPLAY
 #define I2CENC_LAST_UI_STATE 11
#else
 #define I2CENC_LAST_UI_STATE 4
#endif

#define I2CENC_MODE_SORT_SKIP_COUNT 1

// File-static helpers for qsort comparator (internal linkage, no conflict with other mods)
static const char **i2cenc_listBeingSorted;

static int i2cenc_qstringCmp(const void *ap, const void *bp) {
  const char *a = i2cenc_listBeingSorted[*((byte *)ap)];
  const char *b = i2cenc_listBeingSorted[*((byte *)bp)];
  int i = 0;
  do {
    char aVal = pgm_read_byte_near(a + i);
    if (aVal >= 97 && aVal <= 122) aVal -= 32;
    char bVal = pgm_read_byte_near(b + i);
    if (bVal >= 97 && bVal <= 122) bVal -= 32;
    if (aVal == '"' || bVal == '"' || aVal == '\0' || bVal == '\0') {
      if (aVal == bVal)              return  0;
      else if (aVal == '"' || aVal == '\0') return -1;
      else                           return  1;
    }
    if (aVal == bVal) { i++; continue; }
    return (aVal < bVal) ? -1 : 1;
  } while (true);
  return 0;
}


class I2CEncoderUIUsermod : public Usermod {

  private:

    const int8_t fadeAmount;       // step size for most parameters
    unsigned long loopTime;

    unsigned long buttonPressedTime;
    unsigned long buttonWaitTime;
    bool buttonPressedBefore;
    bool buttonLongPressed;

    uint8_t  i2cAddress;           // I2C address of the encoder unit
    uint16_t lastRaw;              // last raw count read from device
    bool     deviceFound;          // set true when encoder responds at setup

    unsigned char select_state;    // which parameter we are currently editing

    uint16_t currentHue1;
    byte     currentSat1;
    uint8_t  currentCCT;

  #ifdef USERMOD_FOUR_LINE_DISPLAY
    FourLineDisplayUsermod *display;
  #else
    void *display;
  #endif

    const char **modes_qstrings;
    byte       *modes_alpha_indexes;
    const char **palettes_qstrings;
    byte       *palettes_alpha_indexes;

    bool    currentEffectAndPaletteInitialized;
    uint8_t effectCurrentIndex;
    uint8_t effectPaletteIndex;

    byte presetHigh;
    byte presetLow;
    bool applyToAll;
    bool initDone;
    bool enabled;

    static const char _name[];
    static const char _enabled[];
    static const char _i2cAddress[];
    static const char _presetHigh[];
    static const char _presetLow[];
    static const char _applyToAll[];

    // ------------------------------------------------------------------ I2C

    // Write a single byte to a register. Returns true on success.
    bool writeReg(uint8_t reg, uint8_t value) {
      Wire.beginTransmission(i2cAddress);
      Wire.write(reg);
      Wire.write(value);
      return Wire.endTransmission() == 0;
    }

    // Read the encoder count as a signed 16-bit delta from the last reading.
    // Returns 0 on I2C error.
    int16_t readEncoderDelta() {
      Wire.beginTransmission(i2cAddress);
      Wire.write(0x10);
      if (Wire.endTransmission(false) != 0) return 0;
      if (Wire.requestFrom((uint8_t)i2cAddress, (uint8_t)2) < 2) return 0;
      uint16_t raw = (uint16_t)Wire.read() | ((uint16_t)Wire.read() << 8);
      int16_t delta = (int16_t)(raw - lastRaw);
      lastRaw = raw;
      return delta;
    }

    // Read the button state. Returns true when the button is pressed.
    bool readButtonState() {
      Wire.beginTransmission(i2cAddress);
      Wire.write(0x20);
      if (Wire.endTransmission(false) != 0) return false;
      if (Wire.requestFrom((uint8_t)i2cAddress, (uint8_t)1) < 1) return false;
      return Wire.read() == 1; // 0=Released, 1=Pressed
    }

    // --------------------------------------------------------- sorting helpers

    void sortModesAndPalettes() {
      modes_qstrings       = strip.getModeDataSrc();
      modes_alpha_indexes  = re_initIndexArray(strip.getModeCount());
      re_sortModes(modes_qstrings, modes_alpha_indexes, strip.getModeCount(), I2CENC_MODE_SORT_SKIP_COUNT);

      palettes_qstrings      = re_findModeStrings(JSON_palette_names, getPaletteCount());
      palettes_alpha_indexes = re_initIndexArray(getPaletteCount());
      if (customPalettes.size()) {
        for (int i = 0; i < (int)customPalettes.size(); i++) {
          palettes_alpha_indexes[FIXED_PALETTE_COUNT + i] = 255 - i;
          palettes_qstrings[FIXED_PALETTE_COUNT + i]      = PSTR("~Custom~");
        }
      }
      int skipPaletteCount = 1;
      while (pgm_read_byte_near(palettes_qstrings[skipPaletteCount]) == '*') skipPaletteCount++;
      re_sortModes(palettes_qstrings, palettes_alpha_indexes, FIXED_PALETTE_COUNT, skipPaletteCount);
    }

    byte *re_initIndexArray(int numModes) {
      byte *indexes = (byte *)malloc(sizeof(byte) * numModes);
      for (unsigned i = 0; i < (unsigned)numModes; i++) indexes[i] = i;
      return indexes;
    }

    const char **re_findModeStrings(const char json[], int numModes) {
      const char **modeStrings = (const char **)malloc(sizeof(const char *) * numModes);
      uint8_t modeIndex = 0;
      bool insideQuotes = false;
      bool complete = false;
      for (size_t i = 0; i < strlen_P(json); i++) {
        char c = pgm_read_byte_near(json + i);
        if (c == '\0') break;
        switch (c) {
          case '"':
            insideQuotes = !insideQuotes;
            if (insideQuotes) modeStrings[modeIndex] = (char *)(json + i + 1);
            break;
          case ']':
            if (!insideQuotes) complete = true;
            break;
          case ',':
            if (!insideQuotes) modeIndex++;
            break;
          default: break;
        }
        if (complete) break;
      }
      return modeStrings;
    }

    void re_sortModes(const char **modeNames, byte *indexes, int count, int numSkip) {
      if (!modeNames) return;
      i2cenc_listBeingSorted = modeNames;
      qsort(indexes + numSkip, count - numSkip, sizeof(byte), i2cenc_qstringCmp);
      i2cenc_listBeingSorted = nullptr;
    }

  public:

    I2CEncoderUIUsermod()
      : fadeAmount(5)
      , loopTime(0)
      , buttonPressedTime(0)
      , buttonWaitTime(0)
      , buttonPressedBefore(false)
      , buttonLongPressed(false)
      , i2cAddress(0x40)
      , lastRaw(0)
      , deviceFound(false)
      , select_state(0)
      , currentHue1(16)
      , currentSat1(255)
      , currentCCT(128)
      , display(nullptr)
      , modes_qstrings(nullptr)
      , modes_alpha_indexes(nullptr)
      , palettes_qstrings(nullptr)
      , palettes_alpha_indexes(nullptr)
      , currentEffectAndPaletteInitialized(false)
      , effectCurrentIndex(0)
      , effectPaletteIndex(0)
      , presetHigh(0)
      , presetLow(0)
      , applyToAll(true)
      , initDone(false)
      , enabled(true)
    {}

    uint16_t getId() override { return USERMOD_ID_I2C_ENCODER_UI; }

    void setup() override {
      DEBUG_PRINTLN(F("I2C Encoder UI: init."));

      if (i2c_sda < 0 || i2c_scl < 0) {
        DEBUG_PRINTLN(F("I2C Encoder UI: I2C pins not configured, disabling."));
        enabled = false;
        return;
      }

      // Set Pulse mode and verify device is present.
      // A failed write means wrong address or not connected — keep enabled so the
      // user can correct the address in settings without rebooting.
      if (!writeReg(0x00, 0x00)) {
        DEBUG_PRINTLN(F("I2C Encoder UI: device not found at address."));
        deviceFound = false;
        return;
      }
      deviceFound = true;

      // Read baseline count so first delta is 0
      Wire.beginTransmission(i2cAddress);
      Wire.write(0x10);
      Wire.endTransmission(false);
      if (Wire.requestFrom((uint8_t)i2cAddress, (uint8_t)2) >= 2) {
        lastRaw = (uint16_t)Wire.read() | ((uint16_t)Wire.read() << 8);
      }

      currentCCT = (approximateKelvinFromRGB(RGBW32(colPri[0], colPri[1], colPri[2], colPri[3])) - 1900) >> 5;

      if (!initDone) sortModesAndPalettes();

    #ifdef USERMOD_FOUR_LINE_DISPLAY
      display = (FourLineDisplayUsermod *)UsermodManager::lookup(USERMOD_ID_FOUR_LINE_DISP);
      if (display != nullptr) display->setMarkLine(1, 0);
    #endif

      loopTime = millis();
      initDone = true;
    }

    void loop() override {
      if (!enabled || !deviceFound) return;

      unsigned long currentTime = millis();
      if (strip.isUpdating() && (currentTime - loopTime) < I2C_ENCODER_POLL_MS) return;
      if (currentTime - loopTime < I2C_ENCODER_POLL_MS) return;

      if (!currentEffectAndPaletteInitialized) findCurrentEffectAndPalette();

      if (modes_alpha_indexes[effectCurrentIndex]    != effectCurrent ||
          palettes_alpha_indexes[effectPaletteIndex] != effectPalette) {
        currentEffectAndPaletteInitialized = false;
      }

      // ---- button ----
      bool buttonPressed = readButtonState();
      if (buttonPressed) {
        if (!buttonPressedBefore) buttonPressedTime = currentTime;
        buttonPressedBefore = true;
        if (currentTime - buttonPressedTime > 3000) {
          if (!buttonLongPressed) displayNetworkInfo();
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
        buttonLongPressed   = false;
        buttonPressedBefore = false;
      }

      // Single-press: cycle modes after 350 ms with no second press
      if (buttonWaitTime && currentTime - buttonWaitTime > 350 && !buttonPressedBefore) {
        buttonWaitTime = 0;
        char newState = select_state + 1;
        bool changedState = false;
        char lineBuffer[64];
        do {
          switch (newState) {
            case  0: strcpy_P(lineBuffer, PSTR("Brightness"));   changedState = true; break;
            case  1: if (!extractModeSlider(effectCurrent, 0, lineBuffer, 63)) newState++; else changedState = true; break;
            case  2: if (!extractModeSlider(effectCurrent, 1, lineBuffer, 63)) newState++; else changedState = true; break;
            case  3: strcpy_P(lineBuffer, PSTR("Color Palette")); changedState = true; break;
            case  4: strcpy_P(lineBuffer, PSTR("Effect"));        changedState = true; break;
            case  5: strcpy_P(lineBuffer, PSTR("Main Color"));    changedState = true; break;
            case  6: strcpy_P(lineBuffer, PSTR("Saturation"));    changedState = true; break;
            case  7:
              if (!(strip.getSegment(applyToAll ? strip.getFirstSelectedSegId() : strip.getMainSegmentId()).getLightCapabilities() & 0x04)) newState++;
              else { strcpy_P(lineBuffer, PSTR("CCT")); changedState = true; }
              break;
            case  8: if (presetHigh == 0 || presetLow == 0) newState++; else { strcpy_P(lineBuffer, PSTR("Preset")); changedState = true; } break;
            case  9:
            case 10:
            case 11: if (!extractModeSlider(effectCurrent, newState - 7, lineBuffer, 63)) newState++; else changedState = true; break;
          }
          if (newState > I2CENC_LAST_UI_STATE) newState = 0;
        } while (!changedState);

        if (display != nullptr) {
          switch (newState) {
            case  0: changedState = changeState(lineBuffer,   1,   0,  1); break;
            case  1: changedState = changeState(lineBuffer,   1,   4,  2); break;
            case  2: changedState = changeState(lineBuffer,   1,   8,  3); break;
            case  3: changedState = changeState(lineBuffer,   2,   0,  4); break;
            case  4: changedState = changeState(lineBuffer,   3,   0,  5); break;
            case  5: changedState = changeState(lineBuffer, 255, 255,  7); break;
            case  6: changedState = changeState(lineBuffer, 255, 255,  8); break;
            case  7: changedState = changeState(lineBuffer, 255, 255, 10); break;
            case  8: changedState = changeState(lineBuffer, 255, 255, 11); break;
            case  9: changedState = changeState(lineBuffer, 255, 255, 10); break;
            case 10: changedState = changeState(lineBuffer, 255, 255, 10); break;
            case 11: changedState = changeState(lineBuffer, 255, 255, 10); break;
          }
        }
        if (changedState) select_state = newState;
      }

      // ---- encoder ----
      int16_t delta = readEncoderDelta();
      if (delta != 0) {
        bool increase = delta > 0;
        switch (select_state) {
          case  0: changeBrightness(increase);      break;
          case  1: changeEffectSpeed(increase);     break;
          case  2: changeEffectIntensity(increase); break;
          case  3: changePalette(increase);         break;
          case  4: changeEffect(increase);          break;
          case  5: changeHue(increase);             break;
          case  6: changeSat(increase);             break;
          case  7: changeCCT(increase);             break;
          case  8: changePreset(increase);          break;
          case  9: changeCustom(1, increase);       break;
          case 10: changeCustom(2, increase);       break;
          case 11: changeCustom(3, increase);       break;
        }
      }

      loopTime = currentTime;
    }

    // ------------------------------------------------------------------ display

    void displayNetworkInfo() {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display) display->networkOverlay(PSTR("NETWORK INFO"), 10000);
    #endif
    }

    void findCurrentEffectAndPalette() {
      currentEffectAndPaletteInitialized = true;
      effectCurrentIndex = 0;
      for (int i = 0; i < strip.getModeCount(); i++) {
        if (modes_alpha_indexes[i] == effectCurrent) { effectCurrentIndex = i; break; }
      }
      effectPaletteIndex = 0;
      for (unsigned i = 0; i < getPaletteCount() + customPalettes.size(); i++) {
        if (palettes_alpha_indexes[i] == effectPalette) { effectPaletteIndex = i; break; }
      }
    }

    bool changeState(const char *stateName, byte markedLine, byte markedCol, byte glyph) {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display != nullptr) {
        if (display->wakeDisplay()) { display->redraw(true); return false; }
        display->overlay(stateName, 750, glyph);
        display->setMarkLine(markedLine, markedCol);
      }
    #endif
      return true;
    }

    void lampUdated() {
      stateUpdated(CALL_MODE_BUTTON);
      updateInterfaces(CALL_MODE_BUTTON);
    }

    // ------------------------------------------------------------ change methods

    void changeBrightness(bool increase) {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display && display->wakeDisplay()) { display->redraw(true); return; }
      if (display) display->updateRedrawTime();
    #endif
      if (bri < 40) bri = max(min((increase ? bri + fadeAmount/2 : bri - fadeAmount/2), 255), 0);
      else          bri = max(min((increase ? bri + fadeAmount   : bri - fadeAmount),   255), 0);
      lampUdated();
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display) display->updateBrightness();
    #endif
    }

    void changeEffect(bool increase) {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display && display->wakeDisplay()) { display->redraw(true); return; }
      if (display) display->updateRedrawTime();
    #endif
      effectCurrentIndex = max(min((increase ? effectCurrentIndex + 1 : effectCurrentIndex - 1), strip.getModeCount() - 1), 0);
      effectCurrent      = modes_alpha_indexes[effectCurrentIndex];
      stateChanged       = true;
      if (applyToAll) {
        for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
          Segment &seg = strip.getSegment(i);
          if (!seg.isActive()) continue;
          seg.setMode(effectCurrent);
        }
      } else {
        strip.getSegment(strip.getMainSegmentId()).setMode(effectCurrent);
      }
      lampUdated();
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display) display->showCurrentEffectOrPalette(effectCurrent, JSON_mode_names, 3);
    #endif
    }

    void changeEffectSpeed(bool increase) {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display && display->wakeDisplay()) { display->redraw(true); return; }
      if (display) display->updateRedrawTime();
    #endif
      effectSpeed  = max(min((increase ? effectSpeed + fadeAmount : effectSpeed - fadeAmount), 255), 0);
      stateChanged = true;
      if (applyToAll) {
        for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
          Segment &seg = strip.getSegment(i);
          if (!seg.isActive()) continue;
          seg.speed = effectSpeed;
        }
      } else {
        strip.getSegment(strip.getMainSegmentId()).speed = effectSpeed;
      }
      lampUdated();
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display) display->updateSpeed();
    #endif
    }

    void changeEffectIntensity(bool increase) {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display && display->wakeDisplay()) { display->redraw(true); return; }
      if (display) display->updateRedrawTime();
    #endif
      effectIntensity = max(min((increase ? effectIntensity + fadeAmount : effectIntensity - fadeAmount), 255), 0);
      stateChanged    = true;
      if (applyToAll) {
        for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
          Segment &seg = strip.getSegment(i);
          if (!seg.isActive()) continue;
          seg.intensity = effectIntensity;
        }
      } else {
        strip.getSegment(strip.getMainSegmentId()).intensity = effectIntensity;
      }
      lampUdated();
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display) display->updateIntensity();
    #endif
    }

    void changeCustom(uint8_t par, bool increase) {
      uint8_t val = 0;
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display && display->wakeDisplay()) { display->redraw(true); return; }
      if (display) display->updateRedrawTime();
    #endif
      stateChanged = true;
      if (applyToAll) {
        uint8_t  id  = strip.getFirstSelectedSegId();
        Segment &sid = strip.getSegment(id);
        switch (par) {
          case 3:  val = sid.custom3 = max(min((increase ? sid.custom3 + fadeAmount : sid.custom3 - fadeAmount), 255), 0); break;
          case 2:  val = sid.custom2 = max(min((increase ? sid.custom2 + fadeAmount : sid.custom2 - fadeAmount), 255), 0); break;
          default: val = sid.custom1 = max(min((increase ? sid.custom1 + fadeAmount : sid.custom1 - fadeAmount), 255), 0); break;
        }
        for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
          Segment &seg = strip.getSegment(i);
          if (!seg.isActive() || i == id) continue;
          switch (par) {
            case 3:  seg.custom3 = sid.custom3; break;
            case 2:  seg.custom2 = sid.custom2; break;
            default: seg.custom1 = sid.custom1; break;
          }
        }
      } else {
        Segment &seg = strip.getMainSegment();
        switch (par) {
          case 3:  val = seg.custom3 = max(min((increase ? seg.custom3 + fadeAmount : seg.custom3 - fadeAmount), 255), 0); break;
          case 2:  val = seg.custom2 = max(min((increase ? seg.custom2 + fadeAmount : seg.custom2 - fadeAmount), 255), 0); break;
          default: val = seg.custom1 = max(min((increase ? seg.custom1 + fadeAmount : seg.custom1 - fadeAmount), 255), 0); break;
        }
      }
      lampUdated();
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display) {
        char lineBuffer[64];
        sprintf(lineBuffer, "%d", val);
        display->overlay(lineBuffer, 500, 10);
      }
    #endif
    }

    void changePalette(bool increase) {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display && display->wakeDisplay()) { display->redraw(true); return; }
      if (display) display->updateRedrawTime();
    #endif
      effectPaletteIndex = max(min((unsigned)(increase ? effectPaletteIndex + 1 : effectPaletteIndex - 1), getPaletteCount() + customPalettes.size() - 1), 0U);
      effectPalette      = palettes_alpha_indexes[effectPaletteIndex];
      stateChanged       = true;
      if (applyToAll) {
        for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
          Segment &seg = strip.getSegment(i);
          if (!seg.isActive()) continue;
          seg.setPalette(effectPalette);
        }
      } else {
        strip.getSegment(strip.getMainSegmentId()).setPalette(effectPalette);
      }
      lampUdated();
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display) display->showCurrentEffectOrPalette(effectPalette, JSON_palette_names, 2);
    #endif
    }

    void changeHue(bool increase) {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display && display->wakeDisplay()) { display->redraw(true); return; }
      if (display) display->updateRedrawTime();
    #endif
      currentHue1 = max(min((increase ? currentHue1 + fadeAmount : currentHue1 - fadeAmount), 255), 0);
      colorHStoRGB(currentHue1 * 256, currentSat1, colPri);
      stateChanged = true;
      if (applyToAll) {
        for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
          Segment &seg = strip.getSegment(i);
          if (!seg.isActive()) continue;
          seg.colors[0] = RGBW32(colPri[0], colPri[1], colPri[2], colPri[3]);
        }
      } else {
        strip.getSegment(strip.getMainSegmentId()).colors[0] = RGBW32(colPri[0], colPri[1], colPri[2], colPri[3]);
      }
      lampUdated();
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display) {
        char lineBuffer[64];
        sprintf(lineBuffer, "%d", currentHue1);
        display->overlay(lineBuffer, 500, 7);
      }
    #endif
    }

    void changeSat(bool increase) {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display && display->wakeDisplay()) { display->redraw(true); return; }
      if (display) display->updateRedrawTime();
    #endif
      currentSat1 = max(min((increase ? currentSat1 + fadeAmount : currentSat1 - fadeAmount), 255), 0);
      colorHStoRGB(currentHue1 * 256, currentSat1, colPri);
      if (applyToAll) {
        for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
          Segment &seg = strip.getSegment(i);
          if (!seg.isActive()) continue;
          seg.colors[0] = RGBW32(colPri[0], colPri[1], colPri[2], colPri[3]);
        }
      } else {
        strip.getSegment(strip.getMainSegmentId()).colors[0] = RGBW32(colPri[0], colPri[1], colPri[2], colPri[3]);
      }
      lampUdated();
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display) {
        char lineBuffer[64];
        sprintf(lineBuffer, "%d", currentSat1);
        display->overlay(lineBuffer, 500, 8);
      }
    #endif
    }

    void changePreset(bool increase) {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display && display->wakeDisplay()) { display->redraw(true); return; }
      if (display) display->updateRedrawTime();
    #endif
      if (presetHigh && presetLow && presetHigh > presetLow) {
        StaticJsonDocument<64> root;
        char str[64];
        sprintf_P(str, PSTR("%d~%d~%s"), presetLow, presetHigh, increase ? "" : "-");
        root["ps"] = str;
        deserializeState(root.as<JsonObject>(), CALL_MODE_BUTTON_PRESET);
        lampUdated();
      #ifdef USERMOD_FOUR_LINE_DISPLAY
        if (display) {
          sprintf(str, "%d", currentPreset);
          display->overlay(str, 500, 11);
        }
      #endif
      }
    }

    void changeCCT(bool increase) {
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display && display->wakeDisplay()) { display->redraw(true); return; }
      if (display) display->updateRedrawTime();
    #endif
      currentCCT = max(min((increase ? currentCCT + fadeAmount : currentCCT - fadeAmount), 255), 0);
      for (unsigned i = 0; i < strip.getSegmentsNum(); i++) {
        Segment &seg = strip.getSegment(i);
        if (!seg.isActive()) continue;
        seg.setCCT(currentCCT);
      }
      lampUdated();
    #ifdef USERMOD_FOUR_LINE_DISPLAY
      if (display) {
        char lineBuffer[64];
        sprintf(lineBuffer, "%d", currentCCT);
        display->overlay(lineBuffer, 500, 10);
      }
    #endif
    }

    // ------------------------------------------------------------------ config

    void addToConfig(JsonObject &root) override {
      JsonObject top       = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)]    = enabled;
      top[FPSTR(_i2cAddress)] = i2cAddress;
      top[FPSTR(_presetLow)]  = presetLow;
      top[FPSTR(_presetHigh)] = presetHigh;
      top[FPSTR(_applyToAll)] = applyToAll;
    }

    void appendConfigData() override {
      oappend(F("addInfo('I2C-Encoder-UI:i2c-address',1,'<i>default: 64 (0x40)</i>');"));
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) {
        DEBUG_PRINT(FPSTR(_name));
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      uint8_t newAddr = top[FPSTR(_i2cAddress)] | i2cAddress;

      presetHigh = top[FPSTR(_presetHigh)] | presetHigh;
      presetLow  = top[FPSTR(_presetLow)]  | presetLow;
      presetHigh = MIN(250, MAX(0, presetHigh));
      presetLow  = MIN(250, MAX(0, presetLow));
      enabled    = top[FPSTR(_enabled)]    | enabled;
      applyToAll = top[FPSTR(_applyToAll)] | applyToAll;

      if (!initDone) {
        i2cAddress = newAddr;
      } else if (i2cAddress != newAddr) {
        i2cAddress = newAddr;
        deviceFound = false;
        setup();
      }

      return !top[FPSTR(_applyToAll)].isNull();
    }
};


const char I2CEncoderUIUsermod::_name[]       PROGMEM = "I2C-Encoder-UI";
const char I2CEncoderUIUsermod::_enabled[]    PROGMEM = "enabled";
const char I2CEncoderUIUsermod::_i2cAddress[] PROGMEM = "i2c-address";
const char I2CEncoderUIUsermod::_presetHigh[] PROGMEM = "preset-high";
const char I2CEncoderUIUsermod::_presetLow[]  PROGMEM = "preset-low";
const char I2CEncoderUIUsermod::_applyToAll[] PROGMEM = "apply-2-all-seg";


static I2CEncoderUIUsermod usermod_v2_i2c_encoder_ui;
REGISTER_USERMOD(usermod_v2_i2c_encoder_ui);
