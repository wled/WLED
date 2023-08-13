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
uint16_t Segment::maxWidth = DEFAULT_LED_COUNT;
uint16_t Segment::maxHeight = 1;

CRGBPalette16 Segment::_randomPalette = CRGBPalette16(DEFAULT_COLOR);
CRGBPalette16 Segment::_newRandomPalette = CRGBPalette16(DEFAULT_COLOR);
unsigned long Segment::_lastPaletteChange = 0; // perhaps it should be per segment

bool Segment::_modeBlend = false;

// copy constructor
Segment::Segment(const Segment &orig) {
  //DEBUG_PRINTF("-- Copy segment constructor: %p -> %p\n", &orig, this);
  memcpy((void*)this, (void*)&orig, sizeof(Segment));
  transitional = false; // copied segment cannot be in transition
  name = nullptr;
  data = nullptr;
  _dataLen = 0;
  _t = nullptr;
  if (orig.name) { name = new char[strlen(orig.name)+1]; if (name) strcpy(name, orig.name); }
  if (orig.data) { if (allocateData(orig._dataLen)) memcpy(data, orig.data, orig._dataLen); }
  //if (orig._t)   { _t = new Transition(orig._t->_dur); }
}

// move constructor
Segment::Segment(Segment &&orig) noexcept {
  //DEBUG_PRINTF("-- Move segment constructor: %p -> %p\n", &orig, this);
  memcpy((void*)this, (void*)&orig, sizeof(Segment));
  orig.transitional = false; // old segment cannot be in transition any more
  orig.name = nullptr;
  orig.data = nullptr;
  orig._dataLen = 0;
  orig._t   = nullptr;
}

// copy assignment
Segment& Segment::operator= (const Segment &orig) {
  //DEBUG_PRINTF("-- Copying segment: %p -> %p\n", &orig, this);
  if (this != &orig) {
    // clean destination
    transitional = false; // copied segment cannot be in transition
    if (name) delete[] name;
    if (_t) {
      if (_t->_segT._dataT) free(_t->_segT._dataT);
      delete _t;
    }
    deallocateData();
    // copy source
    memcpy((void*)this, (void*)&orig, sizeof(Segment));
    transitional = false;
    // erase pointers to allocated data
    name = nullptr;
    data = nullptr;
    _dataLen = 0;
    _t = nullptr;
    // copy source data
    if (orig.name) { name = new char[strlen(orig.name)+1]; if (name) strcpy(name, orig.name); }
    if (orig.data) { if (allocateData(orig._dataLen)) memcpy(data, orig.data, orig._dataLen); }
    //if (orig._t)   { _t = new Transition(orig._t->_dur, orig._t->_briT, orig._t->_cctT, orig._t->_colorT); }
  }
  return *this;
}

// move assignment
Segment& Segment::operator= (Segment &&orig) noexcept {
  //DEBUG_PRINTF("-- Moving segment: %p -> %p\n", &orig, this);
  if (this != &orig) {
    transitional = false; // just temporary
    if (name) { delete[] name; name = nullptr; } // free old name
    deallocateData(); // free old runtime data
    if (_t) {
      if (_t->_segT._dataT) free(_t->_segT._dataT);
      delete _t;
      _t = nullptr;
    }
    memcpy((void*)this, (void*)&orig, sizeof(Segment));
    orig.transitional = false; // old segment cannot be in transition
    orig.name = nullptr;
    orig.data = nullptr;
    orig._dataLen = 0;
    orig._t   = nullptr;
  }
  return *this;
}

bool Segment::allocateData(size_t len) {
  if (data && _dataLen == len) return true; //already allocated
  //DEBUG_PRINTF("--   Allocating data (%d): %p\n", len, this);
  deallocateData();
  if (Segment::getUsedSegmentData() + len > MAX_SEGMENT_DATA) {
    // not enough memory
    DEBUG_PRINTF("!!! Effect RAM depleted: %d/%d !!!\n", len, Segment::getUsedSegmentData());
    return false;
  }
  // do not use SPI RAM on ESP32 since it is slow
  data = (byte*) malloc(len);
  if (!data) { DEBUG_PRINTLN(F("!!! Allocation failed. !!!")); return false; } //allocation failed
  Segment::addUsedSegmentData(len);
  //DEBUG_PRINTF("---  Allocated data (%p): %d/%d -> %p\n", this, len, Segment::getUsedSegmentData(), data);
  _dataLen = len;
  memset(data, 0, len);
  return true;
}

void Segment::deallocateData() {
  if (!data) return;
  //DEBUG_PRINTF("---  Released data (%p): %d/%d -> %p\n", this, _dataLen, Segment::getUsedSegmentData(), data);
  free(data);
  data = nullptr;
  // WARNING it looks like we have a memory leak somewhere
  Segment::addUsedSegmentData(_dataLen <= Segment::getUsedSegmentData() ? -_dataLen : -Segment::getUsedSegmentData());
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
  if (!reset) return;
  //DEBUG_PRINTF("-- Segment reset: %p\n", this);
  deallocateData();
  next_time = 0; step = 0; call = 0; aux0 = 0; aux1 = 0;
  reset = false;
}

CRGBPalette16 &Segment::loadPalette(CRGBPalette16 &targetPalette, uint8_t pal) {
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
    case FX_MODE_RAILWAY    : pal =  3; break; // prim + sec
    case FX_MODE_2DSOAP     : pal = 11; break; // rainbow colors
  }
  switch (pal) {
    case 0: //default palette. Exceptions for specific effects above
      targetPalette = PartyColors_p; break;
    case 1: {//periodically replace palette with a random one. Transition palette change in 500ms
      unsigned long timeSinceLastChange = millis() - _lastPaletteChange;
      if (timeSinceLastChange > randomPaletteChangeTime * 1000U) {
        _randomPalette = _newRandomPalette;
        _newRandomPalette = CRGBPalette16(
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)));
        _lastPaletteChange = millis();
        handleRandomPalette(); // do a 1st pass of blend
      }
      targetPalette = _randomPalette;
      break;}
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
        byte tcp[72];
        memcpy_P(tcp, (byte*)pgm_read_dword(&(gGradientPalettes[pal-13])), 72);
        targetPalette.loadDynamicGradientPalette(tcp);
      }
      break;
  }
  return targetPalette;
}

