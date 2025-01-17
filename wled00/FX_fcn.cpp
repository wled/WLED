/*
  WS2812FX_fcn.cpp contains all utility functions
  Harm Aldick - 2016
  www.aldick.org
  Modified heavily for WLED
*/
#include "wled.h"
#include "FX.h"
#include "palettes.h"
#ifdef ARDUINO_ARCH_ESP32
#include <esp_timer.h>     // WLEDMM to get esp_timer_get_time() 
#endif

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

// WLEDMM experimental . this is a "C style" wrapper for strip.waitUntilIdle()
// This workaround is just needed for the segment class, that does't know about "strip"
void strip_wait_until_idle(String whoCalledMe) {
#if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_PROTECT_SERVICE)  // WLEDMM experimental 
  if (strip.isServicing() && (strncmp(pcTaskGetTaskName(NULL), "loopTask", 8) != 0)) { // if we are in looptask (arduino loop), its safe to proceed without waiting
  USER_PRINTLN(whoCalledMe + String(": strip is still drawing effects."));
  strip.waitUntilIdle();
  }
#endif
}
// WLEDMM another helper for segment class
bool strip_uses_global_leds(void) {
  return strip.useLedsArray;
}

///////////////////////////////////////////////////////////////////////////////
// Segment class implementation
///////////////////////////////////////////////////////////////////////////////
size_t Segment::_usedSegmentData = 0U; // amount of RAM all segments use for their data[]
CRGB    *Segment::_globalLeds = nullptr;
uint16_t Segment::maxWidth = DEFAULT_LED_COUNT;
uint16_t Segment::maxHeight = 1;

CRGBPalette16 Segment::_currentPalette    = CRGBPalette16(CRGB::Black);

// copy constructor - creates a new segment by copy from orig, but does not copy buffers. Does not modify orig!
Segment::Segment(const Segment &orig) {
  DEBUG_PRINTLN(F("-- Copy segment constructor --"));
  memcpy((void*)this, (void*)&orig, sizeof(Segment)); //WLEDMM copy to this
  transitional = false; // copied segment cannot be in transition
  // WLEDMM temporarily prevent any fast draw calls to the new segment
  // _isValid2D = false;
  _isSimpleSegment = false;
  _isSuperSimpleSegment = false;

  name = nullptr;
  data = nullptr;
  _dataLen = 0;
  _t = nullptr;
  if (ledsrgb && !Segment::_globalLeds) {ledsrgb = nullptr; ledsrgbSize = 0;}  // WLEDMM
  if (orig.name) { name = new(std::nothrow) char[strlen(orig.name)+1]; if (name) strcpy(name, orig.name); }
  if (orig.data) { if (allocateData(orig._dataLen)) memcpy(data, orig.data, orig._dataLen); }
  //if (orig._t)   { _t = new(std::nothrow) Transition(orig._t->_dur, orig._t->_briT, orig._t->_cctT, orig._t->_colorT); }
  //else markForReset(); // WLEDMM
  // if (orig.ledsrgb && !Segment::_globalLeds) { allocLeds(); if (ledsrgb) memcpy(ledsrgb, orig.ledsrgb, sizeof(CRGB)*length()); } // WLEDMM
  jMap = nullptr; //WLEDMM jMap
}

//WLEDMM: recreate ledsrgb if more space needed (will not free ledsrgb!)
void Segment::allocLeds() {
  size_t size = sizeof(CRGB)*max((size_t) length(), ledmapMaxSize); // TroyHacks
  if ((size < sizeof(CRGB)) || (size > 164000)) {                   //softhack too small (<3) or too large (>160Kb)
    DEBUG_PRINTF("allocLeds warning: size == %u !!\n", size);
    if (ledsrgb && (ledsrgbSize == 0)) {
      USER_PRINTLN("allocLeds warning: ledsrgbSize == 0 but ledsrgb!=NULL");
      free(ledsrgb); ledsrgb=nullptr;
    } // softhack007 clean up buffer
  }
  if ((size > 0) && (!ledsrgb || size > ledsrgbSize)) {    //softhack dont allocate zero bytes
    USER_PRINTF("allocLeds (%d,%d to %d,%d), %u from %u\n", start, startY, stop, stopY, size, ledsrgb?ledsrgbSize:0);
    if (ledsrgb) free(ledsrgb);   // we need a bigger buffer, so free the old one first
    ledsrgb = (CRGB*)calloc(size, 1);
    ledsrgbSize = ledsrgb?size:0;
    if (ledsrgb == nullptr) {
      USER_PRINTLN("allocLeds failed!!");
      errorFlag = ERR_LOW_BUF; // WLEDMM raise errorflag
    }
  }
  else {
    //USER_PRINTF("reuse Leds %u from %u\n", size, ledsrgb?ledsrgbSize:0);
  }
}

// move constructor --> moves everything (including buffer) from orig to this
Segment::Segment(Segment &&orig) noexcept {
  DEBUG_PRINTLN(F("-- Move segment constructor --"));

  // WLEDMM temporarily prevent any fast draw calls to old and new segment
  orig._isSimpleSegment = false;
  orig._isSuperSimpleSegment = false;

  memcpy((void*)this, (void*)&orig, sizeof(Segment));
  orig.transitional = false; // old segment cannot be in transition any more
#ifdef WLEDMM_FASTPATH
  // WLEDMM prevent any draw calls to old segment
  orig._isValid2D = false;
#endif
  orig.name = nullptr;
  orig.data = nullptr;
  orig._dataLen = 0;
  orig._t   = nullptr;
  orig.ledsrgb = nullptr; //WLEDMM
  orig.ledsrgbSize = 0;   // WLEDMM
  orig.jMap = nullptr;    //WLEDMM jMap
}

// copy assignment --> overwrite segment with orig - deletes old buffers in "this", but does not change orig!
Segment& Segment::operator= (const Segment &orig) {
  DEBUG_PRINTLN(F("-- Copy-assignment segment --"));
  if (this != &orig) {
    // clean destination
    transitional = false; // copied segment cannot be in transition
    if (name) delete[] name;
    if (_t)   delete _t;
    CRGB* oldLeds = ledsrgb;
    size_t oldLedsSize = ledsrgbSize;
    if (ledsrgb && !Segment::_globalLeds) free(ledsrgb);
    deallocateData();
    // copy source
    memcpy((void*)this, (void*)&orig, sizeof(Segment));
    transitional = false;

    // WLEDMM prevent any fast draw calls to this segment until the next frame starts
    //_isValid2D = false;
    _isSimpleSegment = false;
    _isSuperSimpleSegment = false;

    // erase pointers to allocated data
    name = nullptr;
    data = nullptr;
    _dataLen = 0;
    _t = nullptr;
    //if (!Segment::_globalLeds) {ledsrgb = oldLeds; ledsrgbSize = oldLedsSize;}; // WLEDMM reuse leds instead of ledsrgb = nullptr;
    if (!Segment::_globalLeds) {ledsrgb = nullptr; ledsrgbSize = 0;};             // WLEDMM copy has no buffers (yet)
    // copy source data
    if (orig.name) { name = new(std::nothrow) char[strlen(orig.name)+1]; if (name) strcpy(name, orig.name); }
    if (orig.data) { if (allocateData(orig._dataLen)) memcpy(data, orig.data, orig._dataLen); }
    //if (orig._t)   { _t = new(std::nothrow) Transition(orig._t->_dur, orig._t->_briT, orig._t->_cctT, orig._t->_colorT); }
    //else markForReset(); // WLEDMM
    //if (orig.ledsrgb && !Segment::_globalLeds) { allocLeds(); if (ledsrgb) memcpy(ledsrgb, orig.ledsrgb, sizeof(CRGB)*length()); } // WLEDMM don't copy old buffer
    jMap = nullptr; //WLEDMM jMap
  }
  return *this;
}

// move assignment --> moves everything (including buffers) from "orig" to "this"
Segment& Segment::operator= (Segment &&orig) noexcept {
  DEBUG_PRINTLN(F("-- Move-assignment segment --"));
  if (this != &orig) {
    transitional = false; // just temporary
    if (name) { delete[] name; name = nullptr; } // free old name
    deallocateData(); // free old runtime data
    if (_t) { delete _t; _t = nullptr; }
    if (ledsrgb && !Segment::_globalLeds) free(ledsrgb); //WLEDMM: not needed anymore as we will use leds from copy. no need to nullify ledsrgb as it gets new value in memcpy

    // WLEDMM temporarily prevent any fast draw calls to old and new segment
    orig._isSimpleSegment = false;
    orig._isSuperSimpleSegment = false;

    memcpy((void*)this, (void*)&orig, sizeof(Segment));
#ifdef WLEDMM_FASTPATH
    // WLEDMM temporarily prevent any draw calls to old segment
    orig._isValid2D = false;
#endif
    orig.name = nullptr;
    orig.data = nullptr;
    orig._dataLen = 0;
    orig._t   = nullptr;
    orig.ledsrgb = nullptr;  //WLEDMM: do not free as moved to here
    orig.ledsrgbSize = 0;    //WLEDMM
    orig.jMap = nullptr; //WLEDMM jMap
  }
  return *this;
}

bool Segment::allocateData(size_t len) {
  // WLEDMM
  if (data && _dataLen >= len) {                          // already allocated enough (reduce fragmentation)
    if ((call == 0) && (len > 0)) memset(data, 0, len);   // erase buffer if called during effect initialisation
    return true;
  }
  //DEBUG_PRINTF("allocateData(%u) start %d, stop %d, vlen %d\n", len, start, stop, virtualLength());
  deallocateData();
  if (len == 0) return false; // nothing to do
  if (Segment::getUsedSegmentData() + len > MAX_SEGMENT_DATA) {
    //USER_PRINTF("Segment::allocateData: Segment data quota exceeded! used:%u request:%u max:%d\n", Segment::getUsedSegmentData(), len, MAX_SEGMENT_DATA);
    if (len > 0) errorFlag = ERR_LOW_SEG_MEM;  // WLEDMM raise errorflag
    return false; //not enough memory
  }
  // do not use SPI RAM on ESP32 since it is slow
  //#if defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM) && defined(WLED_USE_PSRAM)
  //if (psramFound())
  //  data = (byte*) ps_malloc(len);
  //else
  //#endif
    data = (byte*) malloc(len);
  if (!data) {
      _dataLen = 0; // WLEDMM reset dataLen
      errorFlag = ERR_LOW_MEM; // WLEDMM raise errorflag
      USER_PRINT(F("Segment::allocateData: FAILED to allocate ")); 
      USER_PRINT(len); USER_PRINTLN(F(" bytes."));
      return false;
  } //allocation failed
  Segment::addUsedSegmentData(len);
  _dataLen = len;
  memset(data, 0, len);
  if (errorFlag == ERR_LOW_SEG_MEM) errorFlag = ERR_NONE; // WLEDMM reset errorflag on success
  return true;
}

void Segment::deallocateData() {
  if (!data) {_dataLen = 0; return;}  // WLEDMM reset dataLen
  free(data);
  data = nullptr;
  //USER_PRINTF("Segment::deallocateData: free'd   %d bytes.\n", _dataLen);
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
    if (ledsrgb && !Segment::_globalLeds) { free(ledsrgb); ledsrgb = nullptr; ledsrgbSize=0;} // WLEDMM segment has changed, so we need a fresh buffer.
    if (transitional && _t) { transitional = false; delete _t; _t = nullptr; }
    deallocateData();
    next_time = 0; step = 0; call = 0; aux0 = 0; aux1 = 0;
    reset = false; // setOption(SEG_OPTION_RESET, false);
    startFrame();   // WLEDMM update cached propoerties
    if (isActive() && !freeze) { fill(BLACK); needsBlank = false; } // WLEDMM start clean
    DEBUG_PRINTLN("Segment reset");
  } else if (needsBlank) {
    startFrame();   // WLEDMM update cached propoerties
    if (isActive() && !freeze) {
      fill(BLACK); // WLEDMM start clean
      DEBUG_PRINTLN("Segment blanked");
      needsBlank = false;
    }
  }
}

