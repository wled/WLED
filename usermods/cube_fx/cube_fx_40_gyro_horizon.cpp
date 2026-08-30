#include "wled.h"
#include "cube_fx_common.h"
#include "cube_fx_imu.h"

// ===========================================================================
// 40. ACE GYRO HORIZON
// ===========================================================================
// The first effect in the Gyro class that could not exist without the sensor,
// and the one to run when you want to KNOW the axis map is right: a horizon
// that stays dead level while you turn the cube. Sky above, ground below, a
// bright line where they meet, and the line does not move. If it leans, tips,
// or lags behind in one axis and not the other, that is the driver's mounting
// dropdowns talking, not this effect.
//
// It is also, deliberately, a nice thing to look at. An attitude indicator
// that is only an attitude indicator gets run once and never again.
//
// ---------------------------------------------------------------------------
// WHERE THE MUSIC GOES
// ---------------------------------------------------------------------------
// Same division of labour as the rest of the family: the music owns TIME and
// AMPLITUDE, gravity owns the FRAME. Nothing here lets tilt touch a rate.
//
//   cfx_tempo().phase   the halo breathes on the predicted beat clock, so it
//        swells smoothly between hits and survives a missed one instead of
//        twitching on raw transients. Scaled by confidence, so when the bass
//        drops out the breathing fades rather than running on a stale guess.
//   cfx_tempo().hit     the actual kick sharpens and widens the line.
//   cfx_drop().build    a riser NARROWS the halo - the cube holds its breath.
//   cfx_drop().hit      the slam throws a full-strength wave.
//   imu.shake / .jolt   a shove is a kick. st.shake was shaped to drop into a
//        beat slot, so it does, rather than opening a rival trigger path.
//
// Waves leave the horizon in BOTH directions and expand away from it, which
// costs nothing extra - they ride the same height value every pixel already
// has. Four of them can be in flight at once.
//
// ---------------------------------------------------------------------------
// TWO THINGS THAT ARE NOT OBVIOUS
// ---------------------------------------------------------------------------
// THE LINE HAS TO BE PITCH-RELATIVE, NOT AN ABSOLUTE WIDTH. On a 16-pixel
// face the row nearest the horizon sits about 7 units off it in the +/-127
// scale, because pixel centres land on odd sixteenths and none of them is at
// zero. A fixed half-width of 5 therefore draws NOTHING at perfectly level -
// the one orientation you actually calibrate against - and starts working
// again as soon as you tip it, which is a maddening bug to chase. Width is
// measured in pixel pitches here, so it is one to three rows on any face size
// and the 8x8 mini cube gets a proportionally thicker line for free.
//
// EVERYTHING SCALES BY THE SUPPORT WIDTH. A cube is 128 deep through a face
// and 219 deep through a corner. Without that factor the line thins out and
// the gradient stretches as you roll it onto a corner. One multiply a frame.
//
// ---------------------------------------------------------------------------
// WITH NO SENSOR this settles to a static ring around the cube's own equator,
// still lit, still beat-reactive - the same graceful degradation the rest of
// the family has. Build -D CFX_IMU_SIM=1 to bench it on a plain panel.
//
// The sun sits in a fixed direction in the ROOM, taken from the cube->world
// basis. On a 6-axis IMU that reference is integrated yaw with no compass to
// correct it, so over several minutes the sun will wander a few degrees. That
// is not a bug in here, it is what CFX_IMU_YAW_TRACK costs, and watching how
// fast it drifts is the easiest way to measure it.
// ===========================================================================

#define HZ_UNIT   128       // matches cfx_imuUp()'s >>7, exactly

// Lift a palette colour toward white by amt/255. Local on purpose: one effect
// uses it, so per the README it stays in this file rather than growing the
// shared header. Self-contained rather than color_blend()/FastLED so it can't
// break on a version bump.
static inline uint32_t hz_white(uint32_t c, uint8_t amt) {
  const int r = (int)((c >> 16) & 0xFF), g = (int)((c >> 8) & 0xFF), b = (int)(c & 0xFF);
  return RGBW32((uint8_t)(r + (((255 - r) * (int)amt) >> 8)),
                (uint8_t)(g + (((255 - g) * (int)amt) >> 8)),
                (uint8_t)(b + (((255 - b) * (int)amt) >> 8)), 0);
}

