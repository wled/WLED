/*
 * Class implementation for addressing various light types
 */

#include <Arduino.h>
#include <IPAddress.h>
#ifdef ARDUINO_ARCH_ESP32
#include "driver/ledc.h"
#include "soc/ledc_struct.h"
  #if !(defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3))
    #define LEDC_MUTEX_LOCK()    do {} while (xSemaphoreTake(_ledc_sys_lock, portMAX_DELAY) != pdPASS)
    #define LEDC_MUTEX_UNLOCK()  xSemaphoreGive(_ledc_sys_lock)
    extern xSemaphoreHandle _ledc_sys_lock;
  #else
    #define LEDC_MUTEX_LOCK()
    #define LEDC_MUTEX_UNLOCK()
  #endif
#endif
#include "const.h"
#include "pin_manager.h"
#include "bus_wrapper.h"
#include "bus_manager.h"

extern bool cctICused;

//colors.cpp
uint32_t colorBalanceFromKelvin(uint16_t kelvin, uint32_t rgb);

//udp.cpp
uint8_t realtimeBroadcast(uint8_t type, IPAddress client, uint16_t length, byte *buffer, uint8_t bri=255, bool isRGBW=false);

//color mangling macros
#define RGBW32(r,g,b,w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#define R(c) (byte((c) >> 16))
#define G(c) (byte((c) >> 8))
#define B(c) (byte(c))
#define W(c) (byte((c) >> 24))


bool ColorOrderMap::add(uint16_t start, uint16_t len, uint8_t colorOrder) {
  if (count() >= WLED_MAX_COLOR_ORDER_MAPPINGS || len == 0 || (colorOrder & 0x0F) > COL_ORDER_MAX) return false; // upper nibble contains W swap information
  _mappings.push_back({start,len,colorOrder});
  return true;
}

uint8_t IRAM_ATTR ColorOrderMap::getPixelColorOrder(uint16_t pix, uint8_t defaultColorOrder) const {
  // upper nibble contains W swap information
  // when ColorOrderMap's upper nibble contains value >0 then swap information is used from it, otherwise global swap is used
  for (unsigned i = 0; i < count(); i++) {
    if (pix >= _mappings[i].start && pix < (_mappings[i].start + _mappings[i].len)) {
      return _mappings[i].colorOrder | ((_mappings[i].colorOrder >> 4) ? 0 : (defaultColorOrder & 0xF0));
    }
  }
  return defaultColorOrder;
}


void Bus::calculateCCT(uint32_t c, uint8_t &ww, uint8_t &cw) {
  unsigned cct = 0; //0 - full warm white, 255 - full cold white
  unsigned w = W(c);

  if (_cct > -1) {                                    // using RGB?
    if (_cct >= 1900)    cct = (_cct - 1900) >> 5;    // convert K in relative format
    else if (_cct < 256) cct = _cct;                  // already relative
  } else {
    cct = (approximateKelvinFromRGB(c) - 1900) >> 5;  // convert K (from RGB value) to relative format
  }
  
  //0 - linear (CCT 127 = 50% warm, 50% cold), 127 - additive CCT blending (CCT 127 = 100% warm, 100% cold)
  if (cct       < _cctBlend) ww = 255;
  else                       ww = ((255-cct) * 255) / (255 - _cctBlend);
  if ((255-cct) < _cctBlend) cw = 255;
  else                       cw = (cct * 255) / (255 - _cctBlend);

  ww = (w * ww) / 255; //brightness scaling
  cw = (w * cw) / 255;
}

uint32_t Bus::autoWhiteCalc(uint32_t c) const {
  unsigned aWM = _autoWhiteMode;
  if (_gAWM < AW_GLOBAL_DISABLED) aWM = _gAWM;
  if (aWM == RGBW_MODE_MANUAL_ONLY) return c;
  unsigned w = W(c);
  //ignore auto-white calculation if w>0 and mode DUAL (DUAL behaves as BRIGHTER if w==0)
  if (w > 0 && aWM == RGBW_MODE_DUAL) return c;
  unsigned r = R(c);
  unsigned g = G(c);
  unsigned b = B(c);
  if (aWM == RGBW_MODE_MAX) return RGBW32(r, g, b, r > g ? (r > b ? r : b) : (g > b ? g : b)); // brightest RGB channel
  w = r < g ? (r < b ? r : b) : (g < b ? g : b);
  if (aWM == RGBW_MODE_AUTO_ACCURATE) { r -= w; g -= w; b -= w; } //subtract w in ACCURATE mode
  return RGBW32(r, g, b, w);
}

uint8_t *Bus::allocateData(size_t size) {
  if (_data) free(_data); // should not happen, but for safety
  return _data = (uint8_t *)(size>0 ? calloc(size, sizeof(uint8_t)) : nullptr);
}


BusDigital::BusDigital(BusConfig &bc, uint8_t nr, const ColorOrderMap &com)
: Bus(bc.type, bc.start, bc.autoWhite, bc.count, bc.reversed, (bc.refreshReq || bc.type == TYPE_TM1814))
, _skip(bc.skipAmount) //sacrificial pixels
, _colorOrder(bc.colorOrder)
, _milliAmpsPerLed(bc.milliAmpsPerLed)
, _milliAmpsMax(bc.milliAmpsMax)
, _colorOrderMap(com)
{
  if (!isDigital(bc.type) || !bc.count) return;
  if (!PinManager::allocatePin(bc.pins[0], true, PinOwner::BusDigital)) return;
  _frequencykHz = 0U;
  _pins[0] = bc.pins[0];
  if (is2Pin(bc.type)) {
    if (!PinManager::allocatePin(bc.pins[1], true, PinOwner::BusDigital)) {
      cleanup();
      return;
    }
    _pins[1] = bc.pins[1];
    _frequencykHz = bc.frequency ? bc.frequency : 2000U; // 2MHz clock if undefined
  }
  _iType = PolyBus::getI(bc.type, _pins, nr);
  if (_iType == I_NONE) return;
  _hasRgb = hasRGB(bc.type);
  _hasWhite = hasWhite(bc.type);
  _hasCCT = hasCCT(bc.type);
  if (bc.doubleBuffer && !allocateData(bc.count * Bus::getNumberOfChannels(bc.type))) return;
  //_buffering = bc.doubleBuffer;
  uint16_t lenToCreate = bc.count;
  if (bc.type == TYPE_WS2812_1CH_X3) lenToCreate = NUM_ICS_WS2812_1CH_3X(bc.count); // only needs a third of "RGB" LEDs for NeoPixelBus
  _busPtr = PolyBus::create(_iType, _pins, lenToCreate + _skip, nr, _frequencykHz);
  _valid = (_busPtr != nullptr);
  DEBUGBUS_PRINTF_P(PSTR("%successfully inited strip %u (len %u) with type %u and pins %u,%u (itype %u). mA=%d/%d\n"), _valid?"S":"Uns", nr, bc.count, bc.type, _pins[0], is2Pin(bc.type)?_pins[1]:255, _iType, _milliAmpsPerLed, _milliAmpsMax);
}

