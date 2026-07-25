#pragma once
#ifndef BusWrapper_h
#define BusWrapper_h

#include "src/WLEDpixelBus/WLEDpixelBus.h"
#include "src/WLEDpixelBus/WLEDpixelBus_SPI.h"

#if defined(ARDUINO_ARCH_ESP32)
#include "src/WLEDpixelBus/WLEDpixelBus_RMT.h"
#include "src/WLEDpixelBus/WLEDpixelBus_I2S.h"
#include "src/WLEDpixelBus/WLEDpixelBus_ParallelSpi.h"
#include "src/WLEDpixelBus/WLEDpixelBus_PARLIO.h"
#include "src/WLEDpixelBus/WLEDpixelBus_BitBang.h"
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
    static uint8_t _parHwChannelsAssigned;    // parallel output channels: I2S/LCD (ESP32/S2/S3), parallel SPI (C3), PARLIO (C6/H2/C5/P4)
    static uint8_t _parHwBusType; // Track first parallel bus type to enforce parallel timing
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
    _parHwChannelsAssigned = 0;
    _parHwBusType = 0; // TYPE_NONE
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
    // Driver fallback order: requested driver -> parallel bus (I2S/LCD/SPI/PARLIO) -> BitBang.
    // If the requested driver has no free channel (or is unsupported on this target), the next
    // available driver is used instead.
    if (driverType == BUSDRV_RMT && _rmtChannelsAssigned < WLED_MAX_RMT_CHANNELS) {
      _rmtChannelsAssigned++;
      return true;
    }
    if (driverType != BUSDRV_BITBANG && _parHwChannelsAssigned < WLED_MAX_PARHW_CHANNELS) {
      // BUSDRV_PARHW maps to the chip's parallel output peripheral: I2S on ESP32/S2, LCD on S3, parallel SPI on C3, PARLIO on C6/H2/C5/P4, all use 4-step cadence
      driverType = BUSDRV_PARHW;
      if (_parHwChannelsAssigned == 0) {
        _parHwBusType = busType; // lock LED type to first parallel channel
        #ifdef CONFIG_IDF_TARGET_ESP32C3
        _hardwareSPIused++; // reserve SPI: C3 uses parallel SPI output for "I2S" and takes priority over 2pin buses
        #endif
      }
      _parHwChannelsAssigned++;
      return true;
    }
    // Last resort (or explicitly requested): parallel BitBang, all channels share one timing
    if (_bitBangChannelsAssigned < WLED_MAX_BB_CHANNELS) {
      driverType = BUSDRV_BITBANG;
      if (_bitBangBusType == 0) {
        _bitBangBusType = busType; // lock LED type to first BitBang channel
      } else if (_bitBangBusType != busType) {
        return false; // mismatched LED type — all BitBang channels must share timing
      }
      _bitBangChannelsAssigned++;
      return true;
    }
    return false; // No channels available
    #else
    // ESP8266: assign driverType based on pin number so BusManager::show() can sequence correctly.
    // GPIO1/2 → UART (async, fire-and-forget ISR)
    // GPIO3   → DMA  (async, I2S SLC DMA)
    // others  → BitBang (interrupt-blocking — must run before async buses)
    if (pins[0] == 1 || pins[0] == 2) {
      driverType = BUSDRV_RMT; // reuse BUSDRV_RMT as "async UART" sentinel on ESP8266
    } else if (pins[0] == 3) {
      driverType = BUSDRV_PARHW; // async DMA
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

static WLEDpixelBus::PixelBus* create(uint8_t busType, uint8_t* pins, uint16_t len, uint8_t colorOrder, uint8_t driverType = BUSDRV_RMT, uint8_t busSpeedFactor = 100, uint16_t frequencykHz = 0, uint8_t customNumChannels = 0, const WLEDpixelBus::LedTiming* customTiming = nullptr) {
    if (!Bus::isDigital(busType)) return nullptr;

    #ifndef ESP8266
    if (driverType == BUSDRV_PARHW && _parHwBusType != 0) {
      busType = _parHwBusType; // use the locked in bus type
    }
    #endif
    if (driverType == BUSDRV_BITBANG && _bitBangBusType != 0) {
      busType = _bitBangBusType; // use the locked in bus type
    }

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
      return new WLEDpixelBus::SpiBus(pins[0], pins[1], frequencykHz, colorOrder, numChannels, isHSPI, busType); // TODO: move this into createbus function?
    }

    WLEDpixelBus::BusDriver driver = WLEDpixelBus::BusDriver::RMT; // always overwritten below; initialised to avoid unused-variable warning

    #ifdef ESP8266
    if (pins[0] == 1 || pins[0] == 2) driver = WLEDpixelBus::BusDriver::UART; // GPIO1=TX0, GPIO2=TX1, TX0 is used for debug if enabled
    else if (pins[0] == 3) driver = WLEDpixelBus::BusDriver::DMA; // DMA method uses a lot of RAM!
    else driver = WLEDpixelBus::BusDriver::BitBang;
    #elif !defined(CONFIG_IDF_TARGET_ESP32C61)
    switch (driverType) {
      case BUSDRV_RMT:
        driver = WLEDpixelBus::BusDriver::RMT;
        break;
      case BUSDRV_PARHW:
        #if defined(CONFIG_IDF_TARGET_ESP32C3)  //TODO: should use hardware capabilities instead of MCU types
        driver = WLEDpixelBus::BusDriver::SPI; // parallel SPI on C3
        #elif defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32P4)
        driver = WLEDpixelBus::BusDriver::PARLIO; // PARLIO on C5, C6, H2, P4
        #else
        driver = WLEDpixelBus::BusDriver::I2S; // note: on S3 this uses LCD hardware
        #endif
        break;
      default:
        driver = WLEDpixelBus::BusDriver::BitBang;
        break;
    }
    #else
    // C61 only supports BB for now (might be able to use parallel SPI), it has no RMT, no I2S, no PARLIO hardware
    driver = WLEDpixelBus::BusDriver::BitBang;
    #endif

    // Chip-specific init (prefix/suffix/invert) is applied inside createBus() using ledType.
    return WLEDpixelBus::createBus(driver, pins[0], timing, colorOrder, numChannels, busType, len);
  }
};
#endif

