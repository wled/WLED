#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 29. QUADRANT LABYRINTH
// ===========================================================================
// Each face is split into four quadrants meeting at the face's centre pixel
// pair. Every quadrant gets its own randomly-carved perfect maze (recursive
// backtracker), rooted at the corner nearest the centre, so a face reads as
// one labyrinth that radiates outward from a single hub - real spanning-tree
// corridors, one pixel wide, no loops.
//
// Distance from a hub is measured in CORRIDOR STEPS (a BFS over the carved
// pixels), not straight-line geometry, so a pulse floods down every dead end
// in lock-step, the way water would find its own path down a real maze. The
// pulse leaves a fading trail of the current palette behind it; a steady
// glow lets you see the corridors the pulse is ABOUT to take.
//
// Net geometry (square, divisible by 3) is detected from the matrix itself
// and always used for face/gap layout - the "Unify net" checkbox no longer
// disables that detection the way "Flat mode" does elsewhere in this family.
// Off, you get five independent per-face mazes, each with its own hub at its
// own face's centre (the normal look). On, all five faces share ONE maze and
// ONE hub at the cube's centre, with corridors free to run from face to face
// - but carving still stops exactly at the edge of each face's B x B square,
// so it never wastes a corridor on, or pokes a pixel into, the four corner
// blocks that don't physically exist on the net.
//
// ---------------------------------------------------------------------------
// Controls, and what they actually do now
// ---------------------------------------------------------------------------
//   Speed      Locked to tempo: "beats to cross the maze", 0.5x .. 4x. Set it
//              past 1 beat and pulses NECESSARILY overlap - a new one launches
//              every beat while older ones are still travelling. Unlocked, it
//              falls back to a flat ms-per-corridor-step.
//   Thickness  Pulse head width in corridor steps.
//   Trail      How long the wake behind a pulse lingers - now a genuinely wide
//              range (about 30 ms at 0 up to ~4 s at 255) instead of the old
//              12..97 band where most of the slider looked identical.
//   Glow       Resting brightness of the whole maze - 0 hides it completely
//              (only the pulse and its wake are visible), 255 lights every
//              corridor. This is now a TARGET level, not a per-frame additive
//              nudge, so Trail no longer swamps it: the two are independent.
//   Spread     Palette cycles across the maze, 0.25x .. 4x, normalised against
//              the maze's own depth so it means the same thing on a 16-pixel
//              face and on a unified net. (Needs an actual palette selected -
//              on a single-colour palette there is nothing to spread.)
//
// ---------------------------------------------------------------------------
// Tempo + drop reactivity (cfx_tempo / cfx_drop from cube_fx_common)
// ---------------------------------------------------------------------------
//   - Once locked, a fresh pulse launches on every PREDICTED beat, so the
//     train keeps a musical cadence through a kick the transient detector
//     missed. Unlocked, it falls back to confirmed fx_lowBeat() hits.
//   - A held bass note reads as cfx_drop().surge and SPEEDS THE MAZE UP -
//     pulses and their wake accelerate together, then ease back when the note
//     lets go. (This used to happen by accident, because a sustained note
//     machine-gunned the tempo estimator into a faster and faster lock. It is
//     now deliberate, bounded and reversible.)
//   - A real DROP - bass gone for a while, mids swelling, then the slam -
//     launches a full-strength pulse, fattens and brightens every pulse in
//     flight while it rings out, and (with "Reshuffle on beat") re-carves the
//     maze on the hit.
//   - A build-up, while it's happening, gently HOLDS THE MAZE BACK below
//     normal speed, so the drop has something to release.
// ---------------------------------------------------------------------------
#define QL_MAXCELL  32         // max maze-cell grid per quadrant (32 -> 64px span)
#define QL_MAXQUEUE 4096       // BFS queue, must cover one flood region's pixel count
#define QL_PULSES   8          // concurrent pulses in flight - see "Speed" above
#define QL_PSZ      6          // bytes per pulse record

static uint8_t  ql_openE[QL_MAXCELL * QL_MAXCELL];
static uint8_t  ql_openS[QL_MAXCELL * QL_MAXCELL];
static uint8_t  ql_visited[QL_MAXCELL * QL_MAXCELL];
static uint16_t ql_stack[QL_MAXCELL * QL_MAXCELL];
static uint16_t ql_queue[QL_MAXQUEUE];

