#include "wled.h"
#include "FontsBase.h"
#include "MyriadbitsFont3x5.h"
#include "MyriadbitsFont3x6.h"
#include "MyriadbitsFont3x8.h"
#include "MyriadbitsFont4x7.h"
#include "FX.h"

/*
 * Usermods allow you to add own functionality to WLED more easily
 * See: https://github.com/wled-dev/WLED/wiki/Add-own-functionality
 * 
 * This usermod 
 * 
 * active: enable/disable usermod
 */

 

class ClockOverlay : public Usermod 
{
  private:
    unsigned long lastTime = 0;
    int lastTimeSeconds = -1;
    static const char _txtName[];
    static const char _txtNameLower[];
    static const char _txtMsg[];
    static const char _txtTime[];

    bool initDone = false;

    // Config variables
    bool configEnabled = false;
    uint8_t configSegmentId = 0;
    uint8_t configTimeOptions = 0;
    uint8_t configTimeFont = 1;
    uint32_t configTimeColor = 0xFFFFFF;
    uint configBackgroundFade = 70;
    uint32_t configMessageOptions = 0;

    String displayMsg;
    int displayTime = 0;
    uint32_t activeTextColor = 0xFFFFFF;
    uint32_t activeBkColor = 0x000000;
    
    // defines for mask sizes
    #define maskSizeLeds        256 // TODO Dynamic!
    
    // overall mask to define which LEDs are displaying the time or message
    int maskLedsOn[maskSizeLeds];
    
    //
    // @brief  Draw characters on the LED panel using a specific font
    //
    void DrawGFX(EFontType fontType, ETextAlign align, int x, int y, char* text)
    {
      Segment& seg1 = strip.getSegment(configSegmentId);
      if (!seg1.isActive()) return;

      const GFXfont* pFont = NULL;
      switch (fontType) {
        case EFontType::FT_35: pFont = &MyriadbitsFont3x5; break;
        case EFontType::FT_36: pFont = &MyriadbitsFont3x6; break;
        case EFontType::FT_38: pFont = &MyriadbitsFont3x8; break;
        case EFontType::FT_47: pFont = &MyriadbitsFont4x7; break;
        default:
          return;
      }

      int posX = x;
      int posY = y;
      
      int len = strlen(text);
      if (align & ETextAlign::TA_HCENTER) {
        posX = (seg1.width() - CalculateGFXWidth(pFont, text, len)) / 2;
      }
      if (align & ETextAlign::TA_VCENTER) {
        int h = (seg1.height() - (pFont->yAdvance - 1));
        posY = ((h % 2) == 0) ? (h / 2) : (h / 2) + 1;
      }
      if (align & ETextAlign::TA_MIDTEXT) {
        int sizeX = CalculateGFXWidth(pFont, text, len / 2);
        if ((len % 2) != 0) {
            // Uneven characters, so add half of the width of the center character
            sizeX += (CalculateGFXWidth(pFont, &text[len/2], 1) / 2);
        }
        posX = (seg1.width() / 2) - sizeX;
      }
      if (align & ETextAlign::TA_END) {
        posX = (seg1.width() - CalculateGFXWidth(pFont, text, len)) - x + 1;
      }

      // Loop over the characters and draw each one separately
      for(int i = 0; i < len; i++) {
        char ch = text[i];
        if (ch >= pFont->first && ch < pFont->last) {
          int16_t c = (ch - pFont->first);

          GFXglyph *glyph = &(pFont->glyph[c]);
          uint8_t *bitmap = pFont->bitmap;

          uint16_t bo = glyph->bitmapOffset;
          uint8_t bits = 0;
          uint8_t bit = 0;

          for (int yy = glyph->height - 1; yy >= 0; yy--) {
            int yoffset = seg1.width() * (posY + y + (glyph->height - 1 - yy));
            for (int xx = 0; xx < glyph->width; xx++) {
              if (bit == 0x00) {
                bits = bitmap[bo++];
                bit = 0x80;
              }
              if (bits & bit) {
                maskLedsOn[posX + xx + yoffset] = 1;
              }
              bit = bit >> 1;
            }
          }
          posX += glyph->xAdvance;
        }
      }
    }


    /// @brief Calculate the width of an text
    /// @param font 
    /// @param text 
    int CalculateGFXWidth(const GFXfont* font, char* text, int numberOfChars)
    {
      int16_t xo16 = 0;
      for(int i = 0; i < numberOfChars; i++) {
        char ch = text[i];
        if (ch >= font->first && ch < font->last) {
          xo16 += (font->glyph[ch - font->first]).xAdvance;
        }
      }
      return xo16;
    }


  public:
    //Functions called by WLED

    /*
     * setup() is called once at boot. WiFi is not yet connected at this point.
     * You can use it to initialize variables, sensors or similar.
     */
    void setup() 
    {
      initDone = true;
    }

