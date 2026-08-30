#pragma once

// ===========================================================================
// cube_fx_common.h - shared helpers for the "Cube" family of LED effects
// ===========================================================================
// Included by every cube_fx_*.cpp file in this folder. Anything used by two
// or more effects lives here so it is defined once; anything used by exactly
// one effect stays local to that effect's own .cpp file instead. If you are
// adding a brand-new effect, you almost never need to edit this file - just
// #include it and use what you need from it.
//
// See README.md in this folder for the full picture (net layout, how to add
// a new effect, why the split works this way).
// ===========================================================================

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
 * Cube-native audio-reactive effects for WLED (0.15 / 16.x / 17-dev)
 * ===========================================================================
 *
 *   Cube Axes        calibration - XYZ as RGB, verify the net mapping
 *   Cube Noise       3D noise tumbling through the cube surface
 *   Cube Ripples     beat-spawned spherical wavefronts crossing every edge
 *   Spectral Globe   latitude = frequency, azimuth folded into petals
 *   Cube Slice       tumbling plane of spectrum sweeping through the solid
 *   Cube Edges       edges lined, beat pulses edge<->centre, wave along edges
 *   Cube Frame       free-floating wireframe you can spin out of alignment
 *   Cube Chladni     nodal surface of a 3D box mode, cut by the cube's faces
 *   Cube Bloom       lobed shells, each shape fixed from the FFT at the beat
 *   Rubiks Cube      real 3x3 puzzle: scramble, then unwind on the beat
 *   Cube Plate       two-mode Chladni - broader, calmer sibling of Cube Chladni
 *   Cube Cell        Waving Cell's nested sines, evaluated on the 3D surface
 *   Cube Wire        edges only, beat pulses running ALONG them with trails
 *   Question Block   ? block -> smash -> item roulette -> the item's own show
 *   Simon / Invaders / Snake   games on the five faces and the wall cylinder
 *
 * The idea: every pixel gets a 3D position on the cube's surface. Effects are
 * then functions of (X,Y,Z), so they are continuous across folds for free -
 * no seam handling anywhere in the effect code.
 *
 * Net assumed (3x3 blocks of B x B, corners are gaps):
 *
 *          +--------+
 *          | NORTH  |
 *     +----+--------+----+
 *     |WEST|  TOP   |EAST|
 *     +----+--------+----+
 *          | SOUTH  |
 *          +--------+
 *
 * Cube axes: +X east, +Y north, +Z up. Top face at Z = +1.
 * Each arm folds upward, so the edge touching TOP is that wall's top edge.
 *
 * On any other matrix the LUT falls back to a flat plane at Z = 0 and every
 * effect degrades sensibly (ripples become circles, the globe becomes a polar
 * mandala, the slice becomes sweeping bands).
 *
 * Cube detection is automatic (square, divisible by 3); the "Flat mode"
 * checkbox forces the plane if you ever run a square flat panel.
 *
 * Drop next to user_fx_custom.cpp in the same usermod folder - one
 * library.json covers every .cpp in the directory. The few static helpers are
 * duplicated from that file on purpose: keeping this separate means the nine
 * working effects don't get rewritten every time we iterate on cube geometry.
 * ===========================================================================
 */

// ---------------------------------------------------------------------------
// shared helpers (static: private to this file, may duplicate user_fx_custom)
// ---------------------------------------------------------------------------
static um_data_t *cfx_getAudioData() {
  um_data_t *um_data;
  if (!UsermodManager::getUMData(&um_data, USERMOD_ID_AUDIOREACTIVE)) {
    um_data = simulateSound(SEGMENT.soundSim);
  }
  return um_data;
}

static inline void cfx_bands(const uint8_t *fft, int &bass, int &mid, int &treb) {
  bass = (fft[0] + fft[1] + fft[2]) / 3;
  mid  = (fft[5] + fft[6] + fft[7] + fft[8]) / 4;
  treb = (fft[11] + fft[12] + fft[14] + fft[15]) / 4;
}

static inline uint8_t cfx_drive(float vol, float gain, int floorV) {
  int32_t d = (int32_t)(vol * gain) + floorV;
  if (d < 0) d = 0;
  return (uint8_t)((d > 255) ? 255 : d);
}

static void cfx_smoothSpec(uint8_t *spec, const uint8_t *fft, uint8_t sm) {
  if (sm < 1) sm = 1;
  for (int i = 0; i < 16; i++) {
    int cur = (int)spec[i];
    cur += ((int)fft[i] - cur) / (int)sm;
    if ((int)fft[i] > cur) cur = fft[i];      // fast attack, slow release
    spec[i] = (uint8_t)cur;
  }
}

static inline int8_t cfx_clamp8(float v) {
  const int r = (int)(v * 127.0f);
  return (int8_t)((r < -127) ? -127 : (r > 127 ? 127 : r));
}

// True when this segment looks like the cube's unfolded net.
static inline bool cfx_isCube(int cols, int rows) {
  return !SEGMENT.check3 && cols == rows && cols >= 12 && (cols % 3) == 0;
}

