/*
 * Class implementation for addressing various light types
 */

#include <Arduino.h>
#include <IPAddress.h>
#ifdef ARDUINO_ARCH_ESP32
#include <ESPmDNS.h>
#include "src/dependencies/network/Network.h" // for isConnected() (& WiFi)
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
#ifdef ESP8266
#include "core_esp8266_waveform.h"
#endif
#include "bus_manager.h"
#include "bus_wrapper.h"
#include "wled.h"

extern char cmDNS[];
extern bool cctICused;
extern bool useParallelI2S;

// functions to get/set bits in an array - based on functions created by Brandon for GOL
//  toDo : make this a class that's completely defined in a header file
// note: these functions are automatically inline by the compiler
bool getBitFromArray(const uint8_t* byteArray, size_t position) { // get bit value
  size_t byteIndex = position >> 3;    // divide by 8
  unsigned bitIndex = position & 0x07; // modulo 8
  uint8_t byteValue = byteArray[byteIndex];
  return (byteValue >> bitIndex) & 1;
}

void setBitInArray(uint8_t* byteArray, size_t position, bool value) {  // set bit - with error handling for nullptr
    //if (byteArray == nullptr) return;
    size_t byteIndex = position >> 3;    // divide by 8
    unsigned bitIndex = position & 0x07; // modulo 8
    if (value)
        byteArray[byteIndex] |= (1 << bitIndex);
    else
        byteArray[byteIndex] &= ~(1 << bitIndex);
}

size_t getBitArrayBytes(size_t num_bits) { // number of bytes needed for an array with num_bits bits
  return (num_bits + 7) >> 3;
}

void setBitArray(uint8_t* byteArray, size_t numBits, bool value) {  // set all bits to same value
  if (byteArray == nullptr) return;
  size_t len =  getBitArrayBytes(numBits);
  if (value) memset(byteArray, 0xFF, len);
  else memset(byteArray, 0x00, len);
}

static ColorOrderMap _colorOrderMap = {};

bool ColorOrderMap::add(uint16_t start, uint16_t len, uint8_t colorOrder) {
  if (count() >= WLED_MAX_COLOR_ORDER_MAPPINGS || len == 0 || (colorOrder & 0x0F) > COL_ORDER_MAX) return false; // upper nibble contains W swap information
  _mappings.push_back({start,len,colorOrder});
  DEBUGBUS_PRINTF_P(PSTR("Bus: Add COM (%d,%d,%d)\n"), (int)start, (int)len, (int)colorOrder);
  return true;
}

