#pragma once

// ===========================================================================
// cube_fx_imu.h - orientation and motion for the Ace LED FX effects
// ===========================================================================
// Companion to cube_fx_common.h. Where that file answers "where is this pixel
// on the cube", this one answers "which way is the cube pointing, and what is
// it doing". Include it only in effects that want to react to being tilted,
// spun, shaken or set down on a different face - it costs nothing in effects
// that don't include it.
//
// The design goal is the same as the audio side: effects should never touch
// the sensor. They read ONE struct, once per frame, in cube coordinates that
// already match cfx_pos() - +X east, +Y north, +Z top, unit vectors in the
// -127..127 scale. Everything the sensor does badly (raw axes in whatever
// orientation the board happened to get glued in, noise, drift, gravity mixed
// into the accelerometer) is dealt with here, once.
//
// ---------------------------------------------------------------------------
// WHERE THE DATA COMES FROM
// ---------------------------------------------------------------------------
// A driver usermod owns the sensor and hands us nine numbers. In this project
// that driver is ace_imu_mpu6050.cpp, which reads the raw registers itself and
// runs its own complementary filter:
//
//     #define CFX_IMU_FEED_ONLY 1                     // top of the driver
//     #include "cube_fx_imu.h"
//     ...
//     cfx_imuFeedUnits(upX, upY, upZ,                 // gravity-sense up, g
//                      rateX, rateY, rateZ,           // deg/s
//                      linX, linY, linZ);             // gravity-free, g
//
// The call is safe at any rate - feed it at 100 Hz or 10 Hz, the smoothing
// here is time-based, not frame-based.
//
// Running a DMP usermod instead, or any source that hands you raw counts?
// cfx_imuFeed() does the scaling for you. Either way the contract is the same,
// and the contract is the important part:
//
//   *** THE DRIVER DELIVERS CUBE COORDINATES. THIS FILE DOES NOT REMAP. ***
//
// NO sensor present, or the driver compiled out: cfx_imu().valid stays false,
// gravity eases to "top face up" and every effect renders exactly as it does
// today. Nothing needs an #ifdef.
//
// ---------------------------------------------------------------------------
// MOUNTING  (not here any more - read this before you go looking for it)
// ---------------------------------------------------------------------------
// The board is almost never glued in axis-aligned with the cube. That used to
// be fixed here, with -D CFX_IMU_AXIS_X/Y/Z build flags. It is not any more.
// Mounting belongs to the driver, for two reasons:
//
//   - The driver's settings page is live. Turn the cube, watch the face
//     readout, change a dropdown, save. No rebuild, no reflash.
//   - The driver checks the determinant. Every real mounting is a ROTATION.
//     Flipping exactly one axis is a REFLECTION, which no way of gluing a
//     board into a cube can produce, and the driver refuses to make one
//     silently. A remap layer here had no such guard - so a stale build flag
//     and a perfectly correct dropdown would compose into a mirror that
//     neither one displayed, and the only symptom was a horizon that leaned
//     the wrong way in one axis and the right way in the other.
//
// Two layers that each look right is how you get a bug nobody can see. There
// is exactly one now, and the #error below keeps it that way.
//
// ---------------------------------------------------------------------------
// WHAT EFFECTS ACTUALLY USE
// ---------------------------------------------------------------------------
//   cfx_imuUp(st, px,py,pz)  height of a pixel along world up, -127..127.
//        The workhorse. > 0 is above the cube's centre plane in the ROOM, so
//        `h < level` is a liquid fill, `abs(h) < w` is a horizon band, and a
//        gradient in h is light coming from the sky. One dot product.
//   st.upFace / st.faceLock  which face is skyward, with hysteresis - for
//        effects that want to behave differently depending on how it's set
//        down, without flickering while it's being carried.
//   st.tilt        0 level, 128 on its side, 255 upside down.
//   st.spin        rotation about world up. Signed, so it can drive direction.
//   st.shake       one-shot, proportional, refractory - deliberately the same
//        shape as fx_lowBeat() so it can drop straight into a beat slot.
//   st.jolt        the ring-out tail of the last shake, like cfx_drop().intensity.
//   st.stillMs     ms since anything moved. Ambient/idle modes.
//   st.basis[9]    cube -> world rotation, Q7. cfx_imuWorld() applies it, and a
//        3D field sampled through it is locked to the ROOM, so noise, gyroid
//        and lattice stop tumbling with the cube and the cube slides through
//        them instead. Costs 9 multiplies per pixel, so make it a checkbox.
// ===========================================================================

