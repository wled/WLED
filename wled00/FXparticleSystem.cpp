/*
  FXparticleSystem.cpp

  Particle system with functions for particle generation, particle movement and particle rendering to RGB matrix.
  by DedeHai (Damian Schneider) 2013-2024

  Copyright (c) 2024  Damian Schneider
  Licensed under the EUPL v. 1.2 or later
*/

#ifdef WLED_DISABLE_2D
#define WLED_DISABLE_PARTICLESYSTEM2D
#endif

#if !(defined(WLED_DISABLE_PARTICLESYSTEM2D) && defined(WLED_DISABLE_PARTICLESYSTEM1D)) // not both disabled
#include "FXparticleSystem.h"

//////////////////////////////
// Shared Utility Functions //
//////////////////////////////

// calculate the delta speed (dV) value and update the counter for force calculation (is used several times, function saves on codesize)
// force is in 3.4 fixedpoint notation, +/-127
static int32_t calcForce_dv(const int8_t force, uint8_t &counter) {
  if (force == 0)
    return 0;
  // for small forces, need to use a delay counter
  int32_t force_abs = abs(force); // absolute value (faster than lots of if's only 7 instructions)
  int32_t dv = 0;
  // for small forces, need to use a delay counter, apply force only if it overflows
  if (force_abs < 16) {
    counter += force_abs;
    if (counter > 15) {
      counter -= 16;
      dv = force < 0 ? -1 : 1; // force is either 1 or -1 if it is small (zero force is handled above)
    }
  }
  else
    dv = force / 16; // MSBs, note: cannot use bitshift as dv can be negative

  return dv;
}

// check if particle is out of bounds and wrap it around if required, returns false if out of bounds
static bool checkBoundsAndWrap(int32_t &position, const int32_t max, const int32_t particleradius, const bool wrap) {
  if ((uint32_t)position > (uint32_t)max) { // check if particle reached an edge, cast to uint32_t to save negative checking (max is always positive)
    if (wrap) {
      position = position % (max + 1); // note: cannot optimize modulo, particles can be far out of bounds when wrap is enabled
      if (position < 0)
        position += max + 1;
    }
    else if (((position < -particleradius) || (position > max + particleradius))) // particle is leaving boundaries, out of bounds if it has fully left
      return false; // out of bounds
  }
  return true; // particle is in bounds
}

// update number of particles to use, limit to allocated (= particles allocated by the calling system) in case more are available in the buffer
static void updateUsedParticles(const uint32_t allocated, const uint32_t available, const uint8_t percentage, uint32_t &used) {
  uint32_t wantsToUse = 1 + ((allocated * ((uint32_t)percentage + 1)) >> 8); // always give 1 particle minimum
  used = max((uint32_t)2, min(available, wantsToUse)); // limit to available particles, use a minimum of 2
}

// limit speed of particles (used in 1D and 2D)
static inline int32_t limitSpeed(const int32_t speed) {
  return constrain(speed, -PS_P_MAXSPEED, PS_P_MAXSPEED); // note: this is slightly faster than using min/max at the cost of 50bytes of flash
}
static void blur2D(uint32_t *colorbuffer, const uint32_t xsize, uint32_t ysize, const uint32_t xblur, const uint32_t yblur, const uint32_t xstart = 0, uint32_t ystart = 0, const bool isparticle = false);
static void blur1D(uint32_t *colorbuffer, uint32_t size, uint32_t blur, uint32_t start);

// global variables for memory management
static int32_t globalSmear = 0; // smear-blur to apply if multiple PS are using the buffer
#endif

////////////////////////
// 2D Particle System //
////////////////////////
#ifndef WLED_DISABLE_PARTICLESYSTEM2D

//non class functions to use for initialization
static uint32_t calculateNumberOfParticles2D(uint32_t pixels, uint32_t fraction, uint32_t requestedSources, bool isadvanced, bool sizecontrol) {
  uint32_t numberofParticles = pixels;  // 1 particle per pixel (for example 512 particles on 32x16)
  const uint32_t particlelimit = PS_MAXPARTICLES; // maximum number of paticles allowed (based on two segments of 32x32 and 40k effect ram)
  const uint32_t maxAllowedMemory = MAX_SEGMENT_DATA/strip.getActiveSegmentsNum() - sizeof(ParticleSystem2D) - requestedSources * sizeof(PSsource); // more segments, less memory

  numberofParticles = max((uint32_t)4, min(numberofParticles, particlelimit)); // limit to 4 - particlelimit
  numberofParticles = (numberofParticles * (fraction + 1)) >> 8; // calculate fraction of particles
  // advanced property array needs ram, reduce number of particles to use the same amount
  if (isadvanced) numberofParticles = (numberofParticles * sizeof(PSparticle)) / (sizeof(PSparticle) + sizeof(PSadvancedParticle));
  // advanced property array needs ram, reduce number of particles
  if (sizecontrol) numberofParticles /= 8; // if advanced size control is used, much fewer particles are needed note: if changing this number, adjust FX using this accordingly

  uint32_t requiredmemory = numberofParticles * (sizeof(PSparticle) + isadvanced * sizeof(PSadvancedParticle) + sizecontrol * sizeof(PSsizeControl));
  // check if we can allocate this many particles
  if (requiredmemory > maxAllowedMemory) {
    numberofParticles = numberofParticles * maxAllowedMemory / requiredmemory; // reduce number of particles to fit in memory
  }

  //make sure it is a multiple of 4 for proper memory alignment (easier than using padding bytes)
  numberofParticles = ((numberofParticles+3) & ~3U);
  return numberofParticles;
}

static uint32_t calculateNumberOfSources2D(uint32_t pixels, uint32_t requestedSources) {
  uint32_t numberofSources = min(pixels / 4U, requestedSources);
  numberofSources = constrain(numberofSources, 1, PS_MAXSOURCES);
  // make sure it is a multiple of 4 for proper memory alignment
  //numberofSources = ((numberofSources+3) & ~3U);
  return numberofSources;
}

uint32_t get2DPSmemoryRequirements(uint16_t cols, uint16_t rows, uint32_t fraction, uint32_t requestedSources, bool advanced, bool sizeControl) {
  uint32_t numParticles = calculateNumberOfParticles2D(cols * rows, fraction, requestedSources, advanced, sizeControl);
  uint32_t numSources = calculateNumberOfSources2D(cols * rows, requestedSources);
  uint32_t requiredmemory = sizeof(ParticleSystem2D);
  requiredmemory += sizeof(PSparticle) * numParticles;
  if (advanced) requiredmemory += sizeof(PSadvancedParticle) * numParticles;
  if (sizeControl) requiredmemory += sizeof(PSsizeControl) * numParticles;
  requiredmemory += sizeof(PSsource) * numSources;
  requiredmemory = (requiredmemory + 7) & ~3U; // align to 4 bytes and have 4 bytes for padding
  return requiredmemory;
}

// TODO: implement fraction of particles used
ParticleSystem2D::ParticleSystem2D(Segment *s, uint32_t fraction, uint32_t requestedSources, bool isAdvanced, bool sizeControl) {
  PSPRINTLN("\nParticleSystem2D constructor");
  seg = s; // set segment pointer
  numSources = calculateNumberOfSources2D(Segment::vWidth() * Segment::vHeight(), requestedSources); // number of sources allocated in init
  numParticles = calculateNumberOfParticles2D(Segment::vWidth() * Segment::vHeight(), fraction, requestedSources, isAdvanced, sizeControl); // number of particles allocated in init
  PSPRINT(" sources: "); PSPRINTLN(numSources);
  PSPRINT(" particles: "); PSPRINTLN(numParticles);
  availableParticles = numParticles; // use all
  setUsedParticles(255); // use all particles by default, usedParticles is updated in updatePSpointers()
  advPartProps = nullptr; //make sure we start out with null pointers (just in case memory was not cleared)
  advPartSize = nullptr;
  updatePSpointers(isAdvanced, sizeControl); // set the particle and sources pointer (call this before accessing sprays or particles)
  setMatrixSize(Segment::vWidth(), Segment::vHeight()); //(seg->virtualWidth(), seg->virtualHeight());
  setWallHardness(255); // set default wall hardness to max
  setWallRoughness(0); // smooth walls by default
  setGravity(0); //gravity disabled by default
  setParticleSize(1); // 2x2 rendering size by default
  motionBlur = 0; //no fading by default
  smearBlur = 0; //no smearing by default
  emitIndex = 0;
  collisionStartIdx = 0;

  //initialize some default non-zero values most FX use
  for (uint32_t i = 0; i < numSources; i++) {
    sources[i].source.sat = 255; //set saturation to max by default
    sources[i].source.ttl = 1; //set source alive
  }
  for (uint32_t i = 0; i < numParticles; i++) {
    particles[i].sat = 255; // set full saturation
  }
}

// update function applies gravity, moves the particles, handles collisions and renders the particles
void ParticleSystem2D::update() {
  //apply gravity globally if enabled
  if (particlesettings.useGravity)
    applyGravity();

  //update size settings before handling collisions
  if (advPartSize) {
    for (uint32_t i = 0; i < usedParticles; i++) {
      if(updateSize(&particles[i], &advPartSize[i]) == false) { // if particle shrinks to 0 size
        particles[i].ttl = 0; // kill particle
      }
    }
  }

  // handle collisions (can push particles, must be done before updating particles or they can render out of bounds, causing a crash if using local buffer for speed)
  if (particlesettings.useCollisions)
    handleCollisions();

  //move all particles
  for (uint32_t i = 0; i < usedParticles; i++) {
    particleMoveUpdate(particles[i], nullptr);
  }

  render();
}

// update function for fire animation
void ParticleSystem2D::updateFire(const uint8_t intensity,const bool renderonly) {
  if (!renderonly)
    fireParticleupdate();
  fireIntesity = intensity > 0 ? intensity : 1; // minimum of 1, zero checking is used in render function
  render();
}

// set percentage of used particles as uint8_t i.e 127 means 50% for example
void ParticleSystem2D::setUsedParticles(uint8_t percentage) {
  fractionOfParticlesUsed = percentage; // note usedParticles is updated in memory manager
  updateUsedParticles(numParticles, availableParticles, fractionOfParticlesUsed, usedParticles);
  ///PSPRINTLN("SetUsedpaticles:");
  //PSPRINT(" allocated particles: "); PSPRINTLN(numParticles);
  //PSPRINT(" available particles: "); PSPRINTLN(availableParticles);
  //PSPRINT(" percent: "); PSPRINTLN(fractionOfParticlesUsed*100/255);
  //PSPRINT(" used particles: "); PSPRINTLN(usedParticles);
}

void ParticleSystem2D::setWallHardness(uint8_t hardness) {
  wallHardness = hardness;
}

void ParticleSystem2D::setWallRoughness(uint8_t roughness) {
  wallRoughness = roughness;
}

void ParticleSystem2D::setCollisionHardness(uint8_t hardness) {
  collisionHardness = (int)hardness + 1;
}

void ParticleSystem2D::setMatrixSize(uint32_t x, uint32_t y) {
  maxXpixel = x - 1; // last physical pixel that can be drawn to
  maxYpixel = y - 1;
  maxX = x * PS_P_RADIUS - 1;  // particle system boundary for movements
  maxY = y * PS_P_RADIUS - 1;  // this value is often needed (also by FX) to calculate positions
}

void ParticleSystem2D::setWrapX(bool enable) {
  particlesettings.wrapX = enable;
}

void ParticleSystem2D::setWrapY(bool enable) {
  particlesettings.wrapY = enable;
}

void ParticleSystem2D::setBounceX(bool enable) {
  particlesettings.bounceX = enable;
}

void ParticleSystem2D::setBounceY(bool enable) {
  particlesettings.bounceY = enable;
}

void ParticleSystem2D::setKillOutOfBounds(bool enable) {
  particlesettings.killoutofbounds = enable;
}

void ParticleSystem2D::setColorByAge(bool enable) {
  particlesettings.colorByAge = enable;
}

