#pragma once
#ifndef BusManager_h
#define BusManager_h

/*
 * Class for addressing various light types
 */

#include "const.h"
#include <vector>
#include <memory>

// enable additional debug output
#if defined(WLED_DEBUG_HOST)
  #include "net_debug.h"
  #define DEBUGOUT NetDebug
#else
  #define DEBUGOUT Serial
#endif

#ifdef WLED_DEBUG_BUS
  #ifndef ESP8266
  #include <rom/rtc.h>
  #endif
  #define DEBUGBUS_PRINT(x) DEBUGOUT.print(x)
  #define DEBUGBUS_PRINTLN(x) DEBUGOUT.println(x)
  #define DEBUGBUS_PRINTF(x...) DEBUGOUT.printf(x)
  #define DEBUGBUS_PRINTF_P(x...) DEBUGOUT.printf_P(x)
#else
  #define DEBUGBUS_PRINT(x)
  #define DEBUGBUS_PRINTLN(x)
  #define DEBUGBUS_PRINTF(x...)
  #define DEBUGBUS_PRINTF_P(x...)
#endif

//colors.cpp
uint16_t approximateKelvinFromRGB(uint32_t rgb);

#define GET_BIT(var,bit)    (((var)>>(bit))&0x01)
#define SET_BIT(var,bit)    ((var)|=(uint16_t)(0x0001<<(bit)))
#define UNSET_BIT(var,bit)  ((var)&=(~(uint16_t)(0x0001<<(bit))))

#define NUM_ICS_WS2812_1CH_3X(len) (((len)+2)/3)   // 1 WS2811 IC controls 3 zones (each zone has 1 LED, W)
#define IC_INDEX_WS2812_1CH_3X(i)  ((i)/3)

#define NUM_ICS_WS2812_2CH_3X(len) (((len)+1)*2/3) // 2 WS2811 ICs control 3 zones (each zone has 2 LEDs, CW and WW)
#define IC_INDEX_WS2812_2CH_3X(i)  ((i)*2/3)
#define WS2812_2CH_3X_SPANS_2_ICS(i) ((i)&0x01)    // every other LED zone is on two different ICs

struct BusConfig; // forward declaration

// Defines an LED Strip and its color ordering.
typedef struct {
  uint16_t start;
  uint16_t len;
  uint8_t colorOrder;
} ColorOrderMapEntry;

struct ColorOrderMap {
    bool add(uint16_t start, uint16_t len, uint8_t colorOrder);

    inline uint8_t count() const { return _mappings.size(); }
    inline void reserve(size_t num) { _mappings.reserve(num); }

    void reset() {
      _mappings.clear();
      _mappings.shrink_to_fit();
    }

    const ColorOrderMapEntry* get(uint8_t n) const {
      if (n >= count()) return nullptr;
      return &(_mappings[n]);
    }

    [[gnu::hot]] uint8_t getPixelColorOrder(uint16_t pix, uint8_t defaultColorOrder) const;

  private:
    std::vector<ColorOrderMapEntry> _mappings;
};


typedef struct {
  uint8_t id;
  const char *type;
  const char *name;
} LEDType;


/**
     * Construct a Bus base instance for a specific LED bus configuration.
     *
     * Initializes core bus metadata (type, start index, length, direction, refresh flag)
     * and prepares default runtime state (brightness 255, invalid/uninitialized, no data buffer).
     *
     * @param type LED type identifier (uses Bus::hasWhite(type) to determine white-capability).
     * @param start First pixel index handled by this bus.
     * @param aw Auto-white mode; applied only if the LED type supports white channels.
     * @param len Number of pixels managed by this bus (default 1).
     * @param reversed True if pixel indexing is reversed for this bus.
     * @param refresh True if the bus requires explicit refresh semantics.
     */
class Bus {
  public:
    Bus(uint8_t type, uint16_t start, uint8_t aw, uint16_t len = 1, bool reversed = false, bool refresh = false)
    : _type(type)
    , _bri(255)
    , _start(start)
    , _len(len)
    , _reversed(reversed)
    , _valid(false)
    , _needsRefresh(refresh)
    , _data(nullptr) // keep data access consistent across all types of buses
    {
      _autoWhiteMode = Bus::hasWhite(type) ? aw : RGBW_MODE_MANUAL_ONLY;
    };

    /**
 * Virtual destructor for Bus.
 *
 * No-op in the base class; ensures derived-class destructors are called so
 * resources owned by concrete Bus implementations can be released. */
virtual ~Bus() {} /**
 * Initialize or (re)start the bus hardware.
 *
 * Default implementation is a no-op. Derived classes should override to allocate
 * or initialize hardware resources and may rely on freeData() (or their own
 * cleanup) to release any buffer/resource stored in the base class _data.
 */

