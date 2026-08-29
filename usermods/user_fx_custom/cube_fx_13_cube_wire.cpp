#include "wled.h"
#include "cube_fx_common.h"
#include "cube_fx_imu.h"

// ===========================================================================
// 13. CUBE WIRE  /  ACE GYRO WIRE
// ===========================================================================
// Cube Edges stripped to the wireframe. No edge<->centre travel at all: the
// beat pulses run ALONG the edges instead of across them.
//
// The trick is which coordinate the pulse lives in. Cube Edges pulses through
// ed[] - distance to the nearest edge. This one pulses through al[] - position
// along that edge. Because all twelve edges share one al parametrisation, a
// single pulse expands along every edge of the solid simultaneously, two
// fronts running apart from where it struck, which is Cube Ripples' behaviour
// confined to the wire.
//
// The wave still displaces the lit line, so the pulses ride a snaking wire
// rather than a straight one.
//
// ---------------------------------------------------------------------------
// THE GYRO VARIANT: A WIREFRAME THAT STAYS PUT
// ---------------------------------------------------------------------------
// The wire stops being the cube's own edges and becomes a wireframe hanging
// in the ROOM. Turn the cube and the frame does not turn with it - the
// surface slides underneath, and the lines cut across the faces at whatever
// angle they have to. It is the strongest illusion in the whole Gyro class,
// because straight lines are what make rotation legible: the moment the wire
// refuses to follow the object, the object stops reading as a screen and
// starts reading as a window.
//
// HOW, GIVEN THAT A WIRE IS ONE-DIMENSIONAL. This is the part that does not
// work the obvious way. A literal wireframe fixed in world space intersects
// the cube's two-dimensional surface in POINTS, not curves - you would get a
// handful of lonely dots and nothing else. So the frame is not intersected,
// it is PROJECTED: each pixel's world direction is followed outward from the
// centre until it lands on a virtual unit cube, and the pixel lights if that
// landing point is near an edge of it. The intersection is now a curve, and
// the whole thing collapses to a comparison between the largest and second
// largest world components:
//
//     on the wire   <=>   m2 / m1  is close to 1
//     along the wire <=>  the remaining (smallest) component, over m1
//
// Which is exactly the test the flat version already does - it just never had
// to divide by m1, because a point on the cube's own surface always has its
// largest coordinate sitting at 1 already. Rotate the frame away from the
// solid and that stops being true, so the divide appears. One per pixel.
//
// Tie-breaking on the face diagonals is deliberately identical to the static
// path's if-chain. Where two magnitudes are equal the "along" axis is a coin
// toss, and picking differently there moves a colour discontinuity the
// original already had, for no reason.
//
// WITH NO SENSOR the per-frame recompute is skipped entirely and the table
// built at init stands - so this renders byte for byte the same as the plain
// entry rather than merely close to it.
//
// The wire reads slightly thinner once it is off-axis: an aligned edge lands
// on the fold between two faces and gets drawn on BOTH of them, while a wire
// crossing a face interior is drawn once. That is a property of the net, not
// of the maths, and it is why this entry defaults to a thicker line.
// ---------------------------------------------------------------------------
#define CW_P 4

