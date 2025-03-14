// Credits to @mrVanboy, @gwaland and my dearest friend @westward
// Also for @spiff72 for usermod TTGO-T-Display
// 210217
#pragma once

#include "wled.h"
#include <TFT_eSPI.h>
#include <SPI.h>

#ifndef USER_SETUP_LOADED
    #ifndef ST7789_DRIVER
        #error Please define ST7789_DRIVER
    #endif
    #ifndef TFT_WIDTH
        #error Please define TFT_WIDTH
    #endif
    #ifndef TFT_HEIGHT
        #error Please define TFT_HEIGHT
    #endif
    #ifndef TFT_DC
        #error Please define TFT_DC
    #endif
    #ifndef TFT_RST
        #error Please define TFT_RST
    #endif
    #ifndef TFT_CS
        #error Please define TFT_CS
    #endif
    #ifndef LOAD_GLCD
        #error Please define LOAD_GLCD
    #endif
#endif
#ifndef TFT_BL
    #define TFT_BL -1
#endif

#define USERMOD_ID_ST7789_DISPLAY 97

TFT_eSPI tft = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT); // Invoke custom library

// Extra char (+1) for null
#define LINE_BUFFER_SIZE          20

// How often we are redrawing screen
#define USER_LOOP_REFRESH_RATE_MS 1000

extern int getSignalQuality(int rssi);


//class name. Use something descriptive and leave the ": public Usermod" part :)
class St7789DisplayUsermod : public Usermod {
  private:
    //Private class members. You can declare variables and functions only accessible to your usermod here
    unsigned long lastTime = 0;
    bool enabled = true;

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
    uint8_t knownEffectSpeed = 0;
    uint8_t knownEffectIntensity = 0;
    uint8_t knownMinute = 99;
    uint8_t knownHour = 99;

    const uint8_t tftcharwidth = 19;  // Number of chars that fit on screen with text size set to 2
    long lastUpdate = 0;

    void center(String &line, uint8_t width) {
      int len = line.length();
      if (len<width) for (byte i=(width-len)/2; i>0; i--) line = ' ' + line;
      for (byte i=line.length(); i<width; i++) line += ' ';
    }

    /**
     * Display the current date and time in large characters
     * on the middle rows. Based 24 or 12 hour depending on
     * the useAMPM configuration.
     */
    void showTime() {
        if (!ntpEnabled) return;
        char lineBuffer[LINE_BUFFER_SIZE];

        updateLocalTime();
        byte minuteCurrent = minute(localTime);
        byte hourCurrent   = hour(localTime);
        //byte secondCurrent = second(localTime);
        knownMinute = minuteCurrent;
        knownHour = hourCurrent;

        byte currentMonth = month(localTime);
        sprintf_P(lineBuffer, PSTR("%s %2d "), monthShortStr(currentMonth), day(localTime));
        tft.setTextColor(TFT_SILVER);
        tft.setCursor(84, 0);
        tft.setTextSize(2);
        tft.print(lineBuffer);

        byte showHour = hourCurrent;
        boolean isAM = false;
        if (useAMPM) {
            if (showHour == 0) {
                showHour = 12;
                isAM = true;
            } else if (showHour > 12) {
                showHour -= 12;
                isAM = false;
            } else {
                isAM = true;
            }
        }

        sprintf_P(lineBuffer, PSTR("%2d:%02d"), (useAMPM ? showHour : hourCurrent), minuteCurrent);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(4);
        tft.setCursor(60, 24);
        tft.print(lineBuffer);

        tft.setTextSize(2);
        tft.setCursor(186, 24);
        //sprintf_P(lineBuffer, PSTR("%02d"), secondCurrent);
        if (useAMPM) tft.print(isAM ? "AM" : "PM");
        //else         tft.print(lineBuffer);
    }

