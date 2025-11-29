#include "wled.h"

/*
 * Usermod for analog clock
 */
extern Timezone* tz;

class AnalogClockUsermod : public Usermod {
private:
    static constexpr uint32_t refreshRate = 50; // per second
    static constexpr uint32_t refreshDelay = 1000 / refreshRate;

    struct Segment {
        // config
        int16_t firstLed  = 0;
        int16_t lastLed   = 59;
        int16_t centerLed = 0;

        // runtime
        int16_t size;

        Segment() {
            update();
        }

        void validateAndUpdate() {
            if (firstLed < 0 || firstLed >= strip.getLengthTotal() ||
                    lastLed < firstLed || lastLed >= strip.getLengthTotal()) {
                *this = {};
                return;
            }
            if (centerLed < firstLed || centerLed > lastLed) {
                centerLed = firstLed;
            }
            update();
        }

        void update() {
            size = lastLed - firstLed + 1;
        }
    };

    // configuration (available in API and stored in flash)
    bool     enabled          = false;
    Segment  mainSegment;
    bool     hourMarksEnabled = true;
    uint32_t hourMarkColor    = 0xFF0000;
    uint32_t hourColor        = 0x0000FF;
    uint32_t minuteColor      = 0x00FF00;
    bool     secondsEnabled   = true;
    Segment  secondsSegment;
    uint32_t secondColor      = 0xFF0000;
    bool     blendColors      = true;
    uint16_t secondsEffect    = 0;

    // runtime
    bool     initDone         = false;
    uint32_t lastOverlayDraw  = 0;

    void validateAndUpdate() {
        mainSegment.validateAndUpdate();
        secondsSegment.validateAndUpdate();
        if (secondsEffect < 0 || secondsEffect > 1) {
            secondsEffect = 0;
        }
    }

    int16_t adjustToSegment(double progress, Segment const& segment) {
        int16_t led = segment.centerLed + progress * segment.size;
        return led > segment.lastLed
                ? segment.firstLed + led - segment.lastLed - 1
                : led;
    }

    void setPixelColor(uint16_t n, uint32_t c) {
        if (!blendColors) {
            strip.setPixelColor(n, c);
        } else {
            uint32_t oldC = strip.getPixelColor(n);
            strip.setPixelColor(n, qadd32(oldC, c));
        }
    }

    String colorToHexString(uint32_t c) {
        char buffer[9];
        sprintf(buffer, "%06X", c);
        return buffer;
    }

    bool hexStringToColor(String const& s, uint32_t& c, uint32_t def) {
        char *ep;
        unsigned long long r = strtoull(s.c_str(), &ep, 16);
        if (*ep == 0) {
            c = r;
            return true;
        } else {
            c = def;
            return false;
        }
    }

    void secondsEffectSineFade(int16_t secondLed, Toki::Time const& time) {
        uint32_t ms = time.ms % 1000;
        uint8_t b0 = (cos8_t(ms * 64 / 1000) - 128) * 2;
        setPixelColor(secondLed, scale32(secondColor, b0));
        uint8_t b1 = (sin8_t(ms * 64 / 1000) - 128) * 2;
        setPixelColor(inc(secondLed, 1, secondsSegment), scale32(secondColor, b1));
    }

    static inline uint32_t qadd32(uint32_t c1, uint32_t c2) {
        return RGBW32(
            qadd8(R(c1), R(c2)),
            qadd8(G(c1), G(c2)),
            qadd8(B(c1), B(c2)),
            qadd8(W(c1), W(c2))
        );
    }

    static inline uint32_t scale32(uint32_t c, fract8 scale) {
        return RGBW32(
            scale8(R(c), scale),
            scale8(G(c), scale),
            scale8(B(c), scale),
            scale8(W(c), scale)
        );
    }

    static inline int16_t dec(int16_t n, int16_t i, Segment const& seg) {
        return n - seg.firstLed >= i
                ? n - i
                : seg.lastLed - seg.firstLed - i + n + 1;
    }

    static inline int16_t inc(int16_t n, int16_t i, Segment const& seg) {
        int16_t r = n + i;
        if (r > seg.lastLed) {
            return seg.firstLed + n - seg.lastLed;
        }
        return r;
    }

public:
    AnalogClockUsermod() {
    }

    void setup() override {
        initDone = true;
        validateAndUpdate();
    }

    void loop() override {
        if (millis() - lastOverlayDraw > refreshDelay) {
            strip.trigger();
        }
    }

