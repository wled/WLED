/*
  FX_2Dfcn.cpp contains all 2D utility functions
  Parts of the code adapted from WLED Sound Reactive: Copyright (c) 2022 Andrew Tuline, Ewoud Wijma, Harm Aldick
*/
#include "wled.h"
#include "FX.h"
#include "palettes.h"

// setUpMatrix() - constructs ledmap array from matrix of panels with WxH pixels
// this converts physical (possibly irregular) LED arrangement into well defined
// array of logical pixels: fist entry corresponds to left-topmost logical pixel
// followed by horizontal pixels, when Segment::maxWidth logical pixels are added they
// are followed by next row (down) of Segment::maxWidth pixels (and so forth)
// note: matrix may be comprised of multiple panels each with different orientation
// but ledmap takes care of that. ledmap is constructed upon initialization
// so matrix should disable regular ledmap processing
void WS2812FX::setUpMatrix() {
#ifndef WLED_DISABLE_2D
  // isMatrix is set in cfg.cpp or set.cpp
  if (isMatrix) {
    // calculate width dynamically because it will have gaps
    Segment::maxWidth = 1;
    Segment::maxHeight = 1;
    for (size_t i = 0; i < panel.size(); i++) {
      Panel &p = panel[i];
      if (p.xOffset + p.width > Segment::maxWidth) {
        Segment::maxWidth = p.xOffset + p.width;
      }
      if (p.yOffset + p.height > Segment::maxHeight) {
        Segment::maxHeight = p.yOffset + p.height;
      }
    }

    // safety check 
    // WLEDMM no check on Segment::maxWidth * Segment::maxHeight > MAX_LEDS || 
    if (Segment::maxWidth <= 1 || Segment::maxHeight <= 1) {
      DEBUG_PRINTF("2D Bounds error. %d x %d\n", Segment::maxWidth, Segment::maxHeight);
      isMatrix = false;
      Segment::maxWidth = _length;
      Segment::maxHeight = 1;
      panels = 0;
      panel.clear(); // release memory allocated by panels
      resetSegments(true); //WLEDMM bounds only
      return;
    }

    USER_PRINTF("setUpMatrix %d x %d\n", Segment::maxWidth, Segment::maxHeight);
    
    // WLEDMM check if mapping table is necessary (avoiding heap fragmentation)
#if defined(WLED_ENABLE_HUB75MATRIX)
    bool needLedMap = (loadedLedmap >0);              // ledmap loaded
    needLedMap |= WLED_FS.exists(F("/2d-gaps.json")); // gapFile found
    needLedMap |= panel.size() > 1;                   // 2D config: more than one panel
    if (panel.size() == 1) {
      Panel &p = panel[0];
      needLedMap |= p.serpentine;                        // panel serpentine
      needLedMap |= p.vertical;                          // panel not horizotal
      needLedMap |= p.bottomStart | p.rightStart;        // panel not top left, or not left->light
      needLedMap |= (p.xOffset > 0) || (p.yOffset > 0);  // panel does not start at (0,0)
    }
#else
    bool needLedMap = true;                              // always use ledMaps on non-HUB75 builds
#endif

    //WLEDMM recreate customMappingTable if more space needed
    if (Segment::maxWidth * Segment::maxHeight > customMappingTableSize) {
      size_t size = max(ledmapMaxSize, size_t(Segment::maxWidth * Segment::maxHeight)); // TroyHacks
      if (!needLedMap) size = 0;                                                        // softhack007
      USER_PRINTF("setupmatrix customMappingTable alloc %d from %d\n", size, customMappingTableSize);
      //if (customMappingTable != nullptr) delete[] customMappingTable;
      //customMappingTable = new uint16_t[size];

      // don't use new / delete
      if ((size > 0) && (customMappingTable != nullptr)) {  // resize
        customMappingTable = (uint16_t*) reallocf(customMappingTable, sizeof(uint16_t) * size); // reallocf will free memory if it cannot resize
      }
      if ((size > 0) && (customMappingTable == nullptr)) { // second try
        DEBUG_PRINTLN("setUpMatrix: trying to get fresh memory block.");
        customMappingTable = (uint16_t*) calloc(size, sizeof(uint16_t));
        if (customMappingTable == nullptr) { 
          USER_PRINTLN("setUpMatrix: alloc failed");
          errorFlag = ERR_LOW_MEM; // WLEDMM raise errorflag
        }
      }
      if (customMappingTable != nullptr) customMappingTableSize = size;
    }

    if ((customMappingTable != nullptr) || (!needLedMap)) {                                          // softhack007
      customMappingSize = Segment::maxWidth * Segment::maxHeight;
      if (!needLedMap) customMappingSize = 0;                                                        // softhack007

      // fill with empty in case we don't fill the entire matrix
      for (size_t i = 0; i< customMappingTableSize; i++) { //WLEDMM use customMappingTableSize
        customMappingTable[i] = (uint16_t)-1;
      }

      // we will try to load a "gap" array (a JSON file)
      // the array has to have the same amount of values as mapping array (or larger)
      // "gap" array is used while building ledmap (mapping array)
      // and discarded afterwards as it has no meaning after the process
      // content of the file is just raw JSON array in the form of [val1,val2,val3,...]
      // there are no other "key":"value" pairs in it
      // allowed values are: -1 (missing pixel/no LED attached), 0 (inactive/unused pixel), 1 (active/used pixel)
      char    fileName[32]; strcpy_P(fileName, PSTR("/2d-gaps.json")); // reduce flash footprint
      bool    isFile = WLED_FS.exists(fileName);
      size_t  gapSize = 0;
      int8_t *gapTable = nullptr;

      if (isFile && requestJSONBufferLock(20)) {
        USER_PRINT(F("Reading LED gap from "));
        USER_PRINTLN(fileName);
        // read the array into global JSON buffer
        if (readObjectFromFile(fileName, nullptr, &doc)) {
          // the array is similar to ledmap, except it has only 3 values:
          // -1 ... missing pixel (do not increase pixel count)
          //  0 ... inactive pixel (it does count, but should be mapped out (-1))
          //  1 ... active pixel (it will count and will be mapped)
          JsonArray map = doc.as<JsonArray>();
          gapSize = map.size();
          if (!map.isNull() && (gapSize > 0) && gapSize >= customMappingSize) { // not an empty map //softhack also check gapSize>0 
            gapTable = new int8_t[gapSize];
            if (gapTable) for (size_t i = 0; i < gapSize; i++) {
              gapTable[i] = constrain(map[i], -1, 1);
            }
          }
        }
        DEBUG_PRINTLN(F("Gaps loaded."));
        releaseJSONBufferLock();
      }

      if (needLedMap && customMappingTable != nullptr) {  // softhack007
      uint16_t x, y, pix=0; //pixel
      for (size_t pan = 0; pan < panel.size(); pan++) {
        Panel &p = panel[pan];
        uint16_t h = p.vertical ? p.height : p.width;
        uint16_t v = p.vertical ? p.width  : p.height;
        for (size_t j = 0; j < v; j++){
          for(size_t i = 0; i < h; i++) {
            y = (p.vertical?p.rightStart:p.bottomStart) ? v-j-1 : j;
            x = (p.vertical?p.bottomStart:p.rightStart) ? h-i-1 : i;
            x = p.serpentine && j%2 ? h-x-1 : x;
            size_t index = (p.yOffset + (p.vertical?x:y)) * Segment::maxWidth + p.xOffset + (p.vertical?y:x);
            if (!gapTable || (gapTable && gapTable[index] >  0)) customMappingTable[index] = pix; // a useful pixel (otherwise -1 is retained)
            if (!gapTable || (gapTable && gapTable[index] >= 0)) pix++; // not a missing pixel
          }
        }
      }
      }

      // delete gap array as we no longer need it
      if (gapTable) {delete[] gapTable; gapTable=nullptr;}   // softhack prevent dangling pointer

      #ifdef WLED_DEBUG_MAPS
      DEBUG_PRINTF("Matrix ledmap: \n");
      for (uint16_t i=0; i<customMappingSize; i++) {
        if (!(i%Segment::maxWidth)) DEBUG_PRINTLN();
        DEBUG_PRINTF("%4d,", customMappingTable[i]);
      }
      DEBUG_PRINTLN();
      USER_FLUSH();  // wait until serial buffer is written out - to avoid loss/corruption of future debug messages
      #endif
    } else { // memory allocation error
      customMappingTableSize = 0;
      USER_PRINTLN(F("Ledmap alloc error."));
      errorFlag = ERR_LOW_MEM; // WLEDMM raise errorflag
      isMatrix = false; //WLEDMM does not like this done in teh background while end users are confused whats happened...
      panels = 0;
      panel.clear();
      Segment::maxWidth = _length;
      Segment::maxHeight = 1;
      //WLEDMM: no resetSegments here, only do it in set.cpp/handleSettingsSet - as we want t0 maintain the segment settings after setup has changed
    }
  }

#ifdef WLED_ENABLE_HUB75MATRIX
  // softhack007 hack: delete mapping table in case it only contains "identity"
  if (customMappingTable != nullptr && customMappingTableSize > 0) {
    bool isIdentity = true;
    for (size_t i = 0; (i< customMappingSize) && isIdentity; i++) { //WLEDMM use customMappingTableSize
      if (customMappingTable[i] != (uint16_t)i ) isIdentity = false;
    }
    if (isIdentity) {
      free(customMappingTable); customMappingTable = nullptr;      
      USER_PRINTF("!setupmatrix: customMappingTable is not needed. Dropping %d bytes.\n", customMappingTableSize * sizeof(uint16_t));
      customMappingTableSize = 0;
      customMappingSize = 0;
      loadedLedmap = 0; //WLEDMM
    }
  }
#endif

#else
  isMatrix = false; // no matter what config says
#endif
}

