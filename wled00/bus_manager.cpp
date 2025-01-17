/*
 * Class implementation for addressing various light types
 */

#include <Arduino.h>
#include <IPAddress.h>
#include "const.h"
#include "pin_manager.h"
#include "bus_wrapper.h"
#include "bus_manager.h"

// WLEDMM functions to get/set bits in an array - based on functions created by Brandon for GOL
//  toDo : make this a class that's completely defined in a header file
inline bool getBitFromArray(const uint8_t* byteArray, size_t position) { // get bit value
    size_t byteIndex = position >> 3;      // same as "position/8"
    unsigned bitIndex = position & 0x0007; // last 3 bits
    uint8_t byteValue = byteArray[byteIndex];
    return (byteValue >> bitIndex) & 1;
}

inline void setBitInArray(uint8_t* byteArray, size_t position, bool value) {  // set bit - with error handling for nullptr
    //if (byteArray == nullptr) return;
    size_t byteIndex = position >> 3;
    unsigned bitIndex = position & 0x0007; // last 3 bits
    if (value)
        byteArray[byteIndex] |= (1 << bitIndex); 
    else
        byteArray[byteIndex] &= ~(1 << bitIndex);
}

size_t getBitArrayBytes(size_t num_bits) { // number of bytes needed for an array with num_bits bits
  return (num_bits + 7) / 8; 
}

void setBitArray(uint8_t* byteArray, size_t numBits, bool value) {  // set all bits to same value
  if (byteArray == nullptr) return;
  size_t len =  getBitArrayBytes(numBits);
  if (value) memset(byteArray, 0xFF, len);
  else memset(byteArray, 0x00, len);
}

//WLEDMM: #define DEBUGOUT(x) netDebugEnabled?NetDebug.print(x):Serial.print(x) not supported in this file as netDebugEnabled not in scope
#if 0
//colors.cpp
uint32_t colorBalanceFromKelvin(uint16_t kelvin, uint32_t rgb);
uint16_t approximateKelvinFromRGB(uint32_t rgb);
void colorRGBtoRGBW(byte* rgb);

//udp.cpp
uint8_t realtimeBroadcast(uint8_t type, IPAddress client, uint16_t length, byte *buffer, uint8_t bri=255, bool isRGBW=false);

// enable additional debug output
#if defined(WLED_DEBUG_HOST)
  #include "net_debug.h"
  #define DEBUGOUT NetDebug
#else
  #define DEBUGOUT Serial
#endif

#ifdef WLED_DEBUG
  #ifndef ESP8266
  #include <rom/rtc.h>
  #endif
  #define DEBUG_PRINT(x) DEBUGOUT.print(x)
  #define DEBUG_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUG_PRINTF(x...) DEBUGOUT.printf(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x...)
#endif
#else
 // un-define USER_PRINT macros from bus_wrapper.h
 #undef USER_PRINT
 #undef USER_PRINTF
 #undef USER_PRINTLN
 #undef USER_FLUSH
 // WLEDMM use wled.h
#include "wled.h"
#endif


void ColorOrderMap::add(uint16_t start, uint16_t len, uint8_t colorOrder) {
  if (_count >= WLED_MAX_COLOR_ORDER_MAPPINGS) {
    return;
  }
  if (len == 0) {
    return;
  }
  if (colorOrder > COL_ORDER_MAX) {
    return;
  }
  _mappings[_count].start = start;
  _mappings[_count].len = len;
  _mappings[_count].colorOrder = colorOrder;
  _count++;
}

uint8_t IRAM_ATTR ColorOrderMap::getPixelColorOrder(uint16_t pix, uint8_t defaultColorOrder) const {
  if (_count == 0) return defaultColorOrder;
  // upper nibble contains W swap information
  uint8_t swapW = defaultColorOrder >> 4;
  for (uint8_t i = 0; i < _count; i++) {
    if (pix >= _mappings[i].start && pix < (_mappings[i].start + _mappings[i].len)) {
      return _mappings[i].colorOrder | (swapW << 4);
    }
  }
  return defaultColorOrder;
}


uint32_t Bus::autoWhiteCalc(uint32_t c) const {
  uint8_t aWM = _autoWhiteMode;
  if (_gAWM != AW_GLOBAL_DISABLED) aWM = _gAWM;
  if (aWM == RGBW_MODE_MANUAL_ONLY) return c;
  uint8_t w = W(c);
  //ignore auto-white calculation if w>0 and mode DUAL (DUAL behaves as BRIGHTER if w==0)
  if (w > 0 && aWM == RGBW_MODE_DUAL) return c;
  uint8_t r = R(c);
  uint8_t g = G(c);
  uint8_t b = B(c);
  if (aWM == RGBW_MODE_MAX) return RGBW32(r, g, b, r > g ? (r > b ? r : b) : (g > b ? g : b)); // brightest RGB channel
  w = r < g ? (r < b ? r : b) : (g < b ? g : b);
  if (aWM == RGBW_MODE_AUTO_ACCURATE) { r -= w; g -= w; b -= w; } //subtract w in ACCURATE mode
  return RGBW32(r, g, b, w);
}


BusDigital::BusDigital(BusConfig &bc, uint8_t nr, const ColorOrderMap &com) : Bus(bc.type, bc.start, bc.autoWhite), _colorOrderMap(com) {
  if (!IS_DIGITAL(bc.type) || !bc.count) return;
  if (!pinManager.allocatePin(bc.pins[0], true, PinOwner::BusDigital)) return;
  _frequencykHz = 0U;
  _pins[0] = bc.pins[0];
  if (IS_2PIN(bc.type)) {
    if (!pinManager.allocatePin(bc.pins[1], true, PinOwner::BusDigital)) {
    cleanup(); return;
    }
    _pins[1] = bc.pins[1];
    _frequencykHz = bc.frequency ? bc.frequency : 2000U; // 2MHz clock if undefined
  }
  reversed = bc.reversed;
  _needsRefresh = bc.refreshReq || bc.type == TYPE_TM1814;
  _skip = bc.skipAmount;    //sacrificial pixels
  _len = bc.count + _skip;
  _iType = PolyBus::getI(bc.type, _pins, nr);
  if (_iType == I_NONE) return;
  uint16_t lenToCreate = _len;
  if (bc.type == TYPE_WS2812_1CH_X3) lenToCreate = NUM_ICS_WS2812_1CH_3X(_len); // only needs a third of "RGB" LEDs for NeoPixelBus 
  _busPtr = PolyBus::create(_iType, _pins, lenToCreate, nr, _frequencykHz);
  _valid = (_busPtr != nullptr);
  _colorOrder = bc.colorOrder;
  if (_pins[1] != 255) {  // WLEDMM USER_PRINTF
    USER_PRINTF("%successfully inited strip %u (len %u) with type %u and pins %u,%u (itype %u)", _valid?"S":"Uns", nr, _len, bc.type, _pins[0],_pins[1],_iType);
    if (bc.frequency > 999) USER_PRINTF(", %d MHz", bc.frequency/1000);
    USER_PRINTLN();
  } else {
    USER_PRINTF("%successfully inited strip %u (len %u) with type %u and pin %u (itype %u)\n", _valid?"S":"Uns", nr, _len, bc.type, _pins[0],_iType);
  }
}

void BusDigital::show() {
  PolyBus::show(_busPtr, _iType);
}

bool BusDigital::canShow() {
  return PolyBus::canShow(_busPtr, _iType);
}

void BusDigital::setBrightness(uint8_t b, bool immediate) {
  //Fix for turning off onboard LED breaking bus
  #ifdef LED_BUILTIN
  if (_bri == 0 && b > 0) {
    if (_pins[0] == LED_BUILTIN || _pins[1] == LED_BUILTIN) PolyBus::begin(_busPtr, _iType, _pins);
  }
  #endif
  Bus::setBrightness(b, immediate);
  PolyBus::setBrightness(_busPtr, _iType, b, immediate);
}

