#include "wled.h"
#include <Wire.h>
#include "ace_ui_bus.h"

#ifdef ARDUINO_ARCH_ESP32
  #include "soc/gpio_struct.h"
#endif

// ===========================================================================
// ace_ui_encoder.cpp - N rotary encoders, interrupt driven, gesture aware
// ===========================================================================
// Drop next to the effects in usermods/cube_fx/. Self-registers like
// they do. Remove `rotary_encoder_ui_ALT` from custom_usermods before building
// or you will have two things fighting over the same knob.
//
// ---------------------------------------------------------------------------
// WHY NOT POLL, LIKE THE STOCK ONE DOES
// ---------------------------------------------------------------------------
// The stock usermod reads the pins from loop(). On a 48x48 cube the loop
// period IS the frame period - 23 ms at 43 fps, longer when the FFT and five
// data lines collide - and a fast flick of the wrist puts detent edges 2 ms
// apart. Poll at 23 ms and you lose most of them, which is why the stock knob
// feels like it "sticks" on a busy pattern. Every edge here lands in an IRAM
// ISR that does four instructions and a table lookup, so the count is exact
// no matter how badly the render loop stalls.
//
// Buttons stay polled. Bounce is a 5-15 ms phenomenon and press durations are
// 50-200 ms, so 23 ms sampling is plenty, and it keeps the ISR to one job.
//
// ---------------------------------------------------------------------------
// GESTURE GRAMMAR  (revised - double-click is gone)
// ---------------------------------------------------------------------------
//   turn                     live, immediate
//   press  < 500 ms          click   - accept / enter / drill in
//   press  500 ms - 2 s      back    - one level up
//   press  >= 2 s            home    - main menu
//   press  >= lockMs         lock / unlock, if lockMs is non-zero
//   push-and-turn            shift: x10 coarse, jump-by-letter
//   TWO buttons >= 3 s       REBOOT  - needs two encoders, see below
//
// Both thresholds live on ONE press, and the bar on screen fills toward Home
// with a tick mark at Back, so the whole grammar is discoverable by holding
// the button once and watching. Release before the tick: nothing. Release
// after it: back. Keep going: home.
//
// ---------------------------------------------------------------------------
// THE REBOOT CHORD
// ---------------------------------------------------------------------------
// Hold any TWO encoder buttons together for rebootMs (3 s by default) and the
// board reboots. Three properties make that safe enough to leave armed:
//
//   1. It cannot be done with one hand, so it cannot be done by accident by
//      somebody leaning on the panel or a sleeve catching the knob.
//   2. The hold bar starts filling IMMEDIATELY - no 120 ms grace like the
//      single-button gesture gets - so the chord announces itself the instant
//      it registers, and releasing either button aborts it.
//   3. While a chord is forming, both participating buttons' own gesture
//      machines are switched off. Without that, the 2 s Home hold fires on
//      both knobs on the way to a 3 s reboot and you watch the menu jump to
//      the root a second before the board goes down.
//
// The suppression latches in `chordUsed` and only clears on the NEXT press, so
// bailing out of a chord leaves both buttons inert until you actually let go -
// which is what cancelling a gesture should feel like. The one rough edge: if
// the two presses are more than homeMs APART, the first button crosses Home on
// its own before the second arrives and you get a jump to the main menu first.
// The reboot still lands. Pressing both within two seconds of each other -
// which is what "hold both buttons" means to anybody - never sees it.
//
// rebootMs = 0 disables the chord entirely. With one encoder configured it can
// never form, and the Info page says so rather than staying silent about it.
// System -> Reboot in the menu does the same job with one knob.
//
// The chord is indifferent to what a button is FOR. It watches which buttons
// are physically down, so a knob set to Back still participates - which is
// what you want, since the pair you can reach with two hands should not depend
// on how each one is configured.
//
// ---------------------------------------------------------------------------
// PRESS AND TURN ARE SEPARATE SETTINGS
// ---------------------------------------------------------------------------
// `role` says what turning does. `swRole` says what pressing does, and it only
// has two values because a button only needs two:
//
//   Full   click / back / home / lock. what one knob on its own has to be
//   Back   click backs out one level, hold goes home. nothing else
//
// Splitting them is what makes the obvious two-knob panel expressible at all:
//
//   encoder 0   Nav    + Full   navigate, click to enter
//   encoder 1   Master + Back   brightness under your hand, tap to back out
//
// With one setting covering both you would have to give up brightness to get a
// back button, or give up the back button to keep brightness, and neither is a
// trade anybody would choose. A Back knob loses the lock gesture and the
// preset-save hold; both live on encoder 0 and both also have menu entries, so
// nothing becomes unreachable.
//
// The old scheme used double-click for back, which cost a 250 ms rollback
// window and an entire revert machine in the menu to make "click, oh no, back"
// look invisible. Deciding click-vs-back at RELEASE from the elapsed time is
// free - the duration is already known by then - so nothing got slower and a
// commit is now permanent the moment it happens.
//
// ===========================================================================
// WIRING
// ===========================================================================
// An encoder at rest sits with A and B pulled HIGH. That single fact rules out
// most of the pins you would otherwise reach for, and it is the same failure
// mode as putting a pulled-up sensor line on GPIO12.
//
//   NEVER  GPIO12  held high at reset = 1.8 V flash. board hangs or corrupts
//   NEVER  GPIO0   pressing SW at boot drops it low = bootloader mode
//   AVOID  GPIO2, 15  boot-mode strapping, and 15 is the Gledopto PDM mic
//   NEVER  GPIO6-11  SPI flash
//   NOTE   GPIO34-39 input only and NO internal pull-ups. fine for the
//          MCP23017 INT line (push-pull), not for a bare encoder
//
// Recommended on the 5-face cube:
//
//   A  -> GPIO25      B -> GPIO26      SW -> GPIO27
//
// 25/26/27 are plain bidirectional pins with no boot role and no ADC2/WiFi
// conflict. Those are clear of the IMU (SDA 21 / SCL 13) and of the panel's
// own Wire1 lane (SDA 23 / SCL 22).
//
//   COM/GND -> GND. Common ground with the ESP32, not just the PSU.
//   VCC     -> 3.3 V if the module has pull-ups. NOT 5 V - a 5 V pull-up puts
//              5 V on a 3.3 V input.
//
// KY-040 modules carry 10k pull-ups on A and B but usually NOT on SW, so leave
// `pu` on. A bare EC11 has none at all and needs `pu` on for all three.
// Cable longer than about 30 cm: add 10k to 3.3 V and 100 nF to GND on each of
// A and B at the ESP32 end. The ISR counts every edge, including ringing.
//
// The `bad` counter in Info is your wiring meter. Turn the knob a full
// rotation: a handful of bad transitions is normal, hundreds means bounce.
//
// Knob turns the wrong way? Tick `e0inv` in Settings -> Usermods. It is a
// settings save, not a reflash - the pins are re-attached live.
//
// ---------------------------------------------------------------------------
// ADDING ENCODERS LATER (the "expander-ready" part)
// ---------------------------------------------------------------------------
// Every encoder block has a `src` dropdown: direct GPIO, or MCP23017 pin 0-15.
// The expander needs SDA, SCL - already there for the IMU - plus one interrupt
// line, and GPIO35 is ideal for it precisely because it is input-only and
// useless for anything else. So encoders two, three and four cost ONE pin:
//
//   MCP23017  SDA -> GPIO21 (shared)   A0/A1/A2 -> GND for 0x20
//             SCL -> GPIO13 (shared)   INTA     -> GPIO35
//             RESET -> 3.3 V  (floating RESET is the usual "it does nothing")
//
// INTA is configured MIRROR + interrupt-on-change, so one line covers both
// ports. The ISR on GPIO35 only sets a flag - I2C cannot run in an ISR - and
// loop() does the 2-byte read. Human hands top out around 80 edges/second, so
// that is ~0.2% bus occupancy.
// ===========================================================================

