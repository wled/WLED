#include "wled.h"
#include "cube_fx_common.h"
#include "cube_fx_imu.h"

// ===========================================================================
// 25. ACE 3-D LIQUID  /  ACE GYRO LIQUID
// ===========================================================================
// The first effect here that treats the cube as a VOLUME rather than a
// surface. A tilting plane cuts through the solid and everything below it is
// wet, which needs one dot product per pixel and immediately makes the object
// read as a container with something in it.
//
// The plane is a real damped oscillator, not an animation curve: kicks give it
// an impulse and it sloshes and settles on its own, so the tilt overshoots,
// rocks back and dies away the way liquid actually does.
//
// The top face earns its keep here - when the level sits below it you are
// looking DOWN at the surface, so it becomes a rippling pool while the walls
// show the same water in cross section. Raise the level past it and the whole
// cube submerges.
//
// ---------------------------------------------------------------------------
// THE GYRO VARIANT
// ---------------------------------------------------------------------------
// Same code, one thing changed: what "level" means. The surface used to be a
// spring pulled back toward the cube's own horizontal. Now it is pulled toward
// the ROOM's horizontal, so the water stays flat while you turn the cube and
// the beat still sloshes it - music keeps time and amplitude, gravity supplies
// the frame. Nothing about the audio path moved.
//
// Three things had to be generalised to get there, and they are worth knowing
// because the rest of the Gyro class will want all three:
//
//   1. THE PLANE IS A NORMAL, NOT TWO SLOPES.
//      The old form wrote the surface as a height function,
//      hs = level + tiltX*px + tiltY*py, which is fine while the cube is
//      roughly upright and blows up the moment it isn't - the slopes go to
//      infinity as the cube passes 90 degrees, which is exactly the case the
//      gyro makes interesting. The state is now a unit normal that springs
//      toward world up, and height is one dot product. It works at any
//      attitude including fully inverted.
//
//   2. FILL IS PROPORTIONAL, NOT ABSOLUTE.
//      A cube is deeper along its diagonal than through a face - the height
//      range is +/-128 face-on but +/-219 corner-on. Holding `level` fixed
//      would make a half-full cube look nearly empty when balanced on a
//      corner. Scaling it by the support width (|nx|+|ny|+|nz|) keeps the
//      surface at the same PROPORTIONAL height, so the quantity of water
//      reads as constant while you turn it. Exact at half full, close enough
//      everywhere else.
//
//   3. THE TOP FACE STOPS BEING A WINDOW WHEN IT STOPS FACING THE SKY.
//      "You are looking down into the pool" is an artistic conceit that only
//      works when that face is actually skyward. Rather than pop it off at
//      some angle, it fades out with the face's own skyward-ness, so on its
//      side the top face behaves like any other wall.
//
// Turn the cube upside down and the water goes to what was the ceiling. There
// is no BOTTOM face on this build, so at that point you are looking into an
// open box from below - the walls still carry the cross section, which is the
// honest answer and looks better than pretending.
//
// WITHOUT A SENSOR the gyro entry renders identically to the plain one, down
// to the arithmetic - the normal seeds and stays at (0,0,128), the ripple axes
// are the identity, and every expression collapses to what it was. No #ifdef,
// no branch in the pixel loop.
//
// COST: nine int16 multiplies per pixel instead of the old two, and the plain
// entry pays them too. On a 48x48 net that is ~20k extra multiplies a frame,
// well under a millisecond, and it buys ONE inner loop to maintain instead of
// two that drift apart.
//
// BACK-PRESSURE: cfx_imu() is called only on the gyro path, so selecting
// "Ace 3-D Liquid" leaves the sensor driver in its trickle poll and gives the
// I2C time back to the WS2812 lines. Do not hoist that call.
// ---------------------------------------------------------------------------

// Unit length for the plane normal and the ripple axes. 128, not 127, so the
// >>7 that follows every dot product is exact and the no-gyro path reproduces
// the original bit for bit. The IMU hands out 127-scale unit vectors, so its
// targets get rescaled on the way in - otherwise the spring rests one unit
// short of where renormalisation wants it and chatters there forever.
#define LIQ_UNIT   128
// A target more than ~this far past perpendicular is antipodal, and springing
// toward it means dragging the normal through its own origin: there is no
// halfway between up and down, the magnitude collapses, renormalisation blows
// it straight back out and the plane sits pinned at the unstable equilibrium
// indefinitely. Same threshold and same reasoning as cfx_imu()'s own smoother.
#define LIQ_FLIP  (-13926)          // -0.85 * 128 * 128
// The top face submerges a hair before the plane literally reaches its plane,
// which is where the original's magic 118-against-127 came from.
#define LIQ_BRIM   9

