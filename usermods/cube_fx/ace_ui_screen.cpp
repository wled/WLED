#include "wled.h"
#include "ace_ui_bus.h"

// ===========================================================================
// ace_ui_screen.cpp - u8g2 panel that cannot cost you frames
// ===========================================================================
// Needs the library. Add to platformio_override.ini, in your env:
//
//     lib_deps = ${env.lib_deps}
//                olikraus/U8g2 @ ^2.35.19
//
// Without it this file compiles to nothing at all (see __has_include below),
// so a missing lib_deps line is a screen that does not appear, not a build
// that fails at 90%.
//
// ---------------------------------------------------------------------------
// THE TWO-COLOUR PANEL, USED ON PURPOSE
// ---------------------------------------------------------------------------
// The 0.96" parts in this build are not monochrome. Rows 0-15 are yellow, rows
// 16-63 are blue, and there is an unlit physical gap between the two. That is
// not a defect, it is how the glass is made - one phosphor for the top strip
// and another for the rest.
//
// So the yellow band is a fixed header and nothing else ever draws there:
//
//     rows  0.. 7   the RUNNING EFFECT, on every screen, scrolled if long
//     rows  8..15   where you are (Menu / Palette / Params ...) and how far
//                   through, or brightness when there is nothing to say
//     rows 16..63   the body - list, value editor, status, info page
//
// The IP address used to sit on row 0 of the front page. It is gone; it lives
// on System -> Network now, with the SSID and the key, at a size you can read
// from more than a foot away. Nothing about an IP address deserves permanent
// residency in the one part of the display you always look at.
//
// ---------------------------------------------------------------------------
// THE ARITHMETIC THAT DRIVES EVERY OTHER DESIGN CHOICE HERE
// ---------------------------------------------------------------------------
// The panel sits on Wire1, GPIO23/22, on its own peripheral; the IMU keeps the
// shared bus (SDA 21 / SCL 13) to itself. At 400 kHz and 9 bits per byte that
// is 22.5 us per byte:
//
//     one 128-byte tile row        ~2.9 ms      12% of a 23 ms frame
//     a full 1024-byte refresh    ~23.0 ms      exactly one dropped frame
//
// So a full sendBuffer() is never acceptable, and that single fact is why the
// stock four-line display stutters the strip. Instead: draw into a RAM
// framebuffer, diff it against a shadow copy, and send only the tile rows that
// actually changed - at most two or three per frame, ranked so the thing you
// just turned the knob at goes first and the VU band goes last.
//
// A static screen costs nothing. A brightness twiddle costs two pages. Only
// the VU meter is a continuous expense, which is why it has a governor rather
// than an on switch.
//
// ---------------------------------------------------------------------------
// TWO DIFFERENT VU METERS, AND THEY ARE NOT THE SAME FEATURE
// ---------------------------------------------------------------------------
//   THE BAND   one tile row at the bottom of Now Playing. Governed by vuMode,
//              drawn last out of whatever budget survived, dropped without
//              ceremony when the cube needs the bus. It is a decoration.
//
//   THE SCREEN AUI_V_VU. The whole body below the yellow band, sixteen bands
//              at full height with peak-hold caps. Reached from the main menu,
//              and selectable as the screen the panel boots into. It is not
//              governed by vuMode, because you did not stumble onto it - you
//              navigated to it, and a meter that switches itself off is not a
//              meter. It gets a raised page ceiling and it holds the panel
//              awake, both for the same reason: nothing else is on screen.
//
// The full screen is six tile rows on a 64-row panel, which is 768 bytes - more
// than one frame's budget. Two things make that work rather than tear:
// nextRow() scans from a ROTATING cursor instead of top-down, so the bottom of
// the meter cannot starve behind the top; and the bars only dirty the rows they
// actually cross, so quiet music costs two rows, not six.
//
// ---------------------------------------------------------------------------
// WAKE, DIM, SLEEP
// ---------------------------------------------------------------------------
//     touched          -> briFull  (75%)
//     idle > dimSec    -> briDim   (35%), and scrolling stops
//     idle > sleepSec  -> panel off entirely, and the first input back is
//                         swallowed as a wake rather than acted on
//
// The dim step matters more than it sounds: at 25% the panel stops being the
// brightest thing in a dark room and stops washing out the cube, and because
// scrolling stops with it the bus goes completely silent between interactions.
// All four numbers are on the settings page.
//
// ---------------------------------------------------------------------------
// WHY THE CONTRAST SETTING DID NOTHING - TWO SEPARATE FAULTS
// ---------------------------------------------------------------------------
// 1. THE SENTINEL WAS A LEGAL VALUE.  curContrast was a uint8_t seeded to 255,
//    and both bringUp() and readFromConfig() "forced" a rewrite by setting it
//    back to 255 before calling setContrastPct(). But 255 IS 100%. So with
//    briFull at 100 the guard `if (c == curContrast) return;` matched on the
//    very write that was supposed to be forced, the command never went out,
//    and the panel sat at the SSD1306 reset default of 0x7F forever. The
//    sentinel is -1 in an int16_t now, which is not a contrast anybody can ask
//    for.
//
// 2. LINEAR PERCENT IS THE WRONG CURVE, AND CONTRAST IS THE WRONG REGISTER ON
//    ITS OWN.  0x81 sets segment drive CURRENT, and the eye is roughly
//    logarithmic, so a linear map put 25% at 64 and 75% at 191 - a 3:1 ratio
//    on paper that looks like maybe 1.3:1 on glass. Worse, the SSD1306's real
//    brightness floor is set by the Vcomh deselect level (0xDB), not by 0x81
//    at all: leave Vcomh at its 0x20 default and there is no value of contrast
//    that gives you a genuinely dim panel. So the percentage is squared to
//    make it perceptual, and Vcomh is stepped down with it - 0x30 bright,
//    0x20 mid, 0x00 dim. That is the difference between a dim step you have to
//    be told about and one you can see across the room.
//
//    Turn `deepDim` off if a particular panel dislikes the 0xDB write; you get
//    the gamma without the Vcomh step and the setting still works, just with
//    less range at the bottom.
//
// ---------------------------------------------------------------------------
// IF SOMETHING IS DARK
// ---------------------------------------------------------------------------
// Info tells the truth, because this file owns its I2C byte callback and
// therefore checks the ACK that u8g2's own callback discards:
//
//   "no ACK - check VCC, pin order, address"   nothing is answering at 0x3C
//   "Wire1 pins clash..."                      w1Sda/w1Scl overlap the shared
//                                              pair. two masters, one bus
//   "Panel NAKs: n"                            it answered once and stopped.
//                                              wiring or rise time, not code
// ===========================================================================

#if __has_include(<U8g2lib.h>)
#include <U8g2lib.h>
#include <Wire.h>

#ifndef AUI_LEGACY_PINMGR
  #define AUI_LEGACY_PINMGR 0
#endif

enum : uint8_t { AUI_BUS_WIRE = 0, AUI_BUS_WIRE1 = 1, AUI_BUS_SPI = 2 };
enum : uint8_t { AUI_VU_OFF = 0, AUI_VU_ON = 1, AUI_VU_AUTO = 2 };