void Segment::setUpLeds() {
  // deallocation happens in resetIfRequired() as it is called when segment changes or in destructor
  if (Segment::_globalLeds) {
    #ifndef WLED_DISABLE_2D
    ledsrgb = &Segment::_globalLeds[start + startY*Segment::maxWidth];
    ledsrgbSize = length() * sizeof(CRGB); // also set this when using global leds.
    //USER_PRINTF("\nsetUpLeds() Global LEDs: startX=%d stopx=%d startY=%d stopy=%d maxwidth=%d; length=%d, size=%d\n\n", start, stop, startY, stopY, Segment::maxWidth, length(), ledsrgbSize/3);
    #else
    ledsrgb = &Segment::_globalLeds[start];
    ledsrgbSize = length() * sizeof(CRGB); // also set this when using global leds.
    #endif
  } else if (length() > 0) { //WLEDMM we always want a new buffer //softhack007 quickfix - avoid malloc(0) which is undefined behaviour (should not happen, but i've seen it)
    //#if defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM) && defined(WLED_USE_PSRAM)
    //if (psramFound())
    //  ledsrgb = (CRGB*)ps_malloc(sizeof(CRGB)*length()); // softhack007 disabled; putting leds into psram leads to horrible slowdown on WROVER boards
    //else
    //#endif
    allocLeds(); //WLEDMM
    //USER_PRINTF("\nsetUpLeds() local LEDs: startX=%d stopx=%d startY=%d stopy=%d maxwidth=%d; length=%d, size=%d\n\n", start, stop, startY, stopY, Segment::maxWidth, length(), ledsrgbSize/3);
  }
}

CRGBPalette16 &Segment::loadPalette(CRGBPalette16 &targetPalette, uint8_t pal) const {
  static unsigned long _lastPaletteChange = millis() - 990000; // perhaps it should be per segment //WLEDMM changed init value to avoid pure orange after startup
  static CRGBPalette16 randomPalette = CRGBPalette16(DEFAULT_COLOR);
  static CRGBPalette16 prevRandomPalette = CRGBPalette16(CRGB(BLACK));
  byte tcp[76] = { 255 };   //WLEDMM: prevent out-of-range access in loadDynamicGradientPalette()
  if (pal < 245 && pal > GRADIENT_PALETTE_COUNT+13) pal = 0;
  if (pal > 245 && (strip.customPalettes.size() == 0 || 255U-pal > strip.customPalettes.size()-1)) pal = 0; // TODO remove strip dependency by moving customPalettes out of strip
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
    case 1: {//Random smooth: periodically replace palette with a random one. Transition palette change in 500ms
      uint32_t timeSinceLastChange = millis() - _lastPaletteChange;
      if (timeSinceLastChange > randomPaletteChangeTime * 1000U) {
        prevRandomPalette = randomPalette;
        randomPalette = CRGBPalette16(
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)));
        _lastPaletteChange = millis();
        timeSinceLastChange = 0;
      }

      //WLEDMM: smooth transitions of palettes instead of every 5 sec with short transition
      for (int i=0; i< 16; i++) {
        targetPalette[i].r = prevRandomPalette[i].r*(5000-timeSinceLastChange)/5000 + randomPalette[i].r*timeSinceLastChange/5000;
        targetPalette[i].g = prevRandomPalette[i].g*(5000-timeSinceLastChange)/5000 + randomPalette[i].g*timeSinceLastChange/5000;
        targetPalette[i].b = prevRandomPalette[i].b*(5000-timeSinceLastChange)/5000 + randomPalette[i].b*timeSinceLastChange/5000;
      }
      break;}
    case 74: {//periodically replace palette with a random one. Transition palette change in 500ms
      uint32_t timeSinceLastChange = millis() - _lastPaletteChange;
      if (timeSinceLastChange > randomPaletteChangeTime * 1000U) {
        prevRandomPalette = randomPalette;
        randomPalette = CRGBPalette16(
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)),
                        CHSV(random8(), random8(160, 255), random8(128, 255)));
        _lastPaletteChange = millis();
        timeSinceLastChange = 0;
      }
      if (timeSinceLastChange <= 250) {
        targetPalette = prevRandomPalette;
        // there needs to be 255 palette blends (48) for full blend but that is too resource intensive
        // so 128 is a compromise (we need to perform full blend of the two palettes as each segment can have random
        // palette selected but only 2 static palettes are used)
        size_t noOfBlends = ((128U * timeSinceLastChange) / 250U);
        for (size_t i=0; i<noOfBlends; i++) nblendPaletteTowardPalette(targetPalette, randomPalette, 48);
      } else {
        targetPalette = randomPalette;
      }
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
    case 71: //WLEDMM netmindz ar palette +1
    case 72: //WLEDMM netmindz ar palette +2
    case 73: //WLEDMM netmindz ar palette +3
        targetPalette.loadDynamicGradientPalette(getAudioPalette(pal)); break; 
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
  CRGBPalette16 _palT = CRGBPalette16(DEFAULT_COLOR); loadPalette(_palT, palette);
  uint8_t _modeP = mode;
  uint32_t _colorT[NUM_COLORS];
  for (size_t i=0; i<NUM_COLORS; i++) _colorT[i] = currentColor(i, colors[i]);

  if (!_t) _t = new(std::nothrow) Transition(dur); // no previous transition running
  if (!_t) return; // failed to allocate data
  _t->_briT  = _briT;
  _t->_cctT  = _cctT;
  _t->_palT  = _palT;
  _t->_modeP = _modeP;
  for (size_t i=0; i<NUM_COLORS; i++) _t->_colorT[i] = _colorT[i];
  transitional = true; // setOption(SEG_OPTION_TRANSITIONAL, true);
}

// WLEDMM Segment::progress() is declared inline, see FX.h
#if 0
// transition progression between 0-65535
uint16_t IRAM_ATTR_YN Segment::progress() const {
  if (!transitional || !_t) return 0xFFFFU;
  unsigned long timeNow = millis();
  if (timeNow - _t->_start > _t->_dur || _t->_dur == 0) return 0xFFFFU;
  return (timeNow - _t->_start) * 0xFFFFU / _t->_dur;
}
#endif

// WLEDMM Segment::currentBri() is declared inline, see FX.h
#if 0
uint8_t IRAM_ATTR_YN Segment::currentBri(uint8_t briNew, bool useCct) const {
  uint32_t prog = (transitional && _t) ? progress() : 0xFFFFU;
  if (transitional && _t && prog < 0xFFFFU) {
    if (useCct) return ((briNew * prog) + _t->_cctT * (0xFFFFU - prog)) >> 16;
    else        return ((briNew * prog) + _t->_briT * (0xFFFFU - prog)) >> 16;
  } else {
    return (useCct ? briNew : (on ? briNew : 0));  // WLEDMM aligned with upstream
  }
}
#endif

uint8_t Segment::currentMode(uint8_t newMode) {
  return (progress()>32767U) ? newMode : (_t ? _t->_modeP : newMode); // change effect in the middle of transition
}

uint32_t Segment::currentColor(uint8_t slot, uint32_t colorNew) {
  return transitional && _t ? color_blend(_t->_colorT[slot], colorNew, progress(), true) : colorNew;
}

void Segment::setCurrentPalette() {
  loadPalette(_currentPalette, palette);
  if (transitional && _t && progress() < 0xFFFFU) {
    // blend palettes
    // there are about 255 blend passes of 48 "blends" to completely blend two palettes (in _dur time)
    // minimum blend time is 100ms maximum is 65535ms
    unsigned long timeMS = millis() - _t->_start;
    uint16_t noOfBlends = min(64UL, (255U * timeMS / _t->_dur) - _t->_prevPaletteBlends);  // WLEDMM limit to 64 blends at once, prevent rollover
    for (unsigned i = 0; i < noOfBlends; i++, _t->_prevPaletteBlends++) nblendPaletteTowardPalette(_t->_palT, _currentPalette, 48);
    _currentPalette = _t->_palT; // copy transitioning/temporary palette
  }
}

void Segment::handleTransition() {
  if (!transitional || !_t) return;  // Early exit if no transition active
  unsigned long maxWait = millis() + 20;
  if (mode == FX_MODE_STATIC && next_time > maxWait) next_time = maxWait;
  if (progress() == 0xFFFFU) {
    if (_t) {
      //if (_t->_modeP != mode) markForReset();  // WLEDMM effect transition disabled as it does not work (flashes due to double effect restart)
      delete _t;
      _t = nullptr;
    }
    transitional = false; // finish transitioning segment
  }
}

void Segment::setUp(uint16_t i1, uint16_t i2, uint8_t grp, uint8_t spc, uint16_t ofs, uint16_t i1Y, uint16_t i2Y) {
  //return if neither bounds nor grouping have changed
  bool boundsUnchanged = (start == i1 && stop == i2);
  #ifndef WLED_DISABLE_2D
  if (Segment::maxHeight>1) boundsUnchanged &= (startY == i1Y && stopY == i2Y); // 2D
  #endif
  if (boundsUnchanged
      && (!grp || (grouping == grp && spacing == spc))
      && (ofs == UINT16_MAX || ofs == offset)) return;

  stateChanged = true; // send UDP/WS broadcast

  if (stop>start) markForBlank(); //turn old segment range off // WLEDMM stop > start
  if (i2 <= i1) { //disable segment
    stop = 0;
    markForReset();
    return;
  }
  if (i1 < Segment::maxWidth || (i1 >= Segment::maxWidth*Segment::maxHeight && i1 < strip.getLengthTotal())) start = i1; // Segment::maxWidth equals strip.getLengthTotal() for 1D
  stop = i2 > Segment::maxWidth*Segment::maxHeight ? min(i2,strip.getLengthTotal()) : (i2 > Segment::maxWidth ? Segment::maxWidth : max((uint16_t)1,i2));  // WLEDMM: use native min/max
  startY = 0;
  stopY  = 1;
  #ifndef WLED_DISABLE_2D
  if (Segment::maxHeight>1) { // 2D
    if (i1Y < Segment::maxHeight) startY = i1Y;
    stopY = i2Y > Segment::maxHeight ? Segment::maxHeight : max((uint16_t)1,i2Y);         // WLEDMM: use native min/max
  }
  #endif
  if (grp) {
    grouping = grp;
    spacing = spc;
  }
  if (ofs < UINT16_MAX) offset = ofs;
  markForReset();
  if (!boundsUnchanged) refreshLightCapabilities();
}


bool Segment::setColor(uint8_t slot, uint32_t c) { //returns true if changed
  if (slot >= NUM_COLORS || c == colors[slot]) return false;
  if (!_isRGB && !_hasW) {
    if (slot == 0 && c == BLACK) return false; // on/off segment cannot have primary color black
    if (slot == 1 && c != BLACK) return false; // on/off segment cannot have secondary color non black
  }
  if (fadeTransition && on) startTransition(strip.getTransition()); // start transition prior to change // WLEDMM only on real change
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
  if (fadeTransition && on) startTransition(strip.getTransition()); // start transition prior to change
  cct = k;
  stateChanged = true; // send UDP/WS broadcast
}

