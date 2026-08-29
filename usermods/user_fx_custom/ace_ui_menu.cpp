#include "wled.h"
#include "ace_ui_bus.h"

// ===========================================================================
// ace_ui_menu.cpp - the model. No hardware, no display, no Usermod subclass.
// ===========================================================================
// This file registers nothing and has no loop() of its own. Both the encoder
// and the screen call aceUiMenuService() every pass. Draining the event ring
// is idempotent, so it is no longer guarded per-millisecond - only the
// periodic housekeeping is. The old guard meant whichever of the two the
// linker put second had its call swallowed, and a knob turn could sit in the
// ring for a whole 23 ms frame before anybody looked at it.
//
// ---------------------------------------------------------------------------
// SHAPE OF THE UI
// ---------------------------------------------------------------------------
//   HOME IS THE MAIN MENU.  Hold 2 s from anywhere and you land here.
//
//     Now Playing  ->  status screen. Click again -> the running effect's
//                      params. Turning here is brightness.
//     VU Meter     ->  the whole body is a 16-band spectrum. Turning here is
//                      brightness, a click leaves. Nothing else is drawn.
//     Effects      ->  filter -> list. Click an entry to APPLY it; click the
//                      entry that is already applied to drill into its params.
//                      Same gesture, everywhere, once something is selected.
//     Palette / Params / Brightness / Colour / Presets / Segment / System
//
// ---------------------------------------------------------------------------
// NOTHING MOVES THE SCREEN BUT YOU
// ---------------------------------------------------------------------------
// The 20-second walk back to the main menu is gone as a default. It is now
// aceUi().idleHomeSec, published by the screen usermod, and it DEFAULTS TO 0 -
// never. Park the panel on the VU meter, or halfway down the palette list, and
// that is where it still is an hour later.
//
// The old behaviour was defensible when Home was a status page: leaving a
// half-finished edit on screen advertising itself is untidy. It stopped being
// defensible the moment Home became the MAIN MENU, because then the timeout
// was not tidying up after you - it was throwing away the screen you had
// deliberately chosen, twenty seconds after you chose it. If you want it back,
// AceUI-Screen -> "return to menu after" takes a number of seconds.
//
// The panel is a two-colour part: rows 0-15 are yellow and 16-63 are blue,
// with a physical dead band between them. So the running effect name is pinned
// to the yellow band on EVERY screen and the body never spends a row on it.
// The IP address is gone from the front page - it lives on System -> Network
// with the SSID and the key, which is where you actually go looking for it.
//
// ---------------------------------------------------------------------------
// COMMIT ON CLICK
// ---------------------------------------------------------------------------
// Turning in the effect list moves the cursor and changes nothing. Switching a
// mode reallocates SEGENV data, and scrolling 187 of them at four detents a
// second is a visible stutter on the cube - so the list shows you where you
// are with a filled dot on the LIVE entry and a bar on the CURSOR, and only a
// click commits.
//
// The double-click-to-revert machine that used to sit behind that is GONE.
// Back is a 500 ms hold now, so there is no rollback window to defend and a
// commit is permanent the moment it happens. That removed three globals, a
// timer and a class of "why did my effect change back" report.
//
// ---------------------------------------------------------------------------
// THE PARAMS SCREEN READS YOUR OWN METADATA
// ---------------------------------------------------------------------------
// Every effect in this folder names its sliders in _data_FX_MODE_*:
//
//   "Ace 3-D Cube Cell@Speed,Drive,Beat shift X,...,Tumble,Flat mode;;!;2f;..."
//
// strip.getModeData() hands that string back at runtime, so the knob shows
// "Beat shift X" and "Flat mode" instead of "Custom 1" and "Check 2", and
// unused slots simply do not appear in the list.
//
// ---------------------------------------------------------------------------
// THE PALETTE LIST IS NOT THE PALETTE ID
// ---------------------------------------------------------------------------
// Built-in palettes are ids 0..N-1. Custom ones are ids 255, 254, 253 ...
// counting DOWN, with nothing in between. So the row the encoder sits on and
// the number handed to setPalette() are different quantities - see the palette
// section below for the two conversions and why the naive version makes the
// knob look broken.
//
// ---------------------------------------------------------------------------
// THE EFFECT FILTER IS A TABLE, NOT A SWITCH
// ---------------------------------------------------------------------------
// Effects -> Show lists one row per FAMILY, and a family is nothing more than
// a name prefix. "Ace Gyro Horizon" is not an "Ace 3-D" effect and never
// matched either of the two original prefixes, so the whole gyro roster was
// only reachable by scrolling All effects past 180 stock WLED modes - which is
// to say it was not reachable.
//
// Adding a family is now ONE line in AUI_FILT and one in AUI_FILT_PRE, right
// next to each other so they cannot drift apart. That is the same property the
// effect files have: a new one costs a new file and no edits anywhere else.
//
// ---------------------------------------------------------------------------
// IMU ACTIONS FROM THE MENU
// ---------------------------------------------------------------------------
// System -> Calibrate gyro / Level now / Clear level trim call a weak symbol
// that ace_imu_mpu6050.cpp defines. If that file is not in the build the three
// entries report "not available" and this one still compiles.
//
// ---------------------------------------------------------------------------
// REBOOTING
// ---------------------------------------------------------------------------
// Two ways in, both landing on the same deferred reboot:
//
//   System -> Reboot        confirm screen, click to commit. works with one knob
//   two buttons held 3 s    the encoder's chord. no confirm - the gesture IS
//                           the confirmation, and it is abortable the whole way
//
// Neither calls ESP.restart(). Both set WLED's own doReboot flag, checked at
// the top of WLED::loop(), so the current strip update finishes and the config
// file is not left half-written. Both also wait ~700 ms first, purely so the
// panel gets to draw the word before the board goes down; a reboot you cannot
// see is indistinguishable from a crash.
// ===========================================================================

extern "C" void aceImuAction(int8_t action) __attribute__((weak));

// Palette names live in a PROGMEM JSON blob with no public accessor. Declaring
// it here rather than relying on a header keeps this file independent; set
// -D AUI_PALETTE_NAMES=0 if your tree ever renames or drops the symbol.
#ifndef AUI_PALETTE_NAMES
  #define AUI_PALETTE_NAMES 1
#endif
#if AUI_PALETTE_NAMES
  extern const char JSON_palette_names[] PROGMEM;
#endif

#ifndef AUI_PRESET_NAMES
  #define AUI_PRESET_NAMES 1
#endif
#ifndef AUI_PRESET_SAVE
  #define AUI_PRESET_SAVE 1
#endif

// The network page reads WLED's AP globals and mDNS name directly. Both have
// been stable for many releases; set either to 0 if your tree renames them.
#ifndef AUI_HAVE_APCFG
  #define AUI_HAVE_APCFG 1
#endif
#ifndef AUI_HAVE_MDNS
  #define AUI_HAVE_MDNS 1
#endif
// Printing the Wi-Fi key on a panel bolted to the outside of the cube is a
// deliberate choice - it is the same information printed on the underside of
// every router - but it is one line to turn off.
#ifndef AUI_SHOW_KEYS
  #define AUI_SHOW_KEYS 1
#endif

// Idle return to Home is aceUi().idleHomeSec now, not a macro - see the note
// at the top of this file. Zero means never, and zero is the default.

// doReboot is a WLED global, set by the settings page's own reboot button and
// checked at the top of WLED::loop(). Using it rather than ESP.restart() is
// what lets the strip finish its update and the filesystem settle first.
// Set -D AUI_HAVE_DOREBOOT=0 if your tree ever renames it.
#ifndef AUI_HAVE_DOREBOOT
  #define AUI_HAVE_DOREBOOT 1
