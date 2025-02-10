#pragma once

#include "wled.h"

// Define the necessary constants
#define NIGHT_MODE_DEACTIVATED     -1
#define NIGHT_MODE_BRIGHTNESS      5

// Button Mappings


// Unique integers for each button press type
#define WIZMOTE_BUTTON_OFF_SHORT        1
#define WIZMOTE_BUTTON_OFF_DOUBLE       2
#define WIZMOTE_BUTTON_OFF_TRIPLE       3
#define WIZMOTE_BUTTON_OFF_QUADRUPLE    4
#define WIZMOTE_BUTTON_OFF_QUINTUPLE    5
#define WIZMOTE_BUTTON_OFF_SEXTUPLE     6
#define WIZMOTE_BUTTON_OFF_LONG         7

#define WIZMOTE_BUTTON_ON_SHORT         8
#define WIZMOTE_BUTTON_ON_DOUBLE        9
#define WIZMOTE_BUTTON_ON_TRIPLE        10
#define WIZMOTE_BUTTON_ON_QUADRUPLE     11
#define WIZMOTE_BUTTON_ON_QUINTUPLE     12
#define WIZMOTE_BUTTON_ON_SEXTUPLE      13
#define WIZMOTE_BUTTON_ON_LONG          14

#define WIZMOTE_BUTTON_TWO_SHORT        15
#define WIZMOTE_BUTTON_TWO_DOUBLE       16
#define WIZMOTE_BUTTON_TWO_TRIPLE       17
#define WIZMOTE_BUTTON_TWO_QUADRUPLE    18
#define WIZMOTE_BUTTON_TWO_QUINTUPLE    19
#define WIZMOTE_BUTTON_TWO_SEXTUPLE     20
#define WIZMOTE_BUTTON_TWO_LONG         21

#define WIZMOTE_BUTTON_ONE_SHORT        22
#define WIZMOTE_BUTTON_ONE_DOUBLE       23
#define WIZMOTE_BUTTON_ONE_TRIPLE       24
#define WIZMOTE_BUTTON_ONE_QUADRUPLE    25
#define WIZMOTE_BUTTON_ONE_QUINTUPLE    26
#define WIZMOTE_BUTTON_ONE_SEXTUPLE     27
#define WIZMOTE_BUTTON_ONE_LONG         28

#define WIZMOTE_BUTTON_THREE_SHORT      36
#define WIZMOTE_BUTTON_THREE_DOUBLE     37
#define WIZMOTE_BUTTON_THREE_TRIPLE     38
#define WIZMOTE_BUTTON_THREE_QUADRUPLE  39
#define WIZMOTE_BUTTON_THREE_QUINTUPLE  40
#define WIZMOTE_BUTTON_THREE_SEXTUPLE   41
#define WIZMOTE_BUTTON_THREE_LONG       42

#define WIZMOTE_BUTTON_FOUR_SHORT       29
#define WIZMOTE_BUTTON_FOUR_DOUBLE      30
#define WIZMOTE_BUTTON_FOUR_TRIPLE      31
#define WIZMOTE_BUTTON_FOUR_QUADRUPLE   32
#define WIZMOTE_BUTTON_FOUR_QUINTUPLE   33
#define WIZMOTE_BUTTON_FOUR_SEXTUPLE    34
#define WIZMOTE_BUTTON_FOUR_LONG        35

#define WIZMOTE_BUTTON_BRIGHT_UP_SHORT       43
#define WIZMOTE_BUTTON_BRIGHT_UP_DOUBLE      44
#define WIZMOTE_BUTTON_BRIGHT_UP_TRIPLE      45
#define WIZMOTE_BUTTON_BRIGHT_UP_QUADRUPLE   46
#define WIZMOTE_BUTTON_BRIGHT_UP_QUINTUPLE   47
#define WIZMOTE_BUTTON_BRIGHT_UP_SEXTUPLE    48
#define WIZMOTE_BUTTON_BRIGHT_UP_LONG        49

