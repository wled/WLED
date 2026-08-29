#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 32. ACE 3-D ROBOT HEAD
// ===========================================================================
// The cube becomes a robot's head, and it "speaks" to the music instead of
// just reacting to it. Every face plays a different part of the character:
//
//          +--------------------+
//          |   OSCILLOSCOPE     |   TOP    - a live CRT trace of the 16 FFT
//          |   brain (16 bins)  |            bins running through the skull
//     +----+--------------------+----+
//     | EQ |    EYES + MOUTH    | EQ |   NORTH - the face: two expressive
//     |ribs|  (talks with vol)  |ribs|            eyes + a talking mouth
//     +----+--------------------+----+   WEST/EAST - temple vents: a 16-bin
//          |   POWER CORE /     |            equalizer rib cage plus an
//          |   vent grille      |            optional KITT-style scanner
//          +--------------------+   SOUTH - the back of the head: a
//                                              breathing power core + vents
//
// Mood is driven by two of cube_fx_common's audio analyzers, not by raw
// volume - a robot that just got louder isn't expressive, one whose face
// changes shape is:
//   - cfx_drop().surge   (a held, hot bass note) reads as CRINGE: eyes
//     squint into slits, the mouth clamps shut, the scope glitches and
//     warms toward red, the back panel flashes an amber "overheating" pulse.
//   - cfx_tempo().confidence, once locked, reads as CHEERFUL: eyes curl
//     into a happy arc (round-eye style) or lift at the outer corners
//     (visor/chrome styles), the mouth corners rise, and the scope/back
//     core settle into a calm synced glow.
// Neither is a flag - both are floats, so the face eases between moods
// instead of snapping, and a quiet, unlocked song just sits at a relaxed
// neutral expression.
//
// The "Style" slider swaps the whole head's design without touching any
// other control - three silhouettes built from the same mood math:
//   0 Visor   - a single Daft-Punk-style glowing bar with two scanning
//               glints, vocoder-bar mouth.
//   1 Round   - classic two round eyes with a pupil + a single sparing
//               catchlight, rounded talking mouth.
//   2 Chrome  - narrow angular eye slits, small square vent-grille mouth.
//
// Pure white is intentionally avoided everywhere except the one-pixel eye
// catchlight in the Round style and the odd scanner glint - the rest of the
// glow comes from the accent hue (or a warm shift under cringe).
//
// Flat panel fallback: the whole matrix becomes the NORTH face (eyes +
// mouth) since that's the part that actually reads as "a face" on its own.
// ---------------------------------------------------------------------------

static inline int rh_abs(int v) { return v < 0 ? -v : v; }

// ---------------------------------------------------------------------------
// TOP - oscilloscope brain: a scanning line plot of the 16 live FFT bins,
// interpolated for a smooth trace, with a faint centre grid and a soft
// glitch/warm-shift under cringe.
// ---------------------------------------------------------------------------
static void rh_top(int a, int b, const uint8_t *trace, uint8_t accentHue,
                    float cringeAmt, uint8_t beatFlag, uint16_t nowT,
                    int thick, uint8_t &outHue, uint8_t &outLum) {
  const float u = (a + 127) * 15.0f / 254.0f;
  int k0 = (int)u; if (k0 < 0) k0 = 0; if (k0 > 14) k0 = 14;
  const float frac = u - k0;
  const int amp = trace[k0] + (int)((trace[k0 + 1] - trace[k0]) * frac);   // 0..255

  int lineY = ((amp - 128) * 108) / 128;                 // waveform swings from centre
  if (cringeAmt > 0.05f) {
    const uint8_t n = (uint8_t)((a * 13 + (nowT >> 1)) & 0xFF);
    lineY += (int)(((int)n - 128) * cringeAmt * 0.6f);    // the brain stutters under load
  } else {
    const uint8_t w = sin8_t((uint8_t)(k0 * 22 + (nowT >> 2)));
    lineY += (((int)w - 128) * amp) / 640;                // a live wiggle, not a dead line
  }
  if (lineY > 118) lineY = 118; else if (lineY < -118) lineY = -118;

  int dist = b - lineY; if (dist < 0) dist = -dist;
  uint8_t lum = (dist < thick) ? (uint8_t)(255 - (dist * 255) / (thick + 1)) : 0;

  if (rh_abs(b) < 2) lum = qadd8(lum, 16);                // faint centre baseline
  if (beatFlag) lum = qadd8(lum, 36);                     // a little kick on the predicted beat

  outHue = (cringeAmt > 0.15f) ? (uint8_t)(accentHue + 28) : accentHue;
  outLum = lum;
}

