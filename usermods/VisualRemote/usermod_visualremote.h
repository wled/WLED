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

#define TOTAL_EFFECTS 20
// Define a struct to hold the effect ID and its 5-pixel indicator pattern
struct EffectIndicator {
  uint8_t effectId;
  uint32_t indicator[5]; // Colors for 5 pixels (in hex RGB format)
};


// Define the effectIndicators array for all 187 effects
EffectIndicator effectIndicators[] = {
  // Effect 0: FX_MODE_STATIC
  {
    FX_MODE_STATIC,
    {0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF} // Solid white
  },
  // Effect 1: FX_MODE_BLINK
  {
    FX_MODE_BLINK,
    {0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF} // Alternating white and black
  },
  // Effect 2: FX_MODE_BREATH
  {
    FX_MODE_BREATH,
    {0x666666, 0x999999, 0xCCCCCC, 0x999999, 0x666666} // Gradual brightness (gray shades)
  },
  // Effect 3: FX_MODE_COLOR_WIPE
  {
    FX_MODE_COLOR_WIPE,
    {0xFF0000, 0x330000, 0x660000, 0x990000, 0xCC0000} // Shades of red wiping
  },
  // Effect 4: FX_MODE_COLOR_WIPE_RANDOM
  {
    FX_MODE_COLOR_WIPE_RANDOM,
    {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF} // Random colors
  },
  // Effect 5: FX_MODE_RANDOM_COLOR
  {
    FX_MODE_RANDOM_COLOR,
    {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF} // Random colors
  },
  // Effect 6: FX_MODE_COLOR_SWEEP
  {
    FX_MODE_COLOR_SWEEP,
    {0x000000, 0x333333, 0x666666, 0x999999, 0xFFFFFF} // Sweeping from black to white
  },
  // Effect 7: FX_MODE_DYNAMIC
  {
    FX_MODE_DYNAMIC,
    {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFFFF, 0xFFFF00} // Dynamic mix of colors
  },
  // Effect 8: FX_MODE_RAINBOW
  {
    FX_MODE_RAINBOW,
    {0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF} // Red, Orange, Yellow, Green, Blue
  },
  // Effect 9: FX_MODE_RAINBOW_CYCLE
  {
    FX_MODE_RAINBOW_CYCLE,
    {0x8B00FF, 0x4B0082, 0x0000FF, 0x00FF00, 0xFFFF00} // Violet, Indigo, Blue, Green, Yellow
  },
  // Effect 10: FX_MODE_SCAN
  {
    FX_MODE_SCAN,
    {0x000000, 0x000000, 0xFF0000, 0x000000, 0x000000} // Single red pixel scanning
  },
  // Effect 11: FX_MODE_DUAL_SCAN
  {
    FX_MODE_DUAL_SCAN,
    {0xFF0000, 0x000000, 0x000000, 0x000000, 0xFF0000} // Two red pixels at ends scanning
  },
  // Effect 12: FX_MODE_FADE
  {
    FX_MODE_FADE,
    {0xFF0000, 0xFF4000, 0xFF8000, 0xFFBF00, 0xFFFF00} // Fading through colors
  },
  // Effect 13: FX_MODE_THEATER_CHASE
  {
    FX_MODE_THEATER_CHASE,
    {0xFF0000, 0x000000, 0xFF0000, 0x000000, 0xFF0000} // Chasing red lights
  },
  // Effect 14: FX_MODE_THEATER_CHASE_RAINBOW
  {
    FX_MODE_THEATER_CHASE_RAINBOW,
    {0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF} // Chasing rainbow colors
  },
  // Effect 15: FX_MODE_RUNNING_LIGHTS
  {
    FX_MODE_RUNNING_LIGHTS,
    {0xFFFFFF, 0x7F7F7F, 0x3F3F3F, 0x1F1F1F, 0x0F0F0F} // Running white lights fading
  },
  // Effect 16: FX_MODE_SAW
  {
    FX_MODE_SAW,
    {0xFFFFFF, 0xCCCCCC, 0x999999, 0x666666, 0x333333} // Sawtooth effect with grays
  },
  // Effect 17: FX_MODE_TWINKLE
  {
    FX_MODE_TWINKLE,
    {0xFFFFFF, 0x000000, 0xFFFFFF, 0x000000, 0xFFFFFF} // Twinkling white lights
  },
  // Effect 18: FX_MODE_DISSOLVE
  {
    FX_MODE_DISSOLVE,
    {0xFF0000, 0xCC0000, 0x990000, 0x660000, 0x330000} // Dissolving reds
  },
  // Effect 19: FX_MODE_DISSOLVE_RANDOM
  {
    FX_MODE_DISSOLVE_RANDOM,
    {0xFF69B4, 0x8A2BE2, 0x00CED1, 0x7FFF00, 0xFFD700} // Dissolving random colors
  },
  // Effect 20: FX_MODE_SPARKLE
  {
    FX_MODE_SPARKLE,
    {0xFFFFFF, 0x000000, 0x000000, 0x000000, 0xFFFFFF} // Sparkle with white
  }
};

