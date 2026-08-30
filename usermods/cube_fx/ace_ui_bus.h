#pragma once

// ===========================================================================
// ace_ui_bus.h - shared state between the encoder, the menu and the screen
// ===========================================================================
// Three parts, one shared struct:
//
//   ace_ui_encoder.cpp   hardware in  -> events on the ring
//   ace_ui_menu.cpp      events in    -> WLED state out, view model filled
//   ace_ui_screen.cpp    view model in -> pixels out
//
// Each direction is one-way. Remove the screen and the encoder still works;
// remove the encoder and the screen is a status display. The menu is a plain
// translation unit with no hardware dependency and no Usermod subclass at all.
//
// ---------------------------------------------------------------------------
// LINKAGE - the trap cube_fx_common.h already fell into once
// ---------------------------------------------------------------------------
// Everything here that owns state is plain `inline`, never `static inline`. A
// static function in a header gets a PRIVATE copy of its function-local
// statics in every .cpp that includes it, so `static inline AceUiBus &aceUi()`
// would hand the encoder one bus and the screen a different one, and the two
// would never see each other's data. C++ guarantees exactly one instance of an
// inline function's locals across the whole binary.
//
// ---------------------------------------------------------------------------
// WHAT CHANGED IN THIS REVISION
// ---------------------------------------------------------------------------
// 1. AUI_V_VU - a new view kind whose body is ENTIRELY meter. The one-tile VU
//    strip on Now Playing stays exactly as it was; this is the dedicated
//    full-panel spectrum screen, reachable from the main menu and selectable
//    as the screen the panel comes up on.
// 2. `idleHomeSec` replaces the compile-time AUI_IDLE_HOME_MS. It DEFAULTS TO
//    ZERO, which means "stay on the screen I left you on". Wandering back to
//    the main menu behind your back is now something you have to ask for,
//    which is the right way round: a panel that silently abandons the screen
//    you parked it on is the single most annoying thing a menu can do, and it
//    made the dedicated VU screen impossible to actually use as a display.
// 3. `startScreen` - which screen aceUiMenuInit() lands on at boot. Menu, Now
//    Playing or VU. Together with (2) this is what turns "there is a VU screen
//    in the menu" into "the panel is a VU meter".
//
// ---------------------------------------------------------------------------
// EARLIER REVISIONS, STILL TRUE
// ---------------------------------------------------------------------------
// - Double-click is GONE. Back is a 500 ms hold, Home is a 2 s hold, both on
//   the same press, decided at RELEASE from elapsed time - so it costs nothing
//   in latency and can no longer be undone behind your back.
// - The view model carries `fxName` separately from `title`. The panel is a
//   two-colour part - rows 0-15 yellow, 16-63 blue - so the running effect
//   name is pinned to the yellow band on EVERY screen and the body below is
//   free to be a list, an editor, a value or a meter.
// - AUI_V_INFO: a static multi-line text page, which is what the network
//   screen is, and what any future "about" page will be.
// - aceUiEncoderPoll() - the screen calls it before rendering so knob-to-pixel
//   is one loop pass regardless of which usermod the linker put first.
// ===========================================================================

#include "wled.h"

// ---------------------------------------------------------------------------
// Build-time limits
// ---------------------------------------------------------------------------
// One encoder now, room for more without touching code: the config page always
// renders ACE_UI_MAX_ENC blocks and a block with `en` off costs nothing at
// runtime. Adding a second knob later is a settings change and a reboot.
#ifndef ACE_UI_MAX_ENC
  #define ACE_UI_MAX_ENC 4
#endif

// Event ring depth. Events are produced and consumed in loop() at the same
// rate, so this only has to survive one stalled frame.
#ifndef ACE_UI_RING
  #define ACE_UI_RING 16
#endif

// Seed for AceUiBus::idleHomeSec. ZERO means the UI never walks back to the
// main menu on its own. Override at build time only if you want the old
// behaviour on every unit without touching the settings page.
#ifndef AUI_IDLE_HOME_SEC
  #define AUI_IDLE_HOME_SEC 0
#endif

