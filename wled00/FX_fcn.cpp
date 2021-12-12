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

#if MAX_NUM_SEGMENTS < WLED_MAX_BUSSES
  #error "Max segments must be at least max number of busses!"
#endif

//do not call this method from system context (network callback)
void WS2812FX::finalizeInit(void)
{
  RESET_RUNTIME;
  isRgbw = isOffRefreshRequred = false;

  //if busses failed to load, add default (fresh install, FS issue, ...)
  if (busses.getNumBusses() == 0) {
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
      BusConfig defCfg = BusConfig(DEFAULT_LED_TYPE, defPin, start, count, COL_ORDER_GRB);
      busses.add(defCfg);
    }
  }

  _length = 0;
  for (uint8_t i=0; i<busses.getNumBusses(); i++) {
    Bus *bus = busses.getBus(i);
    if (bus == nullptr) continue;
    if (bus->getStart() + bus->getLength() > MAX_LEDS) break;
    //RGBW mode is enabled if at least one of the strips is RGBW
    isRgbw |= bus->isRgbw();
    //refresh is required to remain off if at least one of the strips requires the refresh.
    isOffRefreshRequred |= bus->isOffRefreshRequired();
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

  //segments are created in makeAutoSegments();

  setBrightness(_brightness);
}

void WS2812FX::service() {
  uint32_t nowUp = millis(); // Be aware, millis() rolls over every 49 days
  now = nowUp + timebase;
  if (nowUp - _lastShow < MIN_SHOW_DELAY) return;
  bool doShow = false;

  for(uint8_t i=0; i < MAX_NUM_SEGMENTS; i++)
  {
    _segment_index = i;

    // reset the segment runtime data if needed, called before isActive to ensure deleted
    // segment's buffers are cleared
    SEGENV.resetIfRequired();

    if (!SEGMENT.isActive()) continue;

    if(nowUp > SEGENV.next_time || _triggered || (doShow && SEGMENT.mode == 0)) //last is temporary
    {
      if (SEGMENT.grouping == 0) SEGMENT.grouping = 1; //sanity check
      doShow = true;
      uint16_t delay = FRAMETIME;

      if (!SEGMENT.getOption(SEG_OPTION_FREEZE)) { //only run effect function if not frozen
        _virtualSegmentLength = SEGMENT.virtualLength();
        _bri_t = SEGMENT.opacity; _colors_t[0] = SEGMENT.colors[0]; _colors_t[1] = SEGMENT.colors[1]; _colors_t[2] = SEGMENT.colors[2];
        uint8_t _cct_t = SEGMENT.cct;
        if (!IS_SEGMENT_ON) _bri_t = 0;
        for (uint8_t t = 0; t < MAX_NUM_TRANSITIONS; t++) {
          if ((transitions[t].segment & 0x3F) != i) continue;
          uint8_t slot = transitions[t].segment >> 6;
          if (slot == 0) _bri_t = transitions[t].currentBri();
          if (slot == 1) _cct_t = transitions[t].currentBri(false, 1);
          _colors_t[slot] = transitions[t].currentColor(SEGMENT.colors[slot]);
        }
        if (!cctFromRgb || correctWB) busses.setSegmentCCT(_cct_t, correctWB);
        for (uint8_t c = 0; c < 3; c++) _colors_t[c] = gamma32(_colors_t[c]);
        handle_palette();
        delay = (this->*_mode[SEGMENT.mode])(); //effect function
        if (SEGMENT.mode != FX_MODE_HALLOWEEN_EYES) SEGENV.call++;
      }

      SEGENV.next_time = nowUp + delay;
    }
  }
  _virtualSegmentLength = 0;
  busses.setSegmentCCT(-1);
  if(doShow) {
    yield();
    show();
  }
  _triggered = false;
}

void WS2812FX::setPixelColor(uint16_t n, uint32_t c) {
  setPixelColor(n, R(c), G(c), B(c), W(c));
}

