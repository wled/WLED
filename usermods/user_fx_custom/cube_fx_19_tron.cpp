#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 19. ACE 3-D TRON
// ===========================================================================
// Cycles run over the WHOLE surface now, top face included, using the
// directional transition table - so a cycle can climb a wall, cross the top
// and come down the far side, its heading rotating correctly at every fold.
//
// The arena is closed: no outer wall exists anywhere on a cube, so the only
// thing that can kill you is a trail. The open bottom rim is the sole hard
// edge, and it reads as one because a step onto the missing face is a wall.
// Kicks make the cycles turn, so the maze gets cut to the music.
// ---------------------------------------------------------------------------
#define TR_N 4
static const uint32_t TR_COL[4] = {
  RGBW32(  0,220,255,0), RGBW32(255,140,  0,0),
  RGBW32(120,255, 60,0), RGBW32(255, 60,180,0) };

static FX_RET mode_tron() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 320 * 4 * 2 + 8 + 320 + 24)) {
    SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint16_t *cellOf = (uint16_t *)SEGENV.data;
  uint16_t *dlut   = cellOf + n;                 // 320 * 4
  uint16_t *cyc    = dlut + 320 * 4;             // cycle cells
  uint8_t  *ar     = (uint8_t *)(cyc + TR_N);    // arena owners
  uint8_t  *st     = ar + 320;
  // st: [0] built [1..2] clock [3] flash [4..7] dir [8..11] alive

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  const uint16_t nowT = (uint16_t)strip.now;

  bool reset = (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2));
  if (reset) {
    cfx_buildCells(cellOf, cols, rows, cube, B);
    cfx_buildDirLut(dlut);
    for (int k = 0; k < 24; k++) st[k] = 0;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];

  int living = 0;
  for (int k = 0; k < TR_N; k++) if (st[8 + k]) living++;
  if (reset || living <= 1) {
    if (!reset) st[3] = 255;
    for (int k = 0; k < 320; k++) ar[k] = 0;
    for (int k = 0; k < TR_N; k++) {             // one per wall, all heading up
      const int c = (k + 1) * 64 + 5 * 8 + 3;
      cyc[k] = (uint16_t)c; st[4 + k] = 3; st[8 + k] = 1;
      ar[c] = (uint8_t)(k + 1);
    }
    st[0] = (uint8_t)(cube ? 1 : 2);
    st[1] = (uint8_t)nowT; st[2] = (uint8_t)(nowT >> 8);
  }

  const uint16_t due = (uint16_t)st[1] | ((uint16_t)st[2] << 8);
  if ((uint16_t)(nowT - due) < 32768) {
    for (int k = 0; k < TR_N; k++) {
      if (!st[8 + k]) continue;
      const int c = cyc[k], d = st[4 + k];
      const uint16_t fwd = dlut[c * 4 + d];
      const bool fwdOK = (fwd != 0xFFFF) && (ar[fwd & 0x0FFF] == 0);
      const bool nudge = (peak && hw_random16(3) == 0) || (hw_random16(16) == 0);

      int pick = -1, nd = d;
      if (fwdOK && !nudge) { pick = fwd & 0x0FFF; nd = fwd >> 12; }
      else {
        int t1 = (d + 1) & 3, t2 = (d + 3) & 3;
        if (hw_random16(2)) { const int t = t1; t1 = t2; t2 = t; }
        const uint16_t e1 = dlut[c * 4 + t1], e2 = dlut[c * 4 + t2];
        if      (e1 != 0xFFFF && ar[e1 & 0x0FFF] == 0) { pick = e1 & 0x0FFF; nd = e1 >> 12; }
        else if (e2 != 0xFFFF && ar[e2 & 0x0FFF] == 0) { pick = e2 & 0x0FFF; nd = e2 >> 12; }
        else if (fwdOK)                                { pick = fwd & 0x0FFF; nd = fwd >> 12; }
      }
      if (pick < 0) { st[8 + k] = 0; continue; }  // boxed in
      cyc[k] = (uint16_t)pick; st[4 + k] = (uint8_t)nd;
      ar[pick] = (uint8_t)(k + 1);
    }
    const uint16_t nx = nowT + (uint16_t)(60 + (255 - (int)SEGMENT.speed));
    st[1] = (uint8_t)nx; st[2] = (uint8_t)(nx >> 8);
  }
  { const int f = (int)st[3] - 9; st[3] = (uint8_t)((f < 0) ? 0 : f); }

  const uint8_t drive = cfx_drive(vol, 1.0f, 130 + (SEGMENT.intensity >> 1));

  SEGMENT.fill(SEGCOLOR(0));
  size_t i = 0;
  for (int y = 0; y < rows; y++)
    for (int x = 0; x < cols; x++, i++) {
      const uint16_t c = cellOf[i];
      if (c == 0xFFFF || c >= 320) continue;
      uint32_t col = 0; uint8_t lvl = 0;
      if (ar[c]) { col = TR_COL[(ar[c] - 1) & 3]; lvl = 90; }
      for (int k = 0; k < TR_N; k++)
        if (st[8 + k] && cyc[k] == c) { col = RGBW32(255,255,255,0); lvl = 255; }
      if (st[3] && !ar[c]) { col = RGBW32(st[3], st[3] >> 3, 0, 0); lvl = 255; }
      if (lvl) SEGMENT.setPixelColorXY(x, y, mq_scale(col, scale8(lvl, drive)));
    }
  FX_DONE;
}

static const char _data_FX_MODE_TRON[] PROGMEM =
  "Ace 3-D Tron@Speed,Brightness,,,,,,Flat mode;;;2f;sx=170,ix=170";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_TronUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_tron, _data_FX_MODE_TRON);
  }
  void loop() override {}
};

static CubeFx_TronUsermod cube_fx_tron;
REGISTER_USERMOD(cube_fx_tron);