// How the dedicated VU screen draws its sixteen bands. Blocks is not just a
// look: quantising the tips to a 4 px pitch means a band has to move a whole
// segment before its tile row goes dirty, which roughly halves the bytes the
// meter costs. On a build that is already tight on bus, it is the one to pick.
enum : uint8_t { AUI_VUS_BARS = 0, AUI_VUS_MIRROR = 1, AUI_VUS_BLOCKS = 2 };

// Vcomh deselect level, SSD1306/SH1106/SSD1309 register 0xDB. The three values
// the datasheet defines are 0.65, 0.77 and 0.83 x Vcc; anything else is
// reserved, so these are the only three worth writing.
#define AUI_CMD_VCOMH   0x0DB
#define AUI_VCOMH_LOW   0x00
#define AUI_VCOMH_MID   0x20
#define AUI_VCOMH_HIGH  0x30

static bool auiAllocPin(int8_t p) {
  if (p < 0) return true;
#if AUI_LEGACY_PINMGR
  return pinManager.allocatePin((byte)p, true, PinOwner::UM_Unspecified);
#else
  return PinManager::allocatePin((byte)p, true, PinOwner::UM_Unspecified);
#endif
}

// --- the panel's own I2C lane ----------------------------------------------
// u8g2 ships a "2ND_HW_I2C" family of constructors for Wire1, but whether they
// exist at all depends on a platform-detection #ifdef that has moved around
// between u8g2 releases and Arduino cores. Betting the build on it is not
// worth it. Instead we construct the ordinary HW_I2C class - which is always
// there - and swap out its byte callback for this one before begin().
//
// Two things fall out of owning the callback that we could not get otherwise:
//
//   1. Wire1 gets its own pins and its own clock, with no chance of u8g2
//      quietly re-begin()ing the shared bus underneath the MPU6050.
//   2. WE SEE THE ACK. u8g2's own callback throws away endTransmission()'s
//      return value, which is why a panel that was never wired up still
//      reported "ok" and kept the governor pushing 128-byte pages into an
//      empty bus. Now a missing panel is visible, counted, and retried.
static int8_t   auiW1Sda = -1, auiW1Scl = -1;
static uint32_t auiW1Hz  = 400000;
static bool     auiAck   = true;
static uint32_t auiNak   = 0;

static uint8_t aui_byte_wire1(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
  static uint8_t chunk = 0;
  uint8_t *data;
  switch (msg) {
    case U8X8_MSG_BYTE_SEND:
      data = (uint8_t *)arg_ptr;
      while (arg_int > 0) {
        // Arduino's I2C buffer is 128 bytes and one tile row is 128 data bytes
        // plus a control byte, so a page would overflow it by exactly one.
        // Break at 32 with a repeated start, which is what u8g2's own callback
        // does for Wire and why nobody notices the limit.
        if (chunk >= 32) {
          Wire1.endTransmission(false);
          Wire1.beginTransmission((uint8_t)(u8x8_GetI2CAddress(u8x8) >> 1));
          chunk = 0;
        }
        Wire1.write(*data++);
        arg_int--; chunk++;
      }
      break;
    case U8X8_MSG_BYTE_INIT:
      Wire1.begin((int)auiW1Sda, (int)auiW1Scl, auiW1Hz);
      break;
    case U8X8_MSG_BYTE_SET_DC:
      break;
    case U8X8_MSG_BYTE_START_TRANSFER:
      Wire1.beginTransmission((uint8_t)(u8x8_GetI2CAddress(u8x8) >> 1));
      chunk = 0;
      break;
    case U8X8_MSG_BYTE_END_TRANSFER:
      auiAck = (Wire1.endTransmission() == 0);
      if (!auiAck) auiNak++;
      break;
    default:
      return 0;
  }
  return 1;
}

// --- panel factory ----------------------------------------------------------
// All u8g2 classes derive from U8G2, and every call this file makes is on that
// base, so one pointer covers every combination. Constructed in setup() rather
// than statically because the pins are runtime config.
static U8G2 *auiMake(uint8_t panel, uint8_t bus, const u8g2_cb_t *rot,
                     int8_t sda, int8_t scl, int8_t cs, int8_t dc, int8_t rst) {
  #define AUI_PANEL(N, T)                                                        \
    case N:                                                                      \
      switch (bus) {                                                             \
        case AUI_BUS_SPI:                                                        \
          return new U8G2_##T##_F_4W_HW_SPI(rot, (uint8_t)cs, (uint8_t)dc,       \
                                            rst < 0 ? U8X8_PIN_NONE : (uint8_t)rst); \
        default:                                                                 \
          return new U8G2_##T##_F_HW_I2C(rot, U8X8_PIN_NONE,                     \
                                         (uint8_t)scl, (uint8_t)sda);            \
      }
  switch (panel) {
    AUI_PANEL(0, SSD1306_128X64_NONAME)
    AUI_PANEL(1, SH1106_128X64_NONAME)
    AUI_PANEL(2, SSD1309_128X64_NONAME0)
    AUI_PANEL(3, SSD1306_128X32_UNIVISION)
  }
  #undef AUI_PANEL
  return nullptr;
}

class AceUiScreenUsermod : public Usermod {
 private:
  // --- persisted config -----------------------------------------------------
  bool enabled  = true;
  int  panel    = 0;
  int  busSel   = AUI_BUS_WIRE1;   // the panel gets its own lane by default
  int  addr     = 0x3C;
  int  rot180   = 0;
  int  split    = 16;         // pixels of yellow header band. 0 = single zone
  int  briFull  = 75;         // % contrast while you are using it
  int  briDim   = 35;         // % contrast once it has been left alone
  int  dimSec   = 10;         // idle seconds before dimming
  int  sleepSec = 30;         // idle seconds before blanking. 0 = never
  bool deepDim  = true;       // step Vcomh with contrast. see the header note
  int  vuMode   = AUI_VU_AUTO;   // governs the BAND on Now Playing only
  int  vuStyle  = AUI_VUS_BARS;  // the dedicated VU screen's look
  int  vuHz     = 30;            // meter refresh rate, both band and screen
  bool vuPeak   = true;          // peak-hold caps on the dedicated screen
  bool vuAwake  = true;          // the VU screen ignores dim and sleep
  int  idleHome = 0;             // sec before the menu walks home. 0 = never
  int  startScr = AUI_START_MENU;
  int  fpsFloor = 35;   // gate is fpsFloor+3; 40 was unreachable at 43 fps
  int  maxPages = 3;
  bool scroll   = true;
  int  spiCs = -1, spiDc = -1, spiRst = -1;
  int  w1Sda = 23, w1Scl = 22;      // Wire1's OWN pins, not the shared pair
  int  w1Hz  = 400000;

  // --- runtime --------------------------------------------------------------
  U8G2    *g = nullptr;
  uint8_t *shadow = nullptr;
  uint8_t  tw = 0, th = 0;      // tiles across, tile rows down
  uint16_t rowBytes = 0;
  uint32_t dirtyMask = 0;       // bit per tile row
  int8_t   hotRow = -1, vuRow = -1;
  int      hdrPx = 16;          // resolved header height for this panel

  uint32_t seenSerial = 0xFFFFFFFF;
  uint32_t lastGovMs = 0, lastSecMs = 0, lastVuMs = 0, lastScrollMs = 0;
  uint32_t flushAcc = 0; uint16_t flushN = 0;
  uint16_t pagesSec = 0;
  int16_t  scrollX = 0;

