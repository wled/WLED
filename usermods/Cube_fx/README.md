# Ace LED FX — one file per effect

This folder used to be a single 4,974-line `user_fx_cube.cpp`. It's now split into
one small `.cpp` per effect plus a small set of shared headers, so you can work on
a single effect without scrolling past the others, and adding a new effect never
means editing a file that already has other people's (or your own past) work in it.

`user_fx_custom.cpp` is untouched and unrelated — WLED's usermod build compiles
every `.cpp` in the folder into one binary, so this split changes nothing about
how the project builds.

Effects now come from three independent capability layers, each opt-in via
`#include`, each costing nothing in effects that don't ask for it:

- **geometry & timing** — `cube_fx_common.h` (always included)
- **audio** — `cube_fx_audio.h`, on top of `cube_fx_common.h`
- **motion/orientation** — `cube_fx_imu.h`, alongside `cube_fx_common.h`

Plus a self-contained on-device UI system (`ace_ui_*`) and sensor driver
(`ace_imu_mpu6050.cpp`) that effects never touch directly.

## Layout

```
cube_fx_common.h         geometry + core audio/timing helpers - always #include this
cube_fx_audio.h          second-tier audio analysis - #include instead of common.h
cube_fx_imu.h            orientation/motion bridge - #include alongside common.h

cube_fx_00_cube_axes.cpp
cube_fx_01_cube_noise.cpp
cube_fx_02_cube_ripples.cpp
...
cube_fx_34_audio_scope.cpp     diagnostic: renders every cube_fx_audio.h helper at once
cube_fx_35_dna_helix.cpp
...
cube_fx_40_gyro_horizon.cpp    Ace Gyro class - starts at slot 40, needs cube_fx_imu.h
cube_fx_41_gyro_sand.cpp
cube_fx_42_gyro_rain.cpp

ace_imu_mpu6050.cpp      sensor driver - owns the MPU6050, feeds cube_fx_imu.h
ace_ui_bus.h             shared state between the three AceUI parts below
ace_ui_encoder.cpp       rotary encoder -> events
ace_ui_menu.cpp          events -> WLED state + view model
ace_ui_screen.cpp        view model -> OLED pixels
```

The number prefix matches the original numbering in the old file's effect list
(there's no #10 — it was retired before this split and the gap was preserved
rather than renumbering everything). Native Gyro-class effects (built around
`cube_fx_imu.h` from the start rather than having tilt bolted onto a plain
effect) start their own numbering at slot 40.

Each `cube_fx_NN_name.cpp` is fully self-contained:
- its `mode_x()` function
- its `_data_FX_MODE_X` metadata string
- any sprites/tables/helpers used ONLY by that effect
- its own tiny `Usermod` subclass that calls `strip.addEffect(...)` and its own
  `REGISTER_USERMOD(...)` call

That last part is what makes new effects drop in cleanly: WLED already supports
any number of `Usermod`s registering themselves independently (this codebase
already did that between `user_fx_custom.cpp` and the old `user_fx_cube.cpp`),
so each effect file registers *itself*. There's no central "add your effect
here" list to touch.

## `cube_fx_common.h`

Holds only things genuinely shared across effects:
- `FX_RET` / `FX_DONE` (0.15 vs 16.x/17-dev signature compatibility)
- the cube net geometry: `cfx_pos`, `cfx_buildCube`, `cfx_isCube`, the
  `CFX_NET_PREP/ROW/SKIP` gap-skipping macros
- audio helpers: `cfx_getAudioData`, `cfx_bands`, `cfx_drive`, `cfx_smoothSpec`,
  `fx_lowBeat`
- frame-timing helpers: `fx_dt`, `fx_dt8`, `fx_step`, `fx_fade`
- `mq_scale` (pixel scale/dim — used by every effect from Question Block on)
- the wall/face surface toolkit (`cfx_buildBand`, `lf_fwd`, `lf_inv`,
  `cfx_buildDirLut`, `cfx_buildCells`) used by Question Block, Invaders, Snake,
  Tetris, Tron, Highway, Life, Pac-Man and Split GEQ

