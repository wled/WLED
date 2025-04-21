#include "wled.h"
#undef U8X8_NO_HW_I2C // borrowed from WLEDMM: we do want I2C hardware drivers - if possible
#include <U8x8lib.h> // from https://github.com/olikraus/u8g2/
#include "4LD_wled_fonts.c"

#ifndef FLD_ESP32_NO_THREADS
  #define FLD_ESP32_USE_THREADS  // comment out to use 0.13.x behaviour without parallel update task - slower, but more robust. May delay other tasks like LEDs or audioreactive!!
#endif

//
// Inspired by the usermod_v2_four_line_display
//
// v2 usermod for using 128x32 or 128x64 i2c
// OLED displays to provide a four line display
// for WLED.
//
// Dependencies
// * This Usermod works best, by far, when coupled
//   with RotaryEncoderUI ALT Usermod.
//
// Make sure to enable NTP and set your time zone in WLED Config | Time.
//
// REQUIREMENT: You must add the following requirements to
// REQUIREMENT: "lib_deps" within platformio.ini / platformio_override.ini
// REQUIREMENT: *  U8g2  (the version already in platformio.ini is fine)
// REQUIREMENT: *  Wire
//
// If display does not work or looks corrupted check the
// constructor reference:
// https://github.com/olikraus/u8g2/wiki/u8x8setupcpp
// or check the gallery:
// https://github.com/olikraus/u8g2/wiki/gallery

#ifndef FLD_PIN_CS
  #define FLD_PIN_CS 15
#endif

#ifdef ARDUINO_ARCH_ESP32
  #ifndef FLD_PIN_DC
    #define FLD_PIN_DC 19
  #endif
  #ifndef FLD_PIN_RESET
    #define FLD_PIN_RESET 26
  #endif
#else
  #ifndef FLD_PIN_DC
    #define FLD_PIN_DC 12
  #endif
  #ifndef FLD_PIN_RESET
    #define FLD_PIN_RESET 16
  #endif
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
  SSD1306,          // U8X8_SSD1306_128X32_UNIVISION_HW_I2C
  SH1106,           // U8X8_SH1106_128X64_WINSTAR_HW_I2C
  SSD1306_64,       // U8X8_SSD1306_128X64_NONAME_HW_I2C
  SSD1305,          // U8X8_SSD1305_128X32_ADAFRUIT_HW_I2C
  SSD1305_64,       // U8X8_SSD1305_128X64_ADAFRUIT_HW_I2C
  SSD1306_SPI,      // U8X8_SSD1306_128X32_NONAME_HW_SPI
  SSD1306_SPI64,    // U8X8_SSD1306_128X64_NONAME_HW_SPI
  SSD1309_SPI64,    // U8X8_SSD1309_128X64_NONAME0_4W_HW_SPI
  SSD1309_64        // U8X8_SSD1309_128X64_NONAME0_HW_I2C
} DisplayType;


class FourLineDisplayUsermod : public Usermod {
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  public:
    FourLineDisplayUsermod() { if (!instance) instance = this; }
    static FourLineDisplayUsermod* getInstance(void) { return instance; }
#endif

  private:

    static FourLineDisplayUsermod *instance;
    bool initDone = false;
    volatile bool drawing = false;
    volatile bool lockRedraw = false;

    // HW interface & configuration
    U8X8 *u8x8 = nullptr;           // pointer to U8X8 display object

    #ifndef FLD_SPI_DEFAULT
    int8_t ioPin[3] = {-1, -1, -1}; // I2C pins: SCL, SDA
    uint32_t ioFrequency = 400000;  // in Hz (minimum is 100000, baseline is 400000 and maximum should be 3400000)
    #else
    int8_t ioPin[3] = {FLD_PIN_CS, FLD_PIN_DC, FLD_PIN_RESET}; // custom SPI pins: CS, DC, RST
    uint32_t ioFrequency = 1000000;  // in Hz (minimum is 500kHz, baseline is 1MHz and maximum should be 20MHz)
    #endif

    DisplayType type = FLD_TYPE;    // display type
    bool flip = false;              // flip display 180°
    uint8_t contrast = 10;          // screen contrast
    uint8_t lineHeight = 1;         // 1 row or 2 rows
    uint16_t refreshRate = REFRESH_RATE_MS;     // in ms
    uint32_t screenTimeout = SCREEN_TIMEOUT_MS; // in ms
    bool sleepMode = true;          // allow screen sleep?
    bool clockMode = false;         // display clock
    bool showSeconds = true;        // display clock with seconds
    bool enabled = true;
    bool contrastFix = false;

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