  public:
    //Functions called by WLED

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() override
    {
        PinManagerPinType spiPins[] = { { spi_mosi, true }, { spi_miso, false}, { spi_sclk, true } };
        if (!PinManager::allocateMultiplePins(spiPins, 3, PinOwner::HW_SPI)) { enabled = false; return; }
        PinManagerPinType displayPins[] = { { TFT_CS, true}, { TFT_DC, true}, { TFT_RST, true }, { TFT_BL, true } };
        if (!PinManager::allocateMultiplePins(displayPins, sizeof(displayPins)/sizeof(PinManagerPinType), PinOwner::UM_FourLineDisplay)) {
            PinManager::deallocateMultiplePins(spiPins, 3, PinOwner::HW_SPI);
            enabled = false;
            return;
        }

        tft.init();
        tft.setRotation(0);  //Rotation here is set up for the text to be readable with the port on the left. Use 1 to flip.
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED);
        tft.setCursor(60, 100);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(2);
        tft.print("Loading...");
        if (TFT_BL >= 0) 
        {
            pinMode(TFT_BL, OUTPUT); // Set backlight pin to output mode
            digitalWrite(TFT_BL, HIGH); // Turn backlight on.
        }
    }

    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() override {
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
    void loop() override {
        char buff[LINE_BUFFER_SIZE];

        // Check if we time interval for redrawing passes.
        if (millis() - lastUpdate < USER_LOOP_REFRESH_RATE_MS)
        {
            return;
        }
        lastUpdate = millis();
  
        // Turn off display after 5 minutes with no change.
        if (!displayTurnedOff && millis() - lastRedraw > 5*60*1000)
        {
            if (TFT_BL >= 0) digitalWrite(TFT_BL, LOW); // Turn backlight off. 
            displayTurnedOff = true;
        } 

        // Check if values which are shown on display changed from the last time.
        if ((((apActive) ? String(apSSID) : WiFi.SSID()) != knownSsid) ||
            (knownIp != (apActive ? IPAddress(4, 3, 2, 1) : Network.localIP())) ||
            (knownBrightness != bri) ||
            (knownEffectSpeed != strip.getMainSegment().speed) ||
            (knownEffectIntensity != strip.getMainSegment().intensity) ||
            (knownMode != strip.getMainSegment().mode) ||
            (knownPalette != strip.getMainSegment().palette))
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
            digitalWrite(TFT_BL, HIGH); // Turn backlight on.
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
        knownEffectSpeed = strip.getMainSegment().speed;
        knownEffectIntensity = strip.getMainSegment().intensity;

        tft.fillScreen(TFT_BLACK);

        showTime();

        tft.setTextSize(2);

        // Wifi name
        tft.setTextColor(TFT_GREEN);
        tft.setCursor(0, 60);
        String line = knownSsid.substring(0, tftcharwidth-1);
        // Print `~` char to indicate that SSID is longer, than our display
        if (knownSsid.length() > tftcharwidth) line = line.substring(0, tftcharwidth-1) + '~';
        center(line, tftcharwidth);
        tft.print(line.c_str());

        // Print AP IP and password in AP mode or knownIP if AP not active.
        if (apActive)
        {
            tft.setCursor(0, 84);
            tft.print("AP IP: ");
            tft.print(knownIp);
            tft.setCursor(0,108);
            tft.print("AP Pass:");
            tft.print(apPass);
        }
        else
        {
            tft.setCursor(0, 84);
            line = knownIp.toString();
            center(line, tftcharwidth);
            tft.print(line.c_str());
            // percent brightness
            tft.setCursor(0, 120);
            tft.setTextColor(TFT_WHITE);
            tft.print("Bri: ");
            tft.print((((int)bri*100)/255));
            tft.print("%");
            // signal quality
            tft.setCursor(124,120);
            tft.print("Sig: ");
            if (getSignalQuality(WiFi.RSSI()) < 10) {
                tft.setTextColor(TFT_RED);
            } else if (getSignalQuality(WiFi.RSSI()) < 25) {
                tft.setTextColor(TFT_ORANGE);
            } else {
                tft.setTextColor(TFT_GREEN);
            }
            tft.print(getSignalQuality(WiFi.RSSI()));
            tft.setTextColor(TFT_WHITE);
            tft.print("%");
        }

        // mode name
        tft.setTextColor(TFT_CYAN);
        tft.setCursor(0, 144);
        char lineBuffer[tftcharwidth+1];
        extractModeName(knownMode, JSON_mode_names, lineBuffer, tftcharwidth);
        tft.print(lineBuffer);

        // palette name
        tft.setTextColor(TFT_YELLOW);
        tft.setCursor(0, 168);
        extractModeName(knownPalette, JSON_palette_names, lineBuffer, tftcharwidth);
        tft.print(lineBuffer);

        tft.setCursor(0, 192);
        tft.setTextColor(TFT_SILVER);
        sprintf_P(buff, PSTR("FX  Spd:%3d Int:%3d"), effectSpeed, effectIntensity);
        tft.print(buff);

        // Fifth row with estimated mA usage
        tft.setTextColor(TFT_SILVER);
        tft.setCursor(0, 216);
        // Print estimated milliamp usage (must specify the LED type in LED prefs for this to be a reasonable estimate).
        tft.print("Current: ");
        tft.setTextColor(TFT_ORANGE);
        tft.print(BusManager::currentMilliamps());
        tft.print("mA");
    }

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject& root) override
    {
      JsonObject user = root["u"];
      if (user.isNull()) user = root.createNestedObject("u");

      JsonArray lightArr = user.createNestedArray("ST7789"); //name
      lightArr.add(enabled?F("installed"):F("disabled")); //unit
    }


    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root) override
    {
      //root["user0"] = userVar0;
    }


    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void readFromJsonState(JsonObject& root) override
    {
      //userVar0 = root["user0"] | userVar0; //if "user0" key exists in JSON, update, else keep old value
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
    void addToConfig(JsonObject& root) override
    {
      JsonObject top = root.createNestedObject("ST7789");
      JsonArray pins = top.createNestedArray("pin");
      pins.add(TFT_CS);
      pins.add(TFT_DC);
      pins.add(TFT_RST);
      pins.add(TFT_BL);
      //top["great"] = userVar0; //save this var persistently whenever settings are saved
    }


    void appendConfigData() override {
      oappend(F("addInfo('ST7789:pin[]',0,'','SPI CS');"));
      oappend(F("addInfo('ST7789:pin[]',1,'','SPI DC');"));
      oappend(F("addInfo('ST7789:pin[]',2,'','SPI RST');"));
      oappend(F("addInfo('ST7789:pin[]',3,'','SPI BL');"));
    }

    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens once immediately after boot)
     *
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     */
    bool readFromConfig(JsonObject& root) override
    {
      //JsonObject top = root["top"];
      //userVar0 = top["great"] | 42; //The value right of the pipe "|" is the default value in case your setting was not present in cfg.json (e.g. first boot)
      return true;
    }


    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId() override
    {
      return USERMOD_ID_ST7789_DISPLAY;
    }

   //More methods can be added in the future, this example will then be extended.
   //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};