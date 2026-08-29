#include "wled.h"
#include "cube_fx_common.h"
#include "cube_fx_imu.h"

// ===========================================================================
// 41. ACE GYRO SAND
// ===========================================================================
// Liquid's boundary is a straight line. Sand's is a heap - and that one
// difference is the whole effect. Tip the cube and the sand does NOT move
// until you pass its angle of repose, then it avalanches and stops at a
// slope, not a level. Punch it and the crater slumps partway back and stays
// dented. Nothing else in the family behaves like that, because nothing else
// has a material with internal friction.
//
// ---------------------------------------------------------------------------
// HOW IT IS MODELLED
// ---------------------------------------------------------------------------
// A 16x16 height field, one byte per column, stored in the cube's own frame -
// not the room's. Gravity supplies a direction; the field supplies the shape.
//
// Which cube axis counts as "up" is chosen from the dominant component of
// gravity, with hysteresis and a deadband on the sign. Roll the cube past
// about 45 degrees and that choice flips, the parameterisation changes under
// the sand, and the pile dumps into its new arrangement. That is not a
// workaround for the snap - it IS what a box of sand does when you tip it
// past its balance point, and the avalanche timer makes it read that way.
//
// The stability test is done in WORLD terms, which is the only part that is
// subtle. Two neighbouring columns are compared not by their stored heights
// but by where their tops actually sit along world up, which folds in both
// the height difference AND the horizontal offset's contribution once the
// cube is tilted:
//
//     dW = ( (G[a]-G[b]) * upAlongHeightAxis  -  step * upAlongGridAxis ) / 127
//
// Material moves only where |dW| exceeds the repose threshold. Set repose to
// zero and that reduces exactly to "the surface must be world-level", i.e.
// water - verified against theory to the unit. Wind it up and it holds a
// steep dune.
//
// Sub-unit residue moves as SINGLE GRAINS on a probability the flow slider
// sets, rather than being lost to integer truncation. That fixes a real bug
// (small slopes stall forever and the pile freezes mid-collapse) and happens
// to be what sand does anyway - the trickle you hear after the slide stops.
//
// ---------------------------------------------------------------------------
// WHAT YOU SEE
// ---------------------------------------------------------------------------
// The walls show the pile through the glass: only the grid's edge columns are
// visible from outside, which is correct and is why the skyward face doubles
// as a top-down contour view of the interior, shaded by local slope. The face
// gravity points AWAY from is buried solid. The face gravity points toward is
// the missing sixth one, so upright you are looking at the pile through four
// walls, which is exactly right.
//
// ---------------------------------------------------------------------------
// MUSIC
// ---------------------------------------------------------------------------
//   tempo.hit    taps the jar - a short boil that lets the pile slump
//   drop.hit     an impact crater, rim and all, that repose then heals partway
//   imu.shake    grab it and shake and the sand levels, which is the gesture
//                everybody tries first
//   check1       bass keeps it simmering
//
// Crest width is measured in PIXEL PITCHES, not absolute units - the same
// trap Gyro Horizon fell into. On a 16-pixel face the surface line would
// otherwise fall between rows and vanish at exactly the wrong moments.
//
// WITH NO SENSOR gravity reads as straight down the cube's own Z, the sand
// sits flat at the fill level, and the beat still stirs it.
// ===========================================================================

#define SD_GX     16                 // height field is SD_GX x SD_GX columns
#define SD_STEP   16                 // one grid cell in cube-coordinate units

