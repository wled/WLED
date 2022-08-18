/*
  WS2812FX_fcn.cpp contains all utility functions
  Harm Aldick - 2016
  www.aldick.org
  LICENSE
  The MIT License (MIT)
  Copyright (c) 2016  Harm Aldick
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

  Modified heavily for WLED
*/
#include "wled.h"
#include "FX.h"
#include "palettes.h"

/*
  Custom per-LED mapping has moved!

  Create a file "ledmap.json" using the edit page.

  this is just an example (30 LEDs). It will first set all even, then all uneven LEDs.
  {"map":[
  0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28,
  1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29]}

  another example. Switches direction every 5 LEDs.
  {"map":[
  0, 1, 2, 3, 4, 9, 8, 7, 6, 5, 10, 11, 12, 13, 14,
  19, 18, 17, 16, 15, 20, 21, 22, 23, 24, 29, 28, 27, 26, 25]}
*/

//factory defaults LED setup
//#define PIXEL_COUNTS 30, 30, 30, 30
//#define DATA_PINS 16, 1, 3, 4
//#define DEFAULT_LED_TYPE TYPE_WS2812_RGB

#ifndef PIXEL_COUNTS
  #define PIXEL_COUNTS DEFAULT_LED_COUNT
#endif

#ifndef DATA_PINS
  #define DATA_PINS LEDPIN
#endif

#ifndef DEFAULT_LED_TYPE
  #define DEFAULT_LED_TYPE TYPE_WS2812_RGB
#endif

#ifndef DEFAULT_LED_COLOR_ORDER
  #define DEFAULT_LED_COLOR_ORDER COL_ORDER_GRB  //default to GRB
#endif


#if MAX_NUM_SEGMENTS < WLED_MAX_BUSSES
  #error "Max segments must be at least max number of busses!"
#endif


///////////////////////////////////////////////////////////////////////////////
// Segment class implementation
///////////////////////////////////////////////////////////////////////////////
uint16_t Segment::_usedSegmentData = 0U; // amount of RAM all segments use for their data[]
CRGB    *Segment::_globalLeds = nullptr;

// copy constructor
Segment::Segment(const Segment &orig) {
  DEBUG_PRINTLN(F("-- Copy segment constructor --"));
  memcpy(this, &orig, sizeof(Segment));
  name = nullptr;
  data = nullptr;
  _dataLen = 0;
  _t = nullptr;
  if (leds && !Segment::_globalLeds) leds = nullptr;
  if (orig.name) { name = new char[strlen(orig.name)+1]; if (name) strcpy(name, orig.name); }
  if (orig.data) { if (allocateData(orig._dataLen)) memcpy(data, orig.data, orig._dataLen); }
  if (orig._t)   { _t = new Transition(orig._t->_dur, orig._t->_briT, orig._t->_cctT, orig._t->_colorT); }
  if (orig.leds && !Segment::_globalLeds) { leds = (CRGB*)malloc(sizeof(CRGB)*length()); if (leds) memcpy(leds, orig.leds, sizeof(CRGB)*length()); }
}

// move constructor
Segment::Segment(Segment &&orig) noexcept {
  DEBUG_PRINTLN(F("-- Move segment constructor --"));
  memcpy(this, &orig, sizeof(Segment));
  orig.name = nullptr;
  orig.data = nullptr;
  orig._dataLen = 0;
  orig._t   = nullptr;
  orig.leds = nullptr;
}

// copy assignment
Segment& Segment::operator= (const Segment &orig) {
  DEBUG_PRINTLN(F("-- Copying segment --"));
  if (this != &orig) {
    // clean destination
    if (name) delete[] name;
    if (_t)   delete _t;
    if (leds && !Segment::_globalLeds) free(leds);
    deallocateData();
    // copy source
    memcpy(this, &orig, sizeof(Segment));
    // erase pointers to allocated data
    name = nullptr;
    data = nullptr;
    _dataLen = 0;
    _t = nullptr;
    if (!Segment::_globalLeds) leds = nullptr;
    // copy source data
    if (orig.name) { name = new char[strlen(orig.name)+1]; if (name) strcpy(name, orig.name); }
    if (orig.data) { if (allocateData(orig._dataLen)) memcpy(data, orig.data, orig._dataLen); }
    if (orig._t)   { _t = new Transition(orig._t->_dur, orig._t->_briT, orig._t->_cctT, orig._t->_colorT); }
    if (orig.leds && !Segment::_globalLeds) { leds = (CRGB*)malloc(sizeof(CRGB)*length()); if (leds) memcpy(leds, orig.leds, sizeof(CRGB)*length()); }
  }
  return *this;
}

// move assignment
Segment& Segment::operator= (Segment &&orig) noexcept {
  DEBUG_PRINTLN(F("-- Moving segment --"));
  if (this != &orig) {
    if (name) delete[] name; // free old name
    deallocateData(); // free old runtime data
    if (_t) delete _t;
    if (leds && !Segment::_globalLeds) free(leds);
    memcpy(this, &orig, sizeof(Segment));
    orig.name = nullptr;
    orig.data = nullptr;
    orig._dataLen = 0;
    orig._t   = nullptr;
    orig.leds = nullptr;
  }
  return *this;
}

bool Segment::allocateData(size_t len) {
  if (data && _dataLen == len) return true; //already allocated
  deallocateData();
  if (Segment::getUsedSegmentData() + len > MAX_SEGMENT_DATA) return false; //not enough memory
  // if possible use SPI RAM on ESP32
  #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_PSRAM)
  if (psramFound())
    data = (byte*) ps_malloc(len);
  else
  #endif
    data = (byte*) malloc(len);
  if (!data) return false; //allocation failed
  Segment::addUsedSegmentData(len);
  _dataLen = len;
  memset(data, 0, len);
  return true;
}

void Segment::deallocateData() {
  if (!data) return;
  free(data);
  data = nullptr;
  Segment::addUsedSegmentData(-_dataLen);
  _dataLen = 0;
}

/** 
  * If reset of this segment was requested, clears runtime
  * settings of this segment.
  * Must not be called while an effect mode function is running
  * because it could access the data buffer and this method 
  * may free that data buffer.
  */
void Segment::resetIfRequired() {
  if (reset) {
    if (leds && !Segment::_globalLeds) { free(leds); leds = nullptr; }
    //if (_t) { delete _t; _t = nullptr; transitional = false; }
    next_time = 0; step = 0; call = 0; aux0 = 0; aux1 = 0; 
    reset = false; // setOption(SEG_OPTION_RESET, false);
  }
}