//If LEDs are skipped, it is possible to use the first as a status LED.
//TODO only show if no new show due in the next 50ms
void BusDigital::setStatusPixel(uint32_t c) {
  if (_skip && canShow()) {
    PolyBus::setPixelColor(_busPtr, _iType, 0, c, _colorOrderMap.getPixelColorOrder(_start, _colorOrder));
    PolyBus::show(_busPtr, _iType);
  }
}

void IRAM_ATTR BusDigital::setPixelColor(uint16_t pix, uint32_t c) {
  if (_type == TYPE_SK6812_RGBW || _type == TYPE_TM1814 || _type == TYPE_WS2812_1CH_X3) c = autoWhiteCalc(c);
  if (_cct >= 1900) c = colorBalanceFromKelvin(_cct, c); //color correction from CCT
  if (reversed) pix = _len - pix -1;
  else pix += _skip;
  uint8_t co = _colorOrderMap.getPixelColorOrder(pix+_start, _colorOrder);
  if (_type == TYPE_WS2812_1CH_X3) { // map to correct IC, each controls 3 LEDs
    uint16_t pOld = pix;
    pix = IC_INDEX_WS2812_1CH_3X(pix);
    uint32_t cOld = PolyBus::getPixelColor(_busPtr, _iType, pix, co);
    switch (pOld % 3) { // change only the single channel (TODO: this can cause loss because of get/set)
      case 0: c = RGBW32(R(cOld), W(c)   , B(cOld), 0); break;
      case 1: c = RGBW32(W(c)   , G(cOld), B(cOld), 0); break;
      case 2: c = RGBW32(R(cOld), G(cOld), W(c)   , 0); break;
    }
  }
  PolyBus::setPixelColor(_busPtr, _iType, pix, c, co);
}

uint32_t IRAM_ATTR_YN BusDigital::getPixelColor(uint16_t pix) const {
  if (reversed) pix = _len - pix -1;
  else pix += _skip;
  uint8_t co = _colorOrderMap.getPixelColorOrder(pix+_start, _colorOrder);
  if (_type == TYPE_WS2812_1CH_X3) { // map to correct IC, each controls 3 LEDs
    uint16_t pOld = pix;
    pix = IC_INDEX_WS2812_1CH_3X(pix);
    uint32_t c = PolyBus::getPixelColor(_busPtr, _iType, pix, co);
    switch (pOld % 3) { // get only the single channel
      case 0: c = RGBW32(G(c), G(c), G(c), G(c)); break;
      case 1: c = RGBW32(R(c), R(c), R(c), R(c)); break;
      case 2: c = RGBW32(B(c), B(c), B(c), B(c)); break;
    }
    return c;
  }
  return PolyBus::getPixelColor(_busPtr, _iType, pix, co);
}

uint8_t BusDigital::getPins(uint8_t* pinArray) const {
  uint8_t numPins = IS_2PIN(_type) ? 2 : 1;
  for (uint8_t i = 0; i < numPins; i++) pinArray[i] = _pins[i];
  return numPins;
}

void BusDigital::setColorOrder(uint8_t colorOrder) {
  // upper nibble contains W swap information
  if ((colorOrder & 0x0F) > 5) return;
  _colorOrder = colorOrder;
}

void BusDigital::reinit() {
  PolyBus::begin(_busPtr, _iType, _pins);
}

void BusDigital::cleanup() {
  DEBUG_PRINTLN(F("Digital Cleanup."));
  PolyBus::cleanup(_busPtr, _iType);
  _iType = I_NONE;
  _valid = false;
  _busPtr = nullptr;
  pinManager.deallocatePin(_pins[1], PinOwner::BusDigital);
  pinManager.deallocatePin(_pins[0], PinOwner::BusDigital);
}


BusPwm::BusPwm(BusConfig &bc) : Bus(bc.type, bc.start, bc.autoWhite) {
  _valid = false;
  if (!IS_PWM(bc.type)) return;
  uint8_t numPins = NUM_PWM_PINS(bc.type);
  _frequency = bc.frequency ? bc.frequency : WLED_PWM_FREQ;

  #ifdef ESP8266
  analogWriteRange(255);  //same range as one RGB channel
  analogWriteFreq(_frequency);
  #else
  _ledcStart = pinManager.allocateLedc(numPins);
  if (_ledcStart == 255) { //no more free LEDC channels
    deallocatePins(); return;
  }
  #endif

  USER_PRINT("[PWM");
  for (uint8_t i = 0; i < numPins; i++) {
    uint8_t currentPin = bc.pins[i];
    if (!pinManager.allocatePin(currentPin, true, PinOwner::BusPwm)) {
    deallocatePins(); return;
    }
    _pins[i] = currentPin; //store only after allocatePin() succeeds
    #ifdef ESP8266
    pinMode(_pins[i], OUTPUT);
    #else
    ledcSetup(_ledcStart + i, _frequency, 8);
    ledcAttachPin(_pins[i], _ledcStart + i);
    #endif
    USER_PRINT(" "); USER_PRINT(currentPin);
  }
  USER_PRINTLN("] ");
  reversed = bc.reversed;
  _valid = true;
}

void BusPwm::setPixelColor(uint16_t pix, uint32_t c) {
  if (pix != 0 || !_valid) return; //only react to first pixel
  if (_type != TYPE_ANALOG_3CH) c = autoWhiteCalc(c);
  if (_cct >= 1900 && (_type == TYPE_ANALOG_3CH || _type == TYPE_ANALOG_4CH)) {
    c = colorBalanceFromKelvin(_cct, c); //color correction from CCT
  }
  uint8_t r = R(c);
  uint8_t g = G(c);
  uint8_t b = B(c);
  uint8_t w = W(c);
  uint8_t cct = 0; //0 - full warm white, 255 - full cold white
  if (_cct > -1) {
    if (_cct >= 1900)    cct = (_cct - 1900) >> 5;
    else if (_cct < 256) cct = _cct;
  } else {
    cct = (approximateKelvinFromRGB(c) - 1900) >> 5;
  }

  uint8_t ww, cw;
  #ifdef WLED_USE_IC_CCT
  ww = w;
  cw = cct;
  #else
  //0 - linear (CCT 127 = 50% warm, 50% cold), 127 - additive CCT blending (CCT 127 = 100% warm, 100% cold)
  if (cct       < _cctBlend) ww = 255;
  else ww = ((255-cct) * 255) / (255 - _cctBlend);

  if ((255-cct) < _cctBlend) cw = 255;
  else                       cw = (cct * 255) / (255 - _cctBlend);

  ww = (w * ww) / 255; //brightness scaling
  cw = (w * cw) / 255;
  #endif

  switch (_type) {
    case TYPE_ANALOG_1CH: //one channel (white), relies on auto white calculation
      _data[0] = w;
      break;
    case TYPE_ANALOG_2CH: //warm white + cold white
      _data[1] = cw;
      _data[0] = ww;
      break;
    case TYPE_ANALOG_5CH: //RGB + warm white + cold white
      _data[4] = cw;
      w = ww;
    case TYPE_ANALOG_4CH: //RGBW
      _data[3] = w;
    case TYPE_ANALOG_3CH: //standard dumb RGB
      _data[0] = r; _data[1] = g; _data[2] = b;
      break;
  }
}

//does no index check
uint32_t BusPwm::getPixelColor(uint16_t pix) const {
  if (!_valid) return 0;
#if 1
  // WLEDMM stick with the old code - we don't have cctICused
  return RGBW32(_data[0], _data[1], _data[2], _data[3]);
#else
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
#endif
}

void BusPwm::show() {
  if (!_valid) return;
  uint8_t numPins = NUM_PWM_PINS(_type);
  for (uint8_t i = 0; i < numPins; i++) {
    uint8_t scaled = (_data[i] * _bri) / 255;
    if (reversed) scaled = 255 - scaled;
    #ifdef ESP8266
    analogWrite(_pins[i], scaled);
    #else
    ledcWrite(_ledcStart + i, scaled);
    #endif
  }
}