void Segment::setOpacity(uint8_t o) {
  if (opacity == o) return;
  if (fadeTransition) startTransition(strip.getTransition()); // start transition prior to change
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

void Segment::setMode(uint8_t fx, bool loadDefaults, bool sliderDefaultsOnly) {
  //WLEDMM: return to old setting if not explicitly set
  static int16_t oldMap = -1;
  static int16_t oldSim = -1;
  static int16_t oldPalette = -1;
  // if we have a valid mode & is not reserved
  if (fx < strip.getModeCount() && strncmp_P("RSVD", strip.getModeData(fx), 4)) {
    if (fx != mode) {
      startTransition(strip.getTransition()); // set effect transitions
      //markForReset(); // transition will handle this
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
        if (!sliderDefaultsOnly) {
          //WLEDMM: return to old setting if not explicitly set
          sOpt = extractModeDefaults(fx, "m12");  if (sOpt >= 0) {if (oldMap==-1) oldMap = map1D2D; map1D2D   = constrain(sOpt, 0, 7);} else {if (oldMap!=-1) map1D2D = oldMap; oldMap = -1;}
          sOpt = extractModeDefaults(fx, "si");   if (sOpt >= 0) {if (oldSim==-1) oldSim = soundSim; soundSim  = constrain(sOpt, 0, 1);} else {if (oldSim!=-1) soundSim = oldSim; oldSim = -1;}
          sOpt = extractModeDefaults(fx, "rev");  if (sOpt >= 0) reverse   = (bool)sOpt;
          sOpt = extractModeDefaults(fx, "mi");   if (sOpt >= 0) mirror    = (bool)sOpt; // NOTE: setting this option is a risky business
          sOpt = extractModeDefaults(fx, "rY");   if (sOpt >= 0) reverse_y = (bool)sOpt;
          sOpt = extractModeDefaults(fx, "mY");   if (sOpt >= 0) mirror_y  = (bool)sOpt; // NOTE: setting this option is a risky business
          sOpt = extractModeDefaults(fx, "pal");  if (sOpt >= 0) {if (oldPalette==-1) oldPalette = palette; setPalette(sOpt);} else {if (oldPalette!=-1) setPalette(oldPalette); oldPalette = -1;}
        }
      }
      /*if (!fadeTransition)*/ markForReset(); // WLEDMM quickfix for effect "double startup" bug.
      stateChanged = true; // send UDP/WS broadcast
    }
  }
}

void Segment::setPalette(uint8_t pal) {
  if (pal < 245 && pal > GRADIENT_PALETTE_COUNT+13) pal = 0; // built in palettes
  if (pal > 245 && (strip.customPalettes.size() == 0 || 255U-pal > strip.customPalettes.size()-1)) pal = 0; // custom palettes
  if (pal != palette) {
    if (strip.paletteFade && on) startTransition(strip.getTransition());
    palette = pal;
    stateChanged = true; // send UDP/WS broadcast
  }
}

// 2D matrix

//
// WLEDMM Segment::virtualWidth() and Segment::virtualHeight() are declared inline, see FX.h
//

uint16_t Segment::nrOfVStrips() const {
  uint16_t vLen = 1;
#ifndef WLED_DISABLE_2D
  if (is2D()) {
    switch (map1D2D) {
      case M12_pBar:
        vLen = calc_virtualWidth();
        break;
      case M12_sCircle: //WLEDMM
        vLen = (calc_virtualWidth() + calc_virtualHeight()) / 6; // take third of the average width
        break;
      case M12_sBlock: //WLEDMM
        vLen = (calc_virtualWidth() + calc_virtualHeight()) / 8; // take half of the average width
        break;
    }
  }
#endif
  return vLen;
}

//WLEDMM jMap
struct XandY {
  uint8_t x;
  uint8_t y;
};
struct ArrayAndSize {
  uint8_t size;
  XandY *array;
};
class JMapC {
  public:
    char previousSegmentName[50] = "";

    ~JMapC() {
      DEBUG_PRINTLN("~JMapC");
      deletejVectorMap();
    }
    void deletejVectorMap() {
      if (jVectorMap.size() > 0) {
        DEBUG_PRINTLN("delete jVectorMap");
        for (size_t i=0; i<jVectorMap.size(); i++)
          if (jVectorMap[i].array) { delete[] jVectorMap[i].array; jVectorMap[i].array = nullptr; } // softhack007 quickfix for memory leak
        jVectorMap.clear();
      }
    }
    uint16_t length() {
      updatejMapDoc();
      size_t size = jVectorMap.size();
      if (size > 0)
        return size;
      else
        return SEGMENT.calc_virtualWidth() * SEGMENT.calc_virtualHeight(); // calc pixel sizes
    }
    void setPixelColor(uint16_t i, uint32_t col) {
      updatejMapDoc();
      if (jVectorMap.size() > i) {
        if (i==0) {
          SEGMENT.fadeToBlackBy(10); //as not all pixels used
        }
        for (int j=0; j<jVectorMap[i].size; j++) {
          SEGMENT.setPixelColorXY(jVectorMap[i].array[j].x * scale, jVectorMap[i].array[j].y * scale, col);
        }
      }
    }
    uint32_t getPixelColor(uint16_t i) {
      updatejMapDoc();
      if (jVectorMap.size() > 0)
        return SEGMENT.getPixelColorXY(jVectorMap[i].array[0].x * scale, jVectorMap[i].array[0].y * scale);
      else
        return 0;
    }
  private:
    std::vector<ArrayAndSize> jVectorMap; 
    StaticJsonDocument<4096> docChunk; //must fit forks with about 32 points each
    uint8_t scale;

    void updatejMapDoc() {
      if (SEGMENT.name == nullptr && jVectorMap.size() > 0) {
        deletejVectorMap();
      }
      else if (SEGMENT.name != nullptr && strcmp(SEGMENT.name, previousSegmentName) != 0) {
        uint32_t dataSize = 0;
        deletejVectorMap();
        DEBUG_PRINT("New "); DEBUG_PRINTLN(SEGMENT.name);
        char jMapFileName[50];
        strcpy(jMapFileName, "/");
        strcat(jMapFileName, SEGMENT.name);
        strcat(jMapFileName, ".json");
        File jMapFile;
        jMapFile = WLED_FS.open(jMapFileName, "r");

        uint_fast16_t maxWidth = 0;       // WLEDMM fix uint8 overflow for large width/height
        uint_fast16_t maxHeight = 0;      // WLEDMM

        //https://arduinojson.org/v6/how-to/deserialize-a-very-large-document/
        jMapFile.find("[");
        do { //for each element in the array
          DeserializationError err = deserializeJson(docChunk, jMapFile);
          // serializeJson(docChunk, Serial); USER_PRINTLN();
          // USER_PRINTF("docChunk  %u / %u%% (%u %u %u) %u\n", (unsigned int)docChunk.memoryUsage(), 100 * docChunk.memoryUsage() / docChunk.capacity(), (unsigned int)docChunk.size(), docChunk.overflowed(), (unsigned int)docChunk.nesting(), jMapFile.size());
          if (err) 
          {
            USER_PRINTF("deserializeJson() of parseTree failed with code %s\n", err.c_str());
            USER_FLUSH();
            if (SEGMENT.name) delete[] SEGMENT.name; SEGMENT.name = nullptr; //need to clear the name as otherwise continuously loaded // softhack007 avoid deleting nullptr
            return;
          }

          if (docChunk.is<JsonArray>()) { //each item is or an array of arrays (fork) or an array of x,y (no fork)
            //fill the vector with arrays and get the width and height of the jMap

            JsonArray arrayChunk = docChunk.as<JsonArray>();
            ArrayAndSize arrayAndSize;
            arrayAndSize.size = 0;
            if (arrayChunk[0].is<JsonArray>()) { //if array of arrays
              arrayAndSize.array = new(std::nothrow) XandY[arrayChunk.size()];
              for (JsonVariant arrayElement: arrayChunk) {
                maxWidth = max((uint16_t)maxWidth, arrayElement[0].as<uint16_t>());       // WLEDMM use native min/max
                maxHeight = max((uint16_t)maxHeight, arrayElement[1].as<uint16_t>());     // WLEDMM
                arrayAndSize.array[arrayAndSize.size].x = arrayElement[0].as<uint8_t>();
                arrayAndSize.array[arrayAndSize.size].y = arrayElement[1].as<uint8_t>();
                arrayAndSize.size++;
                dataSize += sizeof(XandY);
              }
            }
            else { // if array (of x and y)
              arrayAndSize.array = new(std::nothrow) XandY[1];
              maxWidth = max((uint16_t)maxWidth, arrayChunk[0].as<uint16_t>());         // WLEDMM use native min/max
              maxHeight = max((uint16_t)maxHeight, arrayChunk[1].as<uint16_t>());       // WLEDMM
              arrayAndSize.array[arrayAndSize.size].x = arrayChunk[0].as<uint8_t>();
              arrayAndSize.array[arrayAndSize.size].y = arrayChunk[1].as<uint8_t>();
              arrayAndSize.size++;
              dataSize += sizeof(XandY);
            }
            jVectorMap.push_back(arrayAndSize);
            dataSize += sizeof(arrayAndSize);
          }

        } while (jMapFile.findUntil(",", "]"));

        jMapFile.close();

        maxWidth++; maxHeight++;
        scale = min(SEGMENT.calc_virtualWidth() / maxWidth, SEGMENT.calc_virtualHeight() / maxHeight);  // WLEDMM re-calc width/heiht from active settings
        dataSize += sizeof(jVectorMap);
        USER_PRINT("dataSize ");
        USER_PRINT(dataSize);
        USER_PRINT(" scale ");
        USER_PRINTLN(scale);
        strcpy(previousSegmentName, SEGMENT.name);
      }
    } //updatejMapDoc
}; //class JMapC

//WLEDMM jMap
void Segment::createjMap() {
  if (!jMap) {
    DEBUG_PRINTLN("createjMap");
    jMap = new(std::nothrow) JMapC();
  }
}

//WLEDMM jMap
void Segment::deletejMap() {
  //Should be called from ~Segment but causes crash (and ~Segment is called quite often...)
  if (jMap) {
    DEBUG_PRINTLN("deletejMap");
    delete (JMapC *)jMap; jMap = nullptr;
  }
}


// Constants for mapping mode "Pinwheel"
#ifndef WLED_DISABLE_2D
constexpr int Pinwheel_Steps_Small = 72;       // no holes up to 16x16
constexpr int Pinwheel_Size_Small  = 16;       // larger than this -> use "Medium"
constexpr int Pinwheel_Steps_Medium = 192;     // no holes up to 32x32
constexpr int Pinwheel_Size_Medium  = 32;      // larger than this -> use "Big"
constexpr int Pinwheel_Steps_Big = 304;        // no holes up to 50x50
constexpr int Pinwheel_Size_Big  = 50;         // larger than this -> use "XL"
constexpr int Pinwheel_Steps_XL  = 368;
constexpr int Pinwheel_Size_XL   = 58;         // larger than this -> use "XXL"
constexpr int Pinwheel_Steps_XXL = 456;
constexpr int Pinwheel_Size_XXL   = 68;        // larger than this -> use "LL"
constexpr int Pinwheel_Steps_LL = 592;         // 128x64 no holes, 28fps
constexpr float Int_to_Rad_Small = (DEG_TO_RAD * 360) / Pinwheel_Steps_Small;  // conversion: from 0...72 to Radians
constexpr float Int_to_Rad_Med =   (DEG_TO_RAD * 360) / Pinwheel_Steps_Medium; // conversion: from 0...192 to Radians
constexpr float Int_to_Rad_Big =   (DEG_TO_RAD * 360) / Pinwheel_Steps_Big;    // conversion: from 0...304 to Radians
constexpr float Int_to_Rad_XL =    (DEG_TO_RAD * 360) / Pinwheel_Steps_XL;     // conversion: from 0...368 to Radians
constexpr float Int_to_Rad_XXL =   (DEG_TO_RAD * 360) / Pinwheel_Steps_XXL;    // conversion: from 0...456 to Radians
constexpr float Int_to_Rad_LL =   (DEG_TO_RAD * 360) / Pinwheel_Steps_LL;      // conversion: from 0...592 to Radians

constexpr int Fixed_Scale = 32768;               // fixpoint scaling factor (15bit for fraction)

