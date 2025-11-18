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
#include "FXparticleSystem.h"  // TODO: better define the required function (mem service) in FX.h?

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

#if MAX_NUM_SEGMENTS < WLED_MAX_BUSSES
  #error "Max segments must be at least max number of busses!"
#endif


///////////////////////////////////////////////////////////////////////////////
// Segment class implementation
///////////////////////////////////////////////////////////////////////////////
unsigned      Segment::_usedSegmentData   = 0U; // amount of RAM all segments use for their data[]
uint16_t      Segment::maxWidth           = DEFAULT_LED_COUNT;
uint16_t      Segment::maxHeight          = 1;
unsigned      Segment::_vLength           = 0;
unsigned      Segment::_vWidth            = 0;
unsigned      Segment::_vHeight           = 0;
uint32_t      Segment::_currentColors[NUM_COLORS] = {0,0,0};
CRGBPalette16 Segment::_currentPalette    = CRGBPalette16(CRGB::Black);
CRGBPalette16 Segment::_randomPalette     = generateRandomPalette();  // was CRGBPalette16(DEFAULT_COLOR);
CRGBPalette16 Segment::_newRandomPalette  = generateRandomPalette();  // was CRGBPalette16(DEFAULT_COLOR);
uint16_t      Segment::_lastPaletteChange = 0; // in seconds; perhaps it should be per segment
uint16_t      Segment::_nextPaletteBlend  = 0; // in millis

bool     Segment::_modeBlend = false;
uint16_t Segment::_clipStart = 0;
uint16_t Segment::_clipStop = 0;
uint8_t  Segment::_clipStartY = 0;
uint8_t  Segment::_clipStopY = 1;

// copy constructor
Segment::Segment(const Segment &orig) {
  //DEBUG_PRINTF_P(PSTR("-- Copy segment constructor: %p -> %p\n"), &orig, this);
  memcpy((void*)this, (void*)&orig, sizeof(Segment));
  _t   = nullptr; // copied segment cannot be in transition
  name = nullptr;
  data = nullptr;
  _dataLen = 0;
  pixels = nullptr;
  if (!stop) return;  // nothing to do if segment is inactive/invalid
  if (orig.pixels) {
    // allocate pixel buffer: prefer IRAM/PSRAM
    pixels = static_cast<uint32_t*>(allocate_buffer(orig.length() * sizeof(uint32_t), BFRALLOC_PREFER_PSRAM | BFRALLOC_NOBYTEACCESS));
    if (pixels) {
      memcpy(pixels, orig.pixels, sizeof(uint32_t) * orig.length());
      if (orig.name) { name = static_cast<char*>(allocate_buffer(strlen(orig.name)+1, BFRALLOC_PREFER_PSRAM)); if (name) strcpy(name, orig.name); }
      if (orig.data) { if (allocateData(orig._dataLen)) memcpy(data, orig.data, orig._dataLen); }
    } else {
      DEBUGFX_PRINTLN(F("!!! Not enough RAM for pixel buffer !!!"));
      errorFlag = ERR_NORAM_PX;
      stop = 0; // mark segment as inactive/invalid
    }
  } else stop = 0; // mark segment as inactive/invalid
}

// move constructor
Segment::Segment(Segment &&orig) noexcept {
  //DEBUG_PRINTF_P(PSTR("-- Move segment constructor: %p -> %p\n"), &orig, this);
  memcpy((void*)this, (void*)&orig, sizeof(Segment));
  orig._t   = nullptr; // old segment cannot be in transition any more
  orig.name = nullptr;
  orig.data = nullptr;
  orig._dataLen = 0;
  orig.pixels = nullptr;
}

// copy assignment
Segment& Segment::operator= (const Segment &orig) {
  //DEBUG_PRINTF_P(PSTR("-- Copying segment: %p -> %p\n"), &orig, this);
  if (this != &orig) {
    // clean destination
    if (name) { p_free(name); name = nullptr; }
    if (_t) stopTransition(); // also erases _t
    deallocateData();
    p_free(pixels);
    // copy source
    memcpy((void*)this, (void*)&orig, sizeof(Segment));
    // erase pointers to allocated data
    data = nullptr;
    _dataLen = 0;
    pixels = nullptr;
    if (!stop) return *this;  // nothing to do if segment is inactive/invalid
    // copy source data
    if (orig.pixels) {
      // allocate pixel buffer: prefer IRAM/PSRAM
      pixels = static_cast<uint32_t*>(allocate_buffer(orig.length() * sizeof(uint32_t), BFRALLOC_PREFER_PSRAM | BFRALLOC_NOBYTEACCESS));
      if (pixels) {
        memcpy(pixels, orig.pixels, sizeof(uint32_t) * orig.length());
        if (orig.name) { name = static_cast<char*>(allocate_buffer(strlen(orig.name)+1, BFRALLOC_PREFER_PSRAM)); if (name) strcpy(name, orig.name); }
        if (orig.data) { if (allocateData(orig._dataLen)) memcpy(data, orig.data, orig._dataLen); }
      } else {
        DEBUG_PRINTLN(F("!!! Not enough RAM for pixel buffer !!!"));
        errorFlag = ERR_NORAM_PX;
        stop = 0; // mark segment as inactive/invalid
      }
    } else stop = 0; // mark segment as inactive/invalid
  }
  return *this;
}

// move assignment
Segment& Segment::operator= (Segment &&orig) noexcept {
  //DEBUG_PRINTF_P(PSTR("-- Moving segment: %p -> %p\n"), &orig, this);
  if (this != &orig) {
    if (name) { p_free(name); name = nullptr; } // free old name
    if (_t) stopTransition(); // also erases _t
    deallocateData(); // free old runtime data
    p_free(pixels);   // free old pixel buffer
    // move source data
    memcpy((void*)this, (void*)&orig, sizeof(Segment));
    orig.name = nullptr;
    orig.data = nullptr;
    orig._dataLen = 0;
    orig.pixels = nullptr;
    orig._t = nullptr; // old segment cannot be in transition
  }
  return *this;
}

// allocates effect data buffer on heap and initialises (erases) it
bool Segment::allocateData(size_t len) {
  if (len == 0) return false;    // nothing to do
  if (data && _dataLen >= len) { // already allocated enough (reduce fragmentation)
    if (call == 0) {
      if (_dataLen < FAIR_DATA_PER_SEG) { // segment data is small
        //DEBUG_PRINTF_P(PSTR("--   Clearing data (%d): %p\n"), len, this);
        memset(data, 0, len);  // erase buffer if called during effect initialisation
        return true; // no need to reallocate
      }
    }
    else
      return true;
  }
  //DEBUG_PRINTF_P(PSTR("--   Allocating data (%d): %p\n"), len, this);
  // limit to MAX_SEGMENT_DATA if there is no PSRAM, otherwise prefer functionality over speed
  #ifndef BOARD_HAS_PSRAM
  if (Segment::getUsedSegmentData() + len - _dataLen > MAX_SEGMENT_DATA) {
    // not enough memory
    DEBUG_PRINTF_P(PSTR("SegmentData limit reached: %d/%d\n"), len, Segment::getUsedSegmentData());
    errorFlag = ERR_NORAM;
    return false;
  }
  #endif

  if (data) {
    d_free(data); // free data and try to allocate again (segment buffer may be blocking contiguous heap)
    Segment::addUsedSegmentData(-_dataLen); // subtract buffer size
  }

  data = static_cast<byte*>(allocate_buffer(len, BFRALLOC_PREFER_DRAM | BFRALLOC_CLEAR)); // prefer DRAM over PSRAM for speed

  if (data) {
    Segment::addUsedSegmentData(len);
    _dataLen = len;
    //DEBUG_PRINTF_P(PSTR("---  Allocated data (%p): %d/%d -> %p\n"), this, len, Segment::getUsedSegmentData(), data);
    return true;
  }
  // allocation failed
  DEBUG_PRINTLN(F("!!! Allocation failed. !!!"));
  errorFlag = ERR_NORAM;
  return false;
}

