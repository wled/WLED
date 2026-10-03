//--------------------------------------------------------
// Deadline 2026 Trophy compo entry by spacedawwwg BEC-TTC 
//--------------------------------------------------------
#pragma once

#include "FX.h"
#include "palettes.h"
#include "../usermods/DEADLINE_TROPHY/DeadlineTrophy.h"

extern uint16_t mode_static(void);

//////////////////////////////////////////////////////////
//  Deadline Trophy FX
//////////////////////////////////////////////////////////
// There's a lot of useful stuff in
// FX.cpp -> see the DEV INFO, also, other FX for "inspiration" :)
// wled_math.cpp
// util.cpp
// -- you can also reinvent the wheel.
// I have never tested how efficient we really _HAVE_ to be,
// thus all the helper functions. ..qm
//////////////////////////////////////////////////////////
//
// Notes:
// - the if (...SEGMENT) { ... } is due to "this FX will be evaluated for every segment on its own"
//   remember, that you also have to set it for each segment to be actually active.
// - for "state" variables, there are two uint8_t SEGMENT.aux0 and SEGMENT.aux1,
//   but you can also declare any variable "static" to be more flexible (QM figures that makes sense here)
// - the EVERY_NTH_CALL(n) { ... } helper is great for debugging.

//------------------------------------------------------
//  Logo effects
//------------------------------------------------------
// 6^ FX (rainbow wave, matrix rain, fire, ghostbusters beam, sparkle, shimmer),
namespace DeadlineTrophyFX {

    constexpr int N_LOGO_EFFECTS = 6;           //fx count
    constexpr float STEADY_DURATION = 10.f;         // seconds each effect is shown steady
    constexpr float TRANSITION_DURATION = 4.f;      // seconds spent wiping into the next effect
    constexpr float SLOT_DURATION = STEADY_DURATION + TRANSITION_DURATION; //total duration per fx
    constexpr float FULL_CYCLE_DURATION = SLOT_DURATION * N_LOGO_EFFECTS; //total sequ duration

    enum class WipeStyle : uint8_t { LeftToRight, TopToBottom, BottomToTop, Diagonal, Radial };

    // rolling wipe style used for the transition out of each slot, fixed per pair so a given
    constexpr WipeStyle wipeStyleForSlot[N_LOGO_EFFECTS] = {
        WipeStyle::LeftToRight,
        WipeStyle::BottomToTop,
        WipeStyle::Diagonal,
        WipeStyle::TopToBottom,
        WipeStyle::LeftToRight,
        WipeStyle::Radial,
    };

    // init buffer
    using LogoEffectFn = void (*)(CRGB* buffer, float time, float beat, bool entering);

    static void renderRainbowWave(CRGB* buffer, float time, float beat, bool entering);
    static void renderMatrixRain(CRGB* buffer, float time, float beat, bool entering);
    static void renderFire(CRGB* buffer, float time, float beat, bool entering);
    static void renderGhostbustersBeam(CRGB* buffer, float time, float beat, bool entering);
    static void renderSparkle(CRGB* buffer, float time, float beat, bool entering);
    static void renderShimmer(CRGB* buffer, float time, float beat, bool entering);

    constexpr LogoEffectFn logoEffects[N_LOGO_EFFECTS] = {
        renderRainbowWave, renderMatrixRain, renderFire,
        renderGhostbustersBeam, renderSparkle, renderShimmer,
    };

    // How much of the incoming effect (vs. the outgoing one) should show at `coord`,
    // for a wipe front that has swept `progress` (0..1) of the way across the grid.
    static float wipeBlendFactor(const DeadlineTrophy::Coord& coord, WipeStyle style, float progress) {
        using namespace DeadlineTrophy;

        // init radial trqnsition params
        static float maxRadius = -1.f;
        if (maxRadius < 0.f) {
            maxRadius = 0.f;
            for (const auto& c : logoCoordinates()) maxRadius = max(maxRadius, c.uv.length());
        }

        float metric, metricMax;
        switch (style) {
            case WipeStyle::LeftToRight: metric = coord.x; metricMax = logoW - 1; break;
            case WipeStyle::TopToBottom: metric = coord.y; metricMax = logoH - 1; break;
            case WipeStyle::BottomToTop: metric = (logoH - 1) - coord.y; metricMax = logoH - 1; break;
            case WipeStyle::Diagonal:    metric = coord.x + coord.y; metricMax = (logoW - 1) + (logoH - 1); break;
            default:                     metric = coord.uv.length(); metricMax = maxRadius; break; // Radial
        }

        const float edgeSoftness = 1.5f; // soften the hard edge by this many px
        float sweepPos = -edgeSoftness + progress * (metricMax + 2.f * edgeSoftness);
        return FxHelpers::clip((sweepPos - metric) / (2.f * edgeSoftness) + 0.5f); // 0 = outgoing, 1 = incoming
    }

