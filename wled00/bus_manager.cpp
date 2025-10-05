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
#include "bus_manager.h"
#include "bus_wrapper.h"
#include <bits/unique_ptr.h>

extern bool cctICused;
extern bool useParallelI2S;

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


/**
 * @brief Add a color-order mapping for a pixel range.
 *
 * Adds a mapping that applies a per-pixel color-order (and optional W-swap in the upper nibble)
 * to the range [start, start+len). The mapping is appended to the internal list and logged.
 *
 * The call fails and returns false if:
 * - the mapping list is full (>= WLED_MAX_COLOR_ORDER_MAPPINGS),
 * - len is zero,
 * - or the lower nibble of colorOrder is greater than COL_ORDER_MAX.
 *
 * @param start First pixel index of the mapping.
 * @param len Number of pixels in the mapping.
 * @param colorOrder Lower nibble = color order (0..COL_ORDER_MAX); upper nibble reserved for W-swap flags.
 * @return true if the mapping was added; false on validation failure.
 */
bool ColorOrderMap::add(uint16_t start, uint16_t len, uint8_t colorOrder) {
  if (count() >= WLED_MAX_COLOR_ORDER_MAPPINGS || len == 0 || (colorOrder & 0x0F) > COL_ORDER_MAX) return false; // upper nibble contains W swap information
  _mappings.push_back({start,len,colorOrder});
  DEBUGBUS_PRINTF_P(PSTR("Bus: Add COM (%d,%d,%d)\n"), (int)start, (int)len, (int)colorOrder);
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

/**
 * @brief Convert an RGB or RGBW color to an RGBW value according to the bus auto-white mode.
 *
 * Uses the bus's configured auto-white mode (_autoWhiteMode) or the global override (_gAWM)
 * to decide how to derive a white component from the input color. Behavior by mode:
 * - RGBW_MODE_MANUAL_ONLY: return the input unchanged.
 * - RGBW_MODE_DUAL: if the input already contains a non-zero white channel, return unchanged;
 *   otherwise fall through to standard auto-white behavior.
 * - RGBW_MODE_MAX: set white to the brightest RGB channel (max(R,G,B)).
 * - RGBW_MODE_AUTO_ACCURATE: set white to min(R,G,B) and subtract that white amount from R,G,B.
 * - other auto-white modes: set white to min(R,G,B) without subtracting (standard auto-white).
 *
 * The input is interpreted via the R,G,B,W macros and the result is packed with RGBW32.
 *
 * @param c Input color (packed 32-bit RGB or RGBW).
 * @return uint32_t Packed RGBW color after applying the selected auto-white algorithm.
 */
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

/**
 * @brief Allocate or reallocate the per-bus pixel data buffer.
 *
 * Frees any previously allocated buffer, then allocates a zero-initialized
 * buffer of `size` bytes and stores the pointer in the bus' internal
 * `_data` member.
 *
 * @param size Number of bytes to allocate. If zero, no allocation is made and
 *             `_data` is set to nullptr.
 * @return uint8_t* Pointer to the allocated buffer, or nullptr if `size` is
 *                   zero or allocation failed.
 */
uint8_t *Bus::allocateData(size_t size) {
  freeData(); // should not happen, but for safety
  return _data = (uint8_t *)(size>0 ? calloc(size, sizeof(uint8_t)) : nullptr);
}

/**
 * @brief Release the bus's allocated pixel data buffer.
 *
 * Frees the internal _data buffer if it is non-null and sets _data to nullptr.
 * Safe to call repeatedly.
 */
void Bus::freeData() {
  if (_data) free(_data);
  _data = nullptr;
}

/**
 * @brief Construct a digital LED bus from a configuration.
 *
 * Initializes a digital output bus: reserves required GPIO pins, determines
 * the underlying PolyBus I/O type, optionally allocates a per-bus pixel
 * buffer, and creates the PolyBus instance used for driving the LEDs.
 *
 * On success the bus is marked valid; on failure the constructor returns
 * early after leaving the object in an invalid state (resources that were
 * already acquired are released where applicable).
 *
 * @param bc Bus configuration (type, start, count, pins, frequency, skip amount,
 *           color order, double-buffer flag, current limits, etc.). If
 *           `bc.doubleBuffer` is true this constructor attempts to allocate
 *           a data buffer sized for `bc.count * Bus::getNumberOfChannels(bc.type)`.
 * @param nr  Index of this bus (used when selecting the PolyBus backend).
 * @param com Per-bus ColorOrderMap used for per-pixel color-order overrides.
 */
BusDigital::BusDigital(const BusConfig &bc, uint8_t nr, const ColorOrderMap &com)
: Bus(bc.type, bc.start, bc.autoWhite, bc.count, bc.reversed, (bc.refreshReq || bc.type == TYPE_TM1814))
, _skip(bc.skipAmount) //sacrificial pixels
, _colorOrder(bc.colorOrder)
, _milliAmpsPerLed(bc.milliAmpsPerLed)
, _milliAmpsMax(bc.milliAmpsMax)
, _colorOrderMap(com)
{
  DEBUGBUS_PRINTLN(F("Bus: Creating digital bus."));
  if (!isDigital(bc.type) || !bc.count) { DEBUGBUS_PRINTLN(F("Not digial or empty bus!")); return; }
  if (!PinManager::allocatePin(bc.pins[0], true, PinOwner::BusDigital)) { DEBUGBUS_PRINTLN(F("Pin 0 allocated!")); return; }
  _frequencykHz = 0U;
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
  if (bc.doubleBuffer && !allocateData(bc.count * Bus::getNumberOfChannels(bc.type))) { DEBUGBUS_PRINTLN(F("Buffer allocation failed!")); return; }
  //_buffering = bc.doubleBuffer;
  uint16_t lenToCreate = bc.count;
  if (bc.type == TYPE_WS2812_1CH_X3) lenToCreate = NUM_ICS_WS2812_1CH_3X(bc.count); // only needs a third of "RGB" LEDs for NeoPixelBus
  _busPtr = PolyBus::create(_iType, _pins, lenToCreate + _skip, nr);
  _valid = (_busPtr != nullptr);
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

/**
 * @brief Estimate total LED current draw and compute a brightness cap to fit the power budget.
 *
 * Computes an estimated milliamp total for this digital bus based on per-pixel color values,
 * configured per-LED current, and the bus/global current limit. Updates the static
 * BusDigital::_milliAmpsTotal with the estimated (or budget-clamped) current.
 *
 * Behavior notes:
 * - If the bus-level max current is effectively disabled (too small relative to
 *   platform reservation per bus) or the per-LED current is 0, the function returns
 *   the current brightness unchanged.
 * - A special WS2815 power model is used when _milliAmpsPerLed == 255 (treated as ~12 mA).
 * - White-channel handling: for RGBW LEDs the summed channel value is scaled to reflect
 *   lower per-channel contribution.
 * - If the estimated current exceeds the available power budget, the function returns
 *   a reduced brightness value scaled to keep current within the budget. The bus's
 *   stored brightness (_bri) is not modified by this function; the caller should apply
 *   the returned value as needed.
 *
 * @return uint8_t Brightness value to use (possibly reduced to meet current limits).
 */
uint8_t BusDigital::estimateCurrentAndLimitBri() const {
  bool useWackyWS2815PowerModel = false;
  byte actualMilliampsPerLed = _milliAmpsPerLed;

  if (_milliAmpsMax < MA_FOR_ESP/BusManager::getNumBusses() || actualMilliampsPerLed == 0) { //0 mA per LED and too low numbers turn off calculation
    return _bri;
  }

  if (_milliAmpsPerLed == 255) {
    useWackyWS2815PowerModel = true;
    actualMilliampsPerLed = 12; // from testing an actual strip
  }

  unsigned powerBudget = (_milliAmpsMax - MA_FOR_ESP/BusManager::getNumBusses()); //80/120mA for ESP power
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
  BusDigital::_milliAmpsTotal = (busPowerSum * actualMilliampsPerLed * _bri) / (765*255);

  uint8_t newBri = _bri;
  if (BusDigital::_milliAmpsTotal > powerBudget) {
    //scale brightness down to stay in current limit
    unsigned scaleB = powerBudget * 255 / BusDigital::_milliAmpsTotal;
    newBri = (_bri * scaleB) / 256 + 1;
    BusDigital::_milliAmpsTotal = powerBudget;
    //_milliAmpsTotal = (busPowerSum * actualMilliampsPerLed * newBri) / (765*255);
  }
  return newBri;
}

/**
 * @brief Send the bus pixel buffer (or hardware pixels) to the underlying driver.
 *
 * Performs per-pixel color composition, CCT handling, color-order mapping and
 * current-limiting before pushing pixels to PolyBus and issuing a hardware
 * show. If current estimates require it, brightness is temporarily reduced
 * for the update and restored afterwards.
 *
 * Detailed behavior:
 * - Updates BusDigital::_milliAmpsTotal via estimateCurrentAndLimitBri().
 * - If a per-bus data buffer exists, builds each pixel's 32-bit color from
 *   the buffer (special-casing TYPE_WS2812_1CH_X3 and WWA/CCT formats),
 *   applies per-pixel CCT when present by temporarily overriding Bus::_cct,
 *   applies color-order per pixel, and writes via PolyBus::setPixelColor().
 * - If no data buffer is present and brightness was limited, reads hardware
 *   pixels, applies lossy brightness scaling, optionally recalculates CCT,
 *   and rewrites pixels so the hardware reflects the reduced brightness.
 * - Paints any configured skipped pixels black.
 * - Calls PolyBus::show() to flush pixels to hardware and restores the
 *   original bus brightness if it was reduced.
 *
 * Side effects:
 * - Writes pixel data to the hardware driver via PolyBus.
 * - May temporarily change the global Bus::_cct value per pixel and restores
 *   it before returning.
 * - Temporarily lowers hardware brightness to satisfy current limits and
 *   restores it after the show.
 */
void BusDigital::show() {
  BusDigital::_milliAmpsTotal = 0;
  if (!_valid) return;

  uint8_t cctWW = 0, cctCW = 0;
  unsigned newBri = estimateCurrentAndLimitBri();  // will fill _milliAmpsTotal (TODO: could use PolyBus::CalcTotalMilliAmpere())
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
        if (_type == TYPE_WS2812_WWA) c = RGBW32(cctWW, cctCW, 0, W(c)); // may need swapping
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
/**
 * @brief Set the status pixel color for a digital bus and trigger an immediate show if possible.
 *
 * Writes the provided packed color to the first logical pixel (index 0) and requests a hardware update
 * only when the bus is valid and configured with skipped pixels. No effect otherwise.
 *
 * @param c Packed 32-bit color value (RGB or RGBW format depending on the bus) to write to the status pixel.
 */
void BusDigital::setStatusPixel(uint32_t c) {
  if (_valid && _skip) {
    PolyBus::setPixelColor(_busPtr, _iType, 0, c, _colorOrderMap.getPixelColorOrder(_start, _colorOrder));
    if (canShow()) PolyBus::show(_busPtr, _iType);
  }
}

/**
 * @brief Set the color of a single pixel on a digital bus.
 *
 * Applies auto-white and CCT-based color correction, then writes the resulting
 * color either into the bus's RAM buffer (if present) or directly to the
 * underlying PolyBus hardware. Handles bus-level transformations such as
 * reversing, skipped-start offset, per-pixel color-order lookup, and special
 * per-chip mappings (e.g., TYPE_WS2812_1CH_X3). When a per-bus data buffer
 * exists, RGB(W) channels are stored sequentially and per-pixel CCT information
 * is encoded into the buffer if the bus supports CCT.
 *
 * If the bus is invalid no action is taken.
 *
 * @param pix Zero-based pixel index within this bus.
 * @param c  32-bit color value (packed RGBW).
 */
void IRAM_ATTR BusDigital::setPixelColor(unsigned pix, uint32_t c) {
  if (!_valid) return;
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
    uint16_t wwcw = 0;
    if (hasCCT()) {
      uint8_t cctWW = 0, cctCW = 0;
      Bus::calculateCCT(c, cctWW, cctCW);
      wwcw = (cctCW<<8) | cctWW;
      if (_type == TYPE_WS2812_WWA) c = RGBW32(cctWW, cctCW, 0, W(c)); // may need swapping
    }
    PolyBus::setPixelColor(_busPtr, _iType, pix, c, co, wwcw);
  }
}

/**
 * @brief Retrieve the color of a pixel from this digital bus.
 *
 * Returns the original stored color when the bus maintains a local buffer; otherwise
 * reads the color from the underlying PolyBus and returns a lossy-restored RGBW value
 * (brightness-limited). Special per-type adjustments are applied:
 * - TYPE_WS2812_1CH_X3: maps each logical LED to its single-channel IC value and
 *   expands it to an RGBW grayscale value for that channel position.
 * - TYPE_WS2812_WWA: combines red/green into a warm-white-like RGBW value.
 *
 * If the bus is invalid, returns 0. The returned 32-bit value is in packed RGBW form.
 *
 * @return uint32_t Packed RGBW color for the requested pixel (0 on invalid bus).
 */
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
    if (_type == TYPE_WS2812_WWA) {
      uint8_t w = R(c) | G(c);
      c = RGBW32(w, w, 0, w);
    }
    return c;
  }
}

