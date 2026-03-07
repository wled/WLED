#pragma once
#ifndef BusWrapper_h
#define BusWrapper_h

//#define NPB_CONF_4STEP_CADENCE
#include "src/WLEDpixelBus/WLEDpixelBus.h"

//Hardware SPI Pins
#define P_8266_HS_MOSI 13
#define P_8266_HS_CLK  14
#define P_32_HS_MOSI   13
#define P_32_HS_CLK    14
#define P_32_VS_MOSI   23
#define P_32_VS_CLK    18

//The dirty list of possible bus types. Quite a lot...
#define I_NONE 0
//ESP8266 RGB
#define I_8266_U0_NEO_3 1
#define I_8266_U1_NEO_3 2
#define I_8266_DM_NEO_3 3
#define I_8266_BB_NEO_3 4
//RGBW
#define I_8266_U0_NEO_4 5
#define I_8266_U1_NEO_4 6
#define I_8266_DM_NEO_4 7
#define I_8266_BB_NEO_4 8
//400Kbps
#define I_8266_U0_400_3 9
#define I_8266_U1_400_3 10
#define I_8266_DM_400_3 11
#define I_8266_BB_400_3 12
//TM1814 (RGBW)
#define I_8266_U0_TM1_4 13
#define I_8266_U1_TM1_4 14
#define I_8266_DM_TM1_4 15
#define I_8266_BB_TM1_4 16
//TM1829 (RGB)
#define I_8266_U0_TM2_3 17
#define I_8266_U1_TM2_3 18
#define I_8266_DM_TM2_3 19
#define I_8266_BB_TM2_3 20
//UCS8903 (RGB)
#define I_8266_U0_UCS_3 21
#define I_8266_U1_UCS_3 22
#define I_8266_DM_UCS_3 23
#define I_8266_BB_UCS_3 24
//UCS8904 (RGBW)
#define I_8266_U0_UCS_4 25
#define I_8266_U1_UCS_4 26
#define I_8266_DM_UCS_4 27
#define I_8266_BB_UCS_4 28
//FW1906 GRBCW
#define I_8266_U0_FW6_5 29
#define I_8266_U1_FW6_5 30
#define I_8266_DM_FW6_5 31
#define I_8266_BB_FW6_5 32
//ESP8266 APA106
#define I_8266_U0_APA106_3 33
#define I_8266_U1_APA106_3 34
#define I_8266_DM_APA106_3 35
#define I_8266_BB_APA106_3 36
//WS2805 (RGBCW)
#define I_8266_U0_2805_5 37
#define I_8266_U1_2805_5 38
#define I_8266_DM_2805_5 39
#define I_8266_BB_2805_5 40
//TM1914 (RGB)
#define I_8266_U0_TM1914_3 41
#define I_8266_U1_TM1914_3 42
#define I_8266_DM_TM1914_3 43
#define I_8266_BB_TM1914_3 44
//SM16825 (RGBCW)
#define I_8266_U0_SM16825_5 45
#define I_8266_U1_SM16825_5 46
#define I_8266_DM_SM16825_5 47
#define I_8266_BB_SM16825_5 48

/*** ESP32 Neopixel methods ***/
//RGB
#define I_32_RN_NEO_3 1
#define I_32_I2_NEO_3 2
//RGBW
#define I_32_RN_NEO_4 5
#define I_32_I2_NEO_4 6
//400Kbps
#define I_32_RN_400_3 9
#define I_32_I2_400_3 10
//TM1814 (RGBW)
#define I_32_RN_TM1_4 13
#define I_32_I2_TM1_4 14
//TM1829 (RGB)
#define I_32_RN_TM2_3 17
#define I_32_I2_TM2_3 18
//UCS8903 (RGB)
#define I_32_RN_UCS_3 21
#define I_32_I2_UCS_3 22
//UCS8904 (RGBW)
#define I_32_RN_UCS_4 25
#define I_32_I2_UCS_4 26
//FW1906 GRBCW
#define I_32_RN_FW6_5 29
#define I_32_I2_FW6_5 30
//APA106
#define I_32_RN_APA106_3 33
#define I_32_I2_APA106_3 34
//WS2805 (RGBCW)
#define I_32_RN_2805_5 37
#define I_32_I2_2805_5 38
//TM1914 (RGB)
#define I_32_RN_TM1914_3 41
#define I_32_I2_TM1914_3 42
//SM16825 (RGBCW)
#define I_32_RN_SM16825_5 45
#define I_32_I2_SM16825_5 46