//used to map from segment index to physical pixel, taking into account grouping, offsets, reverse and mirroring
uint16_t WS2812FX::realPixelIndex(uint16_t i) {
  int16_t iGroup = i * SEGMENT.groupLength();

  /* reverse just an individual segment */
  int16_t realIndex = iGroup;
  if (IS_REVERSE) {
    if (IS_MIRROR) {
      realIndex = (SEGMENT.length() - 1) / 2 - iGroup;  //only need to index half the pixels
    } else {
      realIndex = (SEGMENT.length() - 1) - iGroup;
    }
  }

  realIndex += SEGMENT.start;
  return realIndex;
}

void WS2812FX::setPixelColor(uint16_t i, byte r, byte g, byte b, byte w)
{
  if (SEGLEN) {//from segment
    uint16_t realIndex = realPixelIndex(i);
    uint16_t len = SEGMENT.length();

    //color_blend(getpixel, col, _bri_t); (pseudocode for future blending of segments)
    if (_bri_t < 255) {  
      r = scale8(r, _bri_t);
      g = scale8(g, _bri_t);
      b = scale8(b, _bri_t);
      w = scale8(w, _bri_t);
    }
    uint32_t col = RGBW32(r, g, b, w);

    /* Set all the pixels in the group */
    for (uint16_t j = 0; j < SEGMENT.grouping; j++) {
      uint16_t indexSet = realIndex + (IS_REVERSE ? -j : j);
      if (indexSet >= SEGMENT.start && indexSet < SEGMENT.stop) {
        if (IS_MIRROR) { //set the corresponding mirrored pixel
          uint16_t indexMir = SEGMENT.stop - indexSet + SEGMENT.start - 1;
          /* offset/phase */
          indexMir += SEGMENT.offset;
          if (indexMir >= SEGMENT.stop) indexMir -= len;

          if (indexMir < customMappingSize) indexMir = customMappingTable[indexMir];
          busses.setPixelColor(indexMir, col);
        }
        /* offset/phase */
        indexSet += SEGMENT.offset;
        if (indexSet >= SEGMENT.stop) indexSet -= len;

        if (indexSet < customMappingSize) indexSet = customMappingTable[indexSet];
        busses.setPixelColor(indexSet, col);
      }
    }
  } else { //live data, etc.
    if (i < customMappingSize) i = customMappingTable[i];
    busses.setPixelColor(i, RGBW32(r, g, b, w));
  }
}


//DISCLAIMER
//The following function attemps to calculate the current LED power usage,
//and will limit the brightness to stay below a set amperage threshold.
//It is NOT a measurement and NOT guaranteed to stay within the ablMilliampsMax margin.
//Stay safe with high amperage and have a reasonable safety margin!
//I am NOT to be held liable for burned down garages!

//fine tune power estimation constants for your setup                  
#define MA_FOR_ESP        80 //how much mA does the ESP use (Wemos D1 about 80mA, ESP32 about 120mA)
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

/**
 * Forces the next frame to be computed on all active segments.
 */
void WS2812FX::trigger() {
  _triggered = true;
}

void WS2812FX::setMode(uint8_t segid, uint8_t m) {
  if (segid >= MAX_NUM_SEGMENTS) return;
   
  if (m >= MODE_COUNT) m = MODE_COUNT - 1;

  if (_segments[segid].mode != m) 
  {
    _segment_runtimes[segid].reset();
    _segments[segid].mode = m;
  }
}

uint8_t WS2812FX::getModeCount()
{
  return MODE_COUNT;
}

uint8_t WS2812FX::getPaletteCount()
{
  return 13 + GRADIENT_PALETTE_COUNT;
}

//TODO effect transitions