//fine tune power estimation constants for your setup
//you can set it to 0 if the ESP is powered by USB and the LEDs by external
#ifndef MA_FOR_ESP
  #ifdef ESP8266
    #define MA_FOR_ESP         80 //how much mA does the ESP use (Wemos D1 about 80mA)
  #else
    #define MA_FOR_ESP        120 //how much mA does the ESP use (ESP32 about 120mA)
  #endif
#endif

//DISCLAIMER
//The following function attemps to calculate the current LED power usage,
//and will limit the brightness to stay below a set amperage threshold.
//It is NOT a measurement and NOT guaranteed to stay within the ablMilliampsMax margin.
//Stay safe with high amperage and have a reasonable safety margin!
//I am NOT to be held liable for burned down garages or houses!

// To disable brightness limiter we either set output max current to 0 or single LED current to 0
uint8_t BusDigital::estimateCurrentAndLimitBri() {
  bool useWackyWS2815PowerModel = false;
  byte actualMilliampsPerLed = _milliAmpsPerLed;

  if (_milliAmpsMax < MA_FOR_ESP/BusManager::getNumBusses() || actualMilliampsPerLed == 0) { //0 mA per LED and too low numbers turn off calculation
    return _bri;
  }

  if (_milliAmpsPerLed == 255) {
    useWackyWS2815PowerModel = true;
    actualMilliampsPerLed = 12; // from testing an actual strip
  }

  size_t powerBudget = (_milliAmpsMax - MA_FOR_ESP/BusManager::getNumBusses()); //80/120mA for ESP power
  if (powerBudget > getLength()) { //each LED uses about 1mA in standby, exclude that from power budget
    powerBudget -= getLength();
  } else {
    powerBudget = 0;
  }

  uint32_t busPowerSum = 0;
  for (unsigned i = 0; i < getLength(); i++) {  //sum up the usage of each LED
    uint32_t c = getPixelColor(i); // always returns original or restored color without brightness scaling
    byte r = R(c), g = G(c), b = B(c), w = W(c);

    if (useWackyWS2815PowerModel) { //ignore white component on WS2815 power calculation
      busPowerSum += (max(max(r,g),b)) * 3;
    } else {
      busPowerSum += (r + g + b + w);
    }
  }

  if (hasWhite()) { //RGBW led total output with white LEDs enabled is still 50mA, so each channel uses less
    busPowerSum *= 3;
    busPowerSum >>= 2; //same as /= 4
  }

  // powerSum has all the values of channels summed (max would be getLength()*765 as white is excluded) so convert to milliAmps
  busPowerSum = (busPowerSum * actualMilliampsPerLed) / 765;
  _milliAmpsTotal = busPowerSum * _bri / 255;

  uint8_t newBri = _bri;
  if (busPowerSum * _bri / 255 > powerBudget) { //scale brightness down to stay in current limit
    float scale = (float)(powerBudget * 255) / (float)(busPowerSum * _bri);
    if (scale >= 1.0f) return _bri;
    _milliAmpsTotal = ceilf((float)_milliAmpsTotal * scale);
    uint8_t scaleB = min((int)(scale * 255), 255);
    newBri = unsigned(_bri * scaleB) / 256 + 1;
  }
  return newBri;
}

void BusDigital::show() {
  _milliAmpsTotal = 0;
  if (!_valid) return;

  uint8_t cctWW = 0, cctCW = 0;
  unsigned newBri = estimateCurrentAndLimitBri();  // will fill _milliAmpsTotal
  if (newBri < _bri) PolyBus::setBrightness(_busPtr, _iType, newBri); // limit brightness to stay within current limits

  if (_data) {
    size_t channels = getNumberOfChannels();
    int16_t oldCCT = Bus::_cct; // temporarily save bus CCT
    for (size_t i=0; i<_len; i++) {
      size_t offset = i * channels;
      unsigned co = _colorOrderMap.getPixelColorOrder(i+_start, _colorOrder);
      uint32_t c;
      if (_type == TYPE_WS2812_1CH_X3) { // map to correct IC, each controls 3 LEDs (_len is always a multiple of 3)
        switch (i%3) {
          case 0: c = RGBW32(_data[offset]  , _data[offset+1], _data[offset+2], 0); break;
          case 1: c = RGBW32(_data[offset-1], _data[offset]  , _data[offset+1], 0); break;
          case 2: c = RGBW32(_data[offset-2], _data[offset-1], _data[offset]  , 0); break;
        }
      } else {
        if (hasRGB()) c = RGBW32(_data[offset], _data[offset+1], _data[offset+2], hasWhite() ? _data[offset+3] : 0);
        else          c = RGBW32(0, 0, 0, _data[offset]);
      }
      if (hasCCT()) {
        // unfortunately as a segment may span multiple buses or a bus may contain multiple segments and each segment may have different CCT
        // we need to extract and appy CCT value for each pixel individually even though all buses share the same _cct variable
        // TODO: there is an issue if CCT is calculated from RGB value (_cct==-1), we cannot do that with double buffer
        Bus::_cct = _data[offset+channels-1];
        Bus::calculateCCT(c, cctWW, cctCW);
      }
      unsigned pix = i;
      if (_reversed) pix = _len - pix -1;
      pix += _skip;
      PolyBus::setPixelColor(_busPtr, _iType, pix, c, co, (cctCW<<8) | cctWW);
    }
    #if !defined(STATUSLED) || STATUSLED>=0
    if (_skip) PolyBus::setPixelColor(_busPtr, _iType, 0, 0, _colorOrderMap.getPixelColorOrder(_start, _colorOrder)); // paint skipped pixels black
    #endif
    for (int i=1; i<_skip; i++) PolyBus::setPixelColor(_busPtr, _iType, i, 0, _colorOrderMap.getPixelColorOrder(_start, _colorOrder)); // paint skipped pixels black
    Bus::_cct = oldCCT;
  } else {
    if (newBri < _bri) {
      unsigned hwLen = _len;
      if (_type == TYPE_WS2812_1CH_X3) hwLen = NUM_ICS_WS2812_1CH_3X(_len); // only needs a third of "RGB" LEDs for NeoPixelBus
      for (unsigned i = 0; i < hwLen; i++) {
        // use 0 as color order, actual order does not matter here as we just update the channel values as-is
        uint32_t c = restoreColorLossy(PolyBus::getPixelColor(_busPtr, _iType, i, 0), _bri);
        if (hasCCT()) Bus::calculateCCT(c, cctWW, cctCW); // this will unfortunately corrupt (segment) CCT data on every bus
        PolyBus::setPixelColor(_busPtr, _iType, i, c, 0, (cctCW<<8) | cctWW); // repaint all pixels with new brightness
      }
    }
  }
  PolyBus::show(_busPtr, _iType, !_data); // faster if buffer consistency is not important (use !_buffering this causes 20% FPS drop)
  // restore bus brightness to its original value
  // this is done right after show, so this is only OK if LED updates are completed before show() returns
  // or async show has a separate buffer (ESP32 RMT and I2S are ok)
  if (newBri < _bri) PolyBus::setBrightness(_busPtr, _iType, _bri);
}

