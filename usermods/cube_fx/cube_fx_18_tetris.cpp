#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 18. ACE 3-D TETRIS
// ===========================================================================
// Wide mode: 2x2 cells give a 32 x 8 field wrapped around the wall cylinder,
// so a completed line is a RING all the way round the cube.
//
// Choreography, not a game - a ring needs all 32 columns and no honest player
// assembles that. The fill is planned as 4x4 TILINGS, each one four real
// tetrominoes interlocking, so L, J, S, Z and T all show up instead of just
// bars and squares. Every column but one gets filled to the very top.
//
// Then the payoff: the well is empty for all eight rows, so the first long
// piece completes the bottom four rings, everything above drops four, and the
// second long piece completes the rest. Two Tetrises and the cube is bare.
// ---------------------------------------------------------------------------
#define TT_PLAN 68

static const uint32_t TT_COL[7] = {
  RGBW32(  0,235,235,0), RGBW32(240,220,  0,0), RGBW32(165, 60,225,0),
  RGBW32( 40,220, 60,0), RGBW32(235, 45, 45,0), RGBW32( 45, 85,235,0),
  RGBW32(245,150,  0,0) };

// Each row is one 4x4 tiling; the four letters mark four interlocking
// tetrominoes. Verified by hand: every letter covers exactly four connected
// cells, and between them these cover I, O, T, S/Z, L and J.
static const char *const TT_TILE[5][4] = {
  { "aaaa", "bbbb", "cccc", "dddd" },   // four flat I
  { "aabb", "aabb", "ccdd", "ccdd" },   // four O
  { "abcd", "abcd", "abcd", "abcd" },   // four upright I
  { "aaab", "cabb", "ccbd", "cddd" },   // T, S, T, L
  { "abbb", "aaab", "cccd", "cddd" } }; // J, L, J, L

static void tt_collapse(uint8_t *fd, int gw, int r0) {
  for (int r = r0 + 3; r >= 0; r--) {
    const int src = r - 4;
    for (int c = 0; c < gw; c++) fd[r * gw + c] = (src >= 0) ? fd[src * gw + c] : 0;
  }
}

