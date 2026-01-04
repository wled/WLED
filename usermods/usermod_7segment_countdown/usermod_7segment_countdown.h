#pragma once
#include <stdint.h>

// Seven-segment overlay constants and utilities for a 6-digit panel with two separators.

// Logical segment indices (A..G) used to build bitmasks for digits/letters.
#define SEG_A 0
#define SEG_B 1
#define SEG_C 2
#define SEG_D 3
#define SEG_E 4
#define SEG_F 5
#define SEG_G 6

// Geometry: LED counts for a single digit and the full panel.
// - One digit has 7 segments, each with LEDS_PER_SEG pixels
// - Two separator blocks exist between digit 2/3 and 4/5, each with SEP_LEDS pixels
static const uint16_t LEDS_PER_SEG   = 5;
static const uint8_t  SEGS_PER_DIGIT = 7;
static const uint16_t SEP_LEDS       = 10;
static constexpr uint16_t LEDS_PER_DIGIT = LEDS_PER_SEG * SEGS_PER_DIGIT; // 35
static constexpr uint16_t TOTAL_PANEL_LEDS = 6 * LEDS_PER_DIGIT + 2 * SEP_LEDS; // 230

// Physical-to-logical segment mapping per digit: physical order F-A-B-G-E-D-C → logical A..G.
// This allows drawing by logical segment index while wiring can remain arbitrary.
static constexpr uint8_t PHYS_TO_LOG[SEGS_PER_DIGIT] = {
    SEG_F,
    SEG_A,
    SEG_B,
    SEG_G,
    SEG_E,
    SEG_D,
    SEG_C
};

// Digit bitmasks (bits A..G correspond to indices 0..6). 1 means the segment is lit.
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

// Runtime state used by the overlay renderer and UI.
// - mask: per-LED on/off mask (1 keeps original color/effect, 0 forces black)
// - enabled: master on/off for the overlay
// - sepsOn: helper for separator drawing
// - SeperatorOn/Off: user-configurable flags controlling separator behavior in clock mode
std::vector<uint8_t> mask;  // 1 = keep underlying pixel, 0 = clear to black
bool enabled = true;
bool sepsOn  = true;
bool SeperatorOn  = true; // force separators on in clock mode
bool SeperatorOff = true; // force separators off in clock mode

// Display mode flags and timing:
// - showClock/showCountdown: which views to render
// - alternatingTime: seconds between views when both are enabled
bool showClock     = true;
bool showCountdown = false;
uint16_t alternatingTime = 10; // seconds

// Countdown target (local time) and derived UNIX timestamp used for math.
int     targetYear   = 2026;
uint8_t targetMonth  = 1;   // 1-12
uint8_t targetDay    = 1;   // 1-31
uint8_t targetHour   = 0;   // 0-23
uint8_t targetMinute = 0;   // 0-59
time_t  targetUnix   = 0;

// Remaining time parts and totals (updated each draw):
// - remX: time parts modulo their units
// - fullX: monotonically increasing totals (minutes, seconds, etc.)
uint32_t fullHours   = 0;   // up to 8760
uint32_t fullMinutes = 0;   // up to 525600
uint32_t fullSeconds = 0;   // up to 31536000
uint32_t remDays     = 0;
uint8_t  remHours    = 0;
uint8_t  remMinutes  = 0;
uint8_t  remSeconds  = 0;

// Second-boundary tracking to compute smooth hundredths display.
unsigned long lastSecondMillis = 0;
int lastSecondValue = -1;

// Index helpers into the linear LED stream for each digit and separator block.
static uint16_t digitBase(uint8_t d) {
  switch (d) {
    case 0: return 0;   // digit 1
    case 1: return 35;  // digit 2
    case 2: return 80;  // digit 3
    case 3: return 115; // digit 4
    case 4: return 160; // digit 5
    case 5: return 195; // digit 6
    default: return 0;
  }
}
static constexpr uint16_t sep1Base() { return 70; }
static constexpr uint16_t sep2Base() { return 150; }

// Letter-to-segment mapping for a 7-seg display.
// Returns a bitmask A..G (0..6). If the character is unsupported, fallback lights A, D, G.
uint8_t LETTER_MASK(char c) {
  switch (c) {
    // Uppercase
    case 'A': return 0b01110111; // A,B,C,E,F,G
    case 'B': return 0b01111100; // b-like (C,D,E,F,G)
    case 'C': return 0b00111001; // A,D,E,F
    case 'D': return 0b01011110; // d-like (B,C,D,E,G)
    case 'E': return 0b01111001; // A,D,E,F,G
    case 'F': return 0b01110001; // A,E,F,G
    case 'H': return 0b01110110; // B,C,E,F,G
    case 'J': return 0b00011110; // B,C,D
    case 'L': return 0b00111000; // D,E,F
    case 'O': return 0b00111111; // A,B,C,D,E,F (like '0')
    case 'P': return 0b01110011; // A,B,E,F,G
    case 'T': return 0b01111000; // D,E,F,G
    case 'U': return 0b00111110; // B,C,D,E,F
    case 'Y': return 0b01101110; // B,C,D,F,G

    // Lowercase
    case 'a': return 0b01011111; // A,B,C,D,E,G
    case 'b': return 0b01111100; // C,D,E,F,G
    case 'c': return 0b01011000; // D,E,G
    case 'd': return 0b01011110; // B,C,D,E,G
    case 'e': return 0b01111011; // A,D,E,F,G (with C off)
    case 'f': return 0b01110001; // A,E,F,G
    case 'h': return 0b01110100; // C,E,F,G
    case 'j': return 0b00001110; // B,C,D
    case 'l': return 0b00110000; // E,F
    case 'n': return 0b01010100; // C,E,G
    case 'o': return 0b01011100; // C,D,E,G
    case 'r': return 0b01010000; // E,G
    case 't': return 0b01111000; // D,E,F,G
    case 'u': return 0b00011100; // C,D,E
    case 'y': return 0b01101110; // B,C,D,F,G

    // Symbols commonly used on 7-seg
    case '-': return 0b01000000; // G
    case '_': return 0b00001000; // D
    case ' ': return 0b00000000; // blank

    default:  return 0b01001001; // fallback: A, D, G
  }
}