// Pinwheel helper function: pixel index to radians
static float getPinwheelAngle(int i, int vW, int vH) {
  int maxXY = max(vW, vH);
  if (maxXY <= Pinwheel_Size_Small)  return float(i) * Int_to_Rad_Small;
  if (maxXY <= Pinwheel_Size_Medium) return float(i) * Int_to_Rad_Med;
  if (maxXY <= Pinwheel_Size_Big)    return float(i) * Int_to_Rad_Big;
  if (maxXY <= Pinwheel_Size_XL)     return float(i) * Int_to_Rad_XL;
  if (maxXY <= Pinwheel_Size_XXL)     return float(i) * Int_to_Rad_XXL;
  // else
  return float(i) * Int_to_Rad_LL;
}
// Pinwheel helper function: matrix dimensions to number of rays
static int getPinwheelLength(int vW, int vH) {
  int maxXY = max(vW, vH);
  if (maxXY <= Pinwheel_Size_Small)  return Pinwheel_Steps_Small;
  if (maxXY <= Pinwheel_Size_Medium) return Pinwheel_Steps_Medium;
  if (maxXY <= Pinwheel_Size_Big)    return Pinwheel_Steps_Big;
  if (maxXY <= Pinwheel_Size_XL)     return Pinwheel_Steps_XL;
  if (maxXY <= Pinwheel_Size_XXL)     return Pinwheel_Steps_XXL;
  // else
  return Pinwheel_Steps_LL;
}
#endif

// 1D strip
uint16_t Segment::calc_virtualLength() const {
#ifndef WLED_DISABLE_2D
  if (is2D()) {
    uint16_t vW = calc_virtualWidth();
    uint16_t vH = calc_virtualHeight();
    uint16_t vLen = vW * vH; // use all pixels from segment
    switch (map1D2D) {
      case M12_pBar:
        vLen = vH;
        break;
      case M12_pCorner:
        vLen = max(vW,vH); // get the longest dimension
        break;
      case M12_pArc:
        { unsigned vLen2 = vW * vW + vH * vH;            // length ^2
          if (vLen2 < UINT16_MAX) vLen = sqrt16(vLen2);  // use faster function for 16bit values
          else vLen = sqrtf(vLen2);                      // fall-back to float if bigger
          if (vW != vH) vLen++; // round up
        }
        break;
      case M12_jMap: //WLEDMM jMap
        if (jMap)
          vLen = ((JMapC *)jMap)->length();
        break;
      case M12_sCircle: //WLEDMM
        vLen = max(vW,vH); // get the longest dimension
        // vLen = (virtualWidth() + virtualHeight()) * 3;
        break;
      case M12_sBlock: //WLEDMM
        if (nrOfVStrips()>1)
          vLen = max(vW,vH) * 4;//0.5; // get the longest dimension
        else 
          vLen = max(vW,vH) * 0.5f; // get the longest dimension
        break;
      case M12_sPinwheel:
        vLen = getPinwheelLength(vW, vH);
        break;
    }
    return vLen;
  }
#endif
  uint16_t groupLen = groupLength();
  uint16_t vLength = (length() + groupLen - 1) / groupLen;
  if (mirror && width() > 1) vLength = (vLength + 1) /2;  // divide by 2 if mirror, leave at least a single LED // WLEDMM bugfix for pseudo 2d strips
  return vLength;
}

//WLEDMM used for M12_sBlock
static void xyFromBlock(uint16_t &x,uint16_t &y, uint16_t i, uint16_t vW, uint16_t vH, uint16_t vStrip) {
  float i2;
  if (i<=SEGLEN*0.25f) { //top, left to right
    i2 = i/(SEGLEN*0.25f);
    x = vW / 2 - vStrip - 1 + i2 * vStrip * 2;
    y = vH / 2 - vStrip - 1;
  }
  else if (i <= SEGLEN * 0.5f) { //right, top to bottom
    i2 = (i-SEGLEN*0.25f)/(SEGLEN*0.25f);
    x = vW / 2 + vStrip;
    y = vH / 2 - vStrip - 1 + i2 * vStrip * 2;
  }
  else if (i <= SEGLEN * 0.75f) { //bottom, right to left
    i2 = (i-SEGLEN*0.5f)/(SEGLEN*0.25f);
    x = vW / 2 + vStrip - i2 * vStrip * 2;
    y = vH / 2 + vStrip;
  }
  else if (i <= SEGLEN) { //left, bottom to top
    i2 = (i-SEGLEN*0.75f)/(SEGLEN*0.25f);
    x = vW / 2 - vStrip - 1;
    y = vH / 2 + vStrip - i2 * vStrip * 2;
  }

}

void IRAM_ATTR_YN __attribute__((hot)) Segment::setPixelColor(int i, uint32_t col) //WLEDMM: IRAM_ATTR conditionally
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
        else drawLine(0,vH-i-1, vW-1,vH-i-1, col, false); // WLEDMM draw line instead of plotting each pixel
        break;
      case M12_pArc:
        // expand in circular fashion from center
        if (i==0)
          setPixelColorXY(0, 0, col);
        else {
          if (i == virtualLength() - 1) setPixelColorXY(vW-1, vH-1, col); // Last i always fill corner
          if (!_isSuperSimpleSegment) { 
            // WLEDMM: drawArc() is faster if it's NOT "super simple" as the regular M12_pArc
            // can do "useSymmetry" to speed things along, but a more complicated segment likey
            // uses mirroring which generates a symmetry speed-up, or other things which mean
            // less pixels are calculated.
            drawArc(0, 0, i, col); 
          } else {
            //WLEDMM: some optimizations for the drawing loop
            //  pre-calculate loop limits, exploit symmetry at 45deg
            float radius = float(i);
            
            // float step = HALF_PI / (2.85f * radius);  // upstream uses this
            float step = HALF_PI / (M_PI * radius);      // WLEDMM we use the correct circumference
            bool useSymmetry = (max(vH, vW) > 20);       // for segments wider than 20 pixels, we exploit symmetry
            unsigned numSteps;
            if (useSymmetry) numSteps = 1 + ((HALF_PI/2.0f + step/2.0f) / step); // with symmetry
            else             numSteps = 1 + ((HALF_PI      + step/2.0f) / step); // without symmetry

            float rad = 0.0f;
            for (unsigned count = 0; count < numSteps; count++) {
              // may want to try float version as well (with or without antialiasing)
              // int x = max(0, min(vW-1, (int)roundf(sinf(rad) * radius)));
              // int y = max(0, min(vH-1, (int)roundf(cosf(rad) * radius)));
              int x = roundf(sinf(rad) * radius);
              int y = roundf(cosf(rad) * radius);
              setPixelColorXY(x, y, col);
              if(useSymmetry) setPixelColorXY(y, x, col);// WLEDMM
              rad += step;
            }

            // // Bresenham’s Algorithm (may not fill every pixel)
            // int d = 3 - (2*i);
            // int y = i, x = 0;
            // while (y >= x) {
            //  setPixelColorXY(x, y, col);
            //  setPixelColorXY(y, x, col);
            //  x++;
            //  if (d > 0) {
            //    y--;
            //    d += 4 * (x - y) + 10;
            //  } else {
            //    d += 4 * x + 6;
            //  }
            // }
          }
        }
        break;
      case M12_pCorner: {
          int x = min(i, vW-1);
          int y = min(i, vH-1);
          if (i <= y) drawLine(0,y, x,y, col, false);  // botton line (if visible)
          if (i <= x) drawLine(x,0, x,y, col, false);  // right line (if visible)
        }
        break;
      case M12_jMap: //WLEDMM jMap
        if (jMap)
          ((JMapC *)jMap)->setPixelColor(i, col);
        break;
      case M12_sCircle: //WLEDMM
        if (vStrip > 0)
        {
          int x = roundf(sinf(360*i/SEGLEN*DEG_TO_RAD) * vW * (vStrip+1)/nrOfVStrips());
          int y = roundf(cosf(360*i/SEGLEN*DEG_TO_RAD) * vW * (vStrip+1)/nrOfVStrips());
          setPixelColorXY(x + vW/2, y + vH/2, col);
        }
        else // pArc -> circle
          drawArc(vW/2, vH/2, i/2, col);
        break;
      case M12_sBlock: //WLEDMM
        if (vStrip > 0)
        {
          //vStrip+1 is distance from centre, i is how much of the square is filled
          uint16_t x=0,y=0;
          xyFromBlock(x,y, i, vW, vH, (vStrip+1)*2);
          setPixelColorXY(x, y, col);
        }
        else { // pCorner -> block
          int centerX = (vW+1)/2 - 1;
          int centerY = (vH+1)/2 - 1;
          int xLeft   = max(centerX-i, 0);
          int yTop    = max(centerY-i, 0);
          int xRight  = min(centerX+i+1, vW-1);
          int yBottom = min(centerY+i+1, vH-1);

          if (yTop == centerY-i)      drawLine(xLeft,yTop, xRight, yTop, col);       // top and bottom horizontal lines, if visible
          if (yBottom == centerY+i+1) drawLine(xLeft,yBottom, xRight, yBottom, col);
          if (xLeft == centerX-i)     drawLine(xLeft,yTop, xLeft, yBottom, col);     // left and right vertical lines, if visible
          if (xRight == centerX+i+1)  drawLine(xRight,yTop, xRight, yBottom, col);
        }
        break;
      case M12_sPinwheel: {
        // WLEDMM shortcut when no grouping/spacing used
        bool simpleSegment = (grouping == 1) && (spacing == 0);
        uint32_t scaled_col = col;
        if (simpleSegment) {
          // segment brightness must be pre-calculated for the "fast" setPixelColorXY variant!
          uint8_t _bri_t = currentBri(on ? opacity : 0);
          if (!_bri_t && !transitional) return;
          if (_bri_t < 255) scaled_col = color_fade(col, _bri_t);
        }

        // i = angle --> 0 - 296  (Big), 0 - 192  (Medium), 0 - 72 (Small)
        float centerX = roundf((vW-1) / 2.0f);
        float centerY = roundf((vH-1) / 2.0f);
        float angleRad = getPinwheelAngle(i, vW, vH); // angle in radians
        float cosVal = cosf(angleRad);
        float sinVal = sinf(angleRad);

        // avoid re-painting the same pixel
        int lastX = INT_MIN; // impossible position
        int lastY = INT_MIN; // impossible position
        // draw line at angle, starting at center and ending at the segment edge
        // we use fixed point math for better speed. Starting distance is 0.5 for better rounding
        // int_fast16_t and int_fast32_t types changed to int, minimum bits commented
        int posx = (centerX + 0.5f * cosVal) * Fixed_Scale; // X starting position in fixed point 18 bit
        int posy = (centerY + 0.5f * sinVal) * Fixed_Scale; // Y starting position in fixed point 18 bit
        int inc_x = cosVal * Fixed_Scale; // X increment per step (fixed point) 10 bit
        int inc_y = sinVal * Fixed_Scale; // Y increment per step (fixed point) 10 bit

        int32_t maxX = vW * Fixed_Scale; // X edge in fixedpoint
        int32_t maxY = vH * Fixed_Scale; // Y edge in fixedpoint

        // Odd rays start further from center if prevRay started at center.
        static int prevRay = INT_MIN; // previous ray number
        if ((i % 2 == 1) && (i - 1 == prevRay || i + 1 == prevRay)) {
          int jump = min(vW/3, vH/3); // can add 2 if using medium pinwheel 
          posx += inc_x * jump;
          posy += inc_y * jump;
        }
        prevRay = i;

        // draw ray until we hit any edge
        while ((posx >= 0) && (posy >= 0) && (posx < maxX)  && (posy < maxY))  {
          // scale down to integer (compiler will replace division with appropriate bitshift)
          int x = posx / Fixed_Scale;
          int y = posy / Fixed_Scale;
          // set pixel
          if (x != lastX || y != lastY) { // only paint if pixel position is different
            if (simpleSegment) setPixelColorXY_fast(x, y, col, scaled_col, vW, vH);
            else setPixelColorXY_slow(x, y, col);  
          }
          lastX = x;
          lastY = y;
          // advance to next position
          posx += inc_x;
          posy += inc_y;
        }
        break;
      }
    }
    return;
  } else if (Segment::maxHeight!=1 && (width()==1 || height()==1)) {
    if (start < Segment::maxWidth*Segment::maxHeight) {
      // we have a vertical or horizontal 1D segment (WARNING: virtual...() may be transposed)
      int x = 0, y = 0;
      if (virtualHeight()>1) y = i;
      else if (virtualWidth() >1) x = i;
      setPixelColorXY(x, y, col);
      return;
    }
  }
