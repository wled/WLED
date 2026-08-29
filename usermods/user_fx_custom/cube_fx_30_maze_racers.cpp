#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 30. ACE 3-D MAZE RACERS
// ===========================================================================
// A real generated maze on all four walls, spawn at bottom-centre, one exit
// point at top-centre. The top face is split into four triangles by its two
// diagonals - a classic "TV screen" quartering - one triangle per wall, each
// carrying its own maze rooted at the edge that wall's exit lands on, all
// four spiralling in toward a shared goal circle at dead centre.
//
// Mazes are real spanning trees (randomized DFS / recursive backtracker), not
// a texture that looks maze-like - every cell is genuinely reachable from the
// start, so a racer using the right-hand wall-following rule is GUARANTEED to
// solve it, no pathfinding needed. Reaching a wall's top-centre cell is a
// fixed, known coordinate, so the crossover onto the matching top triangle is
// just a position check, not a search.
//
// Rendering treats corridors as circuit traces: a soft ambient pulse fills
// every reachable cell, closed walls between neighbours darken the shared
// edge pixels, and a decaying, per-cell stored hue means a racer's palette
// colour lingers and fades in its wake rather than the trail reading as one
// flat colour for everyone.
// ---------------------------------------------------------------------------
#define MZ_RACERS 16

// Four axis-aligned quadrants per face, each an independent single-path
// spiral coiling in from the face's own outer corner toward the shared
// centre - the classic four-quadrant labyrinth. No branching, no stack
// needed (pure greedy "turn right when blocked" walk), and each quadrant is
// self-contained: a racer only ever has to solve the one quadrant it spawned
// into, never crosses into another.
static void mz_quadspiral(uint8_t *fw, int GW, int GH) {
  static const int dR[4] = {-1, 0, 1, 0}, dC[4] = {0, 1, 0, -1};
  const int QW = GW / 2, QH = GH / 2;
  const int r0[4] = {0,    0,    QH,   QH};
  const int c0[4] = {0,    QW,   0,    QW};
  const int sr[4] = {0,    0,    QH-1, QH-1};
  const int sc[4] = {0,    QW-1, 0,    QW-1};
  const int d0[4] = {1,    2,    0,    3};       // TL east, TR south, BL north, BR west

  for (int q = 0; q < 4; q++) {
    bool vis[256];
    for (int i = 0; i < QW * QH; i++) vis[i] = false;
    int row = sr[q], col = sc[q], d = d0[q], count = 1;
    vis[row * QW + col] = true;
    while (count < QW * QH) {
      bool moved = false;
      for (int turn = 0; turn < 4; turn++) {
        const int nd = (d + turn) & 3;
        const int nr = row + dR[nd], nc = col + dC[nd];
        if (nr < 0 || nr >= QH || nc < 0 || nc >= QW) continue;
        if (vis[nr * QW + nc]) continue;
        const int gr = r0[q] + row, gc = c0[q] + col;
        const int gnr = r0[q] + nr, gnc = c0[q] + nc;
        fw[gr * GW + gc]   |= (uint8_t)(1 << nd);
        fw[gnr * GW + gnc] |= (uint8_t)(1 << ((nd + 2) & 3));
        row = nr; col = nc; d = nd; vis[row * QW + col] = true; count++; moved = true;
        break;
      }
      if (!moved) break;
    }
  }
}

// One free racer slot at a face's spawn cell - always the bottom-left
// quadrant's own starting corner, so every face's racer enters the maze the
// same way. Facing is whichever direction is actually open there.
static void mz_spawn(uint8_t *rc, const uint8_t *wall, int CELLS, int GW, int GH,
                     int face, const uint8_t *faceHue) {
  int slot = -1;
  for (int r = 0; r < MZ_RACERS; r++) if (!rc[r * 6 + 5]) { slot = r; break; }
  if (slot < 0) return;
  uint8_t *R = rc + slot * 6;
  const int row = GH - 1, col = 0;
  R[0] = (uint8_t)face; R[1] = (uint8_t)row; R[2] = (uint8_t)col;
  const uint8_t bits = wall[face * CELLS + row * GW + col] & 0x0F;
  uint8_t f0 = 0; for (uint8_t d = 0; d < 4; d++) if (bits & (1 << d)) { f0 = d; break; }
  R[3] = f0;
  R[4] = (uint8_t)(faceHue[face] + (int)hw_random16(30));
  R[5] = 1;
}