bool WS2812FX::setEffectConfig(uint8_t m, uint8_t s, uint8_t in, uint8_t p) {
  Segment& seg = _segments[getMainSegmentId()];
  uint8_t modePrev = seg.mode, speedPrev = seg.speed, intensityPrev = seg.intensity, palettePrev = seg.palette;

  bool applied = false;
  
  if (applyToAllSelected) {
    for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++)
    {
      if (_segments[i].isSelected())
      {
        _segments[i].speed = s;
        _segments[i].intensity = in;
        _segments[i].palette = p;
        setMode(i, m);
        applied = true;
      }
    }
  } 
  
  if (!applyToAllSelected || !applied) {
    seg.speed = s;
    seg.intensity = in;
    seg.palette = p;
    setMode(mainSegment, m);
  }
  
  if (seg.mode != modePrev || seg.speed != speedPrev || seg.intensity != intensityPrev || seg.palette != palettePrev) return true;
  return false;
}

void WS2812FX::setColor(uint8_t slot, uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
  setColor(slot, RGBW32(r, g, b, w));
}

void WS2812FX::setColor(uint8_t slot, uint32_t c) {
  if (slot >= NUM_COLORS) return;

  bool applied = false;
  
  if (applyToAllSelected) {
    for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++)
    {
      if (_segments[i].isSelected()) {
        _segments[i].setColor(slot, c, i);
        applied = true;
      }
    }
  }

  if (!applyToAllSelected || !applied) {
    uint8_t mainseg = getMainSegmentId();
    _segments[mainseg].setColor(slot, c, mainseg);
  }
}

void WS2812FX::setBrightness(uint8_t b) {
  if (gammaCorrectBri) b = gamma8(b);
  if (_brightness == b) return;
  _brightness = b;
  if (_brightness == 0) { //unfreeze all segments on power off
    for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++)
    {
      _segments[i].setOption(SEG_OPTION_FREEZE, false);
    }
  }
	unsigned long t = millis();
  if (_segment_runtimes[0].next_time > t + 22 && t - _lastShow > MIN_SHOW_DELAY) show(); //apply brightness change immediately if no refresh soon
}

uint8_t WS2812FX::getMode(void) {
  return _segments[getMainSegmentId()].mode;
}

uint8_t WS2812FX::getSpeed(void) {
  return _segments[getMainSegmentId()].speed;
}

uint8_t WS2812FX::getBrightness(void) {
  return _brightness;
}

uint8_t WS2812FX::getMaxSegments(void) {
  return MAX_NUM_SEGMENTS;
}

/*uint8_t WS2812FX::getFirstSelectedSegment(void)
{
  for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++)
  {
    if (_segments[i].isActive() && _segments[i].isSelected()) return i;
  }
  for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++) //if none selected, get first active
  {
    if (_segments[i].isActive()) return i;
  }
  return 0;
}*/

uint8_t WS2812FX::getMainSegmentId(void) {
  if (mainSegment >= MAX_NUM_SEGMENTS) return 0;
  if (_segments[mainSegment].isActive()) return mainSegment;
  for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++) //get first active
  {
    if (_segments[i].isActive()) return i;
  }
  return 0;
}

uint8_t WS2812FX::getActiveSegmentsNum(void) {
  uint8_t c = 0;
  for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++)
  {
    if (_segments[i].isActive()) c++;
  }
  return c;
}

uint32_t WS2812FX::getColor(void) {
  return _segments[getMainSegmentId()].colors[0];
}

uint32_t WS2812FX::getPixelColor(uint16_t i)
{
  i = realPixelIndex(i);

  if (SEGLEN) {
    /* offset/phase */
    i += SEGMENT.offset;
    if (i >= SEGMENT.stop) i -= SEGMENT.length();
  }
  
  if (i < customMappingSize) i = customMappingTable[i];
  if (i >= _length) return 0;
  
  return busses.getPixelColor(i);
}

WS2812FX::Segment& WS2812FX::getSegment(uint8_t id) {
  if (id >= MAX_NUM_SEGMENTS) return _segments[0];
  return _segments[id];
}

WS2812FX::Segment_runtime WS2812FX::getSegmentRuntime(void) {
  return SEGENV;
}

WS2812FX::Segment* WS2812FX::getSegments(void) {
  return _segments;
}

