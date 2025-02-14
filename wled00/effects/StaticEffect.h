#pragma once

#include "../FX.h"
#include "Effect.h"

class StaticEffect : public Effect {
public:
    using Effect::Effect;
    static std::unique_ptr<Effect> makeEffect() {
        return std::make_unique<StaticEffect>(effectInformation);
    }

    uint32_t getPixelColor(int x, int y, uint32_t currentColor) override {
        return SEGCOLOR(0);
    }

    static constexpr EffectInformation effectInformation {
        "Solid",
        FX_MODE_STATIC,
        0u,
        &StaticEffect::makeEffect,
    };
};
