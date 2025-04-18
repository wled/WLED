/*
  WS2812FX_fcn.cpp contains all utility functions
  Harm Aldick - 2016
  www.aldick.org

  Copyright (c) 2016  Harm Aldick
  Licensed under the EUPL v. 1.2 or later
  Adapted from code originally licensed under the MIT license

  Modified heavily for WLED
*/
#include "wled.h"
#include "FX.h"
#include "FXparticleSystem.h"  // TODO: better define the required function (mem service) in FX.h?
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

#ifndef PIXEL_COUNTS
  #define PIXEL_COUNTS DEFAULT_LED_COUNT
#endif

#ifndef DATA_PINS
  #define DATA_PINS DEFAULT_LED_PIN
#endif

#ifndef LED_TYPES
  #define LED_TYPES DEFAULT_LED_TYPE
#endif

#ifndef DEFAULT_LED_COLOR_ORDER
  #define DEFAULT_LED_COLOR_ORDER COL_ORDER_GRB  //default to GRB
#endif


#if MAX_NUM_SEGMENTS < WLED_MAX_BUSSES
  #error "Max segments must be at least max number of busses!"
#endif

static constexpr unsigned sumPinsRequired(const unsigned* current, size_t count) {
 return (count > 0) ? (Bus::getNumberOfPins(*current) + sumPinsRequired(current+1,count-1)) : 0;
}

static constexpr bool validatePinsAndTypes(const unsigned* types, unsigned numTypes, unsigned numPins ) {
  // Pins provided < pins required -> always invalid
  // Pins provided = pins required -> always valid
  // Pins provided > pins required -> valid if excess pins are a product of last type pins since it will be repeated
  return (sumPinsRequired(types, numTypes) > numPins) ? false :
          (numPins - sumPinsRequired(types, numTypes)) % Bus::getNumberOfPins(types[numTypes-1]) == 0;
}


///////////////////////////////////////////////////////////////////////////////
// Segment class implementation
///////////////////////////////////////////////////////////////////////////////
unsigned      Segment::_usedSegmentData   = 0U; // amount of RAM all segments use for their data[]
uint16_t      Segment::maxWidth           = DEFAULT_LED_COUNT;
uint16_t      Segment::maxHeight          = 1;
unsigned      Segment::_vLength           = 0;
unsigned      Segment::_vWidth            = 0;
unsigned      Segment::_vHeight           = 0;
uint8_t       Segment::_segBri            = 0;
uint32_t      Segment::_currentColors[NUM_COLORS] = {0,0,0};
bool          Segment::_colorScaled       = false;
CRGBPalette16 Segment::_currentPalette    = CRGBPalette16(CRGB::Black);
CRGBPalette16 Segment::_randomPalette     = generateRandomPalette();  // was CRGBPalette16(DEFAULT_COLOR);
CRGBPalette16 Segment::_newRandomPalette  = generateRandomPalette();  // was CRGBPalette16(DEFAULT_COLOR);
uint16_t      Segment::_lastPaletteChange = 0; // perhaps it should be per segment
uint16_t      Segment::_lastPaletteBlend  = 0; //in millis (lowest 16 bits only)
uint16_t      Segment::_transitionprogress  = 0xFFFF;

#ifndef WLED_DISABLE_MODE_BLEND
bool Segment::_modeBlend = false;
uint16_t Segment::_clipStart = 0;
uint16_t Segment::_clipStop = 0;
uint8_t  Segment::_clipStartY = 0;
uint8_t  Segment::_clipStopY = 1;
#endif

// copy constructor
Segment::Segment(const Segment &orig) {
  //DEBUG_PRINTF_P(PSTR("-- Copy segment constructor: %p -> %p\n"), &orig, this);
  memcpy((void*)this, (void*)&orig, sizeof(Segment));
  _t = nullptr; // copied segment cannot be in transition
  name = nullptr;
  data = nullptr;
  _dataLen = 0;
  if (orig.name) { name = static_cast<char*>(malloc(strlen(orig.name)+1)); if (name) strcpy(name, orig.name); }
  if (orig.data) { if (allocateData(orig._dataLen)) memcpy(data, orig.data, orig._dataLen); }
}

// move constructor
Segment::Segment(Segment &&orig) noexcept {
  //DEBUG_PRINTF_P(PSTR("-- Move segment constructor: %p -> %p\n"), &orig, this);
  memcpy((void*)this, (void*)&orig, sizeof(Segment));
  orig._t   = nullptr; // old segment cannot be in transition any more
  orig.name = nullptr;
  orig.data = nullptr;
  orig._dataLen = 0;
}

// copy assignment
Segment& Segment::operator= (const Segment &orig) {
  //DEBUG_PRINTF_P(PSTR("-- Copying segment: %p -> %p\n"), &orig, this);
  if (this != &orig) {
    // clean destination
    if (name) { free(name); name = nullptr; }
    stopTransition();
    deallocateData();
    // copy source
    memcpy((void*)this, (void*)&orig, sizeof(Segment));
    // erase pointers to allocated data
    data = nullptr;
    _dataLen = 0;
    // copy source data
    if (orig.name) { name = static_cast<char*>(malloc(strlen(orig.name)+1)); if (name) strcpy(name, orig.name); }
    if (orig.data) { if (allocateData(orig._dataLen)) memcpy(data, orig.data, orig._dataLen); }
  }
  return *this;
}

// move assignment
Segment& Segment::operator= (Segment &&orig) noexcept {
  //DEBUG_PRINTF_P(PSTR("-- Moving segment: %p -> %p\n"), &orig, this);
  if (this != &orig) {
    if (name) { free(name); name = nullptr; } // free old name
    stopTransition();
    deallocateData(); // free old runtime data
    memcpy((void*)this, (void*)&orig, sizeof(Segment));
    orig.name = nullptr;
    orig.data = nullptr;
    orig._dataLen = 0;
    orig._t   = nullptr; // old segment cannot be in transition
  }
  return *this;
}

// allocates effect data buffer on heap and initialises (erases) it
bool IRAM_ATTR_YN Segment::allocateData(size_t len) {
  if (len == 0) return false; // nothing to do
  if (data && _dataLen >= len) {          // already allocated enough (reduce fragmentation)
    if (call == 0) memset(data, 0, len);  // erase buffer if called during effect initialisation
    return true;
  }
  //DEBUG_PRINTF_P(PSTR("--   Allocating data (%d): %p\n", len, this);
  deallocateData(); // if the old buffer was smaller release it first
  if (Segment::getUsedSegmentData() + len > MAX_SEGMENT_DATA) {
    // not enough memory
    DEBUG_PRINT(F("!!! Effect RAM depleted: "));
    DEBUG_PRINTF_P(PSTR("%d/%d !!!\n"), len, Segment::getUsedSegmentData());
    errorFlag = ERR_NORAM;
    return false;
  }
  // do not use SPI RAM on ESP32 since it is slow
  data = (byte*)calloc(len, sizeof(byte));
  if (!data) { DEBUG_PRINTLN(F("!!! Allocation failed. !!!")); return false; } // allocation failed
  Segment::addUsedSegmentData(len);
  //DEBUG_PRINTF_P(PSTR("---  Allocated data (%p): %d/%d -> %p\n"), this, len, Segment::getUsedSegmentData(), data);
  _dataLen = len;
  return true;
}

void IRAM_ATTR_YN Segment::deallocateData() {
  if (!data) { _dataLen = 0; return; }
  //DEBUG_PRINTF_P(PSTR("---  Released data (%p): %d/%d -> %p\n"), this, _dataLen, Segment::getUsedSegmentData(), data);
  if ((Segment::getUsedSegmentData() > 0) && (_dataLen > 0)) { // check that we don't have a dangling / inconsistent data pointer
    free(data);
  } else {
    DEBUG_PRINTF_P(PSTR("---- Released data (%p): inconsistent UsedSegmentData (%d/%d), cowardly refusing to free nothing.\n"), this, _dataLen, Segment::getUsedSegmentData());
  }
  data = nullptr;
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
  //DEBUG_PRINTF_P(PSTR("-- Segment reset: %p\n"), this);
  if (data && _dataLen > 0) memset(data, 0, _dataLen);  // prevent heap fragmentation (just erase buffer instead of deallocateData())
  next_time = 0; step = 0; call = 0; aux0 = 0; aux1 = 0;
  reset = false;
  #ifdef WLED_ENABLE_GIF
  endImagePlayback(this);
  #endif
}

