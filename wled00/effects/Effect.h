#pragma once

#include "../wled.h" // for debug prints

class Effect;

// Kinda emulates a v-table without needing an actual v-table.
struct EffectInformation {
    const char* metaData;
    uint8_t effectId;
    uint8_t defaultPaletteId;
    std::unique_ptr<Effect> (*makeEffect)();
};
static_assert(std::is_pod_v<EffectInformation>);

class Effect {
public:
    explicit constexpr Effect(const EffectInformation& ei) : info(ei) {}
    constexpr uint8_t getEffectId() const {
        return info.effectId;
    }
    uint8_t getDefaultPaletteId() const {
        return info.defaultPaletteId;
    }
    virtual void nextFrame() {}
    virtual void nextRow(int y) {}
    virtual uint32_t getPixelColor(int x, int y, uint32_t currentColor) {
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
