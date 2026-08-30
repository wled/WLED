#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 0. CUBE AXES  (calibration, not an effect)
// ===========================================================================
// X, Y and Z drive red, green and blue. If the mapping is right the colour
// flows smoothly over every edge of the cube. A hard colour break at a seam
// means that face is rotated or mirrored relative to what cfx_buildCube
// assumes - note which seam, and the fix is one line in the face block.
// ---------------------------------------------------------------------------
static FX_RET mode_cube_axes() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t *cx = (int8_t *)SEGENV.data;
  int8_t *cy = cx + n;
  int8_t *cz = cy + n;

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || SEGENV.aux0 != (uint16_t)(cube ? 1 : 2)) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    SEGENV.aux0 = cube ? 1 : 2;
  }

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      SEGMENT.setPixelColorXY(x, y, RGBW32((uint8_t)(cx[i] + 128),
                                           (uint8_t)(cy[i] + 128),
                                           (uint8_t)(cz[i] + 128), 0));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_AXES[] PROGMEM =
  "Ace 3-D Cube Axes@,,,,,,,Flat mode;;;2";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeAxesUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_axes, _data_FX_MODE_CUBE_AXES);
  }
  void loop() override {}
};

static CubeFx_CubeAxesUsermod cube_fx_cube_axes;
REGISTER_USERMOD(cube_fx_cube_axes);
