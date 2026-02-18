#pragma once
#include "wled.h"


/*--------------------------------------------------------------------------------------------------------------------------------------
  7-Segment Clock & Countdown Usermod

  Usermod for WLED providing a configurable 7-segment style display
  supporting clock and countdown modes. The display is rendered as a
  logical overlay using a segment-to-LED mapping and integrates
  seamlessly into the WLED usermod framework.

  Creator : DereIBims
  Version : 0.1.0

  Resources:
  - 3D-print files (enclosures/mounts) and PCB design files will be available soon:
    * 3D-print: MakerWorld
    * PCB: GitHub, inside this usermod folder
----------------------------------------------------------------------*/

#define SEG_A 0
#define SEG_B 1
#define SEG_C 2
#define SEG_D 3
#define SEG_E 4
#define SEG_F 5
#define SEG_G 6

/*-------------------------------Begin modifications down here!!!------------------------
-----------------------------------------------------------------------------------------
-----------------------------------------------------------------------------------------*/

#define MQTT_Topic "7seg"

class SevenSegClockCountdown : public Usermod {
private:
    // Geometry: LED counts for a single digit and the full panel.
    // - One digit has 7 segments, each with LEDS_PER_SEG pixels
    // - Two separator blocks exist between digit 2/3 and 4/5, each with SEP_LEDS pixels
    static const uint16_t VERY_FIRST_LED = 0;
    static const uint16_t LEDS_PER_SEG = 5;
    static const uint8_t SEGS_PER_DIGIT = 7;
    static const uint16_t SEP_LEDS = 10;
    static constexpr uint16_t LEDS_PER_DIGIT = LEDS_PER_SEG * SEGS_PER_DIGIT;       // 35
    static constexpr uint16_t TOTAL_PANEL_LEDS = 6 * LEDS_PER_DIGIT + 2 * SEP_LEDS; // 230

    // Physical-to-logical segment mapping per digit: physical order F-A-B-G-E-D-C
    static constexpr uint8_t PHYS_TO_LOG[SEGS_PER_DIGIT] = {
        SEG_F,
        SEG_A,
        SEG_B,
        SEG_G,
        SEG_E,
        SEG_D,
        SEG_C};

    // Index helpers into the linear LED stream for each digit and separator block.
    static uint16_t digitBase(uint8_t d) {
        switch (d) {
        case 0:
            return VERY_FIRST_LED; // digit 1
        case 1:
            return VERY_FIRST_LED + LEDS_PER_DIGIT; // digit 2
        case 2:
            return VERY_FIRST_LED + SEP_LEDS + LEDS_PER_DIGIT * 2; // digit 3
        case 3:
            return VERY_FIRST_LED + SEP_LEDS + LEDS_PER_DIGIT * 3; // digit 4
        case 4:
            return VERY_FIRST_LED + SEP_LEDS * 2 + LEDS_PER_DIGIT * 4; // digit 5
        case 5:
            return VERY_FIRST_LED + SEP_LEDS * 2 + LEDS_PER_DIGIT * 5; // digit 6
        default:
            return 0;
        }
    }
    static constexpr uint16_t sep1Base() { return 70; }
    static constexpr uint16_t sep2Base() { return 150; }

    static const unsigned long countdownBlinkInterval = 500;

    /*--------------------------------------------------------------------------------------
    --------------------------Do NOT edit anything below this line--------------------------
    --------------------------------------------------------------------------------------*/

    /* ===== Digit bitmasks (A..G bits 0..6) ===== */
    static constexpr uint8_t DIGIT_MASKS[10] = {
        0b00111111, // 0 (A..F)
        0b00000110, // 1 (B,C)
        0b01011011, // 2 (A,B,D,E,G)
        0b01001111, // 3 (A,B,C,D,G)
        0b01100110, // 4 (B,C,F,G)
        0b01101101, // 5 (A,C,D,F,G)
        0b01111101, // 6 (A,C,D,E,F,G)
        0b00000111, // 7 (A,B,C)
        0b01111111, // 8 (A..G)
        0b01101111  // 9 (A,B,C,D,F,G)
    };

    /* ===== Runtime state (mask, flags, variables) ===== */
    std::vector<uint8_t> mask;
    bool enabled = true;
    bool sepsOn = true;
    bool SeparatorOn = true;
    bool SeparatorOff = true;

    bool showClock = true;
    bool showCountdown = false;
    uint16_t alternatingTime = 10;

    int targetYear = year(localTime);
    uint8_t targetMonth = month(localTime);
    uint8_t targetDay = day(localTime);
    uint8_t targetHour = 0;
    uint8_t targetMinute = 0;
    time_t targetUnix = 0;

    uint32_t fullHours = 0;
    uint32_t fullMinutes = 0;
    uint32_t fullSeconds = 0;
    uint32_t remDays = 0;
    uint8_t remHours = 0;
    uint8_t remMinutes = 0;
    uint8_t remSeconds = 0;