    // some displays need this to properly apply contrast
    void setVcomh(bool highContrast);
    void startDisplay();

    /**
     * Wrappers for screen drawing
     */
    void setFlipMode(uint8_t mode);
    void setContrast(uint8_t contrast);
    void drawString(uint8_t col, uint8_t row, const char *string, bool ignoreLH=false);
    void draw2x2String(uint8_t col, uint8_t row, const char *string);
    void drawGlyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font, bool ignoreLH=false);
    void draw2x2Glyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font);
    void draw2x2GlyphIcons();
    uint8_t getCols();
    void clear();
    void setPowerSave(uint8_t save);
    void center(String &line, uint8_t width);

    /**
     * Display the current date and time in large characters
     * on the middle rows. Based 24 or 12 hour depending on
     * the useAMPM configuration.
     */
    void showTime();

    /**
     * Enable sleep (turn the display off) or clock mode.
     */
    void sleepOrClock(bool enabled);

  public:

    // gets called once at boot. Do all initialization that doesn't depend on
    // network here
    void setup() override;

    // gets called every time WiFi is (re-)connected. Initialize own network
    // interfaces here
    void connected() override;

    /**
     * Da loop.
     */
    void loop() override;

    //function to update lastredraw
    inline void updateRedrawTime() { lastRedraw = millis(); }

    /**
     * Redraw the screen (but only if things have changed
     * or if forceRedraw).
     */
    void redraw(bool forceRedraw);

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
     * handleButton() can be used to override default button behaviour. Returning true
     * will prevent button working in a default way.
     * Replicating button.cpp
     */
    bool handleButton(uint8_t b);

    void onUpdateBegin(bool init) override;

    /*
     * addToJsonInfo() can be used to add custom entries to the /json/info part of the JSON API.
     * Creating an "u" object allows you to add custom key/value pairs to the Info section of the WLED web UI.
     * Below it is shown how this could be used for e.g. a light sensor
     */
    //void addToJsonInfo(JsonObject& root) override;

    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    //void addToJsonState(JsonObject& root) override;

    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    //void readFromJsonState(JsonObject& root) override;

    void appendConfigData() override;

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
    void addToConfig(JsonObject& root) override;

    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     * This is called by WLED when settings are loaded (currently this only happens once immediately after boot)
     *
     * readFromConfig() is called BEFORE setup(). This means you can use your persistent values in setup() (e.g. pin assignments, buffer sizes),
     * but also that if you want to write persistent values to a dynamic buffer, you'd need to allocate it here instead of in setup.
     * If you don't know what that is, don't fret. It most likely doesn't affect your use case :)
     */
    bool readFromConfig(JsonObject& root) override;

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId() override {
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

#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
FourLineDisplayUsermod *FourLineDisplayUsermod::instance = nullptr;
#endif

// some displays need this to properly apply contrast
void FourLineDisplayUsermod::setVcomh(bool highContrast) {
  if (type == NONE || !enabled) return;
  u8x8_t *u8x8_struct = u8x8->getU8x8();
  u8x8_cad_StartTransfer(u8x8_struct);
  u8x8_cad_SendCmd(u8x8_struct, 0x0db); //address of value
  u8x8_cad_SendArg(u8x8_struct, highContrast ? 0x000 : 0x040); //value 0 for fix, reboot resets default back to 64
  u8x8_cad_EndTransfer(u8x8_struct);
}

void FourLineDisplayUsermod::startDisplay() {
  if (type == NONE || !enabled) return;
  lineHeight = u8x8->getRows() > 4 ? 2 : 1;
  DEBUG_PRINTLN(F("Starting display."));
  u8x8->setBusClock(ioFrequency);  // can be used for SPI too
  u8x8->begin();
  setFlipMode(flip);
  setVcomh(contrastFix);
  setContrast(contrast); //Contrast setup will help to preserve OLED lifetime. In case OLED need to be brighter increase number up to 255
  setPowerSave(0);
  //drawString(0, 0, "Loading...");
  overlayLogo(3500);
}

/**
 * Wrappers for screen drawing
 */
void FourLineDisplayUsermod::setFlipMode(uint8_t mode) {
  if (type == NONE || !enabled) return;
  u8x8->setFlipMode(mode);
}
void FourLineDisplayUsermod::setContrast(uint8_t contrast) {
  if (type == NONE || !enabled) return;
  u8x8->setContrast(contrast);
}
void FourLineDisplayUsermod::drawString(uint8_t col, uint8_t row, const char *string, bool ignoreLH) {
  if (type == NONE || !enabled) return;
  drawing = true;
  u8x8->setFont(u8x8_font_chroma48medium8_r);
  if (!ignoreLH && lineHeight==2) u8x8->draw1x2String(col, row, string);
  else                            u8x8->drawString(col, row, string);
  drawing = false;
}
void FourLineDisplayUsermod::draw2x2String(uint8_t col, uint8_t row, const char *string) {
  if (type == NONE || !enabled) return;
  drawing = true;
  u8x8->setFont(u8x8_font_chroma48medium8_r);
  u8x8->draw2x2String(col, row, string);
  drawing = false;
}
void FourLineDisplayUsermod::drawGlyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font, bool ignoreLH) {
  if (type == NONE || !enabled) return;
  drawing = true;
  u8x8->setFont(font);
  if (!ignoreLH && lineHeight==2) u8x8->draw1x2Glyph(col, row, glyph);
  else                            u8x8->drawGlyph(col, row, glyph);
  drawing = false;
}
void FourLineDisplayUsermod::draw2x2Glyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font) {
  if (type == NONE || !enabled) return;
  drawing = true;
  u8x8->setFont(font);
  u8x8->draw2x2Glyph(col, row, glyph);
  drawing = false;
}
uint8_t FourLineDisplayUsermod::getCols() {
  if (type==NONE || !enabled) return 0;
  return u8x8->getCols();
}
void FourLineDisplayUsermod::clear() {
  if (type == NONE || !enabled) return;
  drawing = true;
  u8x8->clear();
  drawing = false;
}
void FourLineDisplayUsermod::setPowerSave(uint8_t save) {
  if (type == NONE || !enabled) return;
  u8x8->setPowerSave(save);
}