uint8_t IRAM_ATTR ColorOrderMap::getPixelColorOrder(uint16_t pix, uint8_t defaultColorOrder) const {
  // upper nibble contains W swap information
  // when ColorOrderMap's upper nibble contains value >0 then swap information is used from it, otherwise global swap is used
  for (const auto& map : _mappings) {
    if (pix >= map.start && pix < (map.start + map.len)) return map.colorOrder | ((map.colorOrder >> 4) ? 0 : (defaultColorOrder & 0xF0));
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


BusDigital::BusDigital(const BusConfig &bc, uint8_t nr)
: Bus(bc.type, bc.start, bc.autoWhite, bc.count, bc.reversed, (bc.refreshReq || bc.type == TYPE_TM1814))
, _skip(bc.skipAmount) //sacrificial pixels
, _colorOrder(bc.colorOrder)
, _milliAmpsPerLed(bc.milliAmpsPerLed)
, _milliAmpsMax(bc.milliAmpsMax)
{
  DEBUGBUS_PRINTLN(F("Bus: Creating digital bus."));
  if (!isDigital(bc.type) || !bc.count) { DEBUGBUS_PRINTLN(F("Not digial or empty bus!")); return; }
  if (!PinManager::allocatePin(bc.pins[0], true, PinOwner::BusDigital)) { DEBUGBUS_PRINTLN(F("Pin 0 allocated!")); return; }
  _frequencykHz = 0U;
  _colorSum = 0;
  _pins[0] = bc.pins[0];
  if (is2Pin(bc.type)) {
    if (!PinManager::allocatePin(bc.pins[1], true, PinOwner::BusDigital)) {
      cleanup();
      DEBUGBUS_PRINTLN(F("Pin 1 allocated!"));
      return;
    }
    _pins[1] = bc.pins[1];
    _frequencykHz = bc.frequency ? bc.frequency : 2000U; // 2MHz clock if undefined
  }
  _iType = PolyBus::getI(bc.type, _pins, nr);
  if (_iType == I_NONE) { DEBUGBUS_PRINTLN(F("Incorrect iType!")); return; }
  _hasRgb = hasRGB(bc.type);
  _hasWhite = hasWhite(bc.type);
  _hasCCT = hasCCT(bc.type);
  uint16_t lenToCreate = bc.count;
  if (bc.type == TYPE_WS2812_1CH_X3) lenToCreate = NUM_ICS_WS2812_1CH_3X(bc.count); // only needs a third of "RGB" LEDs for NeoPixelBus
  _busPtr = PolyBus::create(_iType, _pins, lenToCreate + _skip, nr);
  _valid = (_busPtr != nullptr) && bc.count > 0;
  // fix for wled#4759
  if (_valid) for (unsigned i = 0; i < _skip; i++) {
    PolyBus::setPixelColor(_busPtr, _iType, i, 0, COL_ORDER_GRB); // set sacrificial pixels to black (CO does not matter here)
  }
  DEBUGBUS_PRINTF_P(PSTR("Bus: %successfully inited #%u (len:%u, type:%u (RGB:%d, W:%d, CCT:%d), pins:%u,%u [itype:%u] mA=%d/%d)\n"),
    _valid?"S":"Uns",
    (int)nr,
    (int)bc.count,
    (int)bc.type,
    (int)_hasRgb, (int)_hasWhite, (int)_hasCCT,
    (unsigned)_pins[0], is2Pin(bc.type)?(unsigned)_pins[1]:255U,
    (unsigned)_iType,
    (int)_milliAmpsPerLed, (int)_milliAmpsMax
  );
}

//DISCLAIMER
//The following function attemps to calculate the current LED power usage,
//and will limit the brightness to stay below a set amperage threshold.
//It is NOT a measurement and NOT guaranteed to stay within the ablMilliampsMax margin.
//Stay safe with high amperage and have a reasonable safety margin!
//I am NOT to be held liable for burned down garages or houses!

// note on ABL implementation:
// ABL is set up in finalizeInit()
// scaled color channels are summed in BusDigital::setPixelColor()
// the used current is estimated and limited in BusManager::show()
// if limit is set too low, brightness is limited to 1 to at least show some light
// to disable brightness limiter for a bus, set LED current to 0

void BusDigital::estimateCurrent() {
  uint32_t actualMilliampsPerLed = _milliAmpsPerLed;
  if (_milliAmpsPerLed == 255) {
    // use wacky WS2815 power model, see WLED issue #549
    _colorSum *= 3; // sum is sum of max value for each color, need to multiply by three to account for clrUnitsPerChannel being 3*255
    actualMilliampsPerLed = 12; // from testing an actual strip
  }
  // _colorSum has all the values of color channels summed, max would be getLength()*(3*255 + (255 if hasWhite()): convert to milliAmps
  uint32_t clrUnitsPerChannel = hasWhite() ? 4*255 : 3*255;
  _milliAmpsTotal = ((uint64_t)_colorSum * actualMilliampsPerLed) / clrUnitsPerChannel + getLength(); // add 1mA standby current per LED to total (WS2812: ~0.7mA, WS2815: ~2mA)
}

void BusDigital::applyBriLimit(uint8_t newBri) {
  // a newBri of 0 means calculate per-bus brightness limit
  _NPBbri = 255; // reset, intermediate value is set below, final value is calculated in bus::show()
  if (newBri == 0) {
    if (_milliAmpsLimit == 0 || _milliAmpsTotal == 0) return; // ABL not used for this bus
    newBri = 255;

    if (_milliAmpsLimit > getLength()) { // each LED uses about 1mA in standby
      if (_milliAmpsTotal > _milliAmpsLimit) {
        // scale brightness down to stay in current limit
        newBri = ((uint32_t)_milliAmpsLimit * 255) / _milliAmpsTotal + 1; // +1 to avoid 0 brightness
        _milliAmpsTotal = _milliAmpsLimit;
      }
    } else {
      newBri = 1; // limit too low, set brightness to 1, this will dim down all colors to minimum since we use video scaling
      _milliAmpsTotal = getLength(); // estimate bus current as minimum
    }
  }

  if (newBri < 255) {
    _NPBbri = newBri; // store value so it can be updated in show() (must be updated even if ABL is not used)
    uint8_t cctWW = 0, cctCW = 0;
    unsigned hwLen = _len;
    if (_type == TYPE_WS2812_1CH_X3) hwLen = NUM_ICS_WS2812_1CH_3X(_len); // only needs a third of "RGB" LEDs for NeoPixelBus
    for (unsigned i = 0; i < hwLen; i++) {
      uint8_t co = _colorOrderMap.getPixelColorOrder(i+_start, _colorOrder); // need to revert color order for correct color scaling and CCT calc in case white is swapped
      uint32_t c = PolyBus::getPixelColor(_busPtr, _iType, i, co);
      c = color_fade(c, newBri, true); // apply additional dimming  note: using inline version is a bit faster but overhead of getPixelColor() dominates the speed impact by far
      if (hasCCT()) Bus::calculateCCT(c, cctWW, cctCW);
      PolyBus::setPixelColor(_busPtr, _iType, i, c, co, (cctCW<<8) | cctWW); // repaint all pixels with new brightness
    }
  }

  _colorSum = 0; // reset for next frame
}

void BusDigital::show() {
  if (!_valid) return;
  _NPBbri = (_NPBbri * _bri) / 255;      // total applied brightness for use in restoreColorLossy (see applyBriLimit())
  PolyBus::show(_busPtr, _iType, _skip); // faster if buffer consistency is not important (no skipped LEDs)
}

bool BusDigital::canShow() const {
  if (!_valid) return true;
  return PolyBus::canShow(_busPtr, _iType);
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
  if (hasWhite()) c = autoWhiteCalc(c);
  if (Bus::_cct >= 1900) c = colorBalanceFromKelvin(Bus::_cct, c); //color correction from CCT
  c = color_fade(c, _bri, true); // apply brightness

  if (BusManager::_useABL) {
    // if using ABL, sum all color channels to estimate current and limit brightness in show()
    uint8_t r = R(c), g = G(c), b = B(c);
    if (_milliAmpsPerLed < 255) { // normal ABL
      _colorSum += r + g + b + W(c);
    } else { // wacky WS2815 power model, ignore white channel, use max of RGB (issue #549)
      _colorSum += ((r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b));
    }
  }

  if (_reversed) pix = _len - pix -1;
  pix += _skip;
  const uint8_t co = _colorOrderMap.getPixelColorOrder(pix+_start, _colorOrder);
  if (_type == TYPE_WS2812_1CH_X3) { // map to correct IC, each controls 3 LEDs
    unsigned pOld = pix;
    pix = IC_INDEX_WS2812_1CH_3X(pix);
    uint32_t cOld = PolyBus::getPixelColor(_busPtr, _iType, pix, co);
    switch (pOld % 3) { // change only the single channel (TODO: this can cause loss because of get/set)
      case 0: c = RGBW32(R(cOld), W(c)   , B(cOld), 0); break;
      case 1: c = RGBW32(W(c)   , G(cOld), B(cOld), 0); break;
      case 2: c = RGBW32(R(cOld), G(cOld), W(c)   , 0); break;
    }
  }
  uint16_t wwcw = 0;
  if (hasCCT()) {
    uint8_t cctWW = 0, cctCW = 0;
    Bus::calculateCCT(c, cctWW, cctCW);
    wwcw = (cctCW<<8) | cctWW;
    if (_type == TYPE_WS2812_WWA) c = RGBW32(cctWW, cctCW, 0, W(c));
  }
  PolyBus::setPixelColor(_busPtr, _iType, pix, c, co, wwcw);
}

// returns lossly restored color from bus
uint32_t IRAM_ATTR BusDigital::getPixelColor(unsigned pix) const {
  if (!_valid) return 0;
  if (_reversed) pix = _len - pix -1;
  pix += _skip;
  const uint8_t co = _colorOrderMap.getPixelColorOrder(pix+_start, _colorOrder);
  uint32_t c = restoreColorLossy(PolyBus::getPixelColor(_busPtr, _iType, (_type==TYPE_WS2812_1CH_X3) ? IC_INDEX_WS2812_1CH_3X(pix) : pix, co),_NPBbri);
  if (_type == TYPE_WS2812_1CH_X3) { // map to correct IC, each controls 3 LEDs
    uint8_t r = R(c);
    uint8_t g = _reversed ? B(c) : G(c); // should G and B be switched if _reversed?
    uint8_t b = _reversed ? G(c) : B(c);
    switch (pix % 3) { // get only the single channel
      case 0: c = RGBW32(g, g, g, g); break;
      case 1: c = RGBW32(r, r, r, r); break;
      case 2: c = RGBW32(b, b, b, b); break;
    }
  }
  if (_type == TYPE_WS2812_WWA) {
    uint8_t w = R(c) | G(c);
    c = RGBW32(w, w, 0, w);
  }
  return c;
}

size_t BusDigital::getPins(uint8_t* pinArray) const {
  unsigned numPins = is2Pin(_type) + 1;
  if (pinArray) for (unsigned i = 0; i < numPins; i++) pinArray[i] = _pins[i];
  return numPins;
}

size_t BusDigital::getBusSize() const {
  return sizeof(BusDigital) + (isOk() ? PolyBus::getDataSize(_busPtr, _iType) : 0); // does not include common I2S DMA buffer
}

void BusDigital::setColorOrder(uint8_t colorOrder) {
  // upper nibble contains W swap information
  if ((colorOrder & 0x0F) > 5) return;
  _colorOrder = colorOrder;
}

// credit @willmmiles & @netmindz https://github.com/wled/WLED/pull/4056
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
    //{TYPE_WS2812_2CH_X3, "D",  PSTR("WS281x CCT")}, // not implemented
    {TYPE_WS2812_WWA,    "D",  PSTR("WS281x WWA")}, // amber ignored
    {TYPE_WS2801,        "2P", PSTR("WS2801")},
    {TYPE_APA102,        "2P", PSTR("APA102")},
    {TYPE_LPD8806,       "2P", PSTR("LPD8806")},
    {TYPE_LPD6803,       "2P", PSTR("LPD6803")},
    {TYPE_P9813,         "2P", PSTR("PP9813")},
  };
}

