#include "wled.h"
#include <Arduino.h>
#include <U8x8lib.h> // from https://github.com/olikraus/u8g2/
#include <Wire.h>
#include <BME280I2C.h> //BME280 sensor

void UpdateBME280Data();

#define Celsius // Show temperature mesaurement in Celcius otherwise is in Fahrenheit 
BME280I2C bme;    // Default : forced mode, standby time = 1000 ms
                  // Oversampling = pressure ×1, temperature ×1, humidity ×1, filter off,

#ifdef ARDUINO_ARCH_ESP32 //ESP32 boards
uint8_t SCL_PIN = 22;
uint8_t SDA_PIN = 21;
#else //ESP8266 boards
uint8_t SCL_PIN = 5;
uint8_t SDA_PIN = 4;
// uint8_t RST_PIN = 16; // Uncoment for Heltec WiFi-Kit-8
#endif

//The SCL and SDA pins are defined here.
//ESP8266 Wemos D1 mini board use SCL=5 SDA=4 while ESP32 Wemos32 mini board use SCL=22 SDA=21
#define U8X8_PIN_SCL SCL_PIN
#define U8X8_PIN_SDA SDA_PIN
//#define U8X8_PIN_RESET RST_PIN // Uncoment for Heltec WiFi-Kit-8

// If display does not work or looks corrupted check the
// constructor reference:
// https://github.com/olikraus/u8g2/wiki/u8x8setupcpp
// or check the gallery:
// https://github.com/olikraus/u8g2/wiki/gallery
// --> First choise of cheap I2C OLED 128X32 0.91"
U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(U8X8_PIN_NONE, U8X8_PIN_SCL, U8X8_PIN_SDA); // Pins are Reset, SCL, SDA
// --> Second choise of cheap I2C OLED 128X64 0.96" or 1.3"
//U8X8_SSD1306_128X64_NONAME_HW_I2C u8x8(U8X8_PIN_NONE, U8X8_PIN_SCL, U8X8_PIN_SDA); // Pins are Reset, SCL, SDA
// --> Third choise of Heltec WiFi-Kit-8 OLED 128X32 0.91"
//U8X8_SSD1306_128X32_UNIVISION_HW_I2C u8x8(U8X8_PIN_RESET, U8X8_PIN_SCL, U8X8_PIN_SDA); // Constructor for Heltec WiFi-Kit-8
// gets called once at boot. Do all initialization that doesn't depend on network here

// BME280 sensor timer
long tempTimer = millis();
long lastMeasure = 0;

float SensorPressure(NAN);
float SensorTemperature(NAN);
float SensorHumidity(NAN);

void userSetup() {
  u8x8.begin();
  u8x8.setPowerSave(0);
  u8x8.setFlipMode(1);
  u8x8.setContrast(10); //Contrast setup will help to preserve OLED lifetime. In case OLED need to be brighter increase number up to 255
  u8x8.setFont(u8x8_font_chroma48medium8_r);
  u8x8.drawString(0, 0, "Loading...");
  Wire.begin(SDA_PIN,SCL_PIN);

while(!bme.begin())
  {
    Serial.println("Could not find BME280I2C sensor!");
    delay(1000);
  }
switch(bme.chipModel())
  {
    case BME280::ChipModel_BME280:
      Serial.println("Found BME280 sensor! Success.");
      break;
    case BME280::ChipModel_BMP280:
      Serial.println("Found BMP280 sensor! No Humidity available.");
      break;
    default:
      Serial.println("Found UNKNOWN sensor! Error!");
  }
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

// BME280 sensor MQTT publishing
  tempTimer = millis();  
// Timer to publish new sensor data every 60 seconds
  if (tempTimer - lastMeasure > 60000) 
  {
    lastMeasure = tempTimer;    

#ifndef WLED_DISABLE_MQTT
// Check if MQTT Connected, otherwise it will crash the 8266
    if (mqtt != nullptr)
    {
      UpdateBME280Data();
      float board_temperature = SensorTemperature;
      float board_pressure = SensorPressure;
      float board_humidity = SensorHumidity;

// Create string populated with user defined device topic from the UI, and the read temperature, humidity and pressure. Then publish to MQTT server.
      String t = String(mqttDeviceTopic);
      t += "/temperature";
      mqtt->publish(t.c_str(), 0, true, String(board_temperature).c_str());
      String p = String(mqttDeviceTopic);
      p += "/pressure";
      mqtt->publish(p.c_str(), 0, true, String(board_pressure).c_str());
      String h = String(mqttDeviceTopic);
      h += "/humidity";
      mqtt->publish(h.c_str(), 0, true, String(board_humidity).c_str());
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

void UpdateBME280Data() {
  float temp(NAN), hum(NAN), pres(NAN);
#ifdef Celsius
  BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  bme.read(pres, temp, hum, tempUnit, presUnit);
#else
  BME280::TempUnit tempUnit(BME280::TempUnit_Fahrenheit);
  BME280::PresUnit presUnit(BME280::PresUnit_Pa);
  bme.read(pres, temp, hum, tempUnit, presUnit);
#endif
  SensorTemperature=temp;
  SensorHumidity=hum;
  SensorPressure=pres;
}