    static void renderRainbowWave(CRGB* buffer, float time, float beat, bool /*entering*/) {
        using namespace DeadlineTrophy;
        // Rainbow wave animation, sweeping left to right across the logo. Uses the
        // built-in "Rainbow Bands" palette (global palette index 12, see
        // wled00/palettes.h -> RainbowStripeColors_gc22)
        static CRGBPalette16 rainbowBandsPalette = RainbowStripeColors_gc22;
        // Slowly drift the S-curve's strength between range over time 
        const float waveAmplitudePeriod = 23.f; // seconds for one full -30..+30..-30 drift
        float waveAmplitude = sinf(time * (2.f * 3.14159265f / waveAmplitudePeriod)) * 30.f;
        for (const auto& coord : logoCoordinates()) {
            // S-shape as it scrolls instead of staying a straight vertical band.
            int8_t rowOffset = static_cast<int8_t>(sin8(static_cast<uint8_t>(coord.y * 256 / logoH)) - 128);
            uint8_t paletteIndex = static_cast<uint8_t>((coord.x * 256 / logoW) - beat * 24.f
                                                         + (rowOffset * waveAmplitude) / 128.f);
            buffer[coord.index] = ColorFromPalette(rainbowBandsPalette, paletteIndex, 255, LINEARBLEND_NOWRAP);
        }
    }

    static void renderMatrixRain(CRGB* buffer, float time, float /*beat*/, bool /*entering*/) {
        using namespace DeadlineTrophy;
        // foe matrix effect fast drops overwrite slower ones
        static uint8_t trailbuffer[logoH][logoW] = {};
        static float lastFrameTime = -1.f;

        struct RainDrop { int8_t x, y; bool active; float speed; float stepAccum; };
        static const uint8_t N_DROPS = 30; // 1.5x the original 10
        static RainDrop drops[N_DROPS] = {};

        const float stepInterval = 0.12f;         // seconds between falls at speed 1.0x
        const float minSpeed = 0.5f, maxSpeed = 2.f; // per-drop speed range, relative to stepInterval
        const uint8_t fadeAmount = 10;      // how much the trail dims, per frame
        const uint8_t spawnChance = 60;     // out of 255, chance an idle drop respawns each step
        const uint8_t maxCatchUpStepsPerDrop = 6; // safety cap so a stall/lag spike can't spin a single drop forever

        // fade the whole trail buffer a little every frame, so trails darken consistantly
        for (uint8_t y = 0; y < logoH; ++y) {
            for (uint8_t x = 0; x < logoW; ++x) {
                trailbuffer[y][x] = qsub8(trailbuffer[y][x], fadeAmount);
            }
        }

        // advance each drop independently, at its own speed, at a fixed cadence...
        float dt = (lastFrameTime < 0.f) ? 0.f : (time - lastFrameTime);
        lastFrameTime = time;

        for (uint8_t i = 0; i < N_DROPS; ++i) {
            RainDrop& drop = drops[i];
            if (drop.active) {
                drop.stepAccum += dt / stepInterval * drop.speed;
                uint8_t steps = 0;
                while (drop.stepAccum >= 1.f && steps < maxCatchUpStepsPerDrop) {
                    drop.stepAccum -= 1.f;
                    ++steps;
                    drop.x -= 1;
                    drop.y += 1;
                    if (drop.x < 0 || drop.y >= logoH) {
                        drop.active = false; // fell off the grid
                        break;
                    }
                    trailbuffer[drop.y][drop.x] = 255; // bright head pixel, marked at every step along the way
                }
                if (steps == maxCatchUpStepsPerDrop) drop.stepAccum = 0.f; // avoid a runaway backlog after a stall
            }
            if (!drop.active && hw_random8() < spawnChance) {
                if (hw_random8(2) == 0) {
                    // spawn along the top row
                    drop.x = static_cast<int8_t>(hw_random8(logoW));
                    drop.y = 0;
                } else {
                    // spawn along the right column, to cover the bottom-right corner
                    drop.x = static_cast<int8_t>(logoW - 1);
                    drop.y = static_cast<int8_t>(hw_random8(logoH));
                }
                drop.active = true;
                drop.stepAccum = 0.f;
                drop.speed = minSpeed + (hw_random8() / 255.f) * (maxSpeed - minSpeed);
                trailbuffer[drop.y][drop.x] = 255;
            }
        }

        // render: light green head/trail, darker as the trail value decays
        for (const auto& coord : logoCoordinates()) {
            uint8_t v = trailbuffer[coord.y][coord.x];
            if (v > 0) buffer[coord.index] = CRGB(CHSV(96, 255, v)); // hue 96 = green
        }
    }

