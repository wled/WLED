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
static void fast_color_add(CRGB &c1, const CRGB &c2, uint8_t scale = 255); // fast and accurate color adding with scaling (scales c2 before adding)
static void fast_color_scale(CRGB &c, const uint8_t scale); // fast scaling function using 32bit variable and pointer. note: keep 'scale' within 0-255
//static CRGB *allocateCRGBbuffer(uint32_t length);
#endif

#ifndef WLED_DISABLE_PARTICLESYSTEM2D
ParticleSystem2D::ParticleSystem2D(uint32_t width, uint32_t height, uint32_t numberofparticles, uint32_t numberofsources, bool isadvanced, bool sizecontrol) {
  PSPRINTLN("\n ParticleSystem2D constructor");
  numSources = numberofsources; // number of sources allocated in init
  numParticles = numberofparticles; // number of particles allocated in init
  usedParticles = numParticles; // use all particles by default
  advPartProps = nullptr; //make sure we start out with null pointers (just in case memory was not cleared)
  advPartSize = nullptr;
  setMatrixSize(width, height);
  updatePSpointers(isadvanced, sizecontrol); // set the particle and sources pointer (call this before accessing sprays or particles)
  setWallHardness(255); // set default wall hardness to max
  setWallRoughness(0); // smooth walls by default
  setGravity(0); //gravity disabled by default
  setParticleSize(1); // 2x2 rendering size by default
  motionBlur = 0; //no fading by default
  smearBlur = 0; //no smearing by default
  emitIndex = 0;
  collisionStartIdx = 0;

  //initialize some default non-zero values most FX use
  for (uint32_t i = 0; i < numParticles; i++) {
     particles[i].sat = 255; // full saturation
  }
  for (uint32_t i = 0; i < numSources; i++) {
    sources[i].source.sat = 255; //set saturation to max by default
    sources[i].source.ttl = 1; //set source alive
    sources[i].sourceFlags.asByte = 0; // all flags disabled
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
      if (updateSize(&advPartProps[i], &advPartSize[i]) == false) { // if particle shrinks to 0 size
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
  usedParticles = (numParticles * ((int)percentage+1)) >> 8; // number of particles to use (percentage is 0-255, 255 = 100%)
  PSPRINT(" SetUsedpaticles: allocated particles: ");
  PSPRINT(numParticles);
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
  if (particlesize > 1) {
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
  if (emitIndex > 0)  particles[emitIndex].ttl += emitter.source.ttl;
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

    if (!checkBoundsAndWrap(newY, maxY, renderradius, options->wrapY)) { // check out of bounds  note: this must not be skipped. if gravity is enabled, particles will never bounce at the top
      partFlags.outofbounds = true;
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
      else if (!checkBoundsAndWrap(newX, maxX, renderradius, options->wrapX)) { // check out of bounds
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
  int32_t deviation = ((uint32_t)size * (uint32_t)advsize->asymmetry + 255) >> 8; // deviation from symmetrical size
  // Calculate x and y size based on deviation and direction (0 is symmetrical, 64 is x, 128 is symmetrical, 192 is y)
  if (asymdir < 64) {
    deviation = (asymdir * deviation) >> 6;
  } else if (asymdir < 192) {
    deviation = ((128 - asymdir) * deviation) >> 6;
  } else {
    deviation = ((asymdir - 255) * deviation) >> 6;
  }
  // Calculate x and y size based on deviation, limit to 255 (rendering function cannot handle larger sizes)
  xsize = min((size - deviation), (int32_t)255);
  ysize = min((size + deviation), (int32_t)255);;
}

// function to bounce a particle from a wall using set parameters (wallHardness and wallRoughness)
void ParticleSystem2D::bounce(int8_t &incomingspeed, int8_t &parallelspeed, int32_t &position, const uint32_t maxposition) {
  incomingspeed = -incomingspeed;
  incomingspeed = (incomingspeed * wallHardness + 128) >> 8; // reduce speed as energy is lost on non-hard surface
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
void ParticleSystem2D::render() {
  CRGB baseRGB;
  uint32_t brightness; // particle brightness, fades if dying
  TBlendType blend = LINEARBLEND; // default color rendering: wrap palette
  if (particlesettings.colorByAge) {
    blend = LINEARBLEND_NOWRAP;
  }

  if (motionBlur) { // motion-blurring active
    for (int32_t y = 0; y <= maxYpixel; y++) {
      int index = y * (maxXpixel + 1);
      for (int32_t x = 0; x <= maxXpixel; x++) {
        fast_color_scale(framebuffer[index], motionBlur); // note: could skip if only globalsmear is active but usually they are both active and scaling is fast enough
        index++;
      }
    }
  }
  else { // no blurring: clear buffer
    memset(framebuffer, 0, (maxXpixel+1) * (maxYpixel+1) * sizeof(CRGB));
  }

  // go over particles and render them to the buffer
  for (uint32_t i = 0; i < usedParticles; i++) {
    if (particles[i].ttl == 0 || particleFlags[i].outofbounds)
      continue;
    // generate RGB values for particle
    if (fireIntesity) { // fire mode
      brightness = (uint32_t)particles[i].ttl * (3 + (fireIntesity >> 5)) + 5;
      brightness = min(brightness, (uint32_t)255);
      baseRGB = ColorFromPaletteWLED(SEGPALETTE, brightness, 255, LINEARBLEND_NOWRAP);
    }
    else {
      brightness = min((particles[i].ttl << 1), (int)255);
      baseRGB = ColorFromPaletteWLED(SEGPALETTE, particles[i].hue, 255, blend);
      if (particles[i].sat < 255) {
        CHSV32 baseHSV;
        rgb2hsv((uint32_t((byte(baseRGB.r) << 16) | (byte(baseRGB.g) << 8) | (byte(baseRGB.b)))), baseHSV); // convert to HSV
        baseHSV.s = min(baseHSV.s, particles[i].sat); // set the saturation but don't increase it
        uint32_t tempcolor;
        hsv2rgb(baseHSV, tempcolor); // convert back to RGB
        baseRGB = (CRGB)tempcolor;
      }
    }
    brightness = gamma8(brightness); // apply gamma correction, used for gamma-inverted brightness distribution
    renderParticle(i, brightness, baseRGB, particlesettings.wrapX, particlesettings.wrapY);
  }

  // apply global size rendering
  if (particlesize > 1) {
    uint32_t passes = particlesize / 64 + 1; // number of blur passes, four passes max
    uint32_t bluramount = particlesize;
    uint32_t bitshift = 0;
    for (uint32_t i = 0; i < passes; i++) {
      if (i == 2) // for the last two passes, use higher amount of blur (results in a nicer brightness gradient with soft edges)
        bitshift = 1;
      blur2D(framebuffer, maxXpixel + 1, maxYpixel + 1, bluramount << bitshift, bluramount << bitshift);
      bluramount -= 64;
    }
  }

  // apply 2D blur to rendered frame
  if (smearBlur) {
    blur2D(framebuffer, maxXpixel + 1, maxYpixel + 1, smearBlur, smearBlur);
  }

  // transfer the framebuffer to the segment
  for (int y = 0; y <= maxYpixel; y++) {
    int index = y * (maxXpixel + 1); // current row index for 1D buffer
    for (int x = 0; x <= maxXpixel; x++) {
      SEGMENT.setPixelColorXY(x, y, framebuffer[index++]);
    }
  }
}

// calculate pixel positions and brightness distribution and render the particle to local buffer or global buffer
__attribute__((optimize("O2"))) void ParticleSystem2D::renderParticle(const uint32_t particleindex, const uint8_t brightness, const CRGB& color, const bool wrapX, const bool wrapY) {
  uint32_t size = particlesize;
  if (advPartProps && advPartProps[particleindex].size > 0) // use advanced size properties (0 means use global size including single pixel rendering)
    size = advPartProps[particleindex].size;

  if (size == 0) { // single pixel rendering
    uint32_t x = particles[particleindex].x >> PS_P_RADIUS_SHIFT;
    uint32_t y = particles[particleindex].y >> PS_P_RADIUS_SHIFT;
    if (x <= (uint32_t)maxXpixel && y <= (uint32_t)maxYpixel) {
      fast_color_add(framebuffer[x + (maxYpixel - y) * (maxXpixel + 1)], color, brightness);
    }
    return;
  }
  uint8_t pxlbrightness[4]; // brightness values for the four pixels representing a particle
  struct {
    int32_t x,y;
  } pixco[4]; // particle pixel coordinates, the order is bottom left [0], bottom right[1], top right [2], top left [3] (thx @blazoncek for improved readability struct)
  bool pixelvalid[4] = {true, true, true, true}; // is set to false if pixel is out of bounds

  // add half a radius as the rendering algorithm always starts at the bottom left, this leaves things positive, so shifts can be used, then shift coordinate by a full pixel (x--/y-- below)
  int32_t xoffset = particles[particleindex].x + PS_P_HALFRADIUS;
  int32_t yoffset = particles[particleindex].y + PS_P_HALFRADIUS;
  int32_t dx = xoffset & (PS_P_RADIUS - 1); // relativ particle position in subpixel space
  int32_t dy = yoffset & (PS_P_RADIUS - 1); // modulo replaced with bitwise AND, as radius is always a power of 2
  int32_t x = (xoffset >> PS_P_RADIUS_SHIFT); // divide by PS_P_RADIUS which is 64, so can bitshift (compiler can not optimize integer)
  int32_t y = (yoffset >> PS_P_RADIUS_SHIFT);

  // set the four raw pixel coordinates
  pixco[1].x = pixco[2].x = x;  // bottom right & top right
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
  // adjust brightness such that distribution is linear after gamma correction:
  // - scale brigthness with gamma correction (done in render())
  // - apply inverse gamma correction to brightness values
  // - gamma is applied again in show() -> the resulting brightness distribution is linear but gamma corrected in total
  pxlbrightness[0] = gamma8inv(pxlbrightness[0]); // use look-up-table for invers gamma
  pxlbrightness[1] = gamma8inv(pxlbrightness[1]);
  pxlbrightness[2] = gamma8inv(pxlbrightness[2]);
  pxlbrightness[3] = gamma8inv(pxlbrightness[3]);

  if (advPartProps && advPartProps[particleindex].size > 1) { //render particle to a bigger size
    CRGB renderbuffer[100]; // 10x10 pixel buffer
    memset(renderbuffer, 0, sizeof(renderbuffer)); // clear buffer
    //particle size to pixels: < 64 is 4x4, < 128 is 6x6, < 192 is 8x8, bigger is 10x10
    //first, render the pixel to the center of the renderbuffer, then apply 2D blurring
    fast_color_add(renderbuffer[4 + (4 * 10)], color, pxlbrightness[0]); // oCrder is: bottom left, bottom right, top right, top left
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
    for (uint32_t i = 0; i < maxsize; i++) {
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
        fast_color_add(framebuffer[xfb + (maxYpixel - yfb) * (maxXpixel + 1)], renderbuffer[xrb + yrb * 10]);
      }
    }
    } else { // standard rendering (2x2 pixels)
    // check for out of frame pixels and wrap them if required: x,y is bottom left pixel coordinate of the particle
    if (x < 0) { // left pixels out of frame
      if (wrapX) { // wrap x to the other side if required
        pixco[0].x = pixco[3].x = maxXpixel;
      } else {
        pixelvalid[0] = pixelvalid[3] = false; // out of bounds
      }
    }
    else if (pixco[1].x > (int32_t)maxXpixel) { // right pixels, only has to be checked if left pixel is in frame
      if (wrapX) { // wrap y to the other side if required
        pixco[1].x = pixco[2].x = 0;
      } else {
        pixelvalid[1] = pixelvalid[2] = false; // out of bounds
      }
    }

    if (y < 0) { // bottom pixels out of frame
      if (wrapY) { // wrap y to the other side if required
        pixco[0].y = pixco[1].y = maxYpixel;
      } else {
        pixelvalid[0] = pixelvalid[1] = false; // out of bounds
      }
    }
    else if (pixco[2].y > maxYpixel) { // top pixels
      if (wrapY) { // wrap y to the other side if required
        pixco[2].y = pixco[3].y = 0;
      } else {
        pixelvalid[2] = pixelvalid[3] = false; // out of bounds
      }
    }
    for (uint32_t i = 0; i < 4; i++) {
      if (pixelvalid[i])
        fast_color_add(framebuffer[pixco[i].x + (maxYpixel - pixco[i].y) * (maxXpixel + 1)], color, pxlbrightness[i]); // order is: bottom left, bottom right, top right, top left
    }
  }
}

// detect collisions in an array of particles and handle them
// uses binning by dividing the frame into slices in x direction which is efficient if using gravity in y direction (but less efficient for FX that use forces in x direction)
// for code simplicity, no y slicing is done, making very tall matrix configurations less efficient
// note: also tested adding y slicing, it gives diminishing returns, some FX even get slower. FX not using gravity would benefit with a 10% FPS improvement
void ParticleSystem2D::handleCollisions() {
  uint32_t collDistSq = particleHardRadius << 1; // distance is double the radius note: particleHardRadius is updated when setting global particle size
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
      if (particles[pidx].ttl > 0) { // is alive
        if (particles[pidx].x >= binStart && particles[pidx].x <= binEnd) { // >= and <= to include particles on the edge of the bin (overlap to ensure boarder particles collide with adjacent bins)
          if(particleFlags[pidx].outofbounds == 0 && particleFlags[pidx].collide) { // particle is in frame and does collide note: checking flags is quite slow and usually these are set, so faster to check here
            if (binParticleCount >= maxBinParticles) { // bin is full, more particles in this bin so do the rest next frame
              nextFrameStartIdx = pidx; // bin overflow can only happen once as bin size is at least half of the particles (or half +1)
              break;
            }
            binIndices[binParticleCount++] = pidx;
          }
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
        int32_t dx = (particles[idx_j].x + particles[idx_j].vx) - (particles[idx_i].x + particles[idx_i].vx); // distance with lookahead
        if (dx * dx < collDistSq) { // check x direction, if close, check y direction (squaring is faster than abs() or dual compare)
          int32_t dy = (particles[idx_j].y + particles[idx_j].vy)  - (particles[idx_i].y + particles[idx_i].vy); // distance with lookahead
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
__attribute__((optimize("O2"))) void ParticleSystem2D::collideParticles(PSparticle &particle1, PSparticle &particle2, int32_t dx, int32_t dy, const uint32_t collDistSq) {
  int32_t distanceSquared = dx * dx + dy * dy;
  // Calculate relative velocity note: could zero check but that does not improve overall speed but deminish it as that is rarely the case and pushing is still required
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
    if (distanceSquared < collDistSq && dotProduct > -250) { // too close and also slow, push them apart
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
  updatePSpointers(advPartProps != nullptr, advPartSize != nullptr); // update pointers to PS data, also updates availableParticles
  PSPRINTLN("\n END update System2D, running FX...");
}

// set the pointers for the class (this only has to be done once and not on every FX call, only the class pointer needs to be reassigned to SEGENV.data every time)
// function returns the pointer to the next byte available for the FX (if it assigned more memory for other stuff using the above allocate function)
// FX handles the PSsources, need to tell this function how many there are
void ParticleSystem2D::updatePSpointers(bool isadvanced, bool sizecontrol) {
  PSPRINTLN("updatePSpointers");
  // Note on memory alignment:
  // a pointer MUST be 4 byte aligned. sizeof() in a struct/class is always aligned to the largest element. if it contains a 32bit, it will be padded to 4 bytes, 16bit is padded to 2byte alignment.
  // The PS is aligned to 4 bytes, a PSparticle is aligned to 2 and a struct containing only byte sized variables is not aligned at all and may need to be padded when dividing the memoryblock.
  // by making sure that the number of sources and particles is a multiple of 4, padding can be skipped here as alignent is ensured, independent of struct sizes.
  particles = reinterpret_cast<PSparticle *>(this + 1); // pointer to particles
  particleFlags = reinterpret_cast<PSparticleFlags *>(particles + numParticles); // pointer to particle flags
  sources = reinterpret_cast<PSsource *>(particleFlags + numParticles); // pointer to source(s) at data+sizeof(ParticleSystem2D)
  framebuffer = reinterpret_cast<CRGB *>(sources + numSources); // pointer to framebuffer
  // align pointer after framebuffer
  uintptr_t p = reinterpret_cast<uintptr_t>(framebuffer + (maxXpixel+1)*(maxYpixel+1));
  p = (p + 3) & ~0x03; // align to 4-byte boundary
  PSdataEnd = reinterpret_cast<uint8_t *>(p); // pointer to first available byte after the PS for FX additional data
  if (isadvanced) {
    advPartProps = reinterpret_cast<PSadvancedParticle *>(PSdataEnd);
    PSdataEnd = reinterpret_cast<uint8_t *>(advPartProps + numParticles);
    if (sizecontrol) {
      advPartSize = reinterpret_cast<PSsizeControl *>(PSdataEnd);
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

  for (uint32_t y = ystart; y < ystart + ysize; y++) {
    carryover =  BLACK;
    uint32_t indexXY = xstart + y * width;
    for (uint32_t x = xstart; x < xstart + xsize; x++) {
      seeppart = colorbuffer[indexXY]; // create copy of current color
      fast_color_scale(seeppart, seep); // scale it and seep to neighbours
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
    for (uint32_t y = ystart; y < ystart + ysize; y++) {
      seeppart = colorbuffer[indexXY]; // create copy of current color
      fast_color_scale(seeppart, seep); // scale it and seep to neighbours
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
  numberofParticles = (numberofParticles+3) & ~0x03;
  return numberofParticles;
}

uint32_t calculateNumberOfSources2D(uint32_t pixels, uint32_t requestedsources) {
#ifdef ESP8266
  int numberofSources = min((pixels) / 8, (uint32_t)requestedsources);
  numberofSources = max(1, min(numberofSources, ESP8266_MAXSOURCES)); // limit
#elif ARDUINO_ARCH_ESP32S2
  int numberofSources = min((pixels) / 6, (uint32_t)requestedsources);
  numberofSources = max(1, min(numberofSources, ESP32S2_MAXSOURCES)); // limit
#else
  int numberofSources = min((pixels) / 4, (uint32_t)requestedsources);
  numberofSources = max(1, min(numberofSources, ESP32_MAXSOURCES)); // limit
#endif
  // make sure it is a multiple of 4 for proper memory alignment
  numberofSources = (numberofSources+3) & ~0x03;
  return numberofSources;
}

//allocate memory for particle system class, particles, sprays plus additional memory requested by FX //TODO: add percentofparticles like in 1D to reduce memory footprint of some FX?
bool allocateParticleSystemMemory2D(uint32_t numparticles, uint32_t numsources, bool isadvanced, bool sizecontrol, uint32_t additionalbytes) {
  PSPRINTLN("PS 2D alloc");
  PSPRINTLN("numparticles:" + String(numparticles) + " numsources:" + String(numsources) + " additionalbytes:" + String(additionalbytes));
  uint32_t requiredmemory = sizeof(ParticleSystem2D);
  // functions above make sure numparticles is a multiple of 4 bytes (to avoid alignment issues)
  requiredmemory += sizeof(PSparticleFlags) * numparticles;
  requiredmemory += sizeof(PSparticle) * numparticles;
  if (isadvanced)
    requiredmemory += sizeof(PSadvancedParticle) * numparticles;
  if (sizecontrol)
    requiredmemory += sizeof(PSsizeControl) * numparticles;
  requiredmemory += sizeof(PSsource) * numsources;
  requiredmemory += sizeof(CRGB) * SEGMENT.virtualLength(); // virtualLength is witdh * height
  requiredmemory += additionalbytes + 3; // add 3 to ensure there is room for stuffing bytes
  //requiredmemory = (requiredmemory + 3) & ~0x03; // align memory block to next 4-byte boundary
  PSPRINTLN("mem alloc: " + String(requiredmemory));
  return(SEGMENT.allocateData(requiredmemory));
}

// initialize Particle System, allocate additional bytes if needed (pointer to those bytes can be read from particle system class: PSdataEnd)
bool initParticleSystem2D(ParticleSystem2D *&PartSys, uint32_t requestedsources, uint32_t additionalbytes, bool advanced, bool sizecontrol) {
  PSPRINT("PS 2D init ");
  if (!strip.isMatrix) return false; // only for 2D
  uint32_t cols = SEGMENT.virtualWidth();
  uint32_t rows = SEGMENT.virtualHeight();
  uint32_t pixels = cols * rows;

  uint32_t numparticles = calculateNumberOfParticles2D(pixels, advanced, sizecontrol);
  PSPRINT(" segmentsize:" + String(cols) + " x " + String(rows));
  PSPRINT(" request numparticles:" + String(numparticles));
  uint32_t numsources = calculateNumberOfSources2D(pixels, requestedsources);
  if (!allocateParticleSystemMemory2D(numparticles, numsources, advanced, sizecontrol, additionalbytes))
  {
    DEBUG_PRINT(F("PS init failed: memory depleted"));
    return false;
  }

  PartSys = new (SEGENV.data) ParticleSystem2D(cols, rows, numparticles, numsources, advanced, sizecontrol); // particle system constructor

  PSPRINTLN("******init done, pointers:");
  return true;
}

#endif // WLED_DISABLE_PARTICLESYSTEM2D


////////////////////////
// 1D Particle System //
////////////////////////
#ifndef WLED_DISABLE_PARTICLESYSTEM1D

ParticleSystem1D::ParticleSystem1D(uint32_t length, uint32_t numberofparticles, uint32_t numberofsources, bool isadvanced) {
  numSources = numberofsources;
  numParticles = numberofparticles; // number of particles allocated in init
  usedParticles = numParticles; // use all particles by default
  advPartProps = nullptr; //make sure we start out with null pointers (just in case memory was not cleared)
  //advPartSize = nullptr;
  setSize(length);
  updatePSpointers(isadvanced); // set the particle and sources pointer (call this before accessing sprays or particles)
  setWallHardness(255); // set default wall hardness to max
  setGravity(0); //gravity disabled by default
  setParticleSize(0); // 1 pixel size by default
  motionBlur = 0; //no fading by default
  smearBlur = 0; //no smearing by default
  emitIndex = 0;
  collisionStartIdx = 0;
  // initialize some default non-zero values most FX use
  for (uint32_t i = 0; i < numSources; i++) {
    sources[i].source.ttl = 1; //set source alive
    sources[i].sourceFlags.asByte = 0; // all flags disabled
  }

  if (isadvanced) {
    for (uint32_t i = 0; i < numParticles; i++) {
      advPartProps[i].sat = 255; // set full saturation
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

  render();
}

// set percentage of used particles as uint8_t i.e 127 means 50% for example
void ParticleSystem1D::setUsedParticles(const uint8_t percentage) {
  usedParticles = (numParticles * ((int)percentage+1)) >> 8; // number of particles to use (percentage is 0-255, 255 = 100%)
  PSPRINT(" SetUsedpaticles: allocated particles: ");
  PSPRINT(numParticles);
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
      particleFlags[emitIndex].collide = emitter.sourceFlags.collide; // TODO: could just set all flags (asByte) but need to check if that breaks any of the FX
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
void ParticleSystem1D::render() {
  CRGB baseRGB;
  uint32_t brightness; // particle brightness, fades if dying
  TBlendType blend = LINEARBLEND; // default color rendering: wrap palette
  if (particlesettings.colorByAge || particlesettings.colorByPosition) {
    blend = LINEARBLEND_NOWRAP;
  }

  #ifdef ESP8266 // no local buffer on ESP8266
  if (motionBlur)
    SEGMENT.fadeToBlackBy(255 - motionBlur);
  else
    SEGMENT.fill(BLACK); // clear the buffer before rendering to it
  #else
  if (motionBlur) { // blurring active
    for (int32_t x = 0; x <= maxXpixel; x++) {
      fast_color_scale(framebuffer[x], motionBlur);
    }
  }
  else { // no blurring: clear buffer
    memset(framebuffer, 0, (maxXpixel+1) * sizeof(CRGB));
  }
  #endif
  // go over particles and render them to the buffer
  for (uint32_t i = 0; i < usedParticles; i++) {
    if ( particles[i].ttl == 0 || particleFlags[i].outofbounds)
      continue;

    // generate RGB values for particle
    brightness = min(particles[i].ttl << 1, (int)255);
    baseRGB = ColorFromPaletteWLED(SEGPALETTE, particles[i].hue, 255, blend);

    if (advPartProps) { //saturation is advanced property in 1D system
      if (advPartProps[i].sat < 255) {
        CHSV32 baseHSV;
        rgb2hsv((uint32_t((byte(baseRGB.r) << 16) | (byte(baseRGB.g) << 8) | (byte(baseRGB.b)))), baseHSV); // convert to HSV
        baseHSV.s = min(baseHSV.s, advPartProps[i].sat); // set the saturation but don't increase it
        uint32_t tempcolor;
        hsv2rgb(baseHSV, tempcolor); // convert back to RGB
        baseRGB = (CRGB)tempcolor;
      }
    }
    brightness = gamma8(brightness); // apply gamma correction, used for gamma-inverted brightness distribution
    renderParticle(i, brightness, baseRGB, particlesettings.wrap);
  }
  // apply smear-blur to rendered frame
  if (smearBlur) {
    #ifdef ESP8266
    SEGMENT.blur(smearBlur, true); // no local buffer on ESP8266
    #else
    blur1D(framebuffer, maxXpixel + 1, smearBlur, 0);
    #endif
  }

  // add background color
  uint32_t bg_color = SEGCOLOR(1);
  if (bg_color > 0) { //if not black
    CRGB bg_color_crgb = bg_color; // convert to CRGB
    for (int32_t i = 0; i <= maxXpixel; i++) {
      #ifdef ESP8266 // no local buffer on ESP8266
      SEGMENT.addPixelColor(i, bg_color, true);
      #else
      fast_color_add(framebuffer[i], bg_color_crgb);
      #endif
    }
  }

  #ifndef ESP8266
  // transfer the frame-buffer to segment
  for (int x = 0; x <= maxXpixel; x++) {
    SEGMENT.setPixelColor(x, framebuffer[x]);
  }
  #endif
}

// calculate pixel positions and brightness distribution and render the particle to local buffer or global buffer
__attribute__((optimize("O2"))) void ParticleSystem1D::renderParticle(const uint32_t particleindex, const uint8_t brightness, const CRGB &color, const bool wrap) {
  uint32_t size = particlesize;
  if (advPartProps) // use advanced size properties (1D system has no large size global rendering TODO: add large global rendering?)
    size = advPartProps[particleindex].size;

  if (size == 0) { //single pixel particle, can be out of bounds as oob checking is made for 2-pixel particles (and updating it uses more code)
    uint32_t x =  particles[particleindex].x >> PS_P_RADIUS_SHIFT_1D;
    if (x <= (uint32_t)maxXpixel) { //by making x unsigned there is no need to check < 0 as it will overflow
      #ifdef ESP8266 // no local buffer on ESP8266
      SEGMENT.addPixelColor(x, color.scale8(brightness), true);
      #else
      fast_color_add(framebuffer[x], color, brightness);
      #endif
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
  // adjust brightness such that distribution is linear after gamma correction:
  // - scale brigthness with gamma correction (done in render())
  // - apply inverse gamma correction to brightness values
  // - gamma is applied again in show() -> the resulting brightness distribution is linear but gamma corrected in total
  pxlbrightness[0] = gamma8inv(pxlbrightness[0]); // use look-up-table for invers gamma
  pxlbrightness[1] = gamma8inv(pxlbrightness[1]);

  // check if particle has advanced size properties and buffer is available
  if (advPartProps && advPartProps[particleindex].size > 1) {
    CRGB renderbuffer[10]; // 10 pixel buffer
    memset(renderbuffer, 0, sizeof(renderbuffer)); // clear buffer
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
      #ifdef ESP8266 // no local buffer on ESP8266
      SEGMENT.addPixelColor(xfb, renderbuffer[xrb], true);
      #else
      fast_color_add(framebuffer[xfb], renderbuffer[xrb]);
      #endif
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
    for (uint32_t i = 0; i < 2; i++) {
      if (pxlisinframe[i]) {
        #ifdef ESP8266 // no local buffer on ESP8266
        SEGMENT.addPixelColor(pixco[i], color.scale8((uint8_t)pxlbrightness[i]), true);
        #else
        fast_color_add(framebuffer[pixco[i]], color, pxlbrightness[i]);
        #endif
      }
    }
  }

}

// detect collisions in an array of particles and handle them
void ParticleSystem1D::handleCollisions() {
  uint32_t collisiondistance = particleHardRadius << 1;
  // note: partices are binned by position, assumption is that no more than half of the particles are in the same bin
  // if they are, collisionStartIdx is increased so each particle collides at least every second frame (which still gives decent collisions)
  constexpr int BIN_WIDTH = 32 * PS_P_RADIUS_1D; // width of each bin, a compromise between speed and accuracy (larger bins are faster but collapse more)
  int32_t overlap = particleHardRadius << 1; // overlap bins to include edge particles to neighbouring bins
  if (advPartProps) //may be using individual particle size
    overlap += 256; // add 2 * max radius (approximately)
  uint32_t maxBinParticles = max((uint32_t)50, (usedParticles + 1) / 4); // do not bin small amounts, limit max to 1/4 of particles
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
      if (particles[pidx].ttl > 0) { // alivee
        if (particles[pidx].x >= binStart && particles[pidx].x <= binEnd) { // >= and <= to include particles on the edge of the bin (overlap to ensure boarder particles collide with adjacent bins)
          if(particleFlags[pidx].outofbounds == 0 && particleFlags[pidx].collide) { // particle is in frame and does collide note: checking flags is quite slow and usually these are set, so faster to check here
            if (binParticleCount >= maxBinParticles) { // bin is full, more particles in this bin so do the rest next frame
              nextFrameStartIdx = pidx; // bin overflow can only happen once as bin size is at least half of the particles (or half +1)
              break;
            }
            binIndices[binParticleCount++] = pidx;
          }
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
          collisiondistance = (PS_P_MINHARDRADIUS_1D << particlesize) + ((advPartProps[idx_i].size + advPartProps[idx_j].size) >> 1);
        }
        int32_t dx = (particles[idx_j].x + particles[idx_j].vx) - (particles[idx_i].x + particles[idx_i].vx); // distance between particles with lookahead
        uint32_t dx_abs = abs(dx);
        if (dx_abs <= collisiondistance) { // collide if close
          collideParticles(particles[idx_i], particleFlags[idx_i], particles[idx_j], particleFlags[idx_j], dx, dx_abs, collisiondistance);
        }
      }
    }
  }
  collisionStartIdx = nextFrameStartIdx; // set the start index for the next frame
}
// handle a collision if close proximity is detected, i.e. dx and/or dy smaller than 2*PS_P_RADIUS
// takes two pointers to the particles to collide and the particle hardness (softer means more energy lost in collision, 255 means full hard)
__attribute__((optimize("O2"))) void ParticleSystem1D::collideParticles(PSparticle1D &particle1, const PSparticleFlags1D &particle1flags, PSparticle1D &particle2, const PSparticleFlags1D &particle2flags, const int32_t dx, const uint32_t dx_abs, const uint32_t collisiondistance) {
  int32_t dv = particle2.vx - particle1.vx;
  int32_t dotProduct = (dx * dv); // is always negative if moving towards each other

  if (dotProduct < 0) { // particles are moving towards each other
    uint32_t surfacehardness = max(collisionHardness, (int32_t)PS_P_MINSURFACEHARDNESS_1D); // if particles are soft, the impulse must stay above a limit or collisions slip through
    // Calculate new velocities after collision  note: not using dot product like in 2D as impulse is purely speed depnedent
    #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP8266) // use bitshifts with rounding instead of division (2x faster)
    int32_t impulse = ((dv * surfacehardness) + ((dv >> 31) & 0xFF)) >> 8; // note: (v>>31) & 0xFF)) extracts the sign and adds 255 if negative for correct rounding using shifts
    #else // division is faster on ESP32, S2 and S3
    int32_t impulse = (dv * surfacehardness) / 255;
    #endif
    particle1.vx += impulse;
    particle2.vx -= impulse;

    // if one of the particles is fixed, transfer the impulse back so it bounces
    if (particle1flags.fixed)
      particle2.vx = -particle1.vx;
    else if (particle2flags.fixed)
      particle1.vx = -particle2.vx;

    if (collisionHardness < PS_P_MINSURFACEHARDNESS_1D && (SEGMENT.call & 0x07) == 0) { // if particles are soft, they become 'sticky' i.e. apply some friction
      const uint32_t coeff = collisionHardness + (250 - PS_P_MINSURFACEHARDNESS_1D);
      #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP8266) // use bitshifts with rounding instead of division (2x faster)
      particle1.vx = ((int32_t)particle1.vx * coeff + (((int32_t)particle1.vx >> 31) & 0xFF)) >> 8; // note: (v>>31) & 0xFF)) extracts the sign and adds 255 if negative for correct rounding using shifts
      particle2.vx = ((int32_t)particle2.vx * coeff + (((int32_t)particle2.vx >> 31) & 0xFF)) >> 8;
      #else // division is faster on ESP32, S2 and S3
      particle1.vx = ((int32_t)particle1.vx * coeff) / 255;
      particle2.vx = ((int32_t)particle2.vx * coeff) / 255;
      #endif
    }
  }

  if (dx_abs < (collisiondistance - 8) && abs(dv) < 5) { // overlapping and moving slowly
    // particles have volume, push particles apart if they are too close
    // behaviour is different than in 2D, we need pixel accurate stacking here, push the top particle
    // note: like in 2D, pushing by a distance makes softer piles collapse, giving particles speed prevents that and looks nicer
    int32_t pushamount = 1;
    if (dx < 0)  // particle2.x < particle1.x
      pushamount = -pushamount;
    particle1.vx -= pushamount;
    particle2.vx += pushamount;

    if (dx_abs < collisiondistance >> 1) { // too close, force push particles so they dont collapse
      pushamount = 1 + ((collisiondistance - dx_abs) >> 3); // note: push amount found by experimentation

      if (particle1.x < (maxX >> 1)) { // lower half, push particle with larger x in positive direction
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
        else if (!particle1flags.fixed) { // particle1.x < particle2.x  -> push particle 1
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
  setSize(SEGMENT.virtualLength()); // update size
  updatePSpointers(advPartProps != nullptr);
}

// set the pointers for the class (this only has to be done once and not on every FX call, only the class pointer needs to be reassigned to SEGENV.data every time)
// function returns the pointer to the next byte available for the FX (if it assigned more memory for other stuff using the above allocate function)
// FX handles the PSsources, need to tell this function how many there are
void ParticleSystem1D::updatePSpointers(bool isadvanced) {
  // Note on memory alignment:
  // a pointer MUST be 4 byte aligned. sizeof() in a struct/class is always aligned to the largest element. if it contains a 32bit, it will be padded to 4 bytes, 16bit is padded to 2byte alignment.
  // The PS is aligned to 4 bytes, a PSparticle is aligned to 2 and a struct containing only byte sized variables is not aligned at all and may need to be padded when dividing the memoryblock.
  // by making sure that the number of sources and particles is a multiple of 4, padding can be skipped here as alignent is ensured, independent of struct sizes.
  particles = reinterpret_cast<PSparticle1D *>(this + 1); // pointer to particles
  particleFlags = reinterpret_cast<PSparticleFlags1D *>(particles + numParticles); // pointer to particle flags
  sources = reinterpret_cast<PSsource1D *>(particleFlags + numParticles); // pointer to source(s)
  #ifdef ESP8266 // no local buffer on ESP8266
  PSdataEnd = reinterpret_cast<uint8_t *>(sources + numSources);
  #else
  framebuffer = reinterpret_cast<CRGB *>(sources + numSources); // pointer to framebuffer
  // align pointer after framebuffer to 4bytes
  uintptr_t p = reinterpret_cast<uintptr_t>(framebuffer + (maxXpixel+1)); // maxXpixel is SEGMENT.virtualLength() - 1
  p = (p + 3) & ~0x03; // align to 4-byte boundary
  PSdataEnd = reinterpret_cast<uint8_t *>(p); // pointer to first available byte after the PS for FX additional data
  #endif
  if (isadvanced) {
    advPartProps = reinterpret_cast<PSadvancedParticle1D *>(PSdataEnd);
    PSdataEnd = reinterpret_cast<uint8_t *>(advPartProps + numParticles); // since numParticles is a multiple of 4, this is always aligned to 4 bytes. No need to add padding bytes here
  }
  #ifdef WLED_DEBUG_PS
  PSPRINTLN(" PS Pointers: ");
  PSPRINT(" PS : 0x");
  Serial.println((uintptr_t)this, HEX);
  PSPRINT(" Particleflags : 0x");
  Serial.println((uintptr_t)particleFlags, HEX);
  PSPRINT(" Particles : 0x");
  Serial.println((uintptr_t)particles, HEX);
  PSPRINT(" Sources : 0x");
  Serial.println((uintptr_t)sources, HEX);
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
  numberofParticles = (numberofParticles+3) & ~0x03; // note: with a separate particle buffer, this is probably unnecessary
  PSPRINTLN(" calc numparticles:" + String(numberofParticles))
  return numberofParticles;
}

uint32_t calculateNumberOfSources1D(const uint32_t requestedsources) {
#ifdef ESP8266
   int numberofSources = max(1, min((int)requestedsources,ESP8266_MAXSOURCES_1D)); // limit
#elif ARDUINO_ARCH_ESP32S2
  int numberofSources = max(1, min((int)requestedsources, ESP32S2_MAXSOURCES_1D)); // limit
#else
  int numberofSources = max(1, min((int)requestedsources, ESP32_MAXSOURCES_1D)); // limit
#endif
  // make sure it is a multiple of 4 for proper memory alignment (so minimum is acutally 4)
  numberofSources = (numberofSources+3) & ~0x03;
  return numberofSources;
}

//allocate memory for particle system class, particles, sprays plus additional memory requested by FX
bool allocateParticleSystemMemory1D(const uint32_t numparticles, const uint32_t numsources, const bool isadvanced, const uint32_t additionalbytes) {
  uint32_t requiredmemory = sizeof(ParticleSystem1D);
  // functions above make sure these are a multiple of 4 bytes (to avoid alignment issues)
  requiredmemory += sizeof(PSparticleFlags1D) * numparticles;
  requiredmemory += sizeof(PSparticle1D) * numparticles;
  requiredmemory += sizeof(PSsource1D) * numsources;
  #ifndef ESP8266 // no local buffer on ESP8266
  requiredmemory += sizeof(CRGB) * SEGMENT.virtualLength();
  #endif
  requiredmemory += additionalbytes + 3; // add 3 to ensure room for stuffing bytes to make it 4 byte aligned
  if (isadvanced)
    requiredmemory += sizeof(PSadvancedParticle1D) * numparticles;
  return(SEGMENT.allocateData(requiredmemory));
}

// initialize Particle System, allocate additional bytes if needed (pointer to those bytes can be read from particle system class: PSdataEnd)
// note: percentofparticles is in uint8_t, for example 191 means 75%, (deafaults to 255 or 100% meaning one particle per pixel), can be more than 100% (but not recommended, can cause out of memory)
bool initParticleSystem1D(ParticleSystem1D *&PartSys, const uint32_t requestedsources, const uint8_t fractionofparticles, const uint32_t additionalbytes, const bool advanced) {
  if (SEGLEN == 1) return false; // single pixel not supported
  uint32_t numparticles = calculateNumberOfParticles1D(fractionofparticles, advanced);
  uint32_t numsources = calculateNumberOfSources1D(requestedsources);
  if (!allocateParticleSystemMemory1D(numparticles, numsources, advanced, additionalbytes)) {
    DEBUG_PRINT(F("PS init failed: memory depleted"));
    return false;
  }
  PartSys = new (SEGENV.data) ParticleSystem1D(SEGMENT.virtualLength(), numparticles, numsources, advanced); // particle system constructor
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
  for (uint32_t x = start; x < start + size; x++) {
    seeppart = colorbuffer[x]; // create copy of current color
    fast_color_scale(seeppart, seep); // scale it and seep to neighbours
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
 __attribute__((optimize("O2"))) static void fast_color_add(CRGB &c1, const CRGB &c2, const uint8_t scale) {
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

  // note: this chained comparison is the fastest method for max of 3 values (faster than std:max() or using xor)
  uint32_t max = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
  if (max <= 255) {
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
 __attribute__((optimize("O2"))) static void fast_color_scale(CRGB &c, const uint8_t scale) {
  c.r = ((c.r * scale) >> 8);
  c.g = ((c.g * scale) >> 8);
  c.b = ((c.b * scale) >> 8);
}

#endif  // !(defined(WLED_DISABLE_PARTICLESYSTEM2D) && defined(WLED_DISABLE_PARTICLESYSTEM1D))
