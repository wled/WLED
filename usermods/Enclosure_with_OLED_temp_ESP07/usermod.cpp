#ifndef WLED_ENABLE_MQTT
#error "This user mod requires MQTT to be enabled."
#endif

#include "wled.h"
#include <Arduino.h>
#include <U8x8lib.h> // from https://github.com/olikraus/u8g2/
#include <DallasTemperature.h> //Dallastemperature sensor
//The SCL and SDA pins are defined here. 
//Lolin32 boards use SCL=5 SDA=4 
#define U8X8_PIN_SCL 5
#define U8X8_PIN_SDA 4
// Dallas sensor
OneWire oneWire(13); 
DallasTemperature sensor(&oneWire);
long temptimer = millis();
long lastMeasure = 0;
#define Celsius // Show temperature mesaurement in Celcius otherwise is in Fahrenheit 

// If display does not work or looks corrupted check the
// constructor reference:
// https://github.com/olikraus/u8g2/wiki/u8x8setupcpp
// or check the gallery:
// https://github.com/olikraus/u8g2/wiki/gallery
// --> First choise of cheap I2C OLED 128X32 0.91"
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(U8X8_PIN_NONE, U8X8_PIN_SCL, U8X8_PIN_SDA); // Pins are Reset, SCL, SDA
// --> Second choise of cheap I2C OLED 128X64 0.96" or 1.3"
//U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE, U8X8_PIN_SCL, U8X8_PIN_SDA); // Pins are Reset, SCL, SDA
// gets called once at boot. Do all initialization that doesn't depend on
// network here
void userSetup() {
  sensor.begin(); //Start Dallas temperature sensor
  u8x8.begin();
  //u8x8.setFlipMode(1); //Uncoment if using WLED Wemos shield 
  u8x8.setPowerSave(0);
  u8x8.setContrast(10); //Contrast setup will help to preserve OLED lifetime. In case OLED need to be brighter increase number up to 255
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 0, "Loading...");
}

// gets called every time WiFi is (re-)connected. Initialize own network
// interfaces here
void userConnected() {}

// needRedraw marks if redraw is required to prevent often redrawing.
bool needRedraw = true;

// Next variables hold the previous known values to determine if redraw is
// required.
String knownSsid = "";
IPAddress knownIp;
uint8_t knownBrightness = 0;
uint8_t knownMode = 0;
uint8_t knownPalette = 0;

long lastUpdate = 0;
long lastRedraw = 0;
bool displayTurnedOff = false;
// How often we are redrawing screen
#define USER_LOOP_REFRESH_RATE_MS 5000

void userLoop() {

//----> Dallas temperature sensor MQTT publishing
  temptimer = millis();  
// Timer to publishe new temperature every 60 seconds
  if (temptimer - lastMeasure > 60000) 
  {
    lastMeasure = temptimer;    
//Check if MQTT Connected, otherwise it will crash the 8266
    if (mqtt != nullptr)
    {
      sensor.requestTemperatures();
//Gets prefered temperature scale based on selection in definitions section
      #ifdef Celsius
      float board_temperature = sensor.getTempCByIndex(0);
      #else
      float board_temperature = sensor.getTempFByIndex(0);
      #endif
//Create character string populated with user defined device topic from the UI, and the read temperature. Then publish to MQTT server.
      char subuf[38];
      strcpy(subuf, mqttDeviceTopic);
      strcat(subuf, "/temperature");
      mqtt->publish(subuf, 0, true, String(board_temperature).c_str());
    }
  }

  // Check if we time interval for redrawing passes.
  if (millis() - lastUpdate < USER_LOOP_REFRESH_RATE_MS) {
    return;
  }
  lastUpdate = millis();
  
  // Turn off display after 3 minutes with no change.
  if(!displayTurnedOff && millis() - lastRedraw > 3*60*1000) {
    u8x8.setPowerSave(1);
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
    u8x8.setPowerSave(0);
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
  u8x8.clear();
  u8x8.setFont(u8x8_font_chroma48medium8_r);

  // First row with Wifi name
  u8x8.setCursor(1, 0);
  u8x8.print(knownSsid.substring(0, u8x8.getCols() > 1 ? u8x8.getCols() - 2 : 0));
  // Print `~` char to indicate that SSID is longer, than owr dicplay
  if (knownSsid.length() > u8x8.getCols())
    u8x8.print("~");

  // Second row with IP or Psssword
  u8x8.setCursor(1, 1);
  // Print password in AP mode and if led is OFF.
  if (apActive && bri == 0)
    u8x8.print(apPass);
  else
    u8x8.print(knownIp);

  // Third row with mode name
  u8x8.setCursor(2, 2);
  char lineBuffer[17];
  extractModeName(knownMode, JSON_mode_names, lineBuffer, 16);
  u8x8.print(lineBuffer);

  // Fourth row with palette name
  u8x8.setCursor(2, 3);
  extractModeName(knownPalette, JSON_palette_names, lineBuffer, 16);
  u8x8.print(lineBuffer);

  u8x8.setFont(u8x8_font_open_iconic_embedded_1x1);
  u8x8.drawGlyph(0, 0, 80); // wifi icon
  u8x8.drawGlyph(0, 1, 68); // home icon
  u8x8.setFont(u8x8_font_open_iconic_weather_2x2);
  u8x8.drawGlyph(0, 2, 66 + (bri > 0 ? 3 : 0)); // sun/moon icon
}