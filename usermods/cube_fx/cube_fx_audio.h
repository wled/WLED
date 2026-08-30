#pragma once

// ===========================================================================
// cube_fx_audio.h - second-tier audio analysis for the Ace LED FX effects
// ===========================================================================
// cube_fx_common.h already carries the primitives: fx_lowBeat() (kick gate),
// cfx_tempo() (predictive beat phase / BPM / confidence) and cfx_drop()
// (build / drop / surge). This file sits on top of those and answers the
// musical questions they don't:
//
//   cfx_hiHit(um)          hat / cymbal transient, kick-leak rejected
//   cfx_midHit(um)         snare / clap transient, kick-leak rejected
//   cfx_offbeat(tempo)     the "and" - fires halfway between predicted beats
//   cfx_bar(tempo)         where we are in the 4-bar, with downbeat flag
//   cfx_silence(um)        track gaps and the "audio just came back" event
//   cfx_roll(um, tempo)    drum rolls: depth, density, acceleration
//   cfx_brightness(um)     spectral brightness (TONE colour, not LED level)
//
// USE: #include "cube_fx_audio.h" instead of "cube_fx_common.h" in any effect
// that wants these. It pulls common.h in for you, so nothing else changes and
// no existing file needs editing. Effects that don't include it pay nothing -
// unreferenced inline functions are dropped at link time.
//
// ---------------------------------------------------------------------------
// LINKAGE - read this before adding anything here
// ---------------------------------------------------------------------------
// Every analyzer below is `inline`, NOT `static inline`. A static function in
// a header gets a PRIVATE copy of its function-local statics in every .cpp
// that includes it, so with ~30 effect files you would get ~30 independent
// detectors and the "one answer per frame" contract would only hold inside a
// single file. Plain `inline` gives one shared instance across the build.
// Same rule as fx_lowBeat/cfx_tempo/cfx_drop in cube_fx_common.h.
//
// The only exception is cfx_onset(), which is `static inline` on purpose: it
// keeps ALL its state in a caller-supplied CfxOnset&, so it has no statics to
// duplicate and each caller owns its own detector.
//
// ---------------------------------------------------------------------------
// HOUSE STYLE these follow
// ---------------------------------------------------------------------------
//   - one answer per frame, guarded on strip.now
//   - every time constant is in MILLISECONDS via fx_dt(), never in frames, so
//     behaviour is identical on the 43 fps cube and the faster panel
//   - detectors return STRENGTH (0..255), not a bare flag, so a soft hit
//     gives a soft response
//   - one-shot: a hit is true on exactly one frame, never latched across
//     several, and every detector has a refractory window
//   - every threshold is a -D overridable #define
// ===========================================================================

#include "cube_fx_common.h"

// ---------------------------------------------------------------------------
// Shared onset core
// ---------------------------------------------------------------------------
// Generic rising-edge transient detector for one frequency band. All state is
// in the caller's CfxOnset, so each detector below owns a private instance.
//
// The band's own slow-moving level ("floor") is what a transient has to clear,
// exactly like the loFloor trick in fx_lowBeat: a steady hat pattern raises
// the floor to its own average so only accents poke through, and a sustained
// pad never fires at all no matter how loud it is.
//
// Strength is measured against `rise` rather than in absolute terms, so each
// detector self-scales to its own sensitivity: clearing the floor by 3x the
// rise threshold reads as full strength.
struct CfxOnset {
  uint8_t  floorV;    // slew level the band is already sitting at
  uint8_t  prev;      // band value on the previous frame
  uint16_t sinceMs;   // ms since the last accepted onset
};

