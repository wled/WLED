#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 14. ACE 3-D QUESTION BLOCK
// ===========================================================================
// The cube IS the block: all five faces carry the same 16x16 sprite, which is
// the resolution the original was drawn at, so no scaling is needed.
//
// Cycle:  IDLE (glyph shimmers) -> SMASH (bursts outward) -> SPIN (item
// roulette, decelerating like a slot reel) -> RESULT (the item's own show,
// held for a duration that belongs to that item).
//
// The reel is rigged honestly: the number of steps is drawn first and the
// winner is simply steps % ITEMS, so the deceleration curve always lands
// exactly on the item it stops at.
//
// Each wall needs its own rotation or the sprite reads sideways or upside
// down once the net is folded. Those five numbers are in MQ_ROT.
// ---------------------------------------------------------------------------
#define MQ_ITEMS 6

// '.' transparent, '0'..'3' palette slots
static const char *const MQ_BLOCK[16] = {
  "0000000000000000","0111111111111110","0102222222222010","0122223333222210",
  "0122233223322210","0122233223322210","0122222233222210","0122222332222210",
  "0122223322222210","0122223322222210","0122222222222210","0122222332222210",
  "0122222332222210","0102222222222010","0111111111111110","0000000000000000"};
static const char *const MQ_MUSH[16] = {
  "................","................",".....000000.....","...0000000000...",
  "..002220022200..","..002220022200..",".000222002220000",".00000000000000.",
  ".00000000000000.","..000000000000..","...1111111111...","...1222222221...",
  "...1222222221...","...1111111111...","................","................"};
static const char *const MQ_STAR[16] = {
  "................",".......00.......","......0000......","......0000......",
  "0000000000000000",".00000000000000.","..000000000000..","...0000000000...",
  "...0000000000...","..00000..00000..","..0000....0000..",".000........000.",
  ".00..........00.","................","................","................"};
static const char *const MQ_FLOWER[16] = {
  "................","................","....00000000....","..000000000000..",
  ".00002222220000.",".00002222220000.","..000022220000..","....00000000....",
  "......1111......","......1111......","..111111111111..","......1111......",
  "......1111......","................","................","................"};
static const char *const MQ_COIN[16] = {
  "................","................","......0000......","....00000000....",
  "...0000000000...","...0000220000...","...0000220000...","...0000220000...",
  "...0000220000...","...0000220000...","...0000000000...","....00000000....",
  "......0000......","................","................","................"};
static const char *const MQ_BOLT[16] = {
  "................","..........000...",".........000....","........000.....",
  ".......000......","......00000.....",".....0000000....","........000.....",
  ".......000......","......000.......",".....000........","....000.........",
  "...000..........","................","................","................"};

static const char *const *const MQ_SPR[MQ_ITEMS] =
  { MQ_MUSH, MQ_FLOWER, MQ_STAR, MQ_COIN, MQ_BOLT, MQ_MUSH };
static const uint32_t MQ_PAL[MQ_ITEMS][4] = {
  { RGBW32(225, 40, 25,0), RGBW32(235,195,150,0), RGBW32(255,255,255,0), 0 },  // mushroom
  { RGBW32(255,110,  0,0), RGBW32( 30,185, 55,0), RGBW32(255,235, 70,0), 0 },  // fire flower
  { RGBW32(255,220,  0,0), RGBW32(255,255,255,0), RGBW32(255,150,  0,0), 0 },  // star
  { RGBW32(255,195,  0,0), RGBW32(165,110,  0,0), RGBW32(255,255,200,0), 0 },  // coin
  { RGBW32(255,240, 80,0), RGBW32(255,255,255,0), RGBW32(140,225,255,0), 0 },  // lightning
  { RGBW32( 35,205, 50,0), RGBW32(235,235,235,0), RGBW32(255,255,255,0), 0 }}; // 1-up
static const uint16_t MQ_DUR[MQ_ITEMS] = { 3000, 3600, 6500, 1600, 2200, 2600 };
static const uint32_t MQ_BPAL[4] = {
  RGBW32( 70, 35,  0,0), RGBW32(255,185, 70,0),
  RGBW32(230,140, 20,0), RGBW32(255,250,225,0) };
