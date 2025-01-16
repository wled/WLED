#ifndef BusManager_h
#define BusManager_h

#ifdef WLED_ENABLE_HUB75MATRIX
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
//extern volatile bool previousBufferFree; // experimental
#endif
/*
 * Class for addressing various light types
 */

#include "const.h"

#if !defined(FASTLED_VERSION) // only pull in FastLED if we don't have it yet
  #define FASTLED_INTERNAL
  #include <FastLED.h>
#endif

//color mangling macros
#if !defined(RGBW32)
  #define RGBW32(r,g,b,w) (uint32_t((byte(w) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
  #define R(c) (byte((c) >> 16))
  #define G(c) (byte((c) >> 8))
  #define B(c) (byte(c))
  #define W(c) (byte((c) >> 24))
#endif

// WLEDMM bitarray utilities
void setBitInArray(uint8_t* byteArray, size_t position, bool value);  // set bit
bool getBitFromArray(const uint8_t* byteArray, size_t position)  __attribute__((pure)); // get bit value
size_t getBitArrayBytes(size_t num_bits) __attribute__((const)); // number of bytes needed for an array with num_bits bits
void setBitArray(uint8_t* byteArray, size_t numBits, bool value);  // set all bits to same value


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
  uint8_t artnet_outputs, artnet_fps_limit;
  uint16_t artnet_leds_per_output;

  uint8_t pins[5] = {LEDPIN, 255, 255, 255, 255}; // WLEDMM warning: this means that BusConfig cannot handle nore than 5 pins per bus!
  uint16_t frequency;
  BusConfig(uint8_t busType, uint8_t* ppins, uint16_t pstart, uint16_t len = 1, uint8_t pcolorOrder = COL_ORDER_GRB, bool rev = false, uint8_t skip = 0, byte aw=RGBW_MODE_MANUAL_ONLY, uint16_t clock_kHz=0U, uint8_t art_o=1, uint16_t art_l=1, uint8_t art_f=30) {
    refreshReq = (bool) GET_BIT(busType,7);
    type = busType & 0x7F;  // bit 7 may be/is hacked to include refresh info (1=refresh in off state, 0=no refresh)
    count = len; start = pstart; colorOrder = pcolorOrder; reversed = rev; skipAmount = skip; autoWhite = aw; frequency = clock_kHz;
    artnet_outputs = art_o; artnet_leds_per_output = art_l; artnet_fps_limit = art_f;
    uint8_t nPins = 1;                                                                 // default = only one pin (clockless LEDs like WS281x)
    if ((type >= TYPE_NET_DDP_RGB) && (type < (TYPE_NET_DDP_RGB + 16))) nPins = 4;     // virtual network bus. 4 "pins" store IP address
    else if ((type > 47) && (type < 63)) nPins = 2;                                    // (data + clock / SPI) busses - two pins
    else if (IS_PWM(type)) nPins = NUM_PWM_PINS(type);                                 // PWM needs 1..5 pins
    else if (type >= TYPE_HUB75MATRIX && type <= (TYPE_HUB75MATRIX + 10)) nPins = 1;   // HUB75 does not use LED pins, but we need to preserve the "chain length" parameter
    for (uint8_t i = 0; i < min(unsigned(nPins), sizeof(pins)/sizeof(pins[0])); i++) pins[i] = ppins[i];   //softhack007 fix for potential array out-of-bounds access
  }

  //validates start and length and extends total if needed // WLEDMM this function is not used anywhere
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
      _autoWhiteMode = Bus::hasWhite(type) ? aw : RGBW_MODE_MANUAL_ONLY;
    };

    virtual ~Bus() {} //throw the bus under the bus

    virtual void     show() = 0;
    virtual bool     canShow() { return true; }
    virtual void     setStatusPixel(uint32_t c) {}
    virtual void     setPixelColor(uint16_t pix, uint32_t c) = 0;
    virtual uint32_t getPixelColor(uint16_t pix) const { return 0; }
    virtual uint32_t getPixelColorRestored(uint16_t pix) const { return restore_Color_Lossy(getPixelColor(pix), _bri); } // override in case your bus has a lossless buffer (HUB75, FastLED, Art-Net)
    virtual void     setBrightness(uint8_t b, bool immediate=false) { _bri = b; }
    virtual void     cleanup() = 0;
    virtual uint8_t  getPins(uint8_t* pinArray) const { return 0; }
    virtual inline uint16_t getLength() const { return _len; }
    virtual void     setColorOrder() {}
    virtual uint8_t  getColorOrder() const { return COL_ORDER_RGB; }
    virtual uint8_t  skippedLeds() const { return 0; }
    virtual uint16_t getFrequency() const { return 0U; }
    virtual uint8_t  get_artnet_fps_limit() const { return 0; }
    virtual uint8_t  get_artnet_outputs() const { return 0; }
    virtual uint16_t get_artnet_leds_per_output() const { return 0; }
    inline  uint16_t getStart() const { return _start; }
    inline  void     setStart(uint16_t start) { _start = start; }
    inline  uint8_t  getType() const { return _type; }
    inline  bool     isOk() const { return _valid; }
    inline  bool     isOffRefreshRequired() const { return _needsRefresh; }
    //inline  bool     containsPixel(uint16_t pix) const { return pix >= _start && pix < _start+_len; } // WLEDMM not used, plus wrong - it does not consider skipped pixels
    virtual uint16_t getMaxPixels() const { return MAX_LEDS_PER_BUS; }

    virtual bool hasRGB() const {
      if ((_type >= TYPE_WS2812_1CH && _type <= TYPE_WS2812_WWA) || _type == TYPE_ANALOG_1CH || _type == TYPE_ANALOG_2CH || _type == TYPE_ONOFF) return false;
      return true;
    }
    virtual bool hasWhite() const { return Bus::hasWhite(_type); }
    static  bool hasWhite(uint8_t type) {
      if ((type >= TYPE_WS2812_1CH && type <= TYPE_WS2812_WWA) || type == TYPE_SK6812_RGBW || type == TYPE_TM1814 || type == TYPE_UCS8904) return true; // digital types with white channel
      if (type > TYPE_ONOFF && type <= TYPE_ANALOG_5CH && type != TYPE_ANALOG_3CH) return true; // analog types with white channel
      if (type == TYPE_NET_DDP_RGBW) return true; // network types with white channel
      return false;
    }
    virtual bool hasCCT() const {
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
    inline        uint8_t getAutoWhiteMode()          const { return _autoWhiteMode; }
    inline static void    setGlobalAWMode(uint8_t m)  { if (m < 5) _gAWM = m; else _gAWM = AW_GLOBAL_DISABLED; }
    inline static uint8_t getGlobalAWMode()           { return _gAWM; }

    inline static uint32_t restore_Color_Lossy(uint32_t c, uint8_t restoreBri) { // shamelessly grabbed from upstream, who grabbed from NPB, who ..
      if (restoreBri < 255) {
        uint8_t* chan = (uint8_t*) &c;
        for (uint_fast8_t i=0; i<4; i++) {
          uint_fast16_t val = chan[i];
          chan[i] = ((val << 8) + restoreBri) / (restoreBri + 1); //adding _bri slightly improves recovery / stops degradation on re-scale
        }
      }
      return c;
    }

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

    uint32_t autoWhiteCalc(uint32_t c) const;
};


