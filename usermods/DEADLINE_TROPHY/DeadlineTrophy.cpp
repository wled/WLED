#include "DeadlineTrophy.h"

namespace DeadlineTrophy {

    uint16_t maxMilliAmps(uint16_t nLeds) {
        // from wled_cfg(2).json (should be in this repo) - set via #define in the DeadlineTrophy.h
        // "maxpwr": maximum 1200 mA total (= ABL_MILLIAMPS_DEFAULT = BusManager::ablMilliampsMax())
        // "ledma":  maximum 255 mA per LED (= LED_MILLIAMPS_DEFAULT)
        return (BusManager::ablMilliampsMax() * nLeds) / N_LEDS_TOTAL;
    }

    BusConfig createBus(uint8_t type, uint16_t count, uint16_t start, uint8_t pin1, uint8_t pin2) {
        const uint8_t colorOrder = COL_ORDER_GRB;
        const uint8_t skipFirst = 0;
        const bool reversed = false;
        const uint8_t AWmode = RGBW_MODE_MANUAL_ONLY;
        const uint16_t freqHz_normal = 5000; // called "Normal" in the UI
        const uint16_t freqHz_pwmWhite = 6510;
        uint16_t freqHz = type == LED_SINGLE_WHITE
            ? freqHz_pwmWhite
            : freqHz_normal;
        uint8_t pins[] = {pin1, pin2};
        return BusConfig(
            type,
            pins,
            start,
            count,
            colorOrder,
            reversed,
            skipFirst,
            AWmode,
            freqHz,
            LED_MILLIAMPS_DEFAULT,
            maxMilliAmps(count)
        );
    }

    void overwriteConfig()
    {
        bool useV2 = false;
        #if DEADLINE_CONFIG_VERSION == 2026
          useV2 = true;
        #endif;

        // Usermods usually only care about their own stuff, but then again:
        // "You're remembered for the rules you break" - Stockton Rush, OceanGate CEO
        DEBUG_PRINTF("[USE_DEADLINE_CONFIG] Overwrite config by hard-coded Trophy values. useV2=%d\n", useV2);

        strip.isMatrix = true;

        busConfigs.clear();
        int start = 0;
        busConfigs.emplace_back(createBus(
            LED_TYPE_HD107S,
            N_LEDS_BASE,
            start,
            PIN_BASE_DATA,
            PIN_BASE_CLOCK
        ));
        start += N_LEDS_BASE;
        busConfigs.emplace_back(createBus(
            LED_TYPE_HD107S,
            N_LEDS_LOGO,
            start,
            PIN_LOGO_DATA,
            useV2 ? PIN_LOGO_CLOCK_V2 : PIN_LOGO_CLOCK_V1
        ));
        start += N_LEDS_LOGO;
        busConfigs.emplace_back(createBus(
            LED_SINGLE_WHITE,
            1,
            start,
            PIN_BACK_SPOT
        ));
        start += 1;
        busConfigs.emplace_back(createBus(
            LED_SINGLE_WHITE,
            1,
            start,
            PIN_FLOOR_SPOT
        ));

        // QM: changing gamma did not survive re-flashing (??), so fix it.
        // for the compo, it should be fixed anyway (and communicated before!)
        gammaCorrectBri = false;
        gammaCorrectCol = true;
        gammaCorrectVal = 1.2; // <-- messy suggested 1.0, default is 2.8
        NeoGammaWLEDMethod::calcGammaTable(gammaCorrectVal);

        #ifdef DEADLINE_INIT_BRIGHTNESS
            briS = DEADLINE_INIT_BRIGHTNESS;
            turnOnAtBoot = briS > 0;
        #else
            briS = 96;
            turnOnAtBoot = true;
        #endif
    }

