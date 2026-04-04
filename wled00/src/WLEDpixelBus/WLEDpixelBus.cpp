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

ColorEncoder::ColorEncoder(uint8_t co, uint8_t numChannels)
{
  _numChannels = numChannels;
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
    _idxW = 3; // default W after RGB
    const uint8_t wSwap = co >> 4;
    if (wSwap == 1) { uint8_t t = _idxW; _idxW = _idxB; _idxB = t; } // swap W & B
    if (wSwap == 2) { uint8_t t = _idxW; _idxW = _idxG; _idxG = t; } // swap W & G
    if (wSwap == 3) { uint8_t t = _idxW; _idxW = _idxR; _idxR = t; } // swap W & R
  } else if (numChannels >= 5) {
    _idxWW = 3;
    _idxCW = 4;
  }
}


//==============================================================================
// Bus Factory Implementation
//==============================================================================

PixelBus* createBus(BusType type, int8_t pin, const LedTiming& timing, uint8_t colorOrder, uint8_t numChannels, size_t bufferSize, int8_t channel) {
  
  if (type == BusType::Auto) {
    type = getRecommendedBusType();
  }

  PixelBus* bus = nullptr;

  switch (type) {
#if defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32S3) || defined(WLEDPB_ESP32C3)
    case BusType::RMT:
      bus = new RmtBus(pin, timing, colorOrder, numChannels, channel);
      break;

#ifdef WLEDPB_I2S_SUPPORT
    case BusType::I2S:
#if defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32C3)
      bus = new I2sBus(pin, timing, colorOrder, numChannels, 0, bufferSize);
#else
      bus = new I2sBus(pin, timing, colorOrder, numChannels, 1, bufferSize);
#endif
      break;
#endif

#ifdef WLEDPB_LCD_SUPPORT
    case BusType::LCD:
      bus = new LcdBus(pin, timing, colorOrder, numChannels, bufferSize);
      break;
#endif

#ifdef WLEDPB_PARALLEL_SPI_SUPPORT
    case BusType::SPI:
      bus = new ParallelSpiBus(pin, timing, colorOrder, numChannels);
      break;
#endif

#elif defined(WLEDPB_ESP8266)
    case BusType::UART:
      bus = new Esp8266UartBus(pin, timing, colorOrder, numChannels);
      break;
    case BusType::DMA:
      bus = new Esp8266DmaBus(pin, timing, colorOrder, numChannels);
      break;
    case BusType::BitBang:
      bus = new Esp8266BitBangBus(pin, timing, colorOrder, numChannels);
      break;
#endif

    default:
      return nullptr;
  }

  return bus;
}

// TODO: this function is not really needed, bus type defaults should be handled by busmanager or buswrapper
BusType getRecommendedBusType() {
#if defined(WLEDPB_ESP8266)
  return BusType::BitBang; // use bitbanging by default (most versatile)
#else
  return BusType::RMT;  // use RMT by default (most versatile)
#endif
}

} // namespace WLEDpixelBus