bool BusDigital::canShow() const {
  if (!_valid) return true;
  return PolyBus::canShow(_busPtr, _iType);
}

void BusDigital::setBrightness(uint8_t b) {
  if (_bri == b) return;
  Bus::setBrightness(b);
  PolyBus::setBrightness(_busPtr, _iType, b);
}

//If LEDs are skipped, it is possible to use the first as a status LED.
//TODO only show if no new show due in the next 50ms
void BusDigital::setStatusPixel(uint32_t c) {
  if (_valid && _skip) {
    PolyBus::setPixelColor(_busPtr, _iType, 0, c, _colorOrderMap.getPixelColorOrder(_start, _colorOrder));
    if (canShow()) PolyBus::show(_busPtr, _iType);
  }
}

void IRAM_ATTR BusDigital::setPixelColor(unsigned pix, uint32_t c) {
  if (!_valid) return;
  uint8_t cctWW = 0, cctCW = 0;
  if (hasWhite()) c = autoWhiteCalc(c);
  if (Bus::_cct >= 1900) c = colorBalanceFromKelvin(Bus::_cct, c); //color correction from CCT
  if (_data) {
    size_t offset = pix * getNumberOfChannels();
    if (hasRGB()) {
      _data[offset++] = R(c);
      _data[offset++] = G(c);
      _data[offset++] = B(c);
    }
    if (hasWhite()) _data[offset++] = W(c);
    // unfortunately as a segment may span multiple buses or a bus may contain multiple segments and each segment may have different CCT
    // we need to store CCT value for each pixel (if there is a color correction in play, convert K in CCT ratio)
    if (hasCCT())   _data[offset]   = Bus::_cct >= 1900 ? (Bus::_cct - 1900) >> 5 : (Bus::_cct < 0 ? 127 : Bus::_cct); // TODO: if _cct == -1 we simply ignore it
  } else {
    if (_reversed) pix = _len - pix -1;
    pix += _skip;
    unsigned co = _colorOrderMap.getPixelColorOrder(pix+_start, _colorOrder);
    if (_type == TYPE_WS2812_1CH_X3) { // map to correct IC, each controls 3 LEDs
      unsigned pOld = pix;
      pix = IC_INDEX_WS2812_1CH_3X(pix);
      uint32_t cOld = restoreColorLossy(PolyBus::getPixelColor(_busPtr, _iType, pix, co),_bri);
      switch (pOld % 3) { // change only the single channel (TODO: this can cause loss because of get/set)
        case 0: c = RGBW32(R(cOld), W(c)   , B(cOld), 0); break;
        case 1: c = RGBW32(W(c)   , G(cOld), B(cOld), 0); break;
        case 2: c = RGBW32(R(cOld), G(cOld), W(c)   , 0); break;
      }
    }
    if (hasCCT()) Bus::calculateCCT(c, cctWW, cctCW);
    PolyBus::setPixelColor(_busPtr, _iType, pix, c, co, (cctCW<<8) | cctWW);
  }
}

// returns original color if global buffering is enabled, else returns lossly restored color from bus
uint32_t IRAM_ATTR BusDigital::getPixelColor(unsigned pix) const {
  if (!_valid) return 0;
  if (_data) {
    size_t offset = pix * getNumberOfChannels();
    uint32_t c;
    if (!hasRGB()) {
      c = RGBW32(_data[offset], _data[offset], _data[offset], _data[offset]);
    } else {
      c = RGBW32(_data[offset], _data[offset+1], _data[offset+2], hasWhite() ? _data[offset+3] : 0);
    }
    return c;
  } else {
    if (_reversed) pix = _len - pix -1;
    pix += _skip;
    unsigned co = _colorOrderMap.getPixelColorOrder(pix+_start, _colorOrder);
    uint32_t c = restoreColorLossy(PolyBus::getPixelColor(_busPtr, _iType, (_type==TYPE_WS2812_1CH_X3) ? IC_INDEX_WS2812_1CH_3X(pix) : pix, co),_bri);
    if (_type == TYPE_WS2812_1CH_X3) { // map to correct IC, each controls 3 LEDs
      unsigned r = R(c);
      unsigned g = _reversed ? B(c) : G(c); // should G and B be switched if _reversed?
      unsigned b = _reversed ? G(c) : B(c);
      switch (pix % 3) { // get only the single channel
        case 0: c = RGBW32(g, g, g, g); break;
        case 1: c = RGBW32(r, r, r, r); break;
        case 2: c = RGBW32(b, b, b, b); break;
      }
    }
    return c;
  }
}

uint8_t BusDigital::getPins(uint8_t* pinArray) const {
  unsigned numPins = is2Pin(_type) + 1;
  if (pinArray) for (unsigned i = 0; i < numPins; i++) pinArray[i] = _pins[i];
  return numPins;
}