void ParticleSystem2D::setMotionBlur(uint8_t bluramount) {
  if (particlesize < 2) // only allow motion blurring on default particle sizes or advanced size (cannot combine motion blur with normal blurring used for particlesize, would require another buffer)
    motionBlur = bluramount;
}

void ParticleSystem2D::setSmearBlur(uint8_t bluramount) {
  smearBlur = bluramount;
}


// render size using smearing (see blur function)
void ParticleSystem2D::setParticleSize(uint8_t size) {
  particlesize = size;
  particleHardRadius = PS_P_MINHARDRADIUS; // ~1 pixel
  if (particlesize > 1) {
    particleHardRadius = max(particleHardRadius, (uint32_t)particlesize); // radius used for wall collisions & particle collisions
    motionBlur = 0; // disable motion blur if particle size is set
  } else if (particlesize == 0)
    particleHardRadius = particleHardRadius >> 1; // single pixel particles have half the radius (i.e. 1/2 pixel)
}

// enable/disable gravity, optionally, set the force (force=8 is default) can be -127 to +127, 0 is disable
// if enabled, gravity is applied to all particles in ParticleSystemUpdate()
// force is in 3.4 fixed point notation so force=16 means apply v+1 each frame default of 8 is every other frame (gives good results)
void ParticleSystem2D::setGravity(int8_t force) {
  if (force) {
    gforce = force;
    particlesettings.useGravity = true;
  } else {
    particlesettings.useGravity = false;
  }
}

void ParticleSystem2D::enableParticleCollisions(bool enable, uint8_t hardness) { // enable/disable gravity, optionally, set the force (force=8 is default) can be 1-255, 0 is also disable
  particlesettings.useCollisions = enable;
  collisionHardness = (int)hardness + 1;
}

// emit one particle with variation, returns index of emitted particle (or -1 if no particle emitted)
int32_t ParticleSystem2D::sprayEmit(const PSsource &emitter) {
  bool success = false;
  for (uint32_t i = 0; i < usedParticles; i++) {
    emitIndex++;
    if (emitIndex >= usedParticles)
      emitIndex = 0;
    if (particles[emitIndex].ttl == 0) { // find a dead particle
      success = true;
      particles[emitIndex].vx = emitter.vx + hw_random16(emitter.var << 1) - emitter.var; // random(-var, var)
      particles[emitIndex].vy = emitter.vy + hw_random16(emitter.var << 1) - emitter.var; // random(-var, var)
      particles[emitIndex].x = emitter.source.x;
      particles[emitIndex].y = emitter.source.y;
      particles[emitIndex].hue = emitter.source.hue;
      particles[emitIndex].sat = emitter.source.sat;
      particles[emitIndex].collide = emitter.source.collide;
      particles[emitIndex].reversegrav = emitter.source.reversegrav;
      particles[emitIndex].ttl = hw_random16(emitter.minLife, emitter.maxLife);
      particles[emitIndex].size = emitter.size;
      break;
    }
  }
  if (success)
    return emitIndex;
  else
    return -1;
}

// Spray emitter for particles used for flames (particle TTL depends on source TTL)
void ParticleSystem2D::flameEmit(const PSsource &emitter) {
  int emitIndex = sprayEmit(emitter);
  if(emitIndex > 0)  particles[emitIndex].ttl += emitter.source.ttl;
}

// Emits a particle at given angle and speed, angle is from 0-65535 (=0-360deg), speed is also affected by emitter->var
// angle = 0 means in positive x-direction (i.e. to the right)
int32_t ParticleSystem2D::angleEmit(PSsource &emitter, const uint16_t angle, const int32_t speed) {
  emitter.vx = ((int32_t)cos16_t(angle) * speed) / (int32_t)32600; // cos16_t() and sin16_t() return signed 16bit, division should be 32767 but 32600 gives slightly better rounding
  emitter.vy = ((int32_t)sin16_t(angle) * speed) / (int32_t)32600; // note: cannot use bit shifts as bit shifting is asymmetrical for positive and negative numbers and this needs to be accurate!
  return sprayEmit(emitter);
}

// particle moves, decays and dies, if killoutofbounds is set, out of bounds particles are set to ttl=0
// uses passed settings to set bounce or wrap, if useGravity is enabled, it will never bounce at the top and killoutofbounds is not applied over the top
void ParticleSystem2D::particleMoveUpdate(PSparticle &part, PSsettings *options) {
  if (options == nullptr)
    options = &particlesettings; //use PS system settings by default

  if (part.ttl > 0) {
    if (!part.perpetual)
      part.ttl--; // age
    if (options->colorByAge)
      part.hue = min(part.ttl, (uint16_t)255); //set color to ttl

    int32_t renderradius = PS_P_HALFRADIUS; // used to check out of bounds
    int32_t newX = part.x + (int32_t)part.vx;
    int32_t newY = part.y + (int32_t)part.vy;
    part.outofbounds = false; // reset out of bounds (in case particle was created outside the matrix and is now moving into view) note: moving this to checks below adds code and is not faster

    if (part.size > 0) { //using individual particle size?
      setParticleSize(particlesize); // updates default particleHardRadius
      if (part.size > PS_P_MINHARDRADIUS) {
        particleHardRadius += (part.size - PS_P_MINHARDRADIUS); // update radius
        renderradius = particleHardRadius;
      }
    }
    // note: if wall collisions are enabled, bounce them before they reach the edge, it looks much nicer if the particle does not go half out of view
    if (options->bounceY) {
      if ((newY < (int32_t)particleHardRadius) || ((newY > (int32_t)(maxY - particleHardRadius)) && !options->useGravity)) { // reached floor / ceiling
        bool bouncethis = true;
        if (options->useGravity) {
          if (part.reversegrav) { // skip bouncing at x = 0
            if (newY < (int32_t)particleHardRadius)
              bouncethis = false;
          } else if (newY > (int32_t)particleHardRadius) { // skip bouncing at x = max
            bouncethis = false;
          }
        }
        if (bouncethis) {
          part.vy = -part.vy; // invert speed
          part.vy = ((int32_t)part.vy * (int32_t)wallHardness) / 255; // reduce speed as energy is lost on non-hard surface
          if (newY < (int32_t)particleHardRadius)
            newY = particleHardRadius; // fast particles will never reach the edge if position is inverted, this looks better
          else
            newY = maxY - particleHardRadius;
        }
        bounce(part.vy, part.vx, newY, maxY);
      }
    }

    if(!checkBoundsAndWrap(newY, maxY, renderradius, options->wrapY)) { // check out of bounds  note: this must not be skipped. if gravity is enabled, particles will never bounce at the top
      part.outofbounds = true;
      if (options->killoutofbounds) {
        if (newY < 0) // if gravity is enabled, only kill particles below ground
          part.ttl = 0;
        else if (!options->useGravity)
          part.ttl = 0;
      }
    }

    if (part.ttl) { //check x direction only if still alive
      if (options->bounceX) {
        if ((newX < (int32_t)particleHardRadius) || (newX > (int32_t)(maxX - particleHardRadius))) // reached a wall
          bounce(part.vx, part.vy, newX, maxX);
      }
      else if(!checkBoundsAndWrap(newX, maxX, renderradius, options->wrapX)) { // check out of bounds
        part.outofbounds = true;
        if (options->killoutofbounds)
          part.ttl = 0;
      }
    }

    if (part.fixed)
      part.vx = part.vy = 0; // set speed to zero. note: particle can get speed in collisions, if unfixed, it should not speed away
    else {
      part.x = (int16_t)newX; // set new position
      part.y = (int16_t)newY; // set new position
    }
  }
}

// move function for fire particles
void ParticleSystem2D::fireParticleupdate() {
  for (uint32_t i = 0; i < usedParticles; i++) {
    if (particles[i].ttl > 0)
    {
      particles[i].ttl--; // age
      int32_t newY = particles[i].y + (int32_t)particles[i].vy + (particles[i].ttl >> 2); // younger particles move faster upward as they are hotter
      int32_t newX = particles[i].x + (int32_t)particles[i].vx;
      particles[i].outofbounds = false; // reset out of bounds flag  note: moving this to checks below is not faster but adds code
      // check if particle is out of bounds, wrap x around to other side if wrapping is enabled
      // as fire particles start below the frame, lots of particles are out of bounds in y direction. to improve speed, only check x direction if y is not out of bounds
      if (newY < -PS_P_HALFRADIUS)
        particles[i].outofbounds = true;
      else if (newY > int32_t(maxY + PS_P_HALFRADIUS)) // particle moved out at the top
        particles[i].ttl = 0;
      else // particle is in frame in y direction, also check x direction now Note: using checkBoundsAndWrap() is slower, only saves a few bytes
      {
        if ((newX < 0) || (newX > (int32_t)maxX)) { // handle out of bounds & wrap
          if (particlesettings.wrapX) {
            newX = newX % (maxX + 1);
            if (newX < 0) // handle negative modulo
              newX += maxX + 1;
          }
          else if ((newX < -PS_P_HALFRADIUS) || (newX > int32_t(maxX + PS_P_HALFRADIUS))) { //if fully out of view
            particles[i].ttl = 0;
          }
        }
        particles[i].x = newX;
      }
      particles[i].y = newY;
    }
  }
}

// update advanced particle size control, returns false if particle shrinks to 0 size
bool ParticleSystem2D::updateSize(PSparticle *particle, PSsizeControl *advsize) {
  if (advsize == nullptr) // safety check
    return false;
  // grow/shrink particle
  //auto abs = [](int32_t x) { return x < 0 ? -x : x; };
  int32_t newsize = particle->size;
  int32_t growspeed = advsize->growspeed;
  uint32_t counter = advsize->sizecounter;
  uint32_t increment = 0;
  int32_t sign = growspeed < 0 ? -1 : 1; // sign of grow speed
  bool grow = advsize->grow; // optimise bitfield use
  // calculate grow speed using 0-8 for low speeds and 9-15 for higher speeds
  if (grow) increment = abs(growspeed);
  if (increment < 9) { // 8 means +1 every frame
    counter += increment;
    if (counter > 7) {
      counter -= 8;
      increment = 1;
    } else
      increment = 0;
    advsize->sizecounter = counter;
  } else {
    increment = (increment - 8) << 1; // 9 means +2, 10 means +4 etc. 15 means +14
  }

  if (grow && increment != 0) {
    if (newsize < advsize->maxsize && newsize > advsize->minsize) { // if size is within limits
      newsize += increment * sign;
      if (newsize >= advsize->maxsize || newsize <= advsize->minsize) {
        advsize->grow = advsize->pulsate; // stop growing, shrink from now on if enabled
        newsize = sign > 0 ? advsize->maxsize : advsize->minsize; // limit
        if (advsize->pulsate) advsize->growspeed = -growspeed; // reverse grow speed
      }
    }
  }
  particle->size = newsize;
  // handle wobbling
  if (advsize->wobble) {
    advsize->asymdir += advsize->wobblespeed; // note: if need better wobblespeed control a counter is already in the struct
  }
  return true;
}

// calculate x and y size for asymmetrical particles (advanced size control)
void ParticleSystem2D::getParticleXYsize(PSparticle *particle, PSsizeControl *advsize, uint32_t &xsize, uint32_t &ysize) {
  if (advsize == nullptr) // if advsize is valid, also advanced properties pointer is valid (handled by updatePSpointers())
    return;
  int32_t size = particle->size;
  int32_t asymdir = advsize->asymdir;
  int32_t deviation = ((uint32_t)size * ((uint32_t)advsize->asymmetry)) / 255; // deviation from symmetrical size
  // Calculate x and y size based on deviation and direction (0 is symmetrical, 64 is x, 128 is symmetrical, 192 is y)
  if (asymdir < 64) {
    deviation = (asymdir * deviation) / 64;
  } else if (asymdir < 192) {
    deviation = ((128 - asymdir) * deviation) / 64;
  } else {
    deviation = ((asymdir - 255) * deviation) / 64;
  }
  // Calculate x and y size based on deviation, limit to 255 (rendering function cannot handle larger sizes)
  xsize = min((size - deviation), (int32_t)255);
  ysize = min((size + deviation), (int32_t)255);;
}