    static void renderFire(CRGB* buffer, float time, float /*beat*/, bool /*entering*/) {
        using namespace DeadlineTrophy;
        // embers randomly ignite along the bottom row and heat diffuses upward each
        // step using the built-in "Fire" gradient palette
        static CRGBPalette16 firePalette;
        static bool firePaletteLoaded = false;
        if (!firePaletteLoaded) {
            // build palate from bytes
            byte tcp[sizeof(lava_gp)];
            memcpy_P(tcp, lava_gp, sizeof(lava_gp));
            firePalette.loadDynamicGradientPalette(tcp);
            firePaletteLoaded = true;
        }

        static uint8_t heatbuffer[logoH][logoW] = {};
        static float lastFireFrameTime = -1.f;
        static float fireAccumulator = 0.f;

        const float fireStepInterval = 0.05f;    // seconds between diffusion steps
        const uint8_t coolingAmount = 40;        // how much every cell cools, per step
        const uint8_t sparkChance = 45;          // out of 255, chance a bottom cell ignites each step
        const uint8_t sparkMinHeat = 130;        // lower spark heat range so the dark-red low
        const uint8_t sparkMaxHeat = 255;        // end of the palette is reached more often
        const uint8_t maxFireCatchUpSteps = 20;  // safety cap
        float fireDt = (lastFireFrameTime < 0.f) ? 0.f : (time - lastFireFrameTime);
        lastFireFrameTime = time;
        fireAccumulator += fireDt / fireStepInterval;

        uint8_t fireCatchUpSteps = 0;
        while (fireAccumulator >= 1.f && fireCatchUpSteps < maxFireCatchUpSteps) {
            fireAccumulator -= 1.f;
            ++fireCatchUpSteps;

            // step1: cool every cell down a little.
            for (uint8_t y = 0; y < logoH; ++y) {
                for (uint8_t x = 0; x < logoW; ++x) {
                    heatbuffer[y][x] = qsub8(heatbuffer[y][x], hw_random8(coolingAmount));
                }
            }

            // step2: diffuse heat upward, biased to the right. Processed from the top
            // row down to the row above the bottom, so each row reads values from the rows below
            //
            // add wrap to prevent falloff at the edges
            for (uint8_t y = 0; y < static_cast<uint8_t>(logoH - 1); ++y) {
                for (uint8_t x = 0; x < logoW; ++x) {
                    uint8_t leftX = (x == 0) ? (logoW - 1) : (x - 1);
                    uint16_t below     = heatbuffer[y + 1][x];
                    uint16_t belowLeft = heatbuffer[y + 1][leftX];
                    heatbuffer[y][x] = static_cast<uint8_t>((below + belowLeft * 2) / 3);
                }
            }

            // step3: randomly instantiate new embers along the bottom row.
            for (uint8_t x = 0; x < logoW; ++x) {
                if (hw_random8() < sparkChance) {
                    heatbuffer[logoH - 1][x] = qadd8(heatbuffer[logoH - 1][x], hw_random8(sparkMinHeat, sparkMaxHeat));
                }
            }
        }
        if (fireCatchUpSteps == maxFireCatchUpSteps) fireAccumulator = 0.f;

        // map the 'fire' palatte.
        for (const auto& coord : logoCoordinates()) {
            uint8_t heatVal = heatbuffer[coord.y][coord.x];
            buffer[coord.index] = ColorFromPalette(firePalette, heatVal, 255, LINEARBLEND_NOWRAP);
        }
    }

