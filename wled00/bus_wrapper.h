#pragma once
#ifndef BusWrapper_h
#define BusWrapper_h

#include "src/WLEDpixelBus/WLEDpixelBus.h"
#include "src/WLEDpixelBus/WLEDpixelBus_SPI.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "src/WLEDpixelBus/WLEDpixelBus_RMT.h"
#include "src/WLEDpixelBus/WLEDpixelBus_I2S.h"
#include "src/WLEDpixelBus/WLEDpixelBus_LCD.h"
#include "src/WLEDpixelBus/WLEDpixelBus_ParallelSpi.h"
#elif defined(ARDUINO_ARCH_ESP8266)
#include "src/WLEDpixelBus/WLEDpixelBus_ESP8266.h"
#endif

//Hardware SPI Pins
#define P_8266_HS_MOSI 13
#define P_8266_HS_CLK  14
#define P_32_HS_MOSI   13
#define P_32_HS_CLK    14
#define P_32_VS_MOSI   23
#define P_32_VS_CLK    18

// LED timing lookup table in flash (PROGMEM on ESP8266, .rodata on ESP32).
//
// WHY PROGMEM: On ESP8266 the linker only sends specific .rodata.* patterns to flash
// (vtables, string literals, RTTI). All other .rodata — including named namespace-scope
// constexpr variables — lands in DRAM. PROGMEM (__attribute__((section(".irom.text"))))
// is the only escape. This table is read once at bus-creation time; the bus stores its own
// copy. Zero permanent DRAM cost.
//
// On ESP32 PROGMEM is a no-op; const data naturally goes to flash via the SPI cache.
//
// Indexed by getTimingIndex() below. The switch returns uint8_t (not a struct) so the
// compiler generates code-based selection, not a DRAM struct-copy table.
//
// Entries with identical timing values are shared: TM1814≡TM1914, UCS8903≡UCS8904,
// APA102≡LPD8806≡P9813≡LPD6803.
static const PROGMEM WLEDpixelBus::LedTiming s_ledTimings[] = {
//            t0h   t0l   t1h   t1l  reset_us
/* 0 WS2812 */ { 300,  900,  700,  500,  100 },  // WS2812B (and 1CH_X3, 2CH_X3, WWA)
/* 1 400KHz */ { 800, 1700, 1600,  900,  300 },  // Generic 400Kbps
/* 2 TM1829 */ { 300,  900,  800,  400,  200 },  // TM1829
/* 3 UCS8x03*/ { 400,  850,  800,  450,  500 },  // UCS8903 / UCS8904 (16-bit)
/* 4 APA106 */ { 350, 1350, 1350,  350,   50 },  // APA106 / PL9823
/* 5 TM1x14 */ { 360,  890,  720,  530,  200 },  // TM1914 / TM1814 (same timing)
/* 6 SK6812 */ { 300,  900,  800,  450,  200 },  // SK6812 / SK6812 RGBW
/* 7 TM1815 */ { 740, 1780, 1440, 1060,  200 },  // TM1815
/* 8 FW1906 */ { 400,  850,  800,  450,  300 },  // FW1906 GRBCW
/* 9 WS2805 */ { 300,  790,  790,  300,  300 },  // WS2805 RGBCW
/*10 SM16825*/ { 300,  900,  900,  300,   80 },  // SM16825 (16-bit)
/*11 SPI    */ { 250,  250,  250,  250,    0 },  // APA102 / LPD8806 / P9813 / LPD6803
/*12 WS2801 */ { 500,  500,  500,  500, 1000 },  // WS2801
// TYPE_CUSTOM_BUS timing is passed directly as a LedTiming struct; no fixed entries here.
};

// Maps WLED TYPE_ to an index into s_ledTimings.
// Returns uint8_t — the compiler generates code-based selection, not a 12-byte-per-entry
// DRAM struct table.
// channels/is16bit are derived via Bus::getNumberOfChannels() / Bus::is16bit() at call site.
static inline uint8_t getTimingIndex(uint8_t wledType) {
  switch (wledType) {
    case TYPE_WS2812_RGB:
    case TYPE_WS2812_WWA:    return  0; // migration: WWA kept for timing selection only
    case TYPE_WS2811_400KHZ: return  1;
    case TYPE_TM1829:        return  2;
    case TYPE_UCS8903:
    case TYPE_UCS8904:       return  3; // identical timing
    case TYPE_APA106:        return  4;
    case TYPE_TM1914:
    case TYPE_TM1814:        return  5; // identical timing
    case TYPE_SK6812_RGBW:   return  6;
    case TYPE_TM1815:        return  7;
    case TYPE_FW1906:        return  8;
    case TYPE_WS2805:        return  9;
    case TYPE_SM16825:       return 10;
    case TYPE_APA102:
    case TYPE_LPD8806:
    case TYPE_P9813:
    case TYPE_LPD6803:       return 11; // TODO: SPI types need testing
    case TYPE_WS2801:        return 12;
    // TYPE_CUSTOM_BUS (36) timing is provided directly as a LedTiming* to create().
    default:                 return  0; // WS2812 fallback
  }
}