// absolute matrix version of setPixelColor(), without error checking
void IRAM_ATTR __attribute__((hot)) WS2812FX::setPixelColorXY_fast(int x, int y, uint32_t col) //WLEDMM: IRAM_ATTR conditionally
{
  uint_fast16_t index = y * Segment::maxWidth + x;
  if (index < customMappingSize) index = customMappingTable[index];
  if (index >= _length) return;
  busses.setPixelColor(index, col);
}

// absolute matrix version of setPixelColor()
void IRAM_ATTR_YN WS2812FX::setPixelColorXY(int x, int y, uint32_t col) //WLEDMM: IRAM_ATTR conditionally
{
#ifndef WLED_DISABLE_2D
  if (!isMatrix) return; // not a matrix set-up
  uint_fast16_t index = y * Segment::maxWidth + x;
#else
  uint16_t index = x;
#endif
  if (index < customMappingSize) index = customMappingTable[index];
  if (index >= _length) return;
  busses.setPixelColor(index, col);
}

// returns RGBW values of pixel
uint32_t __attribute__((hot)) WS2812FX::getPixelColorXY(uint16_t x, uint16_t y) const {
#ifndef WLED_DISABLE_2D
  uint_fast16_t index = (y * Segment::maxWidth + x); //WLEDMM: use fast types
#else
  uint16_t index = x;
#endif
  if (index < customMappingSize) index = customMappingTable[index];
  if (index >= _length) return 0;
  return busses.getPixelColor(index);
}

uint32_t __attribute__((hot)) WS2812FX::getPixelColorXYRestored(uint16_t x, uint16_t y)  const {  // WLEDMM gets the original color from the driver (without downscaling by _bri)
  #ifndef WLED_DISABLE_2D
    uint_fast16_t index = (y * Segment::maxWidth + x); //WLEDMM: use fast types
  #else
    uint16_t index = x;
  #endif
  if (index < customMappingSize) index = customMappingTable[index];
  if (index >= _length) return 0;
  return busses.getPixelColorRestored(index);
}

///////////////////////////////////////////////////////////
// Segment:: routines
///////////////////////////////////////////////////////////

#ifndef WLED_DISABLE_2D

// WLEDMM cache some values so we don't need to re-calc then for each pixel
void Segment::startFrame(void) {
  _isSimpleSegment = (grouping == 1) && (spacing == 0); // we can handle pixels faster when no grouping or spacing is involved
  _isSuperSimpleSegment = !mirror && !mirror_y && (grouping == 1) && (spacing == 0); // fastest - we only draw one pixel per call

#ifdef WLEDMM_FASTPATH
  _isValid2D  = isActive() && is2D();
  _brightness = currentBri(on ? opacity : 0);
  // if (reverse_y) _isSimpleSegment = false; // for A/B testing
  _2dWidth    = is2D() ? calc_virtualWidth() : virtualLength();
  _2dHeight   = calc_virtualHeight();
  #if 0 && defined(WLED_ENABLE_HUB75MATRIX)
    _firstFill = true; // dirty HACK
  #endif
#endif
}
// WLEDMM end