    virtual void     begin()                                    {};
    virtual void     show() = 0;
    virtual bool     canShow() const                            { return true; }
    /**
 * Set the bus' status/diagnostic pixel color.
 *
 * Derived bus implementations may override to update a reserved status or
 * heartbeat pixel (for example an LED used to indicate bus activity or errors).
 * The default implementation is a no-op.
 *
 * @param c Packed 32-bit color value (format consistent with project-wide color representation).
 */
virtual void     setStatusPixel(uint32_t c)                 {}
    virtual void     setPixelColor(unsigned pix, uint32_t c) = 0;
    /**
 * Set the per-bus brightness level.
 * 
 * Updates this Bus instance's stored brightness value; the new level (0–255)
 * will be used the next time the bus is rendered (e.g., on show()).
 *
 * @param b Brightness in the range 0 (off) to 255 (full brightness).
 */
virtual void     setBrightness(uint8_t b)                   { _bri = b; };
    /**
 * Set the color channel ordering used by this bus.
 *
 * Derived bus implementations may override this to change how RGB(A) components
 * are mapped when writing pixels (e.g., COL_ORDER_RGB, COL_ORDER_GRB, etc.).
 * The base implementation is a no-op and does not retain or apply the value.
 *
 * @param co Color order code (one of the COL_ORDER_* constants) to apply.
 */
virtual void     setColorOrder(uint8_t co)                  {}
    /**
 * Get the color of a pixel on this bus.
 *
 * Returns the packed 32-bit color value (0xWWRRGGBB or 0x00RRGGBB depending on bus) for the pixel at index `pix`.
 * The base implementation returns 0; derived bus implementations should override to provide actual pixel values.
 *
 * @param pix Pixel index (0-based) within this bus.
 * @return Packed 32-bit color for the requested pixel, or 0 if the bus does not provide per-pixel state or `pix` is out of range.
 */
virtual uint32_t getPixelColor(unsigned pix) const          { return 0; }
    virtual unsigned getPins(uint8_t* pinArray = nullptr) const { return 0; }
    virtual uint16_t getLength() const                          { return isOk() ? _len : 0; }
    virtual uint8_t  getColorOrder() const                      { return COL_ORDER_RGB; }
    virtual unsigned skippedLeds() const                        { return 0; }
    virtual uint16_t getFrequency() const                       { return 0U; }
    virtual uint16_t getLEDCurrent() const                      { return 0; }
    virtual uint16_t getUsedCurrent() const                     { return 0; }
    virtual uint16_t getMaxCurrent() const                      { return 0; }
    virtual unsigned getBusSize() const                         { return sizeof(Bus); }

    inline  bool     hasRGB() const                             { return _hasRgb; }
    inline  bool     hasWhite() const                           { return _hasWhite; }
    inline  bool     hasCCT() const                             { return _hasCCT; }
    /**
     * Return true when this Bus's type represents a digital LED interface (e.g. WS281x-style).
     *
     * This is a convenience wrapper that queries the static Bus::isDigital(type) predicate
     * for this instance's configured _type.
     */
    inline  bool     isDigital() const                          { return isDigital(_type); }
    inline  bool     is2Pin() const                             { return is2Pin(_type); }
    /**
     * Return true when this Bus instance represents a simple on/off (GPIO) output type.
     *
     * Evaluates the bus's configured type and returns whether it is an on/off-style bus.
     * @return true if the bus type is an on/off output, false otherwise.
     */
    
    /**
     * Return true when this Bus instance represents a PWM-driven output type.
     *
     * Evaluates the bus's configured type and returns whether it is PWM-capable.
     * @return true if the bus type is PWM, false otherwise.
     */
    inline  bool     isOnOff() const                            { return isOnOff(_type); }
    inline  bool     isPWM() const                              { return isPWM(_type); }
    /**
     * Returns true if this bus's LED type is a virtual (non-hardware) device.
     * @return true when the bus's configured LED type is virtual; false otherwise.
     */
    /**
     * Returns true if this bus's LED type uses 16-bit color channels.
     * @return true when the bus's configured LED type is 16-bit; false otherwise.
     */
    inline  bool     isVirtual() const                          { return isVirtual(_type); }
    inline  bool     is16bit() const                            { return is16bit(_type); }
    /**
     * Return whether this bus type requires periodic refreshes to maintain output.
     *
     * Uses the bus's configured type to determine if the underlying protocol/hardware
     * requires repeated update calls (e.g., non-persistent protocols).
     *
     * @return true if the bus type must be refreshed periodically, false otherwise.
     */
    /**
     * Set whether the pixel ordering for this bus is reversed.
     *
     * When true, pixel indices and rendering will be interpreted in reverse order
     * relative to the configured start index.
     *
     * @param reversed true to reverse pixel ordering, false for normal ordering.
     */
    inline  bool     mustRefresh() const                        { return mustRefresh(_type); }
    inline  void     setReversed(bool reversed)                 { _reversed = reversed; }
    /**
 * Set the zero-based starting pixel index for this bus.
 * @param start Index of the first LED managed by the bus (zero-based).
 */
inline  void     setStart(uint16_t start)                   { _start = start; }
    /**
 * Set the bus's Auto White Mode (AWM).
 *
 * Valid values are 0–4; values >= 5 are ignored. Updates the instance's
 * auto-white mode used when automatic white channel calculation is applied.
 * @param m Auto White Mode (0–4)
 */
inline  void     setAutoWhiteMode(uint8_t m)                { if (m < 5) _autoWhiteMode = m; }
    inline  uint8_t  getAutoWhiteMode() const                   { return _autoWhiteMode; }
    inline  unsigned getNumberOfChannels() const                { return hasWhite() + 3*hasRGB() + hasCCT(); }
    inline  uint16_t getStart() const                           { return _start; }
    inline  uint8_t  getType() const                            { return _type; }
    inline  bool     isOk() const                               { return _valid; }
    inline  bool     isReversed() const                         { return _reversed; }
    inline  bool     isOffRefreshRequired() const               { return _needsRefresh; }
    /**
 * Check whether a global pixel index is within this bus's configured range.
 *
 * Returns true when the given global pixel index is >= this bus's start and < start + length.
 *
 * @param pix Global pixel index to test.
 * @return true if the pixel is handled by this bus; false otherwise.
 */
inline  bool     containsPixel(uint16_t pix) const          { return pix >= _start && pix < _start + _len; }

