#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 8. CUBE BLOOM
// ===========================================================================
// Cube Ripples' sibling. Same expanding-shell idea, but every shell is a
// different SHAPE, and the shape is fixed from the spectrum at the instant the
// beat lands - so each kick prints a snapshot of what the music was doing.
//
// A shell's radius varies with direction:
//
//   Reff(n) = R * (1 + a1*T_L1(n.u) + a2*T_L2(n.v))
//
// T_L is a Chebyshev polynomial, which is cos(L * angle) written in terms of
// the cosine itself - so it costs a short recurrence instead of a trig call
// and gives exactly L lobes around axis u. Two terms on two axes is enough for
// peanuts, clovers, six-petal flowers and lopsided blobs, all from one form.
//
// Everything about a bloom is read off the FFT at spawn: lobe counts from the
// loudest bin in each half of the spectrum, lobe depth from mids and treble,
// expansion rate and thickness from how hard the bass hit, hue from the
// dominant bin, and the axes from the bin positions themselves. Spectral
// placement also drops bright hits high on the cube and bass-heavy ones low.
// ---------------------------------------------------------------------------
static inline float cfx_cheb(int L, float c) {
  if (L <= 1) return (L <= 0) ? 1.0f : c;
  float t0 = 1.0f, t1 = c;
  for (int k = 1; k < L; k++) { const float t2 = 2.0f * c * t1 - t0; t0 = t1; t1 = t2; }
  return t1;
}

#define CB_SRC 6
#define CB_REC 20      // bytes per bloom