void FourLineDisplayUsermod::center(String &line, uint8_t width) {
  int len = line.length();
  if (len<width) for (unsigned i=(width-len)/2; i>0; i--) line = ' ' + line;
  for (unsigned i=line.length(); i<width; i++) line += ' ';
}

void FourLineDisplayUsermod::draw2x2GlyphIcons() {
  drawing = true;
  if (lineHeight == 2) {
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
  drawing = false;
}

/**
 * Display the current date and time in large characters
 * on the middle rows. Based 24 or 12 hour depending on
 * the useAMPM configuration.
 */
void FourLineDisplayUsermod::showTime() {
  if (type == NONE || !enabled || !displayTurnedOff) return;

  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing) return;

  char lineBuffer[LINE_BUFFER_SIZE];
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
      sprintf_P(lineBuffer, PSTR("%s %2d "), monthShortStr(month(localTime)), day(localTime));
      draw2x2String(2, lineHeight==1 ? 0 : lineHeight, lineBuffer); // adjust for 8 line displays, draw month and day
    }
    sprintf_P(lineBuffer,PSTR("%2d:%02d"), (useAMPM ? AmPmHour : hourCurrent), minuteCurrent);
    draw2x2String(2, lineHeight*2, lineBuffer); //draw hour, min. blink ":" depending on odd/even seconds
    if (useAMPM) drawString(12, lineHeight*2, (isitAM ? "AM" : "PM"), true); //draw am/pm if using 12 time

    drawStatusIcons(); //icons power, wifi, timer, etc

    knownMinute = minuteCurrent;
    knownHour   = hourCurrent;
  }
  if (showSeconds && secondCurrent != lastSecond) {
    lastSecond = secondCurrent;
    draw2x2String(6, lineHeight*2, secondCurrent%2 ? " " : ":");
    sprintf_P(lineBuffer, PSTR("%02d"), secondCurrent);
    drawString(12, lineHeight*2+1, lineBuffer, true); // even with double sized rows print seconds in 1 line
  }
}

/**
 * Enable sleep (turn the display off) or clock mode.
 */
void FourLineDisplayUsermod::sleepOrClock(bool enabled) {
  if (enabled) {
    displayTurnedOff = true;
    if (clockMode && ntpEnabled) {
      knownMinute = knownHour = 99;
      showTime();
    } else
      setPowerSave(1);
  } else {
    displayTurnedOff = false;
    setPowerSave(0);
  }
}