Two things got promoted into this header during the split even though they
weren't textually next to the other shared helpers in the old file:
- `mq_scale` used to be defined inline inside Question Block's file, just
  because Question Block happened to be the first effect that needed it.
- The wall/face toolkit used to sit in a gap between Question Block and Simon
  for the same reason.

If you ever find yourself copy-pasting a helper into a second effect file,
that's the sign it belongs in `cube_fx_common.h` instead — move it there and
delete the copies.

Anything used by exactly ONE effect (game boards, sprite tables, per-effect
`#define`s like `CFX_SRC`, `MZ_RACERS`, `RB_MOVES`, etc.) stays local to that
effect's own file, same as before.

## `cube_fx_audio.h` — opt-in second-tier audio analysis

Sits on top of `cube_fx_common.h` (which already carries `fx_lowBeat`,
`cfx_tempo` and `cfx_drop`) and answers the musical questions those primitives
don't. `#include "cube_fx_audio.h"` instead of `cube_fx_common.h` — it pulls
common.h in for you, so nothing else changes, and effects that don't include
it pay nothing at link time.

- `cfx_hiHit(um)` / `cfx_midHit(um)` — hat/cymbal and snare/clap transients,
  each with kick-leak rejection so a hard kick doesn't get mistaken for the
  thing it bled into
- `cfx_offbeat(tempo)` — fires once, on the frame the beat clock crosses
  halfway ("the and")
- `cfx_bar(tempo)` — position in a 4-bar cycle, with a downbeat guess. Fails
  **confidently**: treat `beatInBar` as a stable four-cycle counter, not a
  claim about which beat the drummer calls one (see the header for why)
- `cfx_silence(um)` — tracks dead air between tracks and fires `wake` the
  frame the music comes back; distinct from `cfx_drop().build`, which is the
  track deliberately pulling the bass out
- `cfx_roll(um, tempo)` — drum-roll/fill detection: level, density, and
  acceleration (accelerating = the fill is walking toward the next downbeat)
- `cfx_brightness(um)` — spectral centroid (tone, not LED level) for
  colour-by-timbre rather than colour-by-volume

All of these are `inline` (one shared instance across every `.cpp` that
includes the header) except `cfx_onset()`, which is `static inline` on
purpose because its whole state lives in a caller-supplied `CfxOnset&`. Every
detector returns a 0–255 strength, fires at most once per hit, and every
threshold is a `-D`-overridable `#define` — see the header for the full house
style before adding a new analyzer here.