#endif
// Long enough for the panel to paint one frame at any sane fps, short enough
// that nobody reaches for the power switch thinking it did not take.
#ifndef AUI_REBOOT_DELAY_MS
  #define AUI_REBOOT_DELAY_MS 700
#endif

// Same split as ace_imu_mpu6050.cpp: WLED PR #4609 renamed the no-argument
// serializeConfig() and reused the name for a JsonObject overload.
#ifndef AUI_LEGACY_CFG_SAVE
  #define AUI_LEGACY_CFG_SAVE 0
#endif
static void auiSaveConfig() {
#if AUI_LEGACY_CFG_SAVE
  serializeConfig();
#else
  serializeConfigToFS();
#endif
}

// ---------------------------------------------------------------------------
// screens
// ---------------------------------------------------------------------------
enum : uint8_t {
  SC_ROOT = 0, SC_NOWPLAY, SC_VU, SC_FXFILTER, SC_FXLIST, SC_PAL, SC_PARAMS,
  SC_EDIT, SC_COLOUR, SC_PRESETS, SC_SEG, SC_SYSTEM, SC_NET, SC_CONFIRM
};

// Rows on the main menu. Kept as an enum rather than bare numbers in auiClick()
// because inserting VU Meter at 1 renumbered everything below it, and a switch
// on nine magic integers is exactly the kind of thing that survives the edit
// looking correct and lands you on Presets when you asked for Colour.
enum : uint8_t {
  RT_NOWPLAY = 0, RT_VU, RT_FX, RT_PAL, RT_PARAMS, RT_BRI,
  RT_COLOUR, RT_PRESETS, RT_SEG, RT_SYSTEM, RT_COUNT
};

struct AuiFrame { uint8_t id; int16_t cursor; };
static AuiFrame auiStack[7] = {{SC_ROOT, 0}};
static uint8_t  auiDepth   = 1;          // stack[0] is ALWAYS the main menu
static bool     auiStarted = false;
static uint32_t auiHouseMs = 0;

static int8_t   auiFilter  = 0;          // 0 Ace 3-D, 1 Ace 2-D, 2 recent, 3 all
static uint8_t  auiFxIdx[192];
static int16_t  auiFxCount = 0;
static uint8_t  auiRecent[8];
static uint8_t  auiRecentN = 0;

static uint8_t  auiEditParam = AUI_P_BRI;
static uint8_t  auiParamSlot[8];         // metadata slot -> param id, per effect
static uint8_t  auiParamN = 0;

// The confirm screen used to have exactly one customer and said so in its own
// title string. Now that Reboot uses it too, WHAT is being confirmed has to be
// state - otherwise the screen cheerfully offers to overwrite a preset and
// then reboots instead, which is the worst possible way to get that wrong.
enum : uint8_t { AUI_CF_NONE = 0, AUI_CF_PRESET, AUI_CF_REBOOT };
static uint8_t  auiConfirmKind = AUI_CF_NONE;
static uint8_t  auiConfirmAct = 0;       // preset number, when kind is PRESET
static uint8_t  auiSegSel = 0;

static uint32_t auiRebootAt = 0;         // millis() to fire at. 0 = not armed
static bool     auiChordArmed = false;   // a two-button reboot hold is climbing

static uint8_t  auiHue = 0, auiSat = 255;
static char     auiToast[AUI_TITLE_LEN] = {0};

// forward declarations - the event handlers at the bottom call almost
// everything above them and a couple of things below them
static void auiRefresh();
static void auiToastSet(const char *msg, uint16_t ms = 1200);
static int16_t auiBuildFx();       // returns the row count - see the note there
static void auiBuildParams();
static void auiRowText(int16_t i, char *out, uint8_t n);
static int16_t auiPalIndex(uint8_t id);
static int32_t auiGet(uint8_t id);
static void auiSet(uint8_t id, int32_t v);

// ---------------------------------------------------------------------------
// segment access
// ---------------------------------------------------------------------------
static Segment &auiSeg() {
  uint8_t n = strip.getSegmentsNum();
  if (auiSegSel >= n) auiSegSel = strip.getMainSegmentId();
  return strip.getSegment(auiSegSel);
}
static void auiApplied() { stateChanged = true; stateUpdated(CALL_MODE_BUTTON); }

// ---------------------------------------------------------------------------
// mode metadata
// ---------------------------------------------------------------------------
static void auiCopyP(const char *src, char *out, uint8_t n, char stop1, char stop2) {
  uint8_t i = 0;
  while (i < n - 1) {
    const char c = (char)pgm_read_byte(src + i);
    if (!c || c == stop1 || c == stop2) break;
    out[i] = c; i++;
  }
  out[i] = 0;
}

// strncpy() does not terminate when it truncates, and every call below is
// copying a runtime string - an effect name, a palette name, a toast - into a
// fixed-width view field. One bounded copy that always terminates, used
// everywhere, is cheaper than remembering to write the terminator by hand at
// twenty call sites and getting nineteen of them right.
static void auiStr(char *dst, const char *src, uint8_t n) {
  if (!n) return;
  uint8_t i = 0;
  while (i < (uint8_t)(n - 1) && src[i]) { dst[i] = src[i]; i++; }
  dst[i] = 0;
}

static void auiModeName(uint8_t m, char *out, uint8_t n) {
  const char *md = strip.getModeData(m);
  if (!md) { snprintf(out, n, "FX %u", (unsigned)m); return; }
  auiCopyP(md, out, n, '@', ';');
  if (!out[0]) snprintf(out, n, "FX %u", (unsigned)m);
}

// slot 0..7 = speed, intensity, c1, c2, c3, o1, o2, o3.
// Returns false when the effect leaves that slot empty, which is how the
// Params list stays short instead of showing eight rows of nothing.
static bool auiModeSlider(uint8_t m, uint8_t slot, char *out, uint8_t n) {
  out[0] = 0;
  const char *md = strip.getModeData(m);
  if (!md) return false;
  const char *p = md;
  while (pgm_read_byte(p) && pgm_read_byte(p) != '@') p++;
  if (pgm_read_byte(p) != '@') return false;
  p++;
  for (uint8_t s = 0; s < slot; s++) {
    while (pgm_read_byte(p) && pgm_read_byte(p) != ',' && pgm_read_byte(p) != ';') p++;
    if (pgm_read_byte(p) != ',') return false;
    p++;
  }
  auiCopyP(p, out, n, ',', ';');
  if (!out[0]) return false;
  if (out[0] == '!') {                       // WLED shorthand for the defaults
    if (slot == 0) auiStr(out, "Speed", n);
    else if (slot == 1) auiStr(out, "Intensity", n);
  }
  return true;
}

// ---------------------------------------------------------------------------
// palettes: counting them, and the row/id conversion
// ---------------------------------------------------------------------------
// WS2812FX::getPaletteCount() existed in 0.14 and was removed in the 16.x
// palette refactor, so there is nothing to call. Two separate numbers matter
// here and WLED hands over neither:
//
//   BUILT-INS   ids 0 .. N-1, contiguous. N is exactly the number of names in
//               JSON_palette_names - which this file already walks to draw the
//               rows - so counting them once costs nothing and is guaranteed
//               to agree with what the screen says.
//   CUSTOMS     ids 255, 254, 253 ... counting DOWN, one per paletteN.json on
//               the filesystem, ten at most. NOT contiguous with the built-ins.
//
// Scroll one past the last built-in with the naive mapping and you walk into a
// dead band that Segment::setPalette() silently clamps back to 0. The knob
// keeps turning, the rows keep scrolling, and the cube snaps to Default -
// which reads as "the encoder is broken" rather than "that palette does not
// exist". auiPalId() and auiPalIndex() are the two conversions, and AUI_P_PAL
// works in ROW space end to end so a BOUND encoder walks the whole list.
// ---------------------------------------------------------------------------
#ifndef AUI_PAL_BUILTIN_FALLBACK
  #define AUI_PAL_BUILTIN_FALLBACK 13