static FX_RET mode_liquid_core(bool gyro) {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(16 + 3 * n + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int16_t *ph = (int16_t *)SEGENV.data;      // level, levelVel, then the normal
  int8_t  *cx = (int8_t *)(ph + 8);          // ph[0] lev   ph[1] levVel
  int8_t  *cy = cx + n;                      // ph[2] nx    ph[3] nxVel
  int8_t  *cz = cy + n;                      // ph[4] ny    ph[5] nyVel
  uint8_t *st = (uint8_t *)(cz + n);         // ph[6] nz    ph[7] nzVel
                                             // st[0] built [1] splash [2..3] clock
  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  // --- where the surface wants to sit, and which way the ripples run --------
  // Row 0 and row 1 of the matrix are the two in-plane axes the wave trains
  // travel along; row 2 is the surface normal itself, filled in after the
  // spring runs. With the gyro on, rows 0 and 1 come from the cube->world
  // basis, so the ripples stay in the water rather than being a texture
  // painted on the cube - the single change that stops it looking like a
  // screensaver and starts it looking like a tank.
  const CfxImuState *imu = gyro ? &cfx_imu() : nullptr;
  const bool live = (imu != nullptr) && imu->valid;

  int tgt[3];
  int topWin = LIQ_UNIT;                     // how much the top face is a window
  int16_t M[9];

  if (live && cube) {
    tgt[0] = ((int)imu->ux * LIQ_UNIT) / 127;
    tgt[1] = ((int)imu->uy * LIQ_UNIT) / 127;
    tgt[2] = ((int)imu->uz * LIQ_UNIT) / 127;
    topWin = (tgt[2] > 0) ? tgt[2] : 0;      // fades out as it stops facing up
    for (int k = 0; k < 6; k++) M[k] = imu->basis[k];   // east, north
  } else if (live) {
    // Flat panel with a sensor behind it. There is no cube geometry to hang
    // gravity on, so this assumes the panel stands upright with the driver's
    // cube +X to screen right and +Z to screen up; if yours reads sideways,
    // swap AxisX/AxisZ in Settings -> Usermods -> AceIMU. Face-up on a bench
    // there is no in-plane gravity at all, so fall back to screen-down.
    int ax = ((int)imu->ux * LIQ_UNIT) / 127;
    int az = ((int)imu->uz * LIQ_UNIT) / 127;
    if (ax * ax + az * az < 400) { ax = 0; az = LIQ_UNIT; }
    tgt[0] = ax; tgt[1] = az; tgt[2] = 0;
    M[0] = LIQ_UNIT; M[1] = 0; M[2] = 0;
    M[3] = 0; M[4] = LIQ_UNIT; M[5] = 0;
  } else {
    // No sensor, or the plain entry: the cube is its own reference frame and
    // every expression below collapses to the original.
    tgt[0] = 0;
    tgt[1] = cube ? 0 : LIQ_UNIT;
    tgt[2] = cube ? LIQ_UNIT : 0;
    M[0] = LIQ_UNIT; M[1] = 0; M[2] = 0;
    M[3] = 0; M[4] = LIQ_UNIT; M[5] = 0;
  }

  const uint8_t want = (uint8_t)((cube ? 1 : 2) + (gyro ? 2 : 0));
  if (SEGENV.call == 0 || st[0] != want) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    ph[0] = 0; ph[1] = 0;
    ph[2] = (int16_t)tgt[0]; ph[4] = (int16_t)tgt[1]; ph[6] = (int16_t)tgt[2];
    ph[3] = 0; ph[5] = 0; ph[7] = 0;         // seed AT the target, not at zero,
                                             // or it rings up from nothing on
                                             // the first second of every start
    for (int k = 0; k < 16; k++) st[k] = 0;
    st[0] = want;
  }

  int dt = (int)fx_dt8(st + 2);
  if (dt > 60) dt = 60;                      // keep the integrator sane

  // --- the surface as a damped spring --------------------------------------
  int target = -110 + ((int)SEGMENT.custom1 * 230) / 255;
  if (SEGMENT.check1) target += (bass * 70) / 255;         // bass fills it
  if (target > 150) target = 150;

  const int slosh = 20 + (int)SEGMENT.custom2;

  // A shove is a kick. st.shake was shaped to drop straight into a beat slot -
  // one shot, proportional, refractory - so it does, rather than opening a
  // second trigger path that fights the first one.
  uint8_t knock = peak;
  if (live && imu->shake > knock) knock = imu->shake;

  if (knock && SEGMENT.check2) {                           // a kick hits the tank
    ph[1] = (int16_t)(ph[1] + (slosh * (int)knock) / 200);
    ph[3] = (int16_t)(ph[3] + (int)hw_random16((uint16_t)slosh) - slosh / 2);
    ph[5] = (int16_t)(ph[5] + (int)hw_random16((uint16_t)slosh) - slosh / 2);
    ph[7] = (int16_t)(ph[7] + (int)hw_random16((uint16_t)slosh) - slosh / 2);
    st[1] = knock;
  }

  ph[1] = (int16_t)(ph[1] + ((target - (int)ph[0]) * dt) / 150 - ((int)ph[1] * dt) / 400);
  ph[0] = (int16_t)(ph[0] + ((int)ph[1] * dt) / 150);

  if (((int)ph[2] * tgt[0] + (int)ph[4] * tgt[1] + (int)ph[6] * tgt[2]) < LIQ_FLIP) {
    ph[2] = (int16_t)tgt[0]; ph[4] = (int16_t)tgt[1]; ph[6] = (int16_t)tgt[2];
    ph[3] = 0; ph[5] = 0; ph[7] = 0;
  } else {
    for (int k = 0; k < 3; k++) {                          // normal returns to level
      int16_t *p = ph + 2 + k * 2;
      p[1] = (int16_t)(p[1] + ((tgt[k] - (int)p[0]) * dt) / 130 - ((int)p[1] * dt) / 350);
      p[0] = (int16_t)(p[0] + ((int)p[1] * dt) / 130);
    }
  }

  // Back onto the unit sphere. Springing three components independently is a
  // damped oscillator per axis, and at these constants that overshoots ~24% -
  // left alone the normal's LENGTH would pump, and length is what sets how
  // deep the water reads. Renormalising keeps the slosh (a direction change)
  // and throws away the pumping. The radial part of the velocity goes with it,
  // or it spends every frame pushing against a constraint that cancels it.
  {
    int nx = ph[2], ny = ph[4], nz = ph[6];
    const float nl = sqrtf((float)(nx * nx + ny * ny + nz * nz));
    if (nl > 8.0f) {
      const float s = (float)LIQ_UNIT / nl;
      nx = (int)((float)nx * s); ny = (int)((float)ny * s); nz = (int)((float)nz * s);
      ph[2] = (int16_t)nx; ph[4] = (int16_t)ny; ph[6] = (int16_t)nz;
      const int d = ((int)ph[3] * nx + (int)ph[5] * ny + (int)ph[7] * nz) >> 7;
      ph[3] = (int16_t)((int)ph[3] - ((d * nx) >> 7));
      ph[5] = (int16_t)((int)ph[5] - ((d * ny) >> 7));
      ph[7] = (int16_t)((int)ph[7] - ((d * nz) >> 7));
    } else {                                               // collapsed - reseed
      ph[2] = (int16_t)tgt[0]; ph[4] = (int16_t)tgt[1]; ph[6] = (int16_t)tgt[2];
      ph[3] = 0; ph[5] = 0; ph[7] = 0;
    }
  }
  M[6] = ph[2]; M[7] = ph[4]; M[8] = ph[6];

  { const int f = (int)st[1] - (int)fx_step(9, (uint16_t)dt);
    st[1] = (uint8_t)((f < 0) ? 0 : f); }
  if (live && imu->jolt > st[1] && SEGMENT.check2) st[1] = imu->jolt;

  // How far it is from the centre of the cube to the surface along the current
  // normal - 128 through a face, up to 219 through a corner. Fill rides it so
  // the water level stays proportional however the cube is sitting.
  int wSup = ((M[6] < 0) ? -M[6] : M[6])
           + ((M[7] < 0) ? -M[7] : M[7])
           + ((M[8] < 0) ? -M[8] : M[8]);
  if (wSup < 1) wSup = 1;
  const int lev = ((int)ph[0] * wSup) / LIQ_UNIT;

  const int rf   = 1 + (SEGMENT.custom3 >> 1);             // ripple pitch
  const int rAmp = 3 + (SEGMENT.custom3 >> 2) + ((int)st[1] >> 4);
  const uint8_t t1 = (uint8_t)((strip.now * (2 + (SEGMENT.speed >> 5))) >> 6);
  const uint8_t t2 = (uint8_t)((strip.now * (3 + (SEGMENT.speed >> 5))) >> 7);
  const int surfW = 7 + (SEGMENT.intensity >> 5);
  const uint8_t drive = cfx_drive(vol, 1.0f, 150 + (SEGMENT.intensity >> 1));

  SEGMENT.fill(SEGCOLOR(0));
  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int px = cx[i], py = cy[i], pz = cz[i];
      const bool isTop = cube && ((x / B) == 1 && (y / B) == 1);

      // this pixel in surface coordinates: two along the water, one across it
      const int rx = ((int)M[0] * px + (int)M[1] * py + (int)M[2] * pz) >> 7;
      const int ry = ((int)M[3] * px + (int)M[4] * py + (int)M[5] * pz) >> 7;
      const int h  = ((int)M[6] * px + (int)M[7] * py + (int)M[8] * pz) >> 7;

      // two crossing wave trains riding on the flat surface
      const int rip = (((int)sin8_t((uint8_t)(rx * rf + t1)) - 128)
                     + ((int)sin8_t((uint8_t)(ry * rf - t2)) - 128)) * rAmp / 128;
      const int sgn = h - lev - rip;                       // >0 dry, <0 wet

      uint32_t c; int lum;
      if (isTop) {
        if (sgn <= LIQ_BRIM) {                             // submerged to the brim
          lum = 220 + rip * 2;
          c = SEGMENT.color_from_palette((uint8_t)(70 + rip), false, false, 0);
        } else {                                           // looking down at it
          int att = (230 + LIQ_BRIM) - sgn;
          if (att < 25) att = 25;
          lum = (att * (150 + rip * 5)) >> 8;
          lum = (lum * topWin) >> 7;                       // ...only while it faces up
          c = SEGMENT.color_from_palette((uint8_t)(96 + rip * 3), false, false, 0);
        }
      } else if (sgn < -surfW) {                           // body of the water
        int depth = -sgn - surfW; if (depth > 255) depth = 255;
        lum = 210 - depth / 2 + rip;
        c = SEGMENT.color_from_palette((uint8_t)(110 + depth / 3), false, false, 0);
      } else if (sgn <= surfW) {                           // the meniscus
        const int e = (sgn < 0) ? -sgn : sgn;
        lum = 255 - (e * 90) / (surfW + 1);
        c = RGBW32(210, 245, 255, 0);
      } else {
        continue;                                          // dry
      }
      if (lum < 0) lum = 0; else if (lum > 255) lum = 255;
      SEGMENT.setPixelColorXY(x, y, mq_scale(c, scale8((uint8_t)lum, drive)));
    }
  }
  FX_DONE;
}

static FX_RET mode_liquid()      { mode_liquid_core(false); FX_DONE; }
static FX_RET mode_liquid_gyro() { mode_liquid_core(true);  FX_DONE; }

static const char _data_FX_MODE_LIQUID[] PROGMEM =
  "Ace 3-D Liquid@Ripple speed,Surface,Fill,Slosh,Ripple size,Bass fills,Splash on beat,Flat mode;;!;2f;sx=130,ix=140,c1=130,c2=120,c3=14,o1=1,o2=1";

static const char _data_FX_MODE_LIQUID_GYRO[] PROGMEM =
  "Ace Gyro Liquid@Ripple speed,Surface,Fill,Slosh,Ripple size,Bass fills,Splash on hit,Flat mode;;!;2f;sx=130,ix=140,c1=130,c2=120,c3=14,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_LiquidUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_liquid,      _data_FX_MODE_LIQUID);
    strip.addEffect(255, &mode_liquid_gyro, _data_FX_MODE_LIQUID_GYRO);
  }
  void loop() override {}
};

static CubeFx_LiquidUsermod cube_fx_liquid;
REGISTER_USERMOD(cube_fx_liquid);
