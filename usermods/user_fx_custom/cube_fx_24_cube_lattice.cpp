#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 24. ACE 3-D LATTICE
// ===========================================================================
// Cube Cell's geometric opposite. That one is nested sines, so everything it
// draws is round. This folds the surface position into a repeating lattice and
// measures it with NORMS rather than trigonometry, which is where the hard
// angles come from:
//
//   max(|x|,|y|,|z|)      -> nested cubes,       square shells
//   (|x|+|y|+|z|) / 3     -> nested octahedra,   diamond shells
//
// Morph crossfades between the two, so the pattern slides from squares to
// diamonds through every intermediate faceted shape - all of them still flat
// sided, never curved. Quantising the result into bands gives hard steps with
// no gradient anywhere, and an optional dark seam between bands keeps the
// edges legible at a distance.
//
// The lattice tumbles in 3D, so bands sweep across faces and over folds as one
// solid object rather than as a pattern painted on each face.
// ---------------------------------------------------------------------------
static FX_RET mode_cube_lattice() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *cx = (int8_t *)SEGENV.data;
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *st = SEGENV.data + 3 * n;   // [0] built [1] morph [2..3] clock [4] latch

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    for (int k = 0; k < 16; k++) st[k] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2); st[1] = 128; st[4] = 128;
    SEGENV.aux0 = 0; SEGENV.step = 0;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  const uint16_t dtL = fx_dt8(st + 2);

  // tumble, with a hard reorientation on a kick
  SEGENV.aux0 += (uint16_t)fx_step(2 + (SEGMENT.speed >> 4), dtL);
  if (peak && SEGMENT.check1) SEGENV.aux0 += hw_random16();
  const uint8_t r1 = SEGENV.aux0 >> 8, r2 = (uint8_t)(SEGENV.aux0 >> 9);
  const int c1 = (int)cos8_t(r1) - 128, s1 = (int)sin8_t(r1) - 128;
  const int c2 = (int)cos8_t(r2) - 128, s2 = (int)sin8_t(r2) - 128;

  // Morph target: slider plus mids. Latched on the beat when Beat snap is on,
  // so the shape changes in deliberate steps rather than shimmering.
  int tgt = (int)SEGMENT.custom2 + ((mid - 110) >> 1);
  if (tgt < 0) tgt = 0; else if (tgt > 255) tgt = 255;
  if (SEGMENT.check1) { if (peak) st[4] = (uint8_t)tgt; tgt = (int)st[4]; }
  { const int cur = (int)st[1], dif = tgt - cur;
    int mv = (dif * (int)fx_step(14, dtL)) / 64;
    if (mv == 0 && dif != 0) mv = (dif > 0) ? 1 : -1;
    st[1] = (uint8_t)(cur + mv); }
  const int morph = (int)st[1];

  const int zoom   = 5 + (SEGMENT.custom1 >> 4) + (bass >> 6);   // lattice pitch
  const int levels = 2 + (SEGMENT.custom3 >> 1);                 // 2..17 shells
  const int drift  = (int)(strip.now >> 6);                      // slow phase slide
  const bool seam  = SEGMENT.check2;
  const uint8_t drive = cfx_drive(vol, 1.4f, 110 + (SEGMENT.intensity >> 1));
  const uint8_t hue   = (uint8_t)(strip.now >> 8);

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int px = cx[i], py = cy[i], pz = cz[i];

      const int qx = (px * c1 - py * s1) / 128;          // tumble
      const int qy = (px * s1 + py * c1) / 128;
      const int ry = (qy * c2 - pz * s2) / 128;
      const int rz = (qy * s2 + pz * c2) / 128;

      // fold each axis into the lattice: an 8-bit wrap is one period, and the
      // distance from its midpoint is a triangle wave with no rounding at all
      const int fx = ((int)(uint8_t)(qx * zoom + drift) - 128);
      const int fy = ((int)(uint8_t)(ry * zoom + drift) - 128);
      const int fz = ((int)(uint8_t)(rz * zoom + drift) - 128);
      const int ax = (fx < 0) ? -fx : fx;
      const int ay = (fy < 0) ? -fy : fy;
      const int az = (fz < 0) ? -fz : fz;

      int mx = ax; if (ay > mx) mx = ay; if (az > mx) mx = az;   // cube shells
      const int sm = (ax + ay + az) / 3;                        // octahedron shells
      const int d  = (mx * (255 - morph) + sm * morph) / 255;

      const int t2   = d * levels;
      const int band = t2 / 129;
      const int frac = t2 - band * 129;

      uint8_t lum = drive;
      if (seam && (frac < 9 || frac > 119)) lum = (uint8_t)(drive >> 3);
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(band * 38 + hue), false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_LATTICE[] PROGMEM =
  "Ace 3-D Lattice@Tumble,Brightness,Pitch,Morph,Shells,Beat snap,Seams,Flat mode;;!;2f;sx=100,ix=150,c1=110,c2=90,c3=12,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeLatticeUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_lattice, _data_FX_MODE_CUBE_LATTICE);
  }
  void loop() override {}
};

static CubeFx_CubeLatticeUsermod cube_fx_cube_lattice;
REGISTER_USERMOD(cube_fx_cube_lattice);
