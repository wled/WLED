/*
  FX_2Dfcn.cpp contains all 2D utility functions

  LICENSE
  The MIT License (MIT)
  Copyright (c) 2022  Blaz Kristan (https://blaz.at/home)
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

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
    
    //WLEDMM recreate customMappingTable if more space needed
    if (Segment::maxWidth * Segment::maxHeight > customMappingTableSize) {
      size_t size = max(ledmapMaxSize, size_t(Segment::maxWidth * Segment::maxHeight)); // TroyHacks
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

    if (customMappingTable != nullptr) {
      customMappingSize = Segment::maxWidth * Segment::maxHeight;

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
#else
  isMatrix = false; // no matter what config says
#endif
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
uint32_t WS2812FX::getPixelColorXY(uint16_t x, uint16_t y) {
#ifndef WLED_DISABLE_2D
  uint_fast16_t index = (y * Segment::maxWidth + x); //WLEDMM: use fast types
#else
  uint16_t index = x;
#endif
  if (index < customMappingSize) index = customMappingTable[index];
  if (index >= _length) return 0;
  return busses.getPixelColor(index);
}

///////////////////////////////////////////////////////////
// Segment:: routines
///////////////////////////////////////////////////////////

#ifndef WLED_DISABLE_2D

// XY(x,y) - gets pixel index within current segment (often used to reference leds[] array element)
// WLEDMM Segment::XY()is declared inline, see FX.h

void IRAM_ATTR_YN Segment::setPixelColorXY(int x, int y, uint32_t col) //WLEDMM: IRAM_ATTR conditionally
{
  if (Segment::maxHeight==1) return; // not a matrix set-up
  if (x >= virtualWidth() || y >= virtualHeight() || x<0 || y<0) return;  // if pixel would fall out of virtual segment just exit

  if (ledsrgb) ledsrgb[XY(x,y)] = col;

  uint8_t _bri_t = currentBri(on ? opacity : 0);
  if (!_bri_t && !transitional) return;
  if (_bri_t < 255) {
    byte r = scale8(R(col), _bri_t);
    byte g = scale8(G(col), _bri_t);
    byte b = scale8(B(col), _bri_t);
    byte w = scale8(W(col), _bri_t);
    col = RGBW32(r, g, b, w);
  }

  if (reverse  ) x = virtualWidth()  - x - 1;
  if (reverse_y) y = virtualHeight() - y - 1;
  if (transpose) { uint16_t t = x; x = y; y = t; } // swap X & Y if segment transposed

  x *= groupLength(); // expand to physical pixels
  y *= groupLength(); // expand to physical pixels
  if (x >= width() || y >= height()) return;  // if pixel would fall out of segment just exit

  for (int j = 0; j < grouping; j++) {   // groupping vertically
    for (int g = 0; g < grouping; g++) { // groupping horizontally
      uint_fast16_t xX = (x+g), yY = (y+j);    //WLEDMM: use fast types
      if (xX >= width() || yY >= height()) continue; // we have reached one dimension's end

      strip.setPixelColorXY(start + xX, startY + yY, col);

      if (mirror) { //set the corresponding horizontally mirrored pixel
        if (transpose) strip.setPixelColorXY(start + xX, startY + height() - yY - 1, col);
        else           strip.setPixelColorXY(start + width() - xX - 1, startY + yY, col);
      }
      if (mirror_y) { //set the corresponding vertically mirrored pixel
        if (transpose) strip.setPixelColorXY(start + width() - xX - 1, startY + yY, col);
        else           strip.setPixelColorXY(start + xX, startY + height() - yY - 1, col);
      }
      if (mirror_y && mirror) { //set the corresponding vertically AND horizontally mirrored pixel
        strip.setPixelColorXY(width() - xX - 1, height() - yY - 1, col);
      }
    }
  }
}

// anti-aliased version of setPixelColorXY()
void Segment::setPixelColorXY(float x, float y, uint32_t col, bool aa, bool fast) // WLEDMM some speedups due to fast int and faster sqrt16
{
  if (Segment::maxHeight==1) return; // not a matrix set-up
  if (x<0.0f || x>1.0f || y<0.0f || y>1.0f) return; // not normalized

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
}

// returns RGBW values of pixel
uint32_t Segment::getPixelColorXY(uint16_t x, uint16_t y) {
  if (!isActive()) return 0; // not active
  int i = XY(x,y);
  if (ledsrgb) return RGBW32(ledsrgb[i].r, ledsrgb[i].g, ledsrgb[i].b, 0);
  if (reverse  ) x = virtualWidth()  - x - 1;
  if (reverse_y) y = virtualHeight() - y - 1;
  if (transpose) { uint16_t t = x; x = y; y = t; } // swap X & Y if segment transposed
  x *= groupLength(); // expand to physical pixels
  y *= groupLength(); // expand to physical pixels
  if (x >= width() || y >= height()) return 0;
  return strip.getPixelColorXY(start + x, startY + y);
}

// Blends the specified color with the existing pixel color.
void Segment::blendPixelColorXY(uint16_t x, uint16_t y, uint32_t color, uint8_t blend) {
  setPixelColorXY(x, y, color_blend(getPixelColorXY(x,y), color, blend));
}

// Adds the specified color with the existing pixel color perserving color balance.
void Segment::addPixelColorXY(int x, int y, uint32_t color, bool fast) {
  if (!isActive()) return; // not active
  if (x >= virtualWidth() || y >= virtualHeight() || x<0 || y<0) return;  // if pixel would fall out of virtual segment just exit
  uint32_t col = getPixelColorXY(x,y);
  uint8_t r = R(col);
  uint8_t g = G(col);
  uint8_t b = B(col);
  uint8_t w = W(col);
  if (fast) {
    r = qadd8(r, R(color));
    g = qadd8(g, G(color));
    b = qadd8(b, B(color));
    w = qadd8(w, W(color));
    col = RGBW32(r,g,b,w);
  } else {
    col = color_add(col, color);
  }
  setPixelColorXY(x, y, col);
}

void Segment::fadePixelColorXY(uint16_t x, uint16_t y, uint8_t fade) {
  if (!isActive()) return; // not active
  CRGB pix = CRGB(getPixelColorXY(x,y)).nscale8_video(fade);
  setPixelColorXY(x, y, pix);
}

// blurRow: perform a blur on a row of a rectangular matrix
void Segment::blurRow(uint16_t row, fract8 blur_amount) {
  if (!isActive()) return; // not active
  const uint_fast16_t cols = virtualWidth();
  const uint_fast16_t rows = virtualHeight();

  if (row >= rows) return;
  // blur one row
  uint8_t keep = 255 - blur_amount;
  uint8_t seep = blur_amount >> 1;
  CRGB carryover = CRGB::Black;
  for (uint_fast16_t x = 0; x < cols; x++) {
    CRGB cur = getPixelColorXY(x, row);
    uint32_t before = uint32_t(cur);     // remember color before blur
    CRGB part = cur;
    part.nscale8(seep);
    cur.nscale8(keep);
    cur += carryover;
    if (x>0) {
      CRGB prev = CRGB(getPixelColorXY(x-1, row)) + part;
      setPixelColorXY(x-1, row, prev);
    }
    if (before != uint32_t(cur))         // optimization: only set pixel if color has changed
      setPixelColorXY(x, row, cur);
    carryover = part;
  }
}

// blurCol: perform a blur on a column of a rectangular matrix
void Segment::blurCol(uint16_t col, fract8 blur_amount) {
  if (!isActive()) return; // not active
  const uint_fast16_t cols = virtualWidth();
  const uint_fast16_t rows = virtualHeight();

  if (col >= cols) return;
  // blur one column
  uint8_t keep = 255 - blur_amount;
  uint8_t seep = blur_amount >> 1;
  CRGB carryover = CRGB::Black;
  for (uint_fast16_t y = 0; y < rows; y++) {
    CRGB cur = getPixelColorXY(col, y);
    CRGB part = cur;
    uint32_t before = uint32_t(cur);     // remember color before blur
    part.nscale8(seep);
    cur.nscale8(keep);
    cur += carryover;
    if (y>0) {
      CRGB prev = CRGB(getPixelColorXY(col, y-1)) + part;
      setPixelColorXY(col, y-1, prev);
    }
    if (before != uint32_t(cur))         // optimization: only set pixel if color has changed
      setPixelColorXY(col, y, cur);
    carryover = part;
  }
}

// 1D Box blur (with added weight - blur_amount: [0=no blur, 255=max blur])
void Segment::box_blur(uint16_t i, bool vertical, fract8 blur_amount) {  //WLEDMM: use fast types
  const uint_fast16_t cols = virtualWidth();
  const uint_fast16_t rows = virtualHeight();
  const uint_fast16_t dim1 = vertical ? rows : cols;
  const uint_fast16_t dim2 = vertical ? cols : rows;
  if (i >= dim2) return;
  const float seep = blur_amount/255.f;
  const float keep = 3.f - 2.f*seep;
  // 1D box blur
  CRGB tmp[dim1];
  for (uint_fast16_t j = 0; j < dim1; j++) {
    uint_fast16_t x = vertical ? i : j;
    uint_fast16_t y = vertical ? j : i;
    int_fast16_t xp = vertical ? x : x-1;  // "signed" to prevent underflow
    int_fast16_t yp = vertical ? y-1 : y;  // "signed" to prevent underflow
    uint_fast16_t xn = vertical ? x : x+1;
    uint_fast16_t yn = vertical ? y+1 : y;
    CRGB curr = getPixelColorXY(x,y);
    CRGB prev = (xp<0 || yp<0) ? CRGB::Black : getPixelColorXY(xp,yp);
    CRGB next = ((vertical && yn>=dim1) || (!vertical && xn>=dim1)) ? CRGB::Black : getPixelColorXY(xn,yn);
    uint16_t r, g, b;
    r = (curr.r*keep + (prev.r + next.r)*seep) / 3;
    g = (curr.g*keep + (prev.g + next.g)*seep) / 3;
    b = (curr.b*keep + (prev.b + next.b)*seep) / 3;
    tmp[j] = CRGB(r,g,b);
  }
  for (uint_fast16_t j = 0; j < dim1; j++) {
    uint_fast16_t x = vertical ? i : j;
    uint_fast16_t y = vertical ? j : i;
    setPixelColorXY((int)x, (int)y, tmp[j]);
  }
}

// blur1d: one-dimensional blur filter. Spreads light to 2 line neighbors.
// blur2d: two-dimensional blur filter. Spreads light to 8 XY neighbors.
//
//           0 = no spread at all
//          64 = moderate spreading
//         172 = maximum smooth, even spreading
//
//         173..255 = wider spreading, but increasing flicker
//
//         Total light is NOT entirely conserved, so many repeated
//         calls to 'blur' will also result in the light fading,
//         eventually all the way to black; this is by design so that
//         it can be used to (slowly) clear the LEDs to black.

void Segment::blur1d(fract8 blur_amount) {   //WLEDMM: use fast types
  const uint_fast16_t rows = virtualHeight();
  for (uint_fast16_t y = 0; y < rows; y++) blurRow(y, blur_amount);
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

void Segment::draw_circle(uint16_t cx, uint16_t cy, uint8_t radius, CRGB col) {
  if (!isActive()) return; // not active
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

// by stepko, taken from https://editor.soulmatelights.com/gallery/573-blobs
void Segment::fill_circle(uint16_t cx, uint16_t cy, uint8_t radius, CRGB col) {
  if (!isActive()) return; // not active
  const uint16_t cols = virtualWidth();
  const uint16_t rows = virtualHeight();
  for (int16_t y = -radius; y <= radius; y++) {
    for (int16_t x = -radius; x <= radius; x++) {
      if (x * x + y * y <= radius * radius &&
          int16_t(cx)+x>=0 && int16_t(cy)+y>=0 &&
          int16_t(cx)+x<cols && int16_t(cy)+y<rows)
        setPixelColorXY(cx + x, cy + y, col);
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
void Segment::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t c) {
  if (!isActive()) return; // not active
  const uint16_t cols = virtualWidth();
  const uint16_t rows = virtualHeight();
  if (x0 >= cols || x1 >= cols || y0 >= rows || y1 >= rows) return;
  const int16_t dx = abs(x1-x0), sx = x0<x1 ? 1 : -1;
  const int16_t dy = abs(y1-y0), sy = y0<y1 ? 1 : -1;
  int16_t err = (dx>dy ? dx : -dy)/2, e2;
  for (;;) {
    setPixelColorXY(x0,y0,c);
    if (x0==x1 && y0==y1) break;
    e2 = err;
    if (e2 >-dx) { err -= dy; x0 += sx; }
    if (e2 < dy) { err += dx; y0 += sy; }
  }
}

void Segment::drawArc(uint16_t x0, uint16_t y0, uint16_t radius, uint32_t color, uint32_t fillColor) {
  if (!isActive()) return; // not active
  // float step = degrees / (2.85f*MAX(radius,1));
  // for (float rad = 0.0f; rad <= degrees+step/2; rad += step) {
  //   // may want to try float version as well (with or without antialiasing)
  //   int x = roundf(sin_t(rad) * radius);
  //   int y = roundf(cos_t(rad) * radius);
  //   setPixelColorXY(x+x0, y+y0, c);
  // }
  float minradius = radius - .5;
  float maxradius = radius + .5;
  for (int x=0; x<virtualWidth(); x++) for (int y=0; y<virtualHeight(); y++) {

    int newX = x - x0;
    int newY = y - y0;

    if (newX*newX + newY*newY >= minradius * minradius && newX*newX + newY*newY <= maxradius * maxradius)
      setPixelColorXY(x, y, color);
    if (fillColor != 0)
      if (newX*newX + newY*newY < minradius * minradius)
        setPixelColorXY(x, y, fillColor);
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
void Segment::drawCharacter(unsigned char chr, int16_t x, int16_t y, uint8_t w, uint8_t h, uint32_t color, uint32_t col2) {
  if (!isActive()) return; // not active
  if (chr < 32 || chr > 126) return; // only ASCII 32-126 supported
  chr -= 32; // align with font table entries
  const uint16_t cols = virtualWidth();
  const uint16_t rows = virtualHeight();
  const int font = w*h;

  CRGB col = CRGB(color);
  CRGBPalette16 grad = CRGBPalette16(col, col2 ? CRGB(col2) : col);

  //if (w<5 || w>6 || h!=8) return;
  for (int i = 0; i<h; i++) { // character height
    int16_t y0 = y + i;
    if (y0 < 0) continue; // drawing off-screen
    if (y0 >= rows) break; // drawing off-screen
    uint8_t bits = 0;
    switch (font) {
      case 24: bits = pgm_read_byte_near(&console_font_4x6[(chr * h) + i]); break;  // 5x8 font
      case 40: bits = pgm_read_byte_near(&console_font_5x8[(chr * h) + i]); break;  // 5x8 font
      case 48: bits = pgm_read_byte_near(&console_font_6x8[(chr * h) + i]); break;  // 6x8 font
      case 63: bits = pgm_read_byte_near(&console_font_7x9[(chr * h) + i]); break;  // 7x9 font
      case 60: bits = pgm_read_byte_near(&console_font_5x12[(chr * h) + i]); break; // 5x12 font
      default: return;
    }
    col = ColorFromPalette(grad, (i+1)*255/h, 255, NOBLEND);
    for (int j = 0; j<w; j++) { // character width
      int16_t x0 = x + (w-1) - j;
      if ((x0 >= 0 || x0 < cols) && ((bits>>(j+(8-w))) & 0x01)) { // bit set & drawing on-screen
        setPixelColorXY(x0, y0, col);
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
    CRGB led = getPixelColorXY((x >> 8) + (i & 1), (y >> 8) + ((i >> 1) & 1));
    led.r = qadd8(led.r, c.r * wu[i] >> 8);
    led.g = qadd8(led.g, c.g * wu[i] >> 8);
    led.b = qadd8(led.b, c.b * wu[i] >> 8);
    setPixelColorXY(int((x >> 8) + (i & 1)), int((y >> 8) + ((i >> 1) & 1)), led);
  }
}
#undef WU_WEIGHT

#endif // WLED_DISABLE_2D
