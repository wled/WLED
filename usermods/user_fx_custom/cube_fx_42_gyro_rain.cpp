#include "wled.h"
#include "cube_fx_common.h"
#include "cube_fx_imu.h"

// ===========================================================================
// 42. ACE GYRO RAIN
// ===========================================================================
// Rain falling through the ROOM, with the cube held up in the middle of it.
// Turn the cube and the streaks stay vertical; the surface just slides across
// a weather system that does not care what you do with it. That is the whole
// trick, and it is the cheapest of the three to run.
//
// ---------------------------------------------------------------------------
// NO PARTICLES
// ---------------------------------------------------------------------------
// The obvious build is a list of droplets with world positions, which needs
// state, spawning, culling, and an inverse map from world space back onto the
// cube surface. None of that is here. Instead the rain is a FIELD, evaluated
// independently at every pixel:
//
//   1. push the pixel through the cube->world basis (nine multiplies)
//   2. quantise the two horizontal coordinates into a column
//   3. hash the column to get a per-column phase offset and an on/off bit
//   4. the drop's head height is that phase on a shared falling clock
//   5. brightness is the pixel's distance below... above that head
//
// O(1) per pixel, no allocation beyond the geometry cache, and the rain field
// is infinite - there is no edge to fall off and nothing to respawn. Columns
// live in WORLD coordinates, so rotating the cube slides it through a fixed
// downpour rather than carrying the rain around with it.
//
// The best part is emergent. On a wall, world height varies down the face, so
// the drop paints a moving streak. On a face pointing at the sky, world
// height is very nearly CONSTANT across the whole face - it is a level set -
// so the same expression makes the cell flash all at once as the drop passes
// through it. Streaks on the walls and splashes on the roof, from one line of
// arithmetic and no special case.
//
// The splash afterglow does need one: skyward pixels stay lit briefly after
// the head has gone past, scaled by how skyward that face actually is. The
// face normal comes from a one-byte-per-pixel lookup built with the geometry
// cache, so the fade is smooth and continuous rather than snapping between
// faces at 45 degrees.
//
// ---------------------------------------------------------------------------
// MUSIC AND MOTION
// ---------------------------------------------------------------------------
//   drop.speedScale  drives the fall clock through cfx_dropDt(), so the whole
//        sky slows and surges with the track instead of ticking on regardless
//   drop.build       the downpour thickens through a riser
//   tempo.phase      with "Beat drops" on, one column in eight abandons the
//        free-running clock and falls on the beat instead, so part of the
//        rain lands in time and the rest stays weather. Confidence-gated, so
//        when the tempo lock goes it dissolves back into plain rain.
//   tempo.hit        a gust: everything brightens and briefly thickens
//   imu.shake        counts as a gust, same slot
//   imu.spin         shears the columns with height - spin the cube and the
//        rain slants, then straightens as it settles
//
// WITH NO SENSOR the basis is the identity, the rain falls down the cube's
// own Z, and everything else behaves normally.
// ===========================================================================

#define RN_SHIFT  4                   // column size = 16 world units
#define RN_HI     300                 // fall runs from +RN_HI to -RN_HI

// Cheap spatial hash. The casts to unsigned MUST happen before the multiply -
// signed overflow here is undefined behaviour and the optimiser is entitled
// to do anything it likes with it.
static inline uint8_t rn_hash(int a, int b) {
  uint32_t h = (uint32_t)a * 374761393u + (uint32_t)b * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return (uint8_t)(h >> 24);
}

