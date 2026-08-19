
/*
 * This file allows you to add own functionality to WLED more easily
 * See: https://github.com/wled-dev/WLED/wiki/Add-own-functionality
 * EEPROM bytes 2750+ are reserved for your custom use case. (if you extend #define EEPSIZE in const.h)
 * bytes 2400+ are currently unused, but might be used for future wled features
 */

/*
 * Pin 2 of the TTGO T-Display serves as the data line for the LED string.
 * Pin 35 is set up as the button pin in the platformio_overrides.ini file.
 * The button can be set up via the macros section in the web interface.
 * I use the button to cycle between presets.
 * The Pin 35 button is the one on the RIGHT side of the USB-C port on the board,
 * when the port is oriented downwards.  See readme.md file for photo.
 * The display is set up to turn off after 5 minutes, and turns on automatically 
 * when a change in the dipslayed info is detected (within a 5 second interval).
 */
 
#include "wled.h"
#include <SPI.h>
#include <TFT_eSPI.h>
#include "WiFi.h"
#include <Wire.h>

#define DISPLAY_HAS_BACKLIGHT defined(TFT_BL) && defined(TFT_BACKLIGHT_ON) && (TFT_BL >= 0)

// How often we are redrawing screen
#define USER_LOOP_REFRESH_RATE_MS 5000

class TTGO_T_DisplayMod : public Usermod {

  // Member variables  
  bool enabled = true;
  bool ready = false;
  TFT_eSPI tft; // Invoke custom library
  // needRedraw marks if redraw is required to prevent often redrawing.
  bool needRedraw = true;
  // Next variables hold the previous known values to determine if redraw is
  // required.
  String knownSsid = "";
  IPAddress knownIp;
  uint8_t knownBrightness = 0;
  uint8_t knownMode = 0;
  uint8_t knownPalette = 0;
  uint8_t tftcharwidth = 19;  

  long lastUpdate_mod = 0;
  long lastRedraw = 0;
#if DISPLAY_HAS_BACKLIGHT
  bool displayTurnedOff = false;
#endif  

  // string that are used multiple time (this will save some flash memory)
  static const char _name[];
  static const char _enabled[];

public:

TTGO_T_DisplayMod() : Usermod()
{}

//gets called once at boot. Do all initialization that doesn't depend on network here
void setup() override {
  if (!ready) {
    // Reserve pins. allocateMultiplePins() validates every pin before allocating
    // any of them, so a failure here leaves no pins allocated (no manual rollback needed).
    // Pins the active TFT_eSPI User_Setup doesn't define (e.g. parallel-bus displays,
    // or lines hardwired to GND/3V3) are simply left out of the array below.
    const static PinManagerPinType pins[] PROGMEM = {
#ifdef TFT_MISO
      { TFT_MISO, INPUT },
#endif
#ifdef TFT_MOSI
      { TFT_MOSI, OUTPUT },
#endif
#ifdef TFT_SCLK
      { TFT_SCLK, OUTPUT },
#endif
#ifdef TFT_CS
      { TFT_CS, OUTPUT },
#endif
#ifdef TFT_DC
      { TFT_DC, OUTPUT },
#endif
#ifdef TFT_RST
      { TFT_RST, OUTPUT },
#endif
#ifdef TFT_BL
      { TFT_BL, OUTPUT },
#endif      
#ifdef TOUCH_CS
      { TOUCH_CS, OUTPUT },
#endif
    };
    if (!PinManager::allocateMultiplePins(pins, sizeof(pins)/sizeof(pins[0]), (PinOwner) getId())) {
      DEBUG_PRINTLN("TFT: Failed to allocate pins!");
      return;
    }

    tft.init();
    tft.setRotation(3);  //Rotation here is set up for the text to be readable with the port on the left. Use 1 to flip.
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    //tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(1, 10);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    //tft.setTextSize(1);
    tft.print("Loading...");
    DEBUG_PRINTLN("TFT Loading...");

#if DISPLAY_HAS_BACKLIGHT
    if (TFT_BL >= 0) { // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
         pinMode(TFT_BL, OUTPUT); // Set backlight pin to output mode
         digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. 
    }
#endif    

    // tft.setRotation(3);
    ready = true;
  }
}


void loop() override {
  if (!ready) return; 

  if (!enabled) {
#if DISPLAY_HAS_BACKLIGHT
    if (!displayTurnedOff) {
      digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON); // Turn backlight off. 
      displayTurnedOff = true;
    }
#endif
    return;
  }

  // Check if we time interval for redrawing passes.
  if (millis() - lastUpdate_mod < USER_LOOP_REFRESH_RATE_MS) {
    return;
  }
  lastUpdate_mod = millis();
  
