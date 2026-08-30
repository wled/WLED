#include "wled.h"
#include <math.h>

// ---------------------------------------------------------------------------
// Effect signature compatibility
// ---------------------------------------------------------------------------
// WLED 16.0 and up, 17.0.0-dev included:  void     mode_x() { ... }
// WLED 0.15 and older:                    uint16_t mode_x() { ... return FRAMETIME; }
//
// Verified against v16.0.1: WS2812FX::mode_ptr is void (*)(). The uint16_t
// form only applies to 0.15 and older, whatever the older docs say. Set
//   -D FX_LEGACY_RETURN=1
// in your platformio_override.ini build_flags if you ever build that far back.
#ifndef FX_LEGACY_RETURN
  #define FX_LEGACY_RETURN 0
#endif

#if FX_LEGACY_RETURN
  #define FX_RET  uint16_t
  #define FX_DONE return FRAMETIME
#else
  #define FX_RET  void
  #define FX_DONE return
#endif

/*
 * ===========================================================================
 * Custom audio-reactive effects for WLED (0.15 / 16.x)
 * ===========================================================================
 *
 *   Chladni Plate       standing-wave nodal lines, modes from the two loudest bins
 *   Beat Mandala        radially folded spectrum, golden-angle jump on beat
 *   Spectral RD         Gray-Scott reaction-diffusion, bass=feed, treble=kill
 *   Harmonic Lissajous  locks on consonant intervals, wanders on dissonance
 *   Kaleidoscope        true mirror-fold of a live noise chamber
 *   Hex Quilt           p6m wallpaper symmetry, no centre, tiles the panel
 *   Moire Rosette       interference between two counter-rotating gratings
 *   Spectral Wormhole   log-polar spiral, spectrum falling inward forever
 *   Feedback Echo       iterated affine resample of its own previous frame
 *   Glass Kaleidoscope  discrete chips - circles, squares, triangles, stars
 *
 * All are 2D and frequency-reactive; all scale their detail to the panel, so
 * they work on a 16x16 as well as a large gap-mapped matrix.
 *
 * Build:
 *   usermods/cube_fx/cube_fx.cpp   (this file)
 *   usermods/cube_fx/library.json  {"name":"cube_fx",
 *                                   "build":{"libArchive":false}}
 *   platformio_override.ini:  custom_usermods = audioreactive cube_fx
 *
 * Effect functions are void in current WLED - WS2812FX::mode_ptr is
 * void (*)(). Older 0.14-era docs show uint16_t with a return FRAMETIME;
 * that signature will not compile here.
 *
 * This file replaces user_fx_chladni.cpp, user_fx_extras.cpp,
 * user_fx_kaleido.cpp and user_fx_feedback.cpp. Delete those first - keeping
 * them alongside this file gives duplicate symbols at link time and registers
 * every effect twice.
 * ===========================================================================
 */

// ===========================================================================
// SHARED HELPERS
// ===========================================================================

// WLED's built-in AR effects use a getAudioData() helper that is static inside
// FX.cpp and so unreachable from a usermod. This is the same thing.
//
// um_data->u_data[] layout from the audioreactive usermod:
//   [0] float    volumeSmth     smoothed volume
//   [1]          volumeRaw
//   [2] uint8_t* fftResult[16]  16 frequency bins, 0..255
//   [3] uint8_t  samplePeak     1 on a detected beat onset
//   [4] float    FFT_MajorPeak  dominant frequency in Hz
//   [5] float    my_magnitude   magnitude of that peak
static um_data_t *fx_getAudioData() {
  um_data_t *um_data;
  if (!UsermodManager::getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) {
    // audioreactive not compiled in or not enabled: fall back to WLED's
    // fake-audio generator so effects animate instead of freezing
    um_data = simulateSound(SEGMENT.soundSim);
  }
  return um_data;
}

// Triangle wave. Own implementation so this file doesn't depend on which
// lib8tion names survived the FastLED removal.
static inline uint8_t fx_tri8(uint8_t v) {
  return (v < 128) ? (uint8_t)(v << 1) : (uint8_t)((uint8_t)(255 - v) << 1);
}

// Three-band split of the 16 GEQ bins.
static inline void fx_bands(const uint8_t *fft, int &bass, int &mid, int &treb) {
  bass = (fft[0] + fft[1] + fft[2]) / 3;
  mid  = (fft[5] + fft[6] + fft[7] + fft[8]) / 4;
  treb = (fft[12] + fft[13] + fft[14] + fft[15]) / 4;
}

// Volume -> brightness, with a floor so an effect never fully vanishes.
static inline uint8_t fx_drive(float vol, float gain, int floorV) {
  int32_t d = (int32_t)(vol * gain) + floorV;
  if (d < 0) d = 0;
  return (uint8_t)((d > 255) ? 255 : d);
}

// --- frame-rate independence ------------------------------------------------
// Milliseconds since the previous frame, kept in the TOP 16 bits of a 32-bit
// store so the effect can still use the low bits for its own flags.
static inline uint16_t fx_dt(uint32_t &store) {
  const uint16_t now = (uint16_t)strip.now;
  uint16_t dt = (uint16_t)(now - (uint16_t)(store >> 16));
  if (dt > 250) dt = 250;                       // clamp a pause or a fresh start
  store = (store & 0x0000FFFFu) | ((uint32_t)now << 16);
  return dt;
}
static inline uint16_t fx_dt8(uint8_t *store) {
  const uint16_t now = (uint16_t)strip.now;
  uint16_t dt = (uint16_t)(now - ((uint16_t)store[0] | ((uint16_t)store[1] << 8)));
  if (dt > 250) dt = 250;
  store[0] = (uint8_t)(now & 0xFF); store[1] = (uint8_t)(now >> 8);
  return dt;
}
// Turn a per-frame step into a per-elapsed-time one. Calibrated at 23 ms, so
// motion is unchanged at ~43 fps and holds steady above it instead of running
// away with the frame rate.
static inline int32_t fx_step(int32_t perFrame, uint16_t dtMs) {
  return (perFrame * (int32_t)dtMs) / 23;
}
static inline uint8_t fx_fade(int perFrame, uint16_t dtMs) {
  int32_t v = ((int32_t)perFrame * (int32_t)dtMs) / 23;
  return (uint8_t)((v > 255) ? 255 : ((v < 1) ? 1 : v));
}

// --- beat gate ---------------------------------------------------------------
// samplePeak fires on any transient, stays set for several frames, and is
// binary. That combination is what makes beat responses feel violent. This:
//   - consumes the flag the way WLED's own AR effects do, so one hit fires once
//   - ignores transients the LOW bins aren't carrying, so hats and snares and
//     vocal consonants are filtered out and kicks come through
//   - returns the STRENGTH of the hit, so a soft kick gives a soft response
// Build with -D FX_BEAT_FLOOR=25 to let quieter hits through, or a higher
// value to be stricter.
#ifndef FX_BEAT_FLOOR
  #define FX_BEAT_FLOOR 40
#endif
static inline uint8_t fx_lowBeat(um_data_t *um) {
  static uint32_t seenFrame = 0xFFFFFFFFu;
  static uint8_t  cached    = 0;
  static uint8_t  prevPeak  = 0;
  if (strip.now == seenFrame) return cached;   // one answer per frame
  seenFrame = strip.now;
  cached = 0;
  const uint8_t peak = *(uint8_t *)um->u_data[3];
  const bool rising = peak && !prevPeak;       // the flag stays set for frames
  prevPeak = peak ? 1 : 0;
  if (!rising) return 0;
  const uint8_t *fft = (uint8_t *)um->u_data[2];
  const int lo = (fft[0] + fft[1] + fft[2]) / 3;
  int hi = 0;
  for (int k = 7; k < 16; k++) hi += fft[k];
  hi /= 9;
  if (lo < FX_BEAT_FLOOR || lo <= hi) return 0;
  cached = (uint8_t)lo;                        // strength, not a flag
  return cached;
}

