/*
  FX_2Dfcn.cpp contains all 2D utility functions

  Copyright (c) 2022  Blaz Kristan (https://blaz.at/home)
  Licensed under the EUPL v. 1.2 or later
  Adapted from code originally licensed under the MIT license

  Parts of the code adapted from WLED Sound Reactive
*/
#include "wled.h"

// setUpMatrix() - constructs ledmap array from matrix of panels with WxH pixels
// this converts physical (possibly irregular) LED arrangement into well defined
// array of logical pixels: fist entry corresponds to left-topmost logical pixel
// followed by horizontal pixels, when Segment::maxWidth logical pixels are added they
// are followed by next row (down) of Segment::maxWidth pixels (and so forth)
// note: matrix may be comprised of multiple panels each with different orientation
// but ledmap takes care of that. ledmap is constructed upon initialization
// so matrix should disable regular ledmap processing
// WARNING: effect drawing has to be suspended (strip.suspend()) or must be called from loop() context
void WS2812FX::setUpMatrix() {
#ifndef WLED_DISABLE_2D
  // isMatrix is set in cfg.cpp or set.cpp
  if (isMatrix) {
    // calculate width dynamically because it may have gaps
    Segment::maxWidth = 1;
    Segment::maxHeight = 1;
    for (const Panel &p : panel) {
      if (p.xOffset + p.width > Segment::maxWidth) {
        Segment::maxWidth = p.xOffset + p.width;
      }
      if (p.yOffset + p.height > Segment::maxHeight) {
        Segment::maxHeight = p.yOffset + p.height;
      }
    }

    // safety check
    if (Segment::maxWidth * Segment::maxHeight > MAX_LEDS || Segment::maxWidth > 255 || Segment::maxHeight > 255 || Segment::maxWidth <= 1 || Segment::maxHeight <= 1) {
      DEBUG_PRINTLN(F("2D Bounds error."));
      isMatrix = false;
      Segment::maxWidth = _length;
      Segment::maxHeight = 1;
      panel.clear(); // release memory allocated by panels
      panel.shrink_to_fit(); // release memory if allocated
      resetSegments();
      return;
    }

    customMappingSize = 0; // prevent use of mapping if anything goes wrong

    d_free(customMappingTable);
    // Segment::maxWidth and Segment::maxHeight are set according to panel layout
    // and the product will include at least all leds in matrix
    // if actual LEDs are more, getLengthTotal() will return correct number of LEDs
    customMappingTable = static_cast<uint16_t*>(d_malloc(sizeof(uint16_t)*getLengthTotal())); // prefer to not use SPI RAM

    if (customMappingTable) {
      customMappingSize = getLengthTotal();

      // fill with empty in case we don't fill the entire matrix
      unsigned matrixSize = Segment::maxWidth * Segment::maxHeight;
      for (unsigned i = 0; i<matrixSize; i++) customMappingTable[i] = 0xFFFFU;
      for (unsigned i = matrixSize; i<getLengthTotal(); i++) customMappingTable[i] = i; // trailing LEDs for ledmap (after matrix) if it exist

      // we will try to load a "gap" array (a JSON file)
      // the array has to have the same amount of values as mapping array (or larger)
      // "gap" array is used while building ledmap (mapping array)
      // and discarded afterwards as it has no meaning after the process
      // content of the file is just raw JSON array in the form of [val1,val2,val3,...]
      // there are no other "key":"value" pairs in it
      // allowed values are: -1 (missing pixel/no LED attached), 0 (inactive/unused pixel), 1 (active/used pixel)
      char    fileName[32]; strcpy_P(fileName, PSTR("/2d-gaps.json"));
      bool    isFile = WLED_FS.exists(fileName);
      size_t  gapSize = 0;
      int8_t *gapTable = nullptr;

      if (isFile && requestJSONBufferLock(JSON_LOCK_LEDGAP)) {
        DEBUG_PRINT(F("Reading LED gap from "));
        DEBUG_PRINTLN(fileName);
        // read the array into global JSON buffer
        if (readObjectFromFile(fileName, nullptr, pDoc)) {
          // the array is similar to ledmap, except it has only 3 values:
          // -1 ... missing pixel (do not increase pixel count)
          //  0 ... inactive pixel (it does count, but should be mapped out (-1))
          //  1 ... active pixel (it will count and will be mapped)
          JsonArray map = pDoc->as<JsonArray>();
          gapSize = map.size();
          if (!map.isNull() && gapSize >= matrixSize) { // not an empty map
            gapTable = static_cast<int8_t*>(p_malloc(gapSize));
            if (gapTable) for (size_t i = 0; i < gapSize; i++) {
              gapTable[i] = constrain(map[i], -1, 1);
            }
          }
        }
        DEBUG_PRINTLN(F("Gaps loaded."));
        releaseJSONBufferLock();
      }

      unsigned x, y, pix=0; //pixel
      for (const Panel &p : panel) {
        unsigned h = p.vertical ? p.height : p.width;
        unsigned v = p.vertical ? p.width  : p.height;
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
      p_free(gapTable);

      #ifdef WLED_DEBUG
      DEBUG_PRINT(F("Matrix ledmap:"));
      for (unsigned i=0; i<customMappingSize; i++) {
        if (!(i%Segment::maxWidth)) DEBUG_PRINTLN();
        DEBUG_PRINTF_P(PSTR("%4d,"), customMappingTable[i]);
      }
      DEBUG_PRINTLN();
      #endif
    } else { // memory allocation error
      DEBUG_PRINTLN(F("ERROR 2D LED map allocation error."));
      isMatrix = false;
      panel.clear();
      Segment::maxWidth = _length;
      Segment::maxHeight = 1;
      resetSegments();
    }
  }
#else
  isMatrix = false; // no matter what config says
#endif
}


///////////////////////////////////////////////////////////
// Segment:: routines
///////////////////////////////////////////////////////////

#ifndef WLED_DISABLE_2D
// pixel is clipped if it falls outside clipping range
// if clipping start > stop the clipping range is inverted
bool Segment::isPixelXYClipped(int x, int y) const {
  if (blendingStyle != BLEND_STYLE_FADE && isInTransition() && _clipStart != _clipStop) {
    const bool invertX = _clipStart  > _clipStop;
    const bool invertY = _clipStartY > _clipStopY;
    const int  cStartX = invertX ? _clipStop   : _clipStart;
    const int  cStopX  = invertX ? _clipStart  : _clipStop;
    const int  cStartY = invertY ? _clipStopY  : _clipStartY;
    const int  cStopY  = invertY ? _clipStartY : _clipStopY;
    if (blendingStyle == BLEND_STYLE_FAIRY_DUST) {
      const unsigned width = cStopX - cStartX;          // assumes full segment width (faster than virtualWidth())
      const unsigned len = width * (cStopY - cStartY);  // assumes full segment height (faster than virtualHeight())
      if (len < 2) return false;
      const unsigned shuffled = hashInt(x + y * width) % len;
      const unsigned pos = (shuffled * 0xFFFFU) / len;
      return progress() <= pos;
    }
    if (blendingStyle == BLEND_STYLE_CIRCULAR_IN || blendingStyle == BLEND_STYLE_CIRCULAR_OUT) {
      const int cx   = (cStopX-cStartX+1) / 2;
      const int cy   = (cStopY-cStartY+1) / 2;
      const bool out = (blendingStyle == BLEND_STYLE_CIRCULAR_OUT);
      const unsigned prog = out ? progress() : 0xFFFFU - progress();
      int radius2    = max(cx, cy) * prog / 0xFFFF;
      radius2 = 2 * radius2 * radius2;
      if (radius2 == 0) return out;
      const int dx = x - cx;
      const int dy = y - cy;
      const bool outside = dx * dx + dy * dy > radius2;
      return out ? outside : !outside;
    }
    bool xInside = (x >= cStartX && x < cStopX); if (invertX) xInside = !xInside;
    bool yInside = (y >= cStartY && y < cStopY); if (invertY) yInside = !yInside;
    const bool clip = blendingStyle == BLEND_STYLE_OUTSIDE_IN ? xInside || yInside : xInside && yInside;
    return !clip;
  }
  return false;
}

void IRAM_ATTR_YN Segment::setPixelColorXY(int x, int y, uint32_t col) const
{
  if (!isActive()) return; // not active
  if ((unsigned)x >= vWidth() || (unsigned)y >= vHeight()) return;  // if pixel would fall out of virtual segment just exit
  setPixelColorXYRaw(x, y, col);
}

#ifdef WLED_USE_AA_PIXELS
// anti-aliased version of setPixelColorXY()
void Segment::setPixelColorXY(float x, float y, uint32_t col, bool aa) const
{
  if (!isActive()) return; // not active
  if (x<0.0f || x>1.0f || y<0.0f || y>1.0f) return; // not normalized

  float fX = x * (vWidth()-1);
  float fY = y * (vHeight()-1);
  if (aa) {
    unsigned xL = roundf(fX-0.49f);
    unsigned xR = roundf(fX+0.49f);
    unsigned yT = roundf(fY-0.49f);
    unsigned yB = roundf(fY+0.49f);
    float    dL = (fX - xL)*(fX - xL);
    float    dR = (xR - fX)*(xR - fX);
    float    dT = (fY - yT)*(fY - yT);
    float    dB = (yB - fY)*(yB - fY);
    uint32_t cXLYT = getPixelColorXY(xL, yT);
    uint32_t cXRYT = getPixelColorXY(xR, yT);
    uint32_t cXLYB = getPixelColorXY(xL, yB);
    uint32_t cXRYB = getPixelColorXY(xR, yB);

    if (xL!=xR && yT!=yB) {
      setPixelColorXY(xL, yT, color_blend(col, cXLYT, uint8_t(sqrtf(dL*dT)*255.0f))); // blend TL pixel
      setPixelColorXY(xR, yT, color_blend(col, cXRYT, uint8_t(sqrtf(dR*dT)*255.0f))); // blend TR pixel
      setPixelColorXY(xL, yB, color_blend(col, cXLYB, uint8_t(sqrtf(dL*dB)*255.0f))); // blend BL pixel
      setPixelColorXY(xR, yB, color_blend(col, cXRYB, uint8_t(sqrtf(dR*dB)*255.0f))); // blend BR pixel
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
#endif

// returns RGBW values of pixel
uint32_t IRAM_ATTR_YN Segment::getPixelColorXY(int x, int y) const {
  if (!isActive()) return 0; // not active
  if ((unsigned)x >= vWidth() || (unsigned)y >= vHeight()) return 0;  // if pixel would fall out of virtual segment just exit
  return getPixelColorXYRaw(x,y);
}

// 2D blurring, can be asymmetrical
void Segment::blur2D(uint8_t blur_x, uint8_t blur_y, bool smear) const {
  if (!isActive()) return; // not active
  const unsigned cols = vWidth();
  const unsigned rows = vHeight();
  const auto XY = [&](unsigned x, unsigned y){ return x + y*cols; };
  if (blur_x) {
    const uint8_t keepx = smear ? 255 : 255 - blur_x;
    const uint8_t seepx = blur_x >> 1;
    for (unsigned row = 0; row < rows; row++) { // blur rows (x direction)
      // handle first pixel in row to avoid conditional in loop (faster)
      uint32_t cur = getPixelColorRaw(XY(0, row));
      uint32_t carryover = fast_color_scale(cur, seepx);
      setPixelColorRaw(XY(0, row), fast_color_scale(cur, keepx));
      for (unsigned x = 1; x < cols; x++) {
         cur = getPixelColorRaw(XY(x, row));
        uint32_t part = fast_color_scale(cur, seepx);
        cur = fast_color_scale(cur, keepx);
        cur = color_add(cur, carryover);
        setPixelColorRaw(XY(x - 1, row), color_add(getPixelColorRaw(XY(x-1, row)), part)); // previous pixel
        setPixelColorRaw(XY(x, row), cur); // current pixel
        carryover = part;
      }
    }
  }
  if (blur_y) {
    const uint8_t keepy = smear ? 255 : 255 - blur_y;
    const uint8_t seepy = blur_y >> 1;
    for (unsigned col = 0; col < cols; col++) {
      // handle first pixel in column
      uint32_t cur = getPixelColorRaw(XY(col, 0));
      uint32_t carryover = fast_color_scale(cur, seepy);
      setPixelColorRaw(XY(col, 0), fast_color_scale(cur, keepy));
      for (unsigned y = 1; y < rows; y++) {
        cur = getPixelColorRaw(XY(col, y));
        uint32_t part = fast_color_scale(cur, seepy);
        cur = fast_color_scale(cur, keepy);
        cur = color_add(cur, carryover);
        setPixelColorRaw(XY(col, y - 1), color_add(getPixelColorRaw(XY(col, y-1)), part)); // previous pixel
        setPixelColorRaw(XY(col, y), cur); // current pixel
        carryover = part;
      }
    }
  }
}

/*
// 2D Box blur
void Segment::box_blur(unsigned radius, bool smear) {
  if (!isActive() || radius == 0) return; // not active
  if (radius > 3) radius = 3;
  const unsigned d = (1 + 2*radius) * (1 + 2*radius); // averaging divisor
  const unsigned cols = vWidth();
  const unsigned rows = vHeight();
  uint16_t *tmpRSum = new uint16_t[cols*rows];
  uint16_t *tmpGSum = new uint16_t[cols*rows];
  uint16_t *tmpBSum = new uint16_t[cols*rows];
  uint16_t *tmpWSum = new uint16_t[cols*rows];
  // fill summed-area table (https://en.wikipedia.org/wiki/Summed-area_table)
  for (unsigned x = 0; x < cols; x++) {
    unsigned rS, gS, bS, wS;
    unsigned index;
    rS = gS = bS = wS = 0;
    for (unsigned y = 0; y < rows; y++) {
      index = x * cols + y;
      if (x > 0) {
        unsigned index2 = (x - 1) * cols + y;
        tmpRSum[index] = tmpRSum[index2];
        tmpGSum[index] = tmpGSum[index2];
        tmpBSum[index] = tmpBSum[index2];
        tmpWSum[index] = tmpWSum[index2];
      } else {
        tmpRSum[index] = 0;
        tmpGSum[index] = 0;
        tmpBSum[index] = 0;
        tmpWSum[index] = 0;
      }
      uint32_t c = getPixelColorXY(x, y);
      rS += R(c);
      gS += G(c);
      bS += B(c);
      wS += W(c);
      tmpRSum[index] += rS;
      tmpGSum[index] += gS;
      tmpBSum[index] += bS;
      tmpWSum[index] += wS;
    }
  }
  // do a box blur using pre-calculated sums
  for (unsigned x = 0; x < cols; x++) {
    for (unsigned y = 0; y < rows; y++) {
      // sum = D + A - B - C where k = (x,y)
      // +----+-+---- (x)
      // |    | |
      // +----A-B
      // |    |k|
      // +----C-D
      // |
      //(y)
      unsigned x0 = x < radius ? 0 : x - radius;
      unsigned y0 = y < radius ? 0 : y - radius;
      unsigned x1 = x >= cols - radius ? cols - 1 : x + radius;
      unsigned y1 = y >= rows - radius ? rows - 1 : y + radius;
      unsigned A = x0 * cols + y0;
      unsigned B = x1 * cols + y0;
      unsigned C = x0 * cols + y1;
      unsigned D = x1 * cols + y1;
      unsigned r = tmpRSum[D] + tmpRSum[A] - tmpRSum[C] - tmpRSum[B];
      unsigned g = tmpGSum[D] + tmpGSum[A] - tmpGSum[C] - tmpGSum[B];
      unsigned b = tmpBSum[D] + tmpBSum[A] - tmpBSum[C] - tmpBSum[B];
      unsigned w = tmpWSum[D] + tmpWSum[A] - tmpWSum[C] - tmpWSum[B];
      setPixelColorXY(x, y, RGBW32(r/d, g/d, b/d, w/d));
    }
  }
  delete[] tmpRSum;
  delete[] tmpGSum;
  delete[] tmpBSum;
  delete[] tmpWSum;
}
*/
void Segment::moveX(int delta, bool wrap) const {
  if (!isActive() || !delta) return; // not active
  const int vW = vWidth();   // segment width in logical pixels (can be 0 if segment is inactive)
  const int vH = vHeight();  // segment height in logical pixels (is always >= 1)
  const auto XY = [&](unsigned x, unsigned y){ return x + y*vW; };
  int absDelta = abs(delta);
  if (absDelta >= vW) return;
  uint32_t newPxCol[vW];
  int newDelta;
  int stop = vW;
  int start = 0;
  if (wrap) newDelta = (delta + vW) % vW; // +cols in case delta < 0
  else {
    if (delta < 0) start = absDelta;
    stop = vW - absDelta;
    newDelta = delta > 0 ? delta : 0;
  }
  for (int y = 0; y < vH; y++) {
    for (int x = 0; x < stop; x++) {
      int srcX = x + newDelta;
      if (wrap) srcX %= vW; // Wrap using modulo when `wrap` is true
      newPxCol[x] = getPixelColorRaw(XY(srcX, y));
    }
    for (int x = 0; x < stop; x++) setPixelColorRaw(XY(x + start, y), newPxCol[x]);
  }
}

void Segment::moveY(int delta, bool wrap) const {
  if (!isActive() || !delta) return; // not active
  const int vW = vWidth();   // segment width in logical pixels (can be 0 if segment is inactive)
  const int vH = vHeight();  // segment height in logical pixels (is always >= 1)
  const auto XY = [&](unsigned x, unsigned y){ return x + y*vW; };
  int absDelta = abs(delta);
  if (absDelta >= vH) return;
  uint32_t newPxCol[vH];
  int newDelta;
  int stop = vH;
  int start = 0;
  if (wrap) newDelta = (delta + vH) % vH; // +rows in case delta < 0
  else {
    if (delta < 0) start = absDelta;
    stop = vH - absDelta;
    newDelta = delta > 0 ? delta : 0;
  }
  for (int x = 0; x < vW; x++) {
    for (int y = 0; y < stop; y++) {
      int srcY = y + newDelta;
      if (wrap) srcY %= vH; // Wrap using modulo when `wrap` is true
      newPxCol[y] = getPixelColorRaw(XY(x, srcY));
    }
    for (int y = 0; y < stop; y++) setPixelColorRaw(XY(x, y + start), newPxCol[y]);
  }
}

// move() - move all pixels in desired direction delta number of pixels
// @param dir direction: 0=left, 1=left-up, 2=up, 3=right-up, 4=right, 5=right-down, 6=down, 7=left-down
// @param delta number of pixels to move
// @param wrap around
void Segment::move(unsigned dir, unsigned delta, bool wrap) const {
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

void Segment::drawCircle(uint16_t cx, uint16_t cy, uint8_t radius, uint32_t col, bool soft) const {
  if (!isActive() || radius == 0) return; // not active
  if (soft) {
    // Xiaolin Wu’s algorithm
    const int rsq = radius*radius;
    int x = 0;
    int y = radius;
    unsigned oldFade = 0;
    while (x < y) {
      float yf = sqrtf(float(rsq - x*x)); // needs to be floating point
      uint8_t fade = float(0xFF) * (ceilf(yf) - yf); // how much color to keep
      if (oldFade > fade) y--;
      oldFade = fade;
      int px, py;
      for (uint8_t i = 0; i < 16; i++) {
          int swaps = (i & 0x4 ? 1 : 0); // 0,  0,  0,  0,  1,  1,  1,  1,  0,  0,  0,  0,  1,  1,  1,  1
          int adj =  (i < 8) ? 0 : 1;    // 0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  1,  1,  1,  1
          int dx = (i & 1) ? -1 : 1;     // 1, -1,  1, -1,  1, -1,  1, -1,  1, -1,  1, -1,  1, -1,  1, -1
          int dy = (i & 2) ? -1 : 1;     // 1,  1, -1, -1,  1,  1, -1, -1,  1,  1, -1, -1,  1,  1, -1, -1
          if (swaps) {
              px = cx + (y - adj) * dx;
              py = cy + x * dy;
          } else {
              px = cx + x * dx;
              py = cy + (y - adj) * dy;
          }
          uint32_t pixCol = getPixelColorXY(px, py);
          setPixelColorXY(px, py, adj ?
              color_blend(pixCol, col, fade) :
              color_blend(col, pixCol, fade));
      }
      x++;
    }
  } else {
    // Bresenham’s Algorithm
    int d = 3 - (2*radius);
    int y = radius, x = 0;
    while (y >= x) {
    for (int i = 0; i < 4; i++) {
        int dx = (i & 1) ? -x : x;
        int dy = (i & 2) ? -y : y;
        setPixelColorXY(cx + dx, cy + dy, col);
        setPixelColorXY(cx + dy, cy + dx, col);
    }
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
void Segment::fillCircle(uint16_t cx, uint16_t cy, uint8_t radius, uint32_t col, bool soft) const {
  if (!isActive() || radius == 0) return; // not active
  const int vW = vWidth();   // segment width in logical pixels (can be 0 if segment is inactive)
  const int vH = vHeight();  // segment height in logical pixels (is always >= 1)
  // draw soft bounding circle
  if (soft) drawCircle(cx, cy, radius, col, soft);
  // fill it
  for (int y = -radius; y <= radius; y++) {
    for (int x = -radius; x <= radius; x++) {
      if (x * x + y * y <= radius * radius &&
          int(cx)+x >= 0 && int(cy)+y >= 0 &&
          int(cx)+x < vW && int(cy)+y < vH)
        setPixelColorXY(cx + x, cy + y, col);
    }
  }
}

//line function
void Segment::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint32_t c, bool soft) const {
  if (!isActive()) return; // not active
  const int vW = vWidth();   // segment width in logical pixels (can be 0 if segment is inactive)
  const int vH = vHeight();  // segment height in logical pixels (is always >= 1)
  if (x0 >= vW || x1 >= vW || y0 >= vH || y1 >= vH) return;

  const int dx = abs(x1-x0), sx = x0<x1 ? 1 : -1; // x distance & step
  const int dy = abs(y1-y0), sy = y0<y1 ? 1 : -1; // y distance & step

  // single pixel (line length == 0)
  if (dx+dy == 0) {
    setPixelColorXY(x0, y0, c);
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
      uint8_t keep = float(0xFF) * (intersectY-int(intersectY)); // how much color to keep
      uint8_t seep = 0xFF - keep; // how much background to keep
      int y = int(intersectY);
      if (steep) std::swap(x,y);  // temporaryly swap if steep
      // pixel coverage is determined by fractional part of y co-ordinate
      blendPixelColorXY(x, y, c, seep);
      blendPixelColorXY(x+int(steep), y+int(!steep), c, keep);
      intersectY += gradient;
      if (steep) std::swap(x,y);  // restore if steep
    }
  } else {
    // Bresenham's algorithm
    int err = (dx>dy ? dx : -dy)/2;   // error direction
    for (;;) {
      setPixelColorXY(x0, y0, c);
      if (x0==x1 && y0==y1) break;
      int e2 = err;
      if (e2 >-dx) { err -= dy; x0 += sx; }
      if (e2 < dy) { err += dx; y0 += sy; }
    }
  }
}

// Segment::wu_pixel implementation is next

#define WU_WEIGHT(a,b) ((uint8_t) (((a)*(b)+(a)+(b))>>8))
void Segment::wu_pixel(uint32_t x, uint32_t y, CRGB c) const {      //awesome wu_pixel procedure by reddit u/sutaburosu
  if (!isActive()) return; // not active
  // extract the fractional parts and derive their inverses
  unsigned xx = x & 0xff, yy = y & 0xff, ix = 255 - xx, iy = 255 - yy;
  // calculate the intensities for each affected pixel
  uint8_t wu[4] = {WU_WEIGHT(ix, iy), WU_WEIGHT(xx, iy),
                   WU_WEIGHT(ix, yy), WU_WEIGHT(xx, yy)};
  // multiply the intensities by the colour, and saturating-add them to the pixels
  for (int i = 0; i < 4; i++) {
    int wu_x = (x >> 8) + (i & 1);        // precalculate x
    int wu_y = (y >> 8) + ((i >> 1) & 1); // precalculate y
    CRGB led = getPixelColorXY(wu_x, wu_y);
    CRGB oldLed = led;
    led.r = qadd8(led.r, c.r * wu[i] >> 8);
    led.g = qadd8(led.g, c.g * wu[i] >> 8);
    led.b = qadd8(led.b, c.b * wu[i] >> 8);
    if (led != oldLed) setPixelColorXY(wu_x, wu_y, led); // don't repaint if same color
  }
}
#undef WU_WEIGHT

#include "src/font/console_font_4x6.h"
#include "src/font/console_font_5x8.h"
#include "src/font/console_font_5x12.h"
#include "src/font/console_font_6x8.h"
#include "src/font/console_font_7x9.h"

// Pure glyph index calculator (inline for speed)
inline int32_t FontManager::getGlyphIndex(uint32_t unicode, uint8_t first, uint8_t last, uint32_t firstUnicode) {
  if (unicode <= LAST_ASCII_CHAR) {
    if (unicode >= first && unicode <= last) return unicode - first;
  } else if (firstUnicode > 0 && unicode >= firstUnicode) {
    uint32_t adjusted = unicode - firstUnicode + LAST_ASCII_CHAR + 1;
    if (adjusted >= first && adjusted <= last) return adjusted - first;
  }
  return -1;
}

// Parse header from either flash or RAM (IDENTICAL format)
bool FontManager::parseHeader(const ByteReader& reader, FontHeader& hdr) {
  if (reader.readByte(0) != 0x57) return false; // Magic check
  hdr.height = reader.readByte(1);
  hdr.width = reader.readByte(2);
  hdr.spacing = reader.readByte(3);
  hdr.flags = reader.readByte(4);
  hdr.first = reader.readByte(5);
  hdr.last = reader.readByte(6);
  // [7] reserved: 0x00
  hdr.firstUnicode = reader.readUInt32LE(8);
  return true;
}

// Get glyph width
uint8_t FontManager::getGlyphWidth(uint32_t unicode) {
  if (!_headerValid) return 0;
  int32_t idx = getGlyphIndex(unicode, _cachedHeader.first, _cachedHeader.last, _cachedHeader.firstUnicode);
  if (idx < 0) return 0;

  // For flash fonts, read from width table
  if (_useFlashFont) {
    if (_cachedHeader.flags & 0x01) {
      return _reader->readByte(FONT_HEADER_SIZE + idx);
    } else {
      return _cachedHeader.width;
    }
  }

  // For cached fonts, look up in registry
  if (!_segment->data) return 0;
  SegmentFontMetadata* meta = (SegmentFontMetadata*)_segment->data;
  GlyphEntry* registry = (GlyphEntry*)(_segment->data + sizeof(SegmentFontMetadata));

  for (uint8_t k = 0; k < meta->glyphCount; k++) {
    if (registry[k].code == idx) {
      return registry[k].width;
    }
  }

  return 0; // Not found in cache
}

// Get glyph bitmap (uses cached header and reader)
const uint8_t* FontManager::getGlyphBitmap(uint32_t unicode, uint8_t& outWidth, uint8_t& outHeight) {
  if (!_reader || !_fontBase) return nullptr;

  int32_t idx = getGlyphIndex(unicode, _cachedHeader.first, _cachedHeader.last, _cachedHeader.firstUnicode);
  if (idx < 0) return nullptr;
  // For flash fonts, calculate offset normally
  if (_useFlashFont) {
    uint8_t numGlyphs = _cachedHeader.last - _cachedHeader.first + 1;
    outHeight = _cachedHeader.height;
    size_t offset = FONT_HEADER_SIZE; // Start after header

    if (_cachedHeader.flags & 0x01) {
      outWidth = _reader->readByte(FONT_HEADER_SIZE + idx);
      offset += numGlyphs;

      for (int32_t i = 0; i < idx; i++) {
        uint8_t w = _reader->readByte(FONT_HEADER_SIZE + i);
        uint16_t bits = w * _cachedHeader.height;
        offset += (bits + 7) / 8;
      }
    } else {
      outWidth = _cachedHeader.width;
      uint16_t bitsPerGlyph = _cachedHeader.width * _cachedHeader.height;
      uint16_t bytesPerGlyph = (bitsPerGlyph + 7) / 8;
      offset += idx * bytesPerGlyph;
    }

    return _fontBase + offset;
  }

  // For cached fonts, use registry lookup
  if (!_segment->data) return nullptr;
  
  SegmentFontMetadata* meta = (SegmentFontMetadata*)_segment->data;
  GlyphEntry* registry = (GlyphEntry*)(_segment->data + sizeof(SegmentFontMetadata));
  
  // Find glyph in registry
  uint32_t bitmapOffset = 0;
  for (uint8_t k = 0; k < meta->glyphCount; k++) {
    if (registry[k].code == idx) {
      outWidth = registry[k].width;
      outHeight = registry[k].height;
      
      // Bitmap starts after: metadata + registry + header
      const uint8_t* bitmap = _segment->data + sizeof(SegmentFontMetadata) +(meta->glyphCount * sizeof(GlyphEntry)) + FONT_HEADER_SIZE + bitmapOffset;
      return bitmap;
    }
    
    // Accumulate offset to next glyph
    uint16_t bits = registry[k].width * registry[k].height;
    bitmapOffset += (bits + 7) / 8;
  }
  
  return nullptr; // Glyph not found in cache
}

void FontManager::scanAvailableFonts() {
  SegmentFontMetadata* meta = getMetadata();
  if (!meta) return;
  
  meta->availableFonts = 0;

  
  
  for (int i = 0; i < 5; i++) {
    char fileName[16];
    strcpy_P(fileName, PSTR("/font"));
    if (i > 0) sprintf(fileName + 5, "%d", i);
    strcat_P(fileName, PSTR(".wbf"));
    if (WLED_FS.exists(fileName)) {
      meta->availableFonts |= (1 << i);
    }
  }
  meta->fontsScanned = 1;
}

bool FontManager::loadFont(uint8_t fontNum, bool useFile) {
  _fontNum = fontNum;
  _useFlashFont = !useFile;
  switch (_fontNum) {
    default:
    case 0: _flashFont = console_font_4x6;    break;
    case 1: _flashFont = console_font_5x8;  break;
    case 2: _flashFont = console_font_6x8;  break;
    case 3: _flashFont = console_font_7x9;  break;
    case 4: _flashFont = console_font_5x12; break;
  }
  invalidateHeader();

  if (_useFlashFont) {
    updateReader();
    return true;
  }
  // File font requested
  SegmentFontMetadata* meta = getMetadata(); // is null if data is not allocated yet
  // Ensure segment data exists
  if (!meta) {
    if (!_segment->allocateData(sizeof(SegmentFontMetadata))) {
      return false;
    }
    meta = getMetadata();
    memset(meta, 0, sizeof(SegmentFontMetadata));
    meta->cachedFontNum = 0xFF;
    meta->fontsScanned = 0;
  }

  // Scan filesystem only if not already scanned
  if (!meta->fontsScanned) {
    scanAvailableFonts();
  }
  
  // Determine which font to actually use (with fallback)
  uint8_t fontToUse = fontNum;
  
  // Check if requested font is available
  if (!(meta->availableFonts & (1 << fontNum))) {
    // Not available - find first available font
    fontToUse = 0xFF;
    for (int i = 0; i < 5; i++) {
      if (meta->availableFonts & (1 << i)) {
        fontToUse = i;
        break;
      }
    }
    
    if (fontToUse == 0xFF) {
      _useFlashFont = true; // no custom fonts available, use flash font
      updateReader();
      return true;
    }
  }
  
  // Store the actual font being used
  _fontNum = fontToUse;
  
  // Check if the ACTUAL font to use has changed
  if (fontToUse != meta->cachedFontNum) {
    // Font changed - clear cache but preserve scan results
    uint8_t avail = meta->availableFonts;
    uint8_t scanned = meta->fontsScanned;
    
    if (!_segment->allocateData(sizeof(SegmentFontMetadata))) {
      return false;
    }
    meta = getMetadata();
    meta->availableFonts = avail;
    meta->cachedFontNum = fontToUse;
    meta->fontsScanned = scanned;
  }
  
  return true;
}

uint8_t FontManager::collectNeededCodes(const char* text, const FontHeader& hdr, uint8_t* outCodes, uint8_t maxCount) {
  uint8_t count = 0;
  
  // Pre-add numbers if caching
  if (_cacheNumbers) {
    const char* nums = "0123456789:. "; // TOD: use printf
    for (const char* p = nums; *p && count < maxCount; p++) {
      int32_t idx = getGlyphIndex(*p, hdr.first, hdr.last, hdr.firstUnicode);
      if (idx >= 0 && idx < 256) {
        outCodes[count++] = idx;
      }
    }
  }
  
  // Parse text
  size_t i = 0, len = strlen(text);
  while (i < len && count < maxCount) {
    uint8_t charLen;
    uint32_t unicode = utf8_decode(&text[i], &charLen);
    if (!charLen || charLen > 4) break;
    i += charLen;
    
    int32_t idx = getGlyphIndex(unicode, hdr.first, hdr.last, hdr.firstUnicode);
    if (idx < 0) {
      idx = getGlyphIndex('?', hdr.first, hdr.last, hdr.firstUnicode);
    }
    
    if (idx >= 0 && idx < 256) {
      // Add if unique
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

void FontManager::prepare(const char* text) {
  if (!text) return;

  // Helper to ensure header is valid
  auto checkHeader = [this]() -> bool {
    if (!_headerValid) {
      updateReader();
      if (_reader && parseHeader(*_reader, _cachedHeader)) {
        _headerValid = true;
        return true;
      }
      return false;
    }
    return true;
  };

  // Flash fonts - just validate header and done
  if (_useFlashFont) {
    checkHeader();
    return;
  }

  // Check if cache exists
  if (!_segment->data) {
    rebuildCache(text);
    return;
  }
  
  SegmentFontMetadata* meta = getMetadata();
  
  // If glyphCount is 0, cache is empty - rebuild
  if (meta->glyphCount == 0) {
    rebuildCache(text);
    return;
  }
  
  // Validate header from existing cache
  if (!checkHeader()) {
    rebuildCache(text);
    return;
  }

  // Check if all needed glyphs are present
  uint8_t neededCodes[64];
  uint8_t neededCount = collectNeededCodes(text, _cachedHeader, neededCodes, 64);

  GlyphEntry* registry = (GlyphEntry*)(_segment->data + sizeof(SegmentFontMetadata));
  
  for (uint8_t k = 0; k < neededCount; k++) {
    // Look up in registry
    bool found = false;
    for (uint8_t j = 0; j < meta->glyphCount; j++) {
      if (registry[j].code == neededCodes[k]) {
        found = true;
        break;
      }
    }

    if (!found) {
      // Missing glyph - rebuild cache
      rebuildCache(text);
      return;
    }
  }
  
  // All glyphs present, cache is valid
}

void FontManager::rebuildCache(const char* text) {
  if (_useFlashFont || !text) return;

  // Preserve metadata
  SegmentFontMetadata savedMeta = {0, 0xFF, 0, 0};
  if (_segment->data) {
    memcpy(&savedMeta, _segment->data, sizeof(SegmentFontMetadata));
  }

  // Build filename from font number  TODO: make file name generation a function
  char fileName[16];
  strcpy_P(fileName, PSTR("/font"));
  if (_fontNum > 0) sprintf(fileName + 5, "%d", _fontNum);
  strcat_P(fileName, PSTR(".wbf"));
  #ifdef CONFIG_IDF_TARGET_ESP32C3
  while (!BusManager::canAllShow()) yield(); // accessing FS causes glitches due to RMT issue on C3 TODO: remove this when fixed
  #endif
  File file = WLED_FS.open(fileName, "r");

  // Fallback logic - try other available fonts
  if (!file) {
    SegmentFontMetadata* meta = getMetadata();
    if (meta) {
      for (int i = 0; i < 5; i++) {
        if (i == _fontNum) continue; // Already tried this one
        if (meta->availableFonts & (1 << i)) {
          strcpy_P(fileName, PSTR("/font"));
          if (i > 0) sprintf(fileName + 5, "%d", i);
          strcat_P(fileName, PSTR(".wbf"));
          
          file = WLED_FS.open(fileName, "r");
          if (file) {
            _fontNum = i; // Update to fallback font
            break;
          }
        }
      }
    }
  }
  
  if (!file) return;

  // Read header from file
  FontHeader fileHdr;
  if (file.read() != 'W') { file.close(); return; }
  // TODO: check if this works: file.read((uint8_t*)&fileHdr, 10); // Read remaining 10 bytes of header
  fileHdr.height = file.read();
  fileHdr.width = file.read();
  fileHdr.spacing = file.read();
  fileHdr.flags = file.read();
  fileHdr.first = file.read();
  fileHdr.last = file.read();
  file.read(); // skip byte 7 (reserved)
  file.read((uint8_t*)&fileHdr.firstUnicode, 4);

  // Collect needed glyphs (no need to sort!)
  uint8_t neededCodes[64];
  uint8_t neededCount = collectNeededCodes(text, fileHdr, neededCodes, 64);

  uint8_t numGlyphs = fileHdr.last - fileHdr.first + 1;
  uint8_t widthTable[numGlyphs];
  
  // Read width table
  if (fileHdr.flags & 0x01) {
    file.read(widthTable, numGlyphs);
  } else {
    for (uint8_t k = 0; k < numGlyphs; k++) {
      widthTable[k] = fileHdr.width;
    }
  }
  
  // Calculate size: metadata + registry + header + bitmaps
  uint32_t fileDataStart = FONT_HEADER_SIZE + ((fileHdr.flags & 0x01) ? numGlyphs : 0);
  size_t ramFontSize = sizeof(SegmentFontMetadata) + (neededCount * sizeof(GlyphEntry)) + FONT_HEADER_SIZE; // Just the header, no width table needed
  
  for (uint8_t k = 0; k < neededCount; k++) {
    uint8_t code = neededCodes[k];
    if (code < numGlyphs) {
      uint16_t bits = widthTable[code] * fileHdr.height;
      ramFontSize += (bits + 7) / 8;
    }
  }
  
  // Allocate RAM
  if (!_segment->allocateData(ramFontSize)) {
    file.close();
    return;
  }
  
  // Write metadata
  SegmentFontMetadata* meta = (SegmentFontMetadata*)_segment->data;
  meta->availableFonts = savedMeta.availableFonts;
  meta->cachedFontNum = savedMeta.cachedFontNum;
  meta->fontsScanned = savedMeta.fontsScanned;
  meta->glyphCount = neededCount;
  
  uint8_t* ptr = _segment->data + sizeof(SegmentFontMetadata);
  
  // Write registry (GlyphEntry array)
  GlyphEntry* registry = (GlyphEntry*)ptr;
  for (uint8_t k = 0; k < neededCount; k++) {
    uint8_t code = neededCodes[k];
    registry[k].code = code;
    registry[k].width = widthTable[code];
    registry[k].height = fileHdr.height;
  }
  ptr += neededCount * sizeof(GlyphEntry);
  
  // Write font header (for compatibility and easy access to spacing/firstUnicode)
  file.seek(0); // Go back to start of file
  file.read(ptr, FONT_HEADER_SIZE); // could also write it from fileHdr, but this is simpler
  ptr += FONT_HEADER_SIZE;
  
  // Write bitmap data in registry order (no sorting needed)
  for (uint8_t k = 0; k < neededCount; k++) {
    uint8_t code = neededCodes[k];
    uint16_t bits = widthTable[code] * fileHdr.height;
    uint16_t bytes = (bits + 7) / 8;
    
    // Calculate file offset
    uint32_t offset = fileDataStart;
    for (uint8_t j = 0; j < code; j++) {
      uint16_t b = widthTable[j] * fileHdr.height;
      offset += (b + 7) / 8;
    }
    
    // Read from file
    file.seek(offset);
    file.read(ptr, bytes);
    ptr += bytes;
  }
  
  file.close();
  memcpy(&_cachedHeader, &fileHdr, sizeof(FontHeader));
  _headerValid = true;  // Mark as valid, we just loaded it directly
  updateReader();  // Set up reader for cached access
}

void FontManager::drawCharacter(uint32_t unicode, int16_t x, int16_t y, uint32_t color, uint32_t col2, int8_t rotate) {
  if (!_reader || !_fontBase) return;
  
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
      
      // Direct pointer access for speed
      uint8_t byteVal = _reader->readByte((bitmap - _fontBase) + bytePos);
      
      if ((byteVal >> bitPos) & 1) {
        int x0, y0;
        switch (rotate) {
          case -1: x0 = x + (h-1) - row; y0 = y + col;         break;
          case -2:
          case  2: x0 = x + (w-1) - col; y0 = y + (h-1) - row; break;
          case  1: x0 = x + row;         y0 = y + (w-1) - col; break;
          default: x0 = x + col;         y0 = y + row;         break;
        }
        
        if (x0 >= 0 && x0 < (int)_segment->vWidth() && 
            y0 >= 0 && y0 < (int)_segment->vHeight()) {
          _segment->setPixelColorXYRaw(x0, y0, c.color32);
        }
      }
      bitIndex++;
    }
  }
}

#endif // WLED_DISABLE_2D