/**
 * @brief Return the number of pins used by this digital bus and optionally fill an array with them.
 *
 * If `pinArray` is non-null, the function writes up to the returned count of pin numbers into it
 * in bus-specific order.
 *
 * @param pinArray Optional output buffer to receive pin numbers; must be large enough for the returned count.
 * @return unsigned Number of pins used by the bus (1 or 2).
 */
unsigned BusDigital::getPins(uint8_t* pinArray) const {
  unsigned numPins = is2Pin(_type) + 1;
  if (pinArray) for (unsigned i = 0; i < numPins; i++) pinArray[i] = _pins[i];
  return numPins;
}

/**
 * @brief Compute the total memory footprint of this BusDigital instance.
 *
 * Returns the size in bytes occupied by the BusDigital object itself plus any
 * associated dynamic buffers used when the bus is operational. When the bus
 * is valid (isOk()), the calculation includes:
 * - the platform-specific PolyBus internal data size for the underlying bus
 *   implementation, and
 * - the per-pixel data buffer size (_len * number of channels) if an external
 *   data buffer (_data) has been allocated.
 *
 * @return unsigned Total number of bytes used by this BusDigital instance and
 *                 its active dynamic buffers.
 */
unsigned BusDigital::getBusSize() const {
  return sizeof(BusDigital) + (isOk() ? PolyBus::getDataSize(_busPtr, _iType) + (_data ? _len * getNumberOfChannels() : 0) : 0);
}