static inline uint8_t cfx_onset(CfxOnset &o, uint8_t band, uint16_t dtMs,
                                uint8_t minLevel, uint8_t rise,
                                uint16_t slewMs, uint16_t refractMs) {
  o.sinceMs = (uint16_t)((o.sinceMs + dtMs > 60000) ? 60000 : o.sinceMs + dtMs);

  // Two-sided slew, updated every frame. Evaluated against the PREVIOUS
  // frame's value so a transient can't raise its own bar.
  const int floorNow = (int)o.floorV;
  {
    const int tc = (slewMs < 1) ? 1 : (int)slewMs;
    int e = floorNow + (((int)band - floorNow) * (int)dtMs) / tc;
    if (e < 0) e = 0; else if (e > 255) e = 255;
    o.floorV = (uint8_t)e;
  }

  const int was = (int)o.prev;
  o.prev = band;

  if (o.sinceMs < refractMs)            return 0;   // too soon to be a new hit
  if (band < minLevel)                  return 0;   // band isn't carrying anything
  if ((int)band < floorNow + (int)rise) return 0;   // no rise: steady, not a hit
  if ((int)band <= was)                 return 0;   // must be the rising edge itself

  o.sinceMs = 0;
  const int span = (int)rise * 3;
  int s = (span > 0) ? (((int)band - floorNow) * 255) / span : 255;
  if (s > 255) s = 255; else if (s < 1) s = 1;
  return (uint8_t)s;
}

// ---------------------------------------------------------------------------
// cfx_hiHit - hat / cymbal / shaker transient
// ---------------------------------------------------------------------------
// Bins 12..15 (roughly 6 kHz up). Deliberately does NOT use samplePeak: that
// flag is volume-driven and a kick sets it, so anything built on it fires on
// kicks and calls them hats.
//
// Kick-leak rejection: a hard kick lifts every bin including the top ones. If
// the LOW band jumped harder this frame than the high band did, the high-band
// rise is the kick's skirt and gets thrown away.
//
// Refractory is short (55 ms) because 16th hats at 174 BPM are ~86 ms apart.
// Returns 0..255 strength, one frame per hit.
#ifndef FX_HI_FLOOR
  #define FX_HI_FLOOR       20   // high band must reach this to count at all
#endif
#ifndef FX_HI_RISE
  #define FX_HI_RISE        24   // how far it must rise above its own floor
#endif
#ifndef FX_HI_REFRACT_MS
  #define FX_HI_REFRACT_MS  55   // fastest hat pattern we'll track
#endif
#ifndef FX_HI_SLEW_MS
  #define FX_HI_SLEW_MS     400  // how fast the floor follows the band
#endif
inline uint8_t cfx_hiHit(um_data_t *um) {
  static uint32_t seenFrame = 0xFFFFFFFFu;
  static uint8_t  cached    = 0;
  static uint32_t dtStore   = 0;
  static CfxOnset o         = {0, 0, 0};
  static uint8_t  loPrev    = 0;

  if (strip.now == seenFrame) return cached;   // one answer per frame
  seenFrame = strip.now;

  const uint16_t dtMs = fx_dt(dtStore);
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  hi   = (uint8_t)((fft[12] + fft[13] + fft[14] + fft[15]) / 4);
  const uint8_t  lo   = (uint8_t)((fft[0] + fft[1] + fft[2]) / 3);

  const int hiWas = (int)o.prev;               // read before cfx_onset moves it
  cached = cfx_onset(o, hi, dtMs, FX_HI_FLOOR, FX_HI_RISE,
                     FX_HI_SLEW_MS, FX_HI_REFRACT_MS);

  if (cached) {
    const int hiRise = (int)hi - hiWas;
    const int loRise = (int)lo - (int)loPrev;
    if (loRise > hiRise) cached = 0;           // kick skirt, not a hat
  }
  loPrev = lo;
  return cached;
}

// ---------------------------------------------------------------------------
// cfx_midHit - snare / clap transient
// ---------------------------------------------------------------------------
// Bins 5..8, the same definition of "mid" cfx_bands() and cfx_drop() use, so
// the word means one thing everywhere in this codebase.
//
// Longer refractory than the hats (100 ms): snares land on 2 and 4, and a
// clap layered on a snare is one musical event, not two.
#ifndef FX_MID_FLOOR
  #define FX_MID_FLOOR       32
