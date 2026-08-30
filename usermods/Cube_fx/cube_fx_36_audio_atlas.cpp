#include "wled.h"
#include "cube_fx_audio.h"
#include "cube_fx_imu.h"

// ===========================================================================
// 36. ACE 3-D AUDIO ATLAS
// ===========================================================================
// Every analyzer this codebase has, on one cube, at once - and still readable.
// Audio Scope is the diagnostic that proves the analyzers work; this is the
// thing you actually leave running.
//
// ---------------------------------------------------------------------------
// THE ONE IDEA: THE CUBE IS A GLOBE, AND FREQUENCY IS LATITUDE
// ---------------------------------------------------------------------------
// Every pixel is projected onto the unit sphere, and its latitude - measured
// against WORLD up, from the IMU - selects a frequency band. Bass at the
// bottom rim, mids around the equator, treble capping the top. So:
//
//   - the whole surface is one continuous instrument. No face is a leftover,
//     no seam needs handling, and the bottom rim / equator / top cap each own
//     a musically distinct job.
//   - because up comes from the sensor, the spectrum is level in the ROOM.
//     Turn the cube and the bands stay put while the cube slides through them.
//     Kick still pools at the floor when the cube is balanced on a corner.
//
// The sphere projection is not decoration, it is what makes the top face work.
// Take latitude from a flat height instead and the entire top face sits at one
// height - 20% of the lit pixels showing one flat value. Projected, the top
// cap spans latitudes 191..254, which is bands 12-15: the treble cap, where
// the hats already live. Verified by simulation, not by eye.
//
// ---------------------------------------------------------------------------
// ONE MUSICAL THING PER VISUAL CHANNEL
// ---------------------------------------------------------------------------
// This is the whole readability contract. Nothing shares a channel, so nothing
// has to be untangled from anything else:
//
//   POSITION (latitude)  frequency. Fixed, never animated.
//   COLOUR               latitude straight into the palette, so hue IS pitch
//                        and the mapping never moves under you.
//   RING THICKNESS       cfx_smoothSpec() level. A loud band physically
//                        swells; a quiet one is a thin line. Level is legible
//                        without reading brightness.
//   A LONE BRIGHT LINE   cfx_brightness() - the spectral centroid, riding up
//                        on hats and sinking on sub bass. One object, one
//                        number, unmistakable.
//   RISING WAVES         fx_lowBeat()/cfx_tempo().hit - kicks leave the floor
//                        and travel up. imu.shake enters here too: st.shake
//                        was shaped to drop into a beat slot, so it does.
//   FALLING WAVES        cfx_drop().hit - the slam rains down from the sky and
//                        meets the kicks coming up.
//   AZIMUTH (searchlight) cfx_bar() - a lobe that laps the cube once per bar
//                        and flares on each beat, brightest on the downbeat.
//                        World-locked, so it points at a fixed corner of the
//                        room while the cube turns.
//   SPARKLE              cfx_hiHit() and cfx_roll().level, weighted toward the
//                        top cap, so hats land where treble already lives.
//   A BAND AT THE MIDS   cfx_midHit() - snares flare the equator.
//   WHOLE-FIELD SQUEEZE  cfx_drop().build - a riser visibly gathers the whole
//                        spectrum toward the equator and dims it.
//   WHOLE-FIELD FLASH    cfx_drop().hit and cfx_silence().wake.
//   TIME ITSELF          cfx_drop().speedScale via cfx_dropDt(), so a
//                        sustained bass hold speeds everything up and a riser
//                        drags it back.
//   IDLE                 cfx_silence().level cross-fades the column to a slow
//                        breathing gradient rather than dropping to black.
//
// cfx_bar() is used the way its own header says it should be: as a stable
// four-cycle, not as a claim about beat one. The searchlight completing a lap
// per bar is true regardless of where the drummer counts one, and the lap is
// phase-consistent with itself, which is the part that actually reads.
//
// ---------------------------------------------------------------------------
// WHY THIS IS CHEAP DESPITE DOING ALL OF THAT
// ---------------------------------------------------------------------------
// Latitude is one dimension. So the spectrum, the ring swell, the needle, the
// waves, the snare flare, the flash and the idle fade are ALL functions of a
// single 0..255 index - built once per frame into a 256-entry table and then
// read, not recomputed, at every pixel. Same for the searchlight lobe. Per
// pixel the work is two dot products and two table lookups; the sphere
// projection is baked into the position LUT at build time.
//
// A cube of this size spends more time in color_from_palette() than in all of
// the above put together.
//
// ---------------------------------------------------------------------------
// PARAMETERS - all five do something structural
// ---------------------------------------------------------------------------
//   Speed       wave travel and the searchlight's free-run lap time
//   Glow        resting level and how hard volume drives brightness
//   Band width  THE readability knob. Low: thin separated contour rings, the
//               spectrum as a stack of lines. High: bands overlap into one
//               continuous glowing wash.
//   Impact      size of every transient - kick waves, drop flash, snare flare,
//               shake. At 0 the cube is a pure spectrum with no events at all,
//               which is the cleanest possible read.
//   Sparkle     hat/roll shimmer density.
//   World lock  use the IMU. Off (or no sensor) falls back to the cube's own
//               +Z, and everything still works - it just tumbles with the cube
//               instead of staying level in the room.
//   Bar beacon  the searchlight. Off leaves the spectrum shell alone.
//
// WITH NO SENSOR nothing needs an #ifdef: cfx_imu().valid stays false, up
// becomes cube +Z, and the atlas is a top-up spectrum globe. On a flat panel
// latitude becomes screen height (a vertical spectrum) and the searchlight
// becomes a radar sweep. -D CFX_IMU_SIM=1 to bench the motion side on a panel.
// ===========================================================================