// gets called once at boot. Do all initialization that doesn't depend on
// network here
void FourLineDisplayUsermod::setup() {
  bool isSPI = (type == SSD1306_SPI || type == SSD1306_SPI64 || type == SSD1309_SPI64);

  // check if pins are -1 and disable usermod as PinManager::allocateMultiplePins() will accept -1 as a valid pin
  if (isSPI) {
    if (spi_sclk<0 || spi_mosi<0 || ioPin[0]<0 || ioPin[1]<0 || ioPin[1]<0) {
      type = NONE;
    } else {
      PinManagerPinType cspins[3] = { { ioPin[0], true }, { ioPin[1], true }, { ioPin[2], true } };
      if (!PinManager::allocateMultiplePins(cspins, 3, PinOwner::UM_FourLineDisplay)) { type = NONE; }
    }
  } else {
    if (i2c_scl<0 || i2c_sda<0) { type=NONE; }
  }

  DEBUG_PRINTLN(F("Allocating display."));
  switch (type) {
    // U8X8 uses Wire (or Wire1 with 2ND constructor) and will use existing Wire properties (calls Wire.begin() though)
    case SSD1306:       u8x8 = (U8X8 *) new U8X8_SSD1306_128X32_UNIVISION_HW_I2C(); break;
    case SH1106:        u8x8 = (U8X8 *) new U8X8_SH1106_128X64_WINSTAR_HW_I2C();    break;
    case SSD1306_64:    u8x8 = (U8X8 *) new U8X8_SSD1306_128X64_NONAME_HW_I2C();    break;
    case SSD1305:       u8x8 = (U8X8 *) new U8X8_SSD1305_128X32_ADAFRUIT_HW_I2C();  break;
    case SSD1305_64:    u8x8 = (U8X8 *) new U8X8_SSD1305_128X64_ADAFRUIT_HW_I2C();  break;
    case SSD1309_64:    u8x8 = (U8X8 *) new U8X8_SSD1309_128X64_NONAME0_HW_I2C();   break;
    // U8X8 uses global SPI variable that is attached to VSPI bus on ESP32
    case SSD1306_SPI:   u8x8 = (U8X8 *) new U8X8_SSD1306_128X32_UNIVISION_4W_HW_SPI(ioPin[0], ioPin[1], ioPin[2]); break; // Pins are cs, dc, reset
    case SSD1306_SPI64: u8x8 = (U8X8 *) new U8X8_SSD1306_128X64_NONAME_4W_HW_SPI(ioPin[0], ioPin[1], ioPin[2]);    break; // Pins are cs, dc, reset
    case SSD1309_SPI64: u8x8 = (U8X8 *) new U8X8_SSD1309_128X64_NONAME0_4W_HW_SPI(ioPin[0], ioPin[1], ioPin[2]);   break; // Pins are cs, dc, reset
    // catchall
    default:            u8x8 = (U8X8 *) new U8X8_NULL(); enabled = false; break; // catchall to create U8x8 instance
  }

  if (nullptr == u8x8) {
    DEBUG_PRINTLN(F("Display init failed."));
    if (isSPI) {
      PinManager::deallocateMultiplePins((const uint8_t*)ioPin, 3, PinOwner::UM_FourLineDisplay);
    }
    type = NONE;
    return;
  }

  startDisplay();
  onUpdateBegin(false);  // create Display task
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
#if !(defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS))
  if (!enabled || strip.isUpdating()) return;
  unsigned long now = millis();
  if (now < nextUpdate) return;
  nextUpdate = now + ((displayTurnedOff && clockMode && showSeconds) ? 1000 : refreshRate);
  redraw(false);
#endif
}

/**
 * Redraw the screen (but only if things have changed
 * or if forceRedraw).
 */
void FourLineDisplayUsermod::redraw(bool forceRedraw) {
  bool needRedraw = false;
  unsigned long now = millis();

  if (type == NONE || !enabled) return;
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

  while (drawing && millis()-now < 25) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;

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
    } else if (displayTurnedOff && ntpEnabled) {
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
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;
#endif
  knownBrightness = bri;
  if (overlayUntil == 0) {
    lockRedraw = true;
    brightness100 = ((uint16_t)bri*100)/255;
    char lineBuffer[4];
    sprintf_P(lineBuffer, PSTR("%-3d"), brightness100);
    drawString(1, lineHeight, lineBuffer);
    lockRedraw = false;
  }
}