void BusDigital::begin() {
  if (!_valid) return;
  PolyBus::begin(_busPtr, _iType, _pins, _frequencykHz);
}

void BusDigital::cleanup() {
  DEBUGBUS_PRINTLN(F("Digital Cleanup."));
  PolyBus::cleanup(_busPtr, _iType);
  _iType = I_NONE;
  _valid = false;
  _busPtr = nullptr;
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

BusPwm::BusPwm(const BusConfig &bc)
: Bus(bc.type, bc.start, bc.autoWhite, 1, bc.reversed, bc.refreshReq) // hijack Off refresh flag to indicate usage of dithering
{
  if (!isPWM(bc.type)) return;
  const unsigned numPins = numPWMPins(bc.type);
  [[maybe_unused]] const bool dithering = _needsRefresh;
  _frequency = bc.frequency ? bc.frequency : WLED_PWM_FREQ;
  // duty cycle resolution (_depth) can be extracted from this formula: CLOCK_FREQUENCY > _frequency * 2^_depth
  for (_depth = MAX_BIT_WIDTH; _depth > 8; _depth--) if (((CLOCK_FREQUENCY/_frequency) >> _depth) > 0) break;

  managed_pin_type pins[numPins];
  for (unsigned i = 0; i < numPins; i++) pins[i] = {(int8_t)bc.pins[i], true};
  if (PinManager::allocateMultiplePins(pins, numPins, PinOwner::BusPwm)) {
    #ifdef ESP8266
    analogWriteRange((1<<_depth)-1);
    analogWriteFreq(_frequency);
    #else
    // for 2 pin PWM CCT strip pinManager will make sure both LEDC channels are in the same speed group and sharing the same timer
    _ledcStart = PinManager::allocateLedc(numPins);
    if (_ledcStart == 255) { //no more free LEDC channels
      PinManager::deallocateMultiplePins(pins, numPins, PinOwner::BusPwm);
      DEBUGBUS_PRINTLN(F("No more free LEDC channels!"));
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
    _valid = true;
  }
  DEBUGBUS_PRINTF_P(PSTR("%successfully inited PWM strip with type %u, frequency %u, bit depth %u and pins %u,%u,%u,%u,%u\n"), _valid?"S":"Uns", bc.type, _frequency, _depth, _pins[0], _pins[1], _pins[2], _pins[3], _pins[4]);
}

void BusPwm::setPixelColor(unsigned pix, uint32_t c) {
  if (pix != 0 || !_valid) return; //only react to first pixel
  if (_type != TYPE_ANALOG_3CH) c = autoWhiteCalc(c);
  if (Bus::_cct >= 1900 && (_type == TYPE_ANALOG_3CH || _type == TYPE_ANALOG_4CH)) {
    c = colorBalanceFromKelvin(Bus::_cct, c); //color correction from CCT
  }
  uint8_t r = R(c), g = G(c), b = B(c), w = W(c);

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
  const size_t   numPins = getPins();
#ifdef ESP8266
   const unsigned analogPeriod = F_CPU / _frequency;
   const unsigned maxBri = analogPeriod;  // compute to clock cycle accuracy
   constexpr bool dithering = false;
   constexpr unsigned bitShift = 8;  // 256 clocks for dead time, ~3us at 80MHz
#else
  // if _needsRefresh is true (UI hack) we are using dithering (credit @dedehai & @zalatnaicsongor)
  // https://github.com/wled/WLED/pull/4115 and https://github.com/zalatnaicsongor/WLED/pull/1)
  const bool     dithering = _needsRefresh; // avoid working with bitfield
  const unsigned maxBri = (1<<_depth);      // possible values: 16384 (14), 8192 (13), 4096 (12), 2048 (11), 1024 (10), 512 (9) and 256 (8)
  const unsigned bitShift = dithering * 4;  // if dithering, _depth is 12 bit but LEDC channel is set to 8 bit (using 4 fractional bits)
#endif
  // use CIE brightness formula (linear + cubic) to approximate human eye perceived brightness
  // see: https://en.wikipedia.org/wiki/Lightness
  unsigned pwmBri = _bri;
  if (pwmBri < 21) {                                   // linear response for values [0-20]
    pwmBri = (pwmBri * maxBri + 2300 / 2) / 2300 ;     // adding '0.5' before division for correct rounding, 2300 gives a good match to CIE curve
  } else {                                             // cubic response for values [21-255]
    float temp = float(pwmBri + 41) / float(255 + 41); // 41 is to match offset & slope to linear part
    temp = temp * temp * temp * (float)maxBri;
    pwmBri = (unsigned)temp;                           // pwmBri is in range [0-maxBri] C
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
    unsigned deadTime = 0;

    if (_type == TYPE_ANALOG_2CH && Bus::_cctBlend == 0) {
      // add dead time between signals (when using dithering, two full 8bit pulses are required)
      deadTime = (1+dithering) << bitShift;
      // we only need to take care of shortening the signal at (almost) full brightness otherwise pulses may overlap
      if (_bri >= 254 && duty >= maxBri / 2 && duty < maxBri) {
        duty -= deadTime << 1; // shorten duty of larger signal except if full on
      }
    }
    if (_reversed) {
      if (i) hPoint += duty;  // align start at time zero
      duty = maxBri - duty;
    }
    #ifdef ESP8266
    //stopWaveform(_pins[i]);  // can cause the waveform to miss a cycle. instead we risk crossovers.
    startWaveformClockCycles(_pins[i], duty, analogPeriod - duty, 0, i ? _pins[0] : -1, hPoint, false);
    #else
    unsigned channel = _ledcStart + i;
    unsigned gr = channel/8;  // high/low speed group
    unsigned ch = channel%8;  // group channel
    // directly write to LEDC struct as there is no HAL exposed function for dithering
    // duty has 20 bit resolution with 4 fractional bits (24 bits in total)
    LEDC.channel_group[gr].channel[ch].duty.duty = duty << ((!dithering)*4);  // lowest 4 bits are used for dithering, shift by 4 bits if not using dithering
    LEDC.channel_group[gr].channel[ch].hpoint.hpoint = hPoint >> bitShift;    // hPoint is at _depth resolution (needs shifting if dithering)
    ledc_update_duty((ledc_mode_t)gr, (ledc_channel_t)ch);
    #endif

    if (!_reversed) hPoint += duty;
    hPoint += deadTime;        // offset to cascade the signals
    if (hPoint >= maxBri) hPoint -= maxBri; // offset is out of bounds, reset
  }
}

size_t BusPwm::getPins(uint8_t* pinArray) const {
  if (!_valid) return 0;
  unsigned numPins = numPWMPins(_type);
  if (pinArray) for (unsigned i = 0; i < numPins; i++) pinArray[i] = _pins[i];
  return numPins;
}

// credit @willmmiles & @netmindz https://github.com/wled/WLED/pull/4056
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
  size_t numPins = getPins();
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


BusOnOff::BusOnOff(const BusConfig &bc)
: Bus(bc.type, bc.start, bc.autoWhite, 1, bc.reversed)
, _data(0)
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
  _valid = true;
  DEBUGBUS_PRINTF_P(PSTR("%successfully inited On/Off strip with pin %u\n"), _valid?"S":"Uns", _pin);
}

void BusOnOff::setPixelColor(unsigned pix, uint32_t c) {
  if (pix != 0 || !_valid) return; //only react to first pixel
  c = autoWhiteCalc(c);
  uint8_t r = R(c), g = G(c), b = B(c), w = W(c);
  _data = bool(r|g|b|w) && bool(_bri) ? 0xFF : 0;
}

uint32_t BusOnOff::getPixelColor(unsigned pix) const {
  if (!_valid) return 0;
  return RGBW32(_data, _data, _data, _data);
}

void BusOnOff::show() {
  if (!_valid) return;
  digitalWrite(_pin, _reversed ? !(bool)_data : (bool)_data);
}

size_t BusOnOff::getPins(uint8_t* pinArray) const {
  if (!_valid) return 0;
  if (pinArray) pinArray[0] = _pin;
  return 1;
}

// credit @willmmiles & @netmindz https://github.com/wled/WLED/pull/4056
std::vector<LEDType> BusOnOff::getLEDTypes() {
  return {
    {TYPE_ONOFF, "", PSTR("On/Off")},
  };
}

BusNetwork::BusNetwork(const BusConfig &bc)
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
  #ifdef ARDUINO_ARCH_ESP32
  _hostname = bc.text;
  resolveHostname(); // resolve hostname to IP address if needed
  #endif
  _data = (uint8_t*)d_calloc(_len, _UDPchannels);
  _valid = (_data != nullptr);
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

size_t BusNetwork::getPins(uint8_t* pinArray) const {
  if (pinArray) for (unsigned i = 0; i < 4; i++) pinArray[i] = _client[i];
  return 4;
}

#ifdef ARDUINO_ARCH_ESP32
void BusNetwork::resolveHostname() {
  static unsigned long nextResolve = 0;
  if (Network.isConnected() && millis() > nextResolve && _hostname.length() > 0) {
    nextResolve = millis() + 600000; // resolve only every 10 minutes
    IPAddress clnt;
    if (strlen(cmDNS) > 0) clnt = MDNS.queryHost(_hostname);
    else WiFi.hostByName(_hostname.c_str(), clnt);
    if (clnt != IPAddress()) _client = clnt;
  }
}
#endif

// credit @willmmiles & @netmindz https://github.com/wled/WLED/pull/4056
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
    //{TYPE_USERMOD,         "VVVVV", PSTR("Usermod (virtual)")}, // 5 data fields (see https://github.com/wled/WLED/pull/4123)
  };
}