static FX_RET mode_cube_bloom() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + CB_SRC * CB_REC + 8)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *cx = (int8_t *)SEGENV.data;
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *bl = SEGENV.data + 3 * n;                 // blooms
  uint8_t *gs = bl + CB_SRC * CB_REC;                // [0] flag [1] prev beat [2] rr [3..4] timer

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || gs[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    for (int k = 0; k < CB_SRC * CB_REC; k++) bl[k] = 0;
    gs[0] = (uint8_t)(cube ? 1 : 2); gs[1] = 0; gs[2] = 0; gs[3] = 0; gs[4] = 0;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  const uint16_t nowT = (uint16_t)(strip.now >> 4);      // 16 ms ticks
  const int maxLive = 1 + ((int)SEGMENT.custom3 * (CB_SRC - 1)) / 31;

  const bool rising = peak && !gs[1];
  gs[1] = peak ? 1 : 0;

  bool spawn = rising && SEGMENT.check1;
  if (!SEGMENT.check1) {                                 // free-running fallback
    const uint16_t last = (uint16_t)gs[3] | ((uint16_t)gs[4] << 8);
    if ((uint16_t)(nowT - last) > 40) { spawn = true; }
  }

  if (spawn) {
    int slot = -1;
    for (int k = 0; k < maxLive; k++) if (!bl[k * CB_REC + 5]) { slot = k; break; }
    if (slot < 0) { slot = gs[2] % maxLive; gs[2] = (uint8_t)(gs[2] + 1); }
    uint8_t *o = bl + slot * CB_REC;

    // --- read the spectrum at the moment of the hit ------------------------
    uint8_t bd = 0, bLo = 0, bHi = 8;
    for (uint8_t k = 1; k < 16; k++) if (fft[k] > fft[bd])  bd  = k;
    for (uint8_t k = 1; k < 8;  k++) if (fft[k] > fft[bLo]) bLo = k;
    for (uint8_t k = 9; k < 16; k++) if (fft[k] > fft[bHi]) bHi = k;

    int num = 0, den = 0;
    for (int k = 0; k < 16; k++) { num += (int)fft[k] * k; den += fft[k]; }
    const int centroid = den ? (num * 255) / (den * 15) : 128;

    // --- where it starts ---------------------------------------------------
    const int targetZ = -110 + (centroid * 220) / 255;
    size_t bestI = 0; int bestErr = 1 << 20;
    for (int tries = 0; tries < 6; tries++) {
      const size_t si = (size_t)hw_random16((uint16_t)n);
      if (cube) {
        const int bx = (int)(si % cols) / B, by = (int)(si / cols) / B;
        if (bx != 1 && by != 1) continue;                // gap corner
      }
      if (!SEGMENT.check2) { bestI = si; break; }        // random placement
      int err = (int)cz[si] - targetZ; if (err < 0) err = -err;
      if (err < bestErr) { bestErr = err; bestI = si; }
    }

    // --- shape, straight off the FFT ---------------------------------------
    const uint8_t aAng = (uint8_t)(bLo * 21 + bd * 7);
    const uint8_t eAng = (uint8_t)(bHi * 17 + (uint8_t)mid);
    const uint8_t aAn2 = (uint8_t)(bHi * 29 + (uint8_t)treb);
    const uint8_t eAn2 = (uint8_t)(bd * 23 + (uint8_t)bass);
    const int ce = (int)cos8_t(eAng) - 128, se = (int)sin8_t(eAng) - 128;
    const int ca = (int)cos8_t(aAng) - 128, sa = (int)sin8_t(aAng) - 128;
    const int cf = (int)cos8_t(eAn2) - 128, sf = (int)sin8_t(eAn2) - 128;
    const int cb2 = (int)cos8_t(aAn2) - 128, sb2 = (int)sin8_t(aAn2) - 128;

    int L2v = 2 + ((bHi - 8) % 6);
    int L1v = 2 + (bLo % 6);
    if (L2v == L1v) L2v = 2 + ((L1v - 1) % 6);

    o[0] = (uint8_t)cx[bestI]; o[1] = (uint8_t)cy[bestI]; o[2] = (uint8_t)cz[bestI];
    o[3] = (uint8_t)(nowT & 0xFF); o[4] = (uint8_t)(nowT >> 8);
    o[5] = 1;
    o[6] = (uint8_t)(bd << 4);                                   // hue
    o[7] = (uint8_t)(2 + (bass >> 5));                           // growth 2..9
    o[8] = (uint8_t)((mid > 215) ? 255 : (40 + mid));            // lobe depth 1
    o[9]  = (uint8_t)(int8_t)(((ce * ca) / 128 > 127) ? 127 : (ce * ca) / 128);
    o[10] = (uint8_t)(int8_t)(((ce * sa) / 128 > 127) ? 127 : (ce * sa) / 128);
    o[11] = (uint8_t)(int8_t)((se > 127) ? 127 : se);
    o[12] = (uint8_t)L1v;
    o[13] = (uint8_t)((treb > 235) ? 255 : (20 + treb));         // lobe depth 2
    o[14] = (uint8_t)(int8_t)(((cf * cb2) / 128 > 127) ? 127 : (cf * cb2) / 128);
    o[15] = (uint8_t)(int8_t)(((cf * sb2) / 128 > 127) ? 127 : (cf * sb2) / 128);
    o[16] = (uint8_t)(int8_t)((sf > 127) ? 127 : sf);
    o[17] = (uint8_t)L2v;
    o[18] = (uint8_t)(10 + (bass >> 3));                         // shell thickness
    o[19] = (uint8_t)((bass > 250) ? 255 : (120 + (bass >> 1))); // brightness

    gs[3] = (uint8_t)(nowT & 0xFF); gs[4] = (uint8_t)(nowT >> 8);
  }

  // --- resolve live blooms once per frame ----------------------------------
  const int   spd    = 1 + (SEGMENT.speed >> 4);          // 1..16
  const float lobing = ((float)SEGMENT.custom1 / 255.0f) * 0.60f;
  const int   thickM = 4 + (SEGMENT.intensity >> 3);      // 4..35

  int   bx0[CB_SRC], by0[CB_SRC], bz0[CB_SRC];
  int   bux[CB_SRC], buy[CB_SRC], buz[CB_SRC];
  int   bvx[CB_SRC], bvy[CB_SRC], bvz[CB_SRC];
  int   bL1[CB_SRC], bL2[CB_SRC], bW[CB_SRC];
  float bA1[CB_SRC], bA2[CB_SRC], bR[CB_SRC];
  int32_t blo2[CB_SRC], bhi2[CB_SRC];
  uint8_t bHue[CB_SRC], bLum[CB_SRC];
  int live = 0;

  for (int k = 0; k < maxLive; k++) {
    uint8_t *o = bl + k * CB_REC;
    if (!o[5]) continue;
    const uint16_t born = (uint16_t)o[3] | ((uint16_t)o[4] << 8);
    const int ageT = (int)(uint16_t)(nowT - born);
    const float R = (float)(ageT * (int)o[7] * spd) / 8.0f;
    if (R > 520.0f) { o[5] = 0; continue; }

    const float a1 = ((float)o[8]  / 255.0f) * lobing * 0.62f;
    const float a2 = ((float)o[13] / 255.0f) * lobing * 0.38f;
    const int   w  = 6 + ((int)o[18] * thickM) / 24;

    float loR = R * (1.0f - (a1 + a2)) - (float)w;
    float hiR = R * (1.0f + (a1 + a2)) + (float)w;
    if (loR < 0.0f) loR = 0.0f;

    bx0[live] = (int8_t)o[0]; by0[live] = (int8_t)o[1]; bz0[live] = (int8_t)o[2];
    bux[live] = (int8_t)o[9];  buy[live] = (int8_t)o[10]; buz[live] = (int8_t)o[11];
    bvx[live] = (int8_t)o[14]; bvy[live] = (int8_t)o[15]; bvz[live] = (int8_t)o[16];
    bL1[live] = o[12]; bL2[live] = o[17];
    bA1[live] = a1;    bA2[live] = a2;
    bR[live]  = R;     bW[live]  = w;
    blo2[live] = (int32_t)(loR * loR);
    bhi2[live] = (int32_t)(hiR * hiR);
    bHue[live] = o[6];
    // fade out over the journey
    int lf = (int)o[19] - (int)(R * 0.42f);
    bLum[live] = (uint8_t)((lf < 0) ? 0 : lf);
    live++;
  }

  SEGMENT.fadeToBlackBy(fx_fade(30 + (255 - SEGMENT.custom2) / 3, fx_dt8(gs + 5)));

  const uint8_t drive = cfx_drive(vol, 2.0f, 70);

  uint8_t colBlk[cols];
  if (cube) for (int x = 0; x < cols; x++) colBlk[x] = (uint8_t)(x / B);

  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    const uint8_t byb = cube ? (uint8_t)(y / B) : 1;
    for (int x = 0; x < cols; x++, i++) {
      if (cube && byb != 1 && colBlk[x] != 1) continue;
      const int px = cx[i], py = cy[i], pz = cz[i];

      uint8_t best = 0, bh = 0;
      for (int k = 0; k < live; k++) {
        const int dx = px - bx0[k], dy = py - by0[k], dz = pz - bz0[k];
        const int32_t d2 = (int32_t)dx * dx + (int32_t)dy * dy + (int32_t)dz * dz;
        if (d2 < blo2[k] || d2 > bhi2[k]) continue;      // cheap integer reject

        const float d = sqrtf((float)d2);
        if (d < 1.0f) continue;
        const float inv = 1.0f / (d * 128.0f);
        const float c1 = (float)(dx * bux[k] + dy * buy[k] + dz * buz[k]) * inv;
        const float c2 = (float)(dx * bvx[k] + dy * bvy[k] + dz * bvz[k]) * inv;

        const float t1 = cfx_cheb(bL1[k], (c1 < -1.0f) ? -1.0f : ((c1 > 1.0f) ? 1.0f : c1));
        const float t2 = cfx_cheb(bL2[k], (c2 < -1.0f) ? -1.0f : ((c2 > 1.0f) ? 1.0f : c2));

        float def = 1.0f + bA1[k] * t1 + bA2[k] * t2;
        if (def < 0.25f) def = 0.25f;
        const float Reff = bR[k] * def;

        float off = d - Reff; if (off < 0.0f) off = -off;
        if (off >= (float)bW[k]) continue;
        int l = (int)(255.0f - (off * 255.0f) / (float)bW[k]);
        l = (int)scale8((uint8_t)l, bLum[k]);
        if ((uint8_t)l > best) { best = (uint8_t)l; bh = (uint8_t)(bHue[k] + (int)(t1 * 26.0f)); }
      }

      if (!best) continue;
      SEGMENT.addPixelColorXY(x, y,
        SEGMENT.color_from_palette(bh, false, false, 0, scale8(best, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_BLOOM[] PROGMEM =
  "Ace 3-D Cube Bloom@Speed,Thickness,Lobing,Persistence,Max blooms,Spawn on beat,Spectral placement,Flat mode;;!;2f;sx=110,ix=110,c1=150,c2=150,c3=18,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeBloomUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_bloom, _data_FX_MODE_CUBE_BLOOM);
  }
  void loop() override {}
};

static CubeFx_CubeBloomUsermod cube_fx_cube_bloom;
REGISTER_USERMOD(cube_fx_cube_bloom);
