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
    #ifndef LOAD_GLCD
        #error Please define LOAD_GLCD
    #endif
#endif
#ifndef TFT_BL
    #define TFT_BL -1
#endif

#define USERMOD_ID_M5STICK_VISUALISER 541

TFT_eSPI tft = TFT_eSPI(TFT_WIDTH, TFT_HEIGHT); // Invoke custom library

// Extra char (+1) for null
#define LINE_BUFFER_SIZE          20

// How often we are redrawing screen
#define USER_LOOP_REFRESH_RATE_MS 50

extern int getSignalQuality(int rssi);


//class name. Use something descriptive and leave the ": public Usermod" part :)
class M5Stick2Visualiser : public Usermod {
  private:
    //Private class members. You can declare variables and functions only accessible to your usermod here
    unsigned long lastTime = 0;
    bool enabled = true;

    bool displayTurnedOff = false;
    long lastRedraw = 0;
    // needRedraw marks if redraw is required to prevent often redrawing.
    bool needRedraw = true;

    uint8_t knownBrightness = 0;
    uint8_t knownPalette = 0;
    uint8_t knownEffectSpeed = 0;
    uint8_t knownEffectIntensity = 0;
    uint8_t knownMode = 0;

    const uint8_t tftcharwidth = 19;  // Number of chars that fit on screen with text size set to 2
    long lastUpdate = 0;
    
    uint8_t chan_width = TFT_HEIGHT / NUM_GEQ_CHANNELS;

    void center(String &line, uint8_t width) {
      int len = line.length();
      if (len<width) for (byte i=(width-len)/2; i>0; i--) line = ' ' + line;
      for (byte i=line.length(); i<width; i++) line += ' ';
    }

  public:
    //Functions called by WLED

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup()
    {
        DEBUG_PRINTLN("TFT pre init");
        PinManagerPinType spiPins[] = { { spi_mosi, true }, { spi_miso, false}, { spi_sclk, true } };
        DEBUG_PRINTLN("TFT try to allocate SPI");
        if (!pinManager.allocateMultiplePins(spiPins, 3, PinOwner::HW_SPI)) { enabled = false; return; }
        PinManagerPinType displayPins[] = { { TFT_CS, true}, { TFT_DC, true}, { TFT_RST, true }, { TFT_BL, true } };
        DEBUG_PRINTLN("TFT try to allocate display");
        if (!pinManager.allocateMultiplePins(displayPins, sizeof(displayPins)/sizeof(PinManagerPinType), PinOwner::UM_FourLineDisplay)) {
            pinManager.deallocateMultiplePins(spiPins, 3, PinOwner::HW_SPI);
            enabled = false;
            return;
        }
        DEBUG_PRINTLN("TFT init");

        tft.init();
        tft.setRotation(0);  //Rotation here is set up for the text to be readable with the port on the left. Use 1 to flip.
        tft.fillScreen(TFT_BLACK);

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
        // to be used in the future for sprintf stuff
        char buff[LINE_BUFFER_SIZE];

        tft.setTextSize(2);

        // Check if we time interval for redrawing passes.
        if (millis() - lastUpdate < USER_LOOP_REFRESH_RATE_MS)
        {
            return;
        }
        lastUpdate = millis();
  
        // Turn off display after 5 minutes with no change.
        if (!displayTurnedOff && millis() - lastRedraw > 5*60*1000)
        {
            DEBUG_PRINTLN("Turn off display");
            if (TFT_BL >= 0) digitalWrite(TFT_BL, LOW); // Turn backlight off. 
            displayTurnedOff = true;
        } 

        lastRedraw = millis();

        knownBrightness = bri;
        knownMode = strip.getMainSegment().mode;
        knownPalette = strip.getMainSegment().palette;
        knownEffectSpeed = strip.getMainSegment().speed;
        knownEffectIntensity = strip.getMainSegment().intensity;
        tft.setTextSize(1);
        tft.setTextColor(TFT_GREEN);
        tft.setTextFont(1);
        tft.setCursor(100, 100);
        
        um_data_t *um_data;
        if (!usermods.getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) {
          // add support for no audio                                                                                                                           
          um_data = simulateSound(SEGMENT.soundSim);
        }

        uint8_t *fftChannels = (uint8_t*)um_data->u_data[2];

        for(int i = 0; i < NUM_GEQ_CHANNELS; i++) {
            tft.fillRect(fftChannels[i] / 2, chan_width * i, 135 - (fftChannels[i] / 2), chan_width - 1, TFT_BLACK);
            tft.fillRect(0, chan_width * i, fftChannels[i] / 2, chan_width - 1, TFT_GOLD);
        }
    }

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    void addToJsonInfo(JsonObject& root)
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
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject("ST7789");
      JsonArray pins = top.createNestedArray("pin");
      pins.add(TFT_CS);
      pins.add(TFT_DC);
      pins.add(TFT_RST);
      pins.add(TFT_BL);
      //top["great"] = userVar0; //save this var persistently whenever settings are saved
    }


    void appendConfigData() {
      oappend(SET_F("addInfo('ST7789:pin[]',0,'','SPI CS');"));
      oappend(SET_F("addInfo('ST7789:pin[]',1,'','SPI DC');"));
      oappend(SET_F("addInfo('ST7789:pin[]',2,'','SPI RST');"));
      oappend(SET_F("addInfo('ST7789:pin[]',2,'','SPI BL');"));
    }

    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens once immediately after boot)
     *
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     */
    bool readFromConfig(JsonObject& root)
    {
      //JsonObject top = root["top"];
      //userVar0 = top["great"] | 42; //The value right of the pipe "|" is the default value in case your setting was not present in cfg.json (e.g. first boot)
      return true;
    }


    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return USERMOD_ID_M5STICK_VISUALISER;
    }

   //More methods can be added in the future, this example will then be extended.
   //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};