void Segment::startTransition(uint16_t dur) {
  if (!dur) {
    if (_t) _t->_dur = dur; // this will stop transition in next handleTransisiton()
    else    transitional = false;
    return;
  }
  if (transitional && _t) return; // already in transition no need to store anything

  // starting a transition has to occur before change so we get current values 1st
  _t = new Transition(dur); // no previous transition running
  if (!_t) return; // failed to allocate data

  //DEBUG_PRINTF("-- Started transition: %p\n", this);
  swapSegenv(_t->_segT);
  CRGBPalette16 _palT = CRGBPalette16(DEFAULT_COLOR); loadPalette(_palT, palette);
  _t->_palT           = _palT;
  _t->_modeT          = mode;
  _t->_briT           = on ? opacity : 0;
  _t->_cctT           = cct;
  _t->_segT._optionsT |= 0b0000000001000000; // mark old segment transitional
  _t->_segT._dataLenT = 0;
  _t->_segT._dataT    = nullptr;
  if (_dataLen > 0 && data) {
    _t->_segT._dataT = (byte *)malloc(_dataLen);
    if (_t->_segT._dataT) {
      //DEBUG_PRINTF("--  Allocated duplicate data (%d): %p\n", _dataLen, _t->_segT._dataT);
      memcpy(_t->_segT._dataT, data, _dataLen);
      _t->_segT._dataLenT = _dataLen;
    }
  }
  transitional = true; // setOption(SEG_OPTION_TRANSITIONAL, true);
}

void Segment::stopTransition() {
  if (!transitional) return;
  transitional = false; // finish transitioning segment
  //DEBUG_PRINTF("-- Stopping transition: %p\n", this);
  if (_t) {
    if (_t->_segT._dataT && _t->_segT._dataLenT > 0) {
      //DEBUG_PRINTF("--  Released duplicate data (%d): %p\n", _t->_segT._dataLenT, _t->_segT._dataT);
      free(_t->_segT._dataT);
      _t->_segT._dataT = nullptr;
    }
    delete _t;
    _t = nullptr;
  }
}

void Segment::handleTransition() {
  if (!transitional) return;
  uint16_t _progress = progress();
  if (_progress == 0xFFFFU) stopTransition();
}

// transition progression between 0-65535
uint16_t Segment::progress() {
  if (transitional && _t) {
    unsigned long timeNow = millis();
    if (_t->_dur > 0 && timeNow - _t->_start < _t->_dur) return (timeNow - _t->_start) * 0xFFFFU / _t->_dur;
  }
  return 0xFFFFU;
}

void Segment::swapSegenv(tmpsegd_t &tmpSeg) {
  if (!_t) return;
  //DEBUG_PRINTF("--  Saving temp seg: %p (%p)\n", this, tmpSeg);
  tmpSeg._optionsT   = options;
  for (size_t i=0; i<NUM_COLORS; i++) tmpSeg._colorT[i] = colors[i];
  tmpSeg._speedT     = speed;
  tmpSeg._intensityT = intensity;
  tmpSeg._custom1T   = custom1;
  tmpSeg._custom2T   = custom2;
  tmpSeg._custom3T   = custom3;
  tmpSeg._check1T    = check1;
  tmpSeg._check2T    = check2;
  tmpSeg._check3T    = check3;
  tmpSeg._aux0T      = aux0;
  tmpSeg._aux1T      = aux1;
  tmpSeg._stepT      = step;
  tmpSeg._callT      = call;
  tmpSeg._dataT      = data;
  tmpSeg._dataLenT   = _dataLen;
  if (&tmpSeg != &(_t->_segT)) {
    // swap SEGENV with transitional data
    options   = _t->_segT._optionsT;
    for (size_t i=0; i<NUM_COLORS; i++) colors[i] = _t->_segT._colorT[i];
    speed     = _t->_segT._speedT;
    intensity = _t->_segT._intensityT;
    custom1   = _t->_segT._custom1T;
    custom2   = _t->_segT._custom2T;
    custom3   = _t->_segT._custom3T;
    check1    = _t->_segT._check1T;
    check2    = _t->_segT._check2T;
    check3    = _t->_segT._check3T;
    aux0      = _t->_segT._aux0T;
    aux1      = _t->_segT._aux1T;
    step      = _t->_segT._stepT;
    call      = _t->_segT._callT;
    data      = _t->_segT._dataT;
    _dataLen  = _t->_segT._dataLenT;
  }
  //DEBUG_PRINTF("--   temp seg data: %p (%d,%p)\n", this, _dataLen, data);
}

void Segment::restoreSegenv(tmpsegd_t &tmpSeg) {
  //DEBUG_PRINTF("--  Restoring temp seg: %p (%p)\n", this, tmpSeg);
  if (_t && &(_t->_segT) != &tmpSeg) {
    // update possibly changed variables to keep old effect running correctly
    _t->_segT._aux0T = aux0;
    _t->_segT._aux1T = aux1;
    _t->_segT._stepT = step;
    _t->_segT._callT = call;
    //if (_t->_segT._dataT != data) DEBUG_PRINTF("---  data re-allocated: (%p) %p -> %p\n", this, _t->_segT._dataT, data);
    _t->_segT._dataT = data;        // sometimes memory gets re-allocated (!! INVESTIGATE WHY !!)
    _t->_segT._dataLenT = _dataLen; // sometimes memory gets re-allocated (!! INVESTIGATE WHY !!)
  }
  options   = tmpSeg._optionsT;
  for (size_t i=0; i<NUM_COLORS; i++) colors[i] = tmpSeg._colorT[i];
  speed     = tmpSeg._speedT;
  intensity = tmpSeg._intensityT;
  custom1   = tmpSeg._custom1T;
  custom2   = tmpSeg._custom2T;
  custom3   = tmpSeg._custom3T;
  check1    = tmpSeg._check1T;
  check2    = tmpSeg._check2T;
  check3    = tmpSeg._check3T;
  aux0      = tmpSeg._aux0T;
  aux1      = tmpSeg._aux1T;
  step      = tmpSeg._stepT;
  call      = tmpSeg._callT;
  data      = tmpSeg._dataT;
  _dataLen  = tmpSeg._dataLenT;
  //DEBUG_PRINTF("--   temp seg data: %p (%d,%p)\n", this, _dataLen, data);
}

uint8_t Segment::currentBri(uint8_t briNew, bool useCct) {
  uint32_t prog = progress();
  if (prog < 0xFFFFU) {
    if (useCct) return ((briNew * prog) + _t->_cctT * (0xFFFFU - prog)) >> 16;
    else        return ((briNew * prog) + _t->_briT * (0xFFFFU - prog)) >> 16;
  }
  return briNew;
}

uint8_t Segment::currentMode(uint8_t newMode) {
  uint16_t prog = progress(); // implicit check for transitional & _t in progress()
  if (prog < 0xFFFFU) return _t->_modeT;
  return newMode;
}

uint32_t Segment::currentColor(uint8_t slot, uint32_t colorNew) {
  return transitional && _t ? color_blend(_t->_segT._colorT[slot], colorNew, progress(), true) : colorNew;
}

CRGBPalette16 &Segment::currentPalette(CRGBPalette16 &targetPalette, uint8_t pal) {
  loadPalette(targetPalette, pal);
  if (progress() < 0xFFFFU) {
    // blend palettes
    // there are about 255 blend passes of 48 "blends" to completely blend two palettes (in _dur time)
    // minimum blend time is 100ms maximum is 65535ms
    unsigned long timeMS = millis() - _t->_start;
    uint16_t noOfBlends = (255U * timeMS / _t->_dur) - _t->_prevPaletteBlends;
    for (int i=0; i<noOfBlends; i++, _t->_prevPaletteBlends++) nblendPaletteTowardPalette(_t->_palT, targetPalette, 48);
    targetPalette = _t->_palT; // copy transitioning/temporary palette
  }
  return targetPalette;
}

