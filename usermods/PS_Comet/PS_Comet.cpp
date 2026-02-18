#include "wled.h"
#include "FXparticleSystem.h"

unsigned long nextCometCreationTime = 0;

#define FX_FALLBACK_STATIC { SEGMENT.fill(SEGCOLOR(0)); return; }
// Use UINT32_MAX - 1 for the "no comet" case so we can add 1 later and not have it overflow
#define NULL_INDEX UINT32_MAX - 1

///////////////////////
//  Effect Function  //
///////////////////////

void mode_pscomet() {
  ParticleSystem2D *PartSys = nullptr;
  uint32_t i;

  if (SEGMENT.call == 0) { // Initialization
    // Try to allocate one comet for every column
    if (!initParticleSystem2D(PartSys, SEGMENT.vWidth())) {
      FX_FALLBACK_STATIC; // Allocation failed or not 2D
    }
    PartSys->setMotionBlur(170); // Enable motion blur
    PartSys->setParticleSize(0); // Allow small comets to be a single pixel wide
  }
  else {
    PartSys = reinterpret_cast<ParticleSystem2D *>(SEGENV.data); // If not first call, use existing data
  }
  if (PartSys == nullptr || SEGMENT.vHeight() < 2 || SEGMENT.vWidth() < 2) {
    FX_FALLBACK_STATIC;
  }

  PartSys->updateSystem(); // Update system properties (dimensions and data pointers)

  auto has_fallen_off_screen = [PartSys](uint32_t particleIndex) {
    return particleIndex < PartSys->numSources
      ? PartSys->sources[particleIndex].source.y < PartSys->maxY * -1
      : true;
  };

  // This will be SEGMENT.vWidth() unless the particle system had insufficient memory
  uint32_t numComets = PartSys->numSources;
  // Pick a random column for a new comet to spawn, but reset it to null if it's not time yet or there's already a
  // comet nearby
  uint32_t chosenIndex = hw_random(numComets);
  if (
    strip.now < nextCometCreationTime
    || !has_fallen_off_screen(chosenIndex - 1)
    || !has_fallen_off_screen(chosenIndex)
    || !has_fallen_off_screen(chosenIndex + 1)
  ) {
    chosenIndex = NULL_INDEX;
  } else {
    uint16_t cometFrequencyDelay = 2040 - (SEGMENT.intensity << 3);
    nextCometCreationTime = strip.now + cometFrequencyDelay + hw_random16(cometFrequencyDelay);
  }
  uint8_t canLargeCometSpawn =
    // Slider 3 determines % of large comets with extra particle sources on their sides
    SEGMENT.custom1 > hw_random8(254)
    && chosenIndex != 0
    && chosenIndex != numComets - 1;
  uint8_t fallingSpeed = 1 + (SEGMENT.speed >> 2);

  // Update the comets
  for (i = 0; i < numComets; i++) {
    auto& source = PartSys->sources[i];
    auto& sourceParticle = source.source;

    if (!has_fallen_off_screen(i)) {
      // Active comets fall downwards and emit flames
      sourceParticle.y -= fallingSpeed;
      source.vy = (SEGMENT.speed >> 5) - fallingSpeed; // Emitting speed (upwards)
      PartSys->flameEmit(PartSys->sources[i]);
      continue;
    }

    bool isChosenComet = i == chosenIndex;
    bool isChosenSideComet =
      canLargeCometSpawn &&
      (i == chosenIndex - 1 || i == chosenIndex + 1);

    // Chosen comets respawn at the top
    if (isChosenComet || isChosenSideComet) {
      // Map the comet index into an output pixel index
      sourceParticle.x = i * PartSys->maxX / (SEGMENT.vWidth() - 1);
      // Spawn a bit above the top to avoid popping into view
      sourceParticle.y = PartSys->maxY + (2 * fallingSpeed);
      if (isChosenComet) {
        // Slider 4 controls comet length via particle lifetime and fire intensity adjustments
        source.maxLife = 16 + (SEGMENT.custom2 >> 2);
        source.minLife = source.maxLife >> 1;
        sourceParticle.ttl = 16 - (SEGMENT.custom2 >> 4);
      } else {
        // Side comets have fixed length
        source.maxLife = 18;
        source.minLife = 14;
        sourceParticle.ttl = 16;
        // Shift side comets up by 1 pixel
        sourceParticle.y += 2 * PartSys->maxY / (SEGMENT.vHeight() - 1);
      }
    }
  }

  // Slider 4 controls comet length via particle lifetime and fire intensity adjustments
  PartSys->updateFire(max(255U - SEGMENT.custom2, 45U));
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