// Surface position of one pixel. If a face on your cube comes out rotated or
// mirrored, only the five face lines below need changing - every effect in
// this file reads through here.
static inline void cfx_pos(int x, int y, int cols, int rows, int B, bool cubeNet,
                           float &X, float &Y, float &Z) {
  if (cubeNet) {
    const int bx = x / B, by = y / B;
    const float a = 2.0f * ((x % B) + 0.5f) / (float)B - 1.0f;   // -1..1
    const float b = 2.0f * ((y % B) + 0.5f) / (float)B - 1.0f;

    if      (bx == 1 && by == 1) { X =  a; Y = -b; Z =  1.0f; }  // TOP
    else if (bx == 1 && by == 0) { X =  a; Y =  1.0f; Z =  b; }  // NORTH
    else if (bx == 1 && by == 2) { X =  a; Y = -1.0f; Z = -b; }  // SOUTH
    else if (bx == 0 && by == 1) { X = -1.0f; Y = -b; Z =  a; }  // WEST
    else if (bx == 2 && by == 1) { X =  1.0f; Y = -b; Z = -a; }  // EAST
    else                         { X =  a; Y = -b; Z = -1.0f; }  // gap corners
  } else {
    X = 2.0f * (x + 0.5f) / (float)cols - 1.0f;
    Y = 1.0f - 2.0f * (y + 0.5f) / (float)rows;
    Z = 0.0f;
  }
}