uint32_t WS2812FX::getLastShow(void) {
  return _lastShow;
}

uint16_t WS2812FX::getLengthTotal(void) {
  return _length;
}

uint16_t WS2812FX::getLengthPhysical(void) {
  uint16_t len = 0;
  for (uint8_t b = 0; b < busses.getNumBusses(); b++) {
    Bus *bus = busses.getBus(b);
    if (bus->getType() >= TYPE_NET_DDP_RGB) continue; //exclude non-physical network busses
    len += bus->getLength();
  }
  return len;
}

bool WS2812FX::hasCCTBus(void) {
	if (cctFromRgb && !correctWB) return false;
	for (uint8_t b = 0; b < busses.getNumBusses(); b++) {
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

void WS2812FX::setSegment(uint8_t n, uint16_t i1, uint16_t i2, uint8_t grouping, uint8_t spacing, uint16_t offset) {
  if (n >= MAX_NUM_SEGMENTS) return;
  Segment& seg = _segments[n];

  //return if neither bounds nor grouping have changed
  if (seg.start == i1 && seg.stop == i2
			&& (!grouping || (seg.grouping == grouping && seg.spacing == spacing))
			&& (offset == UINT16_MAX || offset == seg.offset)) return;

  if (seg.stop) setRange(seg.start, seg.stop -1, 0); //turn old segment range off
  if (i2 <= i1) //disable segment
  {
    seg.stop = 0;
    if (seg.name) {
      delete[] seg.name;
      seg.name = nullptr;
    }
    if (n == mainSegment) //if main segment is deleted, set first active as main segment
    {
      for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++)
      {
        if (_segments[i].isActive()) {
          mainSegment = i;
          return;
        }
      }
      mainSegment = 0; //should not happen (always at least one active segment)
    }
    return;
  }
  if (i1 < _length) seg.start = i1;
  seg.stop = i2;
  if (i2 > _length) seg.stop = _length;
  if (grouping) {
    seg.grouping = grouping;
    seg.spacing = spacing;
  }
	if (offset < UINT16_MAX) seg.offset = offset;
  _segment_runtimes[n].reset();
}

void WS2812FX::resetSegments() {
  for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++) if (_segments[i].name) delete _segments[i].name;
  mainSegment = 0;
  memset(_segments, 0, sizeof(_segments));
  //memset(_segment_runtimes, 0, sizeof(_segment_runtimes));
  _segment_index = 0;
  _segments[0].mode = DEFAULT_MODE;
  _segments[0].colors[0] = DEFAULT_COLOR;
  _segments[0].start = 0;
  _segments[0].speed = DEFAULT_SPEED;
  _segments[0].intensity = DEFAULT_INTENSITY;
  _segments[0].stop = _length;
  _segments[0].grouping = 1;
  _segments[0].setOption(SEG_OPTION_SELECTED, 1);
  _segments[0].setOption(SEG_OPTION_ON, 1);
  _segments[0].opacity = 255;
  _segments[0].cct = 127;

  for (uint16_t i = 1; i < MAX_NUM_SEGMENTS; i++)
  {
    _segments[i].colors[0] = color_wheel(i*51);
    _segments[i].grouping = 1;
    _segments[i].setOption(SEG_OPTION_ON, 1);
    _segments[i].opacity = 255;
    _segments[i].cct = 127;
    _segments[i].speed = DEFAULT_SPEED;
    _segments[i].intensity = DEFAULT_INTENSITY;
    _segment_runtimes[i].reset();
  }
  _segment_runtimes[0].reset();
}