void BusNetwork::cleanup() {
  DEBUGBUS_PRINTLN(F("Virtual Cleanup."));
  d_free(_data);
  _data = nullptr;
  _type = I_NONE;
  _valid = false;
}

// ***************************************************************************

#ifdef WLED_ENABLE_HUB75MATRIX
#warning "HUB75 driver enabled (experimental)"
#ifdef ESP8266
#error ESP8266 does not support HUB75
#endif

BusHub75Matrix::BusHub75Matrix(const BusConfig &bc) : Bus(bc.type, bc.start, bc.autoWhite) {
  size_t lastHeap = ESP.getFreeHeap();
  _valid = false;
  _hasRgb = true;
  _hasWhite = false;
  virtualDisp = nullptr; // todo: this should be solved properly, can cause memory leak (if omitted here, nothing seems to work)
  // aliases for easier reading
  uint8_t panelWidth  = bc.pins[0];
  uint8_t panelHeight = bc.pins[1];
  uint8_t chainLength = bc.pins[2];
  _rows = bc.pins[3];
  _cols = bc.pins[4];

  mxconfig.double_buff = false; // Use our own memory-optimised buffer rather than the driver's own double-buffer

  // mxconfig.driver = HUB75_I2S_CFG::ICN2038S;  // experimental - use specific shift register driver
  // mxconfig.driver = HUB75_I2S_CFG::FM6124;    // try this driver in case you panel stays dark, or when colors look too pastel

  // mxconfig.latch_blanking = 3;
  // mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;  // experimental - 5MHZ should be enugh, but colours looks slightly better at 10MHz
  // mxconfig.min_refresh_rate = 90;
  // mxconfig.min_refresh_rate = 120;

  mxconfig.clkphase = bc.reversed;
  // allow chain length up to 4, limit to prevent bad data from preventing boot due to low memory
  mxconfig.chain_length = max((uint8_t) 1, min(chainLength, (uint8_t) 4));

  if (mxconfig.mx_height >= 64 && (mxconfig.chain_length > 1)) {
    DEBUGBUS_PRINTLN(F("WARNING, only single panel can be used of 64 pixel boards due to memory"));
    mxconfig.chain_length = 1;
  }

  if (bc.type == TYPE_HUB75MATRIX_HS) {
      mxconfig.mx_width = min((uint8_t) 64, panelWidth); // TODO: UI limit is 128, this limits to 64
      mxconfig.mx_height = min((uint8_t) 64, panelHeight);
  } else if (bc.type == TYPE_HUB75MATRIX_QS) {
      _isVirtual = true;
      mxconfig.mx_width = min((uint8_t) 64, panelWidth) * 2;
      mxconfig.mx_height = min((uint8_t) 64, panelHeight) / 2;
  } else {
    DEBUGBUS_PRINTLN("Unknown type");
    return;
  }

#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)// classic esp32, or esp32-s2: reduce bitdepth for large panels
  if (mxconfig.mx_height >= 64) {
    if (mxconfig.chain_length * mxconfig.mx_width > 192) mxconfig.setPixelColorDepthBits(3);
    else if (mxconfig.chain_length * mxconfig.mx_width > 64)  mxconfig.setPixelColorDepthBits(4);
    else mxconfig.setPixelColorDepthBits(8);
  } else mxconfig.setPixelColorDepthBits(8);
#endif



//  HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};

#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) // MatrixPortal ESP32-S3

  // https://www.adafruit.com/product/5778
  DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA - Matrix Portal S3 config");
  mxconfig.gpio = { 42, 41, 40, 38, 39, 37,  45, 36, 48, 35, 21, 47, 14, 2 };

#elif defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)// ESP32-S3 with PSRAM

#if defined(MOONHUB_S3_PINOUT)
  DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA - T7 S3 with PSRAM, MOONHUB pinout");

  // HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  mxconfig.gpio = { 1, 5, 6, 7, 13, 9, 16, 48, 47, 21, 38, 8, 4, 18 };

#else
  DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA - S3 with PSRAM");
  // HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  mxconfig.gpio = {1, 2, 42, 41, 40, 39, 45, 48, 47, 21, 38, 8, 3, 18};
#endif
#elif defined(ESP32_FORUM_PINOUT) // Common format for boards designed for SmartMatrix

  DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA - ESP32_FORUM_PINOUT");
/*
    ESP32 with SmartMatrix's default pinout - ESP32_FORUM_PINOUT
    https://github.com/pixelmatix/SmartMatrix/blob/teensylc/src/MatrixHardware_ESP32_V0.h
    Can use a board like https://github.com/rorosaurus/esp32-hub75-driver
*/

 mxconfig.gpio = { 2, 15, 4, 16, 27, 17, 5, 18, 19, 21, 12, 26, 25, 22 };

#else
  DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA - Default pins");
  /*
   https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA?tab=readme-ov-file

   Boards

   https://esp32trinity.com/
   https://www.electrodragon.com/product/rgb-matrix-panel-drive-interface-board-for-esp32-dma/

  */
 mxconfig.gpio = { 25, 26, 27, 14, 12, 13, 23, 19, 5, 17, 18, 4, 15, 16 };

#endif

  int8_t pins[PIN_COUNT];
  memcpy(pins, &mxconfig.gpio, sizeof(mxconfig.gpio));
  if (!PinManager::allocateMultiplePins(pins, PIN_COUNT, PinOwner::HUB75, true)) {
    DEBUGBUS_PRINTLN("Failed to allocate pins for HUB75");
    return;
  }

  if (bc.colorOrder == COL_ORDER_RGB) {
    DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA = Default color order (RGB)");
  } else if (bc.colorOrder == COL_ORDER_BGR) {
    DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA = color order BGR");
    int8_t tmpPin;
    tmpPin = mxconfig.gpio.r1;
    mxconfig.gpio.r1 = mxconfig.gpio.b1;
    mxconfig.gpio.b1 = tmpPin;
    tmpPin = mxconfig.gpio.r2;
    mxconfig.gpio.r2 = mxconfig.gpio.b2;
    mxconfig.gpio.b2 = tmpPin;
  }
  else {
    DEBUGBUS_PRINTF("MatrixPanel_I2S_DMA = unsupported color order %u\n", bc.colorOrder);
  }

  DEBUGBUS_PRINTF("MatrixPanel_I2S_DMA config - %ux%u length: %u\n", mxconfig.mx_width, mxconfig.mx_height, mxconfig.chain_length);
  DEBUGBUS_PRINTF("R1_PIN=%u, G1_PIN=%u, B1_PIN=%u, R2_PIN=%u, G2_PIN=%u, B2_PIN=%u, A_PIN=%u, B_PIN=%u, C_PIN=%u, D_PIN=%u, E_PIN=%u, LAT_PIN=%u, OE_PIN=%u, CLK_PIN=%u\n",
                mxconfig.gpio.r1, mxconfig.gpio.g1, mxconfig.gpio.b1, mxconfig.gpio.r2, mxconfig.gpio.g2, mxconfig.gpio.b2,
                mxconfig.gpio.a, mxconfig.gpio.b, mxconfig.gpio.c, mxconfig.gpio.d, mxconfig.gpio.e, mxconfig.gpio.lat, mxconfig.gpio.oe, mxconfig.gpio.clk);

  // OK, now we can create our matrix object
  display = new MatrixPanel_I2S_DMA(mxconfig);
  if (display == nullptr) {
      DEBUGBUS_PRINTLN("****** MatrixPanel_I2S_DMA !KABOOM! driver allocation failed ***********");
      DEBUGBUS_PRINT(F("heap usage: ")); DEBUGBUS_PRINTLN(lastHeap - ESP.getFreeHeap());
      return;
  }

  this->_len = (display->width() * display->height()); // note: this returns correct number of pixels but incorrect dimensions if using virtual display (updated below)

  DEBUGBUS_PRINTF("Length: %u\n", _len);
  if (this->_len >= MAX_LEDS) {
    DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA Too many LEDS - playing safe");
    return;
  }

  DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA created");

  // as noted in HUB75_I2S_DMA library, some panels can show ghosting if set higher than 239, so let users override at compile time
  #ifndef WLED_HUB75_MAX_BRIGHTNESS
  #define WLED_HUB75_MAX_BRIGHTNESS 255
  #endif
  // let's adjust default brightness (128), brightness scaling is handled by WLED
  //display->setBrightness8(WLED_HUB75_MAX_BRIGHTNESS); // range is 0-255, 0 - 0%, 255 - 100%

  delay(24); // experimental
  DEBUGBUS_PRINT(F("heap usage: ")); DEBUGBUS_PRINTLN(lastHeap - ESP.getFreeHeap());
  // Allocate memory and start DMA display
  if (!display->begin() ) {
      DEBUGBUS_PRINTLN("****** MatrixPanel_I2S_DMA !KABOOM! I2S memory allocation failed ***********");
      DEBUGBUS_PRINT(F("heap usage: ")); DEBUGBUS_PRINTLN(lastHeap - ESP.getFreeHeap());
      return;
  }
  else {
    DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA begin ok");
    DEBUGBUS_PRINT(F("heap usage: ")); DEBUGBUS_PRINTLN(lastHeap - ESP.getFreeHeap());
    delay(18);   // experiment - give the driver a moment (~ one full frame @ 60hz) to settle
    _valid = true;
    display->clearScreen();   // initially clear the screen buffer
    DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA clear ok");

    if (_ledBuffer) d_free(_ledBuffer);                 // should not happen
    if (_ledsDirty) d_free(_ledsDirty);                 // should not happen
    DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA allocate memory");
    _ledsDirty = (byte*) d_malloc(getBitArrayBytes(_len));  // create LEDs dirty bits
    DEBUGBUS_PRINTLN("MatrixPanel_I2S_DMA allocate memory ok");

    if (_ledsDirty == nullptr) {
      display->stopDMAoutput();
      delete display; display = nullptr;
      _valid = false;
      DEBUGBUS_PRINTLN(F("MatrixPanel_I2S_DMA not started - not enough memory for dirty bits!"));
      DEBUGBUS_PRINT(F("heap usage: ")); DEBUGBUS_PRINTLN(lastHeap - ESP.getFreeHeap());
      return;  //  fail is we cannot get memory for the buffer
    }
    setBitArray(_ledsDirty, _len, false);             // reset dirty bits

    if (mxconfig.double_buff == false) {
      // create LEDs buffer (initialized to BLACK), prefer DRAM if enough heap is available (faster in case global _pixels buffer is in PSRAM as not both will fit the cache)
      _ledBuffer = static_cast<CRGB*>(allocate_buffer(_len * sizeof(CRGB), BFRALLOC_PREFER_DRAM | BFRALLOC_CLEAR));
    }
  }

  PANEL_CHAIN_TYPE chainType = CHAIN_NONE; // default for quarter-scan panels that do not use chaining
  // chained panels with cols and rows define need the virtual display driver, so do quarter-scan panels
  if (chainLength > 1 && (_rows > 1 || _cols > 1) || bc.type == TYPE_HUB75MATRIX_QS) {
    _isVirtual = true;
    chainType = CHAIN_BOTTOM_LEFT_UP; // TODO: is there any need to support other chaining types?
    DEBUGBUS_PRINTF_P(PSTR("Using virtual matrix: %ux%u panels of %ux%u pixels\n"), _cols, _rows, mxconfig.mx_width, mxconfig.mx_height);
  }
  else {
    _isVirtual = false;
  }

  if (_isVirtual) {
    virtualDisp = new VirtualMatrixPanel((*display), _rows, _cols, mxconfig.mx_width, mxconfig.mx_height, chainType);
    virtualDisp->setRotation(0);
    if (bc.type == TYPE_HUB75MATRIX_QS) {
      switch(panelHeight) {
        case 16:
          virtualDisp->setPhysicalPanelScanRate(FOUR_SCAN_16PX_HIGH);
          break;
        case 32:
          virtualDisp->setPhysicalPanelScanRate(FOUR_SCAN_32PX_HIGH);
          break;
        case 64:
          virtualDisp->setPhysicalPanelScanRate(FOUR_SCAN_64PX_HIGH);
          break;
        default:
          DEBUGBUS_PRINTLN("Unsupported height");
          cleanup();
          return;
      }
    }
  }

  if (_valid) {
    _panelWidth = virtualDisp ? virtualDisp->width() : display->width();  // cache width - it will never change
  }

  DEBUGBUS_PRINT(F("MatrixPanel_I2S_DMA "));
  DEBUGBUS_PRINTF_P(PSTR("%sstarted, width=%u, %u pixels.\n"), _valid? "":"not ", _panelWidth, _len);

  if (_ledBuffer != nullptr) DEBUGBUS_PRINTLN(F("MatrixPanel_I2S_DMA LEDS buffer enabled."));
  if (_ledsDirty != nullptr) DEBUGBUS_PRINTLN(F("MatrixPanel_I2S_DMA LEDS dirty bit optimization enabled."));
  if ((_ledBuffer != nullptr) || (_ledsDirty != nullptr)) {
    DEBUGBUS_PRINT(F("MatrixPanel_I2S_DMA LEDS buffer uses "));
    DEBUGBUS_PRINT((_ledBuffer? _len*sizeof(CRGB) :0) + (_ledsDirty? getBitArrayBytes(_len) :0));
    DEBUGBUS_PRINTLN(F(" bytes."));
  }
}