#ifndef AUI_LEGACY_PINMGR
  #define AUI_LEGACY_PINMGR 0   // -D AUI_LEGACY_PINMGR=1 for pre-0.16 WLED
#endif                          // (global `pinManager` object, not statics)

static bool auiAllocPin(int8_t p) {
  if (p < 0) return true;
#if AUI_LEGACY_PINMGR
  return pinManager.allocatePin((byte)p, false, PinOwner::UM_Unspecified);
#else
  return PinManager::allocatePin((byte)p, false, PinOwner::UM_Unspecified);
#endif
}
static void auiFreePin(int8_t p) {
  if (p < 0) return;
#if AUI_LEGACY_PINMGR
  pinManager.deallocatePin((byte)p, PinOwner::UM_Unspecified);
#else
  PinManager::deallocatePin((byte)p, PinOwner::UM_Unspecified);
#endif
}

// --- fast pin read ----------------------------------------------------------
// digitalRead() is not guaranteed to live in IRAM on every core version, and a
// flash-resident function called from an ISR while the flash cache is busy is
// a reset. Reading the register directly is both safe and about 30x faster.
static inline bool IRAM_ATTR auiRd(uint8_t p) {
#ifdef ARDUINO_ARCH_ESP32
  if (p < 32) return (GPIO.in >> p) & 0x1;
  return (GPIO.in1.val >> (p - 32)) & 0x1;
#else
  return digitalRead(p);
#endif
}

// --- quadrature -------------------------------------------------------------
// Index is (previous << 2) | current, each half being (A << 1) | B. The four
// zero entries at 3, 6, 9 and 12 are the physically impossible double
// transitions - both pins changing between two reads. They mean bounce, a
// missed interrupt, or a bad cable, and they are counted rather than guessed
// at, because guessing is how a knob develops a mind of its own.
static const int8_t AUI_QTAB[16] = {
   0, +1, -1,  0,
  -1,  0,  0, +1,
  +1,  0,  0, -1,
   0, -1, +1,  0
};

struct AuiEncIsr {
  volatile int32_t  acc   = 0;   // signed quadrature steps since last drain
  volatile uint16_t bad   = 0;
  volatile uint8_t  state = 0;
  uint8_t a = 255, b = 255;
  bool    attached = false;
};
static AuiEncIsr auiIsr[ACE_UI_MAX_ENC];