class BusDigital : public Bus {
  public:
    BusDigital(BusConfig &bc, uint8_t nr, const ColorOrderMap &com);

    inline void show();

    bool canShow() override;

    void setBrightness(uint8_t b, bool immediate);

    void setStatusPixel(uint32_t c);

    void setPixelColor(uint16_t pix, uint32_t c);

    uint32_t getPixelColor(uint16_t pix) const override;

    uint8_t getColorOrder() const {
      return _colorOrder;
    }

    uint16_t getLength() const override {
      return _len - _skip;
    }

    uint8_t getPins(uint8_t* pinArray) const;

    void setColorOrder(uint8_t colorOrder);

    uint8_t skippedLeds() const override {
      return _skip;
    }

    uint16_t getFrequency() const override { return _frequencykHz; }

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
    uint32_t getPixelColor(uint16_t pix) const;

    void show();

    uint8_t getPins(uint8_t* pinArray) const;

    uint16_t getFrequency() const override { return _frequency; }

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

    uint32_t getPixelColor(uint16_t pix) const;
    uint32_t getPixelColorRestored(uint16_t pix) const override { return getPixelColor(pix);}  // WLEDMM BusOnOff ignores brightness

    void show();

    uint8_t getPins(uint8_t* pinArray)  const;

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
    BusNetwork(BusConfig &bc, const ColorOrderMap &com);

    uint16_t getMaxPixels() const override { return 4096; };
    bool hasRGB()  const { return true; }
    bool hasWhite()  const { return _rgbw; }

    void setPixelColor(uint16_t pix, uint32_t c);

    uint32_t __attribute__((pure)) getPixelColor(uint16_t pix) const;  // WLEDMM attribute added
    uint32_t __attribute__((pure)) getPixelColorRestored(uint16_t pix) const override { return getPixelColor(pix);}  // WLEDMM BusNetwork ignores brightness

    void show();

    bool canShow() override {
      // this should be a return value from UDP routine if it is still sending data out
      return !_broadcastLock;
    }

    uint8_t getPins(uint8_t* pinArray) const override;

    uint16_t getLength() const override {
      return _len;
    }

    uint8_t get_artnet_fps_limit() const override {
      return _artnet_fps_limit;
    }

    uint8_t get_artnet_outputs() const override {
      return _artnet_outputs;
    }

