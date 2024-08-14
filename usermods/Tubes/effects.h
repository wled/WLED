#pragma once

#include "util.h"
#include "particle.h"
#include "virtual_strip.h"

void addGlitter(CRGB color=CRGB::White, PenMode pen=Draw) 
{
  addParticle(Particle(random16(), color, pen, 128));
}

void addSpark(CRGB color=CRGB::White, PenMode pen=Draw) 
{
  Particle particle {random16(), color, pen, 64};
  uint8_t r = random8();
  if (r > 128)
    particle.velocity = r;
  else
    particle.velocity = -(128 + r);
  addParticle(std::move(particle));
}

void addBeatbox(CRGB color=CRGB::White, PenMode pen=Draw) 
{
  Particle particle {random16(), color, pen, 256, drawBeatbox};
  addParticle(std::move(particle));
}

void addBubble(CRGB color=CRGB::White, PenMode pen=Draw) 
{
  Particle particle {random16(), color, pen, 1024, drawPop};
  particle.velocity = random16(0, 40) - 20;
  addParticle(std::move(particle));
}

void addFlash(CRGB color=CRGB::Blue, PenMode pen=Draw) 
{
  addParticle(Particle(random16(), color, pen, 256, drawFlash));
}

void addDrop(CRGB color, PenMode pen=Draw)
{
   Particle particle {65535, color, pen, 360};
   particle.velocity = -500;
   particle.gravity = -10;
   addParticle(std::move(particle));
}

class Effects {
  public:
    EffectMode effect=None;
    PenMode pen=Draw;
    BeatPulse beat;
    uint8_t chance;

  void load(EffectParameters &params) {
    effect = params.effect;
    pen = params.pen;
    beat = params.beat;
    chance = params.chance;
  }

  void update(VirtualStrip *strip, BeatFrame_24_8 beat_frame, BeatPulse beat_pulse) {
    if (!beat || beat_pulse & beat) {

      if (random8() <= chance) {
        CRGB color = strip->palette_color(random8());
  
        switch (effect) {
          case None:
            break;
      
          case Glitter:
            addGlitter(color, pen);
            break;
      
          case Beatbox1:
          case Beatbox2:
            addBeatbox(color, pen);
            if (effect == Beatbox2)
              addBeatbox(color, pen);
            break;
      
          case Bubble:
            addBubble(color, pen);
            break;
      
          case Spark:
            addSpark(color, pen);
            break;
      
          case Flash:
            addFlash(CRGB::White, pen);
            break;  
        }
      }
    }

    animate(beat_frame, beat_pulse);
  }

  void animate(BeatFrame_24_8 frame, uint8_t beat_pulse) {
    unsigned int len = numParticles;
    for (unsigned i=len; i > 0; --i) {
      Particle& particle = particles[i];
      particle.update(frame);
      if (particle.age > particle.lifetime) {
        removeParticle(i);
      }
    }
  }

  void draw(WS2812FX* leds) {
    uint8_t len = numParticles;
    for (uint8_t i=0; i<len; i++) {
      particles[i].draw(leds);
    }
  }
  
};


typedef struct {
  EffectParameters params;
  ControlParameters control;
} EffectDef;


static const EffectDef gEffects[] = {
  {{None}, {LongDuration}},
  {{Flash, Brighten, Beat, 40}, {MediumDuration, HighEnergy}},
  {{Flash, Darken, TwoBeats, 40}, {MediumDuration, HighEnergy}},
  {{Flash, Brighten, Measure}, {ShortDuration, MediumEnergy}},
  {{Flash, Brighten, Phrase}, {MediumDuration, MediumEnergy}},
  {{Flash, Darken, Measure}, {ShortDuration, MediumEnergy}},
  {{Glitter, Brighten, Eighth, 40}, {ShortDuration, Chill}},
  {{Glitter, Brighten, Eighth, 80}, {MediumDuration, MediumEnergy}},
  {{Glitter, Brighten, Eighth, 40}, {MediumDuration, HighEnergy}},
  {{Glitter, Darken, Eighth, 40}, {MediumDuration, Chill}},

  {{Glitter, Draw, Eighth, 10}, {LongDuration, Chill}},
  {{Glitter, Draw, Eighth, 120}, {MediumDuration, Chill}},
  {{Glitter, Invert, Eighth, 40}, {ShortDuration, Chill}},
  {{Beatbox2, Black}, {MediumDuration, MediumEnergy}},
  {{Beatbox2, Black}, {MediumDuration, MediumEnergy}},
  {{Beatbox2, Draw}, {ShortDuration, HighEnergy}},
  {{Bubble, Darken}, {MediumDuration, Chill}},
  {{Bubble, Brighten}, {MediumDuration, Chill}},
  {{Bubble, Brighten}, {MediumDuration, Chill}},
  {{Bubble, Darken}, {MediumDuration, Chill}},

  {{Bubble, Invert}, {MediumDuration, Chill}},
  {{Bubble, Invert}, {MediumDuration, Chill}},
  {{Glitter, Darken, Eighth, 120}, {MediumDuration, MediumEnergy}},
  {{Glitter, Flicker, Eighth, 120}, {MediumDuration, Chill}},
};
const uint8_t gEffectCount = ARRAY_SIZE(gEffects);