    void handleOverlayDraw() override {
        if (!enabled) {
            return;
        }

        lastOverlayDraw = millis();

        auto time = toki.getTime();
        double secondP = second(localTime) / 60.0;
        double minuteP = minute(localTime) / 60.0;
        double hourP = (hour(localTime) % 12) / 12.0 + minuteP / 12.0;

        if (hourMarksEnabled)         {
            for (int Led = 0; Led <= 55; Led = Led + 5)
            {
                int16_t hourmarkled = adjustToSegment(Led / 60.0, mainSegment);
                setPixelColor(hourmarkled, hourMarkColor);
            }
        }

        if (secondsEnabled) {
            int16_t secondLed = adjustToSegment(secondP, secondsSegment);

            switch (secondsEffect) {
                case 0: // no effect
                    setPixelColor(secondLed, secondColor);
                    break;

                case 1: // fading seconds
                    secondsEffectSineFade(secondLed, time);
                    break;
            }

            // TODO: move to secondsTrailEffect
            // for (uint16_t i = 1; i < secondsTrail + 1; ++i) {
            //     uint16_t trailLed = dec(secondLed, i, secondsSegment);
            //     uint8_t trailBright = 255 / (secondsTrail + 1) * (secondsTrail - i + 1);
            //     setPixelColor(trailLed, scale32(secondColor, trailBright));
            // }
        }

        setPixelColor(adjustToSegment(minuteP, mainSegment), minuteColor);
        setPixelColor(adjustToSegment(hourP, mainSegment), hourColor);
    }

    void addToConfig(JsonObject& root) override {
        validateAndUpdate();

        JsonObject top = root.createNestedObject(F("Analog Clock"));
        top[F("Overlay Enabled")]               = enabled;
        top[F("First LED (Main Ring)")]         = mainSegment.firstLed;
        top[F("Last LED (Main Ring)")]          = mainSegment.lastLed;
        top[F("Center/12h LED (Main Ring)")]    = mainSegment.centerLed;
        top[F("Hour Marks Enabled")]            = hourMarksEnabled;
        top[F("Hour Mark Color (RRGGBB)")]      = colorToHexString(hourMarkColor);
        top[F("Hour Color (RRGGBB)")]           = colorToHexString(hourColor);
        top[F("Minute Color (RRGGBB)")]         = colorToHexString(minuteColor);
        top[F("Show Seconds")]                  = secondsEnabled;
        top[F("First LED (Seconds Ring)")]      = secondsSegment.firstLed;
        top[F("Last LED (Seconds Ring)")]       = secondsSegment.lastLed;
        top[F("Center/12h LED (Seconds Ring)")] = secondsSegment.centerLed;
        top[F("Second Color (RRGGBB)")]         = colorToHexString(secondColor);
        top[F("Seconds Effect (0-1)")]          = secondsEffect;
        top[F("Blend Colors")]                  = blendColors;
    }

    bool readFromConfig(JsonObject& root) override {
        JsonObject top = root[F("Analog Clock")];

        bool configComplete = !top.isNull();

        String color;
        configComplete &= getJsonValue(top[F("Overlay Enabled")], enabled, false);
        configComplete &= getJsonValue(top[F("First LED (Main Ring)")], mainSegment.firstLed, 0);
        configComplete &= getJsonValue(top[F("Last LED (Main Ring)")], mainSegment.lastLed, 59);
        configComplete &= getJsonValue(top[F("Center/12h LED (Main Ring)")], mainSegment.centerLed, 0);
        configComplete &= getJsonValue(top[F("Hour Marks Enabled")], hourMarksEnabled, false);
        configComplete &= getJsonValue(top[F("Hour Mark Color (RRGGBB)")], color, F("161616")) && hexStringToColor(color, hourMarkColor, 0x161616);
        configComplete &= getJsonValue(top[F("Hour Color (RRGGBB)")], color, F("0000FF")) && hexStringToColor(color, hourColor, 0x0000FF);
        configComplete &= getJsonValue(top[F("Minute Color (RRGGBB)")], color, F("00FF00")) && hexStringToColor(color, minuteColor, 0x00FF00);
        configComplete &= getJsonValue(top[F("Show Seconds")], secondsEnabled, true);
        configComplete &= getJsonValue(top[F("First LED (Seconds Ring)")], secondsSegment.firstLed, 0);
        configComplete &= getJsonValue(top[F("Last LED (Seconds Ring)")], secondsSegment.lastLed, 59);
        configComplete &= getJsonValue(top[F("Center/12h LED (Seconds Ring)")], secondsSegment.centerLed, 0);
        configComplete &= getJsonValue(top[F("Second Color (RRGGBB)")], color, F("FF0000")) && hexStringToColor(color, secondColor, 0xFF0000);
        configComplete &= getJsonValue(top[F("Seconds Effect (0-1)")], secondsEffect, 0);
        configComplete &= getJsonValue(top[F("Blend Colors")], blendColors, true);

        if (initDone) {
            validateAndUpdate();
        }

        return configComplete;
    }

    uint16_t getId() override {
        return USERMOD_ID_ANALOG_CLOCK;
    }
};


static AnalogClockUsermod analog_clock;
REGISTER_USERMOD(analog_clock);