    static void renderGhostbustersBeam(CRGB* buffer, float time, float /*beat*/, bool /*entering*/) {
        using namespace DeadlineTrophy;
        // Proton-stream beam: two independent overlaid parts -
        // part1: a narrow orange "plasma" core 
        // 'Autumn' gradient palette (orange)... part 2 uses flat light-blue/navy-blue colors instead of a palette.
        static CRGBPalette16 autumnPalette;
        static bool autumnPaletteLoaded = false;
        if (!autumnPaletteLoaded) {
            byte tcp[sizeof(es_autumn_19_gp)];
            memcpy_P(tcp, es_autumn_19_gp, sizeof(es_autumn_19_gp));
            autumnPalette.loadDynamicGradientPalette(tcp);
            autumnPaletteLoaded = true;
        }

        const float centerY = (logoH - 1) / 2.f; // fixed middle row

        // part1 tuning the core sits on the middle row and only ripples by about few pxls
        const float rippleAmplitude = 0.9f;      // max pixels of ripple (subtle)
        const float rippleSpeed = 9.f;           // radians/sec - fast ripple
        const float ripplePhasePerColumn = 0.9f; // rad/column, gives it a traveling-wave look
        const float coreHalfHeight = 1.1f;       // ~2-3 pixels tall, the orange plasma core

        // part2 tuning: the lightning bolt's path is a sum of a few sine waves at higher amplitude
        const float boltHalfThickness = 0.8f; // thin line
        const float boltRange = (logoH - 1) / 2.f - boltHalfThickness; // how far off-center it can wander

        // high freq wave seed for lightning effect (check this on real thing)
        const float boltRerollInterval = 0.045f; // seconds between phase re-rolls
        static float boltPhase1 = 0.f, boltPhase2 = 0.f, boltPhase3 = 0.f;
        static uint16_t boltNoiseSeed = 0;
        static float boltLastRerollTime = -1000.f;
        if (time - boltLastRerollTime >= boltRerollInterval) {
            boltPhase1 = hw_random16() * (2.f * 3.14159265f / 65536.f);
            boltPhase2 = hw_random16() * (2.f * 3.14159265f / 65536.f);
            boltPhase3 = hw_random16() * (2.f * 3.14159265f / 65536.f);
            boltNoiseSeed = hw_random16();
            boltLastRerollTime = time;
        }

        for (const auto& coord : logoCoordinates()) {
            // p1: orange plasma core fast, small ripple
            float ripple = sinf(time * rippleSpeed + coord.x * ripplePhasePerColumn) * rippleAmplitude;
            float coreDist = fabsf(static_cast<float>(coord.y) - (centerY + ripple));

            bool orangePixelSet = (coreDist <= coreHalfHeight);
            if (orangePixelSet) {
                // pallate index 
                float vertOffsetNorm = (static_cast<float>(coord.y) - (centerY + ripple)) / coreHalfHeight;
                uint8_t coreIdx = static_cast<uint8_t>(128 + vertOffsetNorm * 100.f + time * 40.f);
                buffer[coord.index] = ColorFromPalette(autumnPalette, coreIdx, 255, LINEARBLEND);
            }

            // p2: blue lightning bolt - erratic path summing several sine waves
            float wave1 = sinf(boltPhase1 + coord.x * 0.55f);
            float wave2 = sinf(boltPhase2 - coord.x * 0.31f);
            float wave3 = sinf(boltPhase3 + coord.x * 1.4f);
            float noiseComponent = (inoise8(coord.x * 40, boltNoiseSeed, 42) - 128) / 128.f;
            const float boltAmplitudeMultiplier = 2.0f; // doubles how strongly the bolt swings
            float combined = (wave1 * 0.4f + wave2 * 0.3f + wave3 * 0.2f + noiseComponent * 0.4f) * boltAmplitudeMultiplier;
            // combined can exceed +-1 after amplification 
            // clamp so the bolt doesn't fl y off past the grid edges (boltRange already spans to the grid edge).
            combined = combined < -1.f ? -1.f : (combined > 1.f ? 1.f : combined);
            float boltY = centerY + combined * boltRange;
            float boltDist = fabsf(static_cast<float>(coord.y) - boltY);

            // give p1 orange so it always takes priority.
            const CRGB boltCoreColor = CRGB(90, 170, 255); // light blue
            const CRGB boltHaloColor = CRGB(0, 0, 90);     // navy blue, ~half as bright as the core
            const float boltHaloHalfThickness = boltHalfThickness + 0.8f;

            if (!orangePixelSet && boltDist <= boltHaloHalfThickness) {
                buffer[coord.index] = (boltDist <= boltHalfThickness) ? boltCoreColor : boltHaloColor;
            }
        }
    }