static void IRAM_ATTR auiEncIsrFn(void *arg) {
  AuiEncIsr *e = (AuiEncIsr *)arg;
  const uint8_t cur = (uint8_t)((auiRd(e->a) << 1) | auiRd(e->b));
  const uint8_t prev = e->state;
  if (cur == prev) return;
  const int8_t d = AUI_QTAB[(prev << 2) | cur];
  if (d == 0) e->bad++;
  else        e->acc += d;
  e->state = cur;
}

// Shared step function so the expander path and the ISR path cannot drift.
static void auiStepSoft(AuiEncIsr &e, uint8_t cur) {
  const uint8_t prev = e.state;
  if (cur == prev) return;
  const int8_t d = AUI_QTAB[(prev << 2) | cur];
  if (d == 0) e.bad++;
  else        e.acc += d;
  e.state = cur;
}

// --- MCP23017 ---------------------------------------------------------------
#define AUI_MCP_IODIRA 0x00
#define AUI_MCP_IPOLA  0x02
#define AUI_MCP_GPINTA 0x04
#define AUI_MCP_INTCONA 0x08
#define AUI_MCP_IOCON  0x0A
#define AUI_MCP_GPPUA  0x0C
#define AUI_MCP_GPIOA  0x12

enum : uint8_t { AUI_SRC_GPIO = 0, AUI_SRC_MCP = 1 };

// I2C cannot run inside an ISR, so the expander's interrupt only raises a flag
// and loop() does the two-byte read one pass later. Nobody can feel one pass.
static void IRAM_ATTR auiMcpIsrFn(void *arg) { *(volatile bool *)arg = true; }

class AceUiEncoderUsermod : public Usermod {
 private:
  // --- persisted config -----------------------------------------------------
  bool enabled = true;
  int  backMs  = 500;    // hold -> back one level
  int  homeMs  = 2000;   // hold -> main menu
  int  lockMs  = 0;      // hold -> lock. 0 = disabled, use System -> Lock knob
  int  rebootMs = 3000;  // TWO buttons held this long -> reboot. 0 = disabled
  int  swDeb   = 15;     // button debounce, ms
  int  mcpAddr = 0x20;
  int  mcpInt  = -1;

  struct EncCfg {
    bool en    = false;
    int  src   = AUI_SRC_GPIO;
    int  a     = -1, b = -1, sw = -1;
    int  ppd   = 4;       // pulses per detent: 1, 2 or 4
    bool inv   = false;
    bool pu    = true;    // internal pull-ups
    int  accel = 40;      // 0 = off, 100 = aggressive
    int  role  = AUI_ROLE_NAV;   // what TURNING it does
    int  swRole = AUI_SW_FULL;   // what PRESSING it does
    int  bind  = AUI_P_BRI;
  } enc[ACE_UI_MAX_ENC];

  // --- runtime --------------------------------------------------------------
  struct EncRt {
    // button
    bool     rawSw = true;                    // true = released (pull-up)
    uint32_t swChangeMs = 0;
    // gesture
    bool     down = false, shiftUsed = false;
    bool     homeFired = false, lockFired = false, barShown = false;
    // Set when this button became part of a two-button chord. Same semantics
    // as shiftUsed: the press has already been spent on something else, so it
    // must not also produce a click, a back or a home on the way out.
    bool     chordUsed = false;
    uint32_t pressMs = 0, lastProgMs = 0;
    // motion
    int32_t  carry = 0;
    uint32_t lastTurnMs = 0;
    int16_t  lastDelta = 0;
    uint32_t detents = 0, bounces = 0;
  } rt[ACE_UI_MAX_ENC];

  // Chord state is per-BOARD, not per-encoder: there is one chord at a time and
  // it belongs to the set of buttons that are down, not to any one of them.
  uint32_t chordMs = 0;          // when the chord completed. 0 = no chord
  uint32_t chordProgMs = 0;
  bool     chordFired = false;
  bool     chordBar   = false;   // is a chord bar currently on screen?

  bool     initDone = false, mcpOk = false, mcpWanted = false;
  uint32_t lastMcpMs = 0, lastProbeMs = 0, lastPollMs = 0;
  volatile bool mcpFlag = true;
  static const char _name[];
  static const char _enabled[];

