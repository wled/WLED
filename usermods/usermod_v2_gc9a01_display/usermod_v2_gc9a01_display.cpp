// WLED usermod for GC9A01 240x240 TFT display
// Designed to work with TFT_eSPI library and compatible displays
// Inspired by 4-line display usermod and rotary encoder usermod
// Written by AI copilot based on user request

#include "usermod_v2_gc9a01_display.h"
#include "logo_data.h"

#ifdef USERMOD_GC9A01_DISPLAY

// Static instance definition for singleton pattern
UsermodGC9A01Display* UsermodGC9A01Display::instance = nullptr;

/**
 * @brief Initialize the GC9A01 TFT display and show the welcome logo.
 *
 * If the display is disabled this function returns immediately. When enabled,
 * it configures the backlight pin (if TFT_BL is defined), initializes the
 * TFT controller, applies rotation according to the flip setting, clears the
 * screen, applies the current backlight brightness, marks the welcome screen
 * as active (sets showingWelcomeScreen and welcomeScreenStartTime), and
 * renders the centered WLED logo.
 */
void UsermodGC9A01Display::initDisplay() {
  if (!displayEnabled) return;
  
  DEBUG_PRINTLN(F("[GC9A01] Initializing TFT display..."));
  
  // Configure backlight pin
  #ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  DEBUG_PRINTF("[GC9A01] Backlight pin %d set to HIGH\n", TFT_BL);
  #endif
  
  // Initialize TFT
  tft.init();
  DEBUG_PRINTLN(F("[GC9A01] TFT init() completed"));
  
  tft.setRotation(flip ? 2 : 0); // Apply flip setting (0 or 180 degrees)
  tft.fillScreen(TFT_BLACK);
  setBacklight(backlight); // Apply backlight brightness
  
  DEBUG_PRINTLN(F("[GC9A01] Display initialization complete"));
  
  // Set up welcome screen
  showingWelcomeScreen = true;
  welcomeScreenStartTime = millis();
  drawWLEDLogo();
}

/**
 * @brief Update the GC9A01 display contents based on current device and overlay state.
 *
 * This method evaluates welcome-screen timing, overlay activity, brightness/mode/palette/speed/intensity/color changes,
 * sleep/clock timeouts, and wakes or sleeps the display as needed before drawing the appropriate screen (welcome logo,
 * overlay, main interface, or clock). It also updates internal "known" state used to minimize redraws.
 *
 * @param forceRedraw If true, forces a full redraw of the main interface regardless of detected state changes.
 */
void UsermodGC9A01Display::redraw(bool forceRedraw) {
  if (!displayEnabled) return;
  
  bool needRedraw = false;
  unsigned long now = millis();
  
  // Handle welcome screen transition
  if (showingWelcomeScreen) {
    if (now - welcomeScreenStartTime > 2000) { // Show logo for 2 seconds
      showingWelcomeScreen = false;
      forceRedraw = true;
      DEBUG_PRINTLN(F("[GC9A01] Transitioning from logo to main interface"));
    } else {
      // Still showing welcome screen - don't redraw
      return;
    }
  }
  
  // Handle active overlay mode (always takes priority)
  if (activeOverlayMode >= 0) {
    if (now >= overlayUntil) {
      // Overlay has expired - return to main/clock interface
      DEBUG_PRINTF("[GC9A01] Overlay mode %d expired - returning to main interface\n", activeOverlayMode);
      activeOverlayMode = -1;
      overlayUntil = 0;
      forceRedraw = true;
      needRedraw = true; // Force redraw to show main screen
      lastRedraw = now; // Reset sleep timer when returning to main screen
    } else {
      // Overlay is still active - check if state changed
      if (knownBrightness != bri) {
        if (displayTurnedOff) needRedraw = true;
        else { knownBrightness = bri; drawMainInterface(activeOverlayMode); lastRedraw = now; return; }
      } else if (knownMode != effectCurrent) {
        if (displayTurnedOff) needRedraw = true;
        else { knownMode = effectCurrent; drawMainInterface(activeOverlayMode); lastRedraw = now; return; }
      } else if (knownPalette != effectPalette) {
        if (displayTurnedOff) needRedraw = true;
        else { knownPalette = effectPalette; drawMainInterface(activeOverlayMode); lastRedraw = now; return; }
      } else if (knownEffectSpeed != effectSpeed) {
        if (displayTurnedOff) needRedraw = true;
        else { knownEffectSpeed = effectSpeed; drawMainInterface(activeOverlayMode); lastRedraw = now; return; }
      } else if (knownEffectIntensity != effectIntensity) {
        if (displayTurnedOff) needRedraw = true;
        else { knownEffectIntensity = effectIntensity; drawMainInterface(activeOverlayMode); lastRedraw = now; return; }
      }
      
      if (!needRedraw) return; // Overlay active, no changes, nothing to do
    }
  }
  
  // Get current segment colors (same as web UI csl0, csl1, csl2)
  uint32_t currentColor = strip.getMainSegment().colors[0];       // csl0 - Primary/FX
  uint32_t currentBgColor = strip.getMainSegment().colors[1];     // csl1 - Secondary/BG
  uint32_t currentCustomColor = strip.getMainSegment().colors[2]; // csl2 - Tertiary/CS
  
  // Check for state changes (Four Line Display ALT pattern)
  if (forceRedraw) {
    needRedraw = true;
  } else if (knownMode != effectCurrent || knownPalette != effectPalette) {
    if (displayTurnedOff) needRedraw = true;
    else {
      knownMode = effectCurrent;
      knownPalette = effectPalette;
      drawMainInterface(-1); 
      lastRedraw = now;
      return;
    }
  } else if (knownColor != currentColor || knownBgColor != currentBgColor || knownCustomColor != currentCustomColor) {
    if (displayTurnedOff) needRedraw = true;
    else {
      knownColor = currentColor;
      knownBgColor = currentBgColor;
      knownCustomColor = currentCustomColor;
      drawMainInterface(-1);
      lastRedraw = now;
      return;
    }
  } else if (knownBrightness != bri) {
    if (displayTurnedOff) needRedraw = true;
    else { 
      knownBrightness = bri; 
      drawMainInterface(-1); 
      lastRedraw = now; 
      return; 
    }
  } else if (knownEffectSpeed != effectSpeed) {
    if (displayTurnedOff) needRedraw = true;
    else { 
      knownEffectSpeed = effectSpeed; 
      drawMainInterface(-1); 
      lastRedraw = now; 
      return; 
    }
  } else if (knownEffectIntensity != effectIntensity) {
    if (displayTurnedOff) needRedraw = true;
    else { 
      knownEffectIntensity = effectIntensity; 
      drawMainInterface(-1); 
      lastRedraw = now; 
      return; 
    }
  }
  
  // Nothing changed - check what to do
  if (!needRedraw) {
    // Turn off display after configured timeout (or show clock if enabled)
    if (sleepMode && displayTimeout > 0 && !displayTurnedOff && lastRedraw != ULONG_MAX && (now - lastRedraw > displayTimeout)) {
      sleepOrClock(true);
    } else if (displayTurnedOff && clockMode) {
      // Keep updating clock while display is "off" (showing clock)
      static unsigned long lastClockUpdate = 0;
      if (now - lastClockUpdate > 30000) {
        drawClockScreen();
        lastClockUpdate = now;
      }
    }
    return;
  }
  
  // State changed or need to redraw - wake up if sleeping and redraw main screen
  lastRedraw = now;
  wakeDisplayFromSleep();
  
  // Update all known values
  knownBrightness = bri;
  knownMode = effectCurrent;
  knownPalette = effectPalette;
  knownEffectSpeed = effectSpeed;
  knownEffectIntensity = effectIntensity;
  knownColor = currentColor;
  knownBgColor = currentBgColor;
  knownCustomColor = currentCustomColor;
  
  // Do full redraw of main screen (not overlay)
  drawMainInterface(-1);
}

