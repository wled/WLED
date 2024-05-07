#pragma once

#include <Arduino.h>              // WLEDMM: make sure that I2C drivers have the "right" Wire Object
#include <Wire.h>
#include <SPI.h>
#undef U8X8_NO_HW_I2C             // WLEDMM: we do want I2C hardware drivers - if possible
//#define WIRE_INTERFACES_COUNT 2 // experimental - tell U8x8Lib that there is a second Wire unit

#include "wled.h"
#include <U8x8lib.h> // from https://github.com/olikraus/u8g2/
#include "4LD_wled_fonts.c"

#ifndef FLD_ESP32_NO_THREADS
  #define FLD_ESP32_USE_THREADS  // comment out to use 0.13.x behaviour without parallel update task - slower, but more robust. May delay other tasks like LEDs or audioreactive!!
#endif

//#define OLD_4LD_FONTS          // comment out if you prefer the "classic" look with blocky fonts (saves 1K flash)

//
// Inspired by the usermod_v2_four_line_display
//
// v2 usermod for using 128x32 or 128x64 i2c
// OLED displays to provide a four line display
// for WLED.
//
// Dependencies
// * This usermod does not REQUIRE the ModeSortUsermod any more
// * This usermod works best, by far, when coupled 
//   with RotaryEncoderUI_ALT usermod.
//
// Make sure to enable NTP and set your time zone in WLED Config | Time.
//
// REQUIREMENT: You must add the following requirements to
// REQUIREMENT: "lib_deps" within platformio.ini / platformio_override.ini
// REQUIREMENT: olikraus/U8g2@ ^2.34.15 (the version already in platformio.ini is fine)
//

//The SCL and SDA pins are defined here. 
#ifndef FLD_PIN_SCL
  #define FLD_PIN_SCL -1
#endif
#ifndef FLD_PIN_SDA
  #define FLD_PIN_SDA -1
#endif
#ifndef FLD_PIN_CLOCKSPI
  #define FLD_PIN_CLOCKSPI -1
#endif
#ifndef FLD_PIN_MOSISPI //WLEDMM renamed from HW_PIN_DATASPI
  #define FLD_PIN_MOSISPI -1
#endif   
#ifndef FLD_PIN_CS
  #define FLD_PIN_CS -1
#endif

#ifndef FLD_PIN_DC
  #define FLD_PIN_DC -1
#endif
#ifndef FLD_PIN_RESET
  #define FLD_PIN_RESET -1
#endif

#ifndef FLD_TYPE
  #ifndef FLD_SPI_DEFAULT
    #define FLD_TYPE SSD1306
  #else
    #define FLD_TYPE SSD1306_SPI
  #endif
#endif

// When to time out to the clock or blank the screen
// if SLEEP_MODE_ENABLED.
#define SCREEN_TIMEOUT_MS  60*1000    // 1 min

// Minimum time between redrawing screen in ms
#define REFRESH_RATE_MS 1000

// Extra char (+1) for null
#define LINE_BUFFER_SIZE            16+1
#define MAX_JSON_CHARS              19+1
#define MAX_MODE_LINE_SPACE         13+1


#ifdef ARDUINO_ARCH_ESP32
static TaskHandle_t Display_Task = nullptr;
void DisplayTaskCode(void * parameter);
#endif


typedef enum {
  NONE = 0,
  SSD1306,      // U8X8_SSD1306_128X32_UNIVISION_HW_I2C
  SH1106,       // U8X8_SH1106_128X64_WINSTAR_HW_I2C
  SSD1306_64,   // U8X8_SSD1306_128X64_NONAME_HW_I2C
  SSD1305,      // U8X8_SSD1305_128X32_ADAFRUIT_HW_I2C
  SSD1305_64,   // U8X8_SSD1305_128X64_ADAFRUIT_HW_I2C
  SSD1306_SPI,     // U8X8_SSD1306_128X32_NONAME_HW_SPI
  SSD1306_SPI64=7, // U8X8_SSD1306_128X64_NONAME_HW_SPI
  SSD1309_SPI64=8, // U8X8_SSD1309_128X64_NONAME0_4W_HW_SPI
  SSD1327_SPI128=9,// U8X8_SSD1327_WS_128X128_4W_SW_SPI
  SSD1309_64=10 // U8X8_SSD1309_128X64_NONAME2_HW_I2C
} DisplayType;


class FourLineDisplayUsermod : public Usermod {
  public:
#ifdef ARDUINO_ARCH_ESP32
    FourLineDisplayUsermod() { if (!instance) instance = this; }
    static FourLineDisplayUsermod* getInstance(void) { return instance; }
#endif

  private:

    static FourLineDisplayUsermod *instance;
    bool initDone = false;
    volatile bool drawing = false;          // true of overlay drawing is active
    volatile bool reDrawing = false;        // true if redraw ongoing (on esp32, this happens in a separate task)

    char errorMessage[100] = ""; //WLEDMM: show error in um settings if occurred

    // HW interface & configuration
    U8X8 *u8x8 = nullptr;           // pointer to U8X8 display object

    #ifndef FLD_SPI_DEFAULT
    int8_t ioPin[5] = {FLD_PIN_SCL, FLD_PIN_SDA, -1, -1, -1};        // I2C pins: SCL, SDA
    uint32_t ioFrequency = 400000;  // in Hz (minimum is 100000, baseline is 400000 and maximum should be 3400000)
    #else
    int8_t ioPin[5] = {FLD_PIN_CLOCKSPI, FLD_PIN_MOSISPI, FLD_PIN_CS, FLD_PIN_DC, FLD_PIN_RESET}; // SPI pins: CLK, MOSI, CS, DC, RST
    uint32_t ioFrequency = 1000000;  // in Hz (minimum is 500kHz, baseline is 1MHz and maximum should be 20MHz)
    #endif

    DisplayType type = FLD_TYPE;    // display type
    bool typeOK = false;             //WLEDMM: instead of type == NULL and type=NULL. Initially false, as display was not initialized yet
    bool flip = false;              // flip display 180°
    uint8_t contrast = 10;          // screen contrast
    uint8_t lineHeight = 1;         // 1 row or 2 rows
    uint16_t refreshRate = REFRESH_RATE_MS;     // in ms
    uint32_t screenTimeout = SCREEN_TIMEOUT_MS; // in ms
    bool sleepMode = true;          // allow screen sleep?
    bool clockMode = false;         // display clock
    bool showSeconds = true;        // display clock with seconds
#if defined(ARDUINO_ARCH_ESP32) 
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
    bool enabled = false;  // WLEDMM workaround for I2C bugs in IDF v4.4.1
#else
    bool enabled = true;
#endif
#else
    bool enabled = true;
#endif
    bool contrastFix = false;
    bool driverHW = false;
    bool driverSPI = false;

    // Next variables hold the previous known values to determine if redraw is
    // required.
    String knownSsid = apSSID;
    IPAddress knownIp = IPAddress(4, 3, 2, 1);
    uint8_t knownBrightness = 0;
    uint8_t knownEffectSpeed = 0;
    uint8_t knownEffectIntensity = 0;
    uint8_t knownMode = 0;
    uint8_t knownPalette = 0;
    uint8_t knownMinute = 99;
    uint8_t knownHour = 99;
    byte brightness100;
    byte fxspeed100;
    byte fxintensity100;
    bool knownnightlight = nightlightActive;
    bool wificonnected = interfacesInited;
    bool powerON = true;

    bool displayTurnedOff = false;
    unsigned long nextUpdate = 0;
    unsigned long lastRedraw = 0;
    unsigned long overlayUntil = 0;

    // Set to 2 or 3 to mark lines 2 or 3. Other values ignored.
    byte markLineNum = 255;
    byte markColNum = 255;

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _contrast[];
    static const char _refreshRate[];
    static const char _screenTimeOut[];
    static const char _flip[];
    static const char _sleepMode[];
    static const char _clockMode[];
    static const char _showSeconds[];
    static const char _busClkFrequency[];
    static const char _contrastFix[];

    // If display does not work or looks corrupted check the
    // constructor reference:
    // https://github.com/olikraus/u8g2/wiki/u8x8setupcpp
    // or check the gallery:
    // https://github.com/olikraus/u8g2/wiki/gallery

    // WLEDMM is this display using SPI?
    bool displayIsSPI(DisplayType disp) {
      switch(disp) {
        case SSD1306_SPI:   // falls thru
        case SSD1306_SPI64: // falls thru
        case SSD1309_SPI64: // falls thru
        case SSD1327_SPI128:
          return true;  // yes its SPI
          break; // makes compiler happy
        default: 
          return false; // no anything else is I2C
      }
    }

    // some displays need this to properly apply contrast
    void setVcomh(bool highContrast);