// --- cube-net gap skipping --------------------------------------------------
// A gap-mapped cube net is a square segment of 3x3 blocks with the four corner
// blocks unlit, so 44% of the pixels are computed and thrown away. Returns the
// block size, or 0 when the segment isn't shaped that way. Build with
// -D FX_NO_GAP_SKIP if you ever run a genuinely flat panel of these
// proportions and want the corners lit.
static inline int fx_netB(int cols, int rows) {
  if (cols != rows || cols < 12 || (cols % 3) != 0) return 0;
#ifdef FX_NO_GAP_SKIP
  return 0;
#else
  return cols / 3;
#endif
}
#define FX_NET_PREP()  const int _netB = fx_netB(cols, rows); \
                       uint8_t _outCol[cols]; \
                       if (_netB) for (int _c = 0; _c < cols; _c++) _outCol[_c] = (uint8_t)((_c / _netB) != 1)
#define FX_NET_ROW(Y)  const bool _outRow = _netB && (((Y) / _netB) != 1)
#define FX_NET_SKIP(X) if (_outRow && _outCol[X]) continue

// Fast attack, slow release. The difference between punchy and seizure.
static void fx_smoothSpec(uint8_t *spec, const uint8_t *fft, uint8_t sm) {
  if (sm < 1) sm = 1;
  for (int i = 0; i < 16; i++) {
    int cur = (int)spec[i];
    cur += ((int)fft[i] - cur) / (int)sm;
    if ((int)fft[i] > cur) cur = fft[i];
    spec[i] = (uint8_t)cur;
  }
}

// Per-pixel polar lookup. Runs once per segment, so floats are fine here.
// logRadius stores log2(r) instead of r, which is what makes an infinitely
// zooming pattern possible.
static void fx_buildPolar(uint8_t *ang, uint8_t *rad, int cols, int rows, bool logRadius) {
  const float ccx = (cols - 1) * 0.5f;
  const float ccy = (rows - 1) * 0.5f;
  const float rs  = 255.0f / (0.5f * (float)((cols > rows) ? cols : rows));
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      const size_t i = (size_t)y * cols + x;
      const float dx = (float)x - ccx;
      const float dy = (float)y - ccy;
      // atan2 -> 0..255 for a full turn (+256 keeps the cast positive)
      ang[i] = (uint8_t)(int)(atan2f(dy, dx) * (128.0f / 3.14159265f) + 256.5f);
      float r = sqrtf(dx * dx + dy * dy);
      if (logRadius) {
        if (r < 0.7f) r = 0.7f;
        const int L = (int)(log2f(r) * 40.0f + 40.0f);
        rad[i] = (uint8_t)((L < 0) ? 0 : (L > 255 ? 255 : L));
      } else {
        const float v = r * rs;
        rad[i] = (v > 255.0f) ? 255 : (uint8_t)v;
      }
    }
  }
}

