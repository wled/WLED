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

#include <stdint.h>
#include "wled.h"

#define PS_P_MAXSPEED 120 // maximum speed a particle can have (vx/vy is int8)
#define MAX_MEMIDLE 10 // max idle time (in frames) before memory is deallocated (if deallocated during an effect, it will crash!)

//#define WLED_DEBUG_PS // note: enabling debug uses ~3k of flash

#ifdef WLED_DEBUG_PS
  #define PSPRINT(x) Serial.print(x)
  #define PSPRINTLN(x) Serial.println(x)
#else
  #define PSPRINT(x)
  #define PSPRINTLN(x)
#endif

// limit speed of particles (used in 1D and 2D)
static inline int32_t limitSpeed(const int32_t speed) {
  return speed > PS_P_MAXSPEED ? PS_P_MAXSPEED : (speed < -PS_P_MAXSPEED ? -PS_P_MAXSPEED : speed); // note: this is slightly faster than using min/max at the cost of 50bytes of flash
}
#endif

#ifndef WLED_DISABLE_PARTICLESYSTEM2D
// memory allocation
#define ESP8266_MAXPARTICLES 256 // enough up to 16x16 pixels
#define ESP8266_MAXSOURCES 24
#define ESP32S2_MAXPARTICLES 1024 // enough up to 32x32 pixels
#define ESP32S2_MAXSOURCES 64
#define ESP32_MAXPARTICLES 2048 // enough up to 64x32 pixels
#define ESP32_MAXSOURCES 128

// particle dimensions (subpixel division)
#define PS_P_RADIUS 64 // subpixel size, each pixel is divided by this for particle movement (must be a power of 2)
#define PS_P_HALFRADIUS (PS_P_RADIUS >> 1)
#define PS_P_RADIUS_SHIFT 6 // shift for RADIUS
#define PS_P_SURFACE 12 // shift: 2^PS_P_SURFACE = (PS_P_RADIUS)^2
#define PS_P_MINHARDRADIUS 64 // minimum hard surface radius for collisions
#define PS_P_MINSURFACEHARDNESS 128 // minimum hardness used in collision impulse calculation, below this hardness, particles become sticky

// struct for PS settings (shared for 1D and 2D class)
typedef union {
  struct{ // one byte bit field for 2D settings
    bool wrapX : 1;
    bool wrapY : 1;
    bool bounceX : 1;
    bool bounceY : 1;
    bool killoutofbounds : 1; // if set, out of bound particles are killed immediately
    bool useGravity : 1; // set to 1 if gravity is used, disables bounceY at the top
    bool useCollisions : 1;
    bool colorByAge : 1; // if set, particle hue is set by ttl value in render function
  };
  byte asByte; // access as a byte, order is: LSB is first entry in the list above
} PSsettings2D;

//struct for a single particle
typedef struct { // 10 bytes
  int16_t x;  // x position in particle system
  int16_t y;  // y position in particle system
  uint16_t ttl; // time to live in frames
  int8_t vx;  // horizontal velocity
  int8_t vy;  // vertical velocity
  uint8_t hue;  // color hue
  uint8_t sat; // particle color saturation
} PSparticle;

//struct for particle flags note: this is separate from the particle struct to save memory (ram alignment)
typedef union {
  struct { // 1 byte
    bool outofbounds : 1; // out of bounds flag, set to true if particle is outside of display area
    bool collide : 1; // if set, particle takes part in collisions
    bool perpetual : 1; // if set, particle does not age (TTL is not decremented in move function, it still dies from killoutofbounds)
    bool custom1 : 1; // unused custom flags, can be used by FX to track particle states
    bool custom2 : 1;
    bool custom3 : 1;
    bool custom4 : 1;
    bool custom5 : 1;
  };
  byte asByte; // access as a byte, order is: LSB is first entry in the list above
} PSparticleFlags;

// struct for additional particle settings (option)
typedef struct { // 2 bytes
  uint8_t size; // particle size, 255 means 10 pixels in diameter, 0  means use global size (including single pixel rendering)
  uint8_t forcecounter; // counter for applying forces to individual particles
} PSadvancedParticle;

// struct for advanced particle size control (option)
typedef struct { // 8 bytes
  uint8_t asymmetry; // asymmetrical size (0=symmetrical, 255 fully asymmetric)
  uint8_t asymdir; // direction of asymmetry, 64 is x, 192 is y (0 and 128 is symmetrical)
  uint8_t maxsize; // target size for growing
  uint8_t minsize; // target size for shrinking
  uint8_t sizecounter : 4; // counters used for size contol (grow/shrink/wobble)
  uint8_t wobblecounter : 4;
  uint8_t growspeed : 4;
  uint8_t shrinkspeed : 4;
  uint8_t wobblespeed : 4;
  bool grow : 1; // flags
  bool shrink : 1;
  bool pulsate : 1; // grows & shrinks & grows & ...
  bool wobble : 1; // alternate x and y size
} PSsizeControl;