void BusDigital::setColorOrder(uint8_t colorOrder) {
  // upper nibble contains W swap information
  if ((colorOrder & 0x0F) > 5) return;
  _colorOrder = colorOrder;
}

// credit @willmmiles & @netmindz https://github.com/Aircoookie/WLED/pull/4056
std::vector<LEDType> BusDigital::getLEDTypes() {
  return {
    {TYPE_WS2812_RGB,    "D",  PSTR("WS281x")},
    {TYPE_SK6812_RGBW,   "D",  PSTR("SK6812/WS2814 RGBW")},
    {TYPE_TM1814,        "D",  PSTR("TM1814")},
    {TYPE_WS2811_400KHZ, "D",  PSTR("400kHz")},
    {TYPE_TM1829,        "D",  PSTR("TM1829")},
    {TYPE_UCS8903,       "D",  PSTR("UCS8903")},
    {TYPE_APA106,        "D",  PSTR("APA106/PL9823")},
    {TYPE_TM1914,        "D",  PSTR("TM1914")},
    {TYPE_FW1906,        "D",  PSTR("FW1906 GRBCW")},
    {TYPE_UCS8904,       "D",  PSTR("UCS8904 RGBW")},
    {TYPE_WS2805,        "D",  PSTR("WS2805 RGBCW")},
    {TYPE_SM16825,       "D",  PSTR("SM16825 RGBCW")},
    {TYPE_WS2812_1CH_X3, "D",  PSTR("WS2811 White")},
    //{TYPE_WS2812_2CH_X3, "D",  PSTR("WS2811 CCT")}, // not implemented
    //{TYPE_WS2812_WWA,    "D",  PSTR("WS2811 WWA")}, // not implemented
    {TYPE_WS2801,        "2P", PSTR("WS2801")},
    {TYPE_APA102,        "2P", PSTR("APA102")},
    {TYPE_LPD8806,       "2P", PSTR("LPD8806")},
    {TYPE_LPD6803,       "2P", PSTR("LPD6803")},
    {TYPE_P9813,         "2P", PSTR("PP9813")},
  };
}

void BusDigital::reinit() {
  if (!_valid) return;
  PolyBus::begin(_busPtr, _iType, _pins);
}

void BusDigital::cleanup() {
  DEBUGBUS_PRINTLN(F("Digital Cleanup."));
  PolyBus::cleanup(_busPtr, _iType);
  _iType = I_NONE;
  _valid = false;
  _busPtr = nullptr;
  if (_data != nullptr) freeData();
  PinManager::deallocatePin(_pins[1], PinOwner::BusDigital);
  PinManager::deallocatePin(_pins[0], PinOwner::BusDigital);
}


#ifdef ESP8266
  // 1 MHz clock
  #define CLOCK_FREQUENCY 1000000UL
#else
  // Use XTAL clock if possible to avoid timer frequency error when setting APB clock < 80 Mhz
  // https://github.com/espressif/arduino-esp32/blob/2.0.2/cores/esp32/esp32-hal-ledc.c
  #ifdef SOC_LEDC_SUPPORT_XTAL_CLOCK
    #define CLOCK_FREQUENCY 40000000UL
  #else
    #define CLOCK_FREQUENCY 80000000UL
  #endif
#endif

#ifdef ESP8266
  #define MAX_BIT_WIDTH 10
#else
  #ifdef SOC_LEDC_TIMER_BIT_WIDE_NUM
    // C6/H2/P4: 20 bit, S2/S3/C2/C3: 14 bit
    #define MAX_BIT_WIDTH SOC_LEDC_TIMER_BIT_WIDE_NUM 
  #else
    // ESP32: 20 bit (but in reality we would never go beyond 16 bit as the frequency would be to low)
    #define MAX_BIT_WIDTH 14
  #endif
#endif

BusPwm::BusPwm(BusConfig &bc)
: Bus(bc.type, bc.start, bc.autoWhite, 1, bc.reversed, bc.refreshReq) // hijack Off refresh flag to indicate usage of dithering
{
  if (!isPWM(bc.type)) return;
  unsigned numPins = numPWMPins(bc.type);
  [[maybe_unused]] const bool dithering = _needsRefresh;
  _frequency = bc.frequency ? bc.frequency : WLED_PWM_FREQ;
  // duty cycle resolution (_depth) can be extracted from this formula: CLOCK_FREQUENCY > _frequency * 2^_depth
  for (_depth = MAX_BIT_WIDTH; _depth > 8; _depth--) if (((CLOCK_FREQUENCY/_frequency) >> _depth) > 0) break;

  managed_pin_type pins[numPins];
  for (unsigned i = 0; i < numPins; i++) pins[i] = {(int8_t)bc.pins[i], true};
  if (!PinManager::allocateMultiplePins(pins, numPins, PinOwner::BusPwm)) return;

#ifdef ESP8266
  analogWriteRange((1<<_depth)-1);
  analogWriteFreq(_frequency);
#else
  // for 2 pin PWM CCT strip pinManager will make sure both LEDC channels are in the same speed group and sharing the same timer
  _ledcStart = PinManager::allocateLedc(numPins);
  if (_ledcStart == 255) { //no more free LEDC channels
    PinManager::deallocateMultiplePins(pins, numPins, PinOwner::BusPwm);
    return;
  }
  // if _needsRefresh is true (UI hack) we are using dithering (credit @dedehai & @zalatnaicsongor)
  if (dithering) _depth = 12; // fixed 8 bit depth PWM with 4 bit dithering (ESP8266 has no hardware to support dithering)
#endif

  for (unsigned i = 0; i < numPins; i++) {
    _pins[i] = bc.pins[i]; // store only after allocateMultiplePins() succeeded
    #ifdef ESP8266
    pinMode(_pins[i], OUTPUT);
    #else
    unsigned channel = _ledcStart + i;
    ledcSetup(channel, _frequency, _depth - (dithering*4)); // with dithering _frequency doesn't really matter as resolution is 8 bit
    ledcAttachPin(_pins[i], channel);
    // LEDC timer reset credit @dedehai
    uint8_t group = (channel / 8), timer = ((channel / 2) % 4); // same fromula as in ledcSetup()
    ledc_timer_rst((ledc_mode_t)group, (ledc_timer_t)timer); // reset timer so all timers are almost in sync (for phase shift)
    #endif
  }
  _hasRgb = hasRGB(bc.type);
  _hasWhite = hasWhite(bc.type);
  _hasCCT = hasCCT(bc.type);
  _data = _pwmdata; // avoid malloc() and use stack
  _valid = true;
  DEBUGBUS_PRINTF_P(PSTR("%successfully inited PWM strip with type %u, frequency %u, bit depth %u and pins %u,%u,%u,%u,%u\n"), _valid?"S":"Uns", bc.type, _frequency, _depth, _pins[0], _pins[1], _pins[2], _pins[3], _pins[4]);
}

