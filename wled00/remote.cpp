#include "wled.h"
#ifndef WLED_DISABLE_ESPNOW

#define NIGHT_MODE_DEACTIVATED     -1
#define NIGHT_MODE_BRIGHTNESS      5

#define WIZMOTE_BUTTON_ON          1
#define WIZMOTE_BUTTON_OFF         2
#define WIZMOTE_BUTTON_NIGHT       3
#define WIZMOTE_BUTTON_ONE         16
#define WIZMOTE_BUTTON_TWO         17
#define WIZMOTE_BUTTON_THREE       18
#define WIZMOTE_BUTTON_FOUR        19
#define WIZMOTE_BUTTON_BRIGHT_UP   9
#define WIZMOTE_BUTTON_BRIGHT_DOWN 8

#define WIZ_SMART_BUTTON_ON          100
#define WIZ_SMART_BUTTON_OFF         101
#define WIZ_SMART_BUTTON_BRIGHT_UP   102
#define WIZ_SMART_BUTTON_BRIGHT_DOWN 103

#define ESPNOW_DELAY_PROCESSING 24 // one frame delay

static int brightnessBeforeNightMode = NIGHT_MODE_DEACTIVATED;

// Pulled from the IR Remote logic but reduced to 10 steps with a constant of 3
static const byte brightnessSteps[] = {
  6, 9, 14, 22, 33, 50, 75, 113, 170, 255
};
static const size_t numBrightnessSteps = sizeof(brightnessSteps) / sizeof(byte);

inline bool nightModeActive() {
  return brightnessBeforeNightMode != NIGHT_MODE_DEACTIVATED;
}

static void activateNightMode() {
  if (nightModeActive()) return;
  brightnessBeforeNightMode = bri;
  bri = NIGHT_MODE_BRIGHTNESS;
  stateUpdated(CALL_MODE_BUTTON);
}

static bool resetNightMode() {
  if (!nightModeActive()) return false;
  bri = brightnessBeforeNightMode;
  brightnessBeforeNightMode = NIGHT_MODE_DEACTIVATED;
  stateUpdated(CALL_MODE_BUTTON);
  return true;
}

// increment `bri` to the next `brightnessSteps` value
static void brightnessUp() {
  if (nightModeActive()) return;
  // dumb incremental search is efficient enough for so few items
  for (unsigned index = 0; index < numBrightnessSteps; ++index) {
    if (brightnessSteps[index] > bri) {
      bri = brightnessSteps[index];
      break;
    }
  }
  stateUpdated(CALL_MODE_BUTTON);
}

// decrement `bri` to the next `brightnessSteps` value
static void brightnessDown() {
  if (nightModeActive()) return;
  // dumb incremental search is efficient enough for so few items
  for (int index = numBrightnessSteps - 1; index >= 0; --index) {
    if (brightnessSteps[index] < bri) {
      bri = brightnessSteps[index];
      break;
    }
  }
  stateUpdated(CALL_MODE_BUTTON);
}

