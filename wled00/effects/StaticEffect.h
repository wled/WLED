#pragma once

#include "../FX.h"
#include "Effect.h"

class StaticEffect : public Effect {
private:
    using Self = StaticEffect;
public:
    using Effect::Effect;
    static std::unique_ptr<Effect> makeEffect() {
        return std::make_unique<StaticEffect>(effectInformation);
    }

    static uint32_t getPixelColor(Effect* effect, int x, int y, uint32_t currentColor) {
        return static_cast<Self*>(effect)->getPixelColorImpl(x, y, currentColor);
    }

    static constexpr EffectInformation effectInformation {
        "Solid",
        FX_MODE_STATIC,
        0u,
        &Self::makeEffect,
        &Effect::nextFrameNoop,
        &Effect::nextRowNoop,
        &Self::getPixelColor,
    };

private:
    uint32_t getPixelColorImpl(int x, int y, uint32_t currentColor) {
        return SEGCOLOR(0);
    }
};
