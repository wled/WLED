#pragma once

#include "../FX.h"
#include "Effect.h"

class BouncingBallsEffect : public BaseEffect<BouncingBallsEffect> {
    struct Ball {
        unsigned long lastBounceTime{strip.now};
        float impactVelocity{};
        float height{};
        float pixelHeight{};
        uint32_t color{};
    };

private:
    using Base = BaseEffect<BouncingBallsEffect>;
    using Self = BouncingBallsEffect;
    static constexpr unsigned maxNumBalls = 16;

public:
    using Base::Base;

    static constexpr EffectInformation effectInformation {
        "Bouncing Balls@Gravity,balls per line,,,lines,,Overlay;!,!,!;!;1;m12=1",
        FX_MODE_BOUNCINGBALLS,
        0u,
        &Self::makeEffect,
        &Self::nextFrame,
        &Self::nextRow,
        &Self::getPixelColor,
    };

    void nextFrameImpl() {
        numBalls = (SEGMENT.intensity * (maxNumBalls - 1)) / 255 + 1; // minimum 1 ball
        strips = SEGMENT.custom3;
        useBackgroundColor = !SEGMENT.check2;

        if (useBackgroundColor)
            backgroundColor = (SEGCOLOR(2) ? BLACK : SEGCOLOR(1));
        ballSize = std::max(1u, SEG_H / strips);

        const size_t ballsVectorSize = maxNumBalls * strips; //TODO reduce to actual ball count instead of max ball count?
        balls.resize(ballsVectorSize);
        if (balls.size() != ballsVectorSize) {
            balls.clear();
            return;
        }
        balls.shrink_to_fit();

        for (unsigned stripNr = 0; stripNr < strips; ++stripNr)
            runVirtualStrip(stripNr, &balls[stripNr * maxNumBalls]);
    }

    void nextRowImpl(int y) {
        stripIndex = (y * strips) / SEG_W;
    }

    uint32_t getPixelColorImpl(int x, int y, const LazyColor& currentColor) {
        for (size_t ballIndex = 0; ballIndex < numBalls; ballIndex++) {
            const Ball& ball = balls[stripIndex * maxNumBalls + ballIndex];
            if (ball.pixelHeight - (ballSize / 2) <= x && x < ball.pixelHeight + ((ballSize + 1) / 2))
                return ball.color;
        }

        if (useBackgroundColor)
            return backgroundColor;
        else
            return currentColor.getColor(x, y);
    }

private:
    // virtualStrip idea by @ewowi (Ewoud Wijma)
    // requires virtual strip # to be embedded into upper 16 bits of index in setPixelColor()
    // the following functions will not work on virtual strips: fill(), fade_out(), fadeToBlack(), blur()
    void runVirtualStrip(size_t stripNr, Ball* balls) {
        constexpr float gravity = -9.81f; // standard value of gravity
        constexpr float initialVelocityFactor = 4.4294469f; // sqrtf(-2.0f * gravity);
        // number of balls based on intensity setting to max of 7 (cycles colors)
        // non-chosen color is a random color
        const unsigned long time = strip.now;

        const float bounceTimeFactor = 0.001f / ((255-SEGMENT.speed)/64 +1);
        for (size_t i = 0; i < numBalls; i++) {
            // time since last bounce in seconds
            const float timeSec = (time - balls[i].lastBounceTime) * bounceTimeFactor;
            balls[i].height = ((0.5f * gravity) * timeSec + balls[i].impactVelocity) * timeSec; // avoid use pow(x, 2) - its extremely slow !

            if (balls[i].height <= 0.0f) {
                balls[i].height = 0.0f;
                //damping for better effect using multiple balls
                const float dampening = 0.9f - float(i)/float(numBalls * numBalls); // avoid use pow(x, 2) - its extremely slow !
                balls[i].impactVelocity = dampening * balls[i].impactVelocity;
                balls[i].lastBounceTime = time;

                if (balls[i].impactVelocity < 0.015f) {
                    float impactVelocityStart = initialVelocityFactor * hw_random8(5,11)/10.0f; // randomize impact velocity
                    balls[i].impactVelocity = impactVelocityStart;
                }
            } else if (balls[i].height > 1.0f) {
                continue; // do not draw OOB ball
            }
            balls[i].pixelHeight = balls[i].height * (SEGLEN - 1);

            //TODO currently evaluated once per virtual strip, but the result is the same for all strips
            if (SEGMENT.palette) {
                balls[i].color = SEGMENT.color_wheel(i*(256/MAX(numBalls, 8)));
            } else if (SEGCOLOR(2)) {
                balls[i].color = SEGCOLOR(i % NUM_COLORS);
            } else {
                balls[i].color = SEGCOLOR(0);
            }
        }
    }

private:
    unsigned numBalls;
    unsigned strips;
    std::vector<Ball> balls;
    uint32_t backgroundColor;
    bool useBackgroundColor;
    int stripIndex;
    int ballSize;
};