    /**
     * Wrappers for screen drawing
     */
    void setFlipMode(uint8_t mode);
    void setContrast(uint8_t contrast);
    void drawString(uint8_t col, uint8_t row, const char *string, bool ignoreLH=false);
    void draw2x2String(uint8_t col, uint8_t row, const char *string);
    void drawGlyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font, bool ignoreLH=false);
    void draw2x2Glyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font);
    uint8_t getCols();
    void clear();
    void setPowerSave(uint8_t save);

    void center(String &line, uint8_t width);

    void draw2x2GlyphIcons();

    /**
     * Display the current date and time in large characters
     * on the middle rows. Based 24 or 12 hour depending on
     * the useAMPM configuration.
     */
    void showTime();

  public:

    // gets called once at boot. Do all initialization that doesn't depend on
    // network here
    void setup();

    // gets called every time WiFi is (re-)connected. Initialize own network
    // interfaces here
    void connected();

    /**
     * Da loop.
     */
    void loop();

    //function to update lastredraw
    inline void updateRedrawTime() {lastRedraw = millis(); }

    //function to to check if a redraw or overlay draw is active. Needed for UM Rotary, to avoid random/concurrent drawing
    bool canDraw(void);

    /**
     * Redraw the screen (but only if things have changed
     * or if forceRedraw).
     */
    void redraw(bool forceRedraw);

    void redraw_core(bool forceRedraw);

    void updateBrightness();

    void updateSpeed();

    void updateIntensity();

    void drawStatusIcons();
    
    /**
     * marks the position of the arrow showing
     * the current setting being changed
     * pass line and colum info
     */
    void setMarkLine(byte newMarkLineNum, byte newMarkColNum);

    //Draw the arrow for the current setting being changed
    void drawArrow();

    //Display the current effect or palette (desiredEntry) 
    // on the appropriate line (row). 
    void showCurrentEffectOrPalette(int inputEffPal, const char *qstring, uint8_t row);

    /**
     * If there screen is off or in clock is displayed,
     * this will return true. This allows us to throw away
     * the first input from the rotary encoder but
     * to wake up the screen.
     */
    bool wakeDisplay();

    /**
     * Allows you to show one line and a glyph as overlay for a period of time.
     * Clears the screen and prints.
     * Used in Rotary Encoder usermod.
     */
    void overlay(const char* line1, long showHowLong, byte glyphType);

    /**
     * Allows you to show Akemi WLED logo overlay for a period of time.
     * Clears the screen and prints.
     */
    void overlayLogo(long showHowLong);

    /**
     * Allows you to show two lines as overlay for a period of time.
     * Clears the screen and prints.
     * Used in Auto Save usermod
     */
    void overlay(const char* line1, const char* line2, long showHowLong);

    void networkOverlay(const char* line1, long showHowLong);


    /**
     * Enable sleep (turn the display off) or clock mode.
     */
    void sleepOrClock(bool sleepEnable);

    /**
     * handleButton() can be used to override default button behaviour. Returning true
     * will prevent button working in a default way.
     * Replicating button.cpp
     */
    bool handleButton(uint8_t b);

    void onUpdateBegin(bool init);

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    //void addToJsonInfo(JsonObject& root) {
      //JsonObject user = root["u"];
      //if (user.isNull()) user = root.createNestedObject("u");
      //JsonArray data = user.createNestedArray(F("4LineDisplay"));
      //data.add(F("Loaded."));
    //}

    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    //void addToJsonState(JsonObject& root) {
    //}

    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    //void readFromJsonState(JsonObject& root) {
    //  if (!initDone) return;  // prevent crash on boot applyPreset()
    //}

    void appendConfigData();

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
    void addToConfig(JsonObject& root);

    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens once immediately after boot)
     * 
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     */
    bool readFromConfig(JsonObject& root);

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId() {
      return USERMOD_ID_FOUR_LINE_DISP;
    }
};

// strings to reduce flash memory usage (used more than twice)
const char FourLineDisplayUsermod::_name[]            PROGMEM = "4LineDisplay";
const char FourLineDisplayUsermod::_enabled[]         PROGMEM = "enabled";
const char FourLineDisplayUsermod::_contrast[]        PROGMEM = "contrast";
const char FourLineDisplayUsermod::_refreshRate[]     PROGMEM = "refreshRate-ms";
const char FourLineDisplayUsermod::_screenTimeOut[]   PROGMEM = "screenTimeOutSec";
const char FourLineDisplayUsermod::_flip[]            PROGMEM = "flip";
const char FourLineDisplayUsermod::_sleepMode[]       PROGMEM = "sleepMode";
const char FourLineDisplayUsermod::_clockMode[]       PROGMEM = "clockMode";
const char FourLineDisplayUsermod::_showSeconds[]     PROGMEM = "showSeconds";
const char FourLineDisplayUsermod::_busClkFrequency[] PROGMEM = "i2c-freq-kHz";
const char FourLineDisplayUsermod::_contrastFix[]     PROGMEM = "contrastFix";

#ifdef ARDUINO_ARCH_ESP32
FourLineDisplayUsermod *FourLineDisplayUsermod::instance = nullptr;
#endif

//WLEDMM
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
    // semaphores - needed on ESP32 only, as we use a separate task to update the display
    SemaphoreHandle_t drawMux = xSemaphoreCreateBinary();      // for drawstring and drawglyph functions (to prevent concurrent access to HW)
    SemaphoreHandle_t drawMuxBig = xSemaphoreCreateBinary();   // for draw2x2GlyphIcons() and showCurrentEffectOrPalette() - more complex and not thread-safe
    const TickType_t maxWait =     300 * portTICK_PERIOD_MS;   // wait max. 300ms (drawstring semaphore)
    const TickType_t maxWaitLong = 800 * portTICK_PERIOD_MS;   // wait max. 800ms (big drawing semaphore)
    #define FLD_SemaphoreTake(x,t)  xSemaphoreTake((x),(t))
    #define FLD_SemaphoreGive(x)    xSemaphoreGive(x)
#else
    // 8266 or no tasks - no semaphores
    #define FLD_SemaphoreTake(x,t) pdTRUE
    #define FLD_SemaphoreGive(x)
    #if !defined(ARDUINO_ARCH_ESP32) && !defined(pdTRUE)
      #define pdTRUE true
    #endif
#endif



// some displays need this to properly apply contrast
void FourLineDisplayUsermod::setVcomh(bool highContrast) {
  if (!typeOK || !enabled) return;    // WLEDMM make sure the display is initialized before we try to draw on it
  if (u8x8 == nullptr) return;

  u8x8_t *u8x8_struct = u8x8->getU8x8();
  u8x8_cad_StartTransfer(u8x8_struct);
  u8x8_cad_SendCmd(u8x8_struct, 0x0db); //address of value
  u8x8_cad_SendArg(u8x8_struct, highContrast ? 0x000 : 0x040); //value 0 for fix, reboot resets default back to 64
  u8x8_cad_EndTransfer(u8x8_struct);
}

/**
 * Wrappers for screen drawing
 */
void FourLineDisplayUsermod::setFlipMode(uint8_t mode) {
  if (!typeOK || !enabled) return;    // WLEDMM make sure the display is initialized before we try to draw on it
  if (u8x8 == nullptr) return;
  u8x8->setFlipMode(mode);
}
void FourLineDisplayUsermod::setContrast(uint8_t contrast) {
  if (!typeOK || !enabled) return;    // WLEDMM make sure the display is initialized before we try to draw on it
  if (u8x8 == nullptr) return;
  u8x8->setContrast(contrast);
}
void FourLineDisplayUsermod::drawString(uint8_t col, uint8_t row, const char *string, bool ignoreLH) {
  if (!typeOK || !enabled) return;    // WLEDMM make sure the display is initialized before we try to draw on it
  if (u8x8 == nullptr) return;
  if (FLD_SemaphoreTake(drawMux, maxWait) != pdTRUE) return;      // WLEDMM acquire draw mutex

#if  defined(ARDUINO_ARCH_ESP32) && !defined(OLD_4LD_FONTS)           // WLEDMM use nicer 2x2 font on ESP32
  if (!ignoreLH && lineHeight>1) { 
    if(strlen(string) > 3)                    // WLEDMM little hack - less than 3 chars -> show in bold
      u8x8->setFont(u8x8_font_7x14_1x2_r);    // normal
    else
      u8x8->setFont(u8x8_font_8x13B_1x2_r);   // bold
    u8x8->drawString(col, row, string);
  }
  else {
    u8x8->setFont(u8x8_font_chroma48medium8_r);
    u8x8->drawString(col, row, string);
  }
#else
  u8x8->setFont(u8x8_font_chroma48medium8_r);
  if (!ignoreLH && lineHeight>1) u8x8->draw1x2String(col, row, string);
  else                           u8x8->drawString(col, row, string);
#endif
  FLD_SemaphoreGive(drawMux);                                     // WLEDMM release draw mutex
}
void FourLineDisplayUsermod::draw2x2String(uint8_t col, uint8_t row, const char *string) {
  if (!typeOK || !enabled) return;
  if (u8x8 == nullptr) return;
  if (FLD_SemaphoreTake(drawMux, maxWait) != pdTRUE) return;      // WLEDMM acquire draw mutex
#if  defined(ARDUINO_ARCH_ESP32) && !defined(OLD_4LD_FONTS) && !defined(WLEDMM_SAVE_FLASH)        // WLEDMM use nicer 2x2 font on ESP32
  if (lineHeight>1) {                            // WLEDMM use 2x3 on 128x64 displays
    //u8x8->setFont(u8x8_font_profont29_2x3_r);   // sans serif 2x3
    u8x8->setFont(u8x8_font_courB18_2x3_r);       // courier bold 2x3
    u8x8->drawString(col, row + (row >3? 1:0), string);
  } else {
    //u8x8->setFont(u8x8_font_lucasarts_scumm_subtitle_o_2x2_r);
    //u8x8->setFont(u8x8_font_lucasarts_scumm_subtitle_r_2x2_r);
    u8x8->setFont(u8x8_font_px437wyse700b_2x2_r);
    u8x8->drawString(col, row, string);
  }
#else
  u8x8->setFont(u8x8_font_chroma48medium8_r);
  if (lineHeight>1) {                            // WLEDMM use 2x3 on 128x64 displays
    u8x8->draw2x2String(col, row + (row >3? 1:0), string);
  } else {
    u8x8->draw2x2String(col, row, string);
  }
#endif
  FLD_SemaphoreGive(drawMux);                                     // WLEDMM release draw mutex
}
void FourLineDisplayUsermod::drawGlyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font, bool ignoreLH) {
  if (!typeOK || !enabled) return;
  if (FLD_SemaphoreTake(drawMux, maxWait) != pdTRUE) return;      // WLEDMM acquire draw mutex
  u8x8->setFont(font);
  if (!ignoreLH && lineHeight>1)  u8x8->draw1x2Glyph(col, row, glyph);
  else                            u8x8->drawGlyph(col, row, glyph);
  FLD_SemaphoreGive(drawMux);                                     // WLEDMM release draw mutex
}
void FourLineDisplayUsermod::draw2x2Glyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font) {
  if (!typeOK || !enabled) return;
  if (FLD_SemaphoreTake(drawMux, maxWait) != pdTRUE) return;      // WLEDMM acquire draw mutex
  u8x8->setFont(font);
  u8x8->draw2x2Glyph(col, row, glyph);
  FLD_SemaphoreGive(drawMux);                                     // WLEDMM release draw mutex
}
uint8_t FourLineDisplayUsermod::getCols() {
  if (!typeOK || !enabled) return 0;
  return u8x8->getCols();
}
void FourLineDisplayUsermod::clear() {
  if (!typeOK || !enabled) return;
  if (nullptr == u8x8) return;  // prevents some crashes
  if (FLD_SemaphoreTake(drawMux, maxWaitLong ) != pdTRUE) return; // WLEDMM acquire draw mutex - clear() can take very long in software I2C mode
  u8x8->clear();         // crashes randomly on ESP32
  FLD_SemaphoreGive(drawMux);                                     // WLEDMM release draw mutex
}
void FourLineDisplayUsermod::setPowerSave(uint8_t save) {
  if (!typeOK || !enabled) return;    // WLEDMM make sure the display is initialized before we try to draw on it
  if (u8x8 == nullptr) return;
  u8x8->setPowerSave(save);
}

