#pragma once

#include "wled.h"


//class name. Use something descriptive and leave the ": public Usermod" part :)
class UnUserMod : public Usermod {

  private:

    // Private class members. You can declare variables and functions only accessible to your usermod here
    bool enabled = false;
    bool initDone = false;
    unsigned long lastTime = 0;

    // string that are used multiple time (this will save some flash memory)
    static const char _name[];
    static const char _enabled[];


  public:

    // non WLED related methods, may be used for data exchange between usermods (non-inline methods should be defined out of class)

    /**
     * Enable/Disable the usermod
     */
    inline void enable(bool enable) { enabled = enable; }

    /**
     * Get usermod enabled/disabled state
     */
    inline bool isEnabled() { return enabled; }

   
    void setup() {
   
      //Serial.println("Hello from my usermod!");
      initDone = true;
      Serial.println("UnUser mod active!");
    }


    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() {
      Serial.println("Connected to WiFi!");
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
     void loop() {
    
    }
    


    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
   
    }



    bool readFromConfig(JsonObject& root)
    {
     
      JsonObject top = root[FPSTR(_name)];
      bool configComplete = !top.isNull();

      configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);

      

      return configComplete;
    }


   
    /*
     * handleOverlayDraw() is called just before every show() (LED strip update frame) after effects have set the colors.
     * Use this to blank out some LEDs or set them to a different color regardless of the set effect mode.
     * Commonly used for custom clocks (Cronixie, 7 segment)
     */
    void handleOverlayDraw()
    {
        if (!enabled) return;

        // Block the corners of a 16x16 matrix
        uint32_t cornerColor = RGBW32(0, 0, 0, 0); // Black color for the corners
        blockCorners(cornerColor);      
    }

    void blockCorners(uint32_t color) {
        int width = 16;
        int height = 16;
        int N = 3; // Size of the cut-off (number of pixels from the corner)

        // Top-left corner
        for (int y = 0; y < N; y++) {
            for (int x = 0; x <= N - y - 1; x++) {
                int index = y * width + x;
                strip.setPixelColor(index, color);
            }
        }

        // Top-right corner
        for (int y = 0; y < N; y++) {
            for (int x = width - 1; x >= width - N + y; x--) {
                int index = y * width + x;
                strip.setPixelColor(index, color);
            }
        }

        // Bottom-left corner
        for (int y = height - N; y < height; y++) {
            for (int x = 0; x <= y - (height - N); x++) {
                int index = y * width + x;
                strip.setPixelColor(index, color);
            }
        }

        // Bottom-right corner
        for (int y = height - N; y < height; y++) {
            for (int x = width - 1; x >= width - (y - (height - N)) - 1; x--) {
                int index = y * width + x;
                strip.setPixelColor(index, color);
            }
        }
    }

};


// add more strings here to reduce flash memory usage
const char UnUserMod::_name[]    PROGMEM = "UnUserMod";
const char UnUserMod::_enabled[] PROGMEM = "enabled";

