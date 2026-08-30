#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 35. ACE 3-D DNA HELIX
// ===========================================================================
// A two-oscillator sine generator that happens to look like a molecule.
//
// Everything is drawn in one abstract coordinate pair, built once per segment:
//
//   u  0..255   position ALONG the helix axis
//   v  0..255   position ACROSS it
//
// and the whole effect is then three lines of trigonometry:
//
//   ang     = u * frequency + phase          the carrier
//   strandA = centre + amp * sin(ang)
//   strandB = centre - amp * sin(ang)        the same wave, 180 deg out
//
// Two sine waves in antiphase ARE a double helix seen side-on, which is why
// this reads as DNA rather than as a pair of scrolling sines: cos(ang) tells
// you which strand is nearer the viewer at that point along the axis, so the
// near strand is drawn brighter and the crossings resolve the way they do in
// a real helix instead of just intersecting flatly.
//
// What (u,v) MEANS is the only thing that changes between geometries:
//
//   cube net   u = the angle around the cube's vertical axis (0..255 for one
//              full lap, arc-length uniform, folded through all four walls
//              exactly like cfx_buildBand so the wave closes with no seam at
//              any vertical corner), v = geodesic distance from the CENTRE OF
//              THE TOP FACE outward - across the top face, over the rim, and
//              on down the wall to the open bottom edge. One continuous ruler
//              over all five faces, so a strand that swings far enough simply
//              climbs over the top rim and arcs across the top face.
//
//   flat       u = the panel's long axis, v = its short axis. Same generator,
//              unrolled - the classic textbook DNA ribbon.
//
// Because u wraps on the cube, the frequency is SNAPPED TO WHOLE TURNS there
// (a fractional lap would leave a visible tear at u = 0). Flat panels have no
// wrap, so they keep the full continuous range.
//
// Controls
//   Speed        how fast the structure travels along u. On the cube that
//                reads as the molecule rotating about its vertical axis.
//   Helix width  base amplitude. Low = a tight belt around the walls, high =
//                strands sweeping from near the top-face centre to the bottom.
//   Frequency    turns of the helix over the full axis (whole turns on cube).
//   LFO rate     0 = LFO off. Otherwise ~0.05 Hz .. ~3 Hz.
//   LFO depth    how far the LFO swings the amplitude (breathing), plus a
//                smaller sway on the carrier phase so it never looks like a
//                pure tremolo.
//   Base pairs   the rungs. Spaced by angle, not by pixels, so they travel
//                with the wave; density steps 8 / 4 / 2 per turn to hold a
//                roughly constant gap in PIXELS as the helix winds tighter
//                (DNA_PAIR_SPACING_PX). Lengths vary with |sin| exactly as
//                they do in the diagrams, shaded front-to-back along their
//                length, and the whole ladder lifts on each kick.
//   Pairs on beat  swaps that fixed ladder for spawned ones: every confirmed
//                kick releases a new base pair at the wave's current phase,
//                which then rides the helix and fades over a couple of
//                seconds. Dense passages crowd the ladder, quiet ones thin it
//                out to nothing. Needs actual kicks - with no audio, leave
//                this off.
//
// Audio is always on: the bass envelope opens the helix, volume sets overall
// brightness, and a kick adds a short swell. There is no audio bypass - a
// silent room reads as a dim, slowly breathing helix rather than a bright one.
// ---------------------------------------------------------------------------

// --- tunables (all -D overridable) -----------------------------------------
#ifndef DNA_AMP_MIN
  #define DNA_AMP_MIN 15       // amplitude at Helix width = 0, in v units
#endif
#ifndef DNA_AMP_SPAN
  #define DNA_AMP_SPAN 110     // added at Helix width = 255
#endif
#ifndef DNA_CENTRE_CUBE
  #define DNA_CENTRE_CUBE 150  // v of the axis on the cube: upper wall
#endif
#ifndef DNA_CENTRE_FLAT
  #define DNA_CENTRE_FLAT 128  // v of the axis on a flat panel: dead centre
#endif
#ifndef DNA_LFO_SWING
  #define DNA_LFO_SWING 150    // smaller = deeper breathing at full LFO depth
#endif
#ifndef DNA_SWAY
  #define DNA_SWAY 28          // carrier phase sway at full LFO depth, angle units