void FourLineDisplayUsermod::center(String &line, uint8_t width) {
  int len = line.length();
  if (len<width) for (byte i=(width-len)/2; i>0; i--) line = ' ' + line;
  for (byte i=line.length(); i<width; i++) line += ' ';
}

void FourLineDisplayUsermod::draw2x2GlyphIcons() {
  if (!typeOK || !enabled) return;    // WLEDMM make sure the display is initialized before we try to draw on it
  if (FLD_SemaphoreTake(drawMuxBig, maxWaitLong) != pdTRUE) return;      // WLEDMM acquire BIG draw mutex
  if (lineHeight > 1) {
    drawGlyph( 1,            0, 1, u8x8_4LineDisplay_WLED_icons_2x2, true); //brightness icon
    drawGlyph( 5,            0, 2, u8x8_4LineDisplay_WLED_icons_2x2, true); //speed icon
    drawGlyph( 9,            0, 3, u8x8_4LineDisplay_WLED_icons_2x2, true); //intensity icon
    drawGlyph(14, 2*lineHeight, 4, u8x8_4LineDisplay_WLED_icons_2x2, true); //palette icon
    drawGlyph(14, 3*lineHeight, 5, u8x8_4LineDisplay_WLED_icons_2x2, true); //effect icon
  } else {
    drawGlyph( 1, 0, 1, u8x8_4LineDisplay_WLED_icons_2x1); //brightness icon
    drawGlyph( 5, 0, 2, u8x8_4LineDisplay_WLED_icons_2x1); //speed icon
    drawGlyph( 9, 0, 3, u8x8_4LineDisplay_WLED_icons_2x1); //intensity icon
    drawGlyph(15, 2, 4, u8x8_4LineDisplay_WLED_icons_1x1); //palette icon
    drawGlyph(15, 3, 5, u8x8_4LineDisplay_WLED_icons_1x1); //effect icon
  }
  FLD_SemaphoreGive(drawMuxBig);                                   // WLEDMM release BIG draw mutex
}

/**
 * Display the current date and time in large characters
 * on the middle rows. Based 24 or 12 hour depending on
 * the useAMPM configuration.
 */
void FourLineDisplayUsermod::showTime() {
  if (!typeOK || !enabled || !displayTurnedOff) return;

  unsigned long now = millis();
  while (drawing && millis()-now < 250) delay(1); // wait if someone else is drawing
  drawing = true;

  char lineBuffer[LINE_BUFFER_SIZE] = { '\0' };
  static byte lastSecond;
  byte secondCurrent = second(localTime);
  byte minuteCurrent = minute(localTime);
  byte hourCurrent   = hour(localTime);

  if (knownMinute != minuteCurrent) {  //only redraw clock if it has changed
    //updateLocalTime();
    byte AmPmHour = hourCurrent;
    boolean isitAM = true;
    if (useAMPM) {
      if (AmPmHour > 11) { AmPmHour -= 12; isitAM = false; }
      if (AmPmHour == 0) { AmPmHour  = 12; }
    }
    if (knownHour != hourCurrent) {
      // only update date when hour changes
      snprintf_P(lineBuffer, LINE_BUFFER_SIZE, PSTR("%s %2d "), monthShortStr(month(localTime)), day(localTime)); 
      draw2x2String(2, lineHeight==1 ? 0 : lineHeight, lineBuffer); // adjust for 8 line displays, draw month and day
    }
    snprintf_P(lineBuffer,LINE_BUFFER_SIZE, PSTR("%2d:%02d"), (useAMPM ? AmPmHour : hourCurrent), minuteCurrent);
    draw2x2String(2, lineHeight*2, lineBuffer); //draw hour, min. blink ":" depending on odd/even seconds
    if (useAMPM) drawString(12, lineHeight*2 + (lineHeight-1), (isitAM ? "AM" : "PM"), true); //draw am/pm if using 12 time

    drawStatusIcons(); //icons power, wifi, timer, etc

    if (lineHeight > 1) {       // WLEDMM use extra space for useful information
      #if defined(WLED_DEBUG) || defined(SR_DEBUG)  || defined(SR_STATS)
        snprintf_P(lineBuffer, LINE_BUFFER_SIZE, PSTR(" %-3.3s %-2.2s      "), driverSPI? "SPI" : "I2C", driverHW? "hw" : "sw"); // WLEDMM driver info
      #else
        strncpy_P(lineBuffer, PSTR("             "), LINE_BUFFER_SIZE);
      #endif
      if (apActive) strncpy_P(lineBuffer, PSTR(" AP mode     "), LINE_BUFFER_SIZE);
      else if (!WLED_CONNECTED) strncpy_P(lineBuffer, PSTR(" NO NET      "), LINE_BUFFER_SIZE);
      if (WLED_MQTT_CONNECTED) lineBuffer[9] = 'M'; // "MQTT"
      if (realtimeMode && !realtimeOverride) lineBuffer[10] = 'X'; // "eXternal control"
      //if (transitionActive) lineBuffer[11] = 'T';
      //if (stateChanged) lineBuffer[12] = 'C';
      drawString(1, 0, lineBuffer, false);
    }

    knownMinute = minuteCurrent;
    knownHour   = hourCurrent;
  }
  if (showSeconds && secondCurrent != lastSecond) {
    lastSecond = secondCurrent;
    draw2x2String(6, lineHeight*2, secondCurrent%2 ? " " : ":");
    snprintf_P(lineBuffer, LINE_BUFFER_SIZE, PSTR("%02d"), secondCurrent);
    if (useAMPM)
      drawString(12, lineHeight*2+1 + (lineHeight-1), lineBuffer, true); // even with double sized rows print seconds in 1 line // WLEDMM move it a bit lower
    else
      drawString(12, lineHeight*2+1, lineBuffer, true); // even with double sized rows print seconds in 1 line
  }
  drawing = false;
}

// public:

