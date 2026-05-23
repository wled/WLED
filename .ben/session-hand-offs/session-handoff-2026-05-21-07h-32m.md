# Session Handoff — WLED Demo v3 / Effects Tab + Preset Feature — 2026-05-21

## Decisions locked & what shipped

- **Effects tab icons stripped** — pill buttons now show plain text only; all palette/type icons removed from `populateEffects()`.
- **+ / × buttons per effect** — each non-Solid effect pill has an add (green) or remove (red) icon button on the right; clicking `+` adds the effect as a cycling preset, `×` removes it. Implemented in `populateEffects()` via `extraHtml`; `event.stopPropagation()` prevents the effect from also being applied.
- **localStorage-backed state** — user-added presets stored in `localStorage['wled_user_fx_presets']` as `[{effectId, presetId, effectName}]`. Preset IDs start at 6; fixed presets 1–5 are never touched.
- **Direct file upload for preset saves** — replaced WLED's `psave` JSON API (which causes presets.json corruption via two sequential immediate writes) with `_uploadPresetsJson()` that reads `/presets.json`, updates it in memory, and uploads via `POST /upload`. This is safe because no server-side PIN is configured so `correctPIN` is always `true`.
- **Cycle playlist (preset 101) updated atomically** — both the new effect preset AND the updated `P2` value in preset 101 are written in the same single `/upload` call. Preset 101 keeps the `win: "P1=1&P2=X&PL=~"` format.
- **Cycle restored after add/remove** — `{"ps": 101}` re-applies the cycle preset after upload so the demo cycling resumes immediately.

## Key files for next session

- `wled00/data/index.js` — All add/remove/upload logic at bottom of file (lines ~3568–3641, marked with `// AI: below section...`). Functions: `_uploadPresetsJson`, `addEffectAsPreset`, `removeEffectPreset`, `getFxPresets`, `saveFxPresets`, `getFxPresetByEffect`.
- `wled00/data/index.css` — `#fxlist .lstIcontent` has `padding-right:40px`; `.fx-preset-btn`, `.fx-preset-add`, `.fx-preset-rm` classes style the buttons.
- `wled00/data/presets.json` — Source presets file (uploaded to LittleFS via cmd 3). Contains presets 1–5 and preset 101. User-added presets are written to the device at runtime via `/upload`.
- `.ben/requirements.md` — Full requirements log; REQ 3.23 covers this feature.

## Running state

- **Running**: Unknown — device state not observed this session.
- **Broken / degraded**: presets.json on device may be corrupted from the previous approach (two sequential `psave` writes). The new code no longer uses `psave`, so re-uploading via cmd 3 will reset to a clean state.
- **Half-done**: Nothing — the add/remove feature is complete and the corruption fix is landed.

## Verification

- Deploy: run **cmd 3** (`pio run -e lolin_s2_mini --target uploadfs`) to push the updated JS/CSS.
- Confirm add: navigate to Effects tab → tap `+` on any non-Solid effect → toast appears → go to Presets tab → new preset should appear without any error message.
- Confirm cycle resumes: after tapping `+`, the LED strip should continue cycling (not freeze on one effect).
- Confirm remove: tap `×` on an added effect → toast → Presets tab shows it gone, cycle range adjusted.
- Confirm presets.json integrity: `fetch('/presets.json')` in browser console should return valid JSON.

## Deferred + open questions

- [ ] REQ 3.16 — Remove demo mode alert banner from UI (pending).
- [ ] REQ 3.17 — Add PIN-protected "Factory Settings" pill at bottom of settings page (pending).
- [ ] REQ 3.18 — Remove PIN lock from Config/Settings cog (pending).
- [ ] REQ 3.19 — Top nav: Power, Home, brightness slider only; bottom nav: Colours, Effects, Settings cog (pending).
- [ ] REQ 3.20 — Brightness slider at 70% width with 🔅 icon on left (pending).
- [ ] REQ 3.22 — Reorganise settings vs factory settings page content (pending).
- [ ] REQ 1.7 — Change firmware version string to customer-facing value before shipping (pending).
- [ ] REQ 2.5 — On WiFi connect, send ping to WordPress REST endpoint with MAC, version, commit hash, geolocation (pending).
- [ ] REQ 1.5.1 — Evaluate and add candidate presets: Puzzle, Razzle, Pride 2015, NoisePal (pending).
- [ ] Error handling in `addEffectAsPreset` / `removeEffectPreset` — if `/upload` fails, the toast still fires and localStorage is updated out of sync with device. Consider wrapping in try/catch and showing error toast.

## Pick up from here

**Next concrete action:** Deploy the current fix with cmd 3, then test the add/remove flow end-to-end: add an effect, check Presets tab loads cleanly, check cycle resumes, remove the effect, check Presets tab again.

**Context needed:**
- Device IP depends on network; connect to "LiquidLightDesign" AP or local WiFi.
- Build target is always `lolin_s2_mini`.
- cmd 1 = `npm run build`, cmd 2 = pio upload firmware, cmd 3 = pio uploadfs (JS/CSS changes always need cmd 3).
- `localStorage['wled_user_fx_presets']` persists in the browser; clear it if testing from scratch to reset the + / × button state.