void BusPwm::setPixelColor(unsigned pix, uint32_t c) {
  if (pix != 0 || !_valid) return; //only react to first pixel
  if (_type != TYPE_ANALOG_3CH) c = autoWhiteCalc(c);
  if (Bus::_cct >= 1900 && (_type == TYPE_ANALOG_3CH || _type == TYPE_ANALOG_4CH)) {
    c = colorBalanceFromKelvin(Bus::_cct, c); //color correction from CCT
  }
  uint8_t r = R(c);
  uint8_t g = G(c);
  uint8_t b = B(c);
  uint8_t w = W(c);

  switch (_type) {
    case TYPE_ANALOG_1CH: //one channel (white), relies on auto white calculation
      _data[0] = w;
      break;
    case TYPE_ANALOG_2CH: //warm white + cold white
      if (cctICused) {
        _data[0] = w;
        _data[1] = Bus::_cct < 0 || Bus::_cct > 255 ? 127 : Bus::_cct;
      } else {
        Bus::calculateCCT(c, _data[0], _data[1]);
      }
      break;
    case TYPE_ANALOG_5CH: //RGB + warm white + cold white
      if (cctICused)
        _data[4] = Bus::_cct < 0 || Bus::_cct > 255 ? 127 : Bus::_cct;
      else
        Bus::calculateCCT(c, w, _data[4]);
    case TYPE_ANALOG_4CH: //RGBW
      _data[3] = w;
    case TYPE_ANALOG_3CH: //standard dumb RGB
      _data[0] = r; _data[1] = g; _data[2] = b;
      break;
  }
}

//does no index check
uint32_t BusPwm::getPixelColor(unsigned pix) const {
  if (!_valid) return 0;
  // TODO getting the reverse from CCT is involved (a quick approximation when CCT blending is ste to 0 implemented)
  switch (_type) {
    case TYPE_ANALOG_1CH: //one channel (white), relies on auto white calculation
      return RGBW32(0, 0, 0, _data[0]);
    case TYPE_ANALOG_2CH: //warm white + cold white
      if (cctICused) return RGBW32(0, 0, 0, _data[0]);
      else           return RGBW32(0, 0, 0, _data[0] + _data[1]);
    case TYPE_ANALOG_5CH: //RGB + warm white + cold white
      if (cctICused) return RGBW32(_data[0], _data[1], _data[2], _data[3]);
      else           return RGBW32(_data[0], _data[1], _data[2], _data[3] + _data[4]);
    case TYPE_ANALOG_4CH: //RGBW
      return RGBW32(_data[0], _data[1], _data[2], _data[3]);
    case TYPE_ANALOG_3CH: //standard dumb RGB
      return RGBW32(_data[0], _data[1], _data[2], 0);
  }
  return RGBW32(_data[0], _data[0], _data[0], _data[0]);
}

void BusPwm::show() {
  if (!_valid) return;
  // if _needsRefresh is true (UI hack) we are using dithering (credit @dedehai & @zalatnaicsongor)
  // https://github.com/Aircoookie/WLED/pull/4115 and https://github.com/zalatnaicsongor/WLED/pull/1)
  const bool     dithering = _needsRefresh; // avoid working with bitfield
  const unsigned numPins = getPins();
  const unsigned maxBri = (1<<_depth);      // possible values: 16384 (14), 8192 (13), 4096 (12), 2048 (11), 1024 (10), 512 (9) and 256 (8) 
  [[maybe_unused]] const unsigned bitShift = dithering * 4;  // if dithering, _depth is 12 bit but LEDC channel is set to 8 bit (using 4 fractional bits)

  // use CIE brightness formula (cubic) to fit (or approximate linearity of) human eye perceived brightness
  // the formula is based on 12 bit resolution as there is no need for greater precision
  // see: https://en.wikipedia.org/wiki/Lightness
  unsigned pwmBri = (unsigned)_bri * 100;  // enlarge to use integer math for linear response
  if (pwmBri < 2040) {
    // linear response for values [0-20]
    pwmBri = ((pwmBri << 12) + 115043) / 230087; //adding '0.5' before division for correct rounding
  } else {
    // cubic response for values [21-255]
    pwmBri += 4080;
    float temp = (float)pwmBri / 29580.0f;
    temp = temp * temp * temp * (float)maxBri; 
    pwmBri = (unsigned)temp;  // pwmBri is in range [0-maxBri] 
  }

  [[maybe_unused]] unsigned hPoint = 0;  // phase shift (0 - maxBri)
  // we will be phase shifting every channel by previous pulse length (plus dead time if required)
  // phase shifting is only mandatory when using H-bridge to drive reverse-polarity PWM CCT (2 wire) LED type 
  // CCT additive blending must be 0 (WW & CW will not overlap) otherwise signals *will* overlap
  // for all other cases it will just try to "spread" the load on PSU
  // Phase shifting requires that LEDC timers are synchronised (see setup()). For PWM CCT (and H-bridge) it is
  // also mandatory that both channels use the same timer (pinManager takes care of that).
  for (unsigned i = 0; i < numPins; i++) {
    unsigned duty = (_data[i] * pwmBri) / 255;    
    #ifdef ESP8266
    if (_reversed) duty = maxBri - duty;
    analogWrite(_pins[i], duty);
    #else
    int deadTime = 0;
    if (_type == TYPE_ANALOG_2CH && Bus::getCCTBlend() == 0) {
      // add dead time between signals (when using dithering, two full 8bit pulses are required)
      deadTime = (1+dithering) << bitShift;
      // we only need to take care of shortening the signal at (almost) full brightness otherwise pulses may overlap
      if (_bri >= 254 && duty >= maxBri / 2 && duty < maxBri) duty -= deadTime << 1; // shorten duty of larger signal except if full on
      if (_reversed) deadTime = -deadTime; // need to invert dead time to make phaseshift go the opposite way so low signals dont overlap
    }
    if (_reversed) duty = maxBri - duty;
    unsigned channel = _ledcStart + i;
    unsigned gr = channel/8;  // high/low speed group
    unsigned ch = channel%8;  // group channel
    // directly write to LEDC struct as there is no HAL exposed function for dithering
    // duty has 20 bit resolution with 4 fractional bits (24 bits in total)
    LEDC.channel_group[gr].channel[ch].duty.duty = duty << ((!dithering)*4);  // lowest 4 bits are used for dithering, shift by 4 bits if not using dithering
    LEDC.channel_group[gr].channel[ch].hpoint.hpoint = hPoint >> bitShift;    // hPoint is at _depth resolution (needs shifting if dithering)
    ledc_update_duty((ledc_mode_t)gr, (ledc_channel_t)ch);
    hPoint += duty + deadTime;        // offset to cascade the signals
    if (hPoint >= maxBri) hPoint = 0; // offset it out of bounds, reset
    #endif
  }
}