  // -1 = "nothing has been written yet". A uint8_t seeded to 255 could not
  // express that, because 255 is what 100% resolves to - which is the whole of
  // contrast fault (1) in the header note.
  int16_t  curContrast = -1;
  int16_t  curVcomh    = -1;
  int      shownPct    = -1;      // what the Info page reports

  bool     blanked = false, ready = false, initDone = false;
  bool     dimmed  = false;
  bool     pinFail = false, clash = false;
  uint32_t lastProbeMs = 0;

  // The meter. vuBar is the smoothed level 0..255, vuCap the peak-hold cap in
  // PIXELS (it has to be pixels: the cap falls at a constant visual rate, and
  // converting from level every frame would make it fall faster on tall
  // panels than short ones).
  uint8_t  vuBar[16] = {0};
  uint8_t  vuCap[16] = {0};
  uint8_t  vuCapAge[16] = {0};
  bool     vuWholeBody = false;   // is AUI_V_VU the current view?
  uint8_t  fairRow = 0;           // rotating start for the dirty-row scan

  static const char _name[];
  static const char _enabled[];

  // Percent -> contrast register, squared so it is perceptual rather than
  // linear in drive current. 25% lands on 16 instead of 64, which is the
  // difference between a dim step you can see and one you cannot. Never
  // returns 0: a contrast of zero on some SH1106 clones is indistinguishable
  // from a dead panel, and "I set it to 5% and it broke" is a support question
  // nobody should have to answer twice.
  static uint8_t auiCurve(int p) {
    if (p < 0)   p = 0;
    if (p > 100) p = 100;
    const int q = (p * p) / 100;              // 0..100, gamma 2.0
    const int c = (q * 254) / 100 + 1;        // 1..255
    return (uint8_t)c;
  }

  // --- drawing helpers ------------------------------------------------------
  void bar(int x, int y, int w, int h, int32_t v, int32_t lo, int32_t hi) {
    g->drawFrame(x, y, w, h);
    if (hi <= lo) return;
    int32_t fill = ((v - lo) * (w - 2)) / (hi - lo);
    if (fill < 0) fill = 0;
    if (fill > w - 2) fill = w - 2;
    if (fill) g->drawBox(x + 1, y + 1, (int)fill, h - 2);
  }

  int nameWidth(const char *s) {
    g->setFont(hdrPx >= 16 ? u8g2_font_6x10_tf : u8g2_font_5x8_tf);
    return g->getStrWidth(s);
  }

  // The yellow band. Drawn identically on every screen so the running effect
  // never moves, never disappears behind a menu, and never has to share a row
  // with a status line nobody reads.
  void drawHeader(const AceUiView &v, int W) {
    if (hdrPx <= 0) return;

    g->setFont(hdrPx >= 16 ? u8g2_font_6x10_tf : u8g2_font_5x8_tf);
    const int nw = g->getStrWidth(v.fxName);
    int x = (nw > W && scrollX) ? -scrollX : 0;
    g->drawStr(x, 7, v.fxName);
    if (x < 0) g->drawStr(x + nw + 16, 7, v.fxName);   // wrap-around copy
    if (hdrPx < 16) return;

    g->setFont(u8g2_font_5x8_tf);
    char r[AUI_TITLE_LEN];
    if (aceUi().lockOn)                        strncpy(r, "LOCK", sizeof(r));
    else if (v.crumb[0] && v.kind != AUI_V_NOWPLAY) strncpy(r, v.crumb, sizeof(r));
    else snprintf(r, sizeof(r), "%d%%", (int)(((int)bri * 100 + 127) / 255));
    r[sizeof(r) - 1] = 0;

    g->drawStr(0, 15, v.title);
    g->drawStr(W - g->getStrWidth(r), 15, r);
  }

  void drawNowPlaying(const AceUiView &v, int W, int H) {
    if (H < 64) {                            // 128x32: bar only, no room for more
      bar(0, hdrPx + 2, W, 8, v.valNum, 0, 255);
      hotRow = (int8_t)((hdrPx + 2) / 8);
      return;
    }
    char n[8]; snprintf(n, sizeof(n), "%d", (int)v.valNum);
    g->setFont(u8g2_font_logisoso20_tn);
    g->drawStr(0, 40, n);

    g->setFont(u8g2_font_5x8_tf);
    g->drawStr(56, 28, "brightness");
    bar(56, 32, W - 56, 8, v.valNum, 0, 255);
    g->drawStr(0, 52, v.crumb);                        // palette
    hotRow = 4;                                        // the bar's tile row
  }

  void drawList(const AceUiView &v, int W, int H) {
    const int rowH  = (H >= 64) ? 11 : 8;
    const int first = hdrPx;
    const int shown = (H - first) / rowH;
    if (shown <= 0) return;

    int top = v.cursor - shown / 2;
    if (top > v.count - shown) top = v.count - shown;
    if (top < 0) top = 0;

    g->setFont((H >= 64) ? u8g2_font_6x10_tf : u8g2_font_5x8_tf);
    char buf[AUI_ROW_LEN];
    for (int i = 0; i < shown && top + i < v.count; i++) {
      const int y = first + i * rowH;
      const bool sel = (top + i) == v.cursor;
      if (sel) { g->drawBox(0, y, W - 4, rowH); hotRow = (int8_t)((y + rowH / 2) / 8); }
      g->setDrawColor(sel ? 0 : 1);
      // A filled dot marks what is actually APPLIED, the bar marks the cursor.
      // With commit-on-click those are different things most of the time, and
      // a list that cannot show both is why mode-cycling UIs feel lost.
      if (v.live == top + i) g->drawDisc(4, y + rowH / 2, 2);
      if (v.rowFn) v.rowFn(top + i, buf, sizeof(buf)); else buf[0] = 0;
      g->drawStr(10, y + rowH - 3, buf);
      g->setDrawColor(1);
    }
    if (v.count > shown) {                            // scrollbar
      const int h = (H - first) * shown / v.count;
      const int y = first + (H - first) * top / v.count;
      g->drawVLine(W - 2, first, H - first);
      g->drawBox(W - 3, y, 3, h < 3 ? 3 : h);
    }
  }

  void drawValue(const AceUiView &v, int W, int H) {
    if (H < 64) {
      char n[AUI_ROW_LEN];
      if (v.valText[0]) snprintf(n, sizeof(n), "%s", v.valText);
      else              snprintf(n, sizeof(n), "%d", (int)v.valNum);
      g->setFont(u8g2_font_5x8_tf);
      g->drawStr(W - g->getStrWidth(n), hdrPx + 7, n);
      bar(0, hdrPx + 10, W, 8, v.valNum, v.valMin, v.valMax);
      hotRow = (int8_t)((hdrPx + 10) / 8);
      return;
    }
    char n[AUI_ROW_LEN];
    if (v.valText[0]) {
      g->setFont(u8g2_font_6x10_tf);
      snprintf(n, sizeof(n), "%s", v.valText);
      g->drawStr(0, 38, n);
    } else {
      g->setFont(u8g2_font_logisoso20_tn);
      snprintf(n, sizeof(n), "%d", (int)v.valNum);
      g->drawStr(0, 44, n);
    }
    bar(0, 50, W, 10, v.valNum, v.valMin, v.valMax);
    hotRow = 6;
  }