    /**
 * Get the list of available LED types.
 *
 * Returns a vector containing a single placeholder entry used when no concrete
 * LED types are registered.
 * @return std::vector<LEDType> Vector with one element: {TYPE_NONE, "", PSTR("None")}.
 */
static inline std::vector<LEDType> getLEDTypes()            { return {{TYPE_NONE, "", PSTR("None")}}; } /**
 * Return the required number of physical pins for the given LED bus type.
 *
 * For virtual bus types this reports 4 pins, for PWM-based types it delegates to
 * numPWMPins(type), for two-pin types it returns 2, otherwise 1.
 *
 * @param type LED/bus type identifier (as used by the Bus type constants).
 * @return Number of hardware pins the given type requires.
 */
    static constexpr unsigned getNumberOfPins(uint8_t type)     { return isVirtual(type) ? 4 : isPWM(type) ? numPWMPins(type) : is2Pin(type) + 1; } /**
 * Return the number of color channels for a given LED type.
 *
 * Computes the channel count at compile time: RGB contributes 3 channels,
 * a dedicated white channel adds 1, and a CCT (cold/warm white) channel adds 1.
 *
 * @param type LED type identifier.
 * @return Total number of channels for the specified LED type.
 */
    static constexpr unsigned getNumberOfChannels(uint8_t type) { return hasWhite(type) + 3*hasRGB(type) + hasCCT(type); }
    /****
     * Return whether the specified LED type includes RGB color channels.
     * @param type LED type identifier (one of the TYPE_* constants).
     * @return true if the type supports RGB output; false for single-channel, two-channel analog, or on/off types.
     ****/
    static constexpr bool hasRGB(uint8_t type) {
      return !((type >= TYPE_WS2812_1CH && type <= TYPE_WS2812_WWA) || type == TYPE_ANALOG_1CH || type == TYPE_ANALOG_2CH || type == TYPE_ONOFF);
    }
    static constexpr bool hasWhite(uint8_t type) {
      return  (type >= TYPE_WS2812_1CH && type <= TYPE_WS2812_WWA) ||
              type == TYPE_SK6812_RGBW || type == TYPE_TM1814 || type == TYPE_UCS8904 ||
              type == TYPE_FW1906 || type == TYPE_WS2805 || type == TYPE_SM16825 ||        // digital types with white channel
              (type > TYPE_ONOFF && type <= TYPE_ANALOG_5CH && type != TYPE_ANALOG_3CH) || // analog types with white channel
              type == TYPE_NET_DDP_RGBW || type == TYPE_NET_ARTNET_RGBW;                   // network types with white channel
    }
    static constexpr bool hasCCT(uint8_t type) {
      return  type == TYPE_WS2812_2CH_X3 || type == TYPE_WS2812_WWA ||
              type == TYPE_ANALOG_2CH    || type == TYPE_ANALOG_5CH ||
              type == TYPE_FW1906        || type == TYPE_WS2805     ||
              type == TYPE_SM16825;
    }
    static constexpr bool  isTypeValid(uint8_t type)  { return (type > 15 && type < 128); }
    static constexpr bool  isDigital(uint8_t type)    { return (type >= TYPE_DIGITAL_MIN && type <= TYPE_DIGITAL_MAX) || is2Pin(type); }
    static constexpr bool  is2Pin(uint8_t type)       { return (type >= TYPE_2PIN_MIN && type <= TYPE_2PIN_MAX); }
    static constexpr bool  isOnOff(uint8_t type)      { return (type == TYPE_ONOFF); }
    static constexpr bool  isPWM(uint8_t type)        { return (type >= TYPE_ANALOG_MIN && type <= TYPE_ANALOG_MAX); }
    static constexpr bool  isVirtual(uint8_t type)    { return (type >= TYPE_VIRTUAL_MIN && type <= TYPE_VIRTUAL_MAX); }
    static constexpr bool  is16bit(uint8_t type)      { return type == TYPE_UCS8903 || type == TYPE_UCS8904 || type == TYPE_SM16825; }
    static constexpr bool  mustRefresh(uint8_t type)  { return type == TYPE_TM1814; }
    static constexpr int   numPWMPins(uint8_t type)   { return (type - 40); }

