#pragma once

#include "wled.h"

// Define the necessary constants
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

// Define the WizMoteMessageStructure
typedef struct WizMoteMessageStructure {
  uint8_t program;
  uint8_t seq[4];
  uint8_t dt1;
  uint8_t button;
  uint8_t dt2;
  uint8_t batLevel;
  uint8_t byte10;
  uint8_t byte11;
  uint8_t byte12;
  uint8_t byte13;
} message_structure_t;

// Declare static variables
static uint32_t last_seq_visualremote = UINT32_MAX;
static int brightnessBeforeNightMode_visualremote = NIGHT_MODE_DEACTIVATED;

static const byte brightnessSteps_visualremote[] = {
  6, 9, 14, 22, 33, 50, 75, 113, 170, 255
};
static const size_t numBrightnessSteps_visualremote = sizeof(brightnessSteps_visualremote) / sizeof(byte);

// Inline functions to prevent multiple definitions
inline bool nightModeActive_visualremote() {
  return brightnessBeforeNightMode_visualremote != NIGHT_MODE_DEACTIVATED;
}

inline void activateNightMode_visualremote() {
  if (nightModeActive_visualremote()) return;
  brightnessBeforeNightMode_visualremote = bri;
  bri = NIGHT_MODE_BRIGHTNESS;
  stateUpdated(CALL_MODE_BUTTON);
}

inline bool resetNightMode_visualremote() {
  if (!nightModeActive_visualremote()) return false;
  bri = brightnessBeforeNightMode_visualremote;
  brightnessBeforeNightMode_visualremote = NIGHT_MODE_DEACTIVATED;
  stateUpdated(CALL_MODE_BUTTON);
  return true;
}

inline void brightnessUp_visualremote() {
  if (nightModeActive_visualremote()) return;
  for (unsigned index = 0; index < numBrightnessSteps_visualremote; ++index) {
    if (brightnessSteps_visualremote[index] > bri) {
      bri = brightnessSteps_visualremote[index];
      break;
    }
  }
  stateUpdated(CALL_MODE_BUTTON);
}

inline void brightnessDown_visualremote() {
  if (nightModeActive_visualremote()) return;
  for (int index = numBrightnessSteps_visualremote - 1; index >= 0; --index) {
    if (brightnessSteps_visualremote[index] < bri) {
      bri = brightnessSteps_visualremote[index];
      break;
    }
  }
  stateUpdated(CALL_MODE_BUTTON);
}