//struct for a particle source (20 bytes)
typedef struct {
  uint16_t minLife; // minimum ttl of emittet particles
  uint16_t maxLife; // maximum ttl of emitted particles
  PSparticle source; // use a particle as the emitter source (speed, position, color)
  PSparticleFlags sourceFlags; // flags for the source particle
  int8_t var; // variation of emitted speed (adds random(+/- var) to speed)
  int8_t vx; // emitting speed
  int8_t vy;
  uint8_t size; // particle size (advanced property), global size is added on top to this size
} PSsource;

// class uses approximately 60 bytes
class ParticleSystem2D {
public:
  ParticleSystem2D(const uint32_t width, const uint32_t height, const uint32_t numberofparticles, const uint32_t numberofsources, const bool isadvanced = false,  const bool sizecontrol = false); // constructor
  // note: memory is allcated in the FX function, no deconstructor needed
  void update(void); //update the particles according to set options and render to the matrix
  void updateFire(const uint8_t intensity, const bool renderonly); // update function for fire, if renderonly is set, particles are not updated (required to fix transitions with frameskips)
  void updateSystem(void); // call at the beginning of every FX, updates pointers and dimensions
  void particleMoveUpdate(PSparticle &part, PSparticleFlags &partFlags, PSsettings2D *options = NULL, PSadvancedParticle *advancedproperties = NULL); // move function
  // particle emitters
  int32_t sprayEmit(const PSsource &emitter);
  void flameEmit(const PSsource &emitter);
  int32_t angleEmit(PSsource& emitter, const uint16_t angle, const int32_t speed);
  //particle physics
  void applyGravity(PSparticle &part); // applies gravity to single particle (use this for sources)
  [[gnu::hot]] void applyForce(PSparticle &part, const int8_t xforce, const int8_t yforce, uint8_t &counter);
  [[gnu::hot]] void applyForce(const uint32_t particleindex, const int8_t xforce, const int8_t yforce); // use this for advanced property particles
  void applyForce(const int8_t xforce, const int8_t yforce); // apply a force to all particles
  void applyAngleForce(PSparticle &part, const int8_t force, const uint16_t angle, uint8_t &counter);
  void applyAngleForce(const uint32_t particleindex, const int8_t force, const uint16_t angle); // use this for advanced property particles
  void applyAngleForce(const int8_t force, const uint16_t angle); // apply angular force to all particles
  void applyFriction(PSparticle &part, const int32_t coefficient); // apply friction to specific particle
  void applyFriction(const int32_t coefficient); // apply friction to all used particles
  void pointAttractor(const uint32_t particleindex, PSparticle &attractor, const uint8_t strength, const bool swallow);
  // set options  note: inlining the set function uses more flash so dont optimize
  void setUsedParticles(const uint8_t percentage);  // set the percentage of particles used in the system, 255=100%
  void setCollisionHardness(const uint8_t hardness); // hardness for particle collisions (255 means full hard)
  void setWallHardness(const uint8_t hardness); // hardness for bouncing on the wall if bounceXY is set
  void setWallRoughness(const uint8_t roughness); // wall roughness randomizes wall collisions
  void setMatrixSize(const uint32_t x, const uint32_t y);
  void setWrapX(const bool enable);
  void setWrapY(const bool enable);
  void setBounceX(const bool enable);
  void setBounceY(const bool enable);
  void setKillOutOfBounds(const bool enable); // if enabled, particles outside of matrix instantly die
  void setSaturation(const uint8_t sat); // set global color saturation
  void setColorByAge(const bool enable);
  void setMotionBlur(const uint8_t bluramount); // note: motion blur can only be used if 'particlesize' is set to zero
  void setSmearBlur(const uint8_t bluramount); // enable 2D smeared blurring of full frame
  void setParticleSize(const uint8_t size);
  void setGravity(const int8_t force = 8);
  void enableParticleCollisions(const bool enable, const uint8_t hardness = 255);

