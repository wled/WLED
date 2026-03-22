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
//    if (busPtr) static_cast<WLEDpixelBus::IBus*>(busPtr)->setPixelColor(pix, c);
//  }
  static void setBrightness(WLEDpixelBus::PixelBus* busPtr, uint8_t busType, uint8_t b) {
    if (busPtr) busPtr->setBrightness(b);
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

static WLEDpixelBus::PixelBus* create(uint8_t busType, uint8_t* pins, uint16_t len, WLEDpixelBus::ColorOrder colorOrder, uint8_t driverType = 0, uint8_t busSpeedFactor = 100) {
    if (!Bus::isDigital(busType)) return nullptr;

    #ifndef ESP8266
    if (driverType == 1 && _parallelI2sBusType != 0) {
      busType = _parallelI2sBusType; // Lock type for hardware timing sync
    }
    #endif

    ProtocolDef proto = getProtocol(busType);
    // Apply optional bus speed factor (percent)
    WLEDpixelBus::LedTiming timing = proto.timing;
    if (busSpeedFactor != 100) {
      float factor = (float)busSpeedFactor / 100.0f;
      timing = WLEDpixelBus::scaleTiming(proto.timing, factor);
    }

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
      return new WLEDpixelBus::SpiBus(pins[0], pins[1], timing, finalOrder, isHSPI); // TODO: move this into createbus function?
    }

    auto btype = WLEDpixelBus::BusType::Auto; // TODO: there is the get preferred bus type function but below we set default to RMT

    #ifdef ESP8266
  //  uint8_t offset = pins[0] - 1;
  //  if (pins[0] == 1 || pins[0] == 2) btype = WLEDpixelBus::BusType::UART; // GPIO1=TX0, GPIO2=TX1, TX0 is used for debug if enabled  TODO: there is a bug, TX1 only works after saving, not after reboot, needs investigation, use bitbanging for now
    //else if (offset == 2) btype = WLEDpixelBus::BusType::DMA; // DMA method uses a lot of RAM!
    //else btype = WLEDpixelBus::BusType::BitBang;
    btype = WLEDpixelBus::BusType::BitBang; // bit banging works on all pins and uses less memory
    #else
    switch (driverType) {
      case 0:  btype = WLEDpixelBus::BusType::RMT; break;
      case 1:
        #if defined(CONFIG_IDF_TARGET_ESP32S3)
        btype = WLEDpixelBus::BusType::LCD; // S3 has LCD peripheral with 16 channels, very similar to I2S
        #elif defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S2)
        btype = WLEDpixelBus::BusType::I2S;
        #elif defined(CONFIG_IDF_TARGET_ESP32C3)
        btype = WLEDpixelBus::BusType::SPI; // parallel SPI
        #else
        btype = WLEDpixelBus::BusType::RMT;
        #endif
        break;
      default: btype = WLEDpixelBus::BusType::RMT; break;
    }
    #endif

    int8_t rmtCh = -1; // -1 means auto select by RMT driver and optimize memory block use, if RMT RX is needed, define RMT_USE_SINGLE_MEM_BLOCK
    #ifndef ESP8266
    if (btype == WLEDpixelBus::BusType::RMT) {
      if (_rmtChannel < WLED_MAX_RMT_CHANNELS) {
        #ifdef RMT_USE_SINGLE_MEM_BLOCK
        rmtCh = _rmtChannel++; // assign channels in order, do not use auto-channel function (this uses 1 memory block per channel allowing RX RMT channels to be used as well)
        #else
        _rmtChannel++; // increment channel count for tracking, but use auto-channel  to optimize memory block allocation
        #endif
      } else {
        return nullptr;
      }
    }
    #endif

    return WLEDpixelBus::createBus(btype, pins[0], timing, finalOrder, WLEDpixelBus::DEFAULT_DMA_BUFFER_SIZE, rmtCh);
  }

  static unsigned memUsage(uint8_t busType, unsigned count, const uint8_t* pins, unsigned driverType = 0) {
    if (!Bus::isDigital(busType)) return 0;

    #ifndef ESP8266
    if (driverType == 1 && _parallelI2sBusType != 0) {
      busType = _parallelI2sBusType; // Ensure memory matches the hardware configuration
    }
    #endif
    ProtocolDef proto = getProtocol(busType);

    // base PixelBus allocates a 4-byte _pixelData array per pixel
    unsigned mem = count * 4;
    // and a 2-byte _cctData array per pixel if CCT is enabled
    if (Bus::hasCCT(busType)) mem += count * 2;

    if (Bus::is2Pin(busType)) {
      // Basic buffer overhead for SPI
      bool isFirst = !(_memTrackedDrivers & (1 << (uint8_t)WLEDpixelBus::BusType::SPI));
      _memTrackedDrivers |= (1 << (uint8_t)WLEDpixelBus::BusType::SPI);
      
      #if defined(CONFIG_IDF_TARGET_ESP32C3) && defined(WLEDPB_PARALLEL_SPI_SUPPORT)
      return mem + WLEDpixelBus::ParallelSpiBus::estimateMemory(count, proto.channels, isFirst);
      #else
      return mem + WLEDpixelBus::estimateMemory(WLEDpixelBus::BusType::SPI, count, proto.channels); // fallback to base estimate
      #endif
    }

    auto btype = WLEDpixelBus::BusType::Auto;

    #ifdef ESP8266
      btype = WLEDpixelBus::BusType::BitBang;
      //uint8_t offset = (pins && pins[0] >= 1) ? (pins[0] - 1) : 255;
      //if (pins[0] == 1 || pins[0] == 2)  btype = WLEDpixelBus::BusType::UART;
      //else if (offset == 2)            btype = WLEDpixelBus::BusType::DMA;
      //else                             btype = WLEDpixelBus::BusType::BitBang;

      bool isFirst = !(_memTrackedDrivers & (1 << (uint8_t)btype));
      _memTrackedDrivers |= (1 << (uint8_t)btype);

      switch (btype) {
        case WLEDpixelBus::BusType::UART: return mem + WLEDpixelBus::Esp8266UartBus::estimateMemory(count, proto.channels, isFirst);
        case WLEDpixelBus::BusType::DMA:  return mem + WLEDpixelBus::Esp8266DmaBus::estimateMemory(count, proto.channels, isFirst);
        default:                          return mem + WLEDpixelBus::Esp8266BitBangBus::estimateMemory(count, proto.channels, isFirst);
      }
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

      bool isFirst = !(_memTrackedDrivers & (1 << (uint8_t)btype));
      _memTrackedDrivers |= (1 << (uint8_t)btype);

      switch (btype) {
        #ifdef WLEDPB_LCD_SUPPORT
        case WLEDpixelBus::BusType::LCD: return mem + WLEDpixelBus::LcdBus::estimateMemory(count, proto.channels, isFirst);
        #endif
        #ifdef WLEDPB_I2S_SUPPORT
        case WLEDpixelBus::BusType::I2S: return mem + WLEDpixelBus::I2sBus::estimateMemory(count, proto.channels, isFirst);
        #endif
        #ifdef WLEDPB_PARALLEL_SPI_SUPPORT
        case WLEDpixelBus::BusType::SPI: return mem + WLEDpixelBus::ParallelSpiBus::estimateMemory(count, proto.channels, isFirst);
        #endif
        default: return mem + WLEDpixelBus::RmtBus::estimateMemory(count, proto.channels, isFirst);
      }
    #endif
  }
};
#endif