void WS2812FX::makeAutoSegments() {
  if (autoSegments) { //make one segment per bus
    uint16_t segStarts[MAX_NUM_SEGMENTS] = {0};
    uint16_t segStops [MAX_NUM_SEGMENTS] = {0};
    uint8_t s = 0;
    for (uint8_t i = 0; i < busses.getNumBusses(); i++) {
      Bus* b = busses.getBus(i);

      segStarts[s] = b->getStart();
      segStops[s] = segStarts[s] + b->getLength();

      //check for overlap with previous segments
      for (uint8_t j = 0; j < s; j++) {
        if (segStops[j] > segStarts[s] && segStarts[j] < segStops[s]) {
          //segments overlap, merge
          segStarts[j] = min(segStarts[s],segStarts[j]);
          segStops [j] = max(segStops [s],segStops [j]); segStops[s] = 0;
          s--;
        }
      }
      s++;
    }
    for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++) {
      setSegment(i, segStarts[i], segStops[i]);
    }
  } else {
    //expand the main seg to the entire length, but only if there are no other segments
    uint8_t mainSeg = getMainSegmentId();
    
    if (getActiveSegmentsNum() < 2) {
      setSegment(mainSeg, 0, _length);
    }
  }

  fixInvalidSegments();
}

void WS2812FX::fixInvalidSegments() {
  //make sure no segment is longer than total (sanity check)
  for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++)
  {
    if (_segments[i].start >= _length) setSegment(i, 0, 0); 
    if (_segments[i].stop  >  _length) setSegment(i, _segments[i].start, _length);
  }
}

//true if all segments align with a bus, or if a segment covers the total length
bool WS2812FX::checkSegmentAlignment() {
  for (uint8_t i = 0; i < MAX_NUM_SEGMENTS; i++)
  {
    if (_segments[i].start >= _segments[i].stop) continue; //inactive segment
    bool aligned = false;
    for (uint8_t b = 0; b<busses.getNumBusses(); b++) {
      Bus *bus = busses.getBus(b);
      if (_segments[i].start == bus->getStart() && _segments[i].stop == bus->getStart() + bus->getLength()) aligned = true;
    }
    if (_segments[i].start == 0 && _segments[i].stop == _length) aligned = true;
    if (!aligned) return false;
  }
  return true;
}

//After this function is called, setPixelColor() will use that segment (offsets, grouping, ... will apply)
//Note: If called in an interrupt (e.g. JSON API), it must be reset with "setPixelColor(255)",
//otherwise it can lead to a crash on ESP32 because _segment_index is modified while in use by the main thread
#ifdef ARDUINO_ARCH_ESP32
uint8_t _segment_index_prev = 0;
uint16_t _virtualSegmentLength_prev = 0;
bool _ps_set = false;
#endif