#endif

  if (ledsrgb) ledsrgb[i] = col;

  uint16_t len = length();
  uint8_t _bri_t = currentBri(on ? opacity : 0);
  if (!_bri_t && !transitional && fadeTransition) return; // if _bri_t == 0 && segment is not transitioning && transitions are enabled then save a few CPU cycles
  if (_bri_t < 255) {
    col = color_fade(col, _bri_t);
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

#if 0  // needs more testing
  // WLEDMM shortcut when no grouping/spacing used
  bool simpleSegment = !mirror && (grouping == 1) && (spacing == 0);
  if (simpleSegment) {
      int indexSet = i + offset; // offset/phase
      if (indexSet >= stop) indexSet -= len; // wrap
      strip.setPixelColor(indexSet, col);
    return;
  }
#endif

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

uint32_t __attribute__((hot)) Segment::getPixelColor(int i) const
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
      case M12_pCorner:
      case M12_pArc: {
        if (i < max(vW, vH)) { 
          return vW>vH ? getPixelColorXY(i, 0) : getPixelColorXY(0, i); // Corner and Arc
          break;
        }
        float minradius = float(i) - 0.1f;
        const int minradius2 = roundf(minradius * minradius);
        int startX, startY;
        if (vW >= vH) {startX = vW - 1; startY = 1;} // Last Column
        else          {startX = 1; startY = vH - 1;} // Last Row
        // Loop through only last row/column depending on orientation
        for (int x = startX; x < vW; x++) {
          int newX2 = x * x;
          for (int y = startY; y < vH; y++) {
            int newY2 = y * y;
            if (newX2 + newY2 >= minradius2) return getPixelColorXY(x, y);        
          }
        }
        return getPixelColorXY(vW-1, vH-1); // Last pixel
        break;
      }
      case M12_jMap: //WLEDMM jMap
        if (jMap)
          return ((JMapC *)jMap)->getPixelColor(i);
        break;
      case M12_sCircle: //WLEDMM
        if (vStrip > 0)
        {
          int x = roundf(sinf(360*i/SEGLEN*DEG_TO_RAD) * vW * (vStrip+1)/nrOfVStrips());
          int y = roundf(cosf(360*i/SEGLEN*DEG_TO_RAD) * vW * (vStrip+1)/nrOfVStrips());
          return getPixelColorXY(x + vW/2, y + vH/2);
        }
        else
          return vW>vH ? getPixelColorXY(i, 0) : getPixelColorXY(0, i);
        break;
      case M12_sBlock: //WLEDMM
        if (vStrip > 0)
        {
          uint16_t x=0,y=0;
          xyFromBlock(x,y, i, vW, vH, (vStrip+1)*2);
          return getPixelColorXY(x, y);
        }
        else
          return getPixelColorXY(vW / 2, vH / 2 - i - 1);
        break;
      case M12_sPinwheel:
        // not 100% accurate, returns pixel at outer edge
        // i = angle --> 0 - 296  (Big), 0 - 192  (Medium), 0 - 72 (Small)
        float centerX = roundf((vW-1) / 2.0f);
        float centerY = roundf((vH-1) / 2.0f);
        float angleRad = getPinwheelAngle(i, vW, vH); // angle in radians
        float cosVal = cosf(angleRad);
        float sinVal = sinf(angleRad);

        int posx = (centerX + 0.5f * cosVal) * Fixed_Scale; // X starting position in fixed point 18 bit
        int posy = (centerY + 0.5f * sinVal) * Fixed_Scale; // Y starting position in fixed point 18 bit
        int inc_x = cosVal * Fixed_Scale; // X increment per step (fixed point) 10 bit
        int inc_y = sinVal * Fixed_Scale; // Y increment per step (fixed point) 10 bit
        int32_t maxX = vW * Fixed_Scale; // X edge in fixedpoint
        int32_t maxY = vH * Fixed_Scale; // Y edge in fixedpoint

        // trace ray from center until we hit any edge - to avoid rounding problems, we use the same method as in setPixelColor
        int x = INT_MIN;
        int y = INT_MIN;
        while ((posx >= 0) && (posy >= 0) && (posx < maxX)  && (posy < maxY))  {
          // scale down to integer (compiler will replace division with appropriate bitshift)
          x = posx / Fixed_Scale;
          y = posy / Fixed_Scale;
          // advance to next position
          posx += inc_x;
          posy += inc_y;
        }
        return getPixelColorXY(x, y);
        break;
      }
    return 0;
  }
#endif

  if (ledsrgb) return RGBW32(ledsrgb[i].r, ledsrgb[i].g, ledsrgb[i].b, 0);

  if (reverse) i = virtualLength() - i - 1;
  i *= groupLength();
  i += start;
  /* offset/phase */
  if (offset < INT16_MAX) i += offset; // WLEDMM
  if ((i >= stop) && (stop>0)) i -= length(); // WLEDMM avoid negative index (stop = 0 is a possible value)
  if (i<0) i=0; // WLEDMM just to be 100% sure
  return strip.getPixelColorRestored(i);
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
 * Fills segment with color - WLEDMM using faster sPC if possible
 */
void __attribute__((hot)) Segment::fill(uint32_t c) {
  if (!isActive()) return; // not active

  const uint_fast16_t cols = is2D() ? virtualWidth() : virtualLength();             // WLEDMM use fast int types
  const uint_fast16_t rows = virtualHeight(); // will be 1 for 1D

  if (is2D()) {
    // pre-calculate scaled color
    uint32_t scaled_col = c;
    bool simpleSegment = (grouping == 1) && (spacing == 0);
    if (simpleSegment) {
      uint8_t _bri_t = currentBri(on ? opacity : 0);
      if (!_bri_t && !transitional) return;
      if (_bri_t < 255) scaled_col = color_fade(c, _bri_t);
    }
    // fill 2D segment
    for(unsigned y = 0; y < rows; y++) for (unsigned x = 0; x < cols; x++) {
      if (simpleSegment) setPixelColorXY_fast(x, y, c, scaled_col, cols, rows);
      else setPixelColorXY_slow(x, y, c);
    }
  } else { // fill 1D strip
    for (unsigned x = 0; x < cols; x++) setPixelColor(int(x), c);
  }
}