// function to bounce a particle from a wall using set parameters (wallHardness and wallRoughness)
void ParticleSystem2D::bounce(int8_t &incomingspeed, int8_t &parallelspeed, int32_t &position, const uint32_t maxposition) {
  incomingspeed = -incomingspeed;
  incomingspeed = (incomingspeed * wallHardness) / 255; // reduce speed as energy is lost on non-hard surface
  if (position < (int32_t)particleHardRadius)
    position = particleHardRadius; // fast particles will never reach the edge if position is inverted, this looks better
  else
    position = maxposition - particleHardRadius;
  if (wallRoughness) {
    int32_t incomingspeed_abs = abs((int32_t)incomingspeed);
    int32_t totalspeed = incomingspeed_abs + abs((int32_t)parallelspeed);
    // transfer an amount of incomingspeed speed to parallel speed
    int32_t donatespeed = ((hw_random16(incomingspeed_abs << 1) - incomingspeed_abs) * (int32_t)wallRoughness) / (int32_t)255; // take random portion of + or - perpendicular speed, scaled by roughness
    parallelspeed = limitSpeed((int32_t)parallelspeed + donatespeed);
    // give the remainder of the speed to perpendicular speed
    donatespeed = int8_t(totalspeed - abs(parallelspeed)); // keep total speed the same
    incomingspeed = incomingspeed > 0 ? donatespeed : -donatespeed;
  }
}

// apply a force in x,y direction to individual particle
// caller needs to provide a 8bit counter (for each particle) that holds its value between calls
// force is in 3.4 fixed point notation so force=16 means apply v+1 each frame default of 8 is every other frame (gives good results)
void ParticleSystem2D::applyForce(PSparticle &part, const int8_t xforce, const int8_t yforce, uint8_t &counter) {
  // for small forces, need to use a delay counter
  uint8_t xcounter = counter & 0x0F; // lower four bits
  uint8_t ycounter = counter >> 4;   // upper four bits

  // velocity increase
  int32_t dvx = calcForce_dv(xforce, xcounter);
  int32_t dvy = calcForce_dv(yforce, ycounter);

  // save counter values back
  counter = xcounter & 0x0F; // write lower four bits, make sure not to write more than 4 bits
  counter |= (ycounter << 4) & 0xF0; // write upper four bits

  // apply the force to particle
  part.vx = limitSpeed((int32_t)part.vx + dvx);
  part.vy = limitSpeed((int32_t)part.vy + dvy);
}

// apply a force in x,y direction to individual particle using advanced particle properties
void ParticleSystem2D::applyForce(const uint32_t particleindex, const int8_t xforce, const int8_t yforce) {
  if (advPartProps == nullptr)
    return; // no advanced properties available
  applyForce(particles[particleindex], xforce, yforce, advPartProps[particleindex].forcecounter);
}

// apply a force in x,y direction to all particles
// force is in 3.4 fixed point notation (see above)
void ParticleSystem2D::applyForce(const int8_t xforce, const int8_t yforce) {
  // for small forces, need to use a delay counter
  uint8_t tempcounter;
  // note: this is not the most computationally efficient way to do this, but it saves on duplicate code and is fast enough
  for (uint32_t i = 0; i < usedParticles; i++) {
    tempcounter = forcecounter;
    applyForce(particles[i], xforce, yforce, tempcounter);
  }
  forcecounter = tempcounter; // save value back
}

// apply a force in angular direction to single particle
// caller needs to provide a 8bit counter that holds its value between calls (if using single particles, a counter for each particle is needed)
// angle is from 0-65535 (=0-360deg) angle = 0 means in positive x-direction (i.e. to the right)
// force is in 3.4 fixed point notation so force=16 means apply v+1 each frame (useful force range is +/- 127)
void ParticleSystem2D::applyAngleForce(PSparticle &part, const int8_t force, const uint16_t angle, uint8_t &counter) {
  int8_t xforce = ((int32_t)force * cos16_t(angle)) / 32767; // force is +/- 127
  int8_t yforce = ((int32_t)force * sin16_t(angle)) / 32767; // note: cannot use bit shifts as bit shifting is asymmetrical for positive and negative numbers and this needs to be accurate!
  applyForce(part, xforce, yforce, counter);
}

void ParticleSystem2D::applyAngleForce(const uint32_t particleindex, const int8_t force, const uint16_t angle) {
  if (advPartProps == nullptr)
    return; // no advanced properties available
  applyAngleForce(particles[particleindex], force, angle, advPartProps[particleindex].forcecounter);
}

// apply a force in angular direction to all particles
// angle is from 0-65535 (=0-360deg) angle = 0 means in positive x-direction (i.e. to the right)
void ParticleSystem2D::applyAngleForce(const int8_t force, const uint16_t angle) {
  int8_t xforce = ((int32_t)force * cos16_t(angle)) / 32767; // force is +/- 127
  int8_t yforce = ((int32_t)force * sin16_t(angle)) / 32767; // note: cannot use bit shifts as bit shifting is asymmetrical for positive and negative numbers and this needs to be accurate!
  applyForce(xforce, yforce);
}

// apply gravity to all particles using PS global gforce setting
// force is in 3.4 fixed point notation, see note above
// note: faster than apply force since direction is always down and counter is fixed for all particles
void ParticleSystem2D::applyGravity() {
  int32_t dv = calcForce_dv(gforce, gforcecounter);
  if (dv == 0) return;
  for (uint32_t i = 0; i < usedParticles; i++) {
    if (particles[i].reversegrav) dv = -dv;
    // Note: not checking if particle is dead is faster as most are usually alive and if few are alive, rendering is fast anyways
    particles[i].vy = limitSpeed((int32_t)particles[i].vy - dv);
  }
}

// apply gravity to single particle using system settings (use this for sources)
// function does not increment gravity counter, if gravity setting is disabled, this cannot be used
void ParticleSystem2D::applyGravity(PSparticle &part) {
  uint32_t counterbkp = gforcecounter; // backup PS gravity counter
  int32_t dv = calcForce_dv(gforce, gforcecounter);
  if (part.reversegrav) dv = -dv;
  gforcecounter = counterbkp; //save it back
  part.vy = limitSpeed((int32_t)part.vy - dv);
}

// slow down particle by friction, the higher the speed, the higher the friction. a high friction coefficient slows them more (255 means instant stop)
// note: a coefficient smaller than 0 will speed them up (this is a feature, not a bug), coefficient larger than 255 inverts the speed, so don't do that
void ParticleSystem2D::applyFriction(PSparticle &part, const int32_t coefficient) {
  // note: not checking if particle is dead can be done by caller (or can be omitted)
  #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP8266) // use bitshifts with rounding instead of division (2x faster)
  int32_t friction = 256 - coefficient;
  part.vx = ((int32_t)part.vx * friction + (((int32_t)part.vx >> 31) & 0xFF)) >> 8; // note: (v>>31) & 0xFF)) extracts the sign and adds 255 if negative for correct rounding using shifts
  part.vy = ((int32_t)part.vy * friction + (((int32_t)part.vy >> 31) & 0xFF)) >> 8;
  #else // division is faster on ESP32, S2 and S3
  int32_t friction = 255 - coefficient;
  part.vx = ((int32_t)part.vx * friction) / 255;
  part.vy = ((int32_t)part.vy * friction) / 255;
  #endif
}

// apply friction to all particles
// note: not checking if particle is dead is faster as most are usually alive and if few are alive, rendering is fast anyways
void ParticleSystem2D::applyFriction(const int32_t coefficient) {
  #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP8266) // use bitshifts with rounding instead of division (2x faster)
  int32_t friction = 256 - coefficient;
  for (uint32_t i = 0; i < usedParticles; i++) {
    particles[i].vx = ((int32_t)particles[i].vx * friction + (((int32_t)particles[i].vx >> 31) & 0xFF)) >> 8; // note: (v>>31) & 0xFF)) extracts the sign and adds 255 if negative for correct rounding using shifts
    particles[i].vy = ((int32_t)particles[i].vy * friction + (((int32_t)particles[i].vy >> 31) & 0xFF)) >> 8;
  }
  #else // division is faster on ESP32, S2 and S3
  int32_t friction = 255 - coefficient;
  for (uint32_t i = 0; i < usedParticles; i++) {
    particles[i].vx = ((int32_t)particles[i].vx * friction) / 255;
    particles[i].vy = ((int32_t)particles[i].vy * friction) / 255;
  }
  #endif
}

// attracts a particle to an attractor particle using the inverse square-law
void ParticleSystem2D::pointAttractor(const uint32_t particleindex, PSparticle &attractor, const uint8_t strength, const bool swallow) {
  if (advPartProps == nullptr)
    return; // no advanced properties available

  // Calculate the distance between the particle and the attractor
  int32_t dx = attractor.x - particles[particleindex].x;
  int32_t dy = attractor.y - particles[particleindex].y;

  // Calculate the force based on inverse square law
  int32_t distanceSquared = dx * dx + dy * dy;
  if (distanceSquared < 8192) {
    if (swallow) { // particle is close, age it fast so it fades out, do not attract further
      if (particles[particleindex].ttl > 7)
        particles[particleindex].ttl -= 8;
      else {
        particles[particleindex].ttl = 0;
        return;
      }
    }
    distanceSquared = 2 * PS_P_RADIUS * PS_P_RADIUS; // limit the distance to avoid very high forces
  }

  int32_t force = ((int32_t)strength << 16) / distanceSquared;
  int8_t xforce = (force * dx) / 1024; // scale to a lower value, found by experimenting
  int8_t yforce = (force * dy) / 1024; // note: cannot use bit shifts as bit shifting is asymmetrical for positive and negative numbers and this needs to be accurate!
  applyForce(particleindex, xforce, yforce);
}

// render particles to the LED buffer (uses palette to render the 8bit particle color value)
// if wrap is set, particles half out of bounds are rendered to the other side of the matrix
// warning: do not render out of bounds particles or system will crash! rendering does not check if particle is out of bounds
// firemode is only used for PS Fire FX
void ParticleSystem2D::render() {
  uint32_t baseRGB;
  uint32_t brightness; // particle brightness, fades if dying

  // update global blur (used for blur transitions)
  //int32_t motionbluramount = motionBlur;
  int32_t smearamount = smearBlur;
  globalSmear = smearamount;

  if (motionBlur > 0) seg->fadeToSecondaryBy(255 - motionBlur);
  else                seg->fill(SEGCOLOR(1));

  // go over particles and render them to the buffer
  for (uint32_t i = 0; i < usedParticles; i++) {
    if (particles[i].ttl == 0 || particles[i].outofbounds)
      continue;
    // generate RGB values for particle
    if (fireIntesity) { // fire mode
      brightness = (uint32_t)particles[i].ttl * (3 + (fireIntesity >> 5)) + 20;
      brightness = min(brightness, (uint32_t)255);
      baseRGB = seg->color_from_palette(brightness, false, false, 0, 255); //ColorFromPaletteWLED(SEGPALETTE, brightness, 255);
    } else {
      brightness = min((particles[i].ttl << 1), (int)255);
      baseRGB = seg->color_from_palette(particles[i].hue, false, false, 0, 255); //ColorFromPaletteWLED(SEGPALETTE, particles[i].hue, 255);
      if (particles[i].sat < 255) {
        CHSV32 baseHSV;
        rgb2hsv(baseRGB, baseHSV); // convert to HSV
        baseHSV.s = particles[i].sat; // set the saturation
        hsv2rgb(baseHSV, baseRGB); // convert back to RGB
      }
    }
    renderParticle(i, brightness, baseRGB, particlesettings.wrapX, particlesettings.wrapY);
  }

  if (particlesize > 1) {
    uint32_t passes = particlesize / 64 + 1; // number of blur passes, four passes max
    uint32_t bluramount = particlesize;
    uint32_t bitshift = 0;
    for (uint32_t i = 0; i < passes; i++) {
      // for the last two passes, use higher amount of blur (results in a nicer brightness gradient with soft edges)
      if (i >= 2) bitshift = 1;
      seg->blur(bluramount << bitshift, true);
      bluramount -= 64;
    }
  }
  // apply 2D blur to rendered frame
  if (globalSmear > 0) {
    seg->blur(globalSmear, true);
  }
}