  // Static text page - the network screen, and anything else that is six short
  // facts rather than a list you move through.
  void drawInfo(const AceUiView &v, int W, int H) {
    (void)W;
    g->setFont(u8g2_font_5x8_tf);
    const int rowH = 8;
    const int shown = (H - hdrPx) / rowH;
    char buf[AUI_ROW_LEN + 12];
    for (int i = 0; i < shown && i < v.count; i++) {
      if (v.rowFn) v.rowFn(i, buf, sizeof(buf)); else buf[0] = 0;
      g->drawStr(0, hdrPx + rowH - 1 + i * rowH, buf);
    }
    hotRow = -1;
  }

  void drawToast(const AceUiView &v, int W, int H) {
    g->setFont(u8g2_font_6x10_tf);
    const int y = hdrPx + (H - hdrPx) / 2 + 4;
    g->drawStr((W - g->getStrWidth(v.title)) / 2, y, v.title);
    hotRow = (int8_t)(y / 8);
  }

  void drawConfirm(const AceUiView &v, int W, int H) {
    const int mid = hdrPx + (H - hdrPx) / 2;
    g->setFont(u8g2_font_6x10_tf);
    g->drawStr((W - g->getStrWidth(v.title)) / 2, mid, v.title);
    g->setFont(u8g2_font_5x8_tf);
    g->drawStr((W - g->getStrWidth(v.crumb)) / 2, mid + 12, v.crumb);
    hotRow = -1;
  }

  // Hold progress. A bar that fills toward HOME with a tick where BACK takes
  // over, so the whole gesture grammar is discoverable by holding the button
  // once and watching - release before the tick and nothing happens.
  void drawHold(const AceUiView &v, int W, int H) {
    g->setDrawColor(0); g->drawBox(0, H - 5, W, 5); g->setDrawColor(1);
    g->drawBox(0, H - 3, (W * v.holdPct) / 100, 3);
    if (v.holdMark) g->drawVLine((W * v.holdMark) / 100, H - 5, 5);
  }

  // The sixteen band levels from the audioreactive usermod, or nullptr. No
  // simulateSound() fallback here on purpose: an effect that fakes audio when
  // the mic is missing is being decorative, but a METER that fakes it is
  // lying, and the whole value of a meter is that you can trust it.
  static const uint8_t *auiFft() {
    um_data_t *um = nullptr;
    if (!UsermodManager::getUMData(&um, USERMOD_ID_AUDIOREACTIVE)) return nullptr;
    if (!um || !um->u_data) return nullptr;
    return (const uint8_t *)um->u_data[2];
  }

  // Fast attack, slow release - the same shape cfx_smoothSpec() uses on the
  // cube, so the panel and the faces agree about what the music is doing.
  void vuSmooth(const uint8_t *fft) {
    for (int i = 0; i < 16; i++) {
      const uint8_t t = fft[i];
      vuBar[i] = t > vuBar[i] ? t : (uint8_t)(vuBar[i] - ((vuBar[i] - t) >> 2));
    }
  }

  // --- the decoration: one tile row at the bottom of Now Playing ------------
  void drawVu(int W) {
    if (vuRow < 0) return;
    const uint8_t *fft = auiFft();
    if (!fft) return;
    vuSmooth(fft);
    const int y0 = vuRow * 8;
    const int bw = W / 16;
    g->setDrawColor(0); g->drawBox(0, y0, W, 8); g->setDrawColor(1);
    for (int i = 0; i < 16; i++) {
      int h = (vuBar[i] * 8) / 255; if (h > 8) h = 8;
      if (h) g->drawBox(i * bw, y0 + 8 - h, bw - 1, h);
    }
  }

  // --- the instrument: the whole body ---------------------------------------
  // Peak caps are held in pixels and fall at a fixed pixel rate, which is what
  // makes them read as physical. Holding them in LEVEL and converting each
  // frame would make the cap fall visibly faster near the top of the scale
  // than the bottom, and the eye picks that up immediately as "wrong" without
  // being able to say why.
  void drawVuFull(int W, int H) {
    const int top = hdrPx;
    const int hgt = H - top;
    if (hgt < 8) return;

    const uint8_t *fft = auiFft();
    if (!fft) {
      g->setFont(u8g2_font_6x10_tf);
      const char *m = "no audio source";
      g->drawStr((W - g->getStrWidth(m)) / 2, top + hgt / 2 + 4, m);
      hotRow = -1;
      return;
    }
    vuSmooth(fft);

    const int bw   = W / 16;                 // 8 px on a 128 px panel
    const int nSeg = hgt / 4;                // blocks: 3 px lit, 1 px gap

    for (int i = 0; i < 16; i++) {
      const int x = i * bw;
      int h = (vuBar[i] * hgt) / 255;
      if (h > hgt) h = hgt;

      if (vuPeak) {
        if (h >= (int)vuCap[i]) { vuCap[i] = (uint8_t)h; vuCapAge[i] = 0; }
        else if (vuCapAge[i] < 12) vuCapAge[i]++;      // ~400 ms hold at 30 Hz
        else if (vuCap[i]) vuCap[i]--;                 // then 1 px per frame
      } else vuCap[i] = 0;

      switch (vuStyle) {
        case AUI_VUS_MIRROR: {
          const int lim = hgt / 2;
          const int mid = top + lim;
          const int hh  = (h / 2) > lim ? lim : (h / 2);
          if (hh) {
            g->drawBox(x, mid - hh, bw - 1, hh);
            g->drawBox(x, mid, bw - 1, hh);
          } else {
            g->drawHLine(x, mid, bw - 1);              // idle centre line
          }
          if (vuPeak && vuCap[i] > 1) {
            const int ch = ((int)vuCap[i] / 2) > lim ? lim : (int)vuCap[i] / 2;
            g->drawHLine(x, mid - ch, bw - 1);
            g->drawHLine(x, mid + ch, bw - 1);
          }
          break;
        }

        case AUI_VUS_BLOCKS: {
          const int lit = nSeg ? (vuBar[i] * nSeg) / 255 : 0;
          for (int k = 0; k < lit; k++)
            g->drawBox(x, top + hgt - (k + 1) * 4, bw - 1, 3);
          if (vuPeak && nSeg) {
            const int ck = (vuCap[i] * nSeg) / (hgt ? hgt : 1);
            if (ck > lit && ck <= nSeg)
              g->drawFrame(x, top + hgt - ck * 4, bw - 1, 3);
          }
          break;
        }

        default:                                       // AUI_VUS_BARS
          if (h) g->drawBox(x, top + hgt - h, bw - 1, h);
          if (vuPeak && vuCap[i] > 0)
            g->drawHLine(x, top + hgt - (int)vuCap[i], bw - 1);
          break;
      }
    }
    // No hot row: every body row is equally the meter, so ranking one of them
    // above the others would just mean that row updates and the rest do not.
    // nextRow()'s rotating scan is the right answer here, not a priority.
    hotRow = -1;
  }

  // --- the flush ------------------------------------------------------------
  void diff() {
    uint8_t *buf = g->getBufferPtr();
    for (uint8_t r = 0; r < th; r++) {
      if (memcmp(buf + r * rowBytes, shadow + r * rowBytes, rowBytes) != 0)
        dirtyMask |= (1UL << r);
    }
  }