// XY(x,y) - gets pixel index within current segment (often used to reference leds[] array element)
// WLEDMM Segment::XY()is declared inline, see FX.h


// Simplified version of Segment::setPixelColorXY - without error checking. Does not support grouping or spacing
// * expects scaled color (final brightness) as additional input parameter, plus segment  virtualWidth() and virtualHeight()
void IRAM_ATTR __attribute__((hot)) Segment::setPixelColorXY_fast(int x, int y, uint32_t col, uint32_t scaled_col, int cols, int rows) //WLEDMM
{
  unsigned i = UINT_MAX;
  bool sameColor = false;
  if (ledsrgb) { // WLEDMM small optimization
    i = x + y*cols; // avoid error checking done by XY() - be optimistic about ranges of x and y
    CRGB fastled_col = CRGB(col);
    if (ledsrgb[i] == fastled_col) sameColor = true;
    else ledsrgb[i] = fastled_col;
  }

#if 0 // this is still a dangerous optimization
  if ((i < UINT_MAX) && sameColor && (call > 0) && (!transitional)  && (mode != FX_MODE_2DSCROLLTEXT) && (ledsrgb[i] == CRGB(scaled_col))) return; // WLEDMM looks like nothing to do
#endif

  // handle reverse and transpose
  if (reverse  ) x = cols  - x - 1;
  if (reverse_y) y = rows - y - 1;
  if (transpose) std::swap(x,y); // swap X & Y if segment transposed

  // set the requested pixel
  strip.setPixelColorXY_fast(start + x, startY + y, scaled_col);
  #ifdef WLEDMM_FASTPATH
  bool simpleSegment = _isSuperSimpleSegment;
  #else
  bool simpleSegment = !mirror && !mirror_y;
  #endif
  if (simpleSegment) return;   // WLEDMM shortcut when no mirroring needed

  // handle mirroring
  const int_fast16_t wid_ = stop - start;
  const int_fast16_t hei_ = stopY - startY;
  if (mirror) { //set the corresponding horizontally mirrored pixel
    if (transpose) strip.setPixelColorXY_fast(start + x, startY + hei_ - y - 1, scaled_col);
    else           strip.setPixelColorXY_fast(start + wid_ - x - 1, startY + y, scaled_col);
  }
  if (mirror_y) { //set the corresponding vertically mirrored pixel
    if (transpose) strip.setPixelColorXY_fast(start + wid_ - x - 1, startY + y, scaled_col);
    else           strip.setPixelColorXY_fast(start + x, startY + hei_ - y - 1, scaled_col);
  }
  if (mirror_y && mirror) { //set the corresponding vertically AND horizontally mirrored pixel
    strip.setPixelColorXY_fast(start + wid_ - x - 1, startY + hei_ - y - 1, scaled_col);
  }
}


// normal Segment::setPixelColorXY with error checking, and support for grouping / spacing
#ifdef WLEDMM_FASTPATH
void IRAM_ATTR_YN Segment::setPixelColorXY_slow(int x, int y, uint32_t col) //WLEDMM: IRAM_ATTR conditionally, renamed to "_slow"
#else
void IRAM_ATTR_YN Segment::setPixelColorXY(int x, int y, uint32_t col) //WLEDMM: IRAM_ATTR conditionally
#endif
{
  if ((Segment::maxHeight==1) || !isActive()) return; // not a matrix set-up
  const int_fast16_t cols = virtualWidth();  // WLEDMM optimization
  const int_fast16_t rows = virtualHeight();

  if (x<0 || y<0 || x >= cols || y >= rows) return;  // if pixel would fall out of virtual segment just exit

  unsigned i = UINT_MAX;
  bool sameColor = false;
  if (ledsrgb) { // WLEDMM small optimization
    i = XY(x,y);
    CRGB fastled_col = CRGB(col);
    if (ledsrgb[i] == fastled_col) sameColor = true;
    else ledsrgb[i] = fastled_col;
  }
  uint8_t _bri_t = currentBri(on ? opacity : 0);
  if (!_bri_t && !transitional) return;
  if (_bri_t < 255) {
    col = color_fade(col, _bri_t);
  }

#if 0 // this is a dangerous optimization
  if ((i < UINT_MAX) && sameColor && (call > 0) && (!transitional) && (mode != FX_MODE_2DSCROLLTEXT) && (ledsrgb[i] == CRGB(col))) return; // WLEDMM looks like nothing to do
#endif

  if (reverse  ) x = cols  - x - 1;
  if (reverse_y) y = rows - y - 1;
  if (transpose) { uint16_t t = x; x = y; y = t; } // swap X & Y if segment transposed

  // WLEDMM shortcut when no grouping/spacing used
  #ifdef WLEDMM_FASTPATH
  bool simpleSegment = _isSuperSimpleSegment;
  #else
  bool simpleSegment = !mirror && !mirror_y && (grouping == 1) && (spacing == 0);
  #endif
  if (simpleSegment) {
    strip.setPixelColorXY(start + x, startY + y, col);
    return;
  }

  const uint_fast16_t glen_ = groupLength(); // WLEDMM optimization
  const uint_fast16_t wid_ =  width();
  const uint_fast16_t hei_ = height();

  x *= glen_; // expand to physical pixels
  y *= glen_; // expand to physical pixels
  if (x >= wid_ || y >= hei_) return;  // if pixel would fall out of segment just exit

  const int grp_ = grouping; // WLEDMM optimization
  for (int j = 0; j < grp_; j++) {   // groupping vertically
    for (int g = 0; g < grp_; g++) { // groupping horizontally
      uint_fast16_t xX = (x+g), yY = (y+j);    //WLEDMM: use fast types
      if (xX >= wid_ || yY >= hei_) continue; // we have reached one dimension's end

      strip.setPixelColorXY(start + xX, startY + yY, col);

      if (mirror) { //set the corresponding horizontally mirrored pixel
        if (transpose) strip.setPixelColorXY(start + xX, startY + hei_ - yY - 1, col);
        else           strip.setPixelColorXY(start + wid_ - xX - 1, startY + yY, col);
      }
      if (mirror_y) { //set the corresponding vertically mirrored pixel
        if (transpose) strip.setPixelColorXY(start + wid_ - xX - 1, startY + yY, col);
        else           strip.setPixelColorXY(start + xX, startY + hei_ - yY - 1, col);
      }
      if (mirror_y && mirror) { //set the corresponding vertically AND horizontally mirrored pixel
        strip.setPixelColorXY(start + wid_ - xX - 1, startY + hei_ - yY - 1, col);
      }
    }
  }
}