/**
 * @brief Render the main display UI or a specific overlay on the GC9A01 240x240 TFT.
 *
 * Renders either the primary interface (time, WiFi, power state, effect name,
 * color indicators, and a brightness ring) or a focused overlay showing a
 * single adjustable property. When an overlay is active, the function draws
 * a themed bezel and overlay title/value (or network information) and returns
 * without drawing the normal main-screen elements.
 *
 * Supported overlayMode values:
 * - -1 : normal main interface
 * -  0 : Brightness overlay (percentage + brightness arc)
 * -  1 : Effect overlay (effect name)
 * -  2 : Speed overlay (percentage)
 * -  3 : Intensity overlay (percentage)
 * -  4 : Palette overlay (palette name)
 * - 99 : Network info overlay (IP/MAC and WiFi icon)
 *
 * @param overlayMode Overlay selector; use -1 for the standard main screen or
 *                    one of the values listed above to render the corresponding overlay.
 */
void UsermodGC9A01Display::drawMainInterface(int overlayMode) {
  tft.fillScreen(TFT_BLACK);
  
  showingClock = false; // Clear clock flag - we're showing main interface
  
  // Determine bezel color based on mode
  uint16_t bezelColor = TFT_BLUE; // Default blue bezel for normal mode
  bool showBrightnessRing = true;
  bool showAllElements = true;
  String overlayTitle = "";
  uint16_t overlayColor = TFT_WHITE;
  
  // Modify display for overlay modes (>= 0)
  if (overlayMode >= 0) {
    showAllElements = false;
    
    // Use the stored overlayText if available, otherwise use default titles
    overlayTitle = (overlayText.length() > 0) ? overlayText : "Mode";
    
    switch (overlayMode) {
      case 0: // Brightness mode
        bezelColor = TFT_WHITE;
        overlayColor = TFT_WHITE;
        showBrightnessRing = true;
        break;
      case 1: // Effect mode  
        bezelColor = TFT_CYAN;
        overlayColor = TFT_CYAN;
        showBrightnessRing = false;
        break;
      case 2: // Speed mode
        bezelColor = TFT_GREEN;
        overlayColor = TFT_GREEN;
        showBrightnessRing = false;
        break;
      case 3: // Intensity mode
        bezelColor = TFT_ORANGE;
        overlayColor = TFT_ORANGE;
        showBrightnessRing = false;
        break;
      case 4: // Palette mode
        bezelColor = TFT_MAGENTA;
        overlayColor = TFT_MAGENTA;
        showBrightnessRing = false;
        break;
      case 99: // Network info mode
        bezelColor = TFT_GREEN;
        overlayColor = TFT_GREEN;
        showBrightnessRing = false;
        showAllElements = false;
        break;
    }
  }
  
  // Get static segment colors
  uint32_t fxColor = strip.getMainSegment().colors[0]; // Primary color
  uint8_t fx_r = (fxColor >> 16) & 0xFF;
  uint8_t fx_g = (fxColor >> 8) & 0xFF;
  uint8_t fx_b = fxColor & 0xFF;
  uint16_t fxColor565 = tft.color565(fx_r, fx_g, fx_b);
  
  uint32_t bgColor = strip.getMainSegment().colors[1]; // Secondary color
  uint8_t bg_r = (bgColor >> 16) & 0xFF;
  uint8_t bg_g = (bgColor >> 8) & 0xFF;
  uint8_t bg_b = bgColor & 0xFF;
  uint16_t bgColor565 = tft.color565(bg_r, bg_g, bg_b);
  
  // Draw outer circle border
  tft.drawCircle(120, 120, 115, bezelColor);
  tft.drawCircle(120, 120, 114, bezelColor);
  
  // Special handling for network info
  if (overlayMode == 99) {
    tft.setTextDatum(MC_DATUM);
    bool wifiConnected = (WiFi.status() == WL_CONNECTED);
    int wifiRSSI = wifiConnected ? WiFi.RSSI() : -100;
    
    tft.fillCircle(120, 40, 12, TFT_BLUE);
    drawWiFiIcon(120, 36, wifiConnected, wifiRSSI);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    
    if (wifiConnected) {
      String ipStr = WiFi.localIP().toString();
      tft.drawString(ipStr, 120, 110, 4);
      String macStr = WiFi.macAddress();
      tft.drawString(macStr, 120, 140, 2);
    } else {
      tft.drawString("NOT CONNECTED", 120, 110, 4);
      String macStr = WiFi.macAddress();
      tft.drawString(macStr, 120, 140, 2);
    }
    return;
  }
  
  // Draw brightness ring from 8 o'clock to 4 o'clock (only on main screen or brightness overlay)
  int brightnessPercent = (bri > 0) ? map(bri, 0, 255, 0, 100) : 0;
  
  if (showBrightnessRing && (overlayMode == -1 || overlayMode == 0) && brightnessPercent >= 0 && brightnessPercent <= 100) {
    float startAngle = 240; // 8 o'clock
    float arcLength = 240;
    float progressAngle = (brightnessPercent > 0) ? map(brightnessPercent, 0, 100, 0, arcLength) : 0;
    
    // Draw background arc
    for (float angle = 0; angle < arcLength; angle += 6) {
      float currentAngle = startAngle + angle;
      float rad = radians(currentAngle - 90);
      
      for (int ringWidth = 0; ringWidth < 3; ringWidth++) {
        int radius = 108 - ringWidth;
        int x = 120 + radius * cos(rad);
        int y = 120 + radius * sin(rad);
        
        if (x >= 0 && x < 240 && y >= 0 && y < 240) {
          tft.drawPixel(x, y, TFT_DARKGREY);
        }
      }
    }
    
    // Draw progress arc
    if (brightnessPercent > 0 && progressAngle > 0) {
      for (float angle = 0; angle < progressAngle; angle += 3) {
        float currentAngle = startAngle + angle;
        float rad = radians(currentAngle - 90);
        
        for (int ringWidth = 0; ringWidth < 3; ringWidth++) {
          int radius = 108 - ringWidth;
          int x = 120 + radius * cos(rad);
          int y = 120 + radius * sin(rad);
          
          if (x >= 0 && x < 240 && y >= 0 && y < 240) {
            tft.drawPixel(x, y, TFT_WHITE);
          }
        }
      }
    }
  }
  
  // Show overlay-specific content if in overlay mode
  if (overlayMode >= 0 && overlayTitle.length() > 0) {
    // Draw overlay title at top (white color, moved down 20px total)
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(overlayTitle, 120, 70, 4);
    
    // Get current value and calculate percentage based on overlay mode
    int currentValue = 0;
    int maxValue = 255;
    String valueText = "";
    
    switch (overlayMode) {
      case 1: // Effect mode
        currentValue = effectCurrent;
        maxValue = strip.getModeCount() - 1;
        if (currentValue < strip.getModeCount()) {
          char lineBuffer[64];
          if (extractModeName(currentValue, JSON_mode_names, lineBuffer, 63)) {
            // Remove note symbol from effect names (special character prefix)
            String modeName = String(lineBuffer);
            if (modeName.length() > 0 && modeName.charAt(0) == ' ' && modeName.charAt(1) > 127) {
              modeName = modeName.substring(5); // Remove 5-byte UTF-8 note symbol
            }
            if (modeName.length() > 12) modeName = modeName.substring(0, 10) + "..";
            valueText = modeName;
          } else {
            valueText = "Effect " + String(currentValue);
          }
        }
        break;
      case 2: // Speed mode
        currentValue = effectSpeed;
        valueText = String(map(effectSpeed, 0, 255, 0, 100)) + "%";
        break;
      case 3: // Intensity mode
        currentValue = effectIntensity;
        valueText = String(map(effectIntensity, 0, 255, 0, 100)) + "%";
        break;
      case 4: // Palette mode
        currentValue = effectPalette;
        maxValue = getPaletteCount() - 1;
        if (currentValue < getPaletteCount()) {
          char lineBuffer[64];
          if (extractModeName(currentValue, JSON_palette_names, lineBuffer, 63)) {
            // Remove "* " prefix from dynamic palettes
            String paletteName = String(lineBuffer);
            if (paletteName.startsWith("* ")) {
              paletteName = paletteName.substring(2);
            }
            if (paletteName.length() > 12) paletteName = paletteName.substring(0, 10) + "..";
            valueText = paletteName;
          } else {
            valueText = "Palette " + String(currentValue);
          }
        } else {
          valueText = "Palette " + String(effectPalette);
        }
        break;
      default: // Fallback
        currentValue = bri;
        valueText = String(map(bri, 0, 255, 0, 100)) + "%";
        break;
    }
    
    // Draw value arc from 8 o'clock to 4 o'clock (only for percentage-based overlays)
    // Effect (1) and Palette (4) overlays don't need arcs as they show names
    bool showArc = (overlayMode != 1 && overlayMode != 4);
    
    if (showArc) {
      int valuePercent = map(currentValue, 0, maxValue, 0, 100);
      float startAngle = 240; // 8 o'clock
      float arcLength = 240;
      float progressAngle = (valuePercent > 0) ? map(valuePercent, 0, 100, 0, arcLength) : 0;
      
      // Draw background arc (darkgrey)
      for (float angle = 0; angle < arcLength; angle += 6) {
        float currentAngle = startAngle + angle;
        float rad = radians(currentAngle - 90);
        
        for (int ringWidth = 0; ringWidth < 3; ringWidth++) {
          int radius = 108 - ringWidth;
          int x = 120 + radius * cos(rad);
          int y = 120 + radius * sin(rad);
          
          if (x >= 0 && x < 240 && y >= 0 && y < 240) {
            tft.drawPixel(x, y, TFT_DARKGREY);
          }
        }
      }
      
      // Draw progress arc (white progress)
      if (valuePercent > 0 && progressAngle > 0) {
        for (float angle = 0; angle < progressAngle; angle += 3) {
          float currentAngle = startAngle + angle;
          float rad = radians(currentAngle - 90);
          
          for (int ringWidth = 0; ringWidth < 3; ringWidth++) {
            int radius = 108 - ringWidth;
            int x = 120 + radius * cos(rad);
            int y = 120 + radius * sin(rad);
            
            if (x >= 0 && x < 240 && y >= 0 && y < 240) {
              tft.drawPixel(x, y, TFT_WHITE);
            }
          }
        }
      }
    }
    
    // Draw value text in center
    // Use smaller font (4) for Effect and Palette names, large font (6) for percentages
    int fontSize = (overlayMode == 1 || overlayMode == 4) ? 4 : 6;
    int yOffset = (overlayMode == 1 || overlayMode == 4) ? 120 : 130; // Slightly higher for smaller font
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(valueText, 126, yOffset, fontSize);
    
    // Don't draw main screen elements - return early
    return;
  }
  
  // === Main screen elements (only drawn when NOT in overlay mode) ===
  
  // WiFi icon
  tft.setTextDatum(MC_DATUM);
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  int wifiRSSI = wifiConnected ? WiFi.RSSI() : -100;
  
  tft.fillCircle(120, 40, 12, TFT_BLUE);
  drawWiFiIcon(120, 36, wifiConnected, wifiRSSI);
  
  // Time display
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  
  String timeStr = "--:--";
  if (localTime != 0) {
    uint8_t currentHour = hour(localTime);
    uint8_t currentMinute = minute(localTime);
    
    timeStr = "";
    if (currentHour < 10) timeStr += "0";
    timeStr += String(currentHour);
    timeStr += ":";
    if (currentMinute < 10) timeStr += "0";
    timeStr += String(currentMinute);
  }
  tft.drawString(timeStr, 120, 95, 6);
  
  // Power switch
  bool powerState = (bri > 0);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  
  if (powerState) {
    tft.fillRoundRect(105, 123, 30, 15, 7, TFT_GREEN);
    tft.fillCircle(125, 130, 6, TFT_WHITE);
    tft.drawString("ON", 150, 130, 2);
  } else {
    tft.fillRoundRect(105, 123, 30, 15, 7, TFT_RED);
    tft.fillCircle(115, 130, 6, TFT_WHITE);
    tft.drawString("OFF", 90, 130, 2);
  }
  
  // CSL2 button (tertiary color)
  uint32_t csl2Color = strip.getMainSegment().colors[2];
  uint8_t csl2_r = (csl2Color >> 16) & 0xFF;
  uint8_t csl2_g = (csl2Color >> 8) & 0xFF;
  uint8_t csl2_b = csl2Color & 0xFF;
  uint16_t csl2Color565 = tft.color565(csl2_r, csl2_g, csl2_b);
  
  tft.setTextDatum(TL_DATUM);
  tft.drawString("CS", 94, 154, 2);
  tft.setTextDatum(MC_DATUM);
  tft.fillCircle(120, 162, 8, csl2Color565);
  tft.drawCircle(120, 162, 8, TFT_WHITE);
  
  // Effect name
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  String effectName = "";
  if (currentPlaylist >= 0) {
    effectName = "Playlist";
  } else if (knownMode < strip.getModeCount() && knownMode >= 0) {
    char modeBuffer[64];
    strncpy_P(modeBuffer, strip.getModeData(knownMode), sizeof(modeBuffer)-1);
    modeBuffer[sizeof(modeBuffer)-1] = '\0';
    
    char* sepPtr = strpbrk(modeBuffer, "@;,|=");
    if (sepPtr) *sepPtr = '\0';
    
    effectName = String(modeBuffer);
    
    String cleanName = "";
    for (int i = 0; i < effectName.length(); i++) {
      char c = effectName.charAt(i);
      if (c >= 32 && c <= 126 && c != '@') {
        cleanName += c;
      }
    }
    effectName = cleanName;
    
    if (effectName.length() == 0) {
      effectName = "Effect " + String(knownMode);
    }
  } else {
    effectName = "Unknown";
  }
  
  if (effectName.length() > 12) {
    effectName = effectName.substring(0, 9) + "...";
  }
  tft.drawString(effectName, 120, 190, 2);
  
  // Color indicators
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // FX indicator at 7:30 position
  int fx_x = 120 + 90 * cos(radians(225 - 90));
  int fx_y = 120 + 90 * sin(radians(225 - 90));
  tft.drawString("FX", fx_x, fx_y - 15, 2);
  tft.fillCircle(fx_x, fx_y, 8, fxColor565);
  tft.drawCircle(fx_x, fx_y, 8, TFT_WHITE);
  
  // BG indicator at 4:30 position  
  int bg_x = 120 + 90 * cos(radians(135 - 90));
  int bg_y = 120 + 90 * sin(radians(135 - 90));
  tft.drawString("BG", bg_x, bg_y - 15, 2);
  tft.fillCircle(bg_x, bg_y, 8, bgColor565);
  tft.drawCircle(bg_x, bg_y, 8, TFT_WHITE);
  
  // Brightness percentage
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  String brightStr = String(brightnessPercent) + "%";
  tft.drawString(brightStr, 120, 210, 2);
}

