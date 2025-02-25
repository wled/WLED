#pragma once

#include <memory>

class Effect;
class LazyColor;

// Kinda emulates a v-table without needing an actual v-table.
struct EffectInformation {
    using MakeEffectFunction    = std::unique_ptr<Effect> (*)();
    using NextFrameFunction     = void (*)(Effect* effect);
    using NextRowFunction       = void (*)(Effect* effect, int y);
    using GetPixelColorFunction = uint32_t (*)(Effect* effect, int x, int y, const LazyColor& currentColor);

    const char* metaData;
    const uint8_t effectId;
    const uint8_t defaultPaletteId;
    const MakeEffectFunction makeEffect;
    const NextFrameFunction nextFrame;
    const NextRowFunction nextRow;
    const GetPixelColorFunction getPixelColor;
};
static_assert(std::is_pod_v<EffectInformation>);

class Effect {
public:
    explicit constexpr Effect(const EffectInformation& ei) : info(ei) {}
    constexpr uint8_t getEffectId() const {
        return info.effectId;
    }
    constexpr uint8_t getDefaultPaletteId() const {
        return info.defaultPaletteId;
    }

    constexpr void nextFrame() {
        info.nextFrame(this);
    }
    constexpr void nextRow(int y) {
        info.nextRow(this, y);
    }
    constexpr uint32_t getPixelColor(int x, int y, const LazyColor& currentColor) {
        return info.getPixelColor(this, x, y, currentColor);
    }

    constexpr static void nextFrameNoop(Effect* effect) {
    }
    constexpr static void nextRowNoop(Effect* effect, int y) {
    }
    constexpr static uint32_t getPixelColorNoop(Effect* effect, int x, int y, const LazyColor& currentColor) {
        return 0;
    }

private:
    const EffectInformation& info;
};

class EffectFactory {
public:
    explicit constexpr EffectFactory(const EffectInformation& ei) : info(ei) {}
    constexpr uint8_t getEffectId() const {
        return info.effectId;
    }
    constexpr const char* getMetaData() const {
        return info.metaData;
    }
    std::unique_ptr<Effect> makeEffect() const {
        return info.makeEffect();
    }
private:
    const EffectInformation& info;
};