#endif
#ifndef FX_MID_RISE
  #define FX_MID_RISE        30
#endif
#ifndef FX_MID_REFRACT_MS
  #define FX_MID_REFRACT_MS  100
#endif
#ifndef FX_MID_SLEW_MS
  #define FX_MID_SLEW_MS     320
#endif
inline uint8_t cfx_midHit(um_data_t *um) {
  static uint32_t seenFrame = 0xFFFFFFFFu;
  static uint8_t  cached    = 0;
  static uint32_t dtStore   = 0;
  static CfxOnset o         = {0, 0, 0};
  static uint8_t  loPrev    = 0;

  if (strip.now == seenFrame) return cached;
  seenFrame = strip.now;

  const uint16_t dtMs = fx_dt(dtStore);
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  const uint8_t  mid  = (uint8_t)((fft[5] + fft[6] + fft[7] + fft[8]) / 4);
  const uint8_t  lo   = (uint8_t)((fft[0] + fft[1] + fft[2]) / 3);

  const int midWas = (int)o.prev;
  cached = cfx_onset(o, mid, dtMs, FX_MID_FLOOR, FX_MID_RISE,
                     FX_MID_SLEW_MS, FX_MID_REFRACT_MS);

  if (cached) {
    const int midRise = (int)mid - midWas;
    const int loRise  = (int)lo  - (int)loPrev;
    if (loRise > midRise) cached = 0;          // kick body bleeding upward
  }
  loPrev = lo;
  return cached;
}

// ---------------------------------------------------------------------------
// cfx_offbeat - the "and"
// ---------------------------------------------------------------------------
// 255 on the single frame the tempo phase crosses halfway, 0 otherwise. Pure
// prediction: it does not need anything to actually be playing on the offbeat,
// which is the point - use it to alternate two behaviours across the bar, or
// to put a return stroke between kicks.
//
// Only fires when the lock is worth trusting. The `fired` latch, cleared on
// each real beat, is what stops a double-fire when cfx_tempo's PLL nudges the
// phase backwards across the halfway point and it re-crosses.
#ifndef FX_OFFBEAT_CONF
  #define FX_OFFBEAT_CONF 96   // tempo confidence needed before the "and" is real
#endif
inline uint8_t cfx_offbeat(const CfxTempoState &t) {
  static uint32_t seenFrame = 0xFFFFFFFFu;
  static uint8_t  cached    = 0;
  static uint8_t  prevPhase = 0;
  static bool     fired     = false;

  if (strip.now == seenFrame) return cached;
  seenFrame = strip.now;
  cached = 0;

  if (t.periodMs == 0 || t.confidence < FX_OFFBEAT_CONF) {
    prevPhase = t.phase;
    fired     = false;
    return 0;
  }
  if (t.beat) fired = false;                   // new cycle: re-arm
  if (!fired && prevPhase < 128 && t.phase >= 128) {
    cached = 255;
    fired  = true;
  }
  prevPhase = t.phase;
  return cached;
}

