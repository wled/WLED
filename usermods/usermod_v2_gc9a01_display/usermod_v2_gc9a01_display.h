#pragma once
#include "wled.h"

#ifdef USERMOD_GC9A01_DISPLAY

#include <TFT_eSPI.h>
#include <SPI.h>

// Pin definitions - use TFT_eSPI definitions when available
#ifndef TFT_CS
  #define TFT_CS 5  // Default fallback
#endif
#define GC9A01_CS_PIN TFT_CS

#ifndef TFT_DC  
  #define TFT_DC 16  // Default fallback
#endif
#define GC9A01_DC_PIN TFT_DC

#ifndef TFT_RST
  #define TFT_RST 17  // Default fallback  
#endif
#define GC9A01_RST_PIN TFT_RST

// Use TFT_eSPI's backlight pin definition
#ifndef TFT_BL
  #define TFT_BL 4  // Default fallback if not defined by TFT_eSPI
#endif

#ifndef USERMOD_ID_GC9A01_DISPLAY
  #define USERMOD_ID_GC9A01_DISPLAY 59  // Use the official ID from const.h
#endif

class UsermodGC9A01Display : public Usermod {
  private:
    // Singleton pattern - allows rotary encoder usermod to find us
    static UsermodGC9A01Display* instance;
    
  public:
    UsermodGC9A01Display() { if (!instance) instance = this; }
    static UsermodGC9A01Display* getInstance(void) { return instance; }
    
  private:
    TFT_eSPI tft = TFT_eSPI();
    
    bool displayEnabled = true;
    bool needsRedraw = true;
    bool displayTurnedOff = false;
    bool showingWelcomeScreen = true;
    bool showingClock = false; // Track if currently showing clock (for rotary encoder state reset)
    unsigned long welcomeScreenStartTime = 0;
    uint8_t backlight = 75; // Backlight brightness percentage (0-100%, default 75%)
    uint16_t displayTimeout = 60000; // 60 seconds default
    bool sleepMode = true; // Enable sleep mode by default
    bool clockMode = false; // Show clock only when idle
    bool flip = false; // Display rotation (0 or 2)
    bool clock12hour = false; // false = 24h format, true = 12h format with AM/PM
    
    // Proper state tracking like 4-line display usermod
    uint8_t knownBrightness = 255;
    uint8_t knownMode = 255;
    uint8_t knownPalette = 255;
    uint8_t knownEffectSpeed = 255;
    uint8_t knownEffectIntensity = 255;
    uint32_t knownColor = 0;      // colors[0] - Primary/FX
    uint32_t knownBgColor = 0;    // colors[1] - Secondary/BG
    uint32_t knownCustomColor = 0; // colors[2] - Tertiary/CS
    bool knownPowerState = true;
    unsigned long nextUpdate = 0;
    unsigned long lastRedraw = ULONG_MAX; // Initialize to max value - sleep timer starts after first interaction
    uint16_t refreshRate = 1000; // Match 4-line display usermod (1 second) for better performance
    
    // Time tracking for display updates
    uint8_t knownMinute = 99;
    uint8_t knownHour = 99;
    
    // Integration with rotary encoder usermod (no direct pin handling)
    unsigned long overlayUntil = 0;  // When overlay should expire (millis)
    int activeOverlayMode = -1;      // Which overlay mode is active (-1 = none, 0-4 = overlay modes)
    unsigned long overlayInactivityTimeout = 3000; // 3 seconds of inactivity before returning to main screen
    String overlayText = "";
    
    // Private method declarations
    void initDisplay();
    void drawMainInterface(int overlayMode = -1); // -1 = normal, 0+ = overlay mode
    void drawWLEDLogo();
    void drawWiFiIcon(int x, int y, bool connected, int rssi = 0);
    void setBrightness(uint8_t bri);
    void setBacklight(uint8_t percent); // Set backlight brightness (0-100%)
    void sleepOrClock(bool enabled); // Sleep display or show clock based on settings
    void sleepDisplay();
    bool wakeDisplayFromSleep(); // Return true if display was sleeping
    void drawClockScreen(); // Clock-only display mode
    
    // Mode-specific drawing methods
    void drawModeOverlay();
    void drawCurrentModeIndicator();
    
    // Get encoder state from rotary encoder usermod
    const char* getEncoderModeName(uint8_t mode);

  public:
    
    // Public interface methods for rotary encoder usermod (like 4-line display)
    bool wakeDisplay(); // Return true if was sleeping
    void updateRedrawTime(); // Prevent display timeout
    void overlay(const char* line1, long showHowLong, byte glyphType = 0); // Match 4-line interface
    void redraw(bool forceRedraw); // Force display update
    bool isOverlayActive(); // Check if overlay is currently showing
    int getActiveOverlayMode(); // Get current overlay mode (-1 = none)
    bool isDisplayAsleep(); // Check if display is sleeping
    
    // Usermod API
    // Public method declarations (Usermod interface)
    void setup() override;
    void loop() override;
    void addToJsonInfo(JsonObject& root) override;
    void readFromJsonState(JsonObject& root) override;
    void addToJsonState(JsonObject& root) override;
    bool readFromConfig(JsonObject& root) override;
    void addToConfig(JsonObject& root) override;
    void appendConfigData() override;
    uint16_t getId() override;
};

#endif // USERMOD_GC9A01_DISPLAY