void Segment::deallocateData() {
  if (!data) { _dataLen = 0; return; }
  if ((Segment::getUsedSegmentData() > 0) && (_dataLen > 0)) { // check that we don't have a dangling / inconsistent data pointer
    //DEBUG_PRINTF_P(PSTR("---  Released data (%p): %d/%d -> %p\n"), this, _dataLen, Segment::getUsedSegmentData(), data);
    d_free(data);
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
  if (!reset || !isActive()) return;
  //DEBUG_PRINTF_P(PSTR("-- Segment reset: %p\n"), this);
  if (data && _dataLen > 0) {
    if (_dataLen > FAIR_DATA_PER_SEG) deallocateData(); // do not keep large allocations
    else memset(data, 0, _dataLen);  // can prevent heap fragmentation
    DEBUG_PRINTF_P(PSTR("-- Segment %p reset, data cleared\n"), this);
  }
  if (pixels) for (size_t i = 0; i < length(); i++) pixels[i] = BLACK; // clear pixel buffer
  next_time = 0; step = 0; call = 0; aux0 = 0; aux1 = 0;
  reset = false;
  #ifdef WLED_ENABLE_GIF
  endImagePlayback(this);
  #endif
}

CRGBPalette16 &Segment::loadPalette(CRGBPalette16 &targetPalette, uint8_t pal) {
  // there is one randomy generated palette (1) followed by 4 palettes created from segment colors (2-5)
  // those are followed by 7 fastled palettes (6-12) and 59 gradient palettes (13-71)
  // then come the custom palettes (255,254,...) growing downwards from 255 (255 being 1st custom palette)
  // palette 0 is a varying palette depending on effect and may be replaced by segment's color if so
  // instructed in color_from_palette()
  if (pal > FIXED_PALETTE_COUNT && pal <= 255-customPalettes.size()) pal = 0; // out of bounds palette
  //default palette. Differs depending on effect
  if (pal == 0) pal = _default_palette; // _default_palette is set in setMode()
  switch (pal) {
    case 0: //default palette. Exceptions for specific effects above
      targetPalette = PartyColors_p;
      break;
    case 1: //randomly generated palette
      targetPalette = _randomPalette; //random palette is generated at intervals in handleRandomPalette()
      break;
    case 2: {//primary color only
      CRGB prim = colors[0];
      targetPalette = CRGBPalette16(prim);
      break;}
    case 3: {//primary + secondary
      CRGB prim = colors[0];
      CRGB sec  = colors[1];
      targetPalette = CRGBPalette16(prim,prim,sec,sec);
      break;}
    case 4: {//primary + secondary + tertiary
      CRGB prim = colors[0];
      CRGB sec  = colors[1];
      CRGB ter  = colors[2];
      targetPalette = CRGBPalette16(ter,sec,prim);
      break;}
    case 5: {//primary + secondary (+tertiary if not off), more distinct
      CRGB prim = colors[0];
      CRGB sec  = colors[1];
      if (colors[2]) {
        CRGB ter = colors[2];
        targetPalette = CRGBPalette16(prim,prim,prim,prim,prim,sec,sec,sec,sec,sec,ter,ter,ter,ter,ter,prim);
      } else {
        targetPalette = CRGBPalette16(prim,prim,prim,prim,prim,prim,prim,prim,sec,sec,sec,sec,sec,sec,sec,sec);
      }
      break;}
    default: //progmem palettes
      if (pal > 255 - customPalettes.size()) {
        targetPalette = customPalettes[255-pal]; // we checked bounds above
      } else if (pal < DYNAMIC_PALETTE_COUNT+FASTLED_PALETTE_COUNT+1) { // palette 6 - 12, fastled palettes
        targetPalette = *fastledPalettes[pal-DYNAMIC_PALETTE_COUNT-1];
      } else {
        byte tcp[72];
        memcpy_P(tcp, (byte*)pgm_read_dword(&(gGradientPalettes[pal-(DYNAMIC_PALETTE_COUNT+FASTLED_PALETTE_COUNT)-1])), 72);
        targetPalette.loadDynamicGradientPalette(tcp);
      }
      break;
  }
  return targetPalette;
}

// starting a transition has to occur before change so we get current values 1st
void Segment::startTransition(uint16_t dur, bool segmentCopy) {
  if (dur == 0 || !isActive()) {
    if (isInTransition()) _t->_dur = 0;
    return;
  }
  if (isInTransition()) {
    if (segmentCopy && !_t->_oldSegment) {
      // already in transition but segment copy requested and not yet created
      _t->_oldSegment = new(std::nothrow) Segment(*this); // store/copy current segment settings
      _t->_start = millis();                              // restart countdown
      _t->_dur   = dur;
      _t->_prevPaletteBlends = 0;
      if (_t->_oldSegment) {
        _t->_oldSegment->palette = _t->_palette;          // restore original palette and colors (from start of transition)
        for (unsigned i = 0; i < NUM_COLORS; i++) _t->_oldSegment->colors[i] = _t->_colors[i];
        DEBUGFX_PRINTF_P(PSTR("-- Updated transition with segment copy: S=%p T(%p) O[%p] OP[%p]\n"), this, _t, _t->_oldSegment, _t->_oldSegment->pixels);
        if (!_t->_oldSegment->isActive()) stopTransition();
      }
    }
    return;
  }

  // no previous transition running, start by allocating memory for segment copy
  _t = new(std::nothrow) Transition(dur);
  if (_t) {
    _t->_bri = on ? opacity : 0;
    _t->_cct = cct;
    _t->_palette = palette;
    #ifndef WLED_SAVE_RAM
    loadPalette(_t->_palT, palette);
    #endif
    for (int i=0; i<NUM_COLORS; i++) _t->_colors[i] = colors[i];
    if (segmentCopy) _t->_oldSegment = new(std::nothrow) Segment(*this); // store/copy current segment settings
    if (_t->_oldSegment) {
      DEBUGFX_PRINTF_P(PSTR("-- Started transition: S=%p T(%p) O[%p] OP[%p]\n"), this, _t, _t->_oldSegment, _t->_oldSegment->pixels);
      if (!_t->_oldSegment->isActive()) stopTransition();
    } else {
      DEBUGFX_PRINTF_P(PSTR("-- Started transition without old segment: S=%p T(%p)\n"), this, _t);
    }
  };
}

void Segment::stopTransition() {
  DEBUG_PRINTF_P(PSTR("-- Stopping transition: S=%p T(%p) O[%p]\n"), this, _t, _t->_oldSegment);
  delete _t;
  _t = nullptr;
}

// sets transition progress variable (0-65535) based on time passed since transition start
void Segment::updateTransitionProgress() const {
  if (isInTransition()) {
    _t->_progress = 0xFFFF;
    unsigned diff = millis() - _t->_start;
    if (_t->_dur > 0 && diff < _t->_dur) _t->_progress = diff * 0xFFFFU / _t->_dur;
  }
}

// will return segment's CCT during a transition
// isPreviousMode() is actually not implemented for CCT in strip.service() as WLED does not support per-pixel CCT
uint8_t Segment::currentCCT() const {
  unsigned prog = progress();
  if (prog < 0xFFFFU) {
    if (blendingStyle == BLEND_STYLE_FADE) return (cct * prog + (_t->_cct * (0xFFFFU - prog))) / 0xFFFFU;
    //else                                   return Segment::isPreviousMode() ? _t->_cct : cct;
  }
  return cct;
}

// will return segment's opacity during a transition (blending it with old in case of FADE transition)
uint8_t Segment::currentBri() const {
  unsigned prog = progress();
  unsigned curBri = on ? opacity : 0;
  if (prog < 0xFFFFU) {
    // this will blend opacity in new mode if style is FADE (single effect call)
    if (blendingStyle == BLEND_STYLE_FADE) curBri = (prog * curBri + _t->_bri * (0xFFFFU - prog)) / 0xFFFFU;
    else                                   curBri = Segment::isPreviousMode() ? _t->_bri : curBri;
  }
  return curBri;
}

// pre-calculate drawing parameters for faster access (based on the idea from @softhack007 from MM fork)
// and blends colors and palettes if necessary
// prog is the progress of the transition (0-65535) and is passed to the function as it may be called in the context of old segment
// which does not have transition structure
void Segment::beginDraw(uint16_t prog) {
  setDrawDimensions();
  // load colors into _currentColors
  for (unsigned i = 0; i < NUM_COLORS; i++) _currentColors[i] = colors[i];
  // load palette into _currentPalette
  loadPalette(Segment::_currentPalette, palette);
  if (isInTransition() && prog < 0xFFFFU && blendingStyle == BLEND_STYLE_FADE) {
    // blend colors
    for (unsigned i = 0; i < NUM_COLORS; i++) _currentColors[i] = color_blend16(_t->_colors[i], colors[i], prog);
    // blend palettes
    // there are about 255 blend passes of 48 "blends" to completely blend two palettes (in _dur time)
    // minimum blend time is 100ms maximum is 65535ms
    #ifndef WLED_SAVE_RAM
    unsigned noOfBlends = ((255U * prog) / 0xFFFFU) - _t->_prevPaletteBlends;
    if(noOfBlends > 255) noOfBlends = 255; // safety check
    for (unsigned i = 0; i < noOfBlends; i++, _t->_prevPaletteBlends++) nblendPaletteTowardPalette(_t->_palT, Segment::_currentPalette, 48);
    Segment::_currentPalette = _t->_palT; // copy transitioning/temporary palette
    #else
    unsigned noOfBlends = ((255U * prog) / 0xFFFFU);
    CRGBPalette16 tmpPalette;
    loadPalette(tmpPalette, _t->_palette);
    for (unsigned i = 0; i < noOfBlends; i++) nblendPaletteTowardPalette(tmpPalette, Segment::_currentPalette, 48);
    Segment::_currentPalette = tmpPalette; // copy transitioning/temporary palette
    #endif
  }
}

// relies on WS2812FX::service() to call it for each frame
void Segment::handleRandomPalette() {
  unsigned long now = millis();
  uint16_t now_s = now / 1000; // we only need seconds (and @dedehai hated shift >> 10)
  now = (now_s)*1000 + (now % 1000); // ignore days (now is limited to 18 hours as now_s can only store 65535s ~ 18h 12min)
  if (now_s < Segment::_lastPaletteChange) Segment::_lastPaletteChange = 0; // handle overflow (will cause 2*randomPaletteChangeTime glitch at most)
  // is it time to generate a new palette?
  if (now_s > Segment::_lastPaletteChange + randomPaletteChangeTime) {
    Segment::_newRandomPalette  = useHarmonicRandomPalette ? generateHarmonicRandomPalette(Segment::_randomPalette) : generateRandomPalette();
    Segment::_lastPaletteChange = now_s;
    Segment::_nextPaletteBlend  = now; // starts blending immediately
  }
  // there are about 255 blend passes of 48 "blends" to completely blend two palettes (in strip.getTransition() time)
  // if randomPaletteChangeTime is shorter than strip.getTransition() palette will never fully blend
  unsigned frameTime = strip.getFrameTime();  // in ms [8-1000]
  unsigned transitionTime = strip.getTransition(); // in ms [100-65535]
  if ((uint16_t)now < Segment::_nextPaletteBlend || now > ((Segment::_lastPaletteChange*1000) + transitionTime + 2*frameTime)) return; // not yet time or past transition time, no need to blend
  unsigned transitionFrames = frameTime > transitionTime ? 1 : transitionTime / frameTime; // i.e. 700ms/23ms = 30 or 20000ms/8ms = 2500 or 100ms/1000ms = 0 -> 1
  unsigned noOfBlends = transitionFrames > 255 ? 1 : (255 + (transitionFrames>>1)) / transitionFrames;  // we do some rounding here
  for (unsigned i = 0; i < noOfBlends; i++) nblendPaletteTowardPalette(Segment::_randomPalette, Segment::_newRandomPalette, 48);
  Segment::_nextPaletteBlend = now + ((transitionFrames >> 8) * frameTime); // postpone next blend if necessary
}

// sets Segment geometry (length or width/height and grouping, spacing and offset as well as 2D mapping)
// strip must be suspended (strip.suspend()) before calling this function
// this function may call fill() to clear pixels if spacing or mapping changed (which requires setting _vWidth, _vHeight, _vLength or beginDraw())
void Segment::setGeometry(uint16_t i1, uint16_t i2, uint8_t grp, uint8_t spc, uint16_t ofs, uint16_t i1Y, uint16_t i2Y, uint8_t m12) {
  // return if neither bounds nor grouping have changed
  bool boundsUnchanged = (start == i1 && stop == i2);
  #ifndef WLED_DISABLE_2D
  boundsUnchanged &= (startY == i1Y && stopY == i2Y); // 2D
  #endif
  boundsUnchanged &= (grouping == grp && spacing == spc); // changing grouping and/or spacing changes virtual segment length (painting dimensions)

  if (stop && (spc > 0 || m12 != map1D2D)) clear();
  if (grp) { // prevent assignment of 0
    grouping = grp;
    spacing = spc;
  } else {
    grouping = 1;
    spacing = 0;
  }
  if (ofs < UINT16_MAX) offset = ofs;
  map1D2D  = constrain(m12, 0, 7);

  if (boundsUnchanged) return;

  unsigned oldLength = length();

  DEBUGFX_PRINTF_P(PSTR("Segment geometry: %d,%d -> %d,%d [%d,%d]\n"), (int)i1, (int)i2, (int)i1Y, (int)i2Y, (int)grp, (int)spc);
  markForReset();
  if (_t) stopTransition(); // we can't use transition if segment dimensions changed
  stateChanged = true;      // send UDP/WS broadcast

  // apply change immediately
  if (i2 <= i1) { //disable segment
    #ifdef WLED_ENABLE_GIF
    endImagePlayback(this);
    #endif
    deallocateData();
    p_free(pixels);
    pixels = nullptr;
    stop = 0;
    return;
  }
  if (i1 < Segment::maxWidth || (i1 >= Segment::maxWidth*Segment::maxHeight && i1 < strip.getLengthTotal())) start = i1; // Segment::maxWidth equals strip.getLengthTotal() for 1D
  stop = i2 > Segment::maxWidth*Segment::maxHeight ? MIN(i2,strip.getLengthTotal()) : constrain(i2, 1, Segment::maxWidth);
  startY = 0;
  stopY  = 1;
  #ifndef WLED_DISABLE_2D
  if (Segment::maxHeight>1) { // 2D
    if (i1Y < Segment::maxHeight) startY = i1Y;
    stopY = constrain(i2Y, 1, Segment::maxHeight);
  }
  #endif
  // safety check
  if (start >= stop || startY >= stopY) {
    #ifdef WLED_ENABLE_GIF
    endImagePlayback(this);
    #endif
    deallocateData();
    p_free(pixels);
    pixels = nullptr;
    stop = 0;
    return;
  }
  // allocate FX render buffer
  if (length() != oldLength) {
    // allocate render buffer (always entire segment), prefer IRAM/PSRAM. Note: impact on FPS with PSRAM buffer is low (<2% with QSPI PSRAM) on S2/S3
    p_free(pixels);
    pixels = static_cast<uint32_t*>(allocate_buffer(length() * sizeof(uint32_t), BFRALLOC_PREFER_PSRAM | BFRALLOC_NOBYTEACCESS));
    if (!pixels) {
      DEBUGFX_PRINTLN(F("!!! Not enough RAM for pixel buffer !!!"));
      #ifdef WLED_ENABLE_GIF
      endImagePlayback(this);
      #endif
      deallocateData();
      errorFlag = ERR_NORAM_PX;
      stop = 0;
      return;
    }

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
  startTransition(strip.getTransition(), blendingStyle != BLEND_STYLE_FADE); // start transition prior to change
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
    startTransition(strip.getTransition(), false); // start transition prior to change (no need to copy segment)
    cct = k;
    stateChanged = true; // send UDP/WS broadcast
  }
  return *this;
}

Segment &Segment::setOpacity(uint8_t o) {
  if (opacity != o) {
    //DEBUG_PRINTF_P(PSTR("- Starting opacity transition: %d\n"), o);
    startTransition(strip.getTransition(), blendingStyle != BLEND_STYLE_FADE); // start transition prior to change
    opacity = o;
    stateChanged = true; // send UDP/WS broadcast
  }
  return *this;
}

Segment &Segment::setOption(uint8_t n, bool val) {
  bool prev = (options >> n) & 0x01;
  if (val == prev) return *this;
  //DEBUG_PRINTF_P(PSTR("- Starting option transition: %d\n"), n);
  if (n == SEG_OPTION_ON) startTransition(strip.getTransition(), blendingStyle != BLEND_STYLE_FADE); // start transition prior to change
  if (val) options |=   0x01 << n;
  else     options &= ~(0x01 << n);
  stateChanged = true; // send UDP/WS broadcast
  return *this;
}

Segment &Segment::setMode(uint8_t fx, bool loadDefaults) {
  // skip reserved
  while (fx < strip.getModeCount() && strncmp_P("RSVD", strip.getModeData(fx), 4) == 0) fx++;
  if (fx >= strip.getModeCount()) fx = 0; // set solid mode
  // if we have a valid mode & is not reserved
  if (fx != mode) {
    startTransition(strip.getTransition(), true); // set effect transitions (must create segment copy)
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
    }
    sOpt = extractModeDefaults(fx, "pal"); // always extract 'pal' to set _default_palette
    if (sOpt >= 0 && loadDefaults) setPalette(sOpt);
    if (sOpt <= 0) sOpt = 6; // partycolors if zero or not set
    _default_palette = sOpt; // _deault_palette is loaded into pal0 in loadPalette() (if selected)
    markForReset();
    stateChanged = true; // send UDP/WS broadcast
  }
  return *this;
}

