# Session Handoff — WLED Demo v3 — 2026-05-20

## Decisions locked & what shipped

- **Factory Settings page created** (`factory.htm`). PIN 3865, accessible from Settings page. Contains 9 buttons: Segments, LED & Hardware, Pin Info, 2D Configuration, User Interface, Time & Macros, Usermods, Security & Updates.
- **Settings page slimmed down** to: Presets, WiFi & Network, Info, Sync Interfaces, Factory Settings only (REQ 3.22).
- **Top nav restructured** — Power and Home only; Nodes and PC Mode hidden (kept in DOM for JS). Config/cog moved to bottom nav. (REQ 3.19)
- **Bottom nav** — Colours, Effects, Settings cog (navigates direct to /settings, no PIN). (REQ 3.19.1)
- **PIN removed from Settings cog** on home page — goes straight to /settings. (REQ 3.18)
- **Brightness slider** shrunk to 70% width, 🔅 icon added left side only. (REQ 3.20)
- **PIN modal removed from index.htm** — PIN logic now lives only in settings.htm (for Factory Settings).
- **LittleFS deploy rule confirmed** — all `wled00/data/` changes need cmd 3. S2 Mini always needs manual RST button press after cmd 3 (USB auto-reset fails silently — this is normal, not an error).
- **lld-website project bootstrapped** — CLAUDE.md, agents, `.ben/` folder set up. Requirements logged for standalone PHP pixel tracker (SQLite, PingTracker class, `/api/ping` endpoint, dashboard).

## Key files for next session

- `wled00/data/index.htm` — top nav (Power+Home), bottom nav (Colours/Effects/cog), brightness icon+slider, PIN modal removed
- `wled00/data/index.css` — AI block: #buttonNodes/#buttonPcm hidden, briwrap flex, slider 113px, .bot-settings style
- `wled00/data/settings.htm` — Presets, WiFi & Network, Info, Sync Interfaces, Factory Settings (PIN modal inline)
- `wled00/data/factory.htm` — new file, 9 buttons, no PIN logic (PIN gate is in settings.htm)
- `.ben/requirements.md` — canonical log, REQs up to 3.22; all session REQs at `pending` commit hash
- `.ben/implementation-notes.md` — fully filled in for all shipped REQs this session

## Running state

- **Running**: nothing — firmware deployed to device, no dev server running
- **Broken / degraded**: nothing known
- **Half-done**: nothing — all session work is complete and built

## Verification

- `npm run build` in `WLED-Main-2026-04-29/` — must complete with no errors
- After cmd 3 + RST: navigate to `/settings` → should show Presets, WiFi & Network, Info, Sync Interfaces, Factory Settings
- Tap Factory Settings → PIN keypad appears → enter 3865 → lands on `/factory.htm` with 9 buttons
- Bottom nav gear icon → goes directly to `/settings` (no PIN prompt)
- Top nav should show Power button, Home button, and brightness slider with 🔅 icon — no Nodes, Config, or PC Mode buttons visible

## Deferred + open questions

- [ ] **REQ 1.5.1** — Evaluate and add candidate presets: Puzzle, Razzle, Pride 2015, NoisePal
- [ ] **REQ 1.7** — Update firmware version string in `package.json` from `17.0.0-dev` to customer-facing string before shipping
- [ ] **REQ 2.5 / 2.5.1 / 2.5.2** — WLED-side ping on WiFi connect (C++ work in `network.cpp`) — separated from lld-website tracker work
- [ ] **REQ 3.22 pending commit hashes** — all REQs from this session still have `pending` as commit ID; update after next git commit
- [ ] **lld-website pixel tracker** — requirements logged, no code written yet; PHP standalone app, SQLite, `PingTracker` class

## Pick up from here

Next concrete action: Press RST on the S2 Mini after deploying, then verify the Settings and Factory Settings pages look correct on the device.
Context needed: deploy commands are in `CLAUDE.md` — cmd 1 = `npm run build`, cmd 2 = pio firmware upload, cmd 3 = pio uploadfs. All three needed for web changes on this device (LittleFS priority + S2 Mini RST quirk).