/**
 * @brief Draws a four-bar WiFi signal icon at the given screen coordinates.
 *
 * Draws four vertical bars of increasing height representing signal strength and,
 * when not connected, overlays a red "X". RSSI (in dBm) is mapped to 0–4 bars:
 * ≥ -50 → 4, ≥ -60 → 3, ≥ -70 → 2, ≥ -80 → 1, otherwise 0.
 *
 * @param x X coordinate of the icon's center baseline (pixels).
 * @param y Y coordinate of the icon's top baseline (pixels).
 * @param connected If true, active bars are drawn in white and inactive in dark gray; if false, all bars are dark gray and a red "X" is drawn.
 * @param rssi WiFi signal strength in dBm (used only when `connected` is true).
 */

void UsermodGC9A01Display::drawWiFiIcon(int x, int y, bool connected, int rssi) {
  // Draw WiFi icon as signal strength bars (like mobile phone signal)
  // More recognizable than arcs on small displays
  
  uint16_t strongColor, weakColor;
  int signalStrength = 0;
  
  if (connected) {
    strongColor = TFT_WHITE;
    weakColor = TFT_DARKGREY;
    
    // Convert RSSI to signal strength (0-4 bars)
    if (rssi >= -50) signalStrength = 4;      // Excellent (100%)
    else if (rssi >= -60) signalStrength = 3; // Good (75%)
    else if (rssi >= -70) signalStrength = 2; // Fair (50%)
    else if (rssi >= -80) signalStrength = 1; // Poor (25%)
    else signalStrength = 0;                  // Very poor
  } else {
    strongColor = TFT_DARKGREY;
    weakColor = TFT_DARKGREY;
    signalStrength = 0;
  }
  
  // Draw 4 signal bars of increasing height (like phone signal indicator)
  // Bar 1 (shortest, leftmost)
  uint16_t bar1Color = (signalStrength >= 1) ? strongColor : weakColor;
  tft.fillRect(x - 6, y + 6, 2, 2, bar1Color);
  
  // Bar 2
  uint16_t bar2Color = (signalStrength >= 2) ? strongColor : weakColor;
  tft.fillRect(x - 3, y + 4, 2, 4, bar2Color);
  
  // Bar 3
  uint16_t bar3Color = (signalStrength >= 3) ? strongColor : weakColor;
  tft.fillRect(x, y + 2, 2, 6, bar3Color);
  
  // Bar 4 (tallest, rightmost)
  uint16_t bar4Color = (signalStrength >= 4) ? strongColor : weakColor;
  tft.fillRect(x + 3, y, 2, 8, bar4Color);
  
  // Draw X if disconnected
  if (!connected) {
    tft.drawLine(x - 6, y, x + 6, y + 8, TFT_RED);
    tft.drawLine(x + 6, y, x - 6, y + 8, TFT_RED);
  }
}