`cube_fx_34_audio_scope.cpp` is the diagnostic effect: it renders all seven
helpers above simultaneously so a new analyzer (or a build's tuning) can be
eyeballed directly on the panel.

## `cube_fx_imu.h` — opt-in orientation and motion

Companion to `cube_fx_common.h` for effects that want to react to being
tilted, spun, shaken, or set down on a different face. Effects never touch
the sensor: they read one `CfxImuState` struct per frame, already in cube
coordinates (+X east, +Y north, +Z top, `-127..127`) that match `cfx_pos()`.
No sensor present, or the driver compiled out — `cfx_imu().valid` is `false`
and every effect renders exactly as it does without one; nothing needs an
`#ifdef`.

What effects actually use:
- `cfx_imuUp(st, px,py,pz)` — height of a pixel along world-up; the workhorse
  for liquid fills, horizon bands, and sky-direction lighting
- `st.upFace` / `st.faceLock` — which face is skyward, debounced
- `st.tilt`, `st.spin`, `st.heading` — 0=level tilt scale, signed spin rate,
  free-running heading
- `st.shake` / `st.jolt` — one-shot proportional shake (shaped like
  `fx_lowBeat()` so it drops straight into a beat slot) and its ring-out tail
- `st.stillMs` — ms since anything moved, for ambient/idle behaviour
- `st.basis[9]` — cube→world rotation; `cfx_imuWorld()` applies it so a 3-D
  field (noise, gyroid, lattice) stays locked to the room instead of tumbling
  with the cube

**Mounting is not handled in this header.** It used to be a `-D
CFX_IMU_AXIS_X/Y/Z` build flag here; that's gone, and the file `#error`s if it
sees those flags set. Axis mapping now lives entirely in the driver's live
settings page (Settings → Usermods → AceIMU), because the driver checks the
determinant and refuses to compose an unglueable reflection, which a build-flag
remap layer couldn't guard against. `cube_fx_imu.h` only ever passes the
driver's vectors straight through — no second remap on top.

`ace_imu_mpu6050.cpp` is the driver that owns the sensor: it reads the raw
registers, runs its own complementary filter, and feeds `cube_fx_imu.h` via
`cfx_imuFeedUnits(...)` (compiled with `CFX_IMU_FEED_ONLY` so it only pulls in
the input side of the header, not the whole effect toolkit).

`cube_fx_40_gyro_horizon.cpp` is the effect to run first on new hardware — a
horizon that should stay dead level while the cube turns; if it leans or lags
on one axis, that's the driver's axis-map dropdowns, not the effect.

## AceUI — on-device menu, encoder and OLED (`ace_ui_*`)

Three files, one shared struct in `ace_ui_bus.h`, one direction of data flow
each:

```
ace_ui_encoder.cpp   hardware in   -> events on a ring buffer
ace_ui_menu.cpp      events in     -> WLED state out, view model filled
ace_ui_screen.cpp    view model in -> pixels out
```

Remove the screen and the encoder still drives WLED state; remove the encoder
and the screen is a read-only status display. `ace_ui_menu.cpp` is a plain
translation unit with no hardware dependency and no `Usermod` subclass at all.
As with `cube_fx_audio.h`/`cube_fx_imu.h`, everything in `ace_ui_bus.h` that
owns state is plain `inline`, never `static inline` — a `static inline`
accessor here would silently hand the encoder and the screen two different
buses instead of one shared one.

Notable behaviour:
- The OLED is two-color (yellow rows 0–15, blue rows 16–63); the running
  effect name is pinned to the yellow band on every screen so the body below
  is free to be a list, editor, value, or meter
- A dedicated full-panel VU meter view (`AUI_V_VU`) is selectable as the
  screen the panel boots into via `startScreen`, separate from the one-tile VU
  strip on Now Playing
- `idleHomeSec` (default `0`) means the UI never wanders back to the main menu
  on its own — it stays on whatever screen you left it on
- Input is a single press: a 500 ms hold is Back, a 2 s hold is Home, both
  decided at release so there's no added latency; double-click is gone

## Adding a new effect

1. Copy `cube_fx_00_cube_axes.cpp` (the smallest one) to
   `cube_fx_NN_your_effect.cpp` — pick any unused number (40+ if it's a
   native Gyro-class effect built around `cube_fx_imu.h` from the start).
2. Write `mode_your_effect()` and `_data_FX_MODE_YOUR_EFFECT`.
3. `#include` what you need beyond geometry:
   - `cube_fx_audio.h` instead of `cube_fx_common.h` for beat/hit/roll/bar
     detection beyond the basics
   - `cube_fx_imu.h` alongside `cube_fx_common.h` for tilt/spin/shake
   - both, if the effect reacts to music AND motion — see
     `cube_fx_40_gyro_horizon.cpp` for the pattern of dividing time/amplitude
     (music) from frame (gravity) so the two responses don't fight
4. Update the bottom `Usermod`/`REGISTER_USERMOD` block to match your names.
5. That's it — no other file changes. Nothing else even needs to be recompiled
   except your new file and whatever links the binary.

If your effect needs a helper that already exists in another effect's file
(not one of the three shared headers), don't `#include` that other `.cpp` —
either duplicate the small helper into your file, or, if it's clearly
general-purpose, promote it into the right shared header (geometry/timing →
`cube_fx_common.h`, audio → `cube_fx_audio.h`, motion → `cube_fx_imu.h`) so
every file references one copy.

A gyro effect that wants both a plain and a motion-reactive variant can
register both from one file using a `_core(bool gyro)` pattern rather than
duplicating the effect — see the existing `cube_fx_4x_gyro_*.cpp` files.
