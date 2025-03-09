#pragma once

#include "../FX.h"
#include "Effect.h"

class BufferedEffect : public Effect {
protected:
    class PixelBuffer {
        friend class BufferedEffect;
    public:
        uint32_t getPixelColor(int i) const {
            if (i < 0) [[unlikely]] {
                Serial.printf("BufferedEffect::PixelBuffer::getPixelColor: %d < 0\n", i);
                std::terminate();
            }
            if (static_cast<size_t>(i) >= pixels.size()) [[unlikely]] {
                Serial.printf("BufferedEffect::PixelBuffer::getPixelColor: %d >= %u\n", i, pixels.size());
                std::terminate();
            }
            return pixels[static_cast<size_t>(i)];
        }
        inline uint32_t getPixelColorXY(int x, int y) const {
            return getPixelColor(y * SEG_W + x);
        }
        void setPixelColor(int i, uint32_t c) {
            if (i < 0) [[unlikely]] {
                std::terminate();
            }
            if (static_cast<size_t>(i) >= pixels.size()) [[unlikely]] {
                std::terminate();
            }
            pixels[static_cast<size_t>(i)] = c;
        }
        void blendPixelColor(int n, uint32_t color, uint8_t blend) {
            setPixelColor(n, color_blend(getPixelColor(n), color, blend));
        }

    private:
        std::vector<uint32_t> pixels;
    };

private:
    using Self = BufferedEffect;
    using Base = Effect;

protected:
    explicit BufferedEffect(const EffectInformation& ei, bool initBufferWithCurrentState)
        : Base{ei}
    {
        if (!initBufferWithCurrentState) {
            return;
        }
        const unsigned strips = SEGMENT.nrOfVStrips();
        const unsigned stripLength = SEGLEN;
        const size_t length = SEGMENT.nrOfVStrips() * stripLength;
        buffer.pixels.resize(length); // don't initialize the buffer with specific values
        if (buffer.pixels.size() != length) {
            buffer.pixels.clear();
            return;
        }
        for (unsigned stripIndex = 0; stripIndex < strips; stripIndex++) {
            for (unsigned i = 0; i < stripLength; i++) {
                unsigned encodedIndex = ((i) | (int((stripIndex) + 1) << 16)); // original indexToVStrip
                buffer.pixels[(i * strips + stripIndex)] = SEGMENT.getPixelColor(encodedIndex);
            }
        }
    }

    static inline unsigned indexToVStrip(unsigned index, size_t stripNr) {
        return (SEGLEN - index - 1u) * SEGMENT.nrOfVStrips() + stripNr;
    }

public:
    void nextFrameImpl() {
        const size_t length = SEGMENT.nrOfVStrips() * SEGLEN;
        buffer.pixels.resize(length, 0u);
        if (buffer.pixels.size() != length) {
            buffer.pixels.clear();
            return;
        }
    }

    constexpr void nextRowImpl(int y) {
    }

    uint32_t getPixelColorImpl(int x, int y, const LazyColor& currentColor) {
        return buffer.getPixelColorXY(x, y);
    }

protected:
    PixelBuffer buffer{};
};
