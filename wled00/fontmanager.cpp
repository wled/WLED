/*
 * FontManager class for managing wbf format font loading, caching, and rendering
 * Supports up to 5 fonts (0-4) from flash or file system
 * Caches glyphs in segment data for fast access during rendering
 * Note: fontmanager relies on the Segment class for storing cached font data as well as rendering
 *
 * (c) 2026 by @dedehai
 */

#include "wled.h"
#include "fontmanager.h"

#include "src/font/font_tom_thumb_6px.h"
#include "src/font/font_TinyUnicode_8px.h"
#include "src/font/font_5x12.h"
#include "src/font/console_font_6x8.h"
#include "src/font/c64esque_9px.h"

// get metadata pointer
SegmentFontMetadata* FontManager::getMetadata() {
  return (SegmentFontMetadata*)_segment->data;
}

void FontManager::updateFontBase() {
  SegmentFontMetadata* meta = getMetadata();
  // font data (header + glyph bitmaps) starts after metadata + registry
  _fontBase = _segment->data + sizeof(SegmentFontMetadata) + (meta->glyphCount * sizeof(GlyphEntry));
}

// scan file system for .wbf font files, if scanAll is set, also updates availableFonts
void FontManager::getFontFileName(uint8_t fontNum, char* buffer, bool scanAll) {
  SegmentFontMetadata* meta = getMetadata();
  if (scanAll) {
    if (!meta) return;
    meta->availableFonts = 0; // reset available fonts before scanning
  }
  buffer[0] = '\0'; // invalidate
  #ifdef CONFIG_IDF_TARGET_ESP32C3
    while (!BusManager::canAllShow()) yield(); // accessing FS causes glitches due to RMT issue on C3 TODO: remove this when fixed
  #endif
  File rootdir = WLED_FS.open("/", "r");
  File rootfile = rootdir.openNextFile();
  uint8_t i = 0;
  while (rootfile && i < MAX_FONTS) {
    String name = rootfile.name();
    if (name.endsWith(F(".wbf"))) {
      if (i == fontNum) {
        if (name.charAt(0) != '/') name = "/" + name; // need to add leading slash
        strncpy(buffer, name.c_str(), FONT_NAME_BUFFER_SIZE - 1);
        buffer[FONT_NAME_BUFFER_SIZE - 1] = '\0';
        if (!scanAll) {
          rootfile.close();
          rootdir.close();
          return;
        }
      }
      if (scanAll)
        meta->availableFonts |= (1 << i); // note: openNextFile() usually opens them in alphaetical order but there is no guarantee
      i++;
    }
    rootfile = rootdir.openNextFile();
  }
  rootfile.close();
  rootdir.close();
}

// scan file system for available fonts
void FontManager::scanAvailableFonts() {
  char buffer[FONT_NAME_BUFFER_SIZE];
  getFontFileName(0, buffer, true); // scan all fonts to update availableFonts in metadata
}

// load font by number and prepare/validate font cache, must be called before using any other FontManager functions, must not use the font if this function returns false!
bool FontManager::loadFont(uint8_t fontNum, const char* text, bool useFile) {
  _segment->allocateData(sizeof(SegmentFontMetadata)); // make sure at least metadata is available, sets to 0 if segment.call==0, does nothing if already allocated
  SegmentFontMetadata* meta = getMetadata();
  if (!meta)
    return false; // can not continue if no segment data

  _fontNum = fontNum; // store font to be used
  _useFileFont = useFile;
  uint8_t fontRequested = fontNum | (useFile ? 0x80 : 0x00);

  if (useFile) {
    if (meta->lastFontNum != fontRequested) {
      scanAvailableFonts(); // scan filesystem again if file font changes
    }
    // determine which font to actually use (with fallback): check if requested font is available - find first available font if not
    if (!(meta->availableFonts & (1 << fontNum))) {
      _fontNum = 0xFF; // invalidate
      for (int i = 0; i < MAX_FONTS; i++) {
        if (meta->availableFonts & (1 << i)) {
          _fontNum = i;
          break;
        }
      }
      if (_fontNum == 0xFF) {
        _fontNum = fontNum; // no custom fonts available, use flash font
        _useFileFont = false;
      }
    }
  }
  meta->lastFontNum = fontRequested; // store last requested file font (only used to check file fonts)

  uint8_t fontToUse = _fontNum | (_useFileFont ? 0x80 : 0x00); // highest bit indicates file vs flash
  if (fontToUse != meta->cachedFontNum) {
    meta->glyphCount = 0; // invalidate cache
  }
  cacheGlyphs(text); // prepare cache with needed glyphs
  meta = getMetadata(); // reload metadata after potential cache rebuild
  if (meta->glyphCount == 0) {
    errorFlag = ERR_NOT_IMPL; // TODO: need a better error code if more codes are added
    return false; // cache build failed (invalid font file)
  }
  return true;
}

