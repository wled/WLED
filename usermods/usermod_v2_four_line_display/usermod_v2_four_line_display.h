#pragma once

#include "wled.h"
#include <U8x8lib.h> // from https://github.com/olikraus/u8g2/

//
// Insired by the v1 usermod: ssd1306_i2c_oled_u8g2
//
// v2 usermod for using 128x32 or 128x64 i2c
// OLED displays to provide a four line display
// for WLED.
//
// Dependencies
// * This usermod REQUIRES the ModeSortUsermod
// * This Usermod works best, by far, when coupled 
//   with RotaryEncoderUIUsermod.
//
// Make sure to enable NTP and set your time zone in WLED Config | Time.
//
// REQUIREMENT: You must add the following requirements to
// REQUIREMENT: "lib_deps" within platformio.ini / platformio_override.ini
// REQUIREMENT: *  U8g2  (the version already in platformio.ini is fine)
// REQUIREMENT: *  Wire
//

//The SCL and SDA pins are defined here. 
#ifndef FLD_PIN_SCL
  #define FLD_PIN_SCL i2c_scl
#endif
#ifndef FLD_PIN_SDA
  #define FLD_PIN_SDA i2c_sda
#endif
#ifndef FLD_PIN_CLOCKSPI
  #define FLD_PIN_CLOCKSPI spi_sclk
#endif
  #ifndef FLD_PIN_DATASPI
  #define FLD_PIN_DATASPI spi_mosi
#endif   
#ifndef FLD_PIN_CS
  #define FLD_PIN_CS spi_cs
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

#define TIME_INDENT        0
#define DATE_INDENT        2

// Minimum time between redrawing screen in ms
#define USER_LOOP_REFRESH_RATE_MS 1000

// Extra char (+1) for null
#define LINE_BUFFER_SIZE            16+1

typedef enum {
  FLD_LINE_BRIGHTNESS = 0,
  FLD_LINE_EFFECT_SPEED,
  FLD_LINE_EFFECT_INTENSITY,
  FLD_LINE_MODE,
  FLD_LINE_PALETTE,
  FLD_LINE_TIME
} Line4Type;

typedef enum {
  NONE = 0,
  SSD1306,      // U8X8_SSD1306_128X32_UNIVISION_HW_I2C
  SH1106,       // U8X8_SH1106_128X64_WINSTAR_HW_I2C
  SSD1306_64,   // U8X8_SSD1306_128X64_NONAME_HW_I2C
  SSD1305,      // U8X8_SSD1305_128X32_ADAFRUIT_HW_I2C
  SSD1305_64,   // U8X8_SSD1305_128X64_ADAFRUIT_HW_I2C
  SSD1306_SPI,  // U8X8_SSD1306_128X32_NONAME_HW_SPI
  SSD1306_SPI64 // U8X8_SSD1306_128X64_NONAME_HW_SPI
} DisplayType;

class FourLineDisplayUsermod : public Usermod {

  private:

    bool initDone = false;
    unsigned long lastTime = 0;

    // HW interface & configuration
    U8X8 *u8x8 = nullptr;           // pointer to U8X8 display object
    #ifndef FLD_SPI_DEFAULT
    int8_t ioPin[5] = {FLD_PIN_SCL, FLD_PIN_SDA, -1, -1, -1};        // I2C pins: SCL, SDA
    uint32_t ioFrequency = 400000;  // in Hz (minimum is 100000, baseline is 400000 and maximum should be 3400000)
    #else
    int8_t ioPin[5] = {FLD_PIN_CLOCKSPI, FLD_PIN_DATASPI, FLD_PIN_CS, FLD_PIN_DC, FLD_PIN_RESET}; // SPI pins: CLK, MOSI, CS, DC, RST
    uint32_t ioFrequency = 1000000;  // in Hz (minimum is 500kHz, baseline is 1MHz and maximum should be 20MHz)
    #endif
    DisplayType type = FLD_TYPE;    // display type
    bool flip = false;              // flip display 180°
    uint8_t contrast = 10;          // screen contrast
    uint8_t lineHeight = 1;         // 1 row or 2 rows
    uint32_t refreshRate = USER_LOOP_REFRESH_RATE_MS; // in ms
    uint32_t screenTimeout = SCREEN_TIMEOUT_MS;       // in ms
    bool sleepMode = true;          // allow screen sleep?
    bool clockMode = false;         // display clock
    bool enabled = true;

