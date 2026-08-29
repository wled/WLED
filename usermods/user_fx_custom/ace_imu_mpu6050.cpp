#include "wled.h"
#include <Wire.h>

#define CFX_IMU_FEED_ONLY 1
#include "cube_fx_imu.h"

// ===========================================================================
// ace_imu_mpu6050.cpp - our own MPU6050 driver, tunable from the web UI
// ===========================================================================
// Replaces the stock `mpu6050_imu` usermod entirely. Drop this file next to
// the effects in usermods/user_fx_custom/ and it self-registers like they do -
// no library.json edit, no custom_usermods entry beyond the one you already
// have, and no patch to any WLED file.
//
//   *** REMOVE `mpu6050_imu` FROM custom_usermods BEFORE BUILDING. ***
//
// If both are compiled in, both call cfx_imuFeed() and the bridge sees two
// sensors fighting over one store. You can also drop the ElectronicCats
// MPU6050 lib_deps line - nothing here needs it.
//
// ---------------------------------------------------------------------------
// WIRING ON THIS BUILD
// ---------------------------------------------------------------------------
//   MPU6050 / GY-521      SDA -> GPIO21      SCL -> GPIO13
//   SSD1306 panel         SDA -> GPIO23      SCL -> GPIO22   (Wire1, its own)
//
// Two separate I2C peripherals, one device each. That is not belt and braces:
// sharing one bus between the panel and the sensor back-powers the MPU6050
// through its ESD diodes whenever VCC is absent, and a 128-byte panel page
// blocks the bus for 2.9 ms at 400 kHz, which lands squarely on top of the
// sensor poll.
//
//   VCC -> 3.3 V, NOT 5 V. A 5 V module pulls SDA/SCL to 5 V through its own
//          pull-ups and puts 5 V on a 3.3 V input.
//   AD0 -> GND for 0x68. Floating usually reads 0x68 anyway; tie it.
//
// ---------------------------------------------------------------------------
// I2C PINS: SET THEM HERE, NOT IN LED PREFERENCES
// ---------------------------------------------------------------------------
// Settings -> LED Preferences greys the global SDA/SCL fields out once
// anything has claimed the bus, which means that on a running build you often
// cannot change them at all without wiping the config. So this usermod carries
// its own `sda` and `scl` and defaults them to the pins actually wired: 21 and
// 13. The resolution order is:
//
//   sda/scl = -1        follow WLED's global I2C pins, as before
//   sda/scl = the same  as the global pins - share the bus WLED already began
//   sda/scl = different and the global pins are UNSET - we begin Wire ourselves
//   sda/scl = different and the global pins are SET   - refused, unless you
//                       tick Force. Re-pinning Wire underneath another
//                       usermod that is already talking on it is not something
//                       to do silently.
//
// One consequence worth knowing: usermods that gate themselves on the GLOBAL
// i2c_sda/i2c_scl being set - the MCP23017 path in ace_ui_encoder.cpp is one -
// will still consider the bus unconfigured. If you ever add the expander, set
// the global pins to 21/13 as well and this file will detect the match and
// share rather than re-begin.
//
// ---------------------------------------------------------------------------
// WHY NOT THE DMP
// ---------------------------------------------------------------------------
// The DMP is a fixed-function black box: fixed 200 Hz, fixed +/-2000 dps and
// +/-2 g, undocumented FIFO scaling that has already cost us a bug, and a 3 KB
// blob upload on every boot that fails silently on a marginal bus. It also
// leaves nothing to adjust, which defeats the point of a settings page.
//
// What the effects actually want from the sensor is small: a stable gravity
// direction, body rates, and gravity-free acceleration. A complementary filter
// gives all three from raw registers - gyro integration for the fast, smooth
// part, accelerometer correction for the slow, absolute part, with the
// correction gated off while the cube is being thrown around so linear
// acceleration cannot drag the horizon with it. Roughly forty lines, and every
// constant in it is a slider.
//
// ---------------------------------------------------------------------------
// IDLE THROTTLING - THE SNAPPINESS FIX
// ---------------------------------------------------------------------------
// Most of the 30 cube effects never ask which way up the cube is. Polling at
// 100 Hz for them costs a 14-byte burst read plus a complementary filter
// update a hundred times a second, on the core that is also packing five
// WS2812 data lines, and buys precisely nothing.
//
// cfx_imu() now stamps the shared store every time an EFFECT reads it, so this
// file can tell. No effect needed editing, and one that starts reading the IMU
// tomorrow gets the behaviour for free:
//
//     an effect read it within idleMs   ->  poll at `hz`     (100)
//     nobody has                        ->  poll at `idleHz` (8)
//
// The sensor keeps tracking either way, just coarsely, so selecting an IMU
// effect does not produce a cold-start lurch - the first full-rate sample
// lands within 125 ms and the filter is already in the right hemisphere. A
// gyro calibration always forces full rate, or 200 samples at 8 Hz would take
// twenty-five seconds of holding still.
//
// ---------------------------------------------------------------------------
// BRINGING IT UP  (order matters)
// ---------------------------------------------------------------------------
// 1. Flash, open Settings -> Usermods. The Info row is labelled "AceIMU" and
//    should say `ok` with a live temperature. `not found` means address or
//    wiring; check 3.3 V first, then AD0.
// 2. Sit the cube flat, top face up, and run Calibrate gyro - from this page,
//    or from the knob under System -> Calibrate gyro. Two seconds of
//    stillness. Spin should now read ~0 at rest.
// 3. Fix the mounting. Info shows the up vector and which face it thinks is
//    skyward. Turn the cube by hand until all five faces read correctly.
//      - a face right but its opposite wrong  -> one sign is flipped
//      - turning about one axis moves another -> two entries are swapped
//      - "axis map mirrored" in the status    -> STOP. Read the note above
//        checkMap(). You are trying to build a reflection, which is not a
//        thing a physically mounted board can be. Two signs have to move
//        together: a board fitted upside down is +X, -Y, -Z, not +X, +Y, -Z.
// 4. Any residual lean - the board glued in at 4 degrees - is Level now with
//    the cube sat flat. That captures the current gravity direction as the new
//    "top up" and trims it out. Changing AxisX/Y/Z afterwards discards it
//    automatically; the trim is stored in cube frame, and re-mapping moves the
//    frame out from under it.
// 5. Open "Ace 3-D IMU Axes". The horizon band must stay level in the room
//    however you turn the cube, and tipping the top toward you must drop the
//    band on the face you are looking at.
// 6. Last check, and it catches a whole class of bug: the amber cross must sit
//    on the same face the Info panel names. Info reports THIS file's state;
//    the cross reports what cube_fx_imu.h passed on to the effects.
//
// ---------------------------------------------------------------------------
// BUILD FLAGS: NONE
// ---------------------------------------------------------------------------
// cube_fx_imu.h hard-errors on the old CFX_IMU_AXIS_X/Y/Z and CFX_IMU_UP_SIGN
// flags rather than letting them stack a second axis map on top of this one.
// If platformio_override.ini still carries them from the DMP days, delete the
// lines - that pairing is exactly how you get a horizon that leans wrong in
// one axis and right in the other, with both layers looking correct.
// ===========================================================================

