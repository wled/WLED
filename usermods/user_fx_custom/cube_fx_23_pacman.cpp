#include "wled.h"
#include "cube_fx_common.h"

// ===========================================================================
// 23. ACE 3-D PAC-MAN
// ===========================================================================
// The arcade original drew Pac-Man about 13 px wide on an 8 px tile grid - the
// sprites were always bigger than the maze they moved through. That is what
// makes a detailed version possible here rather than a one-pixel blob.
//
// The maze is two corridors running right around the wall cylinder, joined by
// four connectors, so it wraps with no tunnel needed - the whole ring IS the
// tunnel. Pac-Man is drawn as a real wedge with a mouth that opens and closes
// toward his heading, and the ghosts get a domed head, a wavy skirt and pupils
// that look the way they are travelling.
// ---------------------------------------------------------------------------
#define PM_G 4
static const char *const PM_GH[6] = {
  ".000.", "00000", "00000", "00000", "00000", "0.0.0" };

static inline int pm_wrap(int d, int bw) {
  if (d >  bw / 2) d -= bw;
  if (d < -bw / 2) d += bw;
  return d;
}

static FX_RET mode_pacman() {
  if (!strip.isMatrix || !SEGMENT.is2D()) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int cols = SEG_W, rows = SEG_H;
  const size_t n = (size_t)cols * rows;
  if (!SEGENV.allocateData(2 * n + 160 + 48)) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  uint8_t *bu = SEGENV.data, *bv = bu + n;
  uint8_t *dot = bv + n;                    // 2 lanes x 64 columns
  uint8_t *st  = dot + 160;
  // st: [0] built [1..2] pacU 8.8 [3] pacLane 8.8 hi [4] pacDir [5] fright
  //     [6..7] clock [8] death [9..10] spare, [16..47] ghosts: u lo,hi,lane,dir,mode,tmr

  const bool cube = cfx_isCube(cols, rows);
  const int  B    = cube ? (cols / 3) : 1;
  const int  bw   = cube ? (4 * B) : cols;
  const int  bh   = cube ? B : rows;
  if (bw < 24 || bh < 12) { SEGMENT.fill(SEGCOLOR(0)); FX_DONE; }
  const int  topC = bh / 4, botC = bh - 1 - bh / 4;
  const int  conn[4] = { bw / 8, (3 * bw) / 8, (5 * bw) / 8, (7 * bw) / 8 };
  // The gate sits on the NORTH wall, whose band column equals the top face's
  // own column, so eyes cross the fold without any coordinate juggling.
  const int  gate = bw / 8;
  const int  penC = B / 2;

  const uint16_t nowT = (uint16_t)strip.now;
  if (SEGENV.call == 0 || st[0] != (uint8_t)(cube ? 1 : 2)) {
    cfx_buildBand(bu, bv, cols, rows, cube, B);
    for (int k = 0; k < 48; k++) st[k] = 0;
    st[0] = (uint8_t)(cube ? 1 : 2);
    for (int L = 0; L < 2; L++)
      for (int u = 0; u < bw && u < 80; u++)
        dot[L * 80 + u] = (uint8_t)((u % 3 == 1) ? 1 : 0);
    for (int k = 0; k < 4; k++) { if (conn[k] < 80) { dot[conn[k]] = 2; dot[80 + conn[k]] = 2; } }
    st[1] = 0; st[2] = 0; st[4] = 1;
    for (int g = 0; g < PM_G; g++) {
      uint8_t *G = st + 16 + g * 6;
      const int gu = (g * bw) / PM_G;
      G[0] = (uint8_t)((gu << 8) & 0xFF); G[1] = (uint8_t)(gu);
      G[2] = (uint8_t)((g & 1) ? 255 : 0); G[3] = (uint8_t)((g & 2) ? 1 : 255); G[4] = 0; G[5] = 0;
    }
  }

  um_data_t     *um   = cfx_getAudioData();
  const uint8_t  peak = fx_lowBeat(um);
  const float    vol  = *(float *)um->u_data[0];
  const uint16_t dtP  = fx_dt8(st + 6);

  const int boost   = (SEGMENT.check1 && peak) ? 140 : 0;
  const int pacSpd  = ((28 + (int)SEGMENT.speed / 4 + boost) * (int)dtP) / 24;
  const int ghoSpd  = ((22 + (int)SEGMENT.custom1 / 5 + boost) * (int)dtP) / 24;

  int pacU = ((int)st[1] | ((int)st[2] << 8));
  int pacL = (int)st[3];
  int pacD = (st[4] == 1) ? 1 : -1;

  // scatter / chase alternation, as the arcade did - without it four chasers
  // on a closed ring pincer you every time and the game is unwinnable
  const uint16_t mdue = (uint16_t)st[12] | ((uint16_t)st[13] << 8);
  if ((uint16_t)(nowT - mdue) < 32768) {
    st[14] ^= 1;
    const uint16_t mn = nowT + (uint16_t)(st[14] ? 5000 : 8000);
    st[12] = (uint8_t)mn; st[13] = (uint8_t)(mn >> 8);
  }
  const bool scatter = st[14] != 0;
  if (st[11]) { const int iv = (int)st[11] - (int)fx_step(5, dtP);
                st[11] = (uint8_t)((iv < 0) ? 0 : iv); }

  if (st[8]) {                                              // death spin
    const int d2 = (int)st[8] - (int)fx_step(6, dtP);
    st[8] = (uint8_t)((d2 < 0) ? 0 : d2);
    if (!st[8]) {
      pacU = 0; pacL = 0; st[5] = 0; st[11] = 255;          // grace period
      for (int g = 0; g < PM_G; g++) {                      // send them to the far side
        uint8_t *G = st + 16 + g * 6;
        const int gu2 = ((bw / 2) + g * 3) % bw;
        G[0] = 0; G[1] = (uint8_t)gu2; G[2] = (uint8_t)((g & 1) ? 255 : 0);
        G[4] = 0; G[5] = 0;
      }
    }
  } else {
    // evade: look for the nearest ghost AHEAD on this lane and get out of the way
    int threat = 9999;
    for (int g = 0; g < PM_G; g++) {
      const uint8_t *G = st + 16 + g * 6;
      if (G[4] == 2 || st[5] > 40) continue;                // eaten or edible
      const int gl = (int)G[2];
      if ((gl > 127) != (pacL > 127)) continue;             // different corridor
      const int d = pm_wrap((((int)G[0] | ((int)G[1] << 8)) >> 8) - (pacU >> 8), bw) * pacD;
      if (d > 0 && d < threat) threat = d;
    }
    pacU = (pacU + pacD * pacSpd + (bw << 8)) % (bw << 8);
    const int pu = pacU >> 8;
    bool atConn = false;
    for (int k = 0; k < 4; k++) { const int d = pm_wrap(pu - conn[k], bw);
                                  if (d > -2 && d < 2) atConn = true; }
    if (threat < 16 && atConn) st[9] = (uint8_t)((pacL > 127) ? 0 : 1);   // duck through
    else if (threat < 7)       { pacD = -pacD; st[4] = (uint8_t)((pacD > 0) ? 1 : 0); }
    else for (int k = 0; k < 4; k++)                        // otherwise wander
      if (pu == conn[k] && hw_random16(4) == 0) st[9] = (uint8_t)(pacL < 128 ? 1 : 0);
    const int want = st[9] ? 255 : 0;
    if (pacL < want) pacL += 24; else if (pacL > want) pacL -= 24;
    if (pacL < 0) pacL = 0; if (pacL > 255) pacL = 255;

    const int lane = (pacL > 127) ? 1 : 0;                  // eat
    if (pu < 80 && dot[lane * 80 + pu]) {
      if (dot[lane * 80 + pu] == 2) st[5] = 255;
      dot[lane * 80 + pu] = 0;
    }
    int left = 0;
    for (int k = 0; k < 160; k++) if (dot[k]) left++;
    if (!left) for (int L = 0; L < 2; L++)
      for (int u = 0; u < bw && u < 80; u++) dot[L * 80 + u] = (uint8_t)((u % 3 == 1) ? 1 : 0);
  }
  if (st[5]) { const int f = (int)st[5] - (int)fx_step(3 + (255 - SEGMENT.custom2) / 24, dtP);
               st[5] = (uint8_t)((f < 0) ? 0 : f); }
  st[1] = (uint8_t)(pacU & 0xFF); st[2] = (uint8_t)(pacU >> 8); st[3] = (uint8_t)pacL;

  const int pacPix = pacU >> 8;
  const int pacV   = topC + ((botC - topC) * pacL) / 255;

  // --- ghosts ---------------------------------------------------------------
  for (int g = 0; g < PM_G; g++) {
    uint8_t *G = st + 16 + g * 6;
    int gu = ((int)G[0] | ((int)G[1] << 8));
    if (G[4] == 2) {                                        // eaten: eyes going home
      const int pr = (int)G[5] + (int)fx_step(3, dtP);
      if (pr >= 255) {                                      // released from the pen
        G[4] = 0; G[5] = 0; G[2] = 0; G[0] = 0; G[1] = (uint8_t)gate;
      } else {
        G[5] = (uint8_t)pr;
        if (pr < 80) {                                      // run the band to the gate
          const int d = pm_wrap(gate - (gu >> 8), bw);
          gu = (gu + ((d >= 0) ? 1 : -1) * ghoSpd * 2 + (bw << 8)) % (bw << 8);
          G[2] = 0;                                         // hug the upper corridor
        }
        G[0] = (uint8_t)(gu & 0xFF); G[1] = (uint8_t)(gu >> 8);
      }
      continue;
    }
    // Each ghost aims somewhere different, so they surround rather than stack:
    // one goes straight for him, one cuts ahead, one hangs back, one holds the
    // opposite side of the ring.
    int tgt;
    if (st[5] > 40)      tgt = pacPix + bw / 2;             // frightened: run away
    else if (scatter)    tgt = (g * bw) / PM_G;             // scatter: own corner
    else switch (g & 3) {
      case 0:  tgt = pacPix;                 break;
      case 1:  tgt = pacPix + 7 * pacD;      break;
      case 2:  tgt = pacPix - 9 * pacD;      break;
      default: tgt = pacPix + bw / 4;        break;
    }
    const int away = pm_wrap((gu >> 8) - pacPix, bw);
    const int toT  = pm_wrap(tgt - (gu >> 8), bw);
    const int dir  = (toT >= 0) ? 1 : -1;
    gu = (gu + dir * ghoSpd + (bw << 8)) % (bw << 8);
    const int gp = gu >> 8;
    for (int k = 0; k < 4; k++) { const int dc = pm_wrap(gp - conn[k], bw);
      if (dc > -2 && dc < 2) {
        const int t = (st[5] > 40 || scatter) ? ((pacL > 127) ? 0 : 255)
                                              : ((pacL > 127) ? 255 : 0);
        G[2] = (uint8_t)t;
      }
    }
    const int gl = G[2];
    if (!st[8] && !st[11] && (away > -3 && away < 3)) {
      const int gv = topC + ((botC - topC) * gl) / 255;
      if (gv > pacV - 3 && gv < pacV + 3) {
        if (st[5] > 40) { G[4] = 2; G[5] = 60; }            // Pac wins
        else st[8] = 255;                                   // ghost wins
      }
    }
    G[0] = (uint8_t)(gu & 0xFF); G[1] = (uint8_t)(gu >> 8);
    G[3] = (uint8_t)((dir > 0) ? 1 : 255);
  }

  const uint8_t drive = cfx_drive(vol, 1.0f, 140 + (SEGMENT.intensity >> 1));
  const uint8_t wallL = (uint8_t)(30 + (SEGMENT.custom3 * 5));
  // Half-angle as a cosine in 1/256ths: 250 is shut, 176 is about 45 degrees.
  // The old test needed cos^2 > 1.6, which is impossible - so it never opened.
  const int mCos = 250 - (int)((sin8_t((uint8_t)strip.now) * 74) >> 8);
  const uint32_t GCOL[4] = { RGBW32(255,60,60,0), RGBW32(255,160,220,0),
                             RGBW32(70,230,235,0), RGBW32(255,170,60,0) };

  SEGMENT.fill(SEGCOLOR(0));
  size_t i = 0;
  for (int y = 0; y < rows; y++) {
    for (int x = 0; x < cols; x++, i++) {
      if (bu[i] == 255) {
        if (!cube) continue;
        const int bx = x / B, by = y / B;
        if (bx != 1 || by != 1) continue;               // gap corner
        const int tx = x % B, ty = y % B;               // the top face
        uint32_t tc = 0; int tl = 0;
        if (tx == 0 || ty == 0 || tx == B - 1 || ty == B - 1) {
          tc = RGBW32(20, 30, 190, 0); tl = wallL;      // maze border carries over
        }
        const int px2 = tx - penC, py2 = ty - penC;
        const int ap = (px2 < 0) ? -px2 : px2, aq = (py2 < 0) ? -py2 : py2;
        if ((ap == 3 || aq == 3) && ap <= 3 && aq <= 3
            && !(py2 == -3 && ap <= 1)) {               // pen wall, gate on the north side
          tc = RGBW32(255, 130, 200, 0); tl = 120;
        }
        for (int g = 0; g < PM_G; g++) {                // eyes arriving or waiting
          const uint8_t *G = st + 16 + g * 6;
          if (G[4] != 2 || G[5] < 80) continue;
          int ey;
          if      (G[5] < 130) ey = ((int)G[5] - 80) * penC / 50;
          else if (G[5] < 190) ey = penC;
          else                 ey = penC - ((int)G[5] - 190) * penC / 50;
          const int dxe = tx - gate, dye = ty - ey;
          if (dxe < -1 || dxe > 1 || dye < -1 || dye > 0) continue;
          tc = (dxe == 0) ? RGBW32(30, 30, 140, 0) : RGBW32(255, 255, 255, 0);
          tl = 255;
        }
        if (tl) SEGMENT.setPixelColorXY(x, y, mq_scale(tc, scale8((uint8_t)tl, drive)));
        continue;
      }
      const int u = (int)bu[i], v = (int)bv[i];
      uint32_t c = 0; int lvl = 0;

      const bool inTop = (v >= topC - 2 && v <= topC + 2);
      const bool inBot = (v >= botC - 2 && v <= botC + 2);
      bool inConn = false;
      for (int k = 0; k < 4; k++)
        if (v > topC && v < botC) { const int d = pm_wrap(u - conn[k], bw);
                                    if (d > -2 && d < 2) inConn = true; }
      if (!inTop && !inBot && !inConn) { c = RGBW32(20, 30, 190, 0); lvl = wallL; }
      else {
        if (u < 80) {                                        // pellets
          const uint8_t d0 = (inTop && v == topC) ? dot[u] : ((inBot && v == botC) ? dot[80 + u] : 0);
          if (d0 == 1) { c = RGBW32(255, 220, 170, 0); lvl = 150; }
          else if (d0 == 2) { c = RGBW32(255, 230, 190, 0);
                              lvl = 120 + (sin8_t((uint8_t)(strip.now >> 2)) >> 1); }
        }
      }

      for (int g = 0; g < PM_G; g++) {                       // ghosts
        const uint8_t *G = st + 16 + g * 6;
        const int gp = ((int)G[0] | ((int)G[1] << 8)) >> 8;
        const int gv = topC + ((botC - topC) * (int)G[2]) / 255;
        const int dx = pm_wrap(u - gp, bw), dy = v - gv;
        if (dx < -2 || dx > 2 || dy < -3 || dy > 2) continue;
        const char ch = PM_GH[dy + 3][dx + 2];
        if (ch != '0') continue;
        const bool fright = (st[5] > 40) && G[4] != 2;
        if (G[4] == 2) {
          if (G[5] >= 80) continue;                          // already up on the top face
          c = 0; lvl = 0;                                    // body gone, eyes only below
        }
        else { c = fright ? (((st[5] < 90) && ((strip.now >> 7) & 1)) ? RGBW32(240,240,255,0)
                                                                     : RGBW32(40,60,240,0))
                          : GCOL[g & 3];
               lvl = 255; }
        if (dy <= -1 && dy >= -2 && (dx == -1 || dx == 1)) { // eyes
          const int look = ((int)(int8_t)G[3] > 0) ? 1 : -1;
          c = RGBW32(255,255,255,0); lvl = 255;
          if (dx == look) { c = RGBW32(20,20,120,0); }
        }
      }

      if (!st[8]) {                                          // Pac-Man
        const int dx = pm_wrap(u - pacPix, bw), dy = v - pacV;
        const int r2 = dx * dx + dy * dy;
        if (r2 <= 7) {
          const int dp = dx * pacD;                        // heading runs along u
          //  dp / sqrt(r2) > mCos/256   ->   dp^2 * 65536 > r2 * mCos^2
          const bool inMouth = (dp > 0) && (dp * dp * 65536 > r2 * mCos * mCos);
          if (!inMouth) { c = RGBW32(255, 235, 40, 0);
                          lvl = (st[11] && ((strip.now >> 6) & 1)) ? 70 : 255; }
        }
      } else {
        const int dx = pm_wrap(u - pacPix, bw), dy = v - pacV;
        const int rr = 1 + ((int)st[8] * 2) / 255;
        if (dx * dx + dy * dy <= rr * rr) { c = RGBW32(255, 235, 40, 0); lvl = st[8]; }
      }

      if (lvl) SEGMENT.setPixelColorXY(x, y, mq_scale(c, scale8((uint8_t)lvl, drive)));
    }
  }
  FX_DONE;
}

static const char _data_FX_MODE_PACMAN[] PROGMEM =
  "Ace 3-D Pac-Man@Speed,Brightness,Ghost speed,Fright time,Maze glow,Beat boost,,Flat mode;;;2f;sx=140,ix=175,c1=130,c2=140,c3=8,o1=1";


// ---------------------------------------------------------------------------
// Registration - self-contained, so adding a new effect never means editing
// another file. Each cube_fx_*.cpp registers only its own effect(s).
// ---------------------------------------------------------------------------
class CubeFx_PacmanUsermod : public Usermod {
 public:
  void setup() override {
    strip.addEffect(255, &mode_pacman, _data_FX_MODE_PACMAN);
  }
  void loop() override {}
};

static CubeFx_PacmanUsermod cube_fx_pacman;
REGISTER_USERMOD(cube_fx_pacman);