// ---------------------------------------------------------------------------
// cfx_bar - which beat of the bar, and where the downbeat is
// ---------------------------------------------------------------------------
// cfx_tempo() knows the beat period but has no idea which beat is beat ONE.
// Nothing in the FFT says so directly, so this uses the standard heuristic:
// over a rolling window, the slot that collects the most kick energy is the
// downbeat. Score each of the 4 slots from fx_lowBeat()'s strength, decay it
// slowly, and rotate the mapping when a different slot clearly wins.
//
// READ THIS BEFORE YOU RELY ON beatInBar == 0
// ------------------------------------------
// This is the weakest of the helpers in this file, and it fails CONFIDENTLY.
// Simulation against a 128 BPM loop with a downbeat kick 38% louder than the
// other three settled on beat 2 and sat there for ten bars at confidence 249.
// That is not a tuning problem: once frame sampling and decay are in play, a
// 25-40% accent - which is what real music has - is inside the noise, and no
// amount of scoring recovers information the spectrum doesn't carry.
//
// So treat this as A STABLE FOUR-CYCLE, not as musical beat one. For LED work
// that is nearly always what you actually wanted: something that changes every
// four beats, phase-consistent with itself, so a pattern completes instead of
// restarting mid-way. Whether the cycle starts where the drummer counts one is
// a much stronger claim, and you should only make it when confidence is high
// AND the track has an obviously accented downbeat (a crash, a dropped bar).
//
// `confidence` reports how much the winning slot dominates, so an effect can
// fade bar-locked behaviour toward beat-locked behaviour rather than commit.
//
//   beatInBar   0..len-1, 0 is the believed downbeat
//   bar         free-running bar counter, wraps at 255
//   downbeat    255 on the frame beat 0 lands
//   phase       0..255 across the WHOLE bar, not one beat
//   confidence  alignment trust, already scaled by tempo confidence
//   len         beats per bar in use
struct CfxBarState {
  uint8_t beatInBar;
  uint8_t bar;
  uint8_t downbeat;
  uint8_t phase;
  uint8_t confidence;
  uint8_t len;
};

#ifndef FX_BAR_BEATS
  #define FX_BAR_BEATS      4     // 3 for a waltz
#endif
#ifndef FX_BAR_DECAY_MS
  #define FX_BAR_DECAY_MS   12000 // rolling memory: a few bars, not the whole track
#endif
#ifndef FX_BAR_MARGIN
  #define FX_BAR_MARGIN     220   // score lead needed to move the downbeat
#endif
#ifndef FX_BAR_LEAD_PCT
  #define FX_BAR_LEAD_PCT   33    // % lead over the other beats that reads as certain
#endif
inline CfxBarState &cfx_bar(const CfxTempoState &t) {
  static CfxBarState st        = {0, 0, 0, 0, 0, FX_BAR_BEATS};
  static uint32_t    seenFrame = 0xFFFFFFFFu;
  static uint32_t    dtStore   = 0;
  static uint16_t    score[FX_BAR_BEATS] = {0};
  static uint8_t     slot      = 0;    // free-running: which beat last landed
  static uint8_t     offset    = 0;    // slot that maps to beatInBar 0

  if (strip.now == seenFrame) return st;
  seenFrame = strip.now;

  const uint16_t dtMs = fx_dt(dtStore);
  st.downbeat = 0;
  st.len      = FX_BAR_BEATS;

  if (t.periodMs == 0) {                        // no tempo: nothing to count
    for (int i = 0; i < FX_BAR_BEATS; i++) score[i] = 0;
    slot = offset = 0;
    st.beatInBar = st.phase = st.confidence = 0;
    return st;
  }

  // Credit a confirmed kick to the NEAREST predicted beat, not the last one:
  // in the second half of a cycle the kick belongs to the beat about to land.
  if (t.hit) {
    const uint8_t s = (t.phase < 128) ? slot : (uint8_t)((slot + 1) % FX_BAR_BEATS);
    const uint32_t v = (uint32_t)score[s] + t.hit;
    score[s] = (uint16_t)((v > 4000u) ? 4000u : v);
  }

  // Rolling decay, per-second so the window doesn't depend on frame rate.
  for (int i = 0; i < FX_BAR_BEATS; i++) {
    const uint32_t d = ((uint32_t)score[i] * dtMs) / FX_BAR_DECAY_MS;
    score[i] = (uint16_t)((score[i] > d) ? (score[i] - d) : 0);
  }

  if (t.beat) {
    slot = (uint8_t)((slot + 1) % FX_BAR_BEATS);

    // Re-align only on a beat boundary, so beatInBar never jumps mid-beat.
    int best = 0, bestI = 0, total = 0;
    for (int i = 0; i < FX_BAR_BEATS; i++) {
      total += (int)score[i];
      if ((int)score[i] > best) { best = (int)score[i]; bestI = i; }
    }
    if (best > (int)score[offset] + FX_BAR_MARGIN) offset = (uint8_t)bestI;

    // Confidence is PROPORTIONAL, not an absolute score gap. Real downbeat
    // kicks are maybe 20-40% louder than the rest, never 3x, so an absolute
    // difference reads as "no idea" on a track whose downbeat is perfectly
    // clear. FX_BAR_LEAD_PCT is the lead that counts as certainty: at the
    // default 33%, a downbeat a third stronger than the average of the other
    // beats reads 255, and a flat four-on-the-floor still honestly reads low.
    const int others = (FX_BAR_BEATS > 1)
      ? (total - best) / (FX_BAR_BEATS - 1) : 0;
    int c = 0;
    if (best > 0) {
      c = ((best - others) * 255 * 100) / (best * FX_BAR_LEAD_PCT);
      if (c < 0) c = 0; else if (c > 255) c = 255;
    }
    st.confidence = scale8((uint8_t)c, t.confidence);

    st.beatInBar = (uint8_t)((slot + FX_BAR_BEATS - offset) % FX_BAR_BEATS);
    if (st.beatInBar == 0) { st.downbeat = 255; st.bar++; }
  }

  st.phase = (uint8_t)(((uint16_t)st.beatInBar * 256u + t.phase) / FX_BAR_BEATS);
  return st;
}