// ---------------------------------------------------------------------------
// Config-save compatibility
// ---------------------------------------------------------------------------
// WLED PR #4609 split config serialisation in two: serializeConfig() became
// serializeConfigToFS(), and the name serializeConfig() was reused for a new
// overload taking a JsonObject. On that WLED a bare serializeConfig() call
// fails with "too few arguments in function call".
//
// Older WLED (single no-argument serializeConfig()):  -D AIMU_LEGACY_CFG_SAVE=1
// ---------------------------------------------------------------------------
#ifndef AIMU_LEGACY_CFG_SAVE
  #define AIMU_LEGACY_CFG_SAVE 0
#endif

static inline void aimuSaveConfig() {
#if AIMU_LEGACY_CFG_SAVE
  serializeConfig();
#else
  serializeConfigToFS();
#endif
}

#ifndef AIMU_LEGACY_PINMGR
  #define AIMU_LEGACY_PINMGR 0
#endif
static bool aimuAllocPin(int p) {
  if (p < 0) return true;
#if AIMU_LEGACY_PINMGR
  return pinManager.allocatePin((byte)p, true, PinOwner::UM_Unspecified);
#else
  return PinManager::allocatePin((byte)p, true, PinOwner::UM_Unspecified);
#endif
}

// --- MPU6050 registers ------------------------------------------------------
#define AIMU_SMPLRT_DIV    0x19
#define AIMU_CONFIG        0x1A
#define AIMU_GYRO_CONFIG   0x1B
#define AIMU_ACCEL_CONFIG  0x1C
#define AIMU_ACCEL_XOUT_H  0x3B
#define AIMU_PWR_MGMT_1    0x6B
#define AIMU_WHO_AM_I      0x75

class AceImuUsermod : public Usermod {
 private:
  // --- persisted config -----------------------------------------------------
  bool  enabled   = true;
  int   sda       = 21;     // OUR pins. -1 = follow WLED's global I2C
  int   scl       = 13;
  bool  force     = false;  // re-pin Wire even if the global pins disagree
  int   addr      = 0x68;   // 104 / 105
  int   hz        = 100;    // poll rate while an effect is reading us
  int   idleHz    = 8;      // poll rate while nothing is
  int   idleMs    = 600;    // no effect read for this long = idle
  int   accelG    = 4;      // +/- g full scale
  int   gyroDPS   = 500;    // +/- deg/s full scale
  int   lpfIdx    = 3;      // DLPF_CFG, 0..6 -> 260..5 Hz
  int   axisX     = 1;      // which SENSOR axis feeds cube +X  (+/-1,2,3)
  int   axisY     = 2;      //                        cube +Y
  int   axisZ     = 3;      //                        cube +Z
  int   fuseMs    = 300;    // accel correction time constant
  int   gateMg    = 350;    // |a|-1g beyond this and correction is cut
  float deadDPS   = 1.2f;   // gyro deadband
  int   motionX   = 100;    // linear-accel gain into the bridge, %
  int   action    = 0;      // one-shot: 1 cal, 2 level, 3 clear, 4 re-init
  float gbX = 0, gbY = 0, gbZ = 0;    // gyro bias, deg/s, sensor frame
  float refX = 0, refY = 0, refZ = 1; // level reference, cube frame