static FX_RET mode_tetris() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 256 + TT_PLAN * 5 + 24)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  uint8_t *bu = SEGENV.data, *bv = bu + n;
  uint8_t *fd = bv + n;                     // field cells, 0 = empty
  uint8_t *pl = fd + 256;                   // per piece: 4 cell indices + colour
  uint8_t *st = pl + TT_PLAN * 5;
  // st: [0] built [1] phase [2] count [3] idx [4] well [5] fall [6..7] clock [8] flash

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  const int  bw   = cube ? (4 * B) : cols;
  const int  bh   = cube ? B : rows;
  const int  cw   = ((bw + 31) / 32 < 2) ? 2 : (bw + 31) / 32;
  const int  chh  = ((bh + 7) / 8 < 2) ? 2 : (bh + 7) / 8;
  const int  gw   = bw / cw;
  const int  gh   = 8;
  if (gw < 8 || bh / chh < 8) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const uint16_t nowT = (uint16_t)strip.now;
  bool replan = (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2));
  if (replan) { cfx_buildBand(bu, bv, cols, rows, cube, B); for (int k = 0; k < 24; k++) st[k] = 0; }

  if (replan || st[1] == 5) {
    for (int k = 0; k < 256; k++) fd[k] = 0;
    st[4] = (uint8_t)(3 + hw_random16((uint16_t)(gw - 6)));
    const int well = st[4];
    int cnt = 0;
    for (int band = 0; band < 2; band++) {          // bottom band first
      const int r0 = band ? 0 : 4;
      const int first = cnt;
      int c = 0;
      while (c < gw && cnt <= TT_PLAN - 6) {
        if (c == well) { c++; continue; }
        int run = 0; while (c + run < gw && c + run != well) run++;
        int w = (run >= 4 && hw_random16(5) != 0) ? 4 : ((run >= 2) ? 2 : 1);
        if (w == 4) {
          // Tilings 0 and 2 are all-I; weighting keeps them rare so L, J, S
          // and T carry the fill instead of a wall of bars.
          static const uint8_t TT_PICK[10] = { 3,4,3,4,3,4,1,3,0,2 };
          const int t = TT_PICK[hw_random16(10)];
          for (int L = 0; L < 4; L++) {
            int m = 0;
            for (int r = 0; r < 4; r++)
              for (int q = 0; q < 4; q++)
                if (TT_TILE[t][r][q] == (char)('a' + L) && m < 4)
                  pl[cnt * 5 + m++] = (uint8_t)((r0 + r) * gw + c + q);
            pl[cnt * 5 + 4] = (uint8_t)((t * 3 + L) % 7);
            cnt++;
          }
        } else if (w == 2) {
          if (hw_random16(5)) {                     // two O, mostly
            for (int k2 = 0; k2 < 2; k2++) {
              const int rr = r0 + k2 * 2;
              pl[cnt*5+0] = (uint8_t)(rr * gw + c);       pl[cnt*5+1] = (uint8_t)(rr * gw + c + 1);
              pl[cnt*5+2] = (uint8_t)((rr+1) * gw + c);   pl[cnt*5+3] = (uint8_t)((rr+1) * gw + c + 1);
              pl[cnt*5+4] = 1; cnt++;
            }
          } else {                                  // two upright I
            for (int k2 = 0; k2 < 2; k2++) {
              for (int r = 0; r < 4; r++) pl[cnt*5+r] = (uint8_t)((r0 + r) * gw + c + k2);
              pl[cnt*5+4] = 0; cnt++;
            }
          }
        } else {
          for (int r = 0; r < 4; r++) pl[cnt*5+r] = (uint8_t)((r0 + r) * gw + c);
          pl[cnt*5+4] = 0; cnt++;
        }
        c += w;
      }
      // land the low pieces first so nothing appears to hang in mid-air
      for (int i2 = first + 1; i2 < cnt; i2++)
        for (int j2 = i2; j2 > first; j2--) {
          int lo1 = 0, lo2 = 0;
          for (int q = 0; q < 4; q++) { const int r1 = pl[j2*5+q] / gw, r2 = pl[(j2-1)*5+q] / gw;
                                        if (r1 > lo1) lo1 = r1; if (r2 > lo2) lo2 = r2; }
          if (lo1 <= lo2) break;
          for (int q = 0; q < 5; q++) { const uint8_t t2 = pl[j2*5+q];
            pl[j2*5+q] = pl[(j2-1)*5+q]; pl[(j2-1)*5+q] = t2; }
        }
    }
    st[2] = (uint8_t)cnt; st[3] = 0; st[1] = 0; st[5] = 0; st[8] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2);
    st[6] = (uint8_t)nowT; st[7] = (uint8_t)(nowT >> 8);
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  const uint16_t t0   = (uint16_t)st[6] | ((uint16_t)st[7] << 8);
  const uint16_t el   = (uint16_t)(nowT - t0);

  const int well = st[4];
  const int hover = (st[1] == 1 || st[1] == 3) ? 1600 : (500 + (255 - (int)SEGMENT.speed) * 3);

  // the piece currently in flight: a plan entry, or one of the two long ones
  uint8_t pc[4]; uint8_t pcol;
  if (st[1] == 0) { for (int q = 0; q < 4; q++) pc[q] = pl[st[3] * 5 + q]; pcol = pl[st[3] * 5 + 4]; }
  else { for (int q = 0; q < 4; q++) pc[q] = (uint8_t)((4 + q) * gw + well); pcol = 0; }

  int minR = 8, maxR = 0;
  for (int q = 0; q < 4; q++) { const int r = pc[q] / gw; if (r < minR) minR = r; if (r > maxR) maxR = r; }
  const int startOff = minR + 4;

  const int topMs = hover / 2;                     // spawn, spin and travel up top
  const bool onTop = (st[1] <= 3) && (el < (uint16_t)topMs);

  if (st[1] == 0 || st[1] == 1 || st[1] == 3) {
    const bool land = !onTop &&
      ((peak && (int)el - topMs > ((st[1] == 0) ? 60 : 400)) || el > (uint16_t)hover);
    int f = startOff;
    if (!onTop) {
      const int fe = (int)el - topMs, fs = hover - topMs;
      f = startOff - (startOff * fe) / (fs ? fs : 1);
      if (f < 0) f = 0;
    }
    st[5] = (uint8_t)f;
    if (land) {
      for (int q = 0; q < 4; q++) fd[pc[q]] = (uint8_t)(pcol + 1);
      st[5] = 0;
      if (st[1] == 0) { if (++st[3] >= st[2]) st[1] = 1; }
      else { st[8] = 255; st[1] = (uint8_t)(st[1] + 1); }
      st[6] = (uint8_t)nowT; st[7] = (uint8_t)(nowT >> 8);
    }
  } else if (st[1] == 2 || st[1] == 4) {                 // rings flash then collapse
    const int f = 255 - (int)el; st[8] = (uint8_t)((f < 0) ? 0 : f);
    if (el > 480) {
      tt_collapse(fd, gw, 4);
      st[1] = (uint8_t)((st[1] == 2) ? 3 : 5);
      st[6] = (uint8_t)nowT; st[7] = (uint8_t)(nowT >> 8);
    }
  }

  const uint8_t drive = cfx_drive(vol, 1.0f, 140 + (SEGMENT.intensity >> 1));
  const int fall = (int)st[5];

  // where this piece leaves the top face, matching the band's own wrap order
  const int tc = B / cw, pw4 = gw / 4;
  const int q4 = (pcol / (pw4 ? pw4 : 1)) & 3, offw = pcol % (pw4 ? pw4 : 1);
  const int mm = (offw * tc) / (pw4 ? pw4 : 1);
  int exX, exY;
  if      (q4 == 0) { exX = mm;          exY = 0;          }
  else if (q4 == 1) { exX = tc - 1;      exY = mm;         }
  else if (q4 == 2) { exX = tc - 1 - mm; exY = tc - 1;     }
  else              { exX = 0;           exY = tc - 1 - mm; }
  const int tprog = onTop ? (((int)el * 256) / (topMs ? topMs : 1)) : 256;
  const int tcx = (tc / 2) + (((exX - tc / 2) * tprog) >> 8);
  const int tcy = (tc / 2) + (((exY - tc / 2) * tprog) >> 8);
  const int prot = (tprog * 4) >> 8;
  int minC = 99, minR3 = 99;
  for (int q = 0; q < 4; q++) { const int cq = pc[q] % gw, rq = pc[q] / gw;
                                if (cq < minC) minC = cq; if (rq < minR3) minR3 = rq; }

  SEGMENT.fill(SEGCOLOR(0));
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++, i++) {
      if (bu[i] == 255) {
        if (!cube || !onTop) continue;
        const int bx = x / B, by = y / B;
        if (bx != 1 || by != 1) continue;
        const int tx = (x % B) / cw, ty = (y % B) / chh;
        for (int q = 0; q < 4; q++) {                   // the piece, mid-spin
          const int dx0 = (int)(pc[q] % gw) - minC, dy0 = (int)(pc[q] / gw) - minR3;
          int rx = dx0, ry = dy0;
          for (int r = 0; r < prot; r++) { const int t2 = rx; rx = -ry; ry = t2; }
          if (tx == tcx + rx && ty == tcy + ry) {
            SEGMENT.setPixelColorXY(x, y, mq_scale(TT_COL[pcol % 7], drive));
            break;
          }
        }
        continue;
      }
      const int gu = (int)bu[i] / cw, gv = (int)bv[i] / chh;
      if (gu >= gw || gv >= gh) continue;
      uint32_t c = 0; bool on = false;

      const uint8_t cell = fd[gv * gw + gu];
      if (cell) { c = TT_COL[(cell - 1) % 7]; on = true; }

      if (!on && !onTop && (st[1] == 0 || st[1] == 1 || st[1] == 3))   // falling
        for (int q = 0; q < 4; q++)
          if ((int)(pc[q] % gw) == gu && (int)(pc[q] / gw) - fall == gv) {
            c = TT_COL[pcol % 7]; on = true; break;
          }

      if (st[8]) {                                    // Tetris flash
        int lvl = (gv >= 4) ? (int)st[8] : (int)st[8] / 5;
        const int sweep = (((int)el * bw * 2) / 480) % bw;
        int du = (int)bu[i] - sweep;
        if (du >  bw / 2) du -= bw;
        if (du < -bw / 2) du += bw;
        if (du < 0) du = -du;
        if (du < 7) lvl += (7 - du) * 36;
        if (lvl > 255) lvl = 255;
        if (lvl > 8) { c = RGBW32(lvl, lvl, (uint8_t)((lvl * 3) / 4), 0); on = true; }
      }

      if (on) SEGMENT.setPixelColorXY(x, y, mq_scale(c, drive));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_TETRIS[] PROGMEM =
  "Ace 3-D Tetris@,Brightness,,,,,,Flat mode;;;2f;ix=180";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_TetrisUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_tetris, _data_FX_MODE_TETRIS);
  }
  void loop() override {}
};

static CubeFx_TetrisUsermod cube_fx_tetris;
REGISTER_USERMOD(cube_fx_tetris);
