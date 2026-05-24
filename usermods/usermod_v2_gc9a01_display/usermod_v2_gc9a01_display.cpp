#include "wled.h"

//
// GC9A01 Round Display Usermod
//
// Adds support for the Waveshare 1.28" Round LCD Module (and compatible
// GC9A01-based 240×240 IPS displays) as a WLED status screen.
//
// Uses TFT_eSPI (Bodmer) which must be configured via build flags —
// see platformio_override.sample.ini for a ready-to-use example.
//
// Displays:
//   • Outer ring  : brightness progress arc
//   • Centre fill : primary segment colour
//   • Top text    : current effect name
//   • Centre text : brightness percentage
//   • Bottom text : IP address (or "No WiFi")
//
// Public API (for pairing with a rotary encoder or other usermods):
//   wakeDisplay()      — wake from timeout, returns true if was sleeping
//   showOverlay(...)   — display temporary text over the watch face
//   forceRedraw()      — request immediate full refresh
//

#ifdef USERMOD_GC9A01_DISPLAY

#include <TFT_eSPI.h>
#include <SPI.h>

// Verify required build flags are present
#ifndef USER_SETUP_LOADED
  #ifndef GC9A01_DRIVER
    #error "GC9A01_DISPLAY: define GC9A01_DRIVER=1 in build flags"
  #endif
  #ifndef TFT_WIDTH
    #error "GC9A01_DISPLAY: define TFT_WIDTH=240 in build flags"
  #endif
  #ifndef TFT_HEIGHT
    #error "GC9A01_DISPLAY: define TFT_HEIGHT=240 in build flags"
  #endif
  #ifndef TFT_CS
    #error "GC9A01_DISPLAY: define TFT_CS in build flags"
  #endif
  #ifndef TFT_DC
    #error "GC9A01_DISPLAY: define TFT_DC in build flags"
  #endif
  #ifndef TFT_RST
    #error "GC9A01_DISPLAY: define TFT_RST in build flags"
  #endif
#endif

#ifndef TFT_BL
  #define TFT_BL -1
#endif

// How often the loop checks for state changes (ms)
#define GC9A01_REFRESH_MS      500
// Screen off after this many ms without a state change
#define GC9A01_TIMEOUT_MS      (5UL * 60UL * 1000UL)   // 5 minutes
// Overlay display duration default (ms)
#define GC9A01_OVERLAY_MS      2000
// Max chars for effect/palette name display
#define GC9A01_NAME_BUF        24

// Display geometry
#define GC9A01_CX   120    // centre X
#define GC9A01_CY   120    // centre Y
#define GC9A01_R    120    // display radius
// Brightness arc: outer radius, inner radius
#define GC9A01_ARC_R_OUT  115
#define GC9A01_ARC_R_IN   104

// -------------------------------------------------------------------------
// File-scope TFT object (must be file-scope so build flags apply)
// -------------------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT);

// -------------------------------------------------------------------------

class GC9A01DisplayUsermod : public Usermod {

  private:

    bool     enabled       = true;
    bool     initDone      = false;
    bool     displayOff    = false;
    bool     needRedraw    = true;

    // For state-change detection
    uint8_t  knownBri      = 0;
    uint8_t  knownMode     = 255;
    uint32_t knownColor    = 0;
    bool     knownWifi     = false;
    IPAddress knownIp;

    unsigned long lastCheck   = 0;
    unsigned long lastChange  = 0;   // millis of last detected state change

    // Overlay support
    bool     overlayActive   = false;
    unsigned long overlayUntil = 0;
    char     overlayLine1[GC9A01_NAME_BUF] = {0};
    char     overlayLine2[GC9A01_NAME_BUF] = {0};

    // Config
    uint16_t screenTimeoutSec = 300;  // 5 minutes
    bool     flipDisplay      = false;

    static const char _name[];
    static const char _enabled[];
    static const char _timeout[];
    static const char _flip[];

    // ---------------------------------------------------------------- helpers