// Recursive backtracker on an mw x mh cell grid, rooted at cell (0,0) - the
// cell nearest the hub. openE[c]/openS[c] mark a carved passage from cell c
// to its +u (east) / +v (south) neighbour in quadrant-local space.
//
// Cells whose pixel position (2*ci, 2*cj) falls at or beyond (cutU, cutV) in
// BOTH axes are pre-marked "blocked" (visited = 2) so the backtracker can
// never enter or carve through them. Pass cutU/cutV >= the quadrant's own
// width/height for "no cutout" (a plain rectangular quadrant); pass smaller
// values to carve an L-shaped region with its outer corner excluded - used
// for the unified whole-net maze, where that corner is a face gap.
static void ql_carve(int mw, int mh, int cutU, int cutV) {
  const int nc = mw * mh;
  for (int k = 0; k < nc; k++) { ql_visited[k] = 0; ql_openE[k] = 0; ql_openS[k] = 0; }
  for (int cj = 0; cj < mh; cj++)
    for (int ci = 0; ci < mw; ci++) {
      if (ci * 2 >= cutU && cj * 2 >= cutV) ql_visited[ci + cj * mw] = 2;   // outside the footprint
    }
  if (ql_visited[0] == 2) return;                 // degenerate: hub cell itself excluded
  int sp = 0;
  ql_visited[0] = 1; ql_stack[sp++] = 0;
  while (sp > 0) {
    const int c = ql_stack[sp - 1];
    const int ci = c % mw, cj = c / mw;
    int nbr[4], dir[4], nn = 0;
    if (ci + 1 < mw && !ql_visited[c + 1])  { nbr[nn] = c + 1;  dir[nn] = 0; nn++; }
    if (cj + 1 < mh && !ql_visited[c + mw]) { nbr[nn] = c + mw; dir[nn] = 1; nn++; }
    if (ci - 1 >= 0 && !ql_visited[c - 1])  { nbr[nn] = c - 1;  dir[nn] = 2; nn++; }
    if (cj - 1 >= 0 && !ql_visited[c - mw]) { nbr[nn] = c - mw; dir[nn] = 3; nn++; }
    if (!nn) { sp--; continue; }
    const int pick = (int)hw_random16((uint16_t)nn);
    const int nx = nbr[pick], d = dir[pick];
    if      (d == 0) ql_openE[c]  = 1;
    else if (d == 1) ql_openS[c]  = 1;
    else if (d == 2) ql_openE[nx] = 1;
    else             ql_openS[nx] = 1;
    ql_visited[nx] = 1;
    ql_stack[sp++] = nx;
  }
}

// Carve one quadrant and stamp its "on-path" pixels into dist[] as 0xFFFE
// (meaning: part of the maze, distance not yet assigned). (ox,oy) is the
// pixel at local (0,0) - the cell nearest the hub - and (dux,dvy) are +1/-1
// steps toward the outer corner of the quadrant. cutU/cutV as in ql_carve.
static void ql_quadrant(uint16_t *dist, int cols, int ox, int oy, int dux, int dvy,
                        int w, int h, int cutU, int cutV) {
  if (w < 1 || h < 1) return;
  int mw = (w + 1) / 2; if (mw > QL_MAXCELL) mw = QL_MAXCELL; if (mw < 1) mw = 1;
  int mh = (h + 1) / 2; if (mh > QL_MAXCELL) mh = QL_MAXCELL; if (mh < 1) mh = 1;
  ql_carve(mw, mh, cutU, cutV);
  for (int cj = 0; cj < mh; cj++)
    for (int ci = 0; ci < mw; ci++) {
      const int c = ci + cj * mw;
      if (ql_visited[c] == 2) continue;             // outside the physical footprint
      const int u0 = ci * 2, v0 = cj * 2;
      dist[(size_t)(oy + dvy * v0) * cols + (ox + dux * u0)] = 0xFFFE;
      if (ql_openE[c] && (u0 + 1) < w)
        dist[(size_t)(oy + dvy * v0) * cols + (ox + dux * (u0 + 1))] = 0xFFFE;
      if (ql_openS[c] && (v0 + 1) < h)
        dist[(size_t)(oy + dvy * (v0 + 1)) * cols + (ox + dux * u0)] = 0xFFFE;
    }
}

