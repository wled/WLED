#pragma once

#include "wled.h"
#include <vector>
#include <ctime>

// Generic departure model suitable for SIRI or other sources.
// - Keyed by "agency:stopCode" (string key)
// - Each key holds one current batch of departures
// - Each departure has a LineRef string and an estimated time

struct DepartModel {
  struct Key {
    String agency;
    String stopCode;
    String toString() const { String s(agency); s += ":"; s += stopCode; return s; }
  };

  struct Entry {
    String key; // "agency:stop"
    struct Item { time_t estDep; String lineRef; };
    struct Batch { time_t apiTs; time_t ourTs; std::vector<Item> items; };
    Batch batch; // latest batch
  };

  std::vector<Entry> boards; // small vector; linear search is fine

  void update(std::time_t now, DepartModel&& delta);
  const Entry* find(const String& key) const;
  // Collect unique LineRef values from the most recent batch for a board key
  void currentLinesForBoard(const String& key, std::vector<String>& out) const;

  // Debug: summarize a batch similar to bartdepart (+N minute deltas and times)
  static String describeBatch(const Entry::Batch& batch);

  // Color mapping per agency:LineRef → RGB (0xRRGGBB)
  static bool getColorRGB(const String& agency, const String& lineRef, uint32_t& rgbOut);
  static void setColorRGB(const String& agency, const String& lineRef, uint32_t rgb);
  struct ColorEntry { String key; uint32_t rgb; };
  static const std::vector<ColorEntry>& colorMap();
  static void clearColorMap();
  static bool removeColorKey(const String& agency, const String& lineRef);
  static bool removeColorKeyByKey(const String& key);
  static bool parseColorString(const String& s, uint32_t& rgbOut);
  static String colorToString(uint32_t rgb);
};