    static inline int16_t  getCCT()                   { return _cct; }
    static inline void     setGlobalAWMode(uint8_t m) { if (m < 5) _gAWM = m; else _gAWM = AW_GLOBAL_DISABLED; }
    /**
 * Return the current global Auto-White (AW) mode.
 *
 * @return The global AW mode as an unsigned 8-bit value.
 */
static inline uint8_t  getGlobalAWMode()          { return _gAWM; }
    /**
 * Set the global correlated color temperature (CCT) value used by buses.
 *
 * @param cct CCT value to store (int16_t). The units/interpretation follow the surrounding codebase's conventions (e.g., Kelvin). 
 */
static inline void     setCCT(int16_t cct)        { _cct = cct; }
    /**
 * Get the current global CCT blending factor.
 * @return CCT blend value (0-255) controlling how strongly global color-temperature
 *         adjustments are mixed into output colors — higher values increase CCT influence.
 */
static inline uint8_t  getCCTBlend()              { return _cctBlend; }
    /**
     * Set the global CCT blend level used when mixing warm white and cold white channels.
     *
     * The input `b` is treated as a percentage (0–100). It is clamped to 100, scaled to
     * the internal 0–127 representation, and stored in the static `_cctBlend`. If the
     * compile-time macro `WLED_MAX_CCT_BLEND` is defined, the stored value is further
     * capped to that limit to accommodate hardware that cannot drive both white channels
     * at full power.
     *
     * @param b Blend percentage (0–100).
     */
    static inline void     setCCTBlend(uint8_t b) {
      _cctBlend = (std::min((int)b,100) * 127) / 100;
      //compile-time limiter for hardware that can't power both white channels at max
      #ifdef WLED_MAX_CCT_BLEND
        if (_cctBlend > WLED_MAX_CCT_BLEND) _cctBlend = WLED_MAX_CCT_BLEND;
      #endif
    }
    static void calculateCCT(uint32_t c, uint8_t &ww, uint8_t &cw);

  protected:
    uint8_t  _type;
    uint8_t  _bri;
    uint16_t _start;
    uint16_t _len;
    //struct { //using bitfield struct adds abour 250 bytes to binary size
      bool _reversed;//     : 1;
      bool _valid;//        : 1;
      bool _needsRefresh;// : 1;
      bool _hasRgb;//       : 1;
      bool _hasWhite;//     : 1;
      bool _hasCCT;//       : 1;
    //} __attribute__ ((packed));
    uint8_t  _autoWhiteMode;
    uint8_t  *_data;
    // global Auto White Calculation override
    static uint8_t _gAWM;
    // _cct has the following menaings (see calculateCCT() & BusManager::setSegmentCCT()):
    //    -1 means to extract approximate CCT value in K from RGB (in calcualteCCT())
    //    [0,255] is the exact CCT value where 0 means warm and 255 cold
    //    [1900,10060] only for color correction expressed in K (colorBalanceFromKelvin())
    static int16_t _cct;
    // _cctBlend determines WW/CW blending:
    //    0 - linear (CCT 127 => 50% warm, 50% cold)
    //   63 - semi additive/nonlinear (CCT 127 => 66% warm, 66% cold)
    //  127 - additive CCT blending (CCT 127 => 100% warm, 100% cold)
    static uint8_t _cctBlend;

    uint32_t autoWhiteCalc(uint32_t c) const;
    uint8_t *allocateData(size_t size = 1);
    void     freeData();
};


/**
 * Construct a digital LED bus from a BusConfig.
 * @param bc Configuration describing type, pins, start, length, color order, and other bus properties.
 * @param nr Index of this bus (used for allocation/identification).
 * @param com Reference to the global ColorOrderMap for per-pixel color-order lookups.
 */

/**
 * Render the bus's pixel buffer to the physical LEDs.
 * Blocks or schedules the transfer according to the underlying driver and bus type.
 */

/**
 * Return whether this bus is currently ready to perform a show().
 * Typically false while a previous transfer is still in progress.
 */

/**
 * Set the bus-level brightness (0–255). This updates internal buffers so subsequent show() calls emit the adjusted brightness.
 * @param b Brightness value 0 (off) to 255 (full).
 */

