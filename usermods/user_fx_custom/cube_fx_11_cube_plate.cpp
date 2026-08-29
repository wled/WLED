#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 11. CUBE PLATE  (two-mode Chladni)
// ===========================================================================
// The sibling to Cube Chladni, for comparison. That one uses three mode
// numbers and products of THREE cosines, which is why it laces every face with
// fine detail. This uses two modes and products of TWO cosines, so the
// features are broader and there is far less of them.
//
// Plate mode:   psi = cos(lZ) * [ cos(lX)cos(mY) - cos(mX)cos(lY) ]
//   The bracket is the textbook square-plate figure. On the top face cos(lZ)
//   is constant, so the top literally IS a flat Chladni plate, and the pattern
//   wraps down the walls modulated by height.
//
// Symmetric:    psi = sum of the three cyclic cosine pairs minus the three
//   reverse-cyclic ones. No axis is privileged, nothing factors out, and the
//   figure has no preferred face - but it is busier than plate mode.
//
// Note l == m zeroes both forms, same trap as before, so the modes are
// staggered and the brightness fades if they converge.
// ---------------------------------------------------------------------------
static FX_RET mode_cube_plate() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + 512 + 4)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *cx = (int8_t *)SEGENV.data;
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *tl = SEGENV.data + 3 * n;
  uint8_t *tm = tl + 256;
  uint8_t *st = tm + 256;

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    st[0] = (uint8_t)(cube ? 1 : 2);
    SEGENV.aux0 = 256; SEGENV.aux1 = 512;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const float    vol  = *(float *)um->u_data[0];
  const uint8_t  peak = fx_lowBeat(um);

  uint8_t bl = 0, bh = 8;                       // one mode per half-spectrum
  for (uint8_t k = 1; k < 8;  k++) if (fft[k] > fft[bl]) bl = k;
  for (uint8_t k = 9; k < 16; k++) if (fft[k] > fft[bh]) bh = k;

  int hardCap = cube ? (B / 4) : (((cols < rows) ? cols : rows) / 8);
  if (hardCap < 2) hardCap = 2;
  if (hardCap > 8) hardCap = 8;

  // same bipolar Tune as Cube Chladni: centre is mode 1.0, down goes below one
  // period per face
  const int32_t capFix = (int32_t)hardCap * 256;
  int32_t baseFix;
  if (SEGMENT.custom1 < 128) baseFix = 64 + ((int32_t)SEGMENT.custom1 * 192) / 128;
  else baseFix = 256 + ((int32_t)((int)SEGMENT.custom1 - 128) * (capFix - 256)) / 127;
  int32_t spanFix = ((int32_t)SEGMENT.custom2 * (capFix - baseFix)) / 255;
  if (spanFix < 0) spanFix = 0;

  const int32_t fl = baseFix + ((int32_t)bl * spanFix) / 7;
  const int32_t fm = baseFix + ((int32_t)((int)bh - 8) * spanFix) / 7 + 72;

  int32_t rate = fx_step(1 + (SEGMENT.speed >> 5), fx_dt8(st + 1));
  if (rate > 64) rate = 64;
  if (peak && SEGMENT.check1) rate = 64;
  SEGENV.aux0 += (fl - (int32_t)SEGENV.aux0) * rate / 64;
  SEGENV.aux1 += (fm - (int32_t)SEGENV.aux1) * rate / 64;
  const int32_t L = (int32_t)SEGENV.aux0, M = (int32_t)SEGENV.aux1;

  for (int i = 0; i < 256; i++) {              // 512 lookups, not four per pixel
    const int v = i - 128;
    tl[i] = cos8_t((uint8_t)((L * v) / 256));
    tm[i] = cos8_t((uint8_t)((M * v) / 256));
  }

  const uint32_t sharp = 4 + (SEGMENT.intensity >> 2);
  const uint8_t  glow  = (uint8_t)(((int)SEGMENT.custom3 * 255) / 31);
  uint8_t drive = cfx_drive(vol, 8.0f, 0);
  int32_t sep = L - M; if (sep < 0) sep = -sep;
  if (sep < 64) drive = scale8(drive, (uint8_t)(sep * 4));

  const bool plate = SEGMENT.check2;

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int ux = (int)cx[i] + 128, uy = (int)cy[i] + 128, uz = (int)cz[i] + 128;
      const int lx = (int)tl[ux] - 128, ly = (int)tl[uy] - 128, lz = (int)tl[uz] - 128;
      const int mx = (int)tm[ux] - 128, my = (int)tm[uy] - 128, mz = (int)tm[uz] - 128;

      int32_t psi;
      if (plate) {
        psi = ((int32_t)lz * ((int32_t)lx * my - (int32_t)mx * ly)) / 48;
      } else {
        psi = (int32_t)lx * my + (int32_t)ly * mz + (int32_t)lz * mx
            - (int32_t)mx * ly - (int32_t)my * lz - (int32_t)mz * lx;
      }

      const int32_t  a = (psi < 0) ? -psi : psi;
      const uint32_t d = ((uint32_t)a * sharp) >> 12;
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

static const char _data_FX_MODE_CUBE_PLATE[] PROGMEM =
  "Ace 3-D Cube Plate@Morph speed,Sharpness,Tune,Audio span,Sand-glow,Snap to beat,Plate mode,Flat mode;;!;2f;sx=90,ix=128,c1=100,c2=110,c3=0,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubePlateUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_plate, _data_FX_MODE_CUBE_PLATE);
  }
  void loop() override {}
};

static CubeFx_CubePlateUsermod cube_fx_cube_plate;
REGISTER_USERMOD(cube_fx_cube_plate);
