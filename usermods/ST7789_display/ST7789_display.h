// Credits to @mrVanboy, @gwaland and my dearest friend @westward
// Also for @spiff72 for usermod TTGO-T-Display
// 210217
#pragma once

#include "wled.h"
#include <TFT_eSPI.h>
#include <SPI.h>

#define USERMOD_ST7789_DISPLAY 97

#ifndef TFT_DISPOFF
#define TFT_DISPOFF 0x28
#endif

#ifndef TFT_SLPIN
#define TFT_SLPIN   0x10
#endif

#define TFT_MOSI            21
#define TFT_SCLK            22
#define TFT_DC              18
#define TFT_RST             5
#define TFT_BL              26  // Display backlight control pin

TFT_eSPI tft = TFT_eSPI(240, 240); // Invoke custom library

// How often we are redrawing screen
#define USER_LOOP_REFRESH_RATE_MS 1000


//class name. Use something descriptive and leave the ": public Usermod" part :)
class St7789DisplayUsermod : public Usermod {
  private:
    //Private class members. You can declare variables and functions only accessible to your usermod here
    unsigned long lastTime = 0;

    bool displayTurnedOff = false;
    long lastRedraw = 0;
    // needRedraw marks if redraw is required to prevent often redrawing.
    bool needRedraw = true;
    // Next variables hold the previous known values to determine if redraw is required.
    String knownSsid = "";
    IPAddress knownIp;
    uint8_t knownBrightness = 0;
    uint8_t knownMode = 0;
    uint8_t knownPalette = 0;
    uint8_t tftcharwidth = 19;  // Number of chars that fit on screen with text size set to 2
    long lastUpdate = 0;