// WLEDMM setPixelColorXY(float x, float y, uint32_t col, ..) is depricated. use wu_pixel(x,y,col) instead.
// anti-aliased version of setPixelColorXY()
void Segment::setPixelColorXY(float x, float y, uint32_t col, bool aa, bool fast) // WLEDMM some speedups due to fast int and faster sqrt16
{
  if (Segment::maxHeight==1) return; // not a matrix set-up
  if (x<0.0f || x>1.0f || y<0.0f || y>1.0f) return; // not normalized

#if 0 // depricated
  const uint_fast16_t cols = virtualWidth();
  const uint_fast16_t rows = virtualHeight();

  float fX = x * (cols-1);
  float fY = y * (rows-1);
  if (aa) {
    uint16_t xL = roundf(fX-0.49f);
    uint16_t xR = roundf(fX+0.49f);
    uint16_t yT = roundf(fY-0.49f);
    uint16_t yB = roundf(fY+0.49f);
    float    dL = (fX - xL)*(fX - xL);
    float    dR = (xR - fX)*(xR - fX);
    float    dT = (fY - yT)*(fY - yT);
    float    dB = (yB - fY)*(yB - fY);
    uint32_t cXLYT = getPixelColorXY(xL, yT);
    uint32_t cXRYT = getPixelColorXY(xR, yT);
    uint32_t cXLYB = getPixelColorXY(xL, yB);
    uint32_t cXRYB = getPixelColorXY(xR, yB);

    if (xL!=xR && yT!=yB) {
      if (!fast) {
        setPixelColorXY(xL, yT, color_blend(col, cXLYT, uint8_t(sqrtf(dL*dT)*255.0f))); // blend TL pixel
        setPixelColorXY(xR, yT, color_blend(col, cXRYT, uint8_t(sqrtf(dR*dT)*255.0f))); // blend TR pixel
        setPixelColorXY(xL, yB, color_blend(col, cXLYB, uint8_t(sqrtf(dL*dB)*255.0f))); // blend BL pixel
        setPixelColorXY(xR, yB, color_blend(col, cXRYB, uint8_t(sqrtf(dR*dB)*255.0f))); // blend BR pixel
      } else {
        setPixelColorXY(xL, yT, color_blend(col, cXLYT, uint8_t(sqrt16(dL*dT*65025.0f)))); // blend TL pixel     // WLEDMM: use faster sqrt16 for integer; perform multiplication by 255^2 before sqrt
        setPixelColorXY(xR, yT, color_blend(col, cXRYT, uint8_t(sqrt16(dR*dT*65025.0f)))); // blend TR pixel     //         this is possible because sqrt(a) * sqrt(b)  =  sqrt(a * b)
        setPixelColorXY(xL, yB, color_blend(col, cXLYB, uint8_t(sqrt16(dL*dB*65025.0f)))); // blend BL pixel
        setPixelColorXY(xR, yB, color_blend(col, cXRYB, uint8_t(sqrt16(dR*dB*65025.0f)))); // blend BR pixel
      }
    } else if (xR!=xL && yT==yB) {
      setPixelColorXY(xR, yT, color_blend(col, cXLYT, uint8_t(dL*255.0f))); // blend L pixel
      setPixelColorXY(xR, yT, color_blend(col, cXRYT, uint8_t(dR*255.0f))); // blend R pixel
    } else if (xR==xL && yT!=yB) {
      setPixelColorXY(xR, yT, color_blend(col, cXLYT, uint8_t(dT*255.0f))); // blend T pixel
      setPixelColorXY(xL, yB, color_blend(col, cXLYB, uint8_t(dB*255.0f))); // blend B pixel
    } else {
      setPixelColorXY(xL, yT, col); // exact match (x & y land on a pixel)
    }
  } else {
    setPixelColorXY(uint16_t(roundf(fX)), uint16_t(roundf(fY)), col);
  }

#else // replacement using wu_pixel
  unsigned px = x * ((virtualWidth()-1) <<8);
  unsigned py = y * ((virtualHeight()-1) <<8);
  wu_pixel(px, py, CRGB(col));
#endif
}

// returns RGBW values of pixel
uint32_t IRAM_ATTR_YN Segment::getPixelColorXY(int x, int y) const {
  if (x<0 || y<0 || !isActive()) return 0; // not active or out-of range
  if (ledsrgb) {
    int i = XY(x,y);
    return RGBW32(ledsrgb[i].r, ledsrgb[i].g, ledsrgb[i].b, 0);
  }
  if (reverse  ) x = virtualWidth()  - x - 1;
  if (reverse_y) y = virtualHeight() - y - 1;
  if (transpose) { uint16_t t = x; x = y; y = t; } // swap X & Y if segment transposed
  const uint_fast16_t groupLength_ = groupLength(); // WLEDMM small optimization
  x *= groupLength_; // expand to physical pixels
  y *= groupLength_; // expand to physical pixels
  if (x >= width() || y >= height()) return 0;
  return strip.getPixelColorXYRestored(start + x, startY + y);
}

// Blends the specified color with the existing pixel color.
void Segment::blendPixelColorXY(uint16_t x, uint16_t y, uint32_t color, uint8_t blend) {
  if (blend == UINT8_MAX) setPixelColorXY(x, y, color);
  else setPixelColorXY(x, y, color_blend(getPixelColorXY(x,y), color, blend));
}

