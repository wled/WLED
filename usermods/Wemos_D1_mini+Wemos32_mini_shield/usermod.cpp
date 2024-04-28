#include "wled.h"
#include <Arduino.h>
#include <U8x8lib.h> // from https://github.com/olikraus/u8g2/
#include <OneWire.h> // Dallas temperature sensor

//Dallas sensor quick reading. Credit to - Author: Peter Scargill, August 17th, 2013
int16_t Dallas(int x, byte start)
{
    OneWire DallasSensor(x);
    byte i;
    byte data[2];
    int16_t result;
    do
    {
      DallasSensor.reset();
      DallasSensor.write(0xCC);
      DallasSensor.write(0xBE);
      for ( i = 0; i < 2; i++) data[i] = DallasSensor.read();
      result=(data[1]<<8)|data[0];
      result>>=4; if (data[1]&128) result|=61440;
      if (data[0]&8) ++result;
      DallasSensor.reset();
      DallasSensor.write(0xCC);
      DallasSensor.write(0x44,1);
      if (start) delay(1000);
    } while (start--);
      return result;
}
#ifdef ARDUINO_ARCH_ESP32
uint8_t SCL_PIN = 22; 
uint8_t SDA_PIN = 21;
uint8_t DALLAS_PIN =23;
#else
uint8_t SCL_PIN = 5;
uint8_t SDA_PIN = 4;
uint8_t DALLAS_PIN =13;
// uint8_t RST_PIN = 16; // Un-comment for Heltec WiFi-Kit-8
#endif

//The SCL and SDA pins are defined here.
//ESP8266 Wemos D1 mini board use SCL=5 SDA=4 while ESP32 Wemos32 mini board use SCL=22 SDA=21
#define U8X8_PIN_SCL SCL_PIN
#define U8X8_PIN_SDA SDA_PIN
//#define U8X8_PIN_RESET RST_PIN // Un-comment for Heltec WiFi-Kit-8

// Dallas sensor reading timer
long temptimer = millis();
long lastMeasure = 0;
#define Celsius // Show temperature measurement in Celsius otherwise is in Fahrenheit 

// If display does not work or looks corrupted check the
// constructor reference:
// https://github.com/olikraus/u8g2/wiki/u8x8setupcpp
// or check the gallery:
// https://github.com/olikraus/u8g2/wiki/gallery
// --> First choice of cheap I2C OLED 128X32 0.91"
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(U8X8_PIN_NONE, U8X8_PIN_SCL, U8X8_PIN_SDA); // Pins are Reset, SCL, SDA
// --> Second choice of cheap I2C OLED 128X64 0.96" or 1.3"
//U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE, U8X8_PIN_SCL, U8X8_PIN_SDA); // Pins are Reset, SCL, SDA
// --> Third choice of Heltec WiFi-Kit-8 OLED 128X32 0.91"
//U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(U8X8_PIN_RESET, U8X8_PIN_SCL, U8X8_PIN_SDA); // Constructor for Heltec WiFi-Kit-8
// gets called once at boot. Do all initialization that doesn't depend on network here
void userSetup() {
//Serial.begin(115200);

  Dallas (DALLAS_PIN,1);
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFlipMode(1);
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
// Timer to publish new temperature every 60 seconds
  if (temptimer - lastMeasure > 60000) 
  {
    lastMeasure = temptimer;    
#ifndef WLED_DISABLE_MQTT
//Check if MQTT Connected, otherwise it will crash the 8266
    if (mqtt != nullptr)
    {
//      Serial.println(Dallas(DALLAS_PIN,0));
//Gets preferred temperature scale based on selection in definitions section
        #ifdef Celsius
        int16_t board_temperature = Dallas(DALLAS_PIN,0);
        #else
        int16_t board_temperature = (Dallas(DALLAS_PIN,0)* 1.8 + 32);
        #endif
//Create character string populated with user defined device topic from the UI, and the read temperature. Then publish to MQTT server.
        String t = String(mqttDeviceTopic);
        t += "/temperature";
        mqtt->publish(t.c_str(), 0, true, String(board_temperature).c_str());
    }
  #endif
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
  #ifdef ARDUINO_ARCH_ESP32
  knownSsid = WiFi.SSID();
  #else
  knownSsid = apActive ? WiFi.softAPSSID() : WiFi.SSID();
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
  // Print `~` char to indicate that SSID is longer than our display
  if (knownSsid.length() > u8x8.getCols())
    u8x8.print("~");

  // Second row with IP or Password
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