    /*
     * connected() is called every time the WiFi is (re)connected
     * Use it to initialize network interfaces
     */
    void connected() 
    {
    }

    /*
     * loop() is called continuously. Here you can check for events, read sensors, etc.    
     */
    void loop() {
      // Update every 0.1 seconds (required for text effects)
      if (millis() - lastTime > 100) {
        // check the time
        int seconds = second(localTime);
        if (lastTimeSeconds != seconds) {
          int minutes = minute(localTime);
          int hours = hour(localTime);

          for (int x = 0; x < maskSizeLeds; x++) {
              maskLedsOn[x] = 0;
          }

          EFontType fontType = EFontType::FT_36;
          switch (configTimeFont) {
            case 0: fontType = EFontType::FT_35; break; 
            case 2: fontType = EFontType::FT_38; break;
            case 3: fontType = EFontType::FT_47; break;
            default: fontType = EFontType::FT_36; break;
          }

          if (displayTime > 0) {
            DrawGFX(fontType, (ETextAlign)(ETextAlign::TA_VCENTER | ETextAlign::TA_MIDTEXT), 0, 0, (char*) displayMsg.c_str());
            displayTime--;
          }
          else {
            char buff[32];
            if (configTimeOptions == 0) {
              snprintf(buff, sizeof(buff), "%02d:%02d", hours, minutes);
            }
            else if (configTimeOptions == 1) {
              snprintf(buff, sizeof(buff), "%02d:%02d:%02d", hours, minutes, seconds);
            }
            DrawGFX(fontType, (ETextAlign)(ETextAlign::TA_VCENTER | ETextAlign::TA_MIDTEXT), 0, 0, buff);
            activeTextColor = configTimeColor;
          }

          // remember last clock update time
          lastTimeSeconds = seconds;
        }

        if (displayTime > 0) {
          switch (configMessageOptions) {
          case 0:
            activeTextColor = (activeTextColor == 0xFFA000) ? 0x000000 : 0xFFA000;
            activeBkColor = 0x000001;
            break;
          case 1:
            activeTextColor = 0xFFFFFF;
            activeBkColor = (activeBkColor == 0xFF0000) ? 0x00FF00 : 0xFF0000;
            break;
          case 2:
            activeTextColor = (activeTextColor == 0xFF0000) ? 0x00FF00 : 0xFF0000;
            activeBkColor = 0x000001;
            break;
          case 3:
            activeTextColor = (activeTextColor == 0x00FF80) ? 0xFF8000 : 0x00FF80;
            activeBkColor = (activeBkColor == 0xFF8000) ? 0x00FF80 : 0xFF8000;
            break;
          case 4:
            activeTextColor = (activeTextColor == 0xFF0000) ? 0x000000 : 0xFF0000;
            activeBkColor = 0x000001;
            break;
          default:
            activeTextColor = (activeTextColor == 0xFF0000) ? 0x000000 : 0xFF0000;
            activeBkColor = 0x000000; // Transparent
            break;
          }
        }
    
        // Remember last update
        lastTime = millis();
      }
    }

    /*
     * addToJsonState() can be used to add custom entries to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void addToJsonState(JsonObject& root) override
    {
      if (!initDone || !configEnabled) return;  // prevent crash on boot applyPreset()

      JsonObject usermod = root[FPSTR(_txtNameLower)];
      if (usermod.isNull()) usermod = root.createNestedObject(FPSTR(_txtName));

      usermod[FPSTR(_txtMsg)] = displayMsg;
      usermod[FPSTR(_txtTime)] = 0;
    }


    /*
     * readFromJsonState() can be used to receive data clients send to the /json/state part of the JSON API (state object).
     * Values in the state object may be modified by connected clients
     */
    void readFromJsonState(JsonObject& root) override
    {
      if (!initDone) return;  // prevent crash on boot applyPreset()

      JsonObject usermod = root[FPSTR(_txtNameLower)];
      if (!usermod.isNull()) {
        if (usermod[FPSTR(_txtMsg)] && usermod[FPSTR(_txtMsg)].is<String>()) {
          displayMsg = usermod[FPSTR(_txtMsg)].as<String>();
        }
        if (usermod[FPSTR(_txtTime)] && usermod[FPSTR(_txtTime)].is<int>()) {
          displayTime = usermod[FPSTR(_txtTime)].as<int>();
          if (displayTime > 0) displayTime++; // Make sure we show the all seconds
          DEBUG_PRINTF("Incoming message '%s' for %d s\n", displayMsg.c_str(), displayTime);
        }
      }
    }

    // TODO
    String colorToHexString(uint32_t c)
    {
      char buffer[9];
      sprintf(buffer, "%06X", c);
      return buffer;
    }

    // TODO
    bool hexStringToColor(String const &s, uint32_t &c, uint32_t def)
    {
      char *ep;
      unsigned long long r = strtoull(s.c_str(), &ep, 16);
      if (*ep == 0) {
        c = r;
        return true;
      }
      else {
        c = def;
        return false;
      }
    }