// Adds the specified color with the existing pixel color perserving color balance.
void IRAM_ATTR_YN Segment::addPixelColorXY(int x, int y, uint32_t color, bool fast) {
  // if (!isActive()) return; // not active //WLEDMM sanity check is repeated in getPixelColorXY / setPixelColorXY
  // if (x >= virtualWidth() || y >= virtualHeight() || x<0 || y<0) return;  // if pixel would fall out of virtual segment just exit //WLEDMM
  uint32_t oldCol = getPixelColorXY(x,y);
  uint32_t col = color_add(oldCol, color, fast);
  if (col != oldCol) setPixelColorXY(x, y, col);
}

void Segment::fadePixelColorXY(uint16_t x, uint16_t y, uint8_t fade) {
  // if (!isActive()) return; // not active //WLEDMM sanity check is repeated in getPixelColorXY / setPixelColorXY
  CRGB pix = CRGB(getPixelColorXY(x,y));
  CRGB oldPix = pix;
  pix = pix.nscale8_video(fade);
  if (pix != oldPix) setPixelColorXY(int(x), int(y), pix);
}

// blurRow: perform a blur on a row of a rectangular matrix
void Segment::blurRow(uint32_t row, fract8 blur_amount, bool smear){
  if (!isActive()) return; // not active
  const uint_fast16_t cols = virtualWidth();
  const uint_fast16_t rows = virtualHeight();

  if (row >= rows) return;
  // blur one row
  uint8_t keep = smear ? 255 : 255 - blur_amount;
  uint8_t seep = blur_amount >> 1;
  uint32_t carryover = BLACK;
  uint32_t lastnew;
  uint32_t last;
  uint32_t curnew = 0;
  for (unsigned x = 0; x < cols; x++) {
    uint32_t cur = getPixelColorXY(x, row);
    uint32_t part = color_fade(cur, seep);
    curnew = color_fade(cur, keep);
    if (x > 0) {
      if (carryover)
        curnew = color_add(curnew, carryover, !smear); // WLEDMM don't use "fast" when smear==true (better handling of bright colors)
      uint32_t prev = color_add(lastnew, part, !smear);// WLEDMM
      if (last != prev) // optimization: only set pixel if color has changed
        setPixelColorXY(int(x - 1), int(row), prev);
    }
    else // first pixel
      setPixelColorXY(int(x), int(row), curnew);
    lastnew = curnew;
    last = cur; // save original value for comparison on next iteration
    carryover = part;
  }
  setPixelColorXY(int(cols-1), int(row), curnew); // set last pixel
}

// blurCol: perform a blur on a column of a rectangular matrix
void Segment::blurCol(uint32_t col, fract8 blur_amount, bool smear) {
  if (!isActive()) return; // not active
  const uint_fast16_t cols = virtualWidth();
  const uint_fast16_t rows = virtualHeight();

  if (col >= cols) return;
  // blur one column
  uint8_t keep = smear ? 255 : 255 - blur_amount;
  uint8_t seep = blur_amount >> 1;
  uint32_t carryover = BLACK;
  uint32_t lastnew;
  uint32_t last;
  uint32_t curnew = 0;
  for (unsigned y = 0; y < rows; y++) {
    uint32_t cur = getPixelColorXY(col, y);
    uint32_t part = color_fade(cur, seep);
    curnew = color_fade(cur, keep);
    if (y > 0) {
      if (carryover)
        curnew = color_add(curnew, carryover, !smear);  // WLEDMM don't use "fast" when smear==true (better handling of bright colors)
      uint32_t prev = color_add(lastnew, part, !smear); // WLEDMM
      if (last != prev) // optimization: only set pixel if color has changed
        setPixelColorXY(int(col), int(y - 1), prev);
    }
    else // first pixel
      setPixelColorXY(int(col), int(y), curnew);
    lastnew = curnew;
    last = cur; //save original value for comparison on next iteration
    carryover = part;        
  }
  setPixelColorXY(int(col), int(rows - 1), curnew);
}

// 1D Box blur (with added weight - blur_amount: [0=no blur, 255=max blur])
void Segment::box_blur(uint16_t i, bool vertical, fract8 blur_amount) {
  if (!isActive() || blur_amount == 0) return; // not active
  const int cols = virtualWidth();
  const int rows = virtualHeight();
  const int dim1 = vertical ? rows : cols;
  const int dim2 = vertical ? cols : rows;
  if (i >= dim2) return;
  const float seep = blur_amount/255.f;
  const float keep = 3.f - 2.f*seep;
  // 1D box blur
  uint32_t out[dim1], in[dim1];
  for (int j = 0; j < dim1; j++) {
    int x = vertical ? i : j;
    int y = vertical ? j : i;
    in[j] = getPixelColorXY(x, y);
  }
  for (int j = 0; j < dim1; j++) {
    uint32_t curr = in[j];
    uint32_t prev = j > 0      ? in[j-1] : BLACK;
    uint32_t next = j < dim1-1 ? in[j+1] : BLACK;
    uint8_t r, g, b, w;
    r = (R(curr)*keep + (R(prev) + R(next))*seep) / 3;
    g = (G(curr)*keep + (G(prev) + G(next))*seep) / 3;
    b = (B(curr)*keep + (B(prev) + B(next))*seep) / 3;
    w = (W(curr)*keep + (W(prev) + W(next))*seep) / 3;
    out[j] = RGBW32(r,g,b,w);
  }
  for (int j = 0; j < dim1; j++) {
    int x = vertical ? i : j;
    int y = vertical ? j : i;
    if (in[j] != out[j]) setPixelColorXY(x, y, out[j]);
  }
}

void Segment::moveX(int8_t delta, bool wrap) {
  if (!isActive()) return; // not active
  const uint16_t cols = virtualWidth();
  const uint16_t rows = virtualHeight();
  if (!delta || abs(delta) >= cols) return;
  uint32_t newPxCol[cols];
  for (int y = 0; y < rows; y++) {
    if (delta > 0) {
      for (int x = 0; x < cols-delta; x++)    newPxCol[x] = getPixelColorXY((x + delta), y);
      for (int x = cols-delta; x < cols; x++) newPxCol[x] = getPixelColorXY(wrap ? (x + delta) - cols : x, y);
    } else {
      for (int x = cols-1; x >= -delta; x--) newPxCol[x] = getPixelColorXY((x + delta), y);
      for (int x = -delta-1; x >= 0; x--)    newPxCol[x] = getPixelColorXY(wrap ? (x + delta) + cols : x, y);
    }
    for (int x = 0; x < cols; x++) setPixelColorXY(x, y, newPxCol[x]);
  }
}