// --- frame-rate independence ------------------------------------------------
// Milliseconds since the previous frame. fx_dt keeps its timestamp in the TOP
// 16 bits of a 32-bit store so the low bits stay free for the effect's flags.
static inline uint16_t fx_dt(uint32_t &store) {
  const uint16_t now = (uint16_t)strip.now;
  uint16_t dt = (uint16_t)(now - (uint16_t)(store >> 16));
  if (dt > 250) dt = 250;
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
// Calibrated at 23 ms, so motion is unchanged at ~43 fps and holds steady
// above it rather than running away with the frame rate.
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
//
// NOTE ON LINKAGE: this and the two analyzers below are `inline`, NOT
// `static inline`. A static function in a header gets its own private copy of
// its function-local statics in every .cpp that includes it - so with ~30
// effect files you'd get ~30 independent beat gates, and the "one answer per
// frame" contract would only hold within a single file. Plain `inline` gives
// one shared instance across the whole build, which is what these were always
// documented to be.
#ifndef FX_BEAT_FLOOR
  #define FX_BEAT_FLOOR 40
#endif
// How far the low band must RISE above the level it was already sitting at
// for a transient to count as a kick. A sustained bass note keeps re-arming
// samplePeak every few frames without ever rising, and every one of those
// used to come through as a full-strength beat - which is what made tempo
// lock run away and beat-driven effects machine-gun under a held note.
// Lower this if kicks feel like they're being missed on dense mixes.
#ifndef FX_BEAT_RISE
  #define FX_BEAT_RISE 38
#endif
inline uint8_t fx_lowBeat(um_data_t *um) {
  static uint32_t seenFrame = 0xFFFFFFFFu;
  static uint8_t  cached    = 0;
  static uint8_t  prevPeak  = 0;
  static uint8_t  loFloor   = 0;               // where the low band already sits
  static uint32_t dtStore   = 0;
  if (strip.now == seenFrame) return cached;   // one answer per frame
  seenFrame = strip.now;
  cached = 0;

  const uint16_t dt  = fx_dt(dtStore);
  const uint8_t *fft = (uint8_t *)um->u_data[2];
  const int lo = (fft[0] + fft[1] + fft[2]) / 3;

  // Two-sided ~260 ms slew, updated every frame - this is the "floor" a kick
  // has to clear, not a peak follower. Evaluated against the PREVIOUS frame's
  // value so the kick's own spike can't raise its own bar.
  const int floorNow = (int)loFloor;
  {
    int e = floorNow + (((lo - floorNow) * (int)dt) / 260);
    if (e < 0) e = 0; else if (e > 255) e = 255;
    loFloor = (uint8_t)e;
  }

  const uint8_t peak = *(uint8_t *)um->u_data[3];
  const bool rising = peak && !prevPeak;       // the flag stays set for frames
  prevPeak = peak ? 1 : 0;
  if (!rising) return 0;

  int hi = 0;
  for (int k = 7; k < 16; k++) hi += fft[k];
  hi /= 9;
  if (lo < FX_BEAT_FLOOR || lo <= hi) return 0;
  if (lo < floorNow + FX_BEAT_RISE) return 0;  // not a transient, just a held note
  cached = (uint8_t)lo;                        // strength, not a flag
  return cached;
}

// --- band envelopes ----------------------------------------------------
// Frame-rate-independent fast-attack / slow-release follower. Release is
// expressed as a time constant in ms so the envelope behaves the same at
// 20 fps as at 100 fps - the old `env -= (env-x)>>3` form released ~3x
// faster on the panel than on the cube purely because of frame rate.
inline uint8_t fx_env(uint8_t prev, uint8_t now, uint16_t dtMs, uint16_t releaseMs) {
  if (now >= prev) return now;                       // instant attack
  if (releaseMs < 1) releaseMs = 1;
  int32_t drop = ((int32_t)(prev - now) * (int32_t)dtMs) / (int32_t)releaseMs;
  if (drop < 1) drop = 1;
  const int32_t v = (int32_t)prev - drop;
  return (uint8_t)((v < 0) ? 0 : v);
}

// --- tempo analyzer ----------------------------------------------------
// Turns fx_lowBeat()'s one-shot hits into a continuous, predictive tempo:
// a free-running phase that ticks at the current estimated beat period (so
// animation stays smooth between hits and survives a missed one), pulled
// gently toward each confirmed hit, plus a BPM estimate and a confidence
// value effects use to fade beat-locked motion back to idle when the bass
// drops out rather than firing on a stale prediction.
//
// One shared lock across the whole build - there is only one song playing,
// and the per-frame guard means several segments can all read it cheaply.
//
// `beat` is the one thing most effects actually want: a single-frame flag
// that fires when the PREDICTED beat lands. Before, every effect had to
// stash last frame's phase and test for a wrap itself, which mis-fires every
// time the PLL nudges the phase backwards. Read st.beat instead.
struct CfxTempoState {
  uint16_t periodMs;    // estimated beat period, ms (0 = not locked yet)
  uint8_t  phase;       // 0..255 sawtooth, wraps once per predicted beat
  uint8_t  confidence;  // 0..255 - trust in phase/bpm right now
  uint16_t bpm;         // derived display value, 0 while unlocked
  uint8_t  beat;        // 255 on the single frame the predicted beat lands
  uint8_t  hit;         // fx_lowBeat() strength this frame, 0 if none
};

#ifndef FX_TEMPO_MIN_MS
  #define FX_TEMPO_MIN_MS 300   // 200 BPM ceiling
#endif
#ifndef FX_TEMPO_MAX_MS
  #define FX_TEMPO_MAX_MS 1500  // 40 BPM floor
#endif
// A locked tempo ignores hits landing sooner than this fraction of a period
// after the last accepted beat. Without it a SUSTAINED bass note - which
// re-triggers samplePeak over and over - feeds a stream of tiny intervals
// into the estimator and octave-correction happily doubles them into
// "plausible" ones, so the whole lock accelerates for as long as the note
// holds. That runaway is what used to make beat-locked effects visibly speed
// up under a held bass note. It is now an explicit, tunable behaviour in
// cfx_drop().surge instead of an accident in here.
#ifndef FX_TEMPO_REFRACT_PCT
  #define FX_TEMPO_REFRACT_PCT 60
#endif

inline CfxTempoState &cfx_tempo(um_data_t *um) {
  static CfxTempoState st         = {0, 0, 0, 0, 0, 0};
  static uint32_t       lastBeatMs = 0;
  static bool            haveLast  = false;
  static uint32_t         dtStore  = 0;
  static uint16_t          phaseQ8 = 0;              // phase in 8.8 fixed point
  static uint32_t           seenFrame = 0xFFFFFFFFu;

  if (strip.now == seenFrame) return st;             // one update per frame
  seenFrame = strip.now;

  const uint16_t dtMs = fx_dt(dtStore);
  const uint8_t  hit  = fx_lowBeat(um);
  const uint32_t now  = strip.now;
  st.hit  = hit;
  st.beat = 0;

  // Predict: advance phase every frame regardless of whether a hit arrived.
  // Kept in 8.8 fixed point - the old integer step truncated ~0.8 of a phase
  // unit per frame at 43 fps, which is a ~7% slow drift the PLL then had to
  // fight on every single beat.
  if (st.periodMs > 0) {
    uint32_t adv = ((uint32_t)dtMs * 65536u) / (uint32_t)st.periodMs;
    if (adv > 0xFFFFu) adv = 0xFFFFu;
    const uint32_t np = (uint32_t)phaseQ8 + adv;
    if (np > 0xFFFFu) st.beat = 255;                 // predicted beat lands now
    phaseQ8  = (uint16_t)np;
    st.phase = (uint8_t)(phaseQ8 >> 8);
  }

  // Refractory: once locked, a transient arriving far too soon after the last
  // accepted beat is not a beat. Drop it entirely - don't even restart the
  // interval clock with it.
  const bool refractory = haveLast && st.periodMs > 0 && st.confidence >= 64 &&
    (now - lastBeatMs) < ((uint32_t)st.periodMs * FX_TEMPO_REFRACT_PCT) / 100;

  if (hit && !refractory) {
    if (haveLast) {
      uint32_t interval = now - lastBeatMs;

      // Octave-correct against the current estimate by the NEAREST whole
      // multiple, so one missed beat (2x), two missed beats (3x) and a
      // double-time leak (1/2x) all fold back correctly. The old code only
      // ever halved or doubled once, so a 3x gap folded to 1.5x and dragged
      // the lock somewhere between the two.
      if (st.periodMs > 0 && interval > 0) {
        const uint32_t p = st.periodMs;
        if (interval >= p) {
          const uint32_t m = (interval + p / 2) / p;
          if (m >= 2 && interval / m >= FX_TEMPO_MIN_MS) interval /= m;
        } else {
          const uint32_t k = (p + interval / 2) / interval;
          if (k >= 2 && interval * k <= FX_TEMPO_MAX_MS) interval *= k;
        }
      }

      if (interval >= FX_TEMPO_MIN_MS && interval <= FX_TEMPO_MAX_MS) {
        // Accept freely while acquiring lock; once locked, reject hits that
        // don't land near the current estimate (an off-beat snare/hat leak)
        // instead of letting them drag tempo around.
        const bool plausible = (st.confidence < 64) || (st.periodMs == 0) ||
          (interval > (uint32_t)st.periodMs * 3 / 4 && interval < (uint32_t)st.periodMs * 4 / 3);

        if (plausible) {
          const int sm = (st.confidence < 64) ? 2 : 6;   // fast acquire, slow settle
          st.periodMs = (st.periodMs == 0) ? (uint16_t)interval
            : (uint16_t)(st.periodMs + ((int32_t)interval - (int32_t)st.periodMs) / sm);

          if (st.confidence > 96) {
            // PLL nudge on the SIGNED phase error. Phase near 255 means the
            // real beat beat our prediction to it, so the fix is to wrap
            // forward, not to drag the phase back down toward zero - which
            // is exactly what the old unsigned `phase -= phase/4` did, and
            // why lock never felt tight on the back half of a cycle.
            int32_t err = (int32_t)phaseQ8;
            if (err > 32768) err -= 65536;
            const int32_t np = (int32_t)phaseQ8 - err / 4;
            if (np > 0xFFFF) st.beat = 255;
            phaseQ8 = (uint16_t)((uint32_t)np & 0xFFFFu);
          } else {
            phaseQ8 = 0;                               // snap while acquiring
            st.beat = 255;
          }
          st.phase = (uint8_t)(phaseQ8 >> 8);

          st.confidence = (uint8_t)(st.confidence + (255 - st.confidence) / 3);
        } else if (st.confidence > 20) {
          st.confidence -= 20;   // don't trust it, but let a real tempo change land
        }
      }
    }
    lastBeatMs = now;
    haveLast   = true;
  }

  // Bass likely dropped: keep ticking so a returning kick lands in phase,
  // but stop trusting the prediction. Decay is per-second, not per-frame, so
  // "how long until the scope goes dim" doesn't depend on frame rate.
  if (haveLast && st.periodMs > 0) {
    const uint32_t silence = now - lastBeatMs;
    if (silence > (uint32_t)st.periodMs * 2) {
      const int32_t d = ((int32_t)dtMs * 170) / 1000;     // ~1.5 s to fully doubt
      st.confidence = (st.confidence > d) ? (uint8_t)(st.confidence - d) : 0;
    }
    if (silence > (uint32_t)st.periodMs * 8) {   // stopped, not dropped - forget it
      st.periodMs = 0;
      haveLast    = false;
      phaseQ8     = 0;
      st.phase    = 0;
    }
  }

  st.bpm = (st.periodMs > 0) ? (uint16_t)(60000u / st.periodMs) : 0;
  return st;
}

// --- drop / build / surge helper ---------------------------------------
// A "drop" is NOT just loud bass. It is the bass hit that arrives after the
// track has taken the bass AWAY for a while and let the mids swell - the
// breakdown-then-slam shape. The old version here fired on any sustained
// bass-heavy passage, which on bass-forward music meant it was latched on
// almost permanently and never read as an event at all.
//
// Three separate outputs, because they are three separate musical things:
//
//   build   0..255, live. Rises while the bass is gone and the mids are
//           carrying the track - i.e. you are inside a riser. This is the
//           thing to hold your breath on.
//   hit     one-shot 0..255, the single frame the drop lands. Strength is
//           the product of how long the bass was gone, how much the mids
//           swelled, and how hard the returning kick is.
//   intensity 0..255, `hit` decaying over FX_DROP_DECAY_MS. Use for glow,
//           thickness, extra spawns - anything that should ring out.
//   surge   0..255. A SUSTAINED bass-heavy hold (the long note, not a kick).
//           Separate from the drop entirely, and it is what drives the
//           speed-up in speedScale.
//
// speedScale is a 8.8-style multiplier where 256 == normal speed, so it can
// express both directions: surge pushes it above 256 (faster), a build pulls
// it under (the track holding back). Feed it to cfx_dropDt().
struct CfxDropState {
  uint8_t  hit;         // one-shot: 255-scale strength on the drop frame only
  uint8_t  intensity;   // decaying tail of the last drop
  uint8_t  build;       // 0..255 - how deep into a build-up we are right now
  uint8_t  surge;       // 0..255 - sustained bass-heavy hold
  uint16_t speedScale;  // 256 = normal, >256 faster, <256 slower
  bool     active;      // intensity > 0
};

#ifndef FX_DROP_BASS_GONE
  #define FX_DROP_BASS_GONE   58    // bass envelope at/below this counts as "no bass"
#endif
#ifndef FX_DROP_BASS_BACK
  #define FX_DROP_BASS_BACK   140   // bass envelope crossing this counts as "it's back"
#endif
#ifndef FX_DROP_MID_MIN
  #define FX_DROP_MID_MIN     45    // mids must be carrying the track for it to be a build
#endif
#ifndef FX_DROP_VOID_MS
  #define FX_DROP_VOID_MS     1100  // minimum bass-free stretch before a return counts
#endif
#ifndef FX_DROP_VOID_FULL_MS
  #define FX_DROP_VOID_FULL_MS 3200 // bass-free stretch that scores a full-strength build
#endif
#ifndef FX_DROP_ARM_MS
  #define FX_DROP_ARM_MS      2200  // how long an armed build waits for its slam before fizzling
#endif
#ifndef FX_DROP_MIN_BUILD
  #define FX_DROP_MIN_BUILD   70    // build score required to call a return a drop
#endif
#ifndef FX_DROP_DECAY_MS
  #define FX_DROP_DECAY_MS    1500  // ms for intensity to ring out from 255 to 0
#endif
#ifndef FX_DROP_SURGE_ON
  #define FX_DROP_SURGE_ON    185   // bass envelope that starts the sustained-hold count
#endif
#ifndef FX_DROP_SURGE_OFF
  #define FX_DROP_SURGE_OFF   140   // lower bar that KEEPS an already-running hold alive
#endif
#ifndef FX_DROP_SURGE_MS
  #define FX_DROP_SURGE_MS    260   // continuous ms above threshold to call it a hold
#endif
#ifndef FX_DROP_SURGE_MAX_MS
  #define FX_DROP_SURGE_MAX_MS 7000 // safety: a hold can't ride forever
#endif
#ifndef FX_DROP_RAMP_MS
  #define FX_DROP_RAMP_MS     600   // ms to ease surge from 0<->255 in either direction
#endif
// Speed shaping, in percent of normal at full strength.
#ifndef FX_DROP_SURGE_PCT
  #define FX_DROP_SURGE_PCT   155   // held bass note -> up to 1.55x speed
#endif
#ifndef FX_DROP_BUILD_PCT
  #define FX_DROP_BUILD_PCT   78    // inside a riser -> down to 0.78x speed
#endif

inline CfxDropState &cfx_drop(um_data_t *um, const CfxTempoState &tempo) {
  static CfxDropState st = {0, 0, 0, 0, 256, false};
  static uint32_t   dtStore   = 0;
  static uint8_t     bassEnv  = 0;
  static uint8_t      midEnv  = 0;
  static uint8_t       midAtVoid = 0;
  static uint16_t       voidMs  = 0;
  static uint16_t        surgeMs = 0;
  static uint16_t         heldMs = 0;
  static bool              surging = false;
  static bool               armed  = false;    // a real build has been seen
  static uint8_t             armedBuild = 0;   // peak build score while armed
  static uint16_t             armedMs   = 0;   // how long we've been waiting for the slam
  static uint32_t              seenFrame = 0xFFFFFFFFu;

  if (strip.now == seenFrame) return st;        // one update per frame
  seenFrame = strip.now;

  const uint16_t dtMs = fx_dt(dtStore);
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  bass = (uint8_t)((fft[0] + fft[1] + fft[2]) / 3);
  const uint8_t  mid  = (uint8_t)((fft[5] + fft[6] + fft[7] + fft[8]) / 4);

  const uint8_t prevBass = bassEnv;
  bassEnv = fx_env(bassEnv, bass, dtMs, 220);
  midEnv  = fx_env(midEnv,  mid,  dtMs, 320);

  st.hit = 0;

  // --- the void: bass gone, mids carrying ---------------------------------
  if (bassEnv <= FX_DROP_BASS_GONE) {
    if (voidMs == 0) midAtVoid = midEnv;        // remember where the mids started
    voidMs = (uint16_t)((voidMs + dtMs > 60000) ? 60000 : voidMs + dtMs);
  } else if (bassEnv < FX_DROP_BASS_BACK) {
    // grey zone - neither properly gone nor properly back. Hold the count so
    // one stray thump mid-breakdown doesn't wipe a 3-second build.
  } else {
    voidMs = 0;
  }

  // Live build score: how long the bass has been away x how present the mids
  // are x how much they've swelled since the void started.
  {
    uint32_t vs = ((uint32_t)voidMs * 255u) / FX_DROP_VOID_FULL_MS;
    if (vs > 255) vs = 255;
    int ms = ((int)midEnv - FX_DROP_MID_MIN) * 4;
    if (ms < 0) ms = 0;
    if (ms > 255) ms = 255;
    int swell = ((int)midEnv - (int)midAtVoid) * 3;
    if (swell < 0) swell = 0;
    if (swell > 255) swell = 255;
    const uint8_t midScore = (uint8_t)qadd8((uint8_t)((ms * 3) / 4), (uint8_t)(swell / 4));
    st.build = (voidMs >= FX_DROP_VOID_MS / 3)
      ? (uint8_t)scale8((uint8_t)vs, midScore) : 0;
  }

  // Arming has to LATCH. The frame the bass comes back is the frame voidMs is
  // reset, so a live `armed = build >= threshold` test is always false exactly
  // when the drop lands. Latch the peak build instead and hold it briefly.
  if (voidMs >= FX_DROP_VOID_MS && st.build >= FX_DROP_MIN_BUILD) {
    armed = true;
    armedMs = 0;
    if (st.build > armedBuild) armedBuild = st.build;
  } else if (armed) {
    armedMs = (uint16_t)((armedMs + dtMs > 60000) ? 60000 : armedMs + dtMs);
    if (armedMs > FX_DROP_ARM_MS) { armed = false; armedBuild = 0; }   // build fizzled
  }

  // --- the return: bass slams back while a build was in progress -----------
  if (armed) {
    const bool slam = (bassEnv >= FX_DROP_BASS_BACK) && (prevBass < FX_DROP_BASS_BACK);
    const bool kick = tempo.hit >= FX_BEAT_FLOOR * 2 && bassEnv >= FX_DROP_BASS_BACK;
    if (slam || kick) {
      const uint8_t force = tempo.hit ? (uint8_t)qadd8(150, (uint8_t)(tempo.hit >> 1)) : 190;
      st.hit       = scale8(armedBuild, force);
      st.intensity = st.hit;
      voidMs       = 0;
      armed        = false;
      armedBuild   = 0;
      midAtVoid    = midEnv;
    }
  }

  // Ring-out
  if (st.intensity) {
    const int32_t d = ((int32_t)255 * dtMs) / FX_DROP_DECAY_MS + 1;
    st.intensity = (st.intensity > d) ? (uint8_t)(st.intensity - d) : 0;
  }
  st.active = st.intensity > 0;

  // --- surge: a SUSTAINED bass-heavy hold, tracked independently -----------
  const bool hot = surgeMs > 0 ? (bassEnv >= FX_DROP_SURGE_OFF) : (bassEnv >= FX_DROP_SURGE_ON);
  if (hot) surgeMs = (uint16_t)((surgeMs + dtMs > 60000) ? 60000 : surgeMs + dtMs);
  else     surgeMs = 0;

  if (!surging && surgeMs >= FX_DROP_SURGE_MS) { surging = true; heldMs = 0; }
  if (surging) {
    heldMs = (uint16_t)((heldMs + dtMs > 60000) ? 60000 : heldMs + dtMs);
    if (!hot || heldMs >= FX_DROP_SURGE_MAX_MS) surging = false;
  }

  {
    const uint8_t target = surging ? 255 : 0;
    const int delta = (int)(((uint32_t)255 * dtMs) / FX_DROP_RAMP_MS) + 1;
    if (st.surge < target)
      st.surge = (uint8_t)((st.surge + delta > target) ? target : st.surge + delta);
    else if (st.surge > target)
      st.surge = (uint8_t)((st.surge - delta < target) ? target : st.surge - delta);
  }

  // --- speed multiplier ----------------------------------------------------
  // surge pushes past 256, a live build pulls under it. They can't both be
  // strong at once (one needs loud bass, the other needs none), so a simple
  // sum of the two offsets is well-behaved.
  {
    const int32_t up   = ((int32_t)st.surge * (FX_DROP_SURGE_PCT - 100) * 256) / (100 * 255);
    const int32_t down = ((int32_t)st.build * (100 - FX_DROP_BUILD_PCT) * 256) / (100 * 255);
    int32_t s = 256 + up - down;
    if (s < 64)   s = 64;
    if (s > 1024) s = 1024;
    st.speedScale = (uint16_t)s;
  }
  return st;
}

// Wrap an effect's own fx_dt() result with this before handing it to
// fx_step()/fx_fade(). 256 is normal speed, so this is a no-op at rest.
inline uint16_t cfx_dropDt(uint16_t dtMs, uint16_t speedScale) {
  uint32_t v = ((uint32_t)dtMs * speedScale) >> 8;
  if (v > 1000) v = 1000;
  return (uint16_t)v;
}

// --- cube-net gap skipping --------------------------------------------------
// The four corner blocks of the net are unlit: 44% of the pixel work. These
// need `cube` and `B` in scope, so they follow the Flat mode checkbox.
#define CFX_NET_PREP()  uint8_t _outCol[cols]; \
                        if (cube) for (int _c = 0; _c < cols; _c++) _outCol[_c] = (uint8_t)((_c / B) != 1)
#define CFX_NET_ROW(Y)  const bool _outRow = cube && (((Y) / B) != 1)
#define CFX_NET_SKIP(X) if (_outRow && _outCol[X]) continue

// Waveform selector for the edge ripple. 0 = off.
static inline uint8_t cfx_wave(uint8_t shape, uint8_t t) {
  switch (shape) {
    case 1: return sin8_t(t);                                                   // sine
    case 2: return (t < 128) ? (uint8_t)(t << 1) : (uint8_t)((uint8_t)(255 - t) << 1);
    case 3: return (t < 128) ? 0 : 255;                                         // square
    case 4: return t;                                                           // saw up
    case 5: return (uint8_t)(255 - t);                                          // saw down
    default: return 255;
  }
}

// ---------------------------------------------------------------------------
// The 3D surface lookup. Any pointer may be null. Runs once per segment, so
// floats are fine.
// ---------------------------------------------------------------------------
static void cfx_buildCube(int8_t *cx, int8_t *cy, int8_t *cz,
                          uint8_t *az, uint8_t *el,
                          int cols, int rows, bool cubeNet) {
  const int B = cubeNet ? (cols / 3) : 1;

  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      const size_t i = (size_t)y * cols + x;
      float X, Y, Z;
      cfx_pos(x, y, cols, rows, B, cubeNet, X, Y, Z);

      if (cx) cx[i] = cfx_clamp8(X);
      if (cy) cy[i] = cfx_clamp8(Y);
      if (cz) cz[i] = cfx_clamp8(Z);

      if (az) az[i] = (uint8_t)(int)(atan2f(Y, X) * (128.0f / 3.14159265f) + 256.5f);

      if (el) {
        if (cubeNet) {
          // project the surface point onto a sphere and take its latitude, so
          // the top face is a pole instead of one flat constant value
          const float len = sqrtf(X * X + Y * Y + Z * Z);
          float nz = (len > 0.001f) ? (Z / len) : 0.0f;
          if (nz < -1.0f) nz = -1.0f; else if (nz > 1.0f) nz = 1.0f;
          float t = (asinf(nz) + 0.6155f) / 2.1863f;      // 0..1 over the solid
          if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;
          el[i] = (uint8_t)(t * 255.0f);
        } else {
          float v = 255.0f - sqrtf(X * X + Y * Y) * 180.0f;
          if (v < 0.0f) v = 0.0f; else if (v > 255.0f) v = 255.0f;
          el[i] = (uint8_t)v;
        }
      }
    }
  }
}