void Segment::setUpLeds() {
  // deallocation happens in resetIfRequired() as it is called when segment changes or in destructor
  if (Segment::_globalLeds)
    #ifndef WLED_DISABLE_2D
    leds = &Segment::_globalLeds[start + startY*strip.matrixWidth]; // TODO: remove this hack
    #else
    leds = &Segment::_globalLeds[start];
    #endif
  else if (!leds) {
    #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_PSRAM)
    if (psramFound())
      leds = (CRGB*)ps_malloc(sizeof(CRGB)*length());
    else
    #endif
      leds = (CRGB*)malloc(sizeof(CRGB)*length());
}

CRGBPalette16 &Segment::loadPalette(CRGBPalette16 &targetPalette, uint8_t pal) {
  static unsigned long _lastPaletteChange = 0; // perhaps it should be per segment
  static CRGBPalette16 randomPalette = CRGBPalette16(DEFAULT_COLOR);
  byte tcp[72];
  if (pal < 245 && pal > GRADIENT_PALETTE_COUNT+13) pal = 0;
  if (pal > 245 && (strip.customPalettes.size() == 0 || 255U-pal > strip.customPalettes.size()-1)) pal = 0;
  //default palette. Differs depending on effect
  if (pal == 0) switch (mode) {
    case FX_MODE_FIRE_2012  : pal = 35; break; // heat palette
    case FX_MODE_COLORWAVES : pal = 26; break; // landscape 33
    case FX_MODE_FILLNOISE8 : pal =  9; break; // ocean colors
    case FX_MODE_NOISE16_1  : pal = 20; break; // Drywet
    case FX_MODE_NOISE16_2  : pal = 43; break; // Blue cyan yellow
    case FX_MODE_NOISE16_3  : pal = 35; break; // heat palette
    case FX_MODE_NOISE16_4  : pal = 26; break; // landscape 33
    case FX_MODE_GLITTER    : pal = 11; break; // rainbow colors
    case FX_MODE_SUNRISE    : pal = 35; break; // heat palette
    case FX_MODE_FLOW       : pal =  6; break; // party
  }
  switch (pal) {
    case 0: //default palette. Exceptions for specific effects above
      targetPalette = PartyColors_p; break;
    case 1: //periodically replace palette with a random one. Doesn't work with multiple FastLED segments
      if (millis() - _lastPaletteChange > 5000 /*+ ((uint32_t)(255-intensity))*100*/) {
        randomPalette = CRGBPalette16(
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)));
        _lastPaletteChange = millis();
      }
      targetPalette = randomPalette; break;
    case 2: {//primary color only
      CRGB prim = gamma32(colors[0]);
      targetPalette = CRGBPalette16(prim); break;}
    case 3: {//primary + secondary
      CRGB prim = gamma32(colors[0]);
      CRGB sec  = gamma32(colors[1]);
      targetPalette = CRGBPalette16(prim,prim,sec,sec); break;}
    case 4: {//primary + secondary + tertiary
      CRGB prim = gamma32(colors[0]);
      CRGB sec  = gamma32(colors[1]);
      CRGB ter  = gamma32(colors[2]);
      targetPalette = CRGBPalette16(ter,sec,prim); break;}
    case 5: {//primary + secondary (+tert if not off), more distinct
      CRGB prim = gamma32(colors[0]);
      CRGB sec  = gamma32(colors[1]);
      if (colors[2]) {
        CRGB ter = gamma32(colors[2]);
        targetPalette = CRGBPalette16(prim,prim,prim,prim,prim,sec,sec,sec,sec,sec,ter,ter,ter,ter,ter,prim);
      } else {
        targetPalette = CRGBPalette16(prim,prim,prim,prim,prim,prim,prim,prim,sec,sec,sec,sec,sec,sec,sec,sec);
      }
      break;}
    case 6: //Party colors
      targetPalette = PartyColors_p; break;
    case 7: //Cloud colors
      targetPalette = CloudColors_p; break;
    case 8: //Lava colors
      targetPalette = LavaColors_p; break;
    case 9: //Ocean colors
      targetPalette = OceanColors_p; break;
    case 10: //Forest colors
      targetPalette = ForestColors_p; break;
    case 11: //Rainbow colors
      targetPalette = RainbowColors_p; break;
    case 12: //Rainbow stripe colors
      targetPalette = RainbowStripeColors_p; break;
    default: //progmem palettes
      if (pal>245) {
        targetPalette = strip.customPalettes[255-pal]; // we checked bounds above
      } else {
        memcpy_P(tcp, (byte*)pgm_read_dword(&(gGradientPalettes[pal-13])), 72);
        targetPalette.loadDynamicGradientPalette(tcp);
      }
      break;
  }
  return targetPalette;
}

void Segment::startTransition(uint16_t dur) {
  if (transitional || _t) return; // already in transition no need to store anything

  // starting a transition has to occur before change so we get current values 1st
  uint8_t _briT = currentBri(on ? opacity : 0);
  uint8_t _cctT = currentBri(cct, true);
  CRGBPalette16 _palT; loadPalette(_palT, palette);
  uint8_t _modeP = mode;
  uint32_t _colorT[NUM_COLORS];
  for (size_t i=0; i<NUM_COLORS; i++) _colorT[i] = currentColor(i, colors[i]);

  if (!_t) _t = new Transition(dur); // no previous transition running
  if (!_t) return; // failed to allocate data
  _t->_briT  = _briT;
  _t->_cctT  = _cctT;
  _t->_palT  = _palT;
  _t->_modeP = _modeP;
  for (size_t i=0; i<NUM_COLORS; i++) _t->_colorT[i] = _colorT[i];
  transitional = true; // setOption(SEG_OPTION_TRANSITIONAL, true);
}

// transition progression between 0-65535
uint16_t Segment::progress() {
  if (!transitional || !_t) return 0xFFFFU;
  uint32_t timeNow = millis();
  if (timeNow - _t->_start > _t->_dur || _t->_dur == 0) return 0xFFFFU;
  return (timeNow - _t->_start) * 0xFFFFU / _t->_dur;
}

uint8_t Segment::currentBri(uint8_t briNew, bool useCct) {
  if (transitional && _t) {
    uint32_t prog = progress() + 1;
    if (useCct) return ((briNew * prog) + _t->_cctT * (0x10000 - prog)) >> 16;
    else        return ((briNew * prog) + _t->_briT * (0x10000 - prog)) >> 16;
  } else {
    return briNew;
  }
}

uint8_t Segment::currentMode(uint8_t newMode) {
  if (transitional && _t) {
    return _t->_modeP;
  } else {
    return newMode;
  }
}

uint32_t Segment::currentColor(uint8_t slot, uint32_t colorNew) {
  return transitional && _t ? color_blend(_t->_colorT[slot], colorNew, progress(), true) : colorNew;
}

CRGBPalette16 &Segment::currentPalette(CRGBPalette16 &targetPalette, uint8_t pal) {
  loadPalette(targetPalette, pal);
  if (transitional && _t && progress() < 0xFFFFU) {
    // blend palettes
    // there are about 255 blend passes of 48 "blends" to completely blend two palettes (in _dur time)
    // minimum blend time is 100ms maximum is 65535ms
    uint32_t timeMS = millis() - _t->_start;
    uint16_t noOfBlends = (255U * timeMS / _t->_dur) - _t->_prevPaletteBlends;
    for (int i=0; i<noOfBlends; i++, _t->_prevPaletteBlends++) nblendPaletteTowardPalette(_t->_palT, targetPalette, 48);
    targetPalette = _t->_palT; // copy transitioning/temporary palette
  }
  return targetPalette;
}

void Segment::handleTransition() {
  if (!transitional) return;
  unsigned long maxWait = millis() + 20;
  if (mode == FX_MODE_STATIC && next_time > maxWait) next_time = maxWait;
  if (progress() == 0xFFFFU) {
    if (_t) {
      if (_t->_modeP != mode) markForReset();
      delete _t;
      _t = nullptr;
    }
    transitional = false; // finish transitioning segment
  }
}

bool Segment::setColor(uint8_t slot, uint32_t c) { //returns true if changed
  if (slot >= NUM_COLORS || c == colors[slot]) return false;
  if (fadeTransition) startTransition(strip.getTransition()); // start transition prior to change
  colors[slot] = c;
  return true;
}

void Segment::setCCT(uint16_t k) {
  if (k > 255) { //kelvin value, convert to 0-255
    if (k < 1900)  k = 1900;
    if (k > 10091) k = 10091;
    k = (k - 1900) >> 5;
  }
  if (cct == k) return;
  if (fadeTransition) startTransition(strip.getTransition()); // start transition prior to change
  cct = k;
}

void Segment::setOpacity(uint8_t o) {
  if (opacity == o) return;
  if (fadeTransition) startTransition(strip.getTransition()); // start transition prior to change
  opacity = o;
}

void Segment::setOption(uint8_t n, bool val) {
  bool prevOn = on;
  if (fadeTransition && n == SEG_OPTION_ON && val != prevOn) startTransition(strip.getTransition()); // start transition prior to change
  if (val) options |=   0x01 << n;
  else     options &= ~(0x01 << n);
}

