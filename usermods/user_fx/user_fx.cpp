#include "user_fx.h"

#include <cstdint>

#define XY(_x, _y) (_x + SEG_W * (_y))

static uint16_t mode_static(void) {
  SEGMENT.fill(SEGCOLOR(0));
  return strip.isOffRefreshRequired() ? FRAMETIME : 350;
}

static uint16_t mode_diffusionfire(void) {
  static uint32_t call = 0;

  if (!strip.isMatrix || !SEGMENT.is2D())
    return mode_static();  // not a 2D set-up

  const int cols = SEG_W;
  const int rows = SEG_H;

  const uint8_t refresh_hz = map(SEGMENT.speed, 0, 255, 20, 80);
  const unsigned refresh_ms = 1000 / refresh_hz;
  const int16_t diffusion = map(SEGMENT.custom1, 0, 255, 0, 100);
  const uint8_t spark_rate = SEGMENT.intensity;
  const uint8_t turbulence = SEGMENT.custom2;

  unsigned dataSize = SEGMENT.length() + (cols * sizeof(uint16_t));
  if (!SEGENV.allocateData(dataSize))
    return mode_static();  // allocation failed

  if (SEGENV.call == 0) {
    SEGMENT.fill(BLACK);
    SEGENV.step = 0;
    call = 0;
  }

  if ((strip.now - SEGENV.step) >= refresh_ms) {
    // Reserve one extra byte and align to 2-byte boundary to avoid hard-faults
    uint8_t *bufStart = SEGMENT.data + SEGMENT.length();
    uintptr_t aligned = (uintptr_t(bufStart) + 1u) & ~uintptr_t(0x1u);
    uint16_t *tmp_row = reinterpret_cast<uint16_t *>(aligned);
    SEGENV.step = strip.now;
    call++;

    // scroll up
    for (unsigned y = 1; y < rows; y++)
      for (unsigned x = 0; x < cols; x++) {
        unsigned src = XY(x, y);
        unsigned dst = XY(x, y - 1);
        SEGMENT.data[dst] = SEGMENT.data[src];
      }

    if (hw_random8() > turbulence) {
      // create new sparks at bottom row
      for (unsigned x = 0; x < cols; x++) {
        uint8_t p = hw_random8();
        if (p < spark_rate) {
          unsigned dst = XY(x, rows - 1);
          SEGMENT.data[dst] = 255;
        }
      }
    }

    // diffuse
    for (unsigned y = 0; y < rows; y++) {
      for (unsigned x = 0; x < cols; x++) {
        unsigned v = SEGMENT.data[XY(x, y)];
        if (x > 0) {
          v += SEGMENT.data[XY(x - 1, y)];
        }
        if (x < (cols - 1)) {
          v += SEGMENT.data[XY(x + 1, y)];
        }
        tmp_row[x] = min(255, (int)(v * 100 / (300 + diffusion)));
      }

      for (unsigned x = 0; x < cols; x++) {
        SEGMENT.data[XY(x, y)] = tmp_row[x];
        if (SEGMENT.check1) {
          CRGB color = ColorFromPalette(SEGPALETTE, tmp_row[x], 255, NOBLEND);
          SEGMENT.setPixelColorXY(x, y, color);
        } else {
          uint32_t color = SEGCOLOR(0);
          SEGMENT.setPixelColorXY(x, y, color_fade(color, tmp_row[x]));
        }
      }
    }
  }
  return FRAMETIME;
}
static const char _data_FX_MODE_DIFFUSIONFIRE[] PROGMEM =
    "Diffusion Fire@!,Spark rate,Diffusion Speed,Turbulence,,Use "
    "palette;;Color;;2;pal=35";

void UserFxUsermod::setup() {
  strip.addEffect(255, &mode_diffusionfire, _data_FX_MODE_DIFFUSIONFIRE);
}
void UserFxUsermod::loop() {}
uint16_t UserFxUsermod::getId() { return USERMOD_ID_USER_FX; }

static UserFxUsermod user_fx;
REGISTER_USERMOD(user_fx);