// relies on WS2812FX::service() to call it max every 8ms or more (MIN_SHOW_DELAY)
void Segment::handleRandomPalette() {
  // just do a blend; if the palettes are identical it will just compare 48 bytes (same as _randomPalette == _newRandomPalette)
  // this will slowly blend _newRandomPalette into _randomPalette every 15ms or 8ms (depending on MIN_SHOW_DELAY)
  nblendPaletteTowardPalette(_randomPalette, _newRandomPalette, 48);
}

// segId is given when called from network callback, changes are queued if that segment is currently in its effect function
void Segment::setUp(uint16_t i1, uint16_t i2, uint8_t grp, uint8_t spc, uint16_t ofs, uint16_t i1Y, uint16_t i2Y, uint8_t segId) {
  // return if neither bounds nor grouping have changed
  bool boundsUnchanged = (start == i1 && stop == i2);
  #ifndef WLED_DISABLE_2D
  if (Segment::maxHeight>1) boundsUnchanged &= (startY == i1Y && stopY == i2Y); // 2D
  #endif
  if (boundsUnchanged
      && (!grp || (grouping == grp && spacing == spc))
      && (ofs == UINT16_MAX || ofs == offset)) return;

  if (stop) fill(BLACK); // turn old segment range off (clears pixels if changing spacing)
  if (grp) { // prevent assignment of 0
    grouping = grp;
    spacing = spc;
  } else {
    grouping = 1;
    spacing = 0;
  }
  if (ofs < UINT16_MAX) offset = ofs;

  markForReset();
  if (boundsUnchanged) return;

  // apply change immediately
  if (i2 <= i1) { //disable segment
    stop = 0;
    return;
  }
  if (i1 < Segment::maxWidth || (i1 >= Segment::maxWidth*Segment::maxHeight && i1 < strip.getLengthTotal())) start = i1; // Segment::maxWidth equals strip.getLengthTotal() for 1D
  stop = i2 > Segment::maxWidth*Segment::maxHeight ? MIN(i2,strip.getLengthTotal()) : (i2 > Segment::maxWidth ? Segment::maxWidth : MAX(1,i2));
  startY = 0;
  stopY  = 1;
  #ifndef WLED_DISABLE_2D
  if (Segment::maxHeight>1) { // 2D
    if (i1Y < Segment::maxHeight) startY = i1Y;
    stopY = i2Y > Segment::maxHeight ? Segment::maxHeight : MAX(1,i2Y);
  }
  #endif
  // safety check
  if (start >= stop || startY >= stopY) {
    stop = 0;
    return;
  }
  refreshLightCapabilities();
}


bool Segment::setColor(uint8_t slot, uint32_t c) { //returns true if changed
  if (slot >= NUM_COLORS || c == colors[slot]) return false;
  if (!_isRGB && !_hasW) {
    if (slot == 0 && c == BLACK) return false; // on/off segment cannot have primary color black
    if (slot == 1 && c != BLACK) return false; // on/off segment cannot have secondary color non black
  }
  if (fadeTransition) startTransition(strip.getTransition()); // start transition prior to change
  colors[slot] = c;
  stateChanged = true; // send UDP/WS broadcast
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
  stateChanged = true; // send UDP/WS broadcast
}

void Segment::setOpacity(uint8_t o) {
  if (opacity == o) return;
  if (fadeTransition) startTransition(strip.getTransition()); // start transition prior to change
  DEBUG_PRINT(F("-- Setting opacity: ")); DEBUG_PRINTLN(o);
  opacity = o;
  stateChanged = true; // send UDP/WS broadcast
}

void Segment::setOption(uint8_t n, bool val) {
  bool prevOn = on;
  if (fadeTransition && n == SEG_OPTION_ON && val != prevOn) startTransition(strip.getTransition()); // start transition prior to change
  if (val) options |=   0x01 << n;
  else     options &= ~(0x01 << n);
  if (!(n == SEG_OPTION_SELECTED || n == SEG_OPTION_RESET || n == SEG_OPTION_TRANSITIONAL)) stateChanged = true; // send UDP/WS broadcast
}

void Segment::setMode(uint8_t fx, bool loadDefaults) {
  // if we have a valid mode & is not reserved
  if (fx < strip.getModeCount() && strncmp_P("RSVD", strip.getModeData(fx), 4)) {
    if (fx != mode) {
      if (!transitional) { DEBUG_PRINT(F("-- Started mode transition. ")); DEBUG_PRINTLN(opacity);}
      if (fadeTransition) startTransition(strip.getTransition()); // set effect transitions
      mode = fx;
      // load default values from effect string
      if (loadDefaults) {
        int16_t sOpt;
        sOpt = extractModeDefaults(fx, "sx");   speed     = (sOpt >= 0) ? sOpt : DEFAULT_SPEED;
        sOpt = extractModeDefaults(fx, "ix");   intensity = (sOpt >= 0) ? sOpt : DEFAULT_INTENSITY;
        sOpt = extractModeDefaults(fx, "c1");   custom1   = (sOpt >= 0) ? sOpt : DEFAULT_C1;
        sOpt = extractModeDefaults(fx, "c2");   custom2   = (sOpt >= 0) ? sOpt : DEFAULT_C2;
        sOpt = extractModeDefaults(fx, "c3");   custom3   = (sOpt >= 0) ? sOpt : DEFAULT_C3;
        sOpt = extractModeDefaults(fx, "o1");   check1    = (sOpt >= 0) ? (bool)sOpt : false;
        sOpt = extractModeDefaults(fx, "o2");   check2    = (sOpt >= 0) ? (bool)sOpt : false;
        sOpt = extractModeDefaults(fx, "o3");   check3    = (sOpt >= 0) ? (bool)sOpt : false;
        sOpt = extractModeDefaults(fx, "m12");  if (sOpt >= 0) map1D2D   = constrain(sOpt, 0, 7);
        sOpt = extractModeDefaults(fx, "si");   if (sOpt >= 0) soundSim  = constrain(sOpt, 0, 1);
        sOpt = extractModeDefaults(fx, "rev");  if (sOpt >= 0) reverse   = (bool)sOpt;
        sOpt = extractModeDefaults(fx, "mi");   if (sOpt >= 0) mirror    = (bool)sOpt; // NOTE: setting this option is a risky business
        sOpt = extractModeDefaults(fx, "rY");   if (sOpt >= 0) reverse_y = (bool)sOpt;
        sOpt = extractModeDefaults(fx, "mY");   if (sOpt >= 0) mirror_y  = (bool)sOpt; // NOTE: setting this option is a risky business
        sOpt = extractModeDefaults(fx, "pal");  if (sOpt >= 0) setPalette(sOpt); //else setPalette(0);
      }
      markForReset();
      stateChanged = true; // send UDP/WS broadcast
    }
  }
}