/**
 * @brief Render the WLED logo centered on the 240x240 display.
 *
 * Clears the screen to black and draws the 120x120 WLED logo centered (60px offset on each side).
 * The bitmap is read from program memory (epd_bitmap_), converted from 32-bit ARGB888 to 16-bit RGB565,
 * and streamed to the display within a single draw window.
 */
void UsermodGC9A01Display::drawWLEDLogo() {
  // Display the WLED logo bitmap centered on the display
  // The bitmap is 120x120 pixels, centered on 240x240 display (60px offset on each side)
  
  DEBUG_PRINTLN(F("[GC9A01] Drawing WLED logo bitmap..."));
  
  // Clear screen with black background
  tft.fillScreen(TFT_BLACK);
  
  // Calculate center position for 120x120 logo on 240x240 display
  const int LOGO_SIZE = 120;
  const int OFFSET_X = (240 - LOGO_SIZE) / 2; // 60px offset
  const int OFFSET_Y = (240 - LOGO_SIZE) / 2; // 60px offset
  
  // Set drawing window to the logo area (centered)
  tft.setAddrWindow(OFFSET_X, OFFSET_Y, LOGO_SIZE, LOGO_SIZE);
  
  // Start data transmission
  tft.startWrite();
  
  // Process each pixel in the bitmap
  for (int i = 0; i < 14400; i++) { // 120 * 120 = 14,400 pixels
    uint32_t pixel = pgm_read_dword(&epd_bitmap_[i]);
    
    // Extract RGB components from 32-bit ARGB (format: 0x00RRGGBB)
    uint8_t r = (pixel >> 16) & 0xFF;
    uint8_t g = (pixel >> 8) & 0xFF;
    uint8_t b = pixel & 0xFF;
    
    // Convert to 16-bit RGB565 format
    uint16_t color = tft.color565(r, g, b);
    
    // Write pixel to display
    tft.pushColor(color);
  }
  
  // End data transmission
  tft.endWrite();
  
  DEBUG_PRINTLN(F("[GC9A01] WLED logo bitmap rendered successfully - 120x120 centered"));
}