  PSparticle *particles; // pointer to particle array
  PSparticleFlags *particleFlags; // pointer to particle flags array
  PSsource *sources; // pointer to sources
  PSadvancedParticle *advPartProps; // pointer to advanced particle properties (can be NULL)
  PSsizeControl *advPartSize; // pointer to advanced particle size control (can be NULL)
  uint8_t* PSdataEnd; // points to first available byte after the PSmemory, is set in setPointers(). use this for FX custom data
  int32_t maxX, maxY; // particle system size i.e. width-1 / height-1 in subpixels, Note: all "max" variables must be signed to compare to coordinates (which are signed)
  int32_t maxXpixel, maxYpixel; // last physical pixel that can be drawn to (FX can read this to read segment size if required), equal to width-1 / height-1
  uint32_t numSources; // number of sources
  uint32_t usedParticles; // number of particles used in animation, is relative to 'numParticles'
  //note: some variables are 32bit for speed and code size at the cost of ram

private:
  //rendering functions
  void render();
  [[gnu::hot]] void renderParticle(const uint32_t particleindex, const uint8_t brightness, const CRGB& color, const bool wrapX, const bool wrapY);
  //paricle physics applied by system if flags are set
  void applyGravity(); // applies gravity to all particles
  void handleCollisions();
  [[gnu::hot]] void collideParticles(PSparticle &particle1, PSparticle &particle2, const int32_t dx, const int32_t dy, const uint32_t collDistSq);
  void fireParticleupdate();
  //utility functions
  void updatePSpointers(const bool isadvanced, const bool sizecontrol); // update the data pointers to current segment data space
  bool updateSize(PSadvancedParticle *advprops, PSsizeControl *advsize); // advanced size control
  void getParticleXYsize(PSadvancedParticle *advprops, PSsizeControl *advsize, uint32_t &xsize, uint32_t &ysize);
  [[gnu::hot]] void bounce(int8_t &incomingspeed, int8_t &parallelspeed, int32_t &position, const uint32_t maxposition); // bounce on a wall
  // note: variables that are accessed often are 32bit for speed
  CRGB *framebuffer; // local frame buffer for rendering
  PSsettings2D particlesettings; // settings used when updating particles (can also used by FX to move sources), do not edit properties directly, use functions above
  uint32_t numParticles;  // total number of particles allocated by this system
  uint32_t emitIndex; // index to count through particles to emit so searching for dead pixels is faster
  int32_t collisionHardness;
  uint32_t wallHardness;
  uint32_t wallRoughness; // randomizes wall collisions  
  uint32_t particleHardRadius; // hard surface radius of a particle, used for collision detection (32bit for speed)
  uint16_t collisionStartIdx; // particle array start index for collision detection
  uint8_t fireIntesity = 0; // fire intensity, used for fire mode (flash use optimization, better than passing an argument to render function)
  uint8_t forcecounter; // counter for globally applied forces
  uint8_t gforcecounter; // counter for global gravity
  int8_t gforce; // gravity strength, default is 8 (negative is allowed, positive is downwards)
  // global particle properties for basic particles
  uint8_t particlesize; // global particle size, 0 = 1 pixel, 1 = 2 pixels, 255 = 10 pixels (note: this is also added to individual sized particles, set to 0 or 1 for standard advanced particle rendering)
  uint8_t motionBlur; // motion blur, values > 100 gives smoother animations. Note: motion blurring does not work if particlesize is > 0
  uint8_t smearBlur; // 2D smeared blurring of full frame
};

void blur2D(CRGB *colorbuffer, const uint32_t xsize, uint32_t ysize, const uint32_t xblur, const uint32_t yblur, const uint32_t xstart = 0, uint32_t ystart = 0, const bool isparticle = false);
// initialization functions (not part of class)
bool initParticleSystem2D(ParticleSystem2D *&PartSys, const uint32_t requestedsources, const uint32_t additionalbytes = 0, const bool advanced = false, const bool sizecontrol = false);
uint32_t calculateNumberOfParticles2D(const uint32_t pixels, const bool advanced, const bool sizecontrol);
uint32_t calculateNumberOfSources2D(const uint32_t pixels, const uint32_t requestedsources);
bool allocateParticleSystemMemory2D(const uint32_t numparticles, const uint32_t numsources, const bool advanced, const bool sizecontrol, const uint32_t additionalbytes);
#endif // WLED_DISABLE_PARTICLESYSTEM2D

////////////////////////
// 1D Particle System //
////////////////////////
#ifndef WLED_DISABLE_PARTICLESYSTEM1D
// memory allocation
#define ESP8266_MAXPARTICLES_1D 320
#define ESP8266_MAXSOURCES_1D 16
#define ESP32S2_MAXPARTICLES_1D 1300
#define ESP32S2_MAXSOURCES_1D 32
#define ESP32_MAXPARTICLES_1D 2600
#define ESP32_MAXSOURCES_1D 64