    static void renderSparkle(CRGB* buffer, float time, float /*beat*/, bool entering) {
        using namespace DeadlineTrophy;
        // pixels randomly ignite in a color drawn from the built-in 'drywet' gradient
        static CRGBPalette16 drywetPalette;
        static bool drywetPaletteLoaded = false;
        if (!drywetPaletteLoaded) {
            // borrow bytes indirectly from drywet palate.
            byte tcp[sizeof(GMT_drywet_gp)];
            memcpy_P(tcp, GMT_drywet_gp, sizeof(GMT_drywet_gp));
            drywetPalette.loadDynamicGradientPalette(tcp);
            drywetPaletteLoaded = true;
        }
        struct SparkleState {
            bool active;
            float startTime;
            uint8_t paletteIdx;
        };
        static SparkleState sparklebuffer[logoH][logoW] = {};
        static float lastSparkleFrameTime = -1.f;
        static float sparkleSpawnAccumulator = 0.f;

        const float fadeInTime = 0.5f; // seconds to fade in from black to full color
        const float fadeOutTime = 4.f; // seconds to fade back out to black
        const float totalLifetime = fadeInTime + fadeOutTime;
        const float sparklesPerSecond = N_LEDS_LOGO * 0.36f;
        const uint8_t maxSpawnsPerFrame = 18; // safety cap, raised alongside the 3x rate

        // hacky... seed existing sparkles from the beginning, do this better later
        if (entering) {
            const uint16_t initialSparkleCount = static_cast<uint16_t>(N_LEDS_LOGO * 0.5f);
            for (uint16_t n = 0; n < initialSparkleCount; ++n) {
                const Coord& c = logoCoordinates()[hw_random8(N_LEDS_LOGO)];
                SparkleState& s = sparklebuffer[c.y][c.x];
                s.active = true;
                s.startTime = time - (hw_random8() / 255.f) * totalLifetime;
                s.paletteIdx = hw_random8();
            }
            lastSparkleFrameTime = time;
            sparkleSpawnAccumulator = 0.f;
        }

        // clamp dt to prevent too large a burst of particles
        float sparkleDt = (lastSparkleFrameTime < 0.f) ? 0.f : min(time - lastSparkleFrameTime, 1.f);
        lastSparkleFrameTime = time;
        sparkleSpawnAccumulator += sparkleDt * sparklesPerSecond;

        uint8_t spawnedThisFrame = 0;
        while (sparkleSpawnAccumulator >= 1.f && spawnedThisFrame < maxSpawnsPerFrame) {
            sparkleSpawnAccumulator -= 1.f;
            ++spawnedThisFrame;

            // try a handful of random pixels, search for one that's currently black.
            for (uint8_t attempt = 0; attempt < 6; ++attempt) {
                const Coord& c = logoCoordinates()[hw_random8(N_LEDS_LOGO)];
                SparkleState& s = sparklebuffer[c.y][c.x];
                bool isBlack = !s.active || (time - s.startTime) >= totalLifetime;
                if (isBlack) {
                    s.active = true;
                    s.startTime = time;
                    s.paletteIdx = hw_random8();
                    break;
                }
            }
        }

        for (const auto& coord : logoCoordinates()) {
            SparkleState& s = sparklebuffer[coord.y][coord.x];
            if (!s.active) continue;
            float elapsed = time - s.startTime;
            if (elapsed >= totalLifetime) {
                s.active = false;
                continue;
            }
            float envelope = (elapsed < fadeInTime)
                ? (elapsed / fadeInTime)
                : (1.f - (elapsed - fadeInTime) / fadeOutTime);
            uint8_t val = static_cast<uint8_t>(255.f * FxHelpers::clip(envelope));
            buffer[coord.index] = ColorFromPalette(drywetPalette, s.paletteIdx, val, LINEARBLEND_NOWRAP);
        }
    }