void Segment::setPalette(uint8_t pal) {
  if (pal < 245 && pal > GRADIENT_PALETTE_COUNT+13) pal = 0; // built in palettes
  if (pal > 245 && (strip.customPalettes.size() == 0 || 255U-pal > strip.customPalettes.size()-1)) pal = 0; // custom palettes
  if (pal != palette) {
    if (strip.paletteFade) startTransition(strip.getTransition());
    palette = pal;
    stateChanged = true; // send UDP/WS broadcast
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
  uint16_t groupLen = groupLength(); // is always >= 1
  uint16_t vLength = (length() + groupLen - 1) / groupLen;
  if (mirror) vLength = (vLength + 1) /2;  // divide by 2 if mirror, leave at least a single LED
  return vLength;
}

void IRAM_ATTR Segment::setPixelColor(int i, uint32_t col)
{
  if (!isActive()) return; // not active
#ifndef WLED_DISABLE_2D
  int vStrip = i>>16; // hack to allow running on virtual strips (2D segment columns/rows)
#endif
  i &= 0xFFFF;

  if (i >= virtualLength() || i<0) return;  // if pixel would fall out of segment just exit

#ifndef WLED_DISABLE_2D
  if (is2D()) {
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
          // Bresenham’s Algorithm (may not fill every pixel)
          //int d = 3 - (2*i);
          //int y = i, x = 0;
          //while (y >= x) {
          //  setPixelColorXY(x, y, col);
          //  setPixelColorXY(y, x, col);
          //  x++;
          //  if (d > 0) {
          //    y--;
          //    d += 4 * (x - y) + 10;
          //  } else {
          //    d += 4 * x + 6;
          //  }
          //}
        }
        break;
      case M12_pCorner:
        for (int x = 0; x <= i; x++) setPixelColorXY(x, i, col);
        for (int y = 0; y <  i; y++) setPixelColorXY(i, y, col);
        break;
    }
    return;
  } else if (Segment::maxHeight!=1 && (width()==1 || height()==1)) {
    if (start < Segment::maxWidth*Segment::maxHeight) {
      // we have a vertical or horizontal 1D segment (WARNING: virtual...() may be transposed)
      int x = 0, y = 0;
      if (virtualHeight()>1) y = i;
      if (virtualWidth() >1) x = i;
      setPixelColorXY(x, y, col);
      return;
    }
  }
#endif

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

  uint32_t tmpCol = col;
  // set all the pixels in the group
  for (int j = 0; j < grouping; j++) {
    uint16_t indexSet = i + ((reverse) ? -j : j);
    if (indexSet >= start && indexSet < stop) {
      if (mirror) { //set the corresponding mirrored pixel
        uint16_t indexMir = stop - indexSet + start - 1;
        indexMir += offset; // offset/phase
        if (indexMir >= stop) indexMir -= len; // wrap
        if (_modeBlend) tmpCol = color_blend(strip.getPixelColor(indexMir), col, 0xFFFFU - progress(), true);
        strip.setPixelColor(indexMir, tmpCol);
      }
      indexSet += offset; // offset/phase
      if (indexSet >= stop) indexSet -= len; // wrap
      if (_modeBlend) tmpCol = color_blend(strip.getPixelColor(indexSet), col, 0xFFFFU - progress(), true);
      strip.setPixelColor(indexSet, tmpCol);
    }
  }
}