/**
 * @brief Set the per-bus color channel ordering.
 *
 * Sets the color order used when writing pixel data. The lower nibble
 * (bits 0–3) selects one of the supported orders (0–5). The upper nibble is
 * reserved for white-channel swap flags and is preserved. If the lower nibble
 * is outside 0–5, the call is ignored and the current order is unchanged.
 *
 * @param colorOrder Byte encoding color order (lower nibble = order 0–5;
 *                   upper nibble = W-swap flags).
 */
void BusDigital::setColorOrder(uint8_t colorOrder) {
  // upper nibble contains W swap information
  if ((colorOrder & 0x0F) > 5) return;
  _colorOrder = colorOrder;
}

/**
 * @brief Return the set of LED hardware types supported by the digital bus.
 *
 * Provides a vector of LEDType descriptors representing LED chipset variants
 * that BusDigital can drive (RGB, RGBW, 1‑channel white variants, and
 * two‑wire clocked types). Each entry encodes the LED type identifier and a
 * short transport/category code.
 *
 * @return std::vector<LEDType> List of supported LED type descriptors.
 */
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

/**
 * @brief Initialize and start the underlying digital LED bus.
 *
 * If the BusDigital instance is valid, this forwards configuration (bus pointer,
 * LED type, pin list and frequency) to PolyBus::begin() to initialize the
 * hardware/driver for subsequent show() calls.
 */
void BusDigital::begin() {
  if (!_valid) return;
  PolyBus::begin(_busPtr, _iType, _pins, _frequencykHz);
}

/**
 * @brief Release resources and reset state for a digital LED bus.
 *
 * Cleans up the underlying PolyBus, frees the per-bus pixel data buffer, deallocates configured pins,
 * and marks the bus invalid. After this call the BusDigital instance will not drive LEDs until reinitialized.
 */
void BusDigital::cleanup() {
  DEBUGBUS_PRINTLN(F("Digital Cleanup."));
  PolyBus::cleanup(_busPtr, _iType);
  _iType = I_NONE;
  _valid = false;
  _busPtr = nullptr;
  freeData();
  //PinManager::deallocateMultiplePins(_pins, 2, PinOwner::BusDigital);
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

/**
 * @brief Initialize a PWM output bus from a BusConfig.
 *
 * Constructs and configures a PWM-based bus according to `bc`: allocates the required GPIOs,
 * selects PWM frequency and resolution, configures platform-specific PWM hardware (ESP8266
 * analogWrite or ESP32 LEDC channels/timers), and prepares internal state (RGB/white/CCT flags,
 * data buffer pointer). When the constructor completes successfully the bus is marked valid.
 *
 * @param bc Bus configuration used to determine type, pins, frequency, dithering flag and other options.
 *
 * @note Dithering is enabled when the configuration's refresh flag is set (this code reuses that
 *       field to indicate dithering). On ESP32 LEDC channels/timers are allocated and attached;
 *       on allocation failure the previously allocated pins/LEDC resources are released and the
 *       bus remains invalid. The constructor sets pinMode for allocated pins (ESP8266) or attaches
 *       pins to LEDC channels (ESP32).
 */
BusPwm::BusPwm(const BusConfig &bc)
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
  _data = _pwmdata; // avoid malloc() and use already allocated memory
  _valid = true;
  DEBUGBUS_PRINTF_P(PSTR("%successfully inited PWM strip with type %u, frequency %u, bit depth %u and pins %u,%u,%u,%u,%u\n"), _valid?"S":"Uns", bc.type, _frequency, _depth, _pins[0], _pins[1], _pins[2], _pins[3], _pins[4]);
}