    // Next variables hold the previous known values to determine if redraw is
    // required.
    String knownSsid = "";
    IPAddress knownIp;
    uint8_t knownBrightness = 0;
    uint8_t knownEffectSpeed = 0;
    uint8_t knownEffectIntensity = 0;
    uint8_t knownMode = 0;
    uint8_t knownPalette = 0;
    uint8_t knownMinute = 99;
    uint8_t knownHour = 99;

    bool displayTurnedOff = false;
    unsigned long lastUpdate = 0;
    unsigned long lastRedraw = 0;
    unsigned long overlayUntil = 0;
    Line4Type lineType = FLD_LINE_BRIGHTNESS;
    // Set to 2 or 3 to mark lines 2 or 3. Other values ignored.
    byte markLineNum = 0;

    // strings to reduce flash memory usage (used more than twice)
    static const char _name[];
    static const char _enabled[];
    static const char _contrast[];
    static const char _refreshRate[];
    static const char _screenTimeOut[];
    static const char _flip[];
    static const char _sleepMode[];
    static const char _clockMode[];
    static const char _busClkFrequency[];

    // If display does not work or looks corrupted check the
    // constructor reference:
    // https://github.com/olikraus/u8g2/wiki/u8x8setupcpp
    // or check the gallery:
    // https://github.com/olikraus/u8g2/wiki/gallery

  public:

