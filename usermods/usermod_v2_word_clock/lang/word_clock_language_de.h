#pragma once

#include "../word_clock_core.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifdef ARDUINO
#include <pgmspace.h>
#endif

namespace WordClockGerman {

// Logical 11x10 layout reconstructed from the legacy normal-wiring masks.
// Umlauts use single-byte Latin-1 escapes so each physical letter occupies one
// matrix position. The final four positions are the optional minute dots.
static const char DEFAULT_CHARACTER_MATRIX[] PROGMEM =
  "ESXISTXF\xDC" "NF"
  "ZEHNZWANZIG"
  "DREIVIERTEL"
  "VORXXXXNACH"
  "HALBXELF\xDC" "NF"
  "EINSXXXZWEI"
  "DREIXXXVIER"
  "SECHSXXACHT"
  "SIEBENZW\xD6" "LF"
  "ZEHNEUNXUHR"
  "1234";

constexpr uint8_t DEFAULT_CHARACTER_MATRIX_WIDTH = 11;
constexpr uint8_t MINUTE_DOT_COUNT = 4;
constexpr uint16_t DEFAULT_CHARACTER_MATRIX_LENGTH = sizeof(DEFAULT_CHARACTER_MATRIX) - 1;
constexpr uint16_t LETTER_MATRIX_LENGTH = DEFAULT_CHARACTER_MATRIX_LENGTH - MINUTE_DOT_COUNT;
constexpr uint8_t MATRIX_HEIGHT = LETTER_MATRIX_LENGTH / DEFAULT_CHARACTER_MATRIX_WIDTH;

enum class WordId : uint16_t {
  It,
  Is,
  Five,
  Ten,
  Quarter,
  Past,
  To,
  Half,
  Three,
  Four,
  One,
  Two,
  Six,
  Seven,
  Eight,
  Nine,
  Eleven,
  Twelve,
  Hour,
  Twenty,
  ThreeQuarter,
  OnePlural
};

static const char WORD_IT[] PROGMEM = "ES";
static const char WORD_IS[] PROGMEM = "IST";
static const char WORD_FIVE[] PROGMEM = "F\xDC" "NF";
static const char WORD_TEN[] PROGMEM = "ZEHN";
static const char WORD_QUARTER[] PROGMEM = "VIERTEL";
static const char WORD_PAST[] PROGMEM = "NACH";
static const char WORD_TO[] PROGMEM = "VOR";
static const char WORD_HALF[] PROGMEM = "HALB";
static const char WORD_THREE[] PROGMEM = "DREI";
static const char WORD_FOUR[] PROGMEM = "VIER";
static const char WORD_ONE[] PROGMEM = "EIN";
static const char WORD_TWO[] PROGMEM = "ZWEI";
static const char WORD_SIX[] PROGMEM = "SECHS";
static const char WORD_SEVEN[] PROGMEM = "SIEBEN";
static const char WORD_EIGHT[] PROGMEM = "ACHT";
static const char WORD_NINE[] PROGMEM = "NEUN";
static const char WORD_ELEVEN[] PROGMEM = "ELF";
static const char WORD_TWELVE[] PROGMEM = "ZW\xD6" "LF";
static const char WORD_HOUR[] PROGMEM = "UHR";
static const char WORD_TWENTY[] PROGMEM = "ZWANZIG";
static const char WORD_THREE_QUARTER[] PROGMEM = "DREIVIERTEL";
static const char WORD_ONE_PLURAL[] PROGMEM = "EINS";

// Return the flash-resident text represented by a language token.
// @param id language token to resolve
// @return pointer to the token text, or nullptr for an invalid token
inline const char* wordText(WordId id) {
  switch (id) {
    case WordId::It: return WORD_IT;
    case WordId::Is: return WORD_IS;
    case WordId::Five: return WORD_FIVE;
    case WordId::Ten: return WORD_TEN;
    case WordId::Quarter: return WORD_QUARTER;
    case WordId::Past: return WORD_PAST;
    case WordId::To: return WORD_TO;
    case WordId::Half: return WORD_HALF;
    case WordId::Three: return WORD_THREE;
    case WordId::Four: return WORD_FOUR;
    case WordId::One: return WORD_ONE;
    case WordId::Two: return WORD_TWO;
    case WordId::Six: return WORD_SIX;
    case WordId::Seven: return WORD_SEVEN;
    case WordId::Eight: return WORD_EIGHT;
    case WordId::Nine: return WORD_NINE;
    case WordId::Eleven: return WORD_ELEVEN;
    case WordId::Twelve: return WORD_TWELVE;
    case WordId::Hour: return WORD_HOUR;
    case WordId::Twenty: return WORD_TWENTY;
    case WordId::ThreeQuarter: return WORD_THREE_QUARTER;
    case WordId::OnePlural: return WORD_ONE_PLURAL;
  }
  return nullptr;
}

// Select the word for an hour, including EIN/EINS grammar.
// @param hour hour in the range 1-12
// @param exactHour use EIN for an exact-hour phrase; otherwise use EINS
// @return language token for the selected hour
inline WordId hourWord(uint8_t hour, bool exactHour) {
  if (hour == 1)
    return exactHour ? WordId::One : WordId::OnePlural;

  switch (hour) {
    case 2: return WordId::Two;
    case 3: return WordId::Three;
    case 4: return WordId::Four;
    case 5: return WordId::Five;
    case 6: return WordId::Six;
    case 7: return WordId::Seven;
    case 8: return WordId::Eight;
    case 9: return WordId::Nine;
    case 10: return WordId::Ten;
    case 11: return WordId::Eleven;
    default: return WordId::Twelve;
  }
}

// Append a token to a fixed-capacity display plan.
// @param plan destination plan
// @param id token to append
// @param occurrence zero-based matrix occurrence, or -1 for sequential search
// @return false when the plan has reached its capacity
inline bool append(WordClockCore::DisplayPlan& plan, WordId id, int8_t occurrence = -1) {
  return plan.append(static_cast<uint16_t>(id), WordClockCore::MatchMode::Sequential, occurrence);
}

// Describe which matrix occurrence is used for an ambiguous hour word.
// @param id hour token
// @param exactHour whether the phrase is an exact-hour phrase
// @return zero-based occurrence, or -1 for normal sequential matching
inline int8_t hourOccurrence(WordId id, bool exactHour) {
  if (id == WordId::Three || id == WordId::Four)
    return 1;

  if (exactHour && (id == WordId::Five || id == WordId::Ten))
    return 1;
  return -1;
}

// Append an hour token with the German-specific occurrence rule attached.
// @param plan destination plan
// @param id hour token
// @param exactHour whether the phrase is an exact-hour phrase
inline bool appendHour(WordClockCore::DisplayPlan& plan, WordId id, bool exactHour) {
  return append(plan, id, hourOccurrence(id, exactHour));
}

// Build the phrase plan for a normalized time.
// @param time rounded time context supplied by the shared core
// @param displayItIs include the optional ES IST prefix
// @param nord use VIERTEL NACH/VIERTEL VOR instead of the default quarter forms
// @param plan output sequence of tokens and occurrence metadata
// @return false if the fixed-size plan cannot hold the phrase
inline bool buildPlan(const WordClockCore::TimeContext& time, bool displayItIs,
                      bool nord, WordClockCore::DisplayPlan& plan) {
  plan = {};

  if (displayItIs && (!append(plan, WordId::It) || !append(plan, WordId::Is)))
    return false;

  const uint8_t minute = time.displayedMinute;
  const WordId currentHour = hourWord(time.hour12, minute == 0);
  const WordId nextHour = hourWord(time.nextHour12, false);

  switch (minute) {
    case 0:
      return appendHour(plan, currentHour, true) && append(plan, WordId::Hour);
    case 5:
      return append(plan, WordId::Five) && append(plan, WordId::Past) && appendHour(plan, currentHour, false);
    case 10:
      return append(plan, WordId::Ten) && append(plan, WordId::Past) && appendHour(plan, currentHour, false);
    case 15:
      if (nord)
        return append(plan, WordId::Quarter) && append(plan, WordId::Past) && appendHour(plan, currentHour, false);
      return append(plan, WordId::Quarter) && appendHour(plan, nextHour, false);
    case 20:
      return append(plan, WordId::Twenty) && append(plan, WordId::Past) && appendHour(plan, currentHour, false);
    case 25:
      return append(plan, WordId::Five) && append(plan, WordId::To) && append(plan, WordId::Half) && appendHour(plan, nextHour, false);
    case 30:
      return append(plan, WordId::Half) && appendHour(plan, nextHour, false);
    case 35:
      return append(plan, WordId::Five) && append(plan, WordId::Past) && append(plan, WordId::Half) && appendHour(plan, nextHour, false);
    case 40:
      return append(plan, WordId::Twenty) && append(plan, WordId::To) && appendHour(plan, nextHour, false);
    case 45:
      if (nord)
        return append(plan, WordId::Quarter) && append(plan, WordId::To) && appendHour(plan, nextHour, false);
      return append(plan, WordId::ThreeQuarter) && appendHour(plan, nextHour, false);
    case 50:
      return append(plan, WordId::Ten) && append(plan, WordId::To) && appendHour(plan, nextHour, false);
    case 55:
      return append(plan, WordId::Five) && append(plan, WordId::To) && appendHour(plan, nextHour, false);
    default:
      return false;
  }
}

static_assert(LETTER_MATRIX_LENGTH % DEFAULT_CHARACTER_MATRIX_WIDTH == 0,
              "Letter matrix must contain complete rows");

// Return the length of a flash-resident word on the target platform.
// @param word PROGMEM word pointer
// @return word length in bytes
inline size_t wordLength(const char* word) {
#ifdef ARDUINO
  return strlen_P(word);
#else
  return strlen(word);
#endif
}

// Compare a language word with the default matrix at a logical position.
// @param position zero-based matrix position
// @param word flash-resident word to compare
// @param length number of bytes to compare
// @return true when the matrix contains the word at position
inline bool wordMatchesAt(int position, const char* word, size_t length) {
#ifdef ARDUINO
  return strncmp_P(DEFAULT_CHARACTER_MATRIX + position, word, length) == 0;
#else
  return strncmp(DEFAULT_CHARACTER_MATRIX + position, word, length) == 0;
#endif
}

// Find the first row-contained occurrence at or after a logical position.
// @param word flash-resident word to find
// @param searchFrom zero-based position where searching begins
// @return logical matrix position, or -1 when no occurrence fits
inline int findWord(const char* word, int searchFrom) {
  const size_t length = wordLength(word);

  for (int position = searchFrom; position + static_cast<int>(length) <= LETTER_MATRIX_LENGTH; ++position) {
    if (WordClockCore::wordFitsInRow(position, static_cast<int>(length), DEFAULT_CHARACTER_MATRIX_WIDTH) &&
        wordMatchesAt(position, word, length))
      return position;
  }

  return -1;
}

// Find a specific row-contained occurrence by scanning the default matrix.
// @param word flash-resident word to find
// @param occurrence zero-based occurrence number
// @return logical matrix position, or -1 when that occurrence does not exist
inline int findWordOccurrence(const char* word, int occurrence) {
  const size_t length = wordLength(word);
  int seen = 0;

  for (int position = 0; position + static_cast<int>(length) <= LETTER_MATRIX_LENGTH; ++position) {
    if (WordClockCore::wordFitsInRow(position, static_cast<int>(length), DEFAULT_CHARACTER_MATRIX_WIDTH) &&
        wordMatchesAt(position, word, length) && seen++ == occurrence)
      return position;
  }

  return -1;
}

// Place a display plan into a logical/physical LED mask. The final four
// default-matrix positions are cumulative minute dots; all other units are
// matched against the letter rows and optionally converted to meander wiring.
// @param time normalized time, including the minute-dot count
// @param plan token plan to place
// @param meander reverse odd zero-based rows for physical wiring
// @param ledMask destination mask containing letters followed by dots
// @param maskLength number of entries available in ledMask
// @return false for invalid words, capacity, or out-of-range mappings
inline bool placePlan(const WordClockCore::TimeContext& time,
                      const WordClockCore::DisplayPlan& plan, bool meander,
                      bool* ledMask, size_t maskLength) {
  if (ledMask == nullptr || maskLength < DEFAULT_CHARACTER_MATRIX_LENGTH)
    return false;

  memset(ledMask, 0, maskLength * sizeof(bool));
  for (uint8_t dot = 0; dot < time.minuteDotCount; ++dot)
    ledMask[LETTER_MATRIX_LENGTH + dot] = true;

  int searchFrom = 0;

  for (uint8_t unitIndex = 0; unitIndex < plan.count; ++unitIndex) {
    const char* word = wordText(static_cast<WordId>(plan.units[unitIndex].id));

    if (word == nullptr || plan.units[unitIndex].matchMode != WordClockCore::MatchMode::Sequential)
      return false;

    int position = -1;

    if (plan.units[unitIndex].occurrence >= 0) {
      position = findWordOccurrence(word, plan.units[unitIndex].occurrence);
    } else {
      position = findWord(word, searchFrom);
    }

    if (position < 0)
      return false;

    const int length = static_cast<int>(wordLength(word));

    for (int offset = 0; offset < length; ++offset) {
      int ledIndex = position + offset;

      if (meander)
        ledIndex = WordClockCore::toMeanderIndex(ledIndex, DEFAULT_CHARACTER_MATRIX_WIDTH, LETTER_MATRIX_LENGTH);

      if (ledIndex < 0 || static_cast<size_t>(ledIndex) >= maskLength)
        return false;

      ledMask[ledIndex] = true;
    }

    searchFrom = position + length;
  }

  return true;
}

} // namespace WordClockGerman