void Segment::moveY(int8_t delta, bool wrap) {
  if (!isActive()) return; // not active
  const uint16_t cols = virtualWidth();
  const uint16_t rows = virtualHeight();
  if (!delta || abs(delta) >= rows) return;
  uint32_t newPxCol[rows];
  for (int x = 0; x < cols; x++) {
    if (delta > 0) {
      for (int y = 0; y < rows-delta; y++)    newPxCol[y] = getPixelColorXY(x, (y + delta));
      for (int y = rows-delta; y < rows; y++) newPxCol[y] = getPixelColorXY(x, wrap ? (y + delta) - rows : y);
    } else {
      for (int y = rows-1; y >= -delta; y--) newPxCol[y] = getPixelColorXY(x, (y + delta));
      for (int y = -delta-1; y >= 0; y--)    newPxCol[y] = getPixelColorXY(x, wrap ? (y + delta) + rows : y);
    }
    for (int y = 0; y < rows; y++) setPixelColorXY(x, y, newPxCol[y]);
  }
}

// move() - move all pixels in desired direction delta number of pixels
// @param dir direction: 0=left, 1=left-up, 2=up, 3=right-up, 4=right, 5=right-down, 6=down, 7=left-down
// @param delta number of pixels to move
// @param wrap around
void Segment::move(uint8_t dir, uint8_t delta, bool wrap) {
  if (delta==0) return;
  switch (dir) {
    case 0: moveX( delta, wrap);                      break;
    case 1: moveX( delta, wrap); moveY( delta, wrap); break;
    case 2:                      moveY( delta, wrap); break;
    case 3: moveX(-delta, wrap); moveY( delta, wrap); break;
    case 4: moveX(-delta, wrap);                      break;
    case 5: moveX(-delta, wrap); moveY(-delta, wrap); break;
    case 6:                      moveY(-delta, wrap); break;
    case 7: moveX( delta, wrap); moveY(-delta, wrap); break;
  }
}

void Segment::drawCircle(uint16_t cx, uint16_t cy, uint8_t radius, uint32_t col, bool soft) {
  if (!isActive() || radius == 0) return; // not active
  if (soft) {
    // Xiaolin Wu’s algorithm
    int rsq = radius*radius;
    int x = 0;
    int y = radius;
    unsigned oldFade = 0;
    while (x < y) {
      float yf = sqrtf(float(rsq - x*x)); // needs to be floating point
      unsigned fade = float(0xFFFF) * (ceilf(yf) - yf); // how much color to keep
      if (oldFade > fade) y--;
      oldFade = fade;
      setPixelColorXY(cx+x, cy+y, color_blend(col, getPixelColorXY(cx+x, cy+y), fade, true));
      setPixelColorXY(cx-x, cy+y, color_blend(col, getPixelColorXY(cx-x, cy+y), fade, true));
      setPixelColorXY(cx+x, cy-y, color_blend(col, getPixelColorXY(cx+x, cy-y), fade, true));
      setPixelColorXY(cx-x, cy-y, color_blend(col, getPixelColorXY(cx-x, cy-y), fade, true));
      setPixelColorXY(cx+y, cy+x, color_blend(col, getPixelColorXY(cx+y, cy+x), fade, true));
      setPixelColorXY(cx-y, cy+x, color_blend(col, getPixelColorXY(cx-y, cy+x), fade, true));
      setPixelColorXY(cx+y, cy-x, color_blend(col, getPixelColorXY(cx+y, cy-x), fade, true));
      setPixelColorXY(cx-y, cy-x, color_blend(col, getPixelColorXY(cx-y, cy-x), fade, true));
      setPixelColorXY(cx+x, cy+y-1, color_blend(getPixelColorXY(cx+x, cy+y-1), col, fade, true));
      setPixelColorXY(cx-x, cy+y-1, color_blend(getPixelColorXY(cx-x, cy+y-1), col, fade, true));
      setPixelColorXY(cx+x, cy-y+1, color_blend(getPixelColorXY(cx+x, cy-y+1), col, fade, true));
      setPixelColorXY(cx-x, cy-y+1, color_blend(getPixelColorXY(cx-x, cy-y+1), col, fade, true));
      setPixelColorXY(cx+y-1, cy+x, color_blend(getPixelColorXY(cx+y-1, cy+x), col, fade, true));
      setPixelColorXY(cx-y+1, cy+x, color_blend(getPixelColorXY(cx-y+1, cy+x), col, fade, true));
      setPixelColorXY(cx+y-1, cy-x, color_blend(getPixelColorXY(cx+y-1, cy-x), col, fade, true));
      setPixelColorXY(cx-y+1, cy-x, color_blend(getPixelColorXY(cx-y+1, cy-x), col, fade, true));
      x++;
    }
  } else {
    // Bresenham’s Algorithm
    int d = 3 - (2*radius);
    int y = radius, x = 0;
    while (y >= x) {
      setPixelColorXY(cx+x, cy+y, col);
      setPixelColorXY(cx-x, cy+y, col);
      setPixelColorXY(cx+x, cy-y, col);
      setPixelColorXY(cx-x, cy-y, col);
      setPixelColorXY(cx+y, cy+x, col);
      setPixelColorXY(cx-y, cy+x, col);
      setPixelColorXY(cx+y, cy-x, col);
      setPixelColorXY(cx-y, cy-x, col);
      x++;
      if (d > 0) {
        y--;
        d += 4 * (x - y) + 10;
      } else {
        d += 4 * x + 6;
      }
    }
  }
}

// by stepko, taken from https://editor.soulmatelights.com/gallery/573-blobs
void Segment::fillCircle(unsigned cx, unsigned cy, int radius, uint32_t col, bool soft) {
  if (!isActive() || radius <= 0) return; // not active
  // draw soft bounding circle
  if (soft) drawCircle(cx, cy, radius, col, soft);
  const int cols = virtualWidth();
  const int rows = virtualHeight();

  const int_fast32_t maxRadius2 = radius * radius - (((radius > 3) && !soft) ? 1:0);   // WLEDMM pre-compute r^2; '-1' removes spikes from bigger blobs
  // WLEDMM pre-compute boundaries
  const int startx = max(-radius, -int(cx));
  const int endx = min(radius, cols-1-int(cx));
  const int starty = max(-radius, -int(cy));
  const int endy = min(radius, rows-1-int(cy));

  // fill it - WLEDMM optimized
  for (int y = starty; y <= endy; y++) {
    for (int x = startx; x <= endx; x++) {
      if ((x * x + y * y) <= maxRadius2) {
        setPixelColorXY(cx + x, cy + y, col);
      }
    }
  }
}

