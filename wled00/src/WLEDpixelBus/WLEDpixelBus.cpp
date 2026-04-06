/*-------------------------------------------------------------------------
WLEDpixelBus - Lightweight LED driver library for WLED

by @dedehai, 2026

Features:
- Runtime LED timing configuration
- Double-buffered DMA with interrupt-driven refilling (4-step cadence)
- Support for ESP32, ESP32-S2, ESP32-S3, ESP32-C3
- RMT, I2S parallel, and LCD parallel output methods
- RGBW uint32_t pixel buffer format (WLED native)
- Separate CCT (WW/CW) buffer support
- IDF v4.x compatible

-------------------------------------------------------------------------*/

#include "WLEDpixelBus.h"

#if defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32S3) || defined(WLEDPB_ESP32C3)
#include "WLEDpixelBus_RMT.h"
#include "WLEDpixelBus_I2S.h"
#include "WLEDpixelBus_LCD.h"
#include "WLEDpixelBus_ParallelSpi.h"
#endif

#if defined(WLEDPB_ESP8266)
#include "WLEDpixelBus_ESP8266.h"
#endif

namespace WLEDpixelBus {

//==============================================================================
// Color Encoder Implementation
//==============================================================================

ColorEncoder::ColorEncoder(uint8_t co, uint8_t numChannels, uint8_t ledType)
{
  _numChannels = numChannels;
  _logCh = numChannels;  // save logical count before any 16-bit doubling
  _idxR = _idxG = _idxB = _idxW = _idxWW = _idxCW = 0xFF;

  // RGB position from lower nibble (0=GRB, 1=RGB, 2=BRG, 3=RBG, 4=BGR, 5=GBR)
  uint8_t pos[3];
  switch (co & 0x0F) {
    case 1:  pos[0]=0; pos[1]=1; pos[2]=2; break; // RGB
    case 2:  pos[0]=1; pos[1]=2; pos[2]=0; break; // BRG
    case 3:  pos[0]=0; pos[1]=2; pos[2]=1; break; // RBG
    case 4:  pos[0]=2; pos[1]=1; pos[2]=0; break; // BGR
    case 5:  pos[0]=2; pos[1]=0; pos[2]=1; break; // GBR
    default: pos[0]=1; pos[1]=0; pos[2]=2; break; // GRB (case 0)
  }
  _idxR = pos[0]; _idxG = pos[1]; _idxB = pos[2];

  // White/CCT channels: numChannels from bus type, W-swap from upper nibble
  if (numChannels == 4) {
    // LED-type-specific native wire order: TM1814 and TM1815 send W first.
    // Shift RGB indices up by 1 and place W at index 0 before applying user wSwap.
    if (ledType == TYPE_TM1814 || ledType == TYPE_TM1815) {
      _idxR++; _idxG++; _idxB++;
      _idxW = 0;
    } else {
      _idxW = 3; // standard: W after RGB
    }
    const uint8_t wSwap = co >> 4;
    if (wSwap == 1) { std::swap(_idxW, _idxB); } // swap W & B
    if (wSwap == 2) { std::swap(_idxW, _idxG); } // swap W & G
    if (wSwap == 3) { std::swap(_idxW, _idxR); } // swap W & R
  } else if (numChannels >= 5) {
    _idxWW = 3;
    _idxCW = 4;
    const uint8_t wSwap = co >> 4;
    if (wSwap == 4) { std::swap(_idxWW, _idxCW); } // swap WW & CW
  }

  // 16-bit chips: 2 wire bytes per logical channel.
  // _logCh stays as the logical count; _numChannels becomes the wire count.
  if (ledType == TYPE_UCS8903 || ledType == TYPE_UCS8904 || ledType == TYPE_SM16825) {
    _numChannels *= 2;
  }
}


//==============================================================================
// Bus Factory Implementation
//==============================================================================

PixelBus* createBus(BusDriver driver, int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, size_t bufferSize, int8_t channel, uint8_t ledType) {
  
  if (driver == BusDriver::Auto) {
    driver = getRecommendedBusDriver();
  }

  PixelBus* bus = nullptr;

  switch (driver) {
#if defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32S3) || defined(WLEDPB_ESP32C3)
    case BusDriver::RMT:
      bus = new RmtBus(pin, timing, colorOrder, numChannels, channel, ledType);
      break;

#ifdef WLEDPB_I2S_SUPPORT
        case BusDriver::I2S:
#if defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32C3)
      bus = new I2sBus(pin, timing, colorOrder, numChannels, 0, bufferSize, ledType);
#else
      bus = new I2sBus(pin, timing, colorOrder, numChannels, 1, bufferSize, ledType);
#endif
      break;
#endif

#ifdef WLEDPB_LCD_SUPPORT
    case BusDriver::LCD:
      bus = new LcdBus(pin, timing, colorOrder, numChannels, bufferSize, false, ledType);
      break;
#endif

#ifdef WLEDPB_PARALLEL_SPI_SUPPORT
    case BusDriver::SPI:
      bus = new ParallelSpiBus(pin, timing, colorOrder, numChannels, ledType);
      break;
#endif

#elif defined(WLEDPB_ESP8266)
    case BusDriver::UART:
      bus = new Esp8266UartBus(pin, timing, colorOrder, numChannels, ledType);
      break;
    case BusDriver::DMA:
      bus = new Esp8266DmaBus(pin, timing, colorOrder, numChannels, ledType);
      break;
    case BusDriver::BitBang:
      bus = new Esp8266BitBangBus(pin, timing, colorOrder, numChannels, ledType);
      break;
#endif

    default:
      return nullptr;
  }

  return bus;
}

// TODO: this function is not really needed, bus driver defaults should be handled by busmanager or buswrapper
BusDriver getRecommendedBusDriver() {
#if defined(WLEDPB_ESP8266)
  return BusDriver::BitBang; // use bitbanging by default (most versatile)
#else
  return BusDriver::RMT;  // use RMT by default (most versatile)
#endif
}

} // namespace WLEDpixelBus