// gets called once at boot. Do all initialization that doesn't depend on
// network here
void FourLineDisplayUsermod::setup() {
  if (!enabled) return;   // typeOK = true will be set after successful setup

  bool isHW  = false;
  bool isSPI = displayIsSPI(type);
  PinOwner po = PinOwner::UM_FourLineDisplay;
  if (isSPI) {
    if (ioPin[0] < 0 || ioPin[1] < 0) {
      ioPin[0] = spi_sclk;
      ioPin[1] = spi_mosi;
    } else {
      if ((spi_sclk < 0) && (spi_mosi < 0)) { // WLEDMM UM pins are valid, but global = -1 --> copy pins to "global"
        spi_sclk = ioPin[0];
        spi_mosi = ioPin[1];
      }
    }
    if ((ioPin[0] < 0 || ioPin[1] < 0) && (spi_sclk < 0 || spi_mosi < 0))  {      // invalid pins, or "use global" and global pins not defined
      typeOK=false; strcpy(errorMessage, PSTR("SPI No Pins defined")); return; }  //WLEDMM bugfix - ensure that "final" GPIO are valid

    isHW = (ioPin[0]==spi_sclk && ioPin[1]==spi_mosi);
    if ((ioPin[0] == -1) || (ioPin[1] == -1)) isHW = true;  // WLEDMM "use global" = hardware
    if ((spi_sclk <0) || (spi_mosi < 0)) isHW = false;      // no global pins - use software emulation
    PinManagerPinType cspins[3] = { { ioPin[2], true }, { ioPin[3], true }, { ioPin[4], true } };
    if (!pinManager.allocateMultiplePins(cspins, 3, PinOwner::UM_FourLineDisplay)) { typeOK=false; strcpy(errorMessage, PSTR("SPI3 alloc pins failed")); return; }
    if (isHW) po = PinOwner::HW_SPI;  // allow multiple allocations of HW I2C bus pins
    PinManagerPinType pins[2] = { { ioPin[0], true }, { ioPin[1], true } };
    if (!pinManager.allocateMultiplePins(pins, 2, po)) {
      pinManager.deallocateMultiplePins(cspins, 3, PinOwner::UM_FourLineDisplay);
      typeOK=false;
      strcpy(errorMessage, PSTR("SPI2 alloc pins failed"));
      return;
    }
    // start SPI now!
#ifdef ARDUINO_ARCH_ESP32
    if (isHW) SPI.begin(spi_sclk, spi_miso, spi_mosi);   // ESP32 - will silently fail if SPI already active.
#else
    if (isHW) SPI.begin();                               // ESP8266 - SPI pins are fixed
#endif

  } else {
    //if (ioPin[0] < 0 || ioPin[1] < 0) { //WLEDMM do _not_ copy global pins !!
    //  ioPin[0] = i2c_scl;
    //  ioPin[1] = i2c_sda;
    //}
    isHW = (ioPin[0]==i2c_scl && ioPin[1]==i2c_sda);
    if ((ioPin[0] == -1) || (ioPin[1] == -1)) isHW = true;  // WLEDMM "use global" = hardware
    // isHW = true;
    if (isHW) po = PinOwner::HW_I2C;  // allow multiple allocations of HW I2C bus pins
    PinManagerPinType pins[2] = { {ioPin[0], true }, { ioPin[1], true } };

    if ((ioPin[0] < 0 || ioPin[1] < 0) && (i2c_scl < 0 || i2c_sda < 0))  {        // invalid pins, or "use global" and global pins not defined
      typeOK=false; strcpy(errorMessage, PSTR("I2C No Pins defined")); return; }  //WLEDMM bugfix - ensure that "final" GPIO are valid

    if (isHW) {
      if (!pinManager.joinWire(i2c_sda, i2c_scl)) { typeOK=false; strcpy(errorMessage, PSTR("I2C HW init failed")); return; }  // WLEDMM join the HW bus
    } else {
      if (!pinManager.allocateMultiplePins(pins, 2, po)) { typeOK=false; strcpy(errorMessage, PSTR("I2C Alloc pins failed")); return; } // WLEDMM use software bus
    }
  }

  driverHW = isHW;
  driverSPI= isSPI;
  DEBUG_PRINTLN(F("Allocating display."));
/*
// At some point it may be good to not new/delete U8X8 object but use this instead
// (does not currently work)
//-------------------------------------------------------------------------------
  switch (type) {
    case SSD1306:
      u8x8_Setup(u8x8.getU8x8(), u8x8_d_ssd1306_128x32_univision, u8x8_cad_ssd13xx_fast_i2c, u8x8_byte_arduino_sw_i2c, u8x8_gpio_and_delay_arduino);
      break;
    case SH1106:
      u8x8_Setup(u8x8.getU8x8(), u8x8_d_sh1106_128x64_winstar, u8x8_cad_ssd13xx_i2c, u8x8_byte_arduino_sw_i2c, u8x8_gpio_and_delay_arduino);
      break;
    case SSD1306_64:
      u8x8_Setup(u8x8.getU8x8(), u8x8_d_ssd1306_128x64_noname, u8x8_cad_ssd13xx_fast_i2c, u8x8_byte_arduino_sw_i2c, u8x8_gpio_and_delay_arduino);
      break;
    case SSD1305:
      u8x8_Setup(u8x8.getU8x8(), u8x8_d_ssd1305_128x32_adafruit, u8x8_cad_ssd13xx_i2c, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
      break;
    case SSD1305_64:
      u8x8_Setup(u8x8.getU8x8(), u8x8_d_ssd1305_128x64_adafruit, u8x8_cad_ssd13xx_i2c, u8x8_byte_arduino_sw_i2c, u8x8_gpio_and_delay_arduino);
      break;
    case SSD1306_SPI:
      u8x8_Setup(u8x8.getU8x8(), u8x8_d_ssd1306_128x32_univision, u8x8_cad_001, u8x8_byte_arduino_4wire_sw_spi, u8x8_gpio_and_delay_arduino);
      break;
    case SSD1306_SPI64:
      u8x8_Setup(u8x8.getU8x8(), u8x8_d_ssd1306_128x64_noname, u8x8_cad_001, u8x8_byte_arduino_4wire_sw_spi, u8x8_gpio_and_delay_arduino);
      break;
    default:
      typeOK=false;
      strcpy(errorMessage, PSTR("No valid type"));
      return;
  }
  if (isSPI) {
    if (!isHW) u8x8_SetPin_4Wire_SW_SPI(u8x8.getU8x8(), ioPin[0], ioPin[1], ioPin[2], ioPin[3], ioPin[4]);
    else       u8x8_SetPin_4Wire_HW_SPI(u8x8.getU8x8(), ioPin[2], ioPin[3], ioPin[4]); // Pins are cs, dc, reset
  } else {
    if (!isHW) u8x8_SetPin_SW_I2C(u8x8.getU8x8(), ioPin[0], ioPin[1], U8X8_PIN_NONE); // SCL, SDA, reset
    else       u8x8_SetPin_HW_I2C(u8x8.getU8x8(), U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
  }
*/
  switch (type) {
    case SSD1306:
      if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1306_128X32_UNIVISION_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
      else       u8x8 = (U8X8 *) new U8X8_SSD1306_128X32_UNIVISION_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
      break;
    case SH1106:
      if (!isHW) u8x8 = (U8X8 *) new U8X8_SH1106_128X64_WINSTAR_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
      else       u8x8 = (U8X8 *) new U8X8_SH1106_128X64_WINSTAR_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
      break;
    case SSD1306_64:
      if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1306_128X64_NONAME_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
      else       u8x8 = (U8X8 *) new U8X8_SSD1306_128X64_NONAME_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
      break;
    case  SSD1309_64:
      if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1309_128X64_NONAME2_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
      else       u8x8 = (U8X8 *) new U8X8_SSD1309_128X64_NONAME2_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
      break;
    case SSD1305:
      if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1305_128X32_NONAME_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
      else       u8x8 = (U8X8 *) new U8X8_SSD1305_128X32_ADAFRUIT_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
      break;
    case SSD1305_64:
      if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1305_128X64_ADAFRUIT_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
      else       u8x8 = (U8X8 *) new U8X8_SSD1305_128X64_ADAFRUIT_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
      break;
    case SSD1306_SPI:
      // u8x8 uses global SPI variable that is attached to VSPI bus on ESP32
      if (!isHW)  u8x8 = (U8X8 *) new U8X8_SSD1306_128X32_UNIVISION_4W_SW_SPI(ioPin[0], ioPin[1], ioPin[2], ioPin[3], ioPin[4]);
      else        u8x8 = (U8X8 *) new U8X8_SSD1306_128X32_UNIVISION_4W_HW_SPI(ioPin[2], ioPin[3], ioPin[4]); // Pins are cs, dc, reset
      break;
    case SSD1306_SPI64:
      // u8x8 uses global SPI variable that is attached to VSPI bus on ESP32
      if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1306_128X64_NONAME_4W_SW_SPI(ioPin[0], ioPin[1], ioPin[2], ioPin[3], ioPin[4]);
      else       u8x8 = (U8X8 *) new U8X8_SSD1306_128X64_NONAME_4W_HW_SPI(ioPin[2], ioPin[3], ioPin[4]); // Pins are cs, dc, reset
      break;
    case SSD1309_SPI64:
      // u8x8 uses global SPI variable that is attached to VSPI bus on ESP32
      if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1309_128X64_NONAME0_4W_SW_SPI(ioPin[0], ioPin[1], ioPin[2], ioPin[3], ioPin[4]);
      else       u8x8 = (U8X8 *) new U8X8_SSD1309_128X64_NONAME0_4W_HW_SPI(ioPin[2], ioPin[3], ioPin[4]); // Pins are cs, dc, reset
      break;
    case SSD1327_SPI128:
      // u8x8 uses global SPI variable that is attached to VSPI bus on ESP32
      if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1327_WS_128X128_4W_SW_SPI(ioPin[0], ioPin[1], ioPin[2], ioPin[3], ioPin[4]);
      else       u8x8 = (U8X8 *) new U8X8_SSD1327_WS_128X128_4W_HW_SPI(ioPin[2], ioPin[3], ioPin[4]); // Pins are cs, dc, reset
      break;
    default:
      u8x8 = nullptr;
  }

  if (nullptr == u8x8) {
      USER_PRINTLN(F("Display init failed."));
      if (!isHW || !isSPI) pinManager.deallocateMultiplePins((const uint8_t*)ioPin, isSPI ? 5 : 2, po);   // WLEDMM do not de-alloc global pins
      typeOK=false;
      strcpy(errorMessage, PSTR("Display init failed"));
      return;
  }

  lineHeight = u8x8->getRows() > 4 ? 2 : 1;
  if (u8x8->getRows() > 8) lineHeight =3;
  //if (u8x8->getRows() > 12) lineHeight =4;
  if (isSPI) {
    USER_PRINTLN(isHW ? F("Starting display (SPI HW).") : F("Starting display (SPI Soft)."));
  } else {
    USER_PRINTLN(isHW ? F("Starting display (I2C HW).") : F("Starting display (I2C Soft)."));
  }
  u8x8->setBusClock(ioFrequency);  // can be used for SPI too
  u8x8->begin();
  typeOK = true;

  reDrawing = false;
  drawing = true;
  setFlipMode(flip);
  setVcomh(contrastFix);
  setContrast(contrast); //Contrast setup will help to preserve OLED lifetime. In case OLED need to be brighter increase number up to 255
  setPowerSave(0);
  drawing = false;

  // init semaphores to allow drawing
  FLD_SemaphoreGive(drawMux);
  FLD_SemaphoreGive(drawMuxBig);

  onUpdateBegin(false);  // create Display task // WLEDMM bugfix: before drawing anything
  delay(200);

  //drawString(0, 0, "Loading...");
  overlayLogo(3500);
  initDone = true;
}