// ---------------------------------------------------------------------------
// NORTH - eyes (or visor). Mood in two numbers: cheerAmt curls/lifts the
// shape, cringeAmt squints/tilts it down. "tilt" folds both into one signed
// term so every style can reuse the same corner-lift/droop formula.
// ---------------------------------------------------------------------------
static void rh_eyes(int a, int b, uint8_t style, float eyeOpen, float cheerAmt,
                     float cringeAmt, uint8_t accentHue, int eyeRad,
                     uint16_t nowT, uint8_t &outHue, uint8_t &outLum) {
  const int EYE_X = 46, EYE_Y = 28;
  const float tilt = cheerAmt - cringeAmt;
  uint8_t bestLum = 0, bestHue = accentHue;

  if (style == 0) {
    // Visor: one glowing bar spans both sockets, two glints scan inside it.
    const int vr = 6 + (int)(eyeRad * 0.5f * eyeOpen);
    const int rb = b - EYE_Y - (int)(tilt * 10.0f);
    if (rh_abs(a) < EYE_X + eyeRad && rh_abs(rb) < vr) {
      bestLum = (uint8_t)(140 + 90 * eyeOpen);
      bestHue = accentHue;
      for (int s = -1; s <= 1; s += 2) {
        const int gx = s * EYE_X + (int)(18.0f * sinf(nowT * 0.0017f + s));
        if (rh_abs(a - gx) < 7) {
          bestLum = 255;
          bestHue = (cringeAmt > 0.3f) ? (uint8_t)(accentHue + 40) : accentHue;
        }
      }
    }
    outHue = bestHue; outLum = bestLum; return;
  }

  for (int s = -1; s <= 1; s += 2) {
    const int ra = a - s * EYE_X, rb = b - EYE_Y;
    int hR, vR;
    if (style == 2) { hR = eyeRad; vR = (int)(eyeRad * (0.28f + 0.62f * eyeOpen)); }
    else            { hR = (int)(eyeRad * (0.55f + 0.45f * eyeOpen)); vR = (int)(eyeRad * eyeOpen); }
    if (vR < 3) vR = 3;

    // outer corners lift into a squint-smile or droop into a scowl
    const int rbT = rb - (int)(tilt * 16.0f * (float)rh_abs(ra) / (float)(hR + 1));

    bool inside, archMode = false;
    if (style == 1 && cheerAmt > 0.4f && cringeAmt < 0.1f) {
      archMode = true;                                     // happy closed-eye arc
      const float x = (float)ra / (float)(hR + 1);
      const int archY = (int)(-8.0f + 22.0f * x * x);
      inside = rh_abs(rb - archY) < (4 + eyeRad / 10);
    } else if (style == 2) {
      inside = (rh_abs(ra) <= hR) && (rh_abs(rbT) <= vR);   // rectangular slit
    } else {
      const float ex = (float)ra / (float)(hR + 1), ey = (float)rbT / (float)(vR + 1);
      inside = (ex * ex + ey * ey) <= 1.0f;                 // round / oval
    }

    if (inside) {
      int lum = archMode ? 200 : (130 + (int)(90 * eyeOpen));
      const int cd = ra * ra + rb * rb;
      if (!archMode && cd < (eyeRad * eyeRad) / 9) lum = 255;   // pupil core
      if (lum > 255) lum = 255;
      if ((uint8_t)lum > bestLum) {
        bestLum = (uint8_t)lum;
        bestHue = (cringeAmt > 0.3f) ? (uint8_t)(accentHue + 30) : accentHue;
      }
    }
    // one sparing catchlight per eye - never full-frame, only round styles
    if (!archMode && style != 2 && eyeOpen > 0.55f && cringeAmt < 0.1f) {
      const int gx = ra - s * 8, gy = rb - 9;
      if (gx * gx + gy * gy < 26) { bestLum = 255; bestHue = 245; }  // near-white glint
    }

    // brow: a short line above the eye, echoing the same tilt
    const int browY = EYE_Y + (int)(eyeRad * 1.35f) + (int)(tilt * 10.0f);
    if (rh_abs(ra) < hR && rh_abs(b - browY) < 3) {
      const uint8_t l = (uint8_t)(40 + rh_abs(tilt) * 60);
      if (l > bestLum) { bestLum = l; bestHue = accentHue; }
    }
  }
  outHue = bestHue; outLum = bestLum;
}

