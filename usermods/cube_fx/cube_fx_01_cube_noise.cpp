#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 1. CUBE NOISE
// ===========================================================================
// 3D noise sampled at each pixel's surface position. The sample points are
// rotated about two axes over time, so the field appears to tumble bodily
// through the cube rather than sliding along one direction.
// ---------------------------------------------------------------------------
static FX_RET mode_cube_noise() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t *cx = (int8_t *)SEGENV.data;
  int8_t *cy = cx + n;
  int8_t *cz = cy + n;

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || SEGENV.aux1 != (uint16_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    SEGENV.aux1 = cube ? 1 : 2;
  }

  um_data_t     *um  = cfx_getAudioData();
  const uint8_t *fft = (uint8_t *)um->u_data[2];
  const float    vol = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  SEGENV.aux0 += (uint16_t)fx_step(1 + (SEGMENT.speed >> 4) + (bass >> 5), fx_dt(SEGENV.step));
  const uint8_t t1 = SEGENV.aux0 >> 6;
  const uint8_t t2 = SEGENV.aux0 >> 8;
  const int c1 = (int)cos8_t(t1) - 128, s1 = (int)sin8_t(t1) - 128;
  const int c2 = (int)cos8_t(t2) - 128, s2 = (int)sin8_t(t2) - 128;

  const uint16_t scale = 4 + (uint16_t)(SEGMENT.custom1 >> 3) + (uint16_t)(treb >> 5);
  const uint16_t zt    = (uint16_t)(strip.now >> 4);
  const uint8_t  drive = cfx_drive(vol, 3.0f, 28);
  const uint8_t  spark = (uint8_t)(255 - ((int)SEGMENT.custom3 * 3) - (treb >> 2));
  const uint8_t  hue   = (uint8_t)(strip.now >> 7);

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int ax = cx[i], ay = cy[i], az0 = cz[i];
      // rotate about Z, then about X
      const int rx = (ax * c1 - ay * s1) / 128;
      const int ry = (ax * s1 + ay * c1) / 128;
      const int ry2 = (ry * c2 - az0 * s2) / 128;
      const int rz2 = (ry * s2 + az0 * c2) / 128;

      uint8_t v = perlin8((uint16_t)((rx  + 128) * scale),
                          (uint16_t)((ry2 + 128) * scale),
                          (uint16_t)((rz2 + 128) * scale + zt));
      v = qsub8(v, 16);
      v = qadd8(v, scale8(v, 39));            // perlin clusters mid-range

      if (v > spark) { SEGMENT.setPixelColorXY(x, y, RGBW32(255,255,255,0)); continue; }
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(v + hue), false, false, 0, drive));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_NOISE[] PROGMEM =
  "Ace 3-D Cube Noise@Tumble,,Zoom,,Sparkle,,,Flat mode;;!;2f;sx=90,c1=90,c3=16";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeNoiseUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_noise, _data_FX_MODE_CUBE_NOISE);
  }
  void loop() override {}
};

static CubeFx_CubeNoiseUsermod cube_fx_cube_noise;
REGISTER_USERMOD(cube_fx_cube_noise);
