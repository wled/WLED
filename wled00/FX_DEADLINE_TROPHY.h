/** Deadline Release Notes 09.30.000000000000000000000000000000000000000002026
 * 
 * / haha, ok it evolved around the ship
 * / base effekt is the rampup&down pixelsetting, controllable via gui sliders
 * / hard coded to 120 fps, one of the fps matching bpms
 * / ai aka li (lamer intelligence) has been used, lols mostly for the config string at that bottom, but as well for explaining doing stuff, i confess
 * / super messed up coding style, applying c style experience and copy pasting as hell, no real structure or reference (eh, this is meant only for li to not learn from my changes)
 * / only non paid li services used
 *  
 */


#pragma once

#include "FX.h"
#include "../usermods/DEADLINE_TROPHY/DeadlineTrophy.h"
#include <cmath> // Notwendig fuer std::sin
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

namespace DeadlineTrophy {
    // ------------------------------------------------------------------
    // Ramp buffer for all trophy lamps (logo + base).
    // Drawing code no longer writes colors straight to the LEDs. Instead it
    // sets a per-lamp *target* color each frame via setLogoTarget/setBaseTarget.
    // Every frame the buffer eases the currently shown color towards that target
    // (ramp up / "attack"). As soon as a lamp is no longer targeted its target
    // is black, so it fades back out (ramp down / "release").
    // => a single setPixel-style call becomes a smooth, dynamic animation.
    // ------------------------------------------------------------------


    // lol, so we are incapable of managing any order in this code, so we brutally
    // hack ours on top of most existing code, the plan is the following>
    /*
    1st scene / fade in ship
    2nd scene / animate shipt
    3rd scene / ship shoot zoomed ball basically
    4th scene / bouncing balls effect sized bigger than just 1 pix
    5th scene / some fancy fadeout to allow fade in ship perform nicely
    // repeat with slightly changed colors
    
    
    */

    int currentSceneIndex=3;


    namespace Ramp {
        using namespace FxHelpers;

        // grid-indexed buffers (index = y * width + x)
        static CRGB logoCurrent[logoW * logoH] = {};
        static CRGB logoTarget[logoW * logoH]  = {};
        static CRGB baseCurrent[baseSize * baseSize] = {};
        static CRGB baseTarget[baseSize * baseSize]  = {};

        // per-frame ramp step per 8-bit channel (0..255)
        static uint8_t attack  = 40;  // ramp up speed (bigger = faster color gain)
        static uint8_t release = 12;  // ramp down / fade speed (bigger = faster fade out)

        inline void clearLogoTargets() { memset(logoTarget, 0, sizeof(logoTarget)); }
        inline void clearBaseTargets() { memset(baseTarget, 0, sizeof(baseTarget)); }

        // combine overlapping draws within one frame by keeping the brightest channel
        inline void maxInto(CRGB& t, const CRGB& c) {
            if (c.r > t.r) t.r = c.r;
            if (c.g > t.g) t.g = c.g;
            if (c.b > t.b) t.b = c.b;
        }

        // thresholdInPercent von 0 bis 100
inline void setLogoTargetWithRandomSetPropability(int x, int y, uint32_t color, uint8_t chancePercent = 50) {
    if (x < 0 || y < 0 || x >= logoW || y >= logoH) return;
    
    // FastLED-Zufall (0-99)
    if (random8(100) >= chancePercent) return; 

    maxInto(logoTarget[y * logoW + x], CRGB(color & 0x00FFFFFF));
}
inline void setBaseTargetWithRandomSetPropability(int x, int y, uint32_t color, uint8_t chancePercent = 50) {
        if (x < 0 || y < 0 || x >= baseSize || y >= baseSize) return;
    // FastLED-Zufall (0-99)
    if (random8(100) >= chancePercent) return; 
            maxInto(baseTarget[y * baseSize + x], CRGB(color & 0x00FFFFFF));
}
        inline void setLogoTarget(int x, int y, uint32_t color) {
            if (x < 0 || y < 0 || x >= logoW || y >= logoH) return;
            maxInto(logoTarget[y * logoW + x], CRGB(color & 0x00FFFFFF));
        }
        inline void setBaseTarget(int x, int y, uint32_t color) {
            if (x < 0 || y < 0 || x >= baseSize || y >= baseSize) return;
            maxInto(baseTarget[y * baseSize + x], CRGB(color & 0x00FFFFFF));
        }

        // set a whole predefined logo path (by LED index) to a scaled target color
        // lol, phase is used to shift everythiung horizontallz for some kind of scrolling
        inline void fillLogoTarget(const uint8_t* pixels, size_t n, uint32_t color, float scale = 1.f,uint phase=0) {
            CRGB c(color & 0x00FFFFFF);
            float s = constrain(scale, 0.f, 1.f);
            c.r = static_cast<uint8_t>(c.r * s);
            c.g = static_cast<uint8_t>(c.g * s);
            c.b = static_cast<uint8_t>(c.b * s);
            uint32_t scaled = uint32_t(c);
            for (size_t i = 0; i < n; i++) {
                Coord co = Logo::coord(pixels[i]);
                // letsmake it blink by setting it with high set prop, but blink :) as in tv monitors
                setLogoTargetWithRandomSetPropability(co.x+phase, co.y, scaled,75);
            }
        }