#endif

static uint8_t auiPalBuiltin() {
  static uint8_t cached = 0;                 // POD, zero-init, no guard variable
  if (cached) return cached;
#if AUI_PALETTE_NAMES
  const char *s = JSON_palette_names;
  uint16_t n = 0; bool inStr = false;
  for (char c = (char)pgm_read_byte(s); c; c = (char)pgm_read_byte(++s))
    if (c == '"') { if (inStr) n++; inStr = !inStr; }
  // clamped so a built-in can never reach the custom band at 246..255
  cached = (uint8_t)(n > 246 ? 246 : n);
#endif
  if (!cached) cached = AUI_PAL_BUILTIN_FALLBACK;
  return cached;
}

// strip.customPalettes is a public member in 16.x but has not always been.
// Probe for it; the fallback overload only instantiates when the member is
// truly absent. `strip` goes in as a parameter rather than being named in the
// body, and that is not stylistic: a non-dependent expression inside a
// template body is checked at DEFINITION time, so `strip.customPalettes` there
// is a hard error on a tree that lacks the member even though the overload is
// never chosen. Deducing T makes s.customPalettes dependent, which defers it
// to instantiation.
namespace aui_detail {
  template <typename T>
  static auto palCustom(const T &s, int) -> decltype(s.customPalettes.size(), (uint8_t)0) {
    const size_t n = s.customPalettes.size();
    return (uint8_t)(n > 10 ? 10 : n);       // ids 255..246, WLED's own ceiling
  }
  template <typename T>
  static uint8_t palCustom(const T &, long) { return 0; }
}
static inline uint8_t auiPalCustom() { return aui_detail::palCustom(strip, 0); }

static inline int16_t auiPalCount() {
  return (int16_t)auiPalBuiltin() + (int16_t)auiPalCustom();
}

// row -> palette id
static uint8_t auiPalId(int16_t idx) {
  const int16_t bi = (int16_t)auiPalBuiltin();
  if (idx < 0)  return 0;
  if (idx < bi) return (uint8_t)idx;
  const int16_t k = idx - bi;                        // 0-based custom slot
  if (k >= (int16_t)auiPalCustom()) return 0;        // off the end -> Default
  return (uint8_t)(255 - k);
}

// palette id -> row. Anything in the dead band lands on row 0 rather than
// parking the cursor somewhere the list cannot draw.
static int16_t auiPalIndex(uint8_t id) {
  const int16_t bi = (int16_t)auiPalBuiltin();
  if (id < (uint8_t)bi) return (int16_t)id;
  if (id > 245) {
    const int16_t k = (int16_t)(255 - id);
    if (k < (int16_t)auiPalCustom()) return bi + k;
  }
  return 0;
}

// Takes an ID, not a row.
static void auiPaletteName(uint8_t p, char *out, uint8_t n) {
  if (p > 245) {                             // customs, named after their file
    snprintf(out, n, "Custom %u", (unsigned)(255 - p));
    return;
  }
#if AUI_PALETTE_NAMES
  const char *s = JSON_palette_names;
  uint8_t idx = 0; bool inStr = false; uint8_t o = 0;
  while (true) {
    const char c = (char)pgm_read_byte(s++);
    if (!c) break;
    if (c == '"') {
      if (inStr) { if (idx == p) { out[o] = 0; return; } idx++; o = 0; }
      inStr = !inStr;
      continue;
    }
    if (inStr && idx == p && o < n - 1) out[o++] = c;
  }
#endif
  snprintf(out, n, "Palette %u", (unsigned)p);
}

// Local HSV at full value. Deliberately not FastLED's hsv2rgb_rainbow: which
// colour helpers WLED re-exports has moved between 0.14, 0.15 and 0.16, and
// twelve lines here is cheaper than a build that breaks on the next merge.
static void auiHsv(uint8_t h, uint8_t s, uint8_t &r, uint8_t &g, uint8_t &b) {
  const uint8_t region = h / 43, rem = (uint8_t)((h - region * 43) * 6);
  const uint8_t p = (uint8_t)((255 * (255 - s)) >> 8);
  const uint8_t q = (uint8_t)((255 * (255 - ((s * rem) >> 8))) >> 8);
  const uint8_t t = (uint8_t)((255 * (255 - ((s * (255 - rem)) >> 8))) >> 8);
  switch (region) {
    case 0:  r = 255; g = t;   b = p;   break;
    case 1:  r = q;   g = 255; b = p;   break;
    case 2:  r = p;   g = 255; b = t;   break;
    case 3:  r = p;   g = q;   b = 255; break;
    case 4:  r = t;   g = p;   b = 255; break;
    default: r = 255; g = p;   b = q;   break;
  }
}

// ---------------------------------------------------------------------------
// parameter registry - one table of getters, used by menus and BOUND encoders
// ---------------------------------------------------------------------------
static int32_t auiGet(uint8_t id) {
  Segment &sg = auiSeg();
  switch (id) {
    case AUI_P_BRI:       return bri;
    case AUI_P_FX:        return sg.mode;
    case AUI_P_PAL:       return auiPalIndex(sg.palette);   // rows, not ids
    case AUI_P_SPEED:     return sg.speed;
    case AUI_P_INTENSITY: return sg.intensity;
    case AUI_P_C1:        return sg.custom1;
    case AUI_P_C2:        return sg.custom2;
    case AUI_P_C3:        return sg.custom3;
    case AUI_P_O1:        return sg.check1 ? 1 : 0;
    case AUI_P_O2:        return sg.check2 ? 1 : 0;
    case AUI_P_O3:        return sg.check3 ? 1 : 0;
    case AUI_P_HUE:       return auiHue;
    case AUI_P_SAT:       return auiSat;
    case AUI_P_CCT:       return sg.cct;
    case AUI_P_SEG:       return auiSegSel;
    default:              return 0;
  }
}

static void auiRange(uint8_t id, int32_t &lo, int32_t &hi) {
  lo = 0; hi = 255;
  switch (id) {
    case AUI_P_FX:  hi = (int32_t)strip.getModeCount() - 1; break;
    case AUI_P_PAL: hi = (int32_t)auiPalCount() - 1; break;
    case AUI_P_O1: case AUI_P_O2: case AUI_P_O3: hi = 1; break;
    case AUI_P_C1: case AUI_P_C2: case AUI_P_C3: hi = 255; break;
    case AUI_P_SEG: hi = (int32_t)strip.getSegmentsNum() - 1; break;
    case AUI_P_PRESET: lo = 1; hi = 25; break;
    default: break;
  }
  if (hi < lo) hi = lo;
}