    static void renderShimmer(CRGB* buffer, float time, float /*beat*/, bool entering) {
        using namespace DeadlineTrophy;
        // Two overlapping layers, drawn in this order so each later layer sits on top
        // of the earlier one - background, then shine:
        //  p1 A "smoldering" effect wigh blue pixels as base layer, lines move brightening areas in a dynamic grid
        //  p2 "Shine" a bright white band of shimmer flashes across the logo

        // --p1a: smoldering dark-blue background, per-pixel random phase ----
        static bool bgInitialized = false;
        static float bgPhaseSeed[N_LEDS_LOGO];
        static uint8_t bgHue[N_LEDS_LOGO];
        static uint8_t bgBaseVal[N_LEDS_LOGO];
        if (!bgInitialized) {
            for (size_t i = 0; i < N_LEDS_LOGO; ++i) {
                bgPhaseSeed[i] = static_cast<float>(hw_random16()) * (2.f * 3.14159265f / 65535.f);
                bgHue[i] = 152 + hw_random8(16);     // dark-blue hue range
                bgBaseVal[i] = 100 + hw_random8(40); // smoldering baseline
            }
            bgInitialized = true;
        }

        // --p1b: continuous top-to-bottom brightening wave ----
        const float waveHeightPx = 3.f;
        const float wavePeriod = 3.f; // seconds for one full top->bottom traverse (medium speed)
        float waveCyclePos = fmodf(time, wavePeriod) / wavePeriod;
        float waveY = waveCyclePos * (logoH - 1 + waveHeightPx) - waveHeightPx * 0.5f;

        // --p1c: continuous left-to-right brightening wave, second axis of
        const float waveWidthPxH = 3.f;
        const float wavePeriodH = 4.f; // slightly different tempo than the vertical wave
        static float hWavePhaseOffset = 0.f;
        if (entering) {
            hWavePhaseOffset = static_cast<float>(hw_random16()) / 65535.f * wavePeriodH;
        }
        float hWaveCyclePos = fmodf(time + hWavePhaseOffset, wavePeriodH) / wavePeriodH;
        float waveX = hWaveCyclePos * (logoW - 1 + waveWidthPxH) - waveWidthPxH * 0.5f;

        // --p2: diagonal "Shine" sweep
        const float shineAngleScale = 2.f;
        const float diagH = static_cast<float>(logoH - 1) * shineAngleScale;
        const float diagW = static_cast<float>(logoW - 1);
        const float pLen = sqrtf(diagH * diagH + diagW * diagW);
        const float shineHalfWidthPx = 2.5f;   // 5px wide white core
        const float shineEdgeSoftPx = 1.5f;    // extra soft gray taper outside the core
        const float shineSweepDuration = 0.85f; // seconds to cross the whole grid
        const float shinePeriod = 7.f;         // seconds between sweeps
        float shinePhase = fmodf(time, shinePeriod);
        bool shineActive = shinePhase < shineSweepDuration;
        float shineCenterF = 0.f;
        if (shineActive) {
            float shineProgress = shinePhase / shineSweepDuration;
            float fMax = 2.f * diagW * diagH;
            float halfWidthF = (shineHalfWidthPx + shineEdgeSoftPx) * pLen;
            // band starts fully off-grid to the left and ends fully off-grid to the right.
            shineCenterF = -halfWidthF + shineProgress * (fMax + 2.f * halfWidthF);
        }

        // ---- combine both layers per pixel, drawing white shine last for precidence
        for (size_t i = 0; i < N_LEDS_LOGO; ++i) {
            const Coord& coord = logoCoordinates()[i];

            // background + both waves
            float bgVal = bgBaseVal[i] + 25.f * sinf(time * 1.3f + bgPhaseSeed[i]);
            float distY = fabsf(static_cast<float>(coord.y) - waveY);
            float waveIntensityV = max(0.f, 1.f - distY / (waveHeightPx * 0.5f));
            float distX = fabsf(static_cast<float>(coord.x) - waveX);
            float waveIntensityH = max(0.f, 1.f - distX / (waveWidthPxH * 0.5f));
            float waveFactor = (1.f + 0.5f * waveIntensityV) * (1.f + 0.5f * waveIntensityH); // each brightens x1.5, multiply where they cross
            uint8_t val = static_cast<uint8_t>(FxHelpers::clip(bgVal * waveFactor / 255.f) * 255.f);
            CHSV pixel = CHSV(bgHue[i], 255, val);

            // shine overlay
            if (shineActive) {
                float f = diagH * coord.x + diagW * coord.y;
                float distPx = fabsf(f - shineCenterF) / pLen;
                if (distPx < shineHalfWidthPx) {
                    pixel = CHSV(0, 0, 255); // solid white core
                } else if (distPx < shineHalfWidthPx + shineEdgeSoftPx) {
                    float edgeT = (distPx - shineHalfWidthPx) / shineEdgeSoftPx;
                    pixel = FxHelpers::mixHsv(CHSV(0, 60, 220), pixel, edgeT); // medium-light gray taper
                }
            }

            buffer[i] = CRGB(pixel);
        }
    }

} // namespace DeadlineTrophyFX

