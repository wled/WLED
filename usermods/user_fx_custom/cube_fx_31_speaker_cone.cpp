#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 29. CUBE SPEAKER
// ===========================================================================
// Every face becomes its own paper speaker cone: a bright dust-cap breathes
// in and out with the bass envelope, faint ridges creep across the cone's
// surface, and every beat fires a ring that races out from each face's
// centre toward its four corners and fades just past them - a little
// sound-wave leaving each "driver" in sync.
//
// Radius is measured in each FACE's own local plane, not 3D world distance,
// so every one of the five faces gets an identical, independent cone
// regardless of its size or position in the net - no per-face bookkeeping
// needed, one shared set of rings drives all five at once.
// ---------------------------------------------------------------------------
#define SPK_SRC   4     // concurrent beat rings, shared by every face at once
#define SPK_MAXR  190   // local units - just past a face corner (~180); trails die here

static FX_RET mode_cube_speaker() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(3 * n + SPK_SRC * 4 + 8)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *fid = SEGENV.data;              // 255 = gap / not on any face
  int8_t  *la  = (int8_t *)(fid + n);      // local face-plane coords, -127..127 (same scale as cx/cy/cz)
  int8_t  *lb  = la + n;
  uint8_t *pl  = (uint8_t *)(lb + n);      // SPK_SRC rings: [0]hue [1]born_lo [2]born_hi [3]alive
  uint8_t *gs  = pl + SPK_SRC * 4;         // [0]buildFlag [1]env [2]prevPeak [3]rr [4..5]fadeDt [6..7]spawnDt

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || gs[0] != (uint8_t)(cube ? 1 : 2)) {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        const size_t i = (size_t)y * cols + x;
        if (cube) {
          const int bx = x / B, by = y / B;
          int f;
          if      (bx == 1 && by == 1) f = 0;   // TOP
          else if (bx == 1 && by == 0) f = 1;   // NORTH
          else if (bx == 1 && by == 2) f = 2;   // SOUTH
          else if (bx == 0 && by == 1) f = 3;   // WEST
          else if (bx == 2 && by == 1) f = 4;   // EAST
          else { fid[i] = 255; la[i] = 0; lb[i] = 0; continue; }   // gap corner
          const float a = 2.0f * ((x % B) + 0.5f) / (float)B - 1.0f;
          const float b = 2.0f * ((y % B) + 0.5f) / (float)B - 1.0f;
          fid[i] = (uint8_t)f;
          la[i]  = cfx_clamp8(a);
          lb[i]  = cfx_clamp8(b);
        } else {
          // flat panel: one big cone dead centre of the whole matrix
          fid[i] = 0;
          la[i]  = cfx_clamp8(2.0f * (x + 0.5f) / (float)cols - 1.0f);
          lb[i]  = cfx_clamp8(2.0f * (y + 0.5f) / (float)rows - 1.0f);
        }
      }
    }
    for (int k = 0; k < SPK_SRC * 4; k++) pl[k] = 0;
    gs[0] = (uint8_t)(cube ? 1 : 2);
    gs[1] = 0; gs[2] = 0; gs[3] = 0; gs[4] = 0; gs[5] = 0; gs[6] = 0; gs[7] = 0;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  const uint16_t nowT = (uint16_t)strip.now;

  // --- bass envelope: fast attack, slow release - the cone's push amount ---
  int env = gs[1];
  if (bass > env) env = bass;
  else            env += (bass - env) / 6;
  gs[1] = (uint8_t)env;

  // --- fire a ring on the beat (or free-running if "Spawn on beat" is off) -
  const bool rising = peak && !gs[2];
  gs[2] = peak ? 1 : 0;
  bool spawn = rising && SEGMENT.check1;
  if (!SEGMENT.check1) {
    const uint16_t last = (uint16_t)gs[6] | ((uint16_t)gs[7] << 8);
    if ((uint16_t)(nowT - last) > (uint16_t)(700 - (SEGMENT.speed << 1))) spawn = true;
  }
  if (spawn) {
    int slot = -1;
    for (int k = 0; k < SPK_SRC; k++) if (!pl[k * 4 + 3]) { slot = k; break; }
    if (slot < 0) { slot = gs[3] % SPK_SRC; gs[3] = (uint8_t)(gs[3] + 1); }
    uint8_t *e = pl + slot * 4;
    uint8_t b1 = 0;
    for (uint8_t k = 1; k < 16; k++) if (fft[k] > fft[b1]) b1 = k;
    e[0] = (uint8_t)(b1 << 4);
    e[1] = (uint8_t)(nowT & 0xFF); e[2] = (uint8_t)(nowT >> 8);
    e[3] = 1;
    gs[6] = (uint8_t)(nowT & 0xFF); gs[7] = (uint8_t)(nowT >> 8);
  }

  SEGMENT.fadeToBlackBy(fx_fade(40 + (255 - SEGMENT.custom2) / 3, fx_dt8(gs + 4)));

  const uint8_t drive     = cfx_drive(vol, 2.0f, 60);
  const int     growth    = 3 + (SEGMENT.speed >> 3);                 // ring speed, local units/23ms
  const int     width     = 10 + (SEGMENT.intensity >> 2);            // ring thickness
  const int     pushRange = 8 + (SEGMENT.custom1 >> 2);                // how far the cone can travel
  const int     coneR     = 10 + (env * pushRange) / 255;              // cone's current push position
  const int     coneW     = 20 + (SEGMENT.intensity >> 3);             // cone body thickness
  const uint8_t hueSpr    = SEGMENT.custom3;                           // per-face colour identity

  // resolve live rings once per frame
  int     rad[SPK_SRC], life[SPK_SRC];
  uint8_t hue[SPK_SRC];
  bool    live[SPK_SRC];
  for (int k = 0; k < SPK_SRC; k++) {
    uint8_t *e = pl + k * 4;
    live[k] = false;
    if (!e[3]) continue;
    const uint16_t born = (uint16_t)e[1] | ((uint16_t)e[2] << 8);
    const int r = (int)(((uint32_t)(uint16_t)(nowT - born) * growth * 2) / 23);
    if (r > SPK_MAXR) { e[3] = 0; continue; }
    rad[k]  = r;
    life[k] = 255 - (r * 255) / SPK_MAXR;
    hue[k]  = e[0];
    live[k] = true;
  }

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      if (fid[i] == 255) continue;
      const int   a = la[i], b = lb[i];
      const float r = sqrtf((float)(a * a + b * b));

      // --- cone body: bright dust-cap breathing with the bass, faint ridges ---
      int coneOff = (int)r - coneR; if (coneOff < 0) coneOff = -coneOff;
      uint8_t bestLum = 0, bestHue = 0;
      if (coneOff < coneW) {
        int l = 255 - (coneOff * 255) / coneW;
        if (SEGMENT.check2) {                              // ridge texture across the cone
          const uint8_t ridge = sin8_t((uint8_t)(r * 3 - (env << 1)));
          l = (l * (180 + (ridge >> 2))) >> 8;
        }
        bestLum = (uint8_t)((l > 255) ? 255 : l);
        bestHue = (uint8_t)(16 + fid[i] * hueSpr);          // gentle per-face colour identity
      }

      // --- beat rings racing out to the corners --------------------------
      for (int k = 0; k < SPK_SRC; k++) {
        if (!live[k]) continue;
        int off = (int)r - rad[k]; if (off < 0) off = -off;
        if (off >= width) continue;
        int l = 255 - (off * 255) / width;
        l = scale8((uint8_t)l, (uint8_t)life[k]);
        if ((uint8_t)l > bestLum) { bestLum = (uint8_t)l; bestHue = hue[k]; }
      }

      if (!bestLum) continue;
      SEGMENT.addPixelColorXY(x, y,
        SEGMENT.color_from_palette(bestHue, false, false, 0, scale8(bestLum, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CUBE_SPEAKER[] PROGMEM =
  "Ace 3-D Cube Speaker@Speed,Thickness,Punch,Persistence,Face hues,Spawn on beat,Ring texture,Flat mode;;!;2f;sx=100,ix=110,c1=140,c2=140,c3=40,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeSpeakerUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_speaker, _data_FX_MODE_CUBE_SPEAKER);
  }
  void loop() override {}
};

static CubeFx_CubeSpeakerUsermod cube_fx_cube_speaker;
REGISTER_USERMOD(cube_fx_cube_speaker);
