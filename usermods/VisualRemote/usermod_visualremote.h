#pragma once

#include "wled.h"

// Define the necessary constants
#define NIGHT_MODE_DEACTIVATED     -1
#define NIGHT_MODE_BRIGHTNESS      5

#define WIZMOTE_BUTTON_ON          3
#define WIZMOTE_BUTTON_OFF         1
#define WIZMOTE_BUTTON_NIGHT       37
#define WIZMOTE_BUTTON_ONE         12
#define WIZMOTE_BUTTON_TWO         5
#define WIZMOTE_BUTTON_THREE       26
#define WIZMOTE_BUTTON_FOUR        19
#define WIZMOTE_BUTTON_BRIGHT_UP   33
#define WIZMOTE_BUTTON_BRIGHT_DOWN 35

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
static volatile byte presetToApply = 0;
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
  if (presetToApply == presetID) return;
  presetToApply = presetID;
  resetNightMode_visualremote();
  applyPresetWithFallback(presetID, CALL_MODE_BUTTON_PRESET, effectID, paletteID);
}

struct Pattern {
  uint8_t id;           // ID of the pattern
  String name;          // Name of the pattern
  uint8_t length;       // Actual length of the pattern
  String colors[12];    // Up to 12 colors in HEX string format
};

String colorToHexString(uint32_t c) {
    char buffer[9];
    sprintf(buffer, "%06X", c);
    return buffer;
}

void serializePatternColors(JsonArray& json, String colors[12], uint8_t length) {
  for (int i = 0; i < length; i++) {
    json.add(colors[i]);
  }
}

void deserializePatternColors(JsonArray& json, String colors[12], uint8_t& length) {
  length = json.size();
  for (int i = 0; i < length; i++) {
    colors[i] = json[i].as<String>();
  }
}

unsigned long lastTime = 0;



// Usermod class
class UsermodVisualRemote : public Usermod {
  private:
    bool enabled = false;
    static const char _name[];
    static const char _enabled[];
    int segmentId;
    int segmentPixelOffset;
    int timeOutMenu;
    String presetNames[255]; 
    bool preset_available[255];
    Pattern patterns[255]; // Array to hold patterns for each preset
    bool isDisplayingEffectIndicator = false;

    uint8_t currentEffectIndex = 1;
    unsigned long lastTime = 0;

    Pattern* getPatternById(uint8_t id) {
      for (int i = 0; i < 255; i++) {
        if (patterns[i].id == id) {
          return &patterns[i];
        }
      }
      return nullptr; // Return nullptr if no matching pattern is found
    }

  public:
    void setup() {
      Serial.println("VisualRemote mod active!");
      for (byte i = 0; i < 255; i++) {
        preset_available[i] = getPresetName(i, presetNames[i]);        
        if (preset_available[i]) {
          Serial.printf("Preset %d: %s\n", i, presetNames[i].c_str());

          // Initialize pattern data from config
          if (patterns[i].name == "") {
            // No pattern exists yet for this preset, create a default one
            Serial.printf("No pattern for preset %d found, creating default pattern.\n", i);
            patterns[i].id = i;
            patterns[i].name = presetNames[i];
            patterns[i].length = 3;
            for (int j = 0; j < patterns[i].length; j++) {
              patterns[i].colors[j] = "FFFFFF"; // Default to white
            }
          } else {
            Serial.printf("Pattern for preset %d loaded successfully.\n", i);
          }
        }
      }
    }