// check if all glyphs needed for the given text are present in cache, rebuild cache if not
void FontManager::cacheGlyphs(const char* text) {
  if (!text) return;

  SegmentFontMetadata* meta = getMetadata(); // loadFont ensures pointer is valid
  // if glyphCount is 0, cache is empty - rebuild
  if (meta->glyphCount == 0) {
    rebuildCache(text);
    return; // cache built, we are done
  }

  // if there is a cached font, update the pointers
  updateFontBase();
  FontHeader* hdr = reinterpret_cast<FontHeader*>(_fontBase);

  // check if all needed glyphs for the text are present in cache
  uint8_t neededCodes[MAX_CACHED_GLYPHS];
  uint8_t neededCount = collectNeededCodes(text, hdr, neededCodes);

  GlyphEntry* registry = (GlyphEntry*)(_segment->data + sizeof(SegmentFontMetadata));
  for (uint8_t k = 0; k < neededCount; k++) {
    // look up glyph in registry
    bool found = false;
    for (uint8_t j = 0; j < meta->glyphCount; j++) {
      if (registry[j].code == neededCodes[k]) {
        found = true;
        break;
      }
    }

    if (!found) {
      rebuildCache(text); // missing glyph - rebuild cache
      return;
    }
  }
}

void FontManager::rebuildCache(const char* text) {
  if (!text) return;
  // preserve metadata (function is only called if segment data is allocated so no null check needed)
  SegmentFontMetadata savedMeta;
  SegmentFontMetadata* meta = getMetadata();
  meta->glyphCount = 0; // invalidates cached font
  memcpy(&savedMeta, meta, sizeof(SegmentFontMetadata));

  File file;
  if (_useFileFont) {
    // build filename from font number
    char fileName[FONT_NAME_BUFFER_SIZE];
    getFontFileName(_fontNum, fileName);

    #ifdef CONFIG_IDF_TARGET_ESP32C3
    while (!BusManager::canAllShow()) yield(); // accessing FS causes glitches due to RMT issue on C3 TODO: remove this when fixed
    #endif
    file = WLED_FS.open(fileName, "r");

    // fallback logic - try other available fonts
    if (!file) {
      for (int i = 0; i < MAX_FONTS; i++) {
        if (meta->availableFonts & (1 << i)) {
          getFontFileName(i, fileName);
          file = WLED_FS.open(fileName, "r");
          if (file) {
            _fontNum = i; // update to fallback font
            //savedMeta.cachedFontNum = i | 0x80; // set highest bit to indicate file font
            break;
          }
        }
      }
    }

    if (!file) {
      _useFileFont = false; // fallback straight to flash font
      //savedMeta.cachedFontNum = _fontNum;
    }
  }

  savedMeta.cachedFontNum = _fontNum | (_useFileFont ? 0x80 : 0x00); // set highest bit to indicate file font

  // determine flash font to use (not used if using file font)
  const uint8_t* flashFont;
  switch (_fontNum) {
    default:
    case 0: flashFont = font_tom_thumb_6px;    break;
    case 1: flashFont = font_TinyUnicode_8px;  break;
    case 2: flashFont = console_font_6x8;      break;
    case 3: flashFont = c64esque_9px;          break;
    case 4: flashFont = font_5x12;             break;
  }

  // read wbf font header
  FontHeader hdr;
  if (file) {
    if (file.read((uint8_t*)&hdr, FONT_HEADER_SIZE) != FONT_HEADER_SIZE) { file.close(); return; } // header incomplete
    if (hdr.magic != 0x57) { file.close(); return; } // invalid header
    if (hdr.last < hdr.first) { file.close(); return; } // Invalid header
  } else {
    memcpy_P(&hdr, flashFont, FONT_HEADER_SIZE); // assumes built in fonts are in a valid, compatible format
  }

  // collect needed glyphs
  uint8_t neededCodes[MAX_CACHED_GLYPHS];
  uint8_t neededCount = collectNeededCodes(text, &hdr, neededCodes);
  uint32_t numGlyphs = hdr.last - hdr.first + 1;
  uint8_t widthTable[numGlyphs];

  // read width table
  if (hdr.flags & 0x01) {
    if (file) {
      if (file.read(widthTable, numGlyphs) != numGlyphs) { file.close(); return; } // width table starts directly after header if used
    } else {
      memcpy_P(widthTable, flashFont + FONT_HEADER_SIZE, numGlyphs); // assumes built in fonts are in a valid, compatible format
    }
  } else {
    for (uint32_t k = 0; k < numGlyphs; k++) {
      widthTable[k] = hdr.width; // fixed width, fill with given width from header
    }
  }

  // calculate total size for cache: metadata + registry + header + bitmaps
  size_t ramFontSize = sizeof(SegmentFontMetadata) + (neededCount * sizeof(GlyphEntry)) + FONT_HEADER_SIZE;

  for (uint8_t k = 0; k < neededCount; k++) {
    uint8_t code = neededCodes[k];
    if (code < numGlyphs) {
      uint16_t bits = widthTable[code] * hdr.height;
      ramFontSize += (bits + 7) / 8; // add bitmap size for each needed glyph
    }
  }

  if (!_segment->allocateData(ramFontSize)) {
    if (file) file.close();
    return;
  }

  // write metadata
  meta = getMetadata(); // get pointer again in case segment was reallocated
  memcpy(meta, &savedMeta, sizeof(SegmentFontMetadata));
  meta->glyphCount = neededCount; // glyph count is used to determine if cache is valid. If file is corrupted, ram cache is still large enough to not cause crashes

  uint8_t* dataptr = _segment->data + sizeof(SegmentFontMetadata);

  // write registry (GlyphEntry array)
  GlyphEntry* registry = (GlyphEntry*)dataptr;
  for (uint8_t k = 0; k < neededCount; k++) {
    uint8_t code = neededCodes[k];
    if (code >= numGlyphs) continue; // skip invalid codes (safety check if anything is corrupted)
    registry[k].code = code;
    registry[k].width = widthTable[code];
    registry[k].height = hdr.height;
  }
  dataptr += neededCount * sizeof(GlyphEntry);

  // write font header
  memcpy(dataptr, &hdr, FONT_HEADER_SIZE);
  dataptr += FONT_HEADER_SIZE;

  // write bitmap data to cache in registry order
  uint32_t dataStart = FONT_HEADER_SIZE + ((hdr.flags & 0x01) ? numGlyphs : 0); // bitmap data in wbf font starts after header and width table (if used)
  for (uint8_t k = 0; k < neededCount; k++) {
    uint8_t glyphIdx = neededCodes[k]; // neededCodes contais index of the glyph in the font, not the raw unicode value
    uint16_t bits = widthTable[glyphIdx] * hdr.height;
    uint16_t bytes = (bits + 7) / 8;
    // calculate file offset
    uint32_t offset = dataStart;
    for (uint8_t j = 0; j < glyphIdx; j++) {
      uint16_t b = widthTable[j] * hdr.height;
      offset += (b + 7) / 8;
    }
    // read from file or flash
    if (file) {
      file.seek(offset);
      file.read(dataptr, bytes);
    } else {
      memcpy_P(dataptr, flashFont + offset, bytes);
    }
    dataptr += bytes;
  }

  if (file) file.close();
  updateFontBase(); // set pointer to cached header/bitmaps
}