/**
 * @brief Set the color for the PWM bus (only the first pixel is supported).
 *
 * Updates the internal PWM channel values for this bus according to the provided
 * 32-bit color value. This function only acts when `pix == 0` and the bus is
 * valid; other pixel indices are ignored.
 *
 * The input color is processed as follows:
 * - For non-3CH types, automatic white balancing is applied via Bus::autoWhiteCalc.
 * - If a segment CCT (Bus::_cct) is configured (>= 1900) and the bus is a 3CH or
 *   4CH analog type, colorCorrection is applied using colorBalanceFromKelvin.
 *
 * Channel mapping written into the _data array by bus type:
 * - TYPE_ANALOG_1CH: _data[0] = W (white)
 * - TYPE_ANALOG_2CH: _data[0] = W, _data[1] = CCT (either from cct IC or calculated)
 * - TYPE_ANALOG_3CH: _data[0..2] = R, G, B
 * - TYPE_ANALOG_4CH: _data[0..2] = R, G, B; _data[3] = W
 * - TYPE_ANALOG_5CH: _data[0..2] = R, G, B; _data[4] = CCT (and W is calculated/stored as needed)
 *
 * If a hardware CCT IC is in use (cctICused), the stored CCT values come from
 * Bus::_cct (clamped to 0..255 with 127 as a fallback); otherwise CCT/white are
 * computed with Bus::calculateCCT.
 *
 * @param pix Pixel index (only index 0 is used; other values are ignored).
 * @param c   32-bit color value (format: 0xWWRRGGBB or the codebase's color packing).
 */
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

/**
 * @brief Return the color stored in the PWM bus for a given pixel index.
 *
 * Maps the internal per-channel PWM values to an RGBW 32-bit color according to the bus type.
 * If the bus is not valid this returns 0. This function does not perform bounds/index checking.
 *
 * Behavior by bus type:
 * - TYPE_ANALOG_1CH: returns white only (W = _data[0]).
 * - TYPE_ANALOG_2CH: returns white as either W = _data[0] (when cctICused) or W = _data[0]+_data[1].
 * - TYPE_ANALOG_3CH: standard RGB (R,G,B from _data[0..2], W=0).
 * - TYPE_ANALOG_4CH: RGBW (R,G,B,W from _data[0..3]).
 * - TYPE_ANALOG_5CH: RGB + two whites: when cctICused returns W = _data[3] (cold/warm handled externally),
 *                    otherwise W = _data[3] + _data[4].
 *
 * @param pix Pixel index (ignored for single-address PWM implementations; no range checking).
 * @return uint32_t 32-bit RGBW color (packed as RGBW32). Returns 0 if the bus is invalid.
 */
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

/**
 * @brief Output the current per-pin PWM color data to hardware.
 *
 * Converts stored channel values to PWM duty cycles, applies a perceptual
 * brightness curve (CIE-inspired), optional temporal dithering, and writes
 * the resulting duties to the platform PWM backend (analogWrite on ESP8266,
 * LEDC registers on ESP32). When enabled, phase-shifting is applied across
 * channels to spread instantaneous load (required for certain 2-wire PWM CCT
 * H-bridge drivers); dead-time is inserted when using 2-channel CCT with
 * non-overlapping (additive = 0) white channels.
 *
 * Side effects:
 * - Updates hardware PWM outputs for each configured pin.
 * - Uses _needsRefresh to enable 4-bit fractional dithering.
 * - On ESP32, writes directly to LEDC channel registers and calls ledc_update_duty.
 *
 * Notes:
 * - Brightness mapping uses a small linear region for very low values and a
 *   cubic-like curve for the remainder to better match perceived brightness.
 * - Phase shifting and dead-time behavior are only meaningful when driving
 *   reverse-polarity (H-bridge) or 2-wire PWM CCT configurations; for other
 *   setups it serves to spread PSU load.
 */