#define WIZMOTE_BUTTON_BRIGHT_DOWN_SHORT     50
#define WIZMOTE_BUTTON_BRIGHT_DOWN_DOUBLE    51
#define WIZMOTE_BUTTON_BRIGHT_DOWN_TRIPLE    52
#define WIZMOTE_BUTTON_BRIGHT_DOWN_QUADRUPLE 53
#define WIZMOTE_BUTTON_BRIGHT_DOWN_QUINTUPLE 54
#define WIZMOTE_BUTTON_BRIGHT_DOWN_SEXTUPLE  55
#define WIZMOTE_BUTTON_BRIGHT_DOWN_LONG      56

#define WIZMOTE_BUTTON_NIGHT_SHORT      57
#define WIZMOTE_BUTTON_NIGHT_DOUBLE     58
#define WIZMOTE_BUTTON_NIGHT_TRIPLE     59
#define WIZMOTE_BUTTON_NIGHT_QUADRUPLE  60
#define WIZMOTE_BUTTON_NIGHT_QUINTUPLE  61
#define WIZMOTE_BUTTON_NIGHT_SEXTUPLE   62
#define WIZMOTE_BUTTON_NIGHT_LONG       63

#define WIZMOTE_BUTTON_PROGRAM          255


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
static int NextBrightnessStep = 3;
static bool SyncMode = true;
static bool SyncModeChanged = false;
static bool MenuMode = false;
static bool ButtonPressed = false;
static uint8_t MagicFlowStart = 11;
static uint8_t MagicFlowMode = 0;
static uint8_t MagicFlowProgram = 5;
static bool BroadcastProgram = false;
static bool UpdateBrightness = false;
uint8_t brightnessrepeat = 2;
uint8_t repeat = 10;

// Declare sequenceNumber as a global variable
uint32_t sequenceNumber = 0;

uint32_t nextSequenceNumber() {
  // Increment sequence number
  sequenceNumber++;

  // Return the new sequence number
  return sequenceNumber;
}

static const byte brightnessSteps_visualremote[] = {
  6, 33, 50, 128 
};


static const size_t numBrightnessSteps_visualremote = sizeof(brightnessSteps_visualremote) / sizeof(byte);


inline void applyBrightness_visualremote() {
  brightnessrepeat--;
  if (brightnessrepeat <= 0)
  {
    UpdateBrightness = false;
  }
  bri = brightnessSteps_visualremote[NextBrightnessStep]; 
  DEBUG_PRINTF("Brightness set: %u\n", bri);
  DEBUG_PRINTF("Brightness step: %u\n", NextBrightnessStep);
  stateUpdated(CALL_MODE_BUTTON);
}

inline void setBrightness_visualremote() {    
  NextBrightnessStep++;
  if (NextBrightnessStep >= sizeof(brightnessSteps_visualremote)) {
    NextBrightnessStep = 0;
  }
  applyBrightness_visualremote();
}

inline void resetBrightness_visualremote() {    
  NextBrightnessStep = sizeof(brightnessSteps_visualremote) - 1;
  applyBrightness_visualremote();
}