// Returns the LED timing for the given WLED bus type, read from flash.
// This is a one-time read at bus-creation; the bus constructor stores its own copy.
inline WLEDpixelBus::LedTiming getProtocol(uint8_t wledType) {
  return s_ledTimings[getTimingIndex(wledType)];
}

class PixelBusAllocator {
  private:
  private:
    static uint16_t _memTrackedDrivers; // track which driver types have already accounted for shared memory overhead
  #ifndef ESP8266
    static uint8_t _rmtChannelsAssigned;
    static uint8_t _rmtChannel;
    static uint8_t _i2sChannelsAssigned;
    static uint8_t _2PchannelsAssigned;
    static uint8_t _parallelI2sBusType; // Track first I2S type to enforce parallel timing
  #endif
  public:

  template <class T>
  static void begin(WLEDpixelBus::PixelBus* busPtr, uint8_t busType, uint8_t* pins, uint16_t clock_kHz) {
    if (busPtr) busPtr->begin();
  }
  static WLEDpixelBus::PixelBus* cleanup(WLEDpixelBus::PixelBus* busPtr, uint8_t busType) {
    if (busPtr) delete busPtr;
    return nullptr;
  }
  static void show(WLEDpixelBus::PixelBus* busPtr, uint8_t busType, bool isI2s = false) {
    if (busPtr) busPtr->show();
  }
  static bool canShow(WLEDpixelBus::PixelBus* busPtr, uint8_t busType) {
    if (busPtr) return busPtr->canShow();
    return true;
  }
//  static void setPixelColor(void* busPtr, uint8_t busType, uint16_t pix, uint32_t c, uint32_t cW) {
//    if (busPtr) static_cast<WLEDpixelBus::PixelBus*>(busPtr)->setPixelColor(pix, c);
//  }
  static void setBrightness(WLEDpixelBus::PixelBus* busPtr, uint8_t busType, uint8_t b) {
    // brightness is now applied per-pixel by BusDigital before calling setPixel(); this is a no-op
  }
  static uint32_t getPixelColor(WLEDpixelBus::PixelBus* busPtr, uint8_t busType, uint16_t pix, uint32_t cW) {
    if (busPtr) return busPtr->getPixelColor(pix);
    return 0;
  }

  static void resetChannelTracking() {
    _memTrackedDrivers = 0; // reset memory tracking
    #ifndef ESP8266
    _rmtChannelsAssigned = 0;
    _rmtChannel = 0;
    _i2sChannelsAssigned = 0;
    _2PchannelsAssigned = 0;
    _parallelI2sBusType = 0; // TYPE_NONE
    WLEDpixelBus::RmtBus::resetAutoChannel();
    #endif
  }