uint8_t BusPwm::getPins(uint8_t* pinArray) const {
  if (!_valid) return 0;
  uint8_t numPins = NUM_PWM_PINS(_type);
  for (uint8_t i = 0; i < numPins; i++) {
    pinArray[i] = _pins[i];
  }
  return numPins;
}

void BusPwm::deallocatePins() {
  uint8_t numPins = NUM_PWM_PINS(_type);
  for (uint8_t i = 0; i < numPins; i++) {
    pinManager.deallocatePin(_pins[i], PinOwner::BusPwm);
    if (!pinManager.isPinOk(_pins[i])) continue;
    #ifdef ESP8266
    digitalWrite(_pins[i], LOW); //turn off PWM interrupt
    #else
    if (_ledcStart < 16) ledcDetachPin(_pins[i]);
    #endif
  }
  #ifdef ARDUINO_ARCH_ESP32
  pinManager.deallocateLedc(_ledcStart, numPins);
  #endif
}


BusOnOff::BusOnOff(BusConfig &bc) : Bus(bc.type, bc.start, bc.autoWhite) {
  _valid = false;
  if (bc.type != TYPE_ONOFF) return;

  uint8_t currentPin = bc.pins[0];
  if (!pinManager.allocatePin(currentPin, true, PinOwner::BusOnOff)) {
    return;
  }
  _pin = currentPin; //store only after allocatePin() succeeds
  pinMode(_pin, OUTPUT);
  reversed = bc.reversed;
  _valid = true;
  USER_PRINTF("[On-Off %d] \n", int(currentPin));
}

void BusOnOff::setPixelColor(uint16_t pix, uint32_t c) {
  if (pix != 0 || !_valid) return; //only react to first pixel
  c = autoWhiteCalc(c);
  uint8_t r = R(c);
  uint8_t g = G(c);
  uint8_t b = B(c);
  uint8_t w = W(c);

  _data = bool(r|g|b|w) && bool(_bri) ? 0xFF : 0;
}

uint32_t BusOnOff::getPixelColor(uint16_t pix) const {
  if (!_valid) return 0;
  return RGBW32(_data, _data, _data, _data);
}

void BusOnOff::show() {
  if (!_valid) return;
  digitalWrite(_pin, reversed ? !(bool)_data : (bool)_data);
}

uint8_t BusOnOff::getPins(uint8_t* pinArray) const {
  if (!_valid) return 0;
  pinArray[0] = _pin;
  return 1;
}


BusNetwork::BusNetwork(BusConfig &bc, const ColorOrderMap &com) : Bus(bc.type, bc.start, bc.autoWhite), _colorOrderMap(com) {
  _valid = false;
  USER_PRINT("[");
  switch (bc.type) {
    case TYPE_NET_ARTNET_RGB:
      _rgbw = false;
      _UDPtype = 2;
      USER_PRINT("NET_ARTNET_RGB");
      break;
    case TYPE_NET_ARTNET_RGBW:
      _rgbw = true;
      _UDPtype = 2;
      USER_PRINT("NET_ARTNET_RGBW");
      break;
    case TYPE_NET_E131_RGB:
      _rgbw = false;
      _UDPtype = 1;
      USER_PRINT("NET_E131_RGB");
      break;
    default: // TYPE_NET_DDP_RGB / TYPE_NET_DDP_RGBW
      _rgbw = bc.type == TYPE_NET_DDP_RGBW;
      _UDPtype = 0;
      USER_PRINT(bc.type == TYPE_NET_DDP_RGBW ? "NET_DDP_RGBW" : "NET_DDP_RGB");
      break;
  }
  _UDPchannels = _rgbw ? 4 : 3;
  #ifdef ESP32
  _data = (byte*) heap_caps_calloc_prefer((bc.count * _UDPchannels)+15, sizeof(byte), 3, MALLOC_CAP_DEFAULT, MALLOC_CAP_SPIRAM);
  #else
  _data = (byte*) calloc((bc.count * _UDPchannels)+15, sizeof(byte));
  #endif
  if (_data == nullptr) return;
  _len = bc.count;
  _colorOrder = bc.colorOrder;
  _client = IPAddress(bc.pins[0],bc.pins[1],bc.pins[2],bc.pins[3]);
  _broadcastLock = false;
  _valid = true;
  _artnet_outputs = bc.artnet_outputs;
  _artnet_leds_per_output = bc.artnet_leds_per_output;
  _artnet_fps_limit = max(uint8_t(1), bc.artnet_fps_limit);
  USER_PRINTF(" %u.%u.%u.%u]\n", bc.pins[0],bc.pins[1],bc.pins[2],bc.pins[3]);
}

void IRAM_ATTR_YN BusNetwork::setPixelColor(uint16_t pix, uint32_t c) {
    if (pix >= _len) return;
    if (_rgbw) c = autoWhiteCalc(c);
    if (_cct >= 1900) c = colorBalanceFromKelvin(_cct, c); // color correction from CCT

    uint16_t offset = pix * _UDPchannels;
    uint8_t co = _colorOrderMap.getPixelColorOrder(pix + _start, _colorOrder);

    if (_colorOrder != co || _colorOrder != COL_ORDER_RGB) {
        switch (co) {
            case COL_ORDER_GRB:
                _data[offset] = G(c); _data[offset+1] = R(c); _data[offset+2] = B(c);
                break;
            case COL_ORDER_RGB:
                _data[offset] = R(c); _data[offset+1] = G(c); _data[offset+2] = B(c);
                break;
            case COL_ORDER_BRG:
                _data[offset] = B(c); _data[offset+1] = R(c); _data[offset+2] = G(c);
                break;
            case COL_ORDER_RBG:
                _data[offset] = R(c); _data[offset+1] = B(c); _data[offset+2] = G(c);
                break;
            case COL_ORDER_GBR:
                _data[offset] = G(c); _data[offset+1] = B(c); _data[offset+2] = R(c);
                break;
            case COL_ORDER_BGR:
                _data[offset] = B(c); _data[offset+1] = G(c); _data[offset+2] = R(c);
                break;
        }
        if (_rgbw) _data[offset+3] = W(c);
    } else {
        _data[offset] = R(c); _data[offset+1] = G(c); _data[offset+2] = B(c);
        if (_rgbw) _data[offset+3] = W(c);
    }
}

uint32_t IRAM_ATTR_YN BusNetwork::getPixelColor(uint16_t pix) const {
    if (pix >= _len) return 0;
    uint16_t offset = pix * _UDPchannels;
    uint8_t co = _colorOrderMap.getPixelColorOrder(pix + _start, _colorOrder);

    uint8_t r = _data[offset + 0];
    uint8_t g = _data[offset + 1];
    uint8_t b = _data[offset + 2];
    uint8_t w = _rgbw ? _data[offset + 3] : 0;

    switch (co) {
        case COL_ORDER_GRB: return RGBW32(g, r, b, w);
        case COL_ORDER_RGB: return RGBW32(r, g, b, w);
        case COL_ORDER_BRG: return RGBW32(b, r, g, w);
        case COL_ORDER_RBG: return RGBW32(r, b, g, w);
        case COL_ORDER_GBR: return RGBW32(g, b, r, w);
        case COL_ORDER_BGR: return RGBW32(b, g, r, w);
        default: return RGBW32(r, g, b, w); // default to RGB order
    }
}

void BusNetwork::show() {
  if (!_valid || !canShow()) return;
  _broadcastLock = true;
  realtimeBroadcast(_UDPtype, _client, _len, _data, _bri, _rgbw, _artnet_outputs, _artnet_leds_per_output, _artnet_fps_limit);
  _broadcastLock = false;
}