inline void broadcastProgram() {
  repeat = 10;
  BroadcastProgram = true;
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

inline void applyPreset_visualremote(uint8_t presetID) {
  if (presetToApply == presetID) {
    if (presetID == 3)
    {
      FeedFish();
      return;;
    }
    increaseSpeed();
    return;
  };
  presetToApply = presetID;  
  applyPreset(presetID, CALL_MODE_BUTTON_PRESET);    
  brightnessrepeat = 2;
  UpdateBrightness = true;
}


struct Pattern {
  uint8_t id;           // ID of the pattern
  String name;          // Name of the pattern
  uint8_t length;       // Actual length of the pattern
  String colors[12];    // Up to 12 colors in HEX string format
};


void magic_flow(uint8_t program) {
  MagicFlowProgram = program;
  if (MagicFlowMode == 0) 
  {
    MagicFlowMode = 1;
    applyPreset_visualremote(9);
    return;
  }
  if (MagicFlowMode == 1) 
  {
    MagicFlowMode = 2;
    applyPreset_visualremote(10);
    return;
  }
  if (MagicFlowMode == 2) 
  {
    MagicFlowMode = 0;
    applyPreset_visualremote(MagicFlowProgram);
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
      top["MagicFlowStart"] = MagicFlowStart;     
    }

    bool readFromConfig(JsonObject& root) override {
      JsonObject top = root[FPSTR(_name)];

      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
      configComplete &= getJsonValue(top["SegmentId"], segmentId, 1);  
      configComplete &= getJsonValue(top["SegmentPixelOffset"], segmentPixelOffset, 0);  
      configComplete &= getJsonValue(top["TimeOutMenu"], timeOutMenu, 300);  
      configComplete &= getJsonValue(top["MagicFlowStart"], MagicFlowStart, 11);     

      return configComplete;
    }


   void onButtonUpPress() {
    if (MagicFlowMode > 0) 
    {
      magic_flow(MagicFlowProgram);
    } else {
      do {
        currentEffectIndex++;
        if (currentEffectIndex >= 255) {
          currentEffectIndex = MagicFlowStart;
        }
      } while (!preset_available[currentEffectIndex]);
      DEBUG_PRINTF("> Start Display effect %d \n", currentEffectIndex);
      applyPreset_visualremote(currentEffectIndex);
    } 
   }

    void onButtonDownPress() {
      MenuMode = true;      
      do {
        if (currentEffectIndex <= MagicFlowStart) {
          currentEffectIndex = 255;
        } else {
          currentEffectIndex--;
        }
      } while (!preset_available[currentEffectIndex]);
      DEBUG_PRINTF("< Start Display effect %d \n", currentEffectIndex);
      applyPreset_visualremote(currentEffectIndex);
    }




    inline void handleRemote_visualremote(uint8_t *incomingData, size_t len) {
      message_structure_t *incoming = reinterpret_cast<message_structure_t *>(incomingData);

      if (len != sizeof(message_structure_t)) {
        Serial.printf("Unknown incoming ESP-NOW message received of length %u\n", len);
        return;
      }
      
      if (strcmp(last_signal_src, linked_remote) != 0) {
        DEBUG_PRINT(F("ESP Now Message Received from Unlinked Sender: "));
        DEBUG_PRINTLN(last_signal_src);     
        DEBUG_PRINTLN(incoming->button);     
        DEBUG_PRINTLN(incoming->program);     
        DEBUG_PRINTLN(incoming->seq[0]);     
        if (!SyncMode) {
          return;
        }
        if (
          (incoming->button != WIZMOTE_BUTTON_PROGRAM)
          ) {
            return;
          }
      }

      if (incoming->button == WIZMOTE_BUTTON_PROGRAM) {
          DEBUG_PRINT(F("Program: "));
        DEBUG_PRINTLN(incoming->program);
        sequenceNumber=0;

      }
      DEBUG_PRINTF("Button value: %u\n", incoming->button);
      uint32_t cur_seq = incoming->seq[0] | (incoming->seq[1] << 8) | (incoming->seq[2] << 16) | (incoming->seq[3] << 24);
      if (cur_seq == last_seq_visualremote) {
        return;
      }

      ButtonPressed = true;
      BroadcastProgram = false;    
      lastTime = millis();

      if ((incoming->button != WIZMOTE_BUTTON_BRIGHT_DOWN_SHORT) && (incoming->button != WIZMOTE_BUTTON_BRIGHT_UP_SHORT)) {
        MenuMode = false;
      }
    
      switch (incoming->button) {
        case WIZMOTE_BUTTON_OFF_SHORT             : togglePower_visualremote();                                            break;
        case WIZMOTE_BUTTON_OFF_LONG            : toggleSyncMode_visualremote();                                           break;
        case WIZMOTE_BUTTON_ON_SHORT            : setBrightness_visualremote();                                           break;
        case WIZMOTE_BUTTON_ON_LONG            : resetBrightness_visualremote();                                           break;
        case WIZMOTE_BUTTON_ONE_SHORT            : applyPreset_visualremote(1);    break;
        case WIZMOTE_BUTTON_TWO_SHORT            : applyPreset_visualremote(2);    break;
        case WIZMOTE_BUTTON_THREE_SHORT          : applyPreset_visualremote(3);    break;
        case WIZMOTE_BUTTON_FOUR_SHORT           : applyPreset_visualremote(4);    break;
        case WIZMOTE_BUTTON_NIGHT_SHORT          : broadcastProgram();                                break;
        case WIZMOTE_BUTTON_NIGHT_DOUBLE         : broadcastProgram();                                break;
        case WIZMOTE_BUTTON_NIGHT_TRIPLE         : broadcastProgram();                                break;
        case WIZMOTE_BUTTON_NIGHT_LONG           : broadcastProgram();                                break;
        case WIZMOTE_BUTTON_BRIGHT_UP_SHORT      : onButtonUpPress();                                               break;
        case WIZMOTE_BUTTON_BRIGHT_UP_LONG      : applyPreset_visualremote(5);    break;
        case WIZMOTE_BUTTON_BRIGHT_DOWN_SHORT    : onButtonDownPress();                                             break;
        case WIZMOTE_BUTTON_BRIGHT_DOWN_LONG    : applyPreset_visualremote(6);    break;

      
        case WIZMOTE_BUTTON_ONE_LONG       : applyPreset_visualremote(1);    break;
        case WIZMOTE_BUTTON_TWO_LONG       : applyPreset_visualremote(2);    break;
        case WIZMOTE_BUTTON_THREE_LONG     : applyPreset_visualremote(3);    break;
        case WIZMOTE_BUTTON_FOUR_LONG      : applyPreset_visualremote(4);    break;
        case WIZMOTE_BUTTON_PROGRAM        : applyPreset_visualremote(incoming->program);    break;

        case WIZMOTE_BUTTON_ONE_DOUBLE     : increaseSpeed();    break;
        case WIZMOTE_BUTTON_TWO_DOUBLE     : increaseSpeed();    break;
        case WIZMOTE_BUTTON_THREE_DOUBLE   : increaseSpeed();    break;
        case WIZMOTE_BUTTON_FOUR_DOUBLE    : increaseSpeed();    break;
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
        strip.getSegment(segmentId).setPixelColor(segmentPixelOffset, BLUE);       
      }

      if (BroadcastProgram)
      {        
        strip.getSegment(segmentId).setPixelColor(segmentPixelOffset, PURPLE);            
      }
      

      if (SyncModeChanged) {     
        if (millis() - lastTime > timeOutMenu) {
          SyncModeChanged = false;
        }   
        uint32_t syncColor = SyncMode ? GREEN : RED;
        strip.fill(syncColor);
        //strip.getSegment(segmentId).setPixelColor(segmentPixelOffset, WHITE);       
      }

    }

    void loop() {
      if (UpdateBrightness) {
        applyBrightness_visualremote();
      }
      // Code to run in the main loop
      if (BroadcastProgram) {
        static time_t lastSend = 60000;
        if (millis () - lastSend >= 1000) {
          lastSend = millis ();
          DEBUG_PRINTLN("Broadcasting program");

          message_structure_t msg;
          msg.button = WIZMOTE_BUTTON_PROGRAM;
          msg.program = presetToApply; 
          // Use next sequence number
          uint32_t seq = nextSequenceNumber();
          msg.seq[0] = seq & 0xFF;
          msg.seq[1] = (seq >> 8) & 0xFF;
          msg.seq[2] = (seq >> 16) & 0xFF;
          msg.seq[3] = (seq >> 24) & 0xFF;
          if (!quickEspNow.send(ESPNOW_BROADCAST_ADDRESS, (uint8_t*)&msg, sizeof(msg)))
          {
                        
            repeat--;
            if (repeat <= 0) {
               BroadcastProgram = false;
            }
            
          } else {
            DEBUG_PRINTLN("Failed to send ESP-NOW message");
          }    
          
        }
      }
    
      
    }



    bool onEspNowMessage(uint8_t* sender, uint8_t* data, uint8_t len) {      
      handleRemote_visualremote(data, len);
      
      return true; // Override further processing
    }

     
};

// add more strings here to reduce flash memory usage
const char UsermodVisualRemote::_name[]    PROGMEM = "VisualRemote";
const char UsermodVisualRemote::_enabled[] PROGMEM = "enabled";
