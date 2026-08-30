#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 4. CUBE SLICE
// ===========================================================================
// A stack of spectrum slabs cutting through the solid at a tumbling angle.
// Every pixel is coloured by which slab its 3D position falls in, so the
// bands wrap around corners as flat planes rather than bending at the folds.
// Beats re-aim the axis.
// ---------------------------------------------------------------------------
static FX_RET mode_cube_slice() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + 24)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *cx   = (int8_t *)SEGENV.data;
  int8_t  *cy   = cx + n;
  int8_t  *cz   = cy + n;
  uint8_t *spec = SEGENV.data + 3 * n;
  uint8_t *st   = spec + 16;      // [0] built flag, [1] prev beat, [2..3] prev tick

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    for (int i = 0; i < 16; i++) spec[i] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2);
    st[1] = 0;
    SEGENV.step = 0; SEGENV.aux1 = 0;
    const uint16_t t0 = (uint16_t)(strip.now >> 4);
    st[2] = (uint8_t)(t0 & 0xFF); st[3] = (uint8_t)(t0 >> 8);
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  cfx_smoothSpec(spec, fft, 3);

  // Everything below advances on elapsed TIME, not on frames. The old version
  // stepped per frame, so the same settings ran several times faster once the
  // frame rate went up.
  const uint16_t nowT  = (uint16_t)(strip.now >> 4);          // 16 ms ticks
  const uint16_t prevT = (uint16_t)st[2] | ((uint16_t)st[3] << 8);
  uint16_t dt = (uint16_t)(nowT - prevT);
  if (dt > 32) dt = 32;                                       // clamp after a pause
  st[2] = (uint8_t)(nowT & 0xFF); st[3] = (uint8_t)(nowT >> 8);

  // tumbling slab normal
  const bool rising = peak && !st[1];
  st[1] = peak ? 1 : 0;
  if (rising && SEGMENT.check1) SEGENV.aux1 += hw_random16();
  const uint32_t spin = (strip.now * (uint32_t)(1 + (SEGMENT.speed >> 3))) >> 3;
  const uint16_t angle = (uint16_t)spin + SEGENV.aux1;
  const uint8_t ta = (uint8_t)(angle >> 8);
  const uint8_t tb = (uint8_t)((angle >> 7) + 64);
  const int ca = (int)cos8_t(ta) - 128, sa = (int)sin8_t(ta) - 128;
  const int cb = (int)cos8_t(tb) - 128, sb = (int)sin8_t(tb) - 128;
  const int nx = (ca * cb) / 128;
  const int ny = (sa * cb) / 128;
  const int nz = sb;

  // slab travel: 8.8 accumulator. Scroll speed at 0 with Bass push at 0 stops
  // the bands dead and leaves only the tumble moving.
  int32_t rate = (int32_t)SEGMENT.custom2 + (((int32_t)bass * (int32_t)SEGMENT.custom3) >> 5);
  if (SEGMENT.check2) rate = -rate;
  SEGENV.step = (uint32_t)((int32_t)SEGENV.step + rate * (int32_t)dt);
  const uint8_t scroll = (uint8_t)((SEGENV.step >> 8) & 0xFF);

  const uint8_t pitch = 1 + (SEGMENT.custom1 >> 5);      // slabs across the solid
  const uint8_t width = 1 + (SEGMENT.intensity >> 6);
  const uint8_t hue   = (uint8_t)(strip.now >> 7);

  uint8_t colBlk[cols];
  if (cube) for (int x = 0; x < cols; x++) colBlk[x] = (uint8_t)(x / B);

  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    const uint8_t byb = cube ? (uint8_t)(y / B) : 1;
    for (int x = 0; x < cols; x++, i++) {
      if (cube && byb != 1 && colBlk[x] != 1) continue;
      const int d = (cx[i] * nx + cy[i] * ny + cz[i] * nz) / 128;   // -127..127
      const uint8_t s8 = (uint8_t)(d * pitch - (int)scroll);

      uint8_t prof = cos8_t((uint8_t)(s8 << 4));
      for (uint8_t k = 1; k < width; k++) prof = scale8(prof, prof);

      const uint8_t lum = scale8(spec[(s8 >> 4) & 0x0F], prof);
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(((s8 >> 4) << 4) + hue), false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_SLICE[] PROGMEM =
  "Ace 3-D Cube Slice@Tumble speed,Slab width,Slab count,Scroll speed,Bass push,Re-aim on beat,Reverse,Flat mode;;!;2f;sx=70,ix=100,c1=100,c2=80,c3=8,o1=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeSliceUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_slice, _data_FX_MODE_CUBE_SLICE);
  }
  void loop() override {}
};

static CubeFx_CubeSliceUsermod cube_fx_cube_slice;
REGISTER_USERMOD(cube_fx_cube_slice);