static FX_RET mode_gyro_rain() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(16 + 4 * n + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int16_t *ph = (int16_t *)SEGENV.data;      // ph[0] fall accumulator
  int8_t  *cx = (int8_t *)(ph + 8);          // ph[1] wind
  int8_t  *cy = cx + n;
  int8_t  *cz = cy + n;
  uint8_t *fd = (uint8_t *)(cz + n);         // which way each pixel faces, 0..6
  uint8_t *st = fd + n;                      // st[0] built [1] gust [2..3] clock

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  const uint8_t want = (uint8_t)(cube ? 1 : 2);
  if (SEGENV.call == 0 || st[0] != want) {
    cfx_buildCube(cx, cy, cz, nullptr, nullptr, cols, rows, cube);
    // Face id from the geometry: on any face exactly one coordinate is at the
    // full +/-127 and the others fall short, so the largest magnitude names
    // the face. 6 means "no meaningful normal" and never gets a sheen.
    for (size_t j = 0; j < n; j++) {
      if (!cube) { fd[j] = 6; continue; }
      const int ax = (cx[j] < 0) ? -cx[j] : cx[j];
      const int ay = (cy[j] < 0) ? -cy[j] : cy[j];
      const int az = (cz[j] < 0) ? -cz[j] : cz[j];
      if (ax >= ay && ax >= az)      fd[j] = (uint8_t)((cx[j] > 0) ? 0 : 1);
      else if (ay >= az)             fd[j] = (uint8_t)((cy[j] > 0) ? 2 : 3);
      else                           fd[j] = (uint8_t)((cz[j] > 0) ? 4 : 5);
    }
    for (int k = 0; k < 8; k++) ph[k] = 0;
    for (int k = 0; k < 16; k++) st[k] = 0;
    st[0] = want;
  }

  int dt = (int)fx_dt8(st + 2);
  if (dt > 60) dt = 60;

  const CfxImuState &imu = cfx_imu();
  int16_t M[9];
  if (imu.valid) {
    for (int k = 0; k < 9; k++) M[k] = imu.basis[k];
  } else {
    M[0] = 127; M[1] = 0;   M[2] = 0;
    M[3] = 0;   M[4] = 127; M[5] = 0;
    M[6] = 0;   M[7] = 0;   M[8] = 127;
  }
  // How skyward each face is, once per frame instead of once per pixel.
  int faceUp[7];
  faceUp[0] =  (int)imu.ux; faceUp[1] = -(int)imu.ux;
  faceUp[2] =  (int)imu.uy; faceUp[3] = -(int)imu.uy;
  faceUp[4] =  (int)imu.uz; faceUp[5] = -(int)imu.uz;
  faceUp[6] = 0;
  if (!imu.valid) { faceUp[4] = 127; faceUp[5] = -127;
                    faceUp[0] = faceUp[1] = faceUp[2] = faceUp[3] = 0; }
  for (int k = 0; k < 7; k++) if (faceUp[k] < 0) faceUp[k] = 0;

  // --- audio ---------------------------------------------------------------
  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);
  const CfxTempoState &tempo = cfx_tempo(um);
  const CfxDropState  &drop  = cfx_drop(um, tempo);

  uint8_t knock = tempo.hit;
  if (imu.shake > knock) knock = imu.shake;
  if (drop.hit  > knock) knock = drop.hit;
  if (SEGMENT.check2 && knock > st[1]) st[1] = knock;
  { const int f = (int)st[1] - (int)fx_step(10, (uint16_t)dt);
    st[1] = (uint8_t)((f < 0) ? 0 : f); }
  const int gust = st[1];

  // --- the falling clock ---------------------------------------------------
  const uint16_t pdt = cfx_dropDt((uint16_t)dt, drop.speedScale);
  const int spd = 40 + ((int)SEGMENT.speed >> 1);
  {
    uint16_t acc = (uint16_t)ph[0];
    acc = (uint16_t)(acc + (uint16_t)fx_step(spd, pdt));
    ph[0] = (int16_t)acc;
  }
  const uint8_t fallT = (uint8_t)(((uint16_t)ph[0]) >> 4);

  // wind: spin shears the columns, then decays back to vertical
  {
    int w = ph[1] + ((int)imu.spin * (int)dt) / 60;
    w -= (w * (int)dt) / 220;
    if (w > 900) w = 900; else if (w < -900) w = -900;
    ph[1] = (int16_t)w;
  }
  const int wind = ph[1] / 8;

  // --- how much rain -------------------------------------------------------
  int dens = 30 + ((int)SEGMENT.custom1 * 170) / 255 + (gust >> 3);
  if (SEGMENT.check1) dens += (bass * 50) / 255 + ((int)drop.build >> 3);
  if (dens > 245) dens = 245;

  const int len    = 24 + (int)SEGMENT.custom2;          // streak length
  const int splash = 6 + ((int)SEGMENT.custom3 >> 2);    // afterglow on the roof
  const bool onBeat = SEGMENT.check2 && tempo.confidence > 70;
  const uint8_t drive = cfx_drive(vol, 1.0f, 110 + (SEGMENT.intensity >> 1));

  SEGMENT.fill(SEGCOLOR(0));
  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int px = cx[i], py = cy[i], pz = cz[i];

      const int wx = ((int)M[0] * px + (int)M[1] * py + (int)M[2] * pz) >> 7;
      const int wy = ((int)M[3] * px + (int)M[4] * py + (int)M[5] * pz) >> 7;
      const int wz = ((int)M[6] * px + (int)M[7] * py + (int)M[8] * pz) >> 7;

      // shear the sample with height so gusts slant the rain
      const int sx = wx + ((wind * wz) >> 7);
      const uint8_t hsh = rn_hash(sx >> RN_SHIFT, wy >> RN_SHIFT);
      if (hsh >= dens) continue;                         // no column here

      // this column's drop head, on the free clock or the beat clock
      uint8_t cph = (uint8_t)(fallT + hsh * 3);
      if (onBeat && (hsh & 7) == 0) cph = (uint8_t)(tempo.phase + (hsh >> 3));
      const int dropZ = RN_HI - (((int)cph * (2 * RN_HI)) >> 8);

      const int up = faceUp[fd[i]];
      const int d  = wz - dropZ;                         // >0 = trail above it

      int bright = 0;
      if (d >= 0) {
        if (d < len) bright = 255 - (d * 255) / len;     // the streak
      } else if (up > 20) {
        const int a = -d;                                // splash afterglow,
        if (a < splash) bright = ((splash - a) * 255) / splash;   // roof only
        bright = (bright * up) >> 7;
      }
      if (bright <= 0) continue;

      bright += (bright * gust) >> 9;                    // gusts brighten it
      if (bright > 255) bright = 255;

      // wet sheen on whatever is facing the sky
      int lum = bright;
      lum += (up * (int)SEGMENT.custom3) >> 11;
      if (lum > 255) lum = 255;

      const int pi = 40 + ((bright * 180) >> 8);
      uint32_t c = SEGMENT.color_from_palette((uint8_t)pi, false, false, 0);
      if (bright > 215) {                                // the head runs white
        const int w = (bright - 215) * 6;
        const int r = (int)((c >> 16) & 0xFF), g = (int)((c >> 8) & 0xFF),
                  b = (int)(c & 0xFF);
        c = RGBW32((uint8_t)(r + (((255 - r) * w) >> 8)),
                   (uint8_t)(g + (((255 - g) * w) >> 8)),
                   (uint8_t)(b + (((255 - b) * w) >> 8)), 0);
      }
      SEGMENT.setPixelColorXY(x, y, mq_scale(c, scale8((uint8_t)lum, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_GYRO_RAIN[] PROGMEM =
  "Ace Gyro Rain@Fall speed,Glow,Density,Streak,Splash,Bass downpour,Beat drops,Flat mode;;!;2f;sx=140,ix=128,c1=120,c2=90,c3=110,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_GyroRainUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_gyro_rain, _data_FX_MODE_GYRO_RAIN);
  }
  void loop() override {}
};

static CubeFx_GyroRainUsermod cube_fx_gyro_rain;
REGISTER_USERMOD(cube_fx_gyro_rain);