uint8_t BusNetwork::getPins(uint8_t* pinArray) const {
  for (uint8_t i = 0; i < 4; i++) {
    pinArray[i] = _client[i];
  }
  return 4;
}

void BusNetwork::cleanup() {
  _type = I_NONE;
  _valid = false;
  if (_data != nullptr) free(_data);
  _data = nullptr;
  _len = 0;
}

// ***************************************************************************

#ifdef WLED_ENABLE_HUB75MATRIX
#warning "HUB75 driver enabled (experimental)"

// BusHub75Matrix "global" variables (static members)
MatrixPanel_I2S_DMA* BusHub75Matrix::activeDisplay = nullptr;
VirtualMatrixPanel*  BusHub75Matrix::activeFourScanPanel = nullptr;
HUB75_I2S_CFG BusHub75Matrix::activeMXconfig = HUB75_I2S_CFG();
uint8_t BusHub75Matrix::activeType = 0;
uint8_t BusHub75Matrix::instanceCount = 0;
uint8_t BusHub75Matrix::last_bri = 0;


// --------------------------
// Bitdepth reduction based on panel size
// --------------------------
#if defined(CONFIG_IDF_TARGET_ESP32S3) && CONFIG_SPIRAM_MODE_OCT && defined(BOARD_HAS_PSRAM) && (defined(WLED_USE_PSRAM) || defined(WLED_USE_PSRAM_JSON))
  // esp32-S3 with octal PSRAM
  #if defined(SPIRAM_FRAMEBUFFER)
    // when PSRAM is used for pixel buffers
    #define MAX_PIXELS_8BIT (192 * 64)
    #define MAX_PIXELS_6BIT ( 64 * 64)   // trick: skip this category, so we go directly from 8bit to 4bit
    #define MAX_PIXELS_4BIT (256 * 128)
  #else
    // PSRAM not used for pixel buffers
    #define MAX_PIXELS_8BIT (128 * 64)
    #define MAX_PIXELS_6BIT (192 * 64)
    #define MAX_PIXELS_4BIT (256 * 64)
  #endif
#elif defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)
  // standard esp32-S3 with quad PSRAM
  #define MAX_PIXELS_8BIT ( 96 * 64)
  #define MAX_PIXELS_6BIT (128 * 64)
  #define MAX_PIXELS_4BIT (160 * 64)
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  // HD-WF2 is an esp32-S3 without PSRAM - use same limits as classic esp32
  #define MAX_PIXELS_8BIT ( 64 * 64)
  #define MAX_PIXELS_6BIT ( 96 * 64)
  #define MAX_PIXELS_4BIT (128 * 64)
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  // esp32-S2 only has 320KB RAM
  #define MAX_PIXELS_8BIT ( 48 * 48)
  #define MAX_PIXELS_6BIT ( 64 * 48)
  #define MAX_PIXELS_4BIT ( 96 * 64)
#else
  // classic esp32, and anything else
  #define MAX_PIXELS_8BIT ( 64 * 64)
  #define MAX_PIXELS_6BIT ( 96 * 64)
  #define MAX_PIXELS_4BIT (128 * 64)
#endif
// --------------------------

