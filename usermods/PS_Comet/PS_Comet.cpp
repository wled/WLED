#include "wled.h"
#include "FXparticleSystem.h"

unsigned long nextCometCreationTime = 0;

static bool has_particle_fallen_off_screen(const ParticleSystem2D* PartSys, const PSparticle& particle) {
  return particle.y < PartSys->maxY * -1;
}

static bool has_fallen_off_screen(const ParticleSystem2D* PartSys, uint32_t particleIndex) {
  return has_particle_fallen_off_screen(PartSys, PartSys->sources[particleIndex].source);
}

static uint16_t mode_static(void) {
  SEGMENT.fill(SEGCOLOR(0));
  return strip.isOffRefreshRequired() ? FRAMETIME : 350;
}

///////////////////////
//  Effect Function  //
///////////////////////

uint16_t mode_pscomet(void) {
  ParticleSystem2D *PartSys = nullptr;
  uint32_t i;
  // Allocate three comets for every column to allow for large comets with side emitters
  uint32_t numComets = SEGMENT.vWidth() * 3;

  if (SEGMENT.call == 0) { // Initialization
    if (!initParticleSystem2D(PartSys, numComets)) {
      return mode_static(); // Allocation failed or not 2D
    }
    PartSys->setMotionBlur(170); // Enable motion blur
    PartSys->setParticleSize(0); // Allow small comets to be a single pixel wide
  }
  else {
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // If not first call, use existing data
  }
  if (PartSys == nullptr) {
    return mode_static(); // Something went wrong, no data!
  }

  PartSys->updateSystem(); // Update system properties (dimensions and data pointers)

  uint32_t sideCometVerticalShift = 2 * PartSys->maxY / (SEGMENT.vHeight() - 1); // Shift emitters on left and right of comet up by 1 pixel
  uint32_t chosenIndex = hw_random(numComets); // Only allow one column to spawn a comet each frame
  uint8_t fallingSpeed = 1 + (SEGMENT.speed >> 2);
  uint16_t cometFrequencyDelay = 2040 - (SEGMENT.intensity << 3);

  // Update the comets
  for (i = 0; i < numComets; i++) {
    auto& source = PartSys->sources[i];
    auto& sourceParticle = source.source;
    // Large comets use three particle sources, with i indicating left, center or right:
    // 0: left, 1: center, 2: right, 3: left, 4: center, 5: right, ...
    // Small comets leave the left and right indices unused
    bool isCometCenter = (i % 3) == 1;

    // Map the 3x comet index into an output pixel index
    sourceParticle.x = ((i / 3) + (i % 3)) * PartSys->maxX / (SEGMENT.vWidth() - 1);
    if (isCometCenter) {
      // Slider 4 controls comet length via particle lifetime and fire intensity adjustments
      source.maxLife = 16 + (SEGMENT.custom2 >> 2);
      sourceParticle.ttl = 16 - (SEGMENT.custom2 >> 4);
    } else {
      source.maxLife = 16;
      sourceParticle.ttl = 16;
    }
    source.minLife = source.maxLife >> 1;
    source.vy = 1 + (SEGMENT.speed >> 5); // Emitting speed (upwards)

    bool hasFallenOffScreen = has_fallen_off_screen(PartSys, i);

    if (!hasFallenOffScreen) {
      // Active comets fall downwards and emit flames
      sourceParticle.y -= fallingSpeed;
      PartSys->flameEmit(PartSys->sources[i]);
    }

    // Inactive comets have a random chance to respawn at the top
    if (hasFallenOffScreen
      && strip.now > nextCometCreationTime
      && i == chosenIndex
      && isCometCenter
      // Ensure there are no comets too close to the left
      && (i < 3 || has_fallen_off_screen(PartSys, i - 3))
      // And to the right
      && (i > numComets - 4 || has_fallen_off_screen(PartSys, i + 3))
    ) {
      // Spawn a bit above the top to avoid popping into view
      sourceParticle.y = PartSys->maxY + (2 * fallingSpeed);
      // Slider 3 determines % of large comets with extra particle sources on their sides
      if (hw_random8(254) < SEGMENT.custom1) {
        PartSys->sources[i - 1].source.y = PartSys->maxY + sideCometVerticalShift; // left comet
        PartSys->sources[i + 1].source.y = PartSys->maxY + sideCometVerticalShift; // right comet
      }
      nextCometCreationTime = strip.now + cometFrequencyDelay + hw_random16(cometFrequencyDelay);
    }
  }

  // To avoid stretching comets at faster falling speeds, we need to shift the emitted particles as well
  for (i = 0; i < PartSys->usedParticles; i++) {
    auto& particle = PartSys->particles[i];
    if (!has_particle_fallen_off_screen(PartSys, particle)) {
      particle.y -= fallingSpeed;
    }
  }

  // Slider 4 controls comet length via particle lifetime and fire intensity adjustments
  PartSys->updateFire(max(255 - SEGMENT.custom2, 45), false);

  return FRAMETIME;
}
static const char _data_FX_MODE_PSCOMET[] PROGMEM = "PS Comet@Falling Speed,Comet Frequency,Large Comet Probability,Comet Length;;!;2;pal=35,sx=128,ix=255,c1=32,c2=128";

/////////////////////
//  UserMod Class  //
/////////////////////

class PSCometUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_pscomet, _data_FX_MODE_PSCOMET);
  }

  void loop() override {}
};

static PSCometUsermod ps_comet;
REGISTER_USERMOD(ps_comet);