uint8_t BusPwm::getPins(uint8_t* pinArray) const {
  if (!_valid) return 0;
  unsigned numPins = numPWMPins(_type);
  if (pinArray) for (unsigned i = 0; i < numPins; i++) pinArray[i] = _pins[i];
  return numPins;
}

// credit @willmmiles & @netmindz https://github.com/Aircoookie/WLED/pull/4056
std::vector<LEDType> BusPwm::getLEDTypes() {
  return {
    {TYPE_ANALOG_1CH, "A",      PSTR("PWM White")},
    {TYPE_ANALOG_2CH, "AA",     PSTR("PWM CCT")},
    {TYPE_ANALOG_3CH, "AAA",    PSTR("PWM RGB")},
    {TYPE_ANALOG_4CH, "AAAA",   PSTR("PWM RGBW")},
    {TYPE_ANALOG_5CH, "AAAAA",  PSTR("PWM RGB+CCT")},
    //{TYPE_ANALOG_6CH, "AAAAAA", PSTR("PWM RGB+DCCT")}, // unimplementable ATM
  };
}

void BusPwm::deallocatePins() {
  unsigned numPins = getPins();
  for (unsigned i = 0; i < numPins; i++) {
    PinManager::deallocatePin(_pins[i], PinOwner::BusPwm);
    if (!PinManager::isPinOk(_pins[i])) continue;
    #ifdef ESP8266
    digitalWrite(_pins[i], LOW); //turn off PWM interrupt
    #else
    if (_ledcStart < WLED_MAX_ANALOG_CHANNELS) ledcDetachPin(_pins[i]);
    #endif
  }
  #ifdef ARDUINO_ARCH_ESP32
  PinManager::deallocateLedc(_ledcStart, numPins);
  #endif
}


BusOnOff::BusOnOff(BusConfig &bc)
: Bus(bc.type, bc.start, bc.autoWhite, 1, bc.reversed)
, _onoffdata(0)
{
  if (!Bus::isOnOff(bc.type)) return;

  uint8_t currentPin = bc.pins[0];
  if (!PinManager::allocatePin(currentPin, true, PinOwner::BusOnOff)) {
    return;
  }
  _pin = currentPin; //store only after allocatePin() succeeds
  pinMode(_pin, OUTPUT);
  _hasRgb = false;
  _hasWhite = false;
  _hasCCT = false;
  _data = &_onoffdata; // avoid malloc() and use stack
  _valid = true;
  DEBUGBUS_PRINTF_P(PSTR("%successfully inited On/Off strip with pin %u\n"), _valid?"S":"Uns", _pin);
}

void BusOnOff::setPixelColor(unsigned pix, uint32_t c) {
  if (pix != 0 || !_valid) return; //only react to first pixel
  c = autoWhiteCalc(c);
  uint8_t r = R(c);
  uint8_t g = G(c);
  uint8_t b = B(c);
  uint8_t w = W(c);
  _data[0] = bool(r|g|b|w) && bool(_bri) ? 0xFF : 0;
}

uint32_t BusOnOff::getPixelColor(unsigned pix) const {
  if (!_valid) return 0;
  return RGBW32(_data[0], _data[0], _data[0], _data[0]);
}

void BusOnOff::show() {
  if (!_valid) return;
  digitalWrite(_pin, _reversed ? !(bool)_data[0] : (bool)_data[0]);
}

uint8_t BusOnOff::getPins(uint8_t* pinArray) const {
  if (!_valid) return 0;
  if (pinArray) pinArray[0] = _pin;
  return 1;
}

// credit @willmmiles & @netmindz https://github.com/Aircoookie/WLED/pull/4056
std::vector<LEDType> BusOnOff::getLEDTypes() {
  return {
    {TYPE_ONOFF, "", PSTR("On/Off")},
  };
}

BusNetwork::BusNetwork(BusConfig &bc)
: Bus(bc.type, bc.start, bc.autoWhite, bc.count)
, _broadcastLock(false)
{
  switch (bc.type) {
    case TYPE_NET_ARTNET_RGB:
      _UDPtype = 2;
      break;
    case TYPE_NET_ARTNET_RGBW:
      _UDPtype = 2;
      break;
    case TYPE_NET_E131_RGB:
      _UDPtype = 1;
      break;
    default: // TYPE_NET_DDP_RGB / TYPE_NET_DDP_RGBW
      _UDPtype = 0;
      break;
  }
  _hasRgb = hasRGB(bc.type);
  _hasWhite = hasWhite(bc.type);
  _hasCCT = false;
  _UDPchannels = _hasWhite + 3;
  _client = IPAddress(bc.pins[0],bc.pins[1],bc.pins[2],bc.pins[3]);
  _valid = (allocateData(_len * _UDPchannels) != nullptr);
  DEBUGBUS_PRINTF_P(PSTR("%successfully inited virtual strip with type %u and IP %u.%u.%u.%u\n"), _valid?"S":"Uns", bc.type, bc.pins[0], bc.pins[1], bc.pins[2], bc.pins[3]);
}