static void auiSet(uint8_t id, int32_t v) {
  int32_t lo, hi; auiRange(id, lo, hi);
  if (v < lo) v = lo;
  if (v > hi) v = hi;
  Segment &sg = auiSeg();
  switch (id) {
    case AUI_P_BRI:       bri = (uint8_t)v; break;
    case AUI_P_FX:        sg.setMode((uint8_t)v); break;
    case AUI_P_PAL:       sg.setPalette(auiPalId((int16_t)v)); break;
    case AUI_P_SPEED:     sg.speed = (uint8_t)v; break;
    case AUI_P_INTENSITY: sg.intensity = (uint8_t)v; break;
    case AUI_P_C1:        sg.custom1 = (uint8_t)v; break;
    case AUI_P_C2:        sg.custom2 = (uint8_t)v; break;
    case AUI_P_C3:        sg.custom3 = (uint8_t)v; break;
    case AUI_P_O1:        sg.check1 = v != 0; break;
    case AUI_P_O2:        sg.check2 = v != 0; break;
    case AUI_P_O3:        sg.check3 = v != 0; break;
    case AUI_P_CCT:       sg.setCCT((uint8_t)v); break;
    case AUI_P_SEG:       auiSegSel = (uint8_t)v; return;      // no state push
    case AUI_P_HUE: case AUI_P_SAT: {
      if (id == AUI_P_HUE) auiHue = (uint8_t)v; else auiSat = (uint8_t)v;
      uint8_t r, g2, b2;
      auiHsv(auiHue, auiSat, r, g2, b2);
      sg.setColor(0, RGBW32(r, g2, b2, 0));
      break;
    }
    default: return;
  }
  auiApplied();
}

static void auiParamName(uint8_t id, char *out, uint8_t n) {
  const uint8_t m = auiSeg().mode;
  switch (id) {
    case AUI_P_BRI:       auiStr(out, "Brightness", n); return;
    case AUI_P_HUE:       auiStr(out, "Hue", n); return;
    case AUI_P_SAT:       auiStr(out, "Saturation", n); return;
    case AUI_P_CCT:       auiStr(out, "CCT", n); return;
    case AUI_P_SEG:       auiStr(out, "Segment", n); return;
    case AUI_P_FX:        auiStr(out, "Effect", n); return;
    case AUI_P_PAL:       auiStr(out, "Palette", n); return;
    default: break;
  }
  const uint8_t slot = (uint8_t)(id - AUI_P_SPEED);
  if (slot < 8 && auiModeSlider(m, slot, out, n)) return;
  static const char *FB[8] = {"Speed","Intensity","Custom 1","Custom 2",
                              "Custom 3","Check 1","Check 2","Check 3"};
  auiStr(out, slot < 8 ? FB[slot] : "?", n);
  out[n - 1] = 0;
}

// ---------------------------------------------------------------------------
// effect list
// ---------------------------------------------------------------------------
static bool auiPrefix(const char *name, const char *pre) {
  while (*pre) { if (*name++ != *pre++) return false; }
  return true;
}

// The Show screen, in order. The first AUI_F_PREFIXED rows are name-prefix
// matches; the two after them are special-cased. Keep the two tables adjacent
// and the same length - a family whose label and prefix disagree is a filter
// that silently returns nothing, which looks exactly like a broken build.
//
// Gyro sits with the other two Ace families rather than at the bottom: the
// three of them are what you actually browse, and Recent and All effects are
// the escape hatches underneath.
enum : uint8_t {
  AUI_F_3D = 0, AUI_F_2D, AUI_F_GYRO,   // prefix families
  AUI_F_PREFIXED,                        // <- count of the above. not a row
  AUI_F_RECENT = AUI_F_PREFIXED, AUI_F_ALL,
  AUI_F_COUNT
};
static const char *AUI_FILT[AUI_F_COUNT] = {
  "Ace 3-D", "Ace 2-D", "Ace Gyro", "Recent", "All effects"};
static const char *AUI_FILT_PRE[AUI_F_PREFIXED] = {
  "Ace 3-D", "Ace 2-D", "Ace Gyro"};

// Shift-turn in the list jumps by first letter. Comparing name[0] made that a
// no-op inside any Ace family, because every one of them starts with the same
// four characters: the search walked all 34 entries, never found a different
// first letter, and put you back where you started. Compare the first
// character AFTER the family prefix instead and the Ace 3-D list jumps
// C-G-L-Q-R-S-T, which crosses the roster in two flicks the way it was always
// supposed to.
static char auiFxKey(uint8_t m) {
  char nm[28];
  auiModeName(m, nm, sizeof(nm));
  for (uint8_t f = 0; f < AUI_F_PREFIXED; f++) {
    if (!auiPrefix(nm, AUI_FILT_PRE[f])) continue;
    const char *s = nm + strlen(AUI_FILT_PRE[f]);
    while (*s == ' ') s++;
    return *s ? *s : nm[0];
  }
  return nm[0];
}

// Returns the row count so the caller can tell "this family is empty" from
// "this family is full", which is the difference between a useful message and
// a blank screen.
static int16_t auiBuildFx() {
  auiFxCount = 0;
  const uint8_t n = strip.getModeCount();
  const uint8_t f = (auiFilter >= 0 && auiFilter < (int8_t)AUI_F_COUNT)
                    ? (uint8_t)auiFilter : (uint8_t)AUI_F_ALL;

  if (f == AUI_F_RECENT) {
    for (uint8_t i = 0; i < auiRecentN && auiFxCount < 192; i++)
      auiFxIdx[auiFxCount++] = auiRecent[i];
    return auiFxCount;
  }

  const char *pre = (f < AUI_F_PREFIXED) ? AUI_FILT_PRE[f] : nullptr;
  char nm[28];
  for (uint8_t m = 0; m < n && auiFxCount < 192; m++) {
    if (!pre) { auiFxIdx[auiFxCount++] = m; continue; }
    auiModeName(m, nm, sizeof(nm));
    if (auiPrefix(nm, pre)) auiFxIdx[auiFxCount++] = m;
  }
  return auiFxCount;
}

static void auiPushRecent(uint8_t m) {
  for (uint8_t i = 0; i < auiRecentN; i++) if (auiRecent[i] == m) {
    for (uint8_t j = i; j + 1 < auiRecentN; j++) auiRecent[j] = auiRecent[j + 1];
    auiRecentN--;
    break;
  }
  for (uint8_t i = auiRecentN < 8 ? auiRecentN : 7; i > 0; i--) auiRecent[i] = auiRecent[i - 1];
  auiRecent[0] = m;
  if (auiRecentN < 8) auiRecentN++;
}

// ---------------------------------------------------------------------------
// params list for the current effect
// ---------------------------------------------------------------------------
static void auiBuildParams() {
  auiParamN = 0;
  char tmp[24];
  const uint8_t m = auiSeg().mode;
  for (uint8_t slot = 0; slot < 8; slot++) {
    if (slot < 2 || auiModeSlider(m, slot, tmp, sizeof(tmp)))
      auiParamSlot[auiParamN++] = (uint8_t)(AUI_P_SPEED + slot);
  }
}

// ---------------------------------------------------------------------------
// network page
// ---------------------------------------------------------------------------
// Six short lines, label-first so they align in a 5x8 font at 25 characters.
// WiFi.SSID()/psk() are core Arduino calls and survive WLED refactors; apSSID,
// apPass and cmDNS are WLED globals and are behind #defines in case they move.
#define AUI_NET_LINES 6