// ---------------------------------------------------------------------------
// Shared pixel/colour helper (originally introduced alongside Question Block,
// reused from Simon onward through Maze Racers - promoted here so every
// effect file can reach it without depending on another effect's file).
// ---------------------------------------------------------------------------
static inline uint32_t mq_scale(uint32_t c, uint8_t s) {
  return RGBW32(scale8((uint8_t)(c >> 16), s), scale8((uint8_t)(c >> 8), s),
                scale8((uint8_t)c, s), 0);
}

// ---------------------------------------------------------------------------
// Shared surface toolkit: wall "band" unwrap, face forward/inverse projection,
// direction lookup table, and pixel->cell mapping. Used by Question Block,
// Invaders, Snake, Tetris, Tron, Highway, Life, Pac-Man and Split GEQ.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Wall band: the four side faces are a seamless 4B x B cylinder. bu[] is the
// column around the ring, bv[] the row down from the top rim, and 255 in bu[]
// marks a pixel that is on the top face or in a gap. Derived from the same
// fold logic as cfx_pos, so the ring closes with no discontinuity at any of
// the four vertical corners.
// ---------------------------------------------------------------------------
static void cfx_buildBand(uint8_t *bu, uint8_t *bv, int cols, int rows, bool cubeNet, int B) {
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++) {
      const size_t i = (size_t)y * cols + x;
      if (!cubeNet) { bu[i] = (uint8_t)x; bv[i] = (uint8_t)y; continue; }
      const int bx = x / B, by = y / B, lx = x % B, ly = y % B;
      if (bx == 1 && by == 1) { bu[i] = 255; bv[i] = 0; continue; }   // top face
      if (bx != 1 && by != 1) { bu[i] = 255; bv[i] = 0; continue; }   // gap corner
      if      (by == 0) { bu[i] = (uint8_t)(lx);                 bv[i] = (uint8_t)(B - 1 - ly); }
      else if (bx == 2) { bu[i] = (uint8_t)(B + ly);             bv[i] = (uint8_t)(lx);         }
      else if (by == 2) { bu[i] = (uint8_t)(2 * B + B - 1 - lx); bv[i] = (uint8_t)(ly);         }
      else              { bu[i] = (uint8_t)(3 * B + B - 1 - ly); bv[i] = (uint8_t)(B - 1 - lx); }
    }
  }
}

