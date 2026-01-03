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
static uint32_t fast_color_scaleAdd(const uint32_t c1, const uint32_t c2, uint8_t scale = 255); // fast and accurate color adding with scaling (scales c2 before adding)
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
  setParticleSize(1); // 2x2 rendering size by default (disables per particle size control by default)
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
  if (advPartSize != nullptr) {
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
  usedParticles = max((uint32_t)1, (numParticles * ((int)percentage+1)) >> 8); // number of particles to use (percentage is 0-255, 255 = 100%)
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
    motionBlur = bluramount;
}

void ParticleSystem2D::setSmearBlur(uint8_t bluramount) {
  smearBlur = bluramount;
}


// set global particle size
void ParticleSystem2D::setParticleSize(uint8_t size) {
  particlesize = size;
  particleHardRadius = PS_P_MINHARDRADIUS; // ~1 pixel
  perParticleSize = false; // disable per particle size control if global size is set
  if (particlesize > 1) {
    particleHardRadius = PS_P_MINHARDRADIUS + ((particlesize * 52) >> 6); // use 1 pixel + 80% of size for hard radius (slight overlap with boarders so they do not "float" and nicer stacking)
  }
  else if (particlesize == 0)
    particleHardRadius = PS_P_MINHARDRADIUS >> 1; // single pixel particles have half the radius (i.e. 1/2 pixel)
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
      int32_t dx = hw_random16(emitter.var << 1) - emitter.var;
      int32_t dy = hw_random16(emitter.var << 1) - emitter.var;
      if (emitter.var > 5) { // use circular random distribution for large variance to generate nicer "explosions"
        while (dx*dx + dy*dy > emitter.var*emitter.var) { // reject points outside circle
            dx = hw_random16(emitter.var << 1) - emitter.var;
            dy = hw_random16(emitter.var << 1) - emitter.var;
        }
      }
      particles[emitIndex].vx = emitter.vx + dx;
      particles[emitIndex].vy = emitter.vy + dy;
      particles[emitIndex].x = emitter.source.x;
      particles[emitIndex].y = emitter.source.y;
      particles[emitIndex].hue = emitter.source.hue;
      particles[emitIndex].sat = emitter.source.sat;
      particleFlags[emitIndex].collide = emitter.sourceFlags.collide;
      particles[emitIndex].ttl = hw_random16(emitter.minLife, emitter.maxLife);
      if (advPartProps != nullptr)
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
  emitter.vy = ((int32_t)sin16_t(angle) * speed) / (int32_t)32600; // note: cannot use bit shifts as bit shifting is asymmetrical (1>>1=0 / -1>>1=-1) and this needs to be accurate!
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

    int32_t renderradius = PS_P_HALFRADIUS - 1 + particlesize; // used to check out of bounds, if its more than half a radius out of bounds, it will render to x = -2/-1 or x=max/max+1 in standard 2x2 rendering
    int32_t newX = part.x + (int32_t)part.vx;
    int32_t newY = part.y + (int32_t)part.vy;
    partFlags.outofbounds = false; // reset out of bounds (in case particle was created outside the matrix and is now moving into view) note: moving this to checks below adds code and is not faster

    if (perParticleSize && advancedproperties != nullptr) { // using individual particle size
      renderradius = PS_P_HALFRADIUS - 1 + advancedproperties->size; // note: single pixel particles should be zero but OOB checks in rendering function handle this
      if (advancedproperties->size > 0)
        particleHardRadius = PS_P_MINHARDRADIUS + ((advancedproperties->size * 52) >> 6); // use 1 pixel + 80% of size for hard radius (slight overlap with boarders so they do not "float")
      else // single pixel particles use half the collision distance for walls
        particleHardRadius = PS_P_MINHARDRADIUS >> 1;
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
  int8_t yforce = ((int32_t)force * sin16_t(angle)) / 32767; // note: cannot use bit shifts as bit shifting is asymmetrical (1>>1=0 / -1>>1=-1) and this needs to be accurate!
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
  int8_t yforce = ((int32_t)force * sin16_t(angle)) / 32767; // note: cannot use bit shifts as bit shifting is asymmetrical (1>>1=0 / -1>>1=-1) and this needs to be accurate!
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
  int8_t yforce = (force * dy) / 1024; // note: cannot use bit shifts as bit shifting is asymmetrical (1>>1=0 / -1>>1=-1) and this needs to be accurate!
  applyForce(particleindex, xforce, yforce);
}

// render particles to the LED buffer (uses palette to render the 8bit particle color value)
// if wrap is set, particles half out of bounds are rendered to the other side of the matrix
// warning: do not render out of bounds particles or system will crash! rendering does not check if particle is out of bounds
// firemode is only used for PS Fire FX
void ParticleSystem2D::render() {
  if (framebuffer == nullptr) {
    PSPRINTLN(F("PS render: no framebuffer!"));
    return;
  }
  CRGBW baseRGB;
  uint32_t brightness; // particle brightness, fades if dying
  TBlendType blend = LINEARBLEND; // default color rendering: wrap palette
  if (particlesettings.colorByAge) {
    blend = LINEARBLEND_NOWRAP;
  }

  if (motionBlur) { // motion-blurring active
    for (int32_t y = 0; y <= maxYpixel; y++) {
      int index = y * (maxXpixel + 1);
      for (int32_t x = 0; x <= maxXpixel; x++) {
        framebuffer[index] = fast_color_scale(framebuffer[index], motionBlur); // note: could skip if only globalsmear is active but usually they are both active and scaling is fast enough
        index++;
      }
    }
  }
  else { // no blurring: clear buffer
    memset(framebuffer, 0, (maxXpixel+1) * (maxYpixel+1) * sizeof(CRGBW));
  }

  // go over particles and render them to the buffer
  for (uint32_t i = 0; i < usedParticles; i++) {
    if (particles[i].ttl == 0 || particleFlags[i].outofbounds)
      continue;
    // generate RGB values for particle
    if (fireIntesity) { // fire mode
      brightness = (uint32_t)particles[i].ttl * (3 + (fireIntesity >> 5)) + 5;
      brightness = min(brightness, (uint32_t)255);
      baseRGB = ColorFromPaletteWLED(SEGPALETTE, brightness, 255, LINEARBLEND_NOWRAP); // map hue to brightness for fire effect
    }
    else {
      brightness = min((particles[i].ttl << 1), (int)255);
      baseRGB = ColorFromPaletteWLED(SEGPALETTE, particles[i].hue, 255, blend);
      if (particles[i].sat < 255) {
        CHSV32 baseHSV;
        rgb2hsv(baseRGB.color32, baseHSV); // convert to HSV
        baseHSV.s = min(baseHSV.s, particles[i].sat); // set the saturation but don't increase it
        hsv2rgb(baseHSV, baseRGB.color32); // convert back to RGB
      }
    }
    if (gammaCorrectCol) brightness = gamma8(brightness); // apply gamma correction, used for gamma-inverted brightness distribution
    renderParticle(i, brightness, baseRGB, particlesettings.wrapX, particlesettings.wrapY);
  }

  // apply 2D blur to rendered frame
  if (smearBlur) {
    SEGMENT.blur2D(smearBlur, smearBlur, true);
  }
}

// calculate pixel positions and brightness distribution and render the particle to local buffer or global buffer
void WLED_O2_ATTR ParticleSystem2D::renderParticle(const uint32_t particleindex, const uint8_t brightness, const CRGBW& color, const bool wrapX, const bool wrapY) {
  uint32_t size = particlesize;

  if (perParticleSize && advPartProps != nullptr) // use advanced size properties
    size = 1 + advPartProps[particleindex].size; // add 1 to avoid single pixel size particles (collisions do not support it)

  if (size == 0) { // single pixel rendering
    uint32_t x = particles[particleindex].x >> PS_P_RADIUS_SHIFT;
    uint32_t y = particles[particleindex].y >> PS_P_RADIUS_SHIFT;
    if (x <= (uint32_t)maxXpixel && y <= (uint32_t)maxYpixel) {
      uint32_t index = x + (maxYpixel - y) * (maxXpixel + 1); // flip y coordinate (0,0 is bottom left in PS but top left in framebuffer)
      framebuffer[index] = fast_color_scaleAdd(framebuffer[index], color, brightness);
    }
    return;
  }

  if (size > 1) { // size > 1: render as ellipse
    renderLargeParticle(size, particleindex, brightness, color, wrapX, wrapY); // larger size rendering
    return;
  }

  // size = 1: standard 2x2 pixel rendering using bilinear interpolation (20% faster than ellipse rendering)
  uint8_t pxlbrightness[4]; // brightness values for the four pixels representing a particle
  struct {
    int32_t x,y;
  } pixco[4]; // particle pixel coordinates, the order is bottom left [0], bottom right[1], top right [2], top left [3] (thx @blazoncek for improved readability struct)
  bool pixelvalid[4] = {true, true, true, true}; // is set to false if pixel is out of bounds

  // add half a radius as the rendering algorithm always starts at the bottom left, this leaves things positive, so shifts can be used, then shift coordinate by a full pixel (x--/y-- below)
  // if sub-pixel position is 0-PS_P_HALFRADIUS it will render to x>>PS_P_RADIUS_SHIFT as the right pixel
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
  // precalculate values for speed optimization. Note: rounding is not perfect but close enough, some inaccuracy is traded for speed
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
  if (gammaCorrectCol) {
    for (uint32_t i = 0; i < 4; i++) {
      pxlbrightness[i] = gamma8inv(pxlbrightness[i]); // use look-up-table for invers gamma
    }
  }

  // standard rendering (2x2 pixels)
  // check for out of frame pixels and wrap them if required: x,y is bottom left pixel coordinate of the particle
  if (pixco[0].x < 0) { // left pixels out of frame
    if (wrapX) { // wrap x to the other side if required
      pixco[0].x = pixco[3].x = maxXpixel;
    } else {
      pixelvalid[0] = pixelvalid[3] = false; // out of bounds
      if (pixco[0].x < -1) return; // both left pixels out of bounds, no need to continue (safety check)
    }
  }
  else if (pixco[1].x > (int32_t)maxXpixel) { // right pixels, only has to be checked if left pixel is in frame
    if (wrapX) { // wrap y to the other side if required
      pixco[1].x = pixco[2].x = 0;
    } else {
      pixelvalid[1] = pixelvalid[2] = false; // out of bounds
      if (pixco[0].x > (int32_t)maxXpixel) return; // both pixels out of bounds, no need to continue (safety check)
    }
  }

  if (pixco[0].y < 0) { // bottom pixels out of frame
    if (wrapY) { // wrap y to the other side if required
      pixco[0].y = pixco[1].y = maxYpixel;
    } else {
      pixelvalid[0] = pixelvalid[1] = false; // out of bounds
      if (pixco[0].y < -1) return; // both bottom pixels out of bounds, no need to continue (safety check)
    }
  }
  else if (pixco[2].y > maxYpixel) { // top pixels
    if (wrapY) { // wrap y to the other side if required
      pixco[2].y = pixco[3].y = 0;
    } else {
      pixelvalid[2] = pixelvalid[3] = false; // out of bounds
      if (pixco[2].y > (int32_t)maxYpixel + 1) return; // both top pixels out of bounds, no need to continue (safety check)
    }
  }
  for (uint32_t i = 0; i < 4; i++) {
    if (pixelvalid[i]) {
      uint32_t idx = pixco[i].x + (maxYpixel - pixco[i].y) * (maxXpixel + 1); // flip y coordinate (0,0 is bottom left in PS but top left in framebuffer)
      framebuffer[idx] = fast_color_scaleAdd(framebuffer[idx], color, pxlbrightness[i]); // order is: bottom left, bottom right, top right, top left
    }
  }
}

// render particle as ellipse/circle with linear brightness falloff and sub-pixel precision
void WLED_O2_ATTR ParticleSystem2D::renderLargeParticle(const uint32_t size, const uint32_t particleindex, const uint8_t brightness, const CRGBW& color, const bool wrapX, const bool wrapY) {
  // particle position with sub-pixel precision
  int32_t x_subcenter = particles[particleindex].x;
  int32_t y_subcenter = particles[particleindex].y;

  // example: for x = 128, a paticle is exacly between pixel 1 and 2, with a radius of 2 pixels, we draw pixels 0-3
  // integer center jumps when x = 127 -> pixel 1 goes to x = 128 -> pixel 2
  // when calculating the dx, we need to take this into account: at x = 128 the x offset is 1, the pixel center is at pixel 2:
  // for pixel 1, dx = 1 * PS_P_RADIUS - 128 = -64 but the center of the pixel is actually only -32 from the particle center so need to add half a radius:
  // dx = pixel_x * PS_P_RADIUS - x_subcenter + PS_P_HALFRADIUS

  // sub-pixel offset (0-63)
  int32_t x_offset = x_subcenter & (PS_P_RADIUS - 1); // same as modulo PS_P_RADIUS but faster
  int32_t y_offset = y_subcenter & (PS_P_RADIUS - 1);
  // integer pixel position, this is rounded down
  int32_t x_center = (x_subcenter) >> PS_P_RADIUS_SHIFT;
  int32_t y_center = (y_subcenter) >> PS_P_RADIUS_SHIFT;

  // ellipse radii in pixels
  uint32_t xsize = size;
  uint32_t ysize = size;
  if (advPartSize != nullptr && advPartSize[particleindex].asymmetry > 0) {
    getParticleXYsize(&advPartProps[particleindex], &advPartSize[particleindex], xsize, ysize);
  }

  int32_t rx_subpixel = xsize + PS_P_RADIUS + 1; // size = 1 means radius of just over 1 pixel, + PS_P_RADIUS (+1 to accoutn for bit-shift loss)
  int32_t ry_subpixel = ysize + PS_P_RADIUS + 1; // size = 255 is radius of 5, so add 65 -> 65+255=320, 320>>6=5 pixels

  // rendering bounding box in pixels
  int32_t rx_pixels = (rx_subpixel >> PS_P_RADIUS_SHIFT);
  int32_t ry_pixels = (ry_subpixel >> PS_P_RADIUS_SHIFT);

  int32_t x_min = x_center - rx_pixels; // note: the "+1" extension needed for 1D is not required for 2D, it is smooth as-is
  int32_t x_max = x_center + rx_pixels;
  int32_t y_min = y_center - ry_pixels;
  int32_t y_max = y_center + ry_pixels;

  // cache for speed
  uint32_t matrixX = maxXpixel + 1;
  uint32_t matrixY = maxYpixel + 1;
  uint32_t rx_sq = rx_subpixel * rx_subpixel;
  uint32_t ry_sq = ry_subpixel * ry_subpixel;

  // iterate over bounding box and render each pixel
  for (int32_t py = y_min; py <= y_max; py++) {
    for (int32_t px = x_min; px <= x_max; px++) {
      // Check bounds and apply wrapping
      int32_t render_x = px;
      int32_t render_y = py;
      if (render_x < 0) {
        if (!wrapX) continue;
        render_x += matrixX;
      } else if (render_x > maxXpixel) {
        if (!wrapX) continue;
        render_x -= matrixX;
      }

      if (render_y < 0) {
        if (!wrapY) continue;
        render_y += matrixY;
      } else if (render_y > maxYpixel) {
        if (!wrapY) continue;
        render_y -= matrixY;
      }

      // distance from particle center, explanation see above
      int32_t dx_subpixel = (px << PS_P_RADIUS_SHIFT) - x_subcenter + PS_P_HALFRADIUS;
      int32_t dy_subpixel = (py << PS_P_RADIUS_SHIFT) - y_subcenter + PS_P_HALFRADIUS;

      // calculate brightness based on squared distance to ellipse center
      uint8_t pixel_brightness = calculateEllipseBrightness(dx_subpixel, dy_subpixel, rx_sq, ry_sq, brightness);

      if (pixel_brightness == 0) continue; // skip black pixels

      // apply inverse gamma correction if needed, if this is skipped, particles flicker due to changing total brightness
      if (gammaCorrectCol) {
        pixel_brightness = gamma8inv(pixel_brightness); // invert brigthess so brightness distribution is linear after gamma correction
      }

      // Render pixel
      uint32_t idx = render_x + (maxYpixel - render_y) * matrixX; // flip y coordinate (0,0 is bottom left in PS but top left in framebuffer)
      framebuffer[idx] = fast_color_scaleAdd(framebuffer[idx], color, pixel_brightness);
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
  int binWidth = 6 * PS_P_RADIUS; // width of a bin in sub-pixels
  int32_t overlap = particleHardRadius << 1; // overlap bins to include edge particles to neighbouring bins
  if (perParticleSize && advPartProps != nullptr)
    overlap = 512; // max overlap for collision detection if using per-particle size, enough to catch all particles even at max speed

  uint32_t maxBinParticles = max((uint32_t)50, (usedParticles + 1) / 2); // assume no more than half of the particles are in the same bin, do not bin small amounts of particles
  uint32_t numBins = (maxX + (binWidth - 1)) / binWidth; // number of bins in x direction
  if (usedParticles < maxBinParticles) {
    numBins = 1; // use single bin for small number of particles
    binWidth = maxX + 1;
  }
  uint16_t binIndices[maxBinParticles]; // creat array on stack for indices, 2kB max for 1024 particles (ESP32_MAXPARTICLES/2)
  uint32_t binParticleCount; // number of particles in the current bin
  uint32_t nextFrameStartIdx = hw_random16(usedParticles); // index of the first particle in the next frame (set to fixed value if bin overflow)
  uint32_t pidx = collisionStartIdx; //start index in case a bin is full, process remaining particles next frame

  // fill the binIndices array for this bin
  for (uint32_t bin = 0; bin < numBins; bin++) {
    binParticleCount = 0; // reset for this bin
    int32_t binStart = bin * binWidth - overlap; // note: first bin will extend to negative, but that is ok as out of bounds particles are ignored
    int32_t binEnd = binStart + binWidth + (overlap << 1); // add twice the overlap as start is start-overlap, note: last bin can be out of bounds, see above;

    // fill the binIndices array for this bin
    for (uint32_t i = 0; i < usedParticles; i++) {
      if (particles[pidx].ttl > 0) { // is alive
        if (particles[pidx].x >= binStart && particles[pidx].x <= binEnd) { // >= and <= to include particles on the edge of the bin (overlap to ensure boarder particles collide with adjacent bins)
          if (particleFlags[pidx].outofbounds == 0 && particleFlags[pidx].collide) { // particle is in frame and does collide note: checking flags is quite slow and usually these are set, so faster to check here
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

    int32_t massratio1 = 0; // 0 means dont use mass ratio (equal mass)
    int32_t massratio2 = 0; // TODO: if implementing "fixed" particles, set to 1 (fixed) and 255 (movable)
    for (uint32_t i = 0; i < binParticleCount; i++) { // go though all 'higher number' particles in this bin and see if any of those are in close proximity and if they are, make them collide
      uint32_t idx_i = binIndices[i];
      for (uint32_t j = i + 1; j < binParticleCount; j++) { // check against higher number particles
        uint32_t idx_j = binIndices[j];
        if (perParticleSize && advPartProps != nullptr) { // using individual particle size
          collDistSq = (PS_P_MINHARDRADIUS << 1) + ((((uint32_t)advPartProps[idx_i].size + (uint32_t)advPartProps[idx_j].size) * 52) >> 6); // collision distance, use 80% of size for tighter stacking (slight overlap)
          collDistSq = collDistSq * collDistSq; // square it for faster comparison
          // calculate mass ratio for collision response
          uint32_t mass1 = PS_P_RADIUS + advPartProps[idx_i].size;
          uint32_t mass2 = PS_P_RADIUS + advPartProps[idx_j].size;
          mass1 = mass1 * mass1; // mass proportional to area
          mass2 = mass2 * mass2;
          uint32_t totalmass = mass1 + mass2;
          massratio1 = (mass2 << 8) / totalmass; // massratio 1 depends on mass of particle 2, i.e. if 2 is heavier -> higher velocity impact on 1
          massratio2 = (mass1 << 8) / totalmass;
        }
        // note: using the same logic as in 1D is much slower though it would be more accurate but it is not really needed in 2D: particles slipping through each other is much less visible
        int32_t dx = (particles[idx_j].x + particles[idx_j].vx) - (particles[idx_i].x + particles[idx_i].vx); // distance with lookahead
        if (dx * dx < collDistSq) { // check x direction, if close, check y direction (squaring is faster than abs() or dual compare)
          int32_t dy = (particles[idx_j].y + particles[idx_j].vy)  - (particles[idx_i].y + particles[idx_i].vy); // distance with lookahead
          if (dy * dy < collDistSq) // particles are close
            collideParticles(particles[idx_i], particles[idx_j], dx, dy, collDistSq, massratio1, massratio2);
        }
      }
    }
  }
  collisionStartIdx = nextFrameStartIdx; // set the start index for the next frame
}

// handle a collision if close proximity is detected, i.e. dx and/or dy smaller than 2*PS_P_RADIUS
// takes two pointers to the particles to collide and the particle hardness (softer means more energy lost in collision, 255 means full hard)
void WLED_O2_ATTR ParticleSystem2D::collideParticles(PSparticle &particle1, PSparticle &particle2, int32_t dx, int32_t dy, const uint32_t collDistSq, int32_t massratio1, int32_t massratio2) {
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
    // integer math is much faster than using floats (float divisions are slow on all ESPs)
    // overflow check: dx/dy are 7bit, relativV are 8bit -> dotproduct is 15bit, dotproduct/distsquared ist 8b, multiplied by collisionhardness of 8bit. so a 16bit shift is ok, make it 15 to be sure no overflows happen
    // note: cannot use right shifts as bit shifting in right direction is asymmetrical (1>>1=0 / -1>>1=-1) and this needs to be accurate! the trick is: only shift positive numers
    // Calculate new velocities after collision
    int32_t surfacehardness = max(collisionHardness, (int32_t)PS_P_MINSURFACEHARDNESS >> 1); // if particles are soft, the impulse must stay above a limit or collisions slip through at higher speeds, 170 seems to be a good value
    int32_t impulse = (((((-dotProduct) << 15) / distanceSquared) * surfacehardness) >> 8); // note: inverting before bitshift corrects for asymmetry in right-shifts (is slightly faster)

    #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP8266) // use bitshifts with rounding instead of division (2x faster)
    int32_t ximpulse = (impulse * dx + ((dx >> 31) & 0x7FFF)) >> 15; // note: extracting sign bit and adding rounding value to correct for asymmetry in right shifts
    int32_t yimpulse = (impulse * dy + ((dy >> 31) & 0x7FFF)) >> 15;
    #else
    int32_t ximpulse = (impulse * dx) / 32767;
    int32_t yimpulse = (impulse * dy) / 32767;
    #endif
    // if particles are not the same size, use a mass ratio. mass ratio is set to 0 if particles are the same size
    if (massratio1) {
      int32_t vx1 = (int32_t)particle1.vx - ((ximpulse * massratio1) >> 7); // mass ratio is in fixed point 8bit, multiply by two to account for the fact that we distribute the impulse to both particles
      int32_t vy1 = (int32_t)particle1.vy - ((yimpulse * massratio1) >> 7);
      int32_t vx2 = (int32_t)particle2.vx + ((ximpulse * massratio2) >> 7);
      int32_t vy2 = (int32_t)particle2.vy + ((yimpulse * massratio2) >> 7);
      // limit speeds to max speed (required if a lot of impulse is transferred from a large to a small particle)
      particle1.vx = limitSpeed(vx1);
      particle1.vy = limitSpeed(vy1);
      particle2.vx = limitSpeed(vx2);
      particle2.vy = limitSpeed(vy2);
    }
    else {
      particle1.vx -= ximpulse; // note: impulse is inverted, so subtracting it
      particle1.vy -= yimpulse;
      particle2.vx += ximpulse;
      particle2.vy += yimpulse;
    }
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
  }
    // particles have volume, push particles apart if they are too close
    // tried lots of configurations, what works best is to give one particle a little velocity. When adding hard pushing things tend to oscillate.
    // when hard pushing by offsetting position without velocity, they tend to sink into each other under gravity.
    // when using hard-pushing and velocity, there are some oscillations and softer particles do not pile nicely.
    // oscillation get worse if pushing both particles so one is chosen somewhat randomly.
    // softer collisions are not perfect on purpose: soft particles should pile up and overlap slightly, if separation is made perfect, it does not have the intended look

    if (distanceSquared < collDistSq && (relativeVx*relativeVx + relativeVy*relativeVy < 50)) { // too close and also slow, push them apart
      bool fairlyrandom = dotProduct & 0x01; //dotprouct LSB should be somewhat random, so no need to calculate a random number
      int32_t pushamount = 1 + ((collDistSq - distanceSquared) >> 13); // found this by experimentation: it means push by 1, push more if overlapping more than 1.4 physical pixels (i.e. larger particles only)
      int8_t pushx = dx > 0 ? -pushamount : pushamount; // particle 1 is on the left
      int8_t pushy = dy > 0 ? -pushamount : pushamount; // particle 1 is below particle 2

      // if they are very soft, stop slow particles completely to make them stick to each other
      if (collisionHardness < 5) {
        if (fairlyrandom) { // do not stop them every frame to avoid groups of particles hanging mid-air
          particle1.vx = 0;
          particle1.vy = 0;
          particle2.vx = 0;
          particle2.vy = 0;
          // hard-push particle 1 only: if both are pushed, this oscillates ever so slightly
          particle1.x += pushx;
          particle1.y += pushy;
        }
      }
      else {
        if (fairlyrandom) {
          particle1.vx += pushx;
          //particle1.x += pushx;
          particle1.vy += pushy;
          //particle1.y += pushy;
        }
        else {
          particle2.vx -= pushx;
          //particle2.x -= pushx;
          particle2.vy -= pushy;
          //particle2.y -= pushy;
        }
      }
    }
}

// update size and pointers (memory location and size can change dynamically)
// note: do not access the PS class in FX befor running this function (or it messes up SEGENV.data)
void ParticleSystem2D::updateSystem(void) {
  //PSPRINTLN("updateSystem2D");
  setMatrixSize(SEGMENT.vWidth(), SEGMENT.vHeight());
  updatePSpointers(advPartProps != nullptr, advPartSize != nullptr); // update pointers to PS data, also updates availableParticles
  //PSPRINTLN("\n END update System2D, running FX...");
}

// set the pointers for the class (this only has to be done once and not on every FX call, only the class pointer needs to be reassigned to SEGENV.data every time)
// function returns the pointer to the next byte available for the FX (if it assigned more memory for other stuff using the above allocate function)
// FX handles the PSsources, need to tell this function how many there are
void ParticleSystem2D::updatePSpointers(bool isadvanced, bool sizecontrol) {
  //PSPRINTLN("updatePSpointers");
  // Note on memory alignment:
  // a pointer MUST be 4 byte aligned. sizeof() in a struct/class is always aligned to the largest element. if it contains a 32bit, it will be padded to 4 bytes, 16bit is padded to 2byte alignment.
  // The PS is aligned to 4 bytes, a PSparticle is aligned to 2 and a struct containing only byte sized variables is not aligned at all and may need to be padded when dividing the memoryblock.
  // by making sure that the number of sources and particles is a multiple of 4, padding can be skipped here as alignent is ensured, independent of struct sizes.
  particles = reinterpret_cast<PSparticle *>(this + 1); // pointer to particles
  particleFlags = reinterpret_cast<PSparticleFlags *>(particles + numParticles); // pointer to particle flags
  sources = reinterpret_cast<PSsource *>(particleFlags + numParticles); // pointer to source(s) at data+sizeof(ParticleSystem2D)
  framebuffer = SEGMENT.getPixels(); // pointer to framebuffer
  PSdataEnd = reinterpret_cast<uint8_t *>(sources + numSources); // pointer to first available byte after the PS for FX additional data (already aligned to 4 byte boundary)
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

//non class functions to use for initialization
uint32_t calculateNumberOfParticles2D(uint32_t const pixels, const bool isadvanced, const bool sizecontrol) {
  uint32_t numberofParticles = pixels;  // 1 particle per pixel (for example 512 particles on 32x16)
  uint32_t particlelimit = MAXPARTICLES_2D; // maximum number of paticles allowed
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
  int numberofSources = min((pixels) / SOURCEREDUCTIONFACTOR, (uint32_t)requestedsources);
  numberofSources = max(1, min(numberofSources, MAXSOURCES_2D)); // limit
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
  requiredmemory += additionalbytes;
  return(SEGMENT.allocateData(requiredmemory));
}

// initialize Particle System, allocate additional bytes if needed (pointer to those bytes can be read from particle system class: PSdataEnd)
bool initParticleSystem2D(ParticleSystem2D *&PartSys, uint32_t requestedsources, uint32_t additionalbytes, bool advanced, bool sizecontrol) {
  PSPRINT("PS 2D init ");
  if (!strip.isMatrix) return false; // only for 2D
  uint32_t cols = SEGMENT.virtualWidth();
  uint32_t rows = SEGMENT.virtualHeight();
  uint32_t pixels = cols * rows;
  if (sizecontrol)
    advanced = true; // size control needs advanced properties, prevent wrong usage

  uint32_t numparticles = calculateNumberOfParticles2D(pixels, advanced, sizecontrol);
  PSPRINT(" segmentsize:" + String(cols) + " x " + String(rows));
  PSPRINTLN(" request numparticles:" + String(numparticles));
  uint32_t numsources = calculateNumberOfSources2D(pixels, requestedsources);
  bool allocsuccess = false;
  while(numparticles >= 5) { // make sure we have at least 5 particles or quit
    if (allocateParticleSystemMemory2D(numparticles, numsources, advanced, sizecontrol, additionalbytes)) {
      PSPRINTLN(F("PS 2D alloc succeeded"));
      allocsuccess = true;
      break; // allocation succeeded
    }
    numparticles = ((numparticles / 2) + 3) & ~0x03; // cut number of particles in half and try again, must be 4 byte aligned
    PSPRINTLN(F("PS 2D alloc failed, trying with less particles..."));
  }
  if (!allocsuccess) {
    PSPRINTLN(F("PS 2D alloc failed, not enough memory!"));
    return false; // allocation failed
  }

  PartSys = new (SEGENV.data) ParticleSystem2D(cols, rows, numparticles, numsources, advanced, sizecontrol); // particle system constructor

  PSPRINTLN(F("2D PS init done"));
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
  if (particlesettings.useCollisions) {
    handleCollisions();
    if (perParticleSize)
      handleCollisions(); // second pass for per particle size (as impulse transfer can recoil at high speed, this improves "slip through" issues for small particles but is expensive)
  }

  //move all particles
  for (uint32_t i = 0; i < usedParticles; i++) {
    particleMoveUpdate(particles[i], particleFlags[i], nullptr, advPartProps ? &advPartProps[i] : nullptr);
  }

  if (particlesettings.colorByPosition) {
    uint32_t scale = (255 << 16) / maxX;
    for (uint32_t i = 0; i < usedParticles; i++) {
      particles[i].hue = (scale * particles[i].x) >> 16; // note: x is > 0 if not out of bounds
    }
  }

  render();
}

// set percentage of used particles as uint8_t i.e 127 means 50% for example
void ParticleSystem1D::setUsedParticles(const uint8_t percentage) {
  usedParticles =  max((uint32_t)1, (numParticles * ((int)percentage+1)) >> 8); // number of particles to use (percentage is 0-255, 255 = 100%)
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

// render size, 0 = 1 pixel, 1 = 2 pixel (interpolated), 255 = 18 pixel diameter
void ParticleSystem1D::setParticleSize(const uint8_t size) {
  particlesize = size;
  particleHardRadius = PS_P_MINHARDRADIUS_1D; // ~1 pixel
  perParticleSize = false; // disable per particle size control if global size is set
  if (particlesize > 1) {
    particleHardRadius = PS_P_MINHARDRADIUS_1D + ((particlesize * 52) >> 6); // use 1 pixel + 80% of size for hard radius (slight overlap with boarders so they do not "float" and nicer stacking)
  }
  else if (particlesize == 0)
    particleHardRadius = PS_P_MINHARDRADIUS_1D >> 1; // single pixel particles have half the radius (i.e. 1/2 pixel)
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

    int32_t renderradius = PS_P_HALFRADIUS_1D - 1 + particlesize; // used to check out of bounds, default for 2 pixel rendering
    int32_t newX = part.x + (int32_t)part.vx;
    partFlags.outofbounds = false; // reset out of bounds (in case particle was created outside the matrix and is now moving into view)

    if (perParticleSize && advancedproperties != nullptr) { // using individual particle size?
      renderradius = PS_P_HALFRADIUS_1D - 1 + advancedproperties->size; // note: for single pixel particles, it should be zero, but it does not matter as out of bounds checking is done in rendering function
      if (advancedproperties->size > 1)
        particleHardRadius = PS_P_MINHARDRADIUS_1D + ((advancedproperties->size * 52) >> 6); // use 1 pixel + 80% of size for hard radius (slight overlap with boarders so they do not "float" and nicer stacking)
      else // single pixel particles use half the collision distance for walls
        particleHardRadius = PS_P_MINHARDRADIUS_1D >> 1;
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
  if (framebuffer == nullptr) {
    PSPRINTLN(F("PS render: no framebuffer!"));
    return;
  }
  CRGBW baseRGB;
  uint32_t brightness; // particle brightness, fades if dying
  TBlendType blend = LINEARBLEND; // default color rendering: wrap palette
  if (particlesettings.colorByAge || particlesettings.colorByPosition) {
    blend = LINEARBLEND_NOWRAP;
  }

  if (motionBlur) { // blurring active
    for (int32_t x = 0; x <= maxXpixel; x++) {
      framebuffer[x] = fast_color_scale(framebuffer[x], motionBlur);
    }
  }
  else { // no blurring: clear buffer
    memset(framebuffer, 0, (maxXpixel+1) * sizeof(CRGBW));
  }

  // go over particles and render them to the buffer
  for (uint32_t i = 0; i < usedParticles; i++) {
    if ( particles[i].ttl == 0 || particleFlags[i].outofbounds)
      continue;

    // generate RGB values for particle
    brightness = min(particles[i].ttl << 1, (int)255);
    baseRGB = ColorFromPaletteWLED(SEGPALETTE, particles[i].hue, 255, blend);

    if (advPartProps != nullptr) { //saturation is advanced property in 1D system
      if (advPartProps[i].sat < 255) {
        CHSV32 baseHSV;
        rgb2hsv(baseRGB.color32, baseHSV); // convert to HSV
        baseHSV.s = min(baseHSV.s, advPartProps[i].sat); // set the saturation but don't increase it
        hsv2rgb(baseHSV, baseRGB.color32); // convert back to RGB
      }
    }
    if (gammaCorrectCol) brightness = gamma8(brightness); // apply gamma correction, used for gamma-inverted brightness distribution
    renderParticle(i, brightness, baseRGB, particlesettings.wrap);
  }
  // apply smear-blur to rendered frame
  if (smearBlur) {
    SEGMENT.blur(smearBlur, true);
  }

  // add background color
  CRGBW bg_color = SEGCOLOR(1);
  if (bg_color > 0) { //if not black
    for (int32_t i = 0; i <= maxXpixel; i++) {
      framebuffer[i] = fast_color_scaleAdd(framebuffer[i], bg_color);
    }
  }
#ifndef WLED_DISABLE_2D
  // transfer local buffer to segment if using 1D->2D mapping
  if (SEGMENT.is2D() && SEGMENT.map1D2D) {
    for (int x = 0; x <= maxXpixel; x++) {
    //for (int x = 0; x < SEGMENT.vLength(); x++) {
      SEGMENT.setPixelColor(x, framebuffer[x]); // this applies the mapping
    }
  }
#endif
}

// calculate pixel positions and brightness distribution and render the particle to local buffer or global buffer
void WLED_O2_ATTR ParticleSystem1D::renderParticle(const uint32_t particleindex, const uint8_t brightness, const CRGBW &color, const bool wrap) {
  uint32_t size = particlesize;
  if (perParticleSize && advPartProps != nullptr) // use advanced size properties
    size = 1 + advPartProps[particleindex].size; // add 1 to avoid single pixel size particles (collisions do not support it)

  if (size == 0) { //single pixel particle, can be out of bounds as oob checking is made for 2-pixel particles (and updating it uses more code)
    uint32_t x =  particles[particleindex].x >> PS_P_RADIUS_SHIFT_1D;
    if (x <= (uint32_t)maxXpixel) { //by making x unsigned there is no need to check < 0 as it will overflow
      framebuffer[x] = fast_color_scaleAdd(framebuffer[x], color, brightness);
    }
    return;
  }
  //render larger particles
  if (size > 1) { // size > 1: render as gradient line
    renderLargeParticle(size, particleindex, brightness, color, wrap); // larger size rendering
    return;
  }

  // standard rendering (2 pixels per particle)
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
  // - gamma is applied again in show() -> the resulting brightness distribution is linear but gamma corrected in total -> fixes brightness fluctuations
  if (gammaCorrectCol) {
    pxlbrightness[0] = gamma8inv(pxlbrightness[0]); // use look-up-table for invers gamma
    pxlbrightness[1] = gamma8inv(pxlbrightness[1]);
  }

  // check if any pixels are out of frame
  if (pixco[0] < 0) { // left pixels out of frame
    if (wrap) // wrap x to the other side if required
      pixco[0] = maxXpixel;
    else {
      pxlisinframe[0] = false; // pixel is out of matrix boundaries, do not render
      if (pixco[0] < -1)
        return; // both pixels out of frame (safety check)
    }
  }
  else if (pixco[1] > (int32_t)maxXpixel) { // right pixel, only has to be checkt if left pixel did not overflow
    if (wrap) // wrap y to the other side if required
      pixco[1] = 0;
    else {
      pxlisinframe[1] = false;
      if (pixco[0] > (int32_t)maxXpixel)
        return; // both pixels out of frame (safety check)
    }
  }
  for (uint32_t i = 0; i < 2; i++) {
    if (pxlisinframe[i]) {
      framebuffer[pixco[i]] = fast_color_scaleAdd(framebuffer[pixco[i]], color, pxlbrightness[i]);
    }
  }
}

// render particle as a line with linear brightness falloff and sub-pixel precision, size is in 0-255 (1-9 pixel radius)
void WLED_O2_ATTR ParticleSystem1D::renderLargeParticle(const uint32_t size, const uint32_t particleindex, const uint8_t brightness, const CRGBW& color, const bool wrap) {
  int32_t x_subcenter = particles[particleindex].x; // particle position in sub-pixel space

  // sub-pixel offset (0-31)
  int32_t x_offset = x_subcenter & (PS_P_RADIUS_1D - 1); // same as modulo PS_P_RADIUS but faster
  int32_t x_center = x_subcenter >> PS_P_RADIUS_SHIFT_1D; // integer pixel position, this is rounded down

  // particle radius in pixels, size = 1 means radius of just over 1 pixel
  int32_t r_subpixel = size + PS_P_RADIUS_1D + 1; // size = 255 is radius of 9, so add 33 -> 33+255=288, 288>>5=9 pixels (i.e. the +1 is needed to correct for bitshift losses)
  // rendering bounding box in pixels
  int32_t r_pixels = r_subpixel >> PS_P_RADIUS_SHIFT_1D;

  int32_t x_min = x_center - r_pixels - 1; // extend by one for much smoother movement
  int32_t x_max = x_center + r_pixels + 1;

  // cache for speed
  uint32_t matrixX = maxXpixel + 1;

  // iterate over bounding box and render each pixel
  for (int32_t px = x_min; px <= x_max; px++) {
    // Check bounds and apply wrapping
    int32_t render_x = px;
    if (render_x < 0) {
      if (!wrap) continue; // skip out of frame pixels
      render_x += matrixX;
    } else if (render_x > maxXpixel) {
      if (!wrap) continue;
      render_x -= matrixX;
    }
    // squared distance from particle center
    int32_t dx_sq = ((px << PS_P_RADIUS_SHIFT_1D) - x_subcenter + PS_P_HALFRADIUS_1D); // explanation see 2D version
    dx_sq = dx_sq * dx_sq;
    int32_t rx_sq = r_subpixel * r_subpixel;
    uint32_t dist_sq = (dx_sq << 8) / rx_sq; // normalized squared distance in fixed point (0-256)

    // calculate brightness based on distance from particle center with linear falloff
    uint8_t pixel_brightness = dist_sq >= 256 ? 0 : ((256 - dist_sq) * brightness) >> 8;
    //if (pixel_brightness == 0) continue; // skip black pixels note: very few pixels will be black, skipping this is usually faster

    // Render pixel
    framebuffer[render_x] = fast_color_scaleAdd(framebuffer[render_x], color, pixel_brightness);
  }
}

// detect collisions in an array of particles and handle them
void ParticleSystem1D::handleCollisions() {
  uint32_t collisiondistance = particleHardRadius << 1; // twice the radius is min distance between colliding particles
  uint32_t checkDistSq = max(2 * PS_P_MAXSPEED, (int)collisiondistance);
  if (perParticleSize && advPartProps != nullptr) // using individual particle size
    checkDistSq = max(2 * PS_P_MAXSPEED, (512 * 52) >> 6); // max possible collision distance that catches all collisons
  checkDistSq = checkDistSq * checkDistSq; // square it for distance comparison (faster than abs() )
  // note: partices are binned by position, assumption is that no more than half of the particles are in the same bin
  // if they are, collisionStartIdx is increased so each particle collides at least every second frame (which still gives decent collisions)
  int binWidth = 64 * PS_P_RADIUS_1D; // width of each bin, a compromise between speed and accuracy
  int32_t overlap = collisiondistance + (2 * PS_P_MAXSPEED); // overlap bins to include edge particles to neighbouring bins (+ look-ahead of speed)
  if (perParticleSize && advPartProps != nullptr) //may be using individual particle size
    overlap = 512; // 2 * max radius, enough to catch all collisions even at full speed
  uint32_t maxBinParticles = max((uint32_t)50, (usedParticles + 1) / 4); // do not bin small amounts, limit max to 1/4 of particles
  uint32_t numBins = (maxX + (binWidth - 1)) / binWidth; // calculate number of bins
  if (usedParticles < maxBinParticles) {
    numBins = 1; // use single bin for small number of particles
    binWidth = maxX + 1;
  }
  uint16_t binIndices[maxBinParticles]; // array to store indices of particles in a bin
  uint32_t binParticleCount; // number of particles in the current bin
  uint32_t nextFrameStartIdx = hw_random16(usedParticles); // index of the first particle in the next frame (set to fixed value if bin overflow)
  uint32_t pidx = collisionStartIdx; //start index in case a bin is full, process remaining particles next frame
  for (uint32_t bin = 0; bin < numBins; bin++) {
    binParticleCount = 0; // reset for this bin
    int32_t binStart = bin * binWidth - overlap; // note: first bin will extend to negative, but that is ok as out of bounds particles are ignored
    int32_t binEnd = binStart + binWidth + (overlap << 1); // add twice the overlap as start is start-overlap, note: last bin can be out of bounds, see above

    // fill the binIndices array for this bin
    for (uint32_t i = 0; i < usedParticles; i++) {
      if (particles[pidx].ttl > 0) { // alivee
        if (particles[pidx].x >= binStart && particles[pidx].x <= binEnd) { // >= and <= to include particles on the edge of the bin (overlap to ensure boarder particles collide with adjacent bins)
          if (particleFlags[pidx].outofbounds == 0 && particleFlags[pidx].collide) { // particle is in frame and does collide note: checking flags is quite slow and usually these are set, so faster to check here
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
        int32_t dx = particles[idx_j].x - particles[idx_i].x; // distance between particles
        uint32_t dx_sq = dx * dx; // square distance (faster than abs() and works the same)
        if (dx_sq <= checkDistSq) { // possible collision imminent, check properly note: this is slower than using direct speed look-ahead (like in 2D) but more accurate and fast enough for 1D
          collideParticles(idx_i, idx_j, dx, collisiondistance); // handle the collision
        }
      }
    }
  }
  collisionStartIdx = nextFrameStartIdx; // set the start index for the next frame
}
// handle a collision if close proximity is detected, i.e. dx smaller than 2*radius + speed look-ahead
void WLED_O2_ATTR ParticleSystem1D::collideParticles(uint32_t partIdx1, uint32_t partIdx2, int32_t dx, uint32_t collisiondistance) {
  int32_t massratio1 = 0; // 0 means dont use mass ratio (equal mass)
  int32_t massratio2 = 0;
  if (perParticleSize && advPartProps != nullptr) { // use advanced size properties, calculate collision distance and mass ratio
    collisiondistance = (PS_P_MINHARDRADIUS_1D * 2) + ((((uint32_t)advPartProps[partIdx1].size + (uint32_t)advPartProps[partIdx2].size) * 52) >> 6); // collision distance, use 80% of size for tighter stacking (slight overlap)
    // calculate mass ratio for collision response
    uint32_t mass1 = PS_P_RADIUS_1D + advPartProps[partIdx1].size;
    uint32_t mass2 = PS_P_RADIUS_1D + advPartProps[partIdx2].size;
    uint32_t totalmass = mass1 + mass2 - 2; // -2 to account for rounding
    massratio1 = (mass2 << 8) / totalmass; // massratio 1 depends on mass of particle 2, i.e. if 2 is heavier -> higher velocity impact on 1
    massratio2 = (mass1 << 8) / totalmass;
  }
  int32_t dv = (int)particles[partIdx2].vx - (int)particles[partIdx1].vx;
  int32_t absdv = abs(dv);
  int32_t dotProduct = (dx * dv); // is always negative if moving towards each other
  uint32_t dx_abs = abs(dx);

  if (dotProduct < 0) { // particles are moving towards each other
    uint32_t lookaheadDistance = collisiondistance + absdv; // add look-ahead: if reaching collisiondistance in this frame, collide
    if (dx_abs <= lookaheadDistance) {
      // if one of the particles is fixed, invert the other particle's velocity and multiply by hardness, also set its position to the edge of the fixed particle
      if (particleFlags[partIdx1].fixed) {
        particles[partIdx2].vx = -(particles[partIdx2].vx * collisionHardness) / 255;
        particles[partIdx2].x = particles[partIdx1].x + (dx < 0 ? -collisiondistance : collisiondistance); // dv < 0 means particle2.x < particle1.x
        return;
      }
      else if (particleFlags[partIdx2].fixed) {
        particles[partIdx1].vx = -(particles[partIdx1].vx * collisionHardness) / 255;
        particles[partIdx1].x = particles[partIdx2].x + (dx < 0 ? collisiondistance : -collisiondistance);
        return;
      }
      int32_t surfacehardness = max(collisionHardness, (int32_t)PS_P_MINSURFACEHARDNESS_1D); // if particles are soft, the impulse must stay above a limit or collisions slip through
      // Calculate new velocities after collision  note: not using dot product like in 2D as impulse is purely speed depnedent
      #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP8266) // use bitshifts with rounding instead of division (2x faster)
      int32_t impulse = (dv * surfacehardness + ((dv >> 31) & 0xFF)) >> 8; // note: (v>>31) & 0xFF)) extracts the sign and adds 255 if negative for correct rounding using shifts
      #else // division is faster on ESP32, S2 and S3
      int32_t impulse = (dv * surfacehardness) / 255;
      #endif

      // if particles are not the same size, use a mass ratio. mass ratio is set to 0 if particles are the same size
      if (massratio1) {
        int vx1 = (int)particles[partIdx1].vx + ((impulse * massratio1) >> 7); // mass ratio is in fixed point 8bit
        int vx2 = (int)particles[partIdx2].vx - ((impulse * massratio2) >> 7);
        // limit speeds to max speed (required as a lot of impulse can be transferred from a large to a small particle)
        particles[partIdx1].vx = limitSpeed(vx1);
        particles[partIdx2].vx = limitSpeed(vx2);
      }
      else {
        particles[partIdx1].vx += impulse;
        particles[partIdx2].vx -= impulse;
      }

      if (collisionHardness < PS_P_MINSURFACEHARDNESS_1D && (SEGMENT.call & 0x07) == 0) { // if particles are soft, they become 'sticky' i.e. apply some friction
        const uint32_t coeff = collisionHardness + (250 - PS_P_MINSURFACEHARDNESS_1D);
        #if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(ESP8266) // use bitshifts with rounding instead of division (2x faster)
        particles[partIdx1].vx = ((int32_t)particles[partIdx1].vx * coeff + (((int32_t)particles[partIdx1].vx >> 31) & 0xFF)) >> 8; // note: (v>>31) & 0xFF)) extracts the sign and adds 255 if negative for correct rounding using shifts
        particles[partIdx2].vx = ((int32_t)particles[partIdx2].vx * coeff + (((int32_t)particles[partIdx2].vx >> 31) & 0xFF)) >> 8;
        #else // division is faster on ESP32, S2 and S3
        particles[partIdx1].vx = ((int32_t)particles[partIdx1].vx * coeff) / 255;
        particles[partIdx2].vx = ((int32_t)particles[partIdx2].vx * coeff) / 255;
        #endif
      }
    } else {
      return; // not close enough yet
    }
  }
  // particles have volume, push particles apart if they are too close
  // note: like in 2D, pushing by a distance makes softer piles collapse, giving particles speed prevents that and looks nicer

  if (dx_abs < collisiondistance) { // too close, force push particles so they dont collapse
    int32_t pushamount = 1 + ((collisiondistance - dx_abs) >> 3); // push by eighth of deviation (plus 1 to push at least a little), note: pushing too much leads to pass-throughs and more flickering
    int32_t addspeed = 1;
    if (dx < 0) {  // particle2.x < particle1.x
      pushamount = -pushamount;
      addspeed = -addspeed;
    }
    if (absdv < 4) { // low relative speed, add speed to help with the pushing (less collapsing piles)
      particles[partIdx1].vx -= addspeed;
      particles[partIdx2].vx += addspeed;
    }
    // push only one particle to avoid oscillations
    bool fairlyrandom = dotProduct & 0x01;
    if (fairlyrandom) {
      particles[partIdx1].x -= pushamount;
    }
    else {
      particles[partIdx2].x += pushamount;
    }
  }
}

// update size and pointers (memory location and size can change dynamically)
// note: do not access the PS class in FX befor running this function (or it messes up SEGENV.data)
void ParticleSystem1D::updateSystem(void) {
  setSize(SEGMENT.vLength()); // update size
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
  PSdataEnd = reinterpret_cast<uint8_t *>(sources + numSources);   // pointer to first available byte after the PS for FX additional data (already aligned to 4 byte boundary)
#ifndef WLED_DISABLE_2D
  if (SEGMENT.is2D() && SEGMENT.map1D2D) {
    framebuffer = reinterpret_cast<uint32_t *>(sources + numSources); // use local framebuffer for 1D->2D mapping
    PSdataEnd = reinterpret_cast<uint8_t *>(framebuffer + SEGMENT.maxMappingLength()); // pointer to first available byte after the PS for FX additional data (still aligned to 4 byte boundary)
  }
  else
#endif
    framebuffer = SEGMENT.getPixels();  // use segment buffer for standard 1D rendering

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
  uint32_t particlelimit = MAXPARTICLES_1D; // maximum number of paticles allowed
  numberofParticles = min(numberofParticles, particlelimit); // limit to particlelimit
  if (isadvanced) // advanced property array needs ram, reduce number of particles to use the same amount
    numberofParticles = (numberofParticles * sizeof(PSparticle1D)) / (sizeof(PSparticle1D) + sizeof(PSadvancedParticle1D));
  numberofParticles = (numberofParticles * (fraction + 1)) >> 8; // calculate fraction of particles
  numberofParticles = numberofParticles < 10 ? 10 : numberofParticles; // 10 minimum
  //make sure it is a multiple of 4 for proper memory alignment (easier than using padding bytes)
  numberofParticles = (numberofParticles+3) & ~0x03; // note: with a separate particle buffer, this is probably unnecessary
  PSPRINTLN(" calc numparticles:" + String(numberofParticles));
  return numberofParticles;
}

uint32_t calculateNumberOfSources1D(const uint32_t requestedsources) {
  int numberofSources = max(1, min((int)requestedsources,MAXSOURCES_1D)); // limit
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
#ifndef WLED_DISABLE_2D
  if (SEGMENT.is2D())
    requiredmemory += sizeof(uint32_t) * SEGMENT.maxMappingLength(); // need local buffer for mapped rendering
#endif
  requiredmemory += additionalbytes;
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
  bool allocsuccess = false;
  while(numparticles >= 10) { // make sure we have at least 10 particles or quit
    if (allocateParticleSystemMemory1D(numparticles, numsources, advanced, additionalbytes)) {
      PSPRINT(F("PS 1D alloc succeeded"));
      allocsuccess = true;
      break; // allocation succeeded
    }
    numparticles = ((numparticles / 2) + 3) & ~0x03; // cut number of particles in half and try again, must be 4 byte aligned
    PSPRINTLN(F("PS 1D alloc failed, trying with less particles..."));
  }
  if (!allocsuccess) {
    PSPRINTLN(F("PS init failed: memory depleted"));
    return false; // allocation failed
  }
  PartSys = new (SEGENV.data) ParticleSystem1D(SEGMENT.virtualLength(), numparticles, numsources, advanced); // particle system constructor
  return true;
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

// this is a fast version for RGB color adding ignoring white channel (PS does not handle white) including scaling of second color
// note: function is mainly used to add scaled colors, so checking if one color is black is slower
static uint32_t fast_color_scaleAdd(const uint32_t c1, const uint32_t c2, const uint8_t scale) {
    constexpr uint32_t MASK_RB = 0x00FF00FF;  // red and blue mask
    constexpr uint32_t MASK_G  = 0x0000FF00;  // green mask

    uint32_t rb = c2 & MASK_RB; // 0x00RR00BB
    uint32_t g  = c2 & MASK_G;  // 0x0000GG00
    // scale second color
    rb = ((rb * scale) >> 8) & MASK_RB;
    g  = ((g  * scale) >> 8) & MASK_G;
    // add colors
    rb = (c1 & MASK_RB) + rb;
    g = ((c1 & MASK_G)  + g);

    // check for overflow by looking at the 9th bit of each channel
    if ((rb | (g >> 8)) & 0x01000100) {
        // find max among the three 16-bit values
        g = g >> 8; // shift to get 0x000000GG
        uint32_t max_val = (rb >> 16); // red
        max_val = ((rb & 0xFFFF) > max_val) ? rb & 0xFFFF : max_val;  // blue
        max_val = (g > max_val) ? g : max_val; // green
        // scale down to avoid saturation
        uint32_t scale_factor = (255 << 8) / max_val;
        rb = ((rb * scale_factor) >> 8) & MASK_RB;
        g = (g * scale_factor) & MASK_G;
    }
    return rb | g;
}

#endif  // !(defined(WLED_DISABLE_PARTICLESYSTEM2D) && defined(WLED_DISABLE_PARTICLESYSTEM1D))
