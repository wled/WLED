#pragma once

#include <stdint.h>

namespace WordClockGermanReference {

constexpr uint8_t MATRIX_WIDTH = 11;
constexpr uint8_t MATRIX_HEIGHT = 10;
constexpr uint16_t MATRIX_LENGTH = MATRIX_WIDTH * MATRIX_HEIGHT;
constexpr uint8_t MINUTE_DOT_COUNT = 4;

// Logical 11x10 layout reconstructed from the legacy normal-wiring masks.
// Umlauts use single-byte Latin-1 escapes so each physical letter occupies one
// matrix position. Filler characters are intentionally arbitrary.
static constexpr char CHARACTER_MATRIX[] =
  "ESXISTXF\xDC" "NF"
  "ZEHNZWANZIG"
  "DREIVIERTEL"
  "VORXXXXNACH"
  "HALBXELF\xDC" "NF"
  "EINSXXXZWEI"
  "DREIXXXVIER"
  "SECHSXXACHT"
  "SIEBENZW\xD6" "LF"
  "ZEHNEUNXUHR";

static_assert(sizeof(CHARACTER_MATRIX) - 1 == MATRIX_LENGTH, "German matrix must contain 110 positions");

static constexpr uint16_t MINUTE_DOTS[MINUTE_DOT_COUNT] = {110, 111, 112, 113};

struct WordMask {
  const char* word;
  uint16_t start;
  uint8_t length;
};

static constexpr WordMask WORD_MASKS[] = {
  {"ES", 0, 2},
  {"IST", 3, 3},
  {"F\xDC" "NF", 7, 4},
  {"ZEHN", 11, 4},
  {"ZWANZIG", 15, 7},
  {"DREIVIERTEL", 22, 11},
  {"VOR", 33, 3},
  {"NACH", 40, 4},
  {"HALB", 44, 4},
  {"ELF", 49, 3},
  {"EIN", 55, 3},
  {"EINS", 55, 4},
  {"ZWEI", 62, 4},
  {"DREI", 66, 4},
  {"VIER", 73, 4},
  {"SECHS", 77, 5},
  {"ACHT", 84, 4},
  {"SIEBEN", 88, 6},
  {"ZW\xD6" "LF", 94, 5},
  {"NEUN", 102, 4},
  {"UHR", 107, 3}
};

constexpr uint16_t toMeanderIndex(uint16_t logicalIndex) {
  return ((logicalIndex / MATRIX_WIDTH) % 2 == 0)
    ? logicalIndex
    : (logicalIndex / MATRIX_WIDTH) * MATRIX_WIDTH + MATRIX_WIDTH - 1 - (logicalIndex % MATRIX_WIDTH);
}

} // namespace WordClockGermanReference