// calculate pixel positions and brightness distribution and render the particle to local buffer or global buffer
void ParticleSystem2D::renderParticle(uint32_t particleindex, uint32_t brightness, uint32_t color, bool wrapX, bool wrapY) {
  if (particlesize == 0) { // single pixel rendering
    uint32_t x = particles[particleindex].x >> PS_P_RADIUS_SHIFT;
    uint32_t y = particles[particleindex].y >> PS_P_RADIUS_SHIFT;
    if (x <= (uint32_t)maxXpixel && y <= (uint32_t)maxYpixel) {
      // add particle pixel to existing pixel
      y = maxYpixel - y;
      uint32_t c = seg->getPixelColorXYRaw(x, y);
      fast_color_add(c, color, brightness);
      seg->setPixelColorXYRaw(x, y, c);
    }
    return;
  }
  int32_t pxlbrightness[4]; // brightness values for the four pixels representing a particle
  struct {        // 0,1 [(x-1,y-1), (x,y-1)]
    int32_t x,y;  // 3,2 [(x-1,y)  , (x,y)  ]
  } pixco[4];     //
  bool pixelvalid[4] = {true, true, true, true}; // is set to false if pixel is out of bounds
  bool advancedrender = particles[particleindex].size > 0;

  // add half a radius as the rendering algorithm always starts at the bottom left, this leaves things positive, so shifts can be used, then shift coordinate by a full pixel (x--/y-- below)
  int32_t xoffset = particles[particleindex].x + PS_P_HALFRADIUS;
  int32_t yoffset = particles[particleindex].y + PS_P_HALFRADIUS;
  int32_t dx = xoffset & (PS_P_RADIUS - 1); // relativ particle position in subpixel space
  int32_t dy = yoffset & (PS_P_RADIUS - 1); // modulo replaced with bitwise AND, as radius is always a power of 2
  int32_t x = (xoffset >> PS_P_RADIUS_SHIFT); // divide by PS_P_RADIUS which is 64, so can bitshift (compiler can not optimize integer)
  int32_t y = (yoffset >> PS_P_RADIUS_SHIFT);

  // set the four raw pixel coordinates, the order is bottom left [0], bottom right[1], top right [2], top left [3]
  pixco[2].x = pixco[1].x = x;  // bottom right & top right
  pixco[2].y = pixco[3].y = y;  // top right & top left
  x--; // shift by a full pixel here, this is skipped above to not do -1 and then +1
  y--;
  pixco[0].x = pixco[3].x = x;      // bottom left & top left
  pixco[0].y = pixco[1].y = y;      // bottom left & bottom right

  // calculate brightness values for all four pixels representing a particle using linear interpolation
  // could check for out of frame pixels here but calculating them is faster (very few are out)
  // precalculate values for speed optimization
  int32_t precal1 = (int32_t)PS_P_RADIUS - dx;
  int32_t precal2 = ((int32_t)PS_P_RADIUS - dy) * brightness;
  int32_t precal3 = dy * brightness;
  pxlbrightness[0] = (precal1 * precal2) >> PS_P_SURFACE; // bottom left value equal to ((PS_P_RADIUS - dx) * (PS_P_RADIUS-dy) * brightness) >> PS_P_SURFACE
  pxlbrightness[1] = (dx * precal2) >> PS_P_SURFACE; // bottom right value equal to (dx * (PS_P_RADIUS-dy) * brightness) >> PS_P_SURFACE
  pxlbrightness[2] = (dx * precal3) >> PS_P_SURFACE; // top right value equal to (dx * dy * brightness) >> PS_P_SURFACE
  pxlbrightness[3] = (precal1 * precal3) >> PS_P_SURFACE; // top left value equal to ((PS_P_RADIUS-dx) * dy * brightness) >> PS_P_SURFACE

  if (advancedrender) {
    uint32_t renderbuffer[100];
    memset(renderbuffer, 0, 100 * sizeof(uint32_t)); // clear the buffer, renderbuffer is 10x10 pixels
    // render particle to a bigger size
    // particle size to pixels: < 64 is 4x4, < 128 is 6x6, < 192 is 8x8, bigger is 10x10
    // first, render the pixel to the center of the renderbuffer, then apply 2D blurring
    fast_color_add(renderbuffer[4 + (4 * 10)], color, pxlbrightness[0]); // order is: bottom left, bottom right, top right, top left
    fast_color_add(renderbuffer[5 + (4 * 10)], color, pxlbrightness[1]);
    fast_color_add(renderbuffer[5 + (5 * 10)], color, pxlbrightness[2]);
    fast_color_add(renderbuffer[4 + (5 * 10)], color, pxlbrightness[3]);
    uint32_t rendersize = 2; // initialize render size, minimum is 4x4 pixels, it is incremented int he loop below to start with 4
    uint32_t offset = 4; // offset to zero coordinate to write/read data in renderbuffer (actually needs to be 3, is decremented in the loop below)
    uint32_t maxsize = particles[particleindex].size;
    uint32_t xsize = maxsize;
    uint32_t ysize = maxsize;
    if (advPartSize) { // use advanced size control
      if (advPartSize[particleindex].asymmetry > 0)
        getParticleXYsize(&particles[particleindex], &advPartSize[particleindex], xsize, ysize);
      maxsize = (xsize > ysize) ? xsize : ysize; // choose the bigger of the two
    }
    maxsize = maxsize/64 + 1; // number of blur passes depends on maxsize, four passes max
    uint32_t bitshift = 0;
    for (uint32_t i = 0; i < maxsize; i++) {
      if (i == 2) //for the last two passes, use higher amount of blur (results in a nicer brightness gradient with soft edges)
        bitshift = 1;
      rendersize += 2;
      offset--;
      blur2D(renderbuffer, rendersize, rendersize, xsize << bitshift, ysize << bitshift, offset, offset, true);
      xsize = xsize > 64 ? xsize - 64 : 0;
      ysize = ysize > 64 ? ysize - 64 : 0;
    }

    // calculate origin coordinates to render the particle
    uint32_t xfb_orig = x - (rendersize>>1) + 1 - offset;
    uint32_t yfb_orig = y - (rendersize>>1) + 1 - offset;
    uint32_t xfb, yfb; // coordinates in frame buffer to write to note: by making this uint, only overflow has to be checked (spits a warning though)

    //note on y-axis flip: WLED has the y-axis defined from top to bottom, so y coordinates must be flipped. doing this in the buffer xfer clashes with 1D/2D combined rendering, which does not invert y
    //                     transferring the 1D buffer in inverted fashion will flip the x-axis of overlaid 2D FX, so the y-axis flip is done here so the buffer is flipped in y, giving correct results

    // transfer particle renderbuffer
    for (uint32_t xrb = offset; xrb < rendersize + offset; xrb++) {
      xfb = xfb_orig + xrb;
      if (xfb > (uint32_t)maxXpixel) {
        if (wrapX) { // wrap x to the other side if required
          if (xfb > (uint32_t)maxXpixel << 1) // xfb is "negative", handle it
            xfb = (maxXpixel + 1) + (int32_t)xfb; // this always overflows to within bounds
          else
            xfb = xfb % (maxXpixel + 1); // note: without the above "negative" check, this works only for powers of 2
        } else
          continue;
      }

      for (uint32_t yrb = offset; yrb < rendersize + offset; yrb++) {
        yfb = yfb_orig + yrb;
        if (yfb > (uint32_t)maxYpixel) {
          if (wrapY) {// wrap y to the other side if required
            if (yfb > (uint32_t)maxYpixel << 1) // yfb is "negative", handle it
              yfb = (maxYpixel + 1) + (int32_t)yfb; // this always overflows to within bounds
            else
              yfb = yfb % (maxYpixel + 1); // note: without the above "negative" check, this works only for powers of 2
          } else
            continue;
        }
        // add particle pixel from render buffer to existing pixel (bounds for setPixelColorXYRaw() are checked above)
        yfb = maxYpixel - yfb;
        uint32_t indx = xrb + yrb * 10;
        uint32_t c = seg->getPixelColorXYRaw(xfb, yfb);
        fast_color_add(c, renderbuffer[indx], brightness);
        seg->setPixelColorXYRaw(xfb, yfb, c);
      }
    }
  } else { // standard rendering (2x2 pixels)
    // check for out of frame pixels and wrap them if required: x,y is bottom left pixel coordinate of the particle
    if (pixco[0].x < 0) { // left pixels out of frame
      if (wrapX) { // wrap x to the other side if required
        pixco[0].x = pixco[3].x = maxXpixel;
      } else {
        pixelvalid[0] = pixelvalid[3] = false; // out of bounds
      }
    }
    if (pixco[2].x > (int32_t)maxXpixel) { // right pixels, only has to be checked if left pixel is in frame
      if (wrapX) { // wrap y to the other side if required
        pixco[1].x = pixco[2].x = 0;
      } else {
        pixelvalid[1] = pixelvalid[2] = false; // out of bounds
      }
    }

    if (pixco[0].y < 0) { // bottom pixels out of frame
      if (wrapY) { // wrap y to the other side if required
        pixco[0].y = pixco[1].y = maxYpixel;
      } else {
        pixelvalid[0] = pixelvalid[1] = false; // out of bounds
      }
    }
    if (pixco[2].y > maxYpixel) { // top pixels
      if (wrapY) { // wrap y to the other side if required
        pixco[2].y = pixco[3].y = 0;
      } else {
        pixelvalid[2] = pixelvalid[3] = false; // out of bounds
      }
    }

    for (uint32_t i = 0; i < 4; i++) {
      if (pixelvalid[i]) {
        // add particle pixel to existing pixel (bounds for setPixelColorXYRaw() are checked above)
        uint32_t c = seg->getPixelColorXYRaw(pixco[i].x, maxYpixel - pixco[i].y);
        fast_color_add(c, color, pxlbrightness[i]);
        seg->setPixelColorXYRaw(pixco[i].x, maxYpixel - pixco[i].y, c);
      }
    }
  }
}