static void auiNetRow(int16_t i, char *out, uint8_t n) {
  out[0] = 0;
  switch (i) {
    case 0:
      if (WLED_CONNECTED) snprintf(out, n, "SSID %s", WiFi.SSID().c_str());
      else                snprintf(out, n, "SSID %s", apActive ? "(AP mode)" : "(offline)");
      break;
    case 1:
#if AUI_SHOW_KEYS
      if (WLED_CONNECTED) { snprintf(out, n, "KEY  %s", WiFi.psk().c_str()); break; }
#endif
      snprintf(out, n, "KEY  %s", "-");
      break;
    case 2:
      if (WLED_CONNECTED)   snprintf(out, n, "IP   %s", WiFi.localIP().toString().c_str());
      else if (apActive)    snprintf(out, n, "IP   %s", WiFi.softAPIP().toString().c_str());
      else                  snprintf(out, n, "IP   %s", "none");
      break;
    case 3:
#if AUI_HAVE_MDNS
      if (cmDNS[0]) { snprintf(out, n, "HOST %s.local", cmDNS); break; }
#endif
      snprintf(out, n, "HOST %s", "-");
      break;
    case 4:
#if AUI_HAVE_APCFG
      snprintf(out, n, "AP   %s%s", apSSID, apActive ? " *" : "");
#else
      snprintf(out, n, "AP   %s", WiFi.softAPSSID().c_str());
#endif
      break;
    case 5:
#if AUI_HAVE_APCFG && AUI_SHOW_KEYS
      snprintf(out, n, "APK  %s", apPass);
#else
      snprintf(out, n, "APK  %s", "-");
#endif
      break;
    default: break;
  }
  out[n - 1] = 0;
}

// ---------------------------------------------------------------------------
// row text
// ---------------------------------------------------------------------------
// Order matters and matches the RT_* enum one for one. VU Meter sits at 1, next
// to Now Playing, because the two of them are the screens you PARK on and the
// other eight are screens you pass through.
static const char *AUI_ROOT[RT_COUNT] = {
  "Now Playing","VU Meter","Effects","Palette","Params",
  "Brightness","Colour","Presets","Segment","System"};
// AUI_FILT lives up in the effect-list section, next to the prefix table it
// has to stay in step with.
static const char *AUI_COL[3]  = {"Hue","Saturation","CCT"};
#define AUI_SYS_COUNT 7
static const char *AUI_SYS[AUI_SYS_COUNT] = {
  "Network","Calibrate gyro","Level now","Clear level trim",
  "Lock knob","Save settings","Reboot"};

static void auiRowText(int16_t i, char *out, uint8_t n) {
  const uint8_t sc = auiStack[auiDepth - 1].id;
  switch (sc) {
    case SC_ROOT:     auiStr(out, AUI_ROOT[i % RT_COUNT], n); break;
    case SC_FXFILTER: auiStr(out, AUI_FILT[i % AUI_F_COUNT], n); break;
    case SC_FXLIST:   auiModeName(auiFxIdx[i], out, n); break;
    case SC_PAL:      auiPaletteName(auiPalId(i), out, n); break;
    case SC_PARAMS: {
      char nm[20]; auiParamName(auiParamSlot[i], nm, sizeof(nm));
      const int32_t v = auiGet(auiParamSlot[i]);
      const uint8_t id = auiParamSlot[i];
      if (id >= AUI_P_O1 && id <= AUI_P_O3) snprintf(out, n, "%-12s %s", nm, v ? "on" : "off");
      else                                  snprintf(out, n, "%-12s %3d", nm, (int)v);
      break;
    }
    case SC_COLOUR:   auiStr(out, AUI_COL[i % 3], n); break;
    case SC_PRESETS: {
#if AUI_PRESET_NAMES
      String nm;
      if (getPresetName((byte)(i + 1), nm)) { snprintf(out, n, "%2d %s", (int)i + 1, nm.c_str()); break; }
#endif
      snprintf(out, n, "Preset %d", (int)i + 1);
      break;
    }
    case SC_SEG:      snprintf(out, n, "Segment %d", (int)i); break;
    case SC_SYSTEM:   auiStr(out, AUI_SYS[i % AUI_SYS_COUNT], n); break;
    case SC_NET:      auiNetRow(i, out, n); return;      // already terminated
    default:          out[0] = 0; break;
  }
  out[n - 1] = 0;
}

// ---------------------------------------------------------------------------
// view refresh
// ---------------------------------------------------------------------------
static void auiRefresh() {
  AceUiView &v = aceUi().view;
  const uint8_t sc = auiStack[auiDepth - 1].id;
  v.rowFn = auiRowText;
  v.live = -1;

  // The yellow band shows the running effect on every screen, full stop. It is
  // the one thing you always want to know and the one thing the body of a list
  // screen can never spare a row for.
  auiModeName(auiSeg().mode, v.fxName, AUI_TITLE_LEN);

  if (auiToast[0]) {
    v.kind = AUI_V_TOAST;
    auiStr(v.title, auiToast, AUI_TITLE_LEN);
    v.crumb[0] = 0;
    aceUiTouch();
    return;
  }

  switch (sc) {
    case SC_ROOT:
      v.kind = AUI_V_LIST; v.count = RT_COUNT;
      auiStr(v.title, "Menu", AUI_TITLE_LEN); v.crumb[0] = 0;
      break;

    case SC_NOWPLAY:
      v.kind = AUI_V_NOWPLAY;
      auiStr(v.title, "Now Playing", AUI_TITLE_LEN);
      auiPaletteName(auiSeg().palette, v.crumb, AUI_TITLE_LEN);
      v.valNum = bri; v.valMin = 0; v.valMax = 255;
      auiStr(v.valLabel, "Brightness", AUI_ROW_LEN);
      break;

    // The meter itself is entirely the screen's business. All the model says
    // is "the body is a spectrum now" - it carries no rows, no count and no
    // value, because a view model that had to be refreshed thirty times a
    // second to animate would defeat the point of having a serial number.
    // Leaving `crumb` empty puts the live brightness percentage in the right
    // of the header, which is what the knob does on this screen.
    case SC_VU:
      v.kind = AUI_V_VU;
      v.count = 0;
      auiStr(v.title, "VU", AUI_TITLE_LEN);
      v.crumb[0] = 0;
      v.valNum = bri; v.valMin = 0; v.valMax = 255;
      auiStr(v.valLabel, "Brightness", AUI_ROW_LEN);
      break;

    case SC_FXFILTER:
      v.kind = AUI_V_LIST; v.count = AUI_F_COUNT;
      auiStr(v.title, "Show", AUI_TITLE_LEN);
      v.crumb[0] = 0;
      v.live = auiFilter;
      break;

    case SC_FXLIST: {
      v.kind = AUI_V_LIST; v.count = auiFxCount;
      auiStr(v.title, AUI_FILT[(uint8_t)auiFilter % AUI_F_COUNT], AUI_TITLE_LEN);
      const uint8_t cur = auiSeg().mode;
      for (int16_t i = 0; i < auiFxCount; i++) if (auiFxIdx[i] == cur) { v.live = i; break; }
      snprintf(v.crumb, AUI_TITLE_LEN, "%d/%d", (int)auiStack[auiDepth - 1].cursor + 1, (int)auiFxCount);
      break;
    }

    case SC_PAL:
      v.kind = AUI_V_LIST; v.count = auiPalCount();
      auiStr(v.title, "Palette", AUI_TITLE_LEN);
      v.live = auiPalIndex(auiSeg().palette);
      snprintf(v.crumb, AUI_TITLE_LEN, "%d/%d", (int)auiStack[auiDepth - 1].cursor + 1, (int)v.count);
      break;

    case SC_PARAMS:
      v.kind = AUI_V_LIST; v.count = auiParamN;
      auiStr(v.title, "Params", AUI_TITLE_LEN); v.crumb[0] = 0;
      break;

    case SC_COLOUR:
      v.kind = AUI_V_LIST; v.count = 3;
      auiStr(v.title, "Colour", AUI_TITLE_LEN); v.crumb[0] = 0;
      break;

    case SC_PRESETS:
      v.kind = AUI_V_LIST; v.count = 25;
      auiStr(v.title, "Presets", AUI_TITLE_LEN);
      auiStr(v.crumb, "hold=save", AUI_TITLE_LEN);
      break;

    case SC_SEG:
      v.kind = AUI_V_LIST; v.count = strip.getSegmentsNum();
      auiStr(v.title, "Segment", AUI_TITLE_LEN); v.crumb[0] = 0;
      v.live = auiSegSel;
      break;

    case SC_SYSTEM:
      v.kind = AUI_V_LIST; v.count = AUI_SYS_COUNT;
      auiStr(v.title, "System", AUI_TITLE_LEN); v.crumb[0] = 0;
      break;

    case SC_NET:
      v.kind = AUI_V_INFO; v.count = AUI_NET_LINES;
      auiStr(v.title, "Network", AUI_TITLE_LEN); v.crumb[0] = 0;
      break;

    case SC_EDIT: {
      v.kind = AUI_V_VALUE;
      auiParamName(auiEditParam, v.valLabel, AUI_ROW_LEN);
      auiStr(v.title, v.valLabel, AUI_TITLE_LEN);
      v.crumb[0] = 0;
      auiRange(auiEditParam, v.valMin, v.valMax);
      v.valNum = auiGet(auiEditParam);
      if (auiEditParam >= AUI_P_O1 && auiEditParam <= AUI_P_O3)
        auiStr(v.valText, v.valNum ? "on" : "off", AUI_ROW_LEN);
      else v.valText[0] = 0;
      break;
    }

    case SC_CONFIRM:
      v.kind = AUI_V_CONFIRM;
      auiStr(v.title, auiConfirmKind == AUI_CF_REBOOT ? "Reboot now?"
                                                      : "Overwrite preset?",
             AUI_TITLE_LEN);
      auiStr(v.crumb, "click=yes  hold=no", AUI_TITLE_LEN);
      break;
  }
  v.cursor = auiStack[auiDepth - 1].cursor;
  aceUiTouch();
}