// gets called every time WiFi is (re-)connected. Initialize own network
// interfaces here
void FourLineDisplayUsermod::connected() {
  knownSsid = WiFi.SSID();       //apActive ? apSSID : WiFi.SSID(); //apActive ? WiFi.softAPSSID() : 
  knownIp   = Network.localIP(); //apActive ? IPAddress(4, 3, 2, 1) : Network.localIP();
  networkOverlay(PSTR("NETWORK INFO"),7000);
}

/**
 * Da loop.
 */
void FourLineDisplayUsermod::loop() {
#if !defined(ARDUINO_ARCH_ESP32) || !defined(FLD_ESP32_USE_THREADS)
  static unsigned long lastRunTime = 0;
  unsigned long now = millis();
  if (!enabled || !typeOK || (strip.isUpdating() && (now - lastRunTime < 50))) return;
  lastRunTime = now;

  if (now < nextUpdate) return;
  nextUpdate = now + ((displayTurnedOff && clockMode && showSeconds) ? 1000 : refreshRate);
  reDrawing = true;
  redraw(false);
  reDrawing = false;
#endif
}

//function to to check if a redraw or overlay draw is active. Needed for UM Rotary, to avoid random/concurrent drawing
bool FourLineDisplayUsermod::canDraw(void) {
  if (!typeOK || !enabled || !initDone) return(false);    // WLEDMM make sure the display is initialized before we try to draw on it
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS) // only necessary on ESP32
  if (drawing) return(false);          // overlay draws someting
  if (reDrawing) return(false);        // redraw task draws something
#endif
  return(true);
}

/**
 * Redraw the screen (but only if things have changed
 * or if forceRedraw).
 */
void FourLineDisplayUsermod::redraw(bool forceRedraw) {
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
    // use a wrapper onESP32, to ensure the functions is not running several times in parallel !
    static bool doForceRedraw = false;  // for delaying "force redraw"

    if ((overlayUntil > 0) && (millis() >= overlayUntil)) {
      forceRedraw = true; // Time to display the overlay has elapsed, force redraw needed
    }

    if (forceRedraw) doForceRedraw = true;
    if (reDrawing) return; // redraw already active
    if (drawing) return;   // overlay draw active

    reDrawing = true;    // set redraw lock
    if (doForceRedraw) forceRedraw = true;
    redraw_core(forceRedraw);
    if (overlayUntil == 0) doForceRedraw = false;   // redraw was skipped if overlay is still visible
    reDrawing = false;     // reset activity flag, as redraw has too many early returns that don't take care of this
}

void FourLineDisplayUsermod::redraw_core(bool forceRedraw) {
#endif
  bool needRedraw = false;
  unsigned long now = millis();

  if (!typeOK || !enabled) return;    // WLEDMM make sure the display is initialized before we try to draw on it
  if (nullptr == u8x8) return;        // prevent crash in case u8x8 is re-initialized (du to user changing setings)

  if (overlayUntil > 0) {
    if (now >= overlayUntil) {
      // Time to display the overlay has elapsed.
      overlayUntil = 0;
      forceRedraw = true;
    } else {
      // We are still displaying the overlay
      // Don't redraw.
      return;
    }
  }

  while (drawing && millis()-now < 250) delay(1); // wait if someone else is drawing

  if (apActive && WLED_WIFI_CONFIGURED && now<15000) {
    knownSsid = apSSID;
    networkOverlay(PSTR("NETWORK INFO"),30000);
    return;
  }

  // Check if values which are shown on display changed from the last time.
  if (forceRedraw) {
    needRedraw = true;
    clear();
  } else if ((bri == 0 && powerON) || (bri > 0 && !powerON)) {   //trigger power icon
    powerON = !powerON;
    drawStatusIcons();
    return;
  } else if (knownnightlight != nightlightActive) {   //trigger moon icon 
    knownnightlight = nightlightActive;
    drawStatusIcons();
    if (knownnightlight) {
      String timer = PSTR("Timer On");
      center(timer,LINE_BUFFER_SIZE-1);
      overlay(timer.c_str(), 2500, 6);
    }
    return;
  } else if (wificonnected != interfacesInited) {   //trigger wifi icon
    wificonnected = interfacesInited;
    drawStatusIcons();
    return;
  } else if (knownMode != effectCurrent || knownPalette != effectPalette) {
    if (displayTurnedOff) needRedraw = true;
    else { 
      if (knownPalette != effectPalette) { showCurrentEffectOrPalette(effectPalette, JSON_palette_names, 2); knownPalette = effectPalette; }
      if (knownMode    != effectCurrent) { showCurrentEffectOrPalette(effectCurrent, JSON_mode_names, 3); knownMode = effectCurrent; }
      lastRedraw = now;
      return;
    }
  } else if (knownBrightness != bri) {
    if (displayTurnedOff && nightlightActive) { knownBrightness = bri; }
    else if (!displayTurnedOff) { updateBrightness(); lastRedraw = now; return; }
  } else if (knownEffectSpeed != effectSpeed) {
    if (displayTurnedOff) needRedraw = true;
    else { updateSpeed(); lastRedraw = now; return; }
  } else if (knownEffectIntensity != effectIntensity) {
    if (displayTurnedOff) needRedraw = true;
    else { updateIntensity(); lastRedraw = now; return; }
  }

  if (!needRedraw) {
    // Nothing to change.
    // Turn off display after 1 minutes with no change.
    if (sleepMode && !displayTurnedOff && (millis() - lastRedraw > screenTimeout)) {
      // We will still check if there is a change in redraw()
      // and turn it back on if it changed.
      clear();
      sleepOrClock(true);
    } else if (displayTurnedOff) {  // WLEDMM removed "&& ntpEnabled"
      showTime();
    }
    return;
  }

  lastRedraw = now;
  
  // Turn the display back on
  wakeDisplay();

  // Update last known values.
  knownBrightness      = bri;
  knownMode            = effectCurrent;
  knownPalette         = effectPalette;
  knownEffectSpeed     = effectSpeed;
  knownEffectIntensity = effectIntensity;
  knownnightlight      = nightlightActive;
  wificonnected        = interfacesInited;

  while (drawing && millis()-now < 150) delay(8); // wait if someone else is drawing

  // Do the actual drawing
  // First row: Icons
  draw2x2GlyphIcons();
  drawArrow();
  drawStatusIcons();

  // Second row 
  updateBrightness();
  updateSpeed();
  updateIntensity();

  // Third row
  showCurrentEffectOrPalette(knownPalette, JSON_palette_names, 2); //Palette info

  // Fourth row
  showCurrentEffectOrPalette(knownMode, JSON_mode_names, 3); //Effect Mode info
}

void FourLineDisplayUsermod::updateBrightness() {
  knownBrightness = bri;
  if (overlayUntil == 0) {
    brightness100 = ((uint16_t)bri*100)/255;
    char lineBuffer[4];
    snprintf_P(lineBuffer, 4, PSTR("%-3d"), brightness100);
    drawString(1, lineHeight, lineBuffer);
    //lastRedraw = millis();
  }
}

void FourLineDisplayUsermod::updateSpeed() {
  knownEffectSpeed = effectSpeed;
  if (overlayUntil == 0) {
    fxspeed100 = ((uint16_t)effectSpeed*100)/255;
    char lineBuffer[4];
    snprintf_P(lineBuffer, 4, PSTR("%-3d"), fxspeed100);
    drawString(5, lineHeight, lineBuffer);
    //lastRedraw = millis();
  }
}

void FourLineDisplayUsermod::updateIntensity() {
  knownEffectIntensity = effectIntensity;
  if (overlayUntil == 0) {
    fxintensity100 = ((uint16_t)effectIntensity*100)/255;
    char lineBuffer[4];
    snprintf_P(lineBuffer, 4, PSTR("%-3d"), fxintensity100);
    drawString(9, lineHeight, lineBuffer);
    //lastRedraw = millis();
  }
}

void FourLineDisplayUsermod::drawStatusIcons() {
  uint8_t col = 15;
  uint8_t row = 0;
  drawGlyph(col, row,   (wificonnected ? 20 : 0), u8x8_4LineDisplay_WLED_icons_1x1, true); // wifi icon
  if (lineHeight>1) { col--; } else { row++; }
  drawGlyph(col, row,          (bri > 0 ? 9 : 0), u8x8_4LineDisplay_WLED_icons_1x1, true); // power icon
  if (lineHeight>1) { col--; } else { col = row = 0; }
  drawGlyph(col, row, (nightlightActive ? 6 : 0), u8x8_4LineDisplay_WLED_icons_1x1, true); // moon icon for nightlight mode
}

/**
 * marks the position of the arrow showing
 * the current setting being changed
 * pass line and colum info
 */