/**
 * Set a single status pixel color on this bus (used for global status indicators).
 * @param c 32-bit color (packed RGBA/ARGB as used by the project).
 */

/**
 * Set the color of a pixel within this bus's logical range.
 * This is a hot-path method and should be fast.
 * @param pix Pixel index relative to this bus (0..length-1).
 * @param c Packed 32-bit color value.
 */

/**
 * Update the bus-wide color order used for pixels that don't have a per-pixel override.
 * @param colorOrder One of the defined color-order constants.
 */

/**
 * Read the color of a pixel from the bus's buffer.
 * Does not perform bounds checking; caller must ensure pix is within the bus length.
 * @param pix Pixel index relative to this bus (0..length-1).
 * @return Packed 32-bit color value currently stored for the pixel.
 */

/**
 * Return the GPIO pins used by this digital bus.
 * If pinArray is provided it will be filled with up to two pin numbers.
 * @param pinArray Optional pointer to a 2-byte array to receive pin numbers.
 * @return Number of pins used (0, 1, or 2).
 */

/**
 * Return the runtime size in bytes used by this BusDigital instance, including any per-bus buffer storage.
 * Used for memory accounting.
 * @return Size in bytes of the bus object and its allocated buffers (0 if bus is not initialized).
 */

/**
 * Initialize hardware and allocate any runtime resources required by this bus.
 * Safe to call multiple times; must be called before show() for active buses.
 */

/**
 * Release hardware resources and deallocate any runtime buffers owned by this bus.
 * After cleanup the bus must be reinitialized with begin() before use.
 */

/**
 * Recover color channels after lossy brightness scaling.
 * Restores channel values proportionally to mitigate rounding losses when brightness was reduced.
 * @param c Packed 32-bit color value to restore.
 * @param restoreBri Brightness level that was applied (0..255); if 255 no change is performed.
 * @return Restored packed 32-bit color.
 */

/**
 * Estimate the total current draw of this digital bus and determine if the brightness must be limited.
 * Performs per-LED current estimation and returns the brightness cap index to apply (0..255).
 * @return Brightness cap (0..255) that should be applied to avoid exceeding current limits.
 */

/**
 * Construct a PWM-driven bus from a BusConfig.
 * @param bc Configuration describing pins, start, length, frequency, and other bus properties.
 */

/**
 * Set the color of a pixel on a PWM-driven bus.
 * @param pix Pixel index relative to this bus (0..length-1).
 * @param c Packed 32-bit color value.
 */

/**
 * Read the color for a pixel from the PWM bus buffer.
 * Does not perform bounds checking.
 * @param pix Pixel index relative to this bus (0..length-1).
 * @return Packed 32-bit color value stored for the pixel.
 */

/**
 * Return the GPIO pins used by this PWM bus.
 * If pinArray is provided it will be filled with up to OUTPUT_MAX_PINS entries.
 * @param pinArray Optional array to receive pin numbers.
 * @return Number of pins used.
 */

/**
 * Return the runtime size in bytes used by this BusPwm instance.
 * This includes the object size and any additional per-instance allocation.
 * @return Size in bytes (equal to sizeof(BusPwm) for this implementation).
 */

/**
 * Render the PWM outputs to their GPIOs / timers.
 * Applies the internal PWM buffer to the hardware.
 */

/**
 * Free allocated PWM resources and mark the internal data pointer as released.
 * After cleanup the bus must be reinitialized with begin() before use.
 */
class BusDigital : public Bus {
  public:
    BusDigital(const BusConfig &bc, uint8_t nr, const ColorOrderMap &com);
    ~BusDigital() { cleanup(); }

    void show() override;
    bool canShow() const override;
    void setBrightness(uint8_t b) override;
    void setStatusPixel(uint32_t c) override;
    [[gnu::hot]] void setPixelColor(unsigned pix, uint32_t c) override;
    void setColorOrder(uint8_t colorOrder) override;
    [[gnu::hot]] uint32_t getPixelColor(unsigned pix) const override;
    uint8_t  getColorOrder() const override  { return _colorOrder; }
    unsigned getPins(uint8_t* pinArray = nullptr) const override;
    unsigned skippedLeds() const override    { return _skip; }
    uint16_t getFrequency() const override   { return _frequencykHz; }
    uint16_t getLEDCurrent() const override  { return _milliAmpsPerLed; }
    uint16_t getUsedCurrent() const override { return _milliAmpsTotal; }
    uint16_t getMaxCurrent() const override  { return _milliAmpsMax; }
    unsigned getBusSize() const override;
    void begin() override;
    void cleanup();

    static std::vector<LEDType> getLEDTypes();

  private:
    uint8_t _skip;
    uint8_t _colorOrder;
    uint8_t _pins[2];
    uint8_t _iType;
    uint16_t _frequencykHz;
    uint8_t _milliAmpsPerLed;
    uint16_t _milliAmpsMax;
    void * _busPtr;
    const ColorOrderMap &_colorOrderMap;