// ===========================================================================
// 1. CHLADNI PLATE
// ===========================================================================
// Sand on a vibrating plate collects along the nodal lines of a standing wave.
// The two mode numbers come from the two loudest FFT bins, and the effect
// interpolates between integer modes, so it passes through unstable shapes a
// physical plate can never hold.
// ---------------------------------------------------------------------------
static FX_RET mode_chladni() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const int cols = SEG_W;
  const int rows = SEG_H;
  if (cols < 2 || rows < 2) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  um_data_t     *um   = fx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const float    vol  = *(float *)um->u_data[0];
  const uint8_t  peak = fx_lowBeat(um);

  uint8_t b1 = 0, b2 = 1;
  for (uint8_t i = 1; i < 16; i++) {
    if      (fft[i] > fft[b1]) { b2 = b1; b1 = i; }
    else if (fft[i] > fft[b2]) { b2 = i;          }
  }

  // Cap the top mode to the panel size or fine patterns alias into mush:
  // a 16x16 tops out around 4, a 48x48 handles 8.
  const int shortSide = (cols < rows) ? cols : rows;
  int hardCap = shortSide / 4;
  if (hardCap < 2) hardCap = 2;
  if (hardCap > 8) hardCap = 8;

  // Mode numbers are tracked in HALF steps, so 2 == mode 1.0. Base scale sets
  // where the range starts, Audio span how far above that the music can push
  // it. Base at 0 with full span reproduces the original 1..maxMode sweep.
  const int hMax = hardCap * 2;
  int hBase = 2 + ((int)SEGMENT.custom2 * (hMax - 2)) / 255;
  int hSpan = ((int)SEGMENT.custom3 * (hMax - hBase)) / 31;
  if (hSpan < 2) hSpan = 2;                       // room for two distinct modes
  if (hBase + hSpan > hMax) hBase = hMax - hSpan;
  if (hBase < 2) hBase = 2;

  // 16 bins compress into few mode numbers, so b1 and b2 land on the same one
  // often. When m == n the two halves of the standing wave cancel exactly and
  // s is zero at every pixel - which lights the whole panel. Force them apart.
  int mm = hBase + (b1 * hSpan) / 15;
  int nn = hBase + (b2 * hSpan) / 15;
  if (nn == mm) nn = (nn < hBase + hSpan) ? nn + 1 : nn - 1;
  if (nn < 2) nn = 3;

  const uint16_t tgtM = (uint16_t)(mm * 128);   // half step -> 8.8 fixed
  const uint16_t tgtN = (uint16_t)(nn * 128);

  if (SEGENV.call == 0) { SEGENV.aux0 = tgtM; SEGENV.aux1 = tgtN; }

  int32_t rate = fx_step(1 + (SEGMENT.speed >> 5), fx_dt(SEGENV.step));
  if (rate > 64) rate = 64;
  if (peak && SEGMENT.check1) rate = 64;          // hard snap on beat onset
  SEGENV.aux0 += ((int32_t)tgtM - (int32_t)SEGENV.aux0) * rate / 64;
  SEGENV.aux1 += ((int32_t)tgtN - (int32_t)SEGENV.aux1) * rate / 64;

  const uint16_t m = SEGENV.aux0;
  const uint16_t n = SEGENV.aux1;

  // Separable cosine terms: 2*(W+H) lookups instead of 4*W*H.
  // cos8_t() takes 0..255 for a full turn, so 128 units == pi.
  uint8_t cmx[cols], cnx[cols], cmy[rows], cny[rows];
  for (int x = 0; x < cols; x++) {
    cmx[x] = cos8_t((uint8_t)((uint32_t)m * x / (2u * cols)));
    cnx[x] = cos8_t((uint8_t)((uint32_t)n * x / (2u * cols)));
  }
  for (int y = 0; y < rows; y++) {
    cmy[y] = cos8_t((uint8_t)((uint32_t)m * y / (2u * rows)));
    cny[y] = cos8_t((uint8_t)((uint32_t)n * y / (2u * rows)));
  }

  const uint8_t sharp = 2 + (SEGMENT.intensity >> 4);   // 2..17, higher = thinner
  uint8_t drive = fx_drive(vol, (float)(1 + (SEGMENT.custom1 >> 4)), 0);

  // m and n still cross each other mid-morph; fade through the crossing
  // instead of flashing the panel white
  int sep = (int)m - (int)n;
  if (sep < 0) sep = -sep;
  if (sep < 128) drive = scale8(drive, (uint8_t)(sep * 2));

  FX_NET_PREP();
  for (int y = 0; y < rows; y++) {
    FX_NET_ROW(y);
    for (int x = 0; x < cols; x++) {
      FX_NET_SKIP(x);
      int32_t s = ((int32_t)cmx[x] - 128) * ((int32_t)cny[y] - 128)
                - ((int32_t)cnx[x] - 128) * ((int32_t)cmy[y] - 128);
      if (s < 0) s = -s;
      const uint32_t d = ((uint32_t)s * sharp) >> 8;
      uint8_t lum = (d > 255) ? 0 : (uint8_t)(255 - d);
      lum = scale8(lum, drive);
      const uint8_t idx = (uint8_t)((b1 << 4) + (lum >> 2));
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette(idx, false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_CHLADNI[] PROGMEM =
  "Ace 2-D Chladni Plate@Morph speed,Sharpness,Drive,Base scale,Audio span,Snap to beat;;!;2f;sx=90,ix=110,c1=128,c2=0,c3=31,o1=1";

// ===========================================================================
// 2. BEAT MANDALA
// ===========================================================================
// Radius selects the frequency bin (bass centre, treble rim), angle is folded
// into N mirrored wedges, and the figure jumps by the golden angle on every
// beat so it never lands the same way twice.
// ---------------------------------------------------------------------------
static FX_RET mode_beat_mandala() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const int cols = SEG_W;
  const int rows = SEG_H;
  if (cols < 4 || rows < 4) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  uint8_t *ang  = SEGENV.data;
  uint8_t *rad  = SEGENV.data + n;
  uint8_t *spec = SEGENV.data + 2 * n;

  if (SEGENV.call == 0) {
    fx_buildPolar(ang, rad, cols, rows, false);
    for (int i = 0; i < 16; i++) spec[i] = 0;
  }

  um_data_t     *um   = fx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);

  fx_smoothSpec(spec, fft, 2 + (SEGMENT.custom3 >> 2));   // custom3 is 5-bit

  SEGENV.aux0 += (uint16_t)fx_step((SEGMENT.speed >> 2) + 1, fx_dt(SEGENV.step));
  if (peak && SEGMENT.check1) SEGENV.aux0 += 40503;       // 0.618 of a turn
  const uint8_t rot = SEGENV.aux0 >> 8;

  const uint8_t folds = 2 + (uint8_t)(((uint16_t)SEGMENT.custom1 * 8) >> 8);  // 2..9
  const int     twist = ((int)SEGMENT.custom2 >> 3) - 16;                     // -16..15
  const uint8_t sharp = 1 + (SEGMENT.intensity >> 5);                         // 1..8

  FX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    FX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      FX_NET_SKIP(x);
      const uint8_t r = rad[i];
      const uint8_t a = (uint8_t)(ang[i] - rot);

      uint8_t w = (uint8_t)((uint16_t)a * folds);     // fold into one wedge
      if (w > 127) w = 255 - w;                       // and mirror it

      // shifting the petal phase with radius turns the arms into spirals
      uint8_t petal = cos8_t((uint8_t)((int)(w * 2) + ((int)r * twist) / 8));
      for (uint8_t s = 1; s < sharp; s++) petal = scale8(petal, petal);

      const uint8_t lum = scale8(spec[r >> 4], petal);
      const uint8_t idx = (uint8_t)(((r >> 4) << 4) + rot);
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette(idx, false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_BEAT_MANDALA[] PROGMEM =
  "Ace 2-D Beat Mandala@Rotation,Petal sharpness,Symmetry,Twist,Smoothing,Golden jump;;!;2f;sx=40,ix=128,c1=96,c2=128,c3=8,o1=1";

// ===========================================================================
// 3. SPECTRAL RD  (Gray-Scott reaction-diffusion)
// ===========================================================================
//   dU = Du*lap(U) - U*V^2 + F*(1-U)
//   dV = Dv*lap(V) + U*V^2 - (F+k)*V
// U and V are Q14 fixed point (16384 == 1.0). F and k stay inside the narrow
// corridor where this system makes patterns instead of dying or flooding, and
// beats re-seed it so it can never end up permanently dead.
// ---------------------------------------------------------------------------
static FX_RET mode_spectral_rd() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const int cols = SEG_W;
  const int rows = SEG_H;
  if (cols < 8 || rows < 8) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(4 * n)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  uint16_t *U = (uint16_t *)SEGENV.data;
  uint16_t *V = U + n;

  um_data_t     *um   = fx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];

  if (SEGENV.call == 0) {
    for (size_t k = 0; k < n; k++) { U[k] = 16384; V[k] = 0; }
    for (int s = 0; s < 5; s++) {
      const int sx = 2 + (int)hw_random16(cols - 4);
      const int sy = 2 + (int)hw_random16(rows - 4);
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
          const size_t k = (size_t)(sy + dy) * cols + (sx + dx);
          U[k] = 8192; V[k] = 6000;
        }
    }
  }

  int bass, mid, treb;
  fx_bands(fft, bass, mid, treb);
  const int32_t tgtF = 491 + (bass * 525) / 255;   // F 0.030 .. 0.062
  const int32_t tgtK = 983 + (treb * 100) / 255;   // k 0.0600 .. 0.0661

  if (SEGENV.call == 0) { SEGENV.aux0 = (uint16_t)tgtF; SEGENV.aux1 = (uint16_t)tgtK; }
  SEGENV.aux0 = (uint16_t)((int32_t)SEGENV.aux0 + (tgtF - (int32_t)SEGENV.aux0) / 16);
  SEGENV.aux1 = (uint16_t)((int32_t)SEGENV.aux1 + (tgtK - (int32_t)SEGENV.aux1) / 16);
  const int32_t F = (int32_t)SEGENV.aux0;
  const int32_t K = (int32_t)SEGENV.aux1;

  if (peak && SEGMENT.check1) {
    const int sx = 2 + (int)hw_random16(cols - 4);
    const int sy = 2 + (int)hw_random16(rows - 4);
    for (int dy = -1; dy <= 1; dy++)
      for (int dx = -1; dx <= 1; dx++) {
        const size_t k = (size_t)(sy + dy) * cols + (sx + dx);
        U[k] = 8192; V[k] = 8000;
      }
  }

  // Updating in place would smear the result, so we keep rolling copies of the
  // previous and current row's original values: 4 * cols * 2 bytes of stack.
  const int32_t DU = 41;   // 0.16 * 256
  const int32_t DV = 20;   // 0.08 * 256
  const uint8_t iters = 1 + (SEGMENT.speed >> 6);   // 1..4 steps per frame

  uint16_t pU[cols], pV[cols], cU[cols], cV[cols];

  for (uint8_t it = 0; it < iters; it++) {
    for (int x = 0; x < cols; x++) { cU[x] = U[x]; cV[x] = V[x]; }

    for (int y = 0; y < rows; y++) {
      const size_t row = (size_t)y * cols;
      for (int x = 0; x < cols; x++) {
        const int32_t uc = cU[x], vc = cV[x];
        // Neumann (reflecting) edges: a missing neighbour mirrors the centre
        const int32_t ul = (x > 0)        ? cU[x - 1]         : uc;
        const int32_t ur = (x < cols - 1) ? cU[x + 1]         : uc;
        const int32_t vl = (x > 0)        ? cV[x - 1]         : vc;
        const int32_t vr = (x < cols - 1) ? cV[x + 1]         : vc;
        const int32_t uu = (y > 0)        ? pU[x]             : uc;
        const int32_t vu = (y > 0)        ? pV[x]             : vc;
        const int32_t ud = (y < rows - 1) ? U[row + cols + x] : uc;
        const int32_t vd = (y < rows - 1) ? V[row + cols + x] : vc;

        const int32_t lapU = (ul + ur + uu + ud - 4 * uc) / 4;
        const int32_t lapV = (vl + vr + vu + vd - 4 * vc) / 4;

        const uint32_t v2  = ((uint32_t)vc * (uint32_t)vc) >> 14;
        const int32_t  uvv = (int32_t)(((uint32_t)uc * v2) >> 14);

        int32_t nu = uc + (DU * lapU) / 256 - uvv + (F * (16384 - uc)) / 16384;
        int32_t nv = vc + (DV * lapV) / 256 + uvv - ((F + K) * vc) / 16384;

        U[row + x] = (uint16_t)((nu < 0) ? 0 : (nu > 16384 ? 16384 : nu));
        V[row + x] = (uint16_t)((nv < 0) ? 0 : (nv > 16384 ? 16384 : nv));
      }
      for (int x = 0; x < cols; x++) { pU[x] = cU[x]; pV[x] = cV[x]; }
      if (y < rows - 1)
        for (int x = 0; x < cols; x++) { cU[x] = U[row + cols + x]; cV[x] = V[row + cols + x]; }
    }
  }

  const uint32_t gain = 1 + (SEGMENT.intensity >> 4);   // 1..16
  const uint8_t  tint = fx_drive(vol, 1.0f, 0);
  FX_NET_PREP();
  for (int y = 0; y < rows; y++) {
    FX_NET_ROW(y);
    for (int x = 0; x < cols; x++) {
      FX_NET_SKIP(x);
      uint32_t l = ((uint32_t)V[(size_t)y * cols + x] * gain) >> 8;
      if (l > 255) l = 255;
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(255 - l + tint), false, false, 0, (uint8_t)l));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_SPECTRAL_RD[] PROGMEM =
  "Ace 2-D Spectral RD@Sim speed,Brightness,,,,Seed on beat;;!;2f;sx=160,ix=140,o1=1";

// ===========================================================================
// 4. HARMONIC LISSAJOUS
// ===========================================================================
// A root note is captured on a beat. Each frame the dominant frequency is
// compared to that root, folded into one octave and snapped to the nearest
// simple ratio. Consonant intervals land on a stable closed figure; anything
// dissonant fails to lock and the curve precesses instead.
// ---------------------------------------------------------------------------
static const uint8_t _lissRatio[9][2] = {
  {1,1}, {6,5}, {5,4}, {4,3}, {3,2}, {8,5}, {5,3}, {16,9}, {2,1}
};

static FX_RET mode_harmonic_lissajous() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const int cols = SEG_W;
  const int rows = SEG_H;
  if (cols < 4 || rows < 4) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  um_data_t    *um   = fx_getAudioData();
  const float   hzF  = *(float *)um->u_data[4];
  const float   vol  = *(float *)um->u_data[0];
  const uint8_t peak = fx_lowBeat(um);

  SEGMENT.fadeToBlackBy(fx_fade(1 + (255 - SEGMENT.intensity) / 8, fx_dt(SEGENV.step)));

  const uint16_t hz = (hzF > 30.0f && hzF < 8000.0f) ? (uint16_t)hzF : 0;
  if (SEGENV.call == 0 || SEGENV.aux0 == 0) SEGENV.aux0 = hz ? hz : 220;
  if (hz && peak && SEGMENT.check1) SEGENV.aux0 = hz;
  const uint16_t root = SEGENV.aux0;

  float r = (hz && root) ? (float)hz / (float)root : 1.0f;
  while (r >= 2.0f) r *= 0.5f;
  while (r <  1.0f) r *= 2.0f;

  uint8_t best = 0;
  float   bestErr = 99.0f;
  for (uint8_t i = 0; i < 9; i++) {
    const float e = fabsf(r - (float)_lissRatio[i][0] / (float)_lissRatio[i][1]);
    if (e < bestErr) { bestErr = e; best = i; }
  }
  const bool    locked = (bestErr < 0.035f);
  const uint8_t a = _lissRatio[best][0];
  const uint8_t b = _lissRatio[best][1];

  const uint8_t phase  = (uint8_t)((strip.now * (1 + (SEGMENT.speed >> 4))) >> 6);
  const uint8_t detune = locked ? 0 : (uint8_t)(strip.now >> 5);
  const uint8_t hue    = (uint8_t)(strip.now >> 6);

  const uint8_t dr   = fx_drive(vol, (float)(1 + (SEGMENT.custom1 >> 4)), 0);
  const int     amp  = (dr < 40) ? 40 : dr;
  const int     ampX = ((cols - 1) * amp) / 510;
  const int     ampY = ((rows - 1) * amp) / 510;
  const int     ccx  = (cols - 1) / 2;
  const int     ccy  = (rows - 1) / 2;

  int steps = 96 * (a + b);
  if (steps < 256) steps = 256;
  if (steps > 900) steps = 900;

  for (int i = 0; i < steps; i++) {
    // 16-bit parameter: multiplying before truncation keeps the trace smooth
    const uint16_t t16 = (uint16_t)(((uint32_t)i << 16) / (uint32_t)steps);
    const uint8_t  sx  = sin8_t((uint8_t)((((uint32_t)a * t16) >> 8) + phase));
    const uint8_t  sy  = sin8_t((uint8_t)((((uint32_t)b * t16) >> 8) + detune));

    const int px = ccx + (((int)sx - 128) * ampX) / 128;
    const int py = ccy + (((int)sy - 128) * ampY) / 128;
    if (px < 0 || py < 0 || px >= cols || py >= rows) continue;

    SEGMENT.addPixelColorXY(px, py,
      SEGMENT.color_from_palette((uint8_t)((t16 >> 8) + hue), false, false, 0));
  }

  if (SEGMENT.check2) SEGMENT.blur(40);
  FX_DONE;
}