// Four quadrants around one hub, then a BFS (confined to [fx0,fx0+fw) x
// [fy0,fy0+fh)) that turns the 0xFFFE stamps into real corridor-step
// distances from the shared hub. Returns the region's max distance.
//
// cutInset > 0 carves an outer cutInset x cutInset corner out of each
// quadrant - used with fw=fh=3*B (the whole net) and cutInset=B, so the
// maze covers the net's true cross footprint instead of the full bounding
// square. cutInset=0 gives a plain rectangle (one face, or a flat panel).
static uint16_t ql_carveAndFlood(uint16_t *dist, int cols, int fx0, int fy0,
                                 int fw, int fh, int cutInset) {
  const int leftW = fw / 2, rightW = fw - leftW;
  const int topH  = fh / 2, botH   = fh - topH;
  if (leftW  > 0 && topH > 0) ql_quadrant(dist, cols, fx0 + leftW - 1, fy0 + topH - 1, -1, -1,
                                          leftW,  topH, leftW  - cutInset, topH - cutInset);
  if (rightW > 0 && topH > 0) ql_quadrant(dist, cols, fx0 + leftW,     fy0 + topH - 1,  1, -1,
                                          rightW, topH, rightW - cutInset, topH - cutInset);
  if (leftW  > 0 && botH > 0) ql_quadrant(dist, cols, fx0 + leftW - 1, fy0 + topH,     -1,  1,
                                          leftW,  botH, leftW  - cutInset, botH - cutInset);
  if (rightW > 0 && botH > 0) ql_quadrant(dist, cols, fx0 + leftW,     fy0 + topH,      1,  1,
                                          rightW, botH, rightW - cutInset, botH - cutInset);

  int hubs[4][2], nh = 0;
  if (leftW  > 0 && topH > 0) { hubs[nh][0] = fx0 + leftW - 1; hubs[nh][1] = fy0 + topH - 1; nh++; }
  if (rightW > 0 && topH > 0) { hubs[nh][0] = fx0 + leftW;     hubs[nh][1] = fy0 + topH - 1; nh++; }
  if (leftW  > 0 && botH > 0) { hubs[nh][0] = fx0 + leftW - 1; hubs[nh][1] = fy0 + topH;     nh++; }
  if (rightW > 0 && botH > 0) { hubs[nh][0] = fx0 + leftW;     hubs[nh][1] = fy0 + topH;     nh++; }

  int qh = 0, qt = 0; uint16_t maxD = 0;
  for (int k = 0; k < nh; k++) {
    const size_t idx = (size_t)hubs[k][1] * cols + hubs[k][0];
    if (dist[idx] == 0xFFFE && qt < QL_MAXQUEUE) { dist[idx] = 0; ql_queue[qt++] = (uint16_t)idx; }
  }
  while (qh < qt) {
    const size_t idx = ql_queue[qh++];
    const int x = (int)(idx % cols), y = (int)(idx / cols);
    const uint16_t d = dist[idx];
    if (d > maxD) maxD = d;
    const int nx4[4] = { x + 1, x - 1, x, x };
    const int ny4[4] = { y, y, y + 1, y - 1 };
    for (int k = 0; k < 4; k++) {
      const int nx = nx4[k], ny = ny4[k];
      if (nx < fx0 || nx >= fx0 + fw || ny < fy0 || ny >= fy0 + fh) continue;
      const size_t nidx = (size_t)ny * cols + nx;
      if (dist[nidx] != 0xFFFE) continue;
      dist[nidx] = (uint16_t)(d + 1);
      if (qt < QL_MAXQUEUE) ql_queue[qt++] = (uint16_t)nidx;
    }
  }
  // any stamped pixel the BFS never reached (shouldn't happen) reads as wall
  for (int y = fy0; y < fy0 + fh; y++)
    for (int x = fx0; x < fx0 + fw; x++) {
      const size_t idx = (size_t)y * cols + x;
      if (dist[idx] == 0xFFFE) dist[idx] = 0xFFFF;
    }
  return maxD;
}