    static uint16_t _milliAmpsTotal; // is overwitten/recalculated on each show()

    inline uint32_t restoreColorLossy(uint32_t c, uint8_t restoreBri) const {
      if (restoreBri < 255) {
        uint8_t* chan = (uint8_t*) &c;
        for (uint_fast8_t i=0; i<4; i++) {
          uint_fast16_t val = chan[i];
          chan[i] = ((val << 8) + restoreBri) / (restoreBri + 1); //adding _bri slightly improves recovery / stops degradation on re-scale
        }
      }
      return c;
    }

    uint8_t  estimateCurrentAndLimitBri() const;
};


class BusPwm : public Bus {
  public:
    BusPwm(const BusConfig &bc);
    ~BusPwm() { cleanup(); }

    void setPixelColor(unsigned pix, uint32_t c) override;
    uint32_t getPixelColor(unsigned pix) const override; //does no index check
    unsigned getPins(uint8_t* pinArray = nullptr) const override;
    uint16_t getFrequency() const override { return _frequency; }
    unsigned getBusSize() const override   { return sizeof(BusPwm); }
    void show() override;
    inline void cleanup() { deallocatePins(); _data = nullptr; }

    static std::vector<LEDType> getLEDTypes();

  private:
    uint8_t _pins[OUTPUT_MAX_PINS];
    uint8_t _pwmdata[OUTPUT_MAX_PINS];
    #ifdef ARDUINO_ARCH_ESP32
    uint8_t _ledcStart;
    #endif
    uint8_t _depth;
    uint16_t _frequency;

    void deallocatePins();
};


/**
 * Simple on/off GPIO bus implementation.
 *
 * Controls a single digital output pin as a binary (on/off) LED bus and implements the Bus interface
 * for systems that only require toggling a GPIO rather than driving multi-channel LEDs.
 *
 * Behavior:
 * - setPixelColor(unsigned pix, uint32_t c): interprets any nonzero color as "on" and zero as "off" for
 *   the configured pin; only the bus-level on/off state is stored (per-bus, not per-pixel).
 * - getPixelColor(unsigned pix) const: returns the current on/off color state for the bus.
 * - getPins(uint8_t* pinArray) const: writes the configured pin into pinArray (if provided) and returns 1
 *   when a pin is configured (0 otherwise).
 * - show(): applies the stored on/off state to the GPIO pin.
 * - cleanup(): deallocates the configured pin and clears internal buffer state.
 *
 * getBusSize() returns the memory footprint for this bus implementation (sizeof(BusOnOff)).
 */
class BusOnOff : public Bus {
  public:
    BusOnOff(const BusConfig &bc);
    ~BusOnOff() { cleanup(); }

    void setPixelColor(unsigned pix, uint32_t c) override;
    uint32_t getPixelColor(unsigned pix) const override;
    unsigned getPins(uint8_t* pinArray) const override;
    unsigned getBusSize() const override { return sizeof(BusOnOff); }
    void show() override;
    inline void cleanup() { PinManager::deallocatePin(_pin, PinOwner::BusOnOff); _data = nullptr; }

    static std::vector<LEDType> getLEDTypes();

  private:
    uint8_t _pin;
    uint8_t _onoffdata;
};


