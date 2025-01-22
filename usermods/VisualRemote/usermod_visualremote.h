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

#define WIZMOTE_BUTTON_TWO_DOUBLE  6
#define WIZMOTE_BUTTON_TWO_TRIPLE  7
#define WIZMOTE_BUTTON_TWO_QUAD    8
#define WIZMOTE_BUTTON_TWO_QUINT   9
#define WIZMOTE_BUTTON_TWO_HEX     10
#define WIZMOTE_BUTTON_TWO_LONG    11

#define WIZMOTE_BUTTON_ONE_DOUBLE  13
#define WIZMOTE_BUTTON_ONE_TRIPLE  14
#define WIZMOTE_BUTTON_ONE_QUAD    15
#define WIZMOTE_BUTTON_ONE_QUINT   16
#define WIZMOTE_BUTTON_ONE_HEX     17
#define WIZMOTE_BUTTON_ONE_LONG    18

#define WIZMOTE_BUTTON_FOUR_DOUBLE 20
#define WIZMOTE_BUTTON_FOUR_TRIPLE 21
#define WIZMOTE_BUTTON_FOUR_QUAD   22
#define WIZMOTE_BUTTON_FOUR_QUINT  23
#define WIZMOTE_BUTTON_FOUR_HEX    24
#define WIZMOTE_BUTTON_FOUR_LONG   25

#define WIZMOTE_BUTTON_THREE_DOUBLE 27
#define WIZMOTE_BUTTON_THREE_TRIPLE 28
#define WIZMOTE_BUTTON_THREE_QUAD   29
#define WIZMOTE_BUTTON_THREE_QUINT  30
#define WIZMOTE_BUTTON_THREE_HEX    31
#define WIZMOTE_BUTTON_THREE_LONG   32

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
static int NextBrightnessStep = 0;
static bool SyncMode = true;
static bool SyncModeChanged = false;
static bool MenuMode = false;
static bool ButtonPressed = false;
static uint8_t MagicFlowMode = 0;
static uint8_t MagicFlowProgram = 5;

static const byte brightnessSteps_visualremote[] = {
  6, 33, 50, 128 
};

static const size_t numBrightnessSteps_visualremote = sizeof(brightnessSteps_visualremote) / sizeof(byte);

// Inline functions to prevent multiple definitions
inline bool nightModeActive_visualremote() {
  return brightnessBeforeNightMode_visualremote != NIGHT_MODE_DEACTIVATED;
}


inline bool resetNightMode_visualremote() {
  if (!nightModeActive_visualremote()) return false;
  
  bri = brightnessBeforeNightMode_visualremote;
  brightnessBeforeNightMode_visualremote = NIGHT_MODE_DEACTIVATED;  
  stateUpdated(CALL_MODE_BUTTON);
  return true;
}


inline void activateNightMode_visualremote() {
  if (nightModeActive_visualremote()) 
  {
    resetNightMode_visualremote();
    return;
  }
  brightnessBeforeNightMode_visualremote = bri;
  bri = NIGHT_MODE_BRIGHTNESS;  
  stateUpdated(CALL_MODE_BUTTON);
}