#include "wled.h"

// The sensor usermod has no business dragging in the whole effect toolkit just
// to hand us nine numbers. It defines CFX_IMU_FEED_ONLY before including this
// and gets the input side alone; effects include it normally and get all of it.
#ifndef CFX_IMU_FEED_ONLY
  #include "cube_fx_common.h"
#endif

// Face ids, matching the order used by LF_N in cube_fx_common.h and by the
// per-pixel face ids in the effects that already switch on them.
#define CFX_FACE_TOP    0
#define CFX_FACE_NORTH  1
#define CFX_FACE_SOUTH  2
#define CFX_FACE_WEST   3
#define CFX_FACE_EAST   4
#define CFX_FACE_BOTTOM 5

// --- stale mounting flags: fail loudly rather than compose a silent mirror --
// CFX_IMU_AXIS_X/Y/Z and CFX_IMU_UP_SIGN used to live here. If your
// platformio_override.ini still sets any of them, the build stops on the line
// below instead of quietly applying a second, unguarded axis map on top of the
// driver's. Delete the -D lines; set AxisX / AxisY / AxisZ in
// Settings -> Usermods -> AceIMU instead.
#if defined(CFX_IMU_AXIS_X) || defined(CFX_IMU_AXIS_Y) || \
    defined(CFX_IMU_AXIS_Z) || defined(CFX_IMU_UP_SIGN)
  #error "CFX_IMU_AXIS_* / CFX_IMU_UP_SIGN are gone. Mounting now lives in Settings -> Usermods -> AceIMU (AxisX/AxisY/AxisZ). Delete these -D flags from platformio_override.ini."
#endif

// The vector the driver feeds us is gravity-SENSE up: flat on the bench with
// the top face skyward it reads (0,0,+1), the same convention an accelerometer
// at rest reports. If a source hands you true gravity instead (pointing at the
// floor), negate it in that driver, not here.

// --- sensor scaling ---------------------------------------------------------
// Only cfx_imuFeed() touches these. ace_imu_mpu6050.cpp calls
// cfx_imuFeedUnits() and scales in its own settings page, so on this build
// they are dead weight - changing them does nothing. They matter only if you
// wire up a DMP-based usermod again.
//
// These are the DMP FIFO packet's scales, which are NOT the raw register
// scales - a trap worth knowing about. i2cdevlib's dmpGetLinearAccel() removes
// gravity as `raw - gravity * 8192`, so anything that came out of
// dmpGetAccel()/dmpGetLinearAccel() is 8192 counts per g, half the 16384 you
// get reading the accelerometer registers directly.
#ifndef CFX_IMU_ACCEL_LSB
  #define CFX_IMU_ACCEL_LSB  8192.0f   // counts per g, DMP FIFO packet
#endif
// Gyro scale depends on the range MotionApps set at dmpInitialize() time, and
// forks differ. 16.4 is +/-2000 dps, which is what MotionApps20 configures;
// the alternatives are 32.8 (+/-1000), 65.5 (+/-500) and 131 (+/-250). This is
// THE value to sanity-check on hardware, and it is easy to read off the
// calibration effect: turn the cube steadily, about one revolution a second.
// If the spin ticks slam to full scale instantly, the number here is too small
// - go up a range. If a hard spin barely registers, go down. It only affects
// spin/heading, so a wrong guess costs you rate response, nothing else.
#ifndef CFX_IMU_GYRO_LSB
  #define CFX_IMU_GYRO_LSB   16.4f     // counts per deg/s, DMP FIFO packet