    // Copy TFT_MOSI / TFT_SCLK compile-time pins into WLED's global SPI vars
    // so PinManager and the web UI see them (same pattern as pixels_dice_tray).
    static void setSPIPinsFromMacros() {
#ifdef TFT_MOSI
      spi_mosi = TFT_MOSI;
#endif
#ifdef TFT_MISO
  #if defined(TFT_MOSI) && (TFT_MISO == TFT_MOSI)
      spi_miso = -1;    // shared data line — not a real MISO
  #else
      spi_miso = TFT_MISO;
  #endif
#endif
#ifdef TFT_SCLK
      spi_sclk = TFT_SCLK;
#endif
    }

    // Convert WLED 0xRRGGBB(WW) to TFT RGB565
    uint16_t wledToRGB565(uint32_t color) {
      uint8_t r = (color >> 16) & 0xFF;
      uint8_t g = (color >> 8)  & 0xFF;
      uint8_t b =  color        & 0xFF;
      return tft.color565(r, g, b);
    }

    void setBacklight(bool on) {
      if (TFT_BL < 0) return;
#ifndef TFT_BACKLIGHT_ON
  #define TFT_BACKLIGHT_ON HIGH
#endif
      digitalWrite(TFT_BL, on ? TFT_BACKLIGHT_ON : (TFT_BACKLIGHT_ON == HIGH ? LOW : HIGH));
    }

    // ---------------------------------------------------------------- drawing

    // Fill the circular display background with a solid colour.
    void fillDisplay(uint32_t color) {
      tft.fillCircle(GC9A01_CX, GC9A01_CY, GC9A01_R, wledToRGB565(color));
    }

    // Draw the brightness arc (0° = 12 o'clock, clockwise).
    // TFT_eSPI drawArc: angle 0 = top, clockwise.
    void drawBrightnessArc(uint8_t brightness) {
      // Background ring
      tft.drawArc(GC9A01_CX, GC9A01_CY,
                  GC9A01_ARC_R_OUT, GC9A01_ARC_R_IN,
                  0, 360,
                  0x2104 /*dark grey*/, TFT_BLACK);
      if (brightness > 0) {
        uint16_t endAngle = (uint16_t)((uint32_t)brightness * 360 / 255);
        if (endAngle == 0) endAngle = 1;
        tft.drawArc(GC9A01_CX, GC9A01_CY,
                    GC9A01_ARC_R_OUT, GC9A01_ARC_R_IN,
                    0, endAngle,
                    TFT_YELLOW, TFT_BLACK);
      }
    }

    // Draw the primary-colour filled centre circle.
    void drawColorDisk(uint32_t color) {
      uint16_t c565 = wledToRGB565(color);
      // 80 px radius gives a nice centre disc with room for text
      tft.fillCircle(GC9A01_CX, GC9A01_CY, 80, c565);
      // Thin border between disc and text area
      tft.drawCircle(GC9A01_CX, GC9A01_CY, 80, TFT_BLACK);
    }

    // Centered text helper — writes text centered at (cx, y).
    void drawCenteredText(const char *text, int16_t y,
                          uint8_t textSize, uint32_t color) {
      tft.setTextSize(textSize);
      tft.setTextDatum(TC_DATUM);
      tft.setTextColor(color, TFT_BLACK);
      tft.drawString(text, GC9A01_CX, y);
    }

    // Full watch-face redraw.
    void drawWatchFace() {
      uint32_t primaryColor = RGBW32(colPri[0], colPri[1], colPri[2], colPri[3]);

      // Black background
      tft.fillScreen(TFT_BLACK);

      // Outer brightness ring
      drawBrightnessArc(bri);

      // Coloured centre disc
      drawColorDisk(primaryColor);

      // Effect name — white text above centre
      char modeName[GC9A01_NAME_BUF];
      extractModeName(effectCurrent, JSON_mode_names, modeName, GC9A01_NAME_BUF - 1);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextDatum(TC_DATUM);
      tft.setTextSize(2);
      tft.drawString(modeName, GC9A01_CX, 54);

      // Brightness % — large, centred on the disc
      char briBuf[8];
      snprintf_P(briBuf, sizeof(briBuf), PSTR("%d%%"), (bri * 100 + 127) / 255);

      // Pick a contrasting text colour based on primary colour brightness
      uint8_t luma = (77 * colPri[0] + 150 * colPri[1] + 29 * colPri[2]) >> 8;
      uint16_t textCol = (luma > 128) ? TFT_BLACK : TFT_WHITE;
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(textCol);
      tft.setTextSize(3);
      tft.drawString(briBuf, GC9A01_CX, GC9A01_CY);

      // IP address — small, bottom of display
      char ipBuf[20];
      if (WLED_CONNECTED) {
        IPAddress ip = Network.localIP();
        snprintf_P(ipBuf, sizeof(ipBuf), PSTR("%d.%d.%d.%d"),
                   ip[0], ip[1], ip[2], ip[3]);
      } else if (apActive) {
        strlcpy_P(ipBuf, PSTR("AP 4.3.2.1"), sizeof(ipBuf));
      } else {
        strlcpy_P(ipBuf, PSTR("No WiFi"), sizeof(ipBuf));
      }
      tft.setTextDatum(BC_DATUM);
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextSize(1);
      tft.drawString(ipBuf, GC9A01_CX, 195);
    }