void Segment::setMode(uint8_t fx, bool loadDefaults) {
  // if we have a valid mode & is not reserved
  if (fx < strip.getModeCount() && strncmp_P("RSVD", strip.getModeData(fx), 4)) {
    if (fx != mode) {
      startTransition(strip.getTransition()); // set effect transitions
      //markForReset(); // transition will handle this
      mode = fx;

      // load default values from effect string
      if (loadDefaults) {
        int16_t sOpt;
        sOpt = extractModeDefaults(fx, "sx");   if (sOpt >= 0) speed     = sOpt;
        sOpt = extractModeDefaults(fx, "ix");   if (sOpt >= 0) intensity = sOpt;
        sOpt = extractModeDefaults(fx, "c1");   if (sOpt >= 0) custom1   = sOpt;
        sOpt = extractModeDefaults(fx, "c2");   if (sOpt >= 0) custom2   = sOpt;
        sOpt = extractModeDefaults(fx, "c3");   if (sOpt >= 0) custom3   = sOpt;
        sOpt = extractModeDefaults(fx, "mp12"); if (sOpt >= 0) map1D2D   = constrain(sOpt, 0, 7);
        sOpt = extractModeDefaults(fx, "ssim"); if (sOpt >= 0) soundSim  = constrain(sOpt, 0, 7);
        sOpt = extractModeDefaults(fx, "rev");  if (sOpt >= 0) reverse   = (bool)sOpt;
        sOpt = extractModeDefaults(fx, "mi");   if (sOpt >= 0) mirror    = (bool)sOpt; // NOTE: setting this option is a risky business
        sOpt = extractModeDefaults(fx, "rY");   if (sOpt >= 0) reverse_y = (bool)sOpt;
        sOpt = extractModeDefaults(fx, "mY");   if (sOpt >= 0) mirror_y  = (bool)sOpt; // NOTE: setting this option is a risky business
        sOpt = extractModeDefaults(fx, "pal");
        if (sOpt >= 0 && (size_t)sOpt < strip.getPaletteCount() + strip.customPalettes.size()) {
          if (sOpt != palette) {
            palette = sOpt;
          }
        }
      }
    }
  }
}

void Segment::setPalette(uint8_t pal) {
  if (pal < strip.getPaletteCount()) {
    if (pal != palette) {
      if (strip.paletteFade) startTransition(strip.getTransition());
      palette = pal;
    }
  }
}

// 2D matrix
uint16_t Segment::virtualWidth() const {
  uint16_t groupLen = groupLength();
  uint16_t vWidth = ((transpose ? height() : width()) + groupLen - 1) / groupLen;
  if (mirror) vWidth = (vWidth + 1) /2;  // divide by 2 if mirror, leave at least a single LED
  return vWidth;
}

uint16_t Segment::virtualHeight() const {
  uint16_t groupLen = groupLength();
  uint16_t vHeight = ((transpose ? width() : height()) + groupLen - 1) / groupLen;
  if (mirror_y) vHeight = (vHeight + 1) /2;  // divide by 2 if mirror, leave at least a single LED
  return vHeight;
}

uint16_t Segment::nrOfVStrips() const {
  uint16_t vLen = 1;
#ifndef WLED_DISABLE_2D
  if (is2D()) {
    switch (map1D2D) {
      case M12_pBar:
        vLen = virtualWidth();
        break;
    }
  }
#endif
  return vLen;
}

// 1D strip
uint16_t Segment::virtualLength() const {
#ifndef WLED_DISABLE_2D
  if (is2D()) {
    uint16_t vW = virtualWidth();
    uint16_t vH = virtualHeight();
    uint16_t vLen = vW * vH; // use all pixels from segment
    switch (map1D2D) {
      case M12_pBar:
        vLen = vH;
        break;
      case M12_pCorner:
      case M12_pArc:
        vLen = max(vW,vH); // get the longest dimension
        break;
    }
    return vLen;
  }
#endif
  uint16_t groupLen = groupLength();
  uint16_t vLength = (length() + groupLen - 1) / groupLen;
  if (mirror) vLength = (vLength + 1) /2;  // divide by 2 if mirror, leave at least a single LED
  return vLength;
}

void IRAM_ATTR Segment::setPixelColor(int i, uint32_t col)
{
  int vStrip = i>>16; // hack to allow running on virtual strips (2D segment columns/rows)
  i &= 0xFFFF;

  if (i >= virtualLength() || i<0) return;  // if pixel would fall out of segment just exit

#ifndef WLED_DISABLE_2D
  if (is2D()) { // if this does not work use strip.isMatrix
    uint16_t vH = virtualHeight();  // segment height in logical pixels
    uint16_t vW = virtualWidth();
    switch (map1D2D) {
      case M12_Pixels:
        // use all available pixels as a long strip
        setPixelColorXY(i % vW, i / vW, col);
        break;
      case M12_pBar:
        // expand 1D effect vertically or have it play on virtual strips
        if (vStrip>0) setPixelColorXY(vStrip - 1, vH - i - 1, col);
        else          for (int x = 0; x < vW; x++) setPixelColorXY(x, vH - i - 1, col);
        break;
      case M12_pArc:
        // expand in circular fashion from center
        if (i==0)
          setPixelColorXY(0, 0, col);
        else {
          float step = HALF_PI / (2.85f*i);
          for (float rad = 0.0f; rad <= HALF_PI+step/2; rad += step) {
            // may want to try float version as well (with or without antialiasing)
            int x = roundf(sin_t(rad) * i);
            int y = roundf(cos_t(rad) * i);
            setPixelColorXY(x, y, col);
          }
        }
        break;
      case M12_pCorner:
        for (int x = 0; x <= i; x++) setPixelColorXY(x, i, col);
        for (int y = 0; y <  i; y++) setPixelColorXY(i, y, col);
        break;
    }
    return;
  } else if (strip.isMatrix && (width()==1 || height()==1)) { // TODO remove this hack
    // we have a vertical or horizontal 1D segment (WARNING: virtual...() may be transposed)
    int x = 0, y = 0;
    if (virtualHeight()>1) y = i;
    if (virtualWidth() >1) x = i;
    setPixelColorXY(x, y, col);
    return;
  }
#endif

  if (leds) leds[i] = col;

  uint16_t len = length();
  uint8_t _bri_t = currentBri(on ? opacity : 0);
  if (_bri_t < 255) {
    byte r = scale8(R(col), _bri_t);
    byte g = scale8(G(col), _bri_t);
    byte b = scale8(B(col), _bri_t);
    byte w = scale8(W(col), _bri_t);
    col = RGBW32(r, g, b, w);
  }

  // expand pixel (taking into account start, grouping, spacing [and offset])
  i = i * groupLength();
  if (reverse) { // is segment reversed?
    if (mirror) { // is segment mirrored?
      i = (len - 1) / 2 - i;  //only need to index half the pixels
    } else {
      i = (len - 1) - i;
    }
  }
  i += start; // starting pixel in a group

  // set all the pixels in the group
  for (int j = 0; j < grouping; j++) {
    uint16_t indexSet = i + ((reverse) ? -j : j);
    if (indexSet >= start && indexSet < stop) {
      if (mirror) { //set the corresponding mirrored pixel
        uint16_t indexMir = stop - indexSet + start - 1;          
        indexMir += offset; // offset/phase
        if (indexMir >= stop) indexMir -= len; // wrap
        strip.setPixelColor(indexMir, col);
      }
      indexSet += offset; // offset/phase
      if (indexSet >= stop) indexSet -= len; // wrap
      strip.setPixelColor(indexSet, col);
    }
  }
}

// anti-aliased normalized version of setPixelColor()
void Segment::setPixelColor(float i, uint32_t col, bool aa)
{
  int vStrip = int(i/10.0f); // hack to allow running on virtual strips (2D segment columns/rows)
  i -= int(i);

  if (i<0.0f || i>1.0f) return; // not normalized

  float fC = i * (virtualLength()-1);
  if (aa) {
    uint16_t iL = roundf(fC-0.49f);
    uint16_t iR = roundf(fC+0.49f);
    float    dL = (fC - iL)*(fC - iL);
    float    dR = (iR - fC)*(iR - fC);
    uint32_t cIL = getPixelColor(iL | (vStrip<<16));
    uint32_t cIR = getPixelColor(iR | (vStrip<<16));
    if (iR!=iL) {
      // blend L pixel
      cIL = color_blend(col, cIL, uint8_t(dL*255.0f));
      setPixelColor(iL | (vStrip<<16), cIL);
      // blend R pixel
      cIR = color_blend(col, cIR, uint8_t(dR*255.0f));
      setPixelColor(iR | (vStrip<<16), cIR);
    } else {
      // exact match (x & y land on a pixel)
      setPixelColor(iL | (vStrip<<16), col);
    }
  } else {
    setPixelColor(uint16_t(roundf(fC)) | (vStrip<<16), col);
  }
}

