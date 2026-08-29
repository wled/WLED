#include "wled.h"
#include "cube_fx_audio.h"

// ===========================================================================
// 34. ACE 3-D AUDIO SCOPE  (diagnostic, not a "real" effect)
// ===========================================================================
// Companion to Tempo Scope. That one makes cfx_tempo() visible; this makes
// everything in cube_fx_audio.h visible at once, so the thresholds can be
// tuned against real music instead of guessed at.
//
// THE WALLS ARE A CIRCULAR CHART RECORDER
// ---------------------------------------
// The four side faces form a seamless 4B x B cylinder, so the display is a
// closed loop: a playhead sweeps around the cube writing the current frame
// into the column under it, and each lap overwrites the lap before. There is
// no edge for data to scroll off, and no seam - the trace runs continuously
// past all four vertical corners. One lap is 2-8 seconds depending on Sweep.
//
// Eight horizontal lanes, top rim downward. The first two are the OLD helpers,
// there as a reference to judge the new ones against:
//
//   0  KICK    red      fx_lowBeat() strength
//   1  SNARE   amber    cfx_midHit()
//   2  HAT     cyan     cfx_hiHit()
//   3  BEAT    green    cfx_tempo().beat full height, cfx_offbeat() half
//   4  BAR     magenta  downbeat full, other beats short, DIMMED BY CONFIDENCE
//   5  ROLL    yellow   cfx_roll(): height = level, whiteness = accel
//   6  TONE    palette  cfx_brightness() straight into the palette
//   7  QUIET   blue     cfx_silence().level
//
// WHAT TO LOOK FOR
// ----------------
//   - lane 3's half-height offbeat marks should sit exactly between the full
//     ones. If they drift, the tempo lock is soft.
//   - lanes 1 and 2 should fire where you HEAR snares and hats, and should
//     stay quiet under a kick. If lane 2 lights up on every kick, the kick-leak
//     rejection isn't biting: raise FX_HI_RISE.
//   - lane 4 dims itself when cfx_bar() doesn't trust its own alignment. A
//     bright lane 4 that lands on the wrong beat is the documented failure
//     mode - see the warning above cfx_bar() before believing it.
//   - lane 5 should go white only when a fill genuinely accelerates.
//   - lane 6 should ride high through hats and cymbals, low through bass.
//
// TOP FACE: the bar, as four quadrants clockwise from north-west. The current
// beat's quadrant strikes and decays across the beat, the downbeat quadrant is
// white and the rest magenta, and the WHOLE face is scaled by bar confidence -
// so an uncertain lock reads as a visibly dim top face rather than a confident
// lie. The centre square flashes white on cfx_silence().wake and glows blue
// while quiet.
//
// Freeze stops the playhead so the last full lap can be studied at leisure.
// ---------------------------------------------------------------------------

#define ASC_LANES 8
#define ASC_IDLE  40    // resting brightness of a top-face quadrant

// Lane colours as plain 0x00RRGGBB literals - no FastLED, no palette
// dependency, nothing that shifts between WLED versions.
static const uint32_t ASC_COL[ASC_LANES] = {
  0x00FF2A1E,   // 0 KICK   red
  0x00FF9614,   // 1 SNARE  amber
  0x003CE6FF,   // 2 HAT    cyan
  0x003CFF5A,   // 3 BEAT   green
  0x00E646FF,   // 4 BAR    magenta
  0x00FFE128,   // 5 ROLL   yellow
  0x00FFFFFF,   // 6 TONE   unused - the palette supplies the colour
  0x003C6EFF,   // 7 QUIET  blue
};

// beat-in-bar -> top-face quadrant, laid out clockwise from NW. Happens to be
// its own inverse, so the same table reads quadrant -> beat.
static const uint8_t ASC_Q[4] = {0, 1, 3, 2};

// Blend a colour toward white. Used for the roll lane's acceleration readout
// and for the playhead.
static inline uint32_t asc_toWhite(uint32_t c, uint8_t amt) {
  const uint8_t r = (uint8_t)(c >> 16), g = (uint8_t)(c >> 8), b = (uint8_t)c;
  return RGBW32((uint8_t)(r + scale8((uint8_t)(255 - r), amt)),
                (uint8_t)(g + scale8((uint8_t)(255 - g), amt)),
                (uint8_t)(b + scale8((uint8_t)(255 - b), amt)), 0);
}