  // --- runtime --------------------------------------------------------------
  bool     initDone = false, present = false, busOk = false, mapOk = true, mirrored = false;
  bool     hot = false;                       // an effect is reading us
  int      useSda = -1, useScl = -1;
  uint8_t  whoami = 0;
  uint32_t lastPollMs = 0, lastUs = 0, lastProbeMs = 0;
  float    accLsb = 8192.0f, gyrLsb = 65.5f;
  float    trim[9] = {1,0,0, 0,1,0, 0,0,1};   // level-trim rotation, cube frame
  float    upEst[3] = {0,0,1};                // filtered up, cube frame, unit
  float    rate[3]  = {0,0,0};                // deg/s, cube frame
  float    lin[3]   = {0,0,0};                // g, gravity-free, cube frame
  float    tempC    = 0;
  uint16_t obsHz = 0, sampleCount = 0;
  uint32_t rateWindow = 0;
  bool     haveUp = false;
  int8_t   pendAction = 0;

  // gyro calibration state machine
  uint8_t  calState = 0;
  uint16_t calN = 0;
  float    calSum[3] = {0,0,0}, calMin[3] = {0,0,0}, calMax[3] = {0,0,0};

  const char *statusMsg = "starting";

  static const char _name[];
  static const char _enabled[];

  // --- I2C ------------------------------------------------------------------
  bool wr8(uint8_t reg, uint8_t val) {
    Wire.beginTransmission((uint8_t)addr);
    Wire.write(reg); Wire.write(val);
    return Wire.endTransmission() == 0;
  }
  bool rd(uint8_t reg, uint8_t *buf, uint8_t n) {
    Wire.beginTransmission((uint8_t)addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)addr, (int)n) != n) return false;
    for (uint8_t i = 0; i < n; i++) buf[i] = Wire.read();
    return true;
  }

  // Resolve which pins we are actually going to talk on, and bring the bus up
  // if it is ours to bring up. See the pin note at the top of the file for why
  // this is not simply "use the global pair".
  bool busBringUp() {
    const bool wantOwn = (sda >= 0 && scl >= 0);

    if (!wantOwn) {
      if (i2c_sda < 0 || i2c_scl < 0) { statusMsg = "set I2C pins first"; return false; }
      useSda = i2c_sda; useScl = i2c_scl;
      Wire.setClock(400000);
      return true;
    }
    if (sda == i2c_sda && scl == i2c_scl) {    // WLED already began exactly this
      useSda = sda; useScl = scl;
      Wire.setClock(400000);
      return true;
    }
    if ((i2c_sda >= 0 || i2c_scl >= 0) && !force) {
      statusMsg = "pins differ from global I2C - tick Force";
      return false;
    }
    // Best effort: an allocation failure here usually means we already own the
    // pins from a previous settings save, which is fine. A genuine clash shows
    // up immediately as a dead sensor and a NAK, not as silent corruption.
    aimuAllocPin(sda);
    aimuAllocPin(scl);
    Wire.begin((int)sda, (int)scl, 400000);
    useSda = sda; useScl = scl;
    return true;
  }

  // --- sensor config --------------------------------------------------------
  // Scale codes. Anything unrecognised falls back to the widest range, which
  // is the safe direction to be wrong in - clipping loses the gesture entirely.
  static uint8_t afsCode(int g) { return g <= 2 ? 0 : g <= 4 ? 1 : g <= 8 ? 2 : 3; }
  static uint8_t fsCode(int d)  { return d <= 250 ? 0 : d <= 500 ? 1 : d <= 1000 ? 2 : 3; }

  void applyRanges() {
    if (!present) return;
    const uint8_t afs = afsCode(accelG), fs = fsCode(gyroDPS);
    const uint8_t lpf = (lpfIdx < 0) ? 0 : (lpfIdx > 6 ? 6 : (uint8_t)lpfIdx);
    accLsb = 16384.0f / (float)(1 << afs);
    gyrLsb = 131.0f   / (float)(1 << fs);
    wr8(AIMU_CONFIG, lpf);
    wr8(AIMU_GYRO_CONFIG,  (uint8_t)(fs  << 3));
    wr8(AIMU_ACCEL_CONFIG, (uint8_t)(afs << 3));
    // Keep the internal update rate at 1 kHz whichever DLPF branch we are on,
    // so a poll never lands on a stale register set. The DLPF, not the sample
    // divider, is what does the anti-aliasing when we poll asynchronously -
    // and that matters more now that the poll rate itself moves between 8 and
    // 100 Hz depending on whether anybody is listening.
    wr8(AIMU_SMPLRT_DIV, (lpf == 0) ? 7 : 0);
    buildTrim();
    checkMap();
  }

  bool probe() {
    // Cheap liveness check first: if nothing ACKs, bail before spending 80 ms
    // on delays. That is what makes the retry in loop() free when the sensor
    // simply is not there.
    if (!rd(AIMU_WHO_AM_I, &whoami, 1)) return false;
    // 0x68 is the datasheet value; clones ship 0x70, 0x72, 0x74, 0x98. All of
    // them speak the same register map, so accept anything that answers.
    wr8(AIMU_PWR_MGMT_1, 0x80);   // reset
    delay(60);
    wr8(AIMU_PWR_MGMT_1, 0x01);   // wake, clock = PLL on gyro X
    delay(20);
    uint8_t chk = 0;
    if (!rd(AIMU_PWR_MGMT_1, &chk, 1)) return false;
    return true;
  }

  // --- mounting -------------------------------------------------------------
  // axisN names a SENSOR axis (1=x 2=y 3=z, negative flips) that feeds one CUBE
  // axis. Three of those form a signed permutation matrix; it is only a real
  // rotation if its determinant is +1. A determinant of -1 is a mirror, which
  // no physical mounting can produce, and it silently reverses every rotation
  // the gyro reports - so we flag it rather than let it confuse you later.
  void checkMap() {
    const int sel[3] = {axisX, axisY, axisZ};
    int col[3];
    mapOk = true;
    for (int i = 0; i < 3; i++) {
      int a = sel[i] < 0 ? -sel[i] : sel[i];
      if (a < 1 || a > 3) { mapOk = false; return; }
      col[i] = a - 1;
    }
    if (col[0] == col[1] || col[1] == col[2] || col[0] == col[2]) { mapOk = false; return; }
    int inv = 0;
    for (int i = 0; i < 3; i++) for (int j = i + 1; j < 3; j++) if (col[i] > col[j]) inv++;
    int det = (inv & 1) ? -1 : 1;
    for (int i = 0; i < 3; i++) if (sel[i] < 0) det = -det;
    mirrored = (det < 0);
  }

  static inline void remap(const int *sel, const float *s, float *out) {
    for (int i = 0; i < 3; i++) {
      int a = sel[i] < 0 ? -sel[i] : sel[i];
      float v = s[a - 1];
      out[i] = (sel[i] < 0) ? -v : v;
    }
  }

  // Minimal rotation carrying the captured reference direction onto cube +Z.
  // Rodrigues, built once on config change - per-sample it is nine multiplies.
  void buildTrim() {
    float r[3] = {refX, refY, refZ};
    float m = sqrtf(r[0]*r[0] + r[1]*r[1] + r[2]*r[2]);
    if (m < 0.1f) { r[0] = 0; r[1] = 0; r[2] = 1; m = 1; }
    r[0] /= m; r[1] /= m; r[2] /= m;

    const float c = r[2];                        // dot(r, +Z)
    float v[3] = { r[1], -r[0], 0.0f };          // cross(r, +Z)
    if (c < -0.999f) {                           // antipodal: 180 deg about X
      trim[0]=1; trim[1]=0;  trim[2]=0;
      trim[3]=0; trim[4]=-1; trim[5]=0;
      trim[6]=0; trim[7]=0;  trim[8]=-1;
      return;
    }
    const float k = 1.0f / (1.0f + c);
    trim[0] = 1 - (v[1]*v[1] + v[2]*v[2]) * k;
    trim[1] = -v[2] + v[0]*v[1]*k;
    trim[2] =  v[1] + v[0]*v[2]*k;
    trim[3] =  v[2] + v[0]*v[1]*k;
    trim[4] = 1 - (v[0]*v[0] + v[2]*v[2]) * k;
    trim[5] = -v[0] + v[1]*v[2]*k;
    trim[6] = -v[1] + v[0]*v[2]*k;
    trim[7] =  v[0] + v[1]*v[2]*k;
    trim[8] = 1 - (v[0]*v[0] + v[1]*v[1]) * k;
  }

  inline void applyTrim(const float *in, float *out) {
    out[0] = trim[0]*in[0] + trim[1]*in[1] + trim[2]*in[2];
    out[1] = trim[3]*in[0] + trim[4]*in[1] + trim[5]*in[2];
    out[2] = trim[6]*in[0] + trim[7]*in[1] + trim[8]*in[2];
  }

  // --- the filter -----------------------------------------------------------
  void fuse(const float *aCube, const float *wCube, float dt) {
    if (!haveUp) {                               // first sample: snap, don't ease
      float m = sqrtf(aCube[0]*aCube[0] + aCube[1]*aCube[1] + aCube[2]*aCube[2]);
      if (m > 0.3f) { upEst[0]=aCube[0]/m; upEst[1]=aCube[1]/m; upEst[2]=aCube[2]/m; haveUp = true; }
      return;
    }

    // Gyro: a direction fixed in the world, expressed in the body frame, obeys
    // dv/dt = -w x v. Integrating that is what keeps the horizon steady while
    // the cube is actually moving - the accelerometer is useless right then.
    const float wx = wCube[0] * (float)DEG_TO_RAD;
    const float wy = wCube[1] * (float)DEG_TO_RAD;
    const float wz = wCube[2] * (float)DEG_TO_RAD;
    const float ux = upEst[0], uy = upEst[1], uz = upEst[2];
    upEst[0] -= (wy*uz - wz*uy) * dt;
    upEst[1] -= (wz*ux - wx*uz) * dt;
    upEst[2] -= (wx*uy - wy*ux) * dt;

    // Accelerometer: absolute but only trustworthy when the cube is coasting.
    // Weight the correction by how close |a| is to 1 g and it self-gates -
    // full authority at rest, none mid-shake, and a smooth ramp between.
    const float m = sqrtf(aCube[0]*aCube[0] + aCube[1]*aCube[1] + aCube[2]*aCube[2]);
    if (m > 0.05f) {
      const float gate = (gateMg < 20 ? 20 : gateMg) * 0.001f;
      float trust = 1.0f - fabsf(m - 1.0f) / gate;
      if (trust > 0.0f) {
        const float tau = (fuseMs < 20 ? 20 : fuseMs) * 0.001f;
        const float a = trust * (dt / (tau + dt));
        upEst[0] += (aCube[0]/m - upEst[0]) * a;
        upEst[1] += (aCube[1]/m - upEst[1]) * a;
        upEst[2] += (aCube[2]/m - upEst[2]) * a;
      }
    }

    float n = sqrtf(upEst[0]*upEst[0] + upEst[1]*upEst[1] + upEst[2]*upEst[2]);
    if (n < 1e-4f) { upEst[0]=0; upEst[1]=0; upEst[2]=1; return; }
    upEst[0]/=n; upEst[1]/=n; upEst[2]/=n;
  }

  void doAction(int a) {
    switch (a) {
      case 1:                                    // calibrate gyro
        calState = 1; calN = 0;
        calSum[0]=calSum[1]=calSum[2]=0;
        statusMsg = "hold still...";
        break;
      case 2:                                    // level now
        if (haveUp) {
          // Capture the CURRENT trimmed up vector composed onto the stored
          // reference, so repeated levelling converges instead of fighting.
          float cur[3] = {upEst[0], upEst[1], upEst[2]};
          float inv[3];                          // undo existing trim
          inv[0] = trim[0]*cur[0] + trim[3]*cur[1] + trim[6]*cur[2];
          inv[1] = trim[1]*cur[0] + trim[4]*cur[1] + trim[7]*cur[2];
          inv[2] = trim[2]*cur[0] + trim[5]*cur[1] + trim[8]*cur[2];
          refX = inv[0]; refY = inv[1]; refZ = inv[2];
          buildTrim();
          haveUp = false;
          statusMsg = "levelled";
          aimuSaveConfig();
        } else {
          statusMsg = "no fix yet - wait a second";
        }
        break;
      case 3:                                    // clear level trim
        refX = 0; refY = 0; refZ = 1;
        buildTrim(); haveUp = false;
        statusMsg = "trim cleared";
        aimuSaveConfig();
        break;
      case 4:                                    // re-init sensor
        present = probe();
        if (present) applyRanges();
        haveUp = false;
        statusMsg = present ? "ok" : "not found";
        break;
      default: break;
    }
  }

 public:
  // Called by ace_ui_menu.cpp through the weak aceImuAction() symbol at the
  // bottom of this file, so System -> Calibrate gyro / Level now / Clear level
  // trim work from the knob as well as from the settings page. Only sets a
  // flag; the work happens in loop() where the bus is safe to use.
  void requestAction(int8_t a) { pendAction = a; }

  void setup() override {
    buildTrim();
    checkMap();
    lastUs = micros();
    initDone = true;
    // Everything else - bus check, probe, range programming - happens in
    // loop(). Usermod setup() ordering against WLED's own Wire.begin() is not
    // something worth betting a silent failure on, and this way a sensor
    // plugged in later, or enabled later from the UI, just starts working.
    statusMsg = enabled ? "starting" : "disabled";
  }

  void loop() override {
    if (!initDone || !enabled) { if (initDone && !enabled) statusMsg = "disabled"; return; }
    const uint32_t now = millis();

    if (!busOk) {
      busOk = busBringUp();
      if (!busOk) return;
    }
    if (!present) {
      if (now - lastProbeMs < 3000) return;
      lastProbeMs = now;
      present = probe();
      if (!present) { statusMsg = "not found"; return; }
      applyRanges();
      haveUp = false;
      statusMsg = "ok";
      lastUs = micros();
    }

    // Actions first, so a knob press does not wait out an idle poll interval.
    if (pendAction) { int a = pendAction; pendAction = 0; doAction(a); }

    // Is anything actually looking? A calibration always counts - 200 samples
    // at the idle rate would be twenty-five seconds of holding still.
    hot = (calState != 0) || cfx_imuConsumed((uint32_t)(idleMs < 100 ? 100 : idleMs));

    const int wantHz = hot ? hz : (idleHz < 1 ? 1 : (idleHz > hz ? hz : idleHz));
    const uint32_t due = 1000u / (uint32_t)(wantHz < 1 ? 1 : (wantHz > 250 ? 250 : wantHz));
    if (now - lastPollMs < due) return;

    // Yield to the strip. When an effect is reading us we only yield until we
    // are three intervals late, because on a 48x48 cube isUpdating() is true
    // most of the time and an unconditional skip would starve the sensor. When
    // nobody is reading us there is no such urgency - just wait.
    if (strip.isUpdating() && (!hot || (now - lastPollMs) < due * 3)) return;
    lastPollMs = now;

    uint8_t b[14];
    if (!rd(AIMU_ACCEL_XOUT_H, b, 14)) { present = false; lastProbeMs = now; statusMsg = "bus error"; return; }

    const uint32_t us = micros();
    float dt = (us - lastUs) * 1e-6f;
    lastUs = us;
    if (dt <= 0.0f || dt > 0.5f) dt = 0.02f;     // first frame, or a long stall

    auto i16 = [](const uint8_t *p) { return (int16_t)((p[0] << 8) | p[1]); };
    float aS[3] = { i16(b+0) / accLsb, i16(b+2) / accLsb, i16(b+4) / accLsb };
    tempC = i16(b+6) / 340.0f + 36.53f;
    float wS[3] = { i16(b+8) / gyrLsb, i16(b+10) / gyrLsb, i16(b+12) / gyrLsb };

    // Gyro calibration runs on raw sensor-frame rates, before bias or remap.
    if (calState == 1) {
      if (calN == 0) { for (int i=0;i<3;i++) { calMin[i]=calMax[i]=wS[i]; } }
      for (int i = 0; i < 3; i++) {
        calSum[i] += wS[i];
        if (wS[i] < calMin[i]) calMin[i] = wS[i];
        if (wS[i] > calMax[i]) calMax[i] = wS[i];
      }
      if (++calN >= 200) {
        float spread = 0;
        for (int i = 0; i < 3; i++) { float s = calMax[i]-calMin[i]; if (s > spread) spread = s; }
        if (spread > 8.0f) {                     // it moved - a bad bias is worse than none
          statusMsg = "cal failed: moved";
        } else {
          gbX = calSum[0]/calN; gbY = calSum[1]/calN; gbZ = calSum[2]/calN;
          statusMsg = "ok";
          aimuSaveConfig();
        }
        calState = 0;
      }
      return;                                    // don't feed a moving average out
    }

    wS[0] -= gbX; wS[1] -= gbY; wS[2] -= gbZ;
    const float dead = deadDPS < 0 ? 0 : deadDPS;
    for (int i = 0; i < 3; i++) {
      if (wS[i] > -dead && wS[i] < dead) wS[i] = 0.0f;   // kill residual drift at rest
    }

    const int sel[3] = {axisX, axisY, axisZ};
    float aM[3], wM[3], aCube[3], wCube[3];
    if (mapOk) { remap(sel, aS, aM); remap(sel, wS, wM); }
    else       { for (int i=0;i<3;i++) { aM[i]=aS[i]; wM[i]=wS[i]; } }
    applyTrim(aM, aCube);
    applyTrim(wM, wCube);

    fuse(aCube, wCube, dt);
    for (int i = 0; i < 3; i++) {
      rate[i] = wCube[i];
      lin[i]  = (aCube[i] - upEst[i]) * (motionX * 0.01f);
    }

    // cube_fx_imu.h wants accelerometer-sense gravity: (0,0,+1) sitting level.
    cfx_imuFeedUnits(upEst[0], upEst[1], upEst[2],
                     rate[0],  rate[1],  rate[2],
                     lin[0],   lin[1],   lin[2]);

    sampleCount++;
    if (now - rateWindow >= 1000) { obsHz = sampleCount; sampleCount = 0; rateWindow = now; }
  }

  // --- info -----------------------------------------------------------------
  // Deliberately reports OUR state, not cfx_imu()'s. Calling into the bridge
  // from here would advance its frame clock, eat a one-shot shake out from
  // under an effect, AND stamp the store as "consumed" - which would peg the
  // poll rate to 100 Hz for as long as anybody had the Info page open.
  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray st = user.createNestedArray(FPSTR(_name));
    if (mapOk && mirrored && present) st.add(F("axis map mirrored"));
    else                              st.add(statusMsg);
    if (!mapOk) st.add(F(" - bad axis map")); else st.add("");

    if (!present || !enabled) return;

    char buf[48];
    JsonArray up = user.createNestedArray(F("IMU up"));
    snprintf_P(buf, sizeof(buf), PSTR("%+.2f %+.2f %+.2f"), upEst[0], upEst[1], upEst[2]);
    up.add(buf); up.add("");

    static const char *FACE[6] = {"Top","North","South","West","East","Bottom"};
    int f = 5;
    float ax = fabsf(upEst[0]), ay = fabsf(upEst[1]), az = fabsf(upEst[2]);
    if (az >= ax && az >= ay)      f = upEst[2] >= 0 ? 0 : 5;
    else if (ay >= ax)             f = upEst[1] >= 0 ? 1 : 2;
    else                           f = upEst[0] >= 0 ? 4 : 3;
    float tiltDeg = acosf(upEst[2] > 1 ? 1 : (upEst[2] < -1 ? -1 : upEst[2])) * (float)RAD_TO_DEG;

    JsonArray fa = user.createNestedArray(F("IMU face"));
    snprintf_P(buf, sizeof(buf), PSTR("%s, %.0f"), FACE[f], (double)tiltDeg);
    fa.add(buf); fa.add(F("&deg; tilt"));

    JsonArray mo = user.createNestedArray(F("IMU motion"));
    float spin = rate[0]*upEst[0] + rate[1]*upEst[1] + rate[2]*upEst[2];
    float mg   = sqrtf(lin[0]*lin[0] + lin[1]*lin[1] + lin[2]*lin[2]) * 1000.0f;
    snprintf_P(buf, sizeof(buf), PSTR("%.0f mg, spin %+.0f"), (double)mg, (double)spin);
    mo.add(buf); mo.add(F(" dps"));

    JsonArray hw = user.createNestedArray(F("IMU sensor"));
    snprintf_P(buf, sizeof(buf), PSTR("0x%02X id 0x%02X, %.1f"), addr, whoami, (double)tempC);
    hw.add(buf); hw.add(F("&deg;C"));

    // The one line that tells you the throttle is doing its job: run a plain
    // GEQ effect and this should read idle at ~8 Hz; switch to IMU Axes and it
    // should read active at ~100 Hz within a frame.
    JsonArray rt = user.createNestedArray(F("IMU poll"));
    snprintf_P(buf, sizeof(buf), PSTR("%s, %u Hz, SDA %d SCL %d"),
               hot ? "active" : "idle", obsHz, useSda, useScl);
    rt.add(buf); rt.add("");
  }

  // --- config ---------------------------------------------------------------
  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top["sda"]      = sda;
    top["scl"]      = scl;
    top["force"]    = force;
    top["addr"]     = addr;
    top["hz"]       = hz;
    top["idleHz"]   = idleHz;
    top["idleMs"]   = idleMs;
    top["accelG"]   = accelG;
    top["gyroDPS"]  = gyroDPS;
    top["lpfIdx"]   = lpfIdx;
    top["axisX"]    = axisX;
    top["axisY"]    = axisY;
    top["axisZ"]    = axisZ;
    top["fuseMs"]   = fuseMs;
    top["gateMg"]   = gateMg;
    top["deadDPS"]  = deadDPS;
    top["motionX"]  = motionX;
    top["action"]   = 0;      // always writes back idle - a button, not a state
    top["gbX"]      = gbX;
    top["gbY"]      = gbY;
    top["gbZ"]      = gbZ;
    top["refX"]     = refX;
    top["refY"]     = refY;
    top["refZ"]     = refZ;
  }

  bool readFromConfig(JsonObject &root) override {
    JsonObject top = root[FPSTR(_name)];
    bool ok = !top.isNull();

    // The level trim is stored in CUBE frame and applied after the axis map.
    // Re-mapping moves that frame, so a trim captured under the old map now
    // describes a rotation of something that no longer exists. Remember what
    // the map was so we can throw the trim away if it moved.
    const int prevAxisX = axisX, prevAxisY = axisY, prevAxisZ = axisZ;
    const int prevSda = sda, prevScl = scl;

    ok &= getJsonValue(top[FPSTR(_enabled)], enabled, true);
    ok &= getJsonValue(top["sda"],     sda,     21);
    ok &= getJsonValue(top["scl"],     scl,     13);
    ok &= getJsonValue(top["force"],   force,   false);
    ok &= getJsonValue(top["addr"],    addr,    0x68);
    ok &= getJsonValue(top["hz"],      hz,      100);
    ok &= getJsonValue(top["idleHz"],  idleHz,  8);
    ok &= getJsonValue(top["idleMs"],  idleMs,  600);
    ok &= getJsonValue(top["accelG"],  accelG,  4);
    ok &= getJsonValue(top["gyroDPS"], gyroDPS, 500);
    ok &= getJsonValue(top["lpfIdx"],  lpfIdx,  3);
    ok &= getJsonValue(top["axisX"],   axisX,   1);
    ok &= getJsonValue(top["axisY"],   axisY,   2);
    ok &= getJsonValue(top["axisZ"],   axisZ,   3);
    ok &= getJsonValue(top["fuseMs"],  fuseMs,  300);
    ok &= getJsonValue(top["gateMg"],  gateMg,  350);
    ok &= getJsonValue(top["deadDPS"], deadDPS, 1.2f);
    ok &= getJsonValue(top["motionX"], motionX, 100);
    ok &= getJsonValue(top["gbX"],     gbX,     0.0f);
    ok &= getJsonValue(top["gbY"],     gbY,     0.0f);
    ok &= getJsonValue(top["gbZ"],     gbZ,     0.0f);
    ok &= getJsonValue(top["refX"],    refX,    0.0f);
    ok &= getJsonValue(top["refY"],    refY,    0.0f);
    ok &= getJsonValue(top["refZ"],    refZ,    1.0f);

    int a = 0;
    getJsonValue(top["action"], a, 0);
    action = 0;

    if (initDone) {                 // a settings save, not the boot-time read
      if (sda != prevSda || scl != prevScl) { busOk = false; present = false; }
      checkMap();
      if (axisX != prevAxisX || axisY != prevAxisY || axisZ != prevAxisZ) {
        refX = 0; refY = 0; refZ = 1;          // stale - see note above
        statusMsg = "map changed, level trim cleared";
      }
      buildTrim();
      applyRanges();
      haveUp = false;               // ranges or mounting changed - re-acquire
      if (a) pendAction = (int8_t)a;
    }
    return ok;
  }

  // If your WLED predates the Print& settings API, change the signature to
  // `void appendConfigData()` and swap every `s.print(F(x))` for
  // `oappend(SET_F(x))`. Nothing else in this file is version-sensitive.
  //
  // NOTE ON QUOTING: every string here lands inside a single-quoted JavaScript
  // literal, and ALL usermods share one script block on the settings page. One
  // bare apostrophe is a SyntaxError that silently deletes every dropdown
  // emitted after it - including ones belonging to completely unrelated
  // usermods. jsq() escapes on the way out so it cannot happen again.
  void appendConfigData(Print &s) override {
    auto jsq = [&](const char *t) {
      for (const char *p = t; *p; ++p) {
        if (*p == '\'' || *p == '\\') s.print('\\');
        s.print(*p);
      }
    };
    auto info = [&](const char *k, const char *html) {
      s.print(F("addInfo('")); s.print(FPSTR(_name)); s.print(F(":")); s.print(k);
      s.print(F("',1,'")); jsq(html); s.print(F("');"));
    };
    auto dd = [&](const char *k) {
      s.print(F("dd=addDropdown('")); s.print(FPSTR(_name));
      s.print(F("','")); s.print(k); s.print(F("');"));
    };
    auto opt = [&](const char *label, int v) {
      s.print(F("addOption(dd,'")); jsq(label); s.print(F("',")); s.print(v); s.print(F(");"));
    };
    auto axis = [&](const char *k) {
      dd(k);
      opt("sensor +X", 1);  opt("sensor -X", -1);
      opt("sensor +Y", 2);  opt("sensor -Y", -2);
      opt("sensor +Z", 3);  opt("sensor -Z", -3);
    };

    dd("addr");    opt("0x68 (AD0 low)", 0x68); opt("0x69 (AD0 high)", 0x69);
    dd("accelG");  opt("&plusmn;2 g", 2); opt("&plusmn;4 g", 4); opt("&plusmn;8 g", 8); opt("&plusmn;16 g", 16);
    dd("gyroDPS"); opt("&plusmn;250 &deg;/s", 250); opt("&plusmn;500 &deg;/s", 500);
                   opt("&plusmn;1000 &deg;/s", 1000); opt("&plusmn;2000 &deg;/s", 2000);
    dd("lpfIdx");  opt("260 Hz (off)", 0); opt("184 Hz", 1); opt("94 Hz", 2);
                   opt("44 Hz", 3); opt("21 Hz", 4); opt("10 Hz", 5); opt("5 Hz", 6);
    axis("axisX"); axis("axisY"); axis("axisZ");
    dd("action");  opt("&mdash;", 0); opt("Calibrate gyro (hold still)", 1);
                   opt("Level now (sit it flat)", 2); opt("Clear level trim", 3);
                   opt("Re-initialise sensor", 4);

    info("sda",     "<b>21</b> on this build. &minus;1 follows LED Preferences");
    info("scl",     "<b>13</b> on this build. &minus;1 follows LED Preferences");
    info("force",   "re-pin Wire even if LED Preferences disagrees. only if you are sure");
    info("hz",      "poll rate while an effect is reading the IMU");
    info("idleHz",  "poll rate while nothing is. this is the frame-rate saving");
    info("idleMs",  "no effect has read us for this long = idle");
    info("accelG",  "&plusmn;4 g suits a cube being handled; go &plusmn;8 g if hard shakes clip");
    info("gyroDPS", "&plusmn;500 covers hand spins; raise it if fast flips saturate");
    info("lpfIdx",  "sensor low-pass. lower = smoother and laggier");
    info("axisX",   "which sensor axis points cube EAST");
    info("axisY",   "which sensor axis points cube NORTH");
    info("axisZ",   "which sensor axis points cube UP (top face)");
    info("fuseMs",  "gravity correction time constant, ms. higher = trusts the gyro longer, drifts more");
    info("gateMg",  "milli-g of |a|&minus;1g at which the accelerometer is ignored. lower = stricter");
    info("deadDPS", "gyro deadband, &deg;/s. raise until spin sits at 0 when still");
    info("motionX", "shake sensitivity, %. scales what the effects see as motion");
    info("gbX",     "<i>gyro bias, written by Calibrate</i>");
    info("refX",    "<i>level reference, written by Level now</i>");
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};

const char AceImuUsermod::_name[]    PROGMEM = "AceIMU";
const char AceImuUsermod::_enabled[] PROGMEM = "enabled";

static AceImuUsermod ace_imu_mpu6050;
REGISTER_USERMOD(ace_imu_mpu6050);

// Weak symbol that ace_ui_menu.cpp calls for System -> Calibrate gyro / Level
// now / Clear level trim. Without this file in the build those three entries
// report "no IMU driver" and the menu still compiles.
extern "C" void aceImuAction(int8_t a) { ace_imu_mpu6050.requestAction(a); }
