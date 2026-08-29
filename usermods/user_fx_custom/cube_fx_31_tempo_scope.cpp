#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 31. ACE 3-D TEMPO SCOPE  (diagnostic, not a "real" effect)
// ===========================================================================
// Makes cfx_tempo() visible so its lock/predict/drop behaviour can be judged
// on real hardware instead of guessed at from code:
//
//   - a coloured band sweeps once around the four walls per PREDICTED beat
//     (cube net) or once across the panel (flat) - it should visually lead
//     the music by a hair, not trail it, once locked
//   - the top face breathes a triangular pulse that PEAKS exactly when the
//     next beat is predicted to land, so a peak with no kick under it is an
//     easy "prediction fired, beat didn't" read
//   - a short white flash overlays on every CONFIRMED fx_lowBeat() hit - watch
//     it land on top of the sweep/pulse when locked, and land anywhere when
//     still acquiring
//   - both fade toward black with confidence, so a bass drop reads as the
//     scope visibly going dim/uncertain instead of continuing to guess loud
//   - band/pulse colour tracks the current BPM estimate (blue slow -> red
//     fast) as a rough at-a-glance tempo readout; unlocked = no colour, only
//     the white hit flashes, since there's nothing to trust yet
// ---------------------------------------------------------------------------
static FX_RET mode_tempo_scope() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *bu = SEGENV.data;
  uint8_t *bv = bu + n;

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  if (SEGENV.call == 0 || SEGENV.aux0 != (uint16_t)(cube ? 1 : 2)) {
    cfx_buildBand(bu, bv, cols, rows, cube, B);
    SEGENV.aux0 = cube ? 1 : 2;
  }
  const int ringLen = cube ? (4 * B) : cols;

  um_data_t          *um   = cfx_getAudioData();
  const uint8_t        hit  = fx_lowBeat(um);          // confirmed, for the flash
  const CfxTempoState &t    = cfx_tempo(um);            // predicted phase/confidence/bpm

  SEGMENT.fadeToBlackBy(fx_fade(10 + ((255 - SEGMENT.speed) >> 1), fx_dt(SEGENV.step)));

  const int bandWidth = 1 + (SEGMENT.intensity >> 4);   // sweep width, in ring units
  const int pos       = (t.periodMs > 0) ? ((int)t.phase * ringLen) >> 8 : -1;
  const uint8_t flashAmt = scale8(hit, SEGMENT.custom1);

  // BPM -> palette index: 60 bpm reads cool, 180 bpm reads hot. Unlocked
  // pins to index 0 but that only shows up where confidence lets it through.
  uint8_t idx = 0;
  if (t.bpm > 0) {
    uint16_t b = t.bpm; if (b < 60) b = 60; if (b > 180) b = 180;
    idx = (uint8_t)(160 - ((b - 60) * 160) / 120);
  }

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);

      uint8_t bright;
      if (cube && bu[i] == 255) {
        // top face: triangular envelope peaking when the predicted beat lands
        if (t.periodMs == 0) {
          bright = 0;
        } else {
          const uint8_t dist = (t.phase < 128) ? t.phase : (uint8_t)(255 - t.phase);
          int e = 255 - 2 * (int)dist;
          bright = scale8((uint8_t)((e < 0) ? 0 : e), t.confidence);
        }
      } else if (pos < 0) {
        bright = 0;                                     // not locked: sweep parked, dark
      } else {
        int d = (int)bu[i] - pos; if (d < 0) d = -d;
        if (d > ringLen - d) d = ringLen - d;             // circular distance
        bright = (d <= bandWidth)
          ? (uint8_t)(255 - (d * 255) / (bandWidth + 1))
          : 0;
        bright = scale8(bright, t.confidence);
      }

      bright = qadd8(bright, flashAmt);                   // confirmed-hit overlay
      if (!bright) continue;
      SEGMENT.setPixelColorXY(x, y,
        SEGMENT.color_from_palette(idx, false, false, 0, bright));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_TEMPO_SCOPE[] PROGMEM =
  "Ace 3-D Tempo Scope@Trail,Band width,Flash,,,,,Flat mode;;!;2f;sx=200,ix=90,c1=160";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_TempoScopeUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_tempo_scope, _data_FX_MODE_TEMPO_SCOPE);
  }
  void loop() override {}
};

static CubeFx_TempoScopeUsermod cube_fx_tempo_scope;
REGISTER_USERMOD(cube_fx_tempo_scope);
