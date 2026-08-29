#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 7. CUBE CHLADNI
// ===========================================================================
// A flat Chladni figure is the nodal set of a plate mode. The cube version is
// the nodal SURFACE of a 3D standing wave in a solid cube, intersected with
// that cube's outer faces - so the sand lines are closed curves that run over
// every edge without a break.
//
// A single box mode cos(lX)cos(mY)cos(nZ) has flat nodal planes, which would
// just draw a grid. Chladni's curves need degenerate modes combined, so this
// subtracts a cyclic permutation:
//
//   psi = cos(lX)cos(mY)cos(nZ) - cos(mX)cos(nY)cos(lZ)
//
// Full symmetry adds the other four permutations, antisymmetrised, giving the
// figure the cube's own symmetry group instead of a chiral pair.
//
// Note l == m == n makes psi vanish everywhere, exactly like m == n did on the
// flat version, so the three mode numbers are forced apart and the brightness
// fades out if a morph brings them together.
// ---------------------------------------------------------------------------
static FX_RET mode_cube_chladni() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + 768 + 4)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *cx = (int8_t *)SEGENV.data;
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *tl = SEGENV.data + 3 * n;          // cos table for mode l
  uint8_t *tm = tl + 256;
  uint8_t *tn = tm + 256;
  uint8_t *st = tn + 256;

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    st[0] = (uint8_t)(cube ? 1 : 2);
    SEGENV.aux0 = 256; SEGENV.aux1 = 384; SEGENV.step = 512;   // 1.0, 1.5, 2.0
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const float    vol  = *(float *)um->u_data[0];
  const uint8_t  peak = fx_lowBeat(um);

  // one mode number per band, so bass, mid and treble each own an axis
  uint8_t bl = 0, bm = 5, bh = 11;
  for (uint8_t i = 1;  i < 5;  i++) if (fft[i] > fft[bl]) bl = i;
  for (uint8_t i = 6;  i < 11; i++) if (fft[i] > fft[bm]) bm = i;
  for (uint8_t i = 12; i < 16; i++) if (fft[i] > fft[bh]) bh = i;

  // A 16-pixel face cannot render more than about 4 periods before aliasing,
  // so that is the hard ceiling regardless of what the sliders ask for.
  int hardCap = cube ? (B / 4) : (((cols < rows) ? cols : rows) / 8);
  if (hardCap < 2) hardCap = 2;
  if (hardCap > 8) hardCap = 8;

  // Mode numbers in HALF steps, 2 == mode 1.0. Base scale picks where the
  // window sits, Audio span how wide it is. A narrow window high up gives a
  // fixed fine figure; a narrow window low down gives big readable lobes.
  // Modes in 8.8 fixed point, 256 == mode 1.0. Tune is BIPOLAR: centre sits on
  // mode 1.0, which is what this used to be pinned at. Winding down goes below
  // one period per face, where a single broad nodal surface sweeps the whole
  // cube instead of lacing every face - on a 16-pixel face that is where the
  // figure actually becomes readable. Winding up runs to the aliasing ceiling.
  const int32_t capFix = (int32_t)hardCap * 256;
  int32_t baseFix;
  if (SEGMENT.custom1 < 128) baseFix = 64 + ((int32_t)SEGMENT.custom1 * 192) / 128;
  else baseFix = 256 + ((int32_t)((int)SEGMENT.custom1 - 128) * (capFix - 256)) / 127;

  int32_t spanFix = ((int32_t)SEGMENT.custom2 * (capFix - baseFix)) / 255;
  if (spanFix < 0) spanFix = 0;

  // staggered so the three can never coincide, which would zero psi everywhere
  const int32_t fl = baseFix + ((int32_t)bl * spanFix) / 4;
  const int32_t fm = baseFix + ((int32_t)((int)bm - 5) * spanFix) / 5 + 48;
  const int32_t fh = baseFix + ((int32_t)((int)bh - 11) * spanFix) / 4 + 96;

  int32_t rate = fx_step(1 + (SEGMENT.speed >> 5), fx_dt8(st + 1));
  if (rate > 64) rate = 64;
  if (peak && SEGMENT.check1) rate = 64;
  SEGENV.aux0 += (fl - (int32_t)SEGENV.aux0) * rate / 64;
  SEGENV.aux1 += (fm - (int32_t)SEGENV.aux1) * rate / 64;
  int32_t nz16 = (int32_t)(SEGENV.step & 0xFFFF);
  nz16 += (fh - nz16) * rate / 64;
  SEGENV.step = (uint32_t)(nz16 & 0xFFFF);

  const int32_t L = (int32_t)SEGENV.aux0, M = (int32_t)SEGENV.aux1, N = nz16;

  // one cosine table per mode, indexed by the stored coordinate. 768 lookups
  // a frame instead of nine per pixel.
  for (int i = 0; i < 256; i++) {
    const int v = i - 128;
    tl[i] = cos8_t((uint8_t)((L * v) / 256));
    tm[i] = cos8_t((uint8_t)((M * v) / 256));
    tn[i] = cos8_t((uint8_t)((N * v) / 256));
  }

  const uint32_t sharp = 4 + (SEGMENT.intensity >> 3);   // 4..35
  const uint8_t  glow  = (uint8_t)(((int)SEGMENT.custom3 * 255) / 31);  // sand <-> antinodes
  uint8_t drive = cfx_drive(vol, 8.0f, 0);               // fixed gain; c1/c2 now scale

  // fade out rather than flashing white if the three modes converge
  int lo = L, hi = L;
  if (M < lo) lo = M; if (M > hi) hi = M;
  if (N < lo) lo = N; if (N > hi) hi = N;
  const int spread = hi - lo;
  if (spread < 64) drive = scale8(drive, (uint8_t)(spread * 4));

  const bool full = SEGMENT.check2;

  uint8_t colBlk[cols];
  if (cube) for (int x = 0; x < cols; x++) colBlk[x] = (uint8_t)(x / B);

  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    const uint8_t byb = cube ? (uint8_t)(y / B) : 1;
    for (int x = 0; x < cols; x++, i++) {
      if (cube && byb != 1 && colBlk[x] != 1) continue;

      const int ux = (int)cx[i] + 128, uy = (int)cy[i] + 128, uz = (int)cz[i] + 128;
      const int lx = (int)tl[ux] - 128, ly = (int)tl[uy] - 128, lz = (int)tl[uz] - 128;
      const int mx = (int)tm[ux] - 128, my = (int)tm[uy] - 128, mz = (int)tm[uz] - 128;
      const int nx = (int)tn[ux] - 128, ny = (int)tn[uy] - 128, nzc = (int)tn[uz] - 128;

      int32_t psi = (int32_t)lx * my * nzc - (int32_t)mx * ny * lz;
      if (full) {
        psi += (int32_t)nx * ly * mz
             - (int32_t)mx * ly * nzc
             - (int32_t)lx * ny * mz
             - (int32_t)nx * my * lz
             + (int32_t)mx * ny * lz;        // restores the pair used above
        psi /= 2;                            // keep the range comparable
      }

      int32_t a = (psi < 0) ? -psi : psi;
      const uint32_t d = (((uint32_t)(a >> 8)) * sharp) >> 8;

      const uint8_t lumS = (d > 255) ? 0 : (uint8_t)(255 - d);   // sand on nodes
      const uint8_t lumA = (d > 255) ? 255 : (uint8_t)d;         // antinode glow
      uint8_t lum = (uint8_t)((((uint16_t)lumS * (255 - glow))
                             + ((uint16_t)lumA * glow)) >> 8);
      lum = scale8(lum, drive);

      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)((bl << 4) + (lum >> 2)), false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_CHLADNI[] PROGMEM =
  "Ace 3-D Cube Chladni@Morph speed,Sharpness,Tune,Audio span,Sand-glow,Snap to beat,Full symmetry,Flat mode;;!;2f;sx=90,ix=128,c1=100,c2=110,c3=0,o1=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeChladniUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_chladni, _data_FX_MODE_CUBE_CHLADNI);
  }
  void loop() override {}
};

static CubeFx_CubeChladniUsermod cube_fx_cube_chladni;
REGISTER_USERMOD(cube_fx_cube_chladni);