/**
 * @brief Accepts a legacy 0–255 brightness value and applies it to the display backlight.
 *
 * Maps the 0–255 input range to a 0–100% backlight level and updates the display backlight.
 *
 * @param bri Brightness in the legacy 0–255 scale.
 */
void UsermodGC9A01Display::setBrightness(uint8_t bri) {
  // Legacy method - map 0-255 brightness to 0-100% backlight
  uint8_t percent = map(bri, 0, 255, 0, 100);
  setBacklight(percent);
}

/**
 * @brief Set the display backlight brightness as a percentage.
 *
 * Clamps the provided percentage to the 0–100 range, converts it to an 8-bit
 * PWM value, and writes that PWM value to the configured backlight pin.
 *
 * @param percent Desired backlight level (0-100). Values outside this range are clamped.
 */
void UsermodGC9A01Display::setBacklight(uint8_t percent) {
  backlight = min(percent, (uint8_t)100); // Clamp to 0-100% range
  uint8_t pwm = map(backlight, 0, 100, 0, 255); // Map to PWM range (0-255)
  #ifdef TFT_BL
  analogWrite(TFT_BL, pwm);
  #endif
  DEBUG_PRINTF("[GC9A01] Backlight set to %d%% (PWM: %d)\n", backlight, pwm);
}

/**
 * @brief Render a minimalist full-screen clock interface and Wi‑Fi status on the TFT.
 *
 * Clears the display and draws a centered large-format time using the configured
 * 12/24-hour mode, an optional AM/PM indicator (when 12-hour mode is enabled),
 * a two-ring blue bezel, and a Wi‑Fi signal icon with current RSSI at the top.
 *
 * Side effects:
 * - Marks the usermod as showing the clock (updates `showingClock`).
 * - Updates local time before rendering.
 */
void UsermodGC9A01Display::drawClockScreen() {
  // Minimalist clock-only display for idle mode
  tft.fillScreen(TFT_BLACK);
  
  showingClock = true; // Mark that we're showing clock (for rotary encoder state reset)
  
  // Draw blue bezel circle (same as main screen)
  tft.drawCircle(120, 120, 119, TFT_BLUE);
  tft.drawCircle(120, 120, 118, TFT_BLUE);
  
  // Get current time
  updateLocalTime();
  int hrs = hour(localTime);
  int mins = minute(localTime);
  bool isPmTime = isPM(localTime);
  
  // Convert to 12-hour format if needed
  if (clock12hour) {
    if (hrs == 0) {
      hrs = 12; // Midnight = 12 AM
    } else if (hrs > 12) {
      hrs = hrs - 12; // Convert to 12-hour
    }
  }
  
  // Draw time in large font at center
  tft.setTextDatum(MC_DATUM); // Middle center
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  // Format time string
  char timeStr[10];
  sprintf(timeStr, "%02d:%02d", hrs, mins);
  
  tft.drawString(timeStr, 120, 120, 7); // Large font
  
  // Draw AM/PM indicator if 12-hour format
  if (clock12hour) {
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(isPmTime ? "PM" : "AM", 120, 160, 4);
  }
  
  // Draw WiFi icon at top
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  int wifiRSSI = wifiConnected ? WiFi.RSSI() : -100;
  drawWiFiIcon(120, 30, wifiConnected, wifiRSSI);
  
  DEBUG_PRINTLN(F("[GC9A01] Clock screen drawn"));
}