uint8_t currentEffectIndex = 0;
unsigned long lastTime = 0;
bool isDisplayingEffectIndicator = false;     // Flag to indicate if indicator is currently displayed

void displayEffectIndicator(uint8_t effectIndex) {
    // Set the timestamp and flag
  lastTime = millis();
  isDisplayingEffectIndicator = true;
  for (int i = 0; i < 5; i++) {
    strip.setPixelColor(i, effectIndicators[effectIndex].indicator[i]);
  }
  strip.show();
}

void onButtonUpPress() {
  currentEffectIndex++;
  if (currentEffectIndex >= TOTAL_EFFECTS) {
    currentEffectIndex = 0;
  }
  displayEffectIndicator(currentEffectIndex);
}

void onButtonDownPress() {
  if (currentEffectIndex == 0) {
    currentEffectIndex = TOTAL_EFFECTS - 1;
  } else {
    currentEffectIndex--;
  }
  displayEffectIndicator(currentEffectIndex);
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

DEBUG_PRINT(F("Incoming ESP-NOW Packet ["));
DEBUG_PRINT(cur_seq);
DEBUG_PRINT(F("] button: "));
DEBUG_PRINTLN(incoming->button);

  if (!remoteJson_visualremote(incoming->button)) {
    switch (incoming->button) {
      case WIZMOTE_BUTTON_ON             : setOn_visualremote();                                         break;
      case WIZMOTE_BUTTON_OFF            : setOff_visualremote();                                        break;
      case WIZMOTE_BUTTON_ONE            : presetWithFallback_visualremote(1, FX_MODE_STATIC,        0); break;
      case WIZMOTE_BUTTON_TWO            : presetWithFallback_visualremote(2, FX_MODE_BREATH,        0); break;
      case WIZMOTE_BUTTON_THREE          : presetWithFallback_visualremote(3, FX_MODE_FIRE_FLICKER,  0); break;
      case WIZMOTE_BUTTON_FOUR           : presetWithFallback_visualremote(4, FX_MODE_RAINBOW,       0); break;
      case WIZMOTE_BUTTON_NIGHT          : activateNightMode_visualremote();                             break;
      case WIZMOTE_BUTTON_BRIGHT_UP      : onButtonUpPress();                                            break;
      case WIZMOTE_BUTTON_BRIGHT_DOWN    : onButtonDownPress();                                          break;
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

    void handleOverlayDraw()
    {
      if (isDisplayingEffectIndicator) {
        for (int i = 0; i < 5; i++) {
          strip.setPixelColor(i, effectIndicators[currentEffectIndex].indicator[i]);

        }
        // Check if the indicator has been displayed for 5 seconds
        if (millis() - lastTime > 1000) {
          isDisplayingEffectIndicator = false;
          effectCurrent = effectIndicators[currentEffectIndex].effectId;
          colorUpdated(CALL_MODE_FX_CHANGED);
        }
      }
    }

};