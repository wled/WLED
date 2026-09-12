#pragma once

#include "../word_clock_core.h"

#include <stdint.h>

namespace WordClockDutch {

/*
 * This pack uses the shared byte-matrix placement primitives. A future
 * language with a different writing system may replace placePlan() while
 * keeping the same buildPlan() and display-plan concepts.
 */

// Default byte-oriented Latin-script matrix used when no user-configured
// matrix is available. A future non-Latin pack should use symbol IDs instead.
static const char DEFAULT_CHARACTER_MATRIX[] PROGMEM =
  "NEUEHETHETHT"
  "NFEYIEISISVT"
  "VIJFKWARTNAA"
  "AGBETIENOAEE"
  "AINOVERVOORA"
  "TFUHALFIEENY"
  "OEHIBUZEVENV"
  "NBNMTWEEELFN"
  "DRIEVIERVIJF"
  "NEGENZESTIEN"
  "TWAALFACHTNT"
  "BXNHWEUUROAD"
  "1234";

constexpr uint8_t DEFAULT_CHARACTER_MATRIX_WIDTH = 12;
constexpr uint8_t MINUTE_DOT_COUNT = 4;
constexpr uint16_t DEFAULT_CHARACTER_MATRIX_LENGTH = sizeof(DEFAULT_CHARACTER_MATRIX) - 1;
constexpr uint8_t DEFAULT_CHARACTER_MATRIX_HEIGHT =
  (DEFAULT_CHARACTER_MATRIX_LENGTH - MINUTE_DOT_COUNT) / DEFAULT_CHARACTER_MATRIX_WIDTH;

static_assert((DEFAULT_CHARACTER_MATRIX_LENGTH - MINUTE_DOT_COUNT) % DEFAULT_CHARACTER_MATRIX_WIDTH == 0,
              "Dutch default matrix must contain complete rows");

enum class WordId : uint16_t {
  It,
  Is,
  One,
  Two,
  Three,
  Four,
  Five,
  Six,
  Seven,
  Eight,
  Nine,
  Ten,
  Eleven,
  Twelve,
  Past,
  To,
  Half,
  Quarter,
  Hour
};

static const char WORD_IT[] PROGMEM = "HET";
static const char WORD_IS[] PROGMEM = "IS";
static const char WORD_ONE[] PROGMEM = "EEN";
static const char WORD_TWO[] PROGMEM = "TWEE";
static const char WORD_THREE[] PROGMEM = "DRIE";
static const char WORD_FOUR[] PROGMEM = "VIER";
static const char WORD_FIVE[] PROGMEM = "VIJF";
static const char WORD_SIX[] PROGMEM = "ZES";
static const char WORD_SEVEN[] PROGMEM = "ZEVEN";
static const char WORD_EIGHT[] PROGMEM = "ACHT";
static const char WORD_NINE[] PROGMEM = "NEGEN";
static const char WORD_TEN[] PROGMEM = "TIEN";
static const char WORD_ELEVEN[] PROGMEM = "ELF";
static const char WORD_TWELVE[] PROGMEM = "TWAALF";
static const char WORD_PAST[] PROGMEM = "OVER";
static const char WORD_TO[] PROGMEM = "VOOR";
static const char WORD_HALF[] PROGMEM = "HALF";
static const char WORD_QUARTER[] PROGMEM = "KWART";
static const char WORD_HOUR[] PROGMEM = "UUR";

/*
 * Return the flash-resident text represented by a language token.
 *
 * @param id language token to resolve
 * @return pointer to the token text, or nullptr for an invalid token
 */
inline const char* wordText(WordId id) {
  switch (id) {
    case WordId::It: return WORD_IT;
    case WordId::Is: return WORD_IS;
    case WordId::One: return WORD_ONE;
    case WordId::Two: return WORD_TWO;
    case WordId::Three: return WORD_THREE;
    case WordId::Four: return WORD_FOUR;
    case WordId::Five: return WORD_FIVE;
    case WordId::Six: return WORD_SIX;
    case WordId::Seven: return WORD_SEVEN;
    case WordId::Eight: return WORD_EIGHT;
    case WordId::Nine: return WORD_NINE;
    case WordId::Ten: return WORD_TEN;
    case WordId::Eleven: return WORD_ELEVEN;
    case WordId::Twelve: return WORD_TWELVE;
    case WordId::Past: return WORD_PAST;
    case WordId::To: return WORD_TO;
    case WordId::Half: return WORD_HALF;
    case WordId::Quarter: return WORD_QUARTER;
    case WordId::Hour: return WORD_HOUR;
  }
  return nullptr;
}

/*
 * Select the word for an hour.
 *
 * @param hour hour in the range 1-12
 * @return language token for the selected hour
 */
inline WordId hourWord(uint8_t hour) {
  switch (hour) {
    case 1: return WordId::One;
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

/*
 * Append a token to a fixed-capacity display plan.
 *
 * @param plan destination plan
 * @param id token to append
 * @param mode sequential or random occurrence matching behavior
 * @return false when the plan has reached its capacity
 */
inline bool append(WordClockCore::DisplayPlan& plan, WordId id,
                   WordClockCore::MatchMode mode = WordClockCore::MatchMode::Sequential) {
  return plan.append(static_cast<uint16_t>(id), mode);
}

/*
 * Build the phrase plan for a normalized time.
 *
 * @param time rounded time context supplied by the shared core
 * @param displayItIs include the optional HET IS prefix
 * @param plan output sequence of tokens and match modes
 * @return false if the fixed-size plan cannot hold the phrase
 */
inline bool buildPlan(const WordClockCore::TimeContext& time, bool displayItIs,
                      WordClockCore::DisplayPlan& plan) {
  plan = {};
  if (displayItIs &&
      (!append(plan, WordId::It, WordClockCore::MatchMode::RandomOccurrence) ||
       !append(plan, WordId::Is, WordClockCore::MatchMode::RandomOccurrence)))
    return false;

  const WordId currentHour = hourWord(time.hour12);
  const WordId nextHour = hourWord(time.nextHour12);

  switch (time.displayedMinute) {
    case 0:
      return append(plan, currentHour) && append(plan, WordId::Hour);
    case 5:
      return append(plan, WordId::Five) && append(plan, WordId::Past) && append(plan, currentHour);
    case 10:
      return append(plan, WordId::Ten) && append(plan, WordId::Past) && append(plan, currentHour);
    case 15:
      return append(plan, WordId::Quarter) && append(plan, WordId::Past) && append(plan, currentHour);
    case 20:
      return append(plan, WordId::Ten) && append(plan, WordId::To) && append(plan, WordId::Half) && append(plan, nextHour);
    case 25:
      return append(plan, WordId::Five) && append(plan, WordId::To) && append(plan, WordId::Half) && append(plan, nextHour);
    case 30:
      return append(plan, WordId::Half) && append(plan, nextHour);
    case 35:
      return append(plan, WordId::Five) && append(plan, WordId::Past) && append(plan, WordId::Half) && append(plan, nextHour);
    case 40:
      return append(plan, WordId::Ten) && append(plan, WordId::Past) && append(plan, WordId::Half) && append(plan, nextHour);
    case 45:
      return append(plan, WordId::Quarter) && append(plan, WordId::To) && append(plan, nextHour);
    case 50:
      return append(plan, WordId::Ten) && append(plan, WordId::To) && append(plan, nextHour);
    case 55:
      return append(plan, WordId::Five) && append(plan, WordId::To) && append(plan, nextHour);
    default:
      return false;
  }
}

/*
 * Return the length of a flash-resident word.
 *
 * @param word PROGMEM word pointer
 * @return word length in bytes
 */
inline int wordLength(const char* word) {
  return static_cast<int>(strlen_P(word));
}

/*
 * Return whether a word fits entirely within one matrix row.
 *
 * @param position zero-based matrix position
 * @param length word length in bytes
 * @param rowWidth configured matrix width
 * @param selectionSeed stable seed for selecting repeated word occurrences
 * @return true when the word does not cross a row boundary
 */
inline bool wordFitsInRow(int position, int length, int rowWidth) {
  return WordClockCore::wordFitsInRow(position, length, rowWidth);
}

/*
 * Find the first row-contained occurrence at or after a logical position.
 *
 * @param matrix user-configured character matrix
 * @param word flash-resident word to find
 * @param searchFrom zero-based position where searching begins
 * @param rowWidth configured matrix width
 * @return logical matrix position, or -1 when no occurrence fits
 */
inline int findWord(const String& matrix, const char* word, int searchFrom, int rowWidth) {
  const String target = FPSTR(word);
  const int length = target.length();
  for (int position = searchFrom; position + length <= matrix.length(); ++position) {
    if (wordFitsInRow(position, length, rowWidth) && matrix.substring(position, position + length).equals(target))
      return position;
  }
  return -1;
}

/*
 * Select a stable valid occurrence of a repeated word in the matrix.
 *
 * @param matrix user-configured character matrix
 * @param word flash-resident word to find
 * @param rowWidth configured matrix width
 * @return selected logical position, or -1 when no occurrence fits
 */
inline int findRandomWord(const String& matrix, const char* word, int rowWidth,
                          uint16_t selectionSeed) {
  const String target = FPSTR(word);
  const int length = target.length();
  int count = 0;
  for (int position = 0; position + length <= matrix.length(); ++position)
    if (wordFitsInRow(position, length, rowWidth) && matrix.substring(position, position + length).equals(target))
      ++count;

  if (count == 0)
    return -1;

  int selected = count > 1 ? selectionSeed % count : 0;
  for (int position = 0; position + length <= matrix.length(); ++position) {
    if (wordFitsInRow(position, length, rowWidth) && matrix.substring(position, position + length).equals(target) && selected-- == 0)
      return position;
  }
  return -1;
}

/*
 * Place a display plan into the logical/physical LED mask.
 *
 * @param time normalized time including the cumulative minute-dot count
 * @param plan token plan to place
 * @param matrix user-configured character matrix
 * @param rowWidth configured matrix width
 * @param meander reverse odd zero-based rows for physical wiring
 * @param ledMask destination mask with one entry per matrix position
 * @return false for invalid words or out-of-range mappings
 */
inline bool placePlan(const WordClockCore::TimeContext& time,
                      const WordClockCore::DisplayPlan& plan, const String& matrix,
                      int rowWidth, bool meander, bool* ledMask,
                      uint16_t selectionSeed) {
  if (ledMask == nullptr)
    return false;

  memset(ledMask, 0, matrix.length() * sizeof(bool));
  WordClockCore::MinuteDotMarkers markers;
  if (!WordClockCore::parseMinuteDotMarkers(matrix.c_str(), matrix.length(), markers))
    return false;

  if (markers.enabled()) {
    for (uint8_t dot = 0; dot < time.minuteDotCount; ++dot)
      if (markers.positions[dot] < matrix.length())
        ledMask[markers.positions[dot]] = true;
  }

  int searchFrom = 0;
  for (uint8_t unitIndex = 0; unitIndex < plan.count; ++unitIndex) {
    const char* word = wordText(static_cast<WordId>(plan.units[unitIndex].id));
    int wordIndex = plan.units[unitIndex].matchMode == WordClockCore::MatchMode::RandomOccurrence
      ? findRandomWord(matrix, word, rowWidth, selectionSeed + plan.units[unitIndex].id)
      : findWord(matrix, word, searchFrom, rowWidth);
    if (wordIndex < 0)
      return false;

    const int length = wordLength(word);
    for (int offset = 0; offset < length; ++offset) {
      int ledIndex = wordIndex + offset;
      if (meander)
        ledIndex = WordClockCore::toMeanderIndex(ledIndex, rowWidth, matrix.length());
      if (ledIndex < 0 || ledIndex >= matrix.length())
        return false;
      ledMask[ledIndex] = true;
    }

    if (plan.units[unitIndex].matchMode == WordClockCore::MatchMode::Sequential)
      searchFrom = wordIndex + length;
  }
  return true;
}

/*
 * Return the longest word used by the language pack.
 *
 * @return maximum word length in bytes
 */
inline int maxWordLength() {
  int maximum = 0;
  for (uint8_t id = 0; id <= static_cast<uint8_t>(WordId::Hour); ++id)
    maximum = max(maximum, wordLength(wordText(static_cast<WordId>(id))));
  return maximum;
}

} // namespace WordClockDutch
