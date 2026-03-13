#pragma once
#ifndef BusWrapper_h
#define BusWrapper_h

//#define NPB_CONF_4STEP_CADENCE
#include "src/WLEDpixelBus/WLEDpixelBus.h"
#include "src/WLEDpixelBus/WLEDpixelBus_SPI.h"
#include "src/WLEDpixelBus/WLEDpixelBus_RMT.h"
#include "src/WLEDpixelBus/WLEDpixelBus_I2S.h"
#include "src/WLEDpixelBus/WLEDpixelBus_LCD.h"
#include "src/WLEDpixelBus/WLEDpixelBus_ParallelSpi.h"

//Hardware SPI Pins
#define P_8266_HS_MOSI 13
#define P_8266_HS_CLK  14
#define P_32_HS_MOSI   13
#define P_32_HS_CLK    14
#define P_32_VS_MOSI   23
#define P_32_VS_CLK    18

struct ProtocolDef {
  WLEDpixelBus::LedTiming timing;
  uint8_t channels;
  bool is16bit;
};

// Maps WLED's internal TYPE_ to actual timings and capabilities
inline ProtocolDef getProtocol(uint8_t wledType) {
  switch (wledType) {
    // 3 channels
    case TYPE_WS2812_1CH_X3:
    case TYPE_WS2812_2CH_X3:
    case TYPE_WS2812_RGB:
    case TYPE_WS2812_WWA:   return { WLEDpixelBus::Timing::WS2812, 3, false };
    case TYPE_WS281X_FAST:  return { WLEDpixelBus::Timing::WS281x_FAST, 3, false };
    case TYPE_WS2811_400KHZ:return { WLEDpixelBus::Timing::Generic400Kbps, 3, false };
    case TYPE_TM1829:       return { WLEDpixelBus::Timing::TM1829, 3, false };
    case TYPE_UCS8903:      return { WLEDpixelBus::Timing::UCS8903, 3, true }; // 16bit
    case TYPE_APA106:       return { WLEDpixelBus::Timing::APA106, 3, false };
    case TYPE_TM1914:       return { WLEDpixelBus::Timing::TM1914, 3, false };

    // 4 channels
    case TYPE_SK6812_RGBW:  return { WLEDpixelBus::Timing::SK6812, 4, false };
    case TYPE_TM1814:       return { WLEDpixelBus::Timing::TM1814, 4, false };
    case TYPE_UCS8904:      return { WLEDpixelBus::Timing::UCS8904, 4, true }; // 16bit
    case TYPE_TM1815:       return { WLEDpixelBus::Timing::TM1815, 4, false };

    // 5 channels
    case TYPE_FW1906:       return { WLEDpixelBus::Timing::FW1906, 5, false };
    case TYPE_WS2805:       return { WLEDpixelBus::Timing::WS2805, 5, false };
    case TYPE_SM16825:      return { WLEDpixelBus::Timing::SM16825, 5, true }; // 16bit
    
    // SPI specific
    case TYPE_APA102:       return { WLEDpixelBus::Timing::APA102, 3, false };
    case TYPE_LPD8806:      return { WLEDpixelBus::Timing::LPD8806, 3, false };
    case TYPE_WS2801:       return { WLEDpixelBus::Timing::WS2801, 3, false };
    case TYPE_P9813:        return { WLEDpixelBus::Timing::P9813, 3, false };
    case TYPE_LPD6803:      return { WLEDpixelBus::Timing::LPD6803, 3, false };

    default:                return { WLEDpixelBus::Timing::WS2812, 3, false };
  }
}

class PolyBus {
  private:
  #ifndef ESP8266
    static uint8_t _rmtChannelsAssigned;
    static uint8_t _rmtChannel;           
    static uint8_t _i2sChannelsAssigned;
    static uint8_t _2PchannelsAssigned;  
    static uint8_t _parallelI2sBusType; // Track first I2S type to enforce parallel timing
  #endif
  public:

