#ifndef BusManager_h
#define BusManager_h

/*
 * Class for addressing various light types
 */

#include "const.h"
#include "pin_manager.h"
#include "bus_wrapper.h"
#include <Arduino.h>

// enable additional debug output
#ifdef WLED_DEBUG
  #ifndef ESP8266
  #include <rom/rtc.h>
  #endif
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(x...) Serial.printf(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(x...)
#endif

#define GET_BIT(var,bit)    (((var)>>(bit))&0x01)
#define SET_BIT(var,bit)    ((var)|=(uint16_t)(0x0001<<(bit)))
#define UNSET_BIT(var,bit)  ((var)&=(~(uint16_t)(0x0001<<(bit))))

//temporary struct for passing bus configuration to bus
struct BusConfig {
  uint8_t type = TYPE_WS2812_RGB;
  uint16_t count = 1;
  uint16_t start = 0;
  uint8_t colorOrder = COL_ORDER_GRB;
  bool reversed = false;
  uint8_t skipAmount;
  bool refreshReq;
  uint8_t pins[5] = {LEDPIN, 255, 255, 255, 255};
  BusConfig(uint8_t busType, uint8_t* ppins, uint16_t pstart, uint16_t len = 1, uint8_t pcolorOrder = COL_ORDER_GRB, bool rev = false, uint8_t skip = 0) {
    refreshReq = (bool) GET_BIT(busType,7);
    type = busType & 0x7F;  // bit 7 may be/is hacked to include refresh info (1=refresh in off state, 0=no refresh)
    count = len; start = pstart; colorOrder = pcolorOrder; reversed = rev; skipAmount = skip;
    uint8_t nPins = 1;
    if (type >= TYPE_NET_DDP_RGB && type < 96) nPins = 4; //virtual network bus. 4 "pins" store IP address
    else if (type > 47) nPins = 2;
    else if (type > 40 && type < 46) nPins = NUM_PWM_PINS(type);
    for (uint8_t i = 0; i < nPins; i++) pins[i] = ppins[i];
  }

  //validates start and length and extends total if needed
  bool adjustBounds(uint16_t& total) {
    if (!count) count = 1;
    if (count > MAX_LEDS_PER_BUS) count = MAX_LEDS_PER_BUS;
    if (start >= MAX_LEDS) return false;
    //limit length of strip if it would exceed total permissible LEDs
    if (start + count > MAX_LEDS) count = MAX_LEDS - start;
    //extend total count accordingly
    if (start + count > total) total = start + count;
    return true;
  }
};

//parent class of BusDigital and BusPwm
class Bus {
  public:
  Bus(uint8_t type, uint16_t start) {
    _type = type;
    _start = start;
  };
  
  virtual void show() {}
  virtual bool canShow() { return true; }

  virtual void setPixelColor(uint16_t pix, uint32_t c) {};

  virtual void setBrightness(uint8_t b) {};

  virtual uint32_t getPixelColor(uint16_t pix) { return 0; };

  virtual void cleanup() {};

  virtual ~Bus() { //throw the bus under the bus
  }

  virtual uint8_t getPins(uint8_t* pinArray) { return 0; }

  inline uint16_t getStart() {
    return _start;
  }

  inline void setStart(uint16_t start) {
    _start = start;
  }

  virtual uint16_t getLength() {
    return 1;
  }

  virtual void setColorOrder() {}

  virtual uint8_t getColorOrder() {
    return COL_ORDER_RGB;
  }

  virtual bool isRgbw() {
    return false;
  }

  virtual uint8_t skippedLeds() {
    return 0;
  }

  inline uint8_t getType() {
    return _type;
  }

  inline bool isOk() {
    return _valid;
  }

  static bool isRgbw(uint8_t type) {
    if (type == TYPE_SK6812_RGBW || type == TYPE_TM1814) return true;
    if (type > TYPE_ONOFF && type <= TYPE_ANALOG_5CH && type != TYPE_ANALOG_3CH) return true;
    return false;
  }

  inline bool isOffRefreshRequired() {
    return _needsRefresh;
  }

  bool reversed = false;

  protected:
  uint8_t _type = TYPE_NONE;
  uint8_t _bri = 255;
  uint16_t _start = 0;
  bool _valid = false;
  bool _needsRefresh = false;
};


class BusDigital : public Bus {
  public:
  BusDigital(BusConfig &bc, uint8_t nr) : Bus(bc.type, bc.start) {
    if (!IS_DIGITAL(bc.type) || !bc.count) return;
    if (!pinManager.allocatePin(bc.pins[0], true, PinOwner::BusDigital)) return;
    _pins[0] = bc.pins[0];
    if (IS_2PIN(bc.type)) {
      if (!pinManager.allocatePin(bc.pins[1], true, PinOwner::BusDigital)) {
        cleanup(); return;
      }
      _pins[1] = bc.pins[1];
    }
    reversed = bc.reversed;
    _needsRefresh = bc.refreshReq || bc.type == TYPE_TM1814;
    _skip = bc.skipAmount;    //sacrificial pixels
    _len = bc.count + _skip;
    _iType = PolyBus::getI(bc.type, _pins, nr);
    if (_iType == I_NONE) return;
    _busPtr = PolyBus::create(_iType, _pins, _len, nr);
    _valid = (_busPtr != nullptr);
    _colorOrder = bc.colorOrder;
    DEBUG_PRINTF("Successfully inited strip %u (len %u) with type %u and pins %u,%u (itype %u)\n",nr, _len, bc.type, _pins[0],_pins[1],_iType);
  };

  inline void show() {
    PolyBus::show(_busPtr, _iType);
  }

  inline bool canShow() {
    return PolyBus::canShow(_busPtr, _iType);
  }

  void setBrightness(uint8_t b) {
    //Fix for turning off onboard LED breaking bus
    #ifdef LED_BUILTIN
    if (_bri == 0 && b > 0) {
      if (_pins[0] == LED_BUILTIN || _pins[1] == LED_BUILTIN) PolyBus::begin(_busPtr, _iType, _pins); 
    }
    #endif
    _bri = b;
    PolyBus::setBrightness(_busPtr, _iType, b);
  }

  void setPixelColor(uint16_t pix, uint32_t c) {
    if (reversed) pix = _len - pix -1;
    else pix += _skip;
    PolyBus::setPixelColor(_busPtr, _iType, pix, c, _colorOrder);
  }

  uint32_t getPixelColor(uint16_t pix) {
    if (reversed) pix = _len - pix -1;
    else pix += _skip;
    return PolyBus::getPixelColor(_busPtr, _iType, pix, _colorOrder);
  }

  inline uint8_t getColorOrder() {
    return _colorOrder;
  }

  inline uint16_t getLength() {
    return _len - _skip;
  }

  uint8_t getPins(uint8_t* pinArray) {
    uint8_t numPins = IS_2PIN(_type) ? 2 : 1;
    for (uint8_t i = 0; i < numPins; i++) pinArray[i] = _pins[i];
    return numPins;
  }

  void setColorOrder(uint8_t colorOrder) {
    if (colorOrder > 5) return;
    _colorOrder = colorOrder;
  }

  inline bool isRgbw() {
    return Bus::isRgbw(_type);
  }

  inline uint8_t skippedLeds() {
    return _skip;
  }

  inline void reinit() {
    PolyBus::begin(_busPtr, _iType, _pins);
  }

  void cleanup() {
    DEBUG_PRINTLN(F("Digital Cleanup."));
    PolyBus::cleanup(_busPtr, _iType);
    _iType = I_NONE;
    _valid = false;
    _busPtr = nullptr;
    pinManager.deallocatePin(_pins[1], PinOwner::BusDigital);
    pinManager.deallocatePin(_pins[0], PinOwner::BusDigital);
  }

  ~BusDigital() {
    cleanup();
  }

  private: 
  uint8_t _colorOrder = COL_ORDER_GRB;
  uint8_t _pins[2] = {255, 255};
  uint8_t _iType = I_NONE;
  uint16_t _len = 0;
  uint8_t _skip = 0;
  void * _busPtr = nullptr;
};


class BusPwm : public Bus {
  public:
  BusPwm(BusConfig &bc) : Bus(bc.type, bc.start) {
    _valid = false;
    if (!IS_PWM(bc.type)) return;
    uint8_t numPins = NUM_PWM_PINS(bc.type);

    #ifdef ESP8266
    analogWriteRange(255);  //same range as one RGB channel
    analogWriteFreq(WLED_PWM_FREQ);
    #else
    _ledcStart = pinManager.allocateLedc(numPins);
    if (_ledcStart == 255) { //no more free LEDC channels
      deallocatePins(); return;
    }
    #endif

    for (uint8_t i = 0; i < numPins; i++) {
      uint8_t currentPin = bc.pins[i];
      if (!pinManager.allocatePin(currentPin, true, PinOwner::BusPwm)) {
        deallocatePins(); return;
      }
      _pins[i] = currentPin; // store only after allocatePin() succeeds
      #ifdef ESP8266
      pinMode(_pins[i], OUTPUT);
      #else
      ledcSetup(_ledcStart + i, WLED_PWM_FREQ, 8);
      ledcAttachPin(_pins[i], _ledcStart + i);
      #endif
    }
    reversed = bc.reversed;
    _valid = true;
  };

  void setPixelColor(uint16_t pix, uint32_t c) {
    if (pix != 0 || !_valid) return; //only react to first pixel
    uint8_t r = c >> 16;
    uint8_t g = c >>  8;
    uint8_t b = c      ;
    uint8_t w = c >> 24;

    switch (_type) {
      case TYPE_ANALOG_1CH: //one channel (white), use highest RGBW value
        _data[0] = max(r, max(g, max(b, w))); break;
      
      case TYPE_ANALOG_2CH: //warm white + cold white, we'll need some nice handling here, for now just R+G channels
      case TYPE_ANALOG_3CH: //standard dumb RGB
      case TYPE_ANALOG_4CH: //RGBW
      case TYPE_ANALOG_5CH: //we'll want the white handling from 2CH here + RGB
        _data[0] = r; _data[1] = g; _data[2] = b; _data[3] = w; _data[4] = 0; break;

      default: return;
    }
  }

  //does no index check
  uint32_t getPixelColor(uint16_t pix) {
    if (!_valid) return 0;
    return ((_data[3] << 24) | (_data[0] << 16) | (_data[1] << 8) | (_data[2]));
  }

  void show() {
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

  inline void setBrightness(uint8_t b) {
    _bri = b;
  }

  uint8_t getPins(uint8_t* pinArray) {
    if (!_valid) return 0;
    uint8_t numPins = NUM_PWM_PINS(_type);
    for (uint8_t i = 0; i < numPins; i++) pinArray[i] = _pins[i];
    return numPins;
  }

  bool isRgbw() {
    return Bus::isRgbw(_type);
  }

  inline void cleanup() {
    deallocatePins();
  }

  ~BusPwm() {
    cleanup();
  }

  private: 
  uint8_t _pins[5] = {255, 255, 255, 255, 255};
  uint8_t _data[5] = {255, 255, 255, 255, 255};
  #ifdef ARDUINO_ARCH_ESP32
  uint8_t _ledcStart = 255;
  #endif

  void deallocatePins() {
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
};


class BusNetwork : public Bus {
  public:
    BusNetwork(BusConfig &bc) : Bus(bc.type, bc.start) {
      _valid = false;
//      switch (bc.type) {
//        case TYPE_NET_ARTNET_RGB:
//          _rgbw = false;
//          _UDPtype = 2;
//          break;
//        case TYPE_NET_E131_RGB:
//          _rgbw = false;
//          _UDPtype = 1;
//          break;
//        case TYPE_NET_DDP_RGB:
//          _rgbw = false;
//          _UDPtype = 0;
//          break;
//        default:
          _rgbw = false;
          _UDPtype = bc.type - TYPE_NET_DDP_RGB;
//          break;
//      }
      _UDPchannels = _rgbw ? 4 : 3;
      //_rgbw |= bc.rgbwOverride;  // RGBW override in bit 7 or can have a special type
      _data = (byte *)malloc(bc.count * _UDPchannels);
      if (_data == nullptr) return;
      memset(_data, 0, bc.count * _UDPchannels);
      _len = bc.count;
      //_colorOrder = bc.colorOrder;
      _client = IPAddress(bc.pins[0],bc.pins[1],bc.pins[2],bc.pins[3]);
      _broadcastLock = false;
      _valid = true;
    };

  void setPixelColor(uint16_t pix, uint32_t c) {
    if (!_valid || pix >= _len) return;
    uint16_t offset = pix * _UDPchannels;
    _data[offset]   = 0xFF & (c >> 16);
    _data[offset+1] = 0xFF & (c >>  8);
    _data[offset+2] = 0xFF & (c      );
    if (_rgbw) _data[offset+3] = 0xFF & (c >> 24);
  }

  uint32_t getPixelColor(uint16_t pix) {
    if (!_valid || pix >= _len) return 0;
    uint16_t offset = pix * _UDPchannels;
    return (
      (_rgbw ? (_data[offset+3] << 24) : 0)
      | (_data[offset]   << 16)
      | (_data[offset+1] <<  8)
      | (_data[offset+2]      )
    );
  }

  void show() {
    if (!_valid || !canShow()) return;
    _broadcastLock = true;
    realtimeBroadcast(_UDPtype, _client, _len, _data, _bri, _rgbw);
    _broadcastLock = false;
  }

  inline bool canShow() {
    // this should be a return value from UDP routine if it is still sending data out
    return !_broadcastLock;
  }

  inline void setBrightness(uint8_t b) {
    _bri = b;
  }

  uint8_t getPins(uint8_t* pinArray) {
    for (uint8_t i = 0; i < 4; i++) {
      pinArray[i] = _client[i];
    }
    return 4;
  }

  inline bool isRgbw() {
    return _rgbw;
  }

  inline uint16_t getLength() {
    return _len;
  }

  void cleanup() {
    _type = I_NONE;
    _valid = false;
    if (_data != nullptr) free(_data);
    _data = nullptr;
  }

  ~BusNetwork() {
    cleanup();
  }

  private:
    IPAddress _client;
    uint16_t  _len = 0;
    //uint8_t   _colorOrder;
    uint8_t   _bri = 255;
    uint8_t   _UDPtype;
    uint8_t   _UDPchannels;
    bool      _rgbw;
    bool      _broadcastLock;
    byte     *_data;
};


class BusManager {
  public:
  BusManager() {

  };

  //utility to get the approx. memory usage of a given BusConfig
  static uint32_t memUsage(BusConfig &bc) {
    uint8_t type = bc.type;
    uint16_t len = bc.count;
    if (type > 15 && type < 32) {
      #ifdef ESP8266
        if (bc.pins[0] == 3) { //8266 DMA uses 5x the mem
          if (type > 29) return len*20; //RGBW
          return len*15;
        }
        if (type > 29) return len*4; //RGBW
        return len*3;
      #else //ESP32 RMT uses double buffer?
        if (type > 29) return len*8; //RGBW
        return len*6;
      #endif
    }
    if (type > 31 && type < 48)   return 5;
    if (type == 44 || type == 45) return len*4; //RGBW
    return len*3; //RGB
  }
  
  int add(BusConfig &bc) {
    if (numBusses >= WLED_MAX_BUSSES) return -1;
    if (bc.type >= TYPE_NET_DDP_RGB && bc.type < 96) {
      busses[numBusses] = new BusNetwork(bc);
    } else if (IS_DIGITAL(bc.type)) {
      busses[numBusses] = new BusDigital(bc, numBusses);
    } else {
      busses[numBusses] = new BusPwm(bc);
    }
    return numBusses++;
  }

  //do not call this method from system context (network callback)
  void removeAll() {
    DEBUG_PRINTLN(F("Removing all."));
    //prevents crashes due to deleting busses while in use. 
    while (!canAllShow()) yield();
    for (uint8_t i = 0; i < numBusses; i++) delete busses[i];
    numBusses = 0;
  }

  void show() {
    for (uint8_t i = 0; i < numBusses; i++) {
      busses[i]->show();
    }
  }

  void setPixelColor(uint16_t pix, uint32_t c) {
    for (uint8_t i = 0; i < numBusses; i++) {
      Bus* b = busses[i];
      uint16_t bstart = b->getStart();
      if (pix < bstart || pix >= bstart + b->getLength()) continue;
      busses[i]->setPixelColor(pix - bstart, c);
    }
  }

  void setBrightness(uint8_t b) {
    for (uint8_t i = 0; i < numBusses; i++) {
      busses[i]->setBrightness(b);
    }
  }

  uint32_t getPixelColor(uint16_t pix) {
    for (uint8_t i = 0; i < numBusses; i++) {
      Bus* b = busses[i];
      uint16_t bstart = b->getStart();
      if (pix < bstart || pix >= bstart + b->getLength()) continue;
      return b->getPixelColor(pix - bstart);
    }
    return 0;
  }

  bool canAllShow() {
    for (uint8_t i = 0; i < numBusses; i++) {
      if (!busses[i]->canShow()) return false;
    }
    return true;
  }

  Bus* getBus(uint8_t busNr) {
    if (busNr >= numBusses) return nullptr;
    return busses[busNr];
  }

  inline uint8_t getNumBusses() {
    return numBusses;
  }

  //semi-duplicate of strip.getLengthTotal() (though that just returns strip._length, calculated in finalizeInit())
  uint16_t getTotalLength() {
    uint16_t len = 0;
    for (uint8_t i=0; i<numBusses; i++) len += busses[i]->getLength();
    return len;
  }

  private:
  uint8_t numBusses = 0;
  Bus* busses[WLED_MAX_BUSSES];
};
#endif