  // Check if values which are shown on display changed from the last time.
  if (((apActive) ? String(apSSID) : WiFi.SSID()) != knownSsid) {
    needRedraw = true;
  } else if (knownIp != (apActive ? IPAddress(4, 3, 2, 1) : WiFi.localIP())) {
    needRedraw = true;
  } else if (knownBrightness != bri) {
    needRedraw = true;
  } else if (knownMode != strip.getMainSegment().mode) {
    needRedraw = true;
  } else if (knownPalette != strip.getMainSegment().palette) {
    needRedraw = true;
  }

  if (!needRedraw) {
  // Turn off display after 5 minutes with no change.
#if DISPLAY_HAS_BACKLIGHT
  if(!displayTurnedOff && millis() - lastRedraw > 5*60*1000) {
    digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON); // Turn backlight off. 
    displayTurnedOff = true;
  } 
#endif  
    return;
  }
  needRedraw = false;
  
#if DISPLAY_HAS_BACKLIGHT
  if (displayTurnedOff)
  {
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on.
    displayTurnedOff = false;
  }
#endif  
  lastRedraw = millis();

  // Update last known values.
  knownSsid = apActive ? apSSID : WiFi.SSID();
  knownIp = apActive ? IPAddress(4, 3, 2, 1) : WiFi.localIP();
  knownBrightness = bri;
  knownMode = strip.getMainSegment().mode;
  knownPalette = strip.getMainSegment().palette;

  tft.fillScreen(TFT_BLACK);
  //tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  // First row with Wifi name
  tft.setCursor(1, 1);
  tft.print(knownSsid.substring(0, tftcharwidth > 1 ? tftcharwidth - 1 : 0));
  // Print `~` char to indicate that SSID is longer than our display
  if (knownSsid.length() > tftcharwidth)
    tft.print("~");

  // Second row with AP IP and Password or IP
  tft.setTextSize(2);
  tft.setCursor(1, 24);
  // Print AP IP and password in AP mode or knownIP if AP not active.
  // if (apActive && bri == 0)
  //   tft.print(apPass);
  // else
  //   tft.print(knownIp);
  if (apActive) {
    tft.print("AP IP: ");
    tft.print(knownIp);
    tft.setCursor(1,46);
    tft.print("AP Pass:");
    tft.print(apPass);
  }
  else {
    tft.print("IP: ");
    tft.print(knownIp);
    tft.setCursor(1,46);
    //tft.print("Signal Strength: ");
    //tft.print(i.wifi.signal);
    tft.print("Brightness: ");
    tft.print(((float(bri)/255)*100));
    tft.print("%");
  }

  // Third row with mode name
  tft.setCursor(1, 68);
  char lineBuffer[tftcharwidth+1];
  extractModeName(knownMode, JSON_mode_names, lineBuffer, tftcharwidth);
  tft.print(lineBuffer);

  // Fourth row with palette name
  tft.setCursor(1, 90);
  extractModeName(knownPalette, JSON_palette_names, lineBuffer, tftcharwidth);
  tft.print(lineBuffer);

  // Fifth row with estimated mA usage
  tft.setCursor(1, 112);
  // Print estimated milliamp usage (must specify the LED type in LED prefs for this to be a reasonable estimate).
  tft.print(BusManager::currentMilliamps());
  tft.print("mA (estimated)");  
}


/*
  * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
  * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
  * Below it is shown how this could be used for e.g. a light sensor
  */
void addToJsonInfo(JsonObject& root) override {
  JsonObject user = root["u"];
  if (user.isNull()) user = root.createNestedObject("u");

  auto state = user.createNestedArray(FPSTR(_name));
  if (!ready) {
    state.add(PSTR("Pin allocation failure"));
    return;
  }
  if (enabled) {
#if DISPLAY_HAS_BACKLIGHT    
    state.add(displayTurnedOff ? PSTR("Off") : PSTR("On"));
#else
    state.add(PSTR("On"));
#endif    
  } else {
    state.add(PSTR("Disabled"));
  }
}


void addToConfig(JsonObject &root) override {
  JsonObject top = root.createNestedObject(FPSTR(_name)); // usermodname
  top[FPSTR(_enabled)] = enabled;
}

bool readFromConfig(JsonObject& root) override
{
  // default settings values could be set here (or below using the 3-argument getJsonValue()) instead of in the class definition or constructor
  // setting them inside readFromConfig() is slightly more robust, handling the rare but plausible use case of single value being missing after boot (e.g. if the cfg.json was manually edited and a value was removed)
  JsonObject top = root[FPSTR(_name)];
  bool configComplete = !top.isNull();

  configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled);
  return configComplete;
}

};  // TTGO_T_DisplayMod

// add more strings here to reduce flash memory usage
const char TTGO_T_DisplayMod::_name[]    PROGMEM = "TFT Display";
const char TTGO_T_DisplayMod::_enabled[] PROGMEM = "enabled";

static TTGO_T_DisplayMod usermod_ttgo_t_display;
REGISTER_USERMOD(usermod_ttgo_t_display);
