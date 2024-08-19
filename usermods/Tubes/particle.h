#pragma once

#include "wled.h"
#include "beats.h"
#include "options.h"

#define MAX_PARTICLES 80
#undef PARTICLE_PALETTES

#define DEFAULT_PARTICLE_VOLUME 64  // Default mic average is around 64 during music

class Particle;

typedef void (*ParticleFn)(Particle& particle, WS2812FX* leds);
uint8_t particleVolume = DEFAULT_PARTICLE_VOLUME;

extern void drawPoint(Particle& particle, WS2812FX* leds);


class Particle {
  public:
    Particle(uint16_t pos = 0, CRGB c=CRGB::White, PenMode p=Draw, uint32_t life=20000, ParticleFn fn=drawPoint) :
      born(0),
      lifetime(life),
      age(0),
      color(c),
      pen(p),
      brightness(192<<8),
      drawFn(fn),
      position(pos),
      velocity(0),
      gravity(0)
    {};

    BeatFrame_24_8 born;
    BeatFrame_24_8 lifetime;
    BeatFrame_24_8 age;

    CRGB color;
    PenMode pen;
    uint16_t brightness;
    ParticleFn drawFn;

    uint16_t position;
    int16_t velocity;
    int16_t gravity;
//    void (*die_fn)(Particle *particle) = NULL;

#ifdef PARTICLE_PALETTES
    CRGBPalette16 palette;   // 48 bytes per particle!?
#endif

  void update(BeatFrame_24_8 frame)
  {
    // Particles get brighter with the beat
    brightness = (scale8(particleVolume, 80) + 170) << 8;

    age = frame - born;
    position = udelta16(position, velocity);
    velocity = delta16(velocity, gravity);
  }

  uint16_t age_frac16(BeatFrame_24_8 age) const
  {
    if (age >= lifetime)
      return 65535;
    uint32_t a = age * 65536;
    return a / lifetime;
  }

  uint16_t udelta16(uint16_t x, int16_t dx) const
  {
    if (dx > 0 && 65535-x < dx)
      return 65335;
    if (dx < 0 && x < -dx)
      return 0;
    return x + dx;
  }
  
  int16_t delta16(int16_t x, int16_t dx) const
  {
    if (dx > 0 && 32767-x < dx)
      return 32767;
    if (dx < 0 && x < -32767 - dx)
      return -32767;
    return x + dx;
  }

  CRGB color_at(uint16_t age_frac) {
    // Particles get dimmer with age
    uint8_t a = age_frac >> 8;
    brightness = scale8((uint8_t)(brightness>>8), 255-a);

#ifdef PARTICLE_PALETTES
    // a black pattern actually means to use the current palette
    if (color == CRGB(0,0,0))
      return ColorFromPalette(palette, a, brightness);
#endif

    uint8_t r = scale8(color.r, brightness);
    uint8_t g = scale8(color.g, brightness);
    uint8_t b = scale8(color.b, brightness);
    return CRGB(r,g,b);
  }

  void draw(WS2812FX* leds) {
    drawFn(*this, leds);
  }

  void draw_with_pen(WS2812FX* leds, int pos, CRGB color) {
    CRGB c = CRGB(strip.getPixelColor(pos));
    CRGB new_color;

    switch (pen) {
      case Draw:  
        new_color = color;
        break;
  
      case Blend:
        new_color = c | color;
        break;
  
      case Erase:
        new_color = c & color;
        break;
  
      case Invert:
        new_color = -c;
        break;  

      case Brighten: {
        uint8_t t = color.getAverageLight();
        new_color = c + CRGB(t,t,t);
        break;  
      }

      case Darken: {
        uint8_t t = color.getAverageLight();
        new_color = c - CRGB(t,t,t);
        break;  
      }

      case Flicker: {
        uint8_t t = color.getAverageLight();
        if (millis() % 2) {
          new_color = c - CRGB(t,t,t);
        } else {
          new_color = c + CRGB(t,t,t);
        }
        break;  
      }

      case White:
        new_color = CRGB::White;
        break;  

      case Black:
        new_color = CRGB::Black;
        break;  

      default:
        // Unknown pen
        return;
    }

    strip.setPixelColor(pos, new_color);
  }

};

Particle particles[MAX_PARTICLES];
BeatFrame_24_8 particle_beat_frame;
uint8_t numParticles = 0;

void removeParticle(uint8_t i) {
  if (i >= numParticles)
    return;

  // coalesce current particle list
  int rest = numParticles - i;
  if (rest > 0) {
    memmove(&particles[i], &particles[i+1], sizeof(particles[0]) * rest);
  }

  numParticles -= 1;
}

void addParticle(Particle&& particle) {
  particle.born = particle_beat_frame;
  if (numParticles >= MAX_PARTICLES) {
    removeParticle(0);
  }
  particles[numParticles++] = particle;
}

void drawFlash(Particle& particle, WS2812FX* leds) {
  auto num_leds = leds->getLengthTotal();
  uint16_t age_frac = particle.age_frac16(particle.age);
  CRGB c = particle.color_at(age_frac);
  for (int pos = 0; pos < num_leds; pos++) {
    particle.draw_with_pen(leds, pos, c);
  }
}

void drawPoint(Particle& particle, WS2812FX* leds) {
  uint16_t age_frac = particle.age_frac16(particle.age);
  CRGB c = particle.color_at(age_frac);

  uint16_t pos = scale16(particle.position, leds->getLengthTotal() - 1);
  particle.draw_with_pen(leds, pos, c);
}

void drawRadius(Particle& particle, WS2812FX* leds, uint16_t pos, uint8_t radius, CRGB c, bool dim=true) {
  auto num_leds = leds->getLengthTotal();
  for (int i = 0; i < radius; i++) {
    uint8_t bright = dim ? ((radius-i) * 255) / radius : 255;
    nscale8(&c, 1, bright);

    uint8_t y = pos - i;
    if (y >= 0 && y < num_leds)
      particle.draw_with_pen(leds, y, c);

    if (i == 0)
      continue;

    y = pos + i;
    if (y >= 0 && y < num_leds)
      particle.draw_with_pen(leds, y, c);
  }
}

void drawPop(Particle& particle, WS2812FX* leds) {
  uint16_t age_frac = particle.age_frac16(particle.age);
  CRGB c = particle.color_at(age_frac);
  uint16_t pos = scale16(particle.position, leds->getLengthTotal() - 1);
  uint8_t radius = scale16((sin16(age_frac/2) - 32768) * 2, 8);

  drawRadius(particle, leds, pos, radius, c);
}

void drawBeatbox(Particle& particle, WS2812FX* leds) {
  uint16_t age_frac = particle.age_frac16(particle.age);
  CRGB c = particle.color_at(age_frac);
  uint16_t pos = scale16(particle.position, leds->getLengthTotal() - 1);
  uint8_t radius = 3;

  // Bump up the radius with any beats
  radius += scale8(particleVolume, 8);

  drawRadius(particle, leds, pos, radius, c, false);
}

