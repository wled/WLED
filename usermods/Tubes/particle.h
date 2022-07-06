#pragma once

#include "wled.h"
#include "beats.h"
#include "options.h"
// #include "ustd.h"

#define MAX_PARTICLES 20
#undef PARTICLE_PALETTES

class Particle;

typedef void (*ParticleFn)(Particle *particle, CRGB strip[], uint8_t num_leds);

extern void drawPoint(Particle *particle, CRGB strip[], uint8_t num_leds);
extern void drawFlash(Particle *particle, CRGB strip[], uint8_t num_leds);


class Particle {
  public:
    BeatFrame_24_8 born;
    BeatFrame_24_8 lifetime;
    BeatFrame_24_8 age;

    uint16_t position = 0;
    int16_t velocity = 0;
    int16_t gravity = 0;
    void (*die_fn)(Particle *particle) = NULL;
    PenMode pen = Draw;

#ifdef PARTICLE_PALETTES
    CRGBPalette16 palette;   // 48 bytes per particle!?
#endif

    CRGB color;
    uint16_t brightness;
    ParticleFn drawFn;

  Particle(uint16_t position, CRGB color=CRGB::White, PenMode pen=Draw, uint32_t lifetime=20000, ParticleFn drawFn=drawPoint)
  {
    this->age = 0;
    this->position = position;
    this->color = color;
    this->pen = pen;
    this->lifetime = lifetime;
    this->brightness = (192<<8);
    this->drawFn = drawFn;
  }

  void update(BeatFrame_24_8 frame)
  {
    this->age = frame - this->born;
    this->position = this->udelta16(this->position, this->velocity);
    this->velocity = this->delta16(this->velocity, this->gravity);
  }

  uint16_t age_frac16(BeatFrame_24_8 age)
  {
    if (age >= this->lifetime)
      return 65535;
    uint32_t a = age * 65536;
    return a / this->lifetime;
  }

  uint16_t udelta16(uint16_t x, int16_t dx)
  {
    if (dx > 0 && 65535-x < dx)
      return 65335;
    if (dx < 0 && x < -dx)
      return 0;
    return x + dx;
  }
  
  int16_t delta16(int16_t x, int16_t dx)
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
    uint8_t brightness = scale8((uint8_t)(this->brightness>>8), 255-a);

#ifdef PARTICLE_PALETTES
    // a black pattern actually means to use the current palette
    if (this->color == CRGB(0,0,0))
      return ColorFromPalette(this->palette, a, brightness);
#endif

    uint8_t r = scale8(this->color.r, brightness);
    uint8_t g = scale8(this->color.g, brightness);
    uint8_t b = scale8(this->color.b, brightness);
    return CRGB(r,g,b);
  }

  void draw_with_pen(CRGB strip[], int pos, CRGB color) {
    CRGB new_color;
    
    switch (this->pen) {
      case Draw:  
        strip[pos] = color;
        break;
  
      case Blend:
        strip[pos] |= color;
        break;
  
      case Erase:
        strip[pos] &= color;
        break;
  
      case Invert:
        strip[pos] = -strip[pos];
        break;  

      case Brighten: {
        uint8_t t = color.getAverageLight();
        new_color = CRGB(t,t,t);
        strip[pos] += new_color;
        break;  
      }

      case Darken: {
        uint8_t t = color.getAverageLight();
        new_color = CRGB(t,t,t);
        strip[pos] -= new_color;
        break;  
      }

      case Flicker: {
        uint8_t t = color.getAverageLight();
        new_color = CRGB(t,t,t);
        if (millis() % 2)
          strip[pos] -= new_color;
        else
          strip[pos] += new_color;
        break;  
      }

      case White:
        strip[pos] = CRGB::White;
        break;  

      case Black:
        strip[pos] = CRGB::Black;
        break;  

    }
  }

};

Particle* particles[5];
/* PARTICLES
ustd::array<Particle*> particles = ustd::array<Particle*>(5);
*/
BeatFrame_24_8 particle_beat_frame;

void addParticle(Particle *particle) {
  particle->born = particle_beat_frame;
  /* PARTICLES
  particles.add(particle);
  if (particles.length() > MAX_PARTICLES) {
    Particle *old_particle = particles[0];
    delete old_particle;
    particles.erase(0);
  }
  */
}


void drawFlash(Particle *particle, CRGB strip[], uint8_t num_leds) {
  uint16_t age_frac = particle->age_frac16(particle->age);
  CRGB c = particle->color_at(age_frac);
  for (int pos = 0; pos < num_leds; pos++) {
    particle->draw_with_pen(strip, pos, c);
  }
}

void drawPoint(Particle *particle, CRGB strip[], uint8_t num_leds) {
  uint16_t age_frac = particle->age_frac16(particle->age);
  CRGB c = particle->color_at(age_frac);

  uint16_t pos = scale16(particle->position, num_leds-1);
  particle->draw_with_pen(strip, pos, c);
}

void drawRadius(Particle *particle, CRGB strip[], uint8_t num_leds, uint16_t pos, uint8_t radius, CRGB c, bool dim=true) {
  for (int i = 0; i < radius; i++) {
    uint8_t bright = dim ? ((radius-i) * 255) / radius : 255;
    nscale8(&c, 1, bright);

    uint8_t y = pos - i;
    if (y >= 0 && y < num_leds)
      particle->draw_with_pen(strip, y, c);

    if (i == 0)
      continue;

    y = pos + i;
    if (y >= 0 && y < num_leds)
      particle->draw_with_pen(strip, y, c);
  }
}

void drawPop(Particle *particle, CRGB strip[], uint8_t num_leds) {
  uint16_t age_frac = particle->age_frac16(particle->age);
  CRGB c = particle->color_at(age_frac);
  uint16_t pos = scale16(particle->position, num_leds-1);
  uint8_t radius = scale16((sin16(age_frac/2) - 32768) * 2, 8);

  drawRadius(particle, strip, num_leds, pos, radius, c);
}

void drawBeatbox(Particle *particle, CRGB strip[], uint8_t num_leds) {
  uint16_t age_frac = particle->age_frac16(particle->age);
  CRGB c = particle->color_at(age_frac);
  uint16_t pos = scale16(particle->position, num_leds-1);
  uint8_t radius = 5;

  drawRadius(particle, strip, num_leds, pos, radius, c, false);
}

