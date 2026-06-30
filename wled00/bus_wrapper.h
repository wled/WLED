#pragma once
#ifndef BusWrapper_h
#define BusWrapper_h

#include "src/WLEDpixelBus/WLEDpixelBus.h"
#include "src/WLEDpixelBus/WLEDpixelBus_SPI.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "src/WLEDpixelBus/WLEDpixelBus_RMT.h"
#include "src/WLEDpixelBus/WLEDpixelBus_I2S.h"
#include "src/WLEDpixelBus/WLEDpixelBus_ParallelSpi.h"
#elif defined(ARDUINO_ARCH_ESP8266)
#include "src/WLEDpixelBus/WLEDpixelBus_ESP8266.h"
#include "src/WLEDpixelBus/WLEDpixelBus_BitBang.h"
#endif

//Hardware SPI Pins (ESP8266 only; ESP32 uses bus allocation order to detect HSPI)
#define P_8266_HS_MOSI 13
#define P_8266_HS_CLK  14

// Use single RMT memory block per channel — allows RMT RX channels alongside TX.
//#define RMT_USE_SINGLE_MEM_BLOCK


class PixelBusAllocator {
  private:
  #ifndef ESP8266
    static uint8_t _rmtChannelsAssigned;
    static uint8_t _i2sChannelsAssigned;
    static uint8_t _parallelI2sBusType; // Track first I2S type to enforce parallel timing
    static uint8_t _bitBangChannelsAssigned;
    static uint8_t _bitBangBusType;    // Track first BitBang type to enforce parallel timing
    static uint8_t _hardwareSPIused;   // number of hardware SPI's used, currently only one SPI output is supported. On C3, parallel SPI output takes priority
  #else
    static uint8_t _bitBangBusType;    // Track first ESP8266 BitBang type to enforce parallel timing
  #endif
  public:

  static void resetChannelTracking() {
    #ifndef ESP8266
    _rmtChannelsAssigned = 0;
    _i2sChannelsAssigned = 0;
    _parallelI2sBusType = 0; // TYPE_NONE
    _bitBangChannelsAssigned = 0;
    _bitBangBusType = 0; // TYPE_NONE
    _hardwareSPIused = 0;
    WLEDpixelBus::RmtBus::resetAutoChannel();
    #if (WLED_MAX_BB_CHANNELS > 0)
    WLEDpixelBus::BitBangBus::resetChannels();
    #endif
    #else
    _bitBangBusType = 0; // TYPE_NONE
    WLEDpixelBus::BitBangBus::resetChannels();
    #endif
  }

