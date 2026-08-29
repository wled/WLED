#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 26. ACE 3-D SUNLIGHT
// ===========================================================================
// A light source orbiting the object, with the terminator - the day/night
// boundary - travelling across it. Nothing makes a shape read as SOLID faster
// than watching light move over it.
//
// The catch: a cube's true normal is constant across each face, so honest
// Lambertian shading gives five flat values and the terminator jumps from face
// to face instead of sweeping. So Roundness crossfades the normal between the
// face's own and the direction from the centre - the sphere normal. At 0 it is
// a faceted solid with hard steps between panels; at full it is lit exactly as
// a ball would be, and the terminator glides continuously over the folds. In
// between is a rounded-off cube, which is the most convincing of the three.
//
// On a flat panel the normals are built as a hemisphere instead, so you get a
// properly shaded sphere rather than a lit rectangle.
// ---------------------------------------------------------------------------
static FX_RET mode_sunlight() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int8_t  *sx = (int8_t *)SEGENV.data;      // unit surface normal, x127
  int8_t  *sy = sx + n;
  int8_t  *sz = sy + n;
  uint8_t *st = (uint8_t *)(sz + n);        // [0] built [1] flare [2..3] clock

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    for (int y = 0; y < rows; y++)
      for (int x = 0; x < cols; x++) {
        const size_t i = (size_t)y * cols + x;
        float X, Y, Z;
        cfx_pos(x, y, cols, rows, B, cube, X, Y, Z);
        if (!cube) {                         // flat: raise a hemisphere off the panel
          const float r2 = X * X + Y * Y;
          if (r2 > 1.0f) { sx[i] = 0; sy[i] = 0; sz[i] = 0; continue; }
          Z = sqrtf(1.0f - r2);
        }
        const float L = sqrtf(X * X + Y * Y + Z * Z);
        const float k = (L > 0.001f) ? (127.0f / L) : 0.0f;
        sx[i] = (int8_t)(X * k); sy[i] = (int8_t)(Y * k); sz[i] = (int8_t)(Z * k);
      }
    for (int k = 0; k < 16; k++) st[k] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2);
    SEGENV.aux0 = 0;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);
  const uint16_t dtS = fx_dt8(st + 2);

  if (peak && SEGMENT.check1 && peak > st[1]) st[1] = peak;
  else { const int f = (int)st[1] - (int)fx_step(11, dtS); st[1] = (uint8_t)((f < 0) ? 0 : f); }
  const int flare = (int)st[1];

  // the sun's orbit: azimuth always turning, elevation nodding if tilted
  SEGENV.aux0 += (uint16_t)fx_step(3 + (SEGMENT.speed >> 3), dtS);
  const uint8_t az = SEGENV.aux0 >> 8;
  const uint8_t el = SEGMENT.check2 ? (uint8_t)(SEGENV.aux0 >> 10) : 0;
  const int ce = (int)cos8_t(el) - 128, se = (int)sin8_t(el) - 128;
  const int lx = ((int)cos8_t(az) - 128) * ce / 127;
  const int ly = ((int)sin8_t(az) - 128) * ce / 127;
  const int lz = se / 2;                     // keep the sun off the poles

  const int round = SEGMENT.custom1;
  const int hard  = SEGMENT.custom2;
  const int amb   = (int)SEGMENT.custom3 * 3;              // night side floor
  const int gw    = 9 + (bass >> 5) + (flare >> 4);        // terminator glow width
  const int soft  = 8 + ((255 - hard) * 110) / 255;        // half-width of the fade
  const uint8_t drive = cfx_drive(vol, 1.0f, 150 + (SEGMENT.intensity >> 1));
  const uint8_t hue   = (uint8_t)(strip.now >> 9);

  SEGMENT.fill(SEGCOLOR(0));
  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int px = sx[i], py = sy[i], pz = sz[i];
      if (!px && !py && !pz) continue;                     // outside the flat ball

      // face normal: the axis the sphere normal leans on hardest
      const int ax = (px < 0) ? -px : px;
      const int ay = (py < 0) ? -py : py;
      const int az2 = (pz < 0) ? -pz : pz;
      int fx2 = 0, fy2 = 0, fz2 = 0;
      if      (ax >= ay && ax >= az2) fx2 = (px > 0) ? 127 : -127;
      else if (ay >= az2)             fy2 = (py > 0) ? 127 : -127;
      else                            fz2 = (pz > 0) ? 127 : -127;

      const int nx = (fx2 * (255 - round) + px * round) / 255;
      const int ny = (fy2 * (255 - round) + py * round) / 255;
      const int nz = (fz2 * (255 - round) + pz * round) / 255;

      const int d = (nx * lx + ny * ly + nz * lz) / 127;   // -127..127
      int lit = ((d + soft) * 255) / (2 * soft);
      if (lit < 0) lit = 0; else if (lit > 255) lit = 255;

      int lum = amb + (lit * (255 - amb)) / 255;
      const int ad = (d < 0) ? -d : d;
      uint32_t c = SEGMENT.color_from_palette((uint8_t)(30 + (lit >> 1) + hue), false, false, 0);
      if (ad < gw) {                                       // sunrise band on the edge
        const int g = ((gw - ad) * (200 + flare / 2)) / gw;
        c = RGBW32(qadd8((uint8_t)(c >> 16), (uint8_t)g),
                   qadd8((uint8_t)(c >> 8),  (uint8_t)(g * 3 / 4)),
                   qadd8((uint8_t)c,         (uint8_t)(g / 3)), 0);
        lum = (lum + 255) / 2;
      }
      if (lum > 255) lum = 255;
      SEGMENT.setPixelColorXY(x, y, mq_scale(c, scale8((uint8_t)lum, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_SUNLIGHT[] PROGMEM =
  "Ace 3-D Sunlight@Orbit speed,Brightness,Roundness,Terminator,Night glow,Flare on beat,Tilted orbit,Flat mode;;!;2f;sx=110,ix=150,c1=170,c2=190,c3=6,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_SunlightUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_sunlight, _data_FX_MODE_SUNLIGHT);
  }
  void loop() override {}
};

static CubeFx_SunlightUsermod cube_fx_sunlight;
REGISTER_USERMOD(cube_fx_sunlight);