        inline void fillLogoTargetGradient(const uint8_t* pixels, size_t n, uint32_t color, uint32_t color2, float scale = 1.f, uint phase = 0) {
    if (n == 0) return;

    CRGB c1(color & 0x00FFFFFF);
    CRGB c2(color2 & 0x00FFFFFF);
    float s = constrain(scale, 0.f, 1.f);

    for (size_t i = 0; i < n; i++) {
        // Linearer Verlauf über die Pixel-Indizes (0.0 bis 1.0)
        uint8_t amount = (n > 1) ? static_cast<uint8_t>((i * 255) / (n - 1)) : 0;
        
        // Farben mischen (FastLED blend)
        CRGB blended = blend(c1, c2, amount);

        // Skalierung anwenden
        blended.r = static_cast<uint8_t>(blended.r * s);
        blended.g = static_cast<uint8_t>(blended.g * s);
        blended.b = static_cast<uint8_t>(blended.b * s);

        uint32_t scaled = uint32_t(blended);

        Coord co = Logo::coord(pixels[i]);
        setLogoTargetWithRandomSetPropability(co.x + phase, co.y, scaled, 75);
    }
}
         // Setzt ein Logo-Pixel sofort – umgeht das spätere Ramping in flushLogo()
    inline void setLogoDirect(int x, int y, uint32_t color) {
        if (x < 0 || y < 0 || x >= logoW || y >= logoH) return;
        int idx = y * logoW + x;
        CRGB c(color & 0x00FFFFFF);
        
        maxInto(logoTarget[idx], c);
        maxInto(logoCurrent[idx], c); // Setzt den aktuellen Zustand sofort auf die Ziel-Farbe
    }
    // Liefert die aktuelle Ball-Höhe (0.0 = Boden, 1.0 = Höchster Punkt)
    // beatsPerBounce: z.B. 2.0f für einen Sprung alle 2 Beats
    inline float getBpmBounceHeight(float beat, float beatsPerBounce = 2.0f) {
        // 1. Phasenfortschritt von 0.0 bis 1.0 innerhalb des gewählten Beat-Intervalls
        float phase = fmod(beat, beatsPerBounce) / beatsPerBounce;
        
        // 2. Parabel: Erzeugt perfektes Abbremsen oben und harten Aufprall unten (0 -> 1 -> 0)
        // Formula: 1.0 - (2*phase - 1)^2
        float normalizedHeight = 1.0f - (2.0f * phase - 1.0f) * (2.0f * phase - 1.0f);
        
        return constrain(normalizedHeight, 0.0f, 1.0f);
    }

    // Setzt ein Base-Pixel sofort
    inline void setBaseDirect(int x, int y, uint32_t color) {
        if (x < 0 || y < 0 || x >= baseSize || y >= baseSize) return;
        int idx = y * baseSize + x;
        CRGB c(color & 0x00FFFFFF);

        maxInto(baseTarget[idx], c);
        maxInto(baseCurrent[idx], c);
    }
        inline void fillLogoTargetGradientDirect(const uint8_t* pixels, size_t n, uint32_t color, uint32_t color2, float scale = 1.f, uint phase = 0) {
    if (n == 0) return;

    CRGB c1(color & 0x00FFFFFF);
    CRGB c2(color2 & 0x00FFFFFF);
    float s = constrain(scale, 0.f, 1.f);

    for (size_t i = 0; i < n; i++) {
        // Linearer Verlauf über die Pixel-Indizes (0.0 bis 1.0)
        uint8_t amount = (n > 1) ? static_cast<uint8_t>((i * 255) / (n - 1)) : 0;
        
        // Farben mischen (FastLED blend)
        CRGB blended = blend(c1, c2, amount);

        // Skalierung anwenden
        blended.r = static_cast<uint8_t>(blended.r * s);
        blended.g = static_cast<uint8_t>(blended.g * s);
        blended.b = static_cast<uint8_t>(blended.b * s);

        uint32_t scaled = uint32_t(blended);

        Coord co = Logo::coord(pixels[i]);
//        setLogoTargetWithRandomSetPropability(co.x + phase, co.y, scaled, 75);
        setLogoDirect(co.x + phase, co.y, scaled);
    }
   
}

// Gibt eine 32-Bit RGB-Farbe (0x00RRGGBB) basierend auf dem Beat zurück
    inline uint32_t getBeatHSVColor(float beat, float speedFactor = 0.25f, uint8_t sat = 255, uint8_t val = 255) {
        uint8_t hue = static_cast<uint8_t>(beat * speedFactor * 255.0f);
        CRGB c = CHSV(hue, sat, val); // FastLED konvertiert CHSV automatisch nach CRGB
        return uint32_t(c);
    }