// particle dimensions (subpixel division)
#define PS_P_RADIUS_1D 32 // subpixel size, each pixel is divided by this for particle movement, if this value is changed, also change the shift defines (next two lines)
#define PS_P_HALFRADIUS_1D (PS_P_RADIUS_1D >> 1)
#define PS_P_RADIUS_SHIFT_1D 5 // 1 << PS_P_RADIUS_SHIFT = PS_P_RADIUS
#define PS_P_SURFACE_1D 5 // shift: 2^PS_P_SURFACE = PS_P_RADIUS_1D
#define PS_P_MINHARDRADIUS_1D 32 // minimum hard surface radius note: do not change or hourglass effect will be broken
#define PS_P_MINSURFACEHARDNESS_1D 120 // minimum hardness used in collision impulse calculation

// struct for PS settings (shared for 1D and 2D class)
typedef union {
  struct{
  // one byte bit field for 1D settings
  bool wrap : 1;
  bool bounce : 1;
  bool killoutofbounds : 1; // if set, out of bound particles are killed immediately
  bool useGravity : 1; // set to 1 if gravity is used, disables bounceY at the top
  bool useCollisions : 1;
  bool colorByAge : 1; // if set, particle hue is set by ttl value in render function
  bool colorByPosition : 1; // if set, particle hue is set by its position in the strip segment
  bool unused : 1;
  };
  byte asByte; // access as a byte, order is: LSB is first entry in the list above
} PSsettings1D;

//struct for a single particle (8 bytes)
typedef struct {
  int32_t x;  // x position in particle system
  uint16_t ttl; // time to live in frames
  int8_t vx;  // horizontal velocity
  uint8_t hue;  // color hue
} PSparticle1D;

//struct for particle flags
typedef union {
  struct { // 1 byte
    bool outofbounds : 1; // out of bounds flag, set to true if particle is outside of display area
    bool collide : 1; // if set, particle takes part in collisions
    bool perpetual : 1; // if set, particle does not age (TTL is not decremented in move function, it still dies from killoutofbounds)
    bool reversegrav : 1; // if set, gravity is reversed on this particle
    bool forcedirection : 1; // direction the force was applied, 1 is positive x-direction (used for collision stacking, similar to reversegrav) TODO: not used anymore, can be removed
    bool fixed : 1; // if set, particle does not move (and collisions make other particles revert direction),
    bool custom1 : 1; // unused custom flags, can be used by FX to track particle states
    bool custom2 : 1;
  };
  byte asByte; // access as a byte, order is: LSB is first entry in the list above
} PSparticleFlags1D;

// struct for additional particle settings (optional)
typedef struct {
  uint8_t sat; //color saturation
  uint8_t size; // particle size, 255 means 10 pixels in diameter, this overrides global size setting
  uint8_t forcecounter;
} PSadvancedParticle1D;

//struct for a particle source (20 bytes)
typedef struct {
  uint16_t minLife; // minimum ttl of emittet particles
  uint16_t maxLife; // maximum ttl of emitted particles
  PSparticle1D source; // use a particle as the emitter source (speed, position, color)
  PSparticleFlags1D sourceFlags; // flags for the source particle
  int8_t var; // variation of emitted speed (adds random(+/- var) to speed)
  int8_t v; // emitting speed
  uint8_t sat; // color saturation (advanced property)
  uint8_t size; // particle size (advanced property)
  // note: there is 3 bytes of padding added here
} PSsource1D;

class ParticleSystem1D
{
public:
  ParticleSystem1D(const uint32_t length, const uint32_t numberofparticles, const uint32_t numberofsources, const bool isadvanced = false); // constructor
  // note: memory is allcated in the FX function, no deconstructor needed
  void update(void); //update the particles according to set options and render to the matrix
  void updateSystem(void); // call at the beginning of every FX, updates pointers and dimensions
  // particle emitters
  int32_t sprayEmit(const PSsource1D &emitter);
  void particleMoveUpdate(PSparticle1D &part, PSparticleFlags1D &partFlags, PSsettings1D *options = NULL, PSadvancedParticle1D *advancedproperties = NULL); // move function
  //particle physics
  [[gnu::hot]] void applyForce(PSparticle1D &part, const int8_t xforce, uint8_t &counter); //apply a force to a single particle
  void applyForce(const int8_t xforce); // apply a force to all particles
  void applyGravity(PSparticle1D &part, PSparticleFlags1D &partFlags); // applies gravity to single particle (use this for sources)
  void applyFriction(const int32_t coefficient); // apply friction to all used particles
  // set options
  void setUsedParticles(const uint8_t percentage); // set the percentage of particles used in the system, 255=100%
  void setWallHardness(const uint8_t hardness); // hardness for bouncing on the wall if bounceXY is set
  void setSize(const uint32_t x); //set particle system size (= strip length)
  void setWrap(const bool enable);
  void setBounce(const bool enable);
  void setKillOutOfBounds(const bool enable); // if enabled, particles outside of matrix instantly die
 // void setSaturation(uint8_t sat); // set global color saturation
  void setColorByAge(const bool enable);
  void setColorByPosition(const bool enable);
  void setMotionBlur(const uint8_t bluramount); // note: motion blur can only be used if 'particlesize' is set to zero
  void setSmearBlur(const uint8_t bluramount); // enable 1D smeared blurring of full frame
  void setParticleSize(const uint8_t size); //size 0 = 1 pixel, size 1 = 2 pixels, is overruled if advanced particle is used
  void setGravity(int8_t force = 8);
  void enableParticleCollisions(bool enable, const uint8_t hardness = 255);