    /*
     * addToConfig() can be used to add custom persistent settings to the cfg.json file in the "um" (usermod) object.
     * It will be called by WLED when settings are actually saved (for example, LED settings are saved)
     * If you want to force saving the current state, use serializeConfig() in your loop().
     */
    void addToConfig(JsonObject& root)
    {
      JsonObject top = root.createNestedObject(F(_txtName));

      top[F("Active")] = configEnabled;
      top[F("Segment Id")] = configSegmentId;
      top[F("Time options")] = configTimeOptions;
      top[F("Time font")] = configTimeFont;
      top[F("Time color (RRGGBB)")] = colorToHexString(configTimeColor);
      top[F("Background fade")] = configBackgroundFade;
      top[F("Message options")] = configMessageOptions;
    }

    /*
     * Add some extra information to the config items
     */
    void appendConfigData()
    {
      oappend(F("addInfo('")); oappend(_txtName); oappend(F(":Time color (RRGGBB)', 1, '(RRGGBB)');"));

      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_txtName)).c_str()); oappend(F("','Time options');"));
      oappend(F("addOption(dd,'Hours:Minutes',0);"));
      oappend(F("addOption(dd,'Hours:Minutes:Seconds',1);"));
      //oappend(F("addOption(dd,'Date & Hours:Minutes',2);")); // TODO
      //oappend(F("addOption(dd,'Hours over Minutes',2);")); // TODO

      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_txtName)).c_str()); oappend(F("','Time font');"));
      oappend(F("addOption(dd,'Small 3x5',0);"));
      oappend(F("addOption(dd,'Normal 3x6',1);"));
      oappend(F("addOption(dd,'Tall 3x8',2);"));
      oappend(F("addOption(dd,'Large 5x7',3);"));

      oappend(F("dd=addDropdown('")); oappend(String(FPSTR(_txtName)).c_str()); oappend(F("','Message options');"));
      oappend(F("addOption(dd,'Amber flashing text',0);"));
      oappend(F("addOption(dd,'White on blinking red/green',1);"));
      oappend(F("addOption(dd,'Blinking red/green on white',2);"));
      oappend(F("addOption(dd,'Blinking invert amber/teal',3);"));
      oappend(F("addOption(dd,'Red flashing text',4);"));
      oappend(F("addOption(dd,'Transparent red flashing text',5);"));
    }

    /*
     * readFromConfig() can be used to read back the custom settings you added with addToConfig().
     */
    bool readFromConfig(JsonObject& root)
    {
      JsonObject top = root[F(_txtName)];

      bool configComplete = !top.isNull();

      String color;
      configComplete &= getJsonValue(top[F("Active")], configEnabled);
      configComplete &= getJsonValue(top[F("Segment Id")], configSegmentId);
      configComplete &= getJsonValue(top[F("Time options")], configTimeOptions);
      configComplete &= getJsonValue(top[F("Time color (RRGGBB)")], color, F("FFFFFF")) && hexStringToColor(color, configTimeColor, 0xFFFFFF);
      configComplete &= getJsonValue(top[F("Background fade")], configBackgroundFade);
      configComplete &= getJsonValue(top[F("Message options")], configMessageOptions);
      getJsonValue(top[F("Time font")], configTimeFont);

      return true;//configComplete;
    }

    /*
     * TODO
     */
    void handleOverlayDraw()
    {
      // check if usermod is active
      if (this->configEnabled) {
        // loop over all leds
        for (int x = 0; x < maskSizeLeds; x++) {
          // check mask
          if (maskLedsOn[x] == 1 && activeTextColor != 0x000000) {
            // Clock or mesage text
            strip.setPixelColor(x, activeTextColor);
          }
          else {
            if (displayTime > 0 && activeBkColor != 0x000000) {
              strip.setPixelColor(x, activeBkColor);
            } else {
              // Make the clock text stand out by fading the other pixels to black
              uint32_t color = strip.getPixelColor(x);
              strip.setPixelColor(x, color_fade(color, configBackgroundFade));
            }
          }
        }
      }
    }

    /*
     * getId() allows you to optionally give your V2 usermod an unique ID (please define it in const.h!).
     * This could be used in the future for the system to determine whether your usermod is installed.
     */
    uint16_t getId()
    {
      return 255;
    }

   //More methods can be added in the future, this example will then be extended.
   //Your usermod will remain compatible as it does not need to implement all methods from the Usermod base class!
};

static ClockOverlay usermod_clock_overlay;
REGISTER_USERMOD(usermod_clock_overlay);

// strings to reduce flash memory usage (used more than twice)
const char ClockOverlay::_txtName[]  PROGMEM = "Clock Overlay";
const char ClockOverlay::_txtNameLower[]  PROGMEM = "clock_overlay";
const char ClockOverlay::_txtMsg[]  PROGMEM = "msg";
const char ClockOverlay::_txtTime[]  PROGMEM = "time";