// ---------------------------------------------------------------------------
// NORTH - mouth. Same tilt term as the eyes so the whole face agrees on
// its mood; talks by tracking a fast-attack/slow-release volume envelope.
// ---------------------------------------------------------------------------
static void rh_mouth(int a, int b, uint8_t style, float mouthOpen, float cheerAmt,
                      float cringeAmt, const uint8_t *fft, uint8_t accentHue,
                      uint8_t &outHue, uint8_t &outLum) {
  const int MOUTH_Y = -44, MOUTH_W = 56;
  const float tilt = cheerAmt - cringeAmt;
  const int rb0 = b - MOUTH_Y;
  uint8_t lum = 0, hue = accentHue;

  if (rh_abs(a) > MOUTH_W) { outHue = hue; outLum = 0; return; }

  if (style == 0) {
    // vocoder grille: 5 vertical bars keyed to spread bands, arcing w/ mood
    static const uint8_t binLo[5] = {0, 3, 6, 9, 13};
    int slot = (int)((a + MOUTH_W) * 5 / (2 * MOUTH_W + 1));
    if (slot > 4) slot = 4;
    const int lvl = fft[binLo[slot]];
    const int h = 4 + (int)((lvl / 255.0f) * 30.0f * (0.4f + 0.6f * mouthOpen));
    const int slotCenterA = -MOUTH_W + (2 * slot + 1) * MOUTH_W / 5;
    const int centerB = (int)(tilt * 14.0f * (float)rh_abs(slotCenterA) / (float)MOUTH_W);
    const int off = rh_abs(rb0 - centerB);
    if (off < h) lum = (uint8_t)(200 - (off * 160) / (h + 1));
    if (cringeAmt > 0.4f) { lum = (off < 3) ? 90 : 0; hue = (uint8_t)(accentHue + 30); }
  } else if (style == 1) {
    // open talking mouth, baseline curved into a smile or a frown
    const int curveB = (int)(tilt * 16.0f * (float)(a * a) / (float)(MOUTH_W * MOUTH_W));
    const int vr = 5 + (int)(26.0f * mouthOpen);
    const int hr = (int)(MOUTH_W * 0.62f);
    const float ex = (float)a / (float)(hr + 1), ey = (float)(rb0 - curveB) / (float)(vr + 1);
    if (ex * ex + ey * ey <= 1.0f) {
      lum = (uint8_t)(90 + 120 * (1.0f - (ex * ex + ey * ey)));
    } else if (mouthOpen < 0.12f && rh_abs(rb0 - curveB) < 5) {
      lum = 140;                                            // closed: just the curved line
    }
    if (cringeAmt > 0.4f) { lum = (rh_abs(rb0 - curveB) < 4) ? 110 : 0; hue = (uint8_t)(accentHue + 30); }
  } else {
    // chrome vent grille: 5x2 small square vents, lit like a VU meter
    int col = (int)((a + MOUTH_W) * 5 / (2 * MOUTH_W + 1)); if (col > 4) col = 4;
    const int row = (b > MOUTH_Y) ? 1 : 0;
    const int idx = row * 5 + col;
    const int lit = (int)(mouthOpen * 10.0f + tilt * 3.0f);
    const int cellA = a - (-MOUTH_W + (2 * col + 1) * MOUTH_W / 5);
    const int cellB = rb0 - (row ? 7 : -7);
    if (rh_abs(cellA) < MOUTH_W / 5 - 3 && rh_abs(cellB) < 6) lum = (idx < lit) ? 200 : 20;
    if (cringeAmt > 0.4f) { lum = (idx == 2) ? 90 : 14; hue = (uint8_t)(accentHue + 30); }
  }
  outHue = hue; outLum = lum;
}