#endif
#ifndef CFX_IMU_RATE_FS
  #define CFX_IMU_RATE_FS    240       // deg/s that reads as full scale (127)
#endif
#ifndef CFX_IMU_ACC_FS
  #define CFX_IMU_ACC_FS     800       // milli-g that reads as full scale (255)
#endif

// --- behaviour --------------------------------------------------------------
#ifndef CFX_IMU_SMOOTH_MS
  #define CFX_IMU_SMOOTH_MS  45    // gravity follow time. lower = twitchier.
                                   // Was 110 for raw DMP output. What
                                   // ace_imu_mpu6050.cpp feeds is already
                                   // gyro-stabilised, so anything above ~50
                                   // here is pure added latency.
#endif
#ifndef CFX_IMU_RATE_SMOOTH_MS
  #define CFX_IMU_RATE_SMOOTH_MS 70
#endif
#ifndef CFX_IMU_TIMEOUT_MS
  #define CFX_IMU_TIMEOUT_MS 1500  // no feed for this long = sensor is gone
#endif
#ifndef CFX_IMU_FACE_MARGIN
  #define CFX_IMU_FACE_MARGIN 20   // a new face must lead the old one by this
#endif
#ifndef CFX_IMU_FACE_MS
  #define CFX_IMU_FACE_MS    260   // ...for this long, before upFace switches
#endif
#ifndef CFX_IMU_SHAKE_ON
  #define CFX_IMU_SHAKE_ON   105   // motion (0..255) that counts as a shake.
                                   // ~0.33 g against CFX_IMU_ACC_FS: a deliberate
                                   // jab, not somebody walking past the table
#endif
#ifndef CFX_IMU_SHAKE_MS
  #define CFX_IMU_SHAKE_MS   220   // refractory - one shake per gesture, not ten
#endif
#ifndef CFX_IMU_JOLT_MS
  #define CFX_IMU_JOLT_MS    700   // ms for jolt to ring out from 255 to 0
#endif
#ifndef CFX_IMU_STILL_LEVEL
  #define CFX_IMU_STILL_LEVEL 12   // motion below this, and slow, counts as still.
                                   // ~38 mg, comfortably above the DMP's own
                                   // residual gravity leakage on a tilted cube
#endif
// Integrated yaw so the horizontal half of basis[] tracks the ROOM rather than
// the cube. Honest about what it is: a 6-axis IMU has no compass, so this
// drifts a few degrees a minute. Build with 0 to let the horizontal axes ride
// with the cube instead, which several effects prefer anyway.
#ifndef CFX_IMU_YAW_TRACK
  #define CFX_IMU_YAW_TRACK  1
#endif
// Bench mode: with no sensor connected, tumble gravity slowly so IMU-aware
// effects can be developed on a plain panel. Off by default.
#ifndef CFX_IMU_SIM
  #define CFX_IMU_SIM        0
#endif

// ---------------------------------------------------------------------------
// Input side. Called from whatever usermod owns the sensor, at its own rate.
// ---------------------------------------------------------------------------
struct CfxImuRaw {
  float    g[3];      // gravity, sensor frame, g
  float    w[3];      // angular rate, sensor frame, deg/s
  float    a[3];      // linear acceleration, sensor frame, g
  uint32_t stamp;     // millis() of the last feed
  bool     ever;      // anything has ever arrived
  uint32_t used;      // millis() an EFFECT last asked for the answer
};