  // Ranking, in order:
  //   1. whatever the knob just changed - the row you are looking at
  //   2. the yellow header, which is where every screen says what it is
  //   3. everything else, scanned from a ROTATING cursor
  //   4. the decorative VU band, last, out of whatever budget survived
  //
  // (3) is the one that changed. A strict top-down scan is fine while two or
  // three rows are dirty and the budget covers them all. It is actively broken
  // the moment more rows are dirty than the budget can carry - which is the
  // normal case on the full VU screen, six dirty rows against a three-page
  // budget - because it services the top of the panel every single frame and
  // the bottom rows never come up at all. The meter's lower third simply
  // freezes. Starting the scan one past wherever it stopped costs a byte of
  // state and makes starvation share out evenly instead of landing entirely on
  // the same rows forever.
  int8_t nextRow() {
    if (!dirtyMask) return -1;
    if (hotRow >= 0 && (dirtyMask & (1UL << hotRow))) return hotRow;

    const uint8_t hdrRows = (uint8_t)(hdrPx / 8);
    for (uint8_t r = 0; r < hdrRows && r < th; r++)
      if (dirtyMask & (1UL << r)) return (int8_t)r;

    if (th) {
      for (uint8_t k = 0; k < th; k++) {
        const uint8_t r = (uint8_t)((fairRow + k) % th);
        if (!vuWholeBody && vuRow >= 0 && r == (uint8_t)vuRow) continue;
        if (dirtyMask & (1UL << r)) {
          fairRow = (uint8_t)((r + 1) % th);
          return (int8_t)r;
        }
      }
    }
    if (!vuWholeBody && vuRow >= 0 && (dirtyMask & (1UL << vuRow))) return vuRow;
    return -1;
  }

  void flush() {
    AceUiStats &st = aceUi().stats;
    // One page is the floor here too, independently of governor(). A budget
    // that cannot buy a single row is not a slow panel, it is a dead one, and
    // this loop is the last place that can tell the difference. Belt and
    // braces: a stale budget out of cfg.json, or a future edit to the
    // governor, can no longer take the display off the bus entirely.
    int budget = (int)st.budgetBytes;
    if (budget < (int)rowBytes) budget = (int)rowBytes;
    uint8_t *buf = g->getBufferPtr();

    while (budget >= (int)rowBytes) {
      const int8_t r = nextRow();
      if (r < 0) break;
      const uint32_t t0 = micros();
      g->updateDisplayArea(0, (uint8_t)r, tw, 1);
      const uint32_t dt = micros() - t0;

      memcpy(shadow + r * rowBytes, buf + r * rowBytes, rowBytes);
      dirtyMask &= ~(1UL << r);
      budget -= rowBytes;
      pagesSec++;

      st.busBusyUs += dt;
      flushAcc += dt; flushN++;
      if (dt > st.flushWorstUs) st.flushWorstUs = dt;
    }
    if (dirtyMask) st.starved++;
  }

  void governor(uint32_t now) {
    AceUiStats &st = aceUi().stats;
    if (now - lastGovMs < 500) return;
    lastGovMs = now;
    const uint16_t fps = strip.getFps();
    st.fps = fps;

    // The full VU screen gets a raised CEILING, never a lowered floor: fpsFloor
    // still protects the cube exactly as before, and the governor still backs
    // off the moment the strip suffers. All this says is that when the entire
    // panel is one animated instrument and nothing else is competing for the
    // lane, the sensible ceiling is higher than it is for a menu that redraws
    // twice a minute.
    int pages = (maxPages < 1 ? 1 : maxPages) + (vuWholeBody ? 2 : 0);
    if (th && pages > (int)th) pages = (int)th;
    const int maxB = (int)rowBytes * pages;

    // The floor is ONE WHOLE PAGE, and the step is ONE WHOLE PAGE. Both are
    // load-bearing, and the original had neither.
    //
    // flush() spends the budget in whole rows - `while (budget >= rowBytes)` -
    // so any budget between 1 and rowBytes-1 buys exactly nothing. Stepping by
    // 32 against a 128-byte row meant three of every four ticks moved the
    // number without moving a pixel, and the very first back-off took 128 to
    // 96, which is zero pages. The panel did not slow down, it stopped.
    //
    // That alone would have been recoverable. What made it permanent was the
    // growth gate: `fps > fpsFloor + 3` is 43 against a default floor of 40,
    // and the cube tops out at 43. Once the budget fell below a page the panel
    // went silent, fps came back to 43, and 43 satisfies neither `< 40` nor
    // `> 43` - so the budget latched under one page and stayed there. Only the
    // VU screen ever pushed hard enough to trip it, which is why only the VU
    // screen froze.
    //
    // Backing off has to mean "fewer pages per pass". The smallest honest
    // value of that is one, not none.
    const int step = (int)rowBytes;
    const int minB = (int)rowBytes;
    if (fps && fps < (uint16_t)fpsFloor) {
      int b = (int)st.budgetBytes - step;
      st.budgetBytes = (uint16_t)(b < minB ? minB : b);
    } else if (!fps || fps > (uint16_t)(fpsFloor + 3)) {
      int b = (int)st.budgetBytes + step;
      st.budgetBytes = (uint16_t)(b > maxB ? maxB : b);
    }
    // Repairs a budget that was already latched low before this code ran, and
    // costs one compare per half second.
    if (st.budgetBytes < (uint16_t)minB) st.budgetBytes = (uint16_t)minB;
  }

  // 16 px of yellow on a two-colour 64-row panel, 8 px of header on a mono
  // one, nothing on a 128x32 unless you ask for it.
  void resolveHdr() {
    const int H = g ? g->getDisplayHeight() : 64;
    hdrPx = (H >= 64) ? ((split >= 16) ? 16 : (split > 0 ? 8 : 0))
                      : (split > 0 ? 8 : 0);
  }

  // Contrast alone bottoms out well short of dark on these panels - the floor
  // is set by Vcomh, not by 0x81 - so the two move together. Both writes are
  // skipped when nothing changed, because this is called every single loop
  // pass and an I2C transaction is never free.
  void setContrastPct(int p) {
    if (!g) return;
    const uint8_t c  = auiCurve(p);
    const int16_t vc = deepDim
                     ? (int16_t)(p >= 60 ? AUI_VCOMH_HIGH
                                         : (p >= 30 ? AUI_VCOMH_MID : AUI_VCOMH_LOW))
                     : (int16_t)-1;

    if ((int16_t)c == curContrast && vc == curVcomh) return;
    shownPct = p;

    if (vc != curVcomh) {
      curVcomh = vc;
      // u8x8_SendF wraps its own StartTransfer/EndTransfer, so this is safe to
      // issue outside a draw and goes through the same byte callback - and
      // therefore the same ACK check - as everything else.
      if (vc >= 0) u8x8_SendF(g->getU8x8(), "ca", AUI_CMD_VCOMH, (uint8_t)vc);
    }
    if ((int16_t)c != curContrast) {
      curContrast = (int16_t)c;
      g->setContrast(c);
    }
  }

  // Used after begin() and after a settings save, where the panel's actual
  // state is unknown or has just been reset and the cached values are lies.
  void forceContrast(int p) {
    curContrast = -1;
    curVcomh    = -1;
    setContrastPct(p);
  }