static void auiToastSet(const char *msg, uint16_t ms) {
  auiStr(auiToast, msg, AUI_TITLE_LEN);
  aceUi().view.hudUntil = millis() + ms;
  auiRefresh();
}

// ---------------------------------------------------------------------------
// navigation
// ---------------------------------------------------------------------------
static void auiPush(uint8_t id) {
  if (auiDepth >= 7) return;
  auiStack[auiDepth].id = id;
  auiStack[auiDepth].cursor = 0;
  auiDepth++;
  aceUi().view.holdPct = 0;
  auiRefresh();
}
static void auiPop() {
  if (auiDepth > 1) auiDepth--;               // stack[0] is the main menu
  aceUi().view.holdPct = 0;
  auiRefresh();
}
// Home is the MAIN MENU now, not a status page. The root cursor is left where
// it was so a hold-to-home from six levels deep puts you back on the entry you
// came in through, which is almost always the one you want next.
static void auiHome() {
  auiDepth = 1;
  auiToast[0] = 0;
  auiConfirmKind = AUI_CF_NONE;
  auiConfirmAct = 0;
  aceUi().view.holdPct = 0;
  auiRefresh();
}

static void auiCommitFx(uint8_t m) {
  auiSet(AUI_P_FX, m);
  auiPushRecent(m);
  auiBuildParams();
}

// Deferred on purpose. doReboot is picked up at the top of the next
// WLED::loop(), so setting it here would blank the panel in the same frame
// that armed it and the confirmation would never be seen. 700 ms is one
// unmistakable beat of "yes, that worked" and costs nothing.
static void auiRebootArm(const char *why) {
  auiRebootAt = millis() + AUI_REBOOT_DELAY_MS;
  auiToastSet(why, 5000);
}

static void auiTogglePower() {
  if (bri == 0) bri = briLast ? briLast : 128;
  else { briLast = bri; bri = 0; }
  auiApplied();
  auiToastSet(bri ? "on" : "off", 700);
}

static void auiDoSystem(int16_t i) {
  switch (i) {
    case 0: auiPush(SC_NET); break;
    case 1:
      if (aceImuAction) { aceImuAction(1); auiToastSet("hold still...", 2500); }
      else auiToastSet("no IMU driver");
      break;
    case 2:
      if (aceImuAction) { aceImuAction(2); auiToastSet("levelled"); }
      else auiToastSet("no IMU driver");
      break;
    case 3:
      if (aceImuAction) { aceImuAction(3); auiToastSet("trim cleared"); }
      else auiToastSet("no IMU driver");
      break;
    case 4: aceUi().lockOn = true; auiToastSet("locked - hold 2s to free", 1500); break;
    case 5: auiSaveConfig(); auiToastSet("saved"); break;
    // Through the confirm screen, not straight to the flag. A reboot from a
    // menu row is one detent away from Save settings, and "I nudged the knob
    // and the cube restarted mid-set" is not a bug report anyone should file.
    // The two-button chord skips this because the chord IS the confirmation.
    case 6: auiConfirmKind = AUI_CF_REBOOT; auiConfirmAct = 0; auiPush(SC_CONFIRM); break;
    default: break;
  }
}

// ---------------------------------------------------------------------------
// events
// ---------------------------------------------------------------------------
static void auiTurn(uint8_t enc, int16_t d, bool shift) {
  AceUiBus &b = aceUi();
  const uint8_t role = enc < ACE_UI_MAX_ENC ? b.encRole[enc] : (uint8_t)AUI_ROLE_NAV;

  if (role == AUI_ROLE_MASTER) {
    auiSet(AUI_P_BRI, (int32_t)bri + d * (shift ? 10 : 1));
    b.view.hudUntil = millis() + 1200; auiRefresh(); return;
  }
  if (role == AUI_ROLE_BOUND) {
    const uint8_t p = b.encBind[enc];
    auiSet(p, auiGet(p) + d * (shift ? 10 : 1));
    auiEditParam = p; b.view.hudUntil = millis() + 1200; auiRefresh(); return;
  }

  const uint8_t sc = auiStack[auiDepth - 1].id;

  // Turning on the two "park" pages is brightness. They are the only places
  // where the knob does something without a click, and they are the pages you
  // leave it on - so the one control you actually want at arm's length is the
  // one that is already under your hand.
  if (sc == SC_NOWPLAY || sc == SC_VU) {
    if (role == AUI_ROLE_BROWSE) return;
    auiSet(AUI_P_BRI, (int32_t)bri + d * (shift ? 10 : 1));
    auiRefresh();
    return;
  }

  if (sc == SC_EDIT) {
    if (role == AUI_ROLE_BROWSE) return;
    int32_t step = d;
    if (shift) step = d * 10;
    if (auiEditParam >= AUI_P_O1 && auiEditParam <= AUI_P_O3) step = d > 0 ? 1 : -1;
    auiSet(auiEditParam, auiGet(auiEditParam) + step);
    auiRefresh();
    return;
  }

  if (sc == SC_NET) return;                    // static page, nothing to move

  // list: move the cursor. Shift jumps by first letter in the effect list,
  // which crosses ~30 Ace effects in two flicks, and by ten everywhere else.
  AceUiView &v = b.view;
  int16_t cur = auiStack[auiDepth - 1].cursor;
  const int16_t n = v.count;
  if (n <= 0) return;

  if (shift && sc == SC_FXLIST) {
    const char a = auiFxKey(auiFxIdx[cur]);
    int16_t i = cur;
    for (int16_t k = 0; k < n; k++) {
      i += (d > 0 ? 1 : -1);
      if (i < 0) i = n - 1; else if (i >= n) i = 0;
      if (auiFxKey(auiFxIdx[i]) != a) break;
    }
    cur = i;
  } else {
    cur += shift ? d * 10 : d;
    while (cur < 0) cur += n;
    while (cur >= n) cur -= n;
  }
  auiStack[auiDepth - 1].cursor = cur;
  auiRefresh();
}