BusHub75Matrix::BusHub75Matrix(BusConfig &bc) : Bus(bc.type, bc.start, bc.autoWhite) {
  MatrixPanel_I2S_DMA* display = nullptr;
  VirtualMatrixPanel*  fourScanPanel = nullptr;
  HUB75_I2S_CFG mxconfig;
  size_t lastHeap = ESP.getFreeHeap();

  _valid = false;
  _len = 0;

  // allow exactly one instance
  if (instanceCount > 0) {
    USER_PRINTLN("****** MatrixPanel_I2S_DMA !KABOOM! already active - preventing attempt to create more than one driver instance.");
    return;  
  }

    mxconfig.double_buff = false; // Use our own memory-optimised buffer rather than the driver's own double-buffer
 
  // mxconfig.driver = HUB75_I2S_CFG::ICN2038S;  // experimental - use specific shift register driver
  // mxconfig.driver = HUB75_I2S_CFG::FM6124;    // try this driver in case you panel stays dark, or when colors look too pastel

  // mxconfig.latch_blanking = 1;                // needed for some ICS panels
  // mxconfig.latch_blanking = 3;                // use in case you see gost images
  // mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;  // experimental - 5MHZ should be enugh, but colours looks slightly better at 10MHz
  // mxconfig.min_refresh_rate = 90;

  mxconfig.clkphase = bc.reversed;
  if (bc.refreshReq) mxconfig.latch_blanking = 1;                // needed for some ICS panels (default = 2)
  // fake bus flags
  _needsRefresh = mxconfig.latch_blanking == 1;
  reversed = mxconfig.clkphase;

  if (bc.type > 104) mxconfig.driver = HUB75_I2S_CFG::FM6124;  // use FM6124 for "outdoor" panels - workaround until we can make the driver user-configurable

  // How many panels we have connected, cap at sane value, prevent bad data preventing boot due to low memory
  #if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)        // ESP32-S3: allow up to 6 panels
  mxconfig.chain_length = max((uint8_t) 1, min(bc.pins[0], (uint8_t) 6));
  #elif defined(CONFIG_IDF_TARGET_ESP32S2)                                  // ESP32-S2: only 2 panels due to small RAM
  mxconfig.chain_length = max((uint8_t) 1, min(bc.pins[0], (uint8_t) 2));
  #else                                                                     // others: up to 4 panels
  mxconfig.chain_length = max((uint8_t) 1, min(bc.pins[0], (uint8_t) 4));
  #endif

  #if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)
  if(bc.pins[0] > 4) {
    USER_PRINTLN("WARNING, chain limited to 4");
  }
  # else
  // Disable this check if you are want to try bigger setups and accept you
  // might need to do full erase to recover from memory relayed boot-loop if you push too far
  if(mxconfig.mx_height >= 64 && (bc.pins[0] > 1)) {
    USER_PRINTLN("WARNING, only single panel can be used of 64 pixel boards due to memory");
    //mxconfig.chain_length = 1;
  }
  #endif

  switch(bc.type) {
    case 101:
      mxconfig.mx_width = 32;
      mxconfig.mx_height = 32;
      break;
    case 102:
      mxconfig.mx_width = 64;
      mxconfig.mx_height = 32;
      break;
    case 103:
      mxconfig.mx_width = 64;
      mxconfig.mx_height = 64;
      break;
    case 104:
      mxconfig.mx_width = 128;
      mxconfig.mx_height = 64;
      break;
    case 105:
      mxconfig.mx_width = 32 * 2;
      mxconfig.mx_height = 32 / 2;
      break;
    case 106:
      mxconfig.mx_width = 64 * 2;
      mxconfig.mx_height = 32 / 2;
      break;
    case 107:
      mxconfig.mx_width = 64 * 2;
      mxconfig.mx_height = 64 / 2;
      break;
    case 108: // untested
      mxconfig.mx_width = 128 * 2;
      mxconfig.mx_height = 64 / 2;
      break;
  }

  // reduce bitdepth based on total pixels
  unsigned numPixels = mxconfig.mx_height * mxconfig.mx_width * mxconfig.chain_length;
  if (numPixels <= MAX_PIXELS_8BIT)      mxconfig.setPixelColorDepthBits(8);   // 24bit
  else if (numPixels <= MAX_PIXELS_6BIT) mxconfig.setPixelColorDepthBits(6);   // 18bit
  else if (numPixels <= MAX_PIXELS_4BIT) mxconfig.setPixelColorDepthBits(4);   // 12bit
  else mxconfig.setPixelColorDepthBits(3);                                     //  9bit


#if defined(ARDUINO_ADAFRUIT_MATRIXPORTAL_ESP32S3) // MatrixPortal ESP32-S3

  // https://www.adafruit.com/product/5778

  USER_PRINTLN("MatrixPanel_I2S_DMA - Matrix Portal S3 config");

  mxconfig.gpio.r1 = 42;
  mxconfig.gpio.g1 = 41;
  mxconfig.gpio.b1 = 40;
  mxconfig.gpio.r2 = 38;
  mxconfig.gpio.g2 = 39;
  mxconfig.gpio.b2 = 37; 

  mxconfig.gpio.lat = 47;
  mxconfig.gpio.oe  = 14;
  mxconfig.gpio.clk = 2;

  mxconfig.gpio.a = 45;
  mxconfig.gpio.b = 36;
  mxconfig.gpio.c = 48;
  mxconfig.gpio.d = 35;
  mxconfig.gpio.e = 21;

#elif defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)// ESP32-S3 with PSRAM

  #if defined(MOONHUB_S3_PINOUT)
  USER_PRINTLN("MatrixPanel_I2S_DMA - T7 S3 with PSRAM, MOONHUB pinout");

  // HUB75_I2S_CFG::i2s_pins _pins={R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  mxconfig.gpio = { 1, 5, 6, 7, 13, 9, 16, 48, 47, 21, 38, 8, 4, 18 };

  #else
  USER_PRINTLN("MatrixPanel_I2S_DMA - S3 with PSRAM");

  mxconfig.gpio.r1 =  1;
  mxconfig.gpio.g1 =  2;
  mxconfig.gpio.b1 =  42;
  // 4th pin is GND
  mxconfig.gpio.r2 =  41;
  mxconfig.gpio.g2 =  40;
  mxconfig.gpio.b2 =  39;
  mxconfig.gpio.e =   38;
  mxconfig.gpio.a =   45;
  mxconfig.gpio.b =   48;
  mxconfig.gpio.c =   47;
  mxconfig.gpio.d =   21;
  mxconfig.gpio.clk = 18;
  mxconfig.gpio.lat = 8;
  mxconfig.gpio.oe  = 3;
  // 16th pin is GND
  #endif

#elif defined(CONFIG_IDF_TARGET_ESP32S3) // ESP32-S3 HD-WF2

  // Huidu HD-WF2 ESP32-S3
  // https://www.aliexpress.com/item/1005002258734810.html
  // https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/issues/433

  USER_PRINTLN("MatrixPanel_I2S_DMA - HD-WF2 S3 config");

  mxconfig.gpio.r1 = 2;
  mxconfig.gpio.g1 = 6;
  mxconfig.gpio.b1 = 10;
  mxconfig.gpio.r2 = 3;
  mxconfig.gpio.g2 = 7;
  mxconfig.gpio.b2 = 11;

  mxconfig.gpio.lat = 33;
  mxconfig.gpio.oe  = 35;
  mxconfig.gpio.clk = 34;

  mxconfig.gpio.a = 39;
  mxconfig.gpio.b = 38;
  mxconfig.gpio.c = 37;
  mxconfig.gpio.d = 36;
  mxconfig.gpio.e = 21;

#elif defined(CONFIG_IDF_TARGET_ESP32S2) // ESP32-S2

  // Huidu HD-WF1 ESP32-S2
  // https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA/issues/433

  USER_PRINTLN("MatrixPanel_I2S_DMA - HD-WF1 S2 config");

  mxconfig.gpio.r1 = 2;
  mxconfig.gpio.g1 = 6;
  mxconfig.gpio.b1 = 3;
  mxconfig.gpio.r2 = 4;
  mxconfig.gpio.g2 = 8;
  mxconfig.gpio.b2 = 5;

  mxconfig.gpio.lat = 33;
  mxconfig.gpio.oe  = 35;
  mxconfig.gpio.clk = 34;

  mxconfig.gpio.a = 39;
  mxconfig.gpio.b = 38;
  mxconfig.gpio.c = 37;
  mxconfig.gpio.d = 36;
  mxconfig.gpio.e = 12;

#elif defined(ESP32_FORUM_PINOUT) // Common format for boards designed for SmartMatrix

  USER_PRINTLN("MatrixPanel_I2S_DMA - ESP32_FORUM_PINOUT");

/*
    ESP32 with SmartMatrix's default pinout - ESP32_FORUM_PINOUT
    
    https://github.com/pixelmatix/SmartMatrix/blob/teensylc/src/MatrixHardware_ESP32_V0.h

    Can use a board like https://github.com/rorosaurus/esp32-hub75-driver
*/

  mxconfig.gpio.r1 = 2;
  mxconfig.gpio.g1 = 15;
  mxconfig.gpio.b1 = 4;
  mxconfig.gpio.r2 = 16;
  mxconfig.gpio.g2 = 27;
  mxconfig.gpio.b2 = 17; 

  mxconfig.gpio.lat = 26;
  mxconfig.gpio.oe  = 25;
  mxconfig.gpio.clk = 22;

  mxconfig.gpio.a = 5;
  mxconfig.gpio.b = 18;
  mxconfig.gpio.c = 19;
  mxconfig.gpio.d = 21;
  mxconfig.gpio.e = 12;

#else
  USER_PRINTLN("MatrixPanel_I2S_DMA - Default pins");
  /*
   https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA?tab=readme-ov-file

   Boards

   https://esp32trinity.com/
   https://www.electrodragon.com/product/rgb-matrix-panel-drive-interface-board-for-esp32-dma/
   
  */
  mxconfig.gpio.r1 = 25;
  mxconfig.gpio.g1 = 26;
  mxconfig.gpio.b1 = 27;
  mxconfig.gpio.r2 = 14;
  mxconfig.gpio.g2 = 12;
  mxconfig.gpio.b2 = 13;

  mxconfig.gpio.lat = 4;
  mxconfig.gpio.oe  = 15;
  mxconfig.gpio.clk = 16;

  mxconfig.gpio.a = 23;
  mxconfig.gpio.b = 19;
  mxconfig.gpio.c = 5;
  mxconfig.gpio.d = 17;
  mxconfig.gpio.e = 18;

#endif

  USER_PRINTF("MatrixPanel_I2S_DMA config - %ux%u (type %u) length: %u, %u bits/pixel.\n", mxconfig.mx_width, mxconfig.mx_height, bc.type, mxconfig.chain_length, mxconfig.getPixelColorDepthBits() * 3);
  DEBUG_PRINT(F("Free heap: ")); DEBUG_PRINTLN(ESP.getFreeHeap()); lastHeap = ESP.getFreeHeap();

  // check if we can re-use the existing display driver
  if (activeDisplay) {
    if (   (memcmp(&(activeMXconfig.gpio), &(mxconfig.gpio), sizeof(mxconfig.gpio)) != 0) // other pins?
        || (activeMXconfig.chain_length != mxconfig.chain_length)                         // other chain length?
        || (activeMXconfig.mx_width != mxconfig.mx_width) || (activeMXconfig.mx_height != mxconfig.mx_height) // other size?
        || (bc.type != activeType)                                                        // different panel type ?
        || (activeMXconfig.clkphase != mxconfig.clkphase)                                 // different driver options ?
        || (activeMXconfig.latch_blanking != mxconfig.latch_blanking)
        || (activeMXconfig.i2sspeed != mxconfig.i2sspeed)
        || (activeMXconfig.driver != mxconfig.driver)
        || (activeMXconfig.min_refresh_rate != mxconfig.min_refresh_rate)
        || (activeMXconfig.getPixelColorDepthBits() != mxconfig.getPixelColorDepthBits())  )
    {
      // not the same as before - delete old driver
      DEBUG_PRINTLN("MatrixPanel_I2S_DMA deleting old driver!");
      activeDisplay->stopDMAoutput();
      delay(28);
      //#if !defined(CONFIG_IDF_TARGET_ESP32S3)  // prevent crash
      delete activeDisplay;
      //#endif
      activeDisplay = nullptr;
      activeFourScanPanel = nullptr;
      #if defined(CONFIG_IDF_TARGET_ESP32S3)  // runtime reconfiguration is not working on -S3
      USER_PRINTLN("\n\n****** MatrixPanel_I2S_DMA !KABOOM WARNING! Reboot needed to change driver options ***********\n");
      errorFlag = ERR_REBOOT_NEEDED;
      #endif
    }
  }

  // OK, now we can create our matrix object
  bool newDisplay = false; // true when the previous display object wasn't re-used
  if (!activeDisplay) {
    display = new MatrixPanel_I2S_DMA(mxconfig);   // create new matrix object
    newDisplay = true;
  } else {
    display = activeDisplay;                       // continue with existing matrix object
    fourScanPanel = activeFourScanPanel;
  }

  if (display == nullptr) {
      USER_PRINTLN("****** MatrixPanel_I2S_DMA !KABOOM! driver allocation failed ***********");
      activeDisplay = nullptr;
      activeFourScanPanel = nullptr;
      USER_PRINT(F("heap usage: ")); USER_PRINTLN(int(lastHeap - ESP.getFreeHeap()));
      return;
  }

  this->_len = (display->width() * display->height());

  pinManager.allocatePin(mxconfig.gpio.r1, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.g1, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.b1, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.r2, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.g2, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.b2, true, PinOwner::HUB75);

  pinManager.allocatePin(mxconfig.gpio.lat, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.oe, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.clk, true, PinOwner::HUB75);

  pinManager.allocatePin(mxconfig.gpio.a, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.b, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.c, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.d, true, PinOwner::HUB75);
  pinManager.allocatePin(mxconfig.gpio.e, true, PinOwner::HUB75);

  // display->setLatBlanking(4);

  USER_PRINTLN("MatrixPanel_I2S_DMA created");
  // let's adjust default brightness
  //display->setBrightness8(25);    // range is 0-255, 0 - 0%, 255 - 100% //  [setBrightness()] Tried to set output brightness before begin()
  _bri = (last_bri > 0) ? last_bri : 25;  // try to restore persistent brightness value

  delay(24); // experimental
  DEBUG_PRINT(F("heap usage: ")); DEBUG_PRINTLN(int(lastHeap - ESP.getFreeHeap()));
  // Allocate memory and start DMA display
  if (newDisplay && (display->begin() == false)) {
      USER_PRINTLN("****** MatrixPanel_I2S_DMA !KABOOM! I2S memory allocation failed ***********");
      USER_PRINT(F("heap usage: ")); USER_PRINTLN(int(lastHeap - ESP.getFreeHeap()));
      _valid = false;
      return;
  }
  else {
    if (newDisplay) { USER_PRINTLN("MatrixPanel_I2S_DMA begin, started ok"); }
    else { USER_PRINTLN("MatrixPanel_I2S_DMA begin, using existing display."); }
    
    USER_PRINT(F("heap usage: ")); USER_PRINTLN(int(lastHeap - ESP.getFreeHeap()));
    delay(18);   // experiment - give the driver a moment (~ one full frame @ 60hz) to settle
    _valid = true;
    display->setBrightness8(_bri);    // range is 0-255, 0 - 0%, 255 - 100% //  [setBrightness()] Tried to set output brightness before begin()
    display->clearScreen();   // initially clear the screen buffer
    USER_PRINTLN("MatrixPanel_I2S_DMA clear ok");

    if (_ledBuffer) free(_ledBuffer);                 // should not happen
    if (_ledsDirty) free(_ledsDirty);                 // should not happen

    _ledsDirty = (byte*) malloc(getBitArrayBytes(_len));  // create LEDs dirty bits
    if (_ledsDirty) setBitArray(_ledsDirty, _len, false); // reset dirty bits

    #if defined(CONFIG_IDF_TARGET_ESP32S3) && CONFIG_SPIRAM_MODE_OCT && defined(BOARD_HAS_PSRAM) && (defined(WLED_USE_PSRAM) || defined(WLED_USE_PSRAM_JSON))
      if (psramFound()) {
        _ledBuffer = (CRGB*) ps_calloc(_len, sizeof(CRGB));  // create LEDs buffer (initialized to BLACK)
      } else {
        _ledBuffer = (CRGB*) calloc(_len, sizeof(CRGB));  // create LEDs buffer (initialized to BLACK)
      }
    #else
      _ledBuffer = (CRGB*) calloc(_len, sizeof(CRGB));  // create LEDs buffer (initialized to BLACK)
    #endif
  }

  if ((_ledBuffer == nullptr) || (_ledsDirty == nullptr)) {
      // fail is we cannot get memory for the buffer
      errorFlag = ERR_LOW_MEM; // WLEDMM raise errorflag
      USER_PRINTLN(F("MatrixPanel_I2S_DMA not started - not enough memory for leds buffer!"));
      cleanup();  // free buffers, and deallocate pins
      _valid = false;
      USER_PRINT(F("heap usage: ")); USER_PRINTLN(int(lastHeap - ESP.getFreeHeap()));
      return;  //  fail
  }
  
  switch(bc.type) {
    case 105:
      USER_PRINTLN("MatrixPanel_I2S_DMA FOUR_SCAN_32PX_HIGH - 32x32");
      if (!fourScanPanel) fourScanPanel = new VirtualMatrixPanel((*display), 1, mxconfig.chain_length, 32, 32);
      fourScanPanel->setPhysicalPanelScanRate(FOUR_SCAN_32PX_HIGH);
      fourScanPanel->setRotation(0);
      break;
    case 106:
      USER_PRINTLN("MatrixPanel_I2S_DMA FOUR_SCAN_32PX_HIGH - 64x32");
      if (!fourScanPanel) fourScanPanel = new VirtualMatrixPanel((*display), 1, mxconfig.chain_length, 64, 32);
      fourScanPanel->setPhysicalPanelScanRate(FOUR_SCAN_32PX_HIGH);
      fourScanPanel->setRotation(0);
      break;
    case 107:
      USER_PRINTLN("MatrixPanel_I2S_DMA FOUR_SCAN_64PX_HIGH");
      if (!fourScanPanel) fourScanPanel = new VirtualMatrixPanel((*display), 1, mxconfig.chain_length, 64, 64);
      fourScanPanel->setPhysicalPanelScanRate(FOUR_SCAN_64PX_HIGH);
      fourScanPanel->setRotation(0);
      break;
    case 108: // untested
      USER_PRINTLN("MatrixPanel_I2S_DMA 128x64 FOUR_SCAN_64PX_HIGH");
      if (!fourScanPanel) fourScanPanel = new VirtualMatrixPanel((*display), 1, mxconfig.chain_length, 128, 64);
      fourScanPanel->setPhysicalPanelScanRate(FOUR_SCAN_64PX_HIGH);
      fourScanPanel->setRotation(0);
      break;
  }  

  if (_valid) {
    _panelWidth = fourScanPanel ? fourScanPanel->width() : display->width();  // cache width - it will never change
  }

  USER_PRINT(F("MatrixPanel_I2S_DMA "));
  USER_PRINTF("%sstarted, width=%u, %u pixels.\n", _valid? "":"not ", _panelWidth, _len);

  if (_ledBuffer != nullptr) USER_PRINTLN(F("MatrixPanel_I2S_DMA LEDS buffer enabled."));
  if (_ledsDirty != nullptr) USER_PRINTLN(F("MatrixPanel_I2S_DMA LEDS dirty bit optimization enabled."));
  if ((_ledBuffer != nullptr) || (_ledsDirty != nullptr)) {
    USER_PRINT(F("MatrixPanel_I2S_DMA LEDS buffer uses "));
    USER_PRINT((_ledBuffer? _len*sizeof(CRGB) :0) + (_ledsDirty? getBitArrayBytes(_len) :0));
    USER_PRINTLN(F(" bytes."));
  }

  if (_valid) {
    // config is active, copy to global
    activeType = bc.type;
    activeDisplay = display;
    activeFourScanPanel = fourScanPanel;
    if (newDisplay) memcpy(&activeMXconfig, &mxconfig, sizeof(mxconfig));
  }
  instanceCount++;
  USER_PRINT(F("heap usage: ")); USER_PRINTLN(int(lastHeap - ESP.getFreeHeap()));
}