void BusNetwork::setPixelColor(unsigned pix, uint32_t c) {
  if (!_valid || pix >= _len) return;
  if (_hasWhite) c = autoWhiteCalc(c);
  if (Bus::_cct >= 1900) c = colorBalanceFromKelvin(Bus::_cct, c); //color correction from CCT
  unsigned offset = pix * _UDPchannels;
  _data[offset]   = R(c);
  _data[offset+1] = G(c);
  _data[offset+2] = B(c);
  if (_hasWhite) _data[offset+3] = W(c);
}

uint32_t BusNetwork::getPixelColor(unsigned pix) const {
  if (!_valid || pix >= _len) return 0;
  unsigned offset = pix * _UDPchannels;
  return RGBW32(_data[offset], _data[offset+1], _data[offset+2], (hasWhite() ? _data[offset+3] : 0));
}

void BusNetwork::show() {
  if (!_valid || !canShow()) return;
  _broadcastLock = true;
  realtimeBroadcast(_UDPtype, _client, _len, _data, _bri, hasWhite());
  _broadcastLock = false;
}

uint8_t BusNetwork::getPins(uint8_t* pinArray) const {
  if (pinArray) for (unsigned i = 0; i < 4; i++) pinArray[i] = _client[i];
  return 4;
}

// credit @willmmiles & @netmindz https://github.com/Aircoookie/WLED/pull/4056
std::vector<LEDType> BusNetwork::getLEDTypes() {
  return {
    {TYPE_NET_DDP_RGB,     "N",     PSTR("DDP RGB (network)")},      // should be "NNNN" to determine 4 "pin" fields
    {TYPE_NET_ARTNET_RGB,  "N",     PSTR("Art-Net RGB (network)")},
    {TYPE_NET_DDP_RGBW,    "N",     PSTR("DDP RGBW (network)")},
    {TYPE_NET_ARTNET_RGBW, "N",     PSTR("Art-Net RGBW (network)")},
    // hypothetical extensions
    //{TYPE_VIRTUAL_I2C_W,   "V",     PSTR("I2C White (virtual)")}, // allows setting I2C address in _pin[0]
    //{TYPE_VIRTUAL_I2C_CCT, "V",     PSTR("I2C CCT (virtual)")}, // allows setting I2C address in _pin[0]
    //{TYPE_VIRTUAL_I2C_RGB, "VVV",   PSTR("I2C RGB (virtual)")}, // allows setting I2C address in _pin[0] and 2 additional values in _pin[1] & _pin[2]
    //{TYPE_USERMOD,         "VVVVV", PSTR("Usermod (virtual)")}, // 5 data fields (see https://github.com/Aircoookie/WLED/pull/4123)
  };
}

void BusNetwork::cleanup() {
  _type = I_NONE;
  _valid = false;
  freeData();
}


//utility to get the approx. memory usage of a given BusConfig
uint32_t BusManager::memUsage(BusConfig &bc) {
  if (Bus::isOnOff(bc.type) || Bus::isPWM(bc.type)) return OUTPUT_MAX_PINS;

  unsigned len = bc.count + bc.skipAmount;
  unsigned channels = Bus::getNumberOfChannels(bc.type);
  unsigned multiplier = 1;
  if (Bus::isDigital(bc.type)) { // digital types
    if (Bus::is16bit(bc.type)) len *= 2; // 16-bit LEDs
    #ifdef ESP8266
      if (bc.pins[0] == 3) { //8266 DMA uses 5x the mem
        multiplier = 5;
      }
    #else //ESP32 RMT uses double buffer, parallel I2S uses 8x buffer (3 times)
      multiplier = PolyBus::isParallelI2S1Output() ? 24 : 2;
    #endif
  }
  return (len * multiplier + bc.doubleBuffer * (bc.count + bc.skipAmount)) * channels;
}

uint32_t BusManager::memUsage(unsigned maxChannels, unsigned maxCount, unsigned minBuses) {
  //ESP32 RMT uses double buffer, parallel I2S uses 8x buffer (3 times)
  unsigned multiplier = PolyBus::isParallelI2S1Output() ? 3 : 2;
  return (maxChannels * maxCount * minBuses * multiplier);
}

int BusManager::add(BusConfig &bc) {
  if (getNumBusses() - getNumVirtualBusses() >= WLED_MAX_BUSSES) return -1;
  if (Bus::isVirtual(bc.type)) {
    busses[numBusses] = new BusNetwork(bc);
  } else if (Bus::isDigital(bc.type)) {
    busses[numBusses] = new BusDigital(bc, numBusses, colorOrderMap);
  } else if (Bus::isOnOff(bc.type)) {
    busses[numBusses] = new BusOnOff(bc);
  } else {
    busses[numBusses] = new BusPwm(bc);
  }
  return numBusses++;
}

// credit @willmmiles
static String LEDTypesToJson(const std::vector<LEDType>& types) {
  String json;
  for (const auto &type : types) {
    // capabilities follows similar pattern as JSON API
    int capabilities = Bus::hasRGB(type.id) | Bus::hasWhite(type.id)<<1 | Bus::hasCCT(type.id)<<2 | Bus::is16bit(type.id)<<4 | Bus::mustRefresh(type.id)<<5;
    char str[256];
    sprintf_P(str, PSTR("{i:%d,c:%d,t:\"%s\",n:\"%s\"},"), type.id, capabilities, type.type, type.name);
    json += str;
  }
  return json;
}

// credit @willmmiles & @netmindz https://github.com/Aircoookie/WLED/pull/4056
String BusManager::getLEDTypesJSONString() {
  String json = "[";
  json += LEDTypesToJson(BusDigital::getLEDTypes());
  json += LEDTypesToJson(BusOnOff::getLEDTypes());
  json += LEDTypesToJson(BusPwm::getLEDTypes());
  json += LEDTypesToJson(BusNetwork::getLEDTypes());
  //json += LEDTypesToJson(BusVirtual::getLEDTypes());
  json.setCharAt(json.length()-1, ']'); // replace last comma with bracket
  return json;
}

void BusManager::useParallelOutput() {
  _parallelOutputs = 8; // hardcoded since we use NPB I2S x8 methods
  PolyBus::setParallelI2S1Output();
}

//do not call this method from system context (network callback)
void BusManager::removeAll() {
  DEBUGBUS_PRINTLN(F("Removing all."));
  //prevents crashes due to deleting busses while in use.
  while (!canAllShow()) yield();
  for (unsigned i = 0; i < numBusses; i++) delete busses[i];
  numBusses = 0;
  _parallelOutputs = 1;
  PolyBus::setParallelI2S1Output(false);
}