// anti-aliased normalized version of setPixelColor()
void Segment::setPixelColor(float i, uint32_t col, bool aa)
{
  if (!isActive()) return; // not active
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
  if (!isActive()) return 0; // not active
#ifndef WLED_DISABLE_2D
  int vStrip = i>>16;
#endif
  i &= 0xFFFF;

#ifndef WLED_DISABLE_2D
  if (is2D()) {
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

  if (reverse) i = virtualLength() - i - 1;
  i *= groupLength();
  i += start;
  /* offset/phase */
  i += offset;
  if ((i >= stop) && (stop>0)) i -= length(); // avoids negative pixel index (stop = 0 is a possible value)
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

  //bit pattern: (msb first) set:2, sound:1, mapping:3, transposed, mirrorY, reverseY, [transitional, reset,] paused, mirrored, on, reverse, [selected]
  if ((options & 0b1111111110011110U) != (b.options & 0b1111111110011110U)) d |= SEG_DIFFERS_OPT;
  if ((options & 0x0001U) != (b.options & 0x0001U))                         d |= SEG_DIFFERS_SEL;
  for (uint8_t i = 0; i < NUM_COLORS; i++) if (colors[i] != b.colors[i])    d |= SEG_DIFFERS_COL;

  return d;
}

void Segment::refreshLightCapabilities() {
  uint8_t capabilities = 0;
  uint16_t segStartIdx = 0xFFFFU;
  uint16_t segStopIdx  = 0;

  if (!isActive()) {
    _capabilities = 0;
    return;
  }

  if (start < Segment::maxWidth * Segment::maxHeight) {
    // we are withing 2D matrix (includes 1D segments)
    for (int y = startY; y < stopY; y++) for (int x = start; x < stop; x++) {
      uint16_t index = x + Segment::maxWidth * y;
      if (index < strip.customMappingSize) index = strip.customMappingTable[index]; // convert logical address to physical
      if (index < 0xFFFFU) {
        if (segStartIdx > index) segStartIdx = index;
        if (segStopIdx  < index) segStopIdx  = index;
      }
      if (segStartIdx == segStopIdx) segStopIdx++; // we only have 1 pixel segment
    }
  } else {
    // we are on the strip located after the matrix
    segStartIdx = start;
    segStopIdx  = stop;
  }

  for (uint8_t b = 0; b < busses.getNumBusses(); b++) {
    Bus *bus = busses.getBus(b);
    if (bus == nullptr || bus->getLength()==0) break;
    if (!bus->isOk()) continue;
    if (bus->getStart() >= segStopIdx) continue;
    if (bus->getStart() + bus->getLength() <= segStartIdx) continue;

    //uint8_t type = bus->getType();
    if (bus->hasRGB() || (cctFromRgb && bus->hasCCT())) capabilities |= SEG_CAPABILITY_RGB;
    if (!cctFromRgb && bus->hasCCT())                   capabilities |= SEG_CAPABILITY_CCT;
    if (correctWB && (bus->hasRGB() || bus->hasCCT()))  capabilities |= SEG_CAPABILITY_CCT; //white balance correction (CCT slider)
    if (bus->hasWhite()) {
      uint8_t aWM = Bus::getGlobalAWMode() == AW_GLOBAL_DISABLED ? bus->getAutoWhiteMode() : Bus::getGlobalAWMode();
      bool whiteSlider = (aWM == RGBW_MODE_DUAL || aWM == RGBW_MODE_MANUAL_ONLY); // white slider allowed
      // if auto white calculation from RGB is active (Accurate/Brighter), force RGB controls even if there are no RGB busses
      if (!whiteSlider) capabilities |= SEG_CAPABILITY_RGB;
      // if auto white calculation from RGB is disabled/optional (None/Dual), allow white channel adjustments
      if ( whiteSlider) capabilities |= SEG_CAPABILITY_W;
    }
  }
  _capabilities = capabilities;
}

/*
 * Fills segment with color
 */
void Segment::fill(uint32_t c) {
  if (!isActive()) return; // not active
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
void Segment::addPixelColor(int n, uint32_t color, bool fast) {
  if (!isActive()) return; // not active
  uint32_t col = getPixelColor(n);
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
  setPixelColor(n, col);
}

void Segment::fadePixelColor(uint16_t n, uint8_t fade) {
  if (!isActive()) return; // not active
  CRGB pix = CRGB(getPixelColor(n)).nscale8_video(fade);
  setPixelColor(n, pix);
}

/*
 * fade out function, higher rate = quicker fade
 */
void Segment::fade_out(uint8_t rate) {
  if (!isActive()) return; // not active
  const uint16_t cols = is2D() ? virtualWidth() : virtualLength();
  const uint16_t rows = virtualHeight(); // will be 1 for 1D

  rate = (255-rate) >> 1;
  float mappedRate = float(rate) +1.1f;

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
  if (!isActive() || fadeBy == 0) return;   // optimization - no scaling to apply
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
  if (!isActive() || blur_amount == 0) return; // optimization: 0 means "don't blur"
#ifndef WLED_DISABLE_2D
  if (is2D()) {
    // compatibility with 2D
    const uint_fast16_t cols = virtualWidth();
    const uint_fast16_t rows = virtualHeight();
    for (uint_fast16_t i = 0; i < rows; i++) blurRow(i, blur_amount); // blur all rows
    for (uint_fast16_t k = 0; k < cols; k++) blurCol(k, blur_amount); // blur all columns
    return;
  }
#endif
  uint8_t keep = 255 - blur_amount;
  uint8_t seep = blur_amount >> 1;
  CRGB carryover = CRGB::Black;
  uint_fast16_t vlength = virtualLength();
  for(uint_fast16_t i = 0; i < vlength; i++)
  {
    CRGB cur = CRGB(getPixelColor(i));
    CRGB part = cur;
    CRGB before = cur; // remember color before blur
    part.nscale8(seep);
    cur.nscale8(keep);
    cur += carryover;
    if(i > 0) {
      uint32_t c = getPixelColor(i-1);
      uint8_t r = R(c);
      uint8_t g = G(c);
      uint8_t b = B(c);
      setPixelColor((uint16_t)(i-1), qadd8(r, part.red), qadd8(g, part.green), qadd8(b, part.blue));
    }
    if (before != cur)     // optimization: only set pixel if color has changed
      setPixelColor((uint16_t)i,cur.red, cur.green, cur.blue);
    carryover = part;
  }
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
uint32_t Segment::color_wheel(uint8_t pos) {
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
 * @param wrap FastLED palettes will usually wrap back to the start smoothly. Set false to get a hard edge
 * @param mcol If the default palette 0 is selected, return the standard color 0, 1 or 2 instead. If >2, Party palette is used instead
 * @param pbri Value to scale the brightness of the returned color by. Default is 255. (no scaling)
 * @returns Single color from palette
 */
uint32_t Segment::color_from_palette(uint16_t i, bool mapping, bool wrap, uint8_t mcol, uint8_t pbri)
{
  // default palette or no RGB support on segment
  if ((palette == 0 && mcol < NUM_COLORS) || !_isRGB) {
    uint32_t color = currentColor(mcol, colors[mcol]);
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
    for (uint8_t i = 0; i < defNumBusses && i < WLED_MAX_BUSSES+WLED_MIN_VIRTUAL_BUSSES; i++) {
      uint8_t defPin[] = {defDataPins[i]};
      uint16_t start = prevLen;
      uint16_t count = defCounts[(i < defNumCounts) ? i : defNumCounts -1];
      prevLen += count;
      BusConfig defCfg = BusConfig(DEFAULT_LED_TYPE, defPin, start, count, DEFAULT_LED_COLOR_ORDER, false, 0, RGBW_MODE_MANUAL_ONLY);
      if (busses.add(defCfg) == -1) break;
    }
  }

  _length = 0;
  for (uint8_t i=0; i<busses.getNumBusses(); i++) {
    Bus *bus = busses.getBus(i);
    if (bus == nullptr) continue;
    if (bus->getStart() + bus->getLength() > MAX_LEDS) break;
    //RGBW mode is enabled if at least one of the strips is RGBW
    _hasWhiteChannel |= bus->hasWhite();
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

  if (isMatrix) setUpMatrix();
  else {
    Segment::maxWidth  = _length;
    Segment::maxHeight = 1;
  }

  //segments are created in makeAutoSegments();
  DEBUG_PRINTLN(F("Loading custom palettes"));
  loadCustomPalettes(); // (re)load all custom palettes
  DEBUG_PRINTLN(F("Loading custom ledmaps"));
  deserializeMap();     // (re)load default ledmap
}

void WS2812FX::service() {
  unsigned long nowUp = millis(); // Be aware, millis() rolls over every 49 days
  now = nowUp + timebase;
  if (nowUp - _lastShow < MIN_SHOW_DELAY) return;
  bool doShow = false;

  _isServicing = true;
  _segment_index = 0;
  Segment::handleRandomPalette(); // move it into for loop when each segment has individual random palette
  for (segment &seg : _segments) {
    // process transition (mode changes in the middle of transition)
    seg.handleTransition();
    // reset the segment runtime data if needed
    seg.resetIfRequired();

    if (!seg.isActive()) continue;

    // last condition ensures all solid segments are updated at the same time
    if (nowUp > seg.next_time || _triggered || (doShow && seg.mode == FX_MODE_STATIC))
    {
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

        // Effect blending
        // When two effects are being blended, each may have different segment data, this
        // data needs to be saved first and then restored before running previous/transitional mode.
        // The blending will largely depend on the effect behaviour since actual output (LEDs) may be
        // overwritten by later effect. To enable seamless blending for every effect, additional LED buffer
        // would need to be allocated for each effect and then blended together for each pixel.
        uint8_t tmpMode = seg.currentMode(seg.mode);  // this will return old mode while in transition
        delay = (*_mode[seg.mode])();         // run new/current mode
        if (seg.mode != tmpMode) {
          Segment::tmpsegd_t _tmpSegData;
          Segment::modeBlend(true);           // set semaphore
          seg.swapSegenv(_tmpSegData);        // temporarily store new mode state (and swap it with transitional state)
          uint16_t d2 = (*_mode[tmpMode])();  // run old mode
          seg.restoreSegenv(_tmpSegData);     // restore mode state (will also update transitional state)
          delay = MIN(delay,d2);              // use shortest delay
          Segment::modeBlend(false);          // unset semaphore
        }
        if (seg.mode != FX_MODE_HALLOWEEN_EYES) seg.call++;
        if (seg.transitional && delay > FRAMETIME) delay = FRAMETIME; // force faster updates during transition
      }

      seg.next_time = nowUp + delay;
    }
    if (_segment_index == _queuedChangesSegId) setUpSegmentFromQueuedChanges();
    _segment_index++;
  }
  _virtualSegmentLength = 0;
  busses.setSegmentCCT(-1);
  _isServicing = false;
  _triggered = false;

  #ifdef WLED_DEBUG
  if (millis() - nowUp > _frametime) DEBUG_PRINTLN(F("Slow effects."));
  #endif
  if (doShow) {
    yield();
    show();
  }
  #ifdef WLED_DEBUG
  if (millis() - nowUp > _frametime) DEBUG_PRINTLN(F("Slow strip."));
  #endif
}

void IRAM_ATTR WS2812FX::setPixelColor(int i, uint32_t col)
{
  if (i < customMappingSize) i = customMappingTable[i];
  if (i >= _length) return;
  busses.setPixelColor(i, col);
}

uint32_t WS2812FX::getPixelColor(uint16_t i)
{
  if (i < customMappingSize) i = customMappingTable[i];
  if (i >= _length) return 0;
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

uint8_t WS2812FX::estimateCurrentAndLimitBri() {
  //power limit calculation
  //each LED can draw up 195075 "power units" (approx. 53mA)
  //one PU is the power it takes to have 1 channel 1 step brighter per brightness step
  //so A=2,R=255,G=0,B=0 would use 510 PU per LED (1mA is about 3700 PU)
  bool useWackyWS2815PowerModel = false;
  byte actualMilliampsPerLed = milliampsPerLed;

  if (ablMilliampsMax < 150 || actualMilliampsPerLed == 0) { //0 mA per LED and too low numbers turn off calculation
    currentMilliamps = 0;
    return _brightness;
  }

  if (milliampsPerLed == 255) {
    useWackyWS2815PowerModel = true;
    actualMilliampsPerLed = 12; // from testing an actual strip
  }

  size_t powerBudget = (ablMilliampsMax - MA_FOR_ESP); //100mA for ESP power

  size_t pLen = 0; //getLengthPhysical();
  size_t powerSum = 0;
  for (uint_fast8_t bNum = 0; bNum < busses.getNumBusses(); bNum++) {
    Bus *bus = busses.getBus(bNum);
    if (!IS_DIGITAL(bus->getType())) continue; //exclude non-digital network busses
    uint16_t len = bus->getLength();
    pLen += len;
    uint32_t busPowerSum = 0;
    for (uint_fast16_t i = 0; i < len; i++) { //sum up the usage of each LED
      uint32_t c = bus->getPixelColor(i); // always returns original or restored color without brightness scaling
      byte r = R(c), g = G(c), b = B(c), w = W(c);

      if(useWackyWS2815PowerModel) { //ignore white component on WS2815 power calculation
        busPowerSum += (MAX(MAX(r,g),b)) * 3;
      } else {
        busPowerSum += (r + g + b + w);
      }
    }

    if (bus->hasWhite()) { //RGBW led total output with white LEDs enabled is still 50mA, so each channel uses less
      busPowerSum *= 3;
      busPowerSum >>= 2; //same as /= 4
    }
    powerSum += busPowerSum;
  }

  if (powerBudget > pLen) { //each LED uses about 1mA in standby, exclude that from power budget
    powerBudget -= pLen;
  } else {
    powerBudget = 0;
  }

  // powerSum has all the values of channels summed (max would be pLen*765 as white is excluded) so convert to milliAmps
  powerSum = (powerSum * actualMilliampsPerLed) / 765;

  uint8_t newBri = _brightness;
  if (powerSum * _brightness / 255 > powerBudget) { //scale brightness down to stay in current limit
    float scale = (float)(powerBudget * 255) / (float)(powerSum * _brightness);
    uint16_t scaleI = scale * 255;
    uint8_t scaleB = (scaleI > 255) ? 255 : scaleI;
    newBri = scale8(_brightness, scaleB) + 1;
  }
  currentMilliamps = (powerSum * newBri) / 255;
  currentMilliamps += MA_FOR_ESP; //add power of ESP back to estimate
  currentMilliamps += pLen; //add standby power (1mA/LED) back to estimate
  return newBri;
}

void WS2812FX::show(void) {
  // avoid race condition, caputre _callback value
  show_callback callback = _callback;
  if (callback) callback();

  uint8_t newBri = estimateCurrentAndLimitBri();
  busses.setBrightness(newBri); // "repaints" all pixels if brightness changed

  // some buses send asynchronously and this method will return before
  // all of the data has been sent.
  // See https://github.com/Makuna/NeoPixelBus/wiki/ESP32-NeoMethods#neoesp32rmt-methods
  busses.show();

  // restore bus brightness to its original value
  // this is done right after show, so this is only OK if LED updates are completed before show() returns
  // or async show has a separate buffer (ESP32 RMT and I2S are ok)
  if (newBri < _brightness) busses.setBrightness(_brightness);

  unsigned long now = millis();
  size_t diff = now - _lastShow;
  size_t fpsCurr = 200;
  if (diff > 0) fpsCurr = 1000 / diff;
  _cumulativeFps = (3 * _cumulativeFps + fpsCurr +2) >> 2;   // "+2" for proper rounding (2/4 = 0.5)
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
    _segments[segid].setMode(m); // do not load defaults
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

// direct=true either expects the caller to call show() themselves (realtime modes) or be ok waiting for the next frame for the change to apply
// direct=false immediately triggers an effect redraw
void WS2812FX::setBrightness(uint8_t b, bool direct) {
  if (gammaCorrectBri) b = gamma8(b);
  if (_brightness == b) return;
  _brightness = b;
  if (_brightness == 0) { //unfreeze all segments on power off
    for (segment &seg : _segments) {
      seg.freeze = false;
    }
  }
  // setting brightness with NeoPixelBusLg has no effect on already painted pixels,
  // so we need to force an update to existing buffer
  busses.setBrightness(b);
  if (!direct) {
    unsigned long t = millis();
    if (_segments[0].next_time > t + 22 && t - _lastShow > MIN_SHOW_DELAY) trigger(); //apply brightness change immediately if no refresh soon
  }
}

uint8_t WS2812FX::getActiveSegsLightCapabilities(bool selectedOnly) {
  uint8_t totalLC = 0;
  for (segment &seg : _segments) {
    if (seg.isActive() && (!selectedOnly || seg.isSelected())) totalLC |= seg.getLightCapabilities();
  }
  return totalLC;
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

uint16_t WS2812FX::getLengthTotal(void) {
  uint16_t len = Segment::maxWidth * Segment::maxHeight; // will be _length for 1D (see finalizeInit()) but should cover whole matrix for 2D
  if (isMatrix && _length > len) len = _length; // for 2D with trailing strip
  return len;
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
    if (bus->hasRGB() && bus->hasWhite()) return true;
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

// sets new segment bounds, queues if that segment is currently running
void WS2812FX::setSegment(uint8_t segId, uint16_t i1, uint16_t i2, uint8_t grouping, uint8_t spacing, uint16_t offset, uint16_t startY, uint16_t stopY) {
  if (segId >= getSegmentsNum()) {
    if (i2 <= i1) return; // do not append empty/inactive segments
    appendSegment(Segment(0, strip.getLengthTotal()));
    segId = getSegmentsNum()-1; // segments are added at the end of list
  }

  if (_queuedChangesSegId == segId) _queuedChangesSegId = 255; // cancel queued change if already queued for this segment

  if (segId < getMaxSegments() && segId == getCurrSegmentId() && isServicing()) { // queue change to prevent concurrent access
    // queuing a change for a second segment will lead to the loss of the first change if not yet applied
    // however this is not a problem as the queued change is applied immediately after the effect function in that segment returns
    _qStart  = i1; _qStop   = i2; _qStartY = startY; _qStopY  = stopY;
    _qGrouping = grouping; _qSpacing  = spacing; _qOffset   = offset;
    _queuedChangesSegId = segId;
    return; // queued changes are applied immediately after effect function returns
  }
  
  _segments[segId].setUp(i1, i2, grouping, spacing, offset, startY, stopY);
}

void WS2812FX::setUpSegmentFromQueuedChanges() {
  if (_queuedChangesSegId >= getSegmentsNum()) return;
  getSegment(_queuedChangesSegId).setUp(_qStart, _qStop, _qGrouping, _qSpacing, _qOffset, _qStartY, _qStopY);
  _queuedChangesSegId = 255;
}

void WS2812FX::restartRuntime() {
  for (segment &seg : _segments) seg.markForReset();
}

void WS2812FX::resetSegments() {
  _segments.clear(); // destructs all Segment as part of clearing
  #ifndef WLED_DISABLE_2D
  segment seg = isMatrix ? Segment(0, Segment::maxWidth, 0, Segment::maxHeight) : Segment(0, _length);
  #else
  segment seg = Segment(0, _length);
  #endif
  _segments.push_back(seg);
  _mainSegment = 0;
}

void WS2812FX::makeAutoSegments(bool forceReset) {
  if (autoSegments) { //make one segment per bus
    uint16_t segStarts[MAX_NUM_SEGMENTS] = {0};
    uint16_t segStops [MAX_NUM_SEGMENTS] = {0};
    size_t s = 0;

    #ifndef WLED_DISABLE_2D
    // 2D segment is the 1st one using entire matrix
    if (isMatrix) {
      segStarts[0] = 0;
      segStops[0]  = Segment::maxWidth*Segment::maxHeight;
      s++;
    }
    #endif

    for (size_t i = s; i < busses.getNumBusses(); i++) {
      Bus* b = busses.getBus(i);

      segStarts[s] = b->getStart();
      segStops[s]  = segStarts[s] + b->getLength();

      #ifndef WLED_DISABLE_2D
      if (isMatrix && segStops[s] < Segment::maxWidth*Segment::maxHeight) continue; // ignore buses comprising matrix
      if (isMatrix && segStarts[s] < Segment::maxWidth*Segment::maxHeight) segStarts[s] = Segment::maxWidth*Segment::maxHeight;
      #endif

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
    _segments.reserve(s); // prevent reallocations
    // there is always at least one segment (but we need to differentiate between 1D and 2D)
    #ifndef WLED_DISABLE_2D
    if (isMatrix)
      _segments.push_back(Segment(0, Segment::maxWidth, 0, Segment::maxHeight));
    else
    #endif
      _segments.push_back(Segment(segStarts[0], segStops[0]));
    for (size_t i = 1; i < s; i++) {
      _segments.push_back(Segment(segStarts[i], segStops[i]));
    }

  } else {

    if (forceReset || getSegmentsNum() == 0) resetSegments();
    //expand the main seg to the entire length, but only if there are no other segments, or reset is forced
    else if (getActiveSegmentsNum() == 1) {
      size_t i = getLastActiveSegmentId();
      #ifndef WLED_DISABLE_2D
      _segments[i].start  = 0;
      _segments[i].stop   = Segment::maxWidth;
      _segments[i].startY = 0;
      _segments[i].stopY  = Segment::maxHeight;
      _segments[i].grouping = 1;
      _segments[i].spacing  = 0;
      #else
      _segments[i].start = 0;
      _segments[i].stop  = _length;
      #endif
    }
  }
  _mainSegment = 0;

  fixInvalidSegments();
}

void WS2812FX::fixInvalidSegments() {
  //make sure no segment is longer than total (sanity check)
  for (size_t i = getSegmentsNum()-1; i > 0; i--) {
    if (isMatrix) {
    #ifndef WLED_DISABLE_2D
      if (_segments[i].start >= Segment::maxWidth * Segment::maxHeight) {
        // 1D segment at the end of matrix
        if (_segments[i].start >= _length || _segments[i].startY > 0 || _segments[i].stopY > 1) { _segments.erase(_segments.begin()+i); continue; }
        if (_segments[i].stop  >  _length) _segments[i].stop = _length;
        continue;
      }
      if (_segments[i].start >= Segment::maxWidth || _segments[i].startY >= Segment::maxHeight) { _segments.erase(_segments.begin()+i); continue; }
      if (_segments[i].stop  >  Segment::maxWidth)  _segments[i].stop  = Segment::maxWidth;
      if (_segments[i].stopY >  Segment::maxHeight) _segments[i].stopY = Segment::maxHeight;
    #endif
    } else {
      if (_segments[i].start >= _length) { _segments.erase(_segments.begin()+i); continue; }
      if (_segments[i].stop  >  _length) _segments[i].stop = _length;
    }
  }
  // this is always called as the last step after finalizeInit(), update covered bus types
  for (segment &seg : _segments)
    seg.refreshLightCapabilities();
}

//true if all segments align with a bus, or if a segment covers the total length
//irrelevant in 2D set-up
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
uint8_t WS2812FX::setPixelSegment(uint8_t n) {
  uint8_t prevSegId = _segment_index;
  if (n < _segments.size()) {
    _segment_index = n;
    _virtualSegmentLength = _segments[_segment_index].virtualLength();
  }
  return prevSegId;
}

void WS2812FX::setRange(uint16_t i, uint16_t i2, uint32_t col) {
  if (i2 >= i)
  {
    for (uint16_t x = i; x <= i2; x++) setPixelColor(x, col);
  } else
  {
    for (uint16_t x = i2; x <= i; x++) setPixelColor(x, col);
  }
}

void WS2812FX::setTransitionMode(bool t) {
  for (segment &seg : _segments) seg.startTransition(t ? _transitionDur : 0);
}

#ifdef WLED_DEBUG
void WS2812FX::printSize() {
  size_t size = 0;
  for (const Segment &seg : _segments) size += seg.getSize();
  DEBUG_PRINTF("Segments: %d -> %uB\n", _segments.size(), size);
  DEBUG_PRINTF("Modes: %d*%d=%uB\n", sizeof(mode_ptr), _mode.size(), (_mode.capacity()*sizeof(mode_ptr)));
  DEBUG_PRINTF("Data: %d*%d=%uB\n", sizeof(const char *), _modeData.size(), (_modeData.capacity()*sizeof(const char *)));
  DEBUG_PRINTF("Map: %d*%d=%uB\n", sizeof(uint16_t), (int)customMappingSize, customMappingSize*sizeof(uint16_t));
  size = getLengthTotal();
  if (useGlobalLedBuffer) DEBUG_PRINTF("Buffer: %d*%u=%uB\n", sizeof(CRGB), size, size*sizeof(CRGB));
}
#endif

void WS2812FX::loadCustomPalettes() {
  byte tcp[72]; //support gradient palettes with up to 18 entries
  CRGBPalette16 targetPalette;
  customPalettes.clear(); // start fresh
  for (int index = 0; index<10; index++) {
    char fileName[32];
    sprintf_P(fileName, PSTR("/palette%d.json"), index);

    StaticJsonDocument<1536> pDoc; // barely enough to fit 72 numbers
    if (WLED_FS.exists(fileName)) {
      DEBUG_PRINT(F("Reading palette from "));
      DEBUG_PRINTLN(fileName);

      if (readObjectFromFile(fileName, nullptr, &pDoc)) {
        JsonArray pal = pDoc[F("palette")];
        if (!pal.isNull() && pal.size()>4) { // not an empty palette (at least 2 entries)
          if (pal[0].is<int>() && pal[1].is<const char *>()) {
            // we have an array of index & hex strings
            size_t palSize = MIN(pal.size(), 36);
            palSize -= palSize % 2; // make sure size is multiple of 2
            for (size_t i=0, j=0; i<palSize && pal[i].as<int>()<256; i+=2, j+=4) {
              uint8_t rgbw[] = {0,0,0,0};
              tcp[ j ] = (uint8_t) pal[ i ].as<int>(); // index
              colorFromHexString(rgbw, pal[i+1].as<const char *>()); // will catch non-string entires
              for (size_t c=0; c<3; c++) tcp[j+1+c] = rgbw[c]; // only use RGB component
              DEBUG_PRINTF("%d(%d) : %d %d %d\n", i, int(tcp[j]), int(tcp[j+1]), int(tcp[j+2]), int(tcp[j+3]));
            }
          } else {
            size_t palSize = MIN(pal.size(), 72);
            palSize -= palSize % 4; // make sure size is multiple of 4
            for (size_t i=0; i<palSize && pal[i].as<int>()<256; i+=4) {
              tcp[ i ] = (uint8_t) pal[ i ].as<int>(); // index
              tcp[i+1] = (uint8_t) pal[i+1].as<int>(); // R
              tcp[i+2] = (uint8_t) pal[i+2].as<int>(); // G
              tcp[i+3] = (uint8_t) pal[i+3].as<int>(); // B
              DEBUG_PRINTF("%d(%d) : %d %d %d\n", i, int(tcp[i]), int(tcp[i+1]), int(tcp[i+2]), int(tcp[i+3]));
            }
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
bool WS2812FX::deserializeMap(uint8_t n) {
  // 2D support creates its own ledmap (on the fly) if a ledmap.json exists it will overwrite built one.

  char fileName[32];
  strcpy_P(fileName, PSTR("/ledmap"));
  if (n) sprintf(fileName +7, "%d", n);
  strcat(fileName, ".json");
  bool isFile = WLED_FS.exists(fileName);

  if (!isFile) {
    // erase custom mapping if selecting nonexistent ledmap.json (n==0)
    if (!isMatrix && !n && customMappingTable != nullptr) {
      customMappingSize = 0;
      delete[] customMappingTable;
      customMappingTable = nullptr;
    }
    return false;
  }

  if (!requestJSONBufferLock(7)) return false;

  if (!readObjectFromFile(fileName, nullptr, &doc)) {
    releaseJSONBufferLock();
    return false; //if file does not exist just exit
  }

  DEBUG_PRINT(F("Reading LED map from "));
  DEBUG_PRINTLN(fileName);

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
      customMappingTable[i] = (uint16_t) (map[i]<0 ? 0xFFFFU : map[i]);
    }
  }

  releaseJSONBufferLock();
  return true;
}


WS2812FX* WS2812FX::instance = nullptr;

const char JSON_mode_names[] PROGMEM = R"=====(["FX names moved"])=====";
const char JSON_palette_names[] PROGMEM = R"=====([
"Default","* Random Cycle","* Color 1","* Colors 1&2","* Color Gradient","* Colors Only","Party","Cloud","Lava","Ocean",
"Forest","Rainbow","Rainbow Bands","July","Vintage 57","Vintage 01","Rivendell","RGI 15","Retro","Analogous",
"Pink Splash 08","Pink Splash 07","Coral Reef","Ocean Breeze 68","Ocean Breeze 36","Departure","Landscape 64","Landscape 33","Sherbet","Hult 65",
"Hult 64","Drywet","IB15","Fuschia","Emerald Dragon 08","Hot Lava","Fire","Hiyane","Colorfull","Magenta Evening",
"Pink Purple","Sunset","Autumn","Blue/Magenta/White","Blue/Magenta/Red","Blue/Red/Yellow","Blue/Cyan/Yellow","Sunset Yellow","Cloud","Fire & Ice",
"BHW2","Rainfall","Angel","Butterfly","250K Meters","Night Midnight","Afterdusk","Blue Sky","Gold Orange", "Frizell 10",
"Frizell 12","Fib 18","Fib 13","Fib 17","Fib 05","Analogous 02","Analogous 04a","Cyan Orange","C/W/G","Wild Orange",
"Ikat Radial","Citrus","Teal Blue","Ldby Orange","Purple/Orange","Blue/Tan","Green/Purple","Knoza 00","Knoza 18","Calpan",
"Calbayo","Fib53","Purple/Orange","PMH","Konjo 08","Konikyo","McCahon","Pills","Pink/Yellow/Orange","Autumn 04",
"Autumn 02","Candide","Chic","Coffee","Emerald Dragon 01","Landscape 57","Landscape 22","Landscape 47","Landscape 10","Landscape 76",
"Landscape 61","Landscape 60","Landscape 51","Landscape 06","Ocean Breeze 49","Ocean Breeze 57","Ocean Breeze 74","Pink Splash 05","Pink Splash 10","Vintage 56",
"Vintage 10","Gold/Yellow","Radioactive Slime","Pastel Rainbow","Purple Sunset","Adrift in Dreams","Set3","Pastel1","Es Rosa","Daybreak",
"Melancholiy","Xanidu","Air","Revolution","Sky05","Sky33","Sky45","Carousel","NRWC"
])=====";

