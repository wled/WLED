#pragma once
#include <cstdint>

#define PROGMEM

/// Font data stored PER GLYPH
typedef struct {
  uint16_t bitmapOffset; ///< Pointer into GFXfont->bitmap
  uint8_t width;         ///< Bitmap dimensions in pixels
  uint8_t height;        ///< Bitmap dimensions in pixels
  uint8_t xAdvance;      ///< Distance to advance cursor (x axis)
  int8_t xOffset;        ///< X dist from cursor pos to UL corner
  int8_t yOffset;        ///< Y dist from cursor pos to UL corner
} GFXglyph;

typedef struct {
  uint8_t *bitmap;  ///< Glyph bitmaps, concatenated
  GFXglyph *glyph;  ///< Glyph array
  uint16_t first;   ///< ASCII extents (first char)
  uint16_t last;    ///< ASCII extents (last char)
  uint8_t yAdvance; ///< Newline distance (y axis)
} GFXfont;

enum EFontType
{
    FT_UNKNOWN,
    FT_35,
    FT_36,
    FT_38,
    FT_47,
};

enum ETextAlign
{
    TA_UNKNOWN = 0x00,
    TA_HCENTER = 0x01,
    TA_VCENTER = 0x02,
    TA_MIDTEXT = 0x04,
    TA_END = 0x08, // Align to the right/end side
};