// glyph index calculator
int32_t FontManager::getGlyphIndex(uint32_t unicode, FontHeader* hdr) {
  if (unicode <= LAST_ASCII_CHAR) {
    if (unicode >= hdr->first && unicode <= hdr->last) return unicode - hdr->first;
  } else if (hdr->firstUnicode > 0 && unicode >= hdr->firstUnicode) {
    uint32_t adjusted = unicode - hdr->firstUnicode + LAST_ASCII_CHAR + 1;
    if (adjusted >= hdr->first && adjusted <= hdr->last) return adjusted - hdr->first;
  }
  return -1;
}

// Get glyph width
uint8_t FontManager::getGlyphWidth(uint32_t unicode) {
  FontHeader* hdr = reinterpret_cast<FontHeader*>(_fontBase);
  int32_t idx = getGlyphIndex(unicode, hdr);
  if (idx < 0) return 0;

  SegmentFontMetadata* meta = (SegmentFontMetadata*)_segment->data;
  GlyphEntry* registry = (GlyphEntry*)(_segment->data + sizeof(SegmentFontMetadata));

  for (uint8_t k = 0; k < meta->glyphCount; k++) {
    if (registry[k].code == idx) {
      return registry[k].width;
    }
  }
  return 0; // Not found in cache
}

// Get glyph bitmap
uint8_t* FontManager::getGlyphBitmap(uint32_t unicode, uint8_t& outWidth, uint8_t& outHeight) {
  FontHeader* hdr = reinterpret_cast<FontHeader*>(_fontBase);
  int32_t idx = getGlyphIndex(unicode, hdr);
  if (idx < 0) return nullptr;
  SegmentFontMetadata* meta = (SegmentFontMetadata*)_segment->data;
  GlyphEntry* registry = (GlyphEntry*)(_segment->data + sizeof(SegmentFontMetadata));

  uint32_t bitmapOffset = 0;
  for (uint8_t k = 0; k < meta->glyphCount; k++) {
    if (registry[k].code == idx) {
      outWidth = registry[k].width;
      outHeight = registry[k].height;
      return _fontBase + FONT_HEADER_SIZE + bitmapOffset;
    }
    // Accumulate offset to next glyph
    uint16_t bits = registry[k].width * registry[k].height;
    bitmapOffset += (bits + 7) / 8;
  }
  return nullptr; // Glyph not found in cache
}