void FourLineDisplayUsermod::setMarkLine(byte newMarkLineNum, byte newMarkColNum) {
  markLineNum = newMarkLineNum;
  markColNum = newMarkColNum;
}

//Draw the arrow for the current setting being changed
void FourLineDisplayUsermod::drawArrow() {
  if (markColNum != 255 && markLineNum !=255) drawGlyph(markColNum, markLineNum*lineHeight, 21, u8x8_4LineDisplay_WLED_icons_1x1);
}

//Display the current effect or palette (desiredEntry) 
// on the appropriate line (row). 
void FourLineDisplayUsermod::showCurrentEffectOrPalette(int inputEffPal, const char *qstring, uint8_t row) {
  char lineBuffer[MAX_JSON_CHARS] = { '\0' };
  if (!typeOK || !enabled) return;    // WLEDMM make sure the display is initialized before we try to draw on it
  if (FLD_SemaphoreTake(drawMuxBig, maxWaitLong) != pdTRUE) return;      // WLEDMM acquire BIG draw mutex

  if (overlayUntil == 0) {
    // Find the mode name in JSON
    uint8_t printedChars = extractModeName(inputEffPal, qstring, lineBuffer, MAX_JSON_CHARS-1);
    if (printedChars < 2) strcpy(lineBuffer, "invalid");  // catch possible error
    if (lineBuffer[0]=='*' && lineBuffer[1]==' ') {
      // remove "* " from dynamic palettes
      for (byte i=2; i<=printedChars; i++) lineBuffer[i-2] = lineBuffer[i]; //include '\0'
      printedChars -= 2;
    } else if ((lineBuffer[0]==' ' && lineBuffer[1]>127)) {
      // remove note symbol from effect names
      for (byte i=5; i<=printedChars; i++) lineBuffer[i-5] = lineBuffer[i]; //include '\0'
      printedChars -= 5;
    }
    if (lineHeight > 1) {                                 // use this code for 8 line display
      char smallBuffer1[MAX_MODE_LINE_SPACE+1] = { '\0' };
      char smallBuffer2[MAX_MODE_LINE_SPACE+1] = { '\0' };
      uint8_t smallChars1 = 0;
      uint8_t smallChars2 = 0;
      if (printedChars < MAX_MODE_LINE_SPACE) {            // use big font if the text fits
        while (printedChars < (MAX_MODE_LINE_SPACE-1)) lineBuffer[printedChars++]=' ';
        lineBuffer[printedChars] = 0;
        drawString(1, row*lineHeight, lineBuffer);
      } else {                                             // for long names divide the text into 2 lines and print them small
        bool spaceHit = false;
        for (uint8_t i = 0; i < printedChars; i++) {
          switch (lineBuffer[i]) {
            case ' ':
              if (i > 4 && !spaceHit) {
                spaceHit = true;
                break;
              }
              if (spaceHit) smallBuffer2[smallChars2++] = lineBuffer[i];
              else          smallBuffer1[smallChars1++] = lineBuffer[i];
              break;
            default:
              if (spaceHit) smallBuffer2[smallChars2++] = lineBuffer[i];
              else          smallBuffer1[smallChars1++] = lineBuffer[i];
              break;
          }
        }
        while (smallChars1 < (MAX_MODE_LINE_SPACE-1)) smallBuffer1[smallChars1++]=' ';
        smallBuffer1[smallChars1] = 0;
        smallBuffer1[MAX_MODE_LINE_SPACE -1] = '\0';   // ensure the string ends where it should  (while loop above can overshoot by 1)
        drawString(1, row*lineHeight, smallBuffer1, true);
        while (smallChars2 < (MAX_MODE_LINE_SPACE-1)) smallBuffer2[smallChars2++]=' '; 
        smallBuffer2[smallChars2] = 0;
        smallBuffer2[MAX_MODE_LINE_SPACE -1] = '\0';   // ensure the string ends where it should  (while loop above can overshoot by 1)
        drawString(1, row*lineHeight+1, smallBuffer2, true);
      }
    } else {                                             // use this code for 4 ling displays
      char smallBuffer3[MAX_MODE_LINE_SPACE+1] = {'\0'}; // uses 1x1 icon for mode/palette
      uint8_t smallChars3 = 0;
      for (uint8_t i = 0; i < MAX_MODE_LINE_SPACE; i++) smallBuffer3[smallChars3++] = (i >= printedChars) ? ' ' : lineBuffer[i];
      smallBuffer3[smallChars3] = 0;
      drawString(1, row*lineHeight, smallBuffer3, true);
    }
  }

  FLD_SemaphoreGive(drawMuxBig);                                   // WLEDMM release BIG draw mutex
}

/**
 * If there screen is off or in clock is displayed,
 * this will return true. This allows us to throw away
 * the first input from the rotary encoder but
 * to wake up the screen.
 */
bool FourLineDisplayUsermod::wakeDisplay() {
  if (!typeOK || !enabled) return false;
  if (!initDone)  return false;
  if (displayTurnedOff) {
    unsigned long now = millis();
    while (drawing && millis()-now < 250) delay(1); // wait if someone else is drawing
    drawing = true;
    clear();
    // Turn the display back on
    sleepOrClock(false);
    //lastRedraw = millis();
    drawing = false;
    return true;
  }
  return false;
}

/**
 * Allows you to show one line and a glyph as overlay for a period of time.
 * Clears the screen and prints.
 * Used in Rotary Encoder usermod.
 */
void FourLineDisplayUsermod::overlay(const char* line1, long showHowLong, byte glyphType) {
  if (!typeOK || !enabled) return;   // WLEDMM make sure the display is initialized before we try to draw on it
  if (!initDone) return;             // WLEDMM bugfix
  unsigned long now = millis();
  while (drawing && millis()-now < 250) delay(1); // wait if someone else is drawing
  while ((reDrawing && overlayUntil == 0) && (millis()-now < 250)) delay(10); // wait if someone else is re-drawing
  drawing = true;
  // Turn the display back on
  if (!wakeDisplay()) clear();
  // Print the overlay
  if (glyphType>0 && glyphType<255) {
    if (lineHeight > 1)  drawGlyph(5, 0, glyphType, u8x8_4LineDisplay_WLED_icons_6x6, true); // use 3x3 font with draw2x2Glyph() if flash runs short and comment out 6x6 font
    else                 drawGlyph(6, 0, glyphType, u8x8_4LineDisplay_WLED_icons_3x3, true);
  }
  if (line1) {
    String buf = line1;
    center(buf, getCols());
    drawString(0, (glyphType<255?3:0)*lineHeight, buf.c_str());
  }
  overlayUntil = millis() + showHowLong;
  drawing = false;
}

/**
 * Allows you to show Akemi WLED logo overlay for a period of time.
 * Clears the screen and prints.
 */
void FourLineDisplayUsermod::overlayLogo(long showHowLong) {
  if (!typeOK || !enabled) return;   // WLEDMM make sure the display is initialized before we try to draw on it
  unsigned long now = millis();
  while (drawing && millis()-now < 250) delay(1); // wait if someone else is drawing
  drawing = true;
  // Turn the display back on
  if (!wakeDisplay()) clear();
  // Print the overlay
  if (lineHeight > 1) {
    //add a bit of randomness
    switch (millis()%3) {
      case 0:
        //WLED
        draw2x2Glyph( 0, 2, 1, u8x8_wled_logo_2x2);
        draw2x2Glyph( 4, 2, 2, u8x8_wled_logo_2x2);
        draw2x2Glyph( 8, 2, 3, u8x8_wled_logo_2x2);
        draw2x2Glyph(12, 2, 4, u8x8_wled_logo_2x2);
        break;
      case 1:
        //WLED Akemi
        drawGlyph( 2, 2, 1, u8x8_wled_logo_akemi_4x4, true);
        drawGlyph( 6, 2, 2, u8x8_wled_logo_akemi_4x4, true);
        drawGlyph(10, 2, 3, u8x8_wled_logo_akemi_4x4, true);
        break;
      case 2:
        //Akemi
        //draw2x2Glyph( 5, 0, 12, u8x8_4LineDisplay_WLED_icons_3x3); // use this if flash runs short and comment out 6x6 font
        drawGlyph( 5, 0, 12, u8x8_4LineDisplay_WLED_icons_6x6, true);
        drawString(6, 6, "WLED");
        break;
    }
  } else {
    switch (millis()%3) {
      case 0:
        //WLED
        draw2x2Glyph( 0, 0, 1, u8x8_wled_logo_2x2);
        draw2x2Glyph( 4, 0, 2, u8x8_wled_logo_2x2);
        draw2x2Glyph( 8, 0, 3, u8x8_wled_logo_2x2);
        draw2x2Glyph(12, 0, 4, u8x8_wled_logo_2x2);
        break;
      case 1:
        //WLED Akemi
        drawGlyph( 2, 0, 1, u8x8_wled_logo_akemi_4x4);
        drawGlyph( 6, 0, 2, u8x8_wled_logo_akemi_4x4);
        drawGlyph(10, 0, 3, u8x8_wled_logo_akemi_4x4);
        break;
      case 2:
        //Akemi
        //drawGlyph( 6, 0, 12, u8x8_4LineDisplay_WLED_icons_4x4); // a bit nicer, but uses extra 1.5k flash
        draw2x2Glyph( 6, 0, 12, u8x8_4LineDisplay_WLED_icons_2x2);
        break;
    }
  }
  overlayUntil = millis() + showHowLong;
  drawing = false;
}