#define AUI_TITLE_LEN 24
#define AUI_ROW_LEN   24

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------
// The encoder emits intent, not hardware. "The knob moved 3 detents with the
// button held" is an event; "pin 25 fell" is not.
//
// The gesture grammar, in full:
//
//   turn                     live, immediate
//   press < backMs           CLICK   - accept / enter / drill in
//   press backMs..homeMs     BACK    - one level up, fires on RELEASE
//   press >= homeMs          HOME    - main menu, fires AT the threshold
//   press >= lockMs          LOCK    - child lock (0 disables)
//   push-and-turn            shift: x10 coarse, jump-by-letter
//   TWO buttons >= rebootMs  CHORD   - reboot the board
//
// Deciding CLICK vs BACK at release costs nothing: the press duration is
// already known by then. Nothing is ever delayed waiting for a second press.
//
// CHORD is the only two-handed gesture and the only one that needs a second
// encoder to exist at all. It is deliberately the longest hold in the grammar
// and deliberately impossible single-handed, because it is the one gesture
// that can drop the cube mid-show. While a chord is forming, the participating
// buttons' own gesture machines are suppressed - otherwise the 2 s Home hold
// would fire on both of them a second before the reboot landed.
enum : uint8_t {
  AUI_EV_TURN = 0,   // delta = signed detents, shift = button held while turning
  AUI_EV_CLICK,      // short press, released
  AUI_EV_BACK,       // medium press, released - pop one level
  AUI_EV_HOME,       // long press, fired the moment it qualifies
  AUI_EV_LOCK,       // very long press - lock / unlock
  AUI_EV_HOLD_PROG,  // delta = 0..100 progress toward HOME. drives the bar
  AUI_EV_HOLD_ABORT, // released early, or the gesture was consumed
  // Appended, never inserted. Nothing persists these numbers, but a stable
  // enum means a stale .o in the build tree is a link error rather than a
  // silent reinterpretation of somebody else's event.
  AUI_EV_CHORD_PROG, // delta = 0..100 progress toward the two-button reboot
  AUI_EV_CHORD       // two-button hold completed. the menu decides what it means
};

struct AceUiEvent {
  uint8_t enc;       // which encoder, 0..ACE_UI_MAX_ENC-1
  uint8_t kind;
  int16_t delta;
  bool    shift;     // button was down at the time
};

// ---------------------------------------------------------------------------
// Encoder roles
// ---------------------------------------------------------------------------
// Roles, not modes. The stock usermod makes one knob mean nine different
// things depending on a hidden counter you advance by clicking. A role is
// fixed: the Master knob is brightness on Tuesday and brightness at 3am. With
// one encoder you want AUI_ROLE_NAV.
enum : uint8_t {
  AUI_ROLE_NAV = 0,  // cursor in menus, brightness on Now Playing and VU
  AUI_ROLE_MASTER,   // brightness, always, everywhere. never changes meaning
  AUI_ROLE_BROWSE,   // moves the cursor only, never edits
  AUI_ROLE_VALUE,    // edits the highlighted parameter only
  AUI_ROLE_BOUND     // pinned to one parameter forever, see encBind
};

// Parameters a BOUND encoder can be nailed to, and the same IDs the menu uses
// internally so there is one table of getters instead of two.
enum : uint8_t {
  AUI_P_BRI = 0, AUI_P_FX, AUI_P_PAL, AUI_P_SPEED, AUI_P_INTENSITY,
  AUI_P_C1, AUI_P_C2, AUI_P_C3, AUI_P_O1, AUI_P_O2, AUI_P_O3,
  AUI_P_HUE, AUI_P_SAT, AUI_P_CCT, AUI_P_PRESET, AUI_P_SEG,
  AUI_P_COUNT
};

// ---------------------------------------------------------------------------
// Button roles
// ---------------------------------------------------------------------------
// Separate from the turn role above, and deliberately so: what a knob does
// when you TURN it and what it does when you PRESS it are independent choices.
// The natural two-knob panel is left knob navigates and enters, right knob
// backs out and rides brightness - which is AUI_ROLE_NAV + AUI_SW_FULL on one
// and AUI_ROLE_MASTER + AUI_SW_BACK on the other. Neither of those is
// expressible if press and turn share one setting.
//
// AUI_SW_BACK collapses the grammar to two rungs, because a dedicated button
// does not need three:
//
//   any press < homeMs       BACK - pop one level, on release
//   press >= homeMs          HOME - main menu, at the threshold
//
// There is no intermediate meaning left, so the hold bar on this encoder is
// drawn WITHOUT the tick - a tick would advertise a rung that is not there.
enum : uint8_t {
  AUI_SW_FULL = 0,   // click / back / home / lock, the full grammar
  AUI_SW_BACK        // click = back one level, hold = home
};