// detect collisions in an array of particles and handle them
// uses binning by dividing the frame into slices in x direction which is efficient if using gravity in y direction (but less efficient for FX that use forces in x direction)
// for code simplicity, no y slicing is done, making very tall matrix configurations less efficient
// note: also tested adding y slicing, it gives diminishing returns, some FX even get slower. FX not using gravity would benefit with a 10% FPS improvement
void ParticleSystem2D::handleCollisions() {
  int32_t collDistSq = particleHardRadius << 1; // distance is double the radius note: particleHardRadius is updated when setting global particle size
  collDistSq = collDistSq * collDistSq; // square it for faster comparison (square is one operation)
  // note: partices are binned in x-axis, assumption is that no more than half of the particles are in the same bin
  // if they are, collisionStartIdx is increased so each particle collides at least every second frame (which still gives decent collisions)
  constexpr int BIN_WIDTH = 6 * PS_P_RADIUS; // width of a bin in sub-pixels
  int32_t overlap = particleHardRadius << 1; // overlap bins to include edge particles to neighbouring bins
  if (advPartProps) //may be using individual particle size
    overlap += 512; // add 2 * max radius (approximately)
  uint32_t maxBinParticles = max((uint32_t)50, (usedParticles + 1) / 2); // assume no more than half of the particles are in the same bin, do not bin small amounts of particles
  uint32_t numBins = (maxX + (BIN_WIDTH - 1)) / BIN_WIDTH; // number of bins in x direction
  uint16_t binIndices[maxBinParticles]; // creat array on stack for indices, 2kB max for 1024 particles (ESP32_MAXPARTICLES/2)
  uint32_t binParticleCount; // number of particles in the current bin
  uint16_t nextFrameStartIdx = hw_random16(usedParticles); // index of the first particle in the next frame (set to fixed value if bin overflow)
  uint32_t pidx = collisionStartIdx; //start index in case a bin is full, process remaining particles next frame

  // fill the binIndices array for this bin
  for (uint32_t bin = 0; bin < numBins; bin++) {
    binParticleCount = 0; // reset for this bin
    int32_t binStart = bin * BIN_WIDTH - overlap; // note: first bin will extend to negative, but that is ok as out of bounds particles are ignored
    int32_t binEnd = binStart + BIN_WIDTH + overlap; // note: last bin can be out of bounds, see above;

    // fill the binIndices array for this bin
    for (uint32_t i = 0; i < usedParticles; i++) {
      if (particles[pidx].ttl > 0 && !particles[pidx].outofbounds && particles[pidx].collide) { // colliding particle
        if (particles[pidx].x >= binStart && particles[pidx].x <= binEnd) { // >= and <= to include particles on the edge of the bin (overlap to ensure boarder particles collide with adjacent bins)
          if (binParticleCount >= maxBinParticles) { // bin is full, more particles in this bin so do the rest next frame
            nextFrameStartIdx = pidx; // bin overflow can only happen once as bin size is at least half of the particles (or half +1)
            break;
          }
          binIndices[binParticleCount++] = pidx;
        }
      }
      pidx++;
      if (pidx >= usedParticles) pidx = 0; // wrap around
    }

    for (uint32_t i = 0; i < binParticleCount; i++) { // go though all 'higher number' particles in this bin and see if any of those are in close proximity and if they are, make them collide
      uint32_t idx_i = binIndices[i];
      for (uint32_t j = i + 1; j < binParticleCount; j++) { // check against higher number particles
        uint32_t idx_j = binIndices[j];
        if (particles[idx_i].size > 0) { //may be using individual particle size
          setParticleSize(particlesize); // updates base particleHardRadius
          collDistSq = (particleHardRadius << 1) + (((uint32_t)particles[idx_i].size + (uint32_t)particles[idx_j].size) >> 1); // collision distance note: not 100% clear why the >> 1 is needed, but it is.
          collDistSq = collDistSq * collDistSq; // square it for faster comparison
        }
        int32_t dx = particles[idx_j].x - particles[idx_i].x;
        if (dx * dx < collDistSq) { // check x direction, if close, check y direction (squaring is faster than abs() or dual compare)
          int32_t dy = particles[idx_j].y - particles[idx_i].y;
          if (dy * dy < collDistSq) // particles are close
            collideParticles(particles[idx_i], particles[idx_j], dx, dy, collDistSq);
        }
      }
    }
  }
  collisionStartIdx = nextFrameStartIdx; // set the start index for the next frame
}

// handle a collision if close proximity is detected, i.e. dx and/or dy smaller than 2*PS_P_RADIUS
// takes two pointers to the particles to collide and the particle hardness (softer means more energy lost in collision, 255 means full hard)
void ParticleSystem2D::collideParticles(PSparticle &particle1, PSparticle &particle2, int32_t dx, int32_t dy, const int32_t collDistSq) {
  int32_t distanceSquared = dx * dx + dy * dy;
  // Calculate relative velocity (if it is zero, could exit but extra check does not overall speed but deminish it)
  int32_t relativeVx = (int32_t)particle2.vx - (int32_t)particle1.vx;
  int32_t relativeVy = (int32_t)particle2.vy - (int32_t)particle1.vy;

  // if dx and dy are zero (i.e. same position) give them an offset, if speeds are also zero, also offset them (pushes particles apart if they are clumped before enabling collisions)
  if (distanceSquared == 0) {
    // Adjust positions based on relative velocity direction
    dx = -1;
    if (relativeVx < 0) // if true, particle2 is on the right side
      dx = 1;
    else if (relativeVx == 0)
      relativeVx = 1;

    dy = -1;
    if (relativeVy < 0)
      dy = 1;
    else if (relativeVy == 0)
      relativeVy = 1;

    distanceSquared = 2; // 1 + 1
  }

  // Calculate dot product of relative velocity and relative distance
  int32_t dotProduct = (dx * relativeVx + dy * relativeVy); // is always negative if moving towards each other

  if (dotProduct < 0) {// particles are moving towards each other
    // integer math used to avoid floats.
    // overflow check: dx/dy are 7bit, relativV are 8bit -> dotproduct is 15bit, dotproduct/distsquared ist 8b, multiplied by collisionhardness of 8bit. so a 16bit shift is ok, make it 15 to be sure no overflows happen
    // note: cannot use right shifts as bit shifting in right direction is asymmetrical for positive and negative numbers and this needs to be accurate! the trick is: only shift positive numers
    // Calculate new velocities after collision
    int32_t surfacehardness = 1 + max(collisionHardness, (int32_t)PS_P_MINSURFACEHARDNESS); // if particles are soft, the impulse must stay above a limit or collisions slip through at higher speeds, 170 seems to be a good value
    int32_t impulse = (((((-dotProduct) << 15) / distanceSquared) * surfacehardness) >> 8); // note: inverting before bitshift corrects for asymmetry in right-shifts (is slightly faster)

    #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP8266) // use bitshifts with rounding instead of division (2x faster)
    int32_t ximpulse = (impulse * dx + ((dx >> 31) & 32767)) >> 15; // note: extracting sign bit and adding rounding value to correct for asymmetry in right shifts
    int32_t yimpulse = (impulse * dy + ((dy >> 31) & 32767)) >> 15;
    #else
    int32_t ximpulse = (impulse * dx) / 32767;
    int32_t yimpulse = (impulse * dy) / 32767;
    #endif
    particle1.vx -= ximpulse; // note: impulse is inverted, so subtracting it
    particle1.vy -= yimpulse;
    particle2.vx += ximpulse;
    particle2.vy += yimpulse;

    // if one of the particles is fixed, transfer the impulse back so it bounces
    if (particle1.fixed) {
      particle2.vx = -particle1.vx;
      particle2.vy = -particle1.vy;
    } else if (particle2.fixed) {
      particle1.vx = -particle2.vx;
      particle1.vy = -particle2.vy;
    }

    if (collisionHardness < PS_P_MINSURFACEHARDNESS && (seg->call & 0x07) == 0) { // if particles are soft, they become 'sticky' i.e. apply some friction (they do pile more nicely and stop sloshing around)
      const uint32_t coeff = collisionHardness + (255 - PS_P_MINSURFACEHARDNESS);
      // Note: could call applyFriction, but this is faster and speed is key here
      #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP8266) // use bitshifts with rounding instead of division (2x faster)
      particle1.vx = ((int32_t)particle1.vx * coeff + (((int32_t)particle1.vx >> 31) & 0xFF)) >> 8; // note: (v>>31) & 0xFF)) extracts the sign and adds 255 if negative for correct rounding using shifts
      particle1.vy = ((int32_t)particle1.vy * coeff + (((int32_t)particle1.vy >> 31) & 0xFF)) >> 8;
      particle2.vx = ((int32_t)particle2.vx * coeff + (((int32_t)particle2.vx >> 31) & 0xFF)) >> 8;
      particle2.vy = ((int32_t)particle2.vy * coeff + (((int32_t)particle2.vy >> 31) & 0xFF)) >> 8;
      #else // division is faster on ESP32, S2 and S3
      particle1.vx = ((int32_t)particle1.vx * coeff) / 255;
      particle1.vy = ((int32_t)particle1.vy * coeff) / 255;
      particle2.vx = ((int32_t)particle2.vx * coeff) / 255;
      particle2.vy = ((int32_t)particle2.vy * coeff) / 255;
      #endif
    }

    // particles have volume, push particles apart if they are too close
    // tried lots of configurations, it works best if not moved but given a little velocity, it tends to oscillate less this way
    // when hard pushing by offsetting position, they sink into each other under gravity
    // a problem with giving velocity is, that on harder collisions, this adds up as it is not dampened enough, so add friction in the FX if required
    if (distanceSquared < collDistSq && dotProduct > -250) { // too close and also slow, push them apart
      int32_t notsorandom = dotProduct & 0x01; //dotprouct LSB should be somewhat random, so no need to calculate a random number
      int32_t pushamount = 1 + ((250 + dotProduct) >> 6); // the closer dotproduct is to zero, the closer the particles are
      int32_t push = 0;
      if (dx < 0)  // particle 1 is on the right
        push = pushamount;
      else if (dx > 0)
        push = -pushamount;
      else { // on the same x coordinate, shift it a little so they do not stack
        if (!particle1.fixed) {
          if (notsorandom)
            particle1.x++; // move it so pile collapses
          else
            particle1.x--;
        }
      }
      if (!particle1.fixed) particle1.vx += push;
      push = 0;
      if (dy < 0)
        push = pushamount;
      else if (dy > 0)
        push = -pushamount;
      else { // dy==0
        if (!particle1.fixed) {
          if (notsorandom)
            particle1.y++; // move it so pile collapses
          else
            particle1.y--;
        }
      }
      if (!particle1.fixed) particle1.vy += push;

      // note: pushing may push particles out of frame, if bounce is active, it will move it back as position will be limited to within frame, if bounce is disabled: bye bye
      if (collisionHardness < 5) { // if they are very soft, stop slow particles completely to make them stick to each other
        particle1.vx = 0;
        particle1.vy = 0;
        particle2.vx = 0;
        particle2.vy = 0;
        //push them apart
        particle1.x += push;
        particle1.y += push;
      }
    }
  }
}

// update size and pointers (memory location and size can change dynamically)
// note: do not access the PS class in FX before running this function (or it messes up SEGENV.data)
void ParticleSystem2D::updateSystem(Segment *s) {
  //PSPRINTLN("updateSystem2D");
  seg = s;
  setMatrixSize(Segment::vWidth(), Segment::vHeight()); //(seg->virtualWidth(), seg->virtualHeight());
  updatePSpointers(advPartProps != nullptr, advPartSize != nullptr); // update pointers to PS data, also updates availableParticles
  setUsedParticles(fractionOfParticlesUsed); // update used particles based on percentage (can change during transitions, execute each frame for code simplicity)
  //PSPRINTLN("END update System2D, running FX...");
}

// set the pointers for the class (this only has to be done once and not on every FX call, only the class pointer needs to be reassigned to SEGENV.data every time)
// function returns the pointer to the next byte available for the FX (if it assigned more memory for other stuff using the above allocate function)
// FX handles the PSsources, need to tell this function how many there are
void ParticleSystem2D::updatePSpointers(bool isadvanced, bool sizecontrol) {
  //PSPRINTLN("updatePSpointers");
  // DEBUG_PRINT(F("*** PS pointers ***"));
  // DEBUG_PRINTF_P(PSTR("this PS %p "), this);
  // Note on memory alignment:
  // a pointer MUST be 4 byte aligned. sizeof() in a struct/class is always aligned to the largest element. if it contains a 32bit, it will be padded to 4 bytes, 16bit is padded to 2byte alignment.
  // The PS is aligned to 4 bytes, a PSparticle is aligned to 2 and a struct containing only byte sized variables is not aligned at all and may need to be padded when dividing the memoryblock.
  // by making sure that the number of sources and particles is a multiple of 4, padding can be skipped here as alignent is ensured, independent of struct sizes.
  sources = reinterpret_cast<PSsource *>((byte*)this + sizeof(ParticleSystem2D)); // pointer to source(s)
  particles = reinterpret_cast<PSparticle *>((byte*)sources + numSources * sizeof(PSsource)); // memory is allocated for PS and particles in one block
  PSdataEnd = reinterpret_cast<uint8_t *>((byte*)particles + numParticles * sizeof(PSparticle)); // pointer to first available byte after the PS for FX additional data
  if (isadvanced) {
    advPartProps = reinterpret_cast<PSadvancedParticle *>((byte*)PSdataEnd); // align to 2 bytes, this is the first byte after the particle flags
    PSdataEnd = reinterpret_cast<uint8_t *>((byte*)advPartProps + numParticles * sizeof(PSadvancedParticle));
    if (sizecontrol) {
      advPartSize = reinterpret_cast<PSsizeControl *>((byte*)PSdataEnd); // aligned(1)
      PSdataEnd = reinterpret_cast<uint8_t *>((byte*)advPartSize + numParticles * sizeof(PSsizeControl));
    }
  }
#ifdef WLED_DEBUG_PS
  //Serial.printf_P(PSTR(" this %p (%u) -> 0x%x\n"), this, sizeof(ParticleSystem2D), (uint32_t)this + sizeof(ParticleSystem2D));
  //Serial.printf_P(PSTR(" sources %p (%u) -> 0x%x\n"), sources, numSources * sizeof(PSsource), (uint32_t)sources + numSources * sizeof(PSsource));
  //Serial.printf_P(PSTR(" particles %p (%u) -> 0x%x\n"), particles, numParticles * sizeof(PSparticle), (uint32_t)particles + numParticles * sizeof(PSparticle));
  //Serial.printf_P(PSTR(" adv. props %p\n"), advPartProps);
  //Serial.printf_P(PSTR(" adv. ctrl %p\n"), advPartSize);
  //Serial.printf_P(PSTR("end %p\n"), PSdataEnd);
#endif
}