static void auiClick(uint8_t enc) {
  AceUiBus &b = aceUi();
  const uint8_t role = enc < ACE_UI_MAX_ENC ? b.encRole[enc] : (uint8_t)AUI_ROLE_NAV;
  if (role == AUI_ROLE_MASTER) { auiTogglePower(); return; }

  if (auiToast[0]) { auiToast[0] = 0; auiRefresh(); return; }

  const uint8_t sc  = auiStack[auiDepth - 1].id;
  const int16_t cur = auiStack[auiDepth - 1].cursor;

  switch (sc) {
    case SC_ROOT:
      switch (cur) {
        case RT_NOWPLAY: auiPush(SC_NOWPLAY); break;
        case RT_VU:      auiPush(SC_VU); break;
        case RT_FX:      auiPush(SC_FXFILTER); auiStack[auiDepth-1].cursor = auiFilter; auiRefresh(); break;
        case RT_PAL:     auiPush(SC_PAL); auiStack[auiDepth-1].cursor = auiPalIndex(auiSeg().palette); auiRefresh(); break;
        case RT_PARAMS:  auiBuildParams(); auiPush(SC_PARAMS); break;
        case RT_BRI:     auiEditParam = AUI_P_BRI; auiPush(SC_EDIT); break;
        case RT_COLOUR:  auiPush(SC_COLOUR); break;
        case RT_PRESETS: auiPush(SC_PRESETS); break;
        case RT_SEG:     auiPush(SC_SEG); break;
        case RT_SYSTEM:  auiPush(SC_SYSTEM); break;
      }
      break;

    // Drilling in from the status page lands on the running effect's own
    // parameters, which is the whole point of the entry.
    case SC_NOWPLAY: auiBuildParams(); auiPush(SC_PARAMS); break;

    // A click LEAVES the meter, same as the network page - there is nothing on
    // this screen to select, and a gesture that did nothing would be worse
    // than one that does the obvious thing. Deliberately NOT a style cycle:
    // style is a setting, and burying it in a click here would mean the only
    // way to discover it is to press a button on a screen that looks like it
    // has no buttons.
    case SC_VU: auiPop(); break;

    // An empty family used to fall back to showing EVERY effect, which meant a
    // list headed "Ace Gyro" full of Blink and Rainbow. Say what happened
    // instead and stay put: the answer is always either "those files are not
    // in this build" or "the name prefix moved", and both are worth knowing.
    case SC_FXFILTER: {
      const int8_t was = auiFilter;
      auiFilter = (int8_t)cur;
      if (auiBuildFx() == 0) {
        char msg[AUI_TITLE_LEN];
        snprintf(msg, sizeof(msg), "no %s effects", AUI_FILT[cur % AUI_F_COUNT]);
        auiFilter = was;
        auiBuildFx();
        auiToastSet(msg, 1600);
        break;
      }
      auiPush(SC_FXLIST);
      for (int16_t i = 0; i < auiFxCount; i++)
        if (auiFxIdx[i] == auiSeg().mode) { auiStack[auiDepth-1].cursor = i; break; }
      auiRefresh();
      break;
    }

    // First click applies. Click the entry that is ALREADY applied - the one
    // with the dot - and you drop into its params instead. Same gesture the
    // status page uses, so "click again to configure what is running" holds
    // everywhere once something is selected.
    case SC_FXLIST:
      if (!auiFxCount) break;
      if (auiFxIdx[cur] == auiSeg().mode) { auiBuildParams(); auiPush(SC_PARAMS); }
      else { auiCommitFx(auiFxIdx[cur]); auiRefresh(); }
      break;

    case SC_PAL: auiSet(AUI_P_PAL, cur); auiRefresh(); break;

    case SC_PARAMS: {
      if (!auiParamN) break;
      const uint8_t p = auiParamSlot[cur];
      if (p >= AUI_P_O1 && p <= AUI_P_O3) { auiSet(p, auiGet(p) ? 0 : 1); auiRefresh(); }
      else { auiEditParam = p; auiPush(SC_EDIT); }
      break;
    }

    case SC_COLOUR:
      auiEditParam = (cur == 0) ? AUI_P_HUE : (cur == 1 ? AUI_P_SAT : AUI_P_CCT);
      auiPush(SC_EDIT);
      break;

    case SC_PRESETS:
      applyPreset((byte)(cur + 1), CALL_MODE_BUTTON);
      auiToastSet("preset loaded", 900);
      break;

    case SC_SEG: auiSegSel = (uint8_t)cur; auiBuildParams(); auiRefresh(); break;
    case SC_SYSTEM: auiDoSystem(cur); break;
    case SC_NET: auiPop(); break;               // any click leaves the page

    case SC_EDIT: auiPop(); break;              // click on a value = done

    case SC_CONFIRM: {
      const uint8_t kind = auiConfirmKind;
      auiConfirmKind = AUI_CF_NONE;
      if (kind == AUI_CF_REBOOT) { auiConfirmAct = 0; auiPop(); auiRebootArm("rebooting"); break; }
#if AUI_PRESET_SAVE
      if (auiConfirmAct) { savePreset((byte)auiConfirmAct, nullptr); auiConfirmAct = 0; }
#endif
      auiPop();
      auiToastSet("saved", 900);
      break;
    }
  }
}

// Is this encoder a dedicated back button? Two things downstream depend on
// the answer, and neither can be read off the event itself.
static bool auiIsBackBtn(uint8_t enc) {
  return enc < ACE_UI_MAX_ENC && aceUi().encSw[enc] == AUI_SW_BACK;
}

// The tick on the hold bar marks where BACK becomes HOME. A back button has
// no such point - press means back, hold means home, and there is nothing in
// between - so it gets a plain bar. Drawing the tick anyway would advertise a
// rung that does not exist on that knob.
static uint8_t auiHoldMark(uint8_t enc) {
  return auiIsBackBtn(enc) ? 0 : aceUi().backPct;
}

// 500 ms hold on a full-grammar knob, or any click on a back button.
static void auiBack(uint8_t enc) {
  if (auiToast[0]) { auiToast[0] = 0; auiRefresh(); return; }

  // On a preset row, a medium hold arms the save. Destructive actions never
  // fire on a click, and now they never fire on a plain back either - the
  // confirm screen is the only place a preset gets written.
  //
  // A BACK BUTTON IS EXEMPT, and that exemption is the whole reason the menu
  // needs to know which knob sent this. On a back button the same event comes
  // from a TAP, so without the check a light press on the presets screen would
  // arm an overwrite - turning the one gesture that is supposed to be
  // deliberate into the easiest thing on the panel to do by accident.
  if (!auiIsBackBtn(enc) && auiStack[auiDepth - 1].id == SC_PRESETS) {
    auiConfirmKind = AUI_CF_PRESET;
    auiConfirmAct = (uint8_t)(auiStack[auiDepth - 1].cursor + 1);
    auiPush(SC_CONFIRM);
    return;
  }
  // Backing out of a confirm disarms it. Leaving it armed means the NEXT
  // confirm - whatever it is for - inherits this one's action.
  if (auiStack[auiDepth - 1].id == SC_CONFIRM) { auiConfirmKind = AUI_CF_NONE; auiConfirmAct = 0; }
  if (auiDepth > 1) auiPop();
  else { aceUi().view.holdPct = 0; auiRefresh(); }
}

static void auiHomeEvt(uint8_t enc) {
  (void)enc;
  // Already sitting on the main menu with nothing to leave? Then the gesture
  // is free, so give it to power - which is what a long hold means on every
  // other lamp in the house.
  if (auiDepth == 1 && !auiToast[0]) { auiTogglePower(); return; }
  auiHome();
}