// ---------------------------------------------------------------------------
// cfx_silence - track gaps, and the moment the music comes back
// ---------------------------------------------------------------------------
// Not the same thing as cfx_drop().build. A build is the track deliberately
// pulling the bass out; this is nothing playing at all - between tracks, a
// paused source, someone lifting the needle.
//
// Takes the LOUDER of smoothed volume and mean spectral energy, so neither
// AGC pumping the noise floor nor a lone sub-bass note reads as quiet. The
// envelope releases over 600 ms so the gap between two notes is never
// mistaken for the gap between two tracks, and the in/out thresholds are
// separate (hysteresis) so a signal hovering at the bar can't chatter.
//
//   level  0 = music, 255 = fully quiet, RAMPED - fade idle animation with it
//   quiet  hysteretic latch, true once it's been quiet FX_SILENCE_MS
//   ms     ms of continuous quiet
//   wake   255 on the single frame audio returns. This is the useful one:
//          it's the "new track started" event.
struct CfxSilenceState {
  uint8_t  level;
  bool     quiet;
  uint16_t ms;
  uint8_t  wake;
};

#ifndef FX_SILENCE_IN
  #define FX_SILENCE_IN      6    // envelope at/below this is quiet
#endif
#ifndef FX_SILENCE_OUT
  #define FX_SILENCE_OUT     16   // envelope at/above this is music again
#endif
#ifndef FX_SILENCE_MS
  #define FX_SILENCE_MS      900  // continuous quiet before we believe it
#endif
#ifndef FX_SILENCE_RAMP_MS
  #define FX_SILENCE_RAMP_MS 1200 // how long `level` takes to cross 0<->255
#endif
inline CfxSilenceState &cfx_silence(um_data_t *um) {
  static CfxSilenceState st        = {0, false, 0, 0};
  static uint32_t        seenFrame = 0xFFFFFFFFu;
  static uint32_t        dtStore   = 0;
  static uint8_t         env       = 0;

  if (strip.now == seenFrame) return st;
  seenFrame = strip.now;
  st.wake = 0;

  const uint16_t dtMs = fx_dt(dtStore);
  const float    vol  = *(float *)um->u_data[0];
  const uint8_t *fft  = (uint8_t *)um->u_data[2];

  int sum = 0;
  for (int k = 0; k < 16; k++) sum += fft[k];

  int v = (int)vol;
  if (v < 0) v = 0; else if (v > 255) v = 255;
  const int spec = sum / 16;
  const uint8_t loud = (uint8_t)((v > spec) ? v : spec);
  env = fx_env(env, loud, dtMs, 600);

  if (env <= FX_SILENCE_IN) {
    st.ms = (uint16_t)((st.ms + dtMs > 60000) ? 60000 : st.ms + dtMs);
    if (st.ms >= FX_SILENCE_MS) st.quiet = true;
  } else if (env >= FX_SILENCE_OUT) {
    if (st.quiet) st.wake = 255;                // music is back
    st.quiet = false;
    st.ms    = 0;
  } else if (!st.quiet) {
    st.ms = 0;                                  // grey zone: don't accumulate
  }

  {
    const uint8_t target = st.quiet ? 255 : 0;
    const int delta = (int)(((uint32_t)255 * dtMs) / FX_SILENCE_RAMP_MS) + 1;
    if (st.level < target)
      st.level = (uint8_t)((st.level + delta > target) ? target : st.level + delta);
    else if (st.level > target)
      st.level = (uint8_t)((st.level - delta < target) ? target : st.level - delta);
  }
  return st;
}