void __attribute__((hot)) IRAM_ATTR BusHub75Matrix::setPixelColor(uint16_t pix, uint32_t c) {
  if ( pix >= _len) return;
  // if (_cct >= 1900) c = colorBalanceFromKelvin(_cct, c); //color correction from CCT

  if (_ledBuffer) {
    CRGB fastled_col = CRGB(c);
    if (_ledBuffer[pix] != fastled_col) {
      _ledBuffer[pix] = fastled_col;
      setBitInArray(_ledsDirty, pix, true);  // flag pixel as "dirty"
    }
  }
  #if 0
  // !! this code is not used any more !!
  //   BusHub75Matrix::BusHub75Matrix will fail if allocating _ledBuffer fails.
  //   The fallback code below created lots of flickering so it does not make sense to keep it enabled.
  else {
    // no double buffer allocated --> directly draw pixel
    MatrixPanel_I2S_DMA* display = BusHub75Matrix::activeDisplay;
    VirtualMatrixPanel*  fourScanPanel = BusHub75Matrix::activeFourScanPanel;
    #ifndef NO_CIE1931
    c = unGamma24(c); // to use the driver linear brightness feature, we first need to undo WLED gamma correction
    #endif
    uint8_t r = R(c);
    uint8_t g = G(c);
    uint8_t b = B(c);

    if(fourScanPanel != nullptr) {
      int width = _panelWidth;
      int x = pix % width;
      int y = pix / width;
      fourScanPanel->drawPixelRGB888(int16_t(x), int16_t(y), r, g, b);
    } else {
      int width = _panelWidth;
      int x = pix % width;
      int y = pix / width;
      display->drawPixelRGB888(int16_t(x), int16_t(y), r, g, b);
    }
  }
  #endif
}

