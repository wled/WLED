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
#include "WLEDpixelBus_RMT.h"
#include "WLEDpixelBus_I2S.h"
#include "WLEDpixelBus_LCD.h"
#include "WLEDpixelBus_ParallelSpi.h"

namespace WLEDpixelBus {

//==============================================================================
// Color Encoder Implementation
//==============================================================================

void ColorEncoder::encode(uint32_t pixel, const CctPixel* cct, uint8_t* out) const {
    uint8_t r = getR(pixel);
    uint8_t g = getG(pixel);
    uint8_t b = getB(pixel);
    uint8_t w = getW(pixel);

    // For CCT strips, use CCT data instead of W channel
    uint8_t ww = cct ? cct->ww : w;
    uint8_t cw = cct ? cct->cw : 0;

    switch (_order) {
        // RGB variants
        case ColorOrder::RGB: out[0]=r; out[1]=g; out[2]=b; break;
        case ColorOrder::GRB: out[0]=g; out[1]=r; out[2]=b; break;
        case ColorOrder::BRG: out[0]=b; out[1]=r; out[2]=g; break;
        case ColorOrder::RBG: out[0]=r; out[1]=b; out[2]=g; break;
        case ColorOrder::GBR: out[0]=g; out[1]=b; out[2]=r; break;
        case ColorOrder::BGR: out[0]=b; out[1]=g; out[2]=r; break;

        // RGBW variants
        case ColorOrder::RGBW: out[0]=r; out[1]=g; out[2]=b; out[3]=w; break;
        case ColorOrder::GRBW: out[0]=g; out[1]=r; out[2]=b; out[3]=w; break;
        case ColorOrder::BRGW: out[0]=b; out[1]=r; out[2]=g; out[3]=w; break;
        case ColorOrder::RBGW: out[0]=r; out[1]=b; out[2]=g; out[3]=w; break;
        case ColorOrder::GBRW: out[0]=g; out[1]=b; out[2]=r; out[3]=w; break;
        case ColorOrder::BGRW: out[0]=b; out[1]=g; out[2]=r; out[3]=w; break;
        case ColorOrder::WRGB: out[0]=w; out[1]=r; out[2]=g; out[3]=b; break;
        case ColorOrder::WGRB: out[0]=w; out[1]=g; out[2]=r; out[3]=b; break;
        case ColorOrder::WBRG: out[0]=w; out[1]=b; out[2]=r; out[3]=g; break;
        case ColorOrder::WRBG: out[0]=w; out[1]=r; out[2]=b; out[3]=g; break;
        case ColorOrder::WGBR: out[0]=w; out[1]=g; out[2]=b; out[3]=r; break;
        case ColorOrder::WBGR: out[0]=w; out[1]=b; out[2]=g; out[3]=r; break;

        default: out[0]=g; out[1]=r; out[2]=b; break;
    }
}


//==============================================================================
// Bus Factory Implementation
//==============================================================================

IBus* createBus(BusType type, int8_t pin, const LedTiming& timing,
                ColorOrder order, size_t bufferSize, int8_t channel) {
    
    Serial.printf("[WPB] createBus type=%u pin=%d bufSize=%u ch=%d\n", (unsigned)type, pin, bufferSize, channel);
    if (type == BusType::Auto) {
        type = getRecommendedBusType();
        Serial.printf("[WPB] Auto resolved to type=%u\n", (unsigned)type);
    }

    IBus* bus = nullptr;

    switch (type) {
        case BusType::RMT:
            bus = new RmtBus(pin, timing, order, channel);
            break;

#ifdef WLEDPB_I2S_SUPPORT
        case BusType::I2S:
            bus = new I2sBus(pin, timing, order, 1, bufferSize);
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
#else
    return BusType::RMT;  // Fallback to RMT
#endif
}

} // namespace WLEDpixelBus