// ---------------------------------------------------------------------------
// cfx_roll - drum rolls and fills
// ---------------------------------------------------------------------------
// A roll is a run of transients much faster than the beat, held long enough
// to be a gesture rather than a flam. Detected from the inter-onset interval
// on a combined mid+high band (bins 5..15 - snare rolls, hat rolls and ride
// runs all live in there), with its own short 45 ms refractory, because
// cfx_midHit's 100 ms guard would swallow exactly the thing we're looking for.
//
// Ties to the tempo when there is one: "fast" means faster than a third of a
// beat, so it scales with the track instead of a fixed millisecond guess. If
// nothing is locked it falls back to an absolute interval.
//
//   level    0..255, ramped, so it fades in and out instead of snapping
//   density  onsets per beat x16 - 32 is 2 per beat, 64 is 4, capped at 16
//   accel    0..255, how much the roll has sped up since it started. This is
//            the one that says "the fill is about to end" - a roll that
//            accelerates is walking you into the next downbeat.
//   active   the raw latch, before the ramp
struct CfxRollState {
  uint8_t level;
  uint8_t density;
  uint8_t accel;
  bool    active;
};

#ifndef FX_ROLL_FLOOR
  #define FX_ROLL_FLOOR       28
#endif
#ifndef FX_ROLL_RISE
  #define FX_ROLL_RISE        22
#endif
#ifndef FX_ROLL_REFRACT_MS
  #define FX_ROLL_REFRACT_MS  45   // ~1/32 notes at 170 BPM
#endif
#ifndef FX_ROLL_MIN_DIV
  #define FX_ROLL_MIN_DIV     3    // must be at least this many onsets per beat
#endif
#ifndef FX_ROLL_MAX_IOI_MS
  #define FX_ROLL_MAX_IOI_MS  160  // fallback bar when no tempo is locked
#endif
#ifndef FX_ROLL_MS
  #define FX_ROLL_MS          260  // hold time before a fast run counts as a roll
#endif
#ifndef FX_ROLL_MIN_HITS
  #define FX_ROLL_MIN_HITS    3    // onsets needed before the rate is trustworthy
#endif
#ifndef FX_ROLL_GAP_MAX_MS
  #define FX_ROLL_GAP_MAX_MS  350  // a gap longer than this can't be part of a roll
#endif
#ifndef FX_ROLL_RAMP_MS
  #define FX_ROLL_RAMP_MS     220
#endif
#ifndef FX_ROLL_ACCEL_MIN_PCT
  #define FX_ROLL_ACCEL_MIN_PCT 20  // interval shrink below this is frame jitter
#endif
#ifndef FX_ROLL_ACCEL_FULL_PCT
  #define FX_ROLL_ACCEL_FULL_PCT 55 // shrink that reads as maximum acceleration.
                                    // Real fills usually double in speed (50%),
                                    // so 55 puts a doubling near the top.