#ifdef ESP32_DATA_IDLE_HIGH
// #2478
// If enabled, RMT idle level is set to HIGH when off
// to prevent leakage current when using an N-channel MOSFET to toggle LED power
void BusManager::esp32RMTInvertIdle() {
  bool idle_out;
  unsigned rmt = 0;
  for (unsigned u = 0; u < numBusses(); u++) {
    #if defined(CONFIG_IDF_TARGET_ESP32C3)    // 2 RMT, only has 1 I2S but NPB does not support it ATM
      if (u > 1) return;
      rmt = u;
    #elif defined(CONFIG_IDF_TARGET_ESP32S2)  // 4 RMT, only has 1 I2S bus, supported in NPB
      if (u > 3) return;
      rmt = u;
    #elif defined(CONFIG_IDF_TARGET_ESP32S3)  // 4 RMT, has 2 I2S but NPB does not support them ATM
      if (u > 3) return;
      rmt = u;
    #else
      if (u < _parallelOutputs) continue;
      if (u >= _parallelOutputs + 8) return; // only 8 RMT channels
      rmt = u - _parallelOutputs;
    #endif
    if (busses[u]->getLength()==0 || !busses[u]->isDigital() || busses[u]->is2Pin()) continue;
    //assumes that bus number to rmt channel mapping stays 1:1
    rmt_channel_t ch = static_cast<rmt_channel_t>(rmt);
    rmt_idle_level_t lvl;
    rmt_get_idle_level(ch, &idle_out, &lvl);
    if (lvl == RMT_IDLE_LEVEL_HIGH) lvl = RMT_IDLE_LEVEL_LOW;
    else if (lvl == RMT_IDLE_LEVEL_LOW) lvl = RMT_IDLE_LEVEL_HIGH;
    else continue;
    rmt_set_idle_level(ch, idle_out, lvl);
  }
}
#endif

void BusManager::on() {
  #ifdef ESP8266
  //Fix for turning off onboard LED breaking bus
  if (PinManager::getPinOwner(LED_BUILTIN) == PinOwner::BusDigital) {
    for (unsigned i = 0; i < numBusses; i++) {
      uint8_t pins[2] = {255,255};
      if (busses[i]->isDigital() && busses[i]->getPins(pins)) {
        if (pins[0] == LED_BUILTIN || pins[1] == LED_BUILTIN) {
          BusDigital *bus = static_cast<BusDigital*>(busses[i]);
          bus->reinit();
          break;
        }
      }
    }
  }
  #endif
  #ifdef ESP32_DATA_IDLE_HIGH
  esp32RMTInvertIdle();
  #endif
}

void BusManager::off() {
  #ifdef ESP8266
  // turn off built-in LED if strip is turned off
  // this will break digital bus so will need to be re-initialised on On
  if (PinManager::getPinOwner(LED_BUILTIN) == PinOwner::BusDigital) {
    for (unsigned i = 0; i < numBusses; i++) if (busses[i]->isOffRefreshRequired()) return;
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  #endif
  #ifdef ESP32_DATA_IDLE_HIGH
  esp32RMTInvertIdle();
  #endif
}

void BusManager::show() {
  _milliAmpsUsed = 0;
  for (unsigned i = 0; i < numBusses; i++) {
    busses[i]->show();
    _milliAmpsUsed += busses[i]->getUsedCurrent();
  }
  if (_milliAmpsUsed) _milliAmpsUsed += MA_FOR_ESP;
}

void BusManager::setStatusPixel(uint32_t c) {
  for (unsigned i = 0; i < numBusses; i++) {
    busses[i]->setStatusPixel(c);
  }
}

void IRAM_ATTR BusManager::setPixelColor(unsigned pix, uint32_t c) {
  for (unsigned i = 0; i < numBusses; i++) {
    unsigned bstart = busses[i]->getStart();
    if (pix < bstart || pix >= bstart + busses[i]->getLength()) continue;
    busses[i]->setPixelColor(pix - bstart, c);
  }
}

void BusManager::setBrightness(uint8_t b) {
  for (unsigned i = 0; i < numBusses; i++) {
    busses[i]->setBrightness(b);
  }
}

void BusManager::setSegmentCCT(int16_t cct, bool allowWBCorrection) {
  if (cct > 255) cct = 255;
  if (cct >= 0) {
    //if white balance correction allowed, save as kelvin value instead of 0-255
    if (allowWBCorrection) cct = 1900 + (cct << 5);
  } else cct = -1; // will use kelvin approximation from RGB
  Bus::setCCT(cct);
}

uint32_t BusManager::getPixelColor(unsigned pix) {
  for (unsigned i = 0; i < numBusses; i++) {
    unsigned bstart = busses[i]->getStart();
    if (!busses[i]->containsPixel(pix)) continue;
    return busses[i]->getPixelColor(pix - bstart);
  }
  return 0;
}

bool BusManager::canAllShow() {
  for (unsigned i = 0; i < numBusses; i++) {
    if (!busses[i]->canShow()) return false;
  }
  return true;
}

Bus* BusManager::getBus(uint8_t busNr) {
  if (busNr >= numBusses) return nullptr;
  return busses[busNr];
}

//semi-duplicate of strip.getLengthTotal() (though that just returns strip._length, calculated in finalizeInit())
uint16_t BusManager::getTotalLength() {
  unsigned len = 0;
  for (unsigned i=0; i<numBusses; i++) len += busses[i]->getLength();
  return len;
}

bool PolyBus::useParallelI2S = false;

// Bus static member definition
int16_t Bus::_cct = -1;
uint8_t Bus::_cctBlend = 0;
uint8_t Bus::_gAWM = 255;

uint16_t BusDigital::_milliAmpsTotal = 0;

uint8_t       BusManager::numBusses = 0;
Bus*          BusManager::busses[WLED_MAX_BUSSES+WLED_MIN_VIRTUAL_BUSSES];
ColorOrderMap BusManager::colorOrderMap = {};
uint16_t      BusManager::_milliAmpsUsed = 0;
uint16_t      BusManager::_milliAmpsMax = ABL_MILLIAMPS_DEFAULT;
uint8_t       BusManager::_parallelOutputs = 1;