// `inline`, not `static inline`: one shared store for the whole build. A
// static one would give every .cpp that includes this its own private copy and
// only the file holding the usermod would ever see the sensor.
inline CfxImuRaw &cfx_imuRawStore() {
  static CfxImuRaw r = {{0.0f, 0.0f, 1.0f}, {0, 0, 0}, {0, 0, 0}, 0, false, 0};
  return r;
}

// ---------------------------------------------------------------------------
// Back-pressure: is anything actually LOOKING at the sensor?
// ---------------------------------------------------------------------------
// cfx_imu() stamps `used` every time an effect asks for orientation, so the
// driver can tell the difference between "Cube Cell is tumbling to the gyro at
// 43 fps" and "Split GEQ has been running for an hour and has never once
// cared which way up the cube is".
//
// That distinction is worth real money on this build. A 14-byte burst read at
// 400 kHz is ~315 us of bus plus the complementary filter, a hundred times a
// second, on the same core that is packing five WS2812 lines. Spending it for
// an effect that never reads the result is pure loss - so the driver drops to
// a trickle poll when nobody is asking and jumps back to full rate within one
// sample when somebody does. The sensor is still tracking, just coarsely, so
// there is no cold-start lurch when an IMU effect is selected.
//
// Deliberately time-based rather than a flag an effect has to set: no effect
// had to be edited for this to work, and one that starts reading the IMU
// tomorrow gets the behaviour for free.
inline bool cfx_imuConsumed(uint32_t withinMs) {
  const CfxImuRaw &r = cfx_imuRawStore();
  return r.used && (millis() - r.used) < withinMs;
}

// Already-scaled feed: gravity in g, rates in deg/s, acceleration in g.
inline void cfx_imuFeedUnits(float gravX, float gravY, float gravZ,
                             float rateX, float rateY, float rateZ,
                             float accX,  float accY,  float accZ) {
  CfxImuRaw &r = cfx_imuRawStore();
  r.g[0] = gravX; r.g[1] = gravY; r.g[2] = gravZ;
  r.w[0] = rateX; r.w[1] = rateY; r.w[2] = rateZ;
  r.a[0] = accX;  r.a[1] = accY;  r.a[2] = accZ;
  r.stamp = millis();
  r.ever  = true;
}

// MPU6050 DMP feed: gravity is already a float vector, gyro and linear accel
// are raw counts. Matches the member names in the mpu6050_imu usermod.
inline void cfx_imuFeed(float gravX, float gravY, float gravZ,
                        int16_t gyroX, int16_t gyroY, int16_t gyroZ,
                        int16_t accX,  int16_t accY,  int16_t accZ) {
  cfx_imuFeedUnits(gravX, gravY, gravZ,
                   (float)gyroX / CFX_IMU_GYRO_LSB,
                   (float)gyroY / CFX_IMU_GYRO_LSB,
                   (float)gyroZ / CFX_IMU_GYRO_LSB,
                   (float)accX  / CFX_IMU_ACCEL_LSB,
                   (float)accY  / CFX_IMU_ACCEL_LSB,
                   (float)accZ  / CFX_IMU_ACCEL_LSB);
}

#ifndef CFX_IMU_FEED_ONLY

// ---------------------------------------------------------------------------
// What every effect reads.
// ---------------------------------------------------------------------------
struct CfxImuState {
  int8_t   ux, uy, uz;           // world UP expressed in cube coords, unit
  int8_t   gx, gy, gz;           // gravity - where things fall, = -up
  uint8_t  tilt;                 // 0 top up, 128 on its side, 255 inverted
  uint8_t  upFace;               // CFX_FACE_* currently pointing at the sky
  uint8_t  faceLock;             // 0..255, how settled that answer is
  int8_t   rateX, rateY, rateZ;  // body rotation rate about cube X/Y/Z
  int8_t   spin;                 // rotation rate about WORLD up
  uint8_t  motion;               // 0..255 smoothed gravity-free acceleration
  uint8_t  shake;                // one-shot strength, 0 on every other frame
  uint8_t  jolt;                 // decaying tail of the last shake
  uint8_t  heading;              // integrated yaw, 0..255. relative, drifts
  uint16_t stillMs;              // ms since motion, saturating
  bool     valid;                // a real sensor is feeding us
  int16_t  basis[9];             // cube -> world, Q7, rows = east, north, up
};