// Blends the specified color with the existing pixel color.
void Segment::blendPixelColor(int n, uint32_t color, uint8_t blend) {
  if (blend == UINT8_MAX) setPixelColor(n, color); 
  else setPixelColor(n, color_blend(getPixelColor(n), color, blend));
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
void __attribute__((hot)) Segment::fade_out(uint8_t rate) {
  if (!isActive()) return; // not active
  const uint_fast16_t cols = is2D() ? virtualWidth() : virtualLength();           // WLEDMM use fast int types
  const uint_fast16_t rows = virtualHeight(); // will be 1 for 1D

  uint_fast8_t fadeRate = (255-rate) >> 1;
  float mappedRate_r = 1.0f / (float(fadeRate) +1.1f); // WLEDMM use reciprocal  1/mappedRate -> faster on non-FPU chips

  uint32_t color2 = colors[1]; // SEGCOLOR(1); // target color // WLEDMM minor optimization
  int w2 = W(color2);
  int r2 = R(color2);
  int g2 = G(color2);
  int b2 = B(color2);

  for (unsigned y = 0; y < rows; y++) for (unsigned x = 0; x < cols; x++) {
    uint32_t color = is2D() ? getPixelColorXY(int(x), int(y)) : getPixelColor(int(x));
    if (color == color2) continue;  // WLEDMM speedup - pixel color = target color, so nothing to do
    int w1 = W(color);
    int r1 = R(color);
    int g1 = G(color);
    int b1 = B(color);

    int wdelta = mappedRate_r * (w2 - w1);  // WLEDMM use reciprocal - its faster
    int rdelta = mappedRate_r * (r2 - r1);
    int gdelta = mappedRate_r * (g2 - g1);
    int bdelta = mappedRate_r * (b2 - b1);

    // if fade isn't complete, make sure delta is at least 1 (fixes rounding issues)
    wdelta += (w2 == w1) ? 0 : (w2 > w1) ? 1 : -1;
    rdelta += (r2 == r1) ? 0 : (r2 > r1) ? 1 : -1;
    gdelta += (g2 == g1) ? 0 : (g2 > g1) ? 1 : -1;
    bdelta += (b2 == b1) ? 0 : (b2 > b1) ? 1 : -1;
    uint32_t colorNew = RGBW32(r1 + rdelta, g1 + gdelta, b1 + bdelta, w1 + wdelta); // WLEDMM

    if (colorNew != color) {                                                        // WLEDMM speedup - do not repaint the same color
      if (is2D()) setPixelColorXY(int(x), int(y), colorNew);
      else        setPixelColor(int(x), colorNew);
    }
  }
}

// fades all pixels to black using nscale8()
void __attribute__((hot)) Segment::fadeToBlackBy(uint8_t fadeBy) {
  if (!isActive() || fadeBy == 0) return;   // optimization - no scaling to apply
  const uint_fast16_t cols = is2D() ? virtualWidth() : virtualLength();      // WLEDMM use fast int types
  const uint_fast16_t rows = virtualHeight(); // will be 1 for 1D
  const uint_fast8_t scaledown = 255-fadeBy;  // WLEDMM faster to pre-compute this

  // WLEDMM minor optimization
  if(is2D()) {
    for (unsigned y = 0; y < rows; y++) for (unsigned x = 0; x < cols; x++) {
      uint32_t cc = getPixelColorXY(int(x),int(y));                            // WLEDMM avoid RGBW32 -> CRGB -> RGBW32 conversion
      uint32_t cc2 = color_fade(cc, scaledown);                      // fade
#ifdef WLEDMM_FASTPATH
      if (cc2 != cc)                                               // WLEDMM only re-paint if faded color is different - normally disabled - causes problem with text overlay
#endif
        setPixelColorXY(int(x), int(y), cc2);
    }
  } else {
    for (uint_fast16_t x = 0; x < cols; x++) {
      setPixelColor((uint16_t)x, CRGB(getPixelColor((uint16_t)x)).nscale8(scaledown));
    }
  }
}

/*
 * blurs segment content, source: FastLED colorutils.cpp
 */
void __attribute__((hot)) Segment::blur(uint8_t blur_amount, bool smear) {
  if (!isActive() || blur_amount == 0) return; // optimization: 0 means "don't blur"
#ifndef WLED_DISABLE_2D
  if (is2D()) {
    // compatibility with 2D
    const uint_fast32_t cols = virtualWidth();
    const uint_fast32_t rows = virtualHeight();
    for (uint_fast32_t i = 0; i < rows; i++) blurRow(i, blur_amount, smear); // blur all rows
    for (uint_fast32_t k = 0; k < cols; k++) blurCol(k, blur_amount, smear); // blur all columns
    return;
  }
#endif
  uint8_t keep = smear ? 255 : 255 - blur_amount;
  uint8_t seep = blur_amount >> 1;
  unsigned vlength = virtualLength();
  uint32_t carryover = BLACK;
  uint32_t lastnew;
  uint32_t last;
  uint32_t curnew = 0;
  for (unsigned i = 0; i < vlength; i++) {
    uint32_t cur = getPixelColor(i);
    uint32_t part = color_fade(cur, seep);
    curnew = color_fade(cur, keep);
    if (i > 0) {
      if (carryover)
        curnew = color_add(curnew, carryover, !smear);  // WLEDMM
      uint32_t prev = color_add(lastnew, part, !smear); // WLEDMM
      if (last != prev) // optimization: only set pixel if color has changed
        setPixelColor(int(i - 1), prev);
    }
    else // first pixel
      setPixelColor(int(i), curnew);
    lastnew = curnew;
    last = cur; // save original value for comparison on next iteration
    carryover = part;
  }
  setPixelColor(int(vlength - 1), curnew);
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
uint8_t Segment::get_random_wheel_index(uint8_t pos) const { // WLEDMM use fast int types, use native min/max
  uint_fast8_t r = 0, x = 0, y = 0, d = 0;

  while(d < 42) {
    r = random8();
    x = abs(int(pos - r));
    y = 255 - x;
    d = min(x, y);
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
uint32_t __attribute__((hot)) Segment::color_from_palette(uint_fast16_t i, bool mapping, bool wrap, uint8_t mcol, uint8_t pbri) // WLEDMM use fast int types
{
  // default palette or no RGB support on segment
  if ((palette == 0 && mcol < NUM_COLORS) || !_isRGB) {
    uint32_t color = currentColor(mcol, colors[mcol]);
    color = gamma32(color);
    if (pbri == 255) return color;
    return RGBW32(scale8_video(R(color),pbri), scale8_video(G(color),pbri), scale8_video(B(color),pbri), scale8_video(W(color),pbri));
  }

  uint8_t paletteIndex = i;
  uint_fast16_t vLen = mapping ? virtualLength() : 1;
  if (mapping && vLen > 1) paletteIndex = (i*255)/(vLen -1);
  if (!wrap) paletteIndex = scale8(paletteIndex, 240); //cut off blend at palette "end"
  CRGB fastled_col = ColorFromPalette(_currentPalette, paletteIndex, pbri, (strip.paletteBlend == 3)? NOBLEND:LINEARBLEND); // NOTE: paletteBlend should be global

  return RGBW32(fastled_col.r, fastled_col.g, fastled_col.b, 0);
}

 //WLEDMM netmindz ar palette
uint8_t * Segment::getAudioPalette(int pal) const {
  // https://forum.makerforums.info/t/hi-is-it-possible-to-define-a-gradient-palette-at-runtime-the-define-gradient-palette-uses-the/63339
  
  um_data_t *um_data;
  if (!usermods.getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) {
    um_data = simulateSound(SEGMENT.soundSim);
  }
  uint8_t *fftResult = (uint8_t*)um_data->u_data[2];

  static uint8_t xyz[16];  // Needs to be 4 times however many colors are being used.
                           // 3 colors = 12, 4 colors = 16, etc.

  xyz[0] = 0;  // anchor of first color - must be zero
  xyz[1] = 0;
  xyz[2] = 0;
  xyz[3] = 0;
  
  CRGB rgb = getCRGBForBand(1, fftResult, pal);
  xyz[4] = 1;  // anchor of first color
  xyz[5] = rgb.r;
  xyz[6] = rgb.g;
  xyz[7] = rgb.b;
  
  rgb = getCRGBForBand(128, fftResult, pal);
  xyz[8] = 128;
  xyz[9] = rgb.r;
  xyz[10] = rgb.g;
  xyz[11] = rgb.b;
  
  rgb = getCRGBForBand(255, fftResult, pal);
  xyz[12] = 255;  // anchor of last color - must be 255
  xyz[13] = rgb.r;
  xyz[14] = rgb.g;
  xyz[15] = rgb.b;

  return xyz;
}


///////////////////////////////////////////////////////////////////////////////
// WS2812FX class implementation
///////////////////////////////////////////////////////////////////////////////

//WLEDMM from util.cpp
// enumerate all ledmapX.json files on FS and extract ledmap names if existing
void WS2812FX::enumerateLedmaps() {
  ledmapMaxSize = 0;
  ledMaps = 1;
  for (int i=1; i<10; i++) {
    char fileName[33] = {'\0'};       // WLEDMM ensure termination
    snprintf_P(fileName, sizeof(fileName), PSTR("/ledmap%d.json"), i);
    bool isFile = WLED_FS.exists(fileName);

    #ifndef ESP8266
    if (ledmapNames[i-1]) { //clear old name
      delete[] ledmapNames[i-1];
      ledmapNames[i-1] = nullptr;
    }
    #endif

    if (isFile) {
      ledMaps |= 1 << i;

      #ifndef ESP8266
      if (requestJSONBufferLock(21)) {
        //WLEDMM: upstream code loops over all ledmap files, read them all, every byte (!!!!) and only get the name of the file!!!
        File f;
        f = WLED_FS.open(fileName, "r");
        if (f) {
          f.find("\"n\":");
          char name[34] = { '\0' };  // ensure string termination
          f.readBytesUntil('\n', name, sizeof(name)-1);

          size_t len = strlen(name);
          if (len > 0 && len < 33) {
            (void) cleanUpName(name);
            len = strlen(name);
            ledmapNames[i-1] = new(std::nothrow) char[len+1]; // +1 to include terminating \0 
            if (ledmapNames[i-1]) strlcpy(ledmapNames[i-1], name, 33);
          }
          if (!ledmapNames[i-1]) {
            char tmp[33];
            snprintf_P(tmp, 32, PSTR("ledmap%d.json"), i);
            len = strlen(tmp);
            ledmapNames[i-1] = new(std::nothrow) char[len+1];
            if (ledmapNames[i-1]) strlcpy(ledmapNames[i-1], tmp, 33);
          }

          USER_PRINTF("enumerateLedmaps %s \"%s\"", fileName, name);
          if (isMatrix) {
            //WLEDMM calc ledmapMaxSize (TroyHacks)
            char dim[34] = { '\0' };
            f.find("\"width\":");
            f.readBytesUntil('\n', dim, sizeof(dim)-1); //hack: use fileName as we have this allocated already
            uint16_t maxWidth = atoi(cleanUpName(dim));
            f.find("\"height\":");
            memset(dim, 0, sizeof(dim)); // clear buffer before reading
            f.readBytesUntil('\n', dim, sizeof(dim)-1);
            uint16_t maxHeight = atoi(cleanUpName(dim));
            ledmapMaxSize = MAX(ledmapMaxSize, maxWidth * maxHeight);

            if (maxWidth*maxHeight>0) {
              USER_PRINTF(" (%dx%d -> %d)\n", maxWidth, maxHeight, ledmapMaxSize);
            } else {
              USER_PRINTLN();
            }
          }
          else
            USER_PRINTLN();
        }
        f.close();
        USER_FLUSH();
        releaseJSONBufferLock();
      }
      #endif
    }
  }
  //WLEDMM add segment names to be used as ledmap names
  uint8_t segment_index = 0;
  for (segment &seg : _segments) {
    if (seg.name != nullptr && strlen(seg.name) > 0) {
      char fileName[33];
      snprintf_P(fileName, sizeof(fileName), PSTR("/lm%s.json"), seg.name);
      bool isFile = WLED_FS.exists(fileName);
      if (isFile) ledMaps |= 1 << (10+segment_index);
    }
    segment_index++;
  }
}


//do not call this method from system context (network callback)
void WS2812FX::finalizeInit(void)
{
  //reset segment runtimes
  suspendStripService = true; // WLEDMM avoid running effects on an incomplete strip
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
    if (bus->getStart() + bus->getLength() > MAX_LEDS) {
      USER_PRINT("\nError: too many LEDs, max number is "); USER_PRINTLN(MAX_LEDS);
      break;
    }
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

  //initialize leds array. TBD: realloc if nr of leds change
  if (Segment::_globalLeds) {
    free(Segment::_globalLeds);
    Segment::_globalLeds = nullptr;
    purgeSegments(true);   // WLEDMM moved here, because it seems to improve stability.
  }
  if (useLedsArray && getLengthTotal()>0) { // WLEDMM avoid malloc(0)
    size_t arrSize = sizeof(CRGB) * getLengthTotal();
    // softhack007 disabled; putting leds into psram leads to horrible slowdown on WROVER boards (see setUpLeds())
    //#if defined(ARDUINO_ARCH_ESP32) && defined(BOARD_HAS_PSRAM) && defined(WLED_USE_PSRAM)
    //if (psramFound())
    //  Segment::_globalLeds = (CRGB*) ps_malloc(arrSize);
    //else
    //#endif
      if (arrSize > 0) Segment::_globalLeds = (CRGB*) malloc(arrSize); // WLEDMM avoid malloc(0)
    if ((Segment::_globalLeds != nullptr) && (arrSize > 0)) memset(Segment::_globalLeds, 0, arrSize); // WLEDMM avoid dereferencing nullptr
    if ((Segment::_globalLeds == nullptr) && (arrSize > 0)) errorFlag = ERR_LOW_MEM; // WLEDMM raise errorflag
  }

  //segments are created in makeAutoSegments();
  DEBUG_PRINTLN(F("Loading custom palettes"));
  loadCustomPalettes(); // (re)load all custom palettes
  DEBUG_PRINTLN(F("Loading custom ledmaps"));
  deserializeMap();     // (re)load default ledmap
  _isServicing = false;        // WLEDMM
  suspendStripService = false; // WLEDMM ready, run !
}

// WLEDMM wait until strip is idle (=not servicing).
// on 8266 this function does nothing, because we can only do "busy waiting" on ESP32
#define MAX_IDLE_WAIT_MS 50  // seems to work in most cases
void WS2812FX::waitUntilIdle(void) {
#if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_PROTECT_SERVICE)
  if (isServicing()) {
    unsigned long waitStarted = millis();
    do {
      delay(2);  // Suspending for 1 tick (or more) gives other tasks a chance to run.
      //yield(); // seems to be a no-op on esp32
    } while (isServicing() && (millis() - waitStarted < MAX_IDLE_WAIT_MS));
    USER_PRINTF("strip.waitUntilIdle(): strip %sidle after %d ms. (task %s with prio=%d)\n", isServicing()?"not ":"", int(millis() - waitStarted), pcTaskGetTaskName(NULL), uxTaskPriorityGet(NULL));
  }
  return;
#else
  return;
#endif
}

void WS2812FX::service() {
  unsigned long nowUp = millis(); // Be aware, millis() rolls over every 49 days // WLEDMM avoid losing precision
  if (OTAisRunning) return; // WLEDMM avoid flickering during OTA

  now = nowUp + timebase;
  unsigned long elapsed = nowUp - _lastServiceShow;
  #if defined(ARDUINO_ARCH_ESP32) && defined(WLEDMM_FASTPATH)   // WLEDMM go faster on ESP32
  //if (_suspend) return;
  if (elapsed < 2) return;                                                       // keep wifi alive
  if ( !_triggered && (_targetFps != FPS_UNLIMITED) && (_targetFps != FPS_UNLIMITED_AC)) {
#if 0
    if (elapsed < MIN_SHOW_DELAY) return;                                        // WLEDMM too early for service - delivers higher fps
#else
    if ((elapsed+1) < _frametime) return;                                            // code from upstream - stricter on FPS
#endif
  }
  #else  // legacy
  if (elapsed < _frametime) return;
  #endif

  bool doShow = false;
  unsigned speedLimit = (_targetFps != FPS_UNLIMITED) && (_targetFps != FPS_UNLIMITED_AC) ? (0.85f * FRAMETIME) : 1;      // WLEDMM minimum for effect frametime

  _isServicing = true;
  _segment_index = 0;
  for (segment &seg : _segments) {
#ifdef WLEDMM_FASTPATH
    _currentSeg = &seg;
#endif
    // reset the segment runtime data if needed
    seg.resetIfRequired();

    if (!seg.isActive()) continue;
    if (!seg.on && !seg.transitional) continue;    // WLEDMM skip disabled segments, unless a crossfade is ongoing

    // last condition ensures all solid segments are updated at the same time
    if(nowUp >= seg.next_time || _triggered || (doShow && seg.mode == FX_MODE_STATIC))  // WLEDMM ">=" instead of ">"
    {
      if (seg.grouping == 0) seg.grouping = 1; //sanity check
      if (!seg.freeze) doShow = true;
      uint16_t frameDelay = FRAMETIME;    // WLEDMM avoid name clash with "delay" function

      if (!seg.freeze) { //only run effect function if not frozen
        _virtualSegmentLength = seg.calc_virtualLength();
        _colors_t[0] = seg.currentColor(0, seg.colors[0]);
        _colors_t[1] = seg.currentColor(1, seg.colors[1]);
        _colors_t[2] = seg.currentColor(2, seg.colors[2]);
        seg.setCurrentPalette();              // load actual palette

        if (!cctFromRgb || correctWB) busses.setSegmentCCT(seg.currentBri(seg.cct, true), correctWB);
        for (uint8_t c = 0; c < NUM_COLORS; c++) _colors_t[c] = gamma32(_colors_t[c]);
#if 0  // WARNING this would kill _supersync_
        now = millis() + timebase;
#endif
        seg.startFrame();   // WLEDMM
        if (!_triggered && (seg.currentBri(seg.opacity) == 0) && (seg.lastBri == 0)) continue; // WLEDMM skip totally black segments
        // effect blending (execute previous effect)
        // actual code may be a bit more involved as effects have runtime data including allocated memory
        //if (seg.transitional && seg._modeP) (*_mode[seg._modeP])(progress());
        frameDelay = (*_mode[seg.currentMode(seg.mode)])();

        if (frameDelay < speedLimit) frameDelay = FRAMETIME;                    // WLEDMM limit effects that want to go faster than target FPS
        if (seg.mode != FX_MODE_HALLOWEEN_EYES) seg.call++;

        if (seg.transitional && frameDelay > max(int(FRAMETIME), int(FRAMETIME_FIXED))) 
          frameDelay = max(int(FRAMETIME), int(FRAMETIME_FIXED)); // force faster updates during transition // WLEDMM only if effect requested very slow updates

        seg.lastBri = seg.currentBri(seg.on ? seg.opacity:0);                   // WLEDMM remember for next time
        seg.handleTransition();
      }

      seg.next_time = nowUp + frameDelay;
    }
    _segment_index++;
  }
  _virtualSegmentLength = 0;
  busses.setSegmentCCT(-1);
  if(doShow) {
#if 0 && defined(ARDUINO_ARCH_ESP32)      // EXPERIMENTAL - enabled this to enforce stricter frametime limits
    static unsigned long lastTimeShow = 0;
    long tdelta = millis() - lastTimeShow;
    if ((lastTimeShow > 0) && (tdelta > 1) && (tdelta < _frametime))  // too early - release CPU to slow down
      vTaskDelay((tdelta-1) / portTICK_PERIOD_MS);                    // "-1" because vTaskDelay() may actually delay longer than requested
    lastTimeShow = millis();
#else
    yield();
#endif
    show();
    _lastServiceShow = nowUp; // WLEDMM use correct timestamp
  }
  _triggered = false;
  _isServicing = false;
}

void IRAM_ATTR WS2812FX::setPixelColor(int i, uint32_t col)
{
  if (i < customMappingSize) i = customMappingTable[i];
  if (i >= _length) return;
  busses.setPixelColor(i, col);
}

uint32_t WS2812FX::getPixelColor(uint_fast16_t i) const // WLEDMM fast int types
{
  if (i < customMappingSize) i = customMappingTable[i];
  if (i >= _length) return 0;
  return busses.getPixelColor(i);
}

uint32_t WS2812FX::getPixelColorRestored(uint_fast16_t i)  const  // WLEDMM gets the original color from the driver (without downscaling by _bri)
{
  if (i < customMappingSize) i = customMappingTable[i];
  if (i >= _length) return 0;
  return busses.getPixelColorRestored(i);
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

  for (uint_fast8_t bNum = 0; bNum < busses.getNumBusses(); bNum++) {
    Bus *bus = busses.getBus(bNum);
    auto btype = bus->getType();
    if (EXCLUDE_FROM_ABL(btype)) continue; // WLEDMM exclude non-ABL and network busses
    uint16_t len = bus->getLength();
    uint32_t busPowerSum = 0;
    for (uint_fast16_t i = 0; i < len; i++) { //sum up the usage of each LED
      uint32_t c = bus->getPixelColor(i);
      byte r = R(c), g = G(c), b = B(c), w = W(c);

      if(useWackyWS2815PowerModel) { //ignore white component on WS2815 power calculation
        busPowerSum += (max(max(r,g),b)) * 3; // WLEDMM use native min/max
      } else {
        busPowerSum += (r + g + b + w);
      }
    }

    if (bus->hasWhite()) { //RGBW led total output with white LEDs enabled is still 50mA, so each channel uses less
      busPowerSum *= 3;
      busPowerSum = busPowerSum >> 2; //same as /= 4
    }
    powerSum += busPowerSum;
  }

  uint32_t powerSum0 = powerSum;
  //powerSum *= _brightness; // for NPBrightnessBus
  powerSum *= 255;           // no need to scale down powerSum - NPB-LG getPixelColor returns colors scaled down by brightness

  if (powerSum > powerBudget) //scale brightness down to stay in current limit
  {
    float scale = (float)powerBudget / (float)powerSum;
    uint16_t scaleI = scale * 255;
    uint8_t scaleB = (scaleI > 255) ? 255 : scaleI;
    uint8_t newBri = scale8(_brightness, scaleB);
    // to keep brightness uniform, sets virtual busses too - softhack007: apply reductions immediately
    if (scaleB < 255) busses.setBrightness(scaleB, true); // NPB-LG has already applied brightness, so its sufficient to post-apply scaling ==> use scaleB instead of newBri
    busses.setBrightness(newBri, false);                  // set new brightness for next frame
    //currentMilliamps = (powerSum0 * newBri) / puPerMilliamp; // for NPBrightnessBus
    currentMilliamps = (powerSum0 * scaleB) / puPerMilliamp;   // for NPBus-LG
  } else {
    currentMilliamps = powerSum / puPerMilliamp;
    busses.setBrightness(_brightness, false);            // set new brightness for next frame
  }
  currentMilliamps += MA_FOR_ESP; //add power of ESP back to estimate
  currentMilliamps += pLen; //add standby power back to estimate
}

void WS2812FX::show(void) {
  if (OTAisRunning) return; // WLEDMM avoid flickering during OTA

  // avoid race condition, capture _callback value
  show_callback callback = _callback;
  if (callback) callback();

  estimateCurrentAndLimitBri();

  unsigned long showNow = millis();            // include time needed for busses.show()
  #ifdef ARDUINO_ARCH_ESP32                    // WLEDMM more accurate FPS measurement for ESP32
  uint64_t now500 = esp_timer_get_time() / 2;  // native timer; micros /2 -> millis * 500
  #endif

  // some buses send asynchronously and this method will return before
  // all of the data has been sent.
  // See https://github.com/Makuna/NeoPixelBus/wiki/ESP32-NeoMethods#neoesp32rmt-methods
  busses.show();

  unsigned long diff = showNow - _lastShow;
  uint16_t fpsCurr = 200;
  if (diff > 0) fpsCurr = 1000 / diff;
  _cumulativeFps = (3 * _cumulativeFps + fpsCurr +2) >> 2;   // "+2" for proper rounding (2/4 = 0.5)
  _lastShow = showNow;
  _lastServiceShow = showNow;

#ifdef ARDUINO_ARCH_ESP32                      // WLEDMM more accurate FPS measurement for ESP32
  int64_t diff500 = now500 - _lastShow500;
  if ((diff500 > 1) && (diff500 < 800000)) {   // exclude stupid values (timer rollover, major hickups)
    float fpcCurr500 = 500000.0f / float(diff500);
    if (fpcCurr500 > 2)
      _cumulativeFps500 = (3 * _cumulativeFps500 + (500.0 * fpcCurr500)) / 4;  // average for some smoothing
  }
  _lastShow500 = now500;
#endif
}

/**
 * Returns a true value if any of the strips are still being updated.
 * On some hardware (ESP32), strip updates are done asynchronously.
 */
bool WS2812FX::isUpdating() const {
  return !busses.canAllShow();
}

/**
 * Returns the refresh rate of the LED strip. Useful for finding out whether a given setup is fast enough.
 * Only updates on show() or is set to 0 fps if last show is more than 2 secs ago, so accuracy varies
 */
uint16_t WS2812FX::getFps() const {
  if (millis() - _lastShow > 2000) return 0;
#ifdef ARDUINO_ARCH_ESP32
  return ((_cumulativeFps500 + 250) / 500);  // +250 for proper rounding
#else
  return _cumulativeFps +1;
#endif
}

void WS2812FX::setTargetFps(uint8_t fps) {
  if (fps <= 251) _targetFps = fps;  // WLEDMM allow higher framerates
  //if (fps > 0) _frametime = ((2000 / _targetFps) +1) /2;   // with rounding
  if (fps > 0) _frametime = 1000 / _targetFps;
  else _frametime = 2;                          // AC WLED compatibility
  if (fps >= FPS_UNLIMITED) _frametime = 2;     // WLEDMM unlimited mode
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

uint8_t WS2812FX::getLastActiveSegmentId(void) const {
  for (size_t i = _segments.size() -1; i > 0; i--) {
    if (_segments[i].isActive()) return i;
  }
  return 0;
}

uint8_t WS2812FX::getActiveSegmentsNum(void) const {
  uint8_t c = 0;
  for (size_t i = 0; i < _segments.size(); i++) {
    if (_segments[i].isActive()) c++;
  }
  return c;
}

uint16_t WS2812FX::getLengthTotal(void) const {  // WLEDMM fast int types
  uint_fast16_t len = Segment::maxWidth * Segment::maxHeight; // will be _length for 1D (see finalizeInit()) but should cover whole matrix for 2D
  if (isMatrix && _length > len) len = _length; // for 2D with trailing strip
  return len;
}

uint16_t WS2812FX::getLengthPhysical(void) const {  // WLEDMM fast int types
  uint_fast16_t len = 0;
  for (unsigned b = 0; b < busses.getNumBusses(); b++) {   //  WLEDMM use native (fast) types
    Bus *bus = busses.getBus(b);
    auto btype = bus->getType();
    if (EXCLUDE_FROM_ABL(btype)) continue;  //exclude HUB75, and non-physical network busses
    len += bus->getLength();
  }
  return len;
}

//WLEDMM - getLengthPhysical plus plysical busses not supporting ABL (i.e. HUB75)
uint16_t WS2812FX::getLengthPhysical2(void) const {
  uint_fast16_t len = 0;
  for (unsigned b = 0; b < busses.getNumBusses(); b++) {
    Bus *bus = busses.getBus(b);
    auto btype = bus->getType();
    if (IS_VIRTUAL(btype)) continue;
    len += bus->getLength();
  }
  return len;
}

//used for JSON API info.leds.rgbw. Little practical use, deprecate with info.leds.rgbw.
//returns if there is an RGBW bus (supports RGB and White, not only white)
//not influenced by auto-white mode, also true if white slider does not affect output white channel
bool WS2812FX::hasRGBWBus(void) const {
  for (unsigned b = 0; b < busses.getNumBusses(); b++) {    //  WLEDMM use native (fast) types
    Bus *bus = busses.getBus(b);
    if (bus == nullptr || bus->getLength()==0) break;
    if (bus->hasRGB() && bus->hasWhite()) return true;
  }
  return false;
}

bool WS2812FX::hasCCTBus(void) const {
  if (cctFromRgb && !correctWB) return false;
  for (unsigned b = 0; b < busses.getNumBusses(); b++) {    //  WLEDMM use native (fast) types
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
    /*if (_mainSegment >= _segments.size())*/ setMainSegmentId(0);
  }
}

Segment& WS2812FX::getSegment(uint8_t id) {
  uint8_t mainSegID = getMainSegmentId();
  if (mainSegID >= _segments.size()) mainSegID=0;  // WLEDMM fallback in case that getMainSegmentId() is invalid
  return _segments[id >= _segments.size() ? mainSegID : id]; // vectors
}

void WS2812FX::setSegment(uint8_t n, uint16_t i1, uint16_t i2, uint8_t grouping, uint8_t spacing, uint16_t offset, uint16_t startY, uint16_t stopY) {
  if (n >= _segments.size()) return;
  _segments[n].setUp(i1, i2, grouping, spacing, offset, startY, stopY);
}

void WS2812FX::restartRuntime(bool doReset) {
  for (segment &seg : _segments) {
    if (doReset) {   // WLEDMM we prefer not to perform a complete restart of all effects
      seg.markForReset(); // seg.resetIfRequired(); // WLEDMM calling this function from webserver context will cause troubles
    } else {
      seg.next_time = 0; seg.step = 0;
      seg.markForBlank();
    }
  }
}

void WS2812FX::resetSegments(bool boundsOnly) { //WLEDMM add boundsonly
  DEBUG_PRINTF("resetSegments %d %dx%d\n", boundsOnly, Segment::maxWidth, Segment::maxHeight);
  if (!boundsOnly) {
    _segments.clear(); // destructs all Segment as part of clearing
    #ifndef WLED_DISABLE_2D
    segment seg = isMatrix ? Segment(0, Segment::maxWidth, 0, Segment::maxHeight) : Segment(0, _length);
    #else
    segment seg = Segment(0, _length);
    #endif
    _segments.push_back(seg);
    _mainSegment = 0;
  } else { //WLEDMM boundsonly
    for (segment &seg : _segments) {
      #ifndef WLED_DISABLE_2D
      seg.start = 0;
      seg.stop = Segment::maxWidth;
      seg.startY = 0;
      seg.stopY = Segment::maxHeight;
      #else
      seg.start = 0;
      seg.stop = _length;
      #endif
      seg.allocLeds();
    }
  }
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
    _virtualSegmentLength = _segments[_segment_index].calc_virtualLength();
  }
  return prevSegId;
}

void WS2812FX::setRange(uint16_t i, uint16_t i2, uint32_t col) {
  if (i2 >= i)
  {
    for (uint_fast16_t x = i; x <= i2; x++) setPixelColor((uint16_t)x, col);    //  WLEDMM use fast int types
  } else
  {
    for (uint_fast16_t x = i2; x <= i; x++) setPixelColor((uint16_t)x, col);
  }
}

void WS2812FX::setTransitionMode(bool t) {
  for (segment &seg : _segments) if (!seg.transitional) seg.startTransition(t ? _transitionDur : 0);
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
  if (useLedsArray) DEBUG_PRINTF("Buffer: %d*%u=%uB\n", sizeof(CRGB), size, size*sizeof(CRGB));
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
            size_t palSize = min(pal.size(), (size_t)36);  // WLEDMM use native min/max
            palSize -= palSize % 2; // make sure size is multiple of 2
            for (unsigned i=0, j=0; i<palSize && pal[i].as<int>()<256; i+=2, j+=4) {
              uint8_t rgbw[] = {0,0,0,0};
              tcp[ j ] = (uint8_t) pal[ i ].as<int>(); // index
              colorFromHexString(rgbw, pal[i+1].as<const char *>()); // will catch non-string entires
              for (unsigned c=0; c<3; c++) tcp[j+1+c] = gamma8(rgbw[c]); // only use RGB component
              DEBUG_PRINTF("%d(%d) : %d %d %d\n", i, int(tcp[j]), int(tcp[j+1]), int(tcp[j+2]), int(tcp[j+3]));
            }
          } else {
            size_t palSize = min(pal.size(), (size_t)72);    // WLEDMM use native min/max
            palSize -= palSize % 4; // make sure size is multiple of 4
            for (size_t i=0; i<palSize && pal[i].as<int>()<256; i+=4) {
              tcp[ i ] = (uint8_t) pal[ i ].as<int>(); // index
              tcp[i+1] = gamma8((uint8_t) pal[i+1].as<int>()); // R
              tcp[i+2] = gamma8((uint8_t) pal[i+2].as<int>()); // G
              tcp[i+3] = gamma8((uint8_t) pal[i+3].as<int>()); // B
              DEBUG_PRINTF("%d(%d) : %d %d %d\n", i, int(tcp[i]), int(tcp[i+1]), int(tcp[i+2]), int(tcp[i+3]));
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
bool WS2812FX::deserializeMap(uint8_t n) {
  // 2D support creates its own ledmap (on the fly) if a ledmap.json exists it will overwrite built one.

  char fileName[32] = {'\0'};
  //WLEDMM: als support segment name ledmaps
  bool isFile = false;;
  if (n<10) {
    strcpy_P(fileName, PSTR("/ledmap"));
    if (n) sprintf(fileName +7, "%d", n); //WLEDMM: trick to not include 0 in ledmap.json
    strcat(fileName, ".json");
    isFile = WLED_FS.exists(fileName);
  } else { //WLEDMM add segment name as ledmap.name
    uint8_t segment_index = 0;
    for (segment &seg : _segments) {
      if (n == 10 + segment_index && !isFile && seg.name != nullptr) {
        sprintf_P(fileName, PSTR("/%s.json"), seg.name);
        isFile = WLED_FS.exists(fileName);
      }
      if (isFile) break;
      segment_index++;
    }
  }

  if (!isFile) {
    // erase custom mapping if selecting nonexistent ledmap.json (n==0)
    //WLEDMM: doubt this is necessary as return false causes setupMatrix to deal with this !!!!
    if (!isMatrix && !n) {
      customMappingSize = 0;
      loadedLedmap = 0; //WLEDMM
    }
    return false;
  }

  if (!requestJSONBufferLock(7)) return false;

  // WLEDMM: before changing maps, make sure our strip is _not_ servicing effects in parallel
  if (strip.isServicing()) {
    USER_PRINTLN(F("deserializeMap(): strip is still drawing effects, waiting ..."));
    strip.waitUntilIdle();
  }

  //WLEDMM: change upstream code: do not load complete ledmaps in json as this blows up memory, use file read instead
  //read the file
  File f;
  f = WLED_FS.open(fileName, "r");
  if (!f) {
    releaseJSONBufferLock();
    return false; //if file does not exist just exit
  }

  USER_PRINT(F("Reading LED map from ")); //WLEDMM use USER_PRINT
  USER_PRINTLN(fileName);

  if (isMatrix) {
    //WLEDMM: read width and height
    memset(fileName, 0, sizeof(fileName));              // clear old buffer - readBytesUntil() does not terminate strings !!!
    f.find("\"width\":");
    f.readBytesUntil('\n', fileName, sizeof(fileName)); //hack: use fileName as we have this allocated already
    uint16_t maxWidth = atoi(cleanUpName(fileName));
    //DEBUG_PRINTF(" (\"width\": %s) ", fileName)

    memset(fileName, 0, sizeof(fileName));              // clear old buffer
    f.find("\"height\":");
    f.readBytesUntil('\n', fileName, sizeof(fileName));
    uint16_t maxHeight = atoi(cleanUpName(fileName));
    //DEBUG_PRINTF(" (\"height\": %s) \n", fileName)

    #ifndef WLEDMM_NO_MAP_RESET
    //WLEDMM: support ledmap file properties width and height: if found change segment
    if (maxWidth * maxHeight > 0) {
      Segment::maxWidth = maxWidth;
      Segment::maxHeight = maxHeight;
      resetSegments(true); //WLEDMM not makeAutoSegments() as we only want to change bounds
    }
    else
      setUpMatrix(); //reset segment sizes to panels
    #endif
  }

  USER_PRINTF("deserializeMap %d x %d\n", Segment::maxWidth, Segment::maxHeight);

  //WLEDMM recreate customMappingTable if more space needed
  if (Segment::maxWidth * Segment::maxHeight > customMappingTableSize) {
    size_t size = max(ledmapMaxSize, size_t(Segment::maxWidth * Segment::maxHeight)); // TroyHacks
    USER_PRINTF("deserializemap customMappingTable alloc %u from %u\n", size, customMappingTableSize);
    //if (customMappingTable != nullptr) delete[] customMappingTable;
    //customMappingTable = new(std::nothrow) uint16_t[size];

    // don't use new / delete
    if ((size > 0) && (customMappingTable != nullptr)) {
      customMappingTable = (uint16_t*) reallocf(customMappingTable, sizeof(uint16_t) * size);  // reallocf will free memory if it cannot resize
    }
    if ((size > 0) && (customMappingTable == nullptr)) { // second try
      DEBUG_PRINTLN("deserializeMap: trying to get fresh memory block.");
      customMappingTable = (uint16_t*) calloc(size, sizeof(uint16_t));
      if (customMappingTable == nullptr) { 
        DEBUG_PRINTLN("deserializeMap: alloc failed!");
        errorFlag = ERR_LOW_MEM; // WLEDMM raise errorflag
      }
    }
    if (customMappingTable != nullptr) customMappingTableSize = size;
  }

  if (customMappingTable != nullptr) {
    customMappingSize  = Segment::maxWidth * Segment::maxHeight;
    // WLEDMM reset mapping table before loading
    //memset(customMappingTable, 0xFF, customMappingTableSize * sizeof(uint16_t)); // FFFF = no pixel
    for (unsigned i=0; i<customMappingTableSize; i++) customMappingTable[i]=i;     // "neutral" 1:1 mapping

    //WLEDMM: find the map values
    f.find("\"map\":[");
    uint16_t i=0;
    do { //for each element in the array
      int mapi = f.readStringUntil(',').toInt();
      // USER_PRINTF(", %d(%d)", mapi, i);
      if (i < customMappingSize) customMappingTable[i++] = (uint16_t) (mapi<0 ? 0xFFFFU : mapi);  // WLEDMM do not write past array bounds
    } while (f.available());

    loadedLedmap = n;
    f.close();

    USER_PRINTF("Custom ledmap: %d size=%d\n", loadedLedmap, customMappingSize);
    #ifdef WLED_DEBUG_MAPS
      for (uint16_t j=0; j<customMappingSize; j++) { // fixing a minor warning: declaration of 'i' shadows a previous local
        if (!(j%Segment::maxWidth)) DEBUG_PRINTLN();
        DEBUG_PRINTF("%4d,", customMappingTable[j]);
      }
      DEBUG_PRINTLN();
    #endif
  } else { // memory allocation error
    customMappingTableSize = 0;
    USER_PRINTLN(F("Deserializemap: Ledmap alloc error."));
    USER_FLUSH();
  }

  releaseJSONBufferLock();
  return true;
}


WS2812FX* WS2812FX::instance = nullptr;

const char JSON_mode_names[] PROGMEM = R"=====(["FX names moved"])=====";
 //WLEDMM netmindz ar palette add Audio responsive
const char JSON_palette_names[] PROGMEM = R"=====([
"Default","* Random Smooth ☾","* Color 1","* Colors 1&2","* Color Gradient","* Colors Only","Party","Cloud","Lava","Ocean",
"Forest","Rainbow","Rainbow Bands","Sunset","Rivendell","Breeze","Red & Blue","Yellowout","Analogous","Splash",
"Pastel","Sunset 2","Beach","Vintage","Departure","Landscape","Beech","Sherbet","Hult","Hult 64",
"Drywet","Jul","Grintage","Rewhi","Tertiary","Fire","Icefire","Cyane","Light Pink","Autumn",
"Magenta","Magred","Yelmag","Yelblu","Orange & Teal","Tiamat","April Night","Orangery","C9","Sakura",
"Aurora","Atlantica","C9 2","C9 New","Temperature","Aurora 2","Retro Clown","Candy","Toxy Reaf","Fairy Reaf",
"Semi Blue","Pink Candy","Red Reaf","Aqua Flash","Yelblu Hot","Lite Light","Red Flash","Blink Red","Red Shift","Red Tide",
"Candy2","Audio Responsive Ratio ☾","Audio Responsive Hue ☾","Audio Responsive Ramp ☾","* Random Cycle"
])=====";