    // Overlay: two lines of text in a rounded rect, auto-cleared after duration.
    void drawOverlay() {
      tft.fillRoundRect(30, 85, 180, 70, 12, TFT_NAVY);
      tft.drawRoundRect(30, 85, 180, 70, 12, TFT_WHITE);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_WHITE);
      tft.setTextSize(2);
      tft.drawString(overlayLine1, GC9A01_CX, 108);
      if (overlayLine2[0]) {
        tft.setTextSize(1);
        tft.drawString(overlayLine2, GC9A01_CX, 135);
      }
    }

    // WLED splash screen shown once at boot.
    void drawSplash() {
      tft.fillScreen(TFT_BLACK);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(TFT_ORANGE);
      tft.setTextSize(4);
      tft.drawString("WLED", GC9A01_CX, GC9A01_CY - 20);
      tft.setTextColor(TFT_DARKGREY);
      tft.setTextSize(1);
      tft.drawString(versionString, GC9A01_CX, GC9A01_CY + 25);
    }

  public:

    uint16_t getId() override { return USERMOD_ID_GC9A01_DISPLAY; }

    // ------------------------------------------------------ public API

    // Wake the display from timeout sleep.
    // Returns true if the display was sleeping (so callers can discard input).
    bool wakeDisplay() {
      if (!enabled || !initDone) return false;
      if (displayOff) {
        setBacklight(true);
        displayOff = false;
        needRedraw  = true;
        lastChange  = millis();
        return true;
      }
      return false;
    }

    // Show a temporary two-line overlay over the watch face.
    void showOverlay(const char *line1, const char *line2 = nullptr,
                     uint32_t durationMs = GC9A01_OVERLAY_MS) {
      if (!enabled || !initDone) return;
      wakeDisplay();
      strlcpy(overlayLine1, line1 ? line1 : "", sizeof(overlayLine1));
      strlcpy(overlayLine2, line2 ? line2 : "", sizeof(overlayLine2));
      overlayUntil  = millis() + durationMs;
      overlayActive = true;
      drawOverlay();
    }

    // Request an immediate full redraw on the next loop tick.
    void forceRedraw() {
      needRedraw = true;
      lastChange = millis();
    }

    // ------------------------------------------------------ lifecycle

    void setup() override {
      DEBUG_PRINTLN(F("GC9A01 Display: init."));

      setSPIPinsFromMacros();

      // Register shared SPI bus pins
      PinManagerPinType spiPins[] = {
        { spi_mosi, true }, { spi_miso, false }, { spi_sclk, true }
      };
      if (spi_sclk >= 0 && !PinManager::allocateMultiplePins(spiPins, 3, PinOwner::HW_SPI)) {
        DEBUG_PRINTLN(F("GC9A01 Display: SPI pin allocation failed, disabling."));
        enabled = false;
        return;
      }

      // Register display-specific pins (CS, DC, RST, BL)
      PinManagerPinType dispPins[] = {
        { TFT_CS, true }, { TFT_DC, true }, { TFT_RST, true }, { TFT_BL, true }
      };
      if (!PinManager::allocateMultiplePins(dispPins, 4, PinOwner::UM_GC9A01Display)) {
        PinManager::deallocateMultiplePins(spiPins, 3, PinOwner::HW_SPI);
        DEBUG_PRINTLN(F("GC9A01 Display: display pin allocation failed, disabling."));
        enabled = false;
        return;
      }

      tft.init();
      tft.setRotation(flipDisplay ? 2 : 0);
      tft.fillScreen(TFT_BLACK);

      if (TFT_BL >= 0) {
        pinMode(TFT_BL, OUTPUT);
        setBacklight(true);
      }

      drawSplash();
      delay(1200);

      lastChange = millis();
      lastCheck  = millis();
      initDone   = true;
    }

    void loop() override {
      if (!enabled || !initDone) return;

      unsigned long now = millis();
      if (now - lastCheck < GC9A01_REFRESH_MS) return;
      lastCheck = now;

      // Clear overlay once it expires
      if (overlayActive && now >= overlayUntil) {
        overlayActive = false;
        needRedraw    = true;
      }

      // Detect state changes
      uint32_t curColor = RGBW32(colPri[0], colPri[1], colPri[2], colPri[3]);
      bool     curWifi  = WLED_CONNECTED || apActive;
      if (bri         != knownBri   ||
          effectCurrent != knownMode ||
          curColor    != knownColor ||
          curWifi     != knownWifi  ||
          (curWifi && Network.localIP() != knownIp)) {
        needRedraw = true;
        lastChange = now;
        knownBri   = bri;
        knownMode  = effectCurrent;
        knownColor = curColor;
        knownWifi  = curWifi;
        knownIp    = curWifi ? Network.localIP() : IPAddress(0,0,0,0);
      }

      // Screen timeout
      unsigned long timeoutMs = (unsigned long)screenTimeoutSec * 1000UL;
      if (!displayOff && (now - lastChange > timeoutMs)) {
        setBacklight(false);
        tft.fillScreen(TFT_BLACK);
        displayOff = true;
        needRedraw = false;
        return;
      }

      if (displayOff || !needRedraw || overlayActive) return;

      needRedraw = false;
      drawWatchFace();
    }

    // ------------------------------------------------------ config

    void addToConfig(JsonObject &root) override {
      JsonObject top = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)] = enabled;
      top[FPSTR(_timeout)] = screenTimeoutSec;
      top[FPSTR(_flip)]    = flipDisplay;
      // Expose compile-time pin values so the UI can show them (read-only)
      JsonArray pins = top.createNestedArray("pin");
      pins.add(TFT_CS);
      pins.add(TFT_DC);
      pins.add(TFT_RST);
      pins.add(TFT_BL);
    }

    void appendConfigData() override {
      // Show pin labels (read-only — pins are compile-time constants)
      oappend(F("addInfo('GC9A01-Display:pin[]',0,'','CS (compile-time)');"));
      oappend(F("addInfo('GC9A01-Display:pin[]',1,'','DC (compile-time)');"));
      oappend(F("addInfo('GC9A01-Display:pin[]',2,'','RST (compile-time)');"));
      oappend(F("addInfo('GC9A01-Display:pin[]',3,'','BL (compile-time)');"));
    }

    bool readFromConfig(JsonObject &root) override {
      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) return false;
      bool en = top[FPSTR(_enabled)] | enabled;
      if (en != enabled) {
        enabled = en;
        if (!enabled && TFT_BL >= 0) setBacklight(false);
      }
      screenTimeoutSec = top[FPSTR(_timeout)] | screenTimeoutSec;
      bool newFlip     = top[FPSTR(_flip)]    | flipDisplay;
      if (newFlip != flipDisplay && initDone) {
        flipDisplay = newFlip;
        tft.setRotation(flipDisplay ? 2 : 0);
        needRedraw = true;
      } else {
        flipDisplay = newFlip;
      }
      return !top[FPSTR(_timeout)].isNull();
    }

    void addToJsonInfo(JsonObject &root) override {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");
      JsonArray arr = user.createNestedArray(F("GC9A01"));
      arr.add(enabled ? F("active") : F("disabled"));
    }
};


const char GC9A01DisplayUsermod::_name[]    PROGMEM = "GC9A01-Display";
const char GC9A01DisplayUsermod::_enabled[] PROGMEM = "enabled";
const char GC9A01DisplayUsermod::_timeout[] PROGMEM = "screenTimeoutSec";
const char GC9A01DisplayUsermod::_flip[]    PROGMEM = "flip";


static GC9A01DisplayUsermod gc9a01_display;
REGISTER_USERMOD(gc9a01_display);

#endif // USERMOD_GC9A01_DISPLAY