  template <class T>
  static void begin(void* busPtr, uint8_t busType, uint8_t* pins, uint16_t clock_kHz) {
    if (busPtr) static_cast<WLEDpixelBus::IBus*>(busPtr)->begin();
  }
  static void* cleanup(void* busPtr, uint8_t busType) {
    if (busPtr) delete static_cast<WLEDpixelBus::IBus*>(busPtr);
    return nullptr;
  }
  static void show(void* busPtr, uint8_t busType, bool isI2s = false) {
    if (busPtr) static_cast<WLEDpixelBus::IBus*>(busPtr)->show();
  }
  static bool canShow(void* busPtr, uint8_t busType) {
    if (busPtr) return static_cast<WLEDpixelBus::IBus*>(busPtr)->canShow();
    return true;
  }
  static void setPixelColor(void* busPtr, uint8_t busType, uint16_t pix, uint32_t c, uint32_t cW) {
    if (busPtr) static_cast<WLEDpixelBus::IBus*>(busPtr)->setPixelColor(pix, c);
  }
  static void setBrightness(void* busPtr, uint8_t busType, uint8_t b) {
    if (busPtr) static_cast<WLEDpixelBus::IBus*>(busPtr)->setBrightness(b);
  }
  static uint32_t getPixelColor(void* busPtr, uint8_t busType, uint16_t pix, uint32_t cW) {
    if (busPtr) return static_cast<WLEDpixelBus::IBus*>(busPtr)->getPixelColor(pix);
    return 0;
  }

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

static WLEDpixelBus::IBus* create(uint8_t busType, uint8_t* pins, uint16_t len, WLEDpixelBus::ColorOrder colorOrder, uint8_t driverType = 0) {
    if (!Bus::isDigital(busType)) return nullptr;

    #ifndef ESP8266
    if (driverType == 1 && _parallelI2sBusType != 0) {
      busType = _parallelI2sBusType; // Lock type for hardware timing sync
    }
    #endif

    ProtocolDef proto = getProtocol(busType);

    // Map WLED Order
    uint8_t wledOrder = (uint8_t)colorOrder & 0x0F;
    WLEDpixelBus::ColorOrder finalOrder = WLEDpixelBus::ColorOrder::GRB;
    
    if (proto.channels >= 5) {
      switch (wledOrder) {
        case 0: finalOrder = WLEDpixelBus::ColorOrder::GRBWC; break;
        case 1: finalOrder = WLEDpixelBus::ColorOrder::RGBWC; break;
        case 2: finalOrder = WLEDpixelBus::ColorOrder::BRGWC; break;
        case 3: finalOrder = WLEDpixelBus::ColorOrder::RBGWC; break;
        case 4: finalOrder = WLEDpixelBus::ColorOrder::BGRWC; break;
        case 5: finalOrder = WLEDpixelBus::ColorOrder::GBRWC; break;
        default: finalOrder = WLEDpixelBus::ColorOrder::GRBWC; break;
      }
    } else if (proto.channels == 4) {
      switch (wledOrder) {
        case 0: finalOrder = WLEDpixelBus::ColorOrder::GRBW; break;
        case 1: finalOrder = WLEDpixelBus::ColorOrder::RGBW; break;
        case 2: finalOrder = WLEDpixelBus::ColorOrder::BRGW; break;
        case 3: finalOrder = WLEDpixelBus::ColorOrder::RBGW; break;
        case 4: finalOrder = WLEDpixelBus::ColorOrder::BGRW; break;
        case 5: finalOrder = WLEDpixelBus::ColorOrder::GBRW; break;
        default: finalOrder = WLEDpixelBus::ColorOrder::GRBW; break;
      }
    } else {
      switch (wledOrder) {
        case 0: finalOrder = WLEDpixelBus::ColorOrder::GRB; break;
        case 1: finalOrder = WLEDpixelBus::ColorOrder::RGB; break;
        case 2: finalOrder = WLEDpixelBus::ColorOrder::BRG; break;
        case 3: finalOrder = WLEDpixelBus::ColorOrder::RBG; break;
        case 4: finalOrder = WLEDpixelBus::ColorOrder::BGR; break;
        case 5: finalOrder = WLEDpixelBus::ColorOrder::GBR; break;
        default: finalOrder = WLEDpixelBus::ColorOrder::GRB; break;
      }
    }

    if (Bus::is2Pin(busType)) {
      bool isHSPI = false;
      #ifdef ESP8266
      if (pins[0] == P_8266_HS_MOSI && pins[1] == P_8266_HS_CLK) isHSPI = true;
      #else
      if (_2PchannelsAssigned == 1) isHSPI = true;
      #endif
      return new WLEDpixelBus::SpiBus(pins[0], pins[1], proto.timing, finalOrder, isHSPI);
    }

    auto btype = WLEDpixelBus::BusType::Auto;
    uint8_t hwIndex = pins[0]; 
    
    #ifdef ESP8266
    uint8_t offset = pins[0] - 1; 
    if (offset == 0 || offset == 1) btype = WLEDpixelBus::BusType::UART;
    else if (offset == 2) btype = WLEDpixelBus::BusType::DMA;
    else btype = WLEDpixelBus::BusType::BitBang;
    #else
    switch (driverType) {
      case 0:  btype = WLEDpixelBus::BusType::RMT; break;
      case 1:
        #if defined(CONFIG_IDF_TARGET_ESP32S3)
        btype = WLEDpixelBus::BusType::LCD;
        #elif defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
        btype = WLEDpixelBus::BusType::I2S;
        #elif defined(CONFIG_IDF_TARGET_ESP32C3)
        btype = WLEDpixelBus::BusType::SPI;
        #else
        btype = WLEDpixelBus::BusType::RMT;
        #endif
        break;
      default: btype = WLEDpixelBus::BusType::RMT; break;
    }
    #endif

    int8_t rmtCh = -1;
    #ifndef ESP8266
    if (btype == WLEDpixelBus::BusType::RMT) {
      if (_rmtChannel < WLED_MAX_RMT_CHANNELS) {
        rmtCh = _rmtChannel++;
      } else {
        return nullptr;
      }
    }
    #endif

    return WLEDpixelBus::createBus(btype, hwIndex, proto.timing, finalOrder, WLEDpixelBus::DEFAULT_DMA_BUFFER_SIZE, rmtCh);
  }

