
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

// #ifndef TFT_DISPOFF
// #define TFT_DISPOFF 0x28
// #endif

// #ifndef TFT_SLPIN
// #define TFT_SLPIN   0x10
// #endif

// #define TFT_MOSI            19
// #define TFT_SCLK            18
// #define TFT_CS              5
// #define TFT_DC              16
// #define TFT_RST             23

//#define TFT_BL          4  // Display backlight control pin
//#define ADC_EN          14  // Used for enabling battery voltage measurements - not used in this program
//#define WLED_WATCHDOG_TIMEOUT 3

// How often we are redrawing screen
#define USER_LOOP_REFRESH_RATE_MS 5000

class TTGO_T_DisplayMod : public Usermod {

  // Member variables  
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
  bool displayTurnedOff = false;

public:

TTGO_T_DisplayMod() : Usermod()
{}

//gets called once at boot. Do all initialization that doesn't depend on network here
void setup() override {
    DEBUG_PRINTLN("Start");
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
    DEBUG_PRINTLN("Loading...");

    if (TFT_BL > 0) { // TFT_BL has been set in the TFT_eSPI library in the User Setup file TTGO_T_Display.h
         pinMode(TFT_BL, OUTPUT); // Set backlight pin to output mode
         digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on. 
    }

    // tft.setRotation(3);
}


void loop() override {

  // Check if we time interval for redrawing passes.
  if (millis() - lastUpdate_mod < USER_LOOP_REFRESH_RATE_MS) {
    return;
  }
  lastUpdate_mod = millis();
  
  // Turn off display after 5 minutes with no change.
   if(!displayTurnedOff && millis() - lastRedraw > 5*60*1000) {
    digitalWrite(TFT_BL, !TFT_BACKLIGHT_ON); // Turn backlight off. 
    displayTurnedOff = true;
  } 

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
    return;
  }
  needRedraw = false;
  
  if (displayTurnedOff)
  {
    digitalWrite(TFT_BL, TFT_BACKLIGHT_ON); // Turn backlight on.
    displayTurnedOff = false;
  }
  lastRedraw = millis();

  // Update last known values.
  #if defined(ESP8266)
  knownSsid = apActive ? WiFi.softAPSSID() : WiFi.SSID();
  #else
  knownSsid = WiFi.SSID();
  #endif
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
  DEBUG_PRINTLN("Print known AP");
  if (apActive) {
    tft.print("AP IP: ");
    tft.print(knownIp);
    tft.setCursor(1,46);
    tft.print("AP Pass:");
    tft.print(apPass);
  }
  else {
    DEBUG_PRINTLN("Print IP");
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
  DEBUG_PRINTLN("Print mode name");

  // Fourth row with palette name
  tft.setCursor(1, 90);
  extractModeName(knownPalette, JSON_palette_names, lineBuffer, tftcharwidth);
  tft.print(lineBuffer);
  DEBUG_PRINTLN("Print palette");

  // Fifth row with estimated mA usage
  tft.setCursor(1, 112);
  // Print estimated milliamp usage (must specify the LED type in LED prefs for this to be a reasonable estimate).
  //tft.print(strip.currentMilliamps);
  tft.print(BusManager::currentMilliamps());
  DEBUG_PRINTLN(BusManager::currentMilliamps());
  DEBUG_PRINTLN(" mA (estimated)");
  //tft.print("test ");
  tft.print("mA (estimated)");
  DEBUG_PRINTLN("Print estimated current");
  
}
};  // TTGO_T_DisplayMod

static TTGO_T_DisplayMod usermod_ttgo_t_display;
REGISTER_USERMOD(usermod_ttgo_t_display);