void IRAM_ATTR BusHub75Matrix::setPixelColor(unsigned pix, uint32_t c) {
  if (!_valid) return; // note: no need to check pix >= _len as that is checked in containsPixel()
  // if (_cct >= 1900) c = colorBalanceFromKelvin(_cct, c); //color correction from CCT

  if (_ledBuffer) {
    CRGB fastled_col = CRGB(c);
    if (_ledBuffer[pix] != fastled_col) {
      _ledBuffer[pix] = fastled_col;
      setBitInArray(_ledsDirty, pix, true);  // flag pixel as "dirty"
    }
  }
  else {
    if ((c == IS_BLACK) && (getBitFromArray(_ledsDirty, pix) == false)) return; // ignore black if pixel is already black
    setBitInArray(_ledsDirty, pix, c != IS_BLACK);                              // dirty = true means "color is not BLACK"

    uint8_t r = R(c);
    uint8_t g = G(c);
    uint8_t b = B(c);

    if (virtualDisp != nullptr) {
      int x = pix % _panelWidth; // TODO: check if using & and shift would be faster here, it limits to power-of-2 widths though
      int y = pix / _panelWidth;
      virtualDisp->drawPixelRGB888(int16_t(x), int16_t(y), r, g, b);
    } else {
      int x = pix % _panelWidth;
      int y = pix / _panelWidth;
      display->drawPixelRGB888(int16_t(x), int16_t(y), r, g, b);
    }
  }
}