/**
   * Networked (UDP) LED bus.
   *
   * Implements a Bus that sends pixel data over the network (UDP). Pixel indices passed to this
   * bus are relative to the bus start configured in BusConfig. Uses an internal broadcast lock
   * to prevent concurrent transmissions; callers should check canShow() before calling show().
   */
  
  /**
   * Construct a BusNetwork from a BusConfig.
   *
   * @param bc Configuration describing type, start, length, pins, color order, refresh behavior,
   *           frequency and current limits for this networked bus.
   */
  
  /**
   * Release resources held by the network bus and stop any pending broadcasts.
   */
  
  /**
   * Whether the bus is ready to start a new transmission.
   *
   * Returns true while no UDP broadcast is in progress; false while a previous broadcast is still
   * being sent.
   */
  
  /**
   * Set color for a pixel on this bus.
   *
   * @param pix Pixel index relative to this bus's start (0..len-1).
   * @param c   0xRRGGBB[WW] color value to store for the given pixel. The color is written into
   *            the bus's output buffer; it is not necessarily transmitted immediately (show()
   *            triggers sending).
   */
  
  /**
   * Get the color currently stored for a pixel on this bus.
   *
   * @param pix Pixel index relative to this bus's start (0..len-1).
   * @return    0xRRGGBB[WW] color value currently buffered for that pixel (0 if out of range).
   */
  
  /**
   * Copy this bus's pin list into the provided array (if non-null) and return the number of pins.
   *
   * @param pinArray Optional destination array to receive pin numbers; only the first N entries
   *                 (where N is the returned value) are written.
   * @return         Number of hardware pins used by this bus.
   */
  
  /**
   * Report in-memory size of this BusNetwork instance.
   *
   * Includes the base object size plus any per-pixel UDP channel buffers when the bus is valid.
   *
   * @return Number of bytes used by this BusNetwork instance.
   */
  
  /**
   * Transmit the currently buffered pixel data over the network.
   *
   * This triggers sending the buffered UDP packets for this bus's client/endpoint. The method
   * may set a broadcast lock while transmission is active; callers can use canShow() to detect
   * whether a new transmission may be started.
   */
  
  /**
   * Release network resources and allocated buffers used by this bus.
   */
  
  /**
   * Return supported LED types for networked output.
   *
   * @return Vector of LEDType entries describing LED formats this bus can drive.
   */
  
  /**
   * Construct a BusConfig.
   *
   * The high bit (bit 7) of `busType` is treated as a refresh flag (refreshReq). The effective
   * bus `type` is `busType & 0x7F`. The constructor copies as many pins from `ppins` as required
   * by the resolved bus type (Bus::getNumberOfPins(type)).
   *
   * @param busType      Encoded bus type; bit 7 sets the refresh requirement, lower 7 bits are the type.
   * @param ppins        Pointer to an array of pin numbers (at least Bus::getNumberOfPins(type) entries).
   * @param pstart       Start index (first pixel) for the bus.
   * @param len          Number of pixels on the bus (default 1).
   * @param pcolorOrder  Default color order for pixels on this bus (default COL_ORDER_GRB).
   * @param rev          Whether the pixel order for the bus is reversed (default false).
   * @param skip         Number of leading pixels to skip on this bus (default 0).
   * @param aw           Auto-white mode for this bus (default RGBW_MODE_MANUAL_ONLY).
   * @param clock_kHz    PWM/clock frequency in kHz for buses that use it (default 0).
   * @param dblBfr       Whether to use double buffering for this bus (default false).
   * @param maPerLed     Milliamps budget per LED for this bus (default LED_MILLIAMPS_DEFAULT).
   * @param maMax        Maximum milliamps allowed for this bus (default ABL_MILLIAMPS_DEFAULT).
   */
  class BusNetwork : public Bus {
  public:
    BusNetwork(const BusConfig &bc);
    ~BusNetwork() { cleanup(); }

    bool canShow() const override  { return !_broadcastLock; } // this should be a return value from UDP routine if it is still sending data out
    [[gnu::hot]] void setPixelColor(unsigned pix, uint32_t c) override;
    [[gnu::hot]] uint32_t getPixelColor(unsigned pix) const override;
    unsigned getPins(uint8_t* pinArray = nullptr) const override;
    unsigned getBusSize() const override  { return sizeof(BusNetwork) + (isOk() ? _len * _UDPchannels : 0); }
    void show() override;
    void cleanup();

    static std::vector<LEDType> getLEDTypes();

  private:
    IPAddress _client;
    uint8_t   _UDPtype;
    uint8_t   _UDPchannels;
    bool      _broadcastLock;
};


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
  uint8_t pins[5] = {255, 255, 255, 255, 255};
  uint16_t frequency;
  bool doubleBuffer;
  uint8_t milliAmpsPerLed;
  uint16_t milliAmpsMax;

  BusConfig(uint8_t busType, uint8_t* ppins, uint16_t pstart, uint16_t len = 1, uint8_t pcolorOrder = COL_ORDER_GRB, bool rev = false, uint8_t skip = 0, byte aw=RGBW_MODE_MANUAL_ONLY, uint16_t clock_kHz=0U, bool dblBfr=false, uint8_t maPerLed=LED_MILLIAMPS_DEFAULT, uint16_t maMax=ABL_MILLIAMPS_DEFAULT)
  : count(len)
  , start(pstart)
  , colorOrder(pcolorOrder)
  , reversed(rev)
  , skipAmount(skip)
  , autoWhite(aw)
  , frequency(clock_kHz)
  , doubleBuffer(dblBfr)
  , milliAmpsPerLed(maPerLed)
  , milliAmpsMax(maMax)
  {
    refreshReq = (bool) GET_BIT(busType,7);
    type = busType & 0x7F;  // bit 7 may be/is hacked to include refresh info (1=refresh in off state, 0=no refresh)
    size_t nPins = Bus::getNumberOfPins(type);
    for (size_t i = 0; i < nPins; i++) pins[i] = ppins[i];
    DEBUGBUS_PRINTF_P(PSTR("Bus: Config (%d-%d, type:%d, CO:%d, rev:%d, skip:%d, AW:%d kHz:%d, mA:%d/%d)\n"),
      (int)start, (int)(start+len),
      (int)type,
      (int)colorOrder,
      (int)reversed,
      (int)skipAmount,
      (int)autoWhite,
      (int)frequency,
      (int)milliAmpsPerLed, (int)milliAmpsMax
    );
  }

  /**
   * Validate and clamp this BusConfig's start and count, and update the overall total length.
   *
   * Ensures `count` is at least 1 and no greater than MAX_LEDS_PER_BUS, and that the configured
   * range [start, start+count) lies within 0..MAX_LEDS. If the range would exceed MAX_LEDS it is
   * shortened. If the bus' end exceeds the provided `total`, `total` is extended to cover it.
   *
   * @param total Reference to the running total number of LEDs; may be increased to accommodate this bus.
   * @return true if the start is within bounds and adjustments were applied; false if `start >= MAX_LEDS`.
   */
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

  unsigned memUsage(unsigned nr = 0) const;
};