    // Liefert die exakte Komplementärfarbe (+128 / 180° auf dem Farbkreis)
    inline uint32_t getBeatHSVComplementaryColor(float beat, float speedFactor = 0.25f, uint8_t sat = 255, uint8_t val = 255) {
        uint8_t hue = static_cast<uint8_t>(beat * speedFactor * 255.0f) + 128;
        CRGB c = CHSV(hue, sat, val);
        return uint32_t(c);
    }
        inline uint8_t rampChannel(uint8_t cur, uint8_t tgt) {
            if (tgt > cur) {
                int v = cur + attack;
                return v < tgt ? static_cast<uint8_t>(v) : tgt;
            }
            int v = cur - release;
            return v > tgt ? static_cast<uint8_t>(v) : tgt;
        }


        inline CRGB rampColor(const CRGB& cur, const CRGB& tgt) {
            return CRGB(rampChannel(cur.r, tgt.r),
                        rampChannel(cur.g, tgt.g),
                        rampChannel(cur.b, tgt.b));
        }

        // ramp every lamp towards its target and push the result to the segment
        inline void flushLogo() {
            for (const auto& coord : logoCoordinates()) {
                int i = coord.y * logoW + coord.x;
                logoCurrent[i] = rampColor(logoCurrent[i], logoTarget[i]);
                setLogo(coord.x, coord.y, uint32_t(logoCurrent[i]));
            }
        }
        inline void flushBase() {
            for (const auto& coord : baseCoordinates()) {
                int i = coord.y * baseSize + coord.x;
                baseCurrent[i] = rampColor(baseCurrent[i], baseTarget[i]);
                setBase(coord.x, coord.y, uint32_t(baseCurrent[i]));
            }
        }

  // Cordinate utils
    // Dimension Matrix
    #define MATRIX_WIDTH  26
    #define MATRIX_HEIGHT 12
#define UNUSED_PIXEL  -1 // Platzhalter für deine '__'
    /**
     * Liest den Mapping-Wert an der Koordinate (x, y) aus.
     * 
     * @param x Spalte (0 bis MATRIX_WIDTH - 1)
     * @param y Zeile  (0 bis MATRIX_HEIGHT - 1)
     * @return Der uint16_t Wert oder -1 bei Grenzüberschreitung.
     */
  inline  uint16_t getMappingValue(uint8_t x, uint8_t y) {
        if (x >= MATRIX_WIDTH || y >= MATRIX_HEIGHT) {
            return UNUSED_PIXEL; // Ungültige Koordinate (Fehlercode)
        }

        // 2D-Koordinate in 1D-Flachindex umrechnen
        uint16_t index = (uint16_t)y * MATRIX_WIDTH + x;

        return mappingTable[index];
    }
// Hilfsfunktion: Setzt Pixel nur, wenn die Koordinate ein gültiges Mapping hat
inline void drawPixel(int16_t x, int16_t y, uint32_t color) {
    if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) return;