static void lf_fwd(int f, float a, float b, float &X, float &Y, float &Z) {
  switch (f) {
    case 0:  X =  a; Y = -b; Z =  1.0f; break;   // TOP
    case 1:  X =  a; Y =  1.0f; Z =  b; break;   // NORTH
    case 2:  X =  a; Y = -1.0f; Z = -b; break;   // SOUTH
    case 3:  X = -1.0f; Y = -b; Z =  a; break;   // WEST
    default: X =  1.0f; Y = -b; Z = -a; break;   // EAST
  }
}
static int lf_inv(float X, float Y, float Z, float &a, float &b) {
  const float ax = fabsf(X), ay = fabsf(Y), az = fabsf(Z);
  if (az >= ax && az >= ay) {
    if (Z <= 0.0f) return -1;                    // the open bottom: no cell there
    a = X / az; b = -Y / az; return 0;
  }
  if (ay >= ax) {
    if (Y > 0.0f) { a = X / ay; b =  Z / ay; return 1; }
    a = X / ay; b = -Z / ay; return 2;
  }
  if (X < 0.0f) { a =  Z / ax; b = -Y / ax; return 3; }
  a = -Z / ax; b = -Y / ax; return 4;
}


// Face normals and tangent bases, matching lf_fwd exactly.
static const int8_t LF_N[5][3] = {{0,0,1},{0,1,0},{0,-1,0},{-1,0,0},{1,0,0}};
static const int8_t LF_U[5][3] = {{1,0,0},{1,0,0},{1,0,0},{0,0,1},{0,0,-1}};
static const int8_t LF_V[5][3] = {{0,-1,0},{0,0,1},{0,0,-1},{0,-1,0},{0,-1,0}};