void WS2812FX::setPixelSegment(uint8_t n)
{
  if (n < MAX_NUM_SEGMENTS) {
		#ifdef ARDUINO_ARCH_ESP32
		if (!_ps_set) {
			_segment_index_prev = _segment_index;
			_virtualSegmentLength_prev = _virtualSegmentLength;
			_ps_set = true;
		}
		#endif
    _segment_index = n;
    _virtualSegmentLength = SEGMENT.virtualLength();
  } else {
		_virtualSegmentLength = 0;
		#ifdef ARDUINO_ARCH_ESP32
		if (_ps_set) {
			_segment_index = _segment_index_prev;
			_virtualSegmentLength = _virtualSegmentLength_prev;
			_ps_set = false;
		}
		#endif
  }
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

void WS2812FX::setShowCallback(show_callback cb)
{
  _callback = cb;
}

void WS2812FX::setTransition(uint16_t t)
{
  _transitionDur = t;
}

void WS2812FX::setTransitionMode(bool t)
{
	unsigned long waitMax = millis() + 20; //refresh after 20 ms if transition enabled
  for (uint16_t i = 0; i < MAX_NUM_SEGMENTS; i++)
  {
    _segments[i].setOption(SEG_OPTION_TRANSITIONAL, t);

    if (t && _segments[i].mode == FX_MODE_STATIC && _segment_runtimes[i].next_time > waitMax)
			_segment_runtimes[i].next_time = waitMax;
  }
}

/*
 * color blend function
 */
uint32_t WS2812FX::color_blend(uint32_t color1, uint32_t color2, uint16_t blend, bool b16) {
  if(blend == 0)   return color1;
  uint16_t blendmax = b16 ? 0xFFFF : 0xFF;
  if(blend == blendmax) return color2;
  uint8_t shift = b16 ? 16 : 8;

  uint32_t w1 = W(color1);
  uint32_t r1 = R(color1);
  uint32_t g1 = G(color1);
  uint32_t b1 = B(color1);

  uint32_t w2 = W(color2);
  uint32_t r2 = R(color2);
  uint32_t g2 = G(color2);
  uint32_t b2 = B(color2);

  uint32_t w3 = ((w2 * blend) + (w1 * (blendmax - blend))) >> shift;
  uint32_t r3 = ((r2 * blend) + (r1 * (blendmax - blend))) >> shift;
  uint32_t g3 = ((g2 * blend) + (g1 * (blendmax - blend))) >> shift;
  uint32_t b3 = ((b2 * blend) + (b1 * (blendmax - blend))) >> shift;

  return RGBW32(r3, g3, b3, w3);
}

/*
 * Fills segment with color
 */
void WS2812FX::fill(uint32_t c) {
  for(uint16_t i = 0; i < SEGLEN; i++) {
    setPixelColor(i, c);
  }
}

/*
 * Blends the specified color with the existing pixel color.
 */
void WS2812FX::blendPixelColor(uint16_t n, uint32_t color, uint8_t blend)
{
  setPixelColor(n, color_blend(getPixelColor(n), color, blend));
}

/*
 * fade out function, higher rate = quicker fade
 */
void WS2812FX::fade_out(uint8_t rate) {
  rate = (255-rate) >> 1;
  float mappedRate = float(rate) +1.1;

  uint32_t color = SEGCOLOR(1); // target color
  int w2 = W(color);
  int r2 = R(color);
  int g2 = G(color);
  int b2 = B(color);

  for(uint16_t i = 0; i < SEGLEN; i++) {
    color = getPixelColor(i);
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

    setPixelColor(i, r1 + rdelta, g1 + gdelta, b1 + bdelta, w1 + wdelta);
  }
}

/*
 * blurs segment content, source: FastLED colorutils.cpp
 */
void WS2812FX::blur(uint8_t blur_amount)
{
  uint8_t keep = 255 - blur_amount;
  uint8_t seep = blur_amount >> 1;
  CRGB carryover = CRGB::Black;
  for(uint16_t i = 0; i < SEGLEN; i++)
  {
    CRGB cur = col_to_crgb(getPixelColor(i));
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

uint16_t WS2812FX::triwave16(uint16_t in)
{
  if (in < 0x8000) return in *2;
  return 0xFFFF - (in - 0x8000)*2;
}

uint8_t WS2812FX::sin_gap(uint16_t in) {
  if (in & 0x100) return 0;
  //if (in > 255) return 0;
  return sin8(in + 192); //correct phase shift of sine so that it starts and stops at 0
}

/*
 * Generates a tristate square wave w/ attac & decay 
 * @param x input value 0-255
 * @param pulsewidth 0-127 
 * @param attdec attac & decay, max. pulsewidth / 2
 * @returns signed waveform value
 */
int8_t WS2812FX::tristate_square8(uint8_t x, uint8_t pulsewidth, uint8_t attdec) {
  int8_t a = 127;
  if (x > 127) {
    a = -127;
    x -= 127;
  }

  if (x < attdec) { //inc to max
    return (int16_t) x * a / attdec;
  }
  else if (x < pulsewidth - attdec) { //max
    return a;
  }  
  else if (x < pulsewidth) { //dec to 0
    return (int16_t) (pulsewidth - x) * a / attdec;
  }
  return 0;
}

/*
 * Put a value 0 to 255 in to get a color value.
 * The colours are a transition r -> g -> b -> back to r
 * Inspired by the Adafruit examples.
 */
uint32_t WS2812FX::color_wheel(uint8_t pos) {
  if (SEGMENT.palette) return color_from_palette(pos, false, true, 0);
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
uint8_t WS2812FX::get_random_wheel_index(uint8_t pos) {
  uint8_t r = 0, x = 0, y = 0, d = 0;

  while(d < 42) {
    r = random8();
    x = abs(pos - r);
    y = 255 - x;
    d = MIN(x, y);
  }
  return r;
}


uint32_t WS2812FX::crgb_to_col(CRGB fastled)
{
  return RGBW32(fastled.red, fastled.green, fastled.blue, 0);
}


CRGB WS2812FX::col_to_crgb(uint32_t color)
{
  CRGB fastled_col;
  fastled_col.red =   R(color);
  fastled_col.green = G(color);
  fastled_col.blue =  B(color);
  return fastled_col;
}


void WS2812FX::load_gradient_palette(uint8_t index)
{
  byte i = constrain(index, 0, GRADIENT_PALETTE_COUNT -1);
  byte tcp[72]; //support gradient palettes with up to 18 entries
  memcpy_P(tcp, (byte*)pgm_read_dword(&(gGradientPalettes[i])), 72);
  targetPalette.loadDynamicGradientPalette(tcp);
}


/*
 * FastLED palette modes helper function. Limitation: Due to memory reasons, multiple active segments with FastLED will disable the Palette transitions
 */
void WS2812FX::handle_palette(void)
{
  bool singleSegmentMode = (_segment_index == _segment_index_palette_last);
  _segment_index_palette_last = _segment_index;

  byte paletteIndex = SEGMENT.palette;
  if (paletteIndex == 0) //default palette. Differs depending on effect
  {
    switch (SEGMENT.mode)
    {
      case FX_MODE_FIRE_2012  : paletteIndex = 35; break; //heat palette
      case FX_MODE_COLORWAVES : paletteIndex = 26; break; //landscape 33
      case FX_MODE_FILLNOISE8 : paletteIndex =  9; break; //ocean colors
      case FX_MODE_NOISE16_1  : paletteIndex = 20; break; //Drywet
      case FX_MODE_NOISE16_2  : paletteIndex = 43; break; //Blue cyan yellow
      case FX_MODE_NOISE16_3  : paletteIndex = 35; break; //heat palette
      case FX_MODE_NOISE16_4  : paletteIndex = 26; break; //landscape 33
      case FX_MODE_GLITTER    : paletteIndex = 11; break; //rainbow colors
      case FX_MODE_SUNRISE    : paletteIndex = 35; break; //heat palette
      case FX_MODE_FLOW       : paletteIndex =  6; break; //party
    }
  }
  if (SEGMENT.mode >= FX_MODE_METEOR && paletteIndex == 0) paletteIndex = 4;
  
  switch (paletteIndex)
  {
    case 0: //default palette. Exceptions for specific effects above
      targetPalette = PartyColors_p; break;
    case 1: {//periodically replace palette with a random one. Doesn't work with multiple FastLED segments
      if (!singleSegmentMode)
      {
        targetPalette = PartyColors_p; break; //fallback
      }
      if (millis() - _lastPaletteChange > 1000 + ((uint32_t)(255-SEGMENT.intensity))*100)
      {
        targetPalette = CRGBPalette16(
                        CHSV(random8(), 255, random8(128, 255)),
                        CHSV(random8(), 255, random8(128, 255)),
                        CHSV(random8(), 192, random8(128, 255)),
                        CHSV(random8(), 255, random8(128, 255)));
        _lastPaletteChange = millis();
      } break;}
    case 2: {//primary color only
      CRGB prim = col_to_crgb(SEGCOLOR(0));
      targetPalette = CRGBPalette16(prim); break;}
    case 3: {//primary + secondary
      CRGB prim = col_to_crgb(SEGCOLOR(0));
      CRGB sec  = col_to_crgb(SEGCOLOR(1));
      targetPalette = CRGBPalette16(prim,prim,sec,sec); break;}
    case 4: {//primary + secondary + tertiary
      CRGB prim = col_to_crgb(SEGCOLOR(0));
      CRGB sec  = col_to_crgb(SEGCOLOR(1));
      CRGB ter  = col_to_crgb(SEGCOLOR(2));
      targetPalette = CRGBPalette16(ter,sec,prim); break;}
    case 5: {//primary + secondary (+tert if not off), more distinct
      CRGB prim = col_to_crgb(SEGCOLOR(0));
      CRGB sec  = col_to_crgb(SEGCOLOR(1));
      if (SEGCOLOR(2)) {
        CRGB ter = col_to_crgb(SEGCOLOR(2));
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
      load_gradient_palette(paletteIndex -13);
  }
  
  if (singleSegmentMode && paletteFade && SEGENV.call > 0) //only blend if just one segment uses FastLED mode
  {
    nblendPaletteTowardPalette(currentPalette, targetPalette, 48);
  } else
  {
    currentPalette = targetPalette;
  }
}


/*
 * Gets a single color from the currently selected palette.
 * @param i Palette Index (if mapping is true, the full palette will be SEGLEN long, if false, 255). Will wrap around automatically.
 * @param mapping if true, LED position in segment is considered for color
 * @param wrap FastLED palettes will usally wrap back to the start smoothly. Set false to get a hard edge
 * @param mcol If the default palette 0 is selected, return the standard color 0, 1 or 2 instead. If >2, Party palette is used instead
 * @param pbri Value to scale the brightness of the returned color by. Default is 255. (no scaling)
 * @returns Single color from palette
 */
uint32_t WS2812FX::color_from_palette(uint16_t i, bool mapping, bool wrap, uint8_t mcol, uint8_t pbri)
{
  if (SEGMENT.palette == 0 && mcol < 3) {
    uint32_t color = SEGCOLOR(mcol);
    if (pbri != 255) {
      CRGB crgb_color = col_to_crgb(color);
      crgb_color.nscale8_video(pbri);
      return crgb_to_col(crgb_color);
    } else {
      return color;
    }
  }

  uint8_t paletteIndex = i;
  if (mapping && SEGLEN > 1) paletteIndex = (i*255)/(SEGLEN -1);
  if (!wrap) paletteIndex = scale8(paletteIndex, 240); //cut off blend at palette "end"
  CRGB fastled_col;
  fastled_col = ColorFromPalette( currentPalette, paletteIndex, pbri, (paletteBlend == 3)? NOBLEND:LINEARBLEND);

  return crgb_to_col(fastled_col);
}


//load custom mapping table from JSON file (called from finalizeInit() or deserializeState())
void WS2812FX::deserializeMap(uint8_t n) {
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

  #ifdef WLED_USE_DYNAMIC_JSON
  DynamicJsonDocument doc(JSON_BUFFER_SIZE);
  #else
  if (!requestJSONBufferLock(7)) return;
  #endif

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

//gamma 2.8 lookup table used for color correction
byte gammaT[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };

uint8_t WS2812FX::gamma8_cal(uint8_t b, float gamma) {
  return (int)(pow((float)b / 255.0, gamma) * 255 + 0.5);
}

void WS2812FX::calcGammaTable(float gamma)
{
  for (uint16_t i = 0; i < 256; i++) {
    gammaT[i] = gamma8_cal(i, gamma);
  }
}

uint8_t WS2812FX::gamma8(uint8_t b)
{
  return gammaT[b];
}

uint32_t WS2812FX::gamma32(uint32_t color)
{
  if (!gammaCorrectCol) return color;
  uint8_t w = W(color);
  uint8_t r = R(color);
  uint8_t g = G(color);
  uint8_t b = B(color);
  w = gammaT[w];
  r = gammaT[r];
  g = gammaT[g];
  b = gammaT[b];
  return RGBW32(r, g, b, w);
}

WS2812FX* WS2812FX::instance = nullptr;

//Bus static member definition, would belong in bus_manager.cpp
int16_t Bus::_cct = -1;
uint8_t Bus::_cctBlend = 0;
uint8_t Bus::_autoWhiteMode = RGBW_MODE_DUAL;