uint32_t BusHub75Matrix::getPixelColor(unsigned pix) const {
  if (!_valid) return IS_BLACK; // note: no need to check pix >= _len as that is checked in containsPixel()
  if (_ledBuffer)
    return uint32_t(_ledBuffer[pix]);
  else
    return getBitFromArray(_ledsDirty, pix) ? IS_DARKGREY: IS_BLACK;   // just a hack - we only know if the pixel is black or not
}

void BusHub75Matrix::setBrightness(uint8_t b) {
  _bri = b;
  display->setBrightness(_bri); 
}

void BusHub75Matrix::show(void) {
  if (!_valid) return;
  if (_ledBuffer) {
    // write out buffered LEDs
    unsigned height = _isVirtual ? virtualDisp->height() : display->height();
    unsigned width = _panelWidth;

    //while(!previousBufferFree) delay(1);   // experimental - Wait before we allow any writing to the buffer. Stop flicker.
    size_t pix = 0; // running pixel index
    for (int y=0; y<height; y++) for (int x=0; x<width; x++) {
      if (getBitFromArray(_ledsDirty, pix) == true) {        // only repaint the "dirty"  pixels
        CRGB c = _ledBuffer[pix];
        //c.nscale8_video(_bri); // apply brightness
        if (_isVirtual) virtualDisp->drawPixelRGB888(int16_t(x), int16_t(y), c.r, c.g, c.b);
        else                display->drawPixelRGB888(int16_t(x), int16_t(y), c.r, c.g, c.b);
      }
      pix++;
    }
    setBitArray(_ledsDirty, _len, false);  // buffer shown - reset all dirty bits
  }
}