#endif
inline CfxRollState &cfx_roll(um_data_t *um, const CfxTempoState &t) {
  static CfxRollState st        = {0, 0, 0, false};
  static uint32_t     seenFrame = 0xFFFFFFFFu;
  static uint32_t     dtStore   = 0;
  static CfxOnset     o         = {0, 0, 0};
  static uint16_t     ioiMs     = 0;   // smoothed inter-onset interval
  static uint16_t     gapMs     = 0;   // ms since the last onset
  static uint16_t     runMs     = 0;   // how long the fast pattern has held
  static uint16_t     startIoi  = 0;   // interval when the roll began
  static uint8_t      runHits   = 0;   // onsets in the current fast run

  if (strip.now == seenFrame) return st;
  seenFrame = strip.now;

  const uint16_t dtMs = fx_dt(dtStore);
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  int s = 0;
  for (int k = 5; k < 16; k++) s += fft[k];
  const uint8_t band = (uint8_t)(s / 11);

  gapMs = (uint16_t)((gapMs + dtMs > 60000) ? 60000 : gapMs + dtMs);

  // Interval tracking. A gap too long to belong to a roll CLEARS the estimate
  // rather than being smoothed into it - otherwise the long gap before a fill
  // gets averaged in, ioiMs spends the first several hits converging downward,
  // and a perfectly steady roll reads as a fast-accelerating one.
  if (cfx_onset(o, band, dtMs, FX_ROLL_FLOOR, FX_ROLL_RISE,
                300, FX_ROLL_REFRACT_MS)) {
    const uint16_t gap = gapMs;
    gapMs = 0;
    if (gap > FX_ROLL_GAP_MAX_MS) {
      ioiMs   = 0;                              // next gap seeds it clean
      runHits = 0;
    } else {
      ioiMs = ioiMs ? (uint16_t)((int)ioiMs + ((int)gap - (int)ioiMs) / 3) : gap;
      if (runHits < 255) runHits++;
    }
  }

  uint16_t fastBar = FX_ROLL_MAX_IOI_MS;
  if (t.periodMs > 0 && t.confidence >= 64)
    fastBar = (uint16_t)(t.periodMs / FX_ROLL_MIN_DIV);

  // The gap clause is what ends a roll promptly: once the hits stop coming at
  // the rate they were, it's over, without waiting for a timeout.
  const bool fast = (ioiMs > 0) && (ioiMs <= fastBar) &&
                    (gapMs <= (uint16_t)(ioiMs * 2 + 60));

  if (fast) {
    runMs = (uint16_t)((runMs + dtMs > 60000) ? 60000 : runMs + dtMs);
    // Both gates matter: runMs says the gesture is long enough to be musical,
    // runHits says ioiMs has actually settled, so startIoi is a real baseline.
    if (!st.active && runMs >= FX_ROLL_MS && runHits >= FX_ROLL_MIN_HITS) {
      st.active = true;
      startIoi  = ioiMs;
    }
  } else {
    runMs     = 0;
    st.active = false;
    startIoi  = 0;
    if (gapMs > 1200) { ioiMs = 0; runHits = 0; }   // stale: forget the old rate
  }

  {
    const uint8_t target = st.active ? 255 : 0;
    const int delta = (int)(((uint32_t)255 * dtMs) / FX_ROLL_RAMP_MS) + 1;
    if (st.level < target)
      st.level = (uint8_t)((st.level + delta > target) ? target : st.level + delta);
    else if (st.level > target)
      st.level = (uint8_t)((st.level - delta < target) ? target : st.level - delta);
  }

  if (ioiMs > 0) {
    const uint32_t p = (t.periodMs > 0) ? (uint32_t)t.periodMs : 500u;
    const uint32_t d = (p * 16u) / (uint32_t)ioiMs;
    st.density = (uint8_t)((d > 255u) ? 255u : d);
  } else {
    st.density = 0;
  }

  // Acceleration needs a deadband. At 43 fps an onset can only be timed to the
  // nearest frame, so a 78 ms interval quantises to 69 or 92 ms - a steady roll
  // jitters by ~15% and without this reads as constantly speeding up. Only a
  // shrink past FX_ROLL_ACCEL_MIN_PCT counts, and one of FULL_PCT saturates.
  st.accel = 0;
  if (st.active && startIoi > 0 && ioiMs > 0 && ioiMs < startIoi) {
    int pct = (int)(((uint32_t)(startIoi - ioiMs) * 100u) / (uint32_t)startIoi);
    if (pct > FX_ROLL_ACCEL_MIN_PCT) {
      int a = ((pct - FX_ROLL_ACCEL_MIN_PCT) * 255) /
              (FX_ROLL_ACCEL_FULL_PCT - FX_ROLL_ACCEL_MIN_PCT);
      st.accel = (uint8_t)((a > 255) ? 255 : a);
    }
  }
  return st;
}