// ---------------------------------------------------------------------------
// Startup screen
// ---------------------------------------------------------------------------
// Where aceUiMenuInit() leaves the cursor at boot. With idleHomeSec at 0 this
// is also, in practice, where the panel LIVES - nothing moves it again until
// somebody turns the knob.
enum : uint8_t {
  AUI_START_MENU = 0,
  AUI_START_NOWPLAY,
  AUI_START_VU
};

// ---------------------------------------------------------------------------
// View model
// ---------------------------------------------------------------------------
// Deliberately not a framebuffer and not a u8g2 call list. The menu describes
// WHAT is on screen; the screen decides how it looks and what it can afford to
// send this frame. That separation is what lets the governor drop the VU band
// without the menu knowing or caring, and it is what makes an on-cube OSD a
// second consumer of AceUiView rather than a rewrite.
//
// AUI_V_VU carries no rows and no value: the menu is saying "the body is a
// meter now", and every decision about bands, peak caps, smoothing and how
// many tile rows that is worth this frame belongs to the screen.
enum : uint8_t {
  AUI_V_NOWPLAY = 0, // effect name + big brightness + palette + one VU strip
  AUI_V_LIST,        // scrolling list, cursor + live marker
  AUI_V_VALUE,       // one parameter, bar + number
  AUI_V_INFO,        // static text page - rowFn supplies `count` lines
  AUI_V_CONFIRM,     // destructive action, armed, waiting for a click
  AUI_V_TOAST,       // transient message, auto-expires
  AUI_V_VU           // the whole body is a spectrum. nothing else drawn
};

typedef void (*AceUiRowFn)(int16_t idx, char *out, uint8_t n);

struct AceUiView {
  uint8_t  kind      = AUI_V_NOWPLAY;

  // The yellow band. fxName is the RUNNING effect and is drawn on every
  // screen; title/crumb are the second header line - where you are, and how
  // far through. The body below the split never has to spend rows on either.
  char     fxName[AUI_TITLE_LEN] = {0};
  char     title[AUI_TITLE_LEN]  = {0};
  char     crumb[AUI_TITLE_LEN]  = {0};

  int16_t  count     = 0;    // rows in a list, or lines on an info page
  int16_t  cursor    = 0;    // where the highlight is
  int16_t  live      = -1;   // which row is actually APPLIED right now, -1 none
  AceUiRowFn rowFn   = nullptr;

  // value editor / HUD
  char     valLabel[AUI_ROW_LEN] = {0};
  char     valText[AUI_ROW_LEN]  = {0};
  int32_t  valNum    = 0;
  int32_t  valMin    = 0;
  int32_t  valMax    = 255;

  uint8_t  holdPct   = 0;    // 0 = no bar. driven by AUI_EV_HOLD_PROG
  uint8_t  holdMark  = 0;    // where on that bar BACK turns into HOME
  uint32_t hudUntil  = 0;    // millis() at which a transient overlay expires
  uint32_t serial    = 0;    // bumped on every change - the screen's redraw cue
};

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
// The decision rule, written down so it is not relitigated later: under ~8%
// bus occupancy the panel's own lane is fine and you are done. Over ~15%, or
// `starved` climbing steadily rather than blipping, raise w1Hz or cut maxPages.
struct AceUiStats {
  uint32_t flushWorstUs = 0;
  uint32_t flushAvgUs   = 0;
  uint32_t busBusyUs    = 0;   // accumulator, reset each second
  uint8_t  occupancyPct = 0;
  uint16_t budgetBytes  = 128;
  uint32_t starved      = 0;   // dirty pages that had to wait a frame
  uint16_t fps          = 0;
  uint16_t pagesPerSec  = 0;
};

// ---------------------------------------------------------------------------
// The bus
// ---------------------------------------------------------------------------
struct AceUiBus {
  // event ring - produced and consumed in loop(), never from an ISR
  AceUiEvent ring[ACE_UI_RING];
  uint8_t    head = 0, tail = 0;

  AceUiView  view;
  AceUiStats stats;