/**
 * @brief Enter sleep/clock mode or wake the display.
 *
 * If enabled is true, marks the display as turned off and either shows the clock
 * (when clockMode is enabled) or turns the panel backlight off. If enabled is
 * false, wakes the display, clears sleep/clock state, and restores the backlight
 * to the configured brightness.
 *
 * @param enabled true to enter sleep/clock mode, false to wake the display.
 */
void UsermodGC9A01Display::sleepOrClock(bool enabled) {
  if (enabled) {
    displayTurnedOff = true;
    lastRedraw = ULONG_MAX; // Reset sleep timer
    if (clockMode) {
      // Show clock instead of turning off
      showingClock = true;
      drawClockScreen();
      DEBUG_PRINTLN(F("[GC9A01] Timeout reached - showing clock"));
    } else {
      // Turn off backlight (sleep)
      showingClock = false;
      #ifdef TFT_BL
      analogWrite(TFT_BL, 0);
      #endif
      DEBUG_PRINTLN(F("[GC9A01] Timeout reached - sleeping (backlight off)"));
    }
  } else {
    // Wake up display
    displayTurnedOff = false;
    showingClock = false;
    setBacklight(backlight);
    DEBUG_PRINTF("[GC9A01] Display waking - restoring backlight to %d%%\n", backlight);
  }
}

/**
 * @brief Put the display into a sleeping state and turn off its backlight.
 *
 * Marks the display as turned off and resets the internal sleep timer so it
 * will restart after the next interaction. If a backlight pin is defined,
 * the backlight PWM is set to zero.
 */
void UsermodGC9A01Display::sleepDisplay() {
  #ifdef TFT_BL
  analogWrite(TFT_BL, 0); // Turn off backlight completely
  #endif
  displayTurnedOff = true;
  lastRedraw = ULONG_MAX; // Reset sleep timer - will start again after next interaction
  DEBUG_PRINTLN(F("[GC9A01] Display sleeping - backlight off, sleep timer reset"));
}

/**
 * @brief Wakes the display if it is currently turned off and restores its visible state.
 *
 * If the display was turned off, this restores the configured backlight level, clears the
 * turned-off flag, requests a redraw and resets the sleep timeout timer. It also forces
 * refresh of tracked state values so the next redraw updates all on-screen indicators.
 *
 * @return `true` if the display was sleeping and was woken; `false` if the display was already awake.
 */

bool UsermodGC9A01Display::wakeDisplayFromSleep() {
  if (displayTurnedOff) {
    setBacklight(backlight); // Restore configured backlight level
    displayTurnedOff = false;
    needsRedraw = true;
    lastRedraw = millis(); // Reset sleep timeout to prevent immediate sleep
    
    // Force update of all known values to ensure display shows current state
    knownBrightness = 255; // Force brightness update
    knownMode = 255; // Force effect update  
    knownPowerState = !knownPowerState; // Force power state update
    
    DEBUG_PRINTF("[GC9A01] Display waking from sleep - restoring backlight to %d%%\n", backlight);
    return true; // Was sleeping
  }
  return false; // Was already awake
}

/**
 * @brief Wakes the display if it is currently sleeping.
 *
 * Restores display state and backlight when the display is asleep.
 *
 * @return `true` if the display was sleeping and has been woken, `false` otherwise.
 */
bool UsermodGC9A01Display::wakeDisplay() {
  return wakeDisplayFromSleep(); // Return true if was sleeping
}

/**
 * @brief Refreshes the overlay timeout and prevents the display from sleeping after encoder activity.
 *
 * If an overlay is currently active, extends its expiration to now plus the configured
 * overlay inactivity timeout and requests a redraw. Always updates the last-redraw
 * timestamp to prevent the display sleep timeout.
 */
void UsermodGC9A01Display::updateRedrawTime() {
  // Called when encoder is rotated - extend overlay timeout
  if (activeOverlayMode >= 0) {
    // We're in overlay mode - extend the timeout
    overlayUntil = millis() + overlayInactivityTimeout;
    DEBUG_PRINTF("[GC9A01] Overlay timeout extended to %lu (encoder activity detected)\n", overlayUntil);
    needsRedraw = true; // Trigger redraw to show updated values
  }
  lastRedraw = millis(); // Prevent display sleep timeout
}

/**
 * @brief Activate an on-screen overlay and render it immediately.
 *
 * Sets and displays an overlay with the provided text for a limited time, optionally waking
 * the display if it was sleeping. The overlay mode is derived from the supplied glyphType,
 * the overlay timeout is extended for user interaction, and an immediate redraw of the UI
 * is triggered.
 *
 * @param line1 Text to show on the overlay (may be null).
 * @param showHowLong Requested display duration in milliseconds (caller hint; actual timeout uses the usermod's interaction timeout).
 * @param glyphType Glyph identifier used to choose the overlay mode:
 *                  - 1 → Brightness
 *                  - 2 → Speed
 *                  - 3 → Intensity
 *                  - 4 → Palette
 *                  - 5 → Effect
 *                  - 7,8,10,11 and other mapped values → treated as Brightness (UI proxy)
 *                  - 12 → Network information
 */