uint32_t IRAM_ATTR BusHub75Matrix::getPixelColor(uint16_t pix) const {
  if (pix >= _len || !_ledBuffer) return BLACK;
  return uint32_t(_ledBuffer[pix].scale8(_bri)) & 0x00FFFFFF;  // scale8() is needed to mimic NeoPixelBus, which returns scaled-down colours
}

uint32_t __attribute__((hot)) IRAM_ATTR BusHub75Matrix::getPixelColorRestored(uint16_t pix) const {
  if (pix >= _len || !_ledBuffer) return BLACK;
  return uint32_t(_ledBuffer[pix]) & 0x00FFFFFF;
}

void BusHub75Matrix::setBrightness(uint8_t b, bool immediate) {
  _bri = b;
  if (!_valid) return;
  MatrixPanel_I2S_DMA* display = BusHub75Matrix::activeDisplay;
  // if (_bri > 238) _bri=238; // not strictly needed. Enable this line if you see glitches at highest brightness.
  if ((_bri > 253) && (activeMXconfig.latch_blanking < 2)) _bri=253; // prevent glitches at highest brightness.
  last_bri = _bri;
  if (display) display->setBrightness(_bri);
}

void __attribute__((hot)) IRAM_ATTR BusHub75Matrix::show(void) {
  if (!_valid) return;
  MatrixPanel_I2S_DMA* display = BusHub75Matrix::activeDisplay;
  if (!display) return;
  display->setBrightness(_bri);

  if (_ledBuffer) {
    // write out buffered LEDs
    VirtualMatrixPanel*  fourScanPanel = BusHub75Matrix::activeFourScanPanel;
    bool isFourScan = (fourScanPanel != nullptr);
    //if (isFourScan) fourScanPanel->setRotation(0);
    unsigned height = isFourScan ? fourScanPanel->height() : display->height();
    unsigned width = _panelWidth;

    // Cache pointers to LED array and bitmask array, to avoid repeated accesses
    const byte* ledsDirty = _ledsDirty;
    const CRGB* ledBuffer = _ledBuffer;

    //while(!previousBufferFree) delay(1);   // experimental - Wait before we allow any writing to the buffer. Stop flicker.

    size_t pix = 0; // running pixel index
    for (int y=0; y<height; y++) for (int x=0; x<width; x++) {
      if (getBitFromArray(ledsDirty, pix) == true) {        // only repaint the "dirty"  pixels
        #ifndef NO_CIE1931
        uint32_t c = uint32_t(ledBuffer[pix]) & 0x00FFFFFF; // get RGB color, removing FastLED "alpha" component 
        c = unGamma24(c); // to use the driver linear brightness feature, we first need to undo WLED gamma correction
        uint8_t r = R(c);
        uint8_t g = G(c);
        uint8_t b = B(c);
        #else
        const CRGB c = ledBuffer[pix];  // we stay on CRGB, instead of packing/unpacking the color value to uint32_t
        uint8_t r = c.r;
        uint8_t g = c.g;
        uint8_t b = c.b;
        #endif
        if (isFourScan) fourScanPanel->drawPixelRGB888(int16_t(x), int16_t(y), r, g, b);
        else display->drawPixelRGB888(int16_t(x), int16_t(y), r, g, b);
      }
      pix ++;
    }
    setBitArray(_ledsDirty, _len, false);  // buffer shown - reset all dirty bits
  }
}

void BusHub75Matrix::cleanup() {
  MatrixPanel_I2S_DMA* display = BusHub75Matrix::activeDisplay;
  VirtualMatrixPanel*  fourScanPanel = BusHub75Matrix::activeFourScanPanel;
  if (display) display->clearScreen();

#if !defined(CONFIG_IDF_TARGET_ESP32S3) // S3: don't stop, as we want to re-use the driver later
  if (display && _valid) display->stopDMAoutput();  // terminate DMA driver (display goes black)
  _panelWidth = 0;
  USER_PRINTLN("HUB75 output ended.");
#else
  USER_PRINTLN("HUB75 output paused.");
#endif

  _valid = false;
  deallocatePins();
  _len = 0;
  //if (fourScanPanel != nullptr) delete fourScanPanel;  // warning: deleting object of polymorphic class type 'VirtualMatrixPanel' which has non-virtual destructor might cause undefined behavior
#if !defined(CONFIG_IDF_TARGET_ESP32S3) // S3: don't delete, as we want to re-use the driver later
  if (display) delete display;
  activeDisplay = nullptr;
  activeFourScanPanel = nullptr;
  USER_PRINTLN("HUB75 deleted.");
#else
  USER_PRINTLN("HUB75 cleanup done.");
#endif

  if (instanceCount > 0) instanceCount--;
  if (_ledBuffer != nullptr) free(_ledBuffer); _ledBuffer = nullptr;
  if (_ledsDirty != nullptr) free(_ledsDirty); _ledsDirty = nullptr;      
}

void BusHub75Matrix::deallocatePins() {

  pinManager.deallocatePin(activeMXconfig.gpio.r1, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.g1, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.b1, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.r2, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.g2, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.b2, PinOwner::HUB75);

  pinManager.deallocatePin(activeMXconfig.gpio.lat, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.oe, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.clk, PinOwner::HUB75);

  pinManager.deallocatePin(activeMXconfig.gpio.a, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.b, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.c, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.d, PinOwner::HUB75);
  pinManager.deallocatePin(activeMXconfig.gpio.e, PinOwner::HUB75);

}
#endif
// ***************************************************************************

