#ifndef BusManager_h
#define BusManager_h

/*
 * Class for addressing various light types
 */

#include "const.h"

#define GET_BIT(var,bit)    (((var)>>(bit))&0x01)
#define SET_BIT(var,bit)    ((var)|=(uint16_t)(0x0001<<(bit)))
#define UNSET_BIT(var,bit)  ((var)&=(~(uint16_t)(0x0001<<(bit))))

#define NUM_ICS_WS2812_1CH_3X(len) (((len)+2)/3)   // 1 WS2811 IC controls 3 zones (each zone has 1 LED, W)
#define IC_INDEX_WS2812_1CH_3X(i)  ((i)/3)

#define NUM_ICS_WS2812_2CH_3X(len) (((len)+1)*2/3) // 2 WS2811 ICs control 3 zones (each zone has 2 LEDs, CW and WW)
#define IC_INDEX_WS2812_2CH_3X(i)  ((i)*2/3)
#define WS2812_2CH_3X_SPANS_2_ICS(i) ((i)&0x01)    // every other LED zone is on two different ICs

//temporary struct for passing bus configuration to bus
struct BusConfig {
  uint8_t type;
  uint16_t count;
  uint16_t start;
  uint8_t colorOrder;
  bool reversed;
  uint8_t skipAmount;
  bool refreshReq;
  uint8_t autoWhite;
  uint8_t pins[5] = {LEDPIN, 255, 255, 255, 255};
  uint16_t frequency;
  BusConfig(uint8_t busType, uint8_t* ppins, uint16_t pstart, uint16_t len = 1, uint8_t pcolorOrder = COL_ORDER_GRB, bool rev = false, uint8_t skip = 0, byte aw=RGBW_MODE_MANUAL_ONLY, uint16_t clock_kHz=0U) {
    refreshReq = (bool) GET_BIT(busType,7);
    type = busType & 0x7F;  // bit 7 may be/is hacked to include refresh info (1=refresh in off state, 0=no refresh)
    count = len; start = pstart; colorOrder = pcolorOrder; reversed = rev; skipAmount = skip; autoWhite = aw; frequency = clock_kHz;
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

// Defines an LED Strip and its color ordering.
struct ColorOrderMapEntry {
  uint16_t start;
  uint16_t len;
  uint8_t colorOrder;
};

struct ColorOrderMap {
    void add(uint16_t start, uint16_t len, uint8_t colorOrder);

    uint8_t count() const {
      return _count;
    }

    void reset() {
      _count = 0;
      memset(_mappings, 0, sizeof(_mappings));
    }

    const ColorOrderMapEntry* get(uint8_t n) const {
      if (n > _count) {
        return nullptr;
      }
      return &(_mappings[n]);
    }

    uint8_t getPixelColorOrder(uint16_t pix, uint8_t defaultColorOrder) const;

  private:
    uint8_t _count;
    ColorOrderMapEntry _mappings[WLED_MAX_COLOR_ORDER_MAPPINGS];
};

//parent class of BusDigital, BusPwm, and BusNetwork
class Bus {
  public:
    Bus(uint8_t type, uint16_t start, uint8_t aw)
    : _bri(255)
    , _len(1)
    , _valid(false)
    , _needsRefresh(false)
    {
      _type = type;
      _start = start;
      _autoWhiteMode = Bus::hasWhite(_type) ? aw : RGBW_MODE_MANUAL_ONLY;
    };

    virtual ~Bus() {} //throw the bus under the bus

    virtual void     show() = 0;
    virtual bool     canShow() { return true; }
    virtual void     setStatusPixel(uint32_t c) {}
    virtual void     setPixelColor(uint16_t pix, uint32_t c) = 0;
    virtual uint32_t getPixelColor(uint16_t pix) { return 0; }
    virtual void     setBrightness(uint8_t b, bool immediate=false) { _bri = b; };
    virtual void     cleanup() = 0;
    virtual uint8_t  getPins(uint8_t* pinArray) { return 0; }
    virtual uint16_t getLength() { return _len; }
    virtual void     setColorOrder() {}
    virtual uint8_t  getColorOrder() { return COL_ORDER_RGB; }
    virtual uint8_t  skippedLeds() { return 0; }
    virtual uint16_t getFrequency() { return 0U; }
    inline  uint16_t getStart() { return _start; }
    inline  void     setStart(uint16_t start) { _start = start; }
    inline  uint8_t  getType() { return _type; }
    inline  bool     isOk() { return _valid; }
    inline  bool     isOffRefreshRequired() { return _needsRefresh; }
            bool     containsPixel(uint16_t pix) { return pix >= _start && pix < _start+_len; }

    virtual bool hasRGB() {
      if ((_type >= TYPE_WS2812_1CH && _type <= TYPE_WS2812_WWA) || _type == TYPE_ANALOG_1CH || _type == TYPE_ANALOG_2CH || _type == TYPE_ONOFF) return false;
      return true;
    }
    virtual bool hasWhite() { return Bus::hasWhite(_type); }
    static  bool hasWhite(uint8_t type) {
      if ((type >= TYPE_WS2812_1CH && type <= TYPE_WS2812_WWA) || type == TYPE_SK6812_RGBW || type == TYPE_TM1814) return true; // digital types with white channel
      if (type > TYPE_ONOFF && type <= TYPE_ANALOG_5CH && type != TYPE_ANALOG_3CH) return true; // analog types with white channel
      if (type == TYPE_NET_DDP_RGBW) return true; // network types with white channel
      return false;
    }
    virtual bool hasCCT() {
      if (_type == TYPE_WS2812_2CH_X3 || _type == TYPE_WS2812_WWA ||
          _type == TYPE_ANALOG_2CH    || _type == TYPE_ANALOG_5CH) return true;
      return false;
    }
    static void setCCT(uint16_t cct) {
      _cct = cct;
    }
    static void setCCTBlend(uint8_t b) {
      if (b > 100) b = 100;
      _cctBlend = (b * 127) / 100;
      //compile-time limiter for hardware that can't power both white channels at max
      #ifdef WLED_MAX_CCT_BLEND
        if (_cctBlend > WLED_MAX_CCT_BLEND) _cctBlend = WLED_MAX_CCT_BLEND;
      #endif
    }
    inline        void    setAutoWhiteMode(uint8_t m) { if (m < 5) _autoWhiteMode = m; }
    inline        uint8_t getAutoWhiteMode()          { return _autoWhiteMode; }
    inline static void    setGlobalAWMode(uint8_t m)  { if (m < 5) _gAWM = m; else _gAWM = AW_GLOBAL_DISABLED; }
    inline static uint8_t getGlobalAWMode()           { return _gAWM; }

    bool reversed = false;

  protected:
    uint8_t  _type;
    uint8_t  _bri;
    uint16_t _start;
    uint16_t _len;
    bool     _valid;
    bool     _needsRefresh;
    uint8_t  _autoWhiteMode;
    static uint8_t _gAWM;
    static int16_t _cct;
    static uint8_t _cctBlend;

    uint32_t autoWhiteCalc(uint32_t c);
};


class BusDigital : public Bus {
  public:
    BusDigital(BusConfig &bc, uint8_t nr, const ColorOrderMap &com);

    inline void show();

    bool canShow();

    void setBrightness(uint8_t b, bool immediate);

    void setStatusPixel(uint32_t c);

    void setPixelColor(uint16_t pix, uint32_t c);

    uint32_t getPixelColor(uint16_t pix);

    uint8_t getColorOrder() {
      return _colorOrder;
    }

    uint16_t getLength() {
      return _len - _skip;
    }

    uint8_t getPins(uint8_t* pinArray);

    void setColorOrder(uint8_t colorOrder);

    uint8_t skippedLeds() {
      return _skip;
    }

    uint16_t getFrequency() { return _frequencykHz; }

    void reinit();

    void cleanup();

    ~BusDigital() {
      cleanup();
    }

  private:
    uint8_t _colorOrder = COL_ORDER_GRB;
    uint8_t _pins[2] = {255, 255};
    uint8_t _iType = 0; //I_NONE;
    uint8_t _skip = 0;
    uint16_t _frequencykHz = 0U;
    void * _busPtr = nullptr;
    const ColorOrderMap &_colorOrderMap;
};


class BusPwm : public Bus {
  public:
    BusPwm(BusConfig &bc);

    void setPixelColor(uint16_t pix, uint32_t c);

    //does no index check
    uint32_t getPixelColor(uint16_t pix);

    void show();

    uint8_t getPins(uint8_t* pinArray);

    uint16_t getFrequency() { return _frequency; }

    void cleanup() {
      deallocatePins();
    }

    ~BusPwm() {
      cleanup();
    }

  private:
    uint8_t _pins[5] = {255, 255, 255, 255, 255};
    uint8_t _data[5] = {0};
    #ifdef ARDUINO_ARCH_ESP32
    uint8_t _ledcStart = 255;
    #endif
    uint16_t _frequency = 0U;

    void deallocatePins();
};


class BusOnOff : public Bus {
  public:
    BusOnOff(BusConfig &bc);

    void setPixelColor(uint16_t pix, uint32_t c);

    uint32_t getPixelColor(uint16_t pix);

    void show();

    uint8_t getPins(uint8_t* pinArray);

    void cleanup() {
      pinManager.deallocatePin(_pin, PinOwner::BusOnOff);
    }

    ~BusOnOff() {
      cleanup();
    }

  private:
    uint8_t _pin = 255;
    uint8_t _data = 0;
};


class BusNetwork : public Bus {
  public:
    BusNetwork(BusConfig &bc);

    bool hasRGB() { return true; }
    bool hasWhite() { return _rgbw; }

    void setPixelColor(uint16_t pix, uint32_t c);

    uint32_t getPixelColor(uint16_t pix);

    void show();

    bool canShow() {
      // this should be a return value from UDP routine if it is still sending data out
      return !_broadcastLock;
    }

    uint8_t getPins(uint8_t* pinArray);

    uint16_t getLength() {
      return _len;
    }

    void cleanup();

    ~BusNetwork() {
      cleanup();
    }

  private:
    IPAddress _client;
    uint8_t   _UDPtype;
    uint8_t   _UDPchannels;
    bool      _rgbw;
    bool      _broadcastLock;
    byte     *_data;
};


class BusManager {
  public:
    BusManager() {};

    //utility to get the approx. memory usage of a given BusConfig
    static uint32_t memUsage(BusConfig &bc);

    int add(BusConfig &bc);

    //do not call this method from system context (network callback)
    void removeAll();

    void show();

    void setStatusPixel(uint32_t c);

    void setPixelColor(uint16_t pix, uint32_t c, int16_t cct=-1);

    void setBrightness(uint8_t b, bool immediate=false);          // immediate=true is for use in ABL, it applies brightness immediately (warning: inefficient)

    void setSegmentCCT(int16_t cct, bool allowWBCorrection = false);

    uint32_t getPixelColor(uint16_t pix);

    bool canAllShow();

    Bus* getBus(uint8_t busNr);

    //semi-duplicate of strip.getLengthTotal() (though that just returns strip._length, calculated in finalizeInit())
    uint16_t getTotalLength();

    inline void updateColorOrderMap(const ColorOrderMap &com) {
      memcpy(&colorOrderMap, &com, sizeof(ColorOrderMap));
    }

    inline const ColorOrderMap& getColorOrderMap() const {
      return colorOrderMap;
    }

    inline uint8_t getNumBusses() {
      return numBusses;
    }

  private:
    uint8_t numBusses = 0;
    Bus* busses[WLED_MAX_BUSSES+WLED_MIN_VIRTUAL_BUSSES];
    ColorOrderMap colorOrderMap;

    inline uint8_t getNumVirtualBusses() {
      int j = 0;
      for (int i=0; i<numBusses; i++) if (busses[i]->getType() >= TYPE_NET_DDP_RGB && busses[i]->getType() < 96) j++;
      return j;
    }
};
#endif