void UsermodGC9A01Display::overlay(const char* line1, long showHowLong, byte glyphType) {
  if (!displayEnabled) {
    DEBUG_PRINTLN(F("[GC9A01] Overlay called but display disabled"));
    return;
  }
  
  DEBUG_PRINTF("[GC9A01] *** OVERLAY CALLED *** : '%s' for %ld ms (glyph: %d)\n", line1 ? line1 : "NULL", showHowLong, glyphType);
  
  // Wake display if sleeping (like 4-line display)
  if (displayTurnedOff) {
    wakeDisplayFromSleep();
    DEBUG_PRINTLN(F("[GC9A01] Display was sleeping - woken up for overlay"));
  }
  
  // Map glyph type to overlay mode
  // Rotary encoder states: 0=Brightness, 1=Speed, 2=Intensity, 3=Palette, 4=Effect, 5=Hue, 6=Sat, 7=CCT, 8=Preset, 9-11=Custom
  int overlayMode = 0; // Default to brightness
  
  // Map glyphType to overlay modes based on rotary encoder glyph assignments
  // Rotary encoder calls changeState() with these glyph types for each state
  switch (glyphType) {
    case 1:  overlayMode = 0; break; // Sun glyph → Brightness (encoder state 0)
    case 2:  overlayMode = 2; break; // Skip forward glyph → Speed (encoder state 1)
    case 3:  overlayMode = 3; break; // Fire glyph → Intensity (encoder state 2)
    case 4:  overlayMode = 4; break; // Custom palette glyph → Palette (encoder state 3)
    case 5:  overlayMode = 1; break; // Puzzle piece glyph → Effect (encoder state 4)
    case 7:  overlayMode = 0; break; // Brush glyph → Hue (encoder state 5) - show as brightness
    case 8:  overlayMode = 0; break; // Contrast glyph → Saturation (encoder state 6) - show as brightness
    case 10: overlayMode = 0; break; // Star glyph → CCT/custom (encoder states 7,9-11) - show as brightness
    case 11: overlayMode = 0; break; // Heart glyph → Preset (encoder state 8) - show as brightness
    case 12: overlayMode = 99; break; // Network glyph → Network info
    default: overlayMode = 0; break; // Fallback to brightness
  }
  
  // Activate overlay mode and store the text
  activeOverlayMode = overlayMode;
  overlayText = String(line1 ? line1 : ""); // Store the text for display
  overlayUntil = millis() + overlayInactivityTimeout; // Use longer timeout for user interaction
  lastRedraw = millis(); // Start/reset sleep timer on user interaction
  
  // Draw the overlay interface immediately
  drawMainInterface(overlayMode);
  
  DEBUG_PRINTF("[GC9A01] Overlay mode %d activated with text '%s' - will expire at %lu\n", overlayMode, overlayText.c_str(), overlayUntil);
}

/**
 * @brief Initializes the GC9A01 display usermod and primes internal state for the first redraw.
 *
 * Initializes display hardware and configuration, marks the display as needing an initial redraw, records the initial redraw timestamp, and sets sentinel values for known mode and brightness to force the first UI update.
 */
void UsermodGC9A01Display::setup() {
  DEBUG_PRINTLN(F(""));
  DEBUG_PRINTLN(F("=== GC9A01 Display Usermod ==="));
  DEBUG_PRINTLN(F("[GC9A01] Usermod successfully registered and setup() called"));
  DEBUG_PRINTF("[GC9A01] Usermod ID: %d\n", getId());
  DEBUG_PRINTF("[GC9A01] Instance pointer: %p\n", this);
  DEBUG_PRINTF("[GC9A01] Static instance: %p\n", instance);
  DEBUG_PRINT(F("[GC9A01] TFT_eSPI library version: "));
  DEBUG_PRINTLN(TFT_ESPI_VERSION);
  
  initDisplay();
  
  DEBUG_PRINTLN(F("[GC9A01] Display initialization complete"));
  needsRedraw = true;
  lastRedraw = millis(); // Initialize timeout tracking
  
  // Initialize known values to force initial update
  knownMode = 255; // Force initial effect name update
  knownBrightness = 255; // Force initial brightness update
}

/**
 * @brief Periodically checks whether the display needs updating and triggers a redraw when due.
 *
 * Checks display enablement and strip update state, enforces the refresh interval, advances the internal
 * next-update timestamp, and invokes redraw for the display.
 */
void UsermodGC9A01Display::loop() {
  // Follow the proven pattern from Four Line Display ALT
  if (!displayEnabled || strip.isUpdating()) return;
  
  unsigned long now = millis();
  if (now < nextUpdate) return;
  
  nextUpdate = now + refreshRate;
  
  redraw(false);
}

/**
 * @brief Appends GC9A01 display status information to the provided JSON info object.
 *
 * Adds an entry under the top-level "u" object with an array labeled "GC9A01 Display"
 * containing the display state ("Enabled" or "Disabled") and a placeholder string.
 *
 * @param root JSON object to augment; a nested "u" object is created if missing and the
 *             "GC9A01 Display" array is appended under it.
 */
void UsermodGC9A01Display::addToJsonInfo(JsonObject& root) {
  JsonObject user = root["u"];
  if (user.isNull()) user = root.createNestedObject("u");

  JsonArray temp = user.createNestedArray(F("GC9A01 Display"));
  temp.add(displayEnabled ? F("Enabled") : F("Disabled"));
  temp.add(F(" "));
}

/**
 * @brief Update display enabled state from a JSON state object.
 *
 * Reads the "gc9a01.on" boolean value from the provided JSON object and, if different
 * from the current state, updates the internal displayEnabled flag and either wakes
 * or sleeps the display accordingly.
 *
 * @param root JSON object containing the "gc9a01" state subtree (expects "gc9a01.on").
 */
void UsermodGC9A01Display::readFromJsonState(JsonObject& root) {
  if (root[F("gc9a01")] != nullptr) {
    if (root[F("gc9a01")][F("on")] != nullptr) {
      bool newState = root[F("gc9a01")][F("on")];
      if (newState != displayEnabled) {
        displayEnabled = newState;
        if (displayEnabled) {
          wakeDisplay();
        } else {
          sleepDisplay();
        }
      }
    }
  }
}

/**
 * @brief Appends the GC9A01 display runtime state to the given JSON object.
 *
 * Adds a nested "gc9a01" object containing the "on" property set to the current displayEnabled value.
 *
 * @param root JSON object to which the display state will be added.
 */
void UsermodGC9A01Display::addToJsonState(JsonObject& root) {
  JsonObject gc9a01 = root.createNestedObject(F("gc9a01"));
  gc9a01[F("on")] = displayEnabled;
}

/**
 * @brief Loads GC9A01 display configuration from the given JSON object and applies it.
 *
 * Reads the "GC9A01" object and updates displayEnabled, sleepMode, clockMode,
 * clock12hour, flip, displayTimeout (converts seconds to milliseconds and clamps to 5–300 seconds),
 * and backlight. If the backlight value changes, setBacklight is invoked.
 *
 * @param root Root JSON object expected to contain a "GC9A01" object with configuration keys.
 * @return true if a "GC9A01" configuration object was present and processed, false if it was missing.
 */