#endif
#ifndef DNA_BASS_AMP
  #define DNA_BASS_AMP 30      // amplitude the bass envelope can add
#endif
#ifndef DNA_BEAT_AMP
  #define DNA_BEAT_AMP 24      // extra swell on a confirmed kick
#endif
#ifndef DNA_BEAT_RELEASE
  #define DNA_BEAT_RELEASE 220 // ms
#endif
#ifndef DNA_STRAND_PX8
  #define DNA_STRAND_PX8 7     // strand half-width, in EIGHTHS of a pixel
#endif
#ifndef DNA_HUE_B
  #define DNA_HUE_B 128        // palette offset of the second strand
#endif
#ifndef DNA_PAIR_SPACING_PX
  #define DNA_PAIR_SPACING_PX 4  // aim for a base pair every ~4 px along the axis
#endif
#ifndef DNA_PAIRS
  #define DNA_PAIRS 8          // beat-spawned pairs alive at once
#endif
#ifndef DNA_PAIR_RELEASE
  #define DNA_PAIR_RELEASE 1800 // ms for a spawned pair to fade out
#endif
#ifndef DNA_PAIR_BEAT_LIFT
  #define DNA_PAIR_BEAT_LIFT 55 // how much a kick brightens the fixed ladder
#endif

// state block, appended after the two coordinate LUTs
#define DNA_ST_MODE 0
#define DNA_ST_CLK  1          // + 2, fx_dt8
#define DNA_ST_PH   3          // + 4, carrier phase Q8
#define DNA_ST_LFO  5          // + 6, LFO phase Q8
#define DNA_ST_BEAT 7
#define DNA_ST_BASS 8
#define DNA_ST_RR   9          // round robin, for when every pair slot is busy
#define DNA_ST_PAIR 10         // DNA_PAIRS x (angle, level)
#define DNA_ST_LEN  (DNA_ST_PAIR + DNA_PAIRS * 2)

// ---------------------------------------------------------------------------
// The one piece of geometry this effect owns: (u,v) for every pixel.
//
// Walls use the same fold order as cfx_buildBand, so u agrees with every other
// effect that walks the ring. The top face is the part cfx_buildBand does not
// have: its u is the SQUARE angle (which edge of the face you would leave by,
// and where along it), which meets each wall's u exactly at the rim, and its v
// continues the wall's ruler inward instead of stopping at it.
//
// If a second effect ever wants this mapping, promote it to cube_fx_common.h
// rather than copying it.
// ---------------------------------------------------------------------------
static void dna_buildUV(uint8_t *pu, uint8_t *pv, int cols, int rows,
                        bool cube, int B, bool axisX) {
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      const size_t i = (size_t)y * cols + x;

      if (!cube) {                                   // flat: long axis is u
        const int uPix = axisX ? x : y;
        const int vPix = axisX ? y : x;
        const int uMax = (axisX ? cols : rows) - 1;
        const int vMax = (axisX ? rows : cols) - 1;
        pu[i] = (uint8_t)((uMax > 0) ? (uPix * 255) / uMax : 0);
        pv[i] = (uint8_t)((vMax > 0) ? (vPix * 255) / vMax : 128);
        continue;
      }

      const int bx = x / B, by = y / B, lx = x % B, ly = y % B;

      if (bx != 1 && by != 1) { pu[i] = 0; pv[i] = 255; continue; }   // gap corner

      if (bx == 1 && by == 1) {                      // TOP face
        const float a = 2.0f * (lx + 0.5f) / B - 1.0f;   //  X
        const float b = 2.0f * (ly + 0.5f) / B - 1.0f;   // -Y
        const float aa = fabsf(a), ab = fabsf(b);
        float u01;
        if (ab >= aa) u01 = (b < 0.0f) ? ((a + 1.0f) * 0.125f)             // north edge
                                       : (0.5f + (1.0f - a) * 0.125f);    // south edge
        else          u01 = (a > 0.0f) ? (0.25f + (b + 1.0f) * 0.125f)     // east edge
                                       : (0.75f + (1.0f - b) * 0.125f);    // west edge
        const float r = (aa > ab) ? aa : ab;         // square radius, 0 centre .. 1 rim
        int uq = (int)(u01 * 256.0f);
        pu[i] = (uint8_t)(uq & 0xFF);
        pv[i] = (uint8_t)(int)(r * 85.0f);           // rim lands on 85
        continue;
      }

      int bu, bv;                                    // walls, cfx_buildBand order
      if      (by == 0) { bu = lx;                     bv = B - 1 - ly; }  // NORTH
      else if (bx == 2) { bu = B + ly;                 bv = lx;         }  // EAST
      else if (by == 2) { bu = 2 * B + B - 1 - lx;     bv = ly;         }  // SOUTH
      else              { bu = 3 * B + B - 1 - ly;     bv = B - 1 - lx; }  // WEST

      pu[i] = (uint8_t)(((bu * 2 + 1) * 128) / (4 * B));   // cell centre on the ring
      pv[i] = (uint8_t)(85 + ((bv * 2 + 1) * 85) / B);     // 85 at the rim, 255 at the bottom
    }
  }
}