/**
 * Allows you to show two lines as overlay for a period of time.
 * Clears the screen and prints.
 * Used in Auto Save usermod
 */
void FourLineDisplayUsermod::overlay(const char* line1, const char* line2, long showHowLong) {
  if (!typeOK || !enabled) return;   // WLEDMM make sure the display is initialized before we try to draw on it
  if (!initDone) return;             // WLEDMM bugfix
  unsigned long now = millis();
  while (drawing && millis()-now < 250) delay(1); // wait if someone else is drawing
  while ((reDrawing && overlayUntil == 0) && (millis()-now < 250)) delay(10); // wait if someone else is re-drawing
  drawing = true;
  // Turn the display back on
  if (!wakeDisplay()) clear();
  // Print the overlay
  if (line1) {
    String buf = line1;
    center(buf, getCols());
    drawString(0, 1*lineHeight, buf.c_str());
  }
  if (line2) {
    String buf = line2;
    center(buf, getCols());
    drawString(0, 2*lineHeight, buf.c_str());
  }
  overlayUntil = millis() + showHowLong;
  drawing = false;
}

void FourLineDisplayUsermod::networkOverlay(const char* line1, long showHowLong) {
  if (!typeOK || !enabled) return;   // WLEDMM make sure the display is initialized before we try to draw on it
  if (!initDone) return;             // WLEDMM bugfix
  unsigned long now = millis();
  while (drawing && millis()-now < 250) delay(1); // wait if someone else is drawing
  drawing = true;

  String line;
  // Turn the display back on
  if (!wakeDisplay()) clear();
  // Print the overlay
  if (line1) {
    line = line1;
    center(line, getCols());
    drawString(0, 0, line.c_str());
  }
  // Second row with Wifi name
  line = knownSsid.substring(0, getCols() > 1 ? getCols() - 2 : 0);
  if (line.length() < getCols()) center(line, getCols());
  drawString(0, lineHeight, line.c_str());
  // Print `~` char to indicate that SSID is longer, than our display
  if (knownSsid.length() > getCols()) {
    drawString(getCols() - 1, 0, "~");
  }
  // Third row with IP and Password in AP Mode
  line = knownIp.toString();
  center(line, getCols());
  drawString(0, lineHeight*2, line.c_str());
  line = "";
  if (apActive) {
    line = apPass;
  } else if (strcmp(serverDescription, "WLED") != 0) {
    line = serverDescription;
  }
  center(line, getCols());
  drawString(0, lineHeight*3, line.c_str());
  overlayUntil = millis() + showHowLong;
  drawing = false;
}


/**
 * Enable sleep (turn the display off) or clock mode.
 */
void FourLineDisplayUsermod::sleepOrClock(bool sleepEnable) {
  if (sleepEnable) {
    displayTurnedOff = true;
    //setContrast(contrastFix? 2+ contrast/4 : 0);    // un-comment to dim display in "clock mode"
    if (clockMode) {                                  // WLEDMM removed " && ntpEnabled"
      knownMinute = knownHour = 99;
      showTime();
    } else
      setPowerSave(1);
  } else {
    displayTurnedOff = false;
    setPowerSave(0);
    //setContrast(contrast);                        // un-comment to restore display brightness on wakeup
  }
}

/**
 * handleButton() can be used to override default button behaviour. Returning true
 * will prevent button working in a default way.
 * Replicating button.cpp
 */
bool FourLineDisplayUsermod::handleButton(uint8_t b) {
  yield();
  if (!enabled
    || b // button 0 only
    || buttonType[b] == BTN_TYPE_SWITCH
    || buttonType[b] == BTN_TYPE_NONE
    || buttonType[b] == BTN_TYPE_RESERVED
    || buttonType[b] == BTN_TYPE_PIR_SENSOR
    || buttonType[b] == BTN_TYPE_ANALOG
    || buttonType[b] == BTN_TYPE_ANALOG_INVERTED) {
    return false;
  }

  unsigned long now = millis();
  static bool buttonPressedBefore = false;
  static bool buttonLongPressed = false;
  static unsigned long buttonPressedTime = 0;
  static unsigned long buttonWaitTime = 0;
  bool handled = false;

  //momentary button logic
  if (isButtonPressed(b)) { //pressed

    if (!buttonPressedBefore) buttonPressedTime = now;
    buttonPressedBefore = true;

    if (now - buttonPressedTime > 600) { //long press
      //TODO: handleButton() handles button 0 without preset in a different way for double click
      //so we need to override with same behaviour
      //DEBUG_PRINTLN(F("4LD action."));
      //if (!buttonLongPressed) longPressAction(0);
      buttonLongPressed = true;
      return false;
    }

  } else if (!isButtonPressed(b) && buttonPressedBefore) { //released

    long dur = now - buttonPressedTime;
    if (dur < 50) {
      buttonPressedBefore = false;
      return true;
    } //too short "press", debounce

    bool doublePress = buttonWaitTime; //did we have short press before?
    buttonWaitTime = 0;

    if (!buttonLongPressed) { //short press
      // if this is second release within 350ms it is a double press (buttonWaitTime!=0)
      //TODO: handleButton() handles button 0 without preset in a different way for double click
      if (doublePress) {
        networkOverlay(PSTR("NETWORK INFO"),7000);
        handled = true;
      } else  {
        buttonWaitTime = now;
      }
    }
    buttonPressedBefore = false;
    buttonLongPressed = false;
  }
  // if 350ms elapsed since last press/release it is a short press
  if (buttonWaitTime && now - buttonWaitTime > 350 && !buttonPressedBefore) {
    buttonWaitTime = 0;
    //TODO: handleButton() handles button 0 without preset in a different way for double click
    //so we need to override with same behaviour
    //shortPressAction(0);
    //handled = false;
  }
  return handled;
}

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#ifndef ARDUINO_RUNNING_CORE
#define ARDUINO_RUNNING_CORE 1
#endif
#endif
void FourLineDisplayUsermod::onUpdateBegin(bool init) {
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  if (init && Display_Task) {
    vTaskSuspend(Display_Task);   // update is about to begin, disable task to prevent crash
  } else {
    // update has failed or create task requested
    if (Display_Task)
      vTaskResume(Display_Task);
    else
      xTaskCreateUniversal(               // this is guaranteed to work on any ESP32 (single or dual core)
        [](void * par) {                  // Function to implement the task
          // see https://www.freertos.org/vtaskdelayuntil.html
          const TickType_t xFrequency = REFRESH_RATE_MS * portTICK_PERIOD_MS / 2;
          TickType_t xLastWakeTime = xTaskGetTickCount();
          for(;;) {
            delay(1); // DO NOT DELETE THIS LINE! It is needed to give the IDLE(0) task enough time and to keep the watchdog happy.
                      // taskYIELD(), yield(), vTaskDelay() and esp_task_wdt_feed() didn't seem to work.
            xLastWakeTime = xTaskGetTickCount();         // workaround for vTaskDelayUntil bug: it does not always keep the last time so we refresh it explicitly
            FourLineDisplayUsermod::getInstance()->redraw(false);
            vTaskDelayUntil(&xLastWakeTime, xFrequency); // release CPU, by doing nothing until next REFRESH_RATE_MS millis
          }
        },
        "4LD",                // Name of the task
//            3072,                 // Stack size in words
        4096,                 // bigger Stack size in words
        NULL,                 // Task input parameter
        1,                    // Priority of the task (not idle)
        &Display_Task,        // Task handle
        ARDUINO_RUNNING_CORE
      );
  }
#endif
}

/*
  * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
  * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
  * Below it is shown how this could be used for e.g. a light sensor
  */
//void addToJsonInfo(JsonObject& root) {
  //JsonObject user = root["u"];
  //if (user.isNull()) user = root.createNestedObject("u");
  //JsonArray data = user.createNestedArray(F("4LineDisplay"));
  //data.add(F("Loaded."));
//}

/*
  * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
  * Values in the state object may be modified by connected clients
  */
//void addToJsonState(JsonObject& root) {
//}

/*
  * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
  * Values in the state object may be modified by connected clients
  */
//void readFromJsonState(JsonObject& root) {
//  if (!initDone) return;  // prevent crash on boot applyPreset()
//}