void BusHub75Matrix::cleanup() {
  if (display && _valid) display->stopDMAoutput();  // terminate DMA driver (display goes black)
  _valid = false;
  delay(30); // give some time to finish DMA
  _panelWidth = 0;
  deallocatePins();
  DEBUGBUS_PRINTLN(F("HUB75 output ended."));
  #ifndef CONFIG_IDF_TARGET_ESP32S3 // on ESP32-S3 deleting display/virtualDisp does not work and leads to crash (DMA issues), request reboot from user instead
  if (virtualDisp != nullptr) delete virtualDisp; // note: in MM there is a warning to not do this but if using "NO_GFX" this is safe
  if (display != nullptr) delete display;
  display = nullptr;
  virtualDisp = nullptr; // note: when not using "NO_GFX" this causes a memory leak
  #endif
  if (_ledBuffer != nullptr) d_free(_ledBuffer); _ledBuffer = nullptr;
  if (_ledsDirty != nullptr) d_free(_ledsDirty); _ledsDirty = nullptr;
}

void BusHub75Matrix::deallocatePins() {
  uint8_t pins[PIN_COUNT];
  memcpy(pins, &mxconfig.gpio, sizeof(mxconfig.gpio));
  PinManager::deallocateMultiplePins(pins, PIN_COUNT, PinOwner::HUB75);
}

std::vector<LEDType> BusHub75Matrix::getLEDTypes() {
  return {
    {TYPE_HUB75MATRIX_HS,     "H",     PSTR("HUB75 (Half Scan)")},
    {TYPE_HUB75MATRIX_QS,     "H",     PSTR("HUB75 (Quarter Scan)")},
  };
}

size_t BusHub75Matrix::getPins(uint8_t* pinArray) const {
  if (pinArray) {
    pinArray[0] = mxconfig.mx_width;
    pinArray[1] = mxconfig.mx_height;
    pinArray[2] = mxconfig.chain_length;
    pinArray[3] = _rows;
    pinArray[4] = _cols;
  }
  return 5;
}

#endif
// ***************************************************************************

//utility to get the approx. memory usage of a given BusConfig
size_t BusConfig::memUsage(unsigned nr) const {
  if (Bus::isVirtual(type)) {
    return sizeof(BusNetwork) + (count * Bus::getNumberOfChannels(type));
  } else if (Bus::isDigital(type)) {
    // if any of digital buses uses I2S, there is additional common I2S DMA buffer not accounted for here
    return sizeof(BusDigital) + PolyBus::memUsage(count + skipAmount, PolyBus::getI(type, pins, nr));
  } else if (Bus::isOnOff(type)) {
    return sizeof(BusOnOff);
  } else {
    return sizeof(BusPwm);
  }
}


size_t BusManager::memUsage() {
  // when ESP32, S2 & S3 use parallel I2S only the largest bus determines the total memory requirements for back buffers
  // front buffers are always allocated per bus
  unsigned size = 0;
  unsigned maxI2S = 0;
  #if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(ESP8266)
  unsigned digitalCount = 0;
  #endif
  for (const auto &bus : busses) {
    size += bus->getBusSize();
    #if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(ESP8266)
    if (bus->isDigital() && !bus->is2Pin()) {
      digitalCount++;
      if ((PolyBus::isParallelI2S1Output() && digitalCount <= 8) || (!PolyBus::isParallelI2S1Output() && digitalCount == 1)) {
        #ifdef NPB_CONF_4STEP_CADENCE
        constexpr unsigned stepFactor = 4; // 4 step cadence (4 bits per pixel bit)
        #else
        constexpr unsigned stepFactor = 3; // 3 step cadence (3 bits per pixel bit)
        #endif
        unsigned i2sCommonSize = stepFactor * bus->getLength() * bus->getNumberOfChannels() * (bus->is16bit()+1);
        if (i2sCommonSize > maxI2S) maxI2S = i2sCommonSize;
      }
    }
    #endif
  }
  return size + maxI2S;
}

