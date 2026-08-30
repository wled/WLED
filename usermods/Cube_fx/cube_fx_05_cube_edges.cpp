#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 5. CUBE EDGES
// ===========================================================================
// Lines the twelve edges of the cube, sends pulses travelling between the
// edges and the face centres on each beat, and runs a wave along the edges
// themselves.
//
// Two static fields do all the work, so the per-frame loop is table lookups
// only:
//   ed[]  distance to the nearest edge, 0 at an edge, 255 at a face centre
//   al[]  position ALONG that nearest edge, which is what the wave runs on
//
// Both come out of one observation: on a cube surface the smallest of
// |X|,|Y|,|Z| is the axis the nearest edge runs along, and the second largest
// tells you how far that edge is.
// ---------------------------------------------------------------------------
static FX_RET mode_cube_edges() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 48)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *ed = SEGENV.data;
  uint8_t *al = SEGENV.data + n;
  uint8_t *st = SEGENV.data + 2 * n;
  uint8_t *spec = st + 16;                 // smoothed 16-band spectrum
  // st[k*3], st[k*3+1] = spawn time (16 ms ticks), st[k*3+2] = alive, k = 0..3
  // st[12] = previous beat state, st[13] = built flag

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  if (SEGENV.call == 0 || st[13] != (uint8_t)(cube ? 1 : 2)) {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        const size_t i = (size_t)y * cols + x;
        float X, Y, Z, edge, along;
        cfx_pos(x, y, cols, rows, B, cube, X, Y, Z);
        if (cube) {
          const float mx = fabsf(X), my = fabsf(Y), mz = fabsf(Z);
          if      (mx <= my && mx <= mz) { along = X; edge = 1.0f - ((my < mz) ? my : mz); }
          else if (my <= mx && my <= mz) { along = Y; edge = 1.0f - ((mx < mz) ? mx : mz); }
          else                           { along = Z; edge = 1.0f - ((mx < my) ? mx : my); }
        } else {
          const float mx = fabsf(X), my = fabsf(Y);
          edge  = 1.0f - ((mx > my) ? mx : my);      // distance to the border
          along = (mx > my) ? Y : X;
        }
        if (edge < 0.0f) edge = 0.0f; else if (edge > 1.0f) edge = 1.0f;
        ed[i] = (uint8_t)(edge * 255.0f);
        al[i] = (uint8_t)(int)(along * 127.0f + 128.5f);
      }
    }
    for (int k = 0; k < 14; k++) st[k] = 0;
    st[13] = (uint8_t)(cube ? 1 : 2);
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  // ed[] spans 0..255 from an edge to a face centre, so on a 16-pixel face
  // that is about 32 units per LED. Every width below is in those units.
  const int  growth  = 2 + (SEGMENT.speed >> 4);        // units per 16 ms tick
  const int  pw      = 48 + (SEGMENT.intensity >> 1);   // pulse band, 1.5..5.5 px
  const int  thick   = 16 + (SEGMENT.intensity >> 1);   // lit line, 0.5..3 px
  const bool outward = SEGMENT.check2;
  const uint16_t nowT = (uint16_t)(strip.now >> 4);

  // Hard gate: fx_lowBeat already one-shots and requires the low bins to carry
  // the transient, but this additionally demands a genuinely STRONG kick and a
  // refractory window, so busy passages can't chain pulses together.
  // nowT counts 16 ms TICKS, not milliseconds - 14 ticks is ~220 ms. Writing
  // 220 here made the refractory window three and a half seconds, which is
  // what detached the bloom from the beat entirely.
  const uint16_t lastP = (uint16_t)st[14] | ((uint16_t)st[15] << 8);
  const bool rising = (peak > 64) && ((uint16_t)(nowT - lastP) > 10);
  st[12] = peak ? 1 : 0;
  if (rising && SEGMENT.check1) {
    st[14] = (uint8_t)(nowT & 0xFF); st[15] = (uint8_t)(nowT >> 8);
    for (int k = 0; k < 4; k++) {
      if (st[k * 3 + 2]) continue;
      st[k * 3]     = (uint8_t)(nowT & 0xFF);
      st[k * 3 + 1] = (uint8_t)(nowT >> 8);
      st[k * 3 + 2] = peak;              // strength doubles as the alive flag
      break;
    }
  }

  cfx_smoothSpec(spec, fft, 3);

  // Shape 1 turns the edge into a SPECTRUM ANALYSER. Position along the edge is
  // folded about its midpoint, so each half runs bass-to-treble and the two
  // halves mirror - the reading grows in from both corners and meets in the
  // middle. All twelve edges show the same bands, so a kick swells every
  // vertex of the cube at once.
  //   0 flat   1 GEQ   2 sine   3 triangle   4 square   5 saw up   6 saw down
  uint8_t shape = (uint8_t)(SEGMENT.custom3 / 4);
  if (shape > 6) shape = 6;
  const uint8_t base = 1 + (SEGMENT.custom1 >> 5);      // bands, or cycles
  const uint8_t amp0 = SEGMENT.custom2 >> 1;            // 0..127 units, ~0..4 px

  // In the travelling-wave shapes the music drives BOTH axes: treble adds
  // cycles, bass swells the height.
  const uint8_t freq = (uint8_t)(base + (treb >> 6));
  const uint8_t amp  = (uint8_t)(((int)amp0 * (70 + (bass * 185) / 255)) / 255);
  const uint8_t scrl = (uint8_t)((strip.now * (1 + (SEGMENT.speed >> 4))) >> 5);

  const uint8_t drive = cfx_drive(vol, 2.0f, 70);
  const uint8_t hue   = (uint8_t)(strip.now >> 7);

  // Travel is now NORMALISED 0..255. The absolute position is worked out per
  // pixel, because the near end of the run is the wave line - which moves.
  int pulseT[4], pulseS[4]; int pulseN = 0;
  for (int k = 0; k < 4; k++) {
    if (!st[k * 3 + 2]) continue;
    const uint16_t born = (uint16_t)st[k * 3] | ((uint16_t)st[k * 3 + 1] << 8);
    const int trav = (int)(uint16_t)(nowT - born) * growth;
    if (trav > 330) { st[k * 3 + 2] = 0; continue; }
    pulseS[pulseN] = st[k * 3 + 2];      // soft kick, soft bloom
    pulseT[pulseN++] = trav;
  }

  uint8_t colBlk[cols];
  if (cube) for (int x = 0; x < cols; x++) colBlk[x] = (uint8_t)(x / B);

  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    const uint8_t byb = cube ? (uint8_t)(y / B) : 1;
    for (int x = 0; x < cols; x++, i++) {
      if (cube && byb != 1 && colBlk[x] != 1) continue;  // gap corner - skip the work
      const int d = (int)ed[i];

      // The wave DISPLACES the lit line instead of dimming it: the line sits
      // at "target" units in from the edge and that target rides the wave, so
      // the edge visibly snakes in and out like a plucked string.
      int target = 0;
      if (shape == 1) {                                  // GEQ, mirrored per edge
        const uint8_t a8 = al[i];
        const uint8_t q  = (a8 < 128) ? a8 : (uint8_t)(255 - a8);
        const uint8_t bin = (uint8_t)((((uint16_t)q * base * 16) / 128) & 15);
        target = ((int)amp0 * 2 * (int)spec[bin]) / 255;
      } else if (shape) {
        const uint8_t w = cfx_wave((uint8_t)(shape - 1),
                                   (uint8_t)((uint16_t)al[i] * freq - scrl));
        target = (int)amp + ((int)amp * ((int)w - 128)) / 128;
      }
      int off = d - target; if (off < 0) off = -off;
      uint8_t lum = (off >= thick) ? 0 : (uint8_t)(255 - (off * 255) / thick);

      // The pulse runs between the wave line and the face centre and stops
      // there. Nothing outside the wave line gets lit, so it can never spill
      // onto the physical square edge no matter where the wave has swung to.
      if (d >= target) {
        const int span = 255 - target;
        for (int k = 0; k < pulseN; k++) {
          const int pp = outward ? (255 - (pulseT[k] * span) / 255)
                                 : (target + (pulseT[k] * span) / 255);
          int po = d - pp; if (po < 0) po = -po;
          if (po >= pw) continue;
          lum = qadd8(lum, scale8((uint8_t)(255 - (po * 255) / pw), (uint8_t)pulseS[k]));
        }
      }

      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(d + hue), false, false, 0, scale8(lum, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_EDGES[] PROGMEM =
  "Ace 3-D Cube Edges@Speed,Thickness,Bands,Wave height,Wave shape,Pulse on beat,Outward,Flat mode;;!;2f;sx=110,ix=80,c1=16,c2=170,c3=4,o1=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeEdgesUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_edges, _data_FX_MODE_CUBE_EDGES);
  }
  void loop() override {}
};

static CubeFx_CubeEdgesUsermod cube_fx_cube_edges;
REGISTER_USERMOD(cube_fx_cube_edges);
