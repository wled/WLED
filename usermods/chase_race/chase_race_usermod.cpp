#include "wled.h"

#ifdef USERMOD_CHASE_RACE

/*
 * Chase Race Usermod
 * ------------------
 * Registers a dedicated usermod so we can keep the custom "Chase Race" effect
 * separate from WLED core code. The usermod exists purely to register the
 * effect with strip.addEffect(); all race logic lives in mode_chase_race().
 *
 * Slider/Color mapping:
 *   Speed     -> Pace (advance interval)
 *   Intensity -> Car length (LEDs per car)
 *   Custom1   -> Gap size (blank pixels between cars)
 *   Colors    -> Car 1 / Car 2 / Car 3 body colors
 */

namespace {

// Metadata string that drives the UI and JSON description of the new effect.
// Layout:
//   Speed slider   -> "Pace"  (updates per second)
//   Intensity      -> "Car length"
//   Custom1        -> "Gap size"
//   Custom2        -> unused (reserved)
//   Colors         -> Car 1, Car 2, Car 3
static const char _data_FX_MODE_CHASE_RACE[] PROGMEM =
    "Chase Race@Pace,Car length,Gap size,,,;Car 1,Car 2,Car 3;;";

static uint16_t mode_chase_race(void) {
  const uint16_t segLen = SEGLEN;
  if (!segLen) return FRAMETIME;

  if (SEGENV.call == 0) {
    SEGENV.step = strip.now;
    SEGENV.aux0 = 0; // head position
  }

  const uint16_t minInterval = 10;
  const uint16_t maxInterval = 180;
  const uint16_t interval = maxInterval - ((maxInterval - minInterval) * SEGMENT.speed / 255);

  if (strip.now - SEGENV.step >= interval) {
    SEGENV.step = strip.now;
    SEGENV.aux0 = (SEGENV.aux0 + 1) % segLen;
  }

  // Ensure there is room for each car plus at least 1 blank LED when possible.
  uint16_t carLenMax = 1;
  if (segLen > 3) {
    carLenMax = max<uint16_t>(1, (segLen - 3) / 3);
  }
  carLenMax = max<uint16_t>(1, min<uint16_t>(carLenMax, segLen)); // safety for tiny segments

  uint16_t carLen = map(SEGMENT.intensity, 0, 255, 1, carLenMax);
  carLen = max<uint16_t>(1, min<uint16_t>(carLen, carLenMax));

  uint16_t maxGap = 0;
  if (segLen > carLen * 3) {
    maxGap = (segLen - (carLen * 3)) / 3;
  }

  uint16_t desiredGap = map(SEGMENT.custom1, 0, 255, 1, max<uint16_t>(1, segLen / 3));
  uint16_t gapLen = (maxGap == 0)
                      ? 0
                      : max<uint16_t>(1, min<uint16_t>(desiredGap, maxGap));

  const uint16_t spacing = carLen + gapLen;
  const uint16_t origin = SEGENV.aux0 % segLen;

  SEGMENT.fill(BLACK);

  auto drawCar = [&](uint16_t start, uint32_t color) {
    if (!color) return;
    for (uint16_t i = 0; i < carLen && i < segLen; i++) {
      uint16_t idx = (start + i) % segLen;
      SEGMENT.setPixelColor(idx, color);
    }
  };

  // SEGCOLOR indices are reversed relative to the UI (color slot 1 -> index 2)
  drawCar(origin, SEGCOLOR(2));                            // Car 1 color slot
  drawCar((origin + spacing) % segLen, SEGCOLOR(1));       // Car 2
  drawCar((origin + (2 * spacing)) % segLen, SEGCOLOR(0)); // Car 3

  return FRAMETIME;
}

} // namespace

class ChaseRaceUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_chase_race, _data_FX_MODE_CHASE_RACE);
  }

  void loop() override {}

  uint16_t getId() override { return USERMOD_ID_CHASE_RACE; }

};

static ChaseRaceUsermod chaseRace;
REGISTER_USERMOD(chaseRace);

#endif  // USERMOD_CHASE_RACE