//utility to get the approx. memory usage of a given BusConfig
uint32_t BusManager::memUsage(BusConfig &bc) {
  uint8_t type = bc.type;
  uint16_t len = bc.count + bc.skipAmount;
  if (type > 15 && type < 32) { // digital types
    if (type == TYPE_UCS8903 || type == TYPE_UCS8904) len *= 2; // 16-bit LEDs
    #ifdef ESP8266
      if (bc.pins[0] == 3) { //8266 DMA uses 5x the mem
        if (type > 28) return len*20; //RGBW
        return len*15;
      }
      if (type > 28) return len*4; //RGBW
      return len*3;
    #else //ESP32 RMT uses double buffer?
      if (type > 28) return len*8; //RGBW
      return len*6;
    #endif
  }
  if (type > 31 && type < 48)   return 5;
  return len*3; //RGB
}

int BusManager::add(BusConfig &bc) {
  if (getNumBusses() - getNumVirtualBusses() >= WLED_MAX_BUSSES) return -1;
  // WLEDMM clear cached Bus info first
  lastend = 0;
  laststart = 0;
  lastBus = nullptr;
  slowMode = false;

  DEBUG_PRINTF("BusManager::add(bc.type=%u)\n", bc.type);
  if (bc.type >= TYPE_NET_DDP_RGB && bc.type < 96) {
    busses[numBusses] = new BusNetwork(bc, colorOrderMap);
  } else if (bc.type >= TYPE_HUB75MATRIX && bc.type <= (TYPE_HUB75MATRIX + 10)) {
#ifdef WLED_ENABLE_HUB75MATRIX
    DEBUG_PRINTLN("BusManager::add - Adding BusHub75Matrix");
    busses[numBusses] = new BusHub75Matrix(bc);
    USER_PRINTLN("[BusHub75Matrix] ");
#else
    USER_PRINTLN("[unsupported! BusHub75Matrix - add flag -D WLED_ENABLE_HUB75MATRIX] ");
    return -1;
#endif
  } else if (IS_DIGITAL(bc.type)) {
    busses[numBusses] = new BusDigital(bc, numBusses, colorOrderMap);
  } else if (bc.type == TYPE_ONOFF) {
    busses[numBusses] = new BusOnOff(bc);
  } else {
    busses[numBusses] = new BusPwm(bc);
  }
  return numBusses++;
}

//do not call this method from system context (network callback)
void BusManager::removeAll() {
  DEBUG_PRINTLN(F("Removing all."));
  //prevents crashes due to deleting busses while in use.
#if !defined(ARDUINO_ARCH_ESP32)
  while (!canAllShow()) yield();
#else
  while (!canAllShow()) delay(2); // WLEDMM on esp32, yield() doesn't work as you think it would
#endif
  for (uint8_t i = 0; i < numBusses; i++) delete busses[i];
  numBusses = 0;
  // WLEDMM clear cached Bus info
  lastBus = nullptr;
  laststart = 0;
  lastend = 0;
  slowMode = false;
}

void __attribute__((hot)) BusManager::show() {
  for (unsigned i = 0; i < numBusses; i++) {
#if 1 && defined(ARDUINO_ARCH_ESP32)
    unsigned long t0 = millis();
    while ((busses[i]->canShow() == false) && (millis() - t0 < 80)) delay(1); // WLEDMM experimental: wait until bus driver is ready (max 80ms) - costs us 1-2 fps but reduces flickering
#endif
    busses[i]->show();
  }
}

void BusManager::setStatusPixel(uint32_t c) {
  for (uint8_t i = 0; i < numBusses; i++) {
    if (busses[i]->isOk() == false) continue;  // WLEDMM ignore invalid (=not ready) busses
    busses[i]->setStatusPixel(c);
  }
}

void IRAM_ATTR __attribute__((hot)) BusManager::setPixelColor(uint16_t pix, uint32_t c, int16_t cct) {
  if (!slowMode && (pix >= laststart) && (pix < lastend ) && lastBus->isOk()) {
    // WLEDMM same bus as last time - no need to search again
    lastBus->setPixelColor(pix - laststart, c);
    return;
  }

  for (uint_fast8_t i = 0; i < numBusses; i++) {    // WLEDMM use fast native types
    Bus* b = busses[i];
    if (b->isOk() == false) continue;  // WLEDMM ignore invalid (=not ready) busses
    uint_fast16_t bstart = b->getStart();
    if (pix < bstart || pix >= bstart + b->getLength()) continue;
    else {
      if (!slowMode) {
        // WLEDMM remember last Bus we took
        lastBus = b;
        laststart = bstart; 
        lastend = bstart + b->getLength();
      }
      b->setPixelColor(pix - bstart, c);
      if (!slowMode) break; // WLEDMM found the right Bus -> so we can stop searching - unless we have busses that overlap
    }
  }
}

void BusManager::setBrightness(uint8_t b, bool immediate) {
  for (uint8_t i = 0; i < numBusses; i++) {
    busses[i]->setBrightness(b, immediate);
  }
}

void __attribute__((cold)) BusManager::setSegmentCCT(int16_t cct, bool allowWBCorrection) {
  if (cct > 255) cct = 255;
  if (cct >= 0) {
    //if white balance correction allowed, save as kelvin value instead of 0-255
    if (allowWBCorrection) cct = 1900 + (cct << 5);
  } else cct = -1;
  Bus::setCCT(cct);
}

uint32_t IRAM_ATTR  __attribute__((hot)) BusManager::getPixelColor(uint_fast16_t pix) {     // WLEDMM use fast native types, IRAM_ATTR
  if ((pix >= laststart) && (pix < lastend ) && (lastBus != nullptr) && lastBus->isOk()) {
    // WLEDMM same bus as last time - no need to search again
    return lastBus->getPixelColor(pix - laststart);
  }

  for (uint_fast8_t i = 0; i < numBusses; i++) {
    Bus* b = busses[i];
    if (b->isOk() == false) continue;  // WLEDMM ignore invalid (=not ready) busses
    uint_fast16_t bstart = b->getStart();
    if (pix < bstart || pix >= bstart + b->getLength()) continue;
    else {
      if (!slowMode) {
        // WLEDMM remember last Bus we took
        lastBus = b;
        laststart = bstart; 
        lastend = bstart + b->getLength();
      }
      return b->getPixelColor(pix - bstart);
    }
  }
  return 0;
}

uint32_t IRAM_ATTR  __attribute__((hot)) BusManager::getPixelColorRestored(uint_fast16_t pix) {     // WLEDMM uses bus::getPixelColorRestored()
  if ((pix >= laststart) && (pix < lastend ) && (lastBus != nullptr) && lastBus->isOk()) {
    // WLEDMM same bus as last time - no need to search again
    return lastBus->getPixelColorRestored(pix - laststart);
  }

  for (uint_fast8_t i = 0; i < numBusses; i++) {
    Bus* b = busses[i];
    if (b->isOk() == false) continue;  // WLEDMM ignore invalid (=not ready) busses
    uint_fast16_t bstart = b->getStart();
    if (pix < bstart || pix >= bstart + b->getLength()) continue;
    else {
      if (!slowMode) {
        // WLEDMM remember last Bus we took
        lastBus = b;
        laststart = bstart; 
        lastend = bstart + b->getLength();
      }
      return b->getPixelColorRestored(pix - bstart);
    }
  }
  return 0;
}

bool BusManager::canAllShow() const {
  for (uint8_t i = 0; i < numBusses; i++) {
    if (!busses[i]->canShow()) return false;
  }
  return true;
}

Bus* BusManager::getBus(uint8_t busNr) const {
  if (busNr >= numBusses) return nullptr;
  return busses[busNr];
}

//semi-duplicate of strip.getLengthTotal() (though that just returns strip._length, calculated in finalizeInit())
uint16_t BusManager::getTotalLength() const {
  uint_fast16_t len = 0;
  for (uint_fast8_t i=0; i<numBusses; i++) len += busses[i]->getLength();      // WLEDMM use fast native types
  return len;
}

// Bus static member definition
int16_t Bus::_cct = -1;
uint8_t Bus::_cctBlend = 0;
uint8_t Bus::_gAWM = 255;