void FourLineDisplayUsermod::updateSpeed() {
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;
#endif
  knownEffectSpeed = effectSpeed;
  if (overlayUntil == 0) {
    lockRedraw = true;
    fxspeed100 = ((uint16_t)effectSpeed*100)/255;
    char lineBuffer[4];
    sprintf_P(lineBuffer, PSTR("%-3d"), fxspeed100);
    drawString(5, lineHeight, lineBuffer);
    lockRedraw = false;
  }
}

void FourLineDisplayUsermod::updateIntensity() {
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;
#endif
  knownEffectIntensity = effectIntensity;
  if (overlayUntil == 0) {
    lockRedraw = true;
    fxintensity100 = ((uint16_t)effectIntensity*100)/255;
    char lineBuffer[4];
    sprintf_P(lineBuffer, PSTR("%-3d"), fxintensity100);
    drawString(9, lineHeight, lineBuffer);
    lockRedraw = false;
  }
}

void FourLineDisplayUsermod::drawStatusIcons() {
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;
#endif
  uint8_t col = 15;
  uint8_t row = 0;
  lockRedraw = true;
  drawGlyph(col, row,   (wificonnected ? 20 : 0), u8x8_4LineDisplay_WLED_icons_1x1, true); // wifi icon
  if (lineHeight==2) { col--; } else { row++; }
  drawGlyph(col, row,          (bri > 0 ? 9 : 0), u8x8_4LineDisplay_WLED_icons_1x1, true); // power icon
  if (lineHeight==2) { col--; } else { col = row = 0; }
  drawGlyph(col, row, (nightlightActive ? 6 : 0), u8x8_4LineDisplay_WLED_icons_1x1, true); // moon icon for nighlight mode
  lockRedraw = false;
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
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;
#endif
  lockRedraw = true;
  if (markColNum != 255 && markLineNum !=255) drawGlyph(markColNum, markLineNum*lineHeight, 21, u8x8_4LineDisplay_WLED_icons_1x1);
  lockRedraw = false;
}