    // gets called once at boot. Do all initialization that doesn't depend on
    // network here
    void setup() {
      if (type == NONE || !enabled) return;

      bool isHW;
      PinOwner po = PinOwner::UM_FourLineDisplay;
      if (type == SSD1306_SPI || type == SSD1306_SPI64) {
        isHW = (ioPin[0]==spi_sclk && ioPin[1]==spi_mosi);
        if (isHW) po = PinOwner::HW_SPI;  // allow multiple allocations of HW I2C bus pins
        PinManagerPinType pins[5] = { { ioPin[0], true }, { ioPin[1], true }, { ioPin[2], true }, { ioPin[3], true }, { ioPin[4], true }};
        if (!pinManager.allocateMultiplePins(pins, 5, po)) { type=NONE; return; }
      } else {
        isHW = (ioPin[0]==i2c_scl && ioPin[1]==i2c_sda);
        if (isHW) po = PinOwner::HW_I2C;  // allow multiple allocations of HW I2C bus pins
        PinManagerPinType pins[2] = { { ioPin[0], true }, { ioPin[1], true } };
        if (!pinManager.allocateMultiplePins(pins, 2, po)) { type=NONE; return; }
      }

      DEBUG_PRINTLN(F("Allocating display."));
      switch (type) {
        case SSD1306:
          if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1306_128X32_UNIVISION_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
          else       u8x8 = (U8X8 *) new U8X8_SSD1306_128X32_UNIVISION_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
          lineHeight = 1;
          break;
        case SH1106:
          if (!isHW) u8x8 = (U8X8 *) new U8X8_SH1106_128X64_WINSTAR_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
          else       u8x8 = (U8X8 *) new U8X8_SH1106_128X64_WINSTAR_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
          lineHeight = 2;
          break;
        case SSD1306_64:
          if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1306_128X64_NONAME_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
          else       u8x8 = (U8X8 *) new U8X8_SSD1306_128X64_NONAME_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
          lineHeight = 2;
          break;
        case SSD1305:
          if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1305_128X32_NONAME_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
          else       u8x8 = (U8X8 *) new U8X8_SSD1305_128X32_ADAFRUIT_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
          lineHeight = 1;
          break;
        case SSD1305_64:
          if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1305_128X64_ADAFRUIT_SW_I2C(ioPin[0], ioPin[1]); // SCL, SDA, reset
          else       u8x8 = (U8X8 *) new U8X8_SSD1305_128X64_ADAFRUIT_HW_I2C(U8X8_PIN_NONE, ioPin[0], ioPin[1]); // Pins are Reset, SCL, SDA
          lineHeight = 2;
          break;
        case SSD1306_SPI:
          if (!isHW)  u8x8 = (U8X8 *) new U8X8_SSD1306_128X32_UNIVISION_4W_SW_SPI(ioPin[0], ioPin[1], ioPin[2], ioPin[3], ioPin[4]);
          else        u8x8 = (U8X8 *) new U8X8_SSD1306_128X32_UNIVISION_4W_HW_SPI(ioPin[2], ioPin[3], ioPin[4]); // Pins are cs, dc, reset
          lineHeight = 1;
          break;
        case SSD1306_SPI64:
          if (!isHW) u8x8 = (U8X8 *) new U8X8_SSD1306_128X64_NONAME_4W_SW_SPI(ioPin[0], ioPin[1], ioPin[2], ioPin[3], ioPin[4]);
          else       u8x8 = (U8X8 *) new U8X8_SSD1306_128X64_NONAME_4W_HW_SPI(ioPin[2], ioPin[3], ioPin[4]); // Pins are cs, dc, reset
          lineHeight = 2;
          break;
        default:
          u8x8 = nullptr;
      }

      if (nullptr == u8x8) {
          DEBUG_PRINTLN(F("Display init failed."));
          pinManager.deallocateMultiplePins((const uint8_t*)ioPin, (type == SSD1306_SPI || type == SSD1306_SPI64) ? 5 : 2, po);
          type = NONE;
          return;
      }

      initDone = true;
      DEBUG_PRINTLN(F("Starting display."));
      /*if (!(type == SSD1306_SPI || type == SSD1306_SPI64))*/ u8x8->setBusClock(ioFrequency);  // can be used for SPI too
      u8x8->begin();
      setFlipMode(flip);
      setContrast(contrast); //Contrast setup will help to preserve OLED lifetime. In case OLED need to be brighter increase number up to 255
      setPowerSave(0);
      drawString(0, 0, "Loading...");
    }

    // gets called every time WiFi is (re-)connected. Initialize own network
    // interfaces here
    void connected() {}

    /**
     * Da loop.
     */
    void loop() {
      if (!enabled || millis() - lastUpdate < (clockMode?1000:refreshRate) || strip.isUpdating()) return;
      lastUpdate = millis();

      redraw(false);
    }

    /**
     * Wrappers for screen drawing
     */
    void setFlipMode(uint8_t mode) {
      if (type == NONE || !enabled) return;
      u8x8->setFlipMode(mode);
    }
    void setContrast(uint8_t contrast) {
      if (type == NONE || !enabled) return;
      u8x8->setContrast(contrast);
    }
    void drawString(uint8_t col, uint8_t row, const char *string, bool ignoreLH=false) {
      if (type == NONE || !enabled) return;
      u8x8->setFont(u8x8_font_chroma48medium8_r);
      if (!ignoreLH && lineHeight==2) u8x8->draw1x2String(col, row, string);
      else                            u8x8->drawString(col, row, string);
    }
    void draw2x2String(uint8_t col, uint8_t row, const char *string) {
      if (type == NONE || !enabled) return;
      u8x8->setFont(u8x8_font_chroma48medium8_r);
      u8x8->draw2x2String(col, row, string);
    }
    void drawGlyph(uint8_t col, uint8_t row, char glyph, const uint8_t *font, bool ignoreLH=false) {
      if (type == NONE || !enabled) return;
      u8x8->setFont(font);
      if (!ignoreLH && lineHeight==2) u8x8->draw1x2Glyph(col, row, glyph);
      else                            u8x8->drawGlyph(col, row, glyph);
    }
    uint8_t getCols() {
      if (type==NONE || !enabled) return 0;
      return u8x8->getCols();
    }
    void clear() {
      if (type == NONE || !enabled) return;
      u8x8->clear();
    }
    void setPowerSave(uint8_t save) {
      if (type == NONE || !enabled) return;
      u8x8->setPowerSave(save);
    }