//APA102
#define I_HS_DOT_3 101 //hardware SPI
#define I_SS_DOT_3 102 //soft SPI

//LPD8806
#define I_HS_LPD_3 103
#define I_SS_LPD_3 104

//WS2801
#define I_HS_WS1_3 105
#define I_SS_WS1_3 106

//P9813
#define I_HS_P98_3 107
#define I_SS_P98_3 108

//LPD6803
#define I_HS_LPO_3 109
#define I_SS_LPO_3 110

// In the following NeoGammaNullMethod can be replaced with NeoGammaWLEDMethod to perform Gamma correction implicitly
// unfortunately that may apply Gamma correction to pre-calculated palettes which is undesired

class PolyBus {
  private:
  #ifndef ESP8266
    static bool _useParallelI2S;          // use parallel I2S/LCD (8 channels)
    static uint8_t _rmtChannelsAssigned;  // RMT channel tracking for dynamic allocation
    static uint8_t _rmtChannel;           // physical RMT channel to use during bus creation
    static uint8_t _i2sChannelsAssigned;  // I2S channel tracking for dynamic allocation
    static uint8_t _parallelBusItype;     // parallel output does not allow mixed LED types, track I_Type
    static uint8_t _2PchannelsAssigned;  // 2-Pin (SPI) channel assigned: first one gets the hardware SPI, others use bit-banged SPI
    // note on 2-Pin Types: all supported types except WS2801 use start/stop or latch frames, speed is not critical. WS2801 uses a 500us timeout and is prone to flickering if bit-banged too slow.
    // TODO: according to #4863 using more than one bit-banged output can cause glitches even in APA102. This needs investigation as from a hardware perspective all but WS2801 should be immune to timing issues.
  #endif
  public:

  // initialize SPI bus speed for DotStar methods
  template <class T>
  