// ---------------------------------------------------------------------------
// SOUTH - back of the head: a breathing power core + a ring of vents.
// Overheats amber under cringe, settles calm under a confident lock.
// ---------------------------------------------------------------------------
static void rh_back(int a, int b, float coreEnv, float cringeAmt, uint8_t accentHue,
                     uint16_t nowT, uint8_t &outHue, uint8_t &outLum) {
  const int r = (int)sqrtf((float)(a * a + b * b));
  uint8_t lum = 0, hue = accentHue;

  if (r > 70 && r < 108) {
    const int ang = (int)(atan2f((float)b, (float)a) * 40.74f) + 128;   // 0..255 around the ring
    const int cell = ((ang + 8) / 16) * 16;
    if (rh_abs(ang - cell) < 5) lum = (uint8_t)(26 + coreEnv * 40);      // vent ring, 16 dots
  }

  const int thick = 6 + (int)(coreEnv * 10);
  if (rh_abs(r - 46) < thick) {
    const uint8_t l = (uint8_t)(120 + coreEnv * 135);
    if (l > lum) lum = l;
  }
  if (r < 18) { const uint8_t l = (uint8_t)(90 + coreEnv * 160); if (l > lum) lum = l; }

  if (cringeAmt > 0.1f) {
    const uint8_t p = sin8_t((uint8_t)(nowT >> 2));
    lum = qadd8(lum, (uint8_t)(cringeAmt * (p > 170 ? 60 : 10)));       // overheat flicker
    hue = (uint8_t)(accentHue + 40);
  }
  outHue = hue; outLum = lum;
}

// ---------------------------------------------------------------------------
// WEST/EAST - temple vents: a 16-band equalizer rib cage plus an optional
// KITT-style scanner sweep. Call with 'a' negated for EAST so the two
// sides mirror each other instead of repeating.
// ---------------------------------------------------------------------------
static void rh_side(int a, int b, const uint8_t *fft, uint8_t scanPhase, bool scannerOn,
                     float cringeAmt, uint8_t accentHue, uint16_t nowT,
                     uint8_t &outHue, uint8_t &outLum) {
  uint8_t lum = 0, hue = accentHue;

  int k = (int)((b + 127) * 15 / 254); if (k < 0) k = 0; if (k > 15) k = 15;
  const int barLen = (int)((fft[15 - k] / 255.0f) * 118.0f);
  if (rh_abs(a) < barLen) lum = (uint8_t)(60 + 100 - (rh_abs(a) * 70) / (barLen + 1));

  if (scannerOn) {
    int sx = -118 + ((int)scanPhase * 236) / 255;
    if (cringeAmt > 0.15f) sx += (int)((nowT * 37) & 63) - 32;          // erratic under stress
    const int d = rh_abs(a - sx);
    if (d < 8) {
      const uint8_t l = (uint8_t)(255 - (d * 255) / 9);
      if (l > lum) { lum = l; hue = (cringeAmt > 0.15f) ? (uint8_t)(accentHue + 40) : accentHue; }
    }
  }
  outHue = hue; outLum = lum;
}