//Display the current effect or palette (desiredEntry)
// on the appropriate line (row).
void FourLineDisplayUsermod::showCurrentEffectOrPalette(int inputEffPal, const char *qstring, uint8_t row) {
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;
#endif
  char lineBuffer[MAX_JSON_CHARS];
  if (overlayUntil == 0) {
    lockRedraw = true;
    // Find the mode name in JSON
    unsigned printedChars = extractModeName(inputEffPal, qstring, lineBuffer, MAX_JSON_CHARS-1);
    if (lineBuffer[0]=='*' && lineBuffer[1]==' ') {
      // remove "* " from dynamic palettes
      for (unsigned i=2; i<=printedChars; i++) lineBuffer[i-2] = lineBuffer[i]; //include '\0'
      printedChars -= 2;
    } else if ((lineBuffer[0]==' ' && lineBuffer[1]>127)) {
      // remove note symbol from effect names
      for (unsigned i=5; i<=printedChars; i++) lineBuffer[i-5] = lineBuffer[i]; //include '\0'
      printedChars -= 5;
    }
    if (lineHeight == 2) {                                 // use this code for 8 line display
      char smallBuffer1[MAX_MODE_LINE_SPACE];
      char smallBuffer2[MAX_MODE_LINE_SPACE];
      unsigned smallChars1 = 0;
      unsigned smallChars2 = 0;
      if (printedChars < MAX_MODE_LINE_SPACE) {            // use big font if the text fits
        while (printedChars < (MAX_MODE_LINE_SPACE-1)) lineBuffer[printedChars++]=' ';
        lineBuffer[printedChars] = 0;
        drawString(1, row*lineHeight, lineBuffer);
      } else {                                             // for long names divide the text into 2 lines and print them small
        bool spaceHit = false;
        for (unsigned i = 0; i < printedChars; i++) {
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
        drawString(1, row*lineHeight, smallBuffer1, true);
        while (smallChars2 < (MAX_MODE_LINE_SPACE-1)) smallBuffer2[smallChars2++]=' ';
        smallBuffer2[smallChars2] = 0;
        drawString(1, row*lineHeight+1, smallBuffer2, true);
      }
    } else {                                             // use this code for 4 ling displays
      char smallBuffer3[MAX_MODE_LINE_SPACE+1];          // uses 1x1 icon for mode/palette
      unsigned smallChars3 = 0;
      for (unsigned i = 0; i < MAX_MODE_LINE_SPACE; i++) smallBuffer3[smallChars3++] = (i >= printedChars) ? ' ' : lineBuffer[i];
      smallBuffer3[smallChars3] = 0;
      drawString(1, row*lineHeight, smallBuffer3, true);
    }
    lockRedraw = false;
  }
}

/**
 * If there screen is off or in clock is displayed,
 * this will return true. This allows us to throw away
 * the first input from the rotary encoder but
 * to wake up the screen.
 */
bool FourLineDisplayUsermod::wakeDisplay() {
  if (type == NONE || !enabled) return false;
  if (displayTurnedOff) {
  #if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
    unsigned long now = millis();
    while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
    if (drawing || lockRedraw) return false;
  #endif
    lockRedraw = true;
    clear();
    // Turn the display back on
    sleepOrClock(false);
    lockRedraw = false;
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
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;
#endif
  lockRedraw = true;
  // Turn the display back on
  if (!wakeDisplay()) clear();
  // Print the overlay
  if (glyphType>0 && glyphType<255) {
    if (lineHeight == 2) drawGlyph(5, 0, glyphType, u8x8_4LineDisplay_WLED_icons_6x6, true); // use 3x3 font with draw2x2Glyph() if flash runs short and comment out 6x6 font
    else                 drawGlyph(6, 0, glyphType, u8x8_4LineDisplay_WLED_icons_3x3, true);
  }
  if (line1) {
    String buf = line1;
    center(buf, getCols());
    drawString(0, (glyphType<255?3:0)*lineHeight, buf.c_str());
  }
  overlayUntil = millis() + showHowLong;
  lockRedraw = false;
}

/**
 * Allows you to show Akemi WLED logo overlay for a period of time.
 * Clears the screen and prints.
 */
void FourLineDisplayUsermod::overlayLogo(long showHowLong) {
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;
#endif
  lockRedraw = true;
  // Turn the display back on
  if (!wakeDisplay()) clear();
  // Print the overlay
  if (lineHeight == 2) {
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
  lockRedraw = false;
}

/**
 * Allows you to show two lines as overlay for a period of time.
 * Clears the screen and prints.
 * Used in Auto Save usermod
 */
void FourLineDisplayUsermod::overlay(const char* line1, const char* line2, long showHowLong) {
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;
#endif
  lockRedraw = true;
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
  lockRedraw = false;
}

void FourLineDisplayUsermod::networkOverlay(const char* line1, long showHowLong) {
#if defined(ARDUINO_ARCH_ESP32) && defined(FLD_ESP32_USE_THREADS)
  unsigned long now = millis();
  while (drawing && millis()-now < 125) delay(1); // wait if someone else is drawing
  if (drawing || lockRedraw) return;
#endif
  lockRedraw = true;

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
  lockRedraw = false;
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

#ifndef ARDUINO_RUNNING_CORE
  #if CONFIG_FREERTOS_UNICORE
    #define ARDUINO_RUNNING_CORE 0
  #else
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
      xTaskCreatePinnedToCore(
        [](void * par) {                  // Function to implement the task
          // see https://www.freertos.org/vtaskdelayuntil.html
          const TickType_t xFrequency = REFRESH_RATE_MS * portTICK_PERIOD_MS / 2;
          TickType_t xLastWakeTime = xTaskGetTickCount();
          for(;;) {
            delay(1); // DO NOT DELETE THIS LINE! It is needed to give the IDLE(0) task enough time and to keep the watchdog happy.
                      // taskYIELD(), yield(), vTaskDelay() and esp_task_wdt_feed() didn't seem to work.
            vTaskDelayUntil(&xLastWakeTime, xFrequency); // release CPU, by doing nothing for REFRESH_RATE_MS millis
            FourLineDisplayUsermod::getInstance()->redraw(false);
          }
        },
        "4LD",                // Name of the task
        3072,                 // Stack size in words
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
//void FourLineDisplayUsermod::addToJsonInfo(JsonObject& root) {
  //JsonObject user = root["u"];
  //if (user.isNull()) user = root.createNestedObject("u");
  //JsonArray data = user.createNestedArray(F("4LineDisplay"));
  //data.add(F("Loaded."));
//}

/*
  * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
  * Values in the state object may be modified by connected clients
  */
//void FourLineDisplayUsermod::addToJsonState(JsonObject& root) {
//}

/*
  * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
  * Values in the state object may be modified by connected clients
  */
//void FourLineDisplayUsermod::readFromJsonState(JsonObject& root) {
//  if (!initDone) return;  // prevent crash on boot applyPreset()
//}

void FourLineDisplayUsermod::appendConfigData() {
  oappend(F("dd=addDropdown('4LineDisplay','type');"));
  oappend(F("addOption(dd,'None',0);"));
  oappend(F("addOption(dd,'SSD1306',1);"));
  oappend(F("addOption(dd,'SH1106',2);"));
  oappend(F("addOption(dd,'SSD1306 128x64',3);"));
  oappend(F("addOption(dd,'SSD1305',4);"));
  oappend(F("addOption(dd,'SSD1305 128x64',5);"));
  oappend(F("addOption(dd,'SSD1309 128x64',9);"));
  oappend(F("addOption(dd,'SSD1306 SPI',6);"));
  oappend(F("addOption(dd,'SSD1306 SPI 128x64',7);"));
  oappend(F("addOption(dd,'SSD1309 SPI 128x64',8);"));
  oappend(F("addInfo('4LineDisplay:type',1,'<br><i class=\"warn\">Change may require reboot</i>','');"));
  oappend(F("addInfo('4LineDisplay:pin[]',0,'','SPI CS');"));
  oappend(F("addInfo('4LineDisplay:pin[]',1,'','SPI DC');"));
  oappend(F("addInfo('4LineDisplay:pin[]',2,'','SPI RST');"));
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
  JsonObject top   = root.createNestedObject(FPSTR(_name));
  top[FPSTR(_enabled)]       = enabled;

  top["type"]                = type;
  JsonArray io_pin = top.createNestedArray("pin");
  for (int i=0; i<3; i++) io_pin.add(ioPin[i]);
  top[FPSTR(_flip)]          = (bool) flip;
  top[FPSTR(_contrast)]      = contrast;
  top[FPSTR(_contrastFix)]   = (bool) contrastFix;
  #ifndef ARDUINO_ARCH_ESP32
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
  int8_t oldPin[3]; for (unsigned i=0; i<3; i++) oldPin[i] = ioPin[i];

  JsonObject top = root[FPSTR(_name)];
  if (top.isNull()) {
    DEBUG_PRINT(FPSTR(_name));
    DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
    return false;
  }

  enabled       = top[FPSTR(_enabled)] | enabled;
  newType       = top["type"] | newType;
  for (unsigned i=0; i<3; i++) ioPin[i] = top["pin"][i] | ioPin[i];
  flip          = top[FPSTR(_flip)] | flip;
  contrast      = top[FPSTR(_contrast)] | contrast;
  #ifndef ARDUINO_ARCH_ESP32
  refreshRate   = top[FPSTR(_refreshRate)] | refreshRate;
  refreshRate   = min(5000, max(250, (int)refreshRate));
  #endif
  screenTimeout = (top[FPSTR(_screenTimeOut)] | screenTimeout/1000) * 1000;
  sleepMode     = top[FPSTR(_sleepMode)] | sleepMode;
  clockMode     = top[FPSTR(_clockMode)] | clockMode;
  showSeconds   = top[FPSTR(_showSeconds)] | showSeconds;
  contrastFix   = top[FPSTR(_contrastFix)] | contrastFix;
  if (newType == SSD1306_SPI || newType == SSD1306_SPI64)
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
    for (unsigned i=0; i<3; i++) if (ioPin[i] != oldPin[i]) { pinsChanged = true; break; }
    if (pinsChanged || type!=newType) {
      bool isSPI = (type == SSD1306_SPI || type == SSD1306_SPI64 || type == SSD1309_SPI64);
      bool newSPI = (newType == SSD1306_SPI || newType == SSD1306_SPI64 || newType == SSD1309_SPI64);
      if (isSPI) {
        if (pinsChanged || !newSPI) PinManager::deallocateMultiplePins((const uint8_t*)oldPin, 3, PinOwner::UM_FourLineDisplay);
        if (!newSPI) {
          // was SPI but is no longer SPI
          if (i2c_scl<0 || i2c_sda<0) { newType=NONE; }
        } else {
          // still SPI but pins changed
          PinManagerPinType cspins[3] = { { ioPin[0], true }, { ioPin[1], true }, { ioPin[2], true } };
          if (ioPin[0]<0 || ioPin[1]<0 || ioPin[1]<0) { newType=NONE; }
          else if (!PinManager::allocateMultiplePins(cspins, 3, PinOwner::UM_FourLineDisplay)) { newType=NONE; }
        }
      } else if (newSPI) {
        // was I2C but is now SPI
        if (spi_sclk<0 || spi_mosi<0) {
          newType=NONE;
        } else {
          PinManagerPinType pins[3] = { { ioPin[0], true }, { ioPin[1], true }, { ioPin[2], true } };
          if (ioPin[0]<0 || ioPin[1]<0 || ioPin[1]<0) { newType=NONE; }
          else if (!PinManager::allocateMultiplePins(pins, 3, PinOwner::UM_FourLineDisplay)) { newType=NONE; }
        }
      } else {
        // just I2C type changed
      }
      type = newType;
      switch (type) {
        case SSD1306:
          u8x8_Setup(u8x8->getU8x8(), u8x8_d_ssd1306_128x32_univision, u8x8_cad_ssd13xx_fast_i2c, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
          u8x8_SetPin_HW_I2C(u8x8->getU8x8(), U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE);
          break;
        case SH1106:
          u8x8_Setup(u8x8->getU8x8(), u8x8_d_sh1106_128x64_winstar, u8x8_cad_ssd13xx_fast_i2c, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
          u8x8_SetPin_HW_I2C(u8x8->getU8x8(), U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE);
          break;
        case SSD1306_64:
          u8x8_Setup(u8x8->getU8x8(), u8x8_d_ssd1306_128x64_noname, u8x8_cad_ssd13xx_fast_i2c, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
          u8x8_SetPin_HW_I2C(u8x8->getU8x8(), U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE);
          break;
        case SSD1305:
          u8x8_Setup(u8x8->getU8x8(), u8x8_d_ssd1305_128x32_adafruit, u8x8_cad_ssd13xx_fast_i2c, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
          u8x8_SetPin_HW_I2C(u8x8->getU8x8(), U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE);
          break;
        case SSD1305_64:
          u8x8_Setup(u8x8->getU8x8(), u8x8_d_ssd1305_128x64_adafruit, u8x8_cad_ssd13xx_fast_i2c, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
          u8x8_SetPin_HW_I2C(u8x8->getU8x8(), U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE);
          break;
        case SSD1309_64:
          u8x8_Setup(u8x8->getU8x8(), u8x8_d_ssd1309_128x64_noname0, u8x8_cad_ssd13xx_fast_i2c, u8x8_byte_arduino_hw_i2c, u8x8_gpio_and_delay_arduino);
          u8x8_SetPin_HW_I2C(u8x8->getU8x8(), U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE);
          break;
        case SSD1306_SPI:
          u8x8_Setup(u8x8->getU8x8(), u8x8_d_ssd1306_128x32_univision, u8x8_cad_001, u8x8_byte_arduino_hw_spi, u8x8_gpio_and_delay_arduino);
          u8x8_SetPin_4Wire_HW_SPI(u8x8->getU8x8(), ioPin[0], ioPin[1], ioPin[2]); // Pins are cs, dc, reset
          break;
        case SSD1306_SPI64:
          u8x8_Setup(u8x8->getU8x8(), u8x8_d_ssd1306_128x64_noname, u8x8_cad_001, u8x8_byte_arduino_hw_spi, u8x8_gpio_and_delay_arduino);
          u8x8_SetPin_4Wire_HW_SPI(u8x8->getU8x8(), ioPin[0], ioPin[1], ioPin[2]); // Pins are cs, dc, reset
          break;
        case SSD1309_SPI64:
          u8x8_Setup(u8x8->getU8x8(), u8x8_d_ssd1309_128x64_noname0, u8x8_cad_001, u8x8_byte_arduino_hw_spi, u8x8_gpio_and_delay_arduino);
          u8x8_SetPin_4Wire_HW_SPI(u8x8->getU8x8(), ioPin[0], ioPin[1], ioPin[2]); // Pins are cs, dc, reset
        default:
          u8x8_Setup(u8x8->getU8x8(), u8x8_d_null_cb, u8x8_cad_empty, u8x8_byte_empty, u8x8_dummy_cb);
          enabled = false;
          break;
      }
      startDisplay();
      needsRedraw |= true;
    } else {
      u8x8->setBusClock(ioFrequency); // can be used for SPI too
      setVcomh(contrastFix);
      setContrast(contrast);
      setFlipMode(flip);
    }
    knownHour = 99;
    if (needsRedraw && !wakeDisplay()) redraw(true);
    else overlayLogo(3500);
  }
  // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
  return !top[FPSTR(_contrastFix)].isNull();
}


static FourLineDisplayUsermod usermod_v2_four_line_display_alt;
REGISTER_USERMOD(usermod_v2_four_line_display_alt);