    uint16_t get_artnet_leds_per_output() const override {
      return _artnet_leds_per_output;
    }

    void setColorOrder(uint8_t colorOrder);

    uint8_t getColorOrder() const override {
      return _colorOrder;
    }

    void cleanup();

    ~BusNetwork() {
      cleanup();
    }

  private:
    IPAddress           _client;
    uint8_t             _UDPtype;
    uint8_t             _UDPchannels;
    bool                _rgbw;
    bool                _broadcastLock;
    byte                *_data;
    uint8_t             _colorOrder = COL_ORDER_RGB;
    uint8_t             _artnet_fps_limit;
    uint8_t             _artnet_outputs;
    uint16_t            _artnet_leds_per_output;
    const ColorOrderMap &_colorOrderMap;
};

#ifdef WLED_ENABLE_HUB75MATRIX
class BusHub75Matrix : public Bus {
  public:
    BusHub75Matrix(BusConfig &bc);

    uint16_t getMaxPixels() const override { return MAX_LEDS; };

    bool hasRGB() const override { return true; }
    bool hasWhite() const override { return false; }

    void setPixelColor(uint16_t pix, uint32_t c) override;
    uint32_t getPixelColor(uint16_t pix) const override;
    uint32_t getPixelColorRestored(uint16_t pix) const override; // lossless getPixelColor supported

    void show(void) override;

    void setBrightness(uint8_t b, bool immediate) override;

    uint8_t getPins(uint8_t* pinArray) const override {
      pinArray[0] = activeMXconfig.chain_length;
      return 1;
    } // Fake value due to keep finaliseInit happy

    void deallocatePins();

    void cleanup(void) override;

    ~BusHub75Matrix() {
      cleanup();
    }

  private:
    unsigned _panelWidth = 0;
    CRGB *_ledBuffer = nullptr;
    byte *_ledsDirty = nullptr;
    // C++ dirty trick: private static variables are actually _not_ part of the class (however only visibile to class instances). 
    // These variables persist when BusHub75Matrix gets deleted.
    static MatrixPanel_I2S_DMA *activeDisplay;         // active display object
    static VirtualMatrixPanel  *activeFourScanPanel;   // active fourScan object
    static HUB75_I2S_CFG activeMXconfig;               // last used mxconfig
    static uint8_t activeType;                         // last used type
    static uint8_t instanceCount;                      // active instances - 0 or 1
    static uint8_t last_bri;                           // last used brightness value (persists on driver delete)
};
#endif

class BusManager {
  public:
    BusManager() {};

    //utility to get the approx. memory usage of a given BusConfig
    static uint32_t memUsage(BusConfig &bc)  __attribute__((pure));

    int add(BusConfig &bc);

    //do not call this method from system context (network callback)
    void removeAll();

    void show();

    void invalidateCache(bool isRTMode) {
      // WLEDMM clear cached Bus info
      lastBus = nullptr;
      laststart = 0;
      lastend = 0;
      slowMode = isRTMode;
    }

    void setStatusPixel(uint32_t c);

    void setPixelColor(uint16_t pix, uint32_t c, int16_t cct=-1);

    void setBrightness(uint8_t b, bool immediate=false);          // immediate=true is for use in ABL, it applies brightness immediately (warning: inefficient)

    void setSegmentCCT(int16_t cct, bool allowWBCorrection = false);

    uint32_t __attribute__((pure)) getPixelColor(uint_fast16_t pix); // WLEDMM attribute added
    uint32_t __attribute__((pure)) getPixelColorRestored(uint_fast16_t pix);  // WLEDMM

    bool canAllShow() const;

    Bus* getBus(uint8_t busNr) const;

    //semi-duplicate of strip.getLengthTotal() (though that just returns strip._length, calculated in finalizeInit())
    uint16_t getTotalLength() const;

    inline void updateColorOrderMap(const ColorOrderMap &com) {
      memcpy(&colorOrderMap, &com, sizeof(ColorOrderMap));
    }

    inline const ColorOrderMap& getColorOrderMap() const {
      return colorOrderMap;
    }

    inline uint8_t getNumBusses() const {
      return numBusses;
    }

  private:
    uint8_t numBusses = 0;
    Bus* busses[WLED_MAX_BUSSES+WLED_MIN_VIRTUAL_BUSSES] = {nullptr}; // WLEDMM init array
    ColorOrderMap colorOrderMap;
    // WLEDMM cache last used Bus -> 20% to 30% speedup when using many LED pins
    Bus *lastBus = nullptr;
    unsigned laststart = 0;
    unsigned lastend = 0;
    bool slowMode = false; // WLEDMM not sure why we need this. But its necessary.

    inline uint8_t getNumVirtualBusses() const {
      int j = 0;
      for (int i=0; i<numBusses; i++) if (busses[i]->getType() >= TYPE_NET_DDP_RGB && busses[i]->getType() < 96) j++;
      return j;
    }
};
#endif