CRGBPalette16 &Segment::loadPalette(CRGBPalette16 &targetPalette, uint8_t pal) {
  if (pal < 245 && pal > GRADIENT_PALETTE_COUNT+13) pal = 0;
  if (pal > 245 && (strip.customPalettes.size() == 0 || 255U-pal > strip.customPalettes.size()-1)) pal = 0; // TODO remove strip dependency by moving customPalettes out of strip
  //default palette. Differs depending on effect
  if (pal == 0) pal = _default_palette; //load default palette set in FX _data, party colors as default
  switch (pal) {
    case 0: //default palette. Exceptions for specific effects above
      targetPalette = PartyColors_p; break;
    case 1: //randomly generated palette
      targetPalette = _randomPalette; //random palette is generated at intervals in handleRandomPalette()
      break;
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
    case 5: {//primary + secondary (+tertiary if not off), more distinct
      CRGB prim = gamma32(colors[0]);
      CRGB sec  = gamma32(colors[1]);
      if (colors[2]) {
        CRGB ter = gamma32(colors[2]);
        targetPalette = CRGBPalette16(prim,prim,prim,prim,prim,sec,sec,sec,sec,sec,ter,ter,ter,ter,ter,prim);
      } else {
        targetPalette = CRGBPalette16(prim,prim,prim,prim,prim,prim,prim,prim,sec,sec,sec,sec,sec,sec,sec,sec);
      }
      break;}
    default: //progmem palettes
      if (pal>245) {
        targetPalette = strip.customPalettes[255-pal]; // we checked bounds above
      } else if (pal < 13) { // palette 6 - 12, fastled palettes
        targetPalette = *fastledPalettes[pal-6];
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
  if (dur == 0) {
    if (isInTransition()) _t->_dur = dur; // this will stop transition in next handleTransition()
    return;
  }
  if (isInTransition()) return; // already in transition no need to store anything

  // starting a transition has to occur before change so we get current values 1st
  _t = new(std::nothrow) Transition(dur); // no previous transition running
  if (!_t) return; // failed to allocate data

  //DEBUG_PRINTF_P(PSTR("-- Started transition: %p (%p)\n"), this, _t);
  loadPalette(_t->_palT, palette);
  _t->_palTid         = palette;
  _t->_briT           = on ? opacity : 0;
  _t->_cctT           = cct;
#ifndef WLED_DISABLE_MODE_BLEND
  swapSegenv(_t->_segT); // copy runtime data to temporary
  _t->_modeT          = mode;
  _t->_segT._dataLenT = 0;
  _t->_segT._dataT    = nullptr;
  if (_dataLen > 0 && data) {
    _t->_segT._dataT = (byte *)malloc(_dataLen);
    if (_t->_segT._dataT) {
      //DEBUG_PRINTF_P(PSTR("--  Allocated duplicate data (%d) for %p: %p\n"), _dataLen, this, _t->_segT._dataT);
      memcpy(_t->_segT._dataT, data, _dataLen);
      _t->_segT._dataLenT = _dataLen;
    }
  }
  DEBUG_PRINTF_P(PSTR("-- pal: %d, bri: %d, C:[%08X,%08X,%08X], m: %d\n"),
    (int)_t->_palTid,
    (int)_t->_briT,
    _t->_segT._colorT[0],
    _t->_segT._colorT[1],
    _t->_segT._colorT[2],
    (int)_t->_modeT);
#else
  for (size_t i=0; i<NUM_COLORS; i++) _t->_colorT[i] = colors[i];
#endif
}

void Segment::stopTransition() {
  if (isInTransition()) {
    //DEBUG_PRINTF_P(PSTR("-- Stopping transition: %p\n"), this);
    #ifndef WLED_DISABLE_MODE_BLEND
    if (_t->_segT._dataT && _t->_segT._dataLenT > 0) {
      //DEBUG_PRINTF_P(PSTR("--  Released duplicate data (%d) for %p: %p\n"), _t->_segT._dataLenT, this, _t->_segT._dataT);
      free(_t->_segT._dataT);
      _t->_segT._dataT = nullptr;
      _t->_segT._dataLenT = 0;
    }
    #endif
    delete _t;
    _t = nullptr;
  }
  _transitionprogress = 0xFFFFU; // stop means stop - transition has ended
}

// transition progression between 0-65535
inline void Segment::updateTransitionProgress() {
  _transitionprogress = 0xFFFFU;
  if (isInTransition()) {
    unsigned diff = millis() - _t->_start;
    if (_t->_dur > 0 && diff < _t->_dur) _transitionprogress = diff * 0xFFFFU / _t->_dur;
  }
}

#ifndef WLED_DISABLE_MODE_BLEND
void Segment::swapSegenv(tmpsegd_t &tmpSeg) {
  //DEBUG_PRINTF_P(PSTR("--  Saving temp seg: %p->(%p) [%d->%p]\n"), this, &tmpSeg, _dataLen, data);
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
  if (isInTransition() && &tmpSeg != &(_t->_segT)) {
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
}

void Segment::restoreSegenv(const tmpsegd_t &tmpSeg) {
  //DEBUG_PRINTF_P(PSTR("--  Restoring temp seg: %p->(%p) [%d->%p]\n"), &tmpSeg, this, _dataLen, data);
  if (isInTransition() && &(_t->_segT) != &tmpSeg) {
    // update possibly changed variables to keep old effect running correctly
    _t->_segT._aux0T = aux0;
    _t->_segT._aux1T = aux1;
    _t->_segT._stepT = step;
    _t->_segT._callT = call;
    //if (_t->_segT._dataT != data) DEBUG_PRINTF_P(PSTR("---  data re-allocated: (%p) %p -> %p\n"), this, _t->_segT._dataT, data);
    _t->_segT._dataT = data;
    _t->_segT._dataLenT = _dataLen;
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
}
#endif

uint8_t Segment::currentBri(bool useCct) const {
  unsigned prog = isInTransition() ? progress() : 0xFFFFU;
  uint32_t curBri = useCct ? cct : (on ? opacity : 0);
  if (prog < 0xFFFFU) {
#ifndef WLED_DISABLE_MODE_BLEND
    uint8_t tmpBri = useCct ? _t->_cctT : (_t->_segT._optionsT & 0x0004 ? _t->_briT : 0);
    // _modeBlend==true -> old effect
    if (blendingStyle != BLEND_STYLE_FADE) return _modeBlend ? tmpBri : curBri; // not fade/blend transition, each effect uses its brightness
#else
    uint8_t tmpBri = useCct ? _t->_cctT : _t->_briT;
#endif
    curBri *=  prog;
    curBri += tmpBri * (0xFFFFU - prog);
    return curBri / 0xFFFFU;
  }
  return curBri;
}

uint8_t Segment::currentMode() const {
#ifndef WLED_DISABLE_MODE_BLEND
  unsigned prog = isInTransition() ? progress() : 0xFFFFU;
  if (prog == 0xFFFFU) return mode;
  if (blendingStyle != BLEND_STYLE_FADE) {
    // workaround for on/off transition to respect blending style
    uint8_t modeT = (bri != briT) &&  bri ? FX_MODE_STATIC : _t->_modeT;   // On/Off transition active (bri!=briT) and final bri>0 : old mode is STATIC
    uint8_t modeS = (bri != briT) && !bri ? FX_MODE_STATIC : mode;         // On/Off transition active (bri!=briT) and final bri==0 : new mode is STATIC
    return _modeBlend ? modeT : modeS;    // _modeBlend==true -> old effect
  }
  return _modeBlend ? _t->_modeT : mode;  // _modeBlend==true -> old effect
#else
  return mode;
#endif
}

uint32_t Segment::currentColor(uint8_t slot) const {
  if (slot >= NUM_COLORS) slot = 0;
  unsigned prog = progress();
  if (prog == 0xFFFFU) return colors[slot];
#ifndef WLED_DISABLE_MODE_BLEND
  if (blendingStyle != BLEND_STYLE_FADE) {
    // workaround for on/off transition to respect blending style
    uint32_t colT = (bri != briT) &&  bri ? BLACK : _t->_segT._colorT[slot];  // On/Off transition active (bri!=briT) and final bri>0 : old color is BLACK
    uint32_t colS = (bri != briT) && !bri ? BLACK : colors[slot];             // On/Off transition active (bri!=briT) and final bri==0 : new color is BLACK
    return _modeBlend ? colT : colS;    // _modeBlend==true -> old effect
  }
  return color_blend16(_t->_segT._colorT[slot], colors[slot], prog);
#else
  return color_blend16(_t->_colorT[slot], colors[slot], prog);
#endif
}

// pre-calculate drawing parameters for faster access (based on the idea from @softhack007 from MM fork)
void Segment::beginDraw() {
  _vWidth  = virtualWidth();
  _vHeight = virtualHeight();
  _vLength = virtualLength();
  _segBri  = currentBri();
  unsigned prog = isInTransition() ? progress() : 0xFFFFU;  // transition progress; 0xFFFFU = no transition active
  // adjust gamma for effects
  for (unsigned i = 0; i < NUM_COLORS; i++) {
    #ifndef WLED_DISABLE_MODE_BLEND
    uint32_t col = isInTransition() ? color_blend16(_t->_segT._colorT[i], colors[i], prog) : colors[i];
    #else
    uint32_t col = isInTransition() ? color_blend16(_t->_colorT[i], colors[i], prog) : colors[i];
    #endif
    _currentColors[i] = gamma32(col);
  }
  // load palette into _currentPalette
  loadPalette(_currentPalette, palette);
  if (prog < 0xFFFFU) {
#ifndef WLED_DISABLE_MODE_BLEND
    if (blendingStyle > BLEND_STYLE_FADE) {
      //if (_modeBlend) loadPalette(_currentPalette, _t->_palTid); // not fade/blend transition, each effect uses its palette
      if (_modeBlend) _currentPalette = _t->_palT; // not fade/blend transition, each effect uses its palette
    } else
#endif
    {
      // blend palettes
      // there are about 255 blend passes of 48 "blends" to completely blend two palettes (in _dur time)
      // minimum blend time is 100ms maximum is 65535ms
      unsigned noOfBlends = ((255U * prog) / 0xFFFFU) - _t->_prevPaletteBlends;
      for (unsigned i = 0; i < noOfBlends; i++, _t->_prevPaletteBlends++) nblendPaletteTowardPalette(_t->_palT, _currentPalette, 48);
      _currentPalette = _t->_palT; // copy transitioning/temporary palette
    }
  }
}

// loads palette of the old FX during transitions (used by particle system)
void Segment::loadOldPalette(void) {
  if(isInTransition())
    loadPalette(_currentPalette, _t->_palTid);
}

// relies on WS2812FX::service() to call it for each frame
void Segment::handleRandomPalette() {
  // is it time to generate a new palette?
  if ((uint16_t)(millis()/1000U) - _lastPaletteChange > randomPaletteChangeTime) {
    _newRandomPalette = useHarmonicRandomPalette ? generateHarmonicRandomPalette(_randomPalette) : generateRandomPalette();
    _lastPaletteChange = (uint16_t)(millis()/1000U);
    _lastPaletteBlend = (uint16_t)(millis())-512; // starts blending immediately
  }

  // assumes that 128 updates are sufficient to blend a palette, so shift by 7 (can be more, can be less)
  // in reality there need to be 255 blends to fully blend two entirely different palettes
  if ((uint16_t)millis() - _lastPaletteBlend < strip.getTransition() >> 7) return; // not yet time to fade, delay the update
  _lastPaletteBlend = (uint16_t)millis();
  nblendPaletteTowardPalette(_randomPalette, _newRandomPalette, 48);
}

// sets Segment geometry (length or width/height and grouping, spacing and offset as well as 2D mapping)
// strip must be suspended (strip.suspend()) before calling this function
// this function may call fill() to clear pixels if spacing or mapping changed (which requires setting _vWidth, _vHeight, _vLength or beginDraw())
void Segment::setGeometry(uint16_t i1, uint16_t i2, uint8_t grp, uint8_t spc, uint16_t ofs, uint16_t i1Y, uint16_t i2Y, uint8_t m12) {
  // return if neither bounds nor grouping have changed
  bool boundsUnchanged = (start == i1 && stop == i2);
  #ifndef WLED_DISABLE_2D
  if (Segment::maxHeight>1) boundsUnchanged &= (startY == i1Y && stopY == i2Y); // 2D
  #endif

  if (stop && (spc > 0 || m12 != map1D2D)) clear();
/*
  if (boundsUnchanged
      && (!grp || (grouping == grp && spacing == spc))
      && (ofs == UINT16_MAX || ofs == offset)
      && (m12 == map1D2D)
     ) return;
*/
  stateChanged = true; // send UDP/WS broadcast

  if (grp) { // prevent assignment of 0
    grouping = grp;
    spacing = spc;
  } else {
    grouping = 1;
    spacing = 0;
  }
  if (ofs < UINT16_MAX) offset = ofs;
  map1D2D  = constrain(m12, 0, 7);

  DEBUG_PRINTF_P(PSTR("Segment geometry: %d,%d -> %d,%d\n"), (int)i1, (int)i2, (int)i1Y, (int)i2Y);
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


Segment &Segment::setColor(uint8_t slot, uint32_t c) {
  if (slot >= NUM_COLORS || c == colors[slot]) return *this;
  if (!_isRGB && !_hasW) {
    if (slot == 0 && c == BLACK) return *this; // on/off segment cannot have primary color black
    if (slot == 1 && c != BLACK) return *this; // on/off segment cannot have secondary color non black
  }
  //DEBUG_PRINTF_P(PSTR("- Starting color transition: %d [0x%X]\n"), slot, c);
  startTransition(strip.getTransition()); // start transition prior to change
  colors[slot] = c;
  stateChanged = true; // send UDP/WS broadcast
  return *this;
}

Segment &Segment::setCCT(uint16_t k) {
  if (k > 255) { //kelvin value, convert to 0-255
    if (k < 1900)  k = 1900;
    if (k > 10091) k = 10091;
    k = (k - 1900) >> 5;
  }
  if (cct != k) {
    //DEBUG_PRINTF_P(PSTR("- Starting CCT transition: %d\n"), k);
    startTransition(strip.getTransition()); // start transition prior to change
    cct = k;
    stateChanged = true; // send UDP/WS broadcast
  }
  return *this;
}

Segment &Segment::setOpacity(uint8_t o) {
  if (opacity != o) {
    //DEBUG_PRINTF_P(PSTR("- Starting opacity transition: %d\n"), o);
    startTransition(strip.getTransition()); // start transition prior to change
    opacity = o;
    stateChanged = true; // send UDP/WS broadcast
  }
  return *this;
}

Segment &Segment::setOption(uint8_t n, bool val) {
  bool prevOn = on;
  if (n == SEG_OPTION_ON && val != prevOn) startTransition(strip.getTransition()); // start transition prior to change
  if (val) options |=   0x01 << n;
  else     options &= ~(0x01 << n);
  if (!(n == SEG_OPTION_SELECTED || n == SEG_OPTION_RESET)) stateChanged = true; // send UDP/WS broadcast
  return *this;
}

Segment &Segment::setMode(uint8_t fx, bool loadDefaults) {
  // skip reserved
  while (fx < strip.getModeCount() && strncmp_P("RSVD", strip.getModeData(fx), 4) == 0) fx++;
  if (fx >= strip.getModeCount()) fx = 0; // set solid mode
  // if we have a valid mode & is not reserved
  if (fx != mode) {
#ifndef WLED_DISABLE_MODE_BLEND
    //DEBUG_PRINTF_P(PSTR("- Starting effect transition: %d\n"), fx);
    startTransition(strip.getTransition()); // set effect transitions
#endif
    mode = fx;
    int sOpt;
    // load default values from effect string
    if (loadDefaults) {
      sOpt = extractModeDefaults(fx, "sx");  speed     = (sOpt >= 0) ? sOpt : DEFAULT_SPEED;
      sOpt = extractModeDefaults(fx, "ix");  intensity = (sOpt >= 0) ? sOpt : DEFAULT_INTENSITY;
      sOpt = extractModeDefaults(fx, "c1");  custom1   = (sOpt >= 0) ? sOpt : DEFAULT_C1;
      sOpt = extractModeDefaults(fx, "c2");  custom2   = (sOpt >= 0) ? sOpt : DEFAULT_C2;
      sOpt = extractModeDefaults(fx, "c3");  custom3   = (sOpt >= 0) ? sOpt : DEFAULT_C3;
      sOpt = extractModeDefaults(fx, "o1");  check1    = (sOpt >= 0) ? (bool)sOpt : false;
      sOpt = extractModeDefaults(fx, "o2");  check2    = (sOpt >= 0) ? (bool)sOpt : false;
      sOpt = extractModeDefaults(fx, "o3");  check3    = (sOpt >= 0) ? (bool)sOpt : false;
      sOpt = extractModeDefaults(fx, "m12"); if (sOpt >= 0) map1D2D   = constrain(sOpt, 0, 7); else map1D2D = M12_Pixels;  // reset mapping if not defined (2D FX may not work)
      sOpt = extractModeDefaults(fx, "si");  if (sOpt >= 0) soundSim  = constrain(sOpt, 0, 3);
      sOpt = extractModeDefaults(fx, "rev"); if (sOpt >= 0) reverse   = (bool)sOpt;
      sOpt = extractModeDefaults(fx, "mi");  if (sOpt >= 0) mirror    = (bool)sOpt; // NOTE: setting this option is a risky business
      sOpt = extractModeDefaults(fx, "rY");  if (sOpt >= 0) reverse_y = (bool)sOpt;
      sOpt = extractModeDefaults(fx, "mY");  if (sOpt >= 0) mirror_y  = (bool)sOpt; // NOTE: setting this option is a risky business
      sOpt = extractModeDefaults(fx, "pal"); if (sOpt >= 0) setPalette(sOpt); //else setPalette(0);
    }
    sOpt = extractModeDefaults(fx, "pal"); // always extract 'pal' to set _default_palette
    if(sOpt <= 0) sOpt = 6; // partycolors if zero or not set
    _default_palette = sOpt; // _deault_palette is loaded into pal0 in loadPalette() (if selected)
    markForReset();
    stateChanged = true; // send UDP/WS broadcast
  }
  return *this;
}

Segment &Segment::setPalette(uint8_t pal) {
  if (pal < 245 && pal > GRADIENT_PALETTE_COUNT+13) pal = 0; // built in palettes
  if (pal > 245 && (strip.customPalettes.size() == 0 || 255U-pal > strip.customPalettes.size()-1)) pal = 0; // custom palettes
  if (pal != palette) {
    //DEBUG_PRINTF_P(PSTR("- Starting palette transition: %d\n"), pal);
    startTransition(strip.getTransition());
    palette = pal;
    stateChanged = true; // send UDP/WS broadcast
  }
  return *this;
}

Segment &Segment::setName(const char *newName) {
  if (newName) {
    const int newLen = min(strlen(newName), (size_t)WLED_MAX_SEGNAME_LEN);
    if (newLen) {
      if (name) name = static_cast<char*>(realloc(name, newLen+1));
      else      name = static_cast<char*>(malloc(newLen+1));
      if (name) strlcpy(name, newName, newLen+1);
      name[newLen] = 0;
      return *this;
    }
  }
  return clearName();
}

// 2D matrix
unsigned Segment::virtualWidth() const {
  unsigned groupLen = groupLength();
  unsigned vWidth = ((transpose ? height() : width()) + groupLen - 1) / groupLen;
  if (mirror) vWidth = (vWidth + 1) /2;  // divide by 2 if mirror, leave at least a single LED
  return vWidth;
}

unsigned Segment::virtualHeight() const {
  unsigned groupLen = groupLength();
  unsigned vHeight = ((transpose ? width() : height()) + groupLen - 1) / groupLen;
  if (mirror_y) vHeight = (vHeight + 1) /2;  // divide by 2 if mirror, leave at least a single LED
  return vHeight;
}

// Constants for mapping mode "Pinwheel"
#ifndef WLED_DISABLE_2D
constexpr int Fixed_Scale = 16384; // fixpoint scaling factor (14bit for fraction)
// Pinwheel helper function: matrix dimensions to number of rays
static int getPinwheelLength(int vW, int vH) {
  // Returns multiple of 8, prevents over drawing 
  return (max(vW, vH) + 15) & ~7;
}
static void setPinwheelParameters(int i, int vW, int vH, int& startx, int& starty, int* cosVal, int* sinVal, bool getPixel = false) {
  int steps = getPinwheelLength(vW, vH);
  int baseAngle = ((0xFFFF + steps / 2) / steps);  // 360° / steps, in 16 bit scale round to nearest integer
  int rotate = 0;
  if (getPixel) rotate = baseAngle / 2; // rotate by half a ray width when reading pixel color
  for (int k = 0; k < 2; k++) // angular steps for two consecutive rays
  {
    int angle = (i + k) * baseAngle + rotate;
    cosVal[k] = (cos16_t(angle) * Fixed_Scale) >> 15; // step per pixel in fixed point, cos16 output is -0x7FFF to +0x7FFF
    sinVal[k] = (sin16_t(angle) * Fixed_Scale) >> 15; // using explicit bit shifts as dividing negative numbers is not equivalent (rounding error is acceptable)
  }
  startx = (vW * Fixed_Scale) / 2; // + cosVal[0] / 4; // starting position = center + 1/4 pixel (in fixed point)
  starty = (vH * Fixed_Scale) / 2; // + sinVal[0] / 4; 
}
#endif

// 1D strip
uint16_t Segment::virtualLength() const {
#ifndef WLED_DISABLE_2D
  if (is2D()) {
    unsigned vW = virtualWidth();
    unsigned vH = virtualHeight();
    unsigned vLen;
    switch (map1D2D) {
      case M12_pBar:
        vLen = vH;
        break;
      case M12_pCorner:
        vLen = max(vW,vH); // get the longest dimension
        break;
      case M12_pArc:
        vLen = sqrt32_bw(vH*vH + vW*vW); // use diagonal
        break;
      case M12_sPinwheel:
        vLen = getPinwheelLength(vW, vH);
        break;
      default:
        vLen = vW * vH; // use all pixels from segment
        break;
    }
    return vLen;
  }
#endif
  unsigned groupLen = groupLength(); // is always >= 1
  unsigned vLength = (length() + groupLen - 1) / groupLen;
  if (mirror) vLength = (vLength + 1) /2;  // divide by 2 if mirror, leave at least a single LED
  return vLength;
}

// pixel is clipped if it falls outside clipping range (_modeBlend==true) or is inside clipping range (_modeBlend==false)
// if clipping start > stop the clipping range is inverted
// _modeBlend==true  -> old effect during transition
// _modeBlend==false -> new effect during transition
bool IRAM_ATTR_YN Segment::isPixelClipped(int i) const {
#ifndef WLED_DISABLE_MODE_BLEND
  if (_clipStart != _clipStop && blendingStyle > BLEND_STYLE_FADE) {
    bool invert = _clipStart > _clipStop;  // ineverted start & stop
    int start = invert ? _clipStop : _clipStart;
    int stop  = invert ? _clipStart : _clipStop;
    if (blendingStyle == BLEND_STYLE_FAIRY_DUST) {
      unsigned len = stop - start;
      if (len < 2) return false;
      unsigned shuffled = hashInt(i) % len;
      unsigned pos = (shuffled * 0xFFFFU) / len;
      return (progress() <= pos) ^ _modeBlend;
    }
    const bool iInside = (i >= start && i < stop);
    //if (!invert &&  iInside) return _modeBlend;
    //if ( invert && !iInside) return _modeBlend;
    //return !_modeBlend;
    return !iInside ^ invert ^ _modeBlend; // thanks @willmmiles (https://github.com/wled-dev/WLED/pull/3877#discussion_r1554633876)
  }
#endif
  return false;
}

void IRAM_ATTR_YN Segment::setPixelColor(int i, uint32_t col) const
{
  if (!isActive() || i < 0) return; // not active or invalid index
#ifndef WLED_DISABLE_2D
  int vStrip = 0;
#endif
  int vL = vLength();
  // if the 1D effect is using virtual strips "i" will have virtual strip id stored in upper 16 bits
  // in such case "i" will be > virtualLength()
  if (i >= vL) {
    // check if this is a virtual strip
    #ifndef WLED_DISABLE_2D
    vStrip = i>>16; // hack to allow running on virtual strips (2D segment columns/rows)
    i &= 0xFFFF;    //truncate vstrip index
    if (i >= vL) return;  // if pixel would still fall out of segment just exit
    #else
    return;
    #endif
  }

#ifndef WLED_DISABLE_2D
  if (is2D()) {
    const int vW = vWidth();   // segment width in logical pixels (can be 0 if segment is inactive)
    const int vH = vHeight();  // segment height in logical pixels (is always >= 1)
    // pre-scale color for all pixels
    col = color_fade(col, _segBri);
    _colorScaled = true;
    switch (map1D2D) {
      case M12_Pixels:
        // use all available pixels as a long strip
        setPixelColorXY(i % vW, i / vW, col);
        break;
      case M12_pBar:
        // expand 1D effect vertically or have it play on virtual strips
        if (vStrip > 0) setPixelColorXY(vStrip - 1, vH - i - 1, col);
        else for (int x = 0; x < vW; x++) setPixelColorXY(x, vH - i - 1, col);
        break;
      case M12_pArc:
        // expand in circular fashion from center
        if (i == 0)
          setPixelColorXY(0, 0, col);
        else {
          float r = i;
          float step = HALF_PI / (2.8284f * r + 4); // we only need (PI/4)/(r/sqrt(2)+1) steps
          for (float rad = 0.0f; rad <= (HALF_PI/2)+step/2; rad += step) {
            int x = roundf(sin_t(rad) * r);
            int y = roundf(cos_t(rad) * r);
            // exploit symmetry
            setPixelColorXY(x, y, col);
            setPixelColorXY(y, x, col);
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
        case M12_sPinwheel: {
          // Uses Bresenham's algorithm to place coordinates of two lines in arrays then draws between them
          int startX, startY, cosVal[2], sinVal[2]; // in fixed point scale
          setPinwheelParameters(i, vW, vH, startX, startY, cosVal, sinVal);
  
          unsigned maxLineLength = max(vW, vH) + 2; // pixels drawn is always smaller than dx or dy, +1 pair for rounding errors
          uint16_t lineCoords[2][maxLineLength];    // uint16_t to save ram
          int lineLength[2] = {0};
  
          static int prevRays[2] = {INT_MAX, INT_MAX}; // previous two ray numbers
          int closestEdgeIdx = INT_MAX; // index of the closest edge pixel
  
          for (int lineNr = 0; lineNr < 2; lineNr++) {
            int x0 = startX; // x, y coordinates in fixed scale
            int y0 = startY;
            int x1 = (startX + (cosVal[lineNr] << 9)); // outside of grid
            int y1 = (startY + (sinVal[lineNr] << 9)); // outside of grid
            const int dx =  abs(x1-x0), sx = x0<x1 ? 1 : -1; // x distance & step
            const int dy = -abs(y1-y0), sy = y0<y1 ? 1 : -1; // y distance & step
            uint16_t* coordinates = lineCoords[lineNr]; // 1D access is faster
            int* length = &lineLength[lineNr];          // faster access
            x0 /= Fixed_Scale; // convert to pixel coordinates
            y0 /= Fixed_Scale;
  
            // Bresenham's algorithm
            int idx = 0;
            int err = dx + dy;
            while (true) {
              if (unsigned(x0) >= vW || unsigned(y0) >= vH) {
                closestEdgeIdx = min(closestEdgeIdx, idx-2);
                break; // stop if outside of grid (exploit unsigned int overflow)
              }
              coordinates[idx++] = x0;
              coordinates[idx++] = y0;
              (*length)++;
              // note: since endpoint is out of grid, no need to check if endpoint is reached
              int e2 = 2 * err;
              if (e2 >= dy) { err += dy; x0 += sx; }
              if (e2 <= dx) { err += dx; y0 += sy; }
            }
          }
  
          // fill up the shorter line with missing coordinates, so block filling works correctly and efficiently
          int diff = lineLength[0] - lineLength[1];
          int longLineIdx = (diff > 0) ? 0 : 1;
          int shortLineIdx = longLineIdx ? 0 : 1;
          if (diff != 0) {
            int idx = (lineLength[shortLineIdx] - 1) * 2; // last valid coordinate index
            int lastX = lineCoords[shortLineIdx][idx++];
            int lastY = lineCoords[shortLineIdx][idx++];
            bool keepX = lastX == 0 || lastX == vW - 1;
            for (int d = 0; d < abs(diff); d++) {
              lineCoords[shortLineIdx][idx] = keepX ? lastX :lineCoords[longLineIdx][idx];
              idx++;
              lineCoords[shortLineIdx][idx] =  keepX ? lineCoords[longLineIdx][idx] : lastY;
              idx++;
            }
          }
  
          // draw and block-fill the line coordinates. Note: block filling only efficient if angle between lines is small
          closestEdgeIdx += 2;
          int max_i = getPinwheelLength(vW, vH) - 1;
          bool drawFirst = !(prevRays[0] == i - 1 || (i == 0 && prevRays[0] == max_i)); // draw first line if previous ray was not adjacent including wrap
          bool drawLast  = !(prevRays[0] == i + 1 || (i == max_i && prevRays[0] == 0)); // same as above for last line
          for (int idx = 0; idx < lineLength[longLineIdx] * 2;) { //!! should be long line idx!
            int x1 = lineCoords[0][idx];
            int x2 = lineCoords[1][idx++];
            int y1 = lineCoords[0][idx];
            int y2 = lineCoords[1][idx++];
            int minX, maxX, minY, maxY;
            (x1 < x2) ? (minX = x1, maxX = x2) : (minX = x2, maxX = x1);
            (y1 < y2) ? (minY = y1, maxY = y2) : (minY = y2, maxY = y1);
  
            // fill the block between the two x,y points
            bool alwaysDraw = (drawFirst && drawLast) || // No adjacent rays, draw all pixels
                              (idx > closestEdgeIdx)  || // Edge pixels on uneven lines are always drawn
                              (i == 0 && idx == 2)    || // Center pixel special case
                              (i == prevRays[1]);        // Effect drawing twice in 1 frame
            for (int x = minX; x <= maxX; x++) {
              for (int y = minY; y <= maxY; y++) {
                bool onLine1 = x == x1 && y == y1;
                bool onLine2 = x == x2 && y == y2;
                if ((alwaysDraw) ||
                    (!onLine1 && (!onLine2 || drawLast))  || // Middle pixels and line2 if drawLast
                    (!onLine2 && (!onLine1 || drawFirst))    // Middle pixels and line1 if drawFirst
                  ) {
                  setPixelColorXY(x, y, col);
                }
              }
            }
          }
          prevRays[1] = prevRays[0];
          prevRays[0] = i;
          break;
        }
      }
      return;
  } else if (Segment::maxHeight != 1 && (width() == 1 || height() == 1)) {
    if (start < Segment::maxWidth*Segment::maxHeight) {
      // we have a vertical or horizontal 1D segment (WARNING: virtual...() may be transposed)
      int x = 0, y = 0;
      if (vHeight() > 1) y = i;
      if (vWidth()  > 1) x = i;
      setPixelColorXY(x, y, col);
      return;
    }
  }
#endif

#ifndef WLED_DISABLE_MODE_BLEND
  // if we blend using "push" style we need to "shift" new mode to left or right
  if (isInTransition() && !_modeBlend && (blendingStyle == BLEND_STYLE_PUSH_RIGHT || blendingStyle == BLEND_STYLE_PUSH_LEFT)) {
    unsigned prog = 0xFFFF - progress();
    unsigned dI = prog * vL / 0xFFFF;
    if (blendingStyle == BLEND_STYLE_PUSH_RIGHT) i -= dI;
    else                                         i += dI;
  }
#endif

  if (i >= vL || i < 0 || isPixelClipped(i)) return; // handle clipping on 1D

  unsigned len = length();
  // if color is unscaled
  if (!_colorScaled) col = color_fade(col, _segBri);

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
    unsigned indexSet = i + ((reverse) ? -j : j);
    if (indexSet >= start && indexSet < stop) {
      if (mirror) { //set the corresponding mirrored pixel
        unsigned indexMir = stop - indexSet + start - 1;
        indexMir += offset; // offset/phase
        if (indexMir >= stop) indexMir -= len; // wrap
#ifndef WLED_DISABLE_MODE_BLEND
        // _modeBlend==true -> old effect
        if (_modeBlend && blendingStyle == BLEND_STYLE_FADE) tmpCol = color_blend16(strip.getPixelColor(indexMir), col, 0xFFFFU - progress());
#endif
        strip.setPixelColor(indexMir, tmpCol);
      }
      indexSet += offset; // offset/phase
      if (indexSet >= stop) indexSet -= len; // wrap
#ifndef WLED_DISABLE_MODE_BLEND
        // _modeBlend==true -> old effect
      if (_modeBlend && blendingStyle == BLEND_STYLE_FADE) tmpCol = color_blend16(strip.getPixelColor(indexSet), col, 0xFFFFU - progress());
#endif
      strip.setPixelColor(indexSet, tmpCol);
    }
  }
}

#ifdef WLED_USE_AA_PIXELS
// anti-aliased normalized version of setPixelColor()
void Segment::setPixelColor(float i, uint32_t col, bool aa) const
{
  if (!isActive()) return; // not active
  int vStrip = int(i/10.0f); // hack to allow running on virtual strips (2D segment columns/rows)
  i -= int(i);

  if (i<0.0f || i>1.0f) return; // not normalized

  float fC = i * (virtualLength()-1);
  if (aa) {
    unsigned iL = roundf(fC-0.49f);
    unsigned iR = roundf(fC+0.49f);
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
    setPixelColor(int(roundf(fC)) | (vStrip<<16), col);
  }
}
#endif

uint32_t IRAM_ATTR_YN Segment::getPixelColor(int i) const
{
  if (!isActive()) return 0; // not active

  int vL = vLength();
  if (i >= vL || i < 0) return 0;

#ifndef WLED_DISABLE_2D
  if (is2D()) {
    const int vW = vWidth();   // segment width in logical pixels (can be 0 if segment is inactive)
    const int vH = vHeight();  // segment height in logical pixels (is always >= 1)
    switch (map1D2D) {
      case M12_Pixels:
        return getPixelColorXY(i % vW, i / vW);
        break;
      case M12_pBar: {
        int vStrip = i>>16; // virtual strips are only relevant in Bar expansion mode
        if (vStrip > 0) return getPixelColorXY(vStrip - 1, vH - (i & 0xFFFF) -1);
        else            return getPixelColorXY(0, vH - i -1);
        break; }
      case M12_pArc:
        if (i >= vW && i >= vH) {
          unsigned vI = sqrt32_bw(i*i/2);
          return getPixelColorXY(vI,vI); // use diagonal
        }
      case M12_pCorner:
        // use longest dimension
        return vW>vH ? getPixelColorXY(i, 0) : getPixelColorXY(0, i);
        break;
      case M12_sPinwheel:
        // not 100% accurate, returns pixel at outer edge
        int x, y, cosVal[2], sinVal[2];
        setPinwheelParameters(i, vW, vH, x, y, cosVal, sinVal, true);
        int maxX = (vW-1) * Fixed_Scale;
        int maxY = (vH-1) * Fixed_Scale;
        // trace ray from center until we hit any edge - to avoid rounding problems, we use fixed point coordinates
        while ((x < maxX)  && (y < maxY) && (x > Fixed_Scale) && (y > Fixed_Scale)) {
          x += cosVal[0]; // advance to next position
          y += sinVal[0];
        }
        x /= Fixed_Scale;
        y /= Fixed_Scale;
        return getPixelColorXY(x, y);
        break;
      }
    return 0;
  }
#endif

#ifndef WLED_DISABLE_MODE_BLEND
  if (isInTransition() && !_modeBlend && (blendingStyle == BLEND_STYLE_PUSH_RIGHT || blendingStyle == BLEND_STYLE_PUSH_LEFT)) {
    unsigned prog = 0xFFFF - progress();
    unsigned dI = prog * vL / 0xFFFF;
    if (blendingStyle == BLEND_STYLE_PUSH_RIGHT) i -= dI;
    else                                         i += dI;
  }
#endif

  if (i >= vL || i < 0 || isPixelClipped(i)) return 0; // handle clipping on 1D

  if (reverse) i = vL - i - 1;
  i *= groupLength();
  i += start;
  // offset/phase
  i += offset;
  if (i >= stop) i -= length();
  return strip.getPixelColor(i);
}

uint8_t Segment::differs(const Segment& b) const {
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

  //bit pattern: (msb first)
  // set:2, sound:2, mapping:3, transposed, mirrorY, reverseY, [reset,] paused, mirrored, on, reverse, [selected]
  if ((options & 0b1111111111011110U) != (b.options & 0b1111111111011110U)) d |= SEG_DIFFERS_OPT;
  if ((options & 0x0001U) != (b.options & 0x0001U))                         d |= SEG_DIFFERS_SEL;
  for (unsigned i = 0; i < NUM_COLORS; i++) if (colors[i] != b.colors[i])   d |= SEG_DIFFERS_COL;

  return d;
}

void Segment::refreshLightCapabilities() {
  unsigned capabilities = 0;
  unsigned segStartIdx = 0xFFFFU;
  unsigned segStopIdx  = 0;

  if (!isActive()) {
    _capabilities = 0;
    return;
  }

  if (start < Segment::maxWidth * Segment::maxHeight) {
    // we are withing 2D matrix (includes 1D segments)
    for (int y = startY; y < stopY; y++) for (int x = start; x < stop; x++) {
      unsigned index = strip.getMappedPixelIndex(x + Segment::maxWidth * y); // convert logical address to physical
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

  for (unsigned b = 0; b < BusManager::getNumBusses(); b++) {
    const Bus *bus = BusManager::getBus(b);
    if (!bus || !bus->isOk()) break;
    if (bus->getStart() >= segStopIdx || bus->getStart() + bus->getLength() <= segStartIdx) continue;
    if (bus->hasRGB() || (strip.cctFromRgb && bus->hasCCT())) capabilities |= SEG_CAPABILITY_RGB;
    if (!strip.cctFromRgb && bus->hasCCT())                   capabilities |= SEG_CAPABILITY_CCT;
    if (strip.correctWB && (bus->hasRGB() || bus->hasCCT()))  capabilities |= SEG_CAPABILITY_CCT; //white balance correction (CCT slider)
    if (bus->hasWhite()) {
      unsigned aWM = Bus::getGlobalAWMode() == AW_GLOBAL_DISABLED ? bus->getAutoWhiteMode() : Bus::getGlobalAWMode();
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
 * Fills segment with black
 */
void Segment::clear() {
  if (!isActive()) return; // not active
    unsigned oldVW = _vWidth;
    unsigned oldVH = _vHeight;
    unsigned oldVL = _vLength;
    unsigned oldSB = _segBri;
    _vWidth  = virtualWidth();
    _vHeight = virtualHeight();
    _vLength = virtualLength();
    _segBri  = currentBri();
    fill(BLACK);
    _vWidth  = oldVW;
    _vHeight = oldVH;
    _vLength = oldVL;
    _segBri  = oldSB;
}

/*
 * Fills segment with color
 */
void Segment::fill(uint32_t c) {
  if (!isActive()) return; // not active
  const int cols = is2D() ? vWidth() : vLength();
  const int rows = vHeight(); // will be 1 for 1D
  // pre-scale color for all pixels
  c = color_fade(c, _segBri);
  _colorScaled = true;
  for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) {
    if (is2D()) setPixelColorXY(x, y, c);
    else        setPixelColor(x, c);
  }
  _colorScaled = false;
}

/*
 * fade out function, higher rate = quicker fade
 * fading is highly dependant on frame rate (higher frame rates, faster fading)
 * each frame will fade at max 9% or as little as 0.8%
 */
void Segment::fade_out(uint8_t rate) {
  if (!isActive()) return; // not active
  const int cols = is2D() ? vWidth() : vLength();
  const int rows = vHeight(); // will be 1 for 1D

  rate = (256-rate) >> 1;
  const int mappedRate = 256 / (rate + 1);

  for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) {
    uint32_t color = is2D() ? getPixelColorXY(x, y) : getPixelColor(x);
    if (color == colors[1]) continue; // already at target color
    for (int i = 0; i < 32; i += 8) {
      uint8_t c2 = (colors[1]>>i);  // get background channel
      uint8_t c1 = (color>>i);      // get foreground channel
      // we can't use bitshift since we are using int
      int delta = (c2 - c1) * mappedRate / 256;
      // if fade isn't complete, make sure delta is at least 1 (fixes rounding issues)
      if (delta == 0) delta += (c2 == c1) ? 0 : (c2 > c1) ? 1 : -1;
      // stuff new value back into color
      color &= ~(0xFF<<i);
      color |= ((c1 + delta) & 0xFF) << i;
    }
    if (is2D()) setPixelColorXY(x, y, color);
    else        setPixelColor(x, color);
  }
}

// fades all pixels to secondary color
void Segment::fadeToSecondaryBy(uint8_t fadeBy) {
  if (!isActive() || fadeBy == 0) return;   // optimization - no scaling to apply
  const int cols = is2D() ? vWidth() : vLength();
  const int rows = vHeight(); // will be 1 for 1D

  for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) {
    if (is2D()) setPixelColorXY(x, y, color_blend(getPixelColorXY(x,y), colors[1], fadeBy));
    else        setPixelColor(x, color_blend(getPixelColor(x), colors[1], fadeBy));
  }
}

// fades all pixels to black using nscale8()
void Segment::fadeToBlackBy(uint8_t fadeBy) {
  if (!isActive() || fadeBy == 0) return;   // optimization - no scaling to apply
  const int cols = is2D() ? vWidth() : vLength();
  const int rows = vHeight(); // will be 1 for 1D

  for (int y = 0; y < rows; y++) for (int x = 0; x < cols; x++) {
    if (is2D()) setPixelColorXY(x, y, color_fade(getPixelColorXY(x,y), 255-fadeBy));
    else        setPixelColor(x, color_fade(getPixelColor(x), 255-fadeBy));
  }
}

/*
 * blurs segment content, source: FastLED colorutils.cpp
 * Note: for blur_amount > 215 this function does not work properly (creates alternating pattern)
 */
void Segment::blur(uint8_t blur_amount, bool smear) {
  if (!isActive() || blur_amount == 0) return; // optimization: 0 means "don't blur"
#ifndef WLED_DISABLE_2D
  if (is2D()) {
    // compatibility with 2D
    blur2D(blur_amount, blur_amount, smear); // symmetrical 2D blur
    //box_blur(map(blur_amount,1,255,1,3), smear);
    return;
  }
#endif
  uint8_t keep = smear ? 255 : 255 - blur_amount;
  uint8_t seep = blur_amount >> 1;
  unsigned vlength = vLength();
  uint32_t carryover = BLACK;
  uint32_t lastnew;       // not necessary to initialize lastnew and last, as both will be initialized by the first loop iteration
  uint32_t last;
  uint32_t curnew = BLACK;
  for (unsigned i = 0; i < vlength; i++) {
    uint32_t cur = getPixelColor(i);
    uint32_t part = color_fade(cur, seep);
    curnew = color_fade(cur, keep);
    if (i > 0) {
      if (carryover) curnew = color_add(curnew, carryover);
      uint32_t prev = color_add(lastnew, part);
      // optimization: only set pixel if color has changed
      if (last != prev) setPixelColor(i - 1, prev);
    } else setPixelColor(i, curnew); // first pixel
    lastnew = curnew;
    last = cur; // save original value for comparison on next iteration
    carryover = part;
  }
  setPixelColor(vlength - 1, curnew);
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
uint32_t Segment::color_wheel(uint8_t pos) const {
  if (palette) return color_from_palette(pos, false, true, 0); // perhaps "strip.paletteBlend < 2" should be better instead of "true"
  uint8_t w = W(getCurrentColor(0));
  pos = 255 - pos;
  if (pos < 85) {
    return RGBW32((255 - pos * 3), 0, (pos * 3), w);
  } else if (pos < 170) {
    pos -= 85;
    return RGBW32(0, (pos * 3), (255 - pos * 3), w);
  } else {
    pos -= 170;
    return RGBW32((pos * 3), (255 - pos * 3), 0, w);
  }
}

/*
 * Gets a single color from the currently selected palette.
 * @param i Palette Index (if mapping is true, the full palette will be _virtualSegmentLength long, if false, 255). Will wrap around automatically.
 * @param mapping if true, LED position in segment is considered for color
 * @param moving FastLED palettes will usually wrap back to the start smoothly. Set to true if effect has moving palette and you want wrap.
 * @param mcol If the default palette 0 is selected, return the standard color 0, 1 or 2 instead. If >2, Party palette is used instead
 * @param pbri Value to scale the brightness of the returned color by. Default is 255. (no scaling)
 * @returns Single color from palette
 */
uint32_t Segment::color_from_palette(uint16_t i, bool mapping, bool moving, uint8_t mcol, uint8_t pbri) const {
  uint32_t color = getCurrentColor(mcol < NUM_COLORS ? mcol : 0);
  // default palette or no RGB support on segment
  if ((palette == 0 && mcol < NUM_COLORS) || !_isRGB) {
    return color_fade(color, pbri, true);
  }

  const int vL = vLength();
  unsigned paletteIndex = i;
  if (mapping && vL > 1) paletteIndex = (i*255)/(vL -1);
  // paletteBlend: 0 - wrap when moving, 1 - always wrap, 2 - never wrap, 3 - none (undefined/no interpolation of palette entries)
  // ColorFromPalette interpolations are: NOBLEND, LINEARBLEND, LINEARBLEND_NOWRAP
  TBlendType blend = NOBLEND;
  switch (strip.paletteBlend) { // NOTE: paletteBlend should be global
    case 0: blend = moving ? LINEARBLEND : LINEARBLEND_NOWRAP; break;
    case 1: blend = LINEARBLEND; break;
    case 2: blend = LINEARBLEND_NOWRAP; break;
  }
  CRGBW palcol = ColorFromPalette(_currentPalette, paletteIndex, pbri, blend);
  palcol.w = W(color);

  return palcol.color32;
}


///////////////////////////////////////////////////////////////////////////////
// WS2812FX class implementation
///////////////////////////////////////////////////////////////////////////////

//do not call this method from system context (network callback)
void WS2812FX::finalizeInit() {
  //reset segment runtimes
  restartRuntime();

  // for the lack of better place enumerate ledmaps here
  // if we do it in json.cpp (serializeInfo()) we are getting flashes on LEDs
  // unfortunately this means we do not get updates after uploads
  // the other option is saving UI settings which will cause enumeration
  enumerateLedmaps();

  _hasWhiteChannel = _isOffRefreshRequired = false;

  unsigned digitalCount = 0;
  #if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
  // determine if it is sensible to use parallel I2S outputs on ESP32 (i.e. more than 5 outputs = 1 I2S + 4 RMT)
  unsigned maxLedsOnBus = 0;
  for (const auto &bus : busConfigs) {
    if (Bus::isDigital(bus.type) && !Bus::is2Pin(bus.type)) {
      digitalCount++;
      if (bus.count > maxLedsOnBus) maxLedsOnBus = bus.count;
    }
  }
  DEBUG_PRINTF_P(PSTR("Maximum LEDs on a bus: %u\nDigital buses: %u\n"), maxLedsOnBus, digitalCount);
  // we may remove 300 LEDs per bus limit when NeoPixelBus is updated beyond 2.9.0
  if (maxLedsOnBus <= 300 && useParallelI2S) BusManager::useParallelOutput(); // must call before creating buses
  else useParallelI2S = false; // enforce single I2S
  #endif

  // create buses/outputs
  unsigned mem = 0;
  digitalCount = 0;
  for (const auto &bus : busConfigs) {
    mem += bus.memUsage(Bus::isDigital(bus.type) && !Bus::is2Pin(bus.type) ? digitalCount++ : 0); // includes global buffer
    if (mem <= MAX_LED_MEMORY) {
      if (BusManager::add(bus) == -1) break;
    } else DEBUG_PRINTF_P(PSTR("Out of LED memory! Bus %d (%d) #%u not created."), (int)bus.type, (int)bus.count, digitalCount);
  }
  busConfigs.clear();
  busConfigs.shrink_to_fit();

  //if busses failed to load, add default (fresh install, FS issue, ...)
  if (BusManager::getNumBusses() == 0) {
    DEBUG_PRINTLN(F("No busses, init default"));
    constexpr unsigned defDataTypes[] = {LED_TYPES};
    constexpr unsigned defDataPins[] = {DATA_PINS};
    constexpr unsigned defCounts[] = {PIXEL_COUNTS};
    constexpr unsigned defNumTypes = ((sizeof defDataTypes) / (sizeof defDataTypes[0]));
    constexpr unsigned defNumPins = ((sizeof defDataPins) / (sizeof defDataPins[0]));
    constexpr unsigned defNumCounts = ((sizeof defCounts) / (sizeof defCounts[0]));

    static_assert(validatePinsAndTypes(defDataTypes, defNumTypes, defNumPins),
                  "The default pin list defined in DATA_PINS does not match the pin requirements for the default buses defined in LED_TYPES");

    unsigned prevLen = 0;
    unsigned pinsIndex = 0;
    digitalCount = 0;
    for (unsigned i = 0; i < WLED_MAX_BUSSES+WLED_MIN_VIRTUAL_BUSSES; i++) {
      uint8_t defPin[OUTPUT_MAX_PINS];
      // if we have less types than requested outputs and they do not align, use last known type to set current type
      unsigned dataType = defDataTypes[(i < defNumTypes) ? i : defNumTypes -1];
      unsigned busPins = Bus::getNumberOfPins(dataType);

      // if we need more pins than available all outputs have been configured
      if (pinsIndex + busPins > defNumPins) break;

      // Assign all pins first so we can check for conflicts on this bus
      for (unsigned j = 0; j < busPins && j < OUTPUT_MAX_PINS; j++) defPin[j] = defDataPins[pinsIndex + j];

      for (unsigned j = 0; j < busPins && j < OUTPUT_MAX_PINS; j++) {
        bool validPin = true;
        // When booting without config (1st boot) we need to make sure GPIOs defined for LED output don't clash with hardware
        // i.e. DEBUG (GPIO1), DMX (2), SPI RAM/FLASH (16&17 on ESP32-WROVER/PICO), read/only pins, etc.
        // Pin should not be already allocated, read/only or defined for current bus
        while (PinManager::isPinAllocated(defPin[j]) || !PinManager::isPinOk(defPin[j],true)) {
          if (validPin) {
            DEBUG_PRINTLN(F("Some of the provided pins cannot be used to configure this LED output."));
            defPin[j] = 1; // start with GPIO1 and work upwards
            validPin = false;
          } else if (defPin[j] < WLED_NUM_PINS) {
            defPin[j]++;
          } else {
            DEBUG_PRINTLN(F("No available pins left! Can't configure output."));
            return;
          }
          // is the newly assigned pin already defined or used previously?
          // try next in line until there are no clashes or we run out of pins
          bool clash;
          do {
            clash = false;
            // check for conflicts on current bus
            for (const auto &pin : defPin) {
              if (&pin != &defPin[j] && pin == defPin[j]) {
                clash = true;
                break;
              }
            }
            // We already have a clash on current bus, no point checking next buses
            if (!clash) {
              // check for conflicts in defined pins
              for (const auto &pin : defDataPins) {
                if (pin == defPin[j]) {
                  clash = true;
                  break;
                }
              }
            }
            if (clash) defPin[j]++;
            if (defPin[j] >= WLED_NUM_PINS) break;
          } while (clash);
        }
      }
      pinsIndex += busPins;

      unsigned start = prevLen;
      // if we have less counts than pins and they do not align, use last known count to set current count
      unsigned count = defCounts[(i < defNumCounts) ? i : defNumCounts -1];
      // analog always has length 1
      if (Bus::isPWM(dataType) || Bus::isOnOff(dataType)) count = 1;
      prevLen += count;
      BusConfig defCfg = BusConfig(dataType, defPin, start, count, DEFAULT_LED_COLOR_ORDER, false, 0, RGBW_MODE_MANUAL_ONLY, 0, useGlobalLedBuffer);
      mem += defCfg.memUsage(Bus::isDigital(dataType) && !Bus::is2Pin(dataType) ? digitalCount++ : 0);
      if (BusManager::add(defCfg) == -1) break;
    }
  }
  DEBUG_PRINTF_P(PSTR("LED buffer size: %uB/%uB\n"), mem, BusManager::memUsage());

  _length = 0;
  for (int i=0; i<BusManager::getNumBusses(); i++) {
    Bus *bus = BusManager::getBus(i);
    if (!bus || !bus->isOk() || bus->getStart() + bus->getLength() > MAX_LEDS) break;
    //RGBW mode is enabled if at least one of the strips is RGBW
    _hasWhiteChannel |= bus->hasWhite();
    //refresh is required to remain off if at least one of the strips requires the refresh.
    _isOffRefreshRequired |= bus->isOffRefreshRequired() && !bus->isPWM(); // use refresh bit for phase shift with analog
    unsigned busEnd = bus->getStart() + bus->getLength();
    if (busEnd > _length) _length = busEnd;

    // This must be done after all buses have been created, as some kinds (parallel I2S) interact
    bus->begin();
    bus->setBrightness(bri);
  }
  DEBUG_PRINTF_P(PSTR("Heap after buses: %d\n"), ESP.getFreeHeap());

  Segment::maxWidth  = _length;
  Segment::maxHeight = 1;

  //segments are created in makeAutoSegments();
  DEBUG_PRINTLN(F("Loading custom palettes"));
  loadCustomPalettes(); // (re)load all custom palettes
  DEBUG_PRINTLN(F("Loading custom ledmaps"));
  deserializeMap();     // (re)load default ledmap (will also setUpMatrix() if ledmap does not exist)
}

void WS2812FX::service() {
  unsigned long nowUp = millis(); // Be aware, millis() rolls over every 49 days
  now = nowUp + timebase;
  if (_suspend) return;
  unsigned long elapsed = nowUp - _lastServiceShow;

  if (elapsed <= MIN_FRAME_DELAY) return;                                        // keep wifi alive - no matter if triggered or unlimited
  if ( !_triggered && (_targetFps != FPS_UNLIMITED)) {                           // unlimited mode = no frametime
    if (elapsed < _frametime) return;                                            // too early for service
  }

  bool doShow = false;

  _isServicing = true;
  _segment_index = 0;

  for (segment &seg : _segments) {
    if (_suspend) break; // immediately stop processing segments if suspend requested during service()

    // process transition (mode changes in the middle of transition)
    seg.handleTransition();
    // reset the segment runtime data if needed
    seg.resetIfRequired();

    if (!seg.isActive()) continue;

    // last condition ensures all solid segments are updated at the same time
    if (nowUp >= seg.next_time || _triggered || (doShow && seg.mode == FX_MODE_STATIC))
    {
      doShow = true;
      unsigned frameDelay = FRAMETIME;

      if (!seg.freeze) { //only run effect function if not frozen
        int oldCCT = BusManager::getSegmentCCT(); // store original CCT value (actually it is not Segment based)
        // when correctWB is true we need to correct/adjust RGB value according to desired CCT value, but it will also affect actual WW/CW ratio
        // when cctFromRgb is true we implicitly calculate WW and CW from RGB values
        if (cctFromRgb) BusManager::setSegmentCCT(-1);
        else            BusManager::setSegmentCCT(seg.currentBri(true), correctWB);
        // Effect blending
        // When two effects are being blended, each may have different segment data, this
        // data needs to be saved first and then restored before running previous mode.
        // The blending will largely depend on the effect behaviour since actual output (LEDs) may be
        // overwritten by later effect. To enable seamless blending for every effect, additional LED buffer
        // would need to be allocated for each effect and then blended together for each pixel.
        seg.beginDraw();                      // set up parameters for get/setPixelColor()
#ifndef WLED_DISABLE_MODE_BLEND
        Segment::setClippingRect(0, 0); // disable clipping (just in case)
        if (seg.isInTransition()) {
          // a hack to determine if effect has changed
          uint8_t  m = seg.currentMode();
          Segment::modeBlend(true);           // set semaphore
          bool     sameEffect = (m == seg.currentMode());
          Segment::modeBlend(false);          // clear semaphore
          // set clipping rectangle
          // new mode is run inside clipping area and old mode outside clipping area
          unsigned p = seg.progress();
          unsigned w = seg.is2D() ? Segment::vWidth() : Segment::vLength();
          unsigned h = Segment::vHeight();
          unsigned dw = p * w / 0xFFFFU + 1;
          unsigned dh = p * h / 0xFFFFU + 1;
          unsigned orgBS = blendingStyle;
          if (w*h == 1) blendingStyle = BLEND_STYLE_FADE; // disable style for single pixel segments (use fade instead)
          else if (sameEffect && (blendingStyle & BLEND_STYLE_PUSH_MASK)) {
            // when effect stays the same push will look awful, change it to swipe
            switch (blendingStyle) {
              case BLEND_STYLE_PUSH_BR:
              case BLEND_STYLE_PUSH_TR:
              case BLEND_STYLE_PUSH_RIGHT: blendingStyle = BLEND_STYLE_SWIPE_RIGHT; break;
              case BLEND_STYLE_PUSH_BL:
              case BLEND_STYLE_PUSH_TL:
              case BLEND_STYLE_PUSH_LEFT:  blendingStyle = BLEND_STYLE_SWIPE_LEFT;  break;
              case BLEND_STYLE_PUSH_DOWN:  blendingStyle = BLEND_STYLE_SWIPE_DOWN;  break;
              case BLEND_STYLE_PUSH_UP:    blendingStyle = BLEND_STYLE_SWIPE_UP;    break;
            }
          }
          switch (blendingStyle) {
            case BLEND_STYLE_FAIRY_DUST:  // fairy dust (must set entire segment, see isPixelXYClipped())
              Segment::setClippingRect(0, w, 0, h);
              break;
            case BLEND_STYLE_SWIPE_RIGHT: // left-to-right
            case BLEND_STYLE_PUSH_RIGHT:  // left-to-right
              Segment::setClippingRect(0, dw, 0, h);
              break;
            case BLEND_STYLE_SWIPE_LEFT:  // right-to-left
            case BLEND_STYLE_PUSH_LEFT:   // right-to-left
              Segment::setClippingRect(w - dw, w, 0, h);
              break;
            case BLEND_STYLE_PINCH_OUT:   // corners
              Segment::setClippingRect((w + dw)/2, (w - dw)/2, (h + dh)/2, (h - dh)/2); // inverted!!
              break;
            case BLEND_STYLE_INSIDE_OUT:  // outward
              Segment::setClippingRect((w - dw)/2, (w + dw)/2, (h - dh)/2, (h + dh)/2);
              break;
            case BLEND_STYLE_SWIPE_DOWN:  // top-to-bottom (2D)
            case BLEND_STYLE_PUSH_DOWN:   // top-to-bottom (2D)
              Segment::setClippingRect(0, w, 0, dh);
              break;
            case BLEND_STYLE_SWIPE_UP:    // bottom-to-top (2D)
            case BLEND_STYLE_PUSH_UP:     // bottom-to-top (2D)
              Segment::setClippingRect(0, w, h - dh, h);
              break;
            case BLEND_STYLE_OPEN_H:      // horizontal-outward (2D) same look as INSIDE_OUT on 1D
              Segment::setClippingRect((w - dw)/2, (w + dw)/2, 0, h);
              break;
            case BLEND_STYLE_OPEN_V:      // vertical-outward (2D)
              Segment::setClippingRect(0, w, (h - dh)/2, (h + dh)/2);
              break;
            case BLEND_STYLE_PUSH_TL:     // TL-to-BR (2D)
              Segment::setClippingRect(0, dw, 0, dh);
              break;
            case BLEND_STYLE_PUSH_TR:     // TR-to-BL (2D)
              Segment::setClippingRect(w - dw, w, 0, dh);
              break;
            case BLEND_STYLE_PUSH_BR:     // BR-to-TL (2D)
              Segment::setClippingRect(w - dw, w, h - dh, h);
              break;
            case BLEND_STYLE_PUSH_BL:     // BL-to-TR (2D)
              Segment::setClippingRect(0, dw, h - dh, h);
              break;
          }
          frameDelay = (*_mode[m])();         // run new/current mode
          // now run old/previous mode
          Segment::tmpsegd_t _tmpSegData;
          Segment::modeBlend(true);           // set semaphore
          seg.swapSegenv(_tmpSegData);        // temporarily store new mode state (and swap it with transitional state)
          seg.beginDraw();                    // set up parameters for get/setPixelColor()
          frameDelay = min(frameDelay, (unsigned)(*_mode[seg.currentMode()])());  // run old mode
          seg.call++;                         // increment old mode run counter
          seg.restoreSegenv(_tmpSegData);     // restore mode state (will also update transitional state)
          Segment::modeBlend(false);          // unset semaphore
          blendingStyle = orgBS;              // restore blending style if it was modified for single pixel segment
        } else
#endif
        frameDelay = (*_mode[seg.mode])();         // run effect mode (not in transition)
        seg.call++;
        if (seg.isInTransition() && frameDelay > FRAMETIME) frameDelay = FRAMETIME; // force faster updates during transition
        BusManager::setSegmentCCT(oldCCT); // restore old CCT for ABL adjustments
      }

      seg.next_time = nowUp + frameDelay;
    }
    _segment_index++;
  }
  Segment::setClippingRect(0, 0);             // disable clipping for overlays
  _isServicing = false;
  _triggered = false;

  #ifdef WLED_DEBUG
  if ((_targetFps != FPS_UNLIMITED) && (millis() - nowUp > _frametime)) DEBUG_PRINTF_P(PSTR("Slow effects %u/%d.\n"), (unsigned)(millis()-nowUp), (int)_frametime);
  #endif
  if (doShow) {
    yield();
    Segment::handleRandomPalette(); // slowly transition random palette; move it into for loop when each segment has individual random palette
    _lastServiceShow = nowUp; // update timestamp, for precise FPS control
    if (!_suspend) show();
  }
  #ifdef WLED_DEBUG
  if ((_targetFps != FPS_UNLIMITED) && (millis() - nowUp > _frametime)) DEBUG_PRINTF_P(PSTR("Slow strip %u/%d.\n"), (unsigned)(millis()-nowUp), (int)_frametime);
  #endif
}

void IRAM_ATTR WS2812FX::setPixelColor(unsigned i, uint32_t col) const {
  i = getMappedPixelIndex(i);
  if (i >= _length) return;
  BusManager::setPixelColor(i, col);
}

uint32_t IRAM_ATTR WS2812FX::getPixelColor(unsigned i) const {
  i = getMappedPixelIndex(i);
  if (i >= _length) return 0;
  return BusManager::getPixelColor(i);
}

void WS2812FX::show() {
  // avoid race condition, capture _callback value
  show_callback callback = _callback;
  if (callback) callback();
  unsigned long showNow = millis();

  // some buses send asynchronously and this method will return before
  // all of the data has been sent.
  // See https://github.com/Makuna/NeoPixelBus/wiki/ESP32-NeoMethods#neoesp32rmt-methods
  BusManager::show();

  size_t diff = showNow - _lastShow;

  if (diff > 0) { // skip calculation if no time has passed
    size_t fpsCurr = (1000 << FPS_CALC_SHIFT) / diff; // fixed point math
    _cumulativeFps = (FPS_CALC_AVG * _cumulativeFps + fpsCurr + FPS_CALC_AVG / 2) / (FPS_CALC_AVG + 1);   // "+FPS_CALC_AVG/2" for proper rounding
    _lastShow = showNow;
  }
}

void WS2812FX::setTargetFps(unsigned fps) {
  if (fps <= 250) _targetFps = fps;
  if (_targetFps > 0) _frametime = 1000 / _targetFps;
  else _frametime = MIN_FRAME_DELAY;     // unlimited mode
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
  BusManager::setBrightness(b);
  if (!direct) {
    unsigned long t = millis();
    if (_segments[0].next_time > t + 22 && t - _lastShow > MIN_FRAME_DELAY) trigger(); //apply brightness change immediately if no refresh soon
  }
}

uint8_t WS2812FX::getActiveSegsLightCapabilities(bool selectedOnly) const {
  uint8_t totalLC = 0;
  for (const segment &seg : _segments) {
    if (seg.isActive() && (!selectedOnly || seg.isSelected())) totalLC |= seg.getLightCapabilities();
  }
  return totalLC;
}

uint8_t WS2812FX::getFirstSelectedSegId() const {
  size_t i = 0;
  for (const segment &seg : _segments) {
    if (seg.isActive() && seg.isSelected()) return i;
    i++;
  }
  // if none selected, use the main segment
  return getMainSegmentId();
}

void WS2812FX::setMainSegmentId(unsigned n) {
  _mainSegment = 0;
  if (n < _segments.size()) {
    _mainSegment = n;
  }
  return;
}

uint8_t WS2812FX::getLastActiveSegmentId() const {
  for (size_t i = _segments.size() -1; i > 0; i--) {
    if (_segments[i].isActive()) return i;
  }
  return 0;
}

uint8_t WS2812FX::getActiveSegmentsNum() const {
  uint8_t c = 0;
  for (size_t i = 0; i < _segments.size(); i++) {
    if (_segments[i].isActive()) c++;
  }
  return c;
}

uint16_t WS2812FX::getLengthTotal() const {
  unsigned len = Segment::maxWidth * Segment::maxHeight; // will be _length for 1D (see finalizeInit()) but should cover whole matrix for 2D
  if (isMatrix && _length > len) len = _length; // for 2D with trailing strip
  return len;
}

uint16_t WS2812FX::getLengthPhysical() const {
  unsigned len = 0;
  for (size_t b = 0; b < BusManager::getNumBusses(); b++) {
    Bus *bus = BusManager::getBus(b);
    if (bus->isVirtual()) continue; //exclude non-physical network busses
    len += bus->getLength();
  }
  return len;
}

//used for JSON API info.leds.rgbw. Little practical use, deprecate with info.leds.rgbw.
//returns if there is an RGBW bus (supports RGB and White, not only white)
//not influenced by auto-white mode, also true if white slider does not affect output white channel
bool WS2812FX::hasRGBWBus() const {
  for (size_t b = 0; b < BusManager::getNumBusses(); b++) {
    const Bus *bus = BusManager::getBus(b);
    if (!bus || !bus->isOk()) break;
    if (bus->hasRGB() && bus->hasWhite()) return true;
  }
  return false;
}

bool WS2812FX::hasCCTBus() const {
  if (cctFromRgb && !correctWB) return false;
  for (size_t b = 0; b < BusManager::getNumBusses(); b++) {
    const Bus *bus = BusManager::getBus(b);
    if (!bus || !bus->isOk()) break;
    if (bus->hasCCT()) return true;
  }
  return false;
}

void WS2812FX::purgeSegments() {
  // remove all inactive segments (from the back)
  int deleted = 0;
  if (_segments.size() <= 1) return;
  for (size_t i = _segments.size()-1; i > 0; i--)
    if (_segments[i].stop == 0) {
      deleted++;
      _segments.erase(_segments.begin() + i);
    }
  if (deleted) {
    _segments.shrink_to_fit();
    setMainSegmentId(0);
  }
}

Segment& WS2812FX::getSegment(unsigned id) {
  return _segments[id >= _segments.size() ? getMainSegmentId() : id]; // vectors
}

void WS2812FX::resetSegments() {
  _segments.clear(); // destructs all Segment as part of clearing
  #ifndef WLED_DISABLE_2D
  segment seg = isMatrix ? Segment(0, Segment::maxWidth, 0, Segment::maxHeight) : Segment(0, _length);
  #else
  segment seg = Segment(0, _length);
  #endif
  _segments.push_back(seg);
  _segments.shrink_to_fit(); // just in case ...
  _mainSegment = 0;
}

void WS2812FX::makeAutoSegments(bool forceReset) {
  if (autoSegments) { //make one segment per bus
    unsigned segStarts[MAX_NUM_SEGMENTS] = {0};
    unsigned segStops [MAX_NUM_SEGMENTS] = {0};
    size_t s = 0;

    #ifndef WLED_DISABLE_2D
    // 2D segment is the 1st one using entire matrix
    if (isMatrix) {
      segStarts[0] = 0;
      segStops[0]  = Segment::maxWidth*Segment::maxHeight;
      s++;
    }
    #endif

    for (size_t i = s; i < BusManager::getNumBusses(); i++) {
      const Bus *bus = BusManager::getBus(i);
      if (!bus || !bus->isOk()) break;

      segStarts[s] = bus->getStart();
      segStops[s]  = segStarts[s] + bus->getLength();

      #ifndef WLED_DISABLE_2D
      if (isMatrix && segStops[s] <= Segment::maxWidth*Segment::maxHeight) continue; // ignore buses comprising matrix
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
    DEBUG_PRINTF_P(PSTR("%d auto segments created.\n"), _segments.size());

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
  // if any segments were deleted free memory
  purgeSegments();
  // this is always called as the last step after finalizeInit(), update covered bus types
  for (segment &seg : _segments)
    seg.refreshLightCapabilities();
}

//true if all segments align with a bus, or if a segment covers the total length
//irrelevant in 2D set-up
bool WS2812FX::checkSegmentAlignment() const {
  bool aligned = false;
  for (const segment &seg : _segments) {
    for (unsigned b = 0; b<BusManager::getNumBusses(); b++) {
      const Bus *bus = BusManager::getBus(b);
      if (!bus || !bus->isOk()) break;
      if (seg.start == bus->getStart() && seg.stop == bus->getStart() + bus->getLength()) aligned = true;
    }
    if (seg.start == 0 && seg.stop == _length) aligned = true;
    if (!aligned) return false;
  }
  return true;
}

// used by analog clock overlay
void WS2812FX::setRange(uint16_t i, uint16_t i2, uint32_t col) {
  if (i2 < i) std::swap(i,i2);
  for (unsigned x = i; x <= i2; x++) setPixelColor(x, col);
}

#ifdef WLED_DEBUG
void WS2812FX::printSize() {
  size_t size = 0;
  for (const Segment &seg : _segments) size += seg.getSize();
  DEBUG_PRINTF_P(PSTR("Segments: %d -> %u/%dB\n"), _segments.size(), size, Segment::getUsedSegmentData());
  for (const Segment &seg : _segments) DEBUG_PRINTF_P(PSTR("  Seg: %d,%d [A=%d, 2D=%d, RGB=%d, W=%d, CCT=%d]\n"), seg.width(), seg.height(), seg.isActive(), seg.is2D(), seg.hasRGB(), seg.hasWhite(), seg.isCCT());
  DEBUG_PRINTF_P(PSTR("Modes: %d*%d=%uB\n"), sizeof(mode_ptr), _mode.size(), (_mode.capacity()*sizeof(mode_ptr)));
  DEBUG_PRINTF_P(PSTR("Data: %d*%d=%uB\n"), sizeof(const char *), _modeData.size(), (_modeData.capacity()*sizeof(const char *)));
  DEBUG_PRINTF_P(PSTR("Map: %d*%d=%uB\n"), sizeof(uint16_t), (int)customMappingSize, customMappingSize*sizeof(uint16_t));
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
        if (!pal.isNull() && pal.size()>3) { // not an empty palette (at least 2 entries)
          if (pal[0].is<int>() && pal[1].is<const char *>()) {
            // we have an array of index & hex strings
            size_t palSize = MIN(pal.size(), 36);
            palSize -= palSize % 2; // make sure size is multiple of 2
            for (size_t i=0, j=0; i<palSize && pal[i].as<int>()<256; i+=2, j+=4) {
              uint8_t rgbw[] = {0,0,0,0};
              tcp[ j ] = (uint8_t) pal[ i ].as<int>(); // index
              colorFromHexString(rgbw, pal[i+1].as<const char *>()); // will catch non-string entires
              for (size_t c=0; c<3; c++) tcp[j+1+c] = gamma8(rgbw[c]); // only use RGB component
              DEBUG_PRINTF_P(PSTR("%d(%d) : %d %d %d\n"), i, int(tcp[j]), int(tcp[j+1]), int(tcp[j+2]), int(tcp[j+3]));
            }
          } else {
            size_t palSize = MIN(pal.size(), 72);
            palSize -= palSize % 4; // make sure size is multiple of 4
            for (size_t i=0; i<palSize && pal[i].as<int>()<256; i+=4) {
              tcp[ i ] = (uint8_t) pal[ i ].as<int>(); // index
              tcp[i+1] = gamma8((uint8_t) pal[i+1].as<int>()); // R
              tcp[i+2] = gamma8((uint8_t) pal[i+2].as<int>()); // G
              tcp[i+3] = gamma8((uint8_t) pal[i+3].as<int>()); // B
              DEBUG_PRINTF_P(PSTR("%d(%d) : %d %d %d\n"), i, int(tcp[i]), int(tcp[i+1]), int(tcp[i+2]), int(tcp[i+3]));
            }
          }
          customPalettes.push_back(targetPalette.loadDynamicGradientPalette(tcp));
        } else {
          DEBUG_PRINTLN(F("Wrong palette format."));
        }
      }
    } else {
      break;
    }
  }
}

//load custom mapping table from JSON file (called from finalizeInit() or deserializeState())
bool WS2812FX::deserializeMap(unsigned n) {
  // 2D support creates its own ledmap (on the fly) if a ledmap.json exists it will overwrite built one.

  char fileName[32];
  strcpy_P(fileName, PSTR("/ledmap"));
  if (n) sprintf(fileName +7, "%d", n);
  strcat_P(fileName, PSTR(".json"));
  bool isFile = WLED_FS.exists(fileName);

  customMappingSize = 0; // prevent use of mapping if anything goes wrong
  currentLedmap = 0;
  if (n == 0 || isFile) interfaceUpdateCallMode = CALL_MODE_WS_SEND; // schedule WS update (to inform UI)

  if (!isFile && n==0 && isMatrix) {
    setUpMatrix();
    return false;
  }

  if (!isFile || !requestJSONBufferLock(7)) return false;

  StaticJsonDocument<64> filter;
  filter[F("width")]  = true;
  filter[F("height")] = true;
  if (!readObjectFromFile(fileName, nullptr, pDoc, &filter)) {
    DEBUG_PRINT(F("ERROR Invalid ledmap in ")); DEBUG_PRINTLN(fileName);
    releaseJSONBufferLock();
    return false; // if file does not load properly then exit
  }

  suspend();

  JsonObject root = pDoc->as<JsonObject>();
  // if we are loading default ledmap (at boot) set matrix width and height from the ledmap (compatible with WLED MM ledmaps)
  if (isMatrix && n == 0 && (!root[F("width")].isNull() || !root[F("height")].isNull())) {
    Segment::maxWidth  = min(max(root[F("width")].as<int>(), 1), 128);
    Segment::maxHeight = min(max(root[F("height")].as<int>(), 1), 128);
  }

  if (customMappingTable) free(customMappingTable);
  customMappingTable = static_cast<uint16_t*>(malloc(sizeof(uint16_t)*getLengthTotal()));

  if (customMappingTable) {
    DEBUG_PRINT(F("Reading LED map from ")); DEBUG_PRINTLN(fileName);
    File f = WLED_FS.open(fileName, "r");
    f.find("\"map\":[");
    while (f.available()) { // f.position() < f.size() - 1
      char number[32];
      size_t numRead = f.readBytesUntil(',', number, sizeof(number)-1); // read a single number (may include array terminating "]" but not number separator ',')
      number[numRead] = 0;
      if (numRead > 0) {
        char *end = strchr(number,']'); // we encountered end of array so stop processing if no digit found
        bool foundDigit = (end == nullptr);
        int i = 0;
        if (end != nullptr) do {
          if (number[i] >= '0' && number[i] <= '9') foundDigit = true;
          if (foundDigit || &number[i++] == end) break;
        } while (i < 32);
        if (!foundDigit) break;
        int index = atoi(number);
        if (index < 0 || index > 16384) index = 0xFFFF;
        customMappingTable[customMappingSize++] = index;
        if (customMappingSize > getLengthTotal()) break;
      } else break; // there was nothing to read, stop
    }
    currentLedmap = n;
    f.close();

    #ifdef WLED_DEBUG
    DEBUG_PRINT(F("Loaded ledmap:"));
    for (unsigned i=0; i<customMappingSize; i++) {
      if (!(i%Segment::maxWidth)) DEBUG_PRINTLN();
      DEBUG_PRINTF_P(PSTR("%4d,"), customMappingTable[i]);
    }
    DEBUG_PRINTLN();
    #endif
/*
    JsonArray map = root[F("map")];
    if (!map.isNull() && map.size()) {  // not an empty map
      customMappingSize = min((unsigned)map.size(), (unsigned)getLengthTotal());
      for (unsigned i=0; i<customMappingSize; i++) customMappingTable[i] = (uint16_t) (map[i]<0 ? 0xFFFFU : map[i]);
      currentLedmap = n;
    }
*/
  } else {
    DEBUG_PRINTLN(F("ERROR LED map allocation error."));
  }

  resume();

  releaseJSONBufferLock();
  return (customMappingSize > 0);
}


WS2812FX* WS2812FX::instance = nullptr;

const char JSON_mode_names[] PROGMEM = R"=====(["FX names moved"])=====";
const char JSON_palette_names[] PROGMEM = R"=====([
"Default","* Random Cycle","* Color 1","* Colors 1&2","* Color Gradient","* Colors Only","Party","Cloud","Lava","Ocean",
"Forest","Rainbow","Rainbow Bands","Sunset","Rivendell","Breeze","Red & Blue","Yellowout","Analogous","Splash",
"Pastel","Sunset 2","Beach","Vintage","Departure","Landscape","Beech","Sherbet","Hult","Hult 64",
"Drywet","Jul","Grintage","Rewhi","Tertiary","Fire","Icefire","Cyane","Light Pink","Autumn",
"Magenta","Magred","Yelmag","Yelblu","Orange & Teal","Tiamat","April Night","Orangery","C9","Sakura",
"Aurora","Atlantica","C9 2","C9 New","Temperature","Aurora 2","Retro Clown","Candy","Toxy Reaf","Fairy Reaf",
"Semi Blue","Pink Candy","Red Reaf","Aqua Flash","Yelblu Hot","Lite Light","Red Flash","Blink Red","Red Shift","Red Tide",
"Candy2","Traffic Light"
])=====";