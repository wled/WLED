#include "wled.h"
#include "cube_fx_common.h"
#include "cube_fx_imu.h"

// ===========================================================================
// 33. ACE 3-D IMU AXES  (calibration, not an effect)
// ===========================================================================
// What Cube Axes is to the net, this is to the sensor. It paints the world:
// sky above, ground below, and a bright horizon band exactly where the room's
// eye level cuts through the cube. Pick the cube up and turn it any way you
// like - if the mounting is described correctly, the horizon stays level and
// slides around the faces without ever tipping.
//
// Reading the failures:
//   horizon tips the WRONG way as you tilt   -> one CFX_IMU_AXIS_* sign flipped
//   horizon swings around the wrong axis     -> two of them are swapped
//   horizon lags and swims                   -> lower CFX_IMU_SMOOTH_MS
//   nothing but a slow red pulse             -> no feed arriving at all; the
//        cfx_imuFeed() call isn't in the sensor usermod's loop, or the usermod
//        isn't in the build
//
// The amber cross marks whichever face is currently pointing at the sky, and
// it stays dim until the face lock settles, so you can watch the hysteresis
// work while the cube is being carried. Shake it and the whole world flashes -
// that's the same one-shot, proportional response beat effects get. Turn on
// Spin ticks to put marks on the horizon: they should hold still in the room
// while the cube rotates underneath them.
//
// On a flat panel it degrades to a level line sweeping across the matrix,
// which is still enough to check signs.
// ---------------------------------------------------------------------------
static FX_RET mode_imu_axes() {
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

  const CfxImuState &imu = cfx_imu();

  const int     band      = 3 + ((int)SEGMENT.custom1 * 34) / 255;  // half-width
  const int     markLum   = SEGMENT.custom2;
  const int     ticks     = 1 + (SEGMENT.custom3 >> 5);
  const bool    showTicks = SEGMENT.check1;
  const bool    faceTint  = SEGMENT.check2;
  const uint8_t master    = SEGMENT.intensity;
  const uint8_t drift     = (uint8_t)((strip.now * (1 + (SEGMENT.speed >> 4))) >> 9);
  const int     flash     = imu.jolt >> 1;                          // shake tail
  const uint8_t tintOff   = faceTint ? (uint8_t)(imu.upFace * 42) : 0;
  // No sensor: breathe, so a dead feed can never be mistaken for a level cube.
  const uint8_t alive     = imu.valid ? 255
                                      : (uint8_t)(70 + (sin8_t((uint8_t)(strip.now >> 4)) >> 1));

  SEGMENT.fill(SEGCOLOR(0));
  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int px = cx[i], py = cy[i], pz = cz[i];
      const int h  = cfx_imuUp(imu, px, py, pz);       // height along world up

      uint32_t c;
      int lum;
      if (h > band) {                                  // sky
        const int d = h - band;
        c   = SEGMENT.color_from_palette((uint8_t)(158 + (d >> 2) + drift + tintOff),
                                         false, false, 0);
        lum = 45 + (d * 3) / 2;
      } else if (h < -band) {                          // ground
        const int d = -h - band;
        c   = SEGMENT.color_from_palette((uint8_t)(28 + (d >> 2) + drift + tintOff),
                                         false, false, 0);
        lum = 22 + (d >> 1);
      } else {                                         // the horizon itself
        const int e = (h < 0) ? -h : h;
        lum = 255 - (e * 170) / (band + 1);
        c   = RGBW32(190, 235, 255, 0);
        if (showTicks) {
          const int ee = ((int)imu.basis[0] * px + (int)imu.basis[1] * py
                        + (int)imu.basis[2] * pz) >> 7;
          const int nn = ((int)imu.basis[3] * px + (int)imu.basis[4] * py
                        + (int)imu.basis[5] * pz) >> 7;
          const uint8_t az = (uint8_t)(int)(atan2f((float)nn, (float)ee)
                                            * (128.0f / 3.14159265f) + 256.5f);
          const uint8_t t  = sin8_t((uint8_t)((int)az * ticks));
          lum = scale8((uint8_t)((lum > 255) ? 255 : lum), (uint8_t)(70 + scale8(t, 185)));
        }
      }

      // amber cross on the face pointing at the sky, dim until the lock settles
      if (cube && markLum > 0 && imu.valid) {
        const int bx = x / B, by = y / B;
        int face = 255;
        if      (bx == 1 && by == 1) face = CFX_FACE_TOP;
        else if (bx == 1 && by == 0) face = CFX_FACE_NORTH;
        else if (bx == 1 && by == 2) face = CFX_FACE_SOUTH;
        else if (bx == 0 && by == 1) face = CFX_FACE_WEST;
        else if (bx == 2 && by == 1) face = CFX_FACE_EAST;

        if (face == (int)imu.upFace) {
          const int lx = (x % B) - B / 2, ly = (y % B) - B / 2;
          const int ax = (lx < 0) ? -lx : lx, ay = (ly < 0) ? -ly : ly;
          const int arm = 1 + B / 6;
          if ((ax <= 1 && ay <= arm) || (ay <= 1 && ax <= arm)) {
            c   = RGBW32(255, 205, 55, 0);
            lum = scale8((uint8_t)markLum, (uint8_t)(80 + ((int)imu.faceLock * 175) / 255));
          }
        }
      }

      lum += flash;
      if (lum < 0) lum = 0; else if (lum > 255) lum = 255;
      if (!imu.valid) c = RGBW32(170, 22, 12, 0);      // unmistakably "no data"
      SEGMENT.setPixelColorXY(x, y,
        mq_scale(c, scale8(scale8((uint8_t)lum, master), alive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_IMU_AXES[] PROGMEM =
  "Ace 3-D IMU Axes@Sky drift,Brightness,Horizon,Face mark,Tick pitch,Spin ticks,Tint by face,Flat mode;;!;2;sx=40,ix=215,c1=45,c2=175,c3=96,o1=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_ImuAxesUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_imu_axes, _data_FX_MODE_IMU_AXES);
  }
  void loop() override {}
};

static CubeFx_ImuAxesUsermod cube_fx_imu_axes;
REGISTER_USERMOD(cube_fx_imu_axes);