static const char _data_FX_MODE_HARMONIC_LISSAJOUS[] PROGMEM =
  "Ace 2-D Harmonic Lissajous@Drift,Trail,Drive,,,Re-root on beat,Glow;;!;2f;sx=64,ix=200,c1=128,o1=1,o2=1";

// ===========================================================================
// 5. KALEIDOSCOPE
// ===========================================================================
// The honest version: a chaotic source chamber seen through N mirrors. The LUT
// folds every pixel back into the fundamental wedge and records where in the
// chamber it lands; each frame that chamber is rotated and resampled from 3D
// noise. Beats shake the tube, which is what your hand does to a real one.
// Changing the mirror count rebuilds the LUT - a few ms, once.
// ---------------------------------------------------------------------------
static FX_RET mode_kaleidoscope() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const int cols = SEG_W;
  const int rows = SEG_H;
  if (cols < 4 || rows < 4) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 4)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  uint8_t *su = SEGENV.data + 4;
  uint8_t *sv = SEGENV.data + 4 + n;

  const uint8_t mirrors = 3 + (uint8_t)(((uint16_t)SEGMENT.custom1 * 10) >> 8);  // 3..12

  if (SEGENV.call == 0 || SEGENV.data[0] != mirrors) {
    const float ccx   = (cols - 1) * 0.5f;
    const float ccy   = (rows - 1) * 0.5f;
    const float rmax  = 0.5f * (float)((cols > rows) ? cols : rows);
    const float k     = 110.0f / rmax;
    const float wedge = 6.28318531f / (float)mirrors;
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        const size_t i = (size_t)y * cols + x;
        const float dx = (float)x - ccx;
        const float dy = (float)y - ccy;
        const float r  = sqrtf(dx * dx + dy * dy);
        float th = atan2f(dy, dx);
        if (th < 0.0f) th += 6.28318531f;
        float phi = fmodf(th, wedge);
        if (phi > wedge * 0.5f) phi = wedge - phi;      // the mirror
        const int a = (int)(128.0f + r * k * cosf(phi));
        const int b = (int)(128.0f + r * k * sinf(phi));
        su[i] = (uint8_t)((a < 0) ? 0 : (a > 255 ? 255 : a));
        sv[i] = (uint8_t)((b < 0) ? 0 : (b > 255 ? 255 : b));
      }
    }
    SEGENV.data[0] = mirrors;
  }

  um_data_t     *um   = fx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  fx_bands(fft, bass, mid, treb);

  const uint16_t dtK = fx_dt(SEGENV.step);
  SEGENV.aux0 += (uint16_t)fx_step((SEGMENT.speed >> 3) + 1, dtK);
  if (peak && SEGMENT.check1) SEGENV.aux0 += hw_random16();   // shake the tube
  const uint8_t rot  = SEGENV.aux0 >> 8;
  const int     cosR = (int)cos8_t(rot) - 128;
  const int     sinR = (int)sin8_t(rot) - 128;

  SEGENV.aux1 += (uint16_t)fx_step(1 + (bass >> 4), dtK);    // churn the contents
  const uint16_t zt    = SEGENV.aux1;
  const uint16_t scale = 6 + (uint16_t)(SEGMENT.custom2 >> 3) + (uint16_t)(treb >> 4);

  const uint8_t drive = scale8(fx_drive(vol, 3.0f, 24), SEGMENT.intensity | 0x0F);
  const uint8_t spark = (uint8_t)(255 - ((int)SEGMENT.custom3 * 3) - (treb >> 2));
  const uint8_t hue   = (uint8_t)(strip.now >> 7);

  FX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    FX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      FX_NET_SKIP(x);
      const int u = (int)su[i] - 128;
      const int v = (int)sv[i] - 128;
      int ru = (u * cosR - v * sinR) / 128 + 128;
      int rv = (u * sinR + v * cosR) / 128 + 128;
      if (ru < 0) ru = 0; else if (ru > 255) ru = 255;
      if (rv < 0) rv = 0; else if (rv > 255) rv = 255;

      uint8_t c = perlin8((uint16_t)(ru * scale), (uint16_t)(rv * scale), zt);
      c = qsub8(c, 16);                        // perlin clusters mid-range;
      c = qadd8(c, scale8(c, 39));             // stretch it back out

      if (c > spark) {                         // treble picks out highlights
        SEGMENT.setPixelColorXY(x, y, RGBW32(255, 255, 255, 0));
        continue;
      }
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(c + hue), false, false, 0, drive));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_KALEIDOSCOPE[] PROGMEM =
  "Ace 2-D Kaleidoscope@Chamber spin,Brightness,Mirrors,Zoom,Sparkle,Shake on beat;;!;2f;sx=48,ix=200,c1=110,c2=100,c3=16,o1=1";