uint32_t Segment::getPixelColor(int i)
{
  int vStrip = i>>16;
  i &= 0xFFFF;

#ifndef WLED_DISABLE_2D
  if (is2D()) { // if this does not work use strip.isMatrix
    uint16_t vH = virtualHeight();  // segment height in logical pixels
    uint16_t vW = virtualWidth();
    switch (map1D2D) {
      case M12_Pixels:
        return getPixelColorXY(i % vW, i / vW);
        break;
      case M12_pBar:
        if (vStrip>0) return getPixelColorXY(vStrip - 1, vH - i -1);
        else          return getPixelColorXY(0, vH - i -1);
        break;
      case M12_pArc:
      case M12_pCorner:
        // use longest dimension
        return vW>vH ? getPixelColorXY(i, 0) : getPixelColorXY(0, i);
        break;
    }
    return 0;
  }
#endif

  if (leds) return RGBW32(leds[i].r, leds[i].g, leds[i].b, 0);

  if (reverse) i = virtualLength() - i - 1;
  i *= groupLength();
  i += start;
  /* offset/phase */
  i += offset;
  if (i >= stop) i -= length();
  return strip.getPixelColor(i);
}

uint8_t Segment::differs(Segment& b) const {
  uint8_t d = 0;
  if (start != b.start)         d |= SEG_DIFFERS_BOUNDS;
  if (stop != b.stop)           d |= SEG_DIFFERS_BOUNDS;
  if (offset != b.offset)       d |= SEG_DIFFERS_GSO;
  if (grouping != b.grouping)   d |= SEG_DIFFERS_GSO;
  if (spacing != b.spacing)     d |= SEG_DIFFERS_GSO;
  if (opacity != b.opacity)     d |= SEG_DIFFERS_BRI;
  if (mode != b.mode)           d |= SEG_DIFFERS_FX;
  if (speed != b.speed)         d |= SEG_DIFFERS_FX;
  if (intensity != b.intensity) d |= SEG_DIFFERS_FX;
  if (palette != b.palette)     d |= SEG_DIFFERS_FX;
  if (custom1 != b.custom1)     d |= SEG_DIFFERS_FX;
  if (custom2 != b.custom2)     d |= SEG_DIFFERS_FX;
  if (custom3 != b.custom3)     d |= SEG_DIFFERS_FX;
  if (startY != b.startY)       d |= SEG_DIFFERS_BOUNDS;
  if (stopY != b.stopY)         d |= SEG_DIFFERS_BOUNDS;

  //bit pattern: msb first: [transposed mirrorY reverseY] transitional (tbd) paused needspixelstate mirrored on reverse selected
  if ((options & 0b1111111110011110) != (b.options & 0b1111111110011110)) d |= SEG_DIFFERS_OPT;
  if ((options & 0x01) != (b.options & 0x01))                             d |= SEG_DIFFERS_SEL;
  
  for (uint8_t i = 0; i < NUM_COLORS; i++) if (colors[i] != b.colors[i])  d |= SEG_DIFFERS_COL;

  return d;
}

void Segment::refreshLightCapabilities() {
  uint8_t capabilities = 0x01;

  for (uint8_t b = 0; b < busses.getNumBusses(); b++) {
    Bus *bus = busses.getBus(b);
    if (bus == nullptr || bus->getLength()==0) break;
    if (!bus->isOk()) continue;
    if (bus->getStart() >= stop) continue;
    if (bus->getStart() + bus->getLength() <= start) continue;

    uint8_t type = bus->getType();
    if (type == TYPE_ANALOG_1CH || (!cctFromRgb && type == TYPE_ANALOG_2CH)) capabilities &= 0xFE; // does not support RGB
    if (bus->isRgbw()) capabilities |= 0x02; // segment supports white channel
    if (!cctFromRgb) {
      switch (type) {
        case TYPE_ANALOG_5CH:
        case TYPE_ANALOG_2CH:
          capabilities |= 0x04; //segment supports white CCT 
      }
    }
    if (correctWB && type != TYPE_ANALOG_1CH) capabilities |= 0x04; //white balance correction (uses CCT slider)
    uint8_t aWM = Bus::getAutoWhiteMode()<255 ? Bus::getAutoWhiteMode() : bus->getAWMode();
    bool whiteSlider = (aWM == RGBW_MODE_DUAL || aWM == RGBW_MODE_MANUAL_ONLY); // white slider allowed
    if (bus->isRgbw() && (whiteSlider || !(capabilities & 0x01))) capabilities |= 0x08; // allow white channel adjustments (AWM allows or is not RGB)
  }
  _capabilities = capabilities;
}

/*
 * Fills segment with color
 */
void Segment::fill(uint32_t c) {
  const uint16_t cols = is2D() ? virtualWidth() : virtualLength();
  const uint16_t rows = virtualHeight(); // will be 1 for 1D
  for(uint16_t y = 0; y < rows; y++) for (uint16_t x = 0; x < cols; x++) {
    if (is2D()) setPixelColorXY(x, y, c);
    else        setPixelColor(x, c);
  }
}

// Blends the specified color with the existing pixel color.
void Segment::blendPixelColor(int n, uint32_t color, uint8_t blend) {
  setPixelColor(n, color_blend(getPixelColor(n), color, blend));
}

// Adds the specified color with the existing pixel color perserving color balance.
void Segment::addPixelColor(int n, uint32_t color) {
  setPixelColor(n, color_add(getPixelColor(n), color));
}

void Segment::fadePixelColor(uint16_t n, uint8_t fade) {
  CRGB pix = CRGB(getPixelColor(n)).nscale8_video(fade);
  setPixelColor(n, pix);
}

/*
 * fade out function, higher rate = quicker fade
 */
void Segment::fade_out(uint8_t rate) {
  const uint16_t cols = is2D() ? virtualWidth() : virtualLength();
  const uint16_t rows = virtualHeight(); // will be 1 for 1D

  rate = (255-rate) >> 1;
  float mappedRate = float(rate) +1.1;

  uint32_t color = colors[1]; // SEGCOLOR(1); // target color
  int w2 = W(color);
  int r2 = R(color);
  int g2 = G(color);
  int b2 = B(color);

  for (uint16_t y = 0; y < rows; y++) for (uint16_t x = 0; x < cols; x++) {
    color = is2D() ? getPixelColorXY(x, y) : getPixelColor(x);
    int w1 = W(color);
    int r1 = R(color);
    int g1 = G(color);
    int b1 = B(color);

    int wdelta = (w2 - w1) / mappedRate;
    int rdelta = (r2 - r1) / mappedRate;
    int gdelta = (g2 - g1) / mappedRate;
    int bdelta = (b2 - b1) / mappedRate;

    // if fade isn't complete, make sure delta is at least 1 (fixes rounding issues)
    wdelta += (w2 == w1) ? 0 : (w2 > w1) ? 1 : -1;
    rdelta += (r2 == r1) ? 0 : (r2 > r1) ? 1 : -1;
    gdelta += (g2 == g1) ? 0 : (g2 > g1) ? 1 : -1;
    bdelta += (b2 == b1) ? 0 : (b2 > b1) ? 1 : -1;

    if (is2D()) setPixelColorXY(x, y, r1 + rdelta, g1 + gdelta, b1 + bdelta, w1 + wdelta);
    else        setPixelColor(x, r1 + rdelta, g1 + gdelta, b1 + bdelta, w1 + wdelta);
  }
}

// fades all pixels to black using nscale8()
void Segment::fadeToBlackBy(uint8_t fadeBy) {
  const uint16_t cols = is2D() ? virtualWidth() : virtualLength();
  const uint16_t rows = virtualHeight(); // will be 1 for 1D

  for (uint16_t y = 0; y < rows; y++) for (uint16_t x = 0; x < cols; x++) {
    if (is2D()) setPixelColorXY(x, y, CRGB(getPixelColorXY(x,y)).nscale8(255-fadeBy));
    else        setPixelColor(x, CRGB(getPixelColor(x)).nscale8(255-fadeBy));
  }
}

/*
 * blurs segment content, source: FastLED colorutils.cpp
 */