void Segment::nscale8(uint8_t scale) {  //WLEDMM: use fast types
  if (!isActive()) return; // not active
  const uint_fast16_t cols = virtualWidth();
  const uint_fast16_t rows = virtualHeight();
  for(uint_fast16_t y = 0; y < rows; y++) for (uint_fast16_t x = 0; x < cols; x++) {
    setPixelColorXY((int)x, (int)y, CRGB(getPixelColorXY(x, y)).nscale8(scale));
  }
}

//line function
void Segment::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t c, bool soft, uint8_t depth) {
  if (!isActive()) return; // not active
  // if (Segment::maxHeight==1) return; // not a matrix set-up
  const int cols = virtualWidth();
  const int rows = virtualHeight();
  if (x0 >= cols || x1 >= cols || y0 >= rows || y1 >= rows) return;

  // WLEDMM shortcut when no grouping/spacing used
  bool simpleSegment = (grouping == 1) && (spacing == 0);
  uint32_t scaled_col = c;
  if (simpleSegment) {
      // segment brightness must be pre-calculated for the "fast" setPixelColorXY variant!
      #ifdef WLEDMM_FASTPATH
      uint8_t _bri_t = _brightness;
      #else
      uint8_t _bri_t = currentBri(on ? opacity : 0);
      #endif
      if (!_bri_t && !transitional) return;
      if (_bri_t < 255) scaled_col = color_fade(c, _bri_t);
  }

  // WLEDMM shorten line according to depth
  if (depth < UINT8_MAX) {
    if (depth == 0) return;         // nothing to paint
    if (depth<2) {x1 = x0; y1=y0; } // single pixel
    else {                          // shorten line
      x0 *=2; y0 *=2; // we do everything "*2" for better rounding
      int dx1 = ((int(2*x1) - int(x0)) * int(depth)) / 255;  // X distance, scaled down by depth 
      int dy1 = ((int(2*y1) - int(y0)) * int(depth)) / 255;  // Y distance, scaled down by depth
      x1 = (x0 + dx1 +1) / 2;
      y1 = (y0 + dy1 +1) / 2;
      x0 /=2; y0 /=2;
    }
  }

  const int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1; // x distance & step
  const int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1; // y distance & step

  // single pixel (line length == 0)
  if (dx+dy == 0) {
    if (simpleSegment) setPixelColorXY_fast(x0, y0, c, scaled_col, cols, rows);
    else setPixelColorXY_slow(x0, y0, c);
    return;
  }

  if (soft) {
    // Xiaolin Wu’s algorithm
    const bool steep = dy > dx;
    if (steep) {
      // we need to go along longest dimension
      std::swap(x0,y0);
      std::swap(x1,y1);
    }
    if (x0 > x1) {
      // we need to go in increasing fashion
      std::swap(x0,x1);
      std::swap(y0,y1);
    }
    float gradient = x1-x0 == 0 ? 1.0f : float(y1-y0) / float(x1-x0);
    float intersectY = y0;
    for (int x = x0; x <= x1; x++) {
      unsigned keep = float(0xFFFF) * (intersectY-int(intersectY)); // how much color to keep
      unsigned seep = 0xFFFF - keep; // how much background to keep
      int y = int(intersectY);
      if (steep) std::swap(x,y);  // temporarily swap if steep
      // pixel coverage is determined by fractional part of y co-ordinate
      setPixelColorXY(x, y, color_blend(c, getPixelColorXY(x, y), keep, true));
      setPixelColorXY(x+int(steep), y+int(!steep), color_blend(c, getPixelColorXY(x+int(steep), y+int(!steep)), seep, true));
      intersectY += gradient;
      if (steep) std::swap(x,y);  // restore if steep
    }
  } else {
    // Bresenham's algorithm
    int err = (dx>dy ? dx : -dy)/2;   // error direction
    for (;;) {
      // if (x0 >= cols || y0 >= rows) break; // WLEDMM we hit the edge - should never happen
      if (simpleSegment) setPixelColorXY_fast(x0, y0, c, scaled_col, cols, rows);
      else setPixelColorXY_slow(x0, y0, c);
      if (x0==x1 && y0==y1) break;
      int e2 = err;
      if (e2 >-dx) { err -= dy; x0 += sx; }
      if (e2 < dy) { err += dx; y0 += sy; }
    }
  }
}

void Segment::drawArc(unsigned x0, unsigned y0, int radius, uint32_t color, uint32_t fillColor) {
  if (!isActive() || (radius <=0)) return; // not active
  float minradius = float(radius) - .5;
  float maxradius = float(radius) + .5;
  // WLEDMM pre-calculate values to speed up the loop
  const int minradius2 = roundf(minradius * minradius);
  const int maxradius2 = roundf(maxradius * maxradius);

  // WLEDMM only loop over surrounding square (50% faster)
  const int width = virtualWidth();
  const int height = virtualHeight();
  const int startx = max(0, int(x0)-radius-1);
  const int endx = min(width, int(x0)+radius+1);
  const int starty = max(0, int(y0)-radius-1);
  const int endy = min(height, int(y0)+radius+1);

  for (int x=startx; x<endx; x++) for (int y=starty; y<endy; y++) {
    int newX2 = x - int(x0); newX2 *= newX2; // (distance from centerX) ^2
    int newY2 = y - int(y0); newY2 *= newY2; // (distance from centerY) ^2
    int distance2 = newX2 + newY2;

    if ((distance2 >= minradius2) && (distance2 <= maxradius2)) {
      setPixelColorXY(x, y, color);
    } else {
    if (fillColor != 0)
      if (distance2 < minradius2)
        setPixelColorXY(x, y, fillColor);
    }
  }
}

