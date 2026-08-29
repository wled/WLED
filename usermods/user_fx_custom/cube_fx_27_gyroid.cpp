#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 27. ACE 3-D GYROID
// ===========================================================================
// Triply-periodic minimal surfaces, cut by the cube's own faces. Where the
// field crosses zero the surface has (locally) zero mean curvature - it bends
// exactly as much one way as the other everywhere - which is what gives these
// their famously smooth, soap-film look. Three classic families, one blended
// field so Morph slides continuously between them:
//
//   Gyroid:      sin(X)cos(Y) + sin(Y)cos(Z) + sin(Z)cos(X)
//   Schwarz P:   cos(X) + cos(Y) + cos(Z)
//   Diamond:     sin(X)sin(Y)sin(Z) + sin(X)cos(Y)cos(Z)
//              + cos(X)sin(Y)cos(Z) + cos(X)cos(Y)sin(Z)
//
// Same trick as Cube Chladni: one cos8/sin8 table per axis, built once a
// frame, so the pixel loop is table lookups and adds, not trig.
// ---------------------------------------------------------------------------
static FX_RET mode_gyroid() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + 6 * 256 + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *cx = (int8_t *)SEGENV.data;
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *sT = (uint8_t *)(cz + n);    // sin tables, X Y Z
  uint8_t *cT = sT + 3 * 256;           // cos tables, X Y Z
  uint8_t *st = cT + 3 * 256;           // [0] built [1..2] clock

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    for (int k = 0; k < 16; k++) st[k] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2);
    SEGENV.aux0 = 90;                   // starting spatial frequency
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);
  const uint16_t dtG = fx_dt8(st + 1);

  // spatial frequency: base plus bass, eased so a kick doesn't snap the field
  int freqTgt = 48 + (SEGMENT.custom1 * 3) / 2 + (bass >> 2);
  const int rate = 3 + (SEGMENT.speed >> 3);           // ties both tweens to Speed
  { const int cur = (int)SEGENV.aux0, dif = freqTgt - cur;
    int mv = (dif * (int)fx_step(rate, dtG)) / 64;
    if (mv == 0 && dif != 0) mv = (dif > 0) ? 1 : -1;
    SEGENV.aux0 = (uint16_t)(cur + mv); }
  const int32_t freq = (int32_t)SEGENV.aux0;

  for (int i = 0; i < 256; i++) {
    const int v = i - 128;
    const uint8_t ax = (uint8_t)((freq * v) / 256);
    sT[i] = sin8_t(ax); cT[i] = cos8_t(ax);
    const uint8_t ay = (uint8_t)((freq * v) / 256);
    sT[256 + i] = sin8_t(ay); cT[256 + i] = cos8_t(ay);
    sT[512 + i] = sT[256 + i]; cT[512 + i] = cT[256 + i];   // Y and Z share phase
  }

  // morph target: slider plus mids, latched on the beat like the Chladnis
  int tgt = (int)SEGMENT.custom2 + ((mid - 110) >> 1);
  if (tgt < 0) tgt = 0; else if (tgt > 255) tgt = 255;
  if (SEGMENT.check1) { if (peak) SEGENV.step = (uint32_t)tgt; tgt = (int)SEGENV.step; }
  int morph = (int)((SEGENV.step >> 8) & 0xFFFF);           // smoothed value lives here
  { const int dif = tgt - morph;
    int mv = (dif * (int)fx_step(rate, dtG)) / 64;
    if (mv == 0 && dif != 0) mv = (dif > 0) ? 1 : -1;
    morph = morph + mv; }
  SEGENV.step = (uint32_t)((SEGENV.step & 0xFF) | ((uint32_t)morph << 8));

  const uint32_t sharp = 6 + (SEGMENT.intensity >> 3);
  const uint8_t  glow  = (uint8_t)(((int)SEGMENT.custom3 * 255) / 31);
  const uint8_t  drive = cfx_drive(vol, 6.0f, 30);
  const uint8_t  hue   = (uint8_t)(strip.now >> 8);

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int ux = (int)cx[i] + 128, uy = (int)cy[i] + 128, uz = (int)cz[i] + 128;
      const int sx2 = (int)sT[ux] - 128, sy2 = (int)sT[256+uy] - 128, sz2 = (int)sT[512+uz] - 128;
      const int cx2 = (int)cT[ux] - 128, cy2 = (int)cT[256+uy] - 128, cz2 = (int)cT[512+uz] - 128;

      const int32_t gyroid = (int32_t)sx2*cy2 + (int32_t)sy2*cz2 + (int32_t)sz2*cx2;
      const int32_t schwarz = ((int32_t)cx2 + cy2 + cz2) * 74;         // scaled to match range
      const int32_t diamond = ((int32_t)sx2*sy2*sz2 + sx2*cy2*cz2
                             + cx2*sy2*cz2 + cx2*cy2*sz2) / 128;

      // three-way blend driven by ONE morph value: 0..127 crossfades gyroid
      // into Schwarz P, 128..255 crossfades Schwarz P into diamond
      int32_t psi;
      if (morph < 128) psi = (gyroid * (127 - morph) + schwarz * morph) / 127;
      else              psi = (schwarz * (255 - morph) + diamond * (morph - 128)) / 127;

      const int32_t a2 = (psi < 0) ? -psi : psi;
      const uint32_t d = ((uint32_t)(a2 >> 6) * sharp) >> 8;
      const uint8_t lumS = (d > 255) ? 0 : (uint8_t)(255 - d);   // surface (near zero)
      const uint8_t lumA = (d > 255) ? 255 : (uint8_t)d;         // fill (away from it)
      uint8_t lum = (uint8_t)((((uint16_t)lumS * (255 - glow)) + ((uint16_t)lumA * glow)) >> 8);
      lum = scale8(lum, drive);

      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)((lum >> 1) + hue), false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_GYROID[] PROGMEM =
  "Ace 3-D Gyroid@Field speed,Sharpness,Frequency,Morph,Fill glow,Snap to beat,,Flat mode;;!;2f;sx=90,ix=140,c1=60,c2=0,c3=0,o1=1";

// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_GyroidUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_gyroid, _data_FX_MODE_GYROID);
  }
  void loop() override {}
};

static CubeFx_GyroidUsermod cube_fx_gyroid;
REGISTER_USERMOD(cube_fx_gyroid);