// ===========================================================================
// 6. HEX QUILT
// ===========================================================================
// Wallpaper symmetry (p6m) instead of a rosette: three triangle waves on axes
// 60 degrees apart, multiplied together. No centre, so it fills the whole
// panel evenly - the one that reads best across a folded net.
// ---------------------------------------------------------------------------
static FX_RET mode_hex_quilt() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const int cols = SEG_W;
  const int rows = SEG_H;

  um_data_t     *um   = fx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  fx_bands(fft, bass, mid, treb);

  SEGENV.aux0 += (uint16_t)fx_step((SEGMENT.speed >> 4) + 1, fx_dt(SEGENV.step));
  if (peak && SEGMENT.check1) SEGENV.aux0 += 40503;      // golden-angle reorient
  const uint8_t rot  = SEGENV.aux0 >> 8;
  const int     cosR = (int)cos8_t(rot) - 128;
  const int     sinR = (int)sin8_t(rot) - 128;

  const int sc = 180 + (int)SEGMENT.custom1 * 2 + (bass * 2);   // lattice pitch

  const uint8_t base   = (uint8_t)((strip.now * (1 + (SEGMENT.speed >> 4))) >> 7);
  const uint8_t spread = 1 + (SEGMENT.custom3 >> 1);            // custom3 is 5-bit
  const uint8_t ph1 = base + (uint8_t)((bass * spread) >> 4);
  const uint8_t ph2 = base + (uint8_t)((mid  * spread) >> 4);
  const uint8_t ph3 = base + (uint8_t)((treb * spread) >> 4);

  const uint8_t drive = fx_drive(vol, 3.0f, 24);
  const uint8_t hue   = (uint8_t)(strip.now >> 7);

  const int ccx = cols / 2;
  const int ccy = rows / 2;
  const int OFF = 1024;      // keeps every argument positive; constant phase

  FX_NET_PREP();
  for (int y = 0; y < rows; y++) {
    FX_NET_ROW(y);
    for (int x = 0; x < cols; x++) {
      FX_NET_SKIP(x);
      const int dx = x - ccx;
      const int dy = y - ccy;
      const int rx = (dx * cosR - dy * sinR) / 128;
      const int ry = (dx * sinR + dy * cosR) / 128;

      // three axes at 0, 60 and 120 degrees (0.866 ~ 222/256)
      const int u1 = rx + OFF;
      const int u2 = ((rx * 128 + ry * 222) / 256) + OFF;
      const int u3 = ((-rx * 128 + ry * 222) / 256) + OFF;

      const uint8_t w1 = fx_tri8((uint8_t)(((u1 * sc) / 16) + ph1));
      const uint8_t w2 = fx_tri8((uint8_t)(((u2 * sc) / 16) + ph2));
      const uint8_t w3 = fx_tri8((uint8_t)(((u3 * sc) / 16) + ph3));

      const uint8_t soft = (uint8_t)(((uint16_t)w1 + w2 + w3) / 3);
      const uint8_t hard = scale8(scale8(w1, w2), w3);
      const uint8_t v = (uint8_t)((((uint16_t)soft * (255 - SEGMENT.intensity))
                                 + ((uint16_t)hard * SEGMENT.intensity)) >> 8);

      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(v + hue), false, false, 0, scale8(v, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_HEX_QUILT[] PROGMEM =
  "Ace 2-D Hex Quilt@Drift,Cell sharpness,Zoom,,Band spread,Reorient on beat;;!;2f;sx=64,ix=150,c1=110,c3=16,o1=1";

// ===========================================================================
// 7. MOIRE ROSETTE
// ===========================================================================
// Two radial gratings with different spoke counts, counter-rotating. The
// visible rosette has |S1-S2| arms even though neither grating does - the
// structure is pure interference, so it shifts in ways the audio doesn't
// obviously predict.
// ---------------------------------------------------------------------------
static FX_RET mode_moire_rosette() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const int cols = SEG_W;
  const int rows = SEG_H;
  if (cols < 4 || rows < 4) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  uint8_t *ang = SEGENV.data;
  uint8_t *rad = SEGENV.data + n;

  if (SEGENV.call == 0) fx_buildPolar(ang, rad, cols, rows, false);

  um_data_t     *um   = fx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  int bass, mid, treb;
  fx_bands(fft, bass, mid, treb);

  const int bias = 2 + (SEGMENT.custom1 >> 5);           // 2..9
  int S1 = bias + (bass * 6) / 255;
  int S2 = bias + (treb * 6) / 255 + 1;
  if (S2 == S1) S2++;

  if (peak && SEGMENT.check1) SEGENV.step ^= 1;          // flip spin direction
  const int dir = (SEGENV.step & 1) ? -1 : 1;
  const uint16_t dtM = fx_dt(SEGENV.step);
  SEGENV.aux0 += (uint16_t)fx_step(dir * (1 + (SEGMENT.speed >> 3)), dtM);
  SEGENV.aux1 -= (uint16_t)fx_step(dir * (1 + (SEGMENT.speed >> 4)), dtM);
  const uint8_t r1 = SEGENV.aux0 >> 8;
  const uint8_t r2 = SEGENV.aux1 >> 8;

  const int     tw    = ((int)SEGMENT.custom2 >> 3) - 16;  // spiral twist
  const uint8_t sharp = 1 + (SEGMENT.intensity >> 6);      // 1..4
  const uint8_t drive = fx_drive(vol, 3.0f, 30);
  const uint8_t hue   = (uint8_t)(strip.now >> 7);

  FX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    FX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      FX_NET_SKIP(x);
      const int a = (int)ang[i];
      const int r = (int)rad[i];
      const uint8_t g1 = cos8_t((uint8_t)(a * S1 + (int)r1 + (r * tw) / 8));
      const uint8_t g2 = cos8_t((uint8_t)(a * S2 + (int)r2 - (r * tw) / 8));

      uint8_t v = scale8(g1, g2);
      for (uint8_t s = 1; s < sharp; s++) v = scale8(v, v);
      if (SEGMENT.check2) v = scale8(v, (uint8_t)(255 - (r >> 2)));   // vignette

      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(v + (r >> 1) + hue), false, false, 0,
                                   scale8(v, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_MOIRE_ROSETTE[] PROGMEM =
  "Ace 2-D Moire Rosette@Spin,Sharpness,Spokes,Spiral twist,,Flip on beat,Vignette;;!;2f;sx=80,ix=120,c1=128,c2=128,o1=1,o2=1";

// ===========================================================================
// 8. SPECTRAL WORMHOLE
// ===========================================================================
// Log-polar: the LUT stores log2(radius), so a pattern periodic in that
// coordinate zooms into the centre forever without ever repeating a seam.
// Spectrum bands wrap the spiral and fall inward, mirrored N times.
// ---------------------------------------------------------------------------
static FX_RET mode_spectral_wormhole() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const int cols = SEG_W;
  const int rows = SEG_H;
  if (cols < 4 || rows < 4) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  uint8_t *ang  = SEGENV.data;
  uint8_t *lorg = SEGENV.data + n;
  uint8_t *spec = SEGENV.data + 2 * n;

  if (SEGENV.call == 0) {
    fx_buildPolar(ang, lorg, cols, rows, true);
    for (int i = 0; i < 16; i++) spec[i] = 0;
  }

  um_data_t     *um   = fx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  int bass, mid, treb;
  fx_bands(fft, bass, mid, treb);

  fx_smoothSpec(spec, fft, 2 + (SEGMENT.custom3 >> 2));

  if (peak && SEGMENT.check1) SEGENV.step ^= 1;
  const int dir = (SEGENV.step & 1) ? -1 : 1;
  SEGENV.aux0 += (uint16_t)fx_step(dir * (1 + (SEGMENT.speed >> 4) + (bass >> 3)), fx_dt(SEGENV.step));
  const uint8_t zoom = SEGENV.aux0 >> 6;

  const uint8_t folds = 2 + (uint8_t)(((uint16_t)SEGMENT.custom1 * 8) >> 8);   // 2..9
  const int     tw    = ((int)SEGMENT.custom2 >> 3) - 16;
  const uint8_t hue   = (uint8_t)(strip.now >> 7);
  const uint8_t width = 1 + (SEGMENT.intensity >> 6);

  FX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    FX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      FX_NET_SKIP(x);
      uint8_t w = (uint8_t)((uint16_t)ang[i] * folds);   // mirror the tunnel
      if (w > 127) w = 255 - w;

      const uint8_t s8 = (uint8_t)((int)lorg[i] - (int)zoom + ((int)w * tw) / 4);
      uint8_t prof = cos8_t((uint8_t)(s8 << 4));
      for (uint8_t k = 1; k < width; k++) prof = scale8(prof, prof);

      const uint8_t lum = scale8(spec[(s8 >> 4) & 0x0F], prof);
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(((s8 >> 4) << 4) + hue), false, false, 0, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_SPECTRAL_WORMHOLE[] PROGMEM =
  "Ace 2-D Spectral Wormhole@Fall rate,Band width,Mirrors,Spiral twist,Smoothing,Reverse on beat;;!;2f;sx=110,ix=100,c1=96,c2=140,c3=8,o1=1";

// ===========================================================================
// 9. FEEDBACK ECHO
// ===========================================================================
// The camera-pointed-at-its-own-monitor effect. Each frame the previous frame
// is resampled through a small rotation and scale, dimmed, and a fresh ring of
// spectrum is injected at the rim. Nothing computes the spiral - it emerges
// from iterating an affine map. Keeps its own field (2 bytes per pixel, double
// buffered) rather than reading back the WLED buffer, so brightness scaling
// and transitions don't get fed back into the loop.
// ---------------------------------------------------------------------------
static FX_RET mode_feedback_echo() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const int cols = SEG_W;
  const int rows = SEG_H;
  if (cols < 8 || rows < 8) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(4 * n + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  uint8_t *field = SEGENV.data;
  uint8_t *spec  = SEGENV.data + 4 * n;

  if (SEGENV.call == 0) {
    for (size_t i = 0; i < 4 * n + 16; i++) field[i] = 0;
    SEGENV.step = 0;
    SEGENV.aux0 = 0;
    SEGENV.aux1 = 0;
  }

  // ping-pong: read the buffer we wrote last frame, write the other
  const uint8_t par = (uint8_t)((SEGENV.step >> 1) & 1);
  uint8_t *valS = field + (par ? 2 * n : 0);
  uint8_t *hueS = valS + n;
  uint8_t *valD = field + (par ? 0 : 2 * n);
  uint8_t *hueD = valD + n;

  um_data_t     *um   = fx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];

  fx_smoothSpec(spec, fft, 5);
  const int bass = (spec[0] + spec[1] + spec[2]) / 3;

  if (peak) {
    if (SEGMENT.check1) SEGENV.step ^= 1;                 // reverse direction
    SEGENV.aux1 = peak;                                   // lurch, scaled to the hit
  } else {
    SEGENV.aux1 = (uint16_t)((SEGENV.aux1 * 13) / 16);
  }
  const int surge = (int)SEGENV.aux1;
  const int dir   = (SEGENV.step & 1) ? -1 : 1;           // +1 = falls inward

  // the affine map: two trig calls per frame, then pure integer
  const uint16_t dtF = fx_dt(SEGENV.step);
  const float tscale = (float)dtF / 23.0f;
  const float rot = ((float)SEGMENT.custom2 - 128.0f) * 0.0004f * tscale;
  float step = (0.004f + (float)SEGMENT.speed * 0.00022f
             + (float)(surge + bass) * 0.00012f) * tscale;
  if (step > 0.12f) step = 0.12f;
  const float k = 1.0f + (float)dir * step;

  const int32_t M00 = (int32_t)(cosf(rot) * k * 256.0f);
  const int32_t M01 = (int32_t)(-sinf(rot) * k * 256.0f);
  const int32_t M10 = (int32_t)(sinf(rot) * k * 256.0f);
  const int32_t M11 = (int32_t)(cosf(rot) * k * 256.0f);

  const int32_t CX   = (int32_t)(cols - 1) * 128;         // centre, 8.8
  const int32_t CY   = (int32_t)(rows - 1) * 128;
  const int32_t BIAS = 1 << 22;                           // keeps the divide positive
  const int32_t BOFF = BIAS / 256;

  // exponential decay held constant per unit time, not per frame
  const float   fbase = (float)(232 + (SEGMENT.intensity >> 4)) / 255.0f;
  const uint8_t fade  = (uint8_t)(255.0f * powf(fbase, tscale));
  const uint8_t hueDr = 1 + (SEGMENT.custom3 >> 3);

  for (int y = 0; y < rows; y++) {
    const int32_t dy8 = ((int32_t)y << 8) - CY;
    int32_t fx = ((M00 * (-CX)) + (M01 * dy8) + BIAS) / 256 + CX - BOFF;
    int32_t fy = ((M10 * (-CX)) + (M11 * dy8) + BIAS) / 256 + CY - BOFF;
    const size_t drow = (size_t)y * cols;

    for (int x = 0; x < cols; x++, fx += M00, fy += M10) {
      if (fx < 0 || fy < 0) { valD[drow + x] = 0; hueD[drow + x] = 0; continue; }
      const int ix = (int)(fx >> 8);
      const int iy = (int)(fy >> 8);
      if (ix >= cols - 1 || iy >= rows - 1) { valD[drow + x] = 0; hueD[drow + x] = 0; continue; }

      const int fxr = (int)(fx & 255);
      const int fyr = (int)(fy & 255);
      const size_t o = (size_t)iy * cols + ix;

      // bilinear on value - the interpolation IS the diffusion that makes
      // feedback look organic instead of like a stair-stepped copy
      const int v00 = valS[o],        v10 = valS[o + 1];
      const int v01 = valS[o + cols], v11 = valS[o + cols + 1];
      const int top = v00 + ((v10 - v00) * fxr) / 256;
      const int bot = v01 + ((v11 - v01) * fxr) / 256;
      int v = top + ((bot - top) * fyr) / 256;

      v = (int)scale8((uint8_t)v, fade);
      valD[drow + x] = (uint8_t)v;

      // hue takes the nearest neighbour - interpolating across the wrap point
      // would produce grey mush instead of a colour rotation
      const size_t hs = o + ((fxr > 127) ? 1 : 0) + ((fyr > 127) ? cols : 0);
      hueD[drow + x] = (uint8_t)(hueS[hs] + hueDr);
    }
  }

  // inject a fresh spectrum ring
  const int ccx  = (cols - 1) / 2;
  const int ccy  = (rows - 1) / 2;
  const int rmax = ((cols < rows) ? cols : rows) / 2;
  const int rIn  = (dir > 0) ? (rmax - 1) : (1 + rmax / 5);

  const uint8_t folds = 2 + (uint8_t)(((uint16_t)SEGMENT.custom1 * 8) >> 8);   // 2..9
  SEGENV.aux0 += 1 + (SEGMENT.custom3 >> 2);
  const uint8_t injRot  = SEGENV.aux0 >> 6;
  const uint8_t hueBase = (uint8_t)(strip.now >> 7);

  const int steps = 4 * rmax + 48;
  for (int i = 0; i < steps; i++) {
    const uint8_t th = (uint8_t)(((uint32_t)i * 256) / (uint32_t)steps);

    // fold before rotating, so the injected ring keeps N-fold symmetry
    uint8_t w = (uint8_t)((uint16_t)th * folds);
    if (w > 127) w = 255 - w;
    uint8_t bin = (uint8_t)(((uint16_t)w * 16) >> 7);
    if (bin > 15) bin = 15;

    const uint8_t amp = spec[bin];
    if (amp < 10) continue;

    const uint8_t a = (uint8_t)(th + injRot);
    const int px = ccx + (((int)cos8_t(a) - 128) * rIn) / 128;
    const int py = ccy + (((int)sin8_t(a) - 128) * rIn) / 128;
    if (px < 0 || py < 0 || px >= cols || py >= rows) continue;

    const size_t o = (size_t)py * cols + px;
    if (amp > valD[o]) valD[o] = amp;
    hueD[o] = (uint8_t)(hueBase + (bin << 3));
  }

  SEGENV.step ^= 2;                       // swap buffers for next frame

  const uint8_t drive = fx_drive(vol, 3.0f, 40);
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++, i++) {
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette(hueD[i], false, false, 0, scale8(valD[i], drive)));
    }
  }

  if (SEGMENT.check2) SEGMENT.blur(32);
  FX_DONE;
}