void BusPwm::show() {
  if (!_valid) return;
  // if _needsRefresh is true (UI hack) we are using dithering (credit @dedehai & @zalatnaicsongor)
  // https://github.com/Aircoookie/WLED/pull/4115 and https://github.com/zalatnaicsongor/WLED/pull/1)
  const bool     dithering = _needsRefresh; // avoid working with bitfield
  const unsigned numPins = getPins();
  const unsigned maxBri = (1<<_depth);      // possible values: 16384 (14), 8192 (13), 4096 (12), 2048 (11), 1024 (10), 512 (9) and 256 (8) 
  [[maybe_unused]] const unsigned bitShift = dithering * 4;  // if dithering, _depth is 12 bit but LEDC channel is set to 8 bit (using 4 fractional bits)

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

/**
 * @brief Get the number of PWM pins used by this bus and optionally return their GPIO numbers.
 *
 * If the bus is not valid this returns 0. When `pinArray` is non-null, the function writes
 * up to the returned count of pin GPIO numbers into the provided buffer; the caller must
 * ensure the buffer can hold at least the returned number of elements.
 *
 * @param pinArray Optional output buffer to receive GPIO pin numbers; may be nullptr.
 * @return unsigned Number of PWM pins for this bus (0 if the bus is invalid).
 */
unsigned BusPwm::getPins(uint8_t* pinArray) const {
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

/**
 * @brief Release PWM pins and related hardware resources used by this bus.
 *
 * Deallocates each configured pin from the PinManager and detaches or disables
 * any platform-specific PWM hardware associated with those pins:
 * - On ESP8266: writes LOW to the pin to stop PWM interrupts.
 * - On ESP32: detaches the pin from LEDC and releases the contiguous LEDC
 *   channel block starting at _ledcStart.
 *
 * Safe to call when pins are not initialized; skips pins that are not valid.
 */
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


/**
 * @brief Construct an On/Off (single-pin) bus from a BusConfig.
 *
 * Initializes a simple on/off output bus: validates the bus type, reserves the configured
 * pin via PinManager, configures it as an OUTPUT, and sets internal capability flags.
 * On success the instance is marked valid and uses an internal stack-backed data word
 * (no heap allocation). If the BusConfig type is not an On/Off type or pin allocation
 * fails, the constructor returns early and the bus remains invalid.
 *
 * @param bc Source BusConfig: the constructor uses bc.type, bc.start, bc.autoWhite,
 *           bc.reversed and bc.pins[0] (the output pin).
 */
BusOnOff::BusOnOff(const BusConfig &bc)
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

/**
 * @brief Set the on/off state for an on/off bus based on a color input.
 *
 * Only the first pixel (index 0) is meaningful for On/Off buses; calls with
 * any other pixel index are ignored. The provided color is passed through
 * autoWhiteCalc() before evaluating whether any channel (R, G, B, W) is
 * nonzero. If the resulting color has any nonzero channel and the bus
 * brightness is nonzero, the internal state is set to 0xFF (on); otherwise
 * it is set to 0 (off).
 *
 * @param pix Pixel index (only 0 is used).
 * @param c  32-bit color value (RAW format; R,G,B,W channels accessed).
 */
void BusOnOff::setPixelColor(unsigned pix, uint32_t c) {
  if (pix != 0 || !_valid) return; //only react to first pixel
  c = autoWhiteCalc(c);
  uint8_t r = R(c);
  uint8_t g = G(c);
  uint8_t b = B(c);
  uint8_t w = W(c);
  _data[0] = bool(r|g|b|w) && bool(_bri) ? 0xFF : 0;
}

/**
 * @brief Get the virtual pixel color for an On/Off bus.
 *
 * Returns a 32-bit RGBW color representing the bus' on/off state. If the bus
 * is invalid, returns 0 (off). The returned color uses the same byte value
 * for R, G, B and W derived from the internal on/off level.
 *
 * @param pix Pixel index (ignored for On/Off buses; only the bus-level state is returned).
 * @return uint32_t 32-bit RGBW color (packed with RGBW32) or 0 if the bus is not valid.
 */
uint32_t BusOnOff::getPixelColor(unsigned pix) const {
  if (!_valid) return 0;
  return RGBW32(_data[0], _data[0], _data[0], _data[0]);
}

/**
 * @brief Apply the stored on/off state to the configured output pin.
 *
 * If the bus is valid, writes a digital HIGH/LOW to the configured pin
 * using the internal on/off state stored in _data[0]. The _reversed flag
 * inverts the output when set.
 */
void BusOnOff::show() {
  if (!_valid) return;
  digitalWrite(_pin, _reversed ? !(bool)_data[0] : (bool)_data[0]);
}

/**
 * @brief Get the pin(s) used by this On/Off bus.
 *
 * If a non-null pinArray is provided, the first element will be set to the configured pin.
 *
 * @param pinArray Optional output buffer to receive pin numbers (at least 1 element).
 * @return unsigned Number of pins used: 0 if the bus is invalid, otherwise 1.
 */
unsigned BusOnOff::getPins(uint8_t* pinArray) const {
  if (!_valid) return 0;
  if (pinArray) pinArray[0] = _pin;
  return 1;
}

/**
 * @brief Return the LED type(s) supported by the On/Off bus.
 *
 * @return std::vector<LEDType> A vector containing a single LEDType describing a simple On/Off device.
 */
std::vector<LEDType> BusOnOff::getLEDTypes() {
  return {
    {TYPE_ONOFF, "", PSTR("On/Off")},
  };
}

/**
 * @brief Construct a virtual network LED bus from a configuration.
 *
 * Initializes a network (ArtNet/E1.31/DDP) bus: selects the UDP protocol type based
 * on the configured bus type, sets RGB/white/CCT capability flags, computes per-pixel
 * UDP channel count, derives the destination IP from the four configured pins, and
 * allocates the internal transmit buffer.
 *
 * If buffer allocation fails the instance is marked invalid (_valid == false).
 *
 * @param bc Source BusConfig containing type, start/count, and 4 bytes of destination IP in pins[0..3].
 */
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
  _valid = (allocateData(_len * _UDPchannels) != nullptr);
  DEBUGBUS_PRINTF_P(PSTR("%successfully inited virtual strip with type %u and IP %u.%u.%u.%u\n"), _valid?"S":"Uns", bc.type, bc.pins[0], bc.pins[1], bc.pins[2], bc.pins[3]);
}

/**
 * @brief Set the color for a pixel in the network (UDP) buffer.
 *
 * Updates the internal per-pixel UDP channel buffer for the given pixel index.
 * If the bus is invalid or the pixel index is out of range, the call is a no-op.
 * When the bus exposes a white channel, the color is passed through the bus'
 * auto-white calculation. If a CCT value is active (Bus::_cct >= 1900), the
 * color is further adjusted by colorBalanceFromKelvin before storing.
 *
 * @param pix Zero-based pixel index within this bus.
 * @param c  32-bit packed color (RGB or RGBW); components are extracted and
 *           written into the internal UDP channel buffer at index (pix * _UDPchannels).
 */
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

/**
 * @brief Retrieve the packed RGBW color for a network-managed pixel.
 *
 * Returns a 32-bit packed color (RGBW) for the pixel at index `pix` from the
 * network bus' internal buffer. If the bus is invalid or `pix` is out of
 * range, returns 0.
 *
 * @param pix Zero-based pixel index.
 * @return uint32_t Packed color in RGBW32 format (R,G,B,W). W is 0 when the
 *                 bus does not provide a white channel.
 */
uint32_t BusNetwork::getPixelColor(unsigned pix) const {
  if (!_valid || pix >= _len) return 0;
  unsigned offset = pix * _UDPchannels;
  return RGBW32(_data[offset], _data[offset+1], _data[offset+2], (hasWhite() ? _data[offset+3] : 0));
}

/**
 * @brief Send the bus' pixel data over the network.
 *
 * If the bus is valid and ready to show, sets an internal broadcast lock,
 * invokes the realtime broadcast with the bus' UDP type, destination client,
 * pixel count, data buffer, current brightness, and white-channel flag, then
 * releases the lock.
 *
 * Early-returns without sending if the bus is not valid or cannot show.
 */
void BusNetwork::show() {
  if (!_valid || !canShow()) return;
  _broadcastLock = true;
  realtimeBroadcast(_UDPtype, _client, _len, _data, _bri, hasWhite());
  _broadcastLock = false;
}

/**
 * @brief Retrieve the 4-byte destination address used by this network bus.
 *
 * If a non-null pointer is provided, writes the 4 address octets into pinArray
 * in network byte order (index 0 = first octet). The buffer must be at least
 * 4 bytes long.
 *
 * @param pinArray Pointer to a 4-byte buffer to receive the address (may be nullptr).
 * @return unsigned Always returns 4 (number of address octets written/available).
 */
unsigned BusNetwork::getPins(uint8_t* pinArray) const {
  if (pinArray) for (unsigned i = 0; i < 4; i++) pinArray[i] = _client[i];
  return 4;
}

/**
 * @brief Return the list of LED type descriptors supported by the network bus.
 *
 * Returns a vector of LEDType entries describing network-backed LED strip types
 * that BusNetwork can drive (currently DDP and Art-Net variants for RGB and RGBW).
 * Each LEDType encodes a numeric type ID, a short "pin/field" format string (used
 * by the UI/config to determine how many numeric fields the type exposes — e.g. an
 * "N" denotes a network/address field), and a program-memory description string.
 *
 * @return std::vector<LEDType> List of supported network LED type descriptors.
 */
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

/**
 * @brief Tear down the virtual (network) bus and release its resources.
 *
 * Marks the BusNetwork as invalid, resets its LED type to I_NONE, and frees
 * any per-bus pixel data buffer allocated by the bus.
 */
void BusNetwork::cleanup() {
  DEBUGBUS_PRINTLN(F("Virtual Cleanup."));
  _type = I_NONE;
  _valid = false;
  freeData();
}


/**
 * @brief Estimate the approximate memory footprint required for this bus configuration.
 *
 * Computes an approximate number of bytes that the bus described by this BusConfig
 * will consume if instantiated. Calculation varies by bus type:
 * - Network/virtual buses: sizeof(BusNetwork) plus per-pixel channel storage.
 * - Digital buses: sizeof(BusDigital) plus PolyBus internal buffers (based on
 *   LED count and bus interface) and optional double-buffer LED data.
 * - On/Off buses: sizeof(BusOnOff).
 * - PWM buses: sizeof(BusPwm).
 *
 * This is an estimate only and does not perform any allocation. It uses
 * PolyBus::getI(type, pins, nr) when accounting for PolyBus internals.
 *
 * @param nr Index of this bus within the manager (passed to PolyBus helpers to
 *           select interface-specific memory costs).
 * @return unsigned Estimated memory usage in bytes for the configured bus.
 */
unsigned BusConfig::memUsage(unsigned nr) const {
  if (Bus::isVirtual(type)) {
    return sizeof(BusNetwork) + (count * Bus::getNumberOfChannels(type));
  } else if (Bus::isDigital(type)) {
    return sizeof(BusDigital) + PolyBus::memUsage(count + skipAmount, PolyBus::getI(type, pins, nr)) + doubleBuffer * (count + skipAmount) * Bus::getNumberOfChannels(type);
  } else if (Bus::isOnOff(type)) {
    return sizeof(BusOnOff);
  } else {
    return sizeof(BusPwm);
  }
}


/**
 * @brief Compute the estimated memory footprint required by all configured buses.
 *
 * Calculates the total memory used by per-bus buffers and any platform-specific
 * shared back-buffer allocation for parallel I2S output. On platforms that
 * support parallel I2S (ESP32, S2, S3), the largest digital bus's back-buffer
 * portion is treated as a shared allocation and added once; front buffers remain
 * accounted per-bus. On constrained platforms (ESP32-C3, ESP8266) the special
 * parallel-I2S sharing logic is skipped.
 *
 * @return unsigned Total estimated memory usage (same units returned by getBusSize())
 */
unsigned BusManager::memUsage() {
  // when ESP32, S2 & S3 use parallel I2S only the largest bus determines the total memory requirements for back buffers
  // front buffers are always allocated per bus
  unsigned size = 0;
  unsigned maxI2S = 0;
  #if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(ESP8266)
  unsigned digitalCount = 0;
    #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
      #define MAX_RMT 4
    #else
      #define MAX_RMT 8
    #endif
  #endif
  for (const auto &bus : busses) {
    unsigned busSize = bus->getBusSize();
    #if !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(ESP8266)
    if (bus->isDigital() && !bus->is2Pin()) digitalCount++;
    if (PolyBus::isParallelI2S1Output() && digitalCount > MAX_RMT) {
      unsigned i2sCommonSize = 3 * bus->getLength() * bus->getNumberOfChannels() * (bus->is16bit()+1);
      if (i2sCommonSize > maxI2S) maxI2S = i2sCommonSize;
      busSize -= i2sCommonSize;
    }
    #endif
    size += busSize;
  }
  return size + maxI2S;
}

/**
 * @brief Create and append a new Bus instance from a bus configuration.
 *
 * Constructs the appropriate Bus subclass (Network, Digital, On/Off, or PWM)
 * based on bc.type, allocates it on the heap, and appends it to the manager's
 * internal bus list. For digital buses a sequential index is assigned based on
 * existing non-2-pin digital buses.
 *
 * @param bc Source configuration describing the bus to create.
 * @return int New total number of buses on success; -1 if adding would exceed
 *             the maximum allowed non-virtual buses (no allocation performed).
 */
int BusManager::add(const BusConfig &bc) {
  DEBUGBUS_PRINTF_P(PSTR("Bus: Adding bus (%d - %d >= %d)\n"), getNumBusses(), getNumVirtualBusses(), WLED_MAX_BUSSES);
  if (getNumBusses() - getNumVirtualBusses() >= WLED_MAX_BUSSES) return -1;
  unsigned numDigital = 0;
  for (const auto &bus : busses) if (bus->isDigital() && !bus->is2Pin()) numDigital++;
  if (Bus::isVirtual(bc.type)) {
    //busses.push_back(std::make_unique<BusNetwork>(bc)); // when C++ >11
    busses.push_back(new BusNetwork(bc));
  } else if (Bus::isDigital(bc.type)) {
    //busses.push_back(std::make_unique<BusDigital>(bc, numDigital, colorOrderMap));
    busses.push_back(new BusDigital(bc, numDigital, colorOrderMap));
  } else if (Bus::isOnOff(bc.type)) {
    //busses.push_back(std::make_unique<BusOnOff>(bc));
    busses.push_back(new BusOnOff(bc));
  } else {
    //busses.push_back(std::make_unique<BusPwm>(bc));
    busses.push_back(new BusPwm(bc));
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

/**
 * @brief Build a JSON array string listing supported LED type descriptors.
 *
 * Collects LED type descriptors from all bus implementations (digital, on/off,
 * PWM, network) and concatenates them into a single JSON array string.
 *
 * @return String A JSON-formatted array (e.g. "[]", or "[{...},{...}]") containing
 *         LED type descriptor objects as produced by LEDTypesToJson.
 */
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

/**
 * @brief Enable parallel I2S output.
 *
 * Sets the system-wide parallel I2S mode by configuring PolyBus for 1-bit
 * parallel output. Call during initialization when parallel I2S should be used.
 */
void BusManager::useParallelOutput() {
  DEBUGBUS_PRINTLN(F("Bus: Enabling parallel I2S."));
  PolyBus::setParallelI2S1Output();
}

/**
 * @brief Check if parallel I2S output is enabled for buses.
 *
 * @return true if parallel I2S output is active; false otherwise.
 */
bool BusManager::hasParallelOutput() {
  return PolyBus::isParallelI2S1Output();
}

/**
 * @brief Remove and destroy all configured buses.
 *
 * Blocks until no bus is performing a show, then deletes every Bus instance,
 * clears the internal bus list, and disables parallel I2S output.
 *
 * @note Do not call this from a system/network callback or other restricted
 *       context — the function may block (calls yield()) while waiting for
 *       ongoing shows to finish.
 */
void BusManager::removeAll() {
  DEBUGBUS_PRINTLN(F("Removing all."));
  //prevents crashes due to deleting busses while in use.
  while (!canAllShow()) yield();
  for (auto &bus : busses) delete bus; // needed when not using std::unique_ptr C++ >11
  busses.clear();
  PolyBus::setParallelI2S1Output(false);
}

#ifdef ESP32_DATA_IDLE_HIGH
// #2478
// If enabled, RMT idle level is set to HIGH when off
/**
 * @brief Invert ESP32 RMT channels' idle output level to prevent MOSFET leakage.
 *
 * Iterates configured digital, multi-pin buses and flips the RMT idle level (HIGH->LOW or LOW->HIGH)
 * for the corresponding RMT channel so that an N-channel MOSFET used for power switching does not
 * leak current when the bus is idle.
 *
 * Only considers buses that are digital, have at least one LED, and are not two-pin buses.
 * The mapping from bus index to RMT channel is platform-dependent and may early-return if no
 * corresponding RMT channel is available. The routine assumes a 1:1 bus-to-RMT-channel mapping.
 *
 * This function has side effects on ESP32 RMT hardware state via rmt_get_idle_level / rmt_set_idle_level.
 */
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
    u++
  }
}
#endif

/**
 * @brief Re-enables hardware buses and platform-specific idle states after system-on.
 *
 * Performs platform-specific recovery actions needed when the system is turned on:
 * - On ESP8266: if the onboard LED pin is owned by a digital bus, reinitialize that bus so the built-in LED state change does not leave the bus hardware in a broken state.
 * - On ESP32 (when ESP32_DATA_IDLE_HIGH is set): flip the RMT idle polarity to prevent leakage when RMT is idle.
 *
 * This function has no return value and may call bus initialization routines as a side effect.
 */
void BusManager::on() {
  #ifdef ESP8266
  //Fix for turning off onboard LED breaking bus
  if (PinManager::getPinOwner(LED_BUILTIN) == PinOwner::BusDigital) {
    for (auto &bus : busses) {
      uint8_t pins[2] = {255,255};
      if (bus->isDigital() && bus->getPins(pins)) {
        if (pins[0] == LED_BUILTIN || pins[1] == LED_BUILTIN) {
          BusDigital *b = static_cast<BusDigital*>(bus);
          b->begin();
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

/**
 * @brief Perform platform-specific actions when the system is turned off.
 *
 * On ESP8266: if the built-in LED is currently owned by a digital bus and no bus
 * requires an off-refresh, reconfigure the built-in LED pin as a plain output
 * and drive it HIGH (visible indicator). This will disrupt the digital bus and
 * requires reinitialization when turning back on.
 *
 * On ESP32 (when ESP32_DATA_IDLE_HIGH is defined): invoke esp32RMTInvertIdle()
 * to flip the RMT idle level for reduced power leakage when the transmitter is idle.
 */
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
}

/**
 * @brief Invoke show() on every managed bus and aggregate their current draw.
 *
 * Calls each Bus::show() in turn and sums the reported milliamp usage into
 * BusManager::_milliAmpsUsed.
 */
void BusManager::show() {
  _milliAmpsUsed = 0;
  for (auto &bus : busses) {
    bus->show();
    _milliAmpsUsed += bus->getUsedCurrent();
  }
}

/**
 * @brief Set the status pixel color on all managed buses.
 *
 * Propagates the given 32-bit packed color to every bus; each bus decides
 * how to apply or display the status color (for example writing to a
 * reserved/status pixel).
 *
 * @param c 32-bit packed color (RGB or RGBW format depending on bus).
 */
void BusManager::setStatusPixel(uint32_t c) {
  for (auto &bus : busses) bus->setStatusPixel(c);
}

/**
 * @brief Set color for a global pixel index across managed buses.
 *
 * Locates all buses whose pixel range includes the given global pixel index and
 * forwards the color to each matching bus using its local pixel index.
 *
 * @param pix Global pixel index (0-based) across all buses.
 * @param c  Packed 32-bit color value passed through to the bus' setPixelColor.
 *
 * @note If no bus contains the given index, the call is a no-op. If bus ranges
 * overlap, every bus covering the index will receive the color update.
 */
void IRAM_ATTR BusManager::setPixelColor(unsigned pix, uint32_t c) {
  for (auto &bus : busses) {
    unsigned bstart = bus->getStart();
    if (pix < bstart || pix >= bstart + bus->getLength()) continue;
    bus->setPixelColor(pix - bstart, c);
  }
}

/**
 * @brief Set the brightness for all managed buses.
 *
 * Propagates the provided brightness level to every Bus owned by the manager.
 *
 * @param b Brightness level (0-255) to apply to each bus.
 */
void BusManager::setBrightness(uint8_t b) {
  for (auto &bus : busses) bus->setBrightness(b);
}

/**
 * @brief Set the global segment CCT (correlated color temperature) for all buses.
 *
 * Sets the internal CCT value used by buses. The input is interpreted as follows:
 * - If cct > 255 it is clamped to 255.
 * - If cct >= 0 and allowWBCorrection is true, the 0–255 input is converted to a Kelvin
 *   value via: kelvin = 1900 + (cct << 5).
 * - If cct < 0 the function stores -1 which signals using a Kelvin approximation derived
 *   from RGB values instead of an explicit CCT.
 *
 * The computed value is forwarded to Bus::setCCT().
 *
 * @param cct  CCT value in the 0–255 range, or negative to request RGB-derived approximation.
 * @param allowWBCorrection  If true and cct >= 0, convert the 0–255 input into a Kelvin value.
 */
void BusManager::setSegmentCCT(int16_t cct, bool allowWBCorrection) {
  if (cct > 255) cct = 255;
  if (cct >= 0) {
    //if white balance correction allowed, save as kelvin value instead of 0-255
    if (allowWBCorrection) cct = 1900 + (cct << 5);
  } else cct = -1; // will use kelvin approximation from RGB
  Bus::setCCT(cct);
}

/**
 * @brief Retrieve the color of a global pixel index across all managed buses.
 *
 * Searches each managed bus for the one that contains the given global pixel index
 * and returns that bus's pixel color (as a 0xAARRGGBB/0xRRGGBB-packed uint32_t, matching bus conventions).
 *
 * @param pix Global pixel index (0-based) across all buses managed by BusManager.
 * @return uint32_t Color of the pixel if found; 0 if no bus contains the supplied index.
 */
uint32_t BusManager::getPixelColor(unsigned pix) {
  for (auto &bus : busses) {
    unsigned bstart = bus->getStart();
    if (!bus->containsPixel(pix)) continue;
    return bus->getPixelColor(pix - bstart);
  }
  return 0;
}

/**
 * @brief Checks whether every managed bus is ready to perform a show.
 *
 * Iterates all buses held by the manager and queries each bus's canShow()
 * status. Returns true only if every bus reports it can show.
 *
 * @return true if all buses are ready to show; false if any bus cannot.
 */
bool BusManager::canAllShow() {
  for (const auto &bus : busses) if (!bus->canShow()) return false;
  return true;
}

/**
 * @brief Retrieve a bus by its index.
 *
 * @param busNr 0-based index of the bus to retrieve.
 * @return Bus* Pointer to the Bus at the given index, or nullptr if the index is out of range.
 */
Bus* BusManager::getBus(uint8_t busNr) {
  if (busNr >= busses.size()) return nullptr;
  return busses[busNr];
}

/**
 * @brief Return the total number of LEDs across all configured buses.
 *
 * Sums each bus' length and returns the aggregate as a 16-bit value.
 *
 * @return uint16_t Total LED count across all buses. If the sum exceeds 65535, the value is truncated/wraps to fit into 16 bits.
 */
uint16_t BusManager::getTotalLength() {
  unsigned len = 0;
  for (const auto &bus : busses) len += bus->getLength();
  return len;
}

bool PolyBus::_useParallelI2S = false;

// Bus static member definition
int16_t Bus::_cct = -1;
uint8_t Bus::_cctBlend = 0;
uint8_t Bus::_gAWM = 255;

uint16_t BusDigital::_milliAmpsTotal = 0;

//std::vector<std::unique_ptr<Bus>> BusManager::busses;
std::vector<Bus*> BusManager::busses;
ColorOrderMap BusManager::colorOrderMap = {};
uint16_t      BusManager::_milliAmpsUsed = 0;
uint16_t      BusManager::_milliAmpsMax = ABL_MILLIAMPS_DEFAULT;
