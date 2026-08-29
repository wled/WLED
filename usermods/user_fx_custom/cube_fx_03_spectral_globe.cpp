#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 3. SPECTRAL GLOBE
// ===========================================================================
// Spherical coordinates on the cube: latitude selects the frequency band, so
// the spectrum forms rings that close around all four walls and cap at the
// top. Azimuth is folded into N mirrored wedges - a kaleidoscope whose axis
// is the cube's vertical axis rather than a point on one face.
// ---------------------------------------------------------------------------
static FX_RET mode_spectral_globe() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 20)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *az   = SEGENV.data;
  uint8_t *el   = SEGENV.data + n;
  uint8_t *spec = SEGENV.data + 2 * n;
  uint8_t *gt   = spec + 16;                // 2-byte frame timestamp

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || SEGENV.aux1 != (uint16_t)(cube ? 1 : 2)) {
    cfx_buildCube(nullptr, nullptr, nullptr, az, el, cols, rows, cube);
    for (int i = 0; i < 16; i++) spec[i] = 0;
    SEGENV.aux1 = cube ? 1 : 2;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  cfx_smoothSpec(spec, fft, 3);

  const uint16_t dtG = fx_dt8(gt);
  SEGENV.aux0 += (uint16_t)fx_step(1 + (SEGMENT.speed >> 4), dtG);
  if (peak && SEGMENT.check1) SEGENV.aux0 += 40503;      // golden-angle spin
  const uint8_t rot = SEGENV.aux0 >> 8;

  SEGENV.step += (uint32_t)fx_step(1 + (bass >> 4), dtG);  // bands drift poleward
  const uint8_t scroll = (uint8_t)(SEGENV.step >> 4);

  const uint8_t folds = 2 + (uint8_t)(((uint16_t)SEGMENT.custom1 * 8) >> 8);
  const int     twist = ((int)SEGMENT.custom2 >> 3) - 16;
  const uint8_t depth = (uint8_t)(SEGMENT.custom3 << 3);
  const uint8_t width = 1 + (SEGMENT.intensity >> 6);

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      uint8_t w = (uint8_t)((uint16_t)(uint8_t)(az[i] - rot) * folds);
      if (w > 127) w = 255 - w;

      const uint8_t s8 = (uint8_t)((int)el[i] - (int)scroll);
      uint8_t ring = cos8_t((uint8_t)(s8 << 4));
      for (uint8_t k = 1; k < width; k++) ring = scale8(ring, ring);

      const uint8_t petal = cos8_t((uint8_t)((int)(w * 2) + ((int)el[i] * twist) / 8));
      const uint8_t mod   = (uint8_t)(255 - scale8((uint8_t)(255 - petal), depth));

      const uint8_t lum = scale8(spec[(s8 >> 4) & 0x0F], scale8(ring, mod));
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(((s8 >> 4) << 4) + rot), false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_SPECTRAL_GLOBE[] PROGMEM =
  "Ace 3-D Spectral Globe@Spin,Band width,Symmetry,Twist,Petal depth,Golden spin,,Flat mode;;!;2f;sx=60,ix=100,c1=96,c2=140,c3=12,o1=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_SpectralGlobeUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_spectral_globe, _data_FX_MODE_SPECTRAL_GLOBE);
  }
  void loop() override {}
};

static CubeFx_SpectralGlobeUsermod cube_fx_spectral_globe;
REGISTER_USERMOD(cube_fx_spectral_globe);