    void center(String &line, uint8_t width) {
      int len = line.length();
      if (len<width) for (byte i=(width-len)/2; i>0; i--) line = ' ' + line;
      for (byte i=line.length(); i<width; i++) line += ' ';
    }

    /**
     * Redraw the screen (but only if things have changed
     * or if forceRedraw).
     */
    void redraw(bool forceRedraw) {
      static bool showName = false;
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

      // Check if values which are shown on display changed from the last time.
      if (forceRedraw ||
          (((apActive) ? String(apSSID) : WiFi.SSID()) != knownSsid) ||
          (knownIp != (apActive ? IPAddress(4, 3, 2, 1) : Network.localIP())) ||
          (knownBrightness != bri) ||
          (knownEffectSpeed != effectSpeed) ||
          (knownEffectIntensity != effectIntensity) ||
          (knownMode != strip.getMainSegment().mode) ||
          (knownPalette != strip.getMainSegment().palette)) {
        knownHour = 99;   // force time update
        lastRedraw = now; // update lastRedraw marker
      } else if (sleepMode && !displayTurnedOff && ((now - lastRedraw)/1000)%5 == 0) {
        // change line every 5s
        showName = !showName;
        switch (lineType) {
          case FLD_LINE_BRIGHTNESS:
            lineType = FLD_LINE_EFFECT_SPEED;
            break;
          case FLD_LINE_MODE:
            lineType = FLD_LINE_BRIGHTNESS;
            break;
          case FLD_LINE_PALETTE:
            lineType = clockMode ? FLD_LINE_MODE : FLD_LINE_BRIGHTNESS;
            break;
          case FLD_LINE_EFFECT_SPEED:
            lineType = FLD_LINE_EFFECT_INTENSITY;
            break;
          case FLD_LINE_EFFECT_INTENSITY:
            lineType = FLD_LINE_PALETTE;
            break;
          default:
            lineType = FLD_LINE_MODE;
            break;
        }
        knownHour = 99; // force time update
        // do not update lastRedraw marker if just switching row contenet
      } else {
        // Nothing to change.
        // Turn off display after 3 minutes with no change.
        if(sleepMode && !displayTurnedOff && (millis() - lastRedraw > screenTimeout)) {
          // We will still check if there is a change in redraw()
          // and turn it back on if it changed.
          sleepOrClock(true);
        } else if (displayTurnedOff && clockMode) {
          showTime();
        }
        return;
      }

      // Turn the display back on
      if (displayTurnedOff) sleepOrClock(false);

      // Update last known values.
      knownSsid = apActive ? WiFi.softAPSSID() : WiFi.SSID();
      knownIp = apActive ? IPAddress(4, 3, 2, 1) : Network.localIP();
      knownBrightness = bri;
      knownMode = strip.getMainSegment().mode;
      knownPalette = strip.getMainSegment().palette;
      knownEffectSpeed = effectSpeed;
      knownEffectIntensity = effectIntensity;

      // Do the actual drawing
      String line;
      // First row with Wifi name
      drawGlyph(0, 0, 80, u8x8_font_open_iconic_embedded_1x1); // home icon
      line = knownSsid.substring(0, getCols() > 1 ? getCols() - 2 : 0);
      center(line, getCols()-2);
      drawString(1, 0, line.c_str());
      // Print `~` char to indicate that SSID is longer, than our display
      if (knownSsid.length() > (int)getCols()-1) {
        drawString(getCols() - 1, 0, "~");
      }

      // Second row with IP or Password
      drawGlyph(0, lineHeight, 68, u8x8_font_open_iconic_embedded_1x1); // wifi icon
      // Print password in AP mode and if led is OFF.
      if (apActive && bri == 0) {
        drawString(1, lineHeight, apPass);
      } else {
        // alternate IP address and server name
        line = knownIp.toString();
        if (showName && strcmp(serverDescription, "WLED") != 0) {
          line = serverDescription;
        }
        center(line, getCols()-1);
        drawString(1, lineHeight, line.c_str());
      }

      // draw third and fourth row
      drawLine(2, clockMode ? lineType : FLD_LINE_MODE);
      drawLine(3, clockMode ? FLD_LINE_TIME : lineType);

      drawGlyph(0, 2*lineHeight, 66 + (bri > 0 ? 3 : 0), u8x8_font_open_iconic_weather_2x2); // sun/moon icon
      //if (markLineNum>1) drawGlyph(2, markLineNum*lineHeight, 66, u8x8_font_open_iconic_arrow_1x1); // arrow icon
    }