  static unsigned memUsage(uint8_t busType, unsigned count, const uint8_t* pins, unsigned driverType = 0) {
    if (!Bus::isDigital(busType)) return 0;

    #ifndef ESP8266
    if (driverType == 1 && _parallelI2sBusType != 0) {
      busType = _parallelI2sBusType; // Ensure memory matches the hardware configuration
    }
    #endif

    ProtocolDef proto = getProtocol(busType);

    if (Bus::is2Pin(busType)) {
      // Basic buffer overhead for SPI
      return WLEDpixelBus::estimateMemory(WLEDpixelBus::BusType::Auto, count, proto.channels);
    }

    auto btype = WLEDpixelBus::BusType::Auto;

    #ifdef ESP8266
      btype = WLEDpixelBus::BusType::Auto;
    #else
      switch (driverType) {
        case 0: btype = WLEDpixelBus::BusType::RMT; break;
        case 1:
          #if defined(CONFIG_IDF_TARGET_ESP32S3)
          btype = WLEDpixelBus::BusType::LCD;
          #elif defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
          btype = WLEDpixelBus::BusType::I2S;
          #elif defined(CONFIG_IDF_TARGET_ESP32C3)
          btype = WLEDpixelBus::BusType::SPI;
          #else
          btype = WLEDpixelBus::BusType::RMT;
          #endif
          break;
        default: btype = WLEDpixelBus::BusType::RMT; break;
      }
    #endif

    return WLEDpixelBus::estimateMemory(btype, count, proto.channels);
  }
};
#endif