// ===========================================================================
static FX_RET mode_robot_head() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + 32)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *fid   = SEGENV.data;              // 0=TOP 1=NORTH 2=SOUTH 3=WEST 4=EAST, 255=gap
  int8_t  *la    = (int8_t *)(fid + n);      // local face-plane coords, -127..127
  int8_t  *lb    = la + n;
  uint8_t *trace = (uint8_t *)(lb + n);      // [16] smoothed scope trace, one per FFT bin
  uint8_t *gs    = trace + 16;               // small state block, see indices below

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || gs[0] != (uint8_t)(cube ? 1 : 2)) {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        const size_t i = (size_t)y * cols + x;
        if (cube) {
          const int bx = x / B, by = y / B;
          int f;
          if      (bx == 1 && by == 1) f = 0;   // TOP
          else if (bx == 1 && by == 0) f = 1;   // NORTH
          else if (bx == 1 && by == 2) f = 2;   // SOUTH
          else if (bx == 0 && by == 1) f = 3;   // WEST
          else if (bx == 2 && by == 1) f = 4;   // EAST
          else { fid[i] = 255; la[i] = 0; lb[i] = 0; continue; }
          const float a = 2.0f * ((x % B) + 0.5f) / (float)B - 1.0f;
          const float b = 2.0f * ((y % B) + 0.5f) / (float)B - 1.0f;
          fid[i] = (uint8_t)f; la[i] = cfx_clamp8(a); lb[i] = cfx_clamp8(b);
        } else {
          // flat panel: the whole matrix becomes the front (eyes + mouth)
          fid[i] = 1;
          la[i] = cfx_clamp8(2.0f * (x + 0.5f) / (float)cols - 1.0f);
          lb[i] = cfx_clamp8(2.0f * (y + 0.5f) / (float)rows - 1.0f);
        }
      }
    }
    for (int k = 0; k < 16; k++) trace[k] = 0;
    for (int k = 0; k < 16; k++) gs[k] = 0;
    gs[0] = (uint8_t)(cube ? 1 : 2);
    gs[3] = 0xB8; gs[4] = 0x0B;                // first blink not for ~3s (0x0BB8 = 3000ms)
  }

  um_data_t            *um    = cfx_getAudioData();
  const uint8_t         *fft  = (uint8_t *)um->u_data[2];
  int bass, mid, treb; cfx_bands(fft, bass, mid, treb);
  const CfxTempoState   &tempo = cfx_tempo(um);
  const CfxDropState    &drop  = cfx_drop(um, tempo);
  const bool             locked = tempo.periodMs > 0 && tempo.confidence >= 64;
  const uint16_t          nowT  = (uint16_t)strip.now;
  const uint16_t          dt    = fx_dt8(gs + 5);

  // --- mouth / core envelopes: fast attack, slow release -------------------
  const int mv = (bass * 3 + mid) / 4;
  gs[1] = fx_env(gs[1], (uint8_t)mv, dt, 140);
  const float mouthOpenRaw = gs[1] / 255.0f;
  gs[8] = fx_env(gs[8], (uint8_t)bass, dt, 200);
  const float coreEnv = gs[8] / 255.0f;

  // --- blink, independent of the music, for a bit of life ------------------
  float blinkAmt = 0.0f;
  if (SEGMENT.check1) {
    if (gs[2] > 0) {
      const int rem = (int)gs[2] - (int)dt;
      gs[2] = (uint8_t)((rem < 0) ? 0 : rem);
      blinkAmt = (gs[2] > 0) ? 1.0f : 0.0f;
      if (gs[2] == 0) { const uint16_t nb = 3200 + hw_random16(4800); gs[3] = (uint8_t)(nb & 0xFF); gs[4] = (uint8_t)(nb >> 8); }
    } else {
      uint16_t timer = (uint16_t)gs[3] | ((uint16_t)gs[4] << 8);
      if (timer <= dt) { gs[2] = 130; blinkAmt = 1.0f; }
      else { timer = (uint16_t)(timer - dt); gs[3] = (uint8_t)(timer & 0xFF); gs[4] = (uint8_t)(timer >> 8); }
    }
  } else { gs[2] = 0; }

  // --- mood: two independent floats, cringe wins where they'd overlap ------
  const float reactScale = SEGMENT.custom3 / 255.0f;
  const float cringeAmt  = (drop.surge / 255.0f) * reactScale;
  const float lockAmt    = locked ? (tempo.confidence / 255.0f) : 0.0f;
  const float cheerAmt   = lockAmt * reactScale * (1.0f - cringeAmt);
  float eyeOpen = 1.0f - 0.85f * cringeAmt - blinkAmt;
  if (eyeOpen < 0.05f) eyeOpen = 0.05f; else if (eyeOpen > 1.0f) eyeOpen = 1.0f;
  float mouthOpen = mouthOpenRaw * (1.0f - 0.6f * cringeAmt) + cheerAmt * 0.05f;
  if (mouthOpen < 0.0f) mouthOpen = 0.0f; else if (mouthOpen > 1.0f) mouthOpen = 1.0f;

  // --- style + sizing from the sliders ---------------------------------
  const uint8_t style8   = SEGMENT.custom1;
  const uint8_t style    = (style8 < 85) ? 0 : (style8 < 170 ? 1 : 2);
  const uint8_t accentHue = SEGMENT.custom2;
  const int     eyeRad   = 22 + (SEGMENT.intensity >> 3);
  const int     scopeThick = 4 + (SEGMENT.intensity >> 5);
  const uint8_t scanPhase = locked ? tempo.phase
    : (uint8_t)((nowT * (1 + (SEGMENT.speed >> 5))) >> 3);

  // --- live scope trace: fast attack so it tracks the beat, quick settle ---
  for (int k = 0; k < 16; k++) {
    int cur = trace[k];
    cur += ((int)fft[k] - cur) / 3;
    trace[k] = (uint8_t)cur;
  }

  SEGMENT.fadeToBlackBy(fx_fade(18 + (255 - SEGMENT.speed) / 10, dt));

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const uint8_t f = fid[i];
      if (f == 255) continue;
      const int a = la[i], b = lb[i];
      uint8_t hue = accentHue, lum = 0;

      switch (f) {
        case 0:
          rh_top(a, b, trace, accentHue, cringeAmt, tempo.beat, nowT, scopeThick, hue, lum);
          break;
        case 1: {
          uint8_t h1, l1, h2, l2;
          rh_eyes(a, b, style, eyeOpen, cheerAmt, cringeAmt, accentHue, eyeRad, nowT, h1, l1);
          rh_mouth(a, b, style, mouthOpen, cheerAmt, cringeAmt, fft, accentHue, h2, l2);
          if (l2 > l1) { hue = h2; lum = l2; } else { hue = h1; lum = l1; }
          break;
        }
        case 2:
          rh_back(a, b, coreEnv, cringeAmt, accentHue, nowT, hue, lum);
          break;
        case 3:
          rh_side(a, b, fft, scanPhase, SEGMENT.check2, cringeAmt, accentHue, nowT, hue, lum);
          break;
        default:  // 4 = EAST, mirrored so the two temples aren't identical twins
          rh_side(-a, b, fft, scanPhase, SEGMENT.check2, cringeAmt, accentHue, nowT, hue, lum);
          break;
      }

      // chrome edge trim + a faint panel base, so idle plates never go dead black
      if (lum < 10) { lum = (rh_abs(a) > 116 || rh_abs(b) > 116) ? 22 : 3; hue = accentHue; }

      if (!lum) continue;
      SEGMENT.addPixelColorXY(x, y, SEGMENT.color_from_palette(hue, false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_ROBOT_HEAD[] PROGMEM =
  "Ace 3-D Robot Head@Speed,Glow,Style,Accent hue,Expressiveness,Blink,Temple scanners,Flat mode;;!;2f;sx=110,ix=140,c1=40,c2=170,c3=180,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_RobotHeadUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_robot_head, _data_FX_MODE_ROBOT_HEAD);
  }
  void loop() override {}
};

static CubeFx_RobotHeadUsermod cube_fx_robot_head;
REGISTER_USERMOD(cube_fx_robot_head);