// blur a matrix in x and y direction, blur can be asymmetric in x and y
// for speed, 1D array and 32bit variables are used, make sure to limit them to 8bit (0-255) or result is undefined
// to blur a subset of the buffer, change the xsize/ysize and set xstart/ystart to the desired starting coordinates (default start is 0/0)
// subset blurring only works on 10x10 buffer (single particle rendering), if other sizes are needed, buffer width must be passed as parameter
void blur2D(uint32_t *colorbuffer, uint32_t xsize, uint32_t ysize, uint32_t xblur, uint32_t yblur, uint32_t xstart, uint32_t ystart, bool isparticle) {
  uint32_t seeppart, carryover;
  uint32_t seep = xblur >> 1;
  uint32_t width = xsize; // width of the buffer, used to calculate the index of the pixel

  if (isparticle) { //first and last row are always black in first pass of particle rendering
    ystart++;
    ysize--;
    width = 10; // buffer size is 10x10
  }

  for (uint32_t y = ystart; y < ystart + ysize; y++) {
    carryover = BLACK;
    uint32_t indexXY = xstart + y * width;
    for (uint32_t x = xstart; x < xstart + xsize; x++) {
      seeppart = colorbuffer[indexXY]; // create copy of current color
      seeppart = color_fade(seeppart, seep); // scale it and seep to neighbours
      if (x > 0) {
        fast_color_add(colorbuffer[indexXY - 1], seeppart);
        if (carryover) // note: check adds overhead but is faster on average
          fast_color_add(colorbuffer[indexXY], carryover);
      }
      carryover = seeppart;
      indexXY++; // next pixel in x direction
    }
  }

  if (isparticle) { // first and last row are now smeared
    ystart--;
    ysize++;
  }

  seep = yblur >> 1;
  for (uint32_t x = xstart; x < xstart + xsize; x++) {
    carryover = BLACK;
    uint32_t indexXY = x + ystart * width;
    for(uint32_t y = ystart; y < ystart + ysize; y++) {
      seeppart = colorbuffer[indexXY]; // create copy of current color
      seeppart = color_fade(seeppart, seep); // scale it and seep to neighbours
      if (y > 0) {
        fast_color_add(colorbuffer[indexXY - width], seeppart);
        if (carryover) // note: check adds overhead but is faster on average
          fast_color_add(colorbuffer[indexXY], carryover);
      }
      carryover = seeppart;
      indexXY += width; // next pixel in y direction
    }
  }
}

#endif // WLED_DISABLE_PARTICLESYSTEM2D


////////////////////////
// 1D Particle System //
////////////////////////
#ifndef WLED_DISABLE_PARTICLESYSTEM1D

//non class functions to use for initialization, fraction is uint8_t: 255 means 100%
static uint32_t calculateNumberOfParticles1D(uint32_t numberofParticles, uint32_t fraction, uint32_t requestedSources) {
  const uint32_t particlelimit = PS_MAXPARTICLES_1D; // maximum number of paticles allowed
  const uint32_t maxAllowedMemory = MAX_SEGMENT_DATA/strip.getActiveSegmentsNum() - sizeof(ParticleSystem1D) - requestedSources * sizeof(PSsource1D); // more segments, less memory

  numberofParticles = min(numberofParticles, particlelimit); // limit to particlelimit
  // limit number of particles to fit in memory
  uint32_t requiredmemory = numberofParticles * sizeof(PSparticle1D);
  if (requiredmemory > maxAllowedMemory) {
    numberofParticles = numberofParticles * maxAllowedMemory / requiredmemory; // reduce number of particles to fit in memory
  }
  numberofParticles = (numberofParticles * (fraction + 1)) >> 8; // calculate fraction of particles
  numberofParticles = numberofParticles < 20 ? 20 : numberofParticles; // 20 minimum
  //make sure it is a multiple of 4 for proper memory alignment (easier than using padding bytes)
  numberofParticles = ((numberofParticles+3) & ~3U); // >> 2) << 2; // note: with a separate particle buffer, this is probably unnecessary
  return numberofParticles;
}

static uint32_t calculateNumberOfSources1D(uint32_t requestedSources) {
  uint32_t numberofSources = min(requestedSources, PS_MAXSOURCES_1D); // limit to 1 - 32
  return max(1U, numberofSources);
}

uint32_t get1DPSmemoryRequirements(uint32_t length, uint32_t fraction, uint32_t requestedSources) {
  uint32_t numParticles = calculateNumberOfParticles1D(length, fraction, requestedSources);
  uint32_t numSources = calculateNumberOfSources1D(requestedSources);
  uint32_t requiredmemory = sizeof(ParticleSystem1D);
  requiredmemory += sizeof(PSparticle1D) * numParticles;
  requiredmemory += sizeof(PSsource1D) * numSources;
  requiredmemory = ((requiredmemory + 7) & ~3U); // add 4 bytes for padding and round up to 4 byte alignment
  return requiredmemory;
}

ParticleSystem1D::ParticleSystem1D(Segment *s, uint32_t fraction, uint32_t numberofsources) {
  seg = s;
  numParticles = calculateNumberOfParticles1D(Segment::vLength(), fraction, numberofsources); // number of particles allocated in init
  numSources = calculateNumberOfSources1D(numberofsources);
  availableParticles = numParticles; // use all
  setUsedParticles(255); // fraction of particles used (0-255)
  updatePSpointers(); // set the particle and sources pointer (call this before accessing sprays or particles)  
  setSize(Segment::vLength()); //(seg->virtualLength());
  setWallHardness(255); // set default wall hardness to max
  setGravity(0); //gravity disabled by default
  setParticleSize(0); // 1 pixel size by default
  motionBlur = 0; //no fading by default
  smearBlur = 0; //no smearing by default
  emitIndex = 0;
  collisionStartIdx = 0;
  // initialize some default non-zero values most FX use
  for (uint32_t i = 0; i < numSources; i++) {
    sources[i].source.sat = 255; // set full saturation
    sources[i].source.ttl = 1; //set source alive
  }
  for (uint32_t i = 0; i < numParticles; i++) {
    particles[i].sat = 255; // set full saturation
  }
}

// update function applies gravity, moves the particles, handles collisions and renders the particles
void ParticleSystem1D::update(void) {
  //apply gravity globally if enabled
  if (particlesettings.useGravity) //note: in 1D system, applying gravity after collisions also works but may be worse
    applyGravity();

  // handle collisions (can push particles, must be done before updating particles or they can render out of bounds, causing a crash if using local buffer for speed)
  if (particlesettings.useCollisions)
    handleCollisions();

  //move all particles
  for (uint32_t i = 0; i < usedParticles; i++) {
    particleMoveUpdate(particles[i], nullptr);
  }

  if (particlesettings.colorByPosition) {
    uint32_t scale = (255 << 16) / maxX;  // speed improvement: multiplication is faster than division
    for (uint32_t i = 0; i < usedParticles; i++) {
      particles[i].hue = (scale * particles[i].x) >> 16; // note: x is > 0 if not out of bounds
    }
  }

  render();
}

// set percentage of used particles as uint8_t i.e 127 means 50% for example
void ParticleSystem1D::setUsedParticles(const uint8_t percentage) {
  fractionOfParticlesUsed = percentage; // note usedParticles is updated in memory manager
  updateUsedParticles(numParticles, availableParticles, fractionOfParticlesUsed, usedParticles);
  //PSPRINTLN("SetUsedpaticles:");
  //PSPRINT(" allocated particles: "); PSPRINTLN(numParticles);
  //PSPRINT(" available particles: "); PSPRINTLN(availableParticles);
  //PSPRINT(" percent: "); PSPRINTLN(fractionOfParticlesUsed*100/255);
  //PSPRINT(" used particles: "); PSPRINTLN(usedParticles);
}

void ParticleSystem1D::setWallHardness(const uint8_t hardness) {
  wallHardness = hardness;
}

void ParticleSystem1D::setSize(const uint32_t x) {
  maxXpixel = x - 1; // last physical pixel that can be drawn to
  maxX = x * PS_P_RADIUS_1D - 1;  // particle system boundary for movements
}

void ParticleSystem1D::setWrap(const bool enable) {
  particlesettings.wrapX = enable;
}

void ParticleSystem1D::setBounce(const bool enable) {
  particlesettings.bounceX = enable;
}

void ParticleSystem1D::setKillOutOfBounds(const bool enable) {
  particlesettings.killoutofbounds = enable;
}

void ParticleSystem1D::setColorByAge(const bool enable) {
  particlesettings.colorByAge = enable;
}

void ParticleSystem1D::setColorByPosition(const bool enable) {
  particlesettings.colorByPosition = enable;
}

void ParticleSystem1D::setMotionBlur(const uint8_t bluramount) {
  motionBlur = bluramount;
}

void ParticleSystem1D::setSmearBlur(const uint8_t bluramount) {
  smearBlur = bluramount;
}

// render size, 0 = 1 pixel, 1 = 2 pixel (interpolated), bigger sizes require adanced properties
void ParticleSystem1D::setParticleSize(const uint8_t size) {
  particlesize = size > 0 ? 1 : 0; // TODO: add support for global sizes? see note above (motion blur)
  particleHardRadius = PS_P_MINHARDRADIUS_1D >> (!particlesize); // 2 pixel sized particles or single pixel sized particles
}

// enable/disable gravity, optionally, set the force (force=8 is default) can be -127 to +127, 0 is disable
// if enabled, gravity is applied to all particles in ParticleSystemUpdate()
// force is in 3.4 fixed point notation so force=16 means apply v+1 each frame default of 8 is every other frame (gives good results)
void ParticleSystem1D::setGravity(const int8_t force) {
  if (force) {
    gforce = force;
    particlesettings.useGravity = true;
  }
  else
    particlesettings.useGravity = false;
}

void ParticleSystem1D::enableParticleCollisions(const bool enable, const uint8_t hardness) {
  particlesettings.useCollisions = enable;
  collisionHardness = hardness;
}

// emit one particle with variation, returns index of last emitted particle (or -1 if no particle emitted)
int32_t ParticleSystem1D::sprayEmit(const PSsource1D &emitter) {
  for (uint32_t i = 0; i < usedParticles; i++) {
    emitIndex++;
    if (emitIndex >= usedParticles)
      emitIndex = 0;
    if (particles[emitIndex].ttl == 0) { // find a dead particle
      particles[emitIndex].vx = emitter.v + hw_random16(emitter.var << 1) - emitter.var; // random(-var,var)
      particles[emitIndex].x = emitter.source.x;
      particles[emitIndex].hue = emitter.source.hue;
      particles[emitIndex].ttl = hw_random16(emitter.minLife, emitter.maxLife);
      particles[emitIndex].collide = emitter.source.collide;
      particles[emitIndex].reversegrav = emitter.source.reversegrav;
      particles[emitIndex].perpetual = emitter.source.perpetual;
      particles[emitIndex].sat = emitter.sat;
      particles[emitIndex].size = emitter.size;
      return emitIndex;
    }
  }
  return -1;
}