// ---------------------------------------------------------------------------
// service
// ---------------------------------------------------------------------------
const char *aceUiStatusLine() {
  static char buf[28];
  if (WLED_CONNECTED) { snprintf(buf, sizeof(buf), "%s", WiFi.localIP().toString().c_str()); return buf; }
  if (apActive)       { snprintf(buf, sizeof(buf), "AP %s", WiFi.softAPIP().toString().c_str()); return buf; }
  snprintf(buf, sizeof(buf), "no network");
  return buf;
}

void aceUiMenuInit() {
  if (auiStarted) return;
  auiStarted = true;
  auiDepth = 1;
  auiStack[0].id = SC_ROOT;
  auiStack[0].cursor = 0;
  auiSegSel = strip.getMainSegmentId();
  auiBuildFx();
  auiBuildParams();

  // Land on the configured screen rather than always on the menu. The root
  // cursor is set to match, so the first hold-to-home puts you back on the
  // entry you booted into instead of the top of the list. Both usermods have
  // had readFromConfig() run long before either one's setup() calls us, so
  // startScreen is real by the time we read it.
  switch (aceUi().startScreen) {
    case AUI_START_NOWPLAY:
      auiStack[0].cursor = RT_NOWPLAY; auiPush(SC_NOWPLAY); break;
    case AUI_START_VU:
      auiStack[0].cursor = RT_VU;      auiPush(SC_VU);      break;
    default:
      break;
  }
  auiRefresh();
}

void aceUiMenuService() {
  if (!auiStarted) aceUiMenuInit();
  AceUiBus &b = aceUi();
  const uint32_t now = millis();

  // --- events. Always drained, never throttled: this is the latency path. ---
  AceUiEvent e;
  while (aceUiPop(e)) {
    // First touch on a sleeping panel wakes it and does nothing else. Brushing
    // the knob on the way past should not renumber your effect.
    //
    // The chord is the exception, and has to be: a dark panel is exactly when
    // you reach for a reboot, and a recovery gesture you must first wake the
    // screen to use is a recovery gesture that is missing when it matters.
    if (b.asleep) {
      if (e.kind == AUI_EV_CHORD || e.kind == AUI_EV_CHORD_PROG) {
        b.asleep = false;                  // fall through and handle it
      } else {
        if (e.kind == AUI_EV_TURN || e.kind == AUI_EV_CLICK || e.kind == AUI_EV_BACK ||
            e.kind == AUI_EV_HOME) { b.asleep = false; aceUiTouch(); }
        continue;
      }
    }

    // Lock swallows everything except the gestures that undo it. HOME is in
    // that list on purpose: lockMs defaults to 0, so if the ONLY way out were
    // the extra-long hold, locking from System -> Lock knob on a default build
    // would strand you until the next reboot. A 2 s hold always unlocks.
    // The chord passes the lock too. The lock already yields to a 2 s
    // single-button hold, so swallowing a 3 s TWO-button one would mean the
    // child lock blocks the harder gesture and not the easier one - which is
    // backwards, and would strand a locked panel with nothing but the mains
    // switch on the day it needs a reboot.
    if (b.lockOn) {
      if (e.kind == AUI_EV_LOCK || e.kind == AUI_EV_HOME) {
        b.lockOn = false; auiToastSet("unlocked", 900);
      } else if (e.kind == AUI_EV_CHORD) {
        auiChordArmed = false; b.view.holdPct = 0; auiRebootArm("rebooting");
      } else if (e.kind == AUI_EV_CHORD_PROG) {
        b.view.holdPct = (uint8_t)e.delta; b.view.holdMark = 0; aceUiTouch();
      } else if (e.kind == AUI_EV_HOLD_PROG) {
        b.view.holdPct = (uint8_t)e.delta; b.view.holdMark = auiHoldMark(e.enc); aceUiTouch();
      } else if (e.kind == AUI_EV_HOLD_ABORT) {
        b.view.holdPct = 0; aceUiTouch();
      }
      continue;
    }

    switch (e.kind) {
      case AUI_EV_TURN:       auiTurn(e.enc, e.delta, e.shift); break;
      case AUI_EV_CLICK:      auiClick(e.enc); break;
      case AUI_EV_BACK:       auiBack(e.enc); break;
      case AUI_EV_HOME:       auiHomeEvt(e.enc); break;
      case AUI_EV_LOCK:       b.lockOn = true; auiToastSet("locked", 1200); break;
      case AUI_EV_HOLD_PROG:  b.view.holdPct = (uint8_t)e.delta;
                              b.view.holdMark = auiHoldMark(e.enc); aceUiTouch(); break;

      // holdMark is 0 for a chord: the single-button bar carries a tick where
      // Back becomes Home, and the chord has no such midpoint - it is one
      // threshold, and a tick would imply a second meaning that does not exist.
      case AUI_EV_CHORD_PROG:
        if (!auiChordArmed) { auiChordArmed = true; auiToastSet("keep holding = reboot", 8000); }
        b.view.holdPct = (uint8_t)e.delta; b.view.holdMark = 0; aceUiTouch();
        break;

      case AUI_EV_CHORD:
        auiChordArmed = false;
        b.view.holdPct = 0;
        auiRebootArm("rebooting");
        break;

      // Also the chord's cancel path - the encoder sends this when either
      // button comes up early. Clearing the prompt is the whole feedback, so
      // it has to happen here and not wait for the toast to time out.
      case AUI_EV_HOLD_ABORT:
        b.view.holdPct = 0;
        if (auiChordArmed) { auiChordArmed = false; auiToast[0] = 0; auiRefresh(); }
        else aceUiTouch();
        break;

      default: break;
    }
  }

  // --- housekeeping. Nothing here is worth doing a thousand times a second. -
  if (now - auiHouseMs < 100) return;
  auiHouseMs = now;

  // Armed reboot. Checked before the toast expiry below so the word on the
  // panel is still the one that armed it. 100 ms of granularity on a 700 ms
  // delay is nothing anyone can perceive.
  if (auiRebootAt && (int32_t)(now - auiRebootAt) >= 0) {
    auiRebootAt = 0;
#if AUI_HAVE_DOREBOOT
    doReboot = true;
#else
    ESP.restart();
#endif
    return;
  }

  if (auiToast[0] && now > b.view.hudUntil) { auiToast[0] = 0; auiRefresh(); }

  // idle -> Home, and OFF by default. Not while a toast is up, and not while
  // the knob is being held, or the bar would vanish mid-gesture. Read from the
  // bus every pass rather than cached, so changing it on the settings page
  // takes effect on the next housekeeping tick with no reboot.
  const uint32_t idleHomeMs = (uint32_t)b.idleHomeSec * 1000u;
  if (idleHomeMs && auiDepth > 1 && !b.view.holdPct && !auiToast[0] &&
      (now - b.lastInputMs) > idleHomeMs) auiHome();

  // The mode can change from the app, a preset or a playlist while a menu is
  // open; keep the live marker, the params list and the yellow band honest.
  static uint8_t  seenMode = 255;
  static uint8_t  seenPal  = 255;
  static uint8_t  seenBri  = 255;
  const uint8_t m = auiSeg().mode;
  const uint8_t p = auiSeg().palette;
  if (m != seenMode) { seenMode = m; auiBuildParams(); auiRefresh(); }
  else if (p != seenPal || bri != seenBri) { seenPal = p; seenBri = bri; auiRefresh(); }
  seenPal = p; seenBri = bri;
}