  static void begin(void* busPtr, uint8_t busType, uint8_t* pins, uint16_t clock_kHz) {
    if (busPtr) static_cast<WLEDpixelBus::IBus*>(busPtr)->begin();
  }
  static void* cleanup(void* busPtr, uint8_t busType) {
    if (busPtr) {
        delete static_cast<WLEDpixelBus::IBus*>(busPtr);
    }
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

static WLEDpixelBus::IBus* create(uint8_t busType, uint8_t* pins, uint16_t len, WLEDpixelBus::ColorOrder colorOrder) {
    Serial.printf("[WPB] PolyBus::create busType=%u pin=%u len=%u\n", busType, pins[0], len);
    if (busType == I_NONE) { Serial.println("[WPB] busType=I_NONE, returning null"); return nullptr; }
    auto btype = WLEDpixelBus::BusType::Auto;
    auto timing = WLEDpixelBus::Timing::WS2812;
    uint8_t hwIndex = pins[0]; 
    
    #ifdef ESP8266
    // ESP8266: timing selection based on iType ranges
    if      (busType >= I_8266_U0_NEO_3 && busType <= I_8266_BB_NEO_3) timing = WLEDpixelBus::Timing::WS2812;
    else if (busType >= I_8266_U0_400_3 && busType <= I_8266_BB_400_3) timing = WLEDpixelBus::Timing::Generic400Kbps;
    else if (busType >= I_8266_U0_TM1_4 && busType <= I_8266_BB_TM1_4) timing = WLEDpixelBus::Timing::TM1814;
    else if (busType >= I_8266_U0_TM2_3 && busType <= I_8266_BB_TM2_3) timing = WLEDpixelBus::Timing::TM1829;
    else if (busType >= I_8266_U0_UCS_3 && busType <= I_8266_BB_UCS_3) timing = WLEDpixelBus::Timing::UCS8903;
    else if (busType >= I_8266_U0_UCS_4 && busType <= I_8266_BB_UCS_4) timing = WLEDpixelBus::Timing::UCS8904;
    else if (busType >= I_8266_U0_APA106_3 && busType <= I_8266_BB_APA106_3) timing = WLEDpixelBus::Timing::APA106;
    else if (busType >= I_8266_U0_FW6_5  && busType <= I_8266_BB_FW6_5)  timing = WLEDpixelBus::Timing::FW1906;
    else if (busType >= I_8266_U0_2805_5 && busType <= I_8266_BB_2805_5) timing = WLEDpixelBus::Timing::WS2805;
    else if (busType >= I_8266_U0_TM1914_3 && busType <= I_8266_BB_TM1914_3) timing = WLEDpixelBus::Timing::TM1914;
    else if (busType >= I_8266_U0_SM16825_5 && busType <= I_8266_BB_SM16825_5) timing = WLEDpixelBus::Timing::SM16825;
    // ESP8266: bus type selection (UART0, UART1, DMA, BitBang)
    if (busType == I_8266_U0_NEO_3 || busType == I_8266_U0_400_3 || busType == I_8266_U0_TM1_4 || busType == I_8266_U0_NEO_4 || busType == I_8266_U0_TM2_3 || busType == I_8266_U0_UCS_3 || busType == I_8266_U0_UCS_4 || busType == I_8266_U0_APA106_3 || busType == I_8266_U0_FW6_5 || busType == I_8266_U0_2805_5 || busType == I_8266_U0_TM1914_3 || busType == I_8266_U0_SM16825_5) btype = WLEDpixelBus::BusType::UART;
    else if (busType == I_8266_U1_NEO_3 || busType == I_8266_U1_400_3 || busType == I_8266_U1_TM1_4 || busType == I_8266_U1_NEO_4 || busType == I_8266_U1_TM2_3 || busType == I_8266_U1_UCS_3 || busType == I_8266_U1_UCS_4 || busType == I_8266_U1_APA106_3 || busType == I_8266_U1_FW6_5 || busType == I_8266_U1_2805_5 || busType == I_8266_U1_TM1914_3 || busType == I_8266_U1_SM16825_5) btype = WLEDpixelBus::BusType::UART;
    else if (busType == I_8266_DM_NEO_3 || busType == I_8266_DM_400_3 || busType == I_8266_DM_TM1_4 || busType == I_8266_DM_NEO_4 || busType == I_8266_DM_TM2_3 || busType == I_8266_DM_UCS_3 || busType == I_8266_DM_UCS_4 || busType == I_8266_DM_APA106_3 || busType == I_8266_DM_FW6_5 || busType == I_8266_DM_2805_5 || busType == I_8266_DM_TM1914_3 || busType == I_8266_DM_SM16825_5) btype = WLEDpixelBus::BusType::DMA;
    else btype = WLEDpixelBus::BusType::BitBang;
    #else
    // ESP32: timing selection based on iType (RN=RMT odd, I2=I2S even)
    switch (busType) {
      case I_32_RN_NEO_3:     case I_32_I2_NEO_3:     timing = WLEDpixelBus::Timing::WS2812;         break;
      case I_32_RN_NEO_4:     case I_32_I2_NEO_4:     timing = WLEDpixelBus::Timing::WS2812;         break;
      case I_32_RN_400_3:     case I_32_I2_400_3:     timing = WLEDpixelBus::Timing::Generic400Kbps;  break;
      case I_32_RN_TM1_4:     case I_32_I2_TM1_4:     timing = WLEDpixelBus::Timing::TM1814;         break;
      case I_32_RN_TM2_3:     case I_32_I2_TM2_3:     timing = WLEDpixelBus::Timing::TM1829;         break;
      case I_32_RN_UCS_3:     case I_32_I2_UCS_3:     timing = WLEDpixelBus::Timing::UCS8903;        break;
      case I_32_RN_UCS_4:     case I_32_I2_UCS_4:     timing = WLEDpixelBus::Timing::UCS8904;        break;
      case I_32_RN_FW6_5:     case I_32_I2_FW6_5:     timing = WLEDpixelBus::Timing::FW1906;         break;
      case I_32_RN_APA106_3:  case I_32_I2_APA106_3:  timing = WLEDpixelBus::Timing::APA106;         break;
      case I_32_RN_2805_5:    case I_32_I2_2805_5:    timing = WLEDpixelBus::Timing::WS2805;         break;
      case I_32_RN_TM1914_3:  case I_32_I2_TM1914_3:  timing = WLEDpixelBus::Timing::TM1914;         break;
      case I_32_RN_SM16825_5: case I_32_I2_SM16825_5: timing = WLEDpixelBus::Timing::SM16825;        break;
      default:                                         timing = WLEDpixelBus::Timing::WS2812;         break;
    }
    // ESP32: bus type selection (odd iType = RMT, even = I2S/Auto)
    if (busType & 0x01) btype = WLEDpixelBus::BusType::RMT;
    // else: leave as Auto (will resolve to I2S or LCD depending on platform)
    #endif

    // Assign RMT channel from PolyBus tracking (incremented for each RMT bus)
    int8_t rmtCh = -1; // default: let WLEDpixelBus auto-allocate
    #ifndef ESP8266
    if (btype == WLEDpixelBus::BusType::RMT) {
      if (_rmtChannel < WLED_MAX_RMT_CHANNELS) {
        rmtCh = _rmtChannel++;
      } else {
        Serial.printf("[WPB] ERROR: no RMT channels left (max=%u)\n", WLED_MAX_RMT_CHANNELS);
        return nullptr;
      }
    }
    #endif

    Serial.printf("[WPB] resolved: btype=%u timing_t0h=%u pin=%u rmtCh=%d\n", (unsigned)btype, timing.t0h_ns, hwIndex, rmtCh);
    return WLEDpixelBus::createBus(btype, hwIndex, timing, colorOrder, len, rmtCh);
  }

  [[gnu::hot]] 

  [[gnu::hot]] 

  static unsigned memUsage(unsigned count, unsigned busType) {
    unsigned size = count*3;  // let's assume 3 channels, we will add count or 2*count below for 4 channels or 5 channels
    switch (busType) {
      case I_NONE: size = 0; break;
    #ifdef ESP8266
      // UART methods have front + back buffers + small UART
      case I_8266_U0_NEO_4    : // fallthrough
      case I_8266_U1_NEO_4    : // fallthrough
      case I_8266_BB_NEO_4    : // fallthrough
      case I_8266_U0_TM1_4    : // fallthrough
      case I_8266_U1_TM1_4    : // fallthrough
      case I_8266_BB_TM1_4    : size = (size + count);       break; // 4 channels
      case I_8266_U0_UCS_3    : // fallthrough
      case I_8266_U1_UCS_3    : // fallthrough
      case I_8266_BB_UCS_3    : size *= 2;                   break; // 16 bit
      case I_8266_U0_UCS_4    : // fallthrough
      case I_8266_U1_UCS_4    : // fallthrough
      case I_8266_BB_UCS_4    : size = (size + count)*2;     break; // 16 bit 4 channels
      case I_8266_U0_FW6_5    : // fallthrough
      case I_8266_U1_FW6_5    : // fallthrough
      case I_8266_BB_FW6_5    : // fallthrough
      case I_8266_U0_2805_5   : // fallthrough
      case I_8266_U1_2805_5   : // fallthrough
      case I_8266_BB_2805_5   : size = (size + 2*count);     break; // 5 channels
      case I_8266_U0_SM16825_5: // fallthrough
      case I_8266_U1_SM16825_5: // fallthrough
      case I_8266_BB_SM16825_5: size = (size + 2*count)*2;   break; // 16 bit 5 channels
      // DMA methods have front + DMA buffer = ((1+(3+1)) * channels; exact value is a bit of mistery - needs a dig into NPB)
      case I_8266_DM_NEO_3    : // fallthrough
      case I_8266_DM_400_3    : // fallthrough
      case I_8266_DM_TM2_3    : // fallthrough
      case I_8266_DM_APA106_3 : // fallthrough
      case I_8266_DM_TM1914_3 : size *= 5;                   break;
      case I_8266_DM_NEO_4    : // fallthrough
      case I_8266_DM_TM1_4    : size = (size + count)*5;     break;
      case I_8266_DM_UCS_3    : size *= 2*5;                 break;
      case I_8266_DM_UCS_4    : size = (size + count)*2*5;   break;
      case I_8266_DM_FW6_5    : // fallthrough
      case I_8266_DM_2805_5   : size = (size + 2*count)*5;   break;
      case I_8266_DM_SM16825_5: size = (size + 2*count)*2*5; break;
    #else
      // note: RMT and I2S buses use ~100 bytes of internal NPB memory each, not included here for simplicity
      // RMT buses (1x front and 1x back buffer, does not include small RMT buffer)
      case I_32_RN_NEO_4    : // fallthrough
      case I_32_RN_TM1_4    : size = (size + count)*2;     break; // 4 channels
      case I_32_RN_UCS_3    : size *= 2*2;                 break; // 16bit
      case I_32_RN_UCS_4    : size = (size + count)*2*2;   break; // 16bit, 4 channels
      case I_32_RN_FW6_5    : // fallthrough
      case I_32_RN_2805_5   : size = (size + 2*count)*2;   break; // 5 channels
      case I_32_RN_SM16825_5: size = (size + 2*count)*2*2; break; // 16bit, 5 channels
      // I2S bus or paralell I2S buses (1x front, does not include DMA buffer which is front*cadence, a bit(?) more for LCD)
      #ifndef CONFIG_IDF_TARGET_ESP32C3
      case I_32_I2_NEO_3    : // fallthrough
      case I_32_I2_400_3    : // fallthrough
      case I_32_I2_TM2_3    : // fallthrough
      case I_32_I2_APA106_3 :                              break; // do nothing, I2S uses single buffer + DMA buffer
      case I_32_I2_NEO_4    : // fallthrough
      case I_32_I2_TM1_4    : size = (size + count);       break; // 4 channels
      case I_32_I2_UCS_3    : size *= 2;                   break; // 16 bit
      case I_32_I2_UCS_4    : size = (size + count)*2;     break; // 16 bit, 4 channels
      case I_32_I2_FW6_5    : // fallthrough
      case I_32_I2_2805_5   : size = (size + 2*count);     break; // 5 channels
      case I_32_I2_SM16825_5: size = (size + 2*count)*2;   break; // 16 bit, 5 channels
      #endif
      default               : size *= 2;                   break; // everything else uses 2 buffers
    #endif
    }
    return size;
  }
#ifndef ESP8266
  // Reset channel tracking (call before adding buses)
  static void resetChannelTracking() {
    _useParallelI2S = false;
    _rmtChannelsAssigned = 0;
    _rmtChannel = 0;
    _i2sChannelsAssigned = 0;
    _parallelBusItype = I_NONE;
    _2PchannelsAssigned = 0;
    WLEDpixelBus::RmtBus::resetAutoChannel();  // also reset WLEDpixelBus internal counter
  }
#endif
  // reserves and gives back the internal type index (I_XX_XXX_X above) for the input based on bus type and pins
  static uint8_t getI(uint8_t busType, const uint8_t* pins, uint8_t driverPreference) {
    if (!Bus::isDigital(busType)) return I_NONE;
    uint8_t t = I_NONE;
    if (Bus::is2Pin(busType)) { //SPI LED chips
      bool isHSPI = false;
      #ifdef ESP8266
      if (pins[0] == P_8266_HS_MOSI && pins[1] == P_8266_HS_CLK) isHSPI = true;
      #else
      if (_2PchannelsAssigned == 0) isHSPI = true; // first 2-pin channel uses hardware SPI
      _2PchannelsAssigned++;
      #endif
      switch (busType) {
        case TYPE_APA102:  t = I_SS_DOT_3; break;
        case TYPE_LPD8806: t = I_SS_LPD_3; break;
        case TYPE_LPD6803: t = I_SS_LPO_3; break;
        case TYPE_WS2801:  t = I_SS_WS1_3; break;
        case TYPE_P9813:   t = I_SS_P98_3; break;
      }
      if (t > I_NONE && isHSPI) t--; //hardware SPI has one smaller ID than software
    } else {
      #ifdef ESP8266
      uint8_t offset = pins[0] -1; //for driver: 0 = uart0, 1 = uart1, 2 = dma, 3 = bitbang
      if (offset > 3) offset = 3;
      switch (busType) {
        case TYPE_WS2812_1CH_X3:
        case TYPE_WS2812_2CH_X3:
        case TYPE_WS2812_RGB:
        case TYPE_WS2812_WWA:
          t = I_8266_U0_NEO_3 + offset; break;
        case TYPE_SK6812_RGBW:
          t = I_8266_U0_NEO_4 + offset; break;
        case TYPE_WS2811_400KHZ:
          t = I_8266_U0_400_3 + offset; break;
        case TYPE_TM1814:
          t = I_8266_U0_TM1_4 + offset; break;
        case TYPE_TM1829:
          t = I_8266_U0_TM2_3 + offset; break;
        case TYPE_UCS8903:
          t = I_8266_U0_UCS_3 + offset; break;
        case TYPE_UCS8904:
          t = I_8266_U0_UCS_4 + offset; break;
        case TYPE_APA106:
          t = I_8266_U0_APA106_3 + offset; break;
        case TYPE_FW1906:
          t = I_8266_U0_FW6_5 + offset; break;
        case TYPE_WS2805:
          t = I_8266_U0_2805_5 + offset; break;
        case TYPE_TM1914:
          t = I_8266_U0_TM1914_3 + offset; break;
        case TYPE_SM16825:
          t = I_8266_U0_SM16825_5 + offset; break;
      }
      #else //ESP32
      // dynamic channel allocation based on driver preference
      // determine which driver to use based on preference and availability. First I2S bus locks the I2S type, all subsequent I2S buses are assigned the same type (hardware restriction)
      uint8_t offset = 0; // 0 = RMT, 1 = I2S/LCD
      if (driverPreference == 0 && _rmtChannelsAssigned < WLED_MAX_RMT_CHANNELS) {
        _rmtChannelsAssigned++;
      } else if (_i2sChannelsAssigned < WLED_MAX_I2S_CHANNELS) {
        offset = 1; // I2S requested or RMT full
        _i2sChannelsAssigned++;
      } else {
        return I_NONE; // No channels available
      }

      // Now determine actual bus type with the chosen offset
      switch (busType) {
        case TYPE_WS2812_1CH_X3:
        case TYPE_WS2812_2CH_X3:
        case TYPE_WS2812_RGB:
        case TYPE_WS2812_WWA:
          t = I_32_RN_NEO_3 + offset; break;
        case TYPE_SK6812_RGBW:
          t = I_32_RN_NEO_4 + offset; break;
        case TYPE_WS2811_400KHZ:
          t = I_32_RN_400_3 + offset; break;
        case TYPE_TM1814:
          t = I_32_RN_TM1_4 + offset; break;
        case TYPE_TM1829:
          t = I_32_RN_TM2_3 + offset; break;
        case TYPE_UCS8903:
          t = I_32_RN_UCS_3 + offset; break;
        case TYPE_UCS8904:
          t = I_32_RN_UCS_4 + offset; break;
        case TYPE_APA106:
          t = I_32_RN_APA106_3 + offset; break;
        case TYPE_FW1906:
          t = I_32_RN_FW6_5 + offset; break;
        case TYPE_WS2805:
          t = I_32_RN_2805_5 + offset; break;
        case TYPE_TM1914:
          t = I_32_RN_TM1914_3 + offset; break;
        case TYPE_SM16825:
          t = I_32_RN_SM16825_5 + offset; break;
      }
      // If using parallel I2S, set the type accordingly
      if (_i2sChannelsAssigned == 1 && offset == 1) { // first I2S channel request, lock the type
        _parallelBusItype = t;
        #ifdef CONFIG_IDF_TARGET_ESP32S3
        _useParallelI2S = true; // ESP32-S3 always uses parallel I2S (LCD method)
        #endif
      }
      else if (offset == 1) { // not first I2S channel, use locked type and enable parallel flag
        _useParallelI2S = true;
        t = _parallelBusItype;
      }
      #endif
    }
    return t;
  }
};
#endif