//fine tune power estimation constants for your setup
//you can set it to 0 if the ESP is powered by USB and the LEDs by external
#ifndef MA_FOR_ESP
  #ifdef ESP8266
    #define MA_FOR_ESP         80 //how much mA does the ESP use (Wemos D1 about 80mA)
  #else
    #define MA_FOR_ESP        120 //how much mA does the ESP use (ESP32 about 120mA)
  #endif
#endif

class BusManager {
  public:
    BusManager() {};

    static unsigned memUsage();
    /**
 * Get the current estimated power draw in milliamps including MCU overhead.
 *
 * Returns the sum of the currently tracked LED subsystem consumption and a
 * platform-specific estimate for the microcontroller (MA_FOR_ESP).
 *
 * @return Estimated current draw in milliamps (uint16_t).
 */
static uint16_t currentMilliamps() { return _milliAmpsUsed + MA_FOR_ESP; }
    /**
 * Get the configured maximum milliamps budget used by the adaptive brightness limiter (ABL).
 *
 * This returns the global per-system current limit (in mA) that ABL will use to cap LED output.
 *
 * @return Maximum allowed current in milliamps for ABL.
 */
static uint16_t ablMilliampsMax()  { return _milliAmpsMax; }

    static int add(const BusConfig &bc);
    static void useParallelOutput(); // workaround for inaccessible PolyBus
    static bool hasParallelOutput(); // workaround for inaccessible PolyBus

    //do not call this method from system context (network callback)
    static void removeAll();

    static void on();
    static void off();

    static void show();
    static bool canAllShow();
    static void setStatusPixel(uint32_t c);
    [[gnu::hot]] static void setPixelColor(unsigned pix, uint32_t c);
    static void setBrightness(uint8_t b);
    // for setSegmentCCT(), cct can only be in [-1,255] range; allowWBCorrection will convert it to K
    // WARNING: setSegmentCCT() is a misleading name!!! much better would be setGlobalCCT() or just setCCT()
    static void setSegmentCCT(int16_t cct, bool allowWBCorrection = false);
    /**
 * Set the global maximum allowed LED current budget.
 *
 * Updates the BusManager's global milliamps cap (in milliamps). This value is used
 * to limit per-bus brightness/current calculations and overall power budgeting.
 *
 * @param max Maximum allowed current in milliamps (mA).
 */
static inline void setMilliampsMax(uint16_t max) { _milliAmpsMax = max;}
    static uint32_t getPixelColor(unsigned pix);
    /**
 * Return the current segment color temperature (CCT).
 *
 * @return CCT value (in Kelvin) for the active segment.
 */
static inline int16_t getSegmentCCT() { return Bus::getCCT(); }

    static Bus* getBus(uint8_t busNr);

    //semi-duplicate of strip.getLengthTotal() (though that just returns strip._length, calculated in finalizeInit())
    static uint16_t getTotalLength();
    /**
 * Return the number of configured buses.
 *
 * @return Number of active buses (as uint8_t). If the internal container holds more than 255 buses, the value will wrap/truncate to fit into a uint8_t.
 */
static inline uint8_t getNumBusses() { return busses.size(); }
    static String getLEDTypesJSONString();

    /**
 * Access the shared ColorOrderMap used by all buses.
 *
 * Returns a reference to the global ColorOrderMap instance maintained by the bus manager.
 * The map is owned by the module and has static lifetime; callers may read or modify it to
 * inspect or update per-pixel color-order mappings for configured buses.
 *
 * @return Reference to the module's global ColorOrderMap.
 */
static inline ColorOrderMap& getColorOrderMap() { return colorOrderMap; }

  private:
    //static std::vector<std::unique_ptr<Bus>> busses; // we'd need C++ >11
    static std::vector<Bus*> busses;
    static ColorOrderMap colorOrderMap;
    static uint16_t _milliAmpsUsed;
    static uint16_t _milliAmpsMax;

    #ifdef ESP32_DATA_IDLE_HIGH
    static void    esp32RMTInvertIdle() ;
    #endif
    static uint8_t getNumVirtualBusses() {
      int j = 0;
      for (const auto &bus : busses) j += bus->isVirtual();
      return j;
    }
};
#endif
