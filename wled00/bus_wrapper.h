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

//Hardware SPI Pins (ESP8266 only; ESP32 uses bus allocation order to detect HSPI)
#define P_8266_HS_MOSI 13
#define P_8266_HS_CLK  14

// Use single RMT memory block per channel — allows RMT RX channels alongside TX.
// TODO: hi priority RMT driver does not yet support multi-memory block
#ifndef CONFIG_IDF_TARGET_ESP32C3
  #define RMT_USE_SINGLE_MEM_BLOCK
#endif

class PixelBusAllocator {
  private:
  #ifndef ESP8266
    static uint8_t _rmtChannelsAssigned;
    static uint8_t _rmtChannel;
    static uint8_t _i2sChannelsAssigned;
    static uint8_t _2PchannelsAssigned;
    static uint8_t _parallelI2sBusType; // Track first I2S type to enforce parallel timing
  #endif
  public:

  static void resetChannelTracking() {
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
      if (_2PchannelsAssigned == 1) isHSPI = true;
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

    int8_t rmtCh = -1; // -1 = auto-select; file-scope RMT_USE_SINGLE_MEM_BLOCK switches to sequential assignment
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

    // Chip-specific init (prefix/suffix/invert) is applied inside createBus() using ledType.
    return WLEDpixelBus::createBus(driver, pins[0], timing, colorOrder, numChannels, WLEDpixelBus::DEFAULT_DMA_BUFFER_SIZE, rmtCh, busType);
  }
};
#endif

