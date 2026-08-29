#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 2. CUBE RIPPLES
// ===========================================================================
// Beats drop a point source somewhere on the surface; each expands as a
// spherical shell through 3D space. Because the shell is a function of true
// distance, it crosses every fold correctly - a ripple started on one wall
// climbs the top face and comes down the far side as one continuous ring.
// This is the effect that cannot be faked in 2D.
// ---------------------------------------------------------------------------
#define CFX_SRC 6

static FX_RET mode_cube_ripples() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + CFX_SRC * 8)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *cx  = (int8_t *)SEGENV.data;
  int8_t  *cy  = cx + n;
  int8_t  *cz  = cy + n;
  uint8_t *src = SEGENV.data + 3 * n;      // per source: x,y,z,hue,born_lo,born_hi,alive

  const bool cube = cfx_isCube(cols, rows);
  if (SEGENV.call == 0 || SEGENV.aux1 != (uint16_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    for (int s = 0; s < CFX_SRC * 8; s++) src[s] = 0;
    SEGENV.aux1 = cube ? 1 : 2;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];

  const int B = cube ? (cols / 3) : 1;
  const int growth = 3 + (SEGMENT.speed >> 3);        // units per 23 ms
  const int width  = 12 + (SEGMENT.intensity >> 2);   // shell thickness
  const uint16_t nowT = (uint16_t)strip.now;

  // --- spawn ---------------------------------------------------------------
  if (peak) {
    for (int s = 0; s < CFX_SRC; s++) {
      uint8_t *e = src + s * 8;
      if (e[6]) continue;                              // slot busy
      size_t si = 0;
      for (int tries = 0; tries < 6; tries++) {
        si = (size_t)hw_random16((uint16_t)n);
        if (!cube) break;
        const int bx = (int)(si % cols) / B, by = (int)(si / cols) / B;
        if (bx == 1 || by == 1) break;                 // not a gap corner
      }
      uint8_t b1 = 0;
      for (uint8_t k = 1; k < 16; k++) if (fft[k] > fft[b1]) b1 = k;
      e[0] = (uint8_t)cx[si]; e[1] = (uint8_t)cy[si]; e[2] = (uint8_t)cz[si];
      e[3] = (uint8_t)(b1 << 4);
      e[4] = (uint8_t)(nowT & 0xFF); e[5] = (uint8_t)(nowT >> 8);
      e[6] = 1;
      break;                                           // one per beat
    }
  }

  SEGMENT.fadeToBlackBy(fx_fade(40 + (255 - SEGMENT.custom2) / 4, fx_dt(SEGENV.step)));

  const uint8_t drive = cfx_drive(vol, 2.0f, 60);

  // --- render --------------------------------------------------------------
  for (int s = 0; s < CFX_SRC; s++) {
    uint8_t *e = src + s * 8;
    if (!e[6]) continue;
    const int sx = (int8_t)e[0], sy = (int8_t)e[1], sz = (int8_t)e[2];
    // radius from elapsed TIME, so the shell speed no longer tracks frame rate
    const uint16_t born = (uint16_t)e[4] | ((uint16_t)e[5] << 8);
    const int rad = (int)(((uint32_t)(uint16_t)(nowT - born) * growth * 2) / 23);
    if (rad > 500) { e[6] = 0; continue; }
    const int lo = (rad > width) ? (rad - width) : 0;
    const int hi = rad + width;
    const int32_t lo2 = (int32_t)lo * lo, hi2 = (int32_t)hi * hi;
    // fade the shell out as it gets large
    const uint8_t life = (uint8_t)(255 - (rad * 255) / 500);

    CFX_NET_PREP();
    size_t i = 0;
    for (int y = 0; y < rows; y++) {
      CFX_NET_ROW(y);
      for (int x = 0; x < cols; x++, i++) {
        CFX_NET_SKIP(x);
        const int dx = cx[i] - sx, dy = cy[i] - sy, dz = cz[i] - sz;
        const int32_t d2 = (int32_t)dx * dx + (int32_t)dy * dy + (int32_t)dz * dz;
        if (d2 < lo2 || d2 > hi2) continue;            // skip the sqrt
        const int d = (int)sqrtf((float)d2);
        int off = d - rad; if (off < 0) off = -off;
        uint8_t lum = (uint8_t)(255 - (off * 255) / width);
        lum = scale8(scale8(lum, life), drive);
        SEGMENT.addPixelColorXY(x, y,
          SEGMENT.color_from_palette((uint8_t)(e[3] + (lum >> 2)), false, false, 0, lum));
      }
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_RIPPLES[] PROGMEM =
  "Ace 3-D Cube Ripples@Speed,Thickness,,Persistence,,,,Flat mode;;!;2f;sx=110,ix=90,c2=140";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeRipplesUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_ripples, _data_FX_MODE_CUBE_RIPPLES);
  }
  void loop() override {}
};

static CubeFx_CubeRipplesUsermod cube_fx_cube_ripples;
REGISTER_USERMOD(cube_fx_cube_ripples);