// cube: this matrix is net-shaped (square, divisible by 3) - always decided
//       by geometry alone, never by the "Unify net" checkbox.
// unified: build ONE maze spanning the whole cross-shaped net (one hub) if
//       true, or five independent per-face mazes (five hubs) if false.
static uint16_t ql_build(uint16_t *dist, int cols, int rows, bool cube, bool unified, int B) {
  const size_t n = (size_t)cols * rows;
  for (size_t i = 0; i < n; i++) dist[i] = 0xFFFF;
  uint16_t maxD = 0;
  if (cube && unified) {
    maxD = ql_carveAndFlood(dist, cols, 0, 0, cols, rows, B);
  } else if (cube) {
    static const int8_t FBX[5] = { 1, 1, 1, 0, 2 };   // top, north, south, west, east
    static const int8_t FBY[5] = { 1, 0, 2, 1, 1 };
    for (int f = 0; f < 5; f++) {
      const uint16_t d = ql_carveAndFlood(dist, cols, FBX[f] * B, FBY[f] * B, B, B, 0);
      if (d > maxD) maxD = d;
    }
  } else {
    maxD = ql_carveAndFlood(dist, cols, 0, 0, cols, rows, 0);
  }
  return maxD;
}

static inline int ql_faceIdx(int bx, int by) {
  if (bx == 1 && by == 1) return 0;
  if (bx == 1 && by == 0) return 1;
  if (bx == 1 && by == 2) return 2;
  if (bx == 0 && by == 1) return 3;
  if (bx == 2 && by == 1) return 4;
  return -1;
}

// Pulse cursor is 24-bit Q16.8 corridor steps: 16 bits of whole steps (a
// unified 96px net can exceed 255, which the old 8.8 cursor silently clamped
// to, stalling every pulse at step 255) and 8 bits of fraction.
static inline uint32_t ql_cursor(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}
static inline void ql_setCursor(uint8_t *p, uint32_t c) {
  if (c > 0xFFFFFFu) c = 0xFFFFFFu;
  p[0] = (uint8_t)c; p[1] = (uint8_t)(c >> 8); p[2] = (uint8_t)(c >> 16);
}