//WLEDMM for artifx
bool Segment::jsonToPixels(char * name, uint8_t fileNr) {
  if (!isActive()) return true; // segment not active, nothing to do
  char fileName[32] = { '\0' };
  //WLEDMM: als support segment name ledmaps
  bool isFile = false;
  // strcpy_P(fileName, PSTR("/mario"));
  snprintf(fileName, sizeof(fileName), "/%s%d.json", name, fileNr); //WLEDMM: trick to not include 0 in ledmap.json
  // strcat(fileName, ".json");
  isFile = WLED_FS.exists(fileName);

  if (!isFile) {
    return false;
  }

  if (!requestJSONBufferLock(23)) return false;

  if (!readObjectFromFile(fileName, nullptr, &doc)) {
    releaseJSONBufferLock();
    return false; //if file does not exist just exit
  }

  JsonArray map = doc[F("seg")][F("i")];

  if (!map.isNull() && map.size()) {  // not an empty map

    for (uint16_t i=0; i<map.size(); i+=3) {
      CRGB color = CRGB(map[i+2][0], map[i+2][1], map[i+2][2]);
      for (uint16_t j=map[i]; j<=map[i+1]; j++) {
        setPixelColor(j, color);
      }
    }
  }

  releaseJSONBufferLock();
  return true;
}

#include "src/font/console_font_4x6.h"
#include "src/font/console_font_5x8.h"
#include "src/font/console_font_5x12.h"
#include "src/font/console_font_6x8.h"
#include "src/font/console_font_7x9.h"

// draws a raster font character on canvas
// only supports: 4x6=24, 5x8=40, 5x12=60, 6x8=48 and 7x9=63 fonts ATM
void Segment::drawCharacter(unsigned char chr, int16_t x, int16_t y, uint8_t w, uint8_t h, uint32_t color, uint32_t col2, bool drawShadow) {
  if (!isActive()) return; // not active
  if (chr < 32 || chr > 126) return; // only ASCII 32-126 supported
  chr -= 32; // align with font table entries
  const uint16_t cols = virtualWidth();
  const uint16_t rows = virtualHeight();
  const int font = w*h;

  CRGB col = CRGB(color);
  CRGBPalette16 grad = CRGBPalette16(col, col2 ? CRGB(col2) : col);
  uint32_t bgCol = SEGCOLOR(1);

  //if (w<5 || w>6 || h!=8) return;
  for (int i = 0; i<h; i++) { // character height
    int16_t y0 = y + i;
    if (y0 < 0) continue; // drawing off-screen
    if (y0 >= rows) break; // drawing off-screen
    uint8_t bits = 0;
    uint8_t bits_up = 0; // WLEDMM this is the previous line: font[(chr * h) + i -1]
    switch (font) {
      case 24: bits = pgm_read_byte_near(&console_font_4x6[(chr * h) + i]);
        if ((i>0) && drawShadow) bits_up = pgm_read_byte_near(&console_font_4x6[(chr * h) + i -1]);
        break;  // 5x8 font
      case 40: bits = pgm_read_byte_near(&console_font_5x8[(chr * h) + i]); 
        if ((i>0) && drawShadow) bits_up = pgm_read_byte_near(&console_font_5x8[(chr * h) + i -1]);
        break;  // 5x8 font
      case 48: bits = pgm_read_byte_near(&console_font_6x8[(chr * h) + i]); 
        if ((i>0) && drawShadow) bits_up = pgm_read_byte_near(&console_font_6x8[(chr * h) + i -1]);
        break;  // 6x8 font
      case 63: bits = pgm_read_byte_near(&console_font_7x9[(chr * h) + i]); 
        if ((i>0) && drawShadow) bits_up = pgm_read_byte_near(&console_font_7x9[(chr * h) + i -1]);
        break;  // 7x9 font
      case 60: bits = pgm_read_byte_near(&console_font_5x12[(chr * h) + i]); 
        if ((i>0) && drawShadow) bits_up = pgm_read_byte_near(&console_font_5x12[(chr * h) + i -1]);
        break; // 5x12 font
      default: return;
    }
    col = ColorFromPalette(grad, (i+1)*255/h, 255, NOBLEND);
    for (int j = 0; j<w; j++) { // character width
      int16_t x0 = x + (w-1) - j;
      if ((x0 >= 0) || (x0 < cols)) {
        if ((bits>>(j+(8-w))) & 0x01) { // bit set & drawing on-screen
        setPixelColorXY(x0, y0, col);
        } else {
          if (drawShadow) {
			// WLEDMM
            if ((j < (w-1)) && (bits>>(j+(8-w) +1)) & 0x01) setPixelColorXY(x0, y0, bgCol); // blank when pixel to the right is set
            else if ((j > 0) && (bits>>(j+(8-w) -1)) & 0x01) setPixelColorXY(x0, y0, bgCol);// blank when pixel to the left is set
            else if ((bits_up>>(j+(8-w))) & 0x01) setPixelColorXY(x0, y0, bgCol);           // blank when pixel above is set
          }
        }
      }
    }
  }
}

#define WU_WEIGHT(a,b) ((uint8_t) (((a)*(b)+(a)+(b))>>8))
void Segment::wu_pixel(uint32_t x, uint32_t y, CRGB c) {      //awesome wu_pixel procedure by reddit u/sutaburosu
  if (!isActive()) return; // not active
  // extract the fractional parts and derive their inverses
  uint8_t xx = x & 0xff, yy = y & 0xff, ix = 255 - xx, iy = 255 - yy;
  // calculate the intensities for each affected pixel
  uint8_t wu[4] = {WU_WEIGHT(ix, iy), WU_WEIGHT(xx, iy),
                   WU_WEIGHT(ix, yy), WU_WEIGHT(xx, yy)};
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (int i = 0; i < 4; i++) {
    int wu_x = (x >> 8) + (i & 1);        // WLEDMM precalculate x
    int wu_y = (y >> 8) + ((i >> 1) & 1); // WLEDMM precalculate y
    CRGB led = getPixelColorXY(wu_x, wu_y);
    CRGB oldLed = led;
    led.r = qadd8(led.r, c.r * wu[i] >> 8);
    led.g = qadd8(led.g, c.g * wu[i] >> 8);
    led.b = qadd8(led.b, c.b * wu[i] >> 8);
    if (led != oldLed) setPixelColorXY(wu_x, wu_y, led); // WLEDMM don't repaint same color
  }
}
#undef WU_WEIGHT

#endif // WLED_DISABLE_2D