// ---------------------------------------------------------------------------
// Small maths helpers - no state, so `static inline` is correct here.
// ---------------------------------------------------------------------------
static inline float cfx_imuNorm(float *v) {
  const float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (len > 0.0001f) { v[0] /= len; v[1] /= len; v[2] /= len; }
  return len;
}
static inline void cfx_imuCross(const float *a, const float *b, float *o) {
  o[0] = a[1] * b[2] - a[2] * b[1];
  o[1] = a[2] * b[0] - a[0] * b[2];
  o[2] = a[0] * b[1] - a[1] * b[0];
}
static inline int8_t cfx_imuRate8(float dps) {
  const int v = (int)(dps * (127.0f / (float)CFX_IMU_RATE_FS));
  return (int8_t)((v < -127) ? -127 : (v > 127 ? 127 : v));
}

// ---------------------------------------------------------------------------
// The per-frame update. One answer per frame no matter how many segments or
// effects ask, exactly like cfx_tempo() and cfx_drop().
// ---------------------------------------------------------------------------
inline const CfxImuState &cfx_imu() {
  static CfxImuState st = {0, 0, 127, 0, 0, -127, 0, CFX_FACE_TOP, 255,
                           0, 0, 0, 0, 0, 0, 0, 0, 0, false,
                           {127, 0, 0, 0, 127, 0, 0, 0, 127}};
  static uint32_t seenFrame = 0xFFFFFFFFu;
  static uint32_t dtStore   = 0;
  static float    up[3]     = {0.0f, 0.0f, 1.0f};   // smoothed, cube frame
  static float    east[3]   = {1.0f, 0.0f, 0.0f};   // horizontal reference
  static float    headF     = 0.0f;
  static uint32_t lastShake = 0;
  static uint8_t  cand      = CFX_FACE_TOP;
  static uint16_t candMs    = 0;

  // Stamped BEFORE the same-frame early return, not after: several segments
  // asking in one frame must all count as interest, or a two-segment cube
  // would look idle to the driver on every frame but the first.
  CfxImuRaw &raw = cfx_imuRawStore();
  raw.used = millis();

  if (strip.now == seenFrame) return st;
  seenFrame = strip.now;

  const uint16_t dt = fx_dt(dtStore);
  const bool live = raw.ever && (millis() - raw.stamp) < CFX_IMU_TIMEOUT_MS;

  // --- target gravity, in cube coordinates ---------------------------------
  float g[3], w[3], a[3];
  if (live) {
    // Already cube frame. The driver owns mounting - see MOUNTING above.
    g[0] = raw.g[0]; g[1] = raw.g[1]; g[2] = raw.g[2];
    w[0] = raw.w[0]; w[1] = raw.w[1]; w[2] = raw.w[2];
    a[0] = raw.a[0]; a[1] = raw.a[1]; a[2] = raw.a[2];
    cfx_imuNorm(g);
  } else {
#if CFX_IMU_SIM
    // Bench mode: a slow tumble so IMU-aware effects can be built without one.
    const float t = (float)strip.now * 0.00035f;
    g[0] = sinf(t) * 0.75f; g[1] = sinf(t * 0.61f) * 0.6f; g[2] = cosf(t * 0.43f);
    cfx_imuNorm(g);
#else
    // No sensor: ease back to level rather than freezing mid-tilt, so an
    // effect left running when the usermod drops out settles instead of
    // staying stuck on its side.
    g[0] = 0.0f; g[1] = 0.0f; g[2] = 1.0f;
#endif
    w[0] = w[1] = w[2] = 0.0f;
    a[0] = a[1] = a[2] = 0.0f;
  }
  st.valid = live;

  // --- smooth, frame-rate independently ------------------------------------
  {
    const float tx = g[0];
    const float ty = g[1];
    const float tz = g[2];
    const float d  = up[0] * tx + up[1] * ty + up[2] * tz;
    if (d < -0.85f) {
      // The cube has been turned over. Smoothing toward an antipodal target
      // means dragging the vector through its own origin, which is both
      // meaningless and numerically unstable - there is no "halfway" between
      // up and down. Take the new answer.
      up[0] = tx; up[1] = ty; up[2] = tz;
    } else {
      float k = (float)dt / (float)(CFX_IMU_SMOOTH_MS + dt);
      if (d < 0.0f && k < 0.5f) k = 0.5f;   // big swing: don't crawl after it
      up[0] += (tx - up[0]) * k;
      up[1] += (ty - up[1]) * k;
      up[2] += (tz - up[2]) * k;
    }
    if (cfx_imuNorm(up) < 0.0001f) { up[0] = 0.0f; up[1] = 0.0f; up[2] = 1.0f; }
  }
  st.ux = cfx_clamp8(up[0]); st.uy = cfx_clamp8(up[1]); st.uz = cfx_clamp8(up[2]);
  st.gx = (int8_t)-st.ux;    st.gy = (int8_t)-st.uy;    st.gz = (int8_t)-st.uz;
  st.tilt = (uint8_t)(((127 - (int)st.uz) * 255) / 254);

  // --- rotation rates -------------------------------------------------------
  {
    static float rs[3] = {0.0f, 0.0f, 0.0f};
    const float k = (float)dt / (float)(CFX_IMU_RATE_SMOOTH_MS + dt);
    for (int i = 0; i < 3; i++) rs[i] += (w[i] - rs[i]) * k;
    st.rateX = cfx_imuRate8(rs[0]);
    st.rateY = cfx_imuRate8(rs[1]);
    st.rateZ = cfx_imuRate8(rs[2]);
    const float yaw = rs[0] * up[0] + rs[1] * up[1] + rs[2] * up[2];  // about world up
    st.spin = cfx_imuRate8(yaw);
    headF += yaw * (float)dt * (256.0f / 360000.0f);
    headF -= floorf(headF / 256.0f) * 256.0f;
    st.heading = (uint8_t)headF;
  }

  // --- linear acceleration, shake, stillness -------------------------------
  {
    const float mag = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    int mg = (int)(mag * 1000.0f * 255.0f / (float)CFX_IMU_ACC_FS);
    if (mg > 255) mg = 255;
    st.motion = fx_env(st.motion, (uint8_t)mg, dt, 260);

    st.shake = 0;
    if (mg >= CFX_IMU_SHAKE_ON && (strip.now - lastShake) > CFX_IMU_SHAKE_MS) {
      lastShake = strip.now;
      st.shake  = (uint8_t)mg;             // strength, not a flag
      st.jolt   = (uint8_t)mg;
    } else {
      const int d = (int)st.jolt - (int)((255 * (int)dt) / CFX_IMU_JOLT_MS) - 1;
      st.jolt = (uint8_t)((d < 0) ? 0 : d);
    }

    const int spinAbs = (st.spin < 0) ? -st.spin : st.spin;
    if (st.motion < CFX_IMU_STILL_LEVEL && spinAbs < 6) {
      const uint32_t s = (uint32_t)st.stillMs + dt;
      st.stillMs = (uint16_t)((s > 65000u) ? 65000u : s);
    } else {
      st.stillMs = 0;
    }
  }

  // --- which face is up, with hysteresis -----------------------------------
  {
    const int dots[6] = { st.uz, st.uy, (int)-st.uy, (int)-st.ux, st.ux, (int)-st.uz };
    int best = 0;
    for (int f = 1; f < 6; f++) if (dots[f] > dots[best]) best = f;

    if (best == st.upFace) {
      cand = st.upFace; candMs = 0;
      const uint32_t l = (uint32_t)st.faceLock + (dt * 255u) / CFX_IMU_FACE_MS;
      st.faceLock = (uint8_t)((l > 255u) ? 255u : l);
    } else if (dots[best] > dots[st.upFace] + CFX_IMU_FACE_MARGIN) {
      if (best != cand) { cand = (uint8_t)best; candMs = 0; }
      candMs = (uint16_t)(candMs + dt);
      st.faceLock = (st.faceLock > 8) ? (uint8_t)(st.faceLock - 8) : 0;
      if (candMs >= CFX_IMU_FACE_MS) { st.upFace = cand; candMs = 0; st.faceLock = 0; }
    } else {
      candMs = 0;
    }
  }

  // --- cube -> world basis --------------------------------------------------
  // `east` is carried between frames and only ever nudged, so the horizontal
  // axes never snap when the cube passes through a pole - the failure mode of
  // rebuilding the basis from a fixed reference axis every frame.
  {
#if CFX_IMU_YAW_TRACK
    // Rotate the reference the opposite way to the cube's yaw so it keeps
    // pointing at the same corner of the ROOM.
    const float th = -((float)st.spin * (float)CFX_IMU_RATE_FS / 127.0f)
                     * (float)dt * (3.14159265f / 180000.0f);
    float ue[3];
    cfx_imuCross(up, east, ue);
    east[0] += ue[0] * th; east[1] += ue[1] * th; east[2] += ue[2] * th;
#endif
    const float d = east[0] * up[0] + east[1] * up[1] + east[2] * up[2];
    east[0] -= up[0] * d; east[1] -= up[1] * d; east[2] -= up[2] * d;
    if (cfx_imuNorm(east) < 0.05f) {          // degenerate - reseed off cube +Y
      float seed[3] = {0.0f, 1.0f, 0.0f};
      cfx_imuCross(seed, up, east);
      if (cfx_imuNorm(east) < 0.05f) { east[0] = 1.0f; east[1] = 0.0f; east[2] = 0.0f; }
    }
    float north[3];
    cfx_imuCross(up, east, north);
    for (int i = 0; i < 3; i++) {
      st.basis[i]     = (int16_t)(east[i]  * 127.0f);
      st.basis[3 + i] = (int16_t)(north[i] * 127.0f);
      st.basis[6 + i] = (int16_t)(up[i]    * 127.0f);
    }
  }

  return st;
}