static FX_RET mode_gyro_sand() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(16 + 3 * n + 16 + SD_GX * SD_GX)) {
    SEGMENT.fill(SEGCOLOR(0)); FX_DONE;
  }

  int16_t *ph = (int16_t *)SEGENV.data;
  int8_t  *cx = (int8_t *)(ph + 8);
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *st = (uint8_t *)(cz + n);         // st[0] built  [1] axis code
                                             // [2..3] clock [4] boil [5] flare
  uint8_t *G  = st + 16;                     // the height field

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  const CfxImuState &imu = cfx_imu();
  const int U[3] = { imu.valid ? (int)imu.ux : 0,
                     imu.valid ? (int)imu.uy : 0,
                     imu.valid ? (int)imu.uz : 127 };

  const uint8_t want = (uint8_t)(cube ? 1 : 2);
  if (SEGENV.call == 0 || st[0] != want) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    for (int k = 0; k < 8; k++) ph[k] = 0;
    for (int k = 0; k < 16; k++) st[k] = 0;
    st[0] = want;
    st[1] = 4;                               // start assuming +Z is up
    const uint8_t seed = (uint8_t)(((int)SEGMENT.custom1 * 210) / 255);
    for (int i = 0; i < SD_GX * SD_GX; i++) G[i] = seed;
  }

  int dt = (int)fx_dt8(st + 2);
  if (dt > 60) dt = 60;

  // --- which way is down, with hysteresis ----------------------------------
  int hIdx = st[1] >> 1;
  int hSgn = (st[1] & 1) ? -1 : 1;
  {
    const int cur = (U[hIdx] < 0) ? -U[hIdx] : U[hIdx];
    for (int j = 0; j < 3; j++) {
      const int m = (U[j] < 0) ? -U[j] : U[j];
      if (j != hIdx && m > cur + 26) hIdx = j;
    }
    if (U[hIdx] > 20) hSgn = 1; else if (U[hIdx] < -20) hSgn = -1;
    const uint8_t code = (uint8_t)(hIdx * 2 + ((hSgn < 0) ? 1 : 0));
    if (code != st[1]) { st[1] = code; st[4] = 220; }   // the tipping-point dump
  }
  const int uIdx = (hIdx == 0) ? 1 : 0;
  const int vIdx = (hIdx == 2) ? 1 : 2;
  int hUp = hSgn * U[hIdx];
  if (hUp < 40) hUp = 40;                    // it is the dominant axis, so >=73
                                             // in practice - this only guards
                                             // the divide during a sensor glitch
  const int uUp = U[uIdx], vUp = U[vIdx];

  // --- audio ---------------------------------------------------------------
  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);
  const CfxTempoState &tempo = cfx_tempo(um);
  const CfxDropState  &drop  = cfx_drop(um, tempo);

  uint8_t knock = tempo.hit;
  if (imu.shake > knock) knock = imu.shake;
  if (knock > st[4]) st[4] = knock;                    // tap it and it slumps
  if (imu.shake > 90) st[4] = 255;                     // shaken, not stirred
  if (SEGMENT.check1 && (uint8_t)(bass >> 1) > st[4]) st[4] = (uint8_t)(bass >> 1);
  { const int f = (int)st[4] - (int)fx_step(7, (uint16_t)dt);
    st[4] = (uint8_t)((f < 0) ? 0 : f); }
  if (knock > st[5]) st[5] = knock;
  { const int f = (int)st[5] - (int)fx_step(12, (uint16_t)dt);
    st[5] = (uint8_t)((f < 0) ? 0 : f); }

  // --- an impact on the drop -----------------------------------------------
  if (drop.hit && SEGMENT.check2) {
    const int c0 = (int)hw_random16(SD_GX), c1 = (int)hw_random16(SD_GX);
    const int amt = 40 + ((int)drop.hit >> 2);
    for (int dy = -3; dy <= 3; dy++)
      for (int dx = -3; dx <= 3; dx++) {
        const int gx = c0 + dx, gy = c1 + dy;
        if (gx < 0 || gx >= SD_GX || gy < 0 || gy >= SD_GX) continue;
        const int d = ((dx < 0) ? -dx : dx) + ((dy < 0) ? -dy : dy);
        const int i = gy * SD_GX + gx;
        int v = G[i];
        if (d <= 1)      v -= amt;                     // the pit
        else if (d <= 3) v += amt / 2;                 // the rim it threw up
        G[i] = (uint8_t)((v < 0) ? 0 : ((v > 255) ? 255 : v));
      }
  }

  // --- relaxation: repose, tilt slide, and the grain trickle ---------------
  const int repose = 2 + (((int)SEGMENT.custom2 * 30) / 255);
  const int rep8   = repose * 8;
  int flow = 30 + ((int)SEGMENT.speed >> 1) + ((int)st[4] >> 1);
  flow = (flow * dt) / 23;                             // time-based, not frame-based
  if (flow > 255) flow = 255;
  {
    // Alternating sweep direction each frame, so the scan order does not bias
    // which way material creeps on a level field.
    const bool fwd = ((st[2] & 1) == 0);
    for (int q = 0; q < SD_GX; q++) {
      const int gv = fwd ? q : (SD_GX - 1 - q);
      for (int r = 0; r < SD_GX; r++) {
        const int gu = fwd ? r : (SD_GX - 1 - r);
        const int a = gv * SD_GX + gu;
        for (int dir = 0; dir < 2; dir++) {
          const int gu2 = gu + (dir == 0), gv2 = gv + (dir == 1);
          if (gu2 >= SD_GX || gv2 >= SD_GX) continue;
          const int b = gv2 * SD_GX + gu2;
          const int perp = (dir == 0) ? uUp : vUp;
          // where the two column tops sit along WORLD up, x8 for precision -
          // truncating this at x1 quietly eats half the slope
          const int dW8 = ((((int)G[a] - (int)G[b]) * hUp - SD_STEP * perp) * 8) / 127;
          const int exc = (dW8 > rep8) ? (dW8 - rep8)
                        : ((dW8 < -rep8) ? (dW8 + rep8) : 0);
          if (!exc) continue;
          const int num = (exc * 127 * flow) / (hUp * 32);   // 1/255 of a unit
          int m = num / 255;
          if (m == 0) {                                      // single grains
            const int e = (num < 0) ? -num : num;
            if ((int)hw_random16(255) < e) m = (num > 0) ? 1 : -1;
          }
          if (m > 0) { if (m > (int)G[a]) m = G[a];
                       if ((int)G[b] + m > 255) m = 255 - G[b]; }
          if (m < 0) { if (-m > (int)G[b]) m = -(int)G[b];
                       if ((int)G[a] - m > 255) m = -(255 - (int)G[a]); }
          G[a] = (uint8_t)((int)G[a] - m);
          G[b] = (uint8_t)((int)G[b] + m);
        }
      }
    }
  }

  // --- how much sand there is ----------------------------------------------
  {
    const int fillT = ((int)SEGMENT.custom1 * 210) / 255;
    long sum = 0;
    for (int i = 0; i < SD_GX * SD_GX; i++) sum += G[i];
    const int mean = (int)(sum >> 8);
    int d = fillT - mean;
    if (d < -1 || d > 1) {                             // deadband, or it hunts
      int adj = d / 8;
      if (adj == 0) adj = (d > 0) ? 1 : -1;
      for (int i = 0; i < SD_GX * SD_GX; i++) {
        const int v = (int)G[i] + adj;
        G[i] = (uint8_t)((v < 0) ? 0 : ((v > 255) ? 255 : v));
      }
    }
  }

  // --- render ---------------------------------------------------------------
  const int stepPx = cube ? B : ((rows > 1) ? rows : 2);
  int crest = (254 / stepPx) / 2 + 2;
  if (crest < 3) crest = 3;
  int topWin = hUp * 2;                                // top view fades as the
  if (topWin > 255) topWin = 255;                      // face turns away
  const int grain = (int)SEGMENT.custom3;
  const int lift  = (int)st[5] >> 3;                   // crest flares on a kick
  const uint8_t drive = cfx_drive(vol, 1.0f, 120 + (SEGMENT.intensity >> 1));

  SEGMENT.fill(SEGCOLOR(0));
  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int p3[3] = { cx[i], cy[i], cz[i] };

      const int hc = 127 + hSgn * p3[hIdx];            // 0 buried floor, 254 lid
      int gu = (p3[uIdx] + 128) >> 4; if (gu < 0) gu = 0; else if (gu > 15) gu = 15;
      int gv = (p3[vIdx] + 128) >> 4; if (gv < 0) gv = 0; else if (gv > 15) gv = 15;
      const int surf = G[gv * SD_GX + gu];

      int lum, pi;
      if (hc >= 250) {
        // the skyward face: looking down on the dune, lit from one side by
        // its own local slope
        const int wIdx = gv * SD_GX + (gu > 0 ? gu - 1 : gu);
        const int slope = surf - (int)G[wIdx];
        lum = 40 + (surf * 150) / 255 + slope * 3;
        lum = (lum * topWin) >> 8;
        pi  = 30 + (surf * 200) / 255;
      } else {
        const int depth = surf - hc;
        if (depth < -crest) continue;                  // air
        if (depth <= crest) {                          // the surface itself
          lum = 235 + lift;
          pi  = 200;
        } else {                                       // buried
          const int dd = (depth > 200) ? 200 : depth;
          lum = 165 - dd / 3;
          pi  = 50 + (dd * 3) / 4;
        }
      }

      if (grain) {                                     // texture that rides the
        const int s = (int)sin8_t((uint8_t)(gu * 37 + gv * 53                // sand,
                                          + (surf - hc) * 11)) - 128;        // not the cube
        lum += (s * grain) >> 9;
      }
      if (lum <= 0) continue;
      if (lum > 255) lum = 255;
      if (pi < 0) pi = 0; else if (pi > 255) pi = 255;

      const uint32_t c = SEGMENT.color_from_palette((uint8_t)pi, false, false, 0);
      SEGMENT.setPixelColorXY(x, y, mq_scale(c, scale8((uint8_t)lum, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_GYRO_SAND[] PROGMEM =
  "Ace Gyro Sand@Flow rate,Glow,Fill,Repose,Grain,Bass stirs,Impact craters,Flat mode;;!;2f;sx=120,ix=128,c1=120,c2=90,c3=110,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_GyroSandUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_gyro_sand, _data_FX_MODE_GYRO_SAND);
  }
  void loop() override {}
};

static CubeFx_GyroSandUsermod cube_fx_gyro_sand;
REGISTER_USERMOD(cube_fx_gyro_sand);
