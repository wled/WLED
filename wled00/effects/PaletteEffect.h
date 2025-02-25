#pragma once

#include "../FX.h"
#include "Effect.h"
#include <algorithm>

class PaletteEffect : public Effect {
private:
    using Self = PaletteEffect;
    // Set up some compile time constants so that we can handle integer and float based modes using the same code base.
    #ifdef ESP8266
    using mathType = int32_t;
    using wideMathType = int64_t;
    using angleType = unsigned;
    static constexpr mathType sInt16Scale             = 0x7FFF;
    static constexpr mathType maxAngle                = 0x8000;
    static constexpr mathType staticRotationScale     = 256;
    static constexpr mathType animatedRotationScale   = 1;
    static constexpr int16_t (*sinFunction)(uint16_t) = &sin16_t;
    static constexpr int16_t (*cosFunction)(uint16_t) = &cos16_t;
    #else
    using mathType = float;
    using wideMathType = float;
    using angleType = float;
    static constexpr mathType sInt16Scale           = 1.0f;
    static constexpr mathType maxAngle              = M_PI / 256.0;
    static constexpr mathType staticRotationScale   = 1.0f;
    static constexpr mathType animatedRotationScale = M_TWOPI / double(0xFFFF);
    static constexpr float (*sinFunction)(float)    = &sin_t;
    static constexpr float (*cosFunction)(float)    = &cos_t;
    #endif
public:
    using Effect::Effect;

    static std::unique_ptr<Effect> makeEffect() {
        return std::make_unique<Self>(effectInformation);
    }

    static void nextFrame(Effect* effect) {
        static_cast<Self*>(effect)->nextFrameImpl();
    }

    static constexpr void nextRow(Effect* effect, int y) {
        static_cast<Self*>(effect)->nextRowImpl(y);
    }

    static uint32_t getPixelColor(Effect* effect, int x, int y, const LazyColor& currentColor) {
        return static_cast<Self*>(effect)->getPixelColorImpl(x, y, currentColor);
    }

    static constexpr EffectInformation effectInformation {
        "Palette@Shift,Size,Rotation,,,Animate Shift,Animate Rotation,Anamorphic;;!;12;ix=112,c1=0,o1=1,o2=0,o3=1",
        FX_MODE_PALETTE,
        0u,
        &Self::makeEffect,
        &Self::nextFrame,
        &Self::nextRow,
        &Self::getPixelColor,
    };

private:
    void nextFrameImpl() {
        const bool isMatrix = strip.isMatrix;
        const int cols = SEG_W;
        const int rows = isMatrix ? SEG_H : strip.getActiveSegmentsNum();

        const int  inputRotation        = SEGMENT.custom1;
        const bool inputAnimateRotation = SEGMENT.check2;
        const bool inputAssumeSquare    = SEGMENT.check3;

        const angleType theta = (!inputAnimateRotation) ? ((inputRotation + 128) * maxAngle / staticRotationScale) : (((strip.now * ((inputRotation >> 4) +1)) & 0xFFFF) * animatedRotationScale);
        sinTheta = sinFunction(theta);
        cosTheta = cosFunction(theta);

        const mathType maxX    = std::max(1, cols-1);
        const mathType maxY    = std::max(1, rows-1);
        // Set up some parameters according to inputAssumeSquare, so that we can handle anamorphic mode using the same code base.
        maxXIn  =  inputAssumeSquare ? maxX : mathType(1);
        maxYIn  =  inputAssumeSquare ? maxY : mathType(1);
        maxXOut = !inputAssumeSquare ? maxX : mathType(1);
        const mathType maxYOut = !inputAssumeSquare ? maxY : mathType(1);
        centerX = sInt16Scale * maxXOut / mathType(2);
        centerY = sInt16Scale * maxYOut / mathType(2);
        // The basic idea for this effect is to rotate a rectangle that is filled with the palette along one axis, then map our
        // display to it, to find what color a pixel should have.
        // However, we want a) no areas of solid color (in front of or behind the palette), and b) we want to make use of the full palette.
        // So the rectangle needs to have exactly the right size. That size depends on the rotation.
        // This scale computation here only considers one dimension. You can think of it like the rectangle is always scaled so that
        // the left and right most points always match the left and right side of the display.
        scale = std::abs(sinTheta) + (std::abs(cosTheta) * maxYOut / maxXOut);
    }

    constexpr void nextRowImpl(int y) {
        // translate, scale, rotate
        ytCosTheta = mathType((wideMathType(cosTheta) * wideMathType(y * sInt16Scale - centerY * maxYIn))/wideMathType(maxYIn * scale));
    }

    uint32_t getPixelColorImpl(int x, int y, const LazyColor& currentColor) {
        const int  inputSize            = SEGMENT.intensity;
        const bool inputAnimateShift    = SEGMENT.check1;
        const int  inputShift           = SEGMENT.speed;

        const mathType xtSinTheta = mathType((wideMathType(sinTheta) * wideMathType(x * sInt16Scale - centerX * maxXIn))/wideMathType(maxXIn * scale));
        // Map the pixel coordinate to an imaginary-rectangle-coordinate.
        // The y coordinate doesn't actually matter, as our imaginary rectangle is filled with the palette from left to right,
        // so all points at a given x-coordinate have the same color.
        const mathType sourceX = xtSinTheta + ytCosTheta + centerX;
        // The computation was scaled just right so that the result should always be in range [0, maxXOut], but enforce this anyway
        // to account for imprecision. Then scale it so that the range is [0, 255], which we can use with the palette.
        int colorIndex = (std::min(std::max(sourceX, mathType(0)), maxXOut * sInt16Scale) * wideMathType(255)) / (sInt16Scale * maxXOut);
        // inputSize determines by how much we want to scale the palette:
        // values < 128 display a fraction of a palette,
        // values > 128 display multiple palettes.
        if (inputSize <= 128) {
            colorIndex = (colorIndex * inputSize) / 128;
        } else {
            // Linear function that maps colorIndex 128=>1, 256=>9.
            // With this function every full palette repetition is exactly 16 configuration steps wide.
            // That allows displaying exactly 2 repetitions for example.
            colorIndex = ((inputSize - 112) * colorIndex) / 16;
        }
        // Finally, shift the palette a bit.
        const int paletteOffset = (!inputAnimateShift) ? (inputShift) : (((strip.now * ((inputShift >> 3) +1)) & 0xFFFF) >> 8);
        colorIndex -= paletteOffset;
        const uint32_t color = SEGMENT.color_wheel((uint8_t)colorIndex);
        return color;
    }

private:
    mathType sinTheta;
    mathType cosTheta;
    mathType maxXIn;
    mathType maxYIn;
    mathType maxXOut;
    mathType centerX;
    mathType centerY;
    mathType scale;
    mathType ytCosTheta;
};