 public:
  // Split in two so a panel plugged in later comes up without a reboot: setup()
  // claims pins and builds the object once, bringUp() is retried from loop()
  // until something actually ACKs at the address.
  bool probe() {
    if (busSel == AUI_BUS_SPI) return true;
    TwoWire &w = (busSel == AUI_BUS_WIRE1) ? Wire1 : Wire;
    w.beginTransmission((uint8_t)addr);
    return w.endTransmission() == 0;
  }

  bool bringUp() {
    if (!g || !probe()) return false;
    g->begin();
    forceContrast(briFull);        // begin() reset the panel; the cache is void
    g->setFontMode(1);
    g->clearBuffer();
    g->sendBuffer();                           // the one full refresh, at boot

    tw = g->getBufferTileWidth();
    th = g->getBufferTileHeight();
    rowBytes = (uint16_t)tw * 8;
    if (!shadow) shadow = (uint8_t *)calloc((size_t)rowBytes * th, 1);
    vuRow = (int8_t)(th - 1);

    resolveHdr();

    aceUi().stats.budgetBytes = rowBytes;
    dirtyMask = 0;
    seenSerial = 0xFFFFFFFF;
    return shadow != nullptr;
  }

  void setup() override {
    if (!enabled) return;

    if (busSel == AUI_BUS_SPI) {
      if (spiCs < 0 || spiDc < 0) return;
    } else if (busSel == AUI_BUS_WIRE1) {
      if (w1Sda < 0 || w1Scl < 0) return;
      // Wire1 is a separate peripheral. Handing it a pin the shared bus is
      // already using would put two masters on one pair and wedge both - which
      // presents as a dead sensor and costs an afternoon to find.
      if (w1Sda == i2c_sda || w1Sda == i2c_scl ||
          w1Scl == i2c_sda || w1Scl == i2c_scl) { clash = true; return; }
      if (!auiAllocPin((int8_t)w1Sda) || !auiAllocPin((int8_t)w1Scl)) { pinFail = true; return; }
      auiW1Sda = (int8_t)w1Sda;
      auiW1Scl = (int8_t)w1Scl;
      auiW1Hz  = (uint32_t)(w1Hz < 50000 ? 50000 : w1Hz);
      Wire1.begin((int)w1Sda, (int)w1Scl, auiW1Hz);
    } else if (i2c_sda < 0 || i2c_scl < 0) {
      return;                                  // same rule as the IMU: no pins,
    }                                          // no grabbing them behind your back

    const u8g2_cb_t *rot = rot180 ? U8G2_R2 : U8G2_R0;
    g = auiMake((uint8_t)panel, (uint8_t)busSel, rot,
                (int8_t)i2c_sda, (int8_t)i2c_scl,
                (int8_t)spiCs, (int8_t)spiDc, (int8_t)spiRst);
    if (!g) return;

    g->setI2CAddress((uint8_t)(addr << 1));
    if (busSel == AUI_BUS_WIRE1) {
      g->getU8x8()->byte_cb = aui_byte_wire1;  // our lane, our clock, our ACK
    } else {
      // 400 kHz is the MPU6050's ceiling, so it is the shared bus's ceiling.
      g->setBusClock(400000);
    }

    initDone = true;
    ready = bringUp();
    if (busSel == AUI_BUS_WIRE) Wire.setClock(400000);   // u8g2 may have moved it
    aceUi().screenReady = ready;
    aceUiMenuInit();
  }

  void loop() override {
    // Fresh input BEFORE the model runs, so knob-to-pixel is one loop pass no
    // matter which usermod the linker put first. Weak symbol - a build without
    // the encoder just skips it.
    if (aceUiEncoderPoll) aceUiEncoderPoll();
    aceUiMenuService();

    if (!enabled || !initDone) return;
    const uint32_t now = millis();
    AceUiBus &b = aceUi();

    if (!ready) {                              // panel wired up after boot?
      if (now - lastProbeMs < 3000) return;
      lastProbeMs = now;
      ready = bringUp();
      b.screenReady = ready;
      return;
    }

    // --- wake / dim / sleep ladder -------------------------------------------
    // The dedicated VU screen opts out of the whole ladder. The timers key off
    // the last KNOB event, and on a screen whose entire job is to react to
    // something other than the knob that is the wrong question: it would blank
    // a meter that is visibly working, thirty seconds into the first track,
    // and the only way to keep it alive would be to keep touching a control
    // you do not want to touch. Nothing else in the UI has that property, so
    // nothing else opts out.
    vuWholeBody = (b.view.kind == AUI_V_VU);
    const bool holdAwake = vuWholeBody && vuAwake;

    const uint32_t idle = now - b.lastInputMs;
    const bool wantSleep = !holdAwake && (sleepSec > 0) && (idle > (uint32_t)sleepSec * 1000u);
    const bool wantDim   = !holdAwake && (dimSec   > 0) && (idle > (uint32_t)dimSec   * 1000u);

    if (wantSleep != blanked) {
      blanked = wantSleep;
      b.asleep = wantSleep;
      g->setPowerSave(wantSleep ? 1 : 0);
      if (!wantSleep) { seenSerial = 0xFFFFFFFF; scrollX = 0; }  // force a redraw
    }
    if (blanked) return;                       // nothing below here costs a byte
    dimmed = wantDim;
    setContrastPct(wantDim ? briDim : briFull);

    governor(now);

    bool redraw = (b.view.serial != seenSerial);

    // Scroll only while the panel is at full brightness. Once it dims, the
    // whole thing goes static and the bus falls silent until you touch it.
    if (scroll && !wantDim && b.view.fxName[0] && hdrPx > 0) {
      const int nw = nameWidth(b.view.fxName);
      const int W  = g->getDisplayWidth();
      if (nw > W) {
        if (now - lastScrollMs > 70) {
          lastScrollMs = now;
          if (++scrollX > nw + 16) scrollX = 0;
          redraw = true;
        }
      } else if (scrollX) { scrollX = 0; redraw = true; }
    } else if (scrollX) { scrollX = 0; redraw = true; }

    // The BAND is optional decoration and vuMode governs it. The SCREEN is not
    // - you navigated to it deliberately, and a meter that switches itself off
    // is not a meter. It still answers to the governor's byte budget, which is
    // the mechanism that actually protects the cube; vuMode is only a taste
    // setting, and taste should not be able to blank a screen you chose.
    const bool wantBand = (vuMode != AUI_VU_OFF) && (b.view.kind == AUI_V_NOWPLAY)
                          && b.stats.budgetBytes >= rowBytes;
    const uint32_t vuMs = (uint32_t)(1000 / (vuHz < 5 ? 5 : (vuHz > 60 ? 60 : vuHz)));
    if ((wantBand || vuWholeBody) && now - lastVuMs >= vuMs) {
      lastVuMs = now;
      redraw = true;
    }

    if (redraw) {
      seenSerial = b.view.serial;
      const int H = g->getDisplayHeight();
      const int W = g->getDisplayWidth();
      hotRow = -1;
      g->clearBuffer();
      drawHeader(b.view, W);
      switch (b.view.kind) {
        case AUI_V_NOWPLAY: drawNowPlaying(b.view, W, H); break;
        case AUI_V_LIST:    drawList(b.view, W, H); break;
        case AUI_V_VALUE:   drawValue(b.view, W, H); break;
        case AUI_V_INFO:    drawInfo(b.view, W, H); break;
        case AUI_V_CONFIRM: drawConfirm(b.view, W, H); break;
        case AUI_V_TOAST:   drawToast(b.view, W, H); break;
        case AUI_V_VU:      drawVuFull(W, H); break;
      }
      if (wantBand) drawVu(W);
      if (b.view.holdPct) drawHold(b.view, W, H);
      diff();
    }

    // Never contend with a strip update in progress. One frame of latency on
    // the panel is invisible; a 3 ms I2C stall in the middle of pushing five
    // data lines is not.
    if (dirtyMask && !strip.isUpdating()) flush();

    if (now - lastSecMs >= 1000) {
      lastSecMs = now;
      AceUiStats &st = b.stats;
      st.occupancyPct = (uint8_t)((st.busBusyUs * 100) / 1000000UL);
      st.flushAvgUs = flushN ? flushAcc / flushN : 0;
      st.pagesPerSec = pagesSec;
      st.busBusyUs = 0; flushAcc = 0; flushN = 0; pagesSec = 0;
    }
  }