void Segment::blur(uint8_t blur_amount)
{
#ifndef WLED_DISABLE_2D
  if (is2D()) {
    // compatibility with 2D
    const uint16_t cols = virtualWidth();
    const uint16_t rows = virtualHeight();
    for (uint16_t i = 0; i < rows; i++) blurRow(i, blur_amount); // blur all rows
    for (uint16_t k = 0; k < cols; k++) blurCol(k, blur_amount); // blur all columns
    return;
  }
#endif
  uint8_t keep = 255 - blur_amount;
  uint8_t seep = blur_amount >> 1;
  CRGB carryover = CRGB::Black;
  for(uint16_t i = 0; i < virtualLength(); i++)
  {
    CRGB cur = CRGB(getPixelColor(i));
    CRGB part = cur;
    part.nscale8(seep);
    cur.nscale8(keep);
    cur += carryover;
    if(i > 0) {
      uint32_t c = getPixelColor(i-1);
      uint8_t r = R(c);
      uint8_t g = G(c);
      uint8_t b = B(c);
      setPixelColor(i-1, qadd8(r, part.red), qadd8(g, part.green), qadd8(b, part.blue));
    }
    setPixelColor(i,cur.red, cur.green, cur.blue);
    carryover = part;
  }
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
uint32_t Segment::color_wheel(uint8_t pos) { // TODO
  if (palette) return color_from_palette(pos, false, true, 0);
  pos = 255 - pos;
  if(pos < 85) {
    return ((uint32_t)(255 - pos * 3) << 16) | ((uint32_t)(0) << 8) | (pos * 3);
  } else if(pos < 170) {
    pos -= 85;
    return ((uint32_t)(0) << 16) | ((uint32_t)(pos * 3) << 8) | (255 - pos * 3);
  } else {
    pos -= 170;
    return ((uint32_t)(pos * 3) << 16) | ((uint32_t)(255 - pos * 3) << 8) | (0);
  }
}

/*
 * Returns a new, random wheel index with a minimum distance of 42 from pos.
 */
uint8_t Segment::get_random_wheel_index(uint8_t pos) {
  uint8_t r = 0, x = 0, y = 0, d = 0;

  while(d < 42) {
    r = random8();
    x = abs(pos - r);
    y = 255 - x;
    d = MIN(x, y);
  }
  return r;
}

/*
 * Gets a single color from the currently selected palette.
 * @param i Palette Index (if mapping is true, the full palette will be _virtualSegmentLength long, if false, 255). Will wrap around automatically.
 * @param mapping if true, LED position in segment is considered for color
 * @param wrap FastLED palettes will usally wrap back to the start smoothly. Set false to get a hard edge
 * @param mcol If the default palette 0 is selected, return the standard color 0, 1 or 2 instead. If >2, Party palette is used instead
 * @param pbri Value to scale the brightness of the returned color by. Default is 255. (no scaling)
 * @returns Single color from palette
 */
uint32_t Segment::color_from_palette(uint16_t i, bool mapping, bool wrap, uint8_t mcol, uint8_t pbri)
{
  // default palette or no RGB support on segment
  if ((palette == 0 && mcol < NUM_COLORS) || !(_capabilities & 0x01)) {
    uint32_t color = (transitional && _t) ? _t->_colorT[mcol] : colors[mcol];
    color = gamma32(color);
    if (pbri == 255) return color;
    return RGBW32(scale8_video(R(color),pbri), scale8_video(G(color),pbri), scale8_video(B(color),pbri), scale8_video(W(color),pbri));
  }

  uint8_t paletteIndex = i;
  if (mapping && virtualLength() > 1) paletteIndex = (i*255)/(virtualLength() -1);
  if (!wrap) paletteIndex = scale8(paletteIndex, 240); //cut off blend at palette "end"
  CRGB fastled_col;
  CRGBPalette16 curPal;
  if (transitional && _t) curPal = _t->_palT;
  else                    loadPalette(curPal, palette);
  fastled_col = ColorFromPalette(curPal, paletteIndex, pbri, (strip.paletteBlend == 3)? NOBLEND:LINEARBLEND); // NOTE: paletteBlend should be global

  return RGBW32(fastled_col.r, fastled_col.g, fastled_col.b, 0);
}


///////////////////////////////////////////////////////////////////////////////
// WS2812FX class implementation
///////////////////////////////////////////////////////////////////////////////

//do not call this method from system context (network callback)
void WS2812FX::finalizeInit(void)
{
  //reset segment runtimes
  for (segment &seg : _segments) {
    seg.markForReset();
    seg.resetIfRequired();
  }

  // for the lack of better place enumerate ledmaps here
  // if we do it in json.cpp (serializeInfo()) we are getting flashes on LEDs
  // unfortunately this means we do not get updates after uploads
  enumerateLedmaps();

  _hasWhiteChannel = _isOffRefreshRequired = false;

  //if busses failed to load, add default (fresh install, FS issue, ...)
  if (busses.getNumBusses() == 0) {
    DEBUG_PRINTLN(F("No busses, init default"));
    const uint8_t defDataPins[] = {DATA_PINS};
    const uint16_t defCounts[] = {PIXEL_COUNTS};
    const uint8_t defNumBusses = ((sizeof defDataPins) / (sizeof defDataPins[0]));
    const uint8_t defNumCounts = ((sizeof defCounts)   / (sizeof defCounts[0]));
    uint16_t prevLen = 0;
    for (uint8_t i = 0; i < defNumBusses && i < WLED_MAX_BUSSES; i++) {
      uint8_t defPin[] = {defDataPins[i]};
      uint16_t start = prevLen;
      uint16_t count = defCounts[(i < defNumCounts) ? i : defNumCounts -1];
      prevLen += count;
      BusConfig defCfg = BusConfig(DEFAULT_LED_TYPE, defPin, start, count, DEFAULT_LED_COLOR_ORDER, false, 0, RGBW_MODE_MANUAL_ONLY);
      busses.add(defCfg);
    }
  }

  _length = 0;
  for (uint8_t i=0; i<busses.getNumBusses(); i++) {
    Bus *bus = busses.getBus(i);
    if (bus == nullptr) continue;
    if (bus->getStart() + bus->getLength() > MAX_LEDS) break;
    //RGBW mode is enabled if at least one of the strips is RGBW
    _hasWhiteChannel |= bus->isRgbw();
    //refresh is required to remain off if at least one of the strips requires the refresh.
    _isOffRefreshRequired |= bus->isOffRefreshRequired();
    uint16_t busEnd = bus->getStart() + bus->getLength();
    if (busEnd > _length) _length = busEnd;
    #ifdef ESP8266
    if ((!IS_DIGITAL(bus->getType()) || IS_2PIN(bus->getType()))) continue;
    uint8_t pins[5];
    if (!bus->getPins(pins)) continue;
    BusDigital* bd = static_cast<BusDigital*>(bus);
    if (pins[0] == 3) bd->reinit();
    #endif
  }

  //initialize leds array. TBD: realloc if nr of leds change
  if (Segment::_globalLeds) {
    purgeSegments(true);
    free(Segment::_globalLeds);
    Segment::_globalLeds = nullptr;
  }
  if (useLedsArray) {
    #if defined(ARDUINO_ARCH_ESP32) && defined(WLED_USE_PSRAM)
    if (psramFound())
      Segment::_globalLeds = (CRGB*) ps_malloc(sizeof(CRGB) * _length);
    else
    #endif
      Segment::_globalLeds = (CRGB*) malloc(sizeof(CRGB) * _length);
    memset(Segment::_globalLeds, 0, sizeof(CRGB) * _length);
  }

  //segments are created in makeAutoSegments();
  setBrightness(_brightness);
}

void WS2812FX::service() {
  uint32_t nowUp = millis(); // Be aware, millis() rolls over every 49 days
  now = nowUp + timebase;
  if (nowUp - _lastShow < MIN_SHOW_DELAY) return;
  bool doShow = false;

  _isServicing = true;
  _segment_index = 0;
  for (segment &seg : _segments) {
    // reset the segment runtime data if needed
    seg.resetIfRequired();

    if (!seg.isActive()) continue;

    // last condition ensures all solid segments are updated at the same time
    if(nowUp > seg.next_time || _triggered || (doShow && seg.mode == FX_MODE_STATIC))
    {
      if (seg.grouping == 0) seg.grouping = 1; //sanity check
      doShow = true;
      uint16_t delay = FRAMETIME;

      if (!seg.freeze) { //only run effect function if not frozen
        _virtualSegmentLength = seg.virtualLength();
        _colors_t[0] = seg.currentColor(0, seg.colors[0]);
        _colors_t[1] = seg.currentColor(1, seg.colors[1]);
        _colors_t[2] = seg.currentColor(2, seg.colors[2]);
        seg.currentPalette(_currentPalette, seg.palette);

        if (!cctFromRgb || correctWB) busses.setSegmentCCT(seg.currentBri(seg.cct, true), correctWB);
        for (uint8_t c = 0; c < NUM_COLORS; c++) _colors_t[c] = gamma32(_colors_t[c]);

        // effect blending (execute previous effect)
        // actual code may be a bit more involved as effects have runtime data including allocated memory
        //if (seg.transitional && seg._modeP) (*_mode[seg._modeP])(progress());
        delay = (*_mode[seg.currentMode(seg.mode)])();
        if (seg.mode != FX_MODE_HALLOWEEN_EYES) seg.call++;
        if (seg.transitional && delay > FRAMETIME) delay = FRAMETIME; // foce faster updates during transition

        seg.handleTransition();
      }

      seg.next_time = nowUp + delay;
    }
    _segment_index++;
  }
  _virtualSegmentLength = 0;
  busses.setSegmentCCT(-1);
  if(doShow) {
    yield();
    show();
  }
  _triggered = false;
  _isServicing = false;
}

void IRAM_ATTR WS2812FX::setPixelColor(int i, uint32_t col)
{
  if (i >= _length) return;
  if (i < customMappingSize) i = customMappingTable[i];
  busses.setPixelColor(i, col);
}

uint32_t WS2812FX::getPixelColor(uint16_t i)
{
  if (i >= _length) return 0;
  if (i < customMappingSize) i = customMappingTable[i];
  return busses.getPixelColor(i);
}


//DISCLAIMER
//The following function attemps to calculate the current LED power usage,
//and will limit the brightness to stay below a set amperage threshold.
//It is NOT a measurement and NOT guaranteed to stay within the ablMilliampsMax margin.
//Stay safe with high amperage and have a reasonable safety margin!
//I am NOT to be held liable for burned down garages!

//fine tune power estimation constants for your setup                  
#define MA_FOR_ESP        100 //how much mA does the ESP use (Wemos D1 about 80mA, ESP32 about 120mA)
                              //you can set it to 0 if the ESP is powered by USB and the LEDs by external

void WS2812FX::estimateCurrentAndLimitBri() {
  //power limit calculation
  //each LED can draw up 195075 "power units" (approx. 53mA)
  //one PU is the power it takes to have 1 channel 1 step brighter per brightness step
  //so A=2,R=255,G=0,B=0 would use 510 PU per LED (1mA is about 3700 PU)
  bool useWackyWS2815PowerModel = false;
  byte actualMilliampsPerLed = milliampsPerLed;

  if(milliampsPerLed == 255) {
    useWackyWS2815PowerModel = true;
    actualMilliampsPerLed = 12; // from testing an actual strip
  }

  if (ablMilliampsMax < 150 || actualMilliampsPerLed == 0) { //0 mA per LED and too low numbers turn off calculation
    currentMilliamps = 0;
    busses.setBrightness(_brightness);
    return;
  }

  uint16_t pLen = getLengthPhysical();
  uint32_t puPerMilliamp = 195075 / actualMilliampsPerLed;
  uint32_t powerBudget = (ablMilliampsMax - MA_FOR_ESP) * puPerMilliamp; //100mA for ESP power
  if (powerBudget > puPerMilliamp * pLen) { //each LED uses about 1mA in standby, exclude that from power budget
    powerBudget -= puPerMilliamp * pLen;
  } else {
    powerBudget = 0;
  }

  uint32_t powerSum = 0;

  for (uint8_t b = 0; b < busses.getNumBusses(); b++) {
    Bus *bus = busses.getBus(b);
    if (bus->getType() >= TYPE_NET_DDP_RGB) continue; //exclude non-physical network busses
    uint16_t len = bus->getLength();
    uint32_t busPowerSum = 0;
    for (uint16_t i = 0; i < len; i++) { //sum up the usage of each LED
      uint32_t c = bus->getPixelColor(i);
      byte r = R(c), g = G(c), b = B(c), w = W(c);

      if(useWackyWS2815PowerModel) { //ignore white component on WS2815 power calculation
        busPowerSum += (MAX(MAX(r,g),b)) * 3;
      } else {
        busPowerSum += (r + g + b + w);
      }
    }

    if (bus->isRgbw()) { //RGBW led total output with white LEDs enabled is still 50mA, so each channel uses less
      busPowerSum *= 3;
      busPowerSum = busPowerSum >> 2; //same as /= 4
    }
    powerSum += busPowerSum;
  }

  uint32_t powerSum0 = powerSum;
  powerSum *= _brightness;
  
  if (powerSum > powerBudget) //scale brightness down to stay in current limit
  {
    float scale = (float)powerBudget / (float)powerSum;
    uint16_t scaleI = scale * 255;
    uint8_t scaleB = (scaleI > 255) ? 255 : scaleI;
    uint8_t newBri = scale8(_brightness, scaleB);
    busses.setBrightness(newBri); //to keep brightness uniform, sets virtual busses too
    currentMilliamps = (powerSum0 * newBri) / puPerMilliamp;
  } else {
    currentMilliamps = powerSum / puPerMilliamp;
    busses.setBrightness(_brightness);
  }
  currentMilliamps += MA_FOR_ESP; //add power of ESP back to estimate
  currentMilliamps += pLen; //add standby power back to estimate
}

void WS2812FX::show(void) {

  // avoid race condition, caputre _callback value
  show_callback callback = _callback;
  if (callback) callback();

  estimateCurrentAndLimitBri();
  
  // some buses send asynchronously and this method will return before
  // all of the data has been sent.
  // See https://github.com/Makuna/NeoPixelBus/wiki/ESP32-NeoMethods#neoesp32rmt-methods
  busses.show();
  unsigned long now = millis();
  unsigned long diff = now - _lastShow;
  uint16_t fpsCurr = 200;
  if (diff > 0) fpsCurr = 1000 / diff;
  _cumulativeFps = (3 * _cumulativeFps + fpsCurr) >> 2;
  _lastShow = now;
}

/**
 * Returns a true value if any of the strips are still being updated.
 * On some hardware (ESP32), strip updates are done asynchronously.
 */
bool WS2812FX::isUpdating() {
  return !busses.canAllShow();
}

/**
 * Returns the refresh rate of the LED strip. Useful for finding out whether a given setup is fast enough.
 * Only updates on show() or is set to 0 fps if last show is more than 2 secs ago, so accurary varies
 */
uint16_t WS2812FX::getFps() {
  if (millis() - _lastShow > 2000) return 0;
  return _cumulativeFps +1;
}

void WS2812FX::setTargetFps(uint8_t fps) {
  if (fps > 0 && fps <= 120) _targetFps = fps;
  _frametime = 1000 / _targetFps;
}

void WS2812FX::setMode(uint8_t segid, uint8_t m) {
  if (segid >= _segments.size()) return;
   
  if (m >= getModeCount()) m = getModeCount() - 1;

  if (_segments[segid].mode != m) {
    _segments[segid].startTransition(_transitionDur); // set effect transitions
    //_segments[segid].markForReset();
    _segments[segid].mode = m;
  }
}

//applies to all active and selected segments
void WS2812FX::setColor(uint8_t slot, uint32_t c) {
  if (slot >= NUM_COLORS) return;

  for (segment &seg : _segments) {
    if (seg.isActive() && seg.isSelected()) {
      seg.setColor(slot, c);
    }
  }
}

void WS2812FX::setCCT(uint16_t k) {
  for (segment &seg : _segments) {
    if (seg.isActive() && seg.isSelected()) {
      seg.setCCT(k);
    }
  }
}

void WS2812FX::setBrightness(uint8_t b, bool direct) {
  if (gammaCorrectBri) b = gamma8(b);
  if (_brightness == b) return;
  _brightness = b;
  if (_brightness == 0) { //unfreeze all segments on power off
    for (segment &seg : _segments) {
      seg.freeze = false;
    }
  }
  if (direct) {
    // would be dangerous if applied immediately (could exceed ABL), but will not output until the next show()
    busses.setBrightness(b);
  } else {
    unsigned long t = millis();
    if (_segments[0].next_time > t + 22 && t - _lastShow > MIN_SHOW_DELAY) show(); //apply brightness change immediately if no refresh soon
  }
}

uint8_t WS2812FX::getFirstSelectedSegId(void)
{
  size_t i = 0;
  for (segment &seg : _segments) {
    if (seg.isActive() && seg.isSelected()) return i;
    i++;
  }
  // if none selected, use the main segment
  return getMainSegmentId();
}

void WS2812FX::setMainSegmentId(uint8_t n) {
  _mainSegment = 0;
  if (n < _segments.size()) {
    _mainSegment = n;
  }
  return;
}

uint8_t WS2812FX::getLastActiveSegmentId(void) {
  for (size_t i = _segments.size() -1; i > 0; i--) {
    if (_segments[i].isActive()) return i;
  }
  return 0;
}

uint8_t WS2812FX::getActiveSegmentsNum(void) {
  uint8_t c = 0;
  for (size_t i = 0; i < _segments.size(); i++) {
    if (_segments[i].isActive()) c++;
  }
  return c;
}

uint16_t WS2812FX::getLengthPhysical(void) {
  uint16_t len = 0;
  for (size_t b = 0; b < busses.getNumBusses(); b++) {
    Bus *bus = busses.getBus(b);
    if (bus->getType() >= TYPE_NET_DDP_RGB) continue; //exclude non-physical network busses
    len += bus->getLength();
  }
  return len;
}

//used for JSON API info.leds.rgbw. Little practical use, deprecate with info.leds.rgbw.
//returns if there is an RGBW bus (supports RGB and White, not only white)
//not influenced by auto-white mode, also true if white slider does not affect output white channel
bool WS2812FX::hasRGBWBus(void) {
  for (size_t b = 0; b < busses.getNumBusses(); b++) {
    Bus *bus = busses.getBus(b);
    if (bus == nullptr || bus->getLength()==0) break;
    switch (bus->getType()) {
      case TYPE_SK6812_RGBW:
      case TYPE_TM1814:
      case TYPE_ANALOG_4CH:
        return true;
    }
  }
  return false;
}

bool WS2812FX::hasCCTBus(void) {
  if (cctFromRgb && !correctWB) return false;
  for (size_t b = 0; b < busses.getNumBusses(); b++) {
    Bus *bus = busses.getBus(b);
    if (bus == nullptr || bus->getLength()==0) break;
    switch (bus->getType()) {
      case TYPE_ANALOG_5CH:
      case TYPE_ANALOG_2CH:
        return true;
    }
  }
  return false;
}

void WS2812FX::purgeSegments(bool force) {
  // remove all inactive segments (from the back)
  int deleted = 0;
  if (_segments.size() <= 1) return;
  for (size_t i = _segments.size()-1; i > 0; i--)
    if (_segments[i].stop == 0 || force) {
      DEBUG_PRINT(F("Purging segment segment: ")); DEBUG_PRINTLN(i);
      deleted++;
      _segments.erase(_segments.begin() + i);
    }
  if (deleted) {
    _segments.shrink_to_fit();
    if (_mainSegment >= _segments.size()) setMainSegmentId(0);
  }
}

Segment& WS2812FX::getSegment(uint8_t id) {
  return _segments[id >= _segments.size() ? getMainSegmentId() : id]; // vectors
}

void WS2812FX::setSegment(uint8_t n, uint16_t i1, uint16_t i2, uint8_t grouping, uint8_t spacing, uint16_t offset, uint16_t startY, uint16_t stopY) {
  if (n >= _segments.size()) return;
  Segment& seg = _segments[n];

  //return if neither bounds nor grouping have changed
  bool boundsUnchanged = (seg.start == i1 && seg.stop == i2);
  if (isMatrix) {
    boundsUnchanged &= (seg.startY == startY && seg.stopY == stopY);
  }
  if (boundsUnchanged
      && (!grouping || (seg.grouping == grouping && seg.spacing == spacing))
      && (offset == UINT16_MAX || offset == seg.offset)) return;

  //if (seg.stop) setRange(seg.start, seg.stop -1, BLACK); //turn old segment range off
  if (seg.stop) seg.fill(BLACK); //turn old segment range off
  if (i2 <= i1) //disable segment
  {
    // disabled segments should get removed using purgeSegments()
    DEBUG_PRINT(F("-- Segment ")); DEBUG_PRINT(n); DEBUG_PRINTLN(F(" marked inactive."));
    seg.stop = 0;
    //if (seg.name) {
    //  delete[] seg.name;
    //  seg.name = nullptr;
    //}
    // if main segment is deleted, set first active as main segment
    if (n == _mainSegment) setMainSegmentId(0);
    seg.markForReset();
    return;
  }
  if (isMatrix) {
    #ifndef WLED_DISABLE_2D
    if (i1 < matrixWidth) seg.start = i1;
    seg.stop = i2 > matrixWidth ? matrixWidth : i2;
    if (startY < matrixHeight) seg.startY = startY;
    seg.stopY = stopY > matrixHeight ? matrixHeight : MAX(1,stopY);
    #endif
  } else {
    if (i1 < _length) seg.start = i1;
    seg.stop = i2 > _length ? _length : i2;
    seg.startY = 0;
    seg.stopY  = 1;
  }
  if (grouping) {
    seg.grouping = grouping;
    seg.spacing = spacing;
  }
  if (offset < UINT16_MAX) seg.offset = offset;
  seg.markForReset();
  if (!boundsUnchanged) seg.refreshLightCapabilities();
}

void WS2812FX::restartRuntime() {
  for (segment &seg : _segments) seg.markForReset();
}

void WS2812FX::resetSegments() {
  _segments.clear(); // destructs all Segment as part of clearing
  #ifndef WLED_DISABLE_2D
  segment seg = isMatrix ? Segment(0, matrixWidth, 0, matrixHeight) : Segment(0, _length);
  #else
  segment seg = Segment(0, _length);
  #endif
  _segments.push_back(seg);
  _mainSegment = 0;
}

void WS2812FX::makeAutoSegments(bool forceReset) {
  if (isMatrix) {
    #ifndef WLED_DISABLE_2D
    // only create 1 2D segment
    if (forceReset || getSegmentsNum() == 0) resetSegments(); // initialises 1 segment
    else if (getActiveSegmentsNum() == 1) {
      size_t i = getLastActiveSegmentId();
      _segments[i].start  = 0;
      _segments[i].stop   = matrixWidth;
      _segments[i].startY = 0;
      _segments[i].stopY  = matrixHeight;
      _segments[i].grouping = 1;
      _segments[i].spacing  = 0;
      _mainSegment = i;
    }
    #endif
  } else if (autoSegments) { //make one segment per bus
    uint16_t segStarts[MAX_NUM_SEGMENTS] = {0};
    uint16_t segStops [MAX_NUM_SEGMENTS] = {0};
    uint8_t s = 0;
    for (uint8_t i = 0; i < busses.getNumBusses(); i++) {
      Bus* b = busses.getBus(i);

      segStarts[s] = b->getStart();
      segStops[s]  = segStarts[s] + b->getLength();

      //check for overlap with previous segments
      for (size_t j = 0; j < s; j++) {
        if (segStops[j] > segStarts[s] && segStarts[j] < segStops[s]) {
          //segments overlap, merge
          segStarts[j] = min(segStarts[s],segStarts[j]);
          segStops [j] = max(segStops [s],segStops [j]); segStops[s] = 0;
          s--;
        }
      }
      s++;
    }
    _segments.clear();
    for (size_t i = 0; i < s; i++) {
      Segment seg = Segment(segStarts[i], segStops[i]);
      seg.selected = true;
      _segments.push_back(seg);
    }
    _mainSegment = 0;
  } else {
    if (forceReset || getSegmentsNum() == 0) resetSegments();
    //expand the main seg to the entire length, but only if there are no other segments, or reset is forced
    else if (getActiveSegmentsNum() == 1) {
      size_t i = getLastActiveSegmentId();
      _segments[i].start = 0;
      _segments[i].stop  = _length;
      _mainSegment = 0;
    }
  }

  fixInvalidSegments();
}

void WS2812FX::fixInvalidSegments() {
  //make sure no segment is longer than total (sanity check)
  for (size_t i = getSegmentsNum()-1; i > 0; i--) {
    if (_segments[i].start >= _length) { _segments.erase(_segments.begin()+i); continue; }
    if (_segments[i].stop  >  _length) _segments[i].stop = _length;
    // this is always called as the last step after finalizeInit(), update covered bus types
    _segments[i].refreshLightCapabilities();
  }
}

//true if all segments align with a bus, or if a segment covers the total length
bool WS2812FX::checkSegmentAlignment() {
  bool aligned = false;
  for (segment &seg : _segments) {
    for (uint8_t b = 0; b<busses.getNumBusses(); b++) {
      Bus *bus = busses.getBus(b);
      if (seg.start == bus->getStart() && seg.stop == bus->getStart() + bus->getLength()) aligned = true;
    }
    if (seg.start == 0 && seg.stop == _length) aligned = true;
    if (!aligned) return false;
  }
  return true;
}

//After this function is called, setPixelColor() will use that segment (offsets, grouping, ... will apply)
//Note: If called in an interrupt (e.g. JSON API), original segment must be restored,
//otherwise it can lead to a crash on ESP32 because _segment_index is modified while in use by the main thread
uint8_t WS2812FX::setPixelSegment(uint8_t n)
{
  uint8_t prevSegId = _segment_index;
  if (n < _segments.size()) {
    _segment_index = n;
    _virtualSegmentLength = _segments[_segment_index].virtualLength();
  }
  return prevSegId;
}

void WS2812FX::setRange(uint16_t i, uint16_t i2, uint32_t col)
{
  if (i2 >= i)
  {
    for (uint16_t x = i; x <= i2; x++) setPixelColor(x, col);
  } else
  {
    for (uint16_t x = i2; x <= i; x++) setPixelColor(x, col);
  }
}

void WS2812FX::setTransitionMode(bool t)
{
  for (segment &seg : _segments) if (!seg.transitional) seg.startTransition(t ? _transitionDur : 0);
}

#ifdef WLED_DEBUG
void WS2812FX::printSize()
{
  size_t size = 0;
  for (const Segment &seg : _segments) size += seg.getSize();
  DEBUG_PRINTF("Segments: %d -> %uB\n", _segments.size(), size);
  DEBUG_PRINTF("Modes: %d*%d=%uB\n", sizeof(mode_ptr), _mode.size(), (_mode.capacity()*sizeof(mode_ptr)));
  DEBUG_PRINTF("Data: %d*%d=%uB\n", sizeof(const char *), _modeData.size(), (_modeData.capacity()*sizeof(const char *)));
  DEBUG_PRINTF("Map: %d*%d=%uB\n", sizeof(uint16_t), (int)customMappingSize, customMappingSize*sizeof(uint16_t));
  if (useLedsArray) DEBUG_PRINTF("Buffer: %d*%d=%uB\n", sizeof(CRGB), (int)_length, _length*sizeof(CRGB));
}
#endif

void WS2812FX::loadCustomPalettes()
{
  byte tcp[72]; //support gradient palettes with up to 18 entries
  CRGBPalette16 targetPalette;
  for (int index = 0; index<10; index++) {
    char fileName[32];
    strcpy_P(fileName, PSTR("/palette"));
    sprintf(fileName +8, "%d", index);
    strcat(fileName, ".json");

    StaticJsonDocument<1536> pDoc; // barely enough to fit 72 numbers
    if (WLED_FS.exists(fileName)) {
      DEBUG_PRINT(F("Reading palette from "));
      DEBUG_PRINTLN(fileName);

      if (readObjectFromFile(fileName, nullptr, &pDoc)) {
        JsonArray pal = pDoc[F("palette")];
        if (!pal.isNull() && pal.size()>7) { // not an empty palette (at least 2 entries)
          size_t palSize = MIN(pal.size(), 72);
          palSize -= palSize % 4; // make sure size is multiple of 4
          for (size_t i=0; i<palSize && pal[i].as<int>()<256; i+=4) {
            tcp[ i ] = (uint8_t) pal[ i ].as<int>(); // index
            tcp[i+1] = (uint8_t) pal[i+1].as<int>(); // R
            tcp[i+2] = (uint8_t) pal[i+2].as<int>(); // G
            tcp[i+3] = (uint8_t) pal[i+3].as<int>(); // B
            DEBUG_PRINTF("%d(%d) : %d %d %d\n", i, int(tcp[i]), int(tcp[i+1]), int(tcp[i+2]), int(tcp[i+3]));
          }
          customPalettes.push_back(targetPalette.loadDynamicGradientPalette(tcp));
        }
      }
    } else {
      break;
    }
  }
}

//load custom mapping table from JSON file (called from finalizeInit() or deserializeState())
void WS2812FX::deserializeMap(uint8_t n) {
  if (isMatrix) return; // 2D support creates its own ledmap

  char fileName[32];
  strcpy_P(fileName, PSTR("/ledmap"));
  if (n) sprintf(fileName +7, "%d", n);
  strcat(fileName, ".json");
  bool isFile = WLED_FS.exists(fileName);

  if (!isFile) {
    // erase custom mapping if selecting nonexistent ledmap.json (n==0)
    if (!n && customMappingTable != nullptr) {
      customMappingSize = 0;
      delete[] customMappingTable;
      customMappingTable = nullptr;
    }
    return;
  }

  if (!requestJSONBufferLock(7)) return;

  DEBUG_PRINT(F("Reading LED map from "));
  DEBUG_PRINTLN(fileName);

  if (!readObjectFromFile(fileName, nullptr, &doc)) {
    releaseJSONBufferLock();
    return; //if file does not exist just exit
  }

  // erase old custom ledmap
  if (customMappingTable != nullptr) {
    customMappingSize = 0;
    delete[] customMappingTable;
    customMappingTable = nullptr;
  }

  JsonArray map = doc[F("map")];
  if (!map.isNull() && map.size()) {  // not an empty map
    customMappingSize  = map.size();
    customMappingTable = new uint16_t[customMappingSize];
    for (uint16_t i=0; i<customMappingSize; i++) {
      customMappingTable[i] = (uint16_t) map[i];
    }
  }

  releaseJSONBufferLock();
}


WS2812FX* WS2812FX::instance = nullptr;

//Bus static member definition, would belong in bus_manager.cpp
int16_t Bus::_cct = -1;
uint8_t Bus::_cctBlend = 0;
uint8_t Bus::_gAWM = 255;

const char JSON_mode_names[] PROGMEM = R"=====(["Mode names have moved"])=====";
const char JSON_palette_names[] PROGMEM = R"=====([
"Default","* Random Cycle","* Color 1","* Colors 1&2","* Color Gradient","* Colors Only","Party","Cloud","Lava","Ocean",
"Forest","Rainbow","Rainbow Bands","July","Vintage 57","Vintage 01","Rivendell","RGI 15","Retro","Analogous",
"Pink Splash 08","Pink Splash 07","Coral Reef","Ocean Breeze 68","Ocean Breeze 36","Departure","Landscape 64","Landscape 33","Sherbet","Hult 65",
"Hult 64","Drywet","IB15","Fuschia","Emerald Dragon 08","Hot Lava","Fire","Hiyane","Colorfull","Magenta Evening",
"Pink Purple","Sunset","Autumn","Blue/Magenta/White","Blue/Magenta/Red","Blue/Red/Yellow","Blue/Cyan/Yellow","Sunset Yellow","Cloud","Fire & Ice",
"BHW2","Rainfall","Angel","Butterfly","250K Meters","Night Midnight","Afterdusk","Blue Sky","Gold Orange", "Frizell 10",
"Frizell 12","Fib 18","Fib 13","Fib 17","Fib 05","Analogous 02","Analogous 04a","Cyan Orange","C/W/G",
"Wild Orange","Ikat","Citrus","Teal Blue","Ldby Orange","Purple/Orange","Blue/Tan","Green/Purple","Knoza 00","Knoza 18",
"Calpan","Calbayo","Fib53","Purple/Orange","PMH","Konjo 08","Konikyo","McCahon","Pills","Pink/Yellow/Orange",
"Autumn 04","Autumn 02","Candide","Chic","Coffee","Emerald Dragon 01","Landscape 57","Landscape 22","Landscape 47","Landscape 10",
"Landscape 76","Landscape 61","Landscape 60","Landscape 51","Landscape 06","Ocean Breeze 49","Ocean Breeze 57","Ocean Breeze 74","Pink Splash 05","Pink Splash 10",
"Vintage 56","Vintage 10","Gold/Yellow","Radioactive Slime","Pastel Rainbow","Purple Sunset","Janico"
])=====";

