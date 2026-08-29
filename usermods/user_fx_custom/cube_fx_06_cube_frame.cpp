#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 6. CUBE FRAME
// ===========================================================================
// The same edge skeleton, but as a free-floating wireframe that can be
// rotated out of alignment with the physical cube. With all three spin rates
// centred and Shrink off, it sits exactly on the real edges; wind any of them
// off centre and a virtual wireframe tumbles through the solid, its
// intersection with the surface drawn as glowing lines.
//
// Distance to the wireframe: take the two largest of |X|,|Y|,|Z| - an edge is
// where both of them equal the frame's half-size. That reduces exactly to the
// simple edge distance when the frame is unrotated and full size.
// ---------------------------------------------------------------------------
static FX_RET mode_cube_frame() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + 8)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *cx = (int8_t *)SEGENV.data;
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *st = SEGENV.data + 3 * n;      // [0] built flag, [1] beat envelope

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    st[0] = (uint8_t)(cube ? 1 : 2);
    st[1] = 0;
    SEGENV.aux0 = 0; SEGENV.aux1 = 0; SEGENV.step = 0;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  // three independent spin rates; centred slider = stopped, either side = sign
  // The three spin sliders now carry the full rate range on their own, which
  // frees the speed slider for beat depth.
  const uint16_t dtR = fx_dt8(st + 2);
  SEGENV.aux0 += (uint16_t)fx_step((int)SEGMENT.custom1 - 128, dtR);
  SEGENV.aux1 += (uint16_t)fx_step((int)SEGMENT.custom2 - 128, dtR);
  SEGENV.step += (uint32_t)(int32_t)fx_step(((int)SEGMENT.custom3 - 16) * 8, dtR);

  const float kk  = 6.28318531f / 65536.0f;
  const float axf = (float)SEGENV.aux0 * kk;
  const float ayf = (float)SEGENV.aux1 * kk;
  const float azf = (float)(uint16_t)(SEGENV.step & 0xFFFF) * kk;
  const float c1 = cosf(axf), s1 = sinf(axf);
  const float c2 = cosf(ayf), s2 = sinf(ayf);
  const float c3 = cosf(azf), s3 = sinf(azf);

  // R = Rz * Ry * Rx, 8.8 fixed point
  const int32_t m00 = (int32_t)((c3 * c2) * 256.0f);
  const int32_t m01 = (int32_t)((c3 * s2 * s1 - s3 * c1) * 256.0f);
  const int32_t m02 = (int32_t)((c3 * s2 * c1 + s3 * s1) * 256.0f);
  const int32_t m10 = (int32_t)((s3 * c2) * 256.0f);
  const int32_t m11 = (int32_t)((s3 * s2 * s1 + c3 * c1) * 256.0f);
  const int32_t m12 = (int32_t)((s3 * s2 * c1 - c3 * s1) * 256.0f);
  const int32_t m20 = (int32_t)((-s2) * 256.0f);
  const int32_t m21 = (int32_t)((c2 * s1) * 256.0f);
  const int32_t m22 = (int32_t)((c2 * c1) * 256.0f);

  // Envelope: snaps up only on a low-end onset, then decays over roughly a
  // third of a second so a kick swells and settles instead of strobing.
  int env = (int)st[1];
  if (peak > env) env = peak;          // swell to the strength of the hit
  else { env -= (int)fx_step(1 + (env >> 4), dtR); if (env < 0) env = 0; }
  st[1] = (uint8_t)env;

  // Beat depth scales BOTH responses. At 0 the effect ignores beats entirely
  // and looks the way it does with both boxes unticked; at full it still only
  // moves the contour about a third of the way in, where the old code jumped
  // almost to the face centres and flashed to full white.
  const int depth = SEGMENT.speed;
  int offset = 0;
  if (SEGMENT.check1) offset = (env * depth * 45) / 65025 + (bass * depth) / 6000;
  if (offset > 60) offset = 60;

  const int     thick = 8 + (SEGMENT.intensity >> 2);    // 8..71 of 127
  const uint8_t flash = SEGMENT.check2 ? (uint8_t)((env * depth) / 765) : 0;
  const uint8_t drive = cfx_drive(vol, 2.0f, 70);
  const uint8_t hue   = (uint8_t)(strip.now >> 7);

  uint8_t colBlk[cols];
  if (cube) for (int x = 0; x < cols; x++) colBlk[x] = (uint8_t)(x / B);

  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    const uint8_t byb = cube ? (uint8_t)(y / B) : 1;
    for (int x = 0; x < cols; x++, i++) {
      if (cube && byb != 1 && colBlk[x] != 1) continue;
      const int px = cx[i], py = cy[i], pz = cz[i];
      const int rx = (int)((m00 * px + m01 * py + m02 * pz) / 256);
      const int ry = (int)((m10 * px + m11 * py + m12 * pz) / 256);
      const int rz = (int)((m20 * px + m21 * py + m22 * pz) / 256);

      const int a = (rx < 0) ? -rx : rx;
      const int b = (ry < 0) ? -ry : ry;
      const int c = (rz < 0) ? -rz : rz;
      int L, S;                                          // largest, second largest
      if (a >= b) { if (b >= c) { L = a; S = b; } else if (a >= c) { L = a; S = c; } else { L = c; S = a; } }
      else        { if (a >= c) { L = b; S = a; } else if (b >= c) { L = b; S = c; } else { L = c; S = b; } }
      if (L < 1) L = 1;

      // Project the rotated point radially back onto a unit cube, then take
      // the same edge measure Cube Edges uses. Dividing out the largest
      // component is what makes this a CONTOUR on the surface. Measuring
      // distance to the wireframe itself - a 1D line in 3D - only ever met
      // the surface at isolated points, which is why it showed a few specks.
      const int edge = 127 - (S * 127) / L;              // 0 at an edge, 127 at a face centre

      int off = edge - offset; if (off < 0) off = -off;
      if (off >= thick) { SEGMENT.setPixelColorXY(x, y, (uint32_t)0); continue; }
      uint8_t lum = (uint8_t)(255 - (off * 255) / thick);
      lum = qadd8(scale8(lum, drive), scale8(lum, flash));

      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(edge * 2 + hue), false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_FRAME[] PROGMEM =
  "Ace 3-D Cube Frame@Beat depth,Thickness,Spin X,Spin Y,Spin Z,Pulse in on beat,Beat flash,Flat mode;;!;2f;sx=70,ix=110,c1=134,c2=128,c3=16,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeFrameUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_frame, _data_FX_MODE_CUBE_FRAME);
  }
  void loop() override {}
};

static CubeFx_CubeFrameUsermod cube_fx_cube_frame;
REGISTER_USERMOD(cube_fx_cube_frame);