void FourLineDisplayUsermod::appendConfigData() {
  oappend(SET_F("addHB('4LineDisplay');"));
  
  oappend(SET_F("dd=addDropdown('4LineDisplay','type');"));
  oappend(SET_F("addOption(dd,'None',0);"));
  oappend(SET_F("addOption(dd,'SSD1306',1);"));
  oappend(SET_F("addOption(dd,'SH1106',2);"));
  oappend(SET_F("addOption(dd,'SSD1306 128x64',3);"));
  oappend(SET_F("addOption(dd,'SSD1309 128x64',10);"));
  oappend(SET_F("addOption(dd,'SSD1305',4);"));
  oappend(SET_F("addOption(dd,'SSD1305 128x64',5);"));
  oappend(SET_F("addOption(dd,'SSD1306 SPI',6);"));
  oappend(SET_F("addOption(dd,'SSD1306 SPI 128x64',7);"));
  oappend(SET_F("addOption(dd,'SSD1309 SPI 128x64',8);"));
  oappend(SET_F("addOption(dd,'SSD1327 SPI 128x128',9);"));
  bool isSPI = displayIsSPI(type);
  // WLEDMM add defaults
  oappend(SET_F("addInfo('4LineDisplay:pin[]',0,'','I2C/SPI CLK');"));
  oappend(SET_F("dRO('4LineDisplay:pin[]',0);")); // disable read only pins
  if (isSPI) {
    oappend(SET_F("rOpt('4LineDisplay:pin[]',0,'use global (")); oappendi(spi_sclk); oappend(")',-1);"); 
    #ifdef FLD_PIN_CLOCKSPI
      oappend(SET_F("xOpt('4LineDisplay:pin[]',0,' ⎌',")); oappendi(FLD_PIN_CLOCKSPI); oappend(");"); 
    #endif
  } else {
    oappend(SET_F("rOpt('4LineDisplay:pin[]',0,'use global (")); oappendi(i2c_scl); oappend(")',-1);"); 
    #ifdef FLD_PIN_SCL
      oappend(SET_F("xOpt('4LineDisplay:pin[]',0,' ⎌',")); oappendi(FLD_PIN_SCL); oappend(");"); 
    #endif
  }
  oappend(SET_F("addInfo('4LineDisplay:pin[]',1,'','I2C/SPI DTA');"));
  if (isSPI) {
    oappend(SET_F("rOpt('4LineDisplay:pin[]',1,'use global (")); oappendi(spi_mosi); oappend(")',-1);"); 
    #ifdef FLD_PIN_MOSISPI
      oappend(SET_F("xOpt('4LineDisplay:pin[]',1,' ⎌',")); oappendi(FLD_PIN_MOSISPI); oappend(");"); 
    #endif
  } else {
    oappend(SET_F("rOpt('4LineDisplay:pin[]',1,'use global (")); oappendi(i2c_sda); oappend(")',-1);"); 
    #ifdef FLD_PIN_SDA
      oappend(SET_F("xOpt('4LineDisplay:pin[]',1,' ⎌',")); oappendi(FLD_PIN_SDA); oappend(");"); 
    #endif
  }
  oappend(SET_F("addInfo('4LineDisplay:pin[]',2,'','SPI CS');"));
  #ifdef FLD_PIN_CS
    oappend(SET_F("xOpt('4LineDisplay:pin[]',2,' ⎌',")); oappendi(FLD_PIN_CS); oappend(");"); 
  #endif
  oappend(SET_F("addInfo('4LineDisplay:pin[]',3,'','SPI DC');"));
  #ifdef FLD_PIN_DC
    oappend(SET_F("xOpt('4LineDisplay:pin[]',3,' ⎌',")); oappendi(FLD_PIN_DC); oappend(");"); 
  #endif
  oappend(SET_F("addInfo('4LineDisplay:pin[]',4,'','SPI RST');"));
  #ifdef FLD_PIN_RESET
    oappend(SET_F("xOpt('4LineDisplay:pin[]',4,' ⎌',")); oappendi(FLD_PIN_RESET); oappend(");"); 
  #endif

  //WLEDMM add errorMessage to um settings
  if (strcmp(errorMessage, "") != 0) {
    oappend(SET_F("addInfo('errorMessage', 0, '<i>error: ")); oappend(errorMessage); oappend("! Correct and reboot</i>');");
  }
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
void FourLineDisplayUsermod::addToConfig(JsonObject& root) {
  // determine if we are using global HW pins (data & clock)
  int8_t hw_dta, hw_clk;
  if (displayIsSPI(type)) {
    hw_clk = spi_sclk;
    hw_dta = spi_mosi;
  } else {
    hw_clk = i2c_scl;
    hw_dta = i2c_sda;
  }

  JsonObject top   = root.createNestedObject(FPSTR(_name));
  top[FPSTR(_enabled)]       = enabled;

  JsonArray io_pin = top.createNestedArray("pin");
  for (int i=0; i<5; i++) {
    if      (i==0 && ioPin[i]==hw_clk) io_pin.add(-1); // do not store global HW pin
    else if (i==1 && ioPin[i]==hw_dta) io_pin.add(-1); // do not store global HW pin
    else                               io_pin.add(ioPin[i]);
  }
  top["type"]                = type;
  top[FPSTR(_flip)]          = (bool) flip;
  top[FPSTR(_contrast)]      = contrast;
  top[FPSTR(_contrastFix)]   = (bool) contrastFix;
  #if !defined(ARDUINO_ARCH_ESP32) || !defined(FLD_ESP32_USE_THREADS)
  top[FPSTR(_refreshRate)]   = refreshRate;
  #endif
  top[FPSTR(_screenTimeOut)] = screenTimeout/1000;
  top[FPSTR(_sleepMode)]     = (bool) sleepMode;
  top[FPSTR(_clockMode)]     = (bool) clockMode;
  top[FPSTR(_showSeconds)]   = (bool) showSeconds;
  top[FPSTR(_busClkFrequency)] = ioFrequency/1000;
  DEBUG_PRINTLN(F("4 Line Display config saved."));
}

/*
  * readFromConfig() can be used to read back the custom settings you added with addToConfig().
  * This is called by WLED when settings are loaded (currently this only happens once immediately after boot)
  * 
  * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
  * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
  * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
  */
bool FourLineDisplayUsermod::readFromConfig(JsonObject& root) {
  bool needsRedraw    = false;
  DisplayType newType = type;
  int8_t oldPin[5]; for (byte i=0; i<5; i++) oldPin[i] = ioPin[i];

  JsonObject top = root[FPSTR(_name)];
  if (top.isNull()) {
    DEBUG_PRINT(FPSTR(_name));
    DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
    return false;
  }

  enabled       = top[FPSTR(_enabled)] | enabled;
  newType       = top["type"] | newType;
  for (byte i=0; i<5; i++) ioPin[i] = top["pin"][i] | ioPin[i];
  flip          = top[FPSTR(_flip)] | flip;
  contrast      = top[FPSTR(_contrast)] | contrast;
  #if !defined(ARDUINO_ARCH_ESP32) || !defined(FLD_ESP32_USE_THREADS)
  refreshRate   = top[FPSTR(_refreshRate)] | refreshRate;
  refreshRate   = min(5000, max(250, (int)refreshRate));
  #endif
  screenTimeout = (top[FPSTR(_screenTimeOut)] | screenTimeout/1000) * 1000;
  sleepMode     = top[FPSTR(_sleepMode)] | sleepMode;
  clockMode     = top[FPSTR(_clockMode)] | clockMode;
  showSeconds   = top[FPSTR(_showSeconds)] | showSeconds;
  contrastFix   = top[FPSTR(_contrastFix)] | contrastFix;
  if (displayIsSPI(newType))
    ioFrequency = min(20000, max(500, (int)(top[FPSTR(_busClkFrequency)] | ioFrequency/1000))) * 1000;  // limit frequency
  else
    ioFrequency = min(3400, max(100, (int)(top[FPSTR(_busClkFrequency)] | ioFrequency/1000))) * 1000;  // limit frequency

  DEBUG_PRINT(FPSTR(_name));
  if (!initDone) {
    // first run: reading from cfg.json
    type = newType;
    DEBUG_PRINTLN(F(" config loaded."));
  } else {
    DEBUG_PRINTLN(F(" config (re)loaded."));
    // changing parameters from settings page
    bool pinsChanged = false;
    for (byte i=0; i<5; i++) if (ioPin[i] != oldPin[i]) { pinsChanged = true; break; }
    #if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
    unsigned long now = millis();
    while ((drawing || reDrawing) && millis()-now < 300) delay(10); // wait if someone else is drawing
    #endif
    drawing = false;
    reDrawing = false;

    if (pinsChanged || type!=newType) {
      if (typeOK) {
        typeOK = false;
        if (u8x8 != nullptr) delete u8x8; //WLEDMM warning: deleting object of polymorphic class type 'U8X8' which has non-virtual destructor might cause undefined behaviour [-Wdelete-non-virtual-dtor]
        u8x8 = nullptr;
        USER_PRINTLN(F("Display terminated."));
      }
      PinOwner po = PinOwner::UM_FourLineDisplay;
      bool isSPI = displayIsSPI(type);
      if (isSPI) {
        pinManager.deallocateMultiplePins((const uint8_t *)(&oldPin[2]), 3, po);
        bool isHW = (oldPin[0]==spi_sclk && oldPin[1]==spi_mosi);
        if (oldPin[0]==-1 && oldPin[1]==-1) isHW = true;   // WLEDMM "use global" means hardware driver
        if (spi_sclk==-1 && spi_mosi==-1) isHW = false;    // WLEDMM global pins not set -> software driver
        if (isHW) po = PinOwner::HW_SPI;
      } else {
        //bool isHW = (oldPin[0]==i2c_scl && oldPin[1]==i2c_sda);
        //if ((ioPin[0] == -1) || (ioPin[1] == -1)) isHW = true;  // WLEDMM "use global" = hardware
        //if (isHW) po = PinOwner::HW_I2C;                        // WLEDMM don't try to de-alloc HW pins.
      }
      pinManager.deallocateMultiplePins((const uint8_t *)oldPin, 2, po);
      type = newType;
      setup();
      needsRedraw |= true;
    } else {
      if (enabled && typeOK && (nullptr != u8x8)) { // WLEDMM ensure we have a valid, active driver
        u8x8->setBusClock(ioFrequency); // can be used for SPI too
        setVcomh(contrastFix);
        setContrast(contrast);
        setFlipMode(flip);
      }
    }
    knownHour = 99;
    if (needsRedraw && !wakeDisplay()) redraw(true);
    else overlayLogo(3500);
  }
  // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
  return !top[FPSTR(_contrastFix)].isNull();
}

// WLEDMM clean up some macros, so they don't cause problems in other usermods
#undef FLD_SemaphoreTake
#undef FLD_SemaphoreGive