// ---------------------------------------------------------------------------
// cfx_brightness - spectral brightness (TONE, not LED level)
// ---------------------------------------------------------------------------
// NAME WARNING: this has nothing to do with how bright the LEDs are. It is
// the timbral sense of the word - where the energy sits in the spectrum. A
// sub-heavy breakdown reads low, an open hi-hat or a distorted lead reads
// high, and the same riff played through a filter sweep walks it up smoothly.
// For LED output level you still want cfx_drive(vol, gain, floor).
//
// This is the energy-weighted mean bin index. Because WLED's 16 bins are
// roughly log-spaced in frequency, the plain bin index is already a
// log-frequency axis - which is the perceptually correct one, so no extra
// warping is needed.
//
// The natural use is palette index: SEGMENT.color_from_palette(cfx_brightness(um), ...)
// gives you colour that tracks the tone of the track rather than its volume,
// which stays interesting through a passage where the level never changes.
//
// Below FX_BRIGHT_MIN_ENERGY the centroid is meaningless (dividing noise by
// noise), so the last good value is held rather than allowed to jitter.
#ifndef FX_BRIGHT_MIN_ENERGY
  #define FX_BRIGHT_MIN_ENERGY 120  // summed across all 16 bins
#endif
// Slew is the whole character of this helper, so here is the measured
// tradeoff on a 128 BPM kick/hat pattern at 43 fps - swing is how far the
// output actually travels between the kick-dominated and hat-dominated halves
// of a beat, drift is the mean per-frame step (how strobey it looks):
//
//     60 ms  swing 111   drift 10.7    twitchy, tracks individual hits
//    140 ms  swing  81   drift  7.6    <- default: responsive, still smooth
//    260 ms  swing  49   drift  4.3    calm, reads passages not hits
//    400 ms  swing  31   drift  2.7    nearly static, only big changes show
//
// Raise it if the palette feels busy, drop it if the colour feels dead.
#ifndef FX_BRIGHT_SLEW_MS
  #define FX_BRIGHT_SLEW_MS    140
#endif
inline uint8_t cfx_brightness(um_data_t *um) {
  static uint32_t seenFrame = 0xFFFFFFFFu;
  static uint32_t dtStore   = 0;
  static uint8_t  smooth    = 128;   // start mid so the first frame isn't black

  if (strip.now == seenFrame) return smooth;
  seenFrame = strip.now;

  const uint16_t dtMs = fx_dt(dtStore);
  const uint8_t *fft  = (uint8_t *)um->u_data[2];

  uint32_t num = 0, den = 0;
  for (int k = 0; k < 16; k++) {
    const uint32_t w = fft[k];
    num += w * (uint32_t)k;
    den += w;
  }
  if (den < FX_BRIGHT_MIN_ENERGY) return smooth;   // hold through silence

  uint32_t c = (num * 17u) / den;                  // bin 0..15 -> 0..255
  if (c > 255u) c = 255u;

  const int target = (int)c;
  int d = ((target - (int)smooth) * (int)dtMs) / FX_BRIGHT_SLEW_MS;
  if (d == 0) d = (target > (int)smooth) ? 1 : ((target < (int)smooth) ? -1 : 0);
  int v = (int)smooth + d;
  if (v < 0) v = 0; else if (v > 255) v = 255;
  smooth = (uint8_t)v;
  return smooth;
}