// ---------------------------------------------------------------------------
// Per-pixel helpers. All integer, all -127..127 in and out, so they drop into
// the same expressions the cx/cy/cz LUT already feeds.
// ---------------------------------------------------------------------------

// Height of a surface point along world up. The one every effect wants.
static inline int cfx_imuUp(const CfxImuState &st, int px, int py, int pz) {
  return ((int)st.ux * px + (int)st.uy * py + (int)st.uz * pz) >> 7;
}

// Full cube -> world rotation. Sample a 3D field through this and the field
// stays put in the room while the cube moves through it.
static inline void cfx_imuWorld(const CfxImuState &st, int px, int py, int pz,
                                int &wx, int &wy, int &wz) {
  wx = ((int)st.basis[0] * px + (int)st.basis[1] * py + (int)st.basis[2] * pz) >> 7;
  wy = ((int)st.basis[3] * px + (int)st.basis[4] * py + (int)st.basis[5] * pz) >> 7;
  wz = ((int)st.basis[6] * px + (int)st.basis[7] * py + (int)st.basis[8] * pz) >> 7;
}

// Blend factor for effects that want to fade their IMU response in and out
// rather than switch it: 0 when there is no sensor, 255 when there is.
inline uint8_t cfx_imuMix(uint8_t want) {
  return cfx_imu().valid ? want : 0;
}

#endif  // CFX_IMU_FEED_ONLY