//apply effects to the draw function
uint16_t mode_DeadlineTrophy(void) {
    using namespace DeadlineTrophy;
    using namespace DeadlineTrophy::FxHelpers;

    float bpm = static_cast<float>(SEGMENT.speed);
    float beat = beatNow(bpm);
    float time = secondNow();

    if (SEGENV.call == 0) {
        // DEBUG_PRINTF("[DEADLINE_TROPHY] FX was called, now initialized for segment %d (%s) :)\n", strip.getCurrSegmentId(), SEGMENT.name);
        SEGMENT.fill(BLACK);
        // we use SEGENV.aux0 as a internal store for the random-flashes, but for no particular reason - stop me if you care
        SEGMENT.aux0 = 0;
    }

    // sample te logo pixels an derive average color, apply to base lighting
    static CRGB logoAverageColor = CRGB::Black;
    static CRGB backLedColor = CRGB::Black;

    if (IS_LOGO_SEGMENT) {
        using namespace DeadlineTrophyFX;

        float cyclePos = fmodf(time, FULL_CYCLE_DURATION);
        int currentSlot = static_cast<int>(cyclePos / SLOT_DURATION);
        float timeInSlot = cyclePos - currentSlot * SLOT_DURATION; // 0..SLOT_DURATION
        bool inTransition = timeInSlot >= STEADY_DURATION;
        int nextSlot = (currentSlot + 1) % N_LOGO_EFFECTS;
        float transitionProgress = inTransition ? (timeInSlot - STEADY_DURATION) / TRANSITION_DURATION : 0.f;

        static bool activeLastFrame[N_LOGO_EFFECTS] = {};
        bool activeThisFrame[N_LOGO_EFFECTS] = {};
        activeThisFrame[currentSlot] = true;
        if (inTransition) activeThisFrame[nextSlot] = true;
        bool enteringCurrent = activeThisFrame[currentSlot] && !activeLastFrame[currentSlot];
        bool enteringNext = activeThisFrame[nextSlot] && !activeLastFrame[nextSlot];

        static CRGB bufferCurrent[N_LEDS_LOGO];
        for (size_t i = 0; i < N_LEDS_LOGO; ++i) bufferCurrent[i] = CRGB::Black;
        logoEffects[currentSlot](bufferCurrent, time, beat, enteringCurrent);

        SEGMENT.fill(BLACK);
        if (!inTransition) {
            for (const auto& coord : logoCoordinates()) {
                setLogo(coord.x, coord.y, uint32_t(bufferCurrent[coord.index]));
            }
        } else {
            static CRGB bufferNext[N_LEDS_LOGO];
            for (size_t i = 0; i < N_LEDS_LOGO; ++i) bufferNext[i] = CRGB::Black;
            logoEffects[nextSlot](bufferNext, time, beat, enteringNext);

            WipeStyle style = wipeStyleForSlot[currentSlot];
            for (const auto& coord : logoCoordinates()) {
                float t = wipeBlendFactor(coord, style, transitionProgress);
                const CRGB& a = bufferCurrent[coord.index];
                const CRGB& b = bufferNext[coord.index];
                CRGB blended(mix8(a.r, b.r, t), mix8(a.g, b.g, t), mix8(a.b, b.b, t));
                setLogo(coord.x, coord.y, uint32_t(blended));
            }
        }

        for (int i = 0; i < N_LOGO_EFFECTS; ++i) activeLastFrame[i] = activeThisFrame[i];

        // Sample this frame's fully-rendered logo pixels and average them, for the
        // "Philips Hue"-style ambient back/floor LED effects to follow...
        {
            uint32_t sumR = 0, sumG = 0, sumB = 0;
            uint16_t n = SEGMENT.length();
            for (uint16_t i = 0; i < n; ++i) {
                CRGB c = CRGB(SEGMENT.getPixelColor(i));
                sumR += c.r;
                sumG += c.g;
                sumB += c.b;
            }
            if (n > 0) {
                CRGB avg(sumR / n, sumG / n, sumB / n);
                uint8_t maxChannel = max(avg.r, max(avg.g, avg.b));
                if (maxChannel > 0) {
                    float scale = 255.f / static_cast<float>(maxChannel);
                    avg.r = static_cast<uint8_t>(constrain(avg.r * scale, 0.f, 255.f) + 0.5f);
                    avg.g = static_cast<uint8_t>(constrain(avg.g * scale, 0.f, 255.f) + 0.5f);
                    avg.b = static_cast<uint8_t>(constrain(avg.b * scale, 0.f, 255.f) + 0.5f);
                }
                logoAverageColor = avg;
            }
        }
    }

    if (IS_BASE_SEGMENT) {
        // shimmer background-style ambient fill: every base pixel is colored by the
        // back LED's current (already-smoothed, see IS_BACK_LED below) color, then gets its own persistent random brightness offset
        static bool baseBgInitialized = false;
        static float baseBgPhaseSeed[N_LEDS_BASE];
        static float baseBgBaseFactor[N_LEDS_BASE]; // persistent per-pixel brightness bias, around 1.0
        if (!baseBgInitialized) {
            for (size_t i = 0; i < N_LEDS_BASE; ++i) {
                baseBgPhaseSeed[i] = static_cast<float>(hw_random16()) * (2.f * 3.14159265f / 65535.f);
                baseBgBaseFactor[i] = 0.75f + static_cast<float>(hw_random8()) / 255.f * 0.5f; // ~0.75..1.25
            }
            baseBgInitialized = true;
        }

        size_t i = 0;
        for (const auto& coord : baseCoordinates()) {
            float sineOffset = 0.15f * sinf(time * 1.3f + baseBgPhaseSeed[i]); // +-15% breathing
            float factor = clip(baseBgBaseFactor[i] + sineOffset);
            CRGB rgb = backLedColor;
            rgb.nscale8(static_cast<uint8_t>(factor * 255.f));
            setBase(coord.x, coord.y, uint32_t(rgb));
            ++i;
        }
    }

    if (IS_BACK_LED) {
        // "Philips Hue"-style ambient light: sample the logo segment's average
        static float backLedLastTime = -1.f;
        float dt = (backLedLastTime < 0.f) ? 0.f : (time - backLedLastTime);
        backLedLastTime = time;
        const float lerpDuration = 2.5f; // seconds to (roughly) catch up to the target color
        float lerpT = clip(dt / lerpDuration);
        backLedColor = CRGB(mixRgb(uint32_t(backLedColor), uint32_t(logoAverageColor), lerpT));

        SEGMENT.setPixelColorXY(0, 0, backLedColor);

    } else if (IS_FLOOR_LED) {
        // Same "Philips Hue"-style ambient LERP as the back LED, sampling the same
        static CRGB floorLedColor = CRGB::Black;
        static float floorLedLastTime = -1.f;
        float dt = (floorLedLastTime < 0.f) ? 0.f : (time - floorLedLastTime);
        floorLedLastTime = time;
        const float lerpDuration = 2.5f; // seconds to (roughly) catch up to the target color
        float lerpT = clip(dt / lerpDuration);
        floorLedColor = CRGB(mixRgb(uint32_t(floorLedColor), uint32_t(logoAverageColor), lerpT));

        SEGMENT.setPixelColorXY(0, 0, floorLedColor);

    }

    return FRAMETIME;
}

static const char _data_FX_MODE_DEADLINE_TROPHY[] PROGMEM =
    "DEADLINE Trophy@BPM,!,Contour Intensity,!,!;!,!;!;2;c1=0,sx=110,pal=59";
// <-- cf. https://kno.wled.ge/interfaces/json-api/#effect-metadata
// <EffectParameters>;<Colors>;<Palette>;<Flags>;<Defaults>
// <Colors> = !,! = Two Defaults
// <Palette> = ! = Default enabled (Empty would disable palette selection)
// <Flags> = 2 for "it needs the 2D matrix", add "v" and/or "f" for AudioReactive volume and frequency.
// <Defaults>: <ParameterCode>=<Value>
//             si=0 is "sound interaction" (for AudioReactive),
// <EffectParameters>: These are then read by (if defined, comma-separated)
//   sx = SEGMENT.speed (int 0-255)
//   ix = SEGMENT.intensity (int 0-255)
//   c1 = SEGMENT.custom1 (int 0-255)
//   c2 = SEGMENT.custom2 (int 0-255)
//   c3 = SEGMENT.custom3 (int 0-31)
//   o1 = SEGMENT.check1 (bool)
//   o2 = SEGMENT.check2 (bool)
//   o3 = SEGMENT.check3 (bool)