    void drawLine(uint8_t line, Line4Type lineType) {
      char lineBuffer[LINE_BUFFER_SIZE];
      uint8_t printedChars;
      switch(lineType) {
        case FLD_LINE_BRIGHTNESS:
          sprintf_P(lineBuffer, PSTR("Brightness %3d"), bri);
          drawString(2, line*lineHeight, lineBuffer);
          break;
        case FLD_LINE_EFFECT_SPEED:
          sprintf_P(lineBuffer, PSTR("FX Speed   %3d"), effectSpeed);
          drawString(2, line*lineHeight, lineBuffer);
          break;
        case FLD_LINE_EFFECT_INTENSITY:
          sprintf_P(lineBuffer, PSTR("FX Intens. %3d"), effectIntensity);
          drawString(2, line*lineHeight, lineBuffer);
          break;
        case FLD_LINE_MODE:
          printedChars = extractModeName(knownMode, JSON_mode_names, lineBuffer, LINE_BUFFER_SIZE-1);
          for (;printedChars < getCols()-2 && printedChars < LINE_BUFFER_SIZE-3; printedChars++) lineBuffer[printedChars]=' ';
          lineBuffer[printedChars] = 0;
          drawString(2, line*lineHeight, lineBuffer);
          break;
        case FLD_LINE_PALETTE:
          printedChars = extractModeName(knownPalette, JSON_palette_names, lineBuffer, LINE_BUFFER_SIZE-1);
          for (;printedChars < getCols()-2 && printedChars < LINE_BUFFER_SIZE-3; printedChars++) lineBuffer[printedChars]=' ';
          lineBuffer[printedChars] = 0;
          drawString(2, line*lineHeight, lineBuffer);
          break;
        case FLD_LINE_TIME:
        default:
          showTime(false);
          break;
      }
    }

    /**
     * If there screen is off or in clock is displayed,
     * this will return true. This allows us to throw away
     * the first input from the rotary encoder but
     * to wake up the screen.
     */
    bool wakeDisplay() {
      if (type == NONE || !enabled) return false;
      knownHour = 99;
      if (displayTurnedOff) {
        // Turn the display back on
        sleepOrClock(false);
        redraw(true);
        return true;
      }
      return false;
    }

    /**
     * Allows you to show up to two lines as overlay for a
     * period of time.
     * Clears the screen and prints on the middle two lines.
     */
    void overlay(const char* line1, const char *line2, long showHowLong) {
      if (type == NONE || !enabled) return;

      if (displayTurnedOff) {
        // Turn the display back on (includes clear())
        sleepOrClock(false);
      } else {
        clear();
      }

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
    }

    void setLineType(byte lT) {
      lineType = (Line4Type) lT;
    }

    /**
     * Line 3 or 4 (last two lines) can be marked with an
     * arrow in the first column. Pass 2 or 3 to this to
     * specify which line to mark with an arrow.
     * Any other values are ignored.
     */
    void setMarkLine(byte newMarkLineNum) {
      if (newMarkLineNum == 2 || newMarkLineNum == 3) {
        markLineNum = newMarkLineNum;
      }
      else {
        markLineNum = 0;
      }
    }

