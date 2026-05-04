# Session Handoff — WLED LiquidLightDesign — 2026-05-04

## Decisions locked & what shipped

- AP SSID set to "LiquidLightDesign" with no password (open network). In `const.h` — fires before LittleFS load so works without uploadfs.
- Welcome page bypassed — root `/` handler in `wled_server.cpp` always serves `index.htm` directly.
- Config page gets Segments / Presets / Info pill buttons (in `settings.htm`) — navigate via `/?tab=segments` etc.
- Top menu stripped to: Power / Home / Nodes / Config / PC Mode. Timer, Sync, Peek, Info buttons removed.
- Home button (⌂ `&#8962;`) added between Power and Nodes; routes to `getURL('/')`.
- Brightness slider moved into top menu bar (right-aligned), icon removed, width reduced.
- Colors tab: color slot buttons, hex input, Files/PixelForge/Palettes row, and palette search hidden with `display:none` (IDs kept for JS safety).
- JS TypeError fixed: removed dead `gId()` calls for `buttonNl`, `buttonSync`, `buttonSr`, `buttonI` from `updateUI()`, `toggleLiveview()`, `toggleInfo()`, and PC mode handler.
- `?tab=` query-param deep-link added in `onLoad()` — `openTab(2)` for segments, `openTab(3)` for presets, `toggleInfo()` for info.
- Hardware config in `cfg.json`: LED GPIO 40 / 93 pixels / WS2812, ABL off (`maxpwr: 0`), buttons GPIO 16 & 18 (push type), boot preset 101, transition 700ms (`dur: 7`).
- Presets 1–5 created (Aurora, Blink, Bouncing Balls, Tetris, Strobe) + preset 101 (`z_cycle_preset`, cycles P1–P5). All segments use `grp: 3`.

## Key files for next session

- [wled00/const.h](../../wled00/const.h) — AP SSID/password compile-time defaults (lines ~39–40)
- [wled00/wled_server.cpp](../../wled00/wled_server.cpp) — root `/` handler (no welcome page)
- [wled00/data/index.htm](../../wled00/data/index.htm) — top menu structure, Colors tab hidden elements
- [wled00/data/index.js](../../wled00/data/index.js) — TypeError fixes, `?tab=` deep-link, `cfg.quick = false` default
- [wled00/data/index.css](../../wled00/data/index.css) — `#briwrap` positioning rules
- [wled00/data/settings.htm](../../wled00/data/settings.htm) — pill buttons (Segments/Presets/Info)
- [wled00/data/cfg.json](../../wled00/data/cfg.json) — runtime hardware config (LittleFS)
- [wled00/data/presets.json](../../wled00/data/presets.json) — presets 1–5 + 101 (LittleFS)
- [wled00/data/wsec.json](../../wled00/data/wsec.json) — AP password blank (LittleFS)

## Running state

- **Running**: nothing — all changes are file edits, no dev server active
- **Broken / degraded**: Colors tab cleanup changes not yet built or flashed — device still has old UI
- **Half-done**: `npm run build` was interrupted by user mid-session; index.htm changes are NOT yet in firmware

## Verification

- After build+flash, open `http://<device-ip>/` — should land directly on main UI (no welcome page)
- Top menu should show exactly 5 buttons: Power / Home / Nodes / Config / PC Mode
- Brightness slider should appear inline to the right of Config button, no icon
- Colors tab: colour wheel + H/S/V/K sliders visible; slot buttons, hex, palettes all hidden
- Config page (`/settings`): Segments / Presets / Info pill buttons visible below Back button
- AP should broadcast as "LiquidLightDesign" open network
- Boot should load preset 101 (cycling presets 1–5)
- `curl http://<device-ip>/json/state` — check `"ps":101` and `"seg":[{"grp":3}]`

## Deferred + open questions

- [ ] Confirm presets cycle correctly on the physical device (preset 101 `win` playlist syntax)
- [ ] Verify `grp: 3` grouping looks correct for 93 LEDs (every 3rd pixel addressed together)
- [ ] Mobile layout: check brightness slider stays inline on small screens after CSS changes
- [ ] Consider whether hiding the palette search is sufficient or if the whole Effects tab needs cleanup too

## Pick up from here

**Next concrete action:** Run the three deploy commands below — all three are required; the latest index.htm changes (Colors tab cleanup + TypeError fixes) are not on the device yet.

```powershell
# 1 — Build web UI into firmware headers
npm run build

# 2 — Flash firmware (picks up JS changes, top menu, Colors tab)
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e lolin_s2_mini --target upload

# 3 — Flash LittleFS (picks up cfg.json, presets.json, wsec.json)
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e lolin_s2_mini --target uploadfs
```

**Context needed:**
- Board: lolin_s2_mini (NOT `lolin_s2_mini_demo` — that env doesn't exist)
- Web UI changes → Steps 1+2. LittleFS files (cfg/presets/wsec) → Step 3. Both needed this time.
- `light.tr.dur` unit is 100ms → `7` = 700ms
- Never edit `wled00/html_*.h` / `wled00/js_*.h` — auto-generated, gitignored