// TOP, NORTH, SOUTH, WEST, EAST  - quarter turns needed to stand each face up
static const uint8_t MQ_ROT[5] = { 0, 2, 0, 1, 3 };

// mq_scale() moved to cube_fx_common.h - shared by every effect from
// Question Block onward, so it now lives in one place.

static FX_RET mode_mario_block() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  if (cols < 8 || rows < 8) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  if (!SEGENV.allocateData(16)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  uint8_t *st = SEGENV.data;   // [0] phase [1..2] phase start [3] winner [4] steps

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  const int  fw   = cube ? B : cols;
  const int  fh   = cube ? B : rows;

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  const uint8_t *fft  = (uint8_t *)um->u_data[2];
  int bass, mid, treb;
  cfx_bands(fft, bass, mid, treb);

  const uint16_t nowT = (uint16_t)strip.now;
  if (SEGENV.call == 0) { st[0] = 0; st[1] = (uint8_t)nowT; st[2] = (uint8_t)(nowT >> 8); }
  const uint16_t t0 = (uint16_t)st[1] | ((uint16_t)st[2] << 8);
  const uint16_t el = (uint16_t)(nowT - t0);

  const int holdMs  = 1500 + (int)SEGMENT.custom1 * 14;
  const int smashMs = 700;
  const int spinMs  = 900 + (255 - (int)SEGMENT.speed) * 7;
  const int resMs   = ((int)MQ_DUR[st[3] % MQ_ITEMS] * (64 + (int)SEGMENT.custom2)) / 192;

  // --- advance the state machine -------------------------------------------
  bool step = false;
  switch (st[0]) {
    case 0: if (el > holdMs || (SEGMENT.check1 && peak && el > 600)) step = true; break;
    case 1: if (el > smashMs) step = true; break;
    case 2: if (el > spinMs)  step = true; break;
    default: if (el > resMs)  step = true; break;
  }
  if (step) {
    st[0] = (uint8_t)((st[0] + 1) & 3);
    if (st[0] == 2) {                       // entering the reel: draw the result
      st[4] = (uint8_t)(14 + hw_random16(18));
      st[3] = (uint8_t)(st[4] % MQ_ITEMS);
    }
    st[1] = (uint8_t)nowT; st[2] = (uint8_t)(nowT >> 8);
  }

  const int phase = st[0];
  const int item  = st[3] % MQ_ITEMS;
  const uint8_t drive = cfx_drive(vol, 1.0f, 110 + (SEGMENT.intensity >> 1));

  // --- per-phase setup ------------------------------------------------------
  int   shrink = 256, dropout = 0, whiteout = 0, bob = 0, hueSpin = 0;
  int   reelIdx = 0, reelFrac = 0;
  if (phase == 1) {
    const int u = (el * 255) / smashMs;                       // 0..255
    shrink   = 256 - (u * 190) / 255;                         // sprite flies outward
    dropout  = (u > 60) ? ((u - 60) * 255) / 195 : 0;
    whiteout = (u < 40) ? (255 - (u * 255) / 40) : 0;
    if (SEGMENT.custom3) dropout = (dropout * (int)SEGMENT.custom3) / 31;
  } else if (phase == 2) {
    const int32_t uu = ((int32_t)el << 8) / (spinMs ? spinMs : 1);   // 0..256
    const int32_t inv = 256 - ((uu > 256) ? 256 : uu);
    const int32_t f = ((int32_t)st[4] * (65536 - inv * inv)) >> 8;   // ease-out
    reelIdx  = (int)(f >> 8);
    reelFrac = (int)(f & 0xFF);
    if (reelIdx > st[4]) { reelIdx = st[4]; reelFrac = 0; }
  } else if (phase == 3) {
    bob = (int)((sin8_t((uint8_t)(strip.now >> 3)) - 128) * (fh / 12) / 128);
    if (item == 2) hueSpin = (int)(strip.now >> 3);            // star goes rainbow
  } else {
    if (SEGMENT.check2) bob = -(bass * (fh / 14)) / 255;       // block bounces on bass
  }

  SEGMENT.fill(SEGCOLOR(0));
  CFX_NET_PREP();
  for (int y = 0; y < rows; y++) {
    CFX_NET_ROW(y);
    for (int x = 0; x < cols; x++) {
      CFX_NET_SKIP(x);

      // face-local coords, rotated so every wall stands upright
      int lx = cube ? (x % B) : x, ly = cube ? (y % B) : y;
      int rot = 0;
      if (cube) {
        const int bx = x / B, by = y / B;
        rot = MQ_ROT[(by == 1 && bx == 1) ? 0 : (by == 0 ? 1 : (by == 2 ? 2 : (bx == 0 ? 3 : 4)))];
      }
      int sx = lx, sy = ly;
      if      (rot == 1) { sx = ly;          sy = fw - 1 - lx; }
      else if (rot == 2) { sx = fw - 1 - lx; sy = fh - 1 - ly; }
      else if (rot == 3) { sx = fh - 1 - ly; sy = lx;          }

      sy -= bob;
      // map the face onto the 16x16 art
      int gx = (sx * 16) / fw, gy = (sy * 16) / fh;

      char ch = '.';
      const uint32_t *pal = MQ_BPAL;
      uint8_t lum = drive;

      if (phase == 1) {                                        // SMASH
        int mx = 8 + ((gx - 8) * 256) / shrink;
        int my = 8 + ((gy - 8) * 256) / shrink;
        if (mx >= 0 && mx < 16 && my >= 0 && my < 16) {
          if ((int)hw_random8() >= dropout) ch = MQ_BLOCK[my][mx];
        }
        lum = (uint8_t)(((int)drive * (255 - (el * 200) / smashMs)) >> 8);
      } else if (phase == 2) {                                 // SPIN
        const int shift = (reelFrac * 16) >> 8;
        int rg = gy + shift;
        const int i0 = reelIdx % MQ_ITEMS, i1 = (reelIdx + 1) % MQ_ITEMS;
        const int which = (rg < 16) ? i0 : i1;
        if (rg >= 16) rg -= 16;
        ch  = MQ_SPR[which][rg][gx];
        pal = MQ_PAL[which];
      } else if (phase == 3) {                                 // RESULT
        if (gy >= 0 && gy < 16) ch = MQ_SPR[item][gy][gx];
        pal = MQ_PAL[item];
        if (ch == '.') {                                       // item-tinted glow
          const int r = ((gx - 8) * (gx - 8) + (gy - 8) * (gy - 8));
          const uint8_t g = (uint8_t)((r > 128) ? 0 : (128 - r));
          uint32_t bgc = pal[0];
          if (item == 2) bgc = SEGMENT.color_from_palette((uint8_t)(hueSpin + r), false, false, 0);
          SEGMENT.setPixelColorXY(x, y, mq_scale(bgc, scale8(g, drive)));
          continue;
        }
        if (item == 2) lum = (uint8_t)qadd8(drive, sin8_t((uint8_t)(hueSpin << 2)) >> 1);
      } else {                                                 // IDLE
        if (gy >= 0 && gy < 16) ch = MQ_BLOCK[gy][gx];
        if (ch == '3') lum = (uint8_t)qadd8(drive, sin8_t((uint8_t)(strip.now >> 3)) >> 2);
      }

      if (ch == '.' || ch < '0' || ch > '3') continue;
      uint32_t c = pal[ch - '0'];
      if (whiteout) c = RGBW32(qadd8((uint8_t)(c >> 16), whiteout),
                               qadd8((uint8_t)(c >> 8),  whiteout),
                               qadd8((uint8_t)c,         whiteout), 0);
      SEGMENT.setPixelColorXY(x, y, mq_scale(c, lum));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_MARIO_BLOCK[] PROGMEM =
  "Ace 3-D Question Block@Reel speed,Brightness,Hold time,Item length,Shatter,Smash on beat,Bass bounce,Flat mode;;;2f;sx=140,ix=140,c1=110,c2=128,c3=24,o1=1,o2=1";

// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_MarioBlockUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_mario_block, _data_FX_MODE_MARIO_BLOCK);
  }
  void loop() override {}
};

static CubeFx_MarioBlockUsermod cube_fx_mario_block;
REGISTER_USERMOD(cube_fx_mario_block);