Segment &Segment::setPalette(uint8_t pal) {
  if (pal <= 255-customPalettes.size() && pal > FIXED_PALETTE_COUNT) pal = 0; // not built in palette or custom palette
  if (pal != palette) {
    //DEBUG_PRINTF_P(PSTR("- Starting palette transition: %d\n"), pal);
    startTransition(strip.getTransition(), blendingStyle != BLEND_STYLE_FADE); // start transition prior to change (no need to copy segment)
    palette = pal;
    stateChanged = true; // send UDP/WS broadcast
  }
  return *this;
}

Segment &Segment::setName(const char *newName) {
  if (newName) {
    const int newLen = min(strlen(newName), (size_t)WLED_MAX_SEGNAME_LEN);
    if (newLen) {
      if (name) p_free(name); // free old name
      name = static_cast<char*>(allocate_buffer(newLen+1, BFRALLOC_PREFER_PSRAM));
      if (mode == FX_MODE_2DSCROLLTEXT) startTransition(strip.getTransition(), true); // if the name changes in scrolling text mode, we need to copy the segment for blending
      if (name) strlcpy(name, newName, newLen+1);
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

#ifndef WLED_DISABLE_2D
// maximum length of a mapped 1D segment, used in PS for buffer allocation
uint16_t Segment::maxMappingLength() const {
  uint32_t vW = virtualWidth();
  uint32_t vH = virtualHeight();
  return max(sqrt32_bw(vH*vH + vW*vW), (uint32_t)getPinwheelLength(vW, vH)); // use diagonal
}
#endif
// pixel is clipped if it falls outside clipping range
// if clipping start > stop the clipping range is inverted
bool Segment::isPixelClipped(int i) const {
  if (blendingStyle != BLEND_STYLE_FADE && isInTransition() && _clipStart != _clipStop) {
    bool invert = _clipStart > _clipStop;  // ineverted start & stop
    int start = invert ? _clipStop : _clipStart;
    int stop  = invert ? _clipStart : _clipStop;
    if (blendingStyle == BLEND_STYLE_FAIRY_DUST) {
      unsigned len = stop - start;
      if (len < 2) return false;
      unsigned shuffled = hashInt(i) % len;
      unsigned pos = (shuffled * 0xFFFFU) / len;
      return progress() <= pos;
    }
    const bool iInside = (i >= start && i < stop);
    return !iInside ^ invert; // thanks @willmmiles (https://github.com/wled/WLED/pull/3877#discussion_r1554633876)
  }
  return false;
}

void WLED_O2_ATTR Segment::setPixelColor(int i, uint32_t col) const
{
  if (!isActive() || i < 0) return; // not active or invalid index
#ifndef WLED_DISABLE_2D
  int vStrip = 0;
#endif
  const int vL = vLength();
  // if the 1D effect is using virtual strips "i" will have virtual strip id stored in upper 16 bits
  // in such case "i" will be > virtualLength()
  if (i >= vL) {
    // check if this is a virtual strip
    #ifndef WLED_DISABLE_2D
    vStrip = i>>16; // hack to allow running on virtual strips (2D segment columns/rows)
    #endif
    i &= 0xFFFF;          // truncate vstrip index. note: vStrip index is 1 even in 1D, still need to truncate
    if (i >= vL) return;  // if pixel would still fall out of segment just exit
  }

#ifndef WLED_DISABLE_2D
  if (is2D()) {
    const int vW = vWidth();   // segment width in logical pixels (can be 0 if segment is inactive)
    const int vH = vHeight();  // segment height in logical pixels (is always >= 1)
    const auto XY = [&](unsigned x, unsigned y){ return x + y*vW;};
    switch (map1D2D) {
      case M12_Pixels:
        // use all available pixels as a long strip
        setPixelColorRaw(XY(i % vW, i / vW), col);
        break;
      case M12_pBar:
        // expand 1D effect vertically or have it play on virtual strips
        if (vStrip > 0)                   setPixelColorRaw(XY(vStrip - 1, vH - i - 1), col);
        else for (int x = 0; x < vW; x++) setPixelColorRaw(XY(x, vH - i - 1), col);
        break;
      case M12_pArc:
        // expand in circular fashion from center
        if (i == 0)
          setPixelColorRaw(XY(0, 0), col);
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
        for (int x = 0; x <= i; x++) setPixelColorXY(x, i, col); // note: <= to include i=0. Relies on overflow check in sPC()
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
            if ((unsigned)x0 >= (unsigned)vW || (unsigned)y0 >= (unsigned)vH) {
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
  setPixelColorRaw(i, col);
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

uint32_t WLED_O2_ATTR Segment::getPixelColor(int i) const
{
  if (!isActive() || i < 0) return 0; // not active or invalid index

#ifndef WLED_DISABLE_2D
  int vStrip = i>>16; // virtual strips are only relevant in Bar expansion mode
  i &= 0xFFFF;
#endif
  if (i >= (int)vLength()) return 0;

#ifndef WLED_DISABLE_2D
  if (is2D()) {
    const int vW = vWidth();   // segment width in logical pixels (can be 0 if segment is inactive)
    const int vH = vHeight();  // segment height in logical pixels (is always >= 1)
    int x = 0, y = 0;
    switch (map1D2D) {
      case M12_Pixels:
        x = i % vW;
        y = i / vW;
        break;
      case M12_pBar:
        if (vStrip > 0) { x = vStrip - 1; y = vH - i - 1; }
        else            { y = vH - i - 1; };
        break;
      case M12_pArc:
        if (i > vW && i > vH) {
          x = y = sqrt32_bw(i*i/2);
          break; // use diagonal
        }
        // otherwise fallthrough
      case M12_pCorner:
        // use longest dimension
        if (vW > vH) x = i;
        else         y = i;
        break;
      case M12_sPinwheel: {
        // not 100% accurate, returns pixel at outer edge
        int cosVal[2], sinVal[2];
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
        break;
      }
    }
    return getPixelColorXY(x, y);
  }
#endif
  return getPixelColorRaw(i);
}

void Segment::refreshLightCapabilities() const {
  unsigned capabilities = 0;

  if (!isActive()) {
    _capabilities = 0;
    return;
  }

  // we must traverse each pixel in segment to determine its capabilities (as pixel may be mapped)
  for (unsigned y = startY; y < stopY; y++) for (unsigned x = start; x < stop; x++) {
    unsigned index = x + Segment::maxWidth * y;
    index = strip.getMappedPixelIndex(index); // convert logical address to physical
    if (index == 0xFFFF) continue;  // invalid/missing  pixel
    for (unsigned b = 0; b < BusManager::getNumBusses(); b++) {
      const Bus *bus = BusManager::getBus(b);
      if (!bus || !bus->isOk()) break;
      if (bus->containsPixel(index)) {
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
        break;
      }
    }
  }
  _capabilities = capabilities;
}

/*
 * Fills segment with color
 */
void Segment::fill(uint32_t c) const {
  if (!isActive()) return; // not active
  for (unsigned i = 0; i < length(); i++) setPixelColorRaw(i,c); // always fill all pixels (blending will take care of grouping, spacing and clipping)
}

/*
 * fade out function, higher rate = quicker fade
 * fading is highly dependant on frame rate (higher frame rates, faster fading)
 * each frame will fade at max 9% or as little as 0.8%
 */
void Segment::fade_out(uint8_t rate) const {
  if (!isActive()) return; // not active
  rate = (256-rate) >> 1;
  const int mappedRate = 256 / (rate + 1);
  const size_t rlength = rawLength();  // calculate only once
  for (unsigned j = 0; j < rlength; j++) {
    uint32_t color = getPixelColorRaw(j);
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
    setPixelColorRaw(j, color);
  }
}

// fades all pixels to secondary color
void Segment::fadeToSecondaryBy(uint8_t fadeBy) const {
  if (!isActive() || fadeBy == 0) return;   // optimization - no scaling to apply
  const size_t rlength = rawLength();  // calculate only once
  for (unsigned i = 0; i < rlength; i++) setPixelColorRaw(i, color_blend(getPixelColorRaw(i), colors[1], fadeBy));
}

// fades all pixels to black using nscale8()
void Segment::fadeToBlackBy(uint8_t fadeBy) const {
  if (!isActive() || fadeBy == 0) return;   // optimization - no scaling to apply
  const size_t rlength = rawLength();  // calculate only once
  for (unsigned i = 0; i < rlength; i++) setPixelColorRaw(i, fast_color_scale(getPixelColorRaw(i), 255-fadeBy));
}

/*
 * blurs segment content, source: FastLED colorutils.cpp
 * Note: for blur_amount > 215 this function does not work properly (creates alternating pattern)
 */
void Segment::blur(uint8_t blur_amount, bool smear) const {
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
  // handle first pixel to avoid conditional in loop (faster)
  uint32_t cur = getPixelColorRaw(0);
  uint32_t carryover = fast_color_scale(cur, seep);
  setPixelColorRaw(0, fast_color_scale(cur, keep));
  for (unsigned i = 1; i < vlength; i++) {
    cur = getPixelColorRaw(i);
    uint32_t part = fast_color_scale(cur, seep);
    cur = fast_color_scale(cur, keep);
    cur = color_add(cur, carryover);
    setPixelColorRaw(i - 1, color_add(getPixelColorRaw(i - 1), part)); // previous pixel
    setPixelColorRaw(i, cur); // current pixel
    carryover = part;
  }
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Rotates the color in HSV space, where pos is H. (0=0deg, 256=360deg)
 */
uint32_t Segment::color_wheel(uint8_t pos) const {
  if (palette) return color_from_palette(pos, false, false, 0); // only wrap if "always wrap" is set
  uint8_t w = W(getCurrentColor(0));
  uint32_t rgb;
  hsv2rgb(CHSV32(static_cast<uint16_t>(pos << 8), 255, 255), rgb);
  return rgb | (w << 24); // add white channel
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
  uint32_t color = getCurrentColor(mcol);
  // default palette or no RGB support on segment
  if ((palette == 0 && mcol < NUM_COLORS) || !_isRGB) {
    return color_fade(color, pbri, true);
  }

  unsigned paletteIndex = i;
  if (mapping) paletteIndex = min((i*255)/vLength(), 255U);
  // paletteBlend: 0 - wrap when moving, 1 - always wrap, 2 - never wrap, 3 - none (undefined/no interpolation of palette entries)
  // ColorFromPalette interpolations are: NOBLEND, LINEARBLEND, LINEARBLEND_NOWRAP
  TBlendType blend = NOBLEND;
  switch (paletteBlend) {
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
  BusManager::removeAll();

  unsigned digitalCount = 0;
  #if defined(ARDUINO_ARCH_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32C3)
  // determine if it is sensible to use parallel I2S outputs on ESP32 (i.e. more than 5 outputs = 1 I2S + 4 RMT)
  unsigned maxLedsOnBus = 0;
  unsigned busType = 0;
  for (const auto &bus : busConfigs) {
    if (Bus::isDigital(bus.type) && !Bus::is2Pin(bus.type)) {
      digitalCount++;
      if (busType == 0) busType = bus.type; // remember first bus type
      if (busType != bus.type) {
        DEBUG_PRINTF_P(PSTR("Mixed digital bus types detected! Forcing single I2S output.\n"));
        useParallelI2S = false; // mixed bus types, no parallel I2S
      }
      if (bus.count > maxLedsOnBus) maxLedsOnBus = bus.count;
    }
  }
  DEBUG_PRINTF_P(PSTR("Maximum LEDs on a bus: %u\nDigital buses: %u\n"), maxLedsOnBus, digitalCount);
  // we may remove 600 LEDs per bus limit when NeoPixelBus is updated beyond 2.8.3
  if (maxLedsOnBus <= 600 && useParallelI2S) BusManager::useParallelOutput(); // must call before creating buses
  else useParallelI2S = false; // enforce single I2S
  digitalCount = 0;
  #endif

  DEBUG_PRINTF_P(PSTR("Heap before buses: %d\n"), getFreeHeapSize());
  // create buses/outputs
  unsigned mem = 0;
  unsigned maxI2S = 0;
  for (const auto &bus : busConfigs) {
    unsigned memB = bus.memUsage(Bus::isDigital(bus.type) && !Bus::is2Pin(bus.type) ? digitalCount++ : 0); // does not include DMA/RMT buffer
    mem += memB;
    // estimate maximum I2S memory usage (only relevant for digital non-2pin busses)
    #if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(ESP8266)
      #if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3)
    const bool usesI2S = ((useParallelI2S && digitalCount <= 8) || (!useParallelI2S && digitalCount == 1));
      #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    const bool usesI2S = (useParallelI2S && digitalCount <= 8);
      #else
    const bool usesI2S = false;
      #endif
    if (Bus::isDigital(bus.type) && !Bus::is2Pin(bus.type) && usesI2S) {
      #ifdef NPB_CONF_4STEP_CADENCE
      constexpr unsigned stepFactor = 4; // 4 step cadence (4 bits per pixel bit)
      #else
      constexpr unsigned stepFactor = 3; // 3 step cadence (3 bits per pixel bit)
      #endif
      unsigned i2sCommonSize = stepFactor * bus.count * (3*Bus::hasRGB(bus.type)+Bus::hasWhite(bus.type)+Bus::hasCCT(bus.type)) * (Bus::is16bit(bus.type)+1);
      if (i2sCommonSize > maxI2S) maxI2S = i2sCommonSize;
    }
    #endif
    if (mem + maxI2S <= MAX_LED_MEMORY) {
      BusManager::add(bus);
      DEBUG_PRINTF_P(PSTR("Bus memory: %uB\n"), memB);
    } else {
      errorFlag = ERR_NORAM_PX; // alert UI
      DEBUG_PRINTF_P(PSTR("Out of LED memory! Bus %d (%d) #%u not created."), (int)bus.type, (int)bus.count, digitalCount);
      break;
    }
  }
  DEBUG_PRINTF_P(PSTR("LED buffer size: %uB/%uB\n"), mem + maxI2S, BusManager::memUsage());
  busConfigs.clear();
  busConfigs.shrink_to_fit();

  _length = 0;
  for (size_t i=0; i<BusManager::getNumBusses(); i++) {
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
    bus->setBrightness(scaledBri(bri));
  }
  BusManager::initializeABL(); // init brightness limiter
  DEBUG_PRINTF_P(PSTR("Heap after buses: %d\n"), ESP.getFreeHeap());

  Segment::maxWidth  = _length;
  Segment::maxHeight = 1;

  //segments are created in makeAutoSegments();
  DEBUG_PRINTLN(F("Loading custom palettes"));
  loadCustomPalettes(); // (re)load all custom palettes
  DEBUG_PRINTLN(F("Loading custom ledmaps"));
  deserializeMap();     // (re)load default ledmap (will also setUpMatrix() if ledmap does not exist)

  // allocate frame buffer after matrix has been set up (gaps!)
  p_free(_pixels); // using realloc on large buffers can cause additional fragmentation instead of reducing it
  // use PSRAM if available: there is no measurable perfomance impact between PSRAM and DRAM on S2/S3 with QSPI PSRAM for this buffer
  _pixels = static_cast<uint32_t*>(allocate_buffer(getLengthTotal() * sizeof(uint32_t), BFRALLOC_ENFORCE_PSRAM | BFRALLOC_NOBYTEACCESS | BFRALLOC_CLEAR));
  DEBUG_PRINTF_P(PSTR("strip buffer size: %uB\n"), getLengthTotal() * sizeof(uint32_t));
  DEBUG_PRINTF_P(PSTR("Heap after strip init: %uB\n"), getFreeHeapSize());
}

void WS2812FX::service() {
  unsigned long nowUp = millis(); // Be aware, millis() rolls over every 49 days
  now = nowUp + timebase;
  unsigned long elapsed = nowUp - _lastServiceShow;
  if (_suspend || elapsed <= MIN_FRAME_DELAY) return;   // keep wifi alive - no matter if triggered or unlimited
  if (!_triggered && (_targetFps != FPS_UNLIMITED)) {   // unlimited mode = no frametime
    if (elapsed < _frametime) return;                   // too early for service
  }

  bool doShow = false;

  _isServicing = true;
  _segment_index = 0;

  for (Segment &seg : _segments) {
    if (_suspend) break; // immediately stop processing segments if suspend requested during service()

    // process transition (also pre-calculates progress value)
    seg.handleTransition();
    // reset the segment runtime data if needed
    seg.resetIfRequired();

    if (!seg.isActive()) continue;

    // last condition ensures all solid segments are updated at the same time
    if (nowUp > seg.next_time || _triggered || (doShow && seg.mode == FX_MODE_STATIC))
    {
      doShow = true;
      unsigned frameDelay = FRAMETIME;

      if (!seg.freeze) { //only run effect function if not frozen
        // Effect blending
        uint16_t prog = seg.progress();
        seg.beginDraw(prog);                // set up parameters for get/setPixelColor() (will also blend colors and palette if blend style is FADE)
        _currentSegment = &seg;             // set current segment for effect functions (SEGMENT & SEGENV)
        // workaround for on/off transition to respect blending style
        frameDelay = (*_mode[seg.mode])();  // run new/current mode (needed for bri workaround)
        seg.call++;
        // if segment is in transition and no old segment exists we don't need to run the old mode
        // (blendSegments() takes care of On/Off transitions and clipping)
        Segment *segO = seg.getOldSegment();
        if (segO && segO->isActive() && (seg.mode != segO->mode || blendingStyle != BLEND_STYLE_FADE ||
            (segO->name != seg.name && segO->name && seg.name && strncmp(segO->name, seg.name, WLED_MAX_SEGNAME_LEN) != 0))) {
          Segment::modeBlend(true);         // set semaphore for beginDraw() to blend colors and palette
          segO->beginDraw(prog);            // set up palette & colors (also sets draw dimensions), parent segment has transition progress
          _currentSegment = segO;           // set current segment
          // workaround for on/off transition to respect blending style
          frameDelay = min(frameDelay, (unsigned)(*_mode[segO->mode])());  // run old mode (needed for bri workaround; semaphore!!)
          segO->call++;                     // increment old mode run counter
          Segment::modeBlend(false);        // unset semaphore
        }
        if (seg.isInTransition() && frameDelay > FRAMETIME) frameDelay = FRAMETIME; // force faster updates during transition
      }

      seg.next_time = nowUp + frameDelay;
    }
    _segment_index++;
  }

  #ifdef WLED_DEBUG
  if ((_targetFps != FPS_UNLIMITED) && (millis() - nowUp > _frametime)) DEBUG_PRINTF_P(PSTR("Slow effects %u/%d.\n"), (unsigned)(millis()-nowUp), (int)_frametime);
  #endif
  if (doShow && !_suspend) {
    yield();
    Segment::handleRandomPalette(); // slowly transition random palette; move it into for loop when each segment has individual random palette
    _lastServiceShow = nowUp; // update timestamp, for precise FPS control
    show();
  }
  #ifdef WLED_DEBUG
  if ((_targetFps != FPS_UNLIMITED) && (millis() - nowUp > _frametime)) DEBUG_PRINTF_P(PSTR("Slow strip %u/%d.\n"), (unsigned)(millis()-nowUp), (int)_frametime);
  #endif

  _triggered = false;
  _isServicing = false;
}

// https://en.wikipedia.org/wiki/Blend_modes but using a for top layer & b for bottom layer
static uint8_t _top       (uint8_t a, uint8_t b) { return a; }
static uint8_t _bottom    (uint8_t a, uint8_t b) { return b; }
static uint8_t _add       (uint8_t a, uint8_t b) { unsigned t = a + b; return t > 255 ? 255 : t; }
static uint8_t _subtract  (uint8_t a, uint8_t b) { return b > a ? (b - a) : 0; }
static uint8_t _difference(uint8_t a, uint8_t b) { return b > a ? (b - a) : (a - b); }
static uint8_t _average   (uint8_t a, uint8_t b) { return (a + b) >> 1; }
#if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32C3)
static uint8_t _multiply  (uint8_t a, uint8_t b) { return ((a * b) + 255) >> 8; } // faster than division on C3 but slightly less accurate
#else
static uint8_t _multiply  (uint8_t a, uint8_t b) { return (a * b) / 255; } // origianl uses a & b in range [0,1]
#endif
static uint8_t _divide    (uint8_t a, uint8_t b) { return a > b ? (b * 255) / a : 255; }
static uint8_t _lighten   (uint8_t a, uint8_t b) { return a > b ? a : b; }
static uint8_t _darken    (uint8_t a, uint8_t b) { return a < b ? a : b; }
static uint8_t _screen    (uint8_t a, uint8_t b) { return 255 - _multiply(~a,~b); } // 255 - (255-a)*(255-b)/255
static uint8_t _overlay   (uint8_t a, uint8_t b) { return b < 128 ? 2 * _multiply(a,b) : (255 - 2 * _multiply(~a,~b)); }
static uint8_t _hardlight (uint8_t a, uint8_t b) { return a < 128 ? 2 * _multiply(a,b) : (255 - 2 * _multiply(~a,~b)); }
#if defined(ESP8266) || defined(CONFIG_IDF_TARGET_ESP32C3)
static uint8_t _softlight (uint8_t a, uint8_t b) { return (((b * b * (255 - 2 * a))) + ((2 * a * b + 256) << 8)) >> 16; } // Pegtop's formula (1 - 2a)b^2
#else
static uint8_t _softlight (uint8_t a, uint8_t b) { return (b * b * (255 - 2 * a) + 255 * 2 * a * b) / (255 * 255); } // Pegtop's formula (1 - 2a)b^2 + 2ab
#endif
static uint8_t _dodge     (uint8_t a, uint8_t b) { return _divide(~a,b); }
static uint8_t _burn      (uint8_t a, uint8_t b) { return ~_divide(a,~b); }

void WS2812FX::blendSegment(const Segment &topSegment) const {

  typedef uint8_t(*FuncType)(uint8_t, uint8_t);
  FuncType funcs[] = {
    _top, _bottom,
    _add, _subtract, _difference, _average,
    _multiply, _divide, _lighten, _darken, _screen, _overlay,
    _hardlight, _softlight, _dodge, _burn
  };

  const size_t blendMode = topSegment.blendMode < (sizeof(funcs) / sizeof(FuncType)) ? topSegment.blendMode : 0;
  const auto func  = funcs[blendMode]; // blendMode % (sizeof(funcs) / sizeof(FuncType))
  const auto blend = [&](uint32_t top, uint32_t bottom){ return RGBW32(func(R(top),R(bottom)), func(G(top),G(bottom)), func(B(top),B(bottom)), func(W(top),W(bottom))); };

  const int     length     = topSegment.length();     // physical segment length (counts all pixels in 2D segment)
  const int     width      = topSegment.width();
  const int     height     = topSegment.height();
  const auto    XY         = [](int x, int y){ return x + y*Segment::maxWidth; };
  const size_t  matrixSize = Segment::maxWidth * Segment::maxHeight;
  const size_t  startIndx  = XY(topSegment.start, topSegment.startY);
  const size_t  stopIndx   = startIndx + length;
  const unsigned progress  = topSegment.progress();
  const unsigned progInv   = 0xFFFFU - progress;
  uint8_t       opacity    = topSegment.currentBri(); // returns transitioned opacity for style FADE
  uint8_t       cct        = topSegment.currentCCT();

  Segment::setClippingRect(0, 0);             // disable clipping by default

  const unsigned dw = (blendingStyle==BLEND_STYLE_OUTSIDE_IN ? progInv : progress) * width / 0xFFFFU + 1;
  const unsigned dh = (blendingStyle==BLEND_STYLE_OUTSIDE_IN ? progInv : progress) * height / 0xFFFFU + 1;
  const unsigned orgBS = blendingStyle;
  if (width*height == 1) blendingStyle = BLEND_STYLE_FADE; // disable style for single pixel segments (use fade instead)
  switch (blendingStyle) {
    case BLEND_STYLE_CIRCULAR_IN: // (must set entire segment, see isPixelXYClipped())
    case BLEND_STYLE_CIRCULAR_OUT:// (must set entire segment, see isPixelXYClipped())
    case BLEND_STYLE_FAIRY_DUST:  // fairy dust (must set entire segment, see isPixelXYClipped())
      Segment::setClippingRect(0, width, 0, height);
      break;
    case BLEND_STYLE_SWIPE_RIGHT: // left-to-right
    case BLEND_STYLE_PUSH_RIGHT:  // left-to-right
      Segment::setClippingRect(0, dw, 0, height);
      break;
    case BLEND_STYLE_SWIPE_LEFT:  // right-to-left
    case BLEND_STYLE_PUSH_LEFT:   // right-to-left
      Segment::setClippingRect(width - dw, width, 0, height);
      break;
    case BLEND_STYLE_OUTSIDE_IN:   // corners
      Segment::setClippingRect((width + dw)/2, (width - dw)/2, (height + dh)/2, (height - dh)/2); // inverted!!
      break;
    case BLEND_STYLE_INSIDE_OUT:  // outward
      Segment::setClippingRect((width - dw)/2, (width + dw)/2, (height - dh)/2, (height + dh)/2);
      break;
    case BLEND_STYLE_SWIPE_DOWN:  // top-to-bottom (2D)
    case BLEND_STYLE_PUSH_DOWN:   // top-to-bottom (2D)
      Segment::setClippingRect(0, width, 0, dh);
      break;
    case BLEND_STYLE_SWIPE_UP:    // bottom-to-top (2D)
    case BLEND_STYLE_PUSH_UP:     // bottom-to-top (2D)
      Segment::setClippingRect(0, width, height - dh, height);
      break;
    case BLEND_STYLE_OPEN_H:      // horizontal-outward (2D) same look as INSIDE_OUT on 1D
      Segment::setClippingRect((width - dw)/2, (width + dw)/2, 0, height);
      break;
    case BLEND_STYLE_OPEN_V:      // vertical-outward (2D)
      Segment::setClippingRect(0, width, (height - dh)/2, (height + dh)/2);
      break;
    case BLEND_STYLE_SWIPE_TL:    // TL-to-BR (2D)
    case BLEND_STYLE_PUSH_TL:     // TL-to-BR (2D)
      Segment::setClippingRect(0, dw, 0, dh);
      break;
    case BLEND_STYLE_SWIPE_TR:    // TR-to-BL (2D)
    case BLEND_STYLE_PUSH_TR:     // TR-to-BL (2D)
      Segment::setClippingRect(width - dw, width, 0, dh);
      break;
    case BLEND_STYLE_SWIPE_BR:    // BR-to-TL (2D)
    case BLEND_STYLE_PUSH_BR:     // BR-to-TL (2D)
      Segment::setClippingRect(width - dw, width, height - dh, height);
      break;
    case BLEND_STYLE_SWIPE_BL:    // BL-to-TR (2D)
    case BLEND_STYLE_PUSH_BL:     // BL-to-TR (2D)
      Segment::setClippingRect(0, dw, height - dh, height);
      break;
  }

  if (isMatrix && stopIndx <= matrixSize) {
#ifndef WLED_DISABLE_2D
    const int nCols = topSegment.virtualWidth();
    const int nRows = topSegment.virtualHeight();
    const Segment *segO = topSegment.getOldSegment();
    const int oCols = segO ? segO->virtualWidth() : nCols;
    const int oRows = segO ? segO->virtualHeight() : nRows;

    const auto setMirroredPixel = [&](int x, int y, uint32_t c, uint8_t o) {
      const int baseX = topSegment.start  + x;
      const int baseY = topSegment.startY + y;
      size_t indx = XY(baseX, baseY); // absolute address on strip
      _pixels[indx] = color_blend(_pixels[indx], blend(c, _pixels[indx]), o);
      if (_pixelCCT) _pixelCCT[indx] = cct;
      // Apply mirroring
      if (topSegment.mirror || topSegment.mirror_y) {
        const int mirrorX = topSegment.start  + width  - x - 1;
        const int mirrorY = topSegment.startY + height - y - 1;
        const size_t idxMX = XY(topSegment.transpose ? baseX : mirrorX, topSegment.transpose ? mirrorY : baseY);
        const size_t idxMY = XY(topSegment.transpose ? mirrorX : baseX, topSegment.transpose ? baseY : mirrorY);
        const size_t idxMM = XY(mirrorX, mirrorY);
        if (topSegment.mirror)                        _pixels[idxMX] = color_blend(_pixels[idxMX], blend(c, _pixels[idxMX]), o);
        if (topSegment.mirror_y)                      _pixels[idxMY] = color_blend(_pixels[idxMY], blend(c, _pixels[idxMY]), o);
        if (topSegment.mirror && topSegment.mirror_y) _pixels[idxMM] = color_blend(_pixels[idxMM], blend(c, _pixels[idxMM]), o);
        if (_pixelCCT) {
          if (topSegment.mirror)                        _pixelCCT[idxMX] = cct;
          if (topSegment.mirror_y)                      _pixelCCT[idxMY] = cct;
          if (topSegment.mirror && topSegment.mirror_y) _pixelCCT[idxMM] = cct;
        }
      }
    };

    // if we blend using "push" style we need to "shift" canvas to left/right/up/down
    unsigned offsetX = (blendingStyle == BLEND_STYLE_PUSH_UP   || blendingStyle == BLEND_STYLE_PUSH_DOWN)  ? 0 : progInv * nCols / 0xFFFFU;
    unsigned offsetY = (blendingStyle == BLEND_STYLE_PUSH_LEFT || blendingStyle == BLEND_STYLE_PUSH_RIGHT) ? 0 : progInv * nRows / 0xFFFFU;

    // we only traverse new segment, not old one
    for (int r = 0; r < nRows; r++) for (int c = 0; c < nCols; c++) {
      const bool clipped = topSegment.isPixelXYClipped(c, r);
      // if segment is in transition and pixel is clipped take old segment's pixel and opacity
      const Segment *seg = clipped && segO ? segO : &topSegment;  // pixel is never clipped for FADE
      int vCols = seg == segO ? oCols : nCols;         // old segment may have different dimensions
      int vRows = seg == segO ? oRows : nRows;         // old segment may have different dimensions
      int x = c;
      int y = r;
      // if we blend using "push" style we need to "shift" canvas to left/right/up/down
      switch (blendingStyle) {
        case BLEND_STYLE_PUSH_RIGHT: x = (x + offsetX) % nCols;         break;
        case BLEND_STYLE_PUSH_LEFT:  x = (x - offsetX + nCols) % nCols; break;
        case BLEND_STYLE_PUSH_DOWN:  y = (y + offsetY) % nRows;         break;
        case BLEND_STYLE_PUSH_UP:    y = (y - offsetY + nRows) % nRows; break;
      }
      uint32_t c_a = BLACK;
      if (x < vCols && y < vRows) c_a = seg->getPixelColorRaw(x + y*vCols); // will get clipped pixel from old segment or unclipped pixel from new segment
      if (segO && blendingStyle == BLEND_STYLE_FADE
        && (topSegment.mode != segO->mode || (segO->name != topSegment.name && segO->name && topSegment.name && strncmp(segO->name, topSegment.name, WLED_MAX_SEGNAME_LEN) != 0))
        && x < oCols && y < oRows) {
        // we need to blend old segment using fade as pixels are not clipped
        c_a = color_blend16(c_a, segO->getPixelColorRaw(x + y*oCols), progInv);
      } else if (blendingStyle != BLEND_STYLE_FADE) {
        // workaround for On/Off transition
        // (bri != briT) && !bri => from On to Off
        // (bri != briT) &&  bri => from Off to On
        if ((!clipped && (bri != briT) && !bri) || (clipped && (bri != briT) && bri)) c_a = BLACK;
      }
      // map it into frame buffer
      x = c;  // restore coordiates if we were PUSHing
      y = r;
      if (topSegment.reverse  ) x = nCols - x - 1;
      if (topSegment.reverse_y) y = nRows - y - 1;
      if (topSegment.transpose) std::swap(x,y); // swap X & Y if segment transposed
      // expand pixel
      const unsigned groupLen = topSegment.groupLength();
      if (groupLen == 1) {
        setMirroredPixel(x, y, c_a, opacity);
      } else {
        // handle grouping and spacing
        x *= groupLen; // expand to physical pixels
        y *= groupLen; // expand to physical pixels
        const int maxX = std::min(x + topSegment.grouping, width);
        const int maxY = std::min(y + topSegment.grouping, height);
        while (y < maxY) {
          int _x = x;
          while (_x < maxX) setMirroredPixel(_x++, y, c_a, opacity);
          y++;
        }
      }
    }
#endif
  } else {
    const int nLen = topSegment.virtualLength();
    const Segment *segO = topSegment.getOldSegment();
    const int oLen = segO ? segO->virtualLength() : nLen;

    const auto setMirroredPixel = [&](int i, uint32_t c, uint8_t o) {
      int indx = topSegment.start + i;
      // Apply mirroring
      if (topSegment.mirror) {
        unsigned indxM = topSegment.stop - i - 1;
        indxM += topSegment.offset; // offset/phase
        if (indxM >= topSegment.stop) indxM -= length; // wrap
        _pixels[indxM] = color_blend(_pixels[indxM], blend(c, _pixels[indxM]), o);
        if (_pixelCCT) _pixelCCT[indxM] = cct;
      }
      indx += topSegment.offset; // offset/phase
      if (indx >= topSegment.stop) indx -= length; // wrap
      _pixels[indx] = color_blend(_pixels[indx], blend(c, _pixels[indx]), o);
      if (_pixelCCT) _pixelCCT[indx] = cct;
    };

    // if we blend using "push" style we need to "shift" canvas to left/right/
    unsigned offsetI = progInv * nLen / 0xFFFFU;

    for (int k = 0; k < nLen; k++) {
      const bool clipped = topSegment.isPixelClipped(k);
      // if segment is in transition and pixel is clipped take old segment's pixel and opacity
      const Segment *seg = clipped && segO ? segO : &topSegment;  // pixel is never clipped for FADE
      const int vLen = seg == segO ? oLen : nLen;
      int i = k;
      // if we blend using "push" style we need to "shift" canvas to left or right
      switch (blendingStyle) {
        case BLEND_STYLE_PUSH_RIGHT: i = (i + offsetI) % nLen;        break;
        case BLEND_STYLE_PUSH_LEFT:  i = (i - offsetI + nLen) % nLen; break;
      }
      uint32_t c_a = BLACK;
      if (i < vLen) c_a = seg->getPixelColorRaw(i); // will get clipped pixel from old segment or unclipped pixel from new segment
      if (segO && blendingStyle == BLEND_STYLE_FADE && topSegment.mode != segO->mode && i < oLen) {
        // we need to blend old segment using fade as pixels are not clipped
        c_a = color_blend16(c_a, segO->getPixelColorRaw(i), progInv);
      } else if (blendingStyle != BLEND_STYLE_FADE) {
        // workaround for On/Off transition
        // (bri != briT) && !bri => from On to Off
        // (bri != briT) &&  bri => from Off to On
        if ((!clipped && (bri != briT) && !bri) || (clipped && (bri != briT) && bri)) c_a = BLACK;
      }
      // map into frame buffer
      i = k; // restore index if we were PUSHing
      if (topSegment.reverse) i = nLen - i - 1; // is segment reversed?
      // expand pixel
      i *= topSegment.groupLength();
      // set all the pixels in the group
      const int maxI = std::min(i + topSegment.grouping, length); // make sure to not go beyond physical length
      while (i < maxI) setMirroredPixel(i++, c_a, opacity);
    }
  }

  blendingStyle = orgBS;
  Segment::setClippingRect(0, 0);             // disable clipping for overlays
}

void WS2812FX::show() {
  if (!_pixels) {
    DEBUGFX_PRINTLN(F("Error: no _pixels!"));
    errorFlag = ERR_NORAM;
    return; // no pixels allocated, nothing to show
  }

  unsigned long showNow = millis();
  size_t diff = showNow - _lastShow;

  size_t totalLen = getLengthTotal();
  // WARNING: as WLED doesn't handle CCT on pixel level but on Segment level instead
  // we need to keep track of each pixel's CCT when blending segments (if CCT is present)
  // and then set appropriate CCT from that pixel during paint (see below).
  if ((hasCCTBus() || correctWB) && !cctFromRgb)
    _pixelCCT = static_cast<uint8_t*>(allocate_buffer(totalLen * sizeof(uint8_t), BFRALLOC_PREFER_PSRAM)); // allocate CCT buffer if necessary, prefer PSRAM
  if (_pixelCCT) memset(_pixelCCT, 127, totalLen); // set neutral (50:50) CCT

  if (realtimeMode == REALTIME_MODE_INACTIVE || useMainSegmentOnly || realtimeOverride > REALTIME_OVERRIDE_NONE) {
    // clear frame buffer
    for (size_t i = 0; i < totalLen; i++) _pixels[i] = BLACK; // memset(_pixels, 0, sizeof(uint32_t) * getLengthTotal());
    // blend all segments into (cleared) buffer
    for (Segment &seg : _segments) if (seg.isActive() && (seg.on || seg.isInTransition())) {
      blendSegment(seg);              // blend segment's buffer into frame buffer
    }
  }

  // avoid race condition, capture _callback value
  show_callback callback = _callback;
  if (callback) callback(); // will call setPixelColor or setRealtimePixelColor

  // paint actual pixels
  int oldCCT = Bus::getCCT(); // store original CCT value (since it is global)
  // when cctFromRgb is true we implicitly calculate WW and CW from RGB values (cct==-1)
  if (cctFromRgb) BusManager::setSegmentCCT(-1);
  for (size_t i = 0; i < totalLen; i++) {
    // when correctWB is true setSegmentCCT() will convert CCT into K with which we can then
    // correct/adjust RGB value according to desired CCT value, it will still affect actual WW/CW ratio
    if (_pixelCCT) { // cctFromRgb already exluded at allocation
      if (i == 0 || _pixelCCT[i-1] != _pixelCCT[i]) BusManager::setSegmentCCT(_pixelCCT[i], correctWB);
    }

    uint32_t c = _pixels[i]; // need a copy, do not modify _pixels directly (no byte access allowed on ESP32)
    if(c > 0 && !(realtimeMode && arlsDisableGammaCorrection))
        c = gamma32(c); // apply gamma correction if enabled note: applying gamma after brightness has too much color loss
    BusManager::setPixelColor(getMappedPixelIndex(i), c);
  }
  Bus::setCCT(oldCCT);  // restore old CCT for ABL adjustments

  p_free(_pixelCCT);
  _pixelCCT = nullptr;

  // some buses send asynchronously and this method will return before
  // all of the data has been sent.
  // See https://github.com/Makuna/NeoPixelBus/wiki/ESP32-NeoMethods#neoesp32rmt-methods
  BusManager::show();

  if (diff > 0) { // skip calculation if no time has passed
    size_t fpsCurr = (1000 << FPS_CALC_SHIFT) / diff; // fixed point math
    _cumulativeFps = (FPS_CALC_AVG * _cumulativeFps + fpsCurr + FPS_CALC_AVG / 2) / (FPS_CALC_AVG + 1);   // "+FPS_CALC_AVG/2" for proper rounding
    _lastShow = showNow;
  }
}

void WS2812FX::setRealtimePixelColor(unsigned i, uint32_t c) {
  if (useMainSegmentOnly) {
    const Segment &seg = getMainSegment();
    if (seg.isActive() && i < seg.length()) seg.setPixelColorRaw(i, c);
  } else {
    setPixelColor(i, c);
  }
}

// reset all segments
void WS2812FX::restartRuntime() {
  suspend();
  waitForIt();
  for (Segment &seg : _segments) seg.markForReset().resetIfRequired();
  resume();
}

// start or stop transition for all segments
void WS2812FX::setTransitionMode(bool t) {
  suspend();
  waitForIt();
  for (Segment &seg : _segments) seg.startTransition(t ? _transitionDur : 0);
  resume();
}

// wait until frame is over (service() has finished or time for 2 frames have passed; yield() crashes on 8266)
// the latter may, in rare circumstances, lead to incorrectly assuming strip is done servicing but will not block
// other processing "indefinitely"
// rare circumstances are: setting FPS to high number (i.e. 120) and have very slow effect that will need more
// time than 2 * _frametime (1000/FPS) to draw content
void WS2812FX::waitForIt() {
  unsigned long waitStart = millis();
  unsigned long maxWait = 2*getFrameTime() + 100; // TODO: this needs a proper fix for timeout! see #4779
  while (isServicing() && (millis() - waitStart < maxWait)) delay(1); // safe even when millis() rolls over
  #ifdef WLED_DEBUG
  if (millis()-waitStart >= maxWait) DEBUG_PRINTLN(F("Waited for strip to finish servicing."));
  #endif
};

void WS2812FX::setTargetFps(unsigned fps) {
  if (fps <= 250) _targetFps = fps;
  if (_targetFps > 0) _frametime = 1000 / _targetFps;
  else _frametime = MIN_FRAME_DELAY;     // unlimited mode
}

void WS2812FX::setCCT(uint16_t k) {
  for (Segment &seg : _segments) {
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
    for (const Segment &seg : _segments) seg.freeze = false; // freeze is mutable
  }
  BusManager::setBrightness(scaledBri(b));
  if (!direct) {
    unsigned long t = millis();
    if (_segments[0].next_time > t + 22 && t - _lastShow > MIN_SHOW_DELAY) trigger(); //apply brightness change immediately if no refresh soon
  }
}

uint8_t WS2812FX::getActiveSegsLightCapabilities(bool selectedOnly) const {
  uint8_t totalLC = 0;
  for (const Segment &seg : _segments) {
    if (seg.isActive() && (!selectedOnly || seg.isSelected())) totalLC |= seg.getLightCapabilities();
  }
  return totalLC;
}

uint8_t WS2812FX::getFirstSelectedSegId() const {
  size_t i = 0;
  for (const Segment &seg : _segments) {
    if (seg.isActive() && seg.isSelected()) return i;
    i++;
  }
  // if none selected, use the main segment
  return getMainSegmentId();
}

void WS2812FX::setMainSegmentId(unsigned n) {
  _mainSegment = getLastActiveSegmentId();
  if (n < _segments.size() && _segments[n].isActive()) {  // only set if segment is active
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
  unsigned c = 0;
  for (const Segment &seg : _segments) if (seg.isActive()) c++;
  return c;
}

uint16_t WS2812FX::getLengthTotal() const {
  unsigned len = Segment::maxWidth * Segment::maxHeight; // will be _length for 1D (see finalizeInit()) but should cover whole matrix for 2D
  if (isMatrix && _length > len) len = _length; // for 2D with trailing strip
  return len;
}

uint16_t WS2812FX::getLengthPhysical() const {
  return BusManager::getTotalLength(true);
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

// WARNING: resetSegments(), makeAutoSegments() and fixInvalidSegments() must not be called while
// strip is being serviced (strip.service()), you must call suspend prior if changing segments outside
// loop() context
void WS2812FX::resetSegments() {
  if (isServicing()) return;
  _segments.clear();          // destructs all Segment as part of clearing
  _segments.emplace_back(0, isMatrix ? Segment::maxWidth : _length, 0, isMatrix ? Segment::maxHeight : 1);
  _segments.shrink_to_fit();  // just in case ...
  _mainSegment = 0;
}

void WS2812FX::makeAutoSegments(bool forceReset) {
  if (isServicing()) return;
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
      _segments.emplace_back(0, Segment::maxWidth, 0, Segment::maxHeight);
    else
    #endif
      _segments.emplace_back(segStarts[0], segStops[0]);
    for (size_t i = 1; i < s; i++) {
      _segments.emplace_back(segStarts[i], segStops[i]);
    }
    DEBUGFX_PRINTF_P(PSTR("%d auto segments created.\n"), _segments.size());

  } else {

    if (forceReset || getSegmentsNum() == 0) resetSegments();
    //expand the main seg to the entire length, but only if there are no other segments, or reset is forced
    else if (getActiveSegmentsNum() == 1) {
      size_t i = getLastActiveSegmentId();
      #ifndef WLED_DISABLE_2D
      _segments[i].setGeometry(0, Segment::maxWidth, 1, 0, 0xFFFF, 0, Segment::maxHeight);
      #else
      _segments[i].setGeometry(0, _length);
      #endif
    }
  }
  _mainSegment = 0;

  fixInvalidSegments();
}

void WS2812FX::fixInvalidSegments() {
  if (isServicing()) return;
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
  for (const Segment &seg : _segments)
    seg.refreshLightCapabilities();
}

//true if all segments align with a bus, or if a segment covers the total length
//irrelevant in 2D set-up
bool WS2812FX::checkSegmentAlignment() const {
  bool aligned = false;
  for (const Segment &seg : _segments) {
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

// load custom mapping table from JSON file (called from finalizeInit() or deserializeState())
// if this is a matrix set-up and default ledmap.json file does not exist, create mapping table using setUpMatrix() from panel information
// WARNING: effect drawing has to be suspended (strip.suspend()) or must be called from loop() context
bool WS2812FX::deserializeMap(unsigned n) {
  char fileName[32];
  strcpy_P(fileName, PSTR("/ledmap"));
  if (n) sprintf(fileName +7, "%d", n);
  strcat_P(fileName, PSTR(".json"));
  bool isFile = WLED_FS.exists(fileName);

  customMappingSize = 0; // prevent use of mapping if anything goes wrong
  currentLedmap = 0;
  if (n == 0 || isFile) interfaceUpdateCallMode = CALL_MODE_WS_SEND; // schedule WS update (to inform UI)

  if (!isFile && n==0 && isMatrix) {
    // 2D panel support creates its own ledmap (on the fly) if a ledmap.json does not exist
    setUpMatrix();
    return false;
  }

  if (!isFile || !requestJSONBufferLock(7)) return false;

  StaticJsonDocument<64> filter;
  filter[F("width")]  = true;
  filter[F("height")] = true;
  if (!readObjectFromFile(fileName, nullptr, pDoc, &filter)) {
    DEBUG_PRINTF_P(PSTR("ERROR Invalid ledmap in %s\n"), fileName);
    releaseJSONBufferLock();
    return false; // if file does not load properly then exit
  } else
    DEBUG_PRINTF_P(PSTR("Reading LED map from %s\n"), fileName);

  JsonObject root = pDoc->as<JsonObject>();
  // if we are loading default ledmap (at boot) set matrix width and height from the ledmap (compatible with WLED MM ledmaps)
  if (n == 0 && (!root[F("width")].isNull() || !root[F("height")].isNull())) {
    Segment::maxWidth  = min(max(root[F("width")].as<int>(), 1), 255);
    Segment::maxHeight = min(max(root[F("height")].as<int>(), 1), 255);
    isMatrix = true;
    DEBUG_PRINTF_P(PSTR("LED map width=%d, height=%d\n"), Segment::maxWidth, Segment::maxHeight);
  }

  d_free(customMappingTable);
  customMappingTable = static_cast<uint16_t*>(d_malloc(sizeof(uint16_t)*getLengthTotal())); // prefer DRAM for speed

  if (customMappingTable) {
    DEBUG_PRINTF_P(PSTR("ledmap allocated: %uB\n"), sizeof(uint16_t)*getLengthTotal());
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
        if (index < 0 || index > 65535) index = 0xFFFF; // prevent integer wrap around
        customMappingTable[customMappingSize++] = index;
        if (customMappingSize >= getLengthTotal()) break;
      } else break; // there was nothing to read, stop
    }
    currentLedmap = n;
    f.close();

    #ifdef WLED_DEBUG
    DEBUG_PRINT(F("Loaded ledmap:"));
    for (unsigned i=0; i<customMappingSize; i++) {
      if (!(i%Segment::maxWidth)) DEBUG_PRINTLN();
      DEBUG_PRINTF_P(PSTR("%4d,"), customMappingTable[i] < 0xFFFFU ? customMappingTable[i] : -1);
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

  releaseJSONBufferLock();
  return (customMappingSize > 0);
}


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
