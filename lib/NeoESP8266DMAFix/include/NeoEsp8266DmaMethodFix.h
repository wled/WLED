/*-------------------------------------------------------------------------
NeoPixel library helper functions for Esp8266.

FIXED VERSION FROM https://github.com/Makuna/NeoPixelBus/pull/894
This library will overlay/shadow the base version from NeoPixelBus 

Written by Michael C. Miller.
Thanks to g3gg0.de for porting the initial DMA support which lead to this.
Thanks to github/cnlohr for the original work on DMA support, which opend
all our minds to a better way (located at https://github.com/cnlohr/esp8266ws2812i2s).

I invest time and resources providing this open source code,
please support me by donating (see https://github.com/Makuna/NeoPixelBus)

-------------------------------------------------------------------------
This file is part of the Makuna/NeoPixelBus library.

NeoPixelBus is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as
published by the Free Software Foundation, either version 3 of
the License, or (at your option) any later version.

NeoPixelBus is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with NeoPixel.  If not, see
<http://www.gnu.org/licenses/>.
-------------------------------------------------------------------------*/

#pragma once

#ifdef ARDUINO_ARCH_ESP8266
#include "internal/methods/NeoEsp8266DmaMethod.h"

template<typename T_PATTERN> class NeoEsp8266Dma3StepEncodeFixed : public T_PATTERN
{
public:
    const static size_t DmaBitsPerPixelBit = 3; // 3 step cadence, matches encoding

    static size_t SpacingPixelSize(size_t sizePixel)
    {
        return sizePixel;
    }

    static void FillBuffers(uint8_t* i2sBuffer,
        const uint8_t* data,
        size_t sizeData,
        [[maybe_unused]] size_t sizePixel)
    {
        const uint8_t SrcBitMask = 0x80;
        const size_t BitsInSample = sizeof(uint32_t) * 8;

        uint32_t* pDma = reinterpret_cast<uint32_t*>(i2sBuffer);
        uint32_t dmaValue = 0;
        uint8_t destBitsLeft = BitsInSample;

        const uint8_t* pSrc = data;
        const uint8_t* pEnd = pSrc + sizeData;

        while (pSrc < pEnd)
        {
            uint8_t value = *(pSrc++);

            for (uint8_t bitSrc = 0; bitSrc < 8; bitSrc++)
            {
                const uint16_t Bit = ((value & SrcBitMask) ? T_PATTERN::OneBit3Step : T_PATTERN::ZeroBit3Step);

                if (destBitsLeft > 3)
                {
                    destBitsLeft -= 3;
                    dmaValue |= Bit << destBitsLeft;

#if defined(NEO_DEBUG_DUMP_I2S_BUFFER)
                    NeoUtil::PrintBin<uint32_t>(dmaValue);
                    Serial.print(" < ");
                    Serial.println(destBitsLeft);
#endif
                }
                else if (destBitsLeft <= 3)
                {
                    uint8_t bitSplit = (3 - destBitsLeft);
                    dmaValue |= Bit >> bitSplit;

#if defined(NEO_DEBUG_DUMP_I2S_BUFFER)
                    NeoUtil::PrintBin<uint32_t>(dmaValue);
                    Serial.print(" > ");
                    Serial.println(bitSplit);
#endif
                    // next dma value, store and reset
                    *(pDma++) = dmaValue;
                    dmaValue = 0;

                    destBitsLeft = BitsInSample - bitSplit;
                    if (bitSplit)
                    {
                        dmaValue |= Bit << destBitsLeft;
                    }

#if defined(NEO_DEBUG_DUMP_I2S_BUFFER)
                    NeoUtil::PrintBin<uint32_t>(dmaValue);
                    Serial.print(" v ");
                    Serial.println(bitSplit);
#endif
                }

                // Next
                value <<= 1;
            }
        }
        // store the remaining bits
        if (destBitsLeft != BitsInSample) *pDma++ = dmaValue;
    }
};

// Abuse explict specialization to overlay the methods
template<> class NeoEsp8266Dma3StepEncode<NeoEsp8266DmaNormalPattern> : public NeoEsp8266Dma3StepEncodeFixed<NeoEsp8266DmaNormalPattern> {};
template<> class NeoEsp8266Dma3StepEncode<NeoEsp8266DmaInvertedPattern> : public NeoEsp8266Dma3StepEncodeFixed<NeoEsp8266DmaInvertedPattern> {};

#endif