static FX_RET mode_maze_racers() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (cube && B < 8) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const int GW = cube ? (B / 2) : (cols / 2);
  const int GH = cube ? (B / 2) : (rows / 2);
  const int CELLS = GW * GH;
  if (GW < 4 || GH < 4 || (GW % 2) || (GH % 2)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int nFaces = cube ? 5 : 1;

  if (!SEGENV.allocateData((size_t)15 * CELLS + MZ_RACERS * 6 + 16)) {
    SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *wall = SEGENV.data;                     // 5 * CELLS
  uint8_t *trB  = wall + 5 * CELLS;                 // 5 * CELLS  trail brightness
  uint8_t *trH  = trB  + 5 * CELLS;                 // 5 * CELLS  trail hue
  uint8_t *rc   = trH  + 5 * CELLS;                 // MZ_RACERS * 6
  uint8_t *st   = rc   + MZ_RACERS * 6;             // header
  // st: [0] built [1..2] dt clock [3..4] fallback-spawn due [5..6] step due
  //     [7..8] goal beat clock [9] goal beat envelope

  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    for (int k = 0; k < 15 * CELLS; k++) wall[k] = 0;
    for (int f = 0; f < nFaces; f++) mz_quadspiral(wall + f * CELLS, GW, GH);
    for (int k = 0; k < MZ_RACERS * 6; k++) rc[k] = 0;
    for (int k = 1; k < 16; k++) st[k] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2);
    const uint16_t t0 = (uint16_t)strip.now;
    st[3] = (uint8_t)(t0 & 0xFF); st[4] = (uint8_t)(t0 >> 8);
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  const uint16_t dtM  = fx_dt8(st + 1);
  const uint16_t nowT = (uint16_t)strip.now;

  // --- spawn: one attempt per face per qualifying beat, capped by Max racers -
  const uint8_t faceHue[5] = {0, 4, 90, 150, 210};
  int aliveCount = 0;
  for (int r = 0; r < MZ_RACERS; r++) if (rc[r * 6 + 5]) aliveCount++;
  const int maxAlive = 1 + ((int)SEGMENT.intensity * (MZ_RACERS - 1)) / 255;

  const bool spawnQualifies = SEGMENT.check1 ? (peak != 0)
                             : ((uint16_t)(nowT - ((uint16_t)st[3] | ((uint16_t)st[4] << 8))) < 32768);
  if (spawnQualifies) {
    for (int f = 0; f < nFaces && aliveCount < maxAlive; f++) {
      mz_spawn(rc, wall, CELLS, GW, GH, f, faceHue);
      aliveCount++;
    }
    if (!SEGMENT.check1) {
      const uint16_t nx = nowT + (uint16_t)(900 + (255 - (int)SEGMENT.speed) * 4);
      st[3] = (uint8_t)(nx & 0xFF); st[4] = (uint8_t)(nx >> 8);
    }
  }

  // --- beat envelope for the goal beacons ---------------------------------
  const uint16_t dtG = fx_dt8(st + 7);
  if (peak > st[9]) st[9] = peak;
  else { const int f = (int)st[9] - (int)fx_step(10, dtG); st[9] = (uint8_t)((f < 0) ? 0 : f); }

  // --- step every racer -----------------------------------------------------
  const uint16_t stepAt = (uint16_t)st[5] | ((uint16_t)st[6] << 8);
  const bool doStep = (uint16_t)(nowT - stepAt) < 32768;
  // goal: the 2x2 block of cells nearest the centre - the point where all
  // four quadrant spirals end up adjacent to each other
  const int goalR0 = GH / 2 - 1, goalR1 = GH / 2;
  const int goalC0 = GW / 2 - 1, goalC1 = GW / 2;

  if (doStep) {
    const uint16_t nxStep = nowT + (uint16_t)(30 + (255 - (int)SEGMENT.speed) * 3);
    st[5] = (uint8_t)(nxStep & 0xFF); st[6] = (uint8_t)(nxStep >> 8);
    static const int dR[4] = {-1, 0, 1, 0}, dC[4] = {0, 1, 0, -1};
    for (int ri = 0; ri < MZ_RACERS; ri++) {
      uint8_t *R = rc + ri * 6;
      if (!R[5]) continue;
      int face = R[0], row = R[1], col = R[2], facing = R[3];

      uint8_t *fw = wall + face * CELLS;
      const uint8_t bits = fw[row * GW + col] & 0x0F;
      int order[4] = { (facing + 1) & 3, facing, (facing + 3) & 3, (facing + 2) & 3 };
      for (int k = 0; k < 4; k++) {
        if (!(bits & (1 << order[k]))) continue;
        facing = order[k];
        row += dR[facing]; col += dC[facing];
        break;
      }

      uint8_t *ftr = trB + face * CELLS, *fth = trH + face * CELLS;
      const int idx = row * GW + col;
      ftr[idx] = 255; fth[idx] = R[4];

      if (row >= goalR0 && row <= goalR1 && col >= goalC0 && col <= goalC1) R[5] = 0;

      R[0] = (uint8_t)face; R[1] = (uint8_t)row; R[2] = (uint8_t)col; R[3] = (uint8_t)facing;
    }
    const uint8_t fade = fx_fade(10 + (255 - SEGMENT.custom2), dtM);
    for (int k = 0; k < 5 * CELLS; k++) {
      const int f = (int)trB[k] - fade;
      trB[k] = (uint8_t)((f < 0) ? 0 : f);
    }
  }

  // --- render ----------------------------------------------------------------
  const uint8_t drive    = cfx_drive(vol, 1.0f, 170);        // fixed - Brightness slot is now Max racers
  const uint8_t shimRate = 1 + (SEGMENT.custom1 >> 3);
  const uint8_t shimPhase = (uint8_t)((strip.now * shimRate) >> 6);
  const bool mazeVisible = !SEGMENT.check2;                  // check2: Invisible maze
  const int goalPulse = 60 + (SEGMENT.custom3 * 6) + (((int)st[9] * (int)SEGMENT.custom3) >> 5);

  SEGMENT.fill(SEGCOLOR(0));
  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      int face, llx, lly;
      if (cube) {
        const int bx = x / B, by = y / B;
        face = (bx == 1 && by == 1) ? 0 : (by == 0 ? 1 : (by == 2 ? 2 : (bx == 0 ? 3 : 4)));
        llx = x % B; lly = y % B;
      } else { face = 0; llx = x; lly = y; }

      // corridor cells sit at EVEN local pixel coords (2px apart); walls occupy
      // the ODD positions between them - one dedicated pixel per boundary,
      // rather than sharing space with the corridor the way cell-block
      // rendering did.
      const bool xEven = (llx % 2) == 0, yEven = (lly % 2) == 0;
      const int gxPix = GW * 2 - 2, gyPix = GH * 2 - 2;         // physical centre pixel

      if (llx == gxPix && llx <= (cube ? B - 1 : cols - 1) &&
          lly == gyPix && lly <= (cube ? B - 1 : rows - 1) && xEven && yEven) {
        int g = goalPulse; if (g > 255) g = 255;
        SEGMENT.setPixelColorXY(x, y, mq_scale(RGBW32(255, 255, 255, 0), scale8((uint8_t)g, drive)));
        continue;
      }

      uint8_t lum = 0; uint32_t c = 0;

      if (xEven && yEven) {                                    // a corridor cell
        const int cellCol = llx / 2, cellRow = lly / 2;
        if (cellCol >= GW || cellRow >= GH) continue;
        const int cIdx = cellRow * GW + cellCol;
        const uint8_t tb = trB[face * CELLS + cIdx];
        if (tb) {
          c = SEGMENT.color_from_palette(trH[face * CELLS + cIdx], false, false, 0);
          lum = tb;
        }
        for (int ri = 0; ri < MZ_RACERS; ri++) {
          const uint8_t *R = rc + ri * 6;
          if (R[5] && R[0] == (uint8_t)face && R[1] == (uint8_t)cellRow && R[2] == (uint8_t)cellCol) {
            c = SEGMENT.color_from_palette(R[4], false, false, 0); lum = 255; break;
          }
        }
      } else if (!xEven && yEven) {                             // vertical wall
        const int row = lly / 2, c1 = (llx - 1) / 2;
        if (row < GH && c1 + 1 < GW && mazeVisible) {
          const uint8_t bits = wall[face * CELLS + row * GW + c1] & 0x0F;
          if (!(bits & 2)) {                                     // closed (no East bit) = a real wall
            const uint8_t pIdx = (uint8_t)((row + c1) * 3 + shimPhase);
            c = SEGMENT.color_from_palette(pIdx, false, false, 0); lum = 200;
          }
        }
      } else if (xEven && !yEven) {                             // horizontal wall
        const int col = llx / 2, r1 = (lly - 1) / 2;
        if (col < GW && r1 + 1 < GH && mazeVisible) {
          const uint8_t bits = wall[face * CELLS + r1 * GW + col] & 0x0F;
          if (!(bits & 4)) {                                     // closed (no South bit) = a real wall
            const uint8_t pIdx = (uint8_t)((r1 + col) * 3 + shimPhase);
            c = SEGMENT.color_from_palette(pIdx, false, false, 0); lum = 200;
          }
        }
      }
      // corners (!xEven && !yEven) always stay blank

      if (!lum) continue;
      SEGMENT.setPixelColorXY(x, y, mq_scale(c, scale8(lum, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_MAZE_RACERS[] PROGMEM =
  "Ace 3-D Maze Racers@Speed,Max racers,Shimmer speed,Trail length,Goal pulse,Spawn on beat,Invisible maze,Flat mode;;!;2f;sx=140,ix=110,c1=60,c2=170,c3=10,o1=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_MazeRacersUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_maze_racers, _data_FX_MODE_MAZE_RACERS);
  }
  void loop() override {}
};

static CubeFx_MazeRacersUsermod cube_fx_maze_racers;
REGISTER_USERMOD(cube_fx_maze_racers);
