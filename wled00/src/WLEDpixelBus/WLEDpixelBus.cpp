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

ColorEncoder::ColorEncoder(ColorOrder order) : _order(order), _numChannels(getChannelCount(order))
{
    _idxR = _idxG = _idxB = _idxW = _idxWW = _idxCW = 0xFF;
//TODO: is there a better way to define color orders indices than just this dumb list? it also does not cover all possibilities I think (swapping white and color channels)
    switch(order)
    {
        case ColorOrder::RGB:
            _idxR=0; _idxG=1; _idxB=2;
            break;

        case ColorOrder::GRB:
            _idxG=0; _idxR=1; _idxB=2;
            break;

        case ColorOrder::BRG:
            _idxB=0; _idxR=1; _idxG=2;
            break;

        case ColorOrder::RBG:
            _idxR=0; _idxB=1; _idxG=2;
            break;

        case ColorOrder::GBR:
            _idxG=0; _idxB=1; _idxR=2;
            break;

        case ColorOrder::BGR:
            _idxB=0; _idxG=1; _idxR=2;
            break;

        case ColorOrder::RGBW:
            _idxR=0; _idxG=1; _idxB=2; _idxW=3;
            break;

        case ColorOrder::GRBW:
            _idxG=0; _idxR=1; _idxB=2; _idxW=3;
            break;

        case ColorOrder::BRGW:
            _idxB=0; _idxR=1; _idxG=2; _idxW=3;
            break;

        case ColorOrder::RBGW:
            _idxR=0; _idxB=1; _idxG=2; _idxW=3;
            break;

        case ColorOrder::GBRW:
            _idxG=0; _idxB=1; _idxR=2; _idxW=3;
            break;

        case ColorOrder::BGRW:
            _idxB=0; _idxG=1; _idxR=2; _idxW=3;
            break;

        case ColorOrder::WRGB:
            _idxW=0; _idxR=1; _idxG=2; _idxB=3;
            break;

        case ColorOrder::WGRB:
            _idxW=0; _idxG=1; _idxR=2; _idxB=3;
            break;

        case ColorOrder::WBRG:
            _idxW=0; _idxB=1; _idxR=2; _idxG=3;
            break;

        case ColorOrder::WRBG:
            _idxW=0; _idxR=1; _idxB=2; _idxG=3;
            break;

        case ColorOrder::WGBR:
            _idxW=0; _idxG=1; _idxB=2; _idxR=3;
            break;

        case ColorOrder::WBGR:
            _idxW=0; _idxB=1; _idxG=2; _idxR=3;
            break;

        case ColorOrder::RGBWC:
            _idxR=0; _idxG=1; _idxB=2; _idxWW=3; _idxCW=4;
            break;

        case ColorOrder::GRBWC:
            _idxG=0; _idxR=1; _idxB=2; _idxWW=3; _idxCW=4;
            break;

        case ColorOrder::BRGWC:
            _idxB=0; _idxR=1; _idxG=2; _idxWW=3; _idxCW=4;
            break;

        case ColorOrder::RBGWC:
            _idxR=0; _idxB=1; _idxG=2; _idxWW=3; _idxCW=4;
            break;

        case ColorOrder::GBRWC:
            _idxG=0; _idxB=1; _idxR=2; _idxWW=3; _idxCW=4;
            break;

        case ColorOrder::BGRWC:
            _idxB=0; _idxG=1; _idxR=2; _idxWW=3; _idxCW=4;
            break;

        default:
            _idxG=0; _idxR=1; _idxB=2;
            break;
    }
}


//==============================================================================
// Bus Factory Implementation
//==============================================================================

IBus* createBus(BusType type, int8_t pin, const LedTiming& timing,
                ColorOrder order, size_t bufferSize, int8_t channel) {
    
    Serial.printf("[WPB] createBus type=%u pin=%d bufSize=%u ch=%d\n", (unsigned)type, pin, bufferSize, channel);
    if (type == BusType::Auto) {
        type = getRecommendedBusType(); // TODO: when is "auto" used? should auto default to RMT? it currently defaults to I2S
        Serial.printf("[WPB] Auto resolved to type=%u\n", (unsigned)type);
    }

    IBus* bus = nullptr;

    switch (type) {
#if defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32S3) || defined(WLEDPB_ESP32C3)
        case BusType::RMT:
            bus = new RmtBus(pin, timing, order, channel);
            break;

#ifdef WLEDPB_I2S_SUPPORT
        case BusType::I2S:
#if defined(WLEDPB_ESP32S2) || defined(WLEDPB_ESP32C3)
            bus = new I2sBus(pin, timing, order, 0, bufferSize);
#else
            bus = new I2sBus(pin, timing, order, 1, bufferSize);
#endif
            break;
#endif

#ifdef WLEDPB_LCD_SUPPORT
        case BusType::LCD:
            bus = new LcdBus(pin, timing, order, bufferSize);
            break;
#endif

#ifdef WLEDPB_SPI_SUPPORT
        case BusType::SPI:
            bus = new ParallelSpiBus(pin, timing, order);
            break;
#endif

#elif defined(WLEDPB_ESP8266)
        case BusType::UART:
            bus = new Esp8266UartBus(pin, timing, order);
            break;
        case BusType::DMA:
            bus = new Esp8266DmaBus(pin, timing, order);
            break;
        case BusType::BitBang:
            bus = new Esp8266BitBangBus(pin, timing, order);
            break;
#endif

        default:
            return nullptr;
    }

    return bus;
}

BusType getRecommendedBusType() {
#if defined(WLEDPB_ESP32S3)
    return BusType::LCD;  // S3 has best LCD support
#elif defined(WLEDPB_ESP32) || defined(WLEDPB_ESP32S2)
    return BusType::I2S;  // Original and S2 have I2S parallel
#elif defined(WLEDPB_SPI_SUPPORT)
    return BusType::SPI;  // C3 uses SPI quad mode
#elif defined(WLEDPB_ESP8266)
    return BusType::UART;
#else
    return BusType::RMT;  // Fallback to RMT
#endif
}

} // namespace WLEDpixelBus