uint8_t FontManager::collectNeededCodes(const char* text, FontHeader* hdr, uint8_t* outCodes) {
  uint8_t count = 0;
  // add numbers to cache if needed (for clock use without constant re-caching)
  if (_cacheNumbers) {
    static const char s_nums[] PROGMEM = "0123456789:. ";
    for (const char* p = s_nums; *p && count < MAX_CACHED_GLYPHS; p++) {
      int32_t idx = getGlyphIndex(*p, hdr);
      if (idx >= 0 && idx < 256) {
        outCodes[count++] = idx;
      }
    }
  }
  // parse text
  size_t i = 0, len = strlen(text);
  while (i < len && count < MAX_CACHED_GLYPHS) {
    uint8_t charLen;
    uint32_t unicode = utf8_decode(&text[i], &charLen);
    if (!charLen) break; // invalid input, stop processing
    i += charLen;
    int32_t idx = getGlyphIndex(unicode, hdr);
    if (idx < 0) {
      idx = getGlyphIndex('?', hdr);
    }
    if (idx >= 0 && idx < 256) {
      // add if unique
      bool exists = false;
      for (uint8_t k = 0; k < count; k++) {
        if (outCodes[k] == idx) {
          exists = true;
          break;
        }
      }
      if (!exists) outCodes[count++] = idx;
    }
  }
  return count;
}

void FontManager::drawCharacter(uint32_t unicode, int16_t x, int16_t y, uint32_t color, uint32_t col2, int8_t rotate) {
  uint8_t w, h;
  const uint8_t* bitmap = getGlyphBitmap(unicode, w, h);
  if (!bitmap || w == 0) return;
  CRGBPalette16 grad = col2 ? CRGBPalette16(CRGB(color), CRGB(col2)) : SEGPALETTE;
  uint16_t bitIndex = 0;
  for (int row = 0; row < h; row++) {
    CRGBW c = ColorFromPalette(grad, (row + 1) * 255 / h, 255, LINEARBLEND_NOWRAP);
    for (int col = 0; col < w; col++) {
      uint16_t bytePos = bitIndex >> 3;
      uint8_t bitPos = 7 - (bitIndex & 7);
      uint8_t byteVal = bitmap[bytePos];
      if ((byteVal >> bitPos) & 1) {
        int x0, y0;
        switch (rotate) {
          case -1: x0 = x + row;         y0 = y + col;         break; // 90° CW
          case  1: x0 = x + (h-1) - row; y0 = y + (w-1) - col; break; // 90° CCW
          case -2:
          case  2: x0 = x + (w-1) - col; y0 = y + (h-1) - row; break;
          default: x0 = x + col;         y0 = y + row;         break;
        }
        _segment->setPixelColorXY(x0, y0, c.color32); // bounds checking is done in setPixelColorXY
      }
      bitIndex++;
    }
  }
}