  static bool allocateHardware(uint8_t busType, const uint8_t* pins, uint8_t& driverType) {
    if (!Bus::isDigital(busType)) return false;

    if (Bus::is2Pin(busType)) {
      // TODO: could check if an SPI is still available and set _hardwareSPIused to 1 to prevent hardware collision
      // note: SPI is intentionally not reserved here as only one is supported and first come first serve is used in create()
      return true; // for now, allow as many SPI buses as the UI allows. First one uses hardware SPI if available (on C3, if a parallel SPI output is used it takes priority)
    }

    #ifndef ESP8266
    if (driverType == BUSDRV_RMT && _rmtChannelsAssigned < WLED_MAX_RMT_CHANNELS) {
      _rmtChannelsAssigned++;
    } else if (driverType == BUSDRV_BITBANG) {
      // BitBang: no hardware channel limit, but enforce single LED type for parallel output
      if (_bitBangBusType == 0) {
        _bitBangBusType = busType; // lock LED type to first BitBang channel
      } else if (_bitBangBusType != busType) {
        return false; // mismatched LED type — all BitBang channels must share timing
      }
      _bitBangChannelsAssigned++;
    } else if (_i2sChannelsAssigned < WLED_MAX_I2S_CHANNELS) {
      driverType = BUSDRV_I2S;
      // If first I2S channel request, lock the type to ensure parallel timings match
      if (_i2sChannelsAssigned == 0) {
        _parallelI2sBusType = busType;
        #ifdef CONFIG_IDF_TARGET_ESP32C3
        _hardwareSPIused++; // reserve SPI: C3 uses prallel SPI output for "I2S" and takes priority over 2pin buses
        #endif
      }
      _i2sChannelsAssigned++;
    } else {
      return false; // No channels available
    }
    #else
    // ESP8266: assign driverType based on pin number so BusManager::show() can sequence correctly.
    // GPIO1/2 → UART (async, fire-and-forget ISR)
    // GPIO3   → DMA  (async, I2S SLC DMA)
    // others  → BitBang (interrupt-blocking — must run before async buses)
    if (pins[0] == 1 || pins[0] == 2) {
      driverType = BUSDRV_RMT; // reuse BUSDRV_RMT as "async UART" sentinel on ESP8266
    } else if (pins[0] == 3) {
      driverType = BUSDRV_I2S; // async DMA
    } else {
      driverType = BUSDRV_BITBANG;
      // Enforce single LED type for parallel timing
      if (_bitBangBusType == 0) {
        _bitBangBusType = busType;
      } else if (_bitBangBusType != busType) {
        return false; // mismatched LED type — all ESP8266 BitBang channels must share timing
      }
    }
    #endif

    return true;
  }

static WLEDpixelBus::PixelBus* create(uint8_t busType, uint8_t* pins, uint16_t len, uint8_t colorOrder, uint8_t driverType = BUSDRV_RMT, uint8_t busSpeedFactor = 100, uint8_t customNumChannels = 0, const WLEDpixelBus::LedTiming* customTiming = nullptr) {
    if (!Bus::isDigital(busType)) return nullptr;

    #ifndef ESP8266
    if (driverType == BUSDRV_I2S && _parallelI2sBusType != 0) {
      busType = _parallelI2sBusType; // Lock type for hardware timing sync
    }
    if (driverType == BUSDRV_BITBANG && _bitBangBusType != 0) {
      busType = _bitBangBusType; // Lock type for BitBang parallel timing
    }
    #endif

    // getProtocol() reads from a PROGMEM table (flash on ESP8266, .rodata on ESP32).
    // The timing is a one-time read at bus creation; scale to a local if needed.
    WLEDpixelBus::LedTiming timing = customTiming ? *customTiming : WLEDpixelBus::getProtocol(busType);
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
      if (_hardwareSPIused == 0) {
        isHSPI = true;
        _hardwareSPIused++; // claim hardware SPI (currently only one is supported), on C3 this can also be claimed by parallel SPI (done so in allocateHardware)
      }
      #endif
      return new WLEDpixelBus::SpiBus(pins[0], pins[1], timing, colorOrder, numChannels, isHSPI, busType); // TODO: move this into createbus function?
    }

    WLEDpixelBus::BusDriver driver = WLEDpixelBus::BusDriver::RMT; // always overwritten below; initialised to avoid unused-variable warning

    #ifdef ESP8266
  //  uint8_t offset = pins[0] - 1;
    if (pins[0] == 1 || pins[0] == 2) driver = WLEDpixelBus::BusDriver::UART; // GPIO1=TX0, GPIO2=TX1, TX0 is used for debug if enabled  TODO: there is a bug, TX1 only works after saving, not after reboot, needs investigation, use bitbanging for now
    else if (pins[0] == 3) driver = WLEDpixelBus::BusDriver::DMA; // DMA method uses a lot of RAM!
    else driver = WLEDpixelBus::BusDriver::BitBang;
    //driver = WLEDpixelBus::BusDriver::BitBang; // bit banging works on all pins (debug)
    #else
    switch (driverType) {
      case BUSDRV_RMT:     driver = WLEDpixelBus::BusDriver::RMT; break;
      case BUSDRV_I2S:
        #if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
        driver = WLEDpixelBus::BusDriver::I2S; // uses LCD hardware on S3 no actually I2S
        #elif defined(CONFIG_IDF_TARGET_ESP32C3)
        driver = WLEDpixelBus::BusDriver::SPI; // parallel SPI
        #else
        driver = WLEDpixelBus::BusDriver::RMT;
        #endif
        break;
      case BUSDRV_BITBANG: driver = WLEDpixelBus::BusDriver::BitBang; break;
      default:             driver = WLEDpixelBus::BusDriver::RMT; break;
    }
    #endif

    // Chip-specific init (prefix/suffix/invert) is applied inside createBus() using ledType.
    return WLEDpixelBus::createBus(driver, pins[0], timing, colorOrder, numChannels, busType, len);
  }
};
#endif