inline void setBrightness_visualremote() {
  bri = brightnessSteps_visualremote[NextBrightnessStep];
  NextBrightnessStep++;
  if (NextBrightnessStep >= sizeof(brightnessSteps_visualremote)) {
    NextBrightnessStep = 0;
  }
  DEBUG_PRINTF("Brightness adjusted: %u\n", bri);
  stateUpdated(CALL_MODE_BUTTON);
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

inline void togglePower_visualremote() {
  //resetNightMode_visualremote();
  toggleOnOff();
  stateUpdated(CALL_MODE_BUTTON);
}

inline void toggleSyncMode_visualremote() {  
  SyncMode = !SyncMode;
  SyncModeChanged = true;
}

inline void setOn_visualremote() {
  //resetNightMode_visualremote();
  if (!bri) {
    toggleOnOff();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

inline void setOff_visualremote() {
  //resetNightMode_visualremote();
  if (bri) {
    toggleOnOff();
    stateUpdated(CALL_MODE_BUTTON);
  }
}

inline void presetWithFallback_visualremote(uint8_t presetID, uint8_t effectID, uint8_t paletteID) {
  if (presetToApply == presetID) return;
  presetToApply = presetID;
  //resetNightMode_visualremote();
  applyPresetWithFallback(presetID, CALL_MODE_BUTTON_PRESET, effectID, paletteID);
}

inline void increaseSpeed()
{
  DEBUG_PRINTF("Increase speed: %u\n", effectSpeed);
  effectSpeed = min(effectSpeed + 20, 255);
  stateChanged = true;
  for (unsigned i=0; i<strip.getSegmentsNum(); i++) {
      Segment& seg = strip.getSegment(i);
      if (!seg.isActive()) continue;
      seg.speed = effectSpeed;
  }
  colorUpdated(CALL_MODE_FX_CHANGED);
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


void magic_flow(uint8_t program) {
  MagicFlowProgram = program;
  if (MagicFlowMode == 0) 
  {
    MagicFlowMode = 1;
    presetWithFallback_visualremote(9, FX_MODE_2DCRAZYBEES, 0);
    return;
  }
  if (MagicFlowMode == 1) 
  {
    MagicFlowMode = 2;
    presetWithFallback_visualremote(10, FX_MODE_2DCRAZYBEES, 0);
    return;
  }
  if (MagicFlowMode == 2) 
  {
    MagicFlowMode = 0;
    presetWithFallback_visualremote(MagicFlowProgram, FX_MODE_2DCRAZYBEES, 0);
    return;
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
    bool preset_available[255] = {false};
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
     String name = "";
    for (unsigned presetIndex = 1; presetIndex <= 255; presetIndex++)
    {
       if (!getPresetName(presetIndex, name)) break; // no more presets
       preset_available[presetIndex] = true;
    }
  }
    void addToConfig(JsonObject& root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top["SegmentId"] = segmentId;
      top["SegmentPixelOffset"] = segmentPixelOffset;
      top["TimeOutMenu"] = timeOutMenu;     
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top["SegmentId"], segmentId, 1);  
      configComplete &= getJsonValue(top["SegmentPixelOffset"], segmentPixelOffset, 0);  
      configComplete &= getJsonValue(top["TimeOutMenu"], timeOutMenu, 300);     


      return configComplete;
    }


   void onButtonUpPress() {
    if (MagicFlowMode > 0) 
    {
      magic_flow(MagicFlowProgram);
    }
    if (MenuMode) {
      do {
        currentEffectIndex++;
        if (currentEffectIndex >= 255) {
          currentEffectIndex = 5;
        }
      } while (!preset_available[currentEffectIndex]);
      DEBUG_PRINTF("> Start Display effect %d \n", currentEffectIndex);
      presetWithFallback_visualremote(currentEffectIndex, FX_MODE_BIERTJE,        0);
    } else
    {
      increaseSpeed();
    }
   }

    void onButtonDownPress() {
      MenuMode = true;      
      do {
        if (currentEffectIndex <= 5) {
          currentEffectIndex = 255;
        } else {
          currentEffectIndex--;
        }
      } while (!preset_available[currentEffectIndex]);
      DEBUG_PRINTF("< Start Display effect %d \n", currentEffectIndex);
      presetWithFallback_visualremote(currentEffectIndex, FX_MODE_BIERTJE,        0);
    }




    inline void handleRemote_visualremote(uint8_t *incomingData, size_t len) {
      message_structure_t *incoming = reinterpret_cast<message_structure_t *>(incomingData);

      if (len != sizeof(message_structure_t)) {
        //Serial.printf("Unknown incoming ESP-NOW message received of length %u\n", len);
        return;
      }
      DEBUG_PRINTF("Button value: %u\n", incoming->button);
      if (strcmp(last_signal_src, linked_remote) != 0) {
        DEBUG_PRINT(F("ESP Now Message Received from Unlinked Sender: "));
        DEBUG_PRINTLN(last_signal_src);
        if (!SyncMode) {
          return;
        }
        if (
          (incoming->button != WIZMOTE_BUTTON_ONE_LONG) &&
          (incoming->button != WIZMOTE_BUTTON_TWO_LONG) &&
          (incoming->button != WIZMOTE_BUTTON_THREE_LONG) &&
          (incoming->button != WIZMOTE_BUTTON_FOUR_LONG) 
          ) {
            return;
          }
      }

      uint32_t cur_seq = incoming->seq[0] | (incoming->seq[1] << 8) | (incoming->seq[2] << 16) | (incoming->seq[3] << 24);
      if (cur_seq == last_seq_visualremote) {
        return;
      }

      ButtonPressed = true;    
      lastTime = millis();

      if ((incoming->button != WIZMOTE_BUTTON_BRIGHT_DOWN) && (incoming->button != WIZMOTE_BUTTON_BRIGHT_UP)) {
        MenuMode = false;
      }
    
      switch (incoming->button) {
        case WIZMOTE_BUTTON_ON             : togglePower_visualremote();                                            break;
        case WIZMOTE_BUTTON_OFF            : toggleSyncMode_visualremote();                                           break;
        case WIZMOTE_BUTTON_ONE            : presetWithFallback_visualremote(1, FX_MODE_BIERTJE,        0);    break;
        case WIZMOTE_BUTTON_TWO            : presetWithFallback_visualremote(2, FX_MODE_BREATH,        0);    break;
        case WIZMOTE_BUTTON_THREE          : presetWithFallback_visualremote(3, FX_MODE_FIRE_FLICKER,  0);    break;
        case WIZMOTE_BUTTON_FOUR           : presetWithFallback_visualremote(4, FX_MODE_HEART,       0);    break;
        case WIZMOTE_BUTTON_NIGHT          : setBrightness_visualremote();                                break;
        case WIZMOTE_BUTTON_BRIGHT_UP      : onButtonUpPress();                                               break;
        case WIZMOTE_BUTTON_BRIGHT_DOWN    : onButtonDownPress();                                             break;
      
        case WIZMOTE_BUTTON_ONE_LONG       : presetWithFallback_visualremote(1, FX_MODE_BIERTJE,        0);    break;
        case WIZMOTE_BUTTON_TWO_LONG       : presetWithFallback_visualremote(2, FX_MODE_BREATH,        0);    break;
        case WIZMOTE_BUTTON_THREE_LONG     : presetWithFallback_visualremote(3, FX_MODE_FIRE_FLICKER,        0);    break;
        case WIZMOTE_BUTTON_FOUR_LONG      : presetWithFallback_visualremote(4, FX_MODE_HEART,        0);    break;

        case WIZMOTE_BUTTON_ONE_DOUBLE     : magic_flow(5);    break;
        case WIZMOTE_BUTTON_TWO_DOUBLE     : magic_flow(6);    break;
        case WIZMOTE_BUTTON_THREE_DOUBLE   : magic_flow(7);    break;
        case WIZMOTE_BUTTON_FOUR_DOUBLE    : magic_flow(8);    break;
        case WIZMOTE_BUTTON_ONE_TRIPLE     : magic_flow(5);    break;;
        case WIZMOTE_BUTTON_TWO_TRIPLE     : magic_flow(6);    break;
        case WIZMOTE_BUTTON_THREE_TRIPLE   : magic_flow(7);    break;
        case WIZMOTE_BUTTON_FOUR_TRIPLE    : magic_flow(8);    break;
        default: break;
      }

      last_seq_visualremote = cur_seq;
    }

    void handleOverlayDraw() {
      uint32_t wifiColor = BLACK;  // default color

      switch (WiFi.status()) {
        case 4:
          wifiColor = RED;
          break;
        case 3:
          wifiColor = GREEN;
          break;
        case 2:
          wifiColor = ORANGE;
          break;
        case 1:
          wifiColor = PURPLE;
          break;
        case 0:
          wifiColor = BLUE;
          break;
        default:
          wifiColor = BLUE;  // Off
          break;
      }
  
      //strip.getSegment(segmentId).setPixelColor(segmentPixelOffset, wifiColor); 
      
      //strip.getSegment(segmentId).setPixelColor(segmentPixelOffset + 1, syncColor); 

      if (ButtonPressed) {     
        if (millis() - lastTime > timeOutMenu) {
          ButtonPressed = false;
        }   
        strip.getSegment(segmentId).setPixelColor(segmentPixelOffset, WHITE);       
      }
      

      if (SyncModeChanged) {     
        if (millis() - lastTime > timeOutMenu) {
          SyncModeChanged = false;
        }   
        uint32_t syncColor = SyncMode ? GREEN : RED;
        strip.fill(syncColor);
        //strip.getSegment(segmentId).setPixelColor(segmentPixelOffset, WHITE);       
      }

      /* if (isDisplayingEffectIndicator) {
        strip.fill(BLACK);
        stateUpdated(CALL_MODE_DIRECT_CHANGE);
        if (millis() - lastTime > timeOutMenu) {
          return;
        }   
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

      

          
      }
      */
    }

    void loop() {
      // Code to run in the main loop
    }

    bool onEspNowMessage(uint8_t* sender, uint8_t* data, uint8_t len) {      
      handleRemote_visualremote(data, len);
      
      // Return true to indicate message has been handled
      return true; // Override further processing
    }

  //  void appendConfigData() override {
  //     Serial.println("VisualRemote mod appendConfigData");
  //     oappend(F("addInfo('VisualRemote:patterns',1,'<small style=\"color:orange\">requires reboot</small>');"));
  //     oappend(F("addInfo('VisualRemote")); oappend(String(FPSTR(_name)).c_str()); oappend(F(":great")); oappend(F("',1,'<i>(this is a great config value)</i>');"));
  //     oappend(F("td=addDropdown('VisualRemote','SegmentId');"));          
  //     for (int i = 0; i<= strip.getLastActiveSegmentId(); i++) {
  //       oappend(F("addOption(td,'")); oappend(String(i)); oappend(F("','")); oappend(String(i)); oappend(F("');"));
  //     }
     
};

// add more strings here to reduce flash memory usage
const char UsermodVisualRemote::_name[]    PROGMEM = "VisualRemote";
const char UsermodVisualRemote::_enabled[] PROGMEM = "enabled";