  // --- info -----------------------------------------------------------------
  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");
    const AceUiStats &st = aceUi().stats;

    JsonArray s = user.createNestedArray(FPSTR(_name));
    if (!enabled)      s.add(F("disabled"));
    else if (clash)    s.add(F("Wire1 pins clash with the shared bus"));
    else if (pinFail)  s.add(F("Wire1 pins already in use"));
    else if (!ready)   s.add(F("no ACK - check VCC, pin order, address"));
    else if (blanked)  s.add(F("asleep"));
    else               s.add(F("ok"));
    s.add(busSel == AUI_BUS_WIRE1 ? F(" on Wire1") :
          busSel == AUI_BUS_SPI   ? F(" on SPI")   : F(" shared"));
    if (!ready) return;

    char buf[48];
    if (auiNak) {
      JsonArray k = user.createNestedArray(F("Panel NAKs"));
      snprintf_P(buf, sizeof(buf), PSTR("%lu"), (unsigned long)auiNak);
      k.add(buf); k.add(F(" - wiring, not software"));
    }
    // The resolved contrast, spelled out. "It doesn't seem to be working" is
    // not a debuggable statement; "75% resolved to 143, Vcomh 0x30" is, and it
    // costs one line to turn one into the other.
    JsonArray c = user.createNestedArray(F("Panel contrast"));
    if (curVcomh >= 0)
      snprintf_P(buf, sizeof(buf), PSTR("%d%% -> %d, Vcomh 0x%02X"),
                 shownPct, (int)curContrast, (unsigned)curVcomh);
    else
      snprintf_P(buf, sizeof(buf), PSTR("%d%% -> %d"), shownPct, (int)curContrast);
    c.add(buf); c.add(dimmed ? F(" dimmed") : F(" full"));

    JsonArray f = user.createNestedArray(F("Panel flush"));
    snprintf_P(buf, sizeof(buf), PSTR("%lu us worst, %lu avg"),
               (unsigned long)st.flushWorstUs, (unsigned long)st.flushAvgUs);
    f.add(buf); f.add("");

    JsonArray o = user.createNestedArray(F("Panel bus"));
    snprintf_P(buf, sizeof(buf), PSTR("%u%%, %u pages/s, %u B budget"),
               st.occupancyPct, st.pagesPerSec, st.budgetBytes);
    o.add(buf); o.add(st.occupancyPct > 15 ? F(" - move it off") : F(" - lane ok"));