static const char _data_FX_MODE_FEEDBACK_ECHO[] PROGMEM =
  "Ace 2-D Feedback Echo@Fall rate,Persistence,Mirrors,Spin,Spiral,Reverse on beat,Glow;;!;2f;sx=110,ix=200,c1=96,c2=140,c3=10,o1=1";

// ===========================================================================
// 10. GLASS KALEIDOSCOPE
// ===========================================================================
// A real kaleidoscope's chamber holds loose chips of coloured glass, not a
// continuous smear. This puts discrete shapes in the chamber - circles,
// squares, triangles and six-point stars - and leaves everything between them
// black, so the mirrors multiply hard-edged objects across a lot of empty
// space instead of filling the panel edge to edge.
//
// The fold LUT maps each pixel into a narrow wedge of the chamber, so at any
// moment only the chips the wedge happens to cross are visible. Rotating the
// chamber sweeps new ones into view, exactly like turning the tube.
//
// Shape tests are all cheap integer algebra with a bounding-box reject first:
// circle by squared distance, square by rotated Chebyshev distance, and
// triangle by three half-planes. The star reuses those same three dot
// products - a hexagram is just the up triangle OR the down one.
// ---------------------------------------------------------------------------
#define GK_MAX 12

static FX_RET mode_glass_kaleidoscope() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 8 || rows < 8) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 8 * GK_MAX + 24)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *su   = SEGENV.data;
  uint8_t *sv   = su + n;
  uint8_t *ob   = sv + n;              // 8 bytes per chip
  uint8_t *spec = ob + 8 * GK_MAX;
  uint8_t *gs   = spec + 16;           // [0] mirrors, [1] prev beat, [2..3] tick, [4] round robin

  const uint8_t mirrors = 3 + (uint8_t)(((uint16_t)SEGMENT.custom1 * 10) >> 8);

  if (SEGENV.call == 0 || gs[0] != mirrors) {
    const float ccx  = (cols - 1) * 0.5f, ccy = (rows - 1) * 0.5f;
    const float rmax = 0.5f * (float)((cols > rows) ? cols : rows);
    const float kk   = 110.0f / rmax;
    const float wedge = 6.28318531f / (float)mirrors;
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        const size_t i = (size_t)y * cols + x;
        const float dx = (float)x - ccx, dy = (float)y - ccy;
        const float r = sqrtf(dx * dx + dy * dy);
        float th = atan2f(dy, dx);
        if (th < 0.0f) th += 6.28318531f;
        float phi = fmodf(th, wedge);
        if (phi > wedge * 0.5f) phi = wedge - phi;
        const int a = (int)(128.0f + r * kk * cosf(phi));
        const int b = (int)(128.0f + r * kk * sinf(phi));
        su[i] = (uint8_t)((a < 0) ? 0 : (a > 255 ? 255 : a));
        sv[i] = (uint8_t)((b < 0) ? 0 : (b > 255 ? 255 : b));
      }
    }
    if (SEGENV.call == 0) {
      for (int k = 0; k < GK_MAX; k++) {
        uint8_t *o = ob + k * 8;
        const uint8_t ang = hw_random8();
        const int rad = 20 + (int)hw_random16(80);
        o[0] = (uint8_t)(int8_t)((((int)cos8_t(ang) - 128) * rad) / 128);
        o[1] = (uint8_t)(int8_t)((((int)sin8_t(ang) - 128) * rad) / 128);
        o[2] = (uint8_t)(k & 3);                            // shape type
        o[3] = (uint8_t)(k * 21);                           // hue
        o[4] = hw_random8();                                // rotation
        o[5] = (uint8_t)(int8_t)((int)hw_random16(9) - 4);  // vx
        o[6] = (uint8_t)(int8_t)((int)hw_random16(9) - 4);  // vy
        o[7] = (uint8_t)(int8_t)((int)hw_random16(11) - 5); // spin
      }
      for (int i = 0; i < 16; i++) spec[i] = 0;
      gs[1] = 0; gs[4] = 0;
      const uint16_t t0 = (uint16_t)(strip.now >> 4);
      gs[2] = (uint8_t)(t0 & 0xFF); gs[3] = (uint8_t)(t0 >> 8);
    }
    gs[0] = mirrors;
  }

  um_data_t     *um   = fx_getAudioData();
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  fx_smoothSpec(spec, fft, 3);

  const uint16_t nowT  = (uint16_t)(strip.now >> 4);
  const uint16_t prevT = (uint16_t)gs[2] | ((uint16_t)gs[3] << 8);
  uint16_t dt = (uint16_t)(nowT - prevT);
  if (dt > 16) dt = 16;
  gs[2] = (uint8_t)(nowT & 0xFF); gs[3] = (uint8_t)(nowT >> 8);

  const int spd = 1 + (SEGMENT.speed >> 5);              // 1..8

  SEGENV.aux0 += (uint16_t)((uint32_t)dt * (uint32_t)(spd * 10));
  const bool rising = peak && !gs[1];
  gs[1] = peak ? 1 : 0;
  if (rising && SEGMENT.check1) SEGENV.aux0 += hw_random16();
  const uint8_t rot  = SEGENV.aux0 >> 8;
  const int     cosR = (int)cos8_t(rot) - 128;
  const int     sinR = (int)sin8_t(rot) - 128;

  const int count = 2 + ((int)SEGMENT.custom3 * 10) / 31;    // 2..12 chips
  const int baseR = 8 + ((int)SEGMENT.custom2 * 40) / 255;   // 8..48 chamber units

  for (int k = 0; k < count; k++) {                          // drift and spin
    uint8_t *o = ob + k * 8;
    int ox = (int8_t)o[0] + (((int)(int8_t)o[5] * (int)dt * spd) / 32);
    int oy = (int8_t)o[1] + (((int)(int8_t)o[6] * (int)dt * spd) / 32);
    if (ox >  105) { ox =  105; o[5] = (uint8_t)(int8_t)(-(int)(int8_t)o[5]); }
    if (ox < -105) { ox = -105; o[5] = (uint8_t)(int8_t)(-(int)(int8_t)o[5]); }
    if (oy >  105) { oy =  105; o[6] = (uint8_t)(int8_t)(-(int)(int8_t)o[6]); }
    if (oy < -105) { oy = -105; o[6] = (uint8_t)(int8_t)(-(int)(int8_t)o[6]); }
    o[0] = (uint8_t)(int8_t)ox; o[1] = (uint8_t)(int8_t)oy;
    o[4] = (uint8_t)(o[4] + (((int)(int8_t)o[7] * (int)dt * spd) / 16));
  }

  if (rising && SEGMENT.check2 && count > 0) {               // reshuffle one chip
    const int k = gs[4] % count;
    gs[4] = (uint8_t)(gs[4] + 1);
    uint8_t *o = ob + k * 8;
    const uint8_t ang = hw_random8();
    const int rad = 20 + (int)hw_random16(80);
    o[0] = (uint8_t)(int8_t)((((int)cos8_t(ang) - 128) * rad) / 128);
    o[1] = (uint8_t)(int8_t)((((int)sin8_t(ang) - 128) * rad) / 128);
    o[2] = (uint8_t)hw_random16(4);
    o[3] = hw_random8();
  }

  // resolve every chip once per frame instead of once per pixel
  int cxo[GK_MAX], cyo[GK_MAX], cR[GK_MAX], cT[GK_MAX];
  int nx0[GK_MAX], ny0[GK_MAX], nx1[GK_MAX], ny1[GK_MAX], nx2[GK_MAX], ny2[GK_MAX];
  int cca[GK_MAX], csa[GK_MAX];
  uint8_t cH[GK_MAX];
  int live = 0;
  for (int k = 0; k < count; k++) {
    uint8_t *o = ob + k * 8;
    const int bin = (count > 1) ? ((k * 15) / (count - 1)) : 0;
    int R = (baseR * (100 + ((int)spec[bin] * 155) / 255)) / 255;
    if (R < 3) R = 3;
    cxo[live] = (int8_t)o[0]; cyo[live] = (int8_t)o[1];
    cR[live]  = R; cT[live] = o[2] & 3; cH[live] = o[3];
    const uint8_t a = o[4];
    cca[live] = (int)cos8_t(a) - 128;
    csa[live] = (int)sin8_t(a) - 128;
    const uint8_t a0 = (uint8_t)(a + 64);          // three edge normals, 120 apart
    const uint8_t a1 = (uint8_t)(a + 64 + 85);
    const uint8_t a2 = (uint8_t)(a + 64 + 170);
    nx0[live] = (int)cos8_t(a0) - 128; ny0[live] = (int)sin8_t(a0) - 128;
    nx1[live] = (int)cos8_t(a1) - 128; ny1[live] = (int)sin8_t(a1) - 128;
    nx2[live] = (int)cos8_t(a2) - 128; ny2[live] = (int)sin8_t(a2) - 128;
    live++;
  }

  if (SEGMENT.check3) SEGMENT.fadeToBlackBy(50); else SEGMENT.fill(SEGCOLOR(0));

  const uint8_t drive = fx_drive(vol, 2.0f, 90);
  const uint8_t hueSh = (uint8_t)(strip.now >> 8);

  FX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    FX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      FX_NET_SKIP(x);
      const int u = (int)su[i] - 128, v = (int)sv[i] - 128;
      const int pcx = (u * cosR - v * sinR) / 128;
      const int pcy = (u * sinR + v * cosR) / 128;

      uint8_t best = 0, bh = 0;
      for (int k = 0; k < live; k++) {
        const int R  = cR[k];
        const int dx = pcx - cxo[k], dy = pcy - cyo[k];
        const int bb = R + (R >> 1);
        if (dx > bb || dx < -bb || dy > bb || dy < -bb) continue;

        int inDepth = -1;
        switch (cT[k]) {
          case 0: {                                   // circle
            const int d2 = dx * dx + dy * dy;
            if (d2 > R * R) break;
            inDepth = (R * R - d2) / (2 * R + 1);
            break; }
          case 1: {                                   // square
            const int uu = (dx * cca[k] - dy * csa[k]) / 128;
            const int vv = (dx * csa[k] + dy * cca[k]) / 128;
            const int au = (uu < 0) ? -uu : uu, av = (vv < 0) ? -vv : vv;
            const int m  = (au > av) ? au : av;
            if (m > R) break;
            inDepth = R - m;
            break; }
          default: {                                  // triangle (2), hexagram (3)
            const int T  = R * 64;
            const int t0 = dx * nx0[k] + dy * ny0[k];
            const int t1 = dx * nx1[k] + dy * ny1[k];
            const int t2 = dx * nx2[k] + dy * ny2[k];
            int mxv = t0; if (t1 > mxv) mxv = t1; if (t2 > mxv) mxv = t2;
            int dpt = (T - mxv) / 128;
            if (cT[k] == 3) {
              int mnv = t0; if (t1 < mnv) mnv = t1; if (t2 < mnv) mnv = t2;
              const int dpt2 = (T + mnv) / 128;
              if (dpt2 > dpt) dpt = dpt2;
            }
            if (dpt < 0) break;
            inDepth = dpt;
            break; }
        }
        if (inDepth < 0) continue;

        // edge softness scales with the chip, so small chips stay crisp
        const int soft = 1 + (((255 - (int)SEGMENT.intensity) * R) / 512);
        int l = (inDepth * 255) / soft;
        if (l > 255) l = 255;
        if ((uint8_t)l > best) { best = (uint8_t)l; bh = cH[k]; }
      }

      if (!best) continue;
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(bh + hueSh), false, false, 0, scale8(best, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_GLASS_KALEIDOSCOPE[] PROGMEM =
  "Ace 2-D Glass Kaleidoscope@Speed,Edge hardness,Mirrors,Chip size,Chip count,Shake on beat,Reshuffle on beat,Trails;;!;2f;sx=100,ix=180,c1=110,c2=110,c3=16,o1=1,o2=1";

// ===========================================================================
// REGISTRATION
// ===========================================================================
class CustomAudioFxUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_chladni,             _data_FX_MODE_CHLADNI);
    strip.addEffect(255, &mode_beat_mandala,        _data_FX_MODE_BEAT_MANDALA);
    strip.addEffect(255, &mode_spectral_rd,         _data_FX_MODE_SPECTRAL_RD);
    strip.addEffect(255, &mode_harmonic_lissajous,  _data_FX_MODE_HARMONIC_LISSAJOUS);
    strip.addEffect(255, &mode_kaleidoscope,        _data_FX_MODE_KALEIDOSCOPE);
    strip.addEffect(255, &mode_hex_quilt,           _data_FX_MODE_HEX_QUILT);
    strip.addEffect(255, &mode_moire_rosette,       _data_FX_MODE_MOIRE_ROSETTE);
    strip.addEffect(255, &mode_spectral_wormhole,   _data_FX_MODE_SPECTRAL_WORMHOLE);
    strip.addEffect(255, &mode_feedback_echo,       _data_FX_MODE_FEEDBACK_ECHO);
    strip.addEffect(255, &mode_glass_kaleidoscope, _data_FX_MODE_GLASS_KALEIDOSCOPE);
  }
  void loop() override {}
};

static CustomAudioFxUsermod custom_audio_fx;
REGISTER_USERMOD(custom_audio_fx);