// particle moves, decays and dies, if killoutofbounds is set, out of bounds particles are set to ttl=0
// uses passed settings to set bounce or wrap, if useGravity is set, it will never bounce at the top and killoutofbounds is not applied over the top
void ParticleSystem1D::particleMoveUpdate(PSparticle1D &part, PSsettings *options) {
  if (options == nullptr)
    options = &particlesettings; // use PS system settings by default

  if (part.ttl > 0) {
    if (!part.perpetual)
      part.ttl--; // age
    if (options->colorByAge)
      part.hue = min(part.ttl, (uint16_t)255); // set color to ttl

    int32_t renderradius = PS_P_HALFRADIUS_1D; // used to check out of bounds, default for 2 pixel rendering
    int32_t newX = part.x + (int32_t)part.vx;
    part.outofbounds = false; // reset out of bounds (in case particle was created outside the matrix and is now moving into view)

    if (part.size > 1)
      particleHardRadius = PS_P_MINHARDRADIUS_1D + (part.size >> 1);
    else // single pixel particles use half the collision distance for walls
      particleHardRadius = PS_P_MINHARDRADIUS_1D >> 1;
    renderradius = particleHardRadius; // note: for single pixel particles, it should be zero, but it does not matter as out of bounds checking is done in rendering function

    // if wall collisions are enabled, bounce them before they reach the edge, it looks much nicer if the particle is not half out of view
    if (options->bounceX) {
      if ((newX < (int32_t)particleHardRadius) || ((newX > (int32_t)(maxX - particleHardRadius)))) { // reached a wall
        bool bouncethis = true;
        if (options->useGravity) {
          if (part.reversegrav) { // skip bouncing at x = 0
            if (newX < (int32_t)particleHardRadius)
              bouncethis = false;
          } else if (newX > (int32_t)particleHardRadius) { // skip bouncing at x = max
            bouncethis = false;
          }
        }
        if (bouncethis) {
          part.vx = -part.vx; // invert speed
          part.vx = ((int32_t)part.vx * (int32_t)wallHardness) / 255; // reduce speed as energy is lost on non-hard surface
          if (newX < (int32_t)particleHardRadius)
            newX = particleHardRadius; // fast particles will never reach the edge if position is inverted, this looks better
          else
            newX = maxX - particleHardRadius;
        }
      }
    }

    if (!checkBoundsAndWrap(newX, maxX, renderradius, options->wrapX)) { // check out of bounds note: this must not be skipped or it can lead to crashes
      part.outofbounds = true;
      if (options->killoutofbounds) {
        bool killthis = true;
        if (options->useGravity) { // if gravity is used, only kill below 'floor level'
          if (part.reversegrav) { // skip at x = 0, do not skip far out of bounds
            if (newX < 0 || newX > maxX << 2)
              killthis = false;
          } else { // skip at x = max, do not skip far out of bounds
            if (newX > 0 &&  newX < maxX << 2)
              killthis = false;
          }
        }
        if (killthis)
          part.ttl = 0;
      }
    }

    if (!part.fixed)
      part.x = newX; // set new position
    else
      part.vx = 0; // set speed to zero. note: particle can get speed in collisions, if unfixed, it should not speed away
  }
}

// apply a force in x direction to individual particle (or source)
// caller needs to provide a 8bit counter (for each paticle) that holds its value between calls
// force is in 3.4 fixed point notation so force=16 means apply v+1 each frame default of 8 is every other frame
void ParticleSystem1D::applyForce(PSparticle1D &part, const int8_t xforce, uint8_t &counter) {
  int32_t dv = calcForce_dv(xforce, counter); // velocity increase
  part.vx = limitSpeed((int32_t)part.vx + dv);   // apply the force to particle
}

// apply a force to all particles
// force is in 3.4 fixed point notation (see above)
void ParticleSystem1D::applyForce(const int8_t xforce) {
  int32_t dv = calcForce_dv(xforce, forcecounter); // velocity increase
  for (uint32_t i = 0; i < usedParticles; i++) {
    particles[i].vx = limitSpeed((int32_t)particles[i].vx + dv);
  }
}

// apply gravity to all particles using PS global gforce setting
// gforce is in 3.4 fixed point notation, see note above
void ParticleSystem1D::applyGravity() {
  int32_t dv_raw = calcForce_dv(gforce, gforcecounter);
  for (uint32_t i = 0; i < usedParticles; i++) {
    int32_t dv = dv_raw;
    if (particles[i].reversegrav) dv = -dv_raw;
    // note: not checking if particle is dead is omitted as most are usually alive and if few are alive, rendering is fast anyways
    particles[i].vx = limitSpeed((int32_t)particles[i].vx - dv);
  }
}

// apply gravity to single particle using system settings (use this for sources)
// function does not increment gravity counter, if gravity setting is disabled, this cannot be used
void ParticleSystem1D::applyGravity(PSparticle1D &part) {
  uint32_t counterbkp = gforcecounter;
  int32_t dv = calcForce_dv(gforce, gforcecounter);
  if (part.reversegrav) dv = -dv;
  gforcecounter = counterbkp; //save it back
  part.vx = limitSpeed((int32_t)part.vx - dv);
}


// slow down particle by friction, the higher the speed, the higher the friction. a high friction coefficient slows them more (255 means instant stop)
// note: a coefficient smaller than 0 will speed them up (this is a feature, not a bug), coefficient larger than 255 inverts the speed, so don't do that
void ParticleSystem1D::applyFriction(int32_t coefficient) {
  #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP8266) // use bitshifts with rounding instead of division (2x faster)
  int32_t friction = 256 - coefficient;
  for (uint32_t i = 0; i < usedParticles; i++) {
    if (particles[i].ttl)
      particles[i].vx = ((int32_t)particles[i].vx * friction + (((int32_t)particles[i].vx >> 31) & 0xFF)) >> 8; // note: (v>>31) & 0xFF)) extracts the sign and adds 255 if negative for correct rounding using shifts
  }
  #else // division is faster on ESP32, S2 and S3
  int32_t friction = 255 - coefficient;
  for (uint32_t i = 0; i < usedParticles; i++) {
    if (particles[i].ttl)
      particles[i].vx = ((int32_t)particles[i].vx * friction) / 255;
  }
  #endif
  
}


// render particles to the LED buffer (uses palette to render the 8bit particle color value)
// if wrap is set, particles half out of bounds are rendered to the other side of the matrix
// warning: do not render out of bounds particles or system will crash! rendering does not check if particle is out of bounds
void ParticleSystem1D::render() {
  uint32_t baseRGB;
  uint32_t brightness; // particle brightness, fades if dying

  // update global blur (used for blur transitions)
  //int32_t motionbluramount = motionBlur;
  int32_t smearamount = smearBlur;
  globalSmear = smearamount;

  if (motionBlur > 0) seg->fadeToSecondaryBy(255 - motionBlur);
  else                seg->fill(SEGCOLOR(1)); // clear the buffer before rendering to it

  // go over particles and render them to the buffer
  for (uint32_t i = 0; i < usedParticles; i++) {
    if ( particles[i].ttl == 0 || particles[i].outofbounds)
      continue;

    // generate RGB values for particle
    brightness = min(particles[i].ttl << 1, (int)255);
    baseRGB = seg->color_from_palette(particles[i].hue, false, false, 0, 255); //ColorFromPaletteWLED(SEGPALETTE, particles[i].hue, 255);

    if (particles[i].sat < 255) {
      CHSV32 baseHSV;
      rgb2hsv(baseRGB, baseHSV); // convert to HSV
      baseHSV.s = particles[i].sat; // set the saturation
      hsv2rgb(baseHSV, baseRGB); // convert back to RGB
    }
    renderParticle(i, brightness, baseRGB, particlesettings.wrapX);
  }
  // apply smear-blur to rendered frame
  if (globalSmear > 0) {
    seg->blur(globalSmear, true);
  }
}

// calculate pixel positions and brightness distribution and render the particle to local buffer or global buffer
void ParticleSystem1D::renderParticle(uint32_t particleindex, uint32_t brightness, uint32_t color, bool wrap) {
  //uint32_t size = particlesize;
  uint32_t size = particles[particleindex].size;
  if (size == 0) { //single pixel particle, can be out of bounds as oob checking is made for 2-pixel particles (and updating it uses more code)
    uint32_t x =  particles[particleindex].x >> PS_P_RADIUS_SHIFT_1D;
    if (x <= (uint32_t)maxXpixel) { //by making x unsigned there is no need to check < 0 as it will overflow
      // add particle pixel to existing pixel
      uint32_t c = seg->getPixelColorRaw(x);
      fast_color_add(c, color, brightness);
      seg->setPixelColorRaw(x, c);
    }
    return;
  }
  //render larger particles
  bool pxlisinframe[2] = {true, true};
  int32_t pxlbrightness[2];
  int32_t pixco[2]; // physical pixel coordinates of the two pixels representing a particle

  // add half a radius as the rendering algorithm always starts at the bottom left, this leaves things positive, so shifts can be used, then shift coordinate by a full pixel (x-- below)
  int32_t xoffset = particles[particleindex].x + PS_P_HALFRADIUS_1D;
  int32_t dx = xoffset & (PS_P_RADIUS_1D - 1); //relativ particle position in subpixel space,  modulo replaced with bitwise AND
  int32_t x = xoffset >> PS_P_RADIUS_SHIFT_1D; // divide by PS_P_RADIUS, bitshift of negative number stays negative -> checking below for x < 0 works (but does not when using division)

  // set the raw pixel coordinates
  pixco[1] = x;  // right pixel
  x--; // shift by a full pixel here, this is skipped above to not do -1 and then +1
  pixco[0] = x;  // left pixel

  //calculate the brightness values for both pixels using linear interpolation (note: in standard rendering out of frame pixels could be skipped but if checks add more clock cycles over all)
  pxlbrightness[0] = (((int32_t)PS_P_RADIUS_1D - dx) * brightness) >> PS_P_SURFACE_1D;
  pxlbrightness[1] = (dx * brightness) >> PS_P_SURFACE_1D;

  // check if particle has size > 1 (2 pixels)
  if (particles[particleindex].size > 1) {
    uint32_t renderbuffer[100];
    memset(renderbuffer, 0, 100 * sizeof(uint32_t)); // clear the buffer, renderbuffer is 10x10 pixels

    //render particle to a bigger size
    //particle size to pixels: 2 - 63 is 4 pixels, < 128 is 6pixels, < 192 is 8 pixels, bigger is 10 pixels
    //first, render the pixel to the center of the renderbuffer, then apply 1D blurring
    fast_color_add(renderbuffer[4], color, pxlbrightness[0]);
    fast_color_add(renderbuffer[5], color, pxlbrightness[1]);
    uint32_t rendersize = 2; // initialize render size, minimum is 4 pixels, it is incremented int he loop below to start with 4
    uint32_t offset = 4; // offset to zero coordinate to write/read data in renderbuffer (actually needs to be 3, is decremented in the loop below)
    uint32_t blurpasses = size/64 + 1; // number of blur passes depends on size, four passes max
    uint32_t bitshift = 0;
    for (uint32_t i = 0; i < blurpasses; i++) {
      if (i == 2) //for the last two passes, use higher amount of blur (results in a nicer brightness gradient with soft edges)
        bitshift = 1;
      rendersize += 2;
      offset--;
      blur1D(renderbuffer, rendersize, size << bitshift, offset);
      size = size > 64 ? size - 64 : 0;
    }

    // calculate origin coordinates to render the particle
    uint32_t xfb_orig = x - (rendersize>>1) + 1 - offset; //note: using uint is fine
    uint32_t xfb; // coordinates in frame buffer to write to note: by making this uint, only overflow has to be checked

    // transfer particle renderbuffer
    for (uint32_t xrb = offset; xrb < rendersize+offset; xrb++) {
      xfb = xfb_orig + xrb;
      if (xfb > (uint32_t)maxXpixel) {
        if (wrap) { // wrap x to the other side if required
          if (xfb > (uint32_t)maxXpixel << 1) // xfb is "negative"
            xfb = (maxXpixel + 1) + (int32_t)xfb; // this always overflows to within bounds
          else
            xfb = xfb % (maxXpixel + 1); // note: without the above "negative" check, this works only for powers of 2
        } else
          continue;
      }
      // add particle pixel to existing pixel
      uint32_t c = seg->getPixelColorRaw(xfb);
      fast_color_add(c, renderbuffer[xrb]);
      seg->setPixelColorRaw(xfb, c);
    }
  } else { // standard rendering (2 pixels per particle)
    // check if any pixels are out of frame
    if (pixco[0] < 0) { // left pixels out of frame
      if (wrap) // wrap x to the other side if required
        pixco[0] = maxXpixel;
      else
        pxlisinframe[0] = false; // pixel is out of matrix boundaries, do not render
    }
    if (pixco[1] > (int32_t)maxXpixel) { // right pixel, only has to be checkt if left pixel did not overflow
      if (wrap) // wrap y to the other side if required
        pixco[1] = 0;
      else
        pxlisinframe[1] = false;
    }
    for (uint32_t i = 0; i < 2; i++) {
      if (pxlisinframe[i]) {
        // add particle pixel to existing pixel
        uint32_t c = seg->getPixelColorRaw(pixco[i]);
        fast_color_add(c, color, pxlbrightness[i]);
        seg->setPixelColorRaw(pixco[i], c);
      }
    }
  }
}