static FX_RET mode_audio_scope() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int    cols = SEG_W, rows = SEG_H;
  const size_t n    = (size_t)cols * rows;

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  // Ring length is the wall cylinder on a cube, the panel width when flat.
  const int ringLen  = cube ? (4 * B) : cols;
  const int laneSpan = cube ? B : rows;
  if (ringLen < 4 || laneSpan < 1) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  // 2n for the band LUT, then the history ring, then 4 housekeeping bytes:
  // [0..1] column timer, [2] wake flash, [3] reserved.
  const size_t histBytes = (size_t)ringLen * ASC_LANES;
  if (!SEGENV.allocateData(2 * n + histBytes + 4)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *bu   = SEGENV.data;
  uint8_t *bv   = bu + n;
  uint8_t *hist = bv + n;
  uint8_t *keep = hist + histBytes;

  // Rebuild when the geometry assumption changes. allocateData already wiped
  // the buffer if the size moved, but flat<->cube at the SAME size wouldn't
  // trip that, so the marker is still needed.
  const uint16_t marker = cube ? 1 : 2;
  if (SEGENV.call == 0 || SEGENV.aux0 != marker) {
    cfx_buildBand(bu, bv, cols, rows, cube, B);
    memset(hist, 0, histBytes);
    keep[0] = keep[1] = keep[2] = keep[3] = 0;
    SEGENV.aux0 = marker;
    SEGENV.aux1 = 0;
  }
  if (SEGENV.aux1 >= (uint16_t)ringLen) SEGENV.aux1 = 0;

  // --- read every analyzer once ------------------------------------------
  // cfx_tempo() must be first: offbeat, bar and roll all take its state.
  // Each has its own per-frame guard, so calling them all costs one pass.
  um_data_t             *um   = cfx_getAudioData();
  const CfxTempoState   &tm   = cfx_tempo(um);
  const uint8_t          hiV  = cfx_hiHit(um);
  const uint8_t          mdV  = cfx_midHit(um);
  const uint8_t          obV  = cfx_offbeat(tm);
  const CfxBarState     &br   = cfx_bar(tm);
  const CfxSilenceState &sl   = cfx_silence(um);
  const CfxRollState    &rl   = cfx_roll(um, tm);
  const uint8_t          tone = cfx_brightness(um);

  const uint16_t dtMs = fx_dt(SEGENV.step);

  // --- advance the playhead ----------------------------------------------
  // Time-based, never frame-based: one lap takes the same wall-clock time on
  // the 43 fps cube as on the faster panel.
  const uint16_t colMs = (uint16_t)(20 + (((uint32_t)(255 - SEGMENT.speed) * 120u) / 255u));
  int head = (int)SEGENV.aux1;
  if (!SEGMENT.check2) {                          // Freeze holds the playhead
    uint16_t accMs = (uint16_t)keep[0] | ((uint16_t)keep[1] << 8);
    accMs = (uint16_t)(accMs + dtMs);
    int guard = 0;
    while (accMs >= colMs && guard < 4) {         // guard: never spiral
      accMs = (uint16_t)(accMs - colMs);
      head  = (head + 1) % ringLen;
      memset(hist + (size_t)head * ASC_LANES, 0, ASC_LANES);   // clear before writing
      guard++;
    }
    if (guard >= 4) accMs = 0;
    keep[0] = (uint8_t)accMs;
    keep[1] = (uint8_t)(accMs >> 8);
    SEGENV.aux1 = (uint16_t)head;
  }

  // --- write this frame into the column under the playhead ----------------
  // Event lanes take the MAX across the frames a column spans, so a hat can
  // never be lost between two columns. Level lanes just take the latest.
  {
    uint8_t *col = hist + (size_t)head * ASC_LANES;
    if (tm.hit > col[0]) col[0] = tm.hit;
    if (mdV    > col[1]) col[1] = mdV;
    if (hiV    > col[2]) col[2] = hiV;

    const uint8_t beatV = tm.beat ? 255 : (obV ? 120 : 0);
    if (beatV > col[3]) col[3] = beatV;

    if (tm.beat) {
      // Confidence is folded in here rather than at draw time so the lane
      // shows what cfx_bar() believed AT THE MOMENT, not what it believes now.
      uint8_t v = br.downbeat ? 255 : 70;
      v = scale8(v, (uint8_t)(64 + scale8(br.confidence, 191)));
      if (v > col[4]) col[4] = v;
    }

    // Roll packs two values into one byte: high nibble accel, low nibble
    // level. x17 on the way back out maps 0..15 to 0..255 exactly.
    col[5] = (uint8_t)((rl.accel & 0xF0) | (rl.level >> 4));
    col[6] = tone;
    col[7] = sl.level;
  }

  // Wake needs latching - it is true for a single frame, which is 23 ms and
  // effectively invisible.
  {
    uint8_t w = keep[2];
    if (sl.wake) w = 255;
    else if (w) { const uint8_t d = fx_fade(14, dtMs); w = (w > d) ? (uint8_t)(w - d) : 0; }
    keep[2] = w;
  }

  // --- draw ---------------------------------------------------------------
  const uint8_t gain     = SEGMENT.intensity;
  const uint8_t playhead = SEGMENT.custom1;
  const uint8_t fade     = SEGMENT.custom2;
  const uint8_t topAmt   = SEGMENT.custom3;
  const int     half     = cube ? (B / 2) : 0;
  const int     ctr      = cube ? ((B >= 12) ? 2 : 1) : 0;   // centre square radius

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    const int by = cube ? (y / B) : 0;
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);
      const int bx = cube ? (x / B) : 0;

      // ---- top face: the bar ---------------------------------------------
      if (cube && bx == 1 && by == 1) {
        const int lx = x % B, ly = y % B;
        const int q  = (ly < half ? 0 : 1) * 2 + (lx < half ? 0 : 1);
        const int qb = ASC_Q[q];

        uint8_t  v = 0;
        uint32_t c = (qb == 0) ? 0x00FFFFFF : ASC_COL[4];
        if (tm.periodMs > 0) {
          // The active quadrant strikes at the beat and decays across it, but
          // floors at the idle level - without the floor it fades to black at
          // the end of its beat, so the face reads as three lit quadrants and
          // one hole right up until the next beat hands over.
          const uint8_t act = (uint8_t)(255 - tm.phase);
          v = (qb == br.beatInBar) ? ((act < ASC_IDLE) ? ASC_IDLE : act) : ASC_IDLE;
          v = scale8(v, (uint8_t)(64 + scale8(br.confidence, 191)));
        }
        v = scale8(v, topAmt);

        // Centre square: silence state, drawn OVER the bar display - but only
        // when it has something to say. Blanking it while the audio is fine
        // just punched a permanent hole in the middle of the quadrants.
        if (lx >= half - ctr && lx < half + ctr && ly >= half - ctr && ly < half + ctr) {
          if (keep[2])       { c = 0x00FFFFFF; v = keep[2]; }
          else if (sl.level) { c = ASC_COL[7]; v = sl.level; }
        }
        SEGMENT.setPixelColorXY(x, y, v ? mq_scale(c, v) : 0u);
        continue;
      }

      // ---- walls (or the whole panel when flat) --------------------------
      const int c8 = (int)bu[i];
      if (c8 >= ringLen) { SEGMENT.setPixelColorXY(x, y, 0u); continue; }

      int lane = ((int)bv[i] * ASC_LANES) / laneSpan;
      if (lane >= ASC_LANES) lane = ASC_LANES - 1;
      const uint8_t v = hist[(size_t)c8 * ASC_LANES + lane];

      uint32_t colr;
      uint8_t  bright;
      if (lane == 6) {                    // TONE: value IS the palette index
        colr   = SEGMENT.color_from_palette(v, false, false, 0, 255);
        bright = 200;
      } else if (lane == 5) {             // ROLL: unpack level + accel
        bright = (uint8_t)((v & 0x0F) * 17);
        colr   = asc_toWhite(ASC_COL[5], (uint8_t)((v >> 4) * 17));
      } else {
        colr   = ASC_COL[lane];
        bright = v;
      }
      bright = scale8(bright, gain);

      // Age taper: how far this column sits BEHIND the playhead, so the fresh
      // side of the lap reads brighter than the side about to be overwritten.
      if (fade) {
        int age = head - c8;
        if (age < 0) age += ringLen;
        const uint8_t drop = (uint8_t)(((uint32_t)age * fade) / (uint32_t)ringLen);
        bright = scale8(bright, (uint8_t)(255 - drop));
      }

      // Playhead: drawn ONLY into empty cells, never over live data. In a
      // continuously-filled lane like TONE it simply doesn't appear - the
      // event lanes above are enough to locate "now", and covering a lane
      // you're trying to read would defeat the point.
      //
      // This MUST be tested before the lane guides below. The guides write a
      // faint 10 into each lane's top row, which is non-zero, so running them
      // first made the playhead vanish from every guide row and rendered it as
      // a dotted line with a gap in each lane.
      bool drawn = false;
      if (c8 == head && !bright) { colr = 0x00FFFFFF; bright = playhead; drawn = true; }

      // Lane guides: a faint rail on each lane's top row, only where the lane
      // is empty, so they never hide data.
      if (SEGMENT.check1 && !drawn && bright < 10) {
        const int prevLane = (bv[i] == 0) ? -1 : (((int)bv[i] - 1) * ASC_LANES) / laneSpan;
        if (prevLane != lane) { colr = ASC_COL[lane]; bright = 10; }
      }

      SEGMENT.setPixelColorXY(x, y, bright ? mq_scale(colr, bright) : 0u);
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_AUDIO_SCOPE[] PROGMEM =
  "Ace 3-D Audio Scope@Sweep,Gain,Playhead,Age fade,Top face,Lane guides,Freeze,Flat mode;;!;2f;sx=140,ix=190,c1=170,c2=90,c3=180,o1=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_AudioScopeUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_audio_scope, _data_FX_MODE_AUDIO_SCOPE);
  }
  void loop() override {}
};

static CubeFx_AudioScopeUsermod cube_fx_audio_scope;
REGISTER_USERMOD(cube_fx_audio_scope);
