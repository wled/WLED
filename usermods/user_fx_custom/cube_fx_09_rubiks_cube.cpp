#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 9. RUBIKS CUBE
// ===========================================================================
// Each 16-pixel face splits into a 3x3 of 4-pixel stickers with a 1-pixel dark
// line between them and around the rim, which reads as the black plastic.
//
// The puzzle is simulated properly rather than faked. Stickers are stored by
// (cubie, face normal) in 3D, and a turn rotates every cubie in the layer by
// 90 degrees, carrying its stickers with it. That means no hand-written
// permutation tables - the six face turns fall out of one rotation routine,
// and they are correct by construction. All 54 stickers are simulated even
// though the bottom face isn't wired, because D turns still move stickers
// onto the four sides.
//
// Pixel -> sticker mapping reuses cfx_pos, so the sticker grid inherits the
// verified net orientation; a sticker is identified by where it sits in space,
// not by how the net happens to unfold.
//
// Cycle: hold solved -> rapid scramble -> hold -> unwind back to solved. The
// scramble is recorded and replayed backwards with inverted turns, so it
// always lands exactly on solved without needing a solver.
// ---------------------------------------------------------------------------
#define RB_MOVES 48
#define RB_STICK (27 * 6)

static const uint32_t RB_COL[6] = {          // +X, -X, +Y, -Y, +Z, -Z
  RGBW32(220,  20,  20, 0),                  // R  red
  RGBW32(255, 100,   0, 0),                  // L  orange
  RGBW32(  0,  60, 230, 0),                  // B  blue
  RGBW32(  0, 190,  50, 0),                  // F  green
  RGBW32(235, 235, 235, 0),                  // U  white
  RGBW32(255, 215,   0, 0)                   // D  yellow
};
static const int8_t RB_DIR[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

static inline void rb_rot(int axis, int &x, int &y, int &z) {
  int nx, ny, nz;
  if      (axis == 0) { nx =  x; ny = -z; nz =  y; }   // about X
  else if (axis == 1) { nx =  z; ny =  y; nz = -x; }   // about Y
  else                { nx = -y; ny =  x; nz =  z; }   // about Z
  x = nx; y = ny; z = nz;
}
static inline int rb_dirIdx(int x, int y, int z) {
  for (int d = 0; d < 6; d++)
    if (RB_DIR[d][0] == x && RB_DIR[d][1] == y && RB_DIR[d][2] == z) return d;
  return 0;
}
// One quarter turn of the layer at `layer` along `axis`, repeated `turns` times.
static void rb_turn(uint8_t *col, int axis, int layer, int turns) {
  uint8_t tmp[RB_STICK];
  for (int t = 0; t < turns; t++) {
    memcpy(tmp, col, RB_STICK);
    for (int x = -1; x <= 1; x++)
      for (int y = -1; y <= 1; y++)
        for (int z = -1; z <= 1; z++) {
          const int c[3] = { x, y, z };
          if (c[axis] != layer) continue;
          int nx = x, ny = y, nz = z; rb_rot(axis, nx, ny, nz);
          const int src = ((x + 1) * 9 + (y + 1) * 3 + (z + 1)) * 6;
          const int dst = ((nx + 1) * 9 + (ny + 1) * 3 + (nz + 1)) * 6;
          for (int d = 0; d < 6; d++) {
            int dx = RB_DIR[d][0], dy = RB_DIR[d][1], dz = RB_DIR[d][2];
            rb_rot(axis, dx, dy, dz);
            col[dst + rb_dirIdx(dx, dy, dz)] = tmp[src + d];
          }
        }
  }
}
static inline uint32_t rb_scale(uint32_t c, uint8_t s) {
  return RGBW32(scale8((uint8_t)(c >> 16), s), scale8((uint8_t)(c >> 8), s),
                scale8((uint8_t)c, s), 0);
}

static FX_RET mode_rubiks_cube() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 8 || rows < 8) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + RB_STICK + RB_MOVES + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *cx  = (int8_t *)SEGENV.data;      // 3D surface position per pixel
  int8_t  *cy  = cx + n;
  int8_t  *cz  = cy + n;
  uint8_t *col = (uint8_t *)(cz + n);        // live sticker colours
  uint8_t *mv  = col + RB_STICK;             // recorded scramble
  uint8_t *st  = mv + RB_MOVES;
  // st: [0] built [1] phase [2] count [3] idx [4..5] next move
  //     [6] animating [7] axis [8] layer+2 [9] turns [10..11] start [12..13] duration

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    if (!cube) for (size_t k = 0; k < n; k++) cz[k] = 127;   // flat panel = the U face
    for (int c = 0; c < 27; c++)
      for (int d = 0; d < 6; d++) {
        const int pp[3] = { c / 9 - 1, (c / 3) % 3 - 1, c % 3 - 1 };
        const int ax = (d < 2) ? 0 : ((d < 4) ? 1 : 2);
        col[c * 6 + d] = (uint8_t)((pp[ax] == RB_DIR[d][ax]) ? d : 255);
      }
    for (int k = 0; k < RB_MOVES; k++) mv[k] = 0;
    for (int k = 0; k < 16; k++) st[k] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2);
    const uint16_t t0 = (uint16_t)strip.now;
    st[4] = (uint8_t)(t0 & 0xFF); st[5] = (uint8_t)(t0 >> 8);
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  const uint16_t nowT = (uint16_t)strip.now;

  const int nMoves   = 6 + ((int)SEGMENT.custom1 * (RB_MOVES - 6)) / 255;
  const int scrambMs = 90 + (255 - (int)SEGMENT.speed) / 2;
  const int paceMs   = 300 + (int)SEGMENT.custom2 * 6;

  // --- finish an in-flight turn --------------------------------------------
  if (st[6]) {
    const uint16_t sT = (uint16_t)st[10] | ((uint16_t)st[11] << 8);
    const uint16_t dU = (uint16_t)st[12] | ((uint16_t)st[13] << 8);
    if ((uint16_t)(nowT - sT) >= dU) {
      rb_turn(col, (int)st[7], (int)st[8] - 2, (int)st[9]);
      st[6] = 0;
      const uint16_t nx = nowT + (uint16_t)((st[1] == 1) ? scrambMs : paceMs);
      st[4] = (uint8_t)(nx & 0xFF); st[5] = (uint8_t)(nx >> 8);
    }
  }

  // --- pick the next turn, only while nothing is spinning -------------------
  if (!st[6]) {
    const uint16_t due = (uint16_t)st[4] | ((uint16_t)st[5] << 8);
    const bool timeUp = (uint16_t)(nowT - due) < 32768;
    bool doMove = false; int mAxis = 0, mLayer = 1, mTurns = 1;

    switch (st[1]) {
      case 0: if (timeUp) { st[1] = 1; st[2] = 0; } break;         // hold solved
      case 1:                                                       // scramble
        if (timeUp) {
          mAxis  = (int)hw_random16(3);
          mLayer = hw_random16(2) ? 1 : -1;
          mTurns = 1 + (int)hw_random16(3);
          if (st[2] > 0 && (mv[st[2] - 1] & 3) == (uint8_t)mAxis) mAxis = (mAxis + 1) % 3;
          mv[st[2]] = (uint8_t)(mAxis | (((mLayer + 1) / 2) << 2) | (mTurns << 4));
          doMove = true;
          if (++st[2] >= nMoves) { st[1] = 2; st[3] = st[2]; }
        }
        break;
      case 2: if (timeUp) st[1] = 3; break;                         // hold scrambled
      default: {                                                    // unwind
        const bool ready = SEGMENT.check2 ? (peak != 0) : timeUp;
        if (ready && st[3] > 0) {
          const uint8_t rec = mv[--st[3]];
          mAxis  = rec & 3;
          mLayer = ((rec >> 2) & 3) ? 1 : -1;
          mTurns = 4 - ((rec >> 4) & 3);                            // inverse
          doMove = true;
        }
        if (st[3] == 0 && !doMove) st[1] = 0;
        break; }
    }

    if (doMove) {                                                   // start spinning
      int dur = (st[1] == 1) ? (scrambMs * 3) / 4 : (paceMs * 2) / 3;
      if (dur > 700) dur = 700;
      if (dur < 70)  dur = 70;
      st[6] = 1; st[7] = (uint8_t)mAxis; st[8] = (uint8_t)(mLayer + 2); st[9] = (uint8_t)mTurns;
      st[10] = (uint8_t)(nowT & 0xFF); st[11] = (uint8_t)(nowT >> 8);
      st[12] = (uint8_t)(dur & 0xFF);  st[13] = (uint8_t)(dur >> 8);
    } else if (timeUp) {
      const uint16_t nx = nowT + (uint16_t)((st[1] == 1) ? scrambMs : paceMs);
      st[4] = (uint8_t)(nx & 0xFF); st[5] = (uint8_t)(nx >> 8);
    }
  }

  // --- current turn angle ---------------------------------------------------
  const bool anim = st[6] != 0;
  const int  aAxis = st[7], aLayer = (int)st[8] - 2;
  int ca = 127, sa = 0;
  if (anim) {
    const uint16_t sT = (uint16_t)st[10] | ((uint16_t)st[11] << 8);
    uint16_t dU = (uint16_t)st[12] | ((uint16_t)st[13] << 8); if (!dU) dU = 1;
    uint32_t el = (uint16_t)(nowT - sT); if (el > dU) el = dU;
    const uint8_t a8 = (uint8_t)((el * (uint32_t)(st[9] * 64)) / dU);   // 64 = 90 deg
    ca = (int)cos8_t(a8) - 128;
    sa = (int)sin8_t(a8) - 128;
  }

  if (peak && SEGMENT.check1) SEGENV.aux0 = 0;
  else if (SEGENV.aux0 < 400) SEGENV.aux0 += (uint16_t)fx_step(26, fx_dt8(st + 14));
  const int     pulsePos = (int)SEGENV.aux0;
  const uint8_t pDepth   = SEGMENT.check1 ? SEGMENT.intensity : 0;

  uint32_t face[6];
  const uint8_t pb = SEGMENT.custom3 ? (uint8_t)(((int)SEGMENT.custom3 * 255) / 31) : 0;
  for (int d = 0; d < 6; d++) {
    const uint32_t A = RB_COL[d];
    if (!pb) { face[d] = A; continue; }
    const uint32_t Bc = SEGMENT.color_from_palette((uint8_t)(d * 42), false, false, 0);
    face[d] = RGBW32(
      (uint8_t)((((int)(uint8_t)(A >> 16) * (255 - pb)) + ((int)(uint8_t)(Bc >> 16) * pb)) >> 8),
      (uint8_t)((((int)(uint8_t)(A >>  8) * (255 - pb)) + ((int)(uint8_t)(Bc >>  8) * pb)) >> 8),
      (uint8_t)((((int)(uint8_t)A         * (255 - pb)) + ((int)(uint8_t)Bc         * pb)) >> 8), 0);
  }

  const uint8_t drive = cfx_drive(vol, 1.2f, 150);

  SEGMENT.fill(SEGCOLOR(0));
  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      int qx = cx[i], qy = cy[i], qz = cz[i];
      bool moving = false;

      if (anim) {
        // the layer is fixed in space during its own turn, so membership is
        // decided from the pixel's own position and never changes mid-spin
        const int pa = (aAxis == 0) ? qx : ((aAxis == 1) ? qy : qz);
        const int pq = (pa > 42) ? 1 : ((pa < -42) ? -1 : 0);
        if (pq == aLayer) {
          moving = true;
          // spin this pixel BACKWARDS to find which sticker is standing here
          int rx = qx, ry = qy, rz = qz;
          if      (aAxis == 0) { ry = ( qy * ca + qz * sa) / 128; rz = (-qy * sa + qz * ca) / 128; }
          else if (aAxis == 1) { rx = ( qx * ca - qz * sa) / 128; rz = ( qx * sa + qz * ca) / 128; }
          else                 { rx = ( qx * ca + qy * sa) / 128; ry = (-qx * sa + qy * ca) / 128; }
          // mid-turn that point is off the cube, so push it back out radially
          int m = (rx < 0) ? -rx : rx;
          const int m2 = (ry < 0) ? -ry : ry; if (m2 > m) m = m2;
          const int m3 = (rz < 0) ? -rz : rz; if (m3 > m) m = m3;
          if (m < 1) m = 1;
          const int inv = (127 * 256) / m;
          qx = (rx * inv) >> 8; qy = (ry * inv) >> 8; qz = (rz * inv) >> 8;
        }
      }

      // which face, and the two in-plane coordinates on it
      const int axq = (qx < 0) ? -qx : qx;
      const int ayq = (qy < 0) ? -qy : qy;
      const int azq = (qz < 0) ? -qz : qz;
      int d, u, v;
      if      (axq >= ayq && axq >= azq) { d = (qx > 0) ? 0 : 1; u = qy; v = qz; }
      else if (ayq >= azq)               { d = (qy > 0) ? 2 : 3; u = qx; v = qz; }
      else                               { d = (qz > 0) ? 4 : 5; u = qx; v = qy; }

      // grid lines from POSITION, so they turn with the layer instead of
      // staying painted on the display
      const int au = (u < 0) ? -u : u, av = (v < 0) ? -v : v;
      if (au >= 112 || av >= 112) continue;                 // face rim
      if ((au >= 34 && au <= 50) || (av >= 34 && av <= 50)) continue;   // between stickers

      const int gx = (qx > 42) ? 1 : ((qx < -42) ? -1 : 0);
      const int gy = (qy > 42) ? 1 : ((qy < -42) ? -1 : 0);
      const int gz = (qz > 42) ? 1 : ((qz < -42) ? -1 : 0);
      const uint8_t cIdx = col[(((gx + 1) * 9 + (gy + 1) * 3 + (gz + 1)) * 6) + d];
      if (cIdx > 5) continue;

      int lum = (int)drive;
      if (pDepth) {                                          // ripple inside the sticker
        int pu = (u + 127) % 85; if (pu > 42) pu = 84 - pu;
        int pv = (v + 127) % 85; if (pv > 42) pv = 84 - pv;
        const int ed = (((pu < pv) ? pu : pv) * 255) / 42;
        int off = ed - pulsePos; if (off < 0) off = -off;
        if (off < 150) lum += ((150 - off) * pDepth) / 150;
      }
      if (moving) lum += 28;                                 // slight lift while turning
      if (lum > 255) lum = 255;

      SEGMENT.setPixelColorXY(x, y, rb_scale(face[cIdx], (uint8_t)lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_RUBIKS_CUBE[] PROGMEM =
  "Ace 3-D Rubiks Cube@Scramble speed,Pulse depth,Scramble length,Solve pace,Palette blend,Beat pulse,Solve on beat,Flat mode;;!;2f;sx=200,ix=120,c1=110,c2=110,c3=0,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_RubiksCubeUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_rubiks_cube, _data_FX_MODE_RUBIKS_CUBE);
  }
  void loop() override {}
};

static CubeFx_RubiksCubeUsermod cube_fx_rubiks_cube;
REGISTER_USERMOD(cube_fx_rubiks_cube);