    unsigned long lastSecondMillis = 0;
    int lastSecondValue = -1;

    bool IgnoreBlinking = false;
    bool ModeChanged = false;
    uint8_t prevMode = 0;
    uint32_t prevColor = 0;
    uint8_t prevSpeed = 0;
    uint8_t prevIntensity = 0;
    uint8_t prevBrightness = 0;
    unsigned long lastBlink = 0;
    bool BlinkToggle = false;

    // Returns a bitmask A..G (0..6). If the character is unsupported, fallback lights A, D, G.
    uint8_t LETTER_MASK(char c) {
        switch (c) {
        // Uppercase
        case 'A':
            return 0b01110111; // A,B,C,E,F,G
        case 'B':
            return 0b01111100; // b-like (C,D,E,F,G)
        case 'C':
            return 0b00111001; // A,D,E,F
        case 'D':
            return 0b01011110; // d-like (B,C,D,E,G)
        case 'E':
            return 0b01111001; // A,D,E,F,G
        case 'F':
            return 0b01110001; // A,E,F,G
        case 'H':
            return 0b01110110; // B,C,E,F,G
        case 'J':
            return 0b00011110; // B,C,D
        case 'L':
            return 0b00111000; // D,E,F
        case 'O':
            return 0b00111111; // A,B,C,D,E,F (like '0')
        case 'P':
            return 0b01110011; // A,B,E,F,G
        case 'T':
            return 0b01111000; // D,E,F,G
        case 'U':
            return 0b00111110; // B,C,D,E,F
        case 'Y':
            return 0b01101110; // B,C,D,F,G

        // Lowercase
        case 'a':
            return 0b01011111; // A,B,C,D,E,G
        case 'b':
            return 0b01111100; // C,D,E,F,G
        case 'c':
            return 0b01011000; // D,E,G
        case 'd':
            return 0b01011110; // B,C,D,E,G
        case 'e':
            return 0b01111011; // A,D,E,F,G (with C off)
        case 'f':
            return 0b01110001; // A,E,F,G
        case 'h':
            return 0b01110100; // C,E,F,G
        case 'j':
            return 0b00001110; // B,C,D
        case 'l':
            return 0b00110000; // E,F
        case 'n':
            return 0b01010100; // C,E,G
        case 'o':
            return 0b01011100; // C,D,E,G
        case 'r':
            return 0b01010000; // E,G
        case 't':
            return 0b01111000; // D,E,F,G
        case 'u':
            return 0b00011100; // C,D,E
        case 'y':
            return 0b01101110; // B,C,D,F,G
        case '-':
            return 0b01000000; // G
        case '_':
            return 0b00001000; // D
        case ' ':
            return 0b00000000; // blank

        default:
            return 0b01001001; // fallback: A, D, G
        }
    }

    static int clampVal(int v, int lo, int hi) {
        return (v < lo) ? lo : (v > hi ? hi : v);
    }

    /* ===== Private methods ===== */
    void ensureMaskSize();                                                                            // ensure mask matches panel size
    void clearMask();                                                                                 // clear mask to transparent
    void setRangeOn(uint16_t start, uint16_t len);                                                    // set contiguous mask range on
    int getHundredths(int currentSeconds, bool countDown);                                            // compute hundredths (0..99)
    void drawClock();                                                                                 // render HH:MM:SS into mask
    void revertMode();                                                                                // restore previous effect mode
    void SaveMode();                                                                                  // save current effect mode
    void setMode(uint8_t mode, uint32_t color, uint8_t speed, uint8_t intensity, uint8_t brightness); // apply effect mode
    void drawCountdown();                                                                             // render countdown/count-up into mask
    void setDigitInt(uint8_t digitIndex, int8_t value);                                               // draw numeric digit
    void setDigitChar(uint8_t digitIndex, char c);                                                    // draw character/symbol
    void setSeparator(uint8_t which, bool on);                                                        // set both separator dots
    void setSeparatorHalf(uint8_t which, bool upperDot, bool on);                                     // set separator half
    void applyMaskToStrip();                                                                          // apply mask to physical strip
    void validateTarget(bool changed = false);                                                        // clamp fields and compute targetUnix

public:
    void setup() override;                                   // prepare usermod
    void loop() override;                                    // periodic loop (unused)
    void handleOverlayDraw();                                // main overlay draw entrypoint
    void addToJsonInfo(JsonObject &root) override;           // add compact info UI
    void addToJsonState(JsonObject &root) override;          // serialize state
    void readFromJsonState(JsonObject &root) override;       // read state
    void addToConfig(JsonObject &root) override;             // write persistent config
    bool readFromConfig(JsonObject &root) override;          // read persistent config
    void onMqttConnect(bool sessionPresent) override;        // subscribe on connect
    bool onMqttMessage(char *topic, char *payload) override; // handle mqtt messages
    uint16_t getId() override;                               // usermod id
};