#define AA_WAVES  4
#define AA_BANDS  16

// Lift a palette colour toward white by amt/255. Local on purpose - one effect
// uses it, so per the README it stays here rather than growing a shared header.
static inline uint32_t aa_white(uint32_t c, uint8_t amt) {
  const int r = (int)((c >> 16) & 0xFF), g = (int)((c >> 8) & 0xFF), b = (int)(c & 0xFF);
  return RGBW32((uint8_t)(r + (((255 - r) * (int)amt) >> 8)),
                (uint8_t)(g + (((255 - g) * (int)amt) >> 8)),
                (uint8_t)(b + (((255 - b) * (int)amt) >> 8)), 0);
}

static FX_RET mode_audio_atlas() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(16 + 3 * n + 16 + 768 + 16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  int16_t *wv    = (int16_t *)SEGENV.data;     // wave positions, Q4, in latitude units
  int8_t  *sx    = (int8_t *)(wv + 8);         // unit surface direction, -127..127
  int8_t  *sy    = sx + n;
  int8_t  *sz    = sy + n;
  uint8_t *spec  = (uint8_t *)(sz + n);        // 16 smoothed bands
  uint8_t *lumOf = spec + 16;                  // 256: everything that depends on latitude
  uint8_t *whtOf = lumOf + 256;                // 256: its whiteness
  uint8_t *bcnOf = whtOf + 256;                // 256: searchlight lobe by facing
  uint8_t *st    = bcnOf + 256;                // 16 housekeeping, mapped below

  // st[0] geometry marker   st[1..2] frame clock      st[3]  white flash
  // st[4..7] wave strengths st[8] snare flare         st[9]  shimmer
  // st[10] wave directions  st[11] beat flare         st[12] wave round-robin

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  const uint8_t want = (uint8_t)(cube ? 1 : 2);
  if (SEGENV.call == 0 || st[0] != want) {
    cfx_buildCube(sx, sy, sz, nullptr, nullptr, cols, rows, cube);
    if (cube) {
      // Project onto the unit sphere ONCE, here, so the per-pixel path never
      // divides. After this a dot product with any unit vector is directly the
      // cosine of the angle between them - which is what latitude needs.
      for (size_t k = 0; k < n; k++) {
        const float X = (float)sx[k], Y = (float)sy[k], Z = (float)sz[k];
        float len = sqrtf(X * X + Y * Y + Z * Z);
        if (len < 1.0f) len = 1.0f;
        sx[k] = cfx_clamp8(X / len);
        sy[k] = cfx_clamp8(Y / len);
        sz[k] = cfx_clamp8(Z / len);
      }
    }
    // A flat panel keeps the raw plane coordinates: normalising a disc would
    // push every pixel onto the rim and collapse the whole spectrum into it.
    for (int k = 0; k < 16; k++) { spec[k] = 0; st[k] = 0; }
    for (int k = 0; k < 8; k++) wv[k] = 0;
    st[0] = want;
    SEGENV.aux0 = 0;
  }

  // --- every analyzer, once -------------------------------------------------
  // cfx_tempo() first: drop, bar and roll all take its state. Each carries its
  // own per-frame guard, so the whole set costs one pass no matter who asks.
  um_data_t             *um   = cfx_getAudioData();
  const uint8_t         *fft  = (uint8_t *)um->u_data[2];
  const float            vol  = *(float *)um->u_data[0];
  const CfxTempoState   &tm   = cfx_tempo(um);
  const CfxDropState    &dr   = cfx_drop(um, tm);
  const CfxBarState     &br   = cfx_bar(tm);
  const CfxSilenceState &sl   = cfx_silence(um);
  const CfxRollState    &rl   = cfx_roll(um, tm);
  const uint8_t          hiV  = cfx_hiHit(um);
  const uint8_t          mdV  = cfx_midHit(um);
  const uint8_t          obV  = cfx_offbeat(tm);
  const uint8_t          tone = cfx_brightness(um);
  cfx_smoothSpec(spec, fft, 3);

  // --- which way is up, and which two axes the searchlight turns in ---------
  // cfx_imu() is only called inside the World lock branch: asking stamps the
  // driver's back-pressure clock and holds the sensor at full poll rate, which
  // is pure loss for a run of this effect that never wanted motion.
  int U[3], P[3], Q[3];
  uint8_t shake = 0, handled = 0;
  bool locked = false;

  if (SEGMENT.check1) {
    const CfxImuState &imu = cfx_imu();
    if (imu.valid) {
      locked  = true;
      shake   = imu.shake;
      handled = (imu.jolt > imu.motion) ? imu.jolt : imu.motion;
      if (cube) {
        U[0] = imu.ux; U[1] = imu.uy; U[2] = imu.uz;
        for (int k = 0; k < 3; k++) { P[k] = imu.basis[k]; Q[k] = imu.basis[3 + k]; }
      } else {
        // Panel with a sensor behind it: assume it stands upright with cube +X
        // to screen right and +Z to screen up. Face-up on a bench there is no
        // in-plane gravity at all, so fall back to screen-down.
        int ax = imu.ux, az = imu.uz;
        int m  = (int)sqrtf((float)(ax * ax + az * az));
        if (m < 20) { ax = 0; az = 127; m = 127; }
        U[0] = (ax * 127) / m; U[1] = (az * 127) / m; U[2] = 0;
        P[0] = U[1]; P[1] = -U[0]; P[2] = 0;
        Q[0] = U[0]; Q[1] =  U[1]; Q[2] = 0;
      }
    }
  }
  if (!locked) {
    U[0] = 0; U[1] = (int)(cube ? 0 : 127); U[2] = (int)(cube ? 127 : 0);
    P[0] = 127; P[1] = 0;   P[2] = 0;
    Q[0] = 0;   Q[1] = 127; Q[2] = 0;
  }

  // Latitude -127..127 -> table index 0..255. On a cube the reachable range is
  // -90 (bottom EDGE midpoints, sin -0.707) to +127 (top pole): the missing
  // bottom face is the only thing not covered, and 300/256 stretches what IS
  // reachable across the whole table instead of wasting a quarter of it.
  const int tBias = cube ? 90  : 128;
  const int tMul  = cube ? 300 : 256;

  const uint16_t dtRaw = fx_dt8(st + 1);
  const uint16_t dtMs  = cfx_dropDt(dtRaw, dr.speedScale);   // surge/build own time

  const int spd    = SEGMENT.speed;
  const int glow   = SEGMENT.intensity;
  const int c1     = SEGMENT.custom1;
  const int impact = SEGMENT.custom2;
  const int spark  = SEGMENT.custom3;

  // --- event envelopes ------------------------------------------------------
  uint8_t knock = tm.hit;
  if (shake > knock) knock = shake;              // a shove is a kick

  {                                               // whole-cube white flash
    int f = st[3];
    const int dropF = ((int)dr.hit * (96 + impact)) >> 8;
    if (dropF > f) f = dropF;
    if (sl.wake && f < 200) f = 200;              // the moment a track starts
    if ((handled >> 2) > f) f = handled >> 2;     // being picked up glows a little
    f -= (int)fx_fade(10, dtMs);
    st[3] = (uint8_t)((f < 0) ? 0 : f);
  }
  {                                               // snare / clap
    int s = (int)st[8];
    if (mdV > s) s = mdV;
    s -= (int)fx_fade(26, dtMs);
    st[8] = (uint8_t)((s < 0) ? 0 : s);
  }
  {                                               // hats, held up by a roll
    int s = (int)st[9];
    if (hiV > s) s = hiV;
    if (rl.level > s) s = rl.level;
    s -= (int)fx_fade(22, dtMs);
    st[9] = (uint8_t)((s < 0) ? 0 : s);
  }
  {                                               // beat flare on the searchlight
    int s = (int)st[11];
    if (br.downbeat)   s = 255;
    else if (tm.beat) { if (s < 130) s = 130; }
    else if (obV)     { if (s < 55)  s = 55;  }
    s -= (int)fx_fade(16, dtMs);
    st[11] = (uint8_t)((s < 0) ? 0 : s);
  }

  // --- waves: kicks climb from the floor, drops rain from the sky -----------
  {
    const int adv  = (int)fx_step(90 + spd, dtMs);       // Q4 latitude units
    uint8_t   dirs = st[10];
    for (int r = 0; r < AA_WAVES; r++) {
      if (!st[4 + r]) continue;
      int p = (int)wv[r] + (((dirs >> r) & 1) ? -adv : adv);
      if (p >  8000) p =  8000;
      if (p < -4000) p = -4000;
      wv[r] = (int16_t)p;
      int s = (int)st[4 + r] - (int)fx_step(7, dtMs);
      st[4 + r] = (uint8_t)((s < 0) ? 0 : s);
      const int at = p >> 4;
      if (at > 300 || at < -48) st[4 + r] = 0;           // left the surface
    }
    if (impact) {
      if (knock) {                                       // enters below the floor
        int slot = 0;
        for (int r = 1; r < AA_WAVES; r++) if (st[4 + r] < st[4 + slot]) slot = r;
        wv[slot] = -320;
        st[4 + slot] = knock;
        dirs = (uint8_t)(dirs & ~(1 << slot));
      }
      if (dr.hit) {                                      // enters above the pole
        int slot = 0;
        for (int r = 1; r < AA_WAVES; r++) if (st[4 + r] < st[4 + slot]) slot = r;
        wv[slot] = (int16_t)(255 * 16 + 320);
        st[4 + slot] = dr.hit;
        dirs = (uint8_t)(dirs | (1 << slot));
      }
    }
    st[10] = dirs;
  }

  // --- searchlight heading --------------------------------------------------
  // Free-running, pulled toward the bar phase by however much cfx_bar() trusts
  // its own alignment. At full confidence it IS the bar phase; with none it
  // just turns at Speed. Blending on the shortest arc means it leans into lock
  // instead of snapping when confidence comes and goes.
  SEGENV.aux0 = (uint16_t)(SEGENV.aux0 + (uint16_t)fx_step(40 + (spd << 1), dtMs));
  const uint8_t freeA = (uint8_t)(SEGENV.aux0 >> 8);
  int arc = (int)br.phase - (int)freeA;
  if (arc > 127) arc -= 256; else if (arc < -128) arc += 256;
  const uint8_t beam = (uint8_t)((int)freeA + ((arc * (int)br.confidence) >> 8));

  const int cs = (int)cos8_t(beam) - 128;
  const int sn = (int)sin8_t(beam) - 128;
  int D[3];
  for (int k = 0; k < 3; k++) D[k] = (P[k] * cs + Q[k] * sn) >> 7;

  const int beamAmt = SEGMENT.check2
    ? (70 + (glow >> 2) + (((int)st[11] * (110 + (impact >> 1))) >> 8)) : 0;
  if (beamAmt) {
    for (int k = 0; k < 256; k++) {
      int v = 0;
      if (k > 128) {
        v = ((k - 128) * 255) >> 7;      // 0..254 across the facing hemisphere
        v = (v * v) >> 8;                // squared: a lobe, not a whole side
        v = (v * beamAmt) >> 8;
        if (v > 255) v = 255;
      }
      bcnOf[k] = (uint8_t)v;
    }
  }

  // --- band centres and thicknesses ----------------------------------------
  // A riser pinches every centre toward the equator, so the whole spectrum
  // visibly gathers while the bass is away and springs back when it lands.
  // Bands are lifted with rising index (the usual pink-noise correction):
  // without it the treble cap - a fifth of the lit pixels - is dark on most
  // material, which wastes the best-placed face on the cube.
  const int comp = 256 - (((int)dr.build * 72) / 255);
  int cb[AA_BANDS], bw[AA_BANDS], lv[AA_BANDS];
  for (int b = 0; b < AA_BANDS; b++) {
    cb[b] = 128 + (((b * 16 + 8 - 128) * comp) >> 8);
    int v = ((int)spec[b] * (256 + b * 10)) >> 8;
    lv[b] = (v > 255) ? 255 : v;
    bw[b] = 5 + (c1 >> 5) + ((lv[b] * (18 + (c1 >> 2))) >> 8);
  }
  const int toneT = 128 + ((((int)tone - 128) * comp) >> 8);
  const int snT   = cb[6];

  // --- the latitude table: one pass, then every pixel just reads it ---------
  const int base    = 5 + (glow >> 5);
  const int dim     = 255 - ((int)dr.build / 3);
  const int waveW   = 10 + (impact >> 3);
  const int needleW = 4 + (c1 >> 6);
  const int snW     = 8 + (impact >> 4);
  const int flash   = st[3];
  const int idleP   = (int)(uint8_t)(strip.now >> 6);

  for (int t = 0; t < 256; t++) {
    int lum = base, wht = 0;

    // MAX across bands, never a sum: overlapping bands brighten each other
    // into a single blob under a sum, and the stack stops being countable.
    int body = 0;
    for (int b = 0; b < AA_BANDS; b++) {
      int d = t - cb[b];
      if (d < 0) d = -d;
      if (d >= bw[b]) continue;
      const int v = (lv[b] * (bw[b] - d)) / bw[b];
      if (v > body) body = v;
    }
    lum += (body * dim) >> 8;

    {                                                    // spectral centroid
      int d = t - toneT;
      if (d < 0) d = -d;
      if (d < needleW) {
        const int g = ((needleW - d) * 255) / needleW;
        lum += (g * 3) >> 3;
        wht += g >> 1;
      }
    }

    for (int r = 0; r < AA_WAVES; r++) {                 // kicks and drops
      const int s = st[4 + r];
      if (!s) continue;
      int d = t - ((int)wv[r] >> 4);
      if (d < 0) d = -d;
      if (d >= waveW) continue;
      const int g = ((waveW - d) * s) / waveW;
      lum += (g * impact) >> 8;
      wht += (g * impact) >> 10;
    }

    if (st[8]) {                                         // snare at the equator
      int d = t - snT;
      if (d < 0) d = -d;
      if (d < snW) {
        const int g = ((snW - d) * (int)st[8]) / snW;
        lum += (g * (64 + impact)) >> 9;
        wht += (g * (64 + impact)) >> 10;
      }
    }

    if (sl.level) {                                      // dead air: breathe
      const int idle = 10 + ((int)sin8_t((uint8_t)((t >> 1) - idleP)) >> 2);
      lum += ((idle - lum) * (int)sl.level) / 255;
    }

    lum += flash >> 1;
    wht += flash >> 1;

    lumOf[t] = (uint8_t)((lum < 0) ? 0 : ((lum > 255) ? 255 : lum));
    whtOf[t] = (uint8_t)((wht < 0) ? 0 : ((wht > 255) ? 255 : wht));
  }

  // --- draw -----------------------------------------------------------------
  const uint8_t  drive = cfx_drive(vol, 1.0f, 110 + (glow >> 1));
  const uint32_t seed  = (uint32_t)strip.now * 2246822519u;
  const int      shim  = (spark * (int)st[9]) >> 8;
  const int      accW  = 190 + ((int)rl.accel >> 2);      // an accelerating fill runs white

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int px = sx[i], py = sy[i], pz = sz[i];

      const int hn = ((int)U[0] * px + (int)U[1] * py + (int)U[2] * pz) >> 7;
      int t = ((hn + tBias) * tMul) >> 8;
      if (t < 0) t = 0; else if (t > 255) t = 255;

      int lum = lumOf[t];
      int wht = whtOf[t];

      if (beamAmt) {
        // Mostly multiplicative: the searchlight brightens what is already
        // there rather than painting over it, so the spectrum stays legible
        // through it. The small additive term is only so the beam is still
        // findable across a silent stretch.
        const int s = ((int)D[0] * px + (int)D[1] * py + (int)D[2] * pz) >> 7;
        int k = 128 + s;
        if (k < 0) k = 0; else if (k > 255) k = 255;
        const int bc = bcnOf[k];
        if (bc) { lum += ((lum * bc) >> 8) + (bc >> 3); wht += bc >> 4; }
      }

      if (shim) {
        // Weighted toward the top cap, where the treble bands already are.
        const uint8_t h8 = (uint8_t)((((uint32_t)i * 2654435761u) ^ seed) >> 24);
        if ((int)h8 < ((shim * (40 + t)) >> 11)) { lum += 150; wht = accW; }
      }

      if (lum <= 0) { SEGMENT.setPixelColorXY(x, y, 0u); continue; }
      if (lum > 255) lum = 255;
      if (wht > 255) wht = 255;

      uint32_t c = SEGMENT.color_from_palette((uint8_t)(14 + ((t * 227) >> 8)),
                                              false, false, 0);
      if (wht) c = aa_white(c, (uint8_t)wht);
      SEGMENT.setPixelColorXY(x, y, mq_scale(c, scale8((uint8_t)lum, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_AUDIO_ATLAS[] PROGMEM =
  "Ace 3-D Audio Atlas@Speed,Glow,Band width,Impact,Sparkle,World lock,Bar beacon,Flat mode;;!;2f;sx=140,ix=150,c1=110,c2=160,c3=130,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_AudioAtlasUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_audio_atlas, _data_FX_MODE_AUDIO_ATLAS);
  }
  void loop() override {}
};

static CubeFx_AudioAtlasUsermod cube_fx_audio_atlas;
REGISTER_USERMOD(cube_fx_audio_atlas);