// detect collisions in an array of particles and handle them
void ParticleSystem1D::handleCollisions() {
  int32_t collisiondistance = particleHardRadius << 1;
  // note: partices are binned by position, assumption is that no more than half of the particles are in the same bin
  // if they are, collisionStartIdx is increased so each particle collides at least every second frame (which still gives decent collisions)
  constexpr int BIN_WIDTH = 32 * PS_P_RADIUS_1D; // width of each bin, a compromise between speed and accuracy (lareger bins are faster but collapse more)
  int32_t overlap = particleHardRadius << 1; // overlap bins to include edge particles to neighbouring bins
  //if (advPartProps) //may be using individual particle size
    overlap += 256; // add 2 * max radius (approximately)
  uint32_t maxBinParticles = max((uint32_t)50, (usedParticles + 1) / 4); // do not bin small amounts, limit max to 1/2 of particles
  uint32_t numBins = (maxX + (BIN_WIDTH - 1)) / BIN_WIDTH; // calculate number of bins
  uint16_t binIndices[maxBinParticles]; // array to store indices of particles in a bin
  uint32_t binParticleCount; // number of particles in the current bin
  uint16_t nextFrameStartIdx = hw_random16(usedParticles); // index of the first particle in the next frame (set to fixed value if bin overflow)
  uint32_t pidx = collisionStartIdx; //start index in case a bin is full, process remaining particles next frame
  for (uint32_t bin = 0; bin < numBins; bin++) {
    binParticleCount = 0; // reset for this bin
    int32_t binStart = bin * BIN_WIDTH - overlap; // note: first bin will extend to negative, but that is ok as out of bounds particles are ignored
    int32_t binEnd = binStart + BIN_WIDTH + overlap; // note: last bin can be out of bounds, see above

    // fill the binIndices array for this bin
    for (uint32_t i = 0; i < usedParticles; i++) {
      if (particles[pidx].ttl > 0 && !particles[pidx].outofbounds && particles[pidx].collide) { // colliding particle
        if (particles[pidx].x >= binStart && particles[pidx].x <= binEnd) { // >= and <= to include particles on the edge of the bin (overlap to ensure boarder particles collide with adjacent bins)
          if (binParticleCount >= maxBinParticles) { // bin is full, more particles in this bin so do the rest next frame
            nextFrameStartIdx = pidx; // bin overflow can only happen once as bin size is at least half of the particles (or half +1)
            break;
          }
          binIndices[binParticleCount++] = pidx;
        }
      }
      pidx++;
      if (pidx >= usedParticles) pidx = 0; // wrap around
    }

    for (uint32_t i = 0; i < binParticleCount; i++) { // go though all 'higher number' particles and see if any of those are in close proximity and if they are, make them collide
      uint32_t idx_i = binIndices[i];
      for (uint32_t j = i + 1; j < binParticleCount; j++) { // check against higher number particles
        uint32_t idx_j = binIndices[j];
        if (particles[idx_i].size > 0) { // use advanced size properties
          collisiondistance = (PS_P_MINHARDRADIUS_1D << particlesize) + (((uint32_t)particles[idx_i].size + (uint32_t)particles[idx_j].size) >> 1);
        }
        int32_t dx = particles[idx_j].x - particles[idx_i].x;
        int32_t dv = (int32_t)particles[idx_j].vx - (int32_t)particles[idx_i].vx;
        int32_t proximity = collisiondistance;
        if (dv >= proximity) // particles would go past each other in next move update
          proximity += abs(dv); // add speed difference to catch fast particles
        if (dx <= proximity && dx >= -proximity) { // collide if close
          collideParticles(particles[idx_i], particles[idx_j], dx, dv, collisiondistance);
        }
      }
    }
  }
  collisionStartIdx = nextFrameStartIdx; // set the start index for the next frame
}
// handle a collision if close proximity is detected, i.e. dx and/or dy smaller than 2*PS_P_RADIUS
// takes two pointers to the particles to collide and the particle hardness (softer means more energy lost in collision, 255 means full hard)
void ParticleSystem1D::collideParticles(PSparticle1D &particle1, PSparticle1D &particle2, int32_t dx, int32_t relativeVx, const int32_t collisiondistance) {
  int32_t dotProduct = (dx * relativeVx); // is always negative if moving towards each other
  uint32_t distance = abs(dx);
  if (dotProduct < 0) { // particles are moving towards each other
    uint32_t surfacehardness = max(collisionHardness, (int32_t)PS_P_MINSURFACEHARDNESS_1D); // if particles are soft, the impulse must stay above a limit or collisions slip through
    // Calculate new velocities after collision
    int32_t impulse = relativeVx * surfacehardness / 255; // note: not using dot product like in 2D as impulse is purely speed depnedent
    particle1.vx += impulse;
    particle2.vx -= impulse;

    // if one of the particles is fixed, transfer the impulse back so it bounces
    if (particle1.fixed)
      particle2.vx = -particle1.vx;
    else if (particle2.fixed)
      particle1.vx = -particle2.vx;

    if (collisionHardness < PS_P_MINSURFACEHARDNESS_1D && (seg->call & 0x07) == 0) { // if particles are soft, they become 'sticky' i.e. apply some friction
      const uint32_t coeff = collisionHardness + (250 - PS_P_MINSURFACEHARDNESS_1D);
      particle1.vx = ((int32_t)particle1.vx * coeff) / 255;
      particle2.vx = ((int32_t)particle2.vx * coeff) / 255;
    }
  }

  if (distance < (collisiondistance - 8) && abs(relativeVx) < 5) // overlapping and moving slowly
  {
    // particles have volume, push particles apart if they are too close
    // behaviour is different than in 2D, we need pixel accurate stacking here, push the top particle
    // note: like in 2D, pushing by a distance makes softer piles collapse, giving particles speed prevents that and looks nicer
    int32_t pushamount = 1;
    if (dx < 0)  // particle2.x < particle1.x
      pushamount = -pushamount;
    particle1.vx -= pushamount;
    particle2.vx += pushamount;

    if(distance < collisiondistance >> 1) { // too close, force push particles so they dont collapse
      pushamount = 1 + ((collisiondistance - distance) >> 3); // note: push amount found by experimentation

      if(particle1.x < (maxX >> 1)) { // lower half, push particle with larger x in positive direction
        if (dx < 0 && !particle1.fixed) {  // particle2.x < particle1.x  -> push particle 1
          particle1.vx++;// += pushamount;
          particle1.x += pushamount;
        }
        else if (!particle2.fixed) { // particle1.x < particle2.x  -> push particle 2
          particle2.vx++;// += pushamount;
          particle2.x += pushamount;
        }
      }
      else { // upper half, push particle with smaller x
        if (dx < 0 && !particle2.fixed) {  // particle2.x < particle1.x  -> push particle 2
          particle2.vx--;// -= pushamount;
          particle2.x -= pushamount;
        }
        else if (!particle2.fixed) { // particle1.x < particle2.x  -> push particle 1
          particle1.vx--;// -= pushamount;
          particle1.x -= pushamount;
        }
      }
    }
  }
}

// update size and pointers (memory location and size can change dynamically)
// note: do not access the PS class in FX before running this function (or it messes up SEGENV.data)
void ParticleSystem1D::updateSystem(Segment *s) {
  seg = s;
  setSize(Segment::vLength()); //(seg->virtualLength()); // update size
  updatePSpointers();
  setUsedParticles(fractionOfParticlesUsed); // update used particles based on percentage (can change during transitions, execute each frame for code simplicity)
}

// set the pointers for the class (this only has to be done once and not on every FX call, only the class pointer needs to be reassigned to SEGENV.data every time)
// function returns the pointer to the next byte available for the FX (if it assigned more memory for other stuff using the above allocate function)
// FX handles the PSsources, need to tell this function how many there are
void ParticleSystem1D::updatePSpointers() {
  // Note on memory alignment:
  // a pointer MUST be 4 byte aligned. sizeof() in a struct/class is always aligned to the largest element. if it contains a 32bit, it will be padded to 4 bytes, 16bit is padded to 2byte alignment.
  // The PS is aligned to 4 bytes, a PSparticle is aligned to 2 and a struct containing only byte sized variables is not aligned at all and may need to be padded when dividing the memoryblock.
  // by making sure that the number of sources and particles is a multiple of 4, padding can be skipped here as alignent is ensured, independent of struct sizes.

  sources = reinterpret_cast<PSsource1D *>((byte*)this + sizeof(ParticleSystem1D)); // pointer to source(s)
  particles = reinterpret_cast<PSparticle1D *>((byte*)sources + numSources * sizeof(PSsource1D)); // get memory, leave buffer size as is (request 0)
  PSdataEnd = reinterpret_cast<uint8_t *>((byte*)particles + numParticles * sizeof(PSparticle1D)); // pointer to first available byte after the PS for FX additional data
  #ifdef WLED_DEBUG_PS
  //PSPRINTLN(" PS Pointers: ");
  //PSPRINT(" PS : 0x");
  //Serial.println((uintptr_t)this, HEX);
  //PSPRINT(" Sources : 0x");
  //Serial.println((uintptr_t)sources, HEX);
  //PSPRINT(" Particles : 0x");
  //Serial.println((uintptr_t)particles, HEX);
  #endif
}

// blur a 1D buffer, sub-size blurring can be done using start and size
// for speed, 32bit variables are used, make sure to limit them to 8bit (0-255) or result is undefined
// to blur a subset of the buffer, change the size and set start to the desired starting coordinates
void blur1D(uint32_t *colorbuffer, uint32_t size, uint32_t blur, uint32_t start) {
  uint32_t seeppart, carryover;
  uint32_t seep = blur >> 1;
  carryover =  BLACK;
  for (uint32_t x = start; x < start + size; x++) {
    seeppart = colorbuffer[x]; // create copy of current color
    seeppart = color_fade(seeppart, seep); // scale it and seep to neighbours
    if (x > 0) {
      fast_color_add(colorbuffer[x-1], seeppart);
      if (carryover) // note: check adds overhead but is faster on average
        fast_color_add(colorbuffer[x], carryover); // is black on first pass
    }
    carryover = seeppart;
  }
}
#endif // WLED_DISABLE_PARTICLESYSTEM1D

#if !(defined(WLED_DISABLE_PARTICLESYSTEM2D) && defined(WLED_DISABLE_PARTICLESYSTEM1D)) // not both disabled

#endif  // !(defined(WLED_DISABLE_PARTICLESYSTEM2D) && defined(WLED_DISABLE_PARTICLESYSTEM1D))