  PSparticle1D *particles; // pointer to particle array
  PSparticleFlags1D *particleFlags; // pointer to particle flags array
  PSsource1D *sources; // pointer to sources
  PSadvancedParticle1D *advPartProps; // pointer to advanced particle properties (can be NULL)
  //PSsizeControl *advPartSize; // pointer to advanced particle size control (can be NULL)
  uint8_t* PSdataEnd; // points to first available byte after the PSmemory, is set in setPointers(). use this for FX custom data
  int32_t maxX; // particle system size i.e. width-1, Note: all "max" variables must be signed to compare to coordinates (which are signed)
  int32_t maxXpixel; // last physical pixel that can be drawn to (FX can read this to read segment size if required), equal to width-1
  uint32_t numSources; // number of sources
  uint32_t usedParticles; // number of particles used in animation, is relative to 'numParticles'

private:
  //rendering functions
  void render(void);
  [[gnu::hot]] void renderParticle(const uint32_t particleindex, const uint8_t brightness, const CRGB &color, const bool wrap);

  //paricle physics applied by system if flags are set
  void applyGravity(); // applies gravity to all particles
  void handleCollisions();
  [[gnu::hot]] void collideParticles(PSparticle1D &particle1, const PSparticleFlags1D &particle1flags, PSparticle1D &particle2, const PSparticleFlags1D &particle2flags, const int32_t dx, const uint32_t dx_abs, const uint32_t collisiondistance);

  //utility functions
  void updatePSpointers(const bool isadvanced); // update the data pointers to current segment data space
  //void updateSize(PSadvancedParticle *advprops, PSsizeControl *advsize); // advanced size control
  [[gnu::hot]] void bounce(int8_t &incomingspeed, int8_t &parallelspeed, int32_t &position, const uint32_t maxposition); // bounce on a wall
  // note: variables that are accessed often are 32bit for speed
  #ifndef ESP8266
  CRGB *framebuffer; // local frame buffer for rendering
  #endif
  PSsettings1D particlesettings; // settings used when updating particles
  uint32_t numParticles;  // total number of particles allocated by this system
  uint32_t emitIndex; // index to count through particles to emit so searching for dead pixels is faster
  int32_t collisionHardness;
  uint32_t particleHardRadius; // hard surface radius of a particle, used for collision detection
  uint32_t wallHardness;
  uint8_t gforcecounter; // counter for global gravity
  int8_t gforce; // gravity strength, default is 8 (negative is allowed, positive is downwards)
  uint8_t forcecounter; // counter for globally applied forces
  uint16_t collisionStartIdx; // particle array start index for collision detection
  //global particle properties for basic particles
  uint8_t particlesize; // global particle size, 0 = 1 pixel, 1 = 2 pixels, is overruled by advanced particle size
  uint8_t motionBlur; // enable motion blur, values > 100 gives smoother animations
  uint8_t smearBlur; // smeared blurring of full frame
};

bool initParticleSystem1D(ParticleSystem1D *&PartSys, const uint32_t requestedsources, const uint8_t fractionofparticles = 255, const uint32_t additionalbytes = 0, const bool advanced = false);
uint32_t calculateNumberOfParticles1D(const uint32_t fraction, const bool isadvanced);
uint32_t calculateNumberOfSources1D(const uint32_t requestedsources);
bool allocateParticleSystemMemory1D(const uint32_t numparticles, const uint32_t numsources, const bool isadvanced, const uint32_t additionalbytes);
void blur1D(CRGB *colorbuffer, uint32_t size, uint32_t blur, uint32_t start);
#endif // WLED_DISABLE_PARTICLESYSTEM1D