  // --- MCP helpers ----------------------------------------------------------
  bool mcpWr(uint8_t reg, uint8_t v) {
    Wire.beginTransmission((uint8_t)mcpAddr);
    Wire.write(reg); Wire.write(v);
    return Wire.endTransmission() == 0;
  }
  bool mcpInit() {
    // MIRROR both INT pins onto one line, active low, push-pull. Interrupt on
    // change against the previous value (INTCON = 0), all inputs, pull-ups on.
    if (!mcpWr(AUI_MCP_IOCON, 0x40)) return false;
    mcpWr(AUI_MCP_IODIRA,  0xFF); mcpWr(AUI_MCP_IODIRA + 1, 0xFF);
    mcpWr(AUI_MCP_GPPUA,   0xFF); mcpWr(AUI_MCP_GPPUA  + 1, 0xFF);
    mcpWr(AUI_MCP_IPOLA,   0x00); mcpWr(AUI_MCP_IPOLA  + 1, 0x00);
    mcpWr(AUI_MCP_INTCONA, 0x00); mcpWr(AUI_MCP_INTCONA+ 1, 0x00);
    mcpWr(AUI_MCP_GPINTA,  0xFF); mcpWr(AUI_MCP_GPINTA + 1, 0xFF);
    return true;
  }
  bool mcpRead(uint16_t &val) {
    Wire.beginTransmission((uint8_t)mcpAddr);
    Wire.write(AUI_MCP_GPIOA);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)mcpAddr, 2) != 2) return false;
    const uint8_t a = Wire.read(), b = Wire.read();
    val = (uint16_t)a | ((uint16_t)b << 8);
    return true;
  }

  // --- pin plumbing ---------------------------------------------------------
  void detachAll() {
    for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
      if (auiIsr[i].attached) {
        detachInterrupt(digitalPinToInterrupt(auiIsr[i].a));
        detachInterrupt(digitalPinToInterrupt(auiIsr[i].b));
        auiIsr[i].attached = false;
      }
      if (enc[i].src == AUI_SRC_GPIO) {
        auiFreePin((int8_t)enc[i].a); auiFreePin((int8_t)enc[i].b);
        auiFreePin((int8_t)enc[i].sw);
      }
    }
    if (mcpInt >= 0) { detachInterrupt(digitalPinToInterrupt(mcpInt)); auiFreePin((int8_t)mcpInt); }
  }

  void attachAll() {
    mcpWanted = false;
    chordMs = 0; chordProgMs = 0; chordFired = false; chordBar = false;
    uint8_t live = 0;
    for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
      auiIsr[i].acc = 0; auiIsr[i].bad = 0; auiIsr[i].state = 0;
      rt[i] = EncRt();
      if (!enc[i].en) continue;

      if (enc[i].src == AUI_SRC_MCP) { mcpWanted = true; live = i + 1; continue; }
      if (enc[i].a < 0 || enc[i].b < 0) continue;
      if (!auiAllocPin((int8_t)enc[i].a) || !auiAllocPin((int8_t)enc[i].b)) continue;
      if (enc[i].sw >= 0) auiAllocPin((int8_t)enc[i].sw);

      const uint8_t mode = enc[i].pu ? INPUT_PULLUP : INPUT;
      pinMode((uint8_t)enc[i].a, mode);
      pinMode((uint8_t)enc[i].b, mode);
      if (enc[i].sw >= 0) pinMode((uint8_t)enc[i].sw, mode);

      auiIsr[i].a = (uint8_t)enc[i].a;
      auiIsr[i].b = (uint8_t)enc[i].b;
      auiIsr[i].state = (uint8_t)((auiRd(auiIsr[i].a) << 1) | auiRd(auiIsr[i].b));
      attachInterruptArg(digitalPinToInterrupt(enc[i].a), auiEncIsrFn, &auiIsr[i], CHANGE);
      attachInterruptArg(digitalPinToInterrupt(enc[i].b), auiEncIsrFn, &auiIsr[i], CHANGE);
      auiIsr[i].attached = true;
      live = i + 1;
    }

    if (mcpWanted && mcpInt >= 0 && auiAllocPin((int8_t)mcpInt)) {
      pinMode((uint8_t)mcpInt, INPUT_PULLUP);
      attachInterruptArg(digitalPinToInterrupt(mcpInt), auiMcpIsrFn, (void *)&mcpFlag, FALLING);
    }
    mcpOk = false; mcpFlag = true;

    AceUiBus &bus = aceUi();
    bus.encCount = live;
    bus.backPct  = (uint8_t)((backMs * 100) / (homeMs < 1 ? 1 : homeMs));
    for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
      bus.encRole[i] = (uint8_t)enc[i].role;
      bus.encBind[i] = (uint8_t)enc[i].bind;
      bus.encSw[i]   = (uint8_t)enc[i].swRole;
    }
    bus.encReady = (live > 0);
  }

  // --- gesture machine ------------------------------------------------------
  // One press, three possible outcomes, all decided from elapsed time. Nothing
  // waits on a future event, so nothing is ever delayed.
  void feedButton(uint8_t i, bool pressedRaw, uint32_t now) {
    EncRt &r = rt[i];
    const bool released = !pressedRaw;

    if (released != r.rawSw) { r.rawSw = released; r.swChangeMs = now; r.bounces++; }
    if (now - r.swChangeMs < (uint32_t)swDeb) return;    // still settling
    const bool isDown = !r.rawSw;
    if (isDown == r.down) return;
    r.down = isDown;

    if (isDown) {
      r.pressMs   = now;
      r.lastProgMs = now;
      r.shiftUsed = r.homeFired = r.lockFired = r.chordUsed = false;
      return;
    }

    // Release. Clear the bar first whatever happens next.
    if (r.barShown) { aceUiPush({i, AUI_EV_HOLD_ABORT, 0, false}); r.barShown = false; }
    if (r.chordUsed) return;                 // spent on a two-button gesture
    if (r.shiftUsed) return;                 // push-and-turn is not a press
    if (r.homeFired || r.lockFired) return;  // already acted at the threshold

    // A dedicated back button has no click/back split to make: a tap and a
    // one-second hold both mean the same thing, and a press long enough to
    // mean Home has already fired and returned above via homeFired. So the
    // release is unconditional here rather than timed.
    if (enc[i].swRole == AUI_SW_BACK) { aceUiPush({i, AUI_EV_BACK, 0, false}); return; }

    const uint32_t held = now - r.pressMs;
    aceUiPush({i, held < (uint32_t)backMs ? AUI_EV_CLICK : AUI_EV_BACK, 0, false});
  }

  void serviceHold(uint8_t i, uint32_t now) {
    EncRt &r = rt[i];
    if (!r.down || r.shiftUsed || r.chordUsed) return;
    const uint32_t held = now - r.pressMs;
    const uint32_t span = (uint32_t)(homeMs < 250 ? 250 : homeMs);

    if (!r.homeFired) {
      // No bar for the first 120 ms or every ordinary click flashes one, which
      // reads as lag even though nothing is late.
      if (held > 120 && now - r.lastProgMs > 55) {
        r.lastProgMs = now;
        int pct = (int)((held * 100) / span);
        if (pct > 99) pct = 99;
        aceUiPush({i, AUI_EV_HOLD_PROG, (int16_t)pct, true});
        r.barShown = true;
      }
      if (held >= span) {
        r.homeFired = true; r.barShown = false;
        aceUiPush({i, AUI_EV_HOME, 0, false});
      }
    } else if (lockMs > 0 && !r.lockFired && held >= (uint32_t)lockMs) {
      r.lockFired = true;
      aceUiPush({i, AUI_EV_LOCK, 0, false});
    }
  }

  // --- two-button chord -----------------------------------------------------
  // Any two enabled buttons held together for rebootMs. "Any two" rather than
  // "0 and 1" on purpose: the pair you can reach with two hands depends on how
  // the panel ends up mounted, and a recovery gesture that only works on one
  // specific pair of knobs is a recovery gesture you will get wrong in the
  // dark. With one encoder configured it can never form, which is the correct
  // behaviour and not a bug - see the Info page.
  //
  // Runs AFTER every button has been fed this pass and BEFORE serviceHold(),
  // so chordUsed is already set by the time the single-button holds are
  // serviced. That ordering is the entire trick; get it backwards and Home
  // fires on both knobs one second before the reboot.
  void serviceChord(uint32_t now) {
    if (rebootMs <= 0) { chordMs = 0; chordBar = false; return; }

    uint8_t held = 0, first = 0;
    for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
      if (!enc[i].en || enc[i].sw < 0 || !rt[i].down) continue;
      if (!held) first = i;
      held++;
    }

    // Chord broken, or never formed. Pull the bar down if we put one up.
    if (held < 2) {
      if (chordMs) {
        chordMs = 0; chordFired = false;
        if (chordBar) { aceUiPush({first, AUI_EV_HOLD_ABORT, 0, false}); chordBar = false; }
      }
      return;
    }

    if (!chordMs) {                          // the moment the second one lands
      chordMs = now; chordProgMs = 0; chordFired = false;
      for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
        if (!enc[i].en || !rt[i].down) continue;
        rt[i].chordUsed = true;              // this press is spent
        // Take down any single-button bar already climbing toward Home, or the
        // two bars fight over view.holdPct and the reading jitters.
        if (rt[i].barShown) { aceUiPush({i, AUI_EV_HOLD_ABORT, 0, false}); rt[i].barShown = false; }
      }
    }
    if (chordFired) return;                  // still held, already acted

    const uint32_t span   = (uint32_t)(rebootMs < 500 ? 500 : rebootMs);
    const uint32_t heldMs = now - chordMs;

    // No 120 ms grace period here, unlike the single-button hold. That grace
    // exists so an ordinary click does not flash a bar; a two-button press is
    // never ordinary, and seeing the bar appear the instant the chord forms is
    // exactly the feedback that tells you it registered.
    if (now - chordProgMs > 55) {
      chordProgMs = now;
      int pct = (int)((heldMs * 100) / span);
      if (pct > 99) pct = 99;
      aceUiPush({first, AUI_EV_CHORD_PROG, (int16_t)pct, true});
      chordBar = true;
    }
    if (heldMs >= span) {
      chordFired = true; chordBar = false;   // fired, so the release must NOT abort
      aceUiPush({first, AUI_EV_CHORD, 0, false});
    }
  }

  void drainMotion(uint8_t i, uint32_t now) {
    EncRt &r = rt[i];
    int32_t steps;
    noInterrupts();
    steps = auiIsr[i].acc; auiIsr[i].acc = 0;
    interrupts();
    if (steps == 0) return;

    r.carry += steps;
    const int ppd = enc[i].ppd < 1 ? 1 : (enc[i].ppd > 4 ? 4 : enc[i].ppd);
    int32_t detents = r.carry / ppd;
    r.carry -= detents * ppd;
    if (detents == 0) return;
    if (enc[i].inv) detents = -detents;

    // Acceleration. Squaring the rate is the usual trick and it overshoots
    // badly on a 4-detent encoder, so this scales linearly with how many
    // detents landed in one drain and caps hard. Slow turns stay 1:1, which is
    // the property that actually matters - you must be able to hit an exact
    // value without fighting it.
    int16_t out = (int16_t)detents;
    if (enc[i].accel > 0 && now - r.lastTurnMs < 60) {
      const int32_t mag = detents < 0 ? -detents : detents;
      int32_t boost = 1 + ((mag * enc[i].accel) / 50);
      if (boost > 8) boost = 8;
      out = (int16_t)(detents * boost);
    }
    r.lastTurnMs = now;
    r.lastDelta = out;
    r.detents += (uint32_t)(detents < 0 ? -detents : detents);

    if (r.down) {
      r.shiftUsed = true;
      if (r.barShown) { aceUiPush({i, AUI_EV_HOLD_ABORT, 0, false}); r.barShown = false; }
    }
    aceUiPush({i, AUI_EV_TURN, out, r.down});
  }

 public:
  void setup() override {
    attachAll();
    aceUiMenuInit();
    initDone = true;
  }

  // Public and idempotent so ace_ui_screen.cpp can call it immediately before
  // rendering. Static-init order across translation units is unspecified, so
  // without that call the screen would render one loop pass stale whenever the
  // linker put it ahead of this file - 23 ms of lag that appears and
  // disappears with unrelated build changes.
  void pollNow() {
    if (!initDone || !enabled) return;
    const uint32_t now = millis();
    if (now == lastPollMs) return;              // already done this millisecond
    lastPollMs = now;

    // Expander: read on interrupt, plus a slow safety poll in case the INT
    // line is unwired or an edge was missed while I2C was busy elsewhere.
    if (mcpWanted && i2c_sda >= 0 && i2c_scl >= 0) {
      if (!mcpOk) {
        if (now - lastProbeMs > 3000) { lastProbeMs = now; mcpOk = mcpInit(); }
      } else if (mcpFlag || now - lastMcpMs > 20) {
        mcpFlag = false; lastMcpMs = now;
        uint16_t v;
        if (!mcpRead(v)) { mcpOk = false; lastProbeMs = now; }
        else {
          for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
            if (!enc[i].en || enc[i].src != AUI_SRC_MCP) continue;
            if (enc[i].a >= 0 && enc[i].b >= 0) {
              const uint8_t cur = (uint8_t)((((v >> enc[i].a) & 1) << 1) | ((v >> enc[i].b) & 1));
              auiStepSoft(auiIsr[i], cur);
            }
            if (enc[i].sw >= 0) feedButton(i, ((v >> enc[i].sw) & 1) == 0, now);
          }
        }
      }
    }

    // Three passes, in this order, and the order is load bearing:
    //   1. read every button and drain every knob, so `down` is current
    //   2. decide whether a chord exists, which latches chordUsed
    //   3. service the single-button holds, which now know to stand down
    // Fusing 1 and 3 back into one loop - which is how this started - means
    // encoder 0's Home hold fires before encoder 1 has even been read.
    for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
      if (!enc[i].en) continue;
      if (enc[i].src == AUI_SRC_GPIO && enc[i].sw >= 0)
        feedButton(i, auiRd((uint8_t)enc[i].sw) == 0, now);
      drainMotion(i, now);
    }

    serviceChord(now);

    for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
      if (enc[i].en) serviceHold(i, now);
    }
  }

  void loop() override {
    if (!initDone) return;
    pollNow();
    aceUiMenuService();
  }

  // --- info -----------------------------------------------------------------
  // Stage 1 of bring-up lives here: flash this file alone, open /json/info,
  // turn the knob. Deltas should track your hand, bad should stay near zero,
  // and bounces should be single digits per press.
  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray st = user.createNestedArray(FPSTR(_name));
    if (!enabled)                st.add(F("disabled"));
    else if (!aceUi().encReady)  st.add(F("no encoder configured"));
    else if (aceUi().lockOn)     st.add(F("LOCKED"));
    else                         st.add(F("ok"));
    st.add("");

    if (!enabled) return;
    char buf[48];
    static const char *ROLE[5] = {"nav", "master", "browse", "value", "bound"};

    for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
      if (!enc[i].en) continue;
      snprintf_P(buf, sizeof(buf), PSTR("Enc %u"), (unsigned)i);
      JsonArray e = user.createNestedArray(buf);
      snprintf_P(buf, sizeof(buf), PSTR("%s%s %+d, %lu det, %u bad"),
                 ROLE[enc[i].role & 7],
                 enc[i].swRole == AUI_SW_BACK ? "+back" : "",
                 (int)rt[i].lastDelta,
                 (unsigned long)rt[i].detents, (unsigned)auiIsr[i].bad);
      e.add(buf);
      e.add(enc[i].src == AUI_SRC_MCP ? F(" mcp") : F(" gpio"));
    }
    if (mcpWanted) {
      JsonArray m = user.createNestedArray(F("Expander"));
      snprintf_P(buf, sizeof(buf), PSTR("0x%02X %s"), mcpAddr, mcpOk ? "ok" : "not found");
      m.add(buf); m.add(mcpInt >= 0 ? F(" int") : F(" polled"));
    }

    // "Why does my reboot chord do nothing" has exactly one common answer and
    // it is visible from here, which beats reading the source to find out that
    // a two-button gesture needs two buttons.
    if (rebootMs > 0) {
      uint8_t withSw = 0;
      for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++)
        if (enc[i].en && enc[i].sw >= 0) withSw++;
      JsonArray rb = user.createNestedArray(F("Reboot chord"));
      if (withSw < 2) {
        rb.add(F("needs 2 buttons"));
        snprintf_P(buf, sizeof(buf), PSTR(" have %u"), (unsigned)withSw);
        rb.add(buf);
      } else {
        snprintf_P(buf, sizeof(buf), PSTR("%d ms"), rebootMs);
        rb.add(buf);
        rb.add(chordMs ? F(" HOLDING") : F(" ready"));
      }
    }
  }

  // --- config ---------------------------------------------------------------
  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top["backMs"] = backMs;
    top["homeMs"] = homeMs;
    top["lockMs"] = lockMs;
    top["rebootMs"] = rebootMs;
    top["swDeb"]  = swDeb;
    top["mcpAddr"] = mcpAddr;
    top["mcpInt"]  = mcpInt;
    char k[8];
    for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
      #define AUI_K(s) (snprintf(k, sizeof(k), "e%u" s, (unsigned)i), k)
      top[AUI_K("en")]    = enc[i].en;
      top[AUI_K("src")]   = enc[i].src;
      top[AUI_K("a")]     = enc[i].a;
      top[AUI_K("b")]     = enc[i].b;
      top[AUI_K("sw")]    = enc[i].sw;
      top[AUI_K("ppd")]   = enc[i].ppd;
      top[AUI_K("inv")]   = enc[i].inv;
      top[AUI_K("pu")]    = enc[i].pu;
      top[AUI_K("acc")]   = enc[i].accel;
      top[AUI_K("role")]  = enc[i].role;
      top[AUI_K("btn")]   = enc[i].swRole;
      top[AUI_K("bind")]  = enc[i].bind;
      #undef AUI_K
    }
  }

  bool readFromConfig(JsonObject &root) override {
    JsonObject top = root[FPSTR(_name)];
    bool ok = !top.isNull();

    ok &= getJsonValue(top[FPSTR(_enabled)], enabled, true);
    ok &= getJsonValue(top["backMs"], backMs, 500);
    ok &= getJsonValue(top["homeMs"], homeMs, 2000);
    ok &= getJsonValue(top["lockMs"], lockMs, 0);
    ok &= getJsonValue(top["rebootMs"], rebootMs, 3000);
    ok &= getJsonValue(top["swDeb"],  swDeb,  15);
    ok &= getJsonValue(top["mcpAddr"], mcpAddr, 0x20);
    ok &= getJsonValue(top["mcpInt"],  mcpInt,  -1);

    // Back must land inside the Home gesture or one of them is unreachable.
    if (backMs < 120)  backMs = 120;
    if (homeMs < backMs + 300) homeMs = backMs + 300;
    if (lockMs && lockMs < homeMs + 500) lockMs = homeMs + 500;
    // The chord has to outlast Home by a clear margin or the two gestures are
    // indistinguishable to a hand: you would be aiming for "both buttons, three
    // seconds" and hitting "main menu, twice" on the way there.
    if (rebootMs && rebootMs < homeMs + 500) rebootMs = homeMs + 500;
    if (rebootMs < 0) rebootMs = 0;

    char k[8];
    for (uint8_t i = 0; i < ACE_UI_MAX_ENC; i++) {
      #define AUI_K(s) (snprintf(k, sizeof(k), "e%u" s, (unsigned)i), k)
      ok &= getJsonValue(top[AUI_K("en")],   enc[i].en,   i == 0);
      ok &= getJsonValue(top[AUI_K("src")],  enc[i].src,  AUI_SRC_GPIO);
      ok &= getJsonValue(top[AUI_K("a")],    enc[i].a,    i == 0 ? 25 : -1);
      ok &= getJsonValue(top[AUI_K("b")],    enc[i].b,    i == 0 ? 26 : -1);
      ok &= getJsonValue(top[AUI_K("sw")],   enc[i].sw,   i == 0 ? 27 : -1);
      ok &= getJsonValue(top[AUI_K("ppd")],  enc[i].ppd,  4);
      ok &= getJsonValue(top[AUI_K("inv")],  enc[i].inv,  false);
      ok &= getJsonValue(top[AUI_K("pu")],   enc[i].pu,   true);
      ok &= getJsonValue(top[AUI_K("acc")],  enc[i].accel, 40);
      ok &= getJsonValue(top[AUI_K("role")], enc[i].role, AUI_ROLE_NAV);
      ok &= getJsonValue(top[AUI_K("btn")],  enc[i].swRole, AUI_SW_FULL);
      ok &= getJsonValue(top[AUI_K("bind")], enc[i].bind, AUI_P_BRI);
      #undef AUI_K

      // Config is user/API editable JSON, not just settings-page dropdowns, so
      // out-of-range values have to be caught here rather than trusted. Role,
      // swRole and bind are array/switch selectors elsewhere (e.g. the ROLE[]
      // lookup in addToJsonInfo) - out of range there is an OOB read, not a
      // graceful no-op. Pin fields feed `v >> pin` against a 16-bit MCP read;
      // a pin outside 0-15 is a shift-by-invalid-count, which is UB.
      if (enc[i].src != AUI_SRC_GPIO && enc[i].src != AUI_SRC_MCP) enc[i].src = AUI_SRC_GPIO;
      if (enc[i].role < AUI_ROLE_NAV || enc[i].role > AUI_ROLE_BOUND) enc[i].role = AUI_ROLE_NAV;
      if (enc[i].swRole < AUI_SW_FULL || enc[i].swRole > AUI_SW_BACK) enc[i].swRole = AUI_SW_FULL;
      if (enc[i].bind < 0 || enc[i].bind >= AUI_P_COUNT) enc[i].bind = AUI_P_BRI;
      const int pinMax = (enc[i].src == AUI_SRC_MCP) ? 15 : 39;
      if (enc[i].a  != -1 && (enc[i].a  < 0 || enc[i].a  > pinMax)) enc[i].a  = -1;
      if (enc[i].b  != -1 && (enc[i].b  < 0 || enc[i].b  > pinMax)) enc[i].b  = -1;
      if (enc[i].sw != -1 && (enc[i].sw < 0 || enc[i].sw > pinMax)) enc[i].sw = -1;
    }

    // A settings save, not the boot-time read: rewire live rather than making
    // you reboot to test a pin change or an inverted direction.
    if (initDone) { detachAll(); attachAll(); }
    return ok;
  }

  // NOTE ON QUOTING: every string below is emitted verbatim into a
  // single-quoted JavaScript literal on the settings page, and ALL usermods
  // share one <script> block. One stray apostrophe is a SyntaxError that kills
  // the rest of the block - which is how a broken help string in one usermod
  // makes another usermod's dropdowns silently disappear. Use &#39; if you
  // need an apostrophe. Never a bare one.
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

    char k[12];
    for (unsigned i = 0; i < ACE_UI_MAX_ENC; i++) {
      snprintf(k, sizeof(k), "e%usrc", i);  dd(k);
        opt("direct GPIO", AUI_SRC_GPIO); opt("MCP23017 pin", AUI_SRC_MCP);
      snprintf(k, sizeof(k), "e%uppd", i);  dd(k);
        opt("4 pulses / detent (most)", 4);
        opt("2 pulses / detent", 2);
        opt("1 pulse / detent", 1);
      snprintf(k, sizeof(k), "e%urole", i); dd(k);
        opt("Nav - menu + brightness", AUI_ROLE_NAV);
        opt("Master - brightness only", AUI_ROLE_MASTER);
        opt("Browse - cursor only",     AUI_ROLE_BROWSE);
        opt("Value - edit only",        AUI_ROLE_VALUE);
        opt("Bound - one parameter",    AUI_ROLE_BOUND);
      snprintf(k, sizeof(k), "e%ubtn", i);  dd(k);
        opt("Full - click, back, home", AUI_SW_FULL);
        opt("Back - click backs out",   AUI_SW_BACK);
      snprintf(k, sizeof(k), "e%ubind", i); dd(k);
        opt("Brightness", AUI_P_BRI);   opt("Effect", AUI_P_FX);
        opt("Palette", AUI_P_PAL);      opt("Speed", AUI_P_SPEED);
        opt("Intensity", AUI_P_INTENSITY);
        opt("Custom 1", AUI_P_C1);      opt("Custom 2", AUI_P_C2);
        opt("Custom 3", AUI_P_C3);
        opt("Check 1", AUI_P_O1);       opt("Check 2", AUI_P_O2);
        opt("Check 3", AUI_P_O3);
        opt("Hue", AUI_P_HUE);          opt("Saturation", AUI_P_SAT);
        opt("CCT", AUI_P_CCT);          opt("Preset", AUI_P_PRESET);
        opt("Segment", AUI_P_SEG);
    }

    info("e0a",  "encoder A (CLK). <b>25</b> is safe. never 12, 0, 6-11");
    info("e0b",  "encoder B (DT). take it from the same group as A");
    info("e0sw", "push switch. &minus;1 if the encoder has none");
    info("e0ppd","if one detent moves two steps, this is wrong");
    info("e0inv","tick if clockwise goes down");
    info("e0pu", "internal pull-ups. leave on unless the module has its own");
    info("e0acc","0 = strictly 1:1. higher = fast spins cover more ground");
    info("e0btn","what the PUSH does. set encoder 1 to <b>Back</b> and its role "
                 "to <b>Master</b> and you have a knob that exits and dims");
    info("backMs","hold this long, then release, to go back one level");
    info("homeMs","hold this long to jump to the main menu. must exceed back");
    info("lockMs","hold this long to lock the knob. <b>0</b> = off");
    info("rebootMs","hold TWO knob buttons this long to reboot. needs a second "
                    "encoder. <b>0</b> = off");
    info("swDeb","button debounce, ms. raise if one press registers twice");
    info("mcpAddr","MCP23017 address, 32 = 0x20 with A0-A2 grounded");
    info("mcpInt","INT line. <b>35</b> is ideal - input-only, useless otherwise");
  }

  uint16_t getId() override { return USERMOD_ID_UNSPECIFIED; }
};

const char AceUiEncoderUsermod::_name[]    PROGMEM = "AceUI-Enc";
const char AceUiEncoderUsermod::_enabled[] PROGMEM = "enabled";

static AceUiEncoderUsermod ace_ui_encoder;
REGISTER_USERMOD(ace_ui_encoder);

// The screen calls this before it renders. Declared weak in ace_ui_bus.h, so
// a build without this file still links and the screen simply does not get the
// early poll.
extern "C" void aceUiEncoderPoll() { ace_ui_encoder.pollNow(); }