static FX_RET mode_quadrant_labyrinth() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 4 || rows < 4) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 64)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint16_t *dist = (uint16_t *)SEGENV.data;
  uint8_t  *st   = (uint8_t *)(dist + n);
  // st[0]      built-flag (1 = per-face net, 2 = flat, 3 = unified net)
  // st[1..2]   maxDist
  // st[3]      next spawn hue
  // st[4..5]   glow dither remainder (see "Glow" below)
  // st[6..10]  five per-face stagger fractions, 0..255 of half the maze depth
  // st[11]     spare
  // st[12..59] QL_PULSES x QL_PSZ pulse records:
  //            [0..2] cursor Q16.8   [3] hue   [4] alive   [5] strength
  uint8_t *pulses = st + 12;

  // Net-shape detection is pure geometry - it does NOT go through cfx_isCube,
  // because that helper folds in "!SEGMENT.check3" and here check3 no longer
  // means "pretend this isn't a cube"; it means "merge the five faces".
  const bool netShape = (cols == rows) && (cols >= 12) && ((cols % 3) == 0);
  const bool cube     = netShape;                       // face layout / gap-skip
  const bool unified  = netShape && SEGMENT.check3;      // "Unify net": one maze
  const int  B        = netShape ? (cols / 3) : 1;

  const uint8_t wantFlag = netShape ? (uint8_t)(unified ? 3 : 1) : (uint8_t)2;
  const bool reset = (SEGENV.call == 0) || (st[0] != wantFlag);

  const uint16_t dtMs = fx_dt(SEGENV.step);
  um_data_t            *um    = cfx_getAudioData();
  const CfxTempoState  &tempo = cfx_tempo(um);          // predicted phase/confidence/bpm
  const CfxDropState   &drop  = cfx_drop(um, tempo);    // build / drop / surge
  const bool locked = tempo.periodMs > 0 && tempo.confidence >= 64;

  // A drop re-carves the maze for the reveal; a very hard ordinary kick
  // occasionally does too, so it doesn't feel like it only ever changes on
  // drops.
  const bool reshuffle = reset ||
    (SEGMENT.check1 && (drop.hit || (tempo.hit > 200 && hw_random16(6) == 0)));

  if (reshuffle) {
    const uint16_t built = ql_build(dist, cols, rows, cube, unified, B);
    st[1] = (uint8_t)(built & 0xFF); st[2] = (uint8_t)(built >> 8);
    st[0] = wantFlag;
    for (int f = 0; f < 5; f++) st[6 + f] = (uint8_t)hw_random16(256);
    for (int k = 0; k < QL_PULSES; k++) pulses[k * QL_PSZ + 4] = 0;   // clear pulses
    st[4] = st[5] = 0;
  }

  const uint16_t maxD     = (uint16_t)st[1] | ((uint16_t)st[2] << 8);
  const uint16_t maxDsafe = maxD ? maxD : 1;

  // --- speed ---------------------------------------------------------------
  // Locked: Speed is "beats to cross the maze", 0.5x .. 4.0x, so the pulse
  // stays musically in time whatever size the maze is - and anything above
  // 1.0 guarantees overlapping pulses, which is the point.
  // Unlocked: flat ms per corridor step, as before.
  int stepMs;
  if (locked) {
    const uint32_t beatsX100  = 50 + ((uint32_t)SEGMENT.speed * 350) / 255;
    const uint32_t traverseMs = ((uint32_t)tempo.periodMs * beatsX100) / 100;
    stepMs = (int)(traverseMs / (uint32_t)maxDsafe);
    if (stepMs < 2) stepMs = 2;
  } else {
    stepMs = 6 + ((255 - (int)SEGMENT.speed) >> 2);
  }

  // Surge speeds everything up, a live build holds it back. speedScale is
  // 256 at rest, so this is a no-op when nothing musical is happening.
  const uint16_t pulseDt = cfx_dropDt(dtMs, drop.speedScale);

  const int     width   = 1 + ((int)SEGMENT.intensity >> 5) + (drop.intensity >> 6);
  const uint8_t glow    = SEGMENT.custom2;                 // full range, was >>2
  const bool    stagger = cube && !unified && SEGMENT.check2;
  // Staggered faces start up to half the maze depth late, so a pulse has to
  // stay alive that much longer or the last faces get cut off mid-flood.
  const int     lifeEnd = (int)maxD + width + 2 + (stagger ? ((int)maxD >> 1) : 0);

  // --- spread, normalised against the maze's own depth ----------------------
  // idx = d * cycles * 256 / maxD, with cycles from 0.25 to 4.0. Doing it
  // relative to maxD is what makes the control mean the same thing on a
  // 16-pixel face and on a unified 48-pixel net; the old raw multiplier made
  // low settings look like no colour variation at all on a shallow maze.
  const uint32_t cyc64     = 16u + ((uint32_t)SEGMENT.custom3 * 240u) / 255u;   // 1/64 cycles
  const uint32_t spreadMul = (cyc64 * 4u * 256u) / (uint32_t)maxDsafe;

  // --- launch a pulse -------------------------------------------------------
  uint8_t spawnStr = 0;
  if (drop.hit) {
    spawnStr = 255;                                        // the drop itself
  } else if (locked ? (tempo.beat != 0) : (tempo.hit != 0)) {
    spawnStr = tempo.hit ? qadd8(120, (uint8_t)(tempo.hit >> 1)) : 190;
  }
  if (spawnStr) {
    int slot = -1, oldestSlot = 0; uint32_t oldest = 0;
    for (int k = 0; k < QL_PULSES; k++) {
      uint8_t *p = pulses + k * QL_PSZ;
      if (!p[4]) { slot = k; break; }
      const uint32_t c = ql_cursor(p);
      if (c >= oldest) { oldest = c; oldestSlot = k; }
    }
    if (slot < 0) slot = oldestSlot;      // all busy: recycle the furthest along
    uint8_t *p = pulses + slot * QL_PSZ;
    ql_setCursor(p, 0);
    p[3] = st[3]; p[4] = 1; p[5] = spawnStr;
    st[3] = (uint8_t)(st[3] + 24);
  }

  // --- advance every live pulse once per frame, not once per pixel ---------
  int     pRadius[QL_PULSES];
  uint8_t pHue[QL_PULSES], pStr[QL_PULSES];
  int     nAct = 0;
  const uint32_t inc = ((uint32_t)pulseDt * 256u) / (uint32_t)stepMs;
  for (int k = 0; k < QL_PULSES; k++) {
    uint8_t *p = pulses + k * QL_PSZ;
    if (!p[4]) continue;
    const uint32_t cursor = ql_cursor(p) + inc;
    ql_setCursor(p, cursor);
    const int radius = (int)(cursor >> 8);
    if (radius > lifeEnd) { p[4] = 0; continue; }
    pRadius[nAct] = radius;
    pHue[nAct]    = p[3];
    pStr[nAct]    = qadd8(p[5], (uint8_t)(drop.intensity >> 2));
    nAct++;
  }

  // --- trail + glow ---------------------------------------------------------
  // Trail sets the fade; Glow then adds back exactly enough per frame that the
  // maze settles AT the requested level. Equilibrium of "add A, fade by f" is
  // 255*A/f, so A = glow*f/255 lands on `glow` for any Trail setting - which
  // is what stops a long Trail from washing Glow out (and a short one from
  // erasing it). The remainder is carried in st[4..5] and dithered across
  // frames so low Glow settings don't quantise straight to zero.
  const int trailKnob = 255 - (int)SEGMENT.custom1;        // 0 = longest wake
  const uint8_t fadeAmt = fx_fade(3 + (trailKnob * trailKnob) / 320, pulseDt);
  SEGMENT.fadeToBlackBy(fadeAmt);

  uint8_t ambInc = 0;
  if (glow) {
    const uint32_t acc = (uint32_t)glow * fadeAmt + ((uint32_t)st[4] | ((uint32_t)st[5] << 8));
    ambInc = (uint8_t)(acc / 255u);
    const uint16_t rem = (uint16_t)(acc % 255u);
    st[4] = (uint8_t)rem; st[5] = (uint8_t)(rem >> 8);
  } else {
    st[4] = st[5] = 0;
  }

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const uint16_t d = dist[i];
      if (d == 0xFFFF) continue;                        // wall / not part of the maze

      const uint8_t idx = (uint8_t)(((uint32_t)d * spreadMul) >> 8);

      if (ambInc)
        SEGMENT.addPixelColorXY(x, y,
          SEGMENT.color_from_palette(idx, false, false, 0, ambInc));

      if (!nAct) continue;

      int off = 0;
      if (stagger) {
        const int fidx = ql_faceIdx(x / B, y / B);
        if (fidx >= 0) off = ((int)st[6 + fidx] * (int)maxD) >> 9;   // 0 .. half the depth
      }

      for (int k = 0; k < nAct; k++) {
        const int rad = pRadius[k] - off;
        if (rad < 0) continue;                          // hasn't reached this face yet
        int e = (int)d - rad; if (e < 0) e = -e;
        if (e > width) continue;
        uint8_t lum = (uint8_t)(255 - (e * 255) / (width + 1));
        lum = scale8(lum, pStr[k]);
        if (!lum) continue;
        SEGMENT.addPixelColorXY(x, y,
          SEGMENT.color_from_palette((uint8_t)(pHue[k] + idx), false, false, 0, lum));
      }
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_QUADRANT_LABYRINTH[] PROGMEM =
  "Ace 3-D Quadrant Labyrinth@Speed,Thickness,Trail,Glow,Spread,Reshuffle on beat,Stagger faces,Unify net;;!;2f;sx=150,ix=110,c1=175,c2=70,c3=140,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_QuadrantLabyrinthUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_quadrant_labyrinth, _data_FX_MODE_QUADRANT_LABYRINTH);
  }
  void loop() override {}
};

static CubeFx_QuadrantLabyrinthUsermod cube_fx_quadrant_labyrinth;
REGISTER_USERMOD(cube_fx_quadrant_labyrinth);