inline void setOn_visualremote() {
  resetNightMode_visualremote();
  if (!bri) {
    toggleOnOff();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

inline void setOff_visualremote() {
  resetNightMode_visualremote();
  if (bri) {
    toggleOnOff();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

inline void presetWithFallback_visualremote(uint8_t presetID, uint8_t effectID, uint8_t paletteID) {
  resetNightMode_visualremote();
  applyPresetWithFallback(presetID, CALL_MODE_BUTTON_PRESET, effectID, paletteID);
}

inline bool remoteJson_visualremote(int button) {
  char objKey[10];
  bool parsed = false;

  if (!requestJSONBufferLock(22)) return false;

  sprintf_P(objKey, PSTR("\"%d\":"), button);

  readObjectFromFile(PSTR("/remote.json"), objKey, pDoc);
  JsonObject fdo = pDoc->as<JsonObject>();
  if (fdo.isNull()) {
    releaseJSONBufferLock();
    return parsed;
  }

  String cmdStr = fdo["cmd"].as<String>();
  JsonObject jsonCmdObj = fdo["cmd"];

  if (jsonCmdObj.isNull()) {
    if (cmdStr.startsWith("!")) {
      if (cmdStr.startsWith(F("!incBri"))) {
        brightnessUp_visualremote();
        parsed = true;
      } else if (cmdStr.startsWith(F("!decBri"))) {
        brightnessDown_visualremote();
        parsed = true;
      } else if (cmdStr.startsWith(F("!presetF"))) {
        uint8_t p1 = fdo["PL"] | 1;
        uint8_t p2 = fdo["FX"] | random8(strip.getModeCount() -1);
        uint8_t p3 = fdo["FP"] | 0;
        presetWithFallback_visualremote(p1, p2, p3);
        parsed = true;
      }
    } else {
      String apireq = "win"; apireq += '&';
      if (!cmdStr.startsWith(apireq)) cmdStr = apireq + cmdStr;
      if (!irApplyToAllSelected && cmdStr.indexOf(F("SS="))<0) {
        char tmp[10];
        sprintf_P(tmp, PSTR("&SS=%d"), strip.getMainSegmentId());
        cmdStr += tmp;
      }
      fdo.clear();
      handleSet(nullptr, cmdStr, false);
      stateUpdated(CALL_MODE_BUTTON);
      parsed = true;
    }
  } else {
    deserializeState(jsonCmdObj, CALL_MODE_BUTTON);
    parsed = true;
  }
  releaseJSONBufferLock();
  return parsed;
}

inline void handleRemote_visualremote(uint8_t *incomingData, size_t len) {
  message_structure_t *incoming = reinterpret_cast<message_structure_t *>(incomingData);

  if (len != sizeof(message_structure_t)) {
    Serial.printf("Unknown incoming ESP-NOW message received of length %u\n", len);
    return;
  }

  uint32_t cur_seq = incoming->seq[0] | (incoming->seq[1] << 8) | (incoming->seq[2] << 16) | (incoming->seq[3] << 24);
  if (cur_seq == last_seq_visualremote) {
    return;
  }

  Serial.print("Incoming ESP-NOW Packet [");
  Serial.print(cur_seq);
  Serial.print("] button: ");
  Serial.println(incoming->button);

  if (!remoteJson_visualremote(incoming->button)) {
    switch (incoming->button) {
      case WIZMOTE_BUTTON_ON             : setOn_visualremote();                                         break;
      case WIZMOTE_BUTTON_OFF            : setOff_visualremote();                                        break;
      case WIZMOTE_BUTTON_ONE            : presetWithFallback_visualremote(1, FX_MODE_STATIC,        0); break;
      case WIZMOTE_BUTTON_TWO            : presetWithFallback_visualremote(2, FX_MODE_BREATH,        0); break;
      case WIZMOTE_BUTTON_THREE          : presetWithFallback_visualremote(3, FX_MODE_FIRE_FLICKER,  0); break;
      case WIZMOTE_BUTTON_FOUR           : presetWithFallback_visualremote(4, FX_MODE_RAINBOW,       0); break;
      case WIZMOTE_BUTTON_NIGHT          : activateNightMode_visualremote();                             break;
      case WIZMOTE_BUTTON_BRIGHT_UP      : brightnessUp_visualremote();                                  break;
      case WIZMOTE_BUTTON_BRIGHT_DOWN    : brightnessDown_visualremote();                                break;
      case WIZ_SMART_BUTTON_ON           : setOn_visualremote();                                         break;
      case WIZ_SMART_BUTTON_OFF          : setOff_visualremote();                                        break;
      case WIZ_SMART_BUTTON_BRIGHT_UP    : brightnessUp_visualremote();                                  break;
      case WIZ_SMART_BUTTON_BRIGHT_DOWN  : brightnessDown_visualremote();                                break;
      default: break;
    }
  }
  last_seq_visualremote = cur_seq;
}

// Usermod class
class UsermodVisualRemote : public Usermod {
  private:
    // Private members can be added here if needed

  public:
    void setup() {
      Serial.println("VisualRemote mod active!");
    }

    void loop() {
      // Code to run in the main loop
    }

    bool onEspNowMessage(uint8_t* sender, uint8_t* data, uint8_t len) {
      DEBUG_PRINT(F("CUSTOM ESP-NOW: "));
      // You may need to comment out or define 'last_signal_src' appropriately
      // DEBUG_PRINT(last_signal_src);
      DEBUG_PRINT(F(" -> "));
      DEBUG_PRINTLN(len);
      for (int i = 0; i < len; i++) {
        DEBUG_PRINTF_P(PSTR("%02x "), data[i]);
      }
      DEBUG_PRINTLN();

      // Handle Remote here
      handleRemote_visualremote(data, len);

      // Return true to indicate message has been handled
      return true; // Override further processing
    }
};