static void setOn() {
  resetNightMode();
  if (!bri) {
    toggleOnOff();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

static void setOff() {
  resetNightMode();
  if (bri) {
    toggleOnOff();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

void presetWithFallback(uint8_t presetID, uint8_t effectID, uint8_t paletteID) {
  resetNightMode();
  applyPresetWithFallback(presetID, CALL_MODE_BUTTON_PRESET, effectID, paletteID);
}

// this function follows the same principle as decodeIRJson()
static bool remoteJson(int button)
{
  char objKey[10];
  bool parsed = false;

  unsigned long start = millis();
  while (strip.isUpdating() && millis()-start < ESPNOW_DELAY_PROCESSING) delay(1); // we are not in ISR/callback

  if (!requestJSONBufferLock(22)) return false;

  sprintf_P(objKey, PSTR("\"%d\":"), button);

  // attempt to read command from remote.json
  readObjectFromFile(PSTR("/remote.json"), objKey, pDoc);
  JsonObject fdo = pDoc->as<JsonObject>();
  if (fdo.isNull()) {
    // the received button does not exist
    //if (!WLED_FS.exists(F("/remote.json"))) errorFlag = ERR_FS_RMLOAD; //warn if file itself doesn't exist
    releaseJSONBufferLock();
    return parsed;
  }

  String cmdStr = fdo["cmd"].as<String>();
  JsonObject jsonCmdObj = fdo["cmd"]; //object

  if (jsonCmdObj.isNull())  // we could also use: fdo["cmd"].is<String>()
  {
    if (cmdStr.startsWith("!")) {
      // call limited set of C functions
      if (cmdStr.startsWith(F("!incBri"))) {
        brightnessUp();
        parsed = true;
      } else if (cmdStr.startsWith(F("!decBri"))) {
        brightnessDown();
        parsed = true;
      } else if (cmdStr.startsWith(F("!presetF"))) { //!presetFallback
        uint8_t p1 = fdo["PL"] | 1;
        uint8_t p2 = fdo["FX"] | random8(strip.getModeCount() -1);
        uint8_t p3 = fdo["FP"] | 0;
        presetWithFallback(p1, p2, p3);
        parsed = true;
      }
    } else {
      // HTTP API command
      String apireq = "win"; apireq += '&';                        // reduce flash string usage
      //if (cmdStr.indexOf("~") || fdo["rpt"]) lastValidCode = code; // repeatable action
      if (!cmdStr.startsWith(apireq)) cmdStr = apireq + cmdStr;    // if no "win&" prefix
      if (!irApplyToAllSelected && cmdStr.indexOf(F("SS="))<0) {
        char tmp[10];
        sprintf_P(tmp, PSTR("&SS=%d"), strip.getMainSegmentId());
        cmdStr += tmp;
      }
      fdo.clear();                                                 // clear JSON buffer (it is no longer needed)
      handleSet(nullptr, cmdStr, false);                           // no stateUpdated() call here
      stateUpdated(CALL_MODE_BUTTON);
      parsed = true;
    }
  } else {
    // command is JSON object (TODO: currently will not handle irApplyToAllSelected correctly)
    deserializeState(jsonCmdObj, CALL_MODE_BUTTON);
    parsed = true;
  }
  releaseJSONBufferLock();
  return parsed;
}

// Callback function that will be executed when data is received
void handleRemote() {
  if (wizMoteButton == -1) return;
  if (!remoteJson(wizMoteButton))
    switch (wizMoteButton) {
      case WIZMOTE_BUTTON_ON             : setOn();                                         break;
      case WIZMOTE_BUTTON_OFF            : setOff();                                        break;
      case WIZMOTE_BUTTON_ONE            : presetWithFallback(1, FX_MODE_STATIC,        0); break;
      case WIZMOTE_BUTTON_TWO            : presetWithFallback(2, FX_MODE_BREATH,        0); break;
      case WIZMOTE_BUTTON_THREE          : presetWithFallback(3, FX_MODE_FIRE_FLICKER,  0); break;
      case WIZMOTE_BUTTON_FOUR           : presetWithFallback(4, FX_MODE_RAINBOW,       0); break;
      case WIZMOTE_BUTTON_NIGHT          : activateNightMode();                             break;
      case WIZMOTE_BUTTON_BRIGHT_UP      : brightnessUp();                                  break;
      case WIZMOTE_BUTTON_BRIGHT_DOWN    : brightnessDown();                                break;
      case WIZ_SMART_BUTTON_ON           : setOn();                                         break;
      case WIZ_SMART_BUTTON_OFF          : setOff();                                        break;
      case WIZ_SMART_BUTTON_BRIGHT_UP    : brightnessUp();                                  break;
      case WIZ_SMART_BUTTON_BRIGHT_DOWN  : brightnessDown();                                break;
      default: break;
    }
  wizMoteButton = -1;
}

#else
void handleRemote() {}
#endif