  void StartDisplayEffectIndicator(uint8_t effectIndex) {
    effectCurrent = USERMOD_ID_BIERTJE;

    colorUpdated(CALL_MODE_FX_CHANGED);
      // Set the timestamp and flag
    lastTime = millis();
    isDisplayingEffectIndicator = true;
  }


    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top["SegmentId"] = segmentId;
      top["SegmentPixelOffset"] = segmentPixelOffset;
      top["TimeOutMenu"] = timeOutMenu;
      // Add patterns to config
      JsonArray patternsArray = top.createNestedArray("patterns");
      for (int i = 0; i < 255; i++) {
        if (patterns[i].name != "") {
          JsonObject patternObj = patternsArray.createNestedObject();
          patternObj["id"] = patterns[i].id;
          patternObj["name"] = patterns[i].name;
          patternObj["length"] = patterns[i].length;
          JsonArray colorsArray = patternObj.createNestedArray("colors");
          serializePatternColors(colorsArray, patterns[i].colors, patterns[i].length);
        }
      }
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top["SegmentId"], segmentId, 0);  
      configComplete &= getJsonValue(top["SegmentPixelOffset"], segmentPixelOffset, 0);  
      configComplete &= getJsonValue(top["TimeOutMenu"], timeOutMenu, 1000);  
      

      // Read patterns from config
      JsonArray patternsArray = top["patterns"].as<JsonArray>();
      if (!patternsArray.isNull()) {
        for (int i = 0; i < 255; i++) {
          JsonObject patternObj = patternsArray[i];
          if (!patternObj.isNull()) {
            patterns[i].id = patternObj["id"] | 0;
            patterns[i].name = patternObj["name"].as<String>();
            patterns[i].length = patternObj["length"] | 0;
            JsonArray colorsArray = patternObj["colors"].as<JsonArray>();
            if (!colorsArray.isNull()) {
              deserializePatternColors(colorsArray, patterns[i].colors, patterns[i].length);
            }
          }
        }
      }

      return configComplete;
    }


   void onButtonUpPress() {
      do {
        currentEffectIndex++;
        if (currentEffectIndex >= 255) {
          currentEffectIndex = 0;
        }
      } while (!preset_available[currentEffectIndex]);
      Serial.printf("> Start Display effect %d \n", currentEffectIndex);

      StartDisplayEffectIndicator(currentEffectIndex);
    }

    void onButtonDownPress() {
      do {
        if (currentEffectIndex == 0) {
          currentEffectIndex = 255;
        } else {
          currentEffectIndex--;
        }
      } while (!preset_available[currentEffectIndex]);
      Serial.printf("< Start Display effect %d \n", currentEffectIndex);
      StartDisplayEffectIndicator(currentEffectIndex);
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
 // Debug print the button value
      Serial.printf("Button value: %u\n", incoming->button);

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

      last_seq_visualremote = cur_seq;
    }

    void handleOverlayDraw() {
      if (isDisplayingEffectIndicator) {
            strip.setColor(0, 0, 0, 0);
        stateUpdated(CALL_MODE_DIRECT_CHANGE);

        Pattern* pattern = getPatternById(currentEffectIndex);
        if (pattern == nullptr) {
          Serial.printf("Pattern with ID %d not found\n", currentEffectIndex);
          return;
        }

        int patternLength = pattern->length; // Dynamic length

        for (int i = 0; i < patternLength; i++) {
          int pixelIndex = segmentPixelOffset + i;
          uint32_t color = strtoul(pattern->colors[i].c_str(), nullptr, 16);
          uint8_t r = (color >> 16) & 0xFF;
          uint8_t g = (color >> 8) & 0xFF;
          uint8_t b = color & 0xFF;
          strip.getSegment(segmentId).setPixelColor(pixelIndex, r, g, b); 
         // segment.lastLed
          //strip.setPixelColor(pixelIndex, r, g, b); // Set the segment color

          // Debug print
          //Serial.printf("Setting pixel %d to color R:%d G:%d B:%d\n", pixelIndex, r, g, b);
        }

        // Check if the indicator has been displayed for 5 seconds
        if (millis() - lastTime > timeOutMenu) {
          isDisplayingEffectIndicator = false;
          presetWithFallback_visualremote(pattern->id, FX_MODE_STATIC, 0);
        }
      }
    }

    void loop() {
      // Code to run in the main loop
    }

    bool onEspNowMessage(uint8_t* sender, uint8_t* data, uint8_t len) {      
      handleRemote_visualremote(data, len);
      
      // Return true to indicate message has been handled
      return true; // Override further processing
    }

   void appendConfigData() override {
      Serial.println("VisualRemote mod appendConfigData");
      oappend(F("addInfo('VisualRemote:patterns',1,'<small style=\"color:orange\">requires reboot</small>');"));
      oappend(F("addInfo('VisualRemote")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":great")); oappend(F("',1,'<i>(this is a great config value)</i>');"));
      oappend(F("td=addDropdown('VisualRemote','SegmentId');"));          
      for (int i = 0; i<= strip.getLastActiveSegmentId(); i++) {
        oappend(F("addOption(td,'")); oappend(String(i)); oappend(F("','")); oappend(String(i)); oappend(F("');"));
      }
      // for (int i = 0; i < 255; i++) {
      //   if (preset_available[i]) {
      //     oappend(F("addDropdown('Pattern for Preset "));
      //     oappend(String(i));
      //     oappend(F("','pattern_"));
      //     oappend(String(i));
      //     oappend(F("');"));
      //     for (int j = 0; j < patterns[i].length; j++) {
      //       oappend(F("addOption('pattern_"));
      //       oappend(String(i));
      //       oappend(F("','"));
      //       oappend(String(j));
      //       oappend(F("','"));
      //       oappend(String(patterns[i].colors[j], HEX));
      //       oappend(F("');"));
      //     }
      //   }
      //}
    }
};

// add more strings here to reduce flash memory usage
const char UsermodVisualRemote::_name[]    PROGMEM = "VisualRemote";
const char UsermodVisualRemote::_enabled[] PROGMEM = "enabled";
