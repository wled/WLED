# Session Handoff — WLED Demo v3 — 2026-05-21 (afternoon)

## What shipped this session

### Playwright test suite — now 7/7 passing
Infrastructure was already on disk from the previous session but all 7 tests were failing. Root causes fixed:

1. **Route order bug** — Playwright routes are LIFO (last registered = first matched). The catch-all `**/json/**` was registered last and intercepted every API call including `/json/effects`, returning `{success: true}` instead of the effects array. Fixed by registering the catch-all first and specific routes last in `mockDeviceApi()`.
2. **addInitScript cleared localStorage on reload** — `localStorage.removeItem('wled_user_fx_presets')` in `addInitScript` ran on every navigation including `page.reload()`, wiping the preset that the reload-persistence test had just saved. Removed that line (each test gets a fresh browser context so it's unnecessary).
3. **Reload test missing Effects tab click** — After `page.reload()` the app returns to the first tab (Colours); the test was waiting for `.fx-preset-rm` on the hidden Effects tab content. Fixed by clicking the Effects tab after reload.
4. **Type mismatch in `populateEffects()`** — `id` from `Object.entries(json)` is a **string** (`"1"`), but `effectId` stored in localStorage is a **number** (`1`). The strict `===` comparison always returned false, so the `−` button never restored after a page reload. Fixed at `index.js:970` with `parseInt(id, 10)`. **This is a production bug** — favourites added by a user would show as `+` again after every page reload.

Run tests: `cd .ben/tests && npx playwright test`

### UI changes
- **Bottom nav Settings** — Cog icon (`&#xe0a2;`) replaced with text `Settings`. CSS `.bot-settings` override (font-size 22px, icon padding, text-transform:none) removed so the button inherits the same styling as Colours/Effects buttons (18px, uppercase, same padding).
- **Top nav Home icon** — Changed from `&#8962;` (⌂, Unicode fallback rendered by the system font) to `&#xe88a;` (Material Icons home — filled house silhouette from WIcons font). **Verify on device** — if it renders blank, it means `&#xe88a;` is not in the WIcons subset; report back and we will try a different code or emoji.
- **Bottom nav border** — 10px solid black border above the bottom nav bar added to `.bot` rule.
- **Toast notification colour** — Success toasts changed from dark grey (`var(--c-5)`) background with white text to light yellow (`#ffe066`) background with dark text (`#333`). Error toasts explicitly restored to white text (`color: var(--c-f)`) so they remain readable on the red background.

### Scroll / pull-to-refresh fix (two-phase)
- **Phase 1** (previous attempt) — Removed `-webkit-overflow-scrolling: touch` from `.tabcontent`. This was the right call (deprecated property that bypasses `overscroll-behavior` on iOS) but did not fix Android Chrome pull-to-refresh.
- **Phase 2** (this session) — Separated `html` and `body` in the `position: fixed` rule. Previously both `html` AND `body` were `position: fixed`, which removed `html` from the document scroll root, preventing `overscroll-behavior: none` from propagating to the viewport. Chrome ignores `overscroll-behavior` on fixed elements for viewport PTR control. Fix: `html` now has `height: 100%; overflow: hidden; overscroll-behavior: none;` (no `position: fixed`). `body` keeps `position: fixed`. Visual appearance is unchanged.

### Presets cleanup
- `wled00/data/presets.json` — Stripped presets 1–5 (Aurora, Blink, Bouncing Balls, Tetris, Strobe). Now contains only preset 101 (`z_cycle_preset`, `P2=0`). Will be flashed to device on next cmd 3.
- `.ben/tests/fixtures/presets.json` — Same change (only preset 101). Test 4 P2 expectation updated from 6 → 1.

## Key files changed this session

| File | What changed |
|---|---|
| `wled00/data/index.js` | Line 970: `parseInt(id, 10)` fix in `populateEffects()` |
| `wled00/data/index.css` | `html`/`body` split; toast colour; `.bot` border; `.bot-settings` rule removed; `-webkit-overflow-scrolling: touch` removed |
| `wled00/data/index.htm` | Home icon `&#8962;` → `&#xe88a;`; Settings button icon → text |
| `wled00/data/presets.json` | Presets 1–5 removed, only preset 101 remains |
| `.ben/tests/specs/fx-preset.spec.js` | Route LIFO fix; addInitScript cleanup; reload tab click; P2 expectation 6 → 1 |
| `.ben/tests/fixtures/presets.json` | Presets 1–5 removed |

## Deploy commands needed
- **Cmd 1 + Cmd 3** — JS, CSS, HTM, and presets.json all changed. Firmware unchanged.
  ```
  npm run build
  & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e lolin_s2_mini --target uploadfs
  ```

## Verify after deploy
1. Effects tab: every non-Solid effect has a green `+` button
2. Tap `+` on an effect → yellow toast "Effect added to favorites!" → button turns red `−`
3. Reload page → `−` button still shows (localStorage + `parseInt` fix)
4. Tap `−` → yellow toast "Effect removed from favorites!" → button returns to `+`
5. Bottom nav shows: COLOURS | EFFECTS | SETTINGS (all text, no cog)
6. Top nav Home button shows a house icon (not ⌂) — **flag if blank**
7. Black line visible above the bottom nav bar
8. Scroll down the Effects list, then scroll back up — no page reload triggered

## Open / deferred items (carried forward)
- [ ] REQ 3.16 — Remove demo mode alert banner from UI
- [ ] REQ 3.17 — PIN-protected "Factory Settings" pill at bottom of settings page
- [ ] REQ 3.18 — Remove PIN lock from Config/Settings cog
- [ ] REQ 3.19 — Top nav: Power, Home, brightness slider only; bottom nav: Colours, Effects, Settings
- [ ] REQ 3.20 — Brightness slider at 70% width with 🔅 icon on left
- [ ] REQ 3.22 — Reorganise settings vs factory settings page content
- [ ] REQ 1.7 — Change firmware version string to customer-facing value before shipping
- [ ] REQ 2.5 — On WiFi connect, send ping to WordPress REST endpoint with MAC, version, commit hash, geolocation
- [ ] REQ 1.5.1 — Evaluate and add candidate presets: Puzzle, Razzle, Pride 2015, NoisePal
- [ ] Home icon — confirm `&#xe88a;` renders correctly on device; if blank, try `&#x1F3E0;` (emoji) or a different WIcons codepoint

## Context for next session
- Build target: always `lolin_s2_mini`
- cmd 1 = `npm run build` | cmd 2 = pio upload firmware | cmd 3 = pio uploadfs
- Playwright tests live in `.ben/tests/` — run with `npx playwright test` from that directory
- `localStorage['wled_user_fx_presets']` persists in browser; clear it in DevTools to reset the +/− state for testing from scratch