    JsonArray p = user.createNestedArray(F("Panel fps"));
    snprintf_P(buf, sizeof(buf), PSTR("%u vs %d floor, %lu starved"),
               st.fps, fpsFloor, (unsigned long)st.starved);
    p.add(buf); p.add("");
  }

  // --- config ---------------------------------------------------------------
  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top["panel"]    = panel;
    top["bus"]      = busSel;
    top["addr"]     = addr;
    top["rot180"]   = rot180;
    top["split"]    = split;
    top["briFull"]  = briFull;
    top["briDim"]   = briDim;
    top["dimSec"]   = dimSec;
    top["sleepSec"] = sleepSec;
    top["deepDim"]  = deepDim;
    top["idleHome"] = idleHome;
    top["startScr"] = startScr;
    top["vuMode"]   = vuMode;
    top["vuStyle"]  = vuStyle;
    top["vuHz"]     = vuHz;
    top["vuPeak"]   = vuPeak;
    top["vuAwake"]  = vuAwake;
    top["fpsFloor"] = fpsFloor;
    top["maxPages"] = maxPages;
    top["scroll"]   = scroll;
    top["spiCs"]    = spiCs;
    top["spiDc"]    = spiDc;
    top["spiRst"]   = spiRst;
    top["w1Sda"]    = w1Sda;
    top["w1Scl"]    = w1Scl;
    top["w1Hz"]     = w1Hz;
  }

  bool readFromConfig(JsonObject &root) override {
    JsonObject top = root[FPSTR(_name)];
    bool ok = !top.isNull();
    ok &= getJsonValue(top[FPSTR(_enabled)], enabled, true);
    ok &= getJsonValue(top["panel"],    panel,    0);
    ok &= getJsonValue(top["bus"],      busSel,   AUI_BUS_WIRE1);
    ok &= getJsonValue(top["addr"],     addr,     0x3C);
    ok &= getJsonValue(top["rot180"],   rot180,   0);
    ok &= getJsonValue(top["split"],    split,    16);
    ok &= getJsonValue(top["briFull"],  briFull,  75);
    ok &= getJsonValue(top["briDim"],   briDim,   35);
    ok &= getJsonValue(top["dimSec"],   dimSec,   10);
    ok &= getJsonValue(top["sleepSec"], sleepSec, 30);
    ok &= getJsonValue(top["deepDim"],  deepDim,  true);
    ok &= getJsonValue(top["idleHome"], idleHome, 0);
    ok &= getJsonValue(top["startScr"], startScr, AUI_START_MENU);
    ok &= getJsonValue(top["vuMode"],   vuMode,   AUI_VU_AUTO);
    ok &= getJsonValue(top["vuStyle"],  vuStyle,  AUI_VUS_BARS);
    ok &= getJsonValue(top["vuHz"],     vuHz,     30);
    ok &= getJsonValue(top["vuPeak"],   vuPeak,   true);
    ok &= getJsonValue(top["vuAwake"],  vuAwake,  true);
    ok &= getJsonValue(top["fpsFloor"], fpsFloor, 35);
    ok &= getJsonValue(top["maxPages"], maxPages, 3);
    ok &= getJsonValue(top["scroll"],   scroll,   true);
    ok &= getJsonValue(top["spiCs"],    spiCs,    -1);
    ok &= getJsonValue(top["spiDc"],    spiDc,    -1);
    ok &= getJsonValue(top["spiRst"],   spiRst,   -1);
    ok &= getJsonValue(top["w1Sda"],    w1Sda,    23);
    ok &= getJsonValue(top["w1Scl"],    w1Scl,    22);
    ok &= getJsonValue(top["w1Hz"],     w1Hz,     400000);

    // Dimming after the panel has already blanked is unreachable, and the two
    // being equal makes the dim step invisible rather than broken - clamp so
    // the ladder always has three distinct rungs.
    if (sleepSec > 0 && dimSec > 0 && dimSec >= sleepSec) dimSec = sleepSec / 2;

    // A meter faster than the panel can be flushed is not a faster meter, it
    // is the same meter plus a starvation counter climbing in Info. 60 is
    // already optimistic on a 400 kHz lane; 5 is slow enough to look like a
    // fault, so the floor is there to stop a typo reading as broken hardware.
    if (vuHz < 5)  vuHz = 5;
    if (vuHz > 60) vuHz = 60;
    if (vuStyle < AUI_VUS_BARS || vuStyle > AUI_VUS_BLOCKS) vuStyle = AUI_VUS_BARS;
    if (startScr < AUI_START_MENU || startScr > AUI_START_VU) startScr = AUI_START_MENU;
    if (idleHome < 0)    idleHome = 0;
    if (idleHome > 3600) idleHome = 3600;

    // The menu has no Usermod subclass and therefore no settings page of its
    // own, so its two behavioural knobs are published here. Done on EVERY
    // read, not just the live one, because the boot-time read has to land
    // before aceUiMenuInit() picks a startup screen.
    AceUiBus &b = aceUi();
    b.idleHomeSec = (uint16_t)(idleHome < 0 ? 0 : (idleHome > 3600 ? 3600 : idleHome));
    b.startScreen = (uint8_t)startScr;

    // Brightness, the governor and the header height all take effect live -
    // the last one because it is only ever an arithmetic constant, not a
    // reconfiguration. Panel type and bus still need a reboot, because tearing
    // down a u8g2 instance mid-transaction is a class of bug not worth owning.
    if (initDone && g) {
      resolveHdr();
      forceContrast(briFull);         // NOT a sentinel poke - see the header
      seenSerial = 0xFFFFFFFF;        // force a full redraw at the new layout
      dirtyMask  = 0xFFFFFFFF;
    }
    return ok;
  }

  // NOTE ON QUOTING: everything below is emitted into single-quoted JavaScript
  // literals, and ALL usermods share ONE script block on the settings page. A
  // stray apostrophe is a SyntaxError that kills every statement after it -
  // which is exactly how a help string here made the AceIMU action dropdown
  // disappear from a page it has nothing to do with. jsq() escapes on the way
  // out so it cannot happen again regardless of what anyone types.
  void appendConfigData(Print &s) override {
    auto jsq = [&](const char *t) {
      for (const char *p = t; *p; ++p) {
        if (*p == '\'' || *p == '\\') s.print('\\');
        s.print(*p);
      }
    };
    auto info = [&](const char *k, const char *html) {
      s.print(F("addInfo('")); s.print(FPSTR(_name)); s.print(F(":")); s.print(k);
      s.print(F("',1,'")); jsq(html); s.print(F("');"));
    };
    auto dd = [&](const char *k) {
      s.print(F("dd=addDropdown('")); s.print(FPSTR(_name));
      s.print(F("','")); s.print(k); s.print(F("');"));
    };
    auto opt = [&](const char *label, int v) {
      s.print(F("addOption(dd,'")); jsq(label); s.print(F("',")); s.print(v); s.print(F(");"));
    };

    dd("panel"); opt("SSD1306 128x64", 0); opt("SH1106 128x64", 1);
                 opt("SSD1309 128x64", 2); opt("SSD1306 128x32", 3);
    dd("bus");   opt("Its own I2C bus - Wire1",   AUI_BUS_WIRE1);
                 opt("Shared I2C (with the IMU)", AUI_BUS_WIRE);
                 opt("Hardware SPI",              AUI_BUS_SPI);
    dd("split"); opt("Two colour - 16px yellow header", 16);
                 opt("Mono - 8px header",                8);
                 opt("Mono - no header",                 0);
    dd("w1Hz");  opt("400 kHz - start here", 400000);
                 opt("800 kHz", 800000);
                 opt("1 MHz - halves the flush", 1000000);
    dd("addr");  opt("0x3C", 0x3C); opt("0x3D", 0x3D);
    dd("rot180");opt("normal", 0); opt("upside down", 1);
    dd("vuMode");opt("Off - redraw only on change", AUI_VU_OFF);
                 opt("On - always",                 AUI_VU_ON);
                 opt("Auto - throttle to hold fps", AUI_VU_AUTO);
    dd("vuStyle");opt("Bars - smooth, 16 bands",       AUI_VUS_BARS);
                  opt("Mirror - centre out",           AUI_VUS_MIRROR);
                  opt("Blocks - segmented, cheapest",  AUI_VUS_BLOCKS);
    dd("vuHz");  opt("15 Hz - lazy", 15);
                 opt("20 Hz", 20);
                 opt("30 Hz - start here", 30);
                 opt("45 Hz - needs a 1 MHz lane", 45);
    dd("startScr");opt("Main menu",   AUI_START_MENU);
                   opt("Now Playing", AUI_START_NOWPLAY);
                   opt("VU meter",    AUI_START_VU);

    info("bus",      "Wire1 keeps the panel away from the IMU entirely");
    info("split",    "these panels are yellow on rows 0-15, blue below. the effect name lives in the yellow");
    info("briFull",  "%, contrast while you are using it. the scale is perceptual, not linear");
    info("briDim",   "%, contrast once it has been left alone. the scale changed - <b>25</b> is genuinely dim now, so re-tune this if you had it set before");
    info("deepDim",  "also step Vcomh, which is what actually sets the floor. untick if a panel dislikes it");
    info("dimSec",   "idle seconds before dimming. scrolling stops here too");
    info("sleepSec", "idle seconds before the panel blanks. 0 = never");
    info("idleHome", "seconds of idle before the menu walks back to the top. <b>0</b> = stay where I left it");
    info("startScr", "which screen the panel comes up on");
    info("vuMode",   "the one-row meter on Now Playing ONLY. Auto gives it up before it gives up cube frames");
    info("vuStyle",  "the full VU screen. Blocks quantises the tips, so it costs about half the bus");
    info("vuHz",     "meter refresh. raise only once Info shows no starving");
    info("vuPeak",   "peak-hold caps that fall back slowly");
    info("vuAwake",  "the VU screen ignores the dim and sleep timers - it is reacting to music, not to you");
    info("fpsFloor", "the fps the governor protects. keep it at least 4 under your actual fps or the budget can never grow");
    info("maxPages", "ceiling on bytes per frame, in 128-byte pages");
    info("scroll",   "scroll effect names that do not fit. costs one page");
    info("spiCs",    "<i>SPI only</i>");
    info("w1Sda",    "<b>23</b>. the panel&#39;s own SDA - must NOT be the shared pin");
    info("w1Scl",    "<b>22</b>. the panel&#39;s own SCL");
    info("w1Hz",     "raise once Info shows zero NAKs and a stable panel");
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};

const char AceUiScreenUsermod::_name[]    PROGMEM = "AceUI-Screen";
const char AceUiScreenUsermod::_enabled[] PROGMEM = "enabled";

static AceUiScreenUsermod ace_ui_screen;
REGISTER_USERMOD(ace_ui_screen);

#endif  // __has_include(<U8g2lib.h>)
