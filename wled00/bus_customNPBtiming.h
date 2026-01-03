/*
 * Custom NeoPixelBus timing definitions for WLED
 */

#pragma once
#ifndef BusCustomNPBtiming_h
#define BusCustomNPBtiming_h

#include <Arduino.h>
#include "NeoPixelBus.h"

#ifdef ARDUINO_ARCH_ESP32
// RMT driver selection, includes needed for custom RMT types
#if !defined(WLED_USE_SHARED_RMT)  && !defined(__riscv)
#include <NeoEsp32RmtHIMethod.h>
#define USE_NEOPIXELBUS_RMT_HI
#else
#include "internal/methods/NeoEsp32RmtMethod.h"
#endif
#endif

////////////////////////////////
// TM1815 timing and methods  //
////////////////////////////////

// TM1815 is exactly half the speed of TM1814, other features are identical so can use TM1814 as a base class
// normal timing pulses are inverted compared to WS281x (low pulse timing, high idle/reset)
#ifdef ARDUINO_ARCH_ESP32
// RMT timing
class NeoEsp32RmtSpeedTm1815 : public NeoEsp32RmtInvertedSpeedBase
{
public:
    const static DRAM_ATTR uint32_t RmtBit0 = Item32Val(740, 1780);
    const static DRAM_ATTR uint32_t RmtBit1 = Item32Val(1440, 1060);
    const static DRAM_ATTR uint16_t RmtDurationReset = FromNs(200000); // 200us

    static void IRAM_ATTR Translate(const void* src,
        rmt_item32_t* dest,
        size_t src_size,
        size_t wanted_num,
        size_t* translated_size,
        size_t* item_num);
};

void NeoEsp32RmtSpeedTm1815::Translate(const void* src,
    rmt_item32_t* dest,
    size_t src_size,
    size_t wanted_num,
    size_t* translated_size,
    size_t* item_num)
{
    _translate(src, dest, src_size, wanted_num, translated_size, item_num,
        RmtBit0, RmtBit1, RmtDurationReset);
}
#else // ESP8266
class NeoEspBitBangSpeedTm1815
{
public:
    const static uint32_t T0H = (F_CPU / 5833332 - CYCLES_LOOPTEST); // 0.7us
    const static uint32_t T1H = (F_CPU / 3333332 - CYCLES_LOOPTEST); // 1.5us
    const static uint32_t Period = (F_CPU / 1600000 - CYCLES_LOOPTEST); // 2.5us per bit

    static const uint32_t ResetTimeUs = 200;
    const static uint32_t TLatch = (F_CPU / 20000 - CYCLES_LOOPTEST); // 200us, be generous
};

class NeoEsp8266UartSpeedTm1815 // values taken from "400kHz" speed bus but extended reset time (just like TM1814 does with 800kHz timings)
{
public:
    static const uint32_t ByteSendTimeUs = 20; // us it takes to send a single pixel element at 400khz speed
    static const uint32_t UartBaud = 1600000; // 400mhz, 4 serial bytes per NeoByte
    static const uint32_t ResetTimeUs = 200; // us between data send bursts to reset for next update
};

class NeoWrgbTm1815Feature :
    public Neo4ByteFeature<ColorIndexW, ColorIndexR, ColorIndexG, ColorIndexB>,
    public NeoElementsTm1814Settings<ColorIndexW, ColorIndexR, ColorIndexG, ColorIndexB>
{
};
#endif

// I2S / DMA custom timing
class NeoBitsSpeedTm1815 : public NeoBitsSpeedBase
{
public:
  const static uint16_t BitSendTimeNs = 2500;
  const static uint16_t ResetTimeUs = 200;
};

// TM1815 methods
#ifdef ARDUINO_ARCH_ESP32
  // ESP32 RMT Methods
  #ifdef USE_NEOPIXELBUS_RMT_HI
    typedef NeoEsp32RmtHIMethodBase<NeoEsp32RmtSpeedTm1815, NeoEsp32RmtChannelN> NeoEsp32RmtHINTm1815Method;
  #else
    typedef NeoEsp32RmtMethodBase<NeoEsp32RmtSpeedTm1815, NeoEsp32RmtChannelN> NeoEsp32RmtNTm1815Method;
  #endif
  // ESP32 I2S Methods
  #if !defined(CONFIG_IDF_TARGET_ESP32C3)
    typedef NeoEsp32I2sMethodBase<NeoBitsSpeedTm1815,  NeoEsp32I2sBusZero,  NeoBitsInverted, NeoEsp32I2sCadence> NeoEsp32I2s0Tm1815Method;
    typedef NeoEsp32I2sMethodBase<NeoBitsSpeedTm1815,  NeoEsp32I2sBusOne,   NeoBitsInverted, NeoEsp32I2sCadence> NeoEsp32I2s1Tm1815Method;
    typedef NeoEsp32I2sXMethodBase<NeoBitsSpeedTm1815, NeoEsp32I2s0Mux8Bus, NeoBitsInverted>                     NeoEsp32I2s0X8Tm1815Method; // used by S2
    typedef NeoEsp32I2sXMethodBase<NeoBitsSpeedTm1815, NeoEsp32I2s1Mux8Bus, NeoBitsInverted>                     NeoEsp32I2s1X8Tm1815Method; // used by classic ESP32
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
    typedef NeoEsp32LcdXMethodBase<NeoBitsSpeedTm1815,   NeoEsp32LcdMux8Bus, NeoBitsInverted> NeoEsp32LcdX8Tm1815Method; // S3 only
    #endif
    // I2S Aliases
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
    typedef NeoEsp32LcdX8Tm1815Method    X8Tm1815Method;
    #elif defined(CONFIG_IDF_TARGET_ESP32S2)
    typedef NeoEsp32I2s0X8Tm1815Method   X8Tm1815Method;
    #else // ESP32 classic
    typedef NeoEsp32I2s1X8Tm1815Method   X8Tm1815Method;
    #endif
  #endif // !CONFIG_IDF_TARGET_ESP32C3
#else // ESP8266
  typedef NeoEsp8266UartMethodBase<NeoEsp8266UartSpeedTm1815, NeoEsp8266Uart<UartFeature0, NeoEsp8266UartContext>, NeoEsp8266UartInverted> NeoEsp8266Uart0Tm1815Method;
  typedef NeoEsp8266UartMethodBase<NeoEsp8266UartSpeedTm1815, NeoEsp8266Uart<UartFeature1, NeoEsp8266UartContext>, NeoEsp8266UartInverted> NeoEsp8266Uart1Tm1815Method;
  typedef NeoEsp8266DmaMethodBase<NeoEsp8266I2sCadence<NeoEsp8266DmaInvertedPattern>,NeoBitsSpeedTm1815> NeoEsp8266DmaTm1815Method;
  typedef NeoEspBitBangMethodBase<NeoEspBitBangSpeedTm1815, NeoEspInverted, true> NeoEsp8266BitBangTm1815Method;
#endif

#endif // BusCustomNPBtiming_h