  // published by the encoder so the menu can interpret events without having
  // to reach into another usermod's config
  uint8_t  encCount = 0;
  uint8_t  encRole[ACE_UI_MAX_ENC] = {AUI_ROLE_NAV};
  uint8_t  encBind[ACE_UI_MAX_ENC] = {AUI_P_BRI};

  // What each encoder's BUTTON is for. The menu needs this for two decisions
  // it cannot make from the event alone: whether a BACK means "pop" or "the
  // preset-save gesture", and whether the hold bar gets a tick. Both hang on
  // WHICH knob sent it, which is why this is per-encoder and not one flag.
  uint8_t  encSw[ACE_UI_MAX_ENC] = {AUI_SW_FULL};

  // Where BACK turns into HOME, as a percentage of the hold bar. Published by
  // the encoder because only it knows backMs and homeMs; the screen draws a
  // tick there so the gesture explains itself the first time you hold the
  // button, instead of being something you have to be told.
  uint8_t  backPct = 25;

  // Published by the screen, consumed by the menu. Both live here rather than
  // as file-scope statics in ace_ui_menu.cpp because the menu has no Usermod
  // subclass and therefore no settings page of its own - and because a build
  // with no u8g2 compiles the screen out entirely, in which case these keep
  // their defaults and the menu behaves sensibly on its own.
  //
  // idleHomeSec = 0 means NEVER. See the header note.
  uint16_t idleHomeSec = AUI_IDLE_HOME_SEC;
  uint8_t  startScreen = AUI_START_MENU;

  bool     lockOn      = false;   // child lock - input ignored except unlock
  bool     screenReady = false;
  bool     encReady    = false;

  // Set by the screen when the panel has blanked, cleared by the menu when it
  // swallows the input that woke it. Brushing the knob on the way past a dark
  // panel should turn the light back on, not renumber your effect.
  bool     asleep      = false;

  // Any input at all, of any kind. The screen's wake / dim / sleep ladder
  // hangs off this one number, so every event source must stamp it - not just
  // the ones that change the view.
  uint32_t lastInputMs = 0;
};

inline AceUiBus &aceUi() {
  static AceUiBus bus;            // ONE of these. see the linkage note above.
  return bus;
}

inline bool aceUiPush(const AceUiEvent &e) {
  AceUiBus &b = aceUi();
  const uint8_t nxt = (uint8_t)((b.head + 1) % ACE_UI_RING);
  if (nxt == b.tail) return false;          // full: drop the newest, keep order
  b.ring[b.head] = e;
  b.head = nxt;
  b.lastInputMs = millis();
  return true;
}

inline bool aceUiPop(AceUiEvent &e) {
  AceUiBus &b = aceUi();
  if (b.tail == b.head) return false;
  e = b.ring[b.tail];
  b.tail = (uint8_t)((b.tail + 1) % ACE_UI_RING);
  return true;
}

inline bool aceUiPending() { return aceUi().tail != aceUi().head; }
inline void aceUiTouch()   { aceUi().view.serial++; }

// ---------------------------------------------------------------------------
// Menu entry points - implemented in ace_ui_menu.cpp
// ---------------------------------------------------------------------------
// aceUiMenuService() drains the ring and runs timeouts. BOTH the encoder and
// the screen call it every loop. Draining is idempotent and cheap, so it is
// NOT guarded per-millisecond any more - only the periodic housekeeping is.
// That matters: the old guard meant whichever usermod the linker happened to
// put second had its service call swallowed, and a knob turn could sit in the
// ring for a whole 23 ms frame before anyone looked at it.
void aceUiMenuService();
void aceUiMenuInit();

// Screen asks the menu for things it should not have to know how to compute.
const char *aceUiStatusLine();     // one-line network summary, for Info only

// ---------------------------------------------------------------------------
// Cross-usermod poll hook
// ---------------------------------------------------------------------------
// Defined by ace_ui_encoder.cpp, called by ace_ui_screen.cpp immediately
// before it renders. Static-init order across translation units is not
// specified, so without this the screen renders one loop pass stale exactly
// half the time - 23 ms of lag that comes and goes depending on link order,
// which is the worst kind. It is idempotent and guarded to one run per
// millisecond, so calling it twice a pass costs nothing.
extern "C" void aceUiEncoderPoll() __attribute__((weak));