    /**
     * Enable sleep (turn the display off) or clock mode.
     */
    void sleepOrClock(bool enabled) {
      clear();
      if (enabled) {
        if (clockMode) showTime();
        else           setPowerSave(1);
        displayTurnedOff = true;
      } else {
        setPowerSave(0);
        displayTurnedOff = false;
      }
    }

    /**
     * Display the current date and time in large characters
     * on the middle rows. Based 24 or 12 hour depending on
     * the useAMPM configuration.
     */
    void showTime(bool fullScreen = true) {
      if (type == NONE || !enabled) return;
      char lineBuffer[LINE_BUFFER_SIZE];

      updateLocalTime();
      byte minuteCurrent = minute(localTime);
      byte hourCurrent   = hour(localTime);
      byte secondCurrent = second(localTime);
      if (knownMinute == minuteCurrent && knownHour == hourCurrent) {
        // Time hasn't changed.
        if (!fullScreen) return;
      }
      knownMinute = minuteCurrent;
      knownHour = hourCurrent;

      byte currentMonth = month(localTime);
      sprintf_P(lineBuffer, PSTR("%s %2d "), monthShortStr(currentMonth), day(localTime));
      if (fullScreen)
        draw2x2String(DATE_INDENT, lineHeight==1 ? 0 : lineHeight, lineBuffer); // adjust for 8 line displays
      else
        drawString(2, lineHeight*3, lineBuffer);

      byte showHour = hourCurrent;
      boolean isAM = false;
      if (useAMPM) {
        if (showHour == 0) {
          showHour = 12;
          isAM = true;
        } 
        else if (showHour > 12) {
          showHour -= 12;
          isAM = false;
        }
        else {
          isAM = true;
        }
      }

      sprintf_P(lineBuffer, (secondCurrent%2 || !fullScreen) ? PSTR("%2d:%02d") : PSTR("%2d %02d"), (useAMPM ? showHour : hourCurrent), minuteCurrent);
      // For time, we always use LINE_HEIGHT of 2 since
      // we are printing it big.
      if (fullScreen) {
        draw2x2String(TIME_INDENT+2, lineHeight*2, lineBuffer);
        sprintf_P(lineBuffer, PSTR("%02d"), secondCurrent);
        if (useAMPM) drawString(12+(fullScreen?0:2), lineHeight*2, (isAM ? "AM" : "PM"), true);
        else         drawString(12, lineHeight*2+1, lineBuffer, true); // even with double sized rows print seconds in 1 line
      } else {
        drawString(9+(useAMPM?0:2), lineHeight*3, lineBuffer);
        if (useAMPM) drawString(12+(fullScreen?0:2), lineHeight*3, (isAM ? "AM" : "PM"), true);
      }
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
    void addToConfig(JsonObject& root) {
      JsonObject top   = root.createNestedObject(FPSTR(_name));
      top[FPSTR(_enabled)]       = enabled;
      JsonArray io_pin = top.createNestedArray("pin");
      for (byte i=0; i<5; i++) io_pin.add(ioPin[i]);
      top["help4Pins"]           = F("Clk,Data,CS,DC,RST"); // help for Settings page
      top["type"]                = type;
      top["help4Type"]           = F("1=SSD1306,2=SH1106,3=SSD1306_128x64,4=SSD1305,5=SSD1305_128x64,6=SSD1306_SPI,7=SSD1306_SPI_128x64"); // help for Settings page
      top[FPSTR(_flip)]          = (bool) flip;
      top[FPSTR(_contrast)]      = contrast;
      top[FPSTR(_refreshRate)]   = refreshRate/1000;
      top[FPSTR(_screenTimeOut)] = screenTimeout/1000;
      top[FPSTR(_sleepMode)]     = (bool) sleepMode;
      top[FPSTR(_clockMode)]     = (bool) clockMode;
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
    bool readFromConfig(JsonObject& root) {
      bool needsRedraw    = false;
      DisplayType newType = type;
      int8_t newPin[5]; for (byte i=0; i<5; i++) newPin[i] = ioPin[i];

      JsonObject top = root[FPSTR(_name)];
      if (top.isNull()) {
        DEBUG_PRINT(FPSTR(_name));
        DEBUG_PRINTLN(F(": No config found. (Using defaults.)"));
        return false;
      }

      enabled       = top[FPSTR(_enabled)] | enabled;
      newType       = top["type"] | newType;
      for (byte i=0; i<5; i++) newPin[i] = top["pin"][i] | ioPin[i];
      flip          = top[FPSTR(_flip)] | flip;
      contrast      = top[FPSTR(_contrast)] | contrast;
      refreshRate   = (top[FPSTR(_refreshRate)] | refreshRate/1000) * 1000;
      screenTimeout = (top[FPSTR(_screenTimeOut)] | screenTimeout/1000) * 1000;
      sleepMode     = top[FPSTR(_sleepMode)] | sleepMode;
      clockMode     = top[FPSTR(_clockMode)] | clockMode;
      if (newType == SSD1306_SPI || newType == SSD1306_SPI64)
        ioFrequency = min(20000, max(500, (int)(top[FPSTR(_busClkFrequency)] | ioFrequency/1000))) * 1000;  // limit frequency
      else
        ioFrequency = min(3400, max(100, (int)(top[FPSTR(_busClkFrequency)] | ioFrequency/1000))) * 1000;  // limit frequency

      DEBUG_PRINT(FPSTR(_name));
      if (!initDone) {
        // first run: reading from cfg.json
        for (byte i=0; i<5; i++) ioPin[i] = newPin[i];
        type = newType;
        DEBUG_PRINTLN(F(" config loaded."));
      } else {
        DEBUG_PRINTLN(F(" config (re)loaded."));
        // changing parameters from settings page
        bool pinsChanged = false;
        for (byte i=0; i<5; i++) if (ioPin[i] != newPin[i]) { pinsChanged = true; break; }
        if (pinsChanged || type!=newType) {
          if (type != NONE) delete u8x8;
          PinOwner po = PinOwner::UM_FourLineDisplay;
          bool isSPI = (type == SSD1306_SPI || type == SSD1306_SPI64);
          if (isSPI) {
            if (ioPin[0]==spi_sclk && ioPin[1]==spi_mosi) po = PinOwner::HW_SPI;  // allow multiple allocations of HW SPI bus pins
            pinManager.deallocateMultiplePins((const uint8_t *)ioPin, 5, po);
          } else {
            if (ioPin[0]==i2c_scl && ioPin[1]==i2c_sda) po = PinOwner::HW_I2C;  // allow multiple allocations of HW I2C bus pins
            pinManager.deallocateMultiplePins((const uint8_t *)ioPin, 2, po);
          }
          for (byte i=0; i<5; i++) ioPin[i] = newPin[i];
          if (ioPin[0]<0 || ioPin[1]<0) { // data & clock must be > -1
            type = NONE;
            return true;
          } else type = newType;
          setup();
          needsRedraw |= true;
        }
        if (!(type == SSD1306_SPI || type == SSD1306_SPI64)) u8x8->setBusClock(ioFrequency); // can be used for SPI too
        setContrast(contrast);
        setFlipMode(flip);
        if (needsRedraw && !wakeDisplay()) redraw(true);
      }
      // use "return !top["newestParameter"].isNull();" when updating Usermod with new features
      return !top[FPSTR(_enabled)].isNull();
    }

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
const char FourLineDisplayUsermod::_refreshRate[]     PROGMEM = "refreshRateSec";
const char FourLineDisplayUsermod::_screenTimeOut[]   PROGMEM = "screenTimeOutSec";
const char FourLineDisplayUsermod::_flip[]            PROGMEM = "flip";
const char FourLineDisplayUsermod::_sleepMode[]       PROGMEM = "sleepMode";
const char FourLineDisplayUsermod::_clockMode[]       PROGMEM = "clockMode";
const char FourLineDisplayUsermod::_busClkFrequency[] PROGMEM = "i2c-freq-kHz";