bool UsermodGC9A01Display::readFromConfig(JsonObject& root) {
  JsonObject top = root[FPSTR("GC9A01")];
  if (top.isNull()) {
    return false;
  }

  displayEnabled = top[FPSTR("enabled")] | displayEnabled;
  sleepMode = top[FPSTR("sleepMode")] | sleepMode;
  clockMode = top[FPSTR("clockMode")] | clockMode;
  clock12hour = top[FPSTR("clock12hour")] | clock12hour;
  flip = top[FPSTR("flip")] | flip;
  
  // Convert seconds from UI to milliseconds for internal use
  // Clamp to 5-300 seconds range (5 sec min, 5 min max)
  uint16_t timeoutSeconds = top[FPSTR("screenTimeOutSec")] | (displayTimeout / 1000);
  timeoutSeconds = max((uint16_t)5, min((uint16_t)300, timeoutSeconds));
  displayTimeout = timeoutSeconds * 1000;
  
  // Load backlight setting (0-100% range)
  uint8_t newBacklight = top[FPSTR("backlight")] | backlight;
  if (newBacklight != backlight) {
    setBacklight(newBacklight);
  }
  
  return true;
}

/**
 * @brief Appends GC9A01 display configuration to the provided JSON object.
 *
 * Creates a nested "GC9A01" object on @p root and writes the display settings:
 * - "enabled": whether the display is enabled
 * - "sleepMode": whether sleep mode is enabled
 * - "screenTimeOutSec": screen timeout in seconds (converted from internal milliseconds)
 * - "clockMode": whether the clock screen is enabled
 * - "clock12hour": whether 12-hour clock format is used
 * - "flip": display flip setting
 * - "backlight": backlight percentage (0–100)
 *
 * @param root JSON object to which the GC9A01 configuration will be added (modified in place).
 */
void UsermodGC9A01Display::addToConfig(JsonObject& root) {
  JsonObject top = root.createNestedObject(FPSTR("GC9A01"));
  top[FPSTR("enabled")] = displayEnabled;
  top[FPSTR("sleepMode")] = sleepMode;
  // Convert milliseconds to seconds for UI display
  top[FPSTR("screenTimeOutSec")] = displayTimeout / 1000;
  top[FPSTR("clockMode")] = clockMode;
  top[FPSTR("clock12hour")] = clock12hour;
  top[FPSTR("flip")] = flip;
  top[FPSTR("backlight")] = backlight;
}

/**
 * @brief Appends GC9A01 display configuration metadata to the global UI/help registry.
 *
 * Adds informational entries for the following configurable keys: `GC9A01:enabled`,
 * `GC9A01:sleepMode`, `GC9A01:screenTimeOutSec` (seconds, clamped to 5–300 on save),
 * `GC9A01:clockMode`, `GC9A01:clock12hour`, `GC9A01:flip`, and `GC9A01:backlight`.
 */
void UsermodGC9A01Display::appendConfigData() {
  oappend(SET_F("addInfo('GC9A01:enabled', 1, 'Enable/disable display');"));
  oappend(SET_F("addInfo('GC9A01:sleepMode', 1, 'Enable sleep mode after timeout');"));
  oappend(SET_F("addInfo('GC9A01:screenTimeOutSec', 1, 'Screen timeout in seconds (5-300 range, clamped on save)');"));
  oappend(SET_F("addInfo('GC9A01:clockMode', 1, 'Show clock only when idle (bypasses sleep)');"));
  oappend(SET_F("addInfo('GC9A01:clock12hour', 1, 'Checked=12H format (9:01 PM), Unchecked=24H format (21:01)');"));
  oappend(SET_F("addInfo('GC9A01:flip', 1, 'Rotate display 180 degrees (requires reboot)');"));
  oappend(SET_F("addInfo('GC9A01:backlight', 1, 'Backlight brightness (0-100%, default 75%)');"));
}

/**
 * @brief Maps an encoder overlay mode index to a human-readable name.
 *
 * @param mode Mode index where 0=Brightness, 1=Effect, 2=Speed, 3=Intensity, 4=Palette.
 * @return const char* Name for the given mode, or "Unknown" if the index is out of range.
 */
const char* UsermodGC9A01Display::getEncoderModeName(uint8_t mode) {
  // For rotary encoder integration - just provide names for overlay display
  const char* modeNames[] = {"Brightness", "Effect", "Speed", "Intensity", "Palette"};
  if (mode < 5) return modeNames[mode];
  return "Unknown";
}

/**
 * @brief No-op placeholder retained for API compatibility.
 *
 * This function intentionally performs no action; mode display is handled elsewhere.
 */
void UsermodGC9A01Display::drawCurrentModeIndicator() {
  // No longer needed - overlay handles mode display
}

/**
 * @brief Placeholder no-op kept for interface compatibility.
 *
 * This method intentionally performs no action because mode display is handled
 * by the overlay rendering path.
 */
void UsermodGC9A01Display::drawModeOverlay() {
  // No longer needed - overlay handles mode display
}

/**
 * @brief Indicates whether an on-screen overlay is currently active and not expired.
 *
 * @return true if an overlay mode is set and the current time is before the overlay timeout, false otherwise.
 */
bool UsermodGC9A01Display::isOverlayActive() {
  return (activeOverlayMode >= 0 && millis() < overlayUntil);
}

/**
 * @brief Retrieve the currently active overlay mode or indicate absence.
 *
 * @return `-1` if no overlay is active or the overlay has expired; otherwise the active overlay mode ID.
 */
int UsermodGC9A01Display::getActiveOverlayMode() {
  if (millis() >= overlayUntil) {
    return -1; // Overlay has expired
  }
  return activeOverlayMode;
}

/**
 * @brief Indicates whether the display is currently considered asleep, including when showing the clock.
 *
 * @return `true` if the display is turned off or the clock screen is active, `false` otherwise.
 */
bool UsermodGC9A01Display::isDisplayAsleep() {
  return displayTurnedOff || showingClock; // Consider clock mode as "asleep" for rotary encoder state reset
}

/**
 * @brief Provides the usermod identifier for the GC9A01 display.
 *
 * @return uint16_t The numeric usermod ID constant USERMOD_ID_GC9A01_DISPLAY.
 */
uint16_t UsermodGC9A01Display::getId() {
  return USERMOD_ID_GC9A01_DISPLAY;
}

// Registration
UsermodGC9A01Display gc9a01DisplayUsermod;
REGISTER_USERMOD(gc9a01DisplayUsermod);

#endif // USERMOD_GC9A01_DISPLAY