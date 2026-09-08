#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WordClockCore {

constexpr uint8_t MAX_PLAN_UNITS = 12;
constexpr uint8_t MAX_MINUTE_DOTS = 4;

// Match behavior requested by a language pack for a display unit.
enum class MatchMode : uint8_t {
  Sequential,        // Continue searching after the previous match.
  RandomOccurrence   // Choose among valid occurrences of this unit.
};

// One language-defined item to place in the character matrix.
struct DisplayUnit {
  uint16_t id;       // Language-pack identifier for a word or symbol.
  MatchMode matchMode; // How the generic matcher should select its occurrence.
  int8_t occurrence;  // Zero-based occurrence to use, or -1 for normal searching.
};

// Fixed-capacity display plan produced for one time value.
struct DisplayPlan {
  DisplayUnit units[MAX_PLAN_UNITS]{}; // Ordered units to place.
  uint8_t count = 0;                   // Number of valid entries in units.

  bool append(uint16_t id, MatchMode matchMode = MatchMode::Sequential, int8_t occurrence = -1) {
    if (count >= MAX_PLAN_UNITS)
      return false;

    units[count++] = {id, matchMode, occurrence};

    return true;
  }
};

// Time values normalized for language-specific plan generation.
struct TimeContext {
  uint8_t hour24;          // Current hour in the range 0-23.
  uint8_t hour12;          // Current hour in the range 1-12.
  uint8_t nextHour12;      // Following hour in the range 1-12.
  uint8_t displayedMinute; // Selected five-minute phrase, from 0 to 55.
  uint8_t minuteDotCount;  // Remainder minutes, from 0 to 4, in dot mode.
  uint16_t totalMinutes;   // Normalized total minutes after nearest-rounding carry.
};

// Physical positions of the optional cumulative minute-dot markers.
struct MinuteDotMarkers {
  uint16_t positions[MAX_MINUTE_DOTS]{}; // Raw layout positions for dots 1-4.
  uint8_t count = 0;                     // Number of markers found: 0 or 4.

  bool enabled() const { return count == MAX_MINUTE_DOTS; }
};

// Build a normalized clock context. Complete dot markers select floor rounding;
// a marker-free layout selects nearest-five-minute rounding.
inline TimeContext makeTimeContext(uint16_t totalMinutes, bool minuteDotsEnabled) {
  totalMinutes %= 1440;
  uint8_t hour24 = totalMinutes / 60;
  uint8_t minute = totalMinutes % 60;
  uint8_t displayedMinute = minute;
  uint8_t minuteDotCount = 0;

  if (minuteDotsEnabled) {
    displayedMinute = (minute / 5) * 5;
    minuteDotCount = minute % 5;
  } else {
    displayedMinute = ((minute + 2) / 5) * 5;

    if (displayedMinute == 60) {
      displayedMinute = 0;
      totalMinutes = ((totalMinutes / 60 + 1) * 60) % 1440;
      hour24 = totalMinutes / 60;
    }
  }

  uint8_t hour12 = hour24 % 12;

  if (hour12 == 0)
    hour12 = 12;

  uint8_t nextHour12 = hour12 == 12 ? 1 : hour12 + 1;

  return {hour24, hour12, nextHour12, displayedMinute, minuteDotCount, totalMinutes};
}

// Return whether a word lies wholly within one configured matrix row.
constexpr bool wordFitsInRow(int position, int length, int rowWidth) {
  return rowWidth > 0 && length > 0 &&
         (position / rowWidth) == ((position + length - 1) / rowWidth);
}

// Convert a logical matrix position to a serpentine physical position. The
// final row is allowed to be shorter than rowWidth.
inline int toMeanderIndex(int logicalIndex, int rowWidth, int matrixLength) {
  if (rowWidth <= 0 || logicalIndex < 0 || logicalIndex >= matrixLength)
    return -1;

  const int row = logicalIndex / rowWidth;
  const int column = logicalIndex % rowWidth;
  const int rowStart = row * rowWidth;
  const int rowLength = (matrixLength - rowStart < rowWidth) ?
                        matrixLength - rowStart : rowWidth;

  return (row % 2 == 0) ? logicalIndex : rowStart + rowLength - 1 - column;
}

// Parse the optional physical minute-dot markers from a byte-oriented layout.
// No markers is valid; otherwise exactly one each of '1', '2', '3', and '4' is
// required. Marker positions remain in the raw layout coordinate system.
inline bool parseMinuteDotMarkers(const char* layout, size_t length, MinuteDotMarkers& result) {
  result = {};
  bool seen[MAX_MINUTE_DOTS] = {};

  for (size_t index = 0; index < length; ++index) {
    if (layout[index] < '1' || layout[index] > '4')
      continue;

    const uint8_t marker = static_cast<uint8_t>(layout[index] - '1');

    if (seen[marker]) {
      result = {};
      return false;
    }

    seen[marker] = true;
    result.positions[marker] = static_cast<uint16_t>(index);
    ++result.count;
  }

  if (result.count != 0 && result.count != MAX_MINUTE_DOTS) {
    result = {};
    return false;
  }

  return true;
}

} // namespace WordClockCore