    uint16_t ledIndex = getMappingValue((uint8_t)x, (uint8_t)y);
    if (ledIndex != UNUSED_PIXEL) {
       // setLedColor(ledIndex, color);
           Coord co = Logo::coord(ledIndex);
        setLogoDirect(co.x, co.y, color);
    }
}
/**
 * Zeichnet einen Kreis auf der Matrix.
 * 
 * @param x0     Mittelpunkt X
 * @param y0     Mittelpunkt Y
 * @param radius Radius des Kreises
 * @param color  Farbe (z. B. 0xFF0000 für Rot)
 */
void drawCircle(int16_t x0, int16_t y0, int16_t radius, uint32_t color) {
    int16_t x = radius;
    int16_t y = 0;
    int16_t err = 0;

    while (x >= y) {
        // 8-fache Symmetrie ausnutzen
    drawPixel(x0 + x, y0 + y, color);
        drawPixel(x0 + y, y0 + x, color);
        drawPixel(x0 - y, y0 + x, color);
        drawPixel(x0 - x, y0 + y, color);
        drawPixel(x0 - x, y0 - y, color);
        drawPixel(x0 - y, y0 - x, color);
        drawPixel(x0 + y, y0 - x, color);
        drawPixel(x0 + x, y0 - y, color);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

} 

    }

uint16_t mode_DeadlineTrophy(void) {  
   
    SEGMENT.speed=120; // boh, trying to hard code speed totally 
    using namespace DeadlineTrophy;
    using namespace DeadlineTrophy::FxHelpers;
    CRGB segcolor0 = CRGB(SEGCOLOR(0));
    CHSV color = rgb2hsv_approximate(segcolor0);
    CHSV color_(color);
    float hue = static_cast<float>(color.hue);
 
    float bpm = static_cast<float>(SEGMENT.speed);
    float beat = beatNow(bpm); 
    uint beatInt=uint(beat);
    float fractBeat = fmodf(beat, 1.);
    float time = secondNow(); 
    
    // ok wir haben 5 scenen 200beats
    // 40, 40, 40, 40, 40 -> ALLE scenen gleich lang
    // 40, 20, 80, 20, 40 
   // currentSceneIndex=(beatInt%200)/40;
// Relativer Beat im 200-Beat-Zyklus (0 bis 199)

uint cycleBeat = beatInt % 200;
if (cycleBeat < 40) {
    currentSceneIndex = 0; // Beats 0-39   (Länge 40)
} else if (cycleBeat < 60) {
    currentSceneIndex = 1; // Beats 40-59  (Länge 20)
} else if (cycleBeat < 140) {
    currentSceneIndex = 2; // Beats 60-139 (Länge 80)
} else if (cycleBeat < 160) {
    currentSceneIndex = 3; // Beats 140-159 (Länge 20)
} else {
    currentSceneIndex = 4; // Beats 160-199 (Länge 40)
} 

    // 1. Wie viele Schritte (Sub-Beats) verstreichen pro Sekunde?
    // Bei 60 BPM und z.B. 4 Schritten pro Beat (1/16 Noten) = 4 Steps/Sekunde.
    float stepsPerSecond = (bpm / 60.0f) * 4.0f; // 4 Steps pro Beat

    // 2. Aktueller absoluter Schritt als Ganzzahl (diskret!)
    uint32_t currentStep = static_cast<uint32_t>(time * stepsPerSecond);

    // 3. Wie weit sind wir im aktuellen Schritt (0.0 bis 1.0 für Fades)
    float stepPhase = fmodf(time * stepsPerSecond, 1.0f);

    // ramp buffer speeds are live-controllable via the custom sliders
    //Ramp::attack  = SEGMENT.custom1 ? SEGMENT.custom1 : 1;
    //Ramp::release = SEGMENT.custom2 ? SEGMENT.custom2 : 1;
    
    // Sobald im UI ein anderer Wert eingestellt wird, gilt der UI-Wert.
    Ramp::attack  = (SEGMENT.custom1 == 128 || SEGMENT.custom1 == 0) ? 32: SEGMENT.custom1;
    Ramp::release = (SEGMENT.custom2 == 128 || SEGMENT.custom2 == 0) ? 16 : SEGMENT.custom2;

    if (SEGENV.call == 0) {
        // DEBUG_PRINTF("[DEADLINE_TROPHY] FX was called, now initialized for segment %d (%s) :)\n", strip.getCurrSegmentId(), SEGMENT.name);
        SEGMENT.fill(BLACK);
        // we use SEGENV.aux0 as a internal store for the random-flashes, but for no particular reason - stop me if you care
        SEGMENT.aux0 = 0;
    }

    static Vec2 triangleLeft = Logo::coord(138).uv;
    static Vec2 triangleRight = Logo::coord(103).uv;

    if (IS_LOGO_SEGMENT) {
        // QM: that width is useful for a point with exp(-r*r/w/w), values go from [1/8..1]
        // float width = exp2f((static_cast<float>(SEGMENT.intensity) - 192.) / 64.);
        // mapping 0..255 to [1/4 .. 4] with centered 128 = 1:
        float k = exp2f((static_cast<float>(SEGMENT.intensity) - 128.) / 32.);
        static float k_factor = 1.;
        // custom3 is only 5bit (0..31)
        float w = exp2f((static_cast<float>(SEGMENT.custom3) - 16.) / 8.);

        EVERY_NTH_CALL(5000) {
            measureMicros();
        }
        
      


        // clear only the targets; the buffer keeps the currently lit colors and
        // fades them out (ramp down) wherever we don't set a target this frame.
        Ramp::clearLogoTargets();
        int index=0;
        for (const auto& coord : logoCoordinates()) {
            index++;
            float d = coord.uv.length();
            d = sin_t(M_TWOPI * (d * k * k_factor + w * beat));
            d *= d;
            d = 0.02 / d;

            // nach Polarwinkel unterdrücken...
            float theta = coord.uv.polarAngleFrom({0, 0}, 0.25 * beat * M_TWOPI);
            // aber reicht alle 8 Takte mal.
            float every8Beats = fmodf(beat, 8.);
            float gaussCurveExponent = (every8Beats - 6.) / 2;
            float swirlIntensity = exp(-sq(gaussCurveExponent));
            d *= exp(-theta * swirlIntensity);

            // at some other time, make k large and suppress points
            gaussCurveExponent = (fmodf(beat, 16.) - 9.) / 2.4;
            k_factor = 1. + exp(-sq(gaussCurveExponent));
            d = powf(d, sq(k_factor));

            color.hue = static_cast<uint8_t>(hue - 3. * theta);
            color.sat = color_.sat;
            color.val = static_cast<uint8_t>(50. * clip(d));

            // as an example of controlling the parameters. draws a gauss curve / circle around teh given point, white.
            /*
            d = 255. * coord.gaussAt(center, 0.5 * k, w - 1.);
            color.val = max(color.val, static_cast<uint8_t>(d));
            color.sat = min(color.sat, static_cast<uint8_t>(255. - d));
            */

            // draw one triangle to show usage of left/right tilt vectors
            CHSV triangleColor = CHSV(230, 200, 170);
            Vec2 pointForLeft = triangleLeft + (-8.f + 24.f * perlin1D(10. * beat)) * Logo::xUnit;
            d = coord.sdLine(pointForLeft - 10. * Logo::tiltRight, pointForLeft + 10. * Logo::tiltRight);
            color = mixHsv(color, triangleColor, exp(-6. * d));
            Vec2 pointForRight = triangleRight + (3.f - 7.f * perlin1D(10. * beat + 4.)) * Logo::xUnit;
            d = coord.sdLine(pointForRight - 10. * Logo::tiltLeft, pointForRight + 10. * Logo::tiltLeft);
            color = mixHsv(color, triangleColor, exp(-7. * d));
            float bottomLineHeight = triangleLeft.y + Logo::unit * (-1.f + 5.f * perlin1D(6. * beat + 100.));
            d = coord.sdLine({-10., bottomLineHeight}, {10., bottomLineHeight});
            color = mixHsv(color, triangleColor, exp(-8. * d));

            // EVERY_NTH_CALL(210) {
            //     DEBUG_PRINTF("[QM_DEBUG_LOGO] (%.3f, %.3f)->(%.3f, %.3f) (%.3f, %.3f)->(%.3f, %.3f) bottomY=%.3f, d=%.3f\n",
            //         triangleLeft.x, triangleLeft.y, pointForLeft.x, pointForLeft.y,
            //         triangleRight.x, triangleRight.y, pointForRight.x, pointForRight.y,
            //         bottomLineHeight, d
            //     );
            // } 
            // simple logo painting, cut in the rows
            if(index<9) {
                // First row, white
           // color.raw[0]=(float(index) /float(N_LEDS_LOGO))*255; 
            color.raw[0]=0; 
            color.raw[1]=255;
            color.raw[2]=255; 
            }else if(index<20){
            color.raw[0]=0; 
            color.raw[1]=55;
            color.raw[2]=255; 
            }else if(index<96){
            color.raw[0]=0; 
            color.raw[1]=128;
            color.raw[2]=255; 
            }else{
            color.raw[0]=0; 
            color.raw[1]=0;
            color.raw[2]=255; 
            }

// ok hehe, weiss haben wir nun was machen wir mal als starter, will ja nen shepperd effekt machen
// der geht mit hsv wahrscheinlich cool, also es soll das gesamte logo mit 2 farben die wechseln auch blau
// wie ein shepperd effekt wirken, mal sehen....



          //  Ramp::setLogoTarget(coord.x, coord.y, uint32_t(CRGB(color)));
        }

        // now, about these random draws we did somewhere above
        /*
        if (SEGMENT.aux0 > 150) {
            // these are just two exp decays one eighth of a beat apart
            float lastExpPeakAt = fractBeat < 0.125 ? 0. : 0.125;
            float exponent = fractBeat < 0.125 ? 5. : 1.4;
            float flashIntensity =
                exp(-exponent * (fractBeat-lastExpPeakAt) * static_cast<float>(fractBeat >= lastExpPeakAt));
            if (fractBeat < 0.125) {
                fillLogoArray(Logo::InnerTriangle.data(), Logo::InnerTriangle.size(),
                              RGBW32(200, 255, 50, 0), flashIntensity);
            } else {
                fillLogoArray(Logo::OuterTriangle.data(), Logo::OuterTriangle.size(),
                              RGBW32(210, 110, 10, 0), flashIntensity);
            }
        }
        */

        using namespace Logo;

        // hehe the ship
        
// Papierschiffchen (Paper Boat) LED Setup
const std::array<uint8_t, 32> PaperBoatHull = {{
    // Untere Rumpflinie & Schiffsspitzen (Kiel + Bug/Heck)
    160, 137, 136, 64, 69, 70, 75, 76, 81, 82, 99, 100, 101, 102, 103, 108,
    // Obere Rumpfkante (Deck)
    159, 138, 135, 65, 68, 71, 74, 77, 80, 83, 98, 91, 90, 89, 88, 87
}};

const std::array<uint8_t, 16> PaperBoatSailLeft = {{
    // Linkes Diagonalsegel (vom Deck zur oberen Spitze)
    140, 141, 142, 143, 144, 145, 146, 147, 148,
    150, 151, 152, 153, 154, 155, 156
}};

const std::array<uint8_t, 13> PaperBoatSailRight = {{
    // Rechtes Diagonalsegel (von der oberen Spitze zum rechten Deck)
    125, 124, 123, 122, 120, 119, 118, 117, 114, 113, 111, 110, 109
}};

const std::array<uint8_t, 13> PaperBoatMast = {{
    // Mittlerer Segelfalz / Mast (Trennlinie in der Mitte)
    126, 127, 128, 129, 130, 131, 132, 133, 134, 112, 115, 116
}};

// Gesamtes Papierschiffchen in einer geschlossenen Sequenz
const std::array<uint8_t,37> PaperBoatFull = {{
// Mast          
                 167, 168,
                 153, 154,
                 141, 142,                
                 132, 133,
                 
// decorations
161,103,104,

                 // BottomBar

              160, 137, 136, 64, 69,  70, 75, 76, 81, 82, 99, 100,
              159, 138, 135, 65, 68, 71, 74, 77, 80, 83, 98, 101,
                102,66
}};
 
const std::array<uint8_t,10> PaperBoatUpperDeck = {{ 
 158, 139,134,   67,  72, 73,  78, 79,  84, 97, 
}}; 
const std::array<uint8_t,16> Background = {{ 
    162,163,
    157,156,155,
    140,
    133,132,131, 
    85,86,87,
    96,95,94,105

    

    
}};
const std::array<uint8_t,45> BackgroundSky = {{ 
    164,165,166,169,
    152,151,150,149,
     143,144,145,146,147,148,
    130,129,128,127,126,125,
    88,89,90,
    93,92,91,

    123,124,120,121,122,119,118,117,114,115,116,113,112,111,109,110,
    108,107,106

    
}};




        const std::array<uint8_t, 9> Logo__LeftMost1 = {{
             161, 162, 163, 164, 165, 166, 167, 168, 169
        }};
        const std::array<uint8_t, 11> Logo__LeftMost2 = {{
            159,158, 157, 156,155,154,153,152,151,150,149
        }};
        const std::array<uint8_t, 12> Logo__LeftMost3 = {{
            137,138,139,140,141,142,143,144,145,146,147,148
        }};

        const std::array<uint8_t, 12> Logo__LeftMost4 = {{
            136,135,134,133,132,131,130,129,128,127,126,125
        }};
        const std::array<uint8_t, 5> Logo__LeftMost5 = {{
           64,65,66,123,124
        }};
        const std::array<uint8_t, 6> Logo__LeftMost6 = {{ 
            69,68,67,122,121,120
        }};
        const std::array<uint8_t, 6> Logo__LeftMost7 = {{
           70,71,72,117,118,119
        }};
        const std::array<uint8_t, 6> Logo__LeftMost8 = {{
            75,74,73,116,115,114
        }};
        const std::array<uint8_t, 6> Logo__LeftMost9 = {{
            76,77,78,111,112,113
        }};
        const std::array<uint8_t, 5> Logo__LeftMost10 = {{
         81,80,79,110,109
        }};

        const std::array<uint8_t, 8> Logo__LeftMost11 = {{
         82,83,84,85,86,87,89,90
        }};

        const std::array<uint8_t,9> Logo__LeftMost12 = {{

        99,98,97,96,95,94,93,92,91
        }};

        const std::array<uint8_t,  9> Logo__LeftMost13 = {{
         100,101,102,103,104,105,106,107,108
        }};

// sine wave dot test



        static uint32_t contourColor = uint32_t(CRGB(30, 40, 120));
        static uint32_t waterColor  = uint32_t(CRGB(0, 0x55, 0x88));
        static uint32_t skyColor= uint32_t(CRGB(0x44, 0x88, 0xff));
        #define BROWN (uint32_t)0xA52A2A
        int contourIndex = static_cast<int>(beat * 8.) % 3;
        contourIndex=1;
        
        
        switch (currentSceneIndex)
        {
        case 2:{
    // the ship gfx, hehe, last thing will be some movein&out
           //  Ramp::fillLogoTarget(MiddleTriangle.data(), MiddleTriangle.size(), YELLOW, 1.003); 

            Ramp::fillLogoTarget(Background.data(), Background.size(), waterColor, 1.00); 
            Ramp::fillLogoTarget(BackgroundSky.data(), BackgroundSky.size(), waterColor, .750); 
           Ramp::fillLogoTarget(PaperBoatFull.data(), PaperBoatFull.size(), waterColor, .75); 
            Ramp::fillLogoTarget(PaperBoatUpperDeck.data(), PaperBoatUpperDeck.size(), waterColor, 0.5); 



int cycle = ((beatInt%200)-60) % 80;
int offset = 0;

if (cycle < 25) {
    // Phase 1 (0..24): Reinfahren von -25 bis 0
    offset = -25 + cycle; 
} else if (cycle < 55) {
    // Phase 2 (25..54): 30 Beats Pause bei 0
    offset = 0;
} else {
    // Phase 3 (55..79): Rausfahren von 0 bis +25
    offset = cycle - 55; 
}
           Ramp::fillLogoTargetGradientDirect(PaperBoatFull.data(), PaperBoatFull.size(), WHITE,WHITE, 1.,offset); 
            Ramp::fillLogoTargetGradientDirect(PaperBoatUpperDeck.data(), PaperBoatUpperDeck.size(), GRAY, GRAY,1.00,offset); 



  
 
  
            break;
}
        case 1:
        case 3:{
            


            Ramp::fillLogoTargetGradient(Logo__LeftMost1.data(),Logo__LeftMost1.size(), WHITE, WHITE,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost2.data(),Logo__LeftMost2.size(), WHITE,GRAY,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost3.data(),Logo__LeftMost3.size(), GRAY, GRAY,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost4.data(),Logo__LeftMost4.size(), WHITE, WHITE,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost5.data(),Logo__LeftMost5.size(), WHITE, WHITE,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost6.data(),Logo__LeftMost6.size(), WHITE, WHITE,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost7.data(),Logo__LeftMost7.size(), WHITE, WHITE,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost8.data(),Logo__LeftMost8.size(), BLUE, 0xaaaaaa,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost9.data(),Logo__LeftMost9.size(), BLUE, 0xaaaaaa,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost10.data(),Logo__LeftMost10.size(), BLUE, 0xaaaaaa,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost11.data(),Logo__LeftMost11.size(), BLUE, 0xaaaaaa,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost12.data(),Logo__LeftMost12.size(), BLUE, 0xaaaaaa,1.); 
            Ramp::fillLogoTargetGradient(Logo__LeftMost13.data(),Logo__LeftMost13.size(), BLUE, 0xaaaaaa,1.); 
          //  Ramp::fillLogoTarget(LeftmostBar.data(),LeftmostBar.size(), WHITE, 1.);             
          //  Ramp::fillLogoTarget(BottomBar.data(),BottomBar.size(), GRAY, 1.);             
          //  Ramp::fillLogoTarget(RightmostBar.data(),RightmostBar.size(), WHITE, 1.);             
            
            
        // bouncy ball 
    // 1. Höhe (0.0f bis 1.0f) berechnen – gesynced auf 2 Beats
float height = Ramp::getBpmBounceHeight(beat, 4.0f);

// 2. In Y-Matrix-Koordinaten umrechnen (Boden = 0 oder logoH-1, je nachdem wie dein Koordinatensystem liegt)
int ballY = static_cast<int>(height*0.65 * (logoH - 1));
 
//lol somehow reminding to fountains
Ramp::drawCircle(beatInt%24,ballY,beatInt%4,WHITE);
            break;  
        }
        case 0:{
        // bpm sync
        // 4takt display

    // 1. Beat in diskrete Taktschritte (0, 1, 2, 3) umwandeln
    int currentStep = static_cast<int>(floor(fmod(beat, 4.0)));
    
uint32_t mainColor =Ramp::getBeatHSVColor(float(beatInt/4), 0.125f);              // Langsamer Farbwechsel
uint32_t compColor = Ramp::getBeatHSVComplementaryColor(float(beatInt/4), 0.125f); // Gegenfarbe
    switch (currentStep) {
        
        case 0:
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost1.data(), Logo__LeftMost1.size(), mainColor, 0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost2.data(), Logo__LeftMost2.size(), mainColor, 0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost3.data(), Logo__LeftMost3.size(), mainColor, 0xaaaaaa, 1.0f);
            break;
        case 1:
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost4.data(), Logo__LeftMost4.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost5.data(), Logo__LeftMost5.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost6.data(), Logo__LeftMost6.size(), compColor,  0xaaaaaa, 1.0f);
            break;
        case 2:
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost7.data(), Logo__LeftMost7.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost8.data(), Logo__LeftMost8.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost9.data(), Logo__LeftMost9.size(), compColor,  0xaaaaaa, 1.0f);
            break;
        case 3:
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost10.data(), Logo__LeftMost10.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost11.data(), Logo__LeftMost11.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost12.data(), Logo__LeftMost12.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost13.data(), Logo__LeftMost13.size(), compColor,  0xaaaaaa, 1.0f);
            break;
    }   
    break;
    }   
        case 4:{
        // bpm sync
        // 4takt display

    // 1. Beat in diskrete Taktschritte (0, 1, 2, 3) umwandeln
    int currentStep = static_cast<int>(floor(fmod(beat, 4.0)));
    
uint32_t mainColor =Ramp::getBeatHSVColor(float(beatInt/4), 0.125f);              // Langsamer Farbwechsel
uint32_t compColor = Ramp::getBeatHSVComplementaryColor(float(beatInt/4), 0.125f); // Gegenfarbe
    switch (currentStep) {
        
        case 0:
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost11.data(), Logo__LeftMost1.size(), mainColor, 0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost12.data(), Logo__LeftMost2.size(), mainColor, 0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost13.data(), Logo__LeftMost3.size(), mainColor, 0xaaaaaa, 1.0f);
            break;
        case 1:
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost1.data(), Logo__LeftMost4.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost2.data(), Logo__LeftMost5.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost3.data(), Logo__LeftMost6.size(), compColor,  0xaaaaaa, 1.0f);
            break;
        case 2:
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost4.data(), Logo__LeftMost7.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost5.data(), Logo__LeftMost8.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost6.data(), Logo__LeftMost9.size(), compColor,  0xaaaaaa, 1.0f);
            break;
        case 3:
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost7.data(), Logo__LeftMost10.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost8.data(), Logo__LeftMost11.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost9.data(), Logo__LeftMost12.size(), compColor,  0xaaaaaa, 1.0f);
            Ramp::fillLogoTargetGradientDirect(Logo__LeftMost10.data(), Logo__LeftMost13.size(), compColor,  0xaaaaaa, 1.0f);
            break;
    }  
    break;
        }

        case 31:{
        // bouncy ball 
    // 1. Höhe (0.0f bis 1.0f) berechnen – gesynced auf 2 Beats
float height = Ramp::getBpmBounceHeight(beat, 4.0f);

// 2. In Y-Matrix-Koordinaten umrechnen (Boden = 0 oder logoH-1, je nachdem wie dein Koordinatensystem liegt)
int ballY = static_cast<int>(height*0.65 * (logoH - 1));
 
//lol somehow reminding to fountains
Ramp::drawCircle(beatInt%24,ballY,beatInt%4,WHITE);
break;
        }  