  static bool allocateHardware(uint8_t busType, const uint8_t* pins, uint8_t& driverType) {
    if (!Bus::isDigital(busType)) return false;

    if (Bus::is2Pin(busType)) {
      #ifndef ESP8266
      _2PchannelsAssigned++;
      #endif
      return true; // SPI uses separate pins
    }

    #ifndef ESP8266
    if (driverType == 0 && _rmtChannelsAssigned < WLED_MAX_RMT_CHANNELS) {
      _rmtChannelsAssigned++;
    } else if (_i2sChannelsAssigned < WLED_MAX_I2S_CHANNELS) {
      driverType = 1;
      // If first I2S channel request, lock the type to ensure parallel timings match
      if (_i2sChannelsAssigned == 0) {
        _parallelI2sBusType = busType;
      }
      _i2sChannelsAssigned++;
    } else {
      return false; // No channels available
    }
    #endif

    return true;
  }

static WLEDpixelBus::PixelBus* create(uint8_t busType, uint8_t* pins, uint16_t len, uint8_t colorOrder, uint8_t driverType = 0, uint8_t busSpeedFactor = 100, uint8_t customNumChannels = 0, const WLEDpixelBus::LedTiming* customTiming = nullptr) {
    if (!Bus::isDigital(busType)) return nullptr;

    #ifndef ESP8266
    if (driverType == 1 && _parallelI2sBusType != 0) {
      busType = _parallelI2sBusType; // Lock type for hardware timing sync
    }
    #endif

    // getProtocol() reads from a PROGMEM table (flash on ESP8266, .rodata on ESP32).
    // The timing is a one-time read at bus creation; scale to a local if needed.
    WLEDpixelBus::LedTiming timing = customTiming ? *customTiming : getProtocol(busType);
    if (busSpeedFactor != 100) {
      float factor = (float)busSpeedFactor / 100.0f;
      timing = WLEDpixelBus::scaleTiming(timing, factor);
    }

    const uint8_t numChannels = customNumChannels ? customNumChannels : (uint8_t)Bus::getNumberOfChannels(busType);

    if (Bus::is2Pin(busType)) {
      bool isHSPI = false;
      #ifdef ESP8266
      if (pins[0] == P_8266_HS_MOSI && pins[1] == P_8266_HS_CLK) isHSPI = true;
      #else
      if (_2PchannelsAssigned == 1) isHSPI = true;
      #endif
      return new WLEDpixelBus::SpiBus(pins[0], pins[1], timing, colorOrder, numChannels, isHSPI, busType); // TODO: move this into createbus function?
    }

    auto driver = WLEDpixelBus::BusDriver::Auto; // TODO: there is the get preferred bus type function but below we set default to RMT

    #ifdef ESP8266
  //  uint8_t offset = pins[0] - 1;
  //  if (pins[0] == 1 || pins[0] == 2) btype = WLEDpixelBus::BusDriver::UART; // GPIO1=TX0, GPIO2=TX1, TX0 is used for debug if enabled  TODO: there is a bug, TX1 only works after saving, not after reboot, needs investigation, use bitbanging for now
    //else if (offset == 2) btype = WLEDpixelBus::BusDriver::DMA; // DMA method uses a lot of RAM!
    //else btype = WLEDpixelBus::BusDriver::BitBang;
    driver = WLEDpixelBus::BusDriver::BitBang; // bit banging works on all pins and uses less memory
    #else
    switch (driverType) {
      case 0:  driver = WLEDpixelBus::BusDriver::RMT; break;
      case 1:
        #if defined(CONFIG_IDF_TARGET_ESP32S3)
        driver = WLEDpixelBus::BusDriver::LCD; // S3 has LCD peripheral with 16 channels, very similar to I2S
        #elif defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
        driver = WLEDpixelBus::BusDriver::I2S;
        #elif defined(CONFIG_IDF_TARGET_ESP32C3)
        driver = WLEDpixelBus::BusDriver::SPI; // parallel SPI
        #else
        driver = WLEDpixelBus::BusDriver::RMT;
        #endif
        break;
      default: driver = WLEDpixelBus::BusDriver::RMT; break;
    }
    #endif

    #ifndef CONFIG_IDF_TARGET_ESP32C3
    #define RMT_USE_SINGLE_MEM_BLOCK // TODO: hi priority RMT driver does not yet support multi-memory block
    #endif

    int8_t rmtCh = -1; // -1 means auto select by RMT driver and optimize memory block use, if RMT RX is needed, define RMT_USE_SINGLE_MEM_BLOCK
    #ifndef ESP8266
    if (driver == WLEDpixelBus::BusDriver::RMT) {
      if (_rmtChannel < WLED_MAX_RMT_CHANNELS) {
        #ifdef RMT_USE_SINGLE_MEM_BLOCK
        rmtCh = _rmtChannel++; // assign channels in order, do not use auto-channel function (this uses 1 memory block per channel allowing RX RMT channels to be used as well)
        #else
        _rmtChannel++; // increment channel count for tracking, but use auto-channel to optimize memory block allocation
        #endif
      } else {
        return nullptr;
      }
    }
    #endif

    WLEDpixelBus::PixelBus* bus = WLEDpixelBus::createBus(driver, pins[0], timing, colorOrder, numChannels, WLEDpixelBus::DEFAULT_DMA_BUFFER_SIZE, rmtCh, busType);

    // TM1814/TM1815: inject the 8-byte current-config prefix (C1 + C2).
    // setPrefix() must be called before begin() so allocateEncodeBuffer() reserves space.
    if (bus && (busType == TYPE_TM1814 || busType == TYPE_TM1815)) {
      bus->setPrefixLen(8); // reserve 8-byte C1+C2 prefix; actual bytes written per-frame in BusDigital::show()
      bus->setInverted(true); // invert the output signal
    }
    if (bus && busType == TYPE_TM1914) {
      bus->setPrefixLen(6); // reserve 6-byte mode prefix; bytes written once in BusDigital::begin()
      bus->setInverted(true); // invert the output signal
    }
    if (bus && busType == TYPE_SM16825) {
      bus->setSuffixLen(4); // reserve 4-byte per-frame configuration suffix; default written by allocateEncodeBuffer
    }

    return bus;
  }
};
#endif

