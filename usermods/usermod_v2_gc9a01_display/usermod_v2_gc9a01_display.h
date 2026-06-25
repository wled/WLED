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

/**
 * Create the UsermodGC9A01Display singleton instance if one does not already exist.
 */
 
/**
 * Return the singleton instance of UsermodGC9A01Display.
 * @returns Pointer to the singleton instance, or nullptr if not constructed.
 */

/**
 * Initialize the TFT display hardware and internal display state.
 */

/**
 * Render the main interface. If overlayMode is >= 0, render the specified overlay instead of the normal UI.
 * @param overlayMode Overlay mode to render, or -1 to render the normal interface.
 */

/**
 * Draw the WLED logo on the display.
 */

/**
 * Draw a WiFi connectivity icon at the specified position.
 * @param x X coordinate in pixels.
 * @param y Y coordinate in pixels.
 * @param connected `true` to render as connected, `false` to render as disconnected.
 * @param rssi RSSI value to reflect signal strength (optional).
 */

/**
 * Set display brightness used for UI elements and indicators.
 * @param bri Brightness value (0-255).
 */

/**
 * Set the physical backlight level as a percentage.
 * @param percent Backlight brightness percentage (0-100).
 */

/**
 * Switch display between sleep behavior and clock-only behavior based on `enabled`.
 * @param enabled If `true`, enable clock/sleep behavior; if `false`, disable it.
 */

/**
 * Put the display into a low-power or off state.
 */

/**
 * Wake the display from sleep if it is sleeping.
 * @returns `true` if the display was sleeping and was woken, `false` otherwise.
 */

/**
 * Render the clock-only screen.
 */

/**
 * Render the current mode overlay (mode-specific information).
 */

/**
 * Draw an indicator for the currently active WLED mode.
 */

/**
 * Return a human-readable name for the given encoder mode.
 * @param mode Encoder mode identifier.
 * @returns Null-terminated string describing the mode.
 */

/**
 * Wake the display if it is sleeping.
 * @returns `true` if the display was sleeping and was woken, `false` otherwise.
 */

/**
 * Reset or update the inactivity timer to prevent the display from timing out.
 */

/**
 * Show a temporary overlay with a single line of text and optional glyph.
 * @param line1 Null-terminated string to display.
 * @param showHowLong Duration in milliseconds to show the overlay.
 * @param glyphType Optional glyph type identifier (default 0).
 */

/**
 * Trigger a display redraw.
 * @param forceRedraw If `true`, redraw regardless of internal change tracking.
 */

/**
 * Check whether an overlay is currently active.
 * @returns `true` if an overlay is active, `false` otherwise.
 */

/**
 * Get the currently active overlay mode.
 * @returns Active overlay mode number, or -1 if none is active.
 */

/**
 * Check whether the display is currently asleep.
 * @returns `true` if the display is sleeping, `false` otherwise.
 */

/**
 * Usermod setup hook called once after initialization to configure the display and state.
 */

/**
 * Usermod loop hook called regularly to handle updates, timeouts, and redraw scheduling.
 */

/**
 * Append informational state about the display to the provided JSON object.
 * @param root JSON object to which information will be added.
 */

/**
 * Read transient state values from JSON (runtime state).
 * @param root JSON object containing state values.
 */

/**
 * Append transient state values to JSON (runtime state).
 * @param root JSON object to populate with state values.
 */

/**
 * Read persistent configuration for the display from JSON.
 * @param root JSON object containing configuration.
 * @returns `true` if configuration was successfully read, `false` otherwise.
 */

/**
 * Append persistent configuration for the display to JSON.
 * @param root JSON object to populate with configuration values.
 */

/**
 * Append additional configuration data to the global configuration payload.
 */

/**
 * Return the unique usermod ID for this display usermod.
 * @returns Numeric usermod ID.
 */
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