// ---------------------------------------------------------------------------
// Directional transition table over all five faces: 320 cells x 4 headings,
// each entry packing the next cell and the heading you arrive with.
//
// The heading matters. Walk off the top face heading east and you are then
// heading DOWN the east wall - your direction rotated 90 degrees about the
// edge. That works out to the old face's outward normal, negated, expressed in
// the new face's basis. Without it a cycle would cross a fold and carry on in
// a direction that no longer means anything.
//
// 0xFFFF marks a step onto the unwired bottom, which behaves as a wall.
// ---------------------------------------------------------------------------
static void cfx_buildDirLut(uint16_t *lut) {
  for (int f = 0; f < 5; f++)
    for (int j = 0; j < 8; j++)
      for (int i2 = 0; i2 < 8; i2++) {
        const int c = f * 64 + j * 8 + i2;
        const float a = (i2 + 0.5f) / 4.0f - 1.0f, b = (j + 0.5f) / 4.0f - 1.0f;
        for (int d = 0; d < 4; d++) {
          const float da = (d == 0) ? 0.25f : ((d == 2) ? -0.25f : 0.0f);
          const float db = (d == 1) ? 0.25f : ((d == 3) ? -0.25f : 0.0f);
          float X, Y, Z, na, nb;
          lf_fwd(f, a + da, b + db, X, Y, Z);
          const float m = fmaxf(fabsf(X), fmaxf(fabsf(Y), fabsf(Z)));
          const int nf = lf_inv(X / m, Y / m, Z / m, na, nb);
          if (nf < 0) { lut[c * 4 + d] = 0xFFFF; continue; }
          int ni = (int)((na + 1.0f) * 4.0f); if (ni < 0) ni = 0; if (ni > 7) ni = 7;
          int nj = (int)((nb + 1.0f) * 4.0f); if (nj < 0) nj = 0; if (nj > 7) nj = 7;
          const int nc = nf * 64 + nj * 8 + ni;
          if (nc == c) { lut[c * 4 + d] = 0xFFFF; continue; }
          int nd = d;
          if (nf != f) {                       // rotate the heading over the fold
            const float tx = -(float)LF_N[f][0], ty = -(float)LF_N[f][1], tz = -(float)LF_N[f][2];
            const float du = tx*LF_U[nf][0] + ty*LF_U[nf][1] + tz*LF_U[nf][2];
            const float dv = tx*LF_V[nf][0] + ty*LF_V[nf][1] + tz*LF_V[nf][2];
            if (fabsf(du) >= fabsf(dv)) nd = (du > 0.0f) ? 0 : 2;
            else                        nd = (dv > 0.0f) ? 1 : 3;
          }
          lut[c * 4 + d] = (uint16_t)(nc | (nd << 12));
        }
      }
}

// Pixel -> surface cell, all five faces. 0xFFFF for gaps.
static void cfx_buildCells(uint16_t *cellOf, int cols, int rows, bool cube, int B) {
  for (int y = 0; y < rows; y++)
    for (int x = 0; x < cols; x++) {
      const size_t i = (size_t)y * cols + x;
      if (!cube) { cellOf[i] = (uint16_t)(((y * 8) / rows) * 8 + (x * 8) / cols); continue; }
      const int bx = x / B, by = y / B;
      if ((bx != 1 && by != 1) || B < 8) { cellOf[i] = 0xFFFF; continue; }
      const int f = (bx == 1 && by == 1) ? 0 : (by == 0 ? 1 : (by == 2 ? 2 : (bx == 0 ? 3 : 4)));
      cellOf[i] = (uint16_t)(f * 64 + (((y % B) * 8) / B) * 8 + (((x % B) * 8) / B));
    }
}