default:
            break;
        }


       if(currentSceneIndex==0){
       
         //   Ramp::drawPixel(0,8,0x00ff00);
         //   Ramp::drawCircle(16,val_cpp,3,0xff0000);

        } 

     

        // ramp all logo lamps towards their targets and push to the LEDs
        Ramp::flushLogo();

        EVERY_NTH_CALL(5000) {
            DEBUG_PRINTF("[QM_DEBUG_FX] Logo took %ld µs.\n", measureMicros());
        }

    }

    if (IS_BASE_SEGMENT) {
        static float phi = 0.;
        static float omega = 1.7;

        // ramp buffer handles the fade-out (ramp down), so just clear targets
        Ramp::clearBaseTargets();
uint index=0;
        for (const auto& coord : baseCoordinates()) {
            index++;
            FloatRgb col0 = cosinePalette(
                0.2 * time,
                {0.29, 0.22, 0.5},
                {0.43, 0.74, 0.74},
                {1.3, 0.7, 1.2},
                {0.85, 0.14, 0.83}
            );
            // read the "uvS" coordinate as s == 0 in the middle of every side, becoming (-)0.5 towards the corners
            float uvs = min(abs(coord.uv.x), abs(coord.uv.y));
            float d = (1. - cos_t(M_PI * 5. * uvs)) * (0.6 + 0.4 * sin_t(M_TWOPI * 0.125 * beat));
            col0.scale(d*d);
            col0.grade(0.8);
            float dimTheMiddle = 1. - 0.8 * cos(M_PI * uvs);
            col0.scale(dimTheMiddle);

            // now add a rotating line, showing the use of SDF geometry
            phi = 0.25 * beat * omega;
            Vec2 end{1, 1};
            end.rotate(phi);
            d = coord.sdLine(-end, end);
            d = exp(-2.5*d);

            color = col0.toCHSV();
            color.sat = mix8(255, 20, d);
            color.val = max(color.val, static_cast<uint8_t>(255.*d));

 
            switch (currentSceneIndex)
            {
            case 2:
                /* code */
                // blue waves at the bottom

                color.hue=142+beatInt/4 +int32_t(std::abs(std::sin(beat*2.0+index*0.01))*.2);
                color.value=230 ;
                color.saturation=255;
            Ramp::setBaseTargetWithRandomSetPropability(coord.x, coord.y, uint32_t(CRGB(color)),40);
 
                break;
                case 3:
                case 1:
                                color.hue=123+beatInt/4 +int32_t(std::abs(std::sin(beat*1.00+index*0.01)));
                color.value=225 ;
                color.saturation=255;
            Ramp::setBaseTargetWithRandomSetPropability(coord.x, coord.y, uint32_t(CRGB(color)),50);

            case 0:
            case 4:
            
                color.hue=beatInt/4;
                color.value=200 ;
                color.saturation=25;
            Ramp::setBaseTargetWithRandomSetPropability(coord.x, coord.y, uint32_t(CRGB(color)),60);
            
            default:
                break;
            }

        }

        // ramp all base lamps towards their targets and push to the LEDs
        Ramp::flushBase();
    }

   // for the first argument of beatsin8_t (accum88 type), "BPM << 8" just oscillates once per beat
    //uint8_t fourBeatSineWave = beatsin8_t(SEGMENT.speed << 6, 0, 255);
    uint8_t beatSineWave = beatsin8_t(SEGMENT.speed << 8, 0, 255);
    uint8_t annoyingBlink = mix8(255, 0, exp(-fractBeat));

    if (IS_BACK_LED) {
switch(currentSceneIndex){

    case 4:
    case 0:


        setSingle(annoyingBlink);
    break;
    case 1:
    case 2:
    case 3:

    if(beatInt%4==0){
        setSingle(128);
    }else{
        setSingle(0);
    }
    break;
 
 
}


    } else if (IS_FLOOR_LED) {

switch(currentSceneIndex){
   
    case 4:
    case 0:

        setSingle(annoyingBlink);
    break;
    
    case 1:
     case 2:
    case 3:
    if(beatInt%4==1){
        setSingle(128);
    }else{
        setSingle(0);
    }
    break;

    break;
 
    break;
 
}



    }

    return FRAMETIME;

}

static const char _data_FX_MODE_DEADLINE_TROPHY[] PROGMEM =
    "Mandelbrötchen 'Deadline Trophy' #1@,,Ramp Up,Ramp Down;!,!;!;2;;~Trophy Intro~";
        // <-- cf. https://kno.wled.ge/interfaces/json-api/#effect-metadata
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
 