int BusManager::add(const BusConfig &bc) {
  DEBUGBUS_PRINTF_P(PSTR("Bus: Adding bus (p:%d v:%d)\n"), getNumBusses(), getNumVirtualBusses());
  unsigned digital = 0;
  unsigned analog  = 0;
  unsigned twoPin  = 0;
  for (const auto &bus : busses) {
    if (bus->isPWM()) analog += bus->getPins(); // number of analog channels used
    if (bus->isDigital() && !bus->is2Pin()) digital++;
    if (bus->is2Pin()) twoPin++;
  }
  if (digital > WLED_MAX_DIGITAL_CHANNELS || analog > WLED_MAX_ANALOG_CHANNELS) return -1;
  if (Bus::isVirtual(bc.type)) {
    busses.push_back(make_unique<BusNetwork>(bc));
#ifdef WLED_ENABLE_HUB75MATRIX
  } else if (Bus::isHub75(bc.type)) {
    busses.push_back(make_unique<BusHub75Matrix>(bc));
#endif
  } else if (Bus::isDigital(bc.type)) {
    busses.push_back(make_unique<BusDigital>(bc, Bus::is2Pin(bc.type) ? twoPin : digital));
  } else if (Bus::isOnOff(bc.type)) {
    busses.push_back(make_unique<BusOnOff>(bc));
  } else {
    busses.push_back(make_unique<BusPwm>(bc));
  }
  return busses.size();
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

// credit @willmmiles & @netmindz https://github.com/wled/WLED/pull/4056
String BusManager::getLEDTypesJSONString() {
  String json = "[";
  json += LEDTypesToJson(BusDigital::getLEDTypes());
  json += LEDTypesToJson(BusOnOff::getLEDTypes());
  json += LEDTypesToJson(BusPwm::getLEDTypes());
  json += LEDTypesToJson(BusNetwork::getLEDTypes());
  //json += LEDTypesToJson(BusVirtual::getLEDTypes());
  #ifdef WLED_ENABLE_HUB75MATRIX
  json += LEDTypesToJson(BusHub75Matrix::getLEDTypes());
  #endif

  json.setCharAt(json.length()-1, ']'); // replace last comma with bracket
  return json;
}

void BusManager::useParallelOutput() {
  DEBUGBUS_PRINTLN(F("Bus: Enabling parallel I2S."));
  PolyBus::setParallelI2S1Output();
}

bool BusManager::hasParallelOutput() {
  return PolyBus::isParallelI2S1Output();
}

//do not call this method from system context (network callback)
void BusManager::removeAll() {
  DEBUGBUS_PRINTLN(F("Removing all."));
  //prevents crashes due to deleting busses while in use.
  while (!canAllShow()) yield();
  busses.clear();
  PolyBus::setParallelI2S1Output(false);
}

#ifdef ESP32_DATA_IDLE_HIGH
// #2478
// If enabled, RMT idle level is set to HIGH when off
// to prevent leakage current when using an N-channel MOSFET to toggle LED power
void BusManager::esp32RMTInvertIdle() {
  bool idle_out;
  unsigned rmt = 0;
  unsigned u = 0;
  for (auto &bus : busses) {
    if (bus->getLength()==0 || !bus->isDigital() || bus->is2Pin()) continue;
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
      unsigned numI2S = !PolyBus::isParallelI2S1Output(); // if using parallel I2S, RMT is used 1st
      if (numI2S > u) continue;
      if (u > 7 + numI2S) return;
      rmt = u - numI2S;
    #endif
    //assumes that bus number to rmt channel mapping stays 1:1
    rmt_channel_t ch = static_cast<rmt_channel_t>(rmt);
    rmt_idle_level_t lvl;
    rmt_get_idle_level(ch, &idle_out, &lvl);
    if (lvl == RMT_IDLE_LEVEL_HIGH) lvl = RMT_IDLE_LEVEL_LOW;
    else if (lvl == RMT_IDLE_LEVEL_LOW) lvl = RMT_IDLE_LEVEL_HIGH;
    else continue;
    rmt_set_idle_level(ch, idle_out, lvl);
    u++;
  }
}
#endif

void BusManager::on() {
  #ifdef ESP8266
  //Fix for turning off onboard LED breaking bus
  if (PinManager::getPinOwner(LED_BUILTIN) == PinOwner::BusDigital) {
    for (auto &bus : busses) {
      uint8_t pins[2] = {255,255};
      if (bus->isDigital() && bus->getPins(pins)) {
        if (pins[0] == LED_BUILTIN || pins[1] == LED_BUILTIN) {
          BusDigital &b = static_cast<BusDigital&>(*bus);
          b.begin();
          break;
        }
      }
    }
  }
  #else
  for (auto &bus : busses) if (bus->isVirtual()) {
    // virtual/network bus should check for IP change if hostname is specified
    // otherwise there are no endpoints to force DNS resolution
    BusNetwork &b = static_cast<BusNetwork&>(*bus);
    b.resolveHostname();
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
    for (const auto &bus : busses) if (bus->isOffRefreshRequired()) return;
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
  }
  #endif
  #ifdef ESP32_DATA_IDLE_HIGH
  esp32RMTInvertIdle();
  #endif
  _gMilliAmpsUsed = 0; // reset, assume no LED idle current if relay is off
}

void BusManager::show() {
  applyABL(); // apply brightness limit, updates _gMilliAmpsUsed
  for (auto &bus : busses) {
    bus->show();
  }
}

void IRAM_ATTR BusManager::setPixelColor(unsigned pix, uint32_t c) {
  for (auto &bus : busses) {
    if (!bus->containsPixel(pix)) continue;
    bus->setPixelColor(pix - bus->getStart(), c);
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
  for (auto &bus : busses) {
    if (!bus->containsPixel(pix)) continue;
    return bus->getPixelColor(pix - bus->getStart());
  }
  return 0;
}

bool BusManager::canAllShow() {
  for (const auto &bus : busses) if (!bus->canShow()) return false;
  return true;
}

void BusManager::initializeABL() {
  _useABL = false; // reset
  if (_gMilliAmpsMax > 0) {
    // check global brightness limit
    for (auto &bus : busses) {
      if (bus->isDigital() && bus->getLEDCurrent() > 0) {
        _useABL = true; // at least one bus has valid LED current
        return;
      }
    }
  } else {
    // check per bus brightness limit
    unsigned numABLbuses = 0;
    for (auto &bus : busses) {
      if (bus->isDigital() && bus->getLEDCurrent() > 0 && bus->getMaxCurrent() > 0)
        numABLbuses++; // count ABL enabled buses
    }
    if (numABLbuses > 0) {
      _useABL = true; // at least one bus has ABL set
      uint32_t ESPshare = MA_FOR_ESP / numABLbuses; // share of ESP current per ABL bus
      for (auto &bus : busses) {
        if (bus->isDigital()) {
          BusDigital &busd = static_cast<BusDigital&>(*bus);
          uint32_t busLength = busd.getLength();
          uint32_t busDemand = busLength * busd.getLEDCurrent();
          uint32_t busMax    = busd.getMaxCurrent();
          if (busMax > ESPshare)  busMax -= ESPshare;
          if (busMax < busLength) busMax  = busLength; // give each LED 1mA, ABL will dim down to minimum
          if (busDemand == 0) busMax = 0; // no LED current set, disable ABL for this bus
          busd.setCurrentLimit(busMax);
        }
      }
    }
  }
}

void BusManager::applyABL() {
  if (_useABL) {
    unsigned milliAmpsSum = 0; // use temporary variable to always return a valid _gMilliAmpsUsed to UI
    unsigned totalLEDs = 0;
    for (auto &bus : busses) {
      if (bus->isDigital() && bus->isOk()) {
        BusDigital &busd = static_cast<BusDigital&>(*bus);
        busd.estimateCurrent(); // sets _milliAmpsTotal, current is estimated for all buses even if they have the limit set to 0
        if (_gMilliAmpsMax == 0)
          busd.applyBriLimit(0); // apply per bus ABL limit, updates _milliAmpsTotal if limit reached
        milliAmpsSum += busd.getUsedCurrent();
        totalLEDs += busd.getLength(); // sum total number of LEDs for global Limit
      }
    }
    // check global current limit and apply global ABL limit, total current is summed above
    if (_gMilliAmpsMax > 0) {
      uint8_t  newBri = 255;
      uint32_t globalMax = _gMilliAmpsMax > MA_FOR_ESP ? _gMilliAmpsMax - MA_FOR_ESP : 1; // subtract ESP current consumption, fully limit if too low
      if (globalMax > totalLEDs) { // check if budget is larger than standby current
        if (milliAmpsSum > globalMax) {
          newBri = globalMax * 255 / milliAmpsSum + 1; // scale brightness down to stay in current limit, +1 to avoid 0 brightness
          milliAmpsSum = globalMax; // update total used current
        }
      } else {
        newBri = 1; // limit too low, set brightness to minimum
        milliAmpsSum = totalLEDs; // estimate total used current as minimum
      }

      // apply brightness limit to each bus, if its 255 it will only reset _colorSum
      for (auto &bus : busses) {
        if (bus->isDigital() && bus->isOk()) {
          BusDigital &busd = static_cast<BusDigital&>(*bus);
          if (busd.getLEDCurrent() > 0)  // skip buses with LED current set to 0
            busd.applyBriLimit(newBri);
        }
      }
    }
    _gMilliAmpsUsed = milliAmpsSum;
  }
  else
    _gMilliAmpsUsed = 0; // reset, we have no current estimation without ABL
}

ColorOrderMap& BusManager::getColorOrderMap() { return _colorOrderMap; }


bool PolyBus::_useParallelI2S = false;

// Bus static member definition
int16_t Bus::_cct = -1;
uint8_t Bus::_cctBlend = 0; // 0 - 127
uint8_t Bus::_gAWM = 255;

uint16_t BusDigital::_milliAmpsTotal = 0;

std::vector<std::unique_ptr<Bus>> BusManager::busses;
uint16_t BusManager::_gMilliAmpsUsed = 0;
uint16_t BusManager::_gMilliAmpsMax = ABL_MILLIAMPS_DEFAULT;
bool BusManager::_useABL = false;