static FX_RET mode_gyro_horizon() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(16 + 3 * n + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int16_t *ph = (int16_t *)SEGENV.data;      // ph[0..3] wave distance, Q4
  int8_t  *cx = (int8_t *)(ph + 8);
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *st = (uint8_t *)(cz + n);         // st[0] built  [1] flare
                                             // st[2..3] clock  [4..7] wave strength
  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  const uint8_t want = (uint8_t)(cube ? 1 : 2);
  if (SEGENV.call == 0 || st[0] != want) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    for (int k = 0; k < 8; k++) ph[k] = 0;
    for (int k = 0; k < 16; k++) st[k] = 0;
    st[0] = want;
  }

  // --- which way is up, and which way is the sun ---------------------------
  const CfxImuState &imu = cfx_imu();
  int16_t U[3], E[3];                        // world up / sunward, in cube coords

  if (imu.valid && cube) {
    U[0] = (int16_t)(((int)imu.ux * HZ_UNIT) / 127);
    U[1] = (int16_t)(((int)imu.uy * HZ_UNIT) / 127);
    U[2] = (int16_t)(((int)imu.uz * HZ_UNIT) / 127);
    for (int k = 0; k < 3; k++) E[k] = (int16_t)(((int)imu.basis[k] * HZ_UNIT) / 127);
  } else if (imu.valid) {
    // Flat panel with a sensor behind it - no cube geometry to hang gravity
    // on, so this assumes the panel stands upright with the driver's cube +X
    // to screen right and +Z to screen up. Face-up on a bench there is no
    // in-plane gravity at all, so fall back to screen-down.
    int ax = ((int)imu.ux * HZ_UNIT) / 127;
    int az = ((int)imu.uz * HZ_UNIT) / 127;
    if (ax * ax + az * az < 400) { ax = 0; az = HZ_UNIT; }
    U[0] = (int16_t)ax; U[1] = (int16_t)az; U[2] = 0;
    E[0] = HZ_UNIT; E[1] = 0; E[2] = 0;
  } else {
    U[0] = 0;
    U[1] = (int16_t)(cube ? 0 : HZ_UNIT);
    U[2] = (int16_t)(cube ? HZ_UNIT : 0);
    E[0] = HZ_UNIT; E[1] = 0; E[2] = 0;
  }

  int wSup = ((U[0] < 0) ? -U[0] : U[0]) + ((U[1] < 0) ? -U[1] : U[1])
           + ((U[2] < 0) ? -U[2] : U[2]);
  if (wSup < 1) wSup = 1;
  const int eSup = ((E[0] < 0) ? -E[0] : E[0]) + ((E[1] < 0) ? -E[1] : E[1])
                 + ((E[2] < 0) ? -E[2] : E[2]);

  // --- audio ---------------------------------------------------------------
  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);
  const CfxTempoState &tempo = cfx_tempo(um);
  const CfxDropState  &drop  = cfx_drop(um, tempo);

  int dt = (int)fx_dt8(st + 2);
  if (dt > 60) dt = 60;

  // A shove counts as a kick, and so does a drop - one trigger, three sources.
  uint8_t knock = tempo.hit;
  if (imu.shake > knock) knock = imu.shake;
  if (drop.hit  > knock) knock = drop.hit;

  if (knock > st[1]) st[1] = knock;
  if (imu.jolt > st[1]) st[1] = imu.jolt;
  { const int f = (int)st[1] - (int)fx_step(11, (uint16_t)dt);
    st[1] = (uint8_t)((f < 0) ? 0 : f); }

  // One pixel step along the horizon axis, corrected for how deep the cube is
  // in this direction. Everything below is measured in these.
  const int stepPx = cube ? B : ((rows > 1) ? rows : 2);
  int pitch = ((254 / stepPx) * wSup) / HZ_UNIT;
  if (pitch < 2) pitch = 2;

  const int band  = pitch + (pitch * (int)SEGMENT.custom1) / 128;   // 1..3 rows
  const int ringW = band + (pitch >> 1);

  // The halo breathes on the predicted beat, snaps wider on a real kick, and
  // gets squeezed while a riser is building.
  const int swell = ((int)sin8_t((uint8_t)(tempo.phase + 64)) * (int)tempo.confidence) >> 9;
  int halo = band + pitch * 3
           + (pitch * swell) / 255
           + (pitch * (int)st[1] * 2) / 255
           - (pitch * (int)drop.build) / 400;
  // Nothing playing and nothing moving: breathe on a slow clock of its own so
  // a cube sitting on a shelf still looks alive instead of frozen.
  const int idleAmt = (tempo.confidence < 40) ? (40 - (int)tempo.confidence) : 0;
  if (idleAmt) halo += (pitch * (int)sin8_t((uint8_t)(strip.now >> 6)) * idleAmt) / 10200;
  if (halo < band + 1) halo = band + 1;

  // --- waves leaving the horizon -------------------------------------------
  const int rspd = 4 + ((int)SEGMENT.speed >> 2);        // Q4 units per 23 ms
  for (int r = 0; r < 4; r++) {
    if (!st[4 + r]) continue;
    ph[r] = (int16_t)((int)ph[r] + (int)fx_step(rspd, (uint16_t)dt));
    const int d = (int)st[4 + r] - (int)fx_step(6, (uint16_t)dt);
    st[4 + r] = (uint8_t)((d < 0) ? 0 : d);
    if (((int)ph[r] >> 4) > wSup + halo) st[4 + r] = 0;
  }
  if (knock && SEGMENT.check2) {
    int slot = 0;
    for (int r = 1; r < 4; r++) if (st[4 + r] < st[4 + slot]) slot = r;
    ph[slot] = 0;
    st[4 + slot] = knock;
  }

  // --- level, gradient, sun ------------------------------------------------
  const int lev = SEGMENT.check1 ? ((bass * pitch * 4) / 255) : 0;   // bass raises it
  const int spread = 40 + ((int)SEGMENT.custom2 >> 1);
  const int mul = (spread << 8) / wSup;
  const int sr  = (SEGMENT.custom3 == 0) ? 0
                : (pitch + ((int)SEGMENT.custom3 * pitch * 5) / 255);

  const int sky0 = 34 + ((int)drop.intensity >> 3);
  const int gnd0 = 18;
  const uint8_t drive = cfx_drive(vol, 1.0f, 120 + (SEGMENT.intensity >> 1));

  SEGMENT.fill(SEGCOLOR(0));
  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int px = cx[i], py = cy[i], pz = cz[i];

      const int h  = ((int)U[0] * px + (int)U[1] * py + (int)U[2] * pz) >> 7;
      const int e  = ((int)E[0] * px + (int)E[1] * py + (int)E[2] * pz) >> 7;
      const int hh = h - lev;
      const int a  = (hh < 0) ? -hh : hh;

      int t = 128 + ((hh * mul) >> 8);
      if (t < 0) t = 0; else if (t > 255) t = 255;
      uint32_t c = SEGMENT.color_from_palette((uint8_t)t, false, false, 0);

      int lum = (hh >= 0) ? sky0 : gnd0;
      int wht = 0;

      if (a < halo) {                                    // the glow either side
        const int g = ((halo - a) * 170) / halo;
        lum += g;
        wht += g >> 1;
      }
      if (a <= band) { lum = 255; wht = 255; }           // the line itself

      for (int r = 0; r < 4; r++) {                      // waves in flight
        const int s = (int)st[4 + r];
        if (!s) continue;
        int d = a - ((int)ph[r] >> 4);
        if (d < 0) d = -d;
        if (d < ringW) {
          const int g = ((ringW - d) * s) / ringW;
          lum += g;
          wht += g >> 2;
        }
      }

      if (sr > 0) {                                      // sun on the horizon
        const int de = eSup - e;
        if (de < sr && a < sr) {
          const int g = (((sr - de) * (sr - a)) / sr) * 200 / sr;
          lum += g;
          wht += (g * 3) >> 2;
        }
      }

      if (lum <= 0) continue;
      if (lum > 255) lum = 255;
      if (wht > 255) wht = 255;
      if (wht) c = hz_white(c, (uint8_t)wht);
      SEGMENT.setPixelColorXY(x, y, mq_scale(c, scale8((uint8_t)lum, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_GYRO_HORIZON[] PROGMEM =
  "Ace Gyro Horizon@Wave speed,Glow,Line width,Spread,Sun,Bass lifts,Beat waves,Flat mode;;!;2f;sx=140,ix=128,c1=48,c2=140,c3=90,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_GyroHorizonUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_gyro_horizon, _data_FX_MODE_GYRO_HORIZON);
  }
  void loop() override {}
};

static CubeFx_GyroHorizonUsermod cube_fx_gyro_horizon;
REGISTER_USERMOD(cube_fx_gyro_horizon);