// Stamp one base pair into the angle map, keeping whichever pair is nearest.
static inline void dna_paintPair(uint8_t *rdist, uint8_t *rlev,
                                 uint8_t centre, uint8_t level, int fillR) {
  for (int d = -fillR; d <= fillR; d++) {
    const uint8_t a  = (uint8_t)(centre + d);
    const int     ad = (d < 0) ? -d : d;
    if (ad < (int)rdist[a]) { rdist[a] = (uint8_t)ad; rlev[a] = level; }
  }
}

static FX_RET mode_dna_helix() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 4 || rows < 4) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 512 + DNA_ST_LEN)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *pu    = SEGENV.data;
  uint8_t *pv    = pu + n;
  uint8_t *rdist = pv + n;         // 256: angle -> distance to the nearest pair
  uint8_t *rlev  = rdist + 256;    // 256: angle -> that pair's brightness
  uint8_t *st    = rlev + 256;

  const bool cube  = cfx_isCube(cols, rows);
  const int  B     = cube ? (cols / 3) : 1;
  const bool axisX = (cols >= rows);

  const uint8_t marker = cube ? 1 : (axisX ? 2 : 3);
  if (SEGENV.call == 0 || st[DNA_ST_MODE] != marker) {
    dna_buildUV(pu, pv, cols, rows, cube, B, axisX);
    for (int k = 1; k < DNA_ST_LEN; k++) st[k] = 0;
    st[DNA_ST_MODE] = marker;
  }

  const uint16_t dt = fx_dt8(st + DNA_ST_CLK);

  // --- carrier ---------------------------------------------------------------
  uint16_t ph = (uint16_t)st[DNA_ST_PH] | ((uint16_t)st[DNA_ST_PH + 1] << 8);
  ph = (uint16_t)(ph + (uint16_t)fx_step((int32_t)SEGMENT.speed * 7, dt));
  st[DNA_ST_PH]     = (uint8_t)(ph & 0xFF);
  st[DNA_ST_PH + 1] = (uint8_t)(ph >> 8);

  // --- LFO -------------------------------------------------------------------
  // One sine oscillator, ~0.05 Hz at rate 1 up to ~3 Hz at 255. Rate 0 parks
  // it and forces the swing to zero so "LFO depth" cannot do anything on its
  // own - a stopped LFO should mean a still helix, not a frozen offset one.
  int swing = 0;
  if (SEGMENT.custom2) {
    uint16_t lp = (uint16_t)st[DNA_ST_LFO] | ((uint16_t)st[DNA_ST_LFO + 1] << 8);
    lp = (uint16_t)(lp + (uint16_t)fx_step(60 + (int32_t)SEGMENT.custom2 * 17, dt));
    st[DNA_ST_LFO]     = (uint8_t)(lp & 0xFF);
    st[DNA_ST_LFO + 1] = (uint8_t)(lp >> 8);
    const int m = (int)sin8_t((uint8_t)(lp >> 8)) - 128;         // -128..127
    swing = ((int)SEGMENT.custom3 * m) >> 8;                     // -127..127
  }

  // --- audio -----------------------------------------------------------------
  // Always on now. The bass envelope opens the helix, volume sets overall
  // brightness, and a confirmed kick adds a short swell on top - the same
  // behaviour the old "Audio drive" checkbox used to gate.
  um_data_t     *um  = cfx_getAudioData();
  const uint8_t *fft = (uint8_t *)um->u_data[2];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);
  const uint8_t hit = fx_lowBeat(um);
  st[DNA_ST_BASS] = fx_env(st[DNA_ST_BASS], (uint8_t)bass, dt, 300);
  st[DNA_ST_BEAT] = fx_env(st[DNA_ST_BEAT], hit, dt, DNA_BEAT_RELEASE);
  const int audioAmp = (((int)st[DNA_ST_BASS] * DNA_BASS_AMP) >> 8)
                     + (((int)st[DNA_ST_BEAT] * DNA_BEAT_AMP) >> 8);
  const uint8_t drive = cfx_drive(*(float *)um->u_data[0], 1.1f, 120);

  // --- generator parameters --------------------------------------------------
  const int ampBase = DNA_AMP_MIN + (((int)SEGMENT.intensity * DNA_AMP_SPAN) >> 8);
  int       amp = ampBase + ((ampBase * swing) / DNA_LFO_SWING) + audioAmp;
  if (amp < 4)   amp = 4;
  if (amp > 132) amp = 132;

  int twistQ8 = 128 + (int)SEGMENT.custom1 * 7;          // 0.5 .. 7.5 turns, Q8
  if (cube) {                                            // whole laps only, or it tears
    twistQ8 = ((twistQ8 + 128) >> 8) << 8;
    if (twistQ8 < 256) twistQ8 = 256;
  }

  const uint8_t phase8 = (uint8_t)((ph >> 8) + (uint8_t)((swing * DNA_SWAY) >> 7));
  const int     centre = cube ? DNA_CENTRE_CUBE : DNA_CENTRE_FLAT;

  // --- pixel scales ----------------------------------------------------------
  // Widths are authored in pixels and converted here, so the helix looks the
  // same on a 16 px face as it does on a 32 px one.
  const int uPix  = cube ? (4 * B) : ((axisX ? cols : rows) - 1);
  const int vPix  = cube ? ((3 * B) / 2) : ((axisX ? rows : cols) - 1);
  const int vppQ8 = (255 * 256) / (vPix > 0 ? vPix : 1);          // v units per pixel

  const int wPix8 = DNA_STRAND_PX8;                                // half-width, 1/8 px
  int wV = (wPix8 * vppQ8) >> 11;                                 // ...same, in v units
  if (wV < 3) wV = 3;

  // Angle units per pixel travelled along u. Constant on a flat panel and on
  // the walls; on the top face one pixel is worth more and more of the lap as
  // you approach the centre, which is why it gets recomputed per pixel there.
  const int baseDangQ8 = cube ? (int)(((int32_t)65536 / (4 * B) * twistQ8) >> 8)
                              : (int)(((int32_t)65536 / (uPix > 0 ? uPix : 1) * twistQ8) >> 8);

  const int angPerPxQ4 = (twistQ8 << 4) / (uPix > 0 ? uPix : 1);

  // Rung spacing. Rungs sit at fixed angles, not at fixed pixel intervals, so
  // that they travel with the wave instead of sliding through it - and on the
  // cube the spacing has to divide the 256-unit lap exactly or the ladder would
  // not close at the seam. That leaves powers of two: 8, 4 or 2 per turn,
  // whichever is the densest that still leaves ~4.5 px between rungs.
  //
  // The coarser tiers are offset on purpose. A rung every quarter turn lands
  // only on crossings and on widest points, so every rung comes out either
  // zero length or full length; offset by an eighth of a turn they alternate
  // short/long instead, the way base pairs do in the diagrams.
  const int a4     = (angPerPxQ4 > 0) ? angPerPxQ4 : 1;
  const int wantQ4 = DNA_PAIR_SPACING_PX * 16;                    // target gap, 1/16 px
  int rungMask = 31, rungOff = 0;
  if ((32 << 8) / a4 < wantQ4) { rungMask = 63;  rungOff = 16; }
  if ((64 << 8) / a4 < wantQ4) { rungMask = 127; rungOff = 32; }
  const int rungMid = (rungMask + 1) >> 1;

  int rungHalf = (angPerPxQ4 * 12) >> 8;                          // ~0.75 px each side
  if (rungHalf < 2)            rungHalf = 2;
  if (rungHalf > rungMid - 2)  rungHalf = rungMid - 2;

  // Loop invariants the fast reject needs: the widest the strand band can get,
  // and the rung half-width everywhere except the top face.
  const int wMax = wV * 4;
  int rhBase = rungHalf;
  if (rhBase < (baseDangQ8 >> 9)) rhBase = baseDangQ8 >> 9;
  if (rhBase > rungMid - 1)       rhBase = rungMid - 1;

  // --- base pair map ---------------------------------------------------------
  // A 256-entry map from carrier angle to "distance to the nearest base pair"
  // and that pair's brightness. Building this once a frame instead of testing a
  // modulo per pixel is what lets the pairs come from anywhere - a fixed ladder
  // or a list of beat-spawned ones - without the pixel loop caring which. It is
  // also cheaper than the modulo it replaced: 256 entries against 2304 pixels.
  //
  // Angles here are the wave's OWN coordinate, so a pair pinned to an angle
  // rides the helix for free - it travels, sways and rotates with it, and on
  // the cube it appears once per turn all the way round the lap.
  const bool rungs     = SEGMENT.check1;
  const bool beatPairs = SEGMENT.check2;
  const int  fillR     = rungMid - 1;

  for (int k = 0; k < DNA_PAIRS; k++) {              // age outside the mode test,
    uint8_t *e = st + DNA_ST_PAIR + k * 2;           // so nothing stale pops back
    e[1] = fx_env(e[1], 0, dt, DNA_PAIR_RELEASE);    // in when a mode is re-armed
  }

  if (rungs) {
    for (int k = 0; k < 256; k++) { rdist[k] = 255; rlev[k] = 0; }

    if (beatPairs) {
      if (hit) {                                     // one new pair per kick
        int slot = -1;
        for (int k = 0; k < DNA_PAIRS; k++)
          if (!st[DNA_ST_PAIR + k * 2 + 1]) { slot = k; break; }
        if (slot < 0) { slot = st[DNA_ST_RR] % DNA_PAIRS; st[DNA_ST_RR]++; }
        uint8_t *e = st + DNA_ST_PAIR + slot * 2;
        e[0] = phase8;                               // enters where the wave is now
        e[1] = (uint8_t)(120 + (hit >> 1));          // proportional to the kick
      }
      for (int k = 0; k < DNA_PAIRS; k++) {
        const uint8_t *e = st + DNA_ST_PAIR + k * 2;
        if (e[1]) dna_paintPair(rdist, rlev, e[0], e[1], fillR);
      }
    } else {
      const uint8_t lvl = (uint8_t)(255 - DNA_PAIR_BEAT_LIFT
                        + (((int)st[DNA_ST_BEAT] * DNA_PAIR_BEAT_LIFT) >> 8));
      for (int c = rungOff; c < 256; c += rungMask + 1)
        dna_paintPair(rdist, rlev, (uint8_t)c, lvl, fillR);
    }
  }

  const uint8_t hue = (uint8_t)(strip.now >> 7);

  SEGMENT.fadeToBlackBy(fx_fade(200, dt));

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);

      const int v   = (int)pv[i];
      const uint8_t ang = (uint8_t)((((uint32_t)pu[i] * (uint32_t)twistQ8) >> 8) + phase8);
      const int sn  = (int)sin8_t(ang) - 128;                    // along-view offset

      const int dev  = (amp * sn) >> 7;
      const int adev = (dev < 0) ? -dev : dev;
      const int dc   = v - centre;
      int dA = dc - dev; if (dA < 0) dA = -dA;
      int dB = dc + dev; if (dB < 0) dB = -dB;

      // Fast reject. Most pixels are nowhere near a strand or a rung, and
      // everything below here - the depth lookup, the perpendicular width, the
      // tapers - would be spent on them. wMax is the widest the band can get,
      // and rhGate is loosened over the top face because rh is only known
      // exactly once the per-pixel angular density has been worked out.
      int     rd  = 0;
      uint8_t rlv = 0;
      bool rungCand = false;
      if (rungs) {
        rd  = (int)rdist[ang];
        rlv = rlev[ang];
        const int rhGate = (cube && v < 85) ? (rungMid - 1) : rhBase;
        rungCand = rlv && (rd < rhGate) && (dc >= -adev) && (dc <= adev);
      }
      if (dA >= wMax && dB >= wMax && !rungCand) continue;

      const int cs = (int)sin8_t((uint8_t)(ang + 64)) - 128;     // depth: +front, -back
      const int vA = centre + dev;
      const int vB = centre - dev;

      // Depth only touches brightness, never width - a strand that also got
      // thinner as it went round read as a flickering line at this pixel pitch.
      const int briA = 90 + (((cs + 128) * 165) >> 8);
      const int briB = 90 + (((128 - cs) * 165) >> 8);

      // A stroke of constant thickness has to be measured PERPENDICULAR to the
      // curve, but all we have cheaply is the distance in v. Where the strand
      // runs steeply, one pixel step along u jumps it several v units and a
      // fixed v band breaks the line into dashes; on the top face, where a
      // pixel near the centre is worth tens of u units, it disintegrates into
      // isolated dots. So widen the v band by hypot(1, slope) - same stroke,
      // correctly sampled. hypot is the usual max + 3/8 min approximation.
      int dangQ8 = baseDangQ8;
      if (cube && v < 85) dangQ8 = (baseDangQ8 * 85) / (v > 3 ? v : 3);
      if (dangQ8 > 20000) dangQ8 = 20000;
      const int absCs = (cs < 0) ? -cs : cs;
      const int dvdp  = (int)(((int32_t)((amp * absCs) >> 7) * dangQ8 * 100) >> 20);
      const int q     = (wPix8 * dvdp) >> 3;
      int wEff = (wV > q) ? (wV + ((q * 3) >> 3)) : (q + ((wV * 3) >> 3));
      if (wEff > wMax) wEff = wMax;          // the pole would otherwise bloom

      uint8_t lumA = 0, lumB = 0, lumR = 0, idxR = 0;
      if (dA < wEff) lumA = scale8((uint8_t)(255 - (dA * 255) / wEff), (uint8_t)briA);
      if (dB < wEff) lumB = scale8((uint8_t)(255 - (dB * 255) / wEff), (uint8_t)briB);

      if (rungCand) {
        // Same story sideways: a rung is thin in ANGLE, so it also has to be
        // widened where a pixel is worth a lot of angle.
        int rh = rungHalf;
        if (rh < (dangQ8 >> 9)) rh = dangQ8 >> 9;
        if (rh > rungMid - 1)   rh = rungMid - 1;
        if (rd < rh) {
          const int span = vB - vA;
          int t = (span != 0) ? (((v - vA) * 255) / span) : 128;   // 0 at A, 255 at B
          if (t < 0) t = 0; else if (t > 255) t = 255;
          const int dep  = cs - ((2 * cs * t) >> 8);               // cs at A -> -cs at B
          const int briR = 80 + (((dep + 128) * 120) >> 8);
          lumR = scale8(scale8((uint8_t)(255 - (rd * 255) / rh), (uint8_t)briR), rlv);
          idxR = (uint8_t)(hue + ((t * DNA_HUE_B) >> 8));
        }
      }

      const uint8_t lum = qadd8(qadd8(lumA, lumB), lumR);
      if (!lum) continue;

      uint8_t idx;
      if (lumA >= lumB && lumA >= lumR)      idx = hue;
      else if (lumB >= lumR)                 idx = (uint8_t)(hue + DNA_HUE_B);
      else                                   idx = idxR;

      SEGMENT.setPixelColorXY(x, y,
        mq_scale(SEGMENT.color_from_palette(idx, false, false, 0), scale8(lum, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_DNA_HELIX[] PROGMEM =
  "Ace 3-D DNA Helix@Speed,Helix width,Frequency,LFO rate,LFO depth,Base pairs,Pairs on beat,Flat mode;;!;2f;sx=140,ix=160,c1=128,c2=40,c3=130,o1=1,o2=0";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_DnaHelixUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_dna_helix, _data_FX_MODE_DNA_HELIX);
  }
  void loop() override {}
};

static CubeFx_DnaHelixUsermod cube_fx_dna_helix;
REGISTER_USERMOD(cube_fx_dna_helix);