  public:
    //Functions called by WLED

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup()
    {
        tft.init();
        tft.setRotation(0);  //Rotation here is set up for the text to be readable with the port on the left. Use 1 to flip.
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.setCursor(60, 100);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(2);
        tft.print("Loading...");
        if (TFT_BL > 0) 
        { // TFT_BL has been set in the TFT_eSPI library
         pinMode(TFT_BL, OUTPUT); // Set backlight pin to output mode
         digitalWrite(TFT_BL, HIGH); // Turn backlight on.
        }
    }

    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() {
      //Serial.println("Connected to WiFi!");
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
// Check if we time interval for redrawing passes.
    if (millis() - lastUpdate < USER_LOOP_REFRESH_RATE_MS)
        {
            return;
        }
    lastUpdate = millis();
  
// Turn off display after 5 minutes with no change.
    if(!displayTurnedOff && millis() - lastRedraw > 5*60*1000)
        {
            digitalWrite(TFT_BL, LOW); // Turn backlight off. 
            displayTurnedOff = true;
        } 

// Check if values which are shown on display changed from the last time.
    if (((apActive) ? String(apSSID) : WiFi.SSID()) != knownSsid)
    {
    needRedraw = true;
    }
    else if (knownIp != (apActive ? IPAddress(4, 3, 2, 1) : WiFi.localIP()))
    {
    needRedraw = true;
    }
    else if (knownBrightness != bri)
    {
    needRedraw = true;
    }
    else if (knownMode != strip.getMainSegment().mode)
    {
    needRedraw = true;
    }
    else if (knownPalette != strip.getMainSegment().palette)
    {
    needRedraw = true;
    }

    if (!needRedraw)
    {
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
    tft.setTextSize(2);
// First row with Wifi name
    tft.setTextColor(TFT_SILVER);
    tft.setCursor(3, 40);
    tft.print(knownSsid.substring(0, tftcharwidth > 1 ? tftcharwidth - 1 : 0));
// Print `~` char to indicate that SSID is longer, than our dicplay
    if (knownSsid.length() > tftcharwidth)
        tft.print("~");

// Second row with AP IP and Password or IP
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);
    tft.setCursor(3, 64);
// Print AP IP and password in AP mode or knownIP if AP not active.

    if (apActive)
    {
    tft.setTextColor(TFT_YELLOW);
    tft.print("AP IP: ");
    tft.print(knownIp);
    tft.setCursor(3,86);
    tft.setTextColor(TFT_YELLOW);
    tft.print("AP Pass:");
    tft.print(apPass);
    }
    else
    {
    tft.setTextColor(TFT_GREEN);
    tft.print("IP: ");
    tft.print(knownIp);
    tft.setCursor(3,86);
    //tft.print("Signal Strength: ");
    //tft.print(i.wifi.signal);
    tft.setTextColor(TFT_WHITE);
    tft.print("Bri: ");
    tft.print(((float(bri)/255)*100),0);
    tft.print("%");
    }

// Third row with mode name
    tft.setCursor(3, 108);
    uint8_t qComma = 0;
    bool insideQuotes = false;
    uint8_t printedChars = 0;
    char singleJsonSymbol;
// Find the mode name in JSON
    for (size_t i = 0; i < strlen_P(JSON_mode_names); i++)
    {
        singleJsonSymbol = pgm_read_byte_near(JSON_mode_names + i);
        switch (singleJsonSymbol)
        {
            case '"':
            insideQuotes = !insideQuotes;
        break;
        case '[':
        case ']':
        break;
        case ',':
        qComma++;
        default:
            if (!insideQuotes || (qComma != knownMode))
            break;
        tft.setTextColor(TFT_MAGENTA);
        tft.print(singleJsonSymbol);
        printedChars++;
        }
    if ((qComma > knownMode) || (printedChars > tftcharwidth - 1))
      break;
    }
// Fourth row with palette name
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(3, 130);
    qComma = 0;
    insideQuotes = false;
    printedChars = 0;
// Looking for palette name in JSON.
    for (size_t i = 0; i < strlen_P(JSON_palette_names); i++)
    {
    singleJsonSymbol = pgm_read_byte_near(JSON_palette_names + i);
        switch (singleJsonSymbol)
        {
        case '"':
        insideQuotes = !insideQuotes;
        break;
        case '[':
        case ']':
        break;
        case ',':
        qComma++;
        default:
            if (!insideQuotes || (qComma != knownPalette))
            break;
        tft.print(singleJsonSymbol);
        printedChars++;
        }
// The following is modified from the code from the u8g2/u8g8 based code (knownPalette was knownMode)
    if ((qComma > knownPalette) || (printedChars > tftcharwidth - 1))
      break;
    }
// Fifth row with estimated mA usage
    tft.setTextColor(TFT_SILVER);
    tft.setCursor(3, 152);
// Print estimated milliamp usage (must specify the LED type in LED prefs for this to be a reasonable estimate).
    tft.print("Current: ");
    tft.print(strip.currentMilliamps);
    tft.print("mA");
    }
    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    /*
    void addToJsonInfo(JsonObject& root)
    {
      int reading = 20;
      //this code adds "u":{"Light":[20," lux"]} to the info object
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray lightArr = user.createNestedArray("Light"); //name
      lightArr.add(reading); //value
      lightArr.add(" lux"); //unit
    }
    */


    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root)
    {
      //root["user0"] = userVar0;
    }


    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void readFromJsonState(JsonObject& root)
    {
      userVar0 = root["user0"] | userVar0; //if "user0" key exists in JSON, update, else keep old value
      //if (root["bri"] == 255) Serial.println(F("Don't burn down your garage!"));
    }


    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current state, use serializeConfig() in your loop().
     *
     * CAUTION: serializeConfig() will initiate a filesystem write operation.
     * It might cause the LEDs to stutter and will cause flash wear if called too often.
     * Use it sparingly and always in the loop, never in network callbacks!
     *
     * addToConfig() will also not yet add your setting to one of the settings pages automatically.
     * To make that work you still have to add the setting to the HTML, xml.cpp and set.cpp manually.
     *
     * I highly recommend checking out the basics of ArduinoJson serialization and deserialization in order to use custom settings!
     */
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject("exampleUsermod");
      top["great"] = userVar0; //save this var persistently whenever settings are saved
    }


    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens once immediately after boot)
     *
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     */
    void readFromConfig(JsonObject& root)
    {
      JsonObject top = root["top"];
      userVar0 = top["great"] | 42; //The value right of the pipe "|" is the default value in case your setting was not present in cfg.json (e.g. first boot)
    }


    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ST7789_DISPLAY;
    }

   //More methods can be added in the future, this example will then be extended.
   //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};