#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 12. CUBE CELL   (Waving Cell, made cube-native)
// ===========================================================================
// WLED's Waving Cell owes its look to two things worth keeping:
//   1. a NESTED sine - a sine whose phase is itself a sine - which is what
//      bends the bands into interlocking rounded cells rather than stripes
//   2. summing the terms into a uint8_t that WRAPS. The wrap is not a bug; the
//      hard palette discontinuity at the fold is what draws the cell walls.
//
// The 2D original feeds it pixel coordinates, which on a cube net would break
// at every seam. This feeds it the 3D surface position instead and uses three
// cyclic terms - X modulated by Y, Y by Z, Z by X - so no face is privileged
// and the cells flow over the edges as one continuous structure.
//
// Each axis gets its cell frequency from a different band, so bass, mids and
// treble each stretch and compress the pattern along their own axis.
// ---------------------------------------------------------------------------
static FX_RET mode_cube_cell() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *cx = (int8_t *)SEGENV.data;
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *st = SEGENV.data + 3 * n;
  // [0] built [2..3] clock [4..6] tweened coefficients [7..9] latched targets

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    for (int k = 1; k < 16; k++) st[k] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2);
    for (int k = 4; k < 10; k++) st[k] = 8;
    SEGENV.aux0 = 0; SEGENV.step = 0;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const float    vol  = *(float *)um->u_data[0];
  const uint8_t  peak = fx_lowBeat(um);
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  const uint16_t dtC = fx_dt8(st + 2);

  const uint16_t nowT = (uint16_t)strip.now;

  // Base cell size is a strict 0: the three sliders ARE the pattern. Each is
  // bipolar around centre and fed by its own band. Above centre the axis
  // frequency rises with that band; below centre it falls as the band gets
  // loud, so both halves of the travel are useful even from a zero base.
  const int shx = (int)SEGMENT.custom1 - 128;
  const int shy = (int)SEGMENT.custom2 - 128;
  const int shz = ((int)SEGMENT.custom3 - 16) * 8;
  const int magX = (shx < 0) ? -shx : shx, valX = (shx > 0) ? bass : (255 - bass);
  const int magY = (shy < 0) ? -shy : shy, valY = (shy > 0) ? mid  : (255 - mid);
  const int magZ = (shz < 0) ? -shz : shz, valZ = (shz > 0) ? treb : (255 - treb);

  int tx = (magX * valX) / 512;               // 0..63 each
  int ty = (magY * valY) / 512;
  int tz = (magZ * valZ) / 512;
  if (tx > 63) tx = 63;
  if (ty > 63) ty = 63;
  if (tz > 63) tz = 63;

  // Beat snap latches the targets on a kick and holds them, rather than
  // chasing the bands every frame. If no kick has landed in three seconds it
  // releases back to live tracking - otherwise a strict gate plus a quiet
  // passage would freeze the pattern and make every slider look dead.
  if (SEGMENT.check1) {
    if (peak) {
      st[7] = (uint8_t)tx; st[8] = (uint8_t)ty; st[9] = (uint8_t)tz;
      st[10] = (uint8_t)(nowT & 0xFF); st[11] = (uint8_t)(nowT >> 8);
    }
    const uint16_t lb = (uint16_t)st[10] | ((uint16_t)st[11] << 8);
    if ((uint16_t)(nowT - lb) < 3000) { tx = (int)st[7]; ty = (int)st[8]; tz = (int)st[9]; }
  }

  // tween, with a guaranteed step of at least 1 - integer division was
  // truncating small corrections to zero and stalling short of the target
  int tw = (int)fx_step(16, dtC); if (tw > 64) tw = 64; if (tw < 1) tw = 1;
  const int tgt[3] = { tx, ty, tz };
  for (int k = 0; k < 3; k++) {
    const int cur = (int)st[4 + k], dif = tgt[k] - cur;
    int mvS = (dif * tw) / 64;
    if (mvS == 0 && dif != 0) mvS = (dif > 0) ? 1 : -1;
    st[4 + k] = (uint8_t)(cur + mvS);
  }
  const int ax = (int)st[4], ay = (int)st[5], az = (int)st[6];

  // The 2D original's coefficients are "phase units per PIXEL". Here the
  // coordinates are 3D surface positions spanning -127..127 per face, so a
  // face step is ~254/B units, not 1. Feeding the raw coefficients straight in
  // advanced the phase by up to two whole periods per pixel - past Nyquist,
  // which is exactly why it read as noise. Divide it back down so the sliders
  // mean the same thing they do on the stock effect.
  const int span  = cube ? B : ((cols < rows) ? cols : rows);
  const int stepU = (254 / span) > 0 ? (254 / span) : 1;   // span is >=4, this is just a floor
  const int fxx = (ax << 8) / stepU;
  const int fyy = (ay << 8) / stepU;
  const int fzz = (az << 8) / stepU;

  // Speed remapped: 0 is effectively stopped, 128 matches what 240 used to
  // give, and the top of the travel runs well past the old maximum.
  uint32_t rate;
  if (SEGMENT.speed == 0)        rate = 0;
  else if (SEGMENT.speed <= 128) rate = ((uint32_t)SEGMENT.speed * 241) / 128;
  else                           rate = 241 + ((uint32_t)(SEGMENT.speed - 128) * 2159) / 127;
  SEGENV.step += ((uint32_t)rate * dtC) >> 4;      // accumulate, so it never
  const uint8_t t8 = (uint8_t)(SEGENV.step >> 8);  // overflows on long uptime

  // optional tumble of the sample frame, so the cells drift bodily around the
  // solid instead of only breathing in place
  int c1 = 128, s1 = 0, c2 = 128, s2 = 0;
  if (SEGMENT.check2) {
    SEGENV.aux0 += (uint16_t)fx_step(3 + (SEGMENT.speed >> 5), dtC);
    const uint8_t r1 = SEGENV.aux0 >> 8, r2 = (uint8_t)(SEGENV.aux0 >> 9);
    c1 = (int)cos8_t(r1) - 128; s1 = (int)sin8_t(r1) - 128;
    c2 = (int)cos8_t(r2) - 128; s2 = (int)sin8_t(r2) - 128;
  }

  const uint8_t drive = cfx_drive(vol, (float)(1 + (SEGMENT.intensity >> 5)), 40);
  const uint8_t hue   = (uint8_t)(strip.now >> 10);   // slow palette drift

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      int px = cx[i], py = cy[i], pz = cz[i];
      if (SEGMENT.check2) {
        const int rx = (px * c1 - py * s1) / 128;
        const int ry = (px * s1 + py * c1) / 128;
        const int rz = pz;
        px = rx;
        py = (ry * c2 - rz * s2) / 128;
        pz = (ry * s2 + rz * c2) / 128;
      }

      // nested sines, cycled across the three axes
      const uint8_t gx = (uint8_t)((fxx * px) >> 8);
      const uint8_t gy = (uint8_t)((fyy * py) >> 8);
      const uint8_t gz = (uint8_t)((fzz * pz) >> 8);
      const uint8_t a = sin8_t((uint8_t)(gx + sin8_t((uint8_t)(gy + t8))));
      const uint8_t b = cos8_t((uint8_t)(gy + sin8_t((uint8_t)(gz + t8))));
      const uint8_t c = cos8_t((uint8_t)(gz + sin8_t((uint8_t)(gx + t8))));

      // The stock effect sums TWO terms, so the uint8 wraps once and draws one
      // set of cell walls. Three full-weight terms wrapped three times over and
      // shredded the pattern; halving two keeps all three axes represented at
      // the original's density.
      const uint8_t idx = (uint8_t)(a + (b >> 1) + (c >> 1) + hue);
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette(idx, false, false, 0, drive));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_CELL[] PROGMEM =
  "Ace 3-D Cube Cell@Speed,Drive,Beat shift X,Beat shift Y,Beat shift Z,Beat snap,Tumble,Flat mode;;!;2f;sx=128,ix=150,c1=200,c2=64,c3=26,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeCellUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_cell, _data_FX_MODE_CUBE_CELL);
  }
  void loop() override {}
};

static CubeFx_CubeCellUsermod cube_fx_cube_cell;
REGISTER_USERMOD(cube_fx_cube_cell);