static FX_RET mode_cube_wire_core(bool gyro) {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 6 || rows < 6) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const size_t n = (size_t)cols * rows;
  const size_t geo = gyro ? (5 * n) : (2 * n);        // ed, al [, cx, cy, cz]
  if (!SEGENV.allocateData(geo + CW_P * 5 + 8)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }

  uint8_t *ed = SEGENV.data;
  uint8_t *al = SEGENV.data + n;
  int8_t  *cx = gyro ? (int8_t *)(SEGENV.data + 2 * n) : nullptr;
  int8_t  *cy = gyro ? cx + n : nullptr;
  int8_t  *cz = gyro ? cy + n : nullptr;
  uint8_t *ps = SEGENV.data + geo;     // per pulse: origin, born lo/hi, strength, alive
  uint8_t *st = ps + CW_P * 5;         // [0] built [1..2] clock [3] round robin

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;

  const uint8_t want = (uint8_t)((cube ? 1 : 2) + (gyro ? 2 : 0));
  if (SEGENV.call == 0 || st[0] != want) {
    for (int y = 0; y < rows; y++) {
      for (int x = 0; x < cols; x++) {
        const size_t i = (size_t)y * cols + x;
        float X, Y, Z, edge, along;
        cfx_pos(x, y, cols, rows, B, cube, X, Y, Z);
        if (cube) {
          const float mx = fabsf(X), my = fabsf(Y), mz = fabsf(Z);
          if      (mx <= my && mx <= mz) { along = X; edge = 1.0f - ((my < mz) ? my : mz); }
          else if (my <= mx && my <= mz) { along = Y; edge = 1.0f - ((mx < mz) ? mx : mz); }
          else                           { along = Z; edge = 1.0f - ((mx < my) ? mx : my); }
        } else {
          const float mx = fabsf(X), my = fabsf(Y);
          edge  = 1.0f - ((mx > my) ? mx : my);
          along = (mx > my) ? Y : X;
        }
        if (edge < 0.0f) edge = 0.0f; else if (edge > 1.0f) edge = 1.0f;
        ed[i] = (uint8_t)(edge * 255.0f);
        al[i] = (uint8_t)(int)(along * 127.0f + 128.5f);
        if (gyro) { cx[i] = cfx_clamp8(X); cy[i] = cfx_clamp8(Y); cz[i] = cfx_clamp8(Z); }
      }
    }
    for (int k = 0; k < CW_P * 5 + 8; k++) ps[k] = 0;
    st[0] = want;
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];

  // Only the gyro entry touches the sensor, so selecting the plain one leaves
  // the driver in its trickle poll.
  const CfxImuState *imu = gyro ? &cfx_imu() : nullptr;
  const bool live = (imu != nullptr) && imu->valid;
  int16_t M[9];
  if (live) for (int k = 0; k < 9; k++) M[k] = imu->basis[k];
  else      for (int k = 0; k < 9; k++) M[k] = 0;

  const uint16_t nowT = (uint16_t)strip.now;
  const uint16_t dtW  = fx_dt8(st + 1);

  const int thick  = 12 + (SEGMENT.intensity >> 2);   // wire, 0.4..2.3 px
  const int growth = 2 + (SEGMENT.speed >> 4);        // al units per 23 ms
  const int pw     = 8 + (SEGMENT.intensity >> 4);    // pulse front width

  // A shove strikes the wire, same slot as the kick.
  uint8_t knock = peak;
  if (live && imu->shake > knock) knock = imu->shake;

  if (knock && SEGMENT.check1) {
    int slot = -1;
    for (int k = 0; k < CW_P; k++) if (!ps[k * 5 + 4]) { slot = k; break; }
    if (slot < 0) { slot = st[3] % CW_P; st[3] = (uint8_t)(st[3] + 1); }
    uint8_t *e = ps + slot * 5;
    e[0] = hw_random8();                              // struck somewhere on the wire
    e[1] = (uint8_t)(nowT & 0xFF); e[2] = (uint8_t)(nowT >> 8);
    e[3] = knock; e[4] = 1;
  }

  int pOrig[CW_P], pRad[CW_P], pStr[CW_P]; int live_p = 0;
  for (int k = 0; k < CW_P; k++) {
    uint8_t *e = ps + k * 5;
    if (!e[4]) continue;
    const uint16_t born = (uint16_t)e[1] | ((uint16_t)e[2] << 8);
    const int r = (int)(((uint32_t)(uint16_t)(nowT - born) * growth) / 23);
    if (r > 128 + pw) { e[4] = 0; continue; }
    pOrig[live_p] = e[0];
    pRad[live_p]  = r;
    pStr[live_p]  = ((int)e[3] * (128 + pw - r)) / (128 + pw);   // fade as it spreads
    live_p++;
  }

  uint8_t shape = (uint8_t)(SEGMENT.custom3 / 5);
  if (shape > 5) shape = 5;
  const uint8_t freq = 1 + (SEGMENT.custom1 >> 5);
  const uint8_t amp  = SEGMENT.custom2 >> 1;
  const uint8_t scrl = (uint8_t)((strip.now * (1 + (SEGMENT.speed >> 4))) >> 5);

  const uint8_t base = cfx_drive(vol, 1.4f, 45);
  const uint8_t hue  = (uint8_t)(strip.now >> 7);

  SEGMENT.fadeToBlackBy(SEGMENT.check2 ? fx_fade(26, dtW) : 255);

  CFX_NET_PREP();
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++, i++) {
      CFX_NET_SKIP(x);

      int d, av;
      if (live) {
        // follow this pixel's world direction out to the virtual frame
        const int px = cx[i], py = cy[i], pz = cz[i];
        const int w0 = ((int)M[0] * px + (int)M[1] * py + (int)M[2] * pz) >> 7;
        const int w1 = ((int)M[3] * px + (int)M[4] * py + (int)M[5] * pz) >> 7;
        const int w2 = ((int)M[6] * px + (int)M[7] * py + (int)M[8] * pz) >> 7;
        const int A0 = (w0 < 0) ? -w0 : w0;
        const int A1 = (w1 < 0) ? -w1 : w1;
        const int A2 = (w2 < 0) ? -w2 : w2;
        int wv, aj, ak;                               // ties to the lowest index,
        if (A0 <= A1 && A0 <= A2)      { wv = w0; aj = A1; ak = A2; }   // exactly
        else if (A1 <= A0 && A1 <= A2) { wv = w1; aj = A0; ak = A2; }   // as above
        else                           { wv = w2; aj = A0; ak = A1; }
        const int m2 = (aj < ak) ? aj : ak;
        const int m1 = (aj < ak) ? ak : aj;
        if (m1 < 8) { d = 255; av = 128; }            // dead centre of a flat
        else {                                        // panel - no direction
          const int inv = (255 << 8) / m1;
          d = 255 - ((m2 * inv) >> 8);
          if (d < 0) d = 0; else if (d > 255) d = 255;
          const int t = 128 + ((wv * inv) >> 9);
          av = (t < 0) ? 0 : ((t > 255) ? 255 : t);
        }
      } else {
        d = (int)ed[i]; av = (int)al[i];
      }
      const uint8_t avu = (uint8_t)av;

      int target = 0;
      if (shape) {
        const uint8_t w = cfx_wave(shape, (uint8_t)((uint16_t)avu * freq - scrl));
        target = (int)amp + ((int)amp * ((int)w - 128)) / 128;
      }
      int off = d - target; if (off < 0) off = -off;
      if (off >= thick) continue;                     // not on the wire
      const uint8_t prof = (uint8_t)(255 - (off * 255) / thick);

      uint8_t add = 0;
      for (int k = 0; k < live_p; k++) {
        const uint8_t dd = (uint8_t)(avu - (uint8_t)pOrig[k]);
        const int dist = (dd < 128) ? (int)dd : (256 - (int)dd);   // shortest way round
        int o = dist - pRad[k]; if (o < 0) o = -o;
        if (o >= pw) continue;
        add = qadd8(add, (uint8_t)((((pw - o) * 255) / pw * pStr[k]) >> 8));
      }

      const uint8_t lum = qadd8(scale8(prof, base), scale8(prof, add));
      SEGMENT.addPixelColorXY(x, y,
        SEGMENT.color_from_palette((uint8_t)(avu + hue + (add >> 2)),
                                   false, false, 0, lum));
    }
  }
  FX_DONE;
}

static FX_RET mode_cube_wire()      { mode_cube_wire_core(false); FX_DONE; }
static FX_RET mode_cube_wire_gyro() { mode_cube_wire_core(true);  FX_DONE; }

static const char _data_FX_MODE_CUBE_WIRE[] PROGMEM =
  "Ace 3-D Cube Wire@Speed,Thickness,Wave cycles,Wave height,Wave shape,Pulse on beat,Trails,Flat mode;;!;2f;sx=120,ix=90,c1=16,c2=150,c3=6,o1=1,o2=1";

static const char _data_FX_MODE_CUBE_WIRE_GYRO[] PROGMEM =
  "Ace Gyro Wire@Speed,Thickness,Wave cycles,Wave height,Wave shape,Pulse on hit,Trails,Flat mode;;!;2f;sx=120,ix=130,c1=16,c2=150,c3=6,o1=1,o2=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_CubeWireUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_cube_wire,      _data_FX_MODE_CUBE_WIRE);
    strip.addEffect(255, &mode_cube_wire_gyro, _data_FX_MODE_CUBE_WIRE_GYRO);
  }
  void loop() override {}
};

static CubeFx_CubeWireUsermod cube_fx_cube_wire;
REGISTER_USERMOD(cube_fx_cube_wire);