    const char* segmentName[] = {
        "Base Square",
        "Deadline Logo",
        "Back Spot",
        "Floor Spot"
    };
    const Segment segment[] = {
        { 0, baseSize, logoH, logoH + baseSize },
        { 0, logoW, 0, logoH },
        { baseSize, baseSize + 1, logoH, logoH + 1 },
        { baseSize, baseSize + 1, logoH + 1, logoH + 2 }
    };
    const uint8_t segmentCapabilities[] = {
        SEG_CAPABILITY_RGB,
        SEG_CAPABILITY_RGB,
        SEG_CAPABILITY_W,
        SEG_CAPABILITY_W
    };

    static std::array<Coord, N_LEDS_LOGO> logoCoordinates_;
    static bool logoInitialized = false;
    static std::array<Coord, N_LEDS_BASE> baseCoordinates_;
    static bool baseInitialized = false;

    std::array<Coord, N_LEDS_LOGO>& logoCoordinates() {
        if (logoInitialized) {
            return logoCoordinates_;
        }
        float ds = 1. / static_cast<float>(logoH - 1);
        float x0 = 0, y0 = 0;
        const uint8_t ledWithCenterX = 126;
        const uint8_t ledWithCenterY = 87;
        for (uint8_t x = 0; x < logoW; ++x)
        for (uint8_t y = 0; y < logoH; ++y) {
            uint8_t ledIndex = mappingTable[x + logoW * y];
            uint8_t ledIndexInLogo = ledIndex - N_LEDS_BASE;
            if (ledIndex > N_LEDS_TOTAL) {
                continue;
            }
            Coord coord = {
                x,
                y,
                {
                    +static_cast<float>(x) * ds,
                    -static_cast<float>(y) * ds,
                },
                ledIndexInLogo
            };
            logoCoordinates_[ledIndexInLogo] = coord;
            if (ledIndex == ledWithCenterX) {
                x0 = -coord.uv.x;
            }
            if (ledIndex == ledWithCenterY) {
                y0 = -coord.uv.y;
            }
        }
        for (uint8_t i = 0; i < N_LEDS_LOGO; i++) {
            logoCoordinates_[i].uv.shift(x0, y0);
        }
        Logo::initConstants(logoCoordinates_, ds);
        logoInitialized = true;
        return logoCoordinates_;
    }

    std::array<Coord, N_LEDS_BASE>& baseCoordinates() {
        if (baseInitialized) {
            return baseCoordinates_;
        }
        float ds = 1. / static_cast<float>(baseSize - 1);
        for (uint8_t x = 0; x < baseSize; ++x)
        for (uint8_t y = 0; y < baseSize; ++y) {
            uint8_t ledIndex = mappingTable[x + logoW * (y + logoH)];
            if (ledIndex > N_LEDS_TOTAL) {
                continue;
            }
            baseCoordinates_[ledIndex] = {
                x,
                y,
                {
                    static_cast<float>(x) * ds - 0.5f,
                    -(static_cast<float>(y) * ds - 0.5f),
                },
                ledIndex,
            };
        }
        baseInitialized = true;
        return baseCoordinates_;
    }

    namespace Logo {
        float unit;
        Vec2 xUnit{};
        Vec2 yUnit{};
        Vec2 tiltLeft{};
        Vec2 tiltRight{};

        void initConstants(const std::array<Coord, N_LEDS_LOGO>& coords, float unitSize) {
            // will be called by the logoCoordinates() initialization
            // -> i.e. cannot use before the first logoCoordinates() call, but that should do it
            unit = unitSize;
            xUnit = { unitSize, 0 };
            yUnit = { 0, unitSize };

            const uint8_t ledForRightTiltBottom = 161 - N_LEDS_BASE;
            const uint8_t ledForRightTiltTop = 169 - N_LEDS_BASE;
            const uint8_t ledForLeftTiltBottom = 104 - N_LEDS_BASE;
            const uint8_t ledForLeftTiltTop = 120 - N_LEDS_BASE;
            tiltLeft = coords[ledForLeftTiltTop].uv - coords[ledForLeftTiltBottom].uv;
            tiltLeft *= unit / tiltLeft.y;
            tiltRight = coords[ledForRightTiltTop].uv - coords[ledForRightTiltBottom].uv;
            tiltRight *= unit / tiltRight.y;
        }

    }
}
