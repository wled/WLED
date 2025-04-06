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

// local shared functions (used both in 1D and 2D system)
static int32_t calcForce_dv(const int8_t force, uint8_t &counter);
static bool checkBoundsAndWrap(int32_t &position, const int32_t max, const int32_t particleradius, const bool wrap); // returns false if out of bounds by more than particleradius
static void fast_color_add(CRGB &c1, const CRGB &c2, uint32_t scale = 255); // fast and accurate color adding with scaling (scales c2 before adding)
static void fast_color_scale(CRGB &c, const uint32_t scale); // fast scaling function using 32bit variable and pointer. note: keep 'scale' within 0-255
//static CRGB *allocateCRGBbuffer(uint32_t length);

// global variables for memory management
std::vector<partMem> partMemList; // list of particle memory pointers
partMem *pmem = nullptr; // pointer to particle memory of current segment, updated in particleMemoryManager()
CRGB *framebuffer = nullptr; // local frame buffer for rendering
CRGB *renderbuffer = nullptr; // local particle render buffer for advanced particles
uint16_t frameBufferSize = 0; // size in pixels, used to check if framebuffer is large enough for current segment
uint16_t renderBufferSize = 0; // size in pixels, if allcoated by a 1D system it needs to be updated for 2D
bool renderSolo = false; // is set to true if this is the only particle system using the so it can use the buffer continuously (faster blurring)
int32_t globalBlur = 0; // motion blur to apply if multiple PS are using the buffer
int32_t globalSmear = 0; // smear-blur to apply if multiple PS are using the buffer
#endif

#ifndef WLED_DISABLE_PARTICLESYSTEM2D
ParticleSystem2D::ParticleSystem2D(uint32_t width, uint32_t height, uint32_t numberofparticles, uint32_t numberofsources, bool isadvanced, bool sizecontrol) {
  PSPRINTLN("\n ParticleSystem2D constructor");
  effectID = SEGMENT.mode; // new FX called init, save the effect ID
  numSources = numberofsources; // number of sources allocated in init
  numParticles = numberofparticles; // number of particles allocated in init
  availableParticles = 0; // let the memory manager assign
  fractionOfParticlesUsed = 255; // use all particles by default, usedParticles is updated in updatePSpointers()
  advPartProps = nullptr; //make sure we start out with null pointers (just in case memory was not cleared)
  advPartSize = nullptr;
  updatePSpointers(isadvanced, sizecontrol); // set the particle and sources pointer (call this before accessing sprays or particles)
  setMatrixSize(width, height);
  setWallHardness(255); // set default wall hardness to max
  setWallRoughness(0); // smooth walls by default
  setGravity(0); //gravity disabled by default
  setParticleSize(1); // 2x2 rendering size by default
  motionBlur = 0; //no fading by default
  smearBlur = 0; //no smearing by default
  emitIndex = 0;
  collisionStartIdx = 0;
  lastRender = 0;

  //initialize some default non-zero values most FX use
  for (uint32_t i = 0; i < numSources; i++) {
    sources[i].source.sat = 255; //set saturation to max by default
    sources[i].source.ttl = 1; //set source alive
  }

}

// update function applies gravity, moves the particles, handles collisions and renders the particles
void ParticleSystem2D::update(void) {
  //apply gravity globally if enabled
  if (particlesettings.useGravity)
    applyGravity();

  //update size settings before handling collisions
  if (advPartSize) {
    for (uint32_t i = 0; i < usedParticles; i++) {
      if(updateSize(&advPartProps[i], &advPartSize[i]) == false) { // if particle shrinks to 0 size
        particles[i].ttl = 0; // kill particle
      }
    }
  }

  // handle collisions (can push particles, must be done before updating particles or they can render out of bounds, causing a crash if using local buffer for speed)
  if (particlesettings.useCollisions)
    handleCollisions();

  //move all particles
  for (uint32_t i = 0; i < usedParticles; i++) {
    particleMoveUpdate(particles[i], particleFlags[i], nullptr, advPartProps ? &advPartProps[i] : nullptr); // note: splitting this into two loops is slower and uses more flash
  }

  ParticleSys_render();
}

// update function for fire animation
void ParticleSystem2D::updateFire(const uint8_t intensity,const bool renderonly) {
  if (!renderonly)
    fireParticleupdate();
  fireIntesity = intensity > 0 ? intensity : 1; // minimum of 1, zero checking is used in render function
  ParticleSys_render();
}

