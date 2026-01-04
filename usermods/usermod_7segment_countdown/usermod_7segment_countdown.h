#pragma once
#include <stdint.h>

// Lookup tables are defined in the .cpp and used by the 7-seg renderer.
extern const uint8_t USERMOD_7SEG_PHYS_TO_LOG[7];   // phys idx -> logical A..G (0..6)
extern const uint8_t USERMOD_7SEG_DIGIT_MASKS[10];  // numeric digit to segment bits

// Panel geometry (LED counts)
static const uint16_t LEDS_PER_SEG   = 5;
static const uint8_t  SEGS_PER_DIGIT = 7;
static const uint16_t SEP_LEDS       = 10;
static constexpr uint16_t LEDS_PER_DIGIT = LEDS_PER_SEG * SEGS_PER_DIGIT; // 35
static constexpr uint16_t TOTAL_PANEL_LEDS = 6 * LEDS_PER_DIGIT + 2 * SEP_LEDS; // 230

// Runtime state used by the implementation
std::vector<uint8_t> mask;  // 1=keep current color/effect, 0=force black
bool enabled = true;
bool sepsOn  = true;
// New UI-configurable separator flags (only affect drawClock())
bool SeperatorOn  = true; // when true, separators are forced on in clock view
bool SeperatorOff = true; // when true, separators are forced off in clock view

// Display mode flags (no logic here, just state)
bool showClock     = true;
bool showCountdown = false;
// Seconds to alternate between clock and countdown when both are enabled
uint16_t alternatingTime = 10; // default 10s

// Countdown target (local time) and derived UNIX timestamp
int     targetYear   = 2026;
uint8_t targetMonth  = 1;   // 1-12
uint8_t targetDay    = 1;   // 1-31
uint8_t targetHour   = 0;   // 0-23
uint8_t targetMinute = 0;   // 0-59
time_t  targetUnix   = 0;

// Remaining time parts and totals
uint32_t fullHours   = 0;   // up to 8760
uint32_t fullMinutes = 0;   // up to 525600
uint32_t fullSeconds = 0;   // up to 31536000
uint32_t remDays     = 0;
uint8_t  remHours    = 0;
uint8_t  remMinutes  = 0;
uint8_t  remSeconds  = 0;

// Timekeeping
unsigned long lastSecondMillis = 0;
int lastSecondValue = -1;


// Index helpers into the linear LED stream
static uint16_t digitBase(uint8_t d) {
  switch (d) {
    case 0: return 0;   // Z1
    case 1: return 35;  // Z2
    case 2: return 80;  // Z3
    case 3: return 115; // Z4
    case 4: return 160; // Z5
    case 5: return 195; // Z6
    default: return 0;
  }
}
static constexpr uint16_t sep1Base() { return 70; }
static constexpr uint16_t sep2Base() { return 150; }

// 7-seg letter helper: returns mask bits A..G (0..6).
// Fallback (unsupported): segments A + D + G.
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

    // Symbols often useful on 7-seg
    case '-': return 0b01000000; // G
    case '_': return 0b00001000; // D
    case ' ': return 0b00000000; // blank

    default:  return 0b01001001; // fallback: A, D, G
  }
}