// set percentage of used particles as uint8_t i.e 127 means 50% for example
void ParticleSystem2D::setUsedParticles(uint8_t percentage) {
  fractionOfParticlesUsed = percentage; // note usedParticles is updated in memory manager
  updateUsedParticles(numParticles, availableParticles, fractionOfParticlesUsed, usedParticles);
  PSPRINT(" SetUsedpaticles: allocated particles: ");
  PSPRINT(numParticles);
  PSPRINT(" available particles: ");
  PSPRINT(availableParticles);
  PSPRINT(" ,used percentage: ");
  PSPRINT(fractionOfParticlesUsed);
  PSPRINT(" ,used particles: ");
  PSPRINTLN(usedParticles);
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
  if(particlesize > 1) {
    particleHardRadius = max(particleHardRadius, (uint32_t)particlesize); // radius used for wall collisions & particle collisions
    motionBlur = 0; // disable motion blur if particle size is set
  }
  else if (particlesize == 0)
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
      particleFlags[emitIndex].collide = emitter.sourceFlags.collide;
      particles[emitIndex].ttl = hw_random16(emitter.minLife, emitter.maxLife);
      if (advPartProps)
        advPartProps[emitIndex].size = emitter.size;
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
void ParticleSystem2D::particleMoveUpdate(PSparticle &part, PSparticleFlags &partFlags, PSsettings2D *options, PSadvancedParticle *advancedproperties) {
  if (options == nullptr)
    options = &particlesettings; //use PS system settings by default

  if (part.ttl > 0) {
    if (!partFlags.perpetual)
      part.ttl--; // age
    if (options->colorByAge)
      part.hue = min(part.ttl, (uint16_t)255); //set color to ttl

    int32_t renderradius = PS_P_HALFRADIUS; // used to check out of bounds
    int32_t newX = part.x + (int32_t)part.vx;
    int32_t newY = part.y + (int32_t)part.vy;
    partFlags.outofbounds = false; // reset out of bounds (in case particle was created outside the matrix and is now moving into view) note: moving this to checks below adds code and is not faster

    if (advancedproperties) { //using individual particle size?
      setParticleSize(particlesize); // updates default particleHardRadius
      if (advancedproperties->size > PS_P_MINHARDRADIUS) {
        particleHardRadius += (advancedproperties->size - PS_P_MINHARDRADIUS); // update radius
        renderradius = particleHardRadius;
      }
    }
    // note: if wall collisions are enabled, bounce them before they reach the edge, it looks much nicer if the particle does not go half out of view
    if (options->bounceY) {
      if ((newY < (int32_t)particleHardRadius) || ((newY > (int32_t)(maxY - particleHardRadius)) && !options->useGravity)) { // reached floor / ceiling
         bounce(part.vy, part.vx, newY, maxY);
      }
    }

    if(!checkBoundsAndWrap(newY, maxY, renderradius, options->wrapY)) { // check out of bounds  note: this must not be skipped. if gravity is enabled, particles will never bounce at the top
      partFlags.outofbounds = true;
      if (options->killoutofbounds) {
        if (newY < 0) // if gravity is enabled, only kill particles below ground
          part.ttl = 0;
        else if (!options->useGravity)
          part.ttl = 0;
      }
    }

    if(part.ttl) { //check x direction only if still alive
      if (options->bounceX) {
        if ((newX < (int32_t)particleHardRadius) || (newX > (int32_t)(maxX - particleHardRadius))) // reached a wall
          bounce(part.vx, part.vy, newX, maxX);
      }
      else if(!checkBoundsAndWrap(newX, maxX, renderradius, options->wrapX)) { // check out of bounds
        partFlags.outofbounds = true;
        if (options->killoutofbounds)
          part.ttl = 0;
      }
    }

    part.x = (int16_t)newX; // set new position
    part.y = (int16_t)newY; // set new position
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
      particleFlags[i].outofbounds = 0; // reset out of bounds flag  note: moving this to checks below is not faster but adds code
      // check if particle is out of bounds, wrap x around to other side if wrapping is enabled
      // as fire particles start below the frame, lots of particles are out of bounds in y direction. to improve speed, only check x direction if y is not out of bounds
      if (newY < -PS_P_HALFRADIUS)
        particleFlags[i].outofbounds = 1;
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
bool ParticleSystem2D::updateSize(PSadvancedParticle *advprops, PSsizeControl *advsize) {
  if (advsize == nullptr) // safety check
    return false;
  // grow/shrink particle
  int32_t newsize = advprops->size;
  uint32_t counter = advsize->sizecounter;
  uint32_t increment = 0;
  // calculate grow speed using 0-8 for low speeds and 9-15 for higher speeds
  if (advsize->grow) increment = advsize->growspeed;
  else if (advsize->shrink) increment = advsize->shrinkspeed;
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

  if (advsize->grow) {
    if (newsize < advsize->maxsize) {
      newsize += increment;
      if (newsize >= advsize->maxsize) {
        advsize->grow = false; // stop growing, shrink from now on if enabled
        newsize = advsize->maxsize; // limit
        if (advsize->pulsate) advsize->shrink = true;
      }
    }
  } else if (advsize->shrink) {
    if (newsize > advsize->minsize) {
      newsize -= increment;
      if (newsize <= advsize->minsize) {
        if (advsize->minsize == 0) 
          return false; // particle shrunk to zero
        advsize->shrink = false; // disable shrinking
        newsize = advsize->minsize; // limit
        if (advsize->pulsate) advsize->grow = true;
      }
    }
  }
  advprops->size = newsize;
  // handle wobbling
  if (advsize->wobble) {
    advsize->asymdir += advsize->wobblespeed; // note: if need better wobblespeed control a counter is already in the struct
  }
  return true;
}

// calculate x and y size for asymmetrical particles (advanced size control)
void ParticleSystem2D::getParticleXYsize(PSadvancedParticle *advprops, PSsizeControl *advsize, uint32_t &xsize, uint32_t &ysize) {
  if (advsize == nullptr) // if advsize is valid, also advanced properties pointer is valid (handled by updatePSpointers())
    return;
  int32_t size = advprops->size;
  int32_t asymdir = advsize->asymdir;
  int32_t deviation = ((uint32_t)size * (uint32_t)advsize->asymmetry) / 255; // deviation from symmetrical size
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
  if(dv == 0) return;
  for (uint32_t i = 0; i < usedParticles; i++) {
    // Note: not checking if particle is dead is faster as most are usually alive and if few are alive, rendering is fast anyways
    particles[i].vy = limitSpeed((int32_t)particles[i].vy - dv);
  }
}

// apply gravity to single particle using system settings (use this for sources)
// function does not increment gravity counter, if gravity setting is disabled, this cannot be used
void ParticleSystem2D::applyGravity(PSparticle &part) {
  uint32_t counterbkp = gforcecounter; // backup PS gravity counter
  int32_t dv = calcForce_dv(gforce, gforcecounter);
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
void ParticleSystem2D::ParticleSys_render() {
  if(blendingStyle == BLEND_STYLE_FADE && SEGMENT.isInTransition() && lastRender + (strip.getFrameTime() >> 1) > strip.now) // fixes speedup during transitions TODO: find a better solution
    return;
  lastRender = strip.now;
  CRGB baseRGB;
  uint32_t brightness; // particle brightness, fades if dying
  static bool useAdditiveTransfer = false; // use add instead of set for buffer transferring (must persist between calls)
  bool isNonFadeTransition = (pmem->inTransition || pmem->finalTransfer) && blendingStyle != BLEND_STYLE_FADE;
  bool isOverlay = segmentIsOverlay();

  // update global blur (used for blur transitions)
  int32_t motionbluramount = motionBlur;
  int32_t smearamount = smearBlur;
  if(pmem->inTransition == effectID && blendingStyle == BLEND_STYLE_FADE) { // FX transition and this is the new FX: fade blur amount but only if using fade style
    motionbluramount = globalBlur + (((motionbluramount - globalBlur) * (int)SEGMENT.progress()) >> 16); // fade from old blur to new blur during transitions
    smearamount = globalSmear + (((smearamount - globalSmear) * (int)SEGMENT.progress()) >> 16);  
  }
  globalBlur = motionbluramount;
  globalSmear = smearamount;

  if(isOverlay) {
    globalSmear = 0; // do not apply smear or blur in overlay or it turns everything into a blurry mess
    globalBlur = 0;
  }
  // handle blurring and framebuffer update
  if (framebuffer) {
    if(!pmem->inTransition)
      useAdditiveTransfer = false; // additive transfer is only usd in transitions (or in overlay)
    // handle buffer blurring or clearing
    bool bufferNeedsUpdate = !pmem->inTransition || pmem->inTransition == effectID || isNonFadeTransition; // not a transition; or new FX or not fading style: update buffer (blur, or clear)
    if(bufferNeedsUpdate) {
      bool loadfromSegment = !renderSolo || isNonFadeTransition;
      if (globalBlur > 0 || globalSmear > 0) { // blurring active: if not a transition or is newFX, read data from segment before blurring (old FX can render to it afterwards)
        for (int32_t y = 0; y <= maxYpixel; y++) {
          int index = y * (maxXpixel + 1);
          for (int32_t x = 0; x <= maxXpixel; x++) {
            if (loadfromSegment) { // sharing the framebuffer with another segment or not using fade style blending: update buffer by reading back from segment
              framebuffer[index] = SEGMENT.getPixelColorXY(x, y); // read from segment
            }
            fast_color_scale(framebuffer[index], globalBlur); // note: could skip if only globalsmear is active but usually they are both active and scaling is fast enough
            index++;
          }
        }
      }
      else { // no blurring: clear buffer
        memset(framebuffer, 0, frameBufferSize * sizeof(CRGB));
      }
    }
    // handle buffer for global large particle size rendering
    if(particlesize > 1 && pmem->inTransition) { // if particle size is used by FX we need a clean buffer
      if(bufferNeedsUpdate && !globalBlur) { // transfer without adding if buffer was not cleared above (happens if this is the new FX and other FX does not use blurring)
        useAdditiveTransfer = false; // no blurring and big size particle FX is the new FX (rendered first after clearing), can just render normally
      }
      else { // this is the old FX (rendering second) or blurring is active: new FX already rendered to the buffer and blurring was applied above; transfer it to segment and clear it
        transferBuffer(maxXpixel + 1, maxYpixel + 1, isOverlay);
        memset(framebuffer, 0, frameBufferSize * sizeof(CRGB)); // clear the buffer after transfer
        useAdditiveTransfer = true; // additive transfer reads from segment, adds that to the frame-buffer and writes back to segment, after transfer, segment and buffer are identical
      }
    }
  }
  else { // no local buffer available, apply blur to segment
    if (motionBlur > 0)
      SEGMENT.fadeToBlackBy(255 - motionBlur);
    else
      SEGMENT.fill(BLACK); //clear the buffer before rendering next frame
  }

  // go over particles and render them to the buffer
  for (uint32_t i = 0; i < usedParticles; i++) {
    if (particles[i].ttl == 0 || particleFlags[i].outofbounds)
      continue;
    // generate RGB values for particle
    if (fireIntesity) { // fire mode
      brightness = (uint32_t)particles[i].ttl * (3 + (fireIntesity >> 5)) + 20;
      brightness = min(brightness, (uint32_t)255);
      baseRGB = ColorFromPaletteWLED(SEGPALETTE, brightness, 255);
    }
    else {
      brightness = min((particles[i].ttl << 1), (int)255);
      baseRGB = ColorFromPaletteWLED(SEGPALETTE, particles[i].hue, 255);
      if (particles[i].sat < 255) {
        CHSV32 baseHSV;
        rgb2hsv((uint32_t((byte(baseRGB.r) << 16) | (byte(baseRGB.g) << 8) | (byte(baseRGB.b)))), baseHSV); // convert to HSV
        baseHSV.s = particles[i].sat; // set the saturation
        uint32_t tempcolor;
        hsv2rgb(baseHSV, tempcolor); // convert back to RGB
        baseRGB = (CRGB)tempcolor;
      }
    }
    renderParticle(i, brightness, baseRGB, particlesettings.wrapX, particlesettings.wrapY);
  }

  if (particlesize > 1) {
    uint32_t passes = particlesize / 64 + 1; // number of blur passes, four passes max
    uint32_t bluramount = particlesize;
    uint32_t bitshift = 0;
    for (uint32_t i = 0; i < passes; i++) {
      if (i == 2) // for the last two passes, use higher amount of blur (results in a nicer brightness gradient with soft edges)
        bitshift = 1;

      if (framebuffer)
        blur2D(framebuffer, maxXpixel + 1, maxYpixel + 1, bluramount << bitshift, bluramount << bitshift);
      else {
        SEGMENT.blur(bluramount << bitshift, true);
      }
      bluramount -= 64;
    }
  }
  // apply 2D blur to rendered frame
  if(globalSmear > 0) {
    if (framebuffer)
      blur2D(framebuffer, maxXpixel + 1, maxYpixel + 1, globalSmear, globalSmear);
    else
      SEGMENT.blur(globalSmear, true);
  }
  // transfer framebuffer to segment if available
  if (pmem->inTransition != effectID || isNonFadeTransition) // not in transition or is old FX (rendered second) or not fade style
    transferBuffer(maxXpixel + 1, maxYpixel + 1, useAdditiveTransfer | isOverlay);
}

// calculate pixel positions and brightness distribution and render the particle to local buffer or global buffer
void ParticleSystem2D::renderParticle(const uint32_t particleindex, const uint32_t brightness, const CRGB& color, const bool wrapX, const bool wrapY) {
  if(particlesize == 0) { // single pixel rendering
    uint32_t x = particles[particleindex].x >> PS_P_RADIUS_SHIFT;
    uint32_t y = particles[particleindex].y >> PS_P_RADIUS_SHIFT;
    if (x <= (uint32_t)maxXpixel && y <= (uint32_t)maxYpixel) {
      if (framebuffer)
        fast_color_add(framebuffer[x + (maxYpixel - y) * (maxXpixel + 1)], color, brightness);
      else
        SEGMENT.addPixelColorXY(x, maxYpixel - y, color.scale8(brightness), true);
    }
    return;
  }
  int32_t pxlbrightness[4]; // brightness values for the four pixels representing a particle
  int32_t pixco[4][2]; // physical pixel coordinates of the four pixels a particle is rendered to. x,y pairs
  bool pixelvalid[4] = {true, true, true, true}; // is set to false if pixel is out of bounds
  bool advancedrender = false; // rendering for advanced particles
  // check if particle has advanced size properties and buffer is available
  if (advPartProps && advPartProps[particleindex].size > 0) {
      if (renderbuffer) {
        advancedrender = true;
        memset(renderbuffer, 0, 100 * sizeof(CRGB)); // clear the buffer, renderbuffer is 10x10 pixels
      }
      else return; // cannot render without buffers
  }
  // add half a radius as the rendering algorithm always starts at the bottom left, this leaves things positive, so shifts can be used, then shift coordinate by a full pixel (x--/y-- below)
  int32_t xoffset = particles[particleindex].x + PS_P_HALFRADIUS;
  int32_t yoffset = particles[particleindex].y + PS_P_HALFRADIUS;
  int32_t dx = xoffset & (PS_P_RADIUS - 1); // relativ particle position in subpixel space
  int32_t dy = yoffset & (PS_P_RADIUS - 1); // modulo replaced with bitwise AND, as radius is always a power of 2
  int32_t x = (xoffset >> PS_P_RADIUS_SHIFT); // divide by PS_P_RADIUS which is 64, so can bitshift (compiler can not optimize integer)
  int32_t y = (yoffset >> PS_P_RADIUS_SHIFT);

  // set the four raw pixel coordinates, the order is bottom left [0], bottom right[1], top right [2], top left [3]
  pixco[1][0] = pixco[2][0] = x;  // bottom right & top right
  pixco[2][1] = pixco[3][1] = y;  // top right & top left
  x--; // shift by a full pixel here, this is skipped above to not do -1 and then +1
  y--;
  pixco[0][0] = pixco[3][0] = x;      // bottom left & top left
  pixco[0][1] = pixco[1][1] = y;      // bottom left & bottom right

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
    //render particle to a bigger size
    //particle size to pixels: < 64 is 4x4, < 128 is 6x6, < 192 is 8x8, bigger is 10x10
    //first, render the pixel to the center of the renderbuffer, then apply 2D blurring
    fast_color_add(renderbuffer[4 + (4 * 10)], color, pxlbrightness[0]); // order is: bottom left, bottom right, top right, top left
    fast_color_add(renderbuffer[5 + (4 * 10)], color, pxlbrightness[1]);
    fast_color_add(renderbuffer[5 + (5 * 10)], color, pxlbrightness[2]);
    fast_color_add(renderbuffer[4 + (5 * 10)], color, pxlbrightness[3]);
    uint32_t rendersize = 2; // initialize render size, minimum is 4x4 pixels, it is incremented int he loop below to start with 4
    uint32_t offset = 4; // offset to zero coordinate to write/read data in renderbuffer (actually needs to be 3, is decremented in the loop below)
    uint32_t maxsize = advPartProps[particleindex].size;
    uint32_t xsize = maxsize;
    uint32_t ysize = maxsize;
    if (advPartSize) { // use advanced size control
      if (advPartSize[particleindex].asymmetry > 0)
        getParticleXYsize(&advPartProps[particleindex], &advPartSize[particleindex], xsize, ysize);
      maxsize = (xsize > ysize) ? xsize : ysize; // choose the bigger of the two
    }
    maxsize = maxsize/64 + 1; // number of blur passes depends on maxsize, four passes max
    uint32_t bitshift = 0;
    for(uint32_t i = 0; i < maxsize; i++) {
      if (i == 2) //for the last two passes, use higher amount of blur (results in a nicer brightness gradient with soft edges)
        bitshift = 1;
      rendersize += 2;
      offset--;
      blur2D(renderbuffer, rendersize, rendersize, xsize << bitshift, ysize << bitshift, offset, offset, true);
      xsize = xsize > 64 ? xsize - 64 : 0;
      ysize = ysize > 64 ? ysize - 64 : 0;
    }

    // calculate origin coordinates to render the particle to in the framebuffer
    uint32_t xfb_orig = x - (rendersize>>1) + 1 - offset;
    uint32_t yfb_orig = y - (rendersize>>1) + 1 - offset;
    uint32_t xfb, yfb; // coordinates in frame buffer to write to note: by making this uint, only overflow has to be checked (spits a warning though)

    //note on y-axis flip: WLED has the y-axis defined from top to bottom, so y coordinates must be flipped. doing this in the buffer xfer clashes with 1D/2D combined rendering, which does not invert y
    //                     transferring the 1D buffer in inverted fashion will flip the x-axis of overlaid 2D FX, so the y-axis flip is done here so the buffer is flipped in y, giving correct results

    // transfer particle renderbuffer to framebuffer
    for (uint32_t xrb = offset; xrb < rendersize + offset; xrb++) {
      xfb = xfb_orig + xrb;
      if (xfb > (uint32_t)maxXpixel) {
      if (wrapX) { // wrap x to the other side if required
        if (xfb > (uint32_t)maxXpixel << 1) // xfb is "negative", handle it
          xfb = (maxXpixel + 1) + (int32_t)xfb; // this always overflows to within bounds
        else
          xfb = xfb % (maxXpixel + 1); // note: without the above "negative" check, this works only for powers of 2
      }
      else
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
          }
          else
          continue;
        }
        if (framebuffer)
          fast_color_add(framebuffer[xfb + (maxYpixel - yfb) * (maxXpixel + 1)], renderbuffer[xrb + yrb * 10]);
        else
          SEGMENT.addPixelColorXY(xfb, maxYpixel - yfb, renderbuffer[xrb + yrb * 10],true);
      }
    }
    } else { // standard rendering (2x2 pixels)
    // check for out of frame pixels and wrap them if required: x,y is bottom left pixel coordinate of the particle
    if (x < 0) { // left pixels out of frame
      if (wrapX) { // wrap x to the other side if required
        pixco[0][0] = pixco[3][0] = maxXpixel;
      } else {
        pixelvalid[0] = pixelvalid[3] = false; // out of bounds
      }
    }
    else if (pixco[1][0] > (int32_t)maxXpixel) { // right pixels, only has to be checked if left pixel is in frame
      if (wrapX) { // wrap y to the other side if required
        pixco[1][0] = pixco[2][0] = 0;
      } else {
        pixelvalid[1] = pixelvalid[2] = false; // out of bounds
      }
    }

    if (y < 0) { // bottom pixels out of frame
      if (wrapY) { // wrap y to the other side if required
        pixco[0][1] = pixco[1][1] = maxYpixel;
      } else {
        pixelvalid[0] = pixelvalid[1] = false; // out of bounds
      }
    }
    else if (pixco[2][1] > maxYpixel) { // top pixels
      if (wrapY) { // wrap y to the other side if required
        pixco[2][1] = pixco[3][1] = 0;
      } else {
        pixelvalid[2] = pixelvalid[3] = false; // out of bounds
      }
    }
    if (framebuffer) {
      for (uint32_t i = 0; i < 4; i++) {
        if (pixelvalid[i])
          fast_color_add(framebuffer[pixco[i][0] + (maxYpixel - pixco[i][1]) * (maxXpixel + 1)], color, pxlbrightness[i]); // order is: bottom left, bottom right, top right, top left
      }
    }
    else {
      for (uint32_t i = 0; i < 4; i++) {
      if (pixelvalid[i])
        SEGMENT.addPixelColorXY(pixco[i][0], maxYpixel - pixco[i][1], color.scale8((uint8_t)pxlbrightness[i]), true);
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
      if (particles[pidx].ttl > 0 && particleFlags[pidx].outofbounds == 0 && particleFlags[pidx].collide) { // colliding particle
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
        if (advPartProps) { //may be using individual particle size
          setParticleSize(particlesize); // updates base particleHardRadius
          collDistSq = (particleHardRadius << 1) + (((uint32_t)advPartProps[idx_i].size + (uint32_t)advPartProps[idx_j].size) >> 1); // collision distance note: not 100% clear why the >> 1 is needed, but it is.
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

    if (collisionHardness < PS_P_MINSURFACEHARDNESS && (SEGMENT.call & 0x07) == 0) { // if particles are soft, they become 'sticky' i.e. apply some friction (they do pile more nicely and stop sloshing around)
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
    if(distanceSquared < collDistSq && dotProduct > -250) { // too close and also slow, push them apart
      int32_t notsorandom = dotProduct & 0x01; //dotprouct LSB should be somewhat random, so no need to calculate a random number
      int32_t pushamount = 1 + ((250 + dotProduct) >> 6); // the closer dotproduct is to zero, the closer the particles are
      int32_t push = 0;
      if (dx < 0)  // particle 1 is on the right
        push = pushamount;
      else if (dx > 0)
        push = -pushamount;
      else { // on the same x coordinate, shift it a little so they do not stack
        if (notsorandom)
          particle1.x++; // move it so pile collapses
        else
          particle1.x--;
      }
      particle1.vx += push;
      push = 0;
      if (dy < 0)
        push = pushamount;
      else if (dy > 0)
        push = -pushamount;
      else { // dy==0
        if (notsorandom)
          particle1.y++; // move it so pile collapses
        else
          particle1.y--;
      }
      particle1.vy += push;

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
// note: do not access the PS class in FX befor running this function (or it messes up SEGENV.data)
void ParticleSystem2D::updateSystem(void) {
  PSPRINTLN("updateSystem2D");
  setMatrixSize(SEGMENT.vWidth(), SEGMENT.vHeight());
  updateRenderingBuffer(SEGMENT.vWidth() * SEGMENT.vHeight(), true, false); // update rendering buffer (segment size can change at any time)
  updatePSpointers(advPartProps != nullptr, advPartSize != nullptr); // update pointers to PS data, also updates availableParticles
  setUsedParticles(fractionOfParticlesUsed); // update used particles based on percentage (can change during transitions, execute each frame for code simplicity)
  if (partMemList.size() == 1) // if number of vector elements is one, this is the only system
    renderSolo = true;
  else
    renderSolo = false;
  PSPRINTLN("\n END update System2D, running FX...");
}

// set the pointers for the class (this only has to be done once and not on every FX call, only the class pointer needs to be reassigned to SEGENV.data every time)
// function returns the pointer to the next byte available for the FX (if it assigned more memory for other stuff using the above allocate function)
// FX handles the PSsources, need to tell this function how many there are
void ParticleSystem2D::updatePSpointers(bool isadvanced, bool sizecontrol) {
  PSPRINTLN("updatePSpointers");
  // DEBUG_PRINT(F("*** PS pointers ***"));
  // DEBUG_PRINTF_P(PSTR("this PS %p "), this);
  // Note on memory alignment:
  // a pointer MUST be 4 byte aligned. sizeof() in a struct/class is always aligned to the largest element. if it contains a 32bit, it will be padded to 4 bytes, 16bit is padded to 2byte alignment.
  // The PS is aligned to 4 bytes, a PSparticle is aligned to 2 and a struct containing only byte sized variables is not aligned at all and may need to be padded when dividing the memoryblock.
  // by making sure that the number of sources and particles is a multiple of 4, padding can be skipped here as alignent is ensured, independent of struct sizes.

  // memory manager needs to know how many particles the FX wants to use so transitions can be handled properly (i.e. pointer will stop changing if enough particles are available during transitions)
  uint32_t usedByFX = (numParticles * ((uint32_t)fractionOfParticlesUsed + 1)) >> 8; // final number of particles the FX wants to use (fractionOfParticlesUsed is 0-255)
  particles = reinterpret_cast<PSparticle *>(particleMemoryManager(0, sizeof(PSparticle), availableParticles, usedByFX, effectID)); // get memory, leave buffer size as is (request 0)
  particleFlags = reinterpret_cast<PSparticleFlags *>(this + 1); // pointer to particle flags
  sources = reinterpret_cast<PSsource *>(particleFlags + numParticles); // pointer to source(s) at data+sizeof(ParticleSystem2D)
  PSdataEnd = reinterpret_cast<uint8_t *>(sources + numSources); // pointer to first available byte after the PS for FX additional data
  if (isadvanced) {
    advPartProps = reinterpret_cast<PSadvancedParticle *>(sources + numSources);
    PSdataEnd = reinterpret_cast<uint8_t *>(advPartProps + numParticles);
    if (sizecontrol) {
      advPartSize = reinterpret_cast<PSsizeControl *>(advPartProps + numParticles);
      PSdataEnd = reinterpret_cast<uint8_t *>(advPartSize + numParticles);
    }
  }
#ifdef DEBUG_PS
  Serial.printf_P(PSTR(" particles %p "), particles);
  Serial.printf_P(PSTR(" sources %p "), sources);
  Serial.printf_P(PSTR(" adv. props %p "), advPartProps);
  Serial.printf_P(PSTR(" adv. ctrl %p "), advPartSize);
  Serial.printf_P(PSTR("end %p\n"), PSdataEnd);
  #endif

}

// blur a matrix in x and y direction, blur can be asymmetric in x and y
// for speed, 1D array and 32bit variables are used, make sure to limit them to 8bit (0-255) or result is undefined
// to blur a subset of the buffer, change the xsize/ysize and set xstart/ystart to the desired starting coordinates (default start is 0/0)
// subset blurring only works on 10x10 buffer (single particle rendering), if other sizes are needed, buffer width must be passed as parameter
void blur2D(CRGB *colorbuffer, uint32_t xsize, uint32_t ysize, uint32_t xblur, uint32_t yblur, uint32_t xstart, uint32_t ystart, bool isparticle) {
  CRGB seeppart, carryover;
  uint32_t seep = xblur >> 1;
  uint32_t width = xsize; // width of the buffer, used to calculate the index of the pixel

  if (isparticle) { //first and last row are always black in first pass of particle rendering
    ystart++;
    ysize--;
    width = 10; // buffer size is 10x10
  }

  for(uint32_t y = ystart; y < ystart + ysize; y++) {
    carryover =  BLACK;
    uint32_t indexXY = xstart + y * width;
    for(uint32_t x = xstart; x < xstart + xsize; x++) {
      seeppart = colorbuffer[indexXY]; // create copy of current color
      fast_color_scale(seeppart, seep); // scale it and seep to neighbours
      if (x > 0) {
        fast_color_add(colorbuffer[indexXY - 1], seeppart);
        if(carryover) // note: check adds overhead but is faster on average
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
  for(uint32_t x = xstart; x < xstart + xsize; x++) {
    carryover = BLACK;
    uint32_t indexXY = x + ystart * width;
    for(uint32_t y = ystart; y < ystart + ysize; y++) {
      seeppart = colorbuffer[indexXY]; // create copy of current color
      fast_color_scale(seeppart, seep); // scale it and seep to neighbours
      if (y > 0) {
        fast_color_add(colorbuffer[indexXY - width], seeppart);
        if(carryover) // note: check adds overhead but is faster on average
          fast_color_add(colorbuffer[indexXY], carryover);
      }
      carryover = seeppart;
      indexXY += width; // next pixel in y direction
    }
  }
}

//non class functions to use for initialization
uint32_t calculateNumberOfParticles2D(uint32_t const pixels, const bool isadvanced, const bool sizecontrol) {
  uint32_t numberofParticles = pixels;  // 1 particle per pixel (for example 512 particles on 32x16)
#ifdef ESP8266
  uint32_t particlelimit = ESP8266_MAXPARTICLES; // maximum number of paticles allowed (based on one segment of 16x16 and 4k effect ram)
#elif ARDUINO_ARCH_ESP32S2
  uint32_t particlelimit = ESP32S2_MAXPARTICLES; // maximum number of paticles allowed (based on one segment of 32x32 and 24k effect ram)
#else
  uint32_t particlelimit = ESP32_MAXPARTICLES; // maximum number of paticles allowed (based on two segments of 32x32 and 40k effect ram)
#endif
  numberofParticles = max((uint32_t)4, min(numberofParticles, particlelimit)); // limit to 4 - particlelimit
  if (isadvanced) // advanced property array needs ram, reduce number of particles to use the same amount
    numberofParticles = (numberofParticles * sizeof(PSparticle)) / (sizeof(PSparticle) + sizeof(PSadvancedParticle));
  if (sizecontrol) // advanced property array needs ram, reduce number of particles
    numberofParticles /= 8; // if advanced size control is used, much fewer particles are needed note: if changing this number, adjust FX using this accordingly

  //make sure it is a multiple of 4 for proper memory alignment (easier than using padding bytes)
  numberofParticles = ((numberofParticles+3) >> 2) << 2; // note: with a separate particle buffer, this is probably unnecessary
  return numberofParticles;
}

uint32_t calculateNumberOfSources2D(uint32_t pixels, uint32_t requestedsources) {
#ifdef ESP8266
  int numberofSources = min((pixels) / 8, (uint32_t)requestedsources);
  numberofSources = max(1, min(numberofSources, ESP8266_MAXSOURCES)); // limit to 1 - 16
#elif ARDUINO_ARCH_ESP32S2
  int numberofSources = min((pixels) / 6, (uint32_t)requestedsources);
  numberofSources = max(1, min(numberofSources, ESP32S2_MAXSOURCES)); // limit to 1 - 48
#else
  int numberofSources = min((pixels) / 4, (uint32_t)requestedsources);
  numberofSources = max(1, min(numberofSources, ESP32_MAXSOURCES)); // limit to 1 - 64
#endif
  // make sure it is a multiple of 4 for proper memory alignment
  numberofSources = ((numberofSources+3) >> 2) << 2;
  return numberofSources;
}

//allocate memory for particle system class, particles, sprays plus additional memory requested by FX //TODO: add percentofparticles like in 1D to reduce memory footprint of some FX?
bool allocateParticleSystemMemory2D(uint32_t numparticles, uint32_t numsources, bool isadvanced, bool sizecontrol, uint32_t additionalbytes) {
  PSPRINTLN("PS 2D alloc");
  uint32_t requiredmemory = sizeof(ParticleSystem2D);
  uint32_t dummy; // dummy variable
  if((particleMemoryManager(numparticles, sizeof(PSparticle), dummy, dummy, SEGMENT.mode)) == nullptr) // allocate memory for particles
    return false; // not enough memory, function ensures a minimum of numparticles are available

  // functions above make sure these are a multiple of 4 bytes (to avoid alignment issues)
  requiredmemory += sizeof(PSparticleFlags) * numparticles;
  if (isadvanced)
    requiredmemory += sizeof(PSadvancedParticle) * numparticles;
  if (sizecontrol)
    requiredmemory += sizeof(PSsizeControl) * numparticles;
  requiredmemory += sizeof(PSsource) * numsources;
  requiredmemory += additionalbytes;
  PSPRINTLN("mem alloc: " + String(requiredmemory));
  return(SEGMENT.allocateData(requiredmemory));
}

// initialize Particle System, allocate additional bytes if needed (pointer to those bytes can be read from particle system class: PSdataEnd)
bool initParticleSystem2D(ParticleSystem2D *&PartSys, uint32_t requestedsources, uint32_t additionalbytes, bool advanced, bool sizecontrol) {
  PSPRINT("PS 2D init ");
  if(!strip.isMatrix) return false; // only for 2D
  uint32_t cols = SEGMENT.virtualWidth();
  uint32_t rows = SEGMENT.virtualHeight();
  uint32_t pixels = cols * rows;

  if(advanced)
    updateRenderingBuffer(100, false, true); // allocate a 10x10 buffer for rendering advanced particles
  uint32_t numparticles = calculateNumberOfParticles2D(pixels, advanced, sizecontrol);
  PSPRINT(" segmentsize:" + String(cols) + " " + String(rows));
  PSPRINT(" request numparticles:" + String(numparticles));
  uint32_t numsources = calculateNumberOfSources2D(pixels, requestedsources);
  if (!allocateParticleSystemMemory2D(numparticles, numsources, advanced, sizecontrol, additionalbytes))
  {
    DEBUG_PRINT(F("PS init failed: memory depleted"));
    return false;
  }

  PartSys = new (SEGENV.data) ParticleSystem2D(cols, rows, numparticles, numsources, advanced, sizecontrol); // particle system constructor
  updateRenderingBuffer(SEGMENT.vWidth() * SEGMENT.vHeight(), true, true); // update or create rendering buffer note: for fragmentation it might be better to allocate this first, but if memory is scarce, system has a buffer but no particles and will return false
  
  PSPRINTLN("******init done, pointers:");
  #ifdef WLED_DEBUG_PS
  PSPRINT("framebfr size:");
  PSPRINT(frameBufferSize);
  PSPRINT(" @ addr: 0x");
  Serial.println((uintptr_t)framebuffer, HEX);
  PSPRINT("renderbfr size:");
  PSPRINT(renderBufferSize);
  PSPRINT(" @ addr: 0x");
  Serial.println((uintptr_t)renderbuffer, HEX);
  #endif
  return true;
}

#endif // WLED_DISABLE_PARTICLESYSTEM2D


////////////////////////
// 1D Particle System //
////////////////////////
#ifndef WLED_DISABLE_PARTICLESYSTEM1D

ParticleSystem1D::ParticleSystem1D(uint32_t length, uint32_t numberofparticles, uint32_t numberofsources, bool isadvanced) {
  effectID = SEGMENT.mode;
  numSources = numberofsources;
  numParticles = numberofparticles; // number of particles allocated in init
  availableParticles = 0; // let the memory manager assign
  fractionOfParticlesUsed = 255; // use all particles by default
  advPartProps = nullptr; //make sure we start out with null pointers (just in case memory was not cleared)
  //advPartSize = nullptr;
  updatePSpointers(isadvanced); // set the particle and sources pointer (call this before accessing sprays or particles)  
  setSize(length);
  setWallHardness(255); // set default wall hardness to max
  setGravity(0); //gravity disabled by default
  setParticleSize(0); // 1 pixel size by default
  motionBlur = 0; //no fading by default
  smearBlur = 0; //no smearing by default
  emitIndex = 0;
  collisionStartIdx = 0;
  lastRender = 0;
  // initialize some default non-zero values most FX use
  for (uint32_t i = 0; i < numSources; i++) {
    sources[i].source.ttl = 1; //set source alive
  }

  if(isadvanced) {
    for (uint32_t i = 0; i < numParticles; i++) {
      advPartProps[i].sat = 255; // set full saturation (for particles that are transferred from non-advanced system)
    }
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
    particleMoveUpdate(particles[i], particleFlags[i], nullptr, advPartProps ? &advPartProps[i] : nullptr);
  }

  if (particlesettings.colorByPosition) {
    uint32_t scale = (255 << 16) / maxX;  // speed improvement: multiplication is faster than division
    for (uint32_t i = 0; i < usedParticles; i++) {
      particles[i].hue = (scale * particles[i].x) >> 16; // note: x is > 0 if not out of bounds
    }
  }

  ParticleSys_render();
}

// set percentage of used particles as uint8_t i.e 127 means 50% for example
void ParticleSystem1D::setUsedParticles(const uint8_t percentage) {
  fractionOfParticlesUsed = percentage; // note usedParticles is updated in memory manager
  updateUsedParticles(numParticles, availableParticles, fractionOfParticlesUsed, usedParticles);
  PSPRINT(" SetUsedpaticles: allocated particles: ");
  PSPRINT(numParticles);
  PSPRINT(" available particles: ");
  PSPRINT(availableParticles);
  PSPRINT(" ,used percentage: ");
  PSPRINT(fractionOfParticlesUsed);
  PSPRINT(" ,used particles: ");
  PSPRINTLN(usedParticles);
}

void ParticleSystem1D::setWallHardness(const uint8_t hardness) {
  wallHardness = hardness;
}

void ParticleSystem1D::setSize(const uint32_t x) {
  maxXpixel = x - 1; // last physical pixel that can be drawn to
  maxX = x * PS_P_RADIUS_1D - 1;  // particle system boundary for movements
}

void ParticleSystem1D::setWrap(const bool enable) {
  particlesettings.wrap = enable;
}

void ParticleSystem1D::setBounce(const bool enable) {
  particlesettings.bounce = enable;
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
      particleFlags[emitIndex].collide = emitter.sourceFlags.collide;
      particleFlags[emitIndex].reversegrav = emitter.sourceFlags.reversegrav;
      particleFlags[emitIndex].perpetual = emitter.sourceFlags.perpetual;
      if (advPartProps) {
        advPartProps[emitIndex].sat = emitter.sat;
        advPartProps[emitIndex].size = emitter.size;
      }
      return emitIndex;
    }
  }
  return -1;
}

// particle moves, decays and dies, if killoutofbounds is set, out of bounds particles are set to ttl=0
// uses passed settings to set bounce or wrap, if useGravity is set, it will never bounce at the top and killoutofbounds is not applied over the top
void ParticleSystem1D::particleMoveUpdate(PSparticle1D &part, PSparticleFlags1D &partFlags, PSsettings1D *options, PSadvancedParticle1D *advancedproperties) {
  if (options == nullptr)
    options = &particlesettings; // use PS system settings by default

  if (part.ttl > 0) {
    if (!partFlags.perpetual)
      part.ttl--; // age
    if (options->colorByAge)
      part.hue = min(part.ttl, (uint16_t)255); // set color to ttl

    int32_t renderradius = PS_P_HALFRADIUS_1D; // used to check out of bounds, default for 2 pixel rendering
    int32_t newX = part.x + (int32_t)part.vx;
    partFlags.outofbounds = false; // reset out of bounds (in case particle was created outside the matrix and is now moving into view)

    if (advancedproperties) { // using individual particle size?
      if (advancedproperties->size > 1)
        particleHardRadius = PS_P_MINHARDRADIUS_1D + (advancedproperties->size >> 1);
      else // single pixel particles use half the collision distance for walls
        particleHardRadius = PS_P_MINHARDRADIUS_1D >> 1;
      renderradius = particleHardRadius; // note: for single pixel particles, it should be zero, but it does not matter as out of bounds checking is done in rendering function
    }

    // if wall collisions are enabled, bounce them before they reach the edge, it looks much nicer if the particle is not half out of view
    if (options->bounce) {
      if ((newX < (int32_t)particleHardRadius) || ((newX > (int32_t)(maxX - particleHardRadius)))) { // reached a wall
        bool bouncethis = true;
        if (options->useGravity) {
          if (partFlags.reversegrav) { // skip bouncing at x = 0
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

    if (!checkBoundsAndWrap(newX, maxX, renderradius, options->wrap)) { // check out of bounds note: this must not be skipped or it can lead to crashes
      partFlags.outofbounds = true;
      if (options->killoutofbounds) {
        bool killthis = true;
        if (options->useGravity) { // if gravity is used, only kill below 'floor level'
          if (partFlags.reversegrav) { // skip at x = 0, do not skip far out of bounds
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

    if (!partFlags.fixed)
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
    if (particleFlags[i].reversegrav) dv = -dv_raw;
    // note: not checking if particle is dead is omitted as most are usually alive and if few are alive, rendering is fast anyways
    particles[i].vx = limitSpeed((int32_t)particles[i].vx - dv);
  }
}

// apply gravity to single particle using system settings (use this for sources)
// function does not increment gravity counter, if gravity setting is disabled, this cannot be used
void ParticleSystem1D::applyGravity(PSparticle1D &part, PSparticleFlags1D &partFlags) {
  uint32_t counterbkp = gforcecounter;
  int32_t dv = calcForce_dv(gforce, gforcecounter);
  if (partFlags.reversegrav) dv = -dv;
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
void ParticleSystem1D::ParticleSys_render() {
  if(blendingStyle == BLEND_STYLE_FADE && SEGMENT.isInTransition() && lastRender + (strip.getFrameTime() >> 1) > strip.now) // fixes speedup during transitions TODO: find a better solution
    return;
  lastRender = strip.now;
  CRGB baseRGB;
  uint32_t brightness; // particle brightness, fades if dying
 // bool useAdditiveTransfer; // use add instead of set for buffer transferring
  bool isNonFadeTransition = (pmem->inTransition || pmem->finalTransfer) && blendingStyle != BLEND_STYLE_FADE;
  bool isOverlay = segmentIsOverlay();

  // update global blur (used for blur transitions)
  int32_t motionbluramount = motionBlur;
  int32_t smearamount = smearBlur;
  if(pmem->inTransition == effectID) { // FX transition and this is the new FX: fade blur amount
    motionbluramount = globalBlur + (((motionbluramount - globalBlur) * (int)SEGMENT.progress()) >> 16); // fade from old blur to new blur during transitions
    smearamount = globalSmear + (((smearamount - globalSmear) * (int)SEGMENT.progress()) >> 16);
  }
  globalBlur = motionbluramount;
  globalSmear = smearamount;

  if (framebuffer) {
    // handle buffer blurring or clearing
    bool bufferNeedsUpdate = !pmem->inTransition || pmem->inTransition == effectID || isNonFadeTransition; // not a transition; or new FX: update buffer (blur, or clear)
    if(bufferNeedsUpdate) {
      bool loadfromSegment = !renderSolo || isNonFadeTransition;
      if (globalBlur > 0 || globalSmear > 0) { // blurring active: if not a transition or is newFX, read data from segment before blurring (old FX can render to it afterwards)
        for (int32_t x = 0; x <= maxXpixel; x++) {
          if (loadfromSegment) // sharing the framebuffer with another segment: read buffer back from segment
            framebuffer[x] = SEGMENT.getPixelColor(x); // copy to local buffer
          fast_color_scale(framebuffer[x], motionBlur);
        }
      }
      else { // no blurring: clear buffer
        memset(framebuffer, 0, frameBufferSize * sizeof(CRGB));
      }
    }
  }
  else { // no local buffer available
    if (motionBlur > 0)
      SEGMENT.fadeToBlackBy(255 - motionBlur);
    else
      SEGMENT.fill(BLACK); // clear the buffer before rendering to it
  }

  // go over particles and render them to the buffer
  for (uint32_t i = 0; i < usedParticles; i++) {
    if ( particles[i].ttl == 0 || particleFlags[i].outofbounds)
      continue;

    // generate RGB values for particle
    brightness = min(particles[i].ttl << 1, (int)255);
    baseRGB = ColorFromPaletteWLED(SEGPALETTE, particles[i].hue, 255);

    if (advPartProps) { //saturation is advanced property in 1D system
      if (advPartProps[i].sat < 255) {
        CHSV32 baseHSV;
        rgb2hsv((uint32_t((byte(baseRGB.r) << 16) | (byte(baseRGB.g) << 8) | (byte(baseRGB.b)))), baseHSV); // convert to HSV
        baseHSV.s = advPartProps[i].sat; // set the saturation
        uint32_t tempcolor;
        hsv2rgb(baseHSV, tempcolor); // convert back to RGB
        baseRGB = (CRGB)tempcolor;
      }
    }
    renderParticle(i, brightness, baseRGB, particlesettings.wrap);
  }
  // apply smear-blur to rendered frame
  if(globalSmear > 0) {
    if (framebuffer)
      blur1D(framebuffer, maxXpixel + 1, globalSmear, 0);
    else
      SEGMENT.blur(globalSmear, true);
  }

  // add background color
  uint32_t bg_color = SEGCOLOR(1);
  if (bg_color > 0) { //if not black
    for(int32_t i = 0; i <= maxXpixel; i++) {
      if (framebuffer)
        fast_color_add(framebuffer[i], bg_color);
      else
        SEGMENT.addPixelColor(i, bg_color, true);
    }
  }
  // transfer local buffer back to segment (if available)
  if (pmem->inTransition != effectID || isNonFadeTransition)
    transferBuffer(maxXpixel + 1, 0, isOverlay);
}

// calculate pixel positions and brightness distribution and render the particle to local buffer or global buffer
void ParticleSystem1D::renderParticle(const uint32_t particleindex, const uint32_t brightness, const CRGB &color, const bool wrap) {
  uint32_t size = particlesize;
  if (advPartProps) {// use advanced size properties
    size = advPartProps[particleindex].size;
  }
  if (size == 0) { //single pixel particle, can be out of bounds as oob checking is made for 2-pixel particles (and updating it uses more code)
    uint32_t x =  particles[particleindex].x >> PS_P_RADIUS_SHIFT_1D;
    if (x <= (uint32_t)maxXpixel) { //by making x unsigned there is no need to check < 0 as it will overflow
      if (framebuffer)
        fast_color_add(framebuffer[x], color, brightness);
      else
        SEGMENT.addPixelColor(x, color.scale8((uint8_t)brightness), true);
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

  // check if particle has advanced size properties and buffer is available
  if (advPartProps && advPartProps[particleindex].size > 1) {
    if (renderbuffer) {
      memset(renderbuffer, 0, 10 * sizeof(CRGB)); // clear the buffer, renderbuffer is 10 pixels
    }
    else
      return; // cannot render advanced particles without buffer

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

    // calculate origin coordinates to render the particle to in the framebuffer
    uint32_t xfb_orig = x - (rendersize>>1) + 1 - offset; //note: using uint is fine
    uint32_t xfb; // coordinates in frame buffer to write to note: by making this uint, only overflow has to be checked

    // transfer particle renderbuffer to framebuffer
    for (uint32_t xrb = offset; xrb < rendersize+offset; xrb++) {
      xfb = xfb_orig + xrb;
      if (xfb > (uint32_t)maxXpixel) {
        if (wrap) { // wrap x to the other side if required
          if (xfb > (uint32_t)maxXpixel << 1) // xfb is "negative"
            xfb = (maxXpixel + 1) + (int32_t)xfb; // this always overflows to within bounds
          else
            xfb = xfb % (maxXpixel + 1); // note: without the above "negative" check, this works only for powers of 2
        }
        else
          continue;
      }
      if (framebuffer)
        fast_color_add(framebuffer[xfb], renderbuffer[xrb]);
      else
      SEGMENT.addPixelColor(xfb, renderbuffer[xrb]);
    }
  }
  else { // standard rendering (2 pixels per particle)
    // check if any pixels are out of frame
    if (x < 0) { // left pixels out of frame
      if (wrap) // wrap x to the other side if required
        pixco[0] = maxXpixel;
      else
        pxlisinframe[0] = false; // pixel is out of matrix boundaries, do not render
    }
    else if (pixco[1] > (int32_t)maxXpixel) { // right pixel, only has to be checkt if left pixel did not overflow
      if (wrap) // wrap y to the other side if required
        pixco[1] = 0;
      else
        pxlisinframe[1] = false;
    }
    for(uint32_t i = 0; i < 2; i++) {
      if (pxlisinframe[i]) {
        if (framebuffer)
          fast_color_add(framebuffer[pixco[i]], color, pxlbrightness[i]);
        else
            SEGMENT.addPixelColor(pixco[i], color.scale8((uint8_t)pxlbrightness[i]), true);
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
  if (advPartProps) //may be using individual particle size
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
      if (particles[pidx].ttl > 0 && particleFlags[pidx].outofbounds == 0 && particleFlags[pidx].collide) { // colliding particle
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
        if (advPartProps) { // use advanced size properties
          collisiondistance = (PS_P_MINHARDRADIUS_1D << particlesize) + (((uint32_t)advPartProps[idx_i].size + (uint32_t)advPartProps[idx_j].size) >> 1);
        }
        int32_t dx = particles[idx_j].x - particles[idx_i].x;
        int32_t dv = (int32_t)particles[idx_j].vx - (int32_t)particles[idx_i].vx;
        int32_t proximity = collisiondistance;
        if (dv >= proximity) // particles would go past each other in next move update
          proximity += abs(dv); // add speed difference to catch fast particles
        if (dx <= proximity && dx >= -proximity) { // collide if close
          collideParticles(particles[idx_i], particleFlags[idx_i], particles[idx_j], particleFlags[idx_j], dx, dv, collisiondistance);
        }
      }
    }
  }
  collisionStartIdx = nextFrameStartIdx; // set the start index for the next frame
}
// handle a collision if close proximity is detected, i.e. dx and/or dy smaller than 2*PS_P_RADIUS
// takes two pointers to the particles to collide and the particle hardness (softer means more energy lost in collision, 255 means full hard)
void ParticleSystem1D::collideParticles(PSparticle1D &particle1, const PSparticleFlags1D &particle1flags, PSparticle1D &particle2, const PSparticleFlags1D &particle2flags, int32_t dx, int32_t relativeVx, const int32_t collisiondistance) {
  int32_t dotProduct = (dx * relativeVx); // is always negative if moving towards each other
  uint32_t distance = abs(dx);
  if (dotProduct < 0) { // particles are moving towards each other
    uint32_t surfacehardness = max(collisionHardness, (int32_t)PS_P_MINSURFACEHARDNESS_1D); // if particles are soft, the impulse must stay above a limit or collisions slip through
    // Calculate new velocities after collision
    int32_t impulse = relativeVx * surfacehardness / 255; // note: not using dot product like in 2D as impulse is purely speed depnedent
    particle1.vx += impulse;
    particle2.vx -= impulse;

    // if one of the particles is fixed, transfer the impulse back so it bounces
    if (particle1flags.fixed)
      particle2.vx = -particle1.vx;
    else if (particle2flags.fixed)
      particle1.vx = -particle2.vx;

    if (collisionHardness < PS_P_MINSURFACEHARDNESS_1D && (SEGMENT.call & 0x07) == 0) { // if particles are soft, they become 'sticky' i.e. apply some friction
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
        if (dx < 0 && !particle1flags.fixed) {  // particle2.x < particle1.x  -> push particle 1
          particle1.vx++;// += pushamount;
          particle1.x += pushamount;
        }
        else if (!particle2flags.fixed) { // particle1.x < particle2.x  -> push particle 2
          particle2.vx++;// += pushamount;
          particle2.x += pushamount;
        }
      }
      else { // upper half, push particle with smaller x
        if (dx < 0 && !particle2flags.fixed) {  // particle2.x < particle1.x  -> push particle 2
          particle2.vx--;// -= pushamount;
          particle2.x -= pushamount;
        }
        else if (!particle2flags.fixed) { // particle1.x < particle2.x  -> push particle 1
          particle1.vx--;// -= pushamount;
          particle1.x -= pushamount;
        }
      }
    }
  }
}

// update size and pointers (memory location and size can change dynamically)
// note: do not access the PS class in FX befor running this function (or it messes up SEGENV.data)
void ParticleSystem1D::updateSystem(void) {
  setSize(SEGMENT.vLength()); // update size
  updateRenderingBuffer(SEGMENT.vLength(), true, false); // update rendering buffer (segment size can change at any time)
  updatePSpointers(advPartProps != nullptr);
  setUsedParticles(fractionOfParticlesUsed); // update used particles based on percentage (can change during transitions, execute each frame for code simplicity)
  if (partMemList.size() == 1) // if number of vector elements is one, this is the only system
    renderSolo = true;
  else
    renderSolo = false;
}

// set the pointers for the class (this only has to be done once and not on every FX call, only the class pointer needs to be reassigned to SEGENV.data every time)
// function returns the pointer to the next byte available for the FX (if it assigned more memory for other stuff using the above allocate function)
// FX handles the PSsources, need to tell this function how many there are
void ParticleSystem1D::updatePSpointers(bool isadvanced) {
  // Note on memory alignment:
  // a pointer MUST be 4 byte aligned. sizeof() in a struct/class is always aligned to the largest element. if it contains a 32bit, it will be padded to 4 bytes, 16bit is padded to 2byte alignment.
  // The PS is aligned to 4 bytes, a PSparticle is aligned to 2 and a struct containing only byte sized variables is not aligned at all and may need to be padded when dividing the memoryblock.
  // by making sure that the number of sources and particles is a multiple of 4, padding can be skipped here as alignent is ensured, independent of struct sizes.

  // memory manager needs to know how many particles the FX wants to use so transitions can be handled properly (i.e. pointer will stop changing if enough particles are available during transitions)
  uint32_t usedByFX = (numParticles * ((uint32_t)fractionOfParticlesUsed + 1)) >> 8; // final number of particles the FX wants to use (fractionOfParticlesUsed is 0-255)
  particles = reinterpret_cast<PSparticle1D *>(particleMemoryManager(0, sizeof(PSparticle1D), availableParticles, usedByFX, effectID)); // get memory, leave buffer size as is (request 0)
  particleFlags = reinterpret_cast<PSparticleFlags1D *>(this + 1); // pointer to particle flags
  sources = reinterpret_cast<PSsource1D *>(particleFlags + numParticles); // pointer to source(s)
  PSdataEnd = reinterpret_cast<uint8_t *>(sources + numSources); // pointer to first available byte after the PS for FX additional data
  if (isadvanced) {
    advPartProps = reinterpret_cast<PSadvancedParticle1D *>(sources + numSources);
    PSdataEnd = reinterpret_cast<uint8_t *>(advPartProps + numParticles);
  }
  #ifdef WLED_DEBUG_PS
  PSPRINTLN(" PS Pointers: ");
  PSPRINT(" PS : 0x");
  Serial.println((uintptr_t)this, HEX);
  PSPRINT(" Sources : 0x");
  Serial.println((uintptr_t)sources, HEX);
  PSPRINT(" Particles : 0x");
  Serial.println((uintptr_t)particles, HEX);
  #endif
}

//non class functions to use for initialization, fraction is uint8_t: 255 means 100%
uint32_t calculateNumberOfParticles1D(const uint32_t fraction, const bool isadvanced) {
  uint32_t numberofParticles = SEGMENT.virtualLength();  // one particle per pixel (if possible)
#ifdef ESP8266
  uint32_t particlelimit = ESP8266_MAXPARTICLES_1D; // maximum number of paticles allowed
#elif ARDUINO_ARCH_ESP32S2
  uint32_t particlelimit = ESP32S2_MAXPARTICLES_1D; // maximum number of paticles allowed
#else
  uint32_t particlelimit = ESP32_MAXPARTICLES_1D; // maximum number of paticles allowed
#endif
  numberofParticles = min(numberofParticles, particlelimit); // limit to particlelimit
  if (isadvanced) // advanced property array needs ram, reduce number of particles to use the same amount
    numberofParticles = (numberofParticles * sizeof(PSparticle1D)) / (sizeof(PSparticle1D) + sizeof(PSadvancedParticle1D));
  numberofParticles = (numberofParticles * (fraction + 1)) >> 8; // calculate fraction of particles
  numberofParticles = numberofParticles < 20 ? 20 : numberofParticles; // 20 minimum
  //make sure it is a multiple of 4 for proper memory alignment (easier than using padding bytes)
  numberofParticles = ((numberofParticles+3) >> 2) << 2; // note: with a separate particle buffer, this is probably unnecessary
  return numberofParticles;
}

uint32_t calculateNumberOfSources1D(const uint32_t requestedsources) {
#ifdef ESP8266
   int numberofSources = max(1, min((int)requestedsources,ESP8266_MAXSOURCES_1D)); // limit to 1 - 8
#elif ARDUINO_ARCH_ESP32S2
  int numberofSources = max(1, min((int)requestedsources, ESP32S2_MAXSOURCES_1D)); // limit to 1 - 16
#else
  int numberofSources = max(1, min((int)requestedsources, ESP32_MAXSOURCES_1D)); // limit to 1 - 32
#endif
  // make sure it is a multiple of 4 for proper memory alignment (so minimum is acutally 4)
  numberofSources = ((numberofSources+3) >> 2) << 2;
  return numberofSources;
}

//allocate memory for particle system class, particles, sprays plus additional memory requested by FX
bool allocateParticleSystemMemory1D(const uint32_t numparticles, const uint32_t numsources, const bool isadvanced, const uint32_t additionalbytes) {
  uint32_t requiredmemory = sizeof(ParticleSystem1D);
  uint32_t dummy; // dummy variable
  if(particleMemoryManager(numparticles, sizeof(PSparticle1D), dummy, dummy, SEGMENT.mode) == nullptr) // allocate memory for particles
    return false; // not enough memory, function ensures a minimum of numparticles are avialable
  // functions above make sure these are a multiple of 4 bytes (to avoid alignment issues)
  requiredmemory += sizeof(PSparticleFlags1D) * numparticles;
  requiredmemory += sizeof(PSsource1D) * numsources;
  requiredmemory += additionalbytes;
  if (isadvanced)
    requiredmemory += sizeof(PSadvancedParticle1D) * numparticles;
  return(SEGMENT.allocateData(requiredmemory));
}

// initialize Particle System, allocate additional bytes if needed (pointer to those bytes can be read from particle system class: PSdataEnd)
// note: percentofparticles is in uint8_t, for example 191 means 75%, (deafaults to 255 or 100% meaning one particle per pixel), can be more than 100% (but not recommended, can cause out of memory)
bool initParticleSystem1D(ParticleSystem1D *&PartSys, const uint32_t requestedsources, const uint8_t fractionofparticles, const uint32_t additionalbytes, const bool advanced) {
  if (SEGLEN == 1) return false; // single pixel not supported  
  if(advanced)
    updateRenderingBuffer(10, false, true); // buffer for advanced particles, fixed size
  uint32_t numparticles = calculateNumberOfParticles1D(fractionofparticles, advanced);
  uint32_t numsources = calculateNumberOfSources1D(requestedsources);
  if (!allocateParticleSystemMemory1D(numparticles, numsources, advanced, additionalbytes)) {
    DEBUG_PRINT(F("PS init failed: memory depleted"));
    return false;
  }
  PartSys = new (SEGENV.data) ParticleSystem1D(SEGMENT.virtualLength(), numparticles, numsources, advanced); // particle system constructor
  updateRenderingBuffer(SEGMENT.vLength(), true, true); // update/create frame rendering buffer note: for fragmentation it might be better to allocate this first, but if memory is scarce, system has a buffer but no particles and will return false
  return true;
}

// blur a 1D buffer, sub-size blurring can be done using start and size
// for speed, 32bit variables are used, make sure to limit them to 8bit (0-255) or result is undefined
// to blur a subset of the buffer, change the size and set start to the desired starting coordinates
void blur1D(CRGB *colorbuffer, uint32_t size, uint32_t blur, uint32_t start)
{
  CRGB seeppart, carryover;
  uint32_t seep = blur >> 1;
  carryover =  BLACK;
  for(uint32_t x = start; x < start + size; x++) {
    seeppart = colorbuffer[x]; // create copy of current color
    fast_color_scale(seeppart, seep); // scale it and seep to neighbours
    if (x > 0) {
      fast_color_add(colorbuffer[x-1], seeppart);
      if(carryover) // note: check adds overhead but is faster on average
        fast_color_add(colorbuffer[x], carryover); // is black on first pass
    }
    carryover = seeppart;
  }
}
#endif // WLED_DISABLE_PARTICLESYSTEM1D

#if !(defined(WLED_DISABLE_PARTICLESYSTEM2D) && defined(WLED_DISABLE_PARTICLESYSTEM1D)) // not both disabled

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

// fastled color adding is very inaccurate in color preservation (but it is fast)
// a better color add function is implemented in colors.cpp but it uses 32bit RGBW. to use it colors need to be shifted just to then be shifted back by that function, which is slow
// this is a fast version for RGB (no white channel, PS does not handle white) and with native CRGB including scaling of second color
// note: result is stored in c1, not using a return value is faster as the CRGB struct does not need to be copied upon return
// note2: function is mainly used to add scaled colors, so checking if one color is black is slower
// note3: scale is 255 when using blur, checking for that makes blur faster
static void fast_color_add(CRGB &c1, const CRGB &c2, const uint32_t scale) {
  uint32_t r, g, b;
  if (scale < 255) {
    r = c1.r + ((c2.r * scale) >> 8);
    g = c1.g + ((c2.g * scale) >> 8);
    b = c1.b + ((c2.b * scale) >> 8);
  } else {
    r = c1.r + c2.r;
    g = c1.g + c2.g;
    b = c1.b + c2.b;
  }

  uint32_t max = std::max(r,g); // check for overflow, using max() is faster as the compiler can optimize
  max = std::max(max,b);
  if (max < 256) {
    c1.r = r; // save result to c1
    c1.g = g;
    c1.b = b;
  } else {
    uint32_t newscale = (255U << 16) / max;
    c1.r = (r * newscale) >> 16;
    c1.g = (g * newscale) >> 16;
    c1.b = (b * newscale) >> 16;
  }
}

// faster than fastled color scaling as it does in place scaling
static void fast_color_scale(CRGB &c, const uint32_t scale) {
  c.r = ((c.r * scale) >> 8);
  c.g = ((c.g * scale) >> 8);
  c.b = ((c.b * scale) >> 8);
}


//////////////////////////////////////////////////////////
// memory and transition management for particle system //
//////////////////////////////////////////////////////////
// note: these functions can only be called while strip is servicing

// allocate memory using the FX data limit, if overridelimit is set, temporarily ignore the limit
void* allocatePSmemory(size_t size, bool overridelimit) {
  PSPRINT(" PS mem alloc: ");
      PSPRINTLN(size);
  // buffer uses effect data, check if there is enough space
  if (!overridelimit && Segment::getUsedSegmentData() + size > MAX_SEGMENT_DATA) {
    // not enough memory
    PSPRINT(F("!!! Effect RAM depleted: "));
    DEBUG_PRINTF_P(PSTR("%d/%d !!!\n"), size, Segment::getUsedSegmentData());
    errorFlag = ERR_NORAM;
    return nullptr;
  }
  void* buffer = calloc(size, sizeof(byte));
  if (buffer == nullptr) {
    PSPRINT(F("!!! Memory allocation failed !!!"));
    errorFlag = ERR_NORAM;
    return nullptr;
  }
  Segment::addUsedSegmentData(size);
  #ifdef WLED_DEBUG_PS
  PSPRINT("Pointer address: 0x");
  Serial.println((uintptr_t)buffer, HEX);
  #endif
  return buffer;
}

// deallocate memory and update data usage, use with care!
void deallocatePSmemory(void* dataptr, uint32_t size) {
  PSPRINTLN("deallocating PSmemory:" + String(size));
  if(dataptr == nullptr) return; // safety check
  free(dataptr); // note: setting pointer null must be done by caller, passing a reference to a cast void pointer is not possible
  Segment::addUsedSegmentData(size <= Segment::getUsedSegmentData() ? -size : -Segment::getUsedSegmentData());
}

// Particle transition manager, creates/extends buffer if needed and handles transition memory-handover
void* particleMemoryManager(const uint32_t requestedParticles, size_t structSize, uint32_t &availableToPS, uint32_t numParticlesUsed, const uint8_t effectID) {
  pmem = getPartMem();
  void* buffer = nullptr;
  PSPRINTLN("PS MemManager");
  if (pmem) { // segment has a buffer
    if (requestedParticles) { // request for a new buffer, this is an init call
      PSPRINTLN("Buffer exists, request for particles: " + String(requestedParticles));
      pmem->transferParticles = true; // set flag to transfer particles
      uint32_t requestsize = structSize * requestedParticles; // required buffer size
      if (requestsize > pmem->buffersize) { // request is larger than buffer, try to extend it
        if (Segment::getUsedSegmentData() + requestsize - pmem->buffersize <= MAX_SEGMENT_DATA) { // enough memory available to extend buffer
          PSPRINTLN("Extending buffer");
          buffer = allocatePSmemory(requestsize, true); // calloc new memory in FX data, override limit (temporary buffer)
          if (buffer) { // allocaction successful, copy old particles to new buffer
            memcpy(buffer,  pmem->particleMemPointer, pmem->buffersize); // copy old particle buffer note: only required if transition but copy is fast and rarely happens
            deallocatePSmemory(pmem->particleMemPointer, pmem->buffersize); // free old memory
            pmem->particleMemPointer = buffer; // set new buffer
            pmem->buffersize = requestsize; // update buffer size
          }
          else
            return nullptr; // no memory available
        }
      }
      if (pmem->watchdog == 1) { // if a PS already exists during particle request, it kicked the watchdog in last frame, servicePSmem() adds 1 afterwards -> PS to PS transition
        if(pmem->currentFX == effectID) // if the new effect is the same as the current one, do not transition: transferParticles is set above, so this will transfer all particles back if called during transition
          pmem->inTransition = false; // reset transition flag
        else
          pmem->inTransition = effectID; // save the ID of the new effect (required to determine blur amount in rendering function)
        PSPRINTLN("PS to PS transition");
      }
      return pmem->particleMemPointer; // return the available buffer on init call
    }
    pmem->watchdog = 0; // kick watchdog
    buffer = pmem->particleMemPointer; // buffer is already allocated
  }
  else { // if the id was not found create a buffer and add an element to the list
    PSPRINTLN("New particle buffer request: " + String(requestedParticles));
    uint32_t requestsize = structSize * requestedParticles; // required buffer size
    buffer = allocatePSmemory(requestsize, false); // allocate new memory
    if (buffer)
      partMemList.push_back({buffer, requestsize, 0, strip.getCurrSegmentId(), 0, 0, 0, false, true});  // add buffer to list, set flag to transfer/init the particles note: if pushback fails, it may crash
    else
      return nullptr; // there is no memory available TODO: if localbuffer is allocated, free it and try again, its no use having a buffer but no particles
    pmem = getPartMem(); // get the pointer to the new element (check that it was added)
    if (!pmem) { // something went wrong
      free(buffer);
      return nullptr;
    }
    return buffer; // directly return the buffer on init call
  }

  // now we have a valid buffer, if this is a PS to PS FX transition: transfer particles slowly to new FX
  if(!SEGMENT.isInTransition()) pmem->inTransition = false; // transition has ended, invoke final transfer
  if (pmem->inTransition) {
    uint32_t maxParticles = pmem->buffersize / structSize; // maximum number of particles that fit in the buffer
    uint16_t progress = SEGMENT.progress(); // transition progress
    uint32_t newAvailable = 0;
    if (SEGMENT.mode == effectID) { // new effect ID -> function was called from new FX
      PSPRINTLN("new effect");
      newAvailable = (maxParticles * progress) >> 16; // update total particles available to this PS (newAvailable is guaranteed to be smaller than maxParticles)
      if(newAvailable < 2) newAvailable = 2; // give 2 particle minimum (some FX may crash with less as they do i+1 access)
      if(newAvailable > numParticlesUsed) newAvailable = numParticlesUsed; // limit to number of particles used, do not move the pointer anymore (will be set to base in final handover)
      uint32_t bufferoffset = (maxParticles - 1) - newAvailable; // offset to new effect particles (in particle structs, not bytes)
      if(bufferoffset < maxParticles) // safety check
        buffer = (void*)((uint8_t*)buffer + bufferoffset * structSize); // new effect gets the end of the buffer
      int32_t totransfer = newAvailable - availableToPS; // number of particles to transfer in this transition update
      if(totransfer > 0) // safety check
        particleHandover(buffer, structSize, totransfer);
    }
    else { // this was called from the old FX
      PSPRINTLN("old effect");
      SEGMENT.loadOldPalette(); // load the old palette into segment palette
      progress = 0xFFFFU - progress; // inverted transition progress
      newAvailable = ((maxParticles * progress) >> 16); // result is guaranteed to be smaller than maxParticles
      if(newAvailable > 0) newAvailable--; // -1 to avoid overlapping memory in 1D<->2D transitions
      if(newAvailable < 2) newAvailable = 2; // give 2 particle minimum (some FX may crash with less as they do i+1 access)
      // note: buffer pointer stays the same, number of available particles is reduced
    }
    availableToPS = newAvailable;
  } else if(pmem->transferParticles) { // no PS transition, full buffer available
    // transition ended (or blending is disabled) -> transfer all remaining particles
    PSPRINTLN("PS transition ended, final particle handover");
    uint32_t maxParticles = pmem->buffersize / structSize; // maximum number of particles that fit in the buffer
    if (maxParticles > availableToPS) { // not all particles transferred yet
      uint32_t totransfer = maxParticles - availableToPS; // transfer all remaining particles
      if(totransfer <= maxParticles) // safety check
        particleHandover(buffer, structSize, totransfer);
      if(maxParticles > numParticlesUsed) { // FX uses less than max: move the already existing particles to the beginning of the buffer
        uint32_t usedbytes = availableToPS * structSize;
        int32_t bufferoffset = (maxParticles - 1) - availableToPS; // offset to existing particles (see above)
        if(bufferoffset < (int)maxParticles) { // safety check
          void* currentBuffer = (void*)((uint8_t*)buffer + bufferoffset * structSize); // pointer to current buffer start
          memmove(buffer, currentBuffer, usedbytes); // move the existing particles to the beginning of the buffer
        }
      }
    }
    // kill unused particles so they do not re-appear when transitioning to next FX
    //TODO: should this be done in the handover function? maybe with a "cleanup" parameter?
    //TODO2: the memmove above should be done here (or in handover function): it should copy all alive particles to the beginning of the buffer (to TTL=0 particles maybe?)
    // -> currently when moving form blobs to ballpit particles disappear
    #ifndef WLED_DISABLE_PARTICLESYSTEM2D
    if (structSize == sizeof(PSparticle)) { // 2D particle
      PSparticle *particles = (PSparticle*)buffer;
      for (uint32_t i = availableToPS; i < maxParticles; i++) {
        particles[i].ttl = 0; // kill unused particles
      }
    }
    else // 1D particle system
    #endif
    {
      #ifndef WLED_DISABLE_PARTICLESYSTEM1D
      PSparticle1D *particles = (PSparticle1D*)buffer;
      for (uint32_t i = availableToPS; i < maxParticles; i++) {
        particles[i].ttl = 0; // kill unused particles
      }
      #endif
    }
    availableToPS = maxParticles; // now all particles are available to new FX
    PSPRINTLN("final available particles: " + String(availableToPS));
    pmem->particleType = structSize; // update particle type
    pmem->transferParticles = false;
    pmem->finalTransfer = true; // let rendering function update its buffer if required
    pmem->currentFX = effectID; // FX has now settled in, update the FX ID to track future transitions
  }
  else // no transition
    pmem->finalTransfer = false;

  #ifdef WLED_DEBUG_PS
  PSPRINT(" Particle memory Pointer address: 0x");
  Serial.println((uintptr_t)buffer, HEX);
  #endif
  return buffer;
}

// (re)initialize particles in the particle buffer for use in the new FX
void particleHandover(void *buffer, size_t structSize, int32_t numToTransfer) {
  if (pmem->particleType != structSize) { // check if we are being handed over from a different system (1D<->2D), clear buffer if so
    memset(buffer, 0, numToTransfer * structSize); // clear buffer
  }
  uint16_t maxTTL = 0;
  uint32_t TTLrandom = 0;
  maxTTL = ((unsigned)strip.getTransition() << 1) / FRAMETIME_FIXED; // tie TTL to transition time: limit to double the transition time + some randomness
  #ifndef WLED_DISABLE_PARTICLESYSTEM2D
  if (structSize == sizeof(PSparticle)) { // 2D particle
    PSparticle *particles = (PSparticle *)buffer;
    for (int32_t i = 0; i < numToTransfer; i++) {
      if (blendingStyle == BLEND_STYLE_FADE) {
        if(particles[i].ttl > maxTTL)
          particles[i].ttl = maxTTL + hw_random16(150); // reduce TTL so it will die soon
      }
      else
        particles[i].ttl = 0; // kill transferred particles if not using fade blending style
      particles[i].sat = 255; // full saturation
    }
  }
  else // 1D particle system
  #endif
  {
    #ifndef WLED_DISABLE_PARTICLESYSTEM1D
    PSparticle1D *particles = (PSparticle1D *)buffer;
    for (int32_t i = 0; i < numToTransfer; i++) {
      if (blendingStyle == BLEND_STYLE_FADE) {
        if(particles[i].ttl > maxTTL)
          particles[i].ttl = maxTTL + hw_random16(150); // reduce TTL so it will die soon
      }
      else
        particles[i].ttl = 0; // kill transferred particles if not using fade blending style
    }
    #endif
  }
}

// update number of particles to use, limit to allocated (= particles allocated by the calling system) in case more are available in the buffer
void updateUsedParticles(const uint32_t allocated, const uint32_t available, const uint8_t percentage, uint32_t &used) {
  uint32_t wantsToUse = 1 + ((allocated * ((uint32_t)percentage + 1)) >> 8); // always give 1 particle minimum
  used = max((uint32_t)2, min(available, wantsToUse)); // limit to available particles, use a minimum of 2
}

// check if a segment is partially overlapping with an underlying segment (used to enable overlay rendering i.e. adding instead of overwriting pixels)
bool segmentIsOverlay(void) { // TODO: this only needs to be checked when segment is created, could move this to segment class or PS init
  unsigned segID = strip.getCurrSegmentId();
  if (segID == 0) return false; // is base segment, no overlay
  unsigned newStartX = strip._segments[segID].start;
  unsigned newEndX   = strip._segments[segID].stop;
  unsigned newStartY = strip._segments[segID].startY;
  unsigned newEndY   = strip._segments[segID].stopY;

  // Check for overlap with all previous segments
  for (unsigned i = 0; i < segID; i++) {
    if(strip._segments[i].freeze) continue; // skip inactive segments
    unsigned startX = strip._segments[i].start;
    unsigned endX   = strip._segments[i].stop;
    unsigned startY = strip._segments[i].startY;
    unsigned endY   = strip._segments[i].stopY;

    if (newStartX < endX && newEndX > startX &&  // x-range overlap
        newStartY < endY && newEndY > startY) {  // y-range overlap
      return true;
    }
  }

  return false; // No overlap detected
}

// get the pointer to the particle memory for the segment
partMem* getPartMem(void) {
  uint8_t segID = strip.getCurrSegmentId();
  for (partMem &pmem : partMemList) {
    if (pmem.id == segID) {
      return &pmem;
    }
  }
  return nullptr;
}

// function to update the framebuffer and renderbuffer
void updateRenderingBuffer(uint32_t requiredpixels, bool isFramebuffer, bool initialize) {
  PSPRINTLN("updateRenderingBuffer");
  uint16_t& targetBufferSize = isFramebuffer ? frameBufferSize : renderBufferSize; // corresponding buffer size

  // if(isFramebuffer) return; // debug/testing only: disable frame-buffer

  if(targetBufferSize < requiredpixels) { // check current buffer size
    CRGB** targetBuffer = isFramebuffer ? &framebuffer : &renderbuffer; // pointer to target buffer
    if(*targetBuffer || initialize) { // update only if initilizing or if buffer exists (prevents repeatet allocation attempts if initial alloc failed)
      if(*targetBuffer) // buffer exists, free it
        deallocatePSmemory((void*)(*targetBuffer), targetBufferSize * sizeof(CRGB));
      *targetBuffer = reinterpret_cast<CRGB *>(allocatePSmemory(requiredpixels * sizeof(CRGB), false));
      if(*targetBuffer)
        targetBufferSize = requiredpixels;
      else
        targetBufferSize = 0;
    }
  }
}

// service the particle system memory, free memory if idle too long
// note: doing it this way makes it independent of the implementation of segment management but is not the most memory efficient way
void servicePSmem() {
  // Increment watchdog for each entry and deallocate if idle too long (i.e. no PS running on that segment)
  if(partMemList.size() > 0) {
    for (size_t i = 0; i < partMemList.size(); i++) {
      if(strip.getSegmentsNum() > i) { // segment still exists
        if(strip._segments[i].freeze) continue; // skip frozen segments (incrementing watchdog will delete memory, leading to crash)
      }
      partMemList[i].watchdog++;  // Increment watchdog counter
      PSPRINT("pmem servic. list size: ");
      PSPRINT(partMemList.size());
      PSPRINT(" element: ");
      PSPRINT(i);
      PSPRINT(" watchdog: ");
      PSPRINTLN(partMemList[i].watchdog);
      if (partMemList[i].watchdog > MAX_MEMIDLE) {
          deallocatePSmemory(partMemList[i].particleMemPointer, partMemList[i].buffersize); // Free memory
          partMemList.erase(partMemList.begin() + i);  // Remove entry
          //partMemList.shrink_to_fit(); // partMemList is small, memory operations should be unproblematic (this may lead to mem fragmentation, removed for now)
      }
    }
  }
  else { // no particle system running, release buffer memory
    if(framebuffer) {
      deallocatePSmemory((void*)framebuffer, frameBufferSize * sizeof(CRGB)); // free the buffers
      framebuffer = nullptr;
      frameBufferSize = 0;
    }
    if(renderbuffer) {
      deallocatePSmemory((void*)renderbuffer, renderBufferSize * sizeof(CRGB));
      renderbuffer = nullptr;
      renderBufferSize = 0;
    }
  }
}

// transfer the frame buffer to the segment and handle transitional rendering (both FX render to the same buffer so they mix)
void transferBuffer(uint32_t width, uint32_t height, bool useAdditiveTransfer) {
  if(!framebuffer) return; // no buffer, nothing to transfer
  PSPRINT(" xfer buf ");
  #ifndef WLED_DISABLE_MODE_BLEND
  bool tempBlend = SEGMENT.getmodeBlend();
  if(pmem->inTransition && blendingStyle == BLEND_STYLE_FADE) {
      SEGMENT.modeBlend(false); // temporarily disable FX blending in PS to PS transition (using local buffer to do PS blending)
  }
  #endif
  if(height) { // is 2D, 1D passes height = 0
    for (uint32_t y = 0; y < height; y++) {
      int index = y * width; // current row index for 1D buffer
      for (uint32_t x = 0; x < width; x++) {
        CRGB *c = &framebuffer[index++];
        uint32_t clr = RGBW32(c->r,c->g,c->b,0); // convert to 32bit color
        if(useAdditiveTransfer) {
          uint32_t segmentcolor = SEGMENT.getPixelColorXY((int)x, (int)y);
          CRGB segmentRGB = CRGB(segmentcolor);
          if(clr == 0) // frame buffer is black, just update the framebuffer
            *c = segmentRGB;
          else { // color to add to segment is not black
            if(segmentcolor) {
              fast_color_add(*c, segmentRGB); // add segment color back to buffer if not black
              clr = RGBW32(c->r,c->g,c->b,0); // convert to 32bit color (again) and set the segment
            }
            SEGMENT.setPixelColorXY((int)x, (int)y, clr); // save back to segment after adding local buffer
          }
        }
        //if(clr > 0) // not black  TODO: not transferring black is faster and enables overlay, but requires proper handling of buffer clearing, which is quite complex and probably needs a change to SEGMENT handling.
        else
          SEGMENT.setPixelColorXY((int)x, (int)y, clr);
      }
    }
  } else { // 1D system
    for (uint32_t x = 0; x < width; x++) {
      CRGB *c = &framebuffer[x];
      uint32_t clr = RGBW32(c->r,c->g,c->b,0);
      if(useAdditiveTransfer) {
        uint32_t segmentcolor = SEGMENT.getPixelColor((int)x);;
        CRGB segmentRGB = CRGB(segmentcolor);
        if(clr == 0) // frame buffer is black, just load the color (for next frame)
          *c = segmentRGB;
        else { // color to add to segment is not black
          if(segmentcolor) {
            fast_color_add(*c, segmentRGB); // add segment color back to buffer if not black
            clr = RGBW32(c->r,c->g,c->b,0); // convert to 32bit color (again)
          }
          SEGMENT.setPixelColor((int)x, clr); // save back to segment after adding local buffer
        }
      }
      //if(color > 0) // not black
      else
        SEGMENT.setPixelColor((int)x, clr);
    }
  }
  #ifndef WLED_DISABLE_MODE_BLEND
  SEGMENT.modeBlend(tempBlend); // restore blending mode
  #endif
}

#endif  // !(defined(WLED_DISABLE_PARTICLESYSTEM2D) && defined(WLED_DISABLE_PARTICLESYSTEM1D))
