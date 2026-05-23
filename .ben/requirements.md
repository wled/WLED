# WLED Requirements

**Goal:** Modify the WLED baseline code to build a custom firmware for the Lolin S2 Mini, tailored for demo/display use.

---

## 1. Config

**1.1** `ea5cb351` [2026-04-29 21:53] — Set up dev branch with .gitignore to exclude sensitive config files from version control.
**1.2** `935c169b` [2026-05-03 17:22] — Change the AP (access point) name to a custom value.
  **1.2.1** `935c169b` [2026-05-03 17:22] — AP SSID must be set to "LiquidLightDesign" with no password (open network).
**1.3** `f4f1478d` [2026-05-03 19:40] — Apply default device configuration including presets and WiFi credentials via cfg.json, presets.json, and wsec.json.
  **1.3.1** `f4f1478d` [2026-05-03 19:40] — LED strip configured on GPIO 40, 93 pixels, WS2812 type, with ABL disabled (maxpwr: 0).
  **1.3.2** `f4f1478d` [2026-05-03 19:40] — Hardware buttons configured on GPIO 16 and GPIO 18 (push type).
  **1.3.3** `f4f1478d` [2026-05-03 19:40] — Boot preset set to 101 and transition duration set to 700ms.
  **1.3.4** `f4f1478d` [2026-05-03 19:40] — Device name set to blank (empty string).
  **1.3.5** `f4f1478d` [2026-05-03 19:40] — AP set to always-on mode (not just when no router is available).
  **1.3.6** `f4f1478d` [2026-05-03 19:40] — UI defaults: Colour wheel enabled, RGB sliders disabled, Quick colour selectors disabled, HEX colour input disabled, dark mode disabled.
**1.4** `209ae8c9` [2026-05-04 21:36] — Assign the z_preset action to GPIO18 button 2 press.
**1.5** `f4f1478d` [2026-05-03 19:40] — Pre-configure presets 1–5 on the device: Preset 1 Aurora, Preset 2 Blink, Preset 3 Bouncing Balls, Preset 4 Tetris, Preset 5 Strobe. All segments use group 3 (grp:3).
  **1.5.1** `pending` [2026-05-04] — Candidate additional presets to evaluate and add: Puzzle, Razzle, Pride 2015, NoisePal.
**1.6** `f4f1478d` [2026-05-03 19:40] — Pre-configure preset 101 (z_cycle_preset) as a playlist that cycles through presets 1–5.
**1.7** `pending` [2026-05-13] — Change the firmware version in package.json from 17.0.0-dev to a customer-facing version string before shipping.

---

## 2. Behaviour

**2.1** `935c169b` [2026-05-03 17:22] — Skip the WLED welcome/splash page on load and send the user directly to the Controls page.
**2.2** `216c96f0` [2026-05-04 20:19] — Disable the side-swipe gesture on mobile to prevent navigating between windows.
**2.3** `209ae8c9` [2026-05-04 21:36] — When a device disconnects from the WiFi access point, automatically revert to the z_cycle_preset.
**2.4** `993e334c` [2026-05-11 22:07] — Disable IR (infrared) remote control functionality to prevent IR commands from being registered.
**2.5** `pending` [2026-05-13] — On WiFi connection, the device must send a GET request to a WordPress REST endpoint (wp-json/wled/v1/ping) carrying the device MAC address, WLED base version, commit hash, device name, and IP-based geolocation for tracking purposes.
  **2.5.1** `pending` [2026-05-13] — The WordPress plugin must expose the REST endpoint, log incoming data to a database, and display results in a WP Admin dashboard table.
  **2.5.2** `pending` [2026-05-13] — The tracking request must include both the WLED base version and the custom build commit hash so the WordPress dashboard can identify which build is running.

---

## 3. UI

~~**3.1** `deff283f` [2026-05-03 17:29] — Add Segments, Presets, and Info navigation buttons to the Config page so removed nav items remain accessible.~~ *(superseded by REQ 3.22)*
  **3.1.1** `deff283f` [2026-05-03 17:29] — Pill buttons styled as dark gold: background #4a4a00, text #ffd700, border-radius 20px, touch-friendly sizing (3.5vmin font size).
  **3.1.2** `deff283f` [2026-05-03 17:29] — Default tab on UI load must be Effects (tab index 1), not Colors.
**3.2** `54f70d1e` [2026-05-03 17:58] — Remove icon buttons from the top navigation bar.
**3.3** `54f70d1e` [2026-05-03 17:58] — Add a home icon to the top navigation bar.
**3.4** `54f70d1e` [2026-05-03 17:58] — Align the brightness slider within the top navigation area.
**3.5** `54f70d1e` [2026-05-03 17:58] — Top navigation bar must contain exactly these buttons: Power, Home, Nodes, Config, and PC Mode.
**3.6** `778f7cc6` [2026-05-04 07:06] — Remove sliders from the main controls page.
**3.7** `778f7cc6` [2026-05-04 07:06] — Widen the colour pill buttons.
**3.8** `778f7cc6` [2026-05-04 07:06] — In the Colors tab, hide the colour slot buttons, hex input field, and the Files/PixelForge/Palettes row.
  **3.8.1** `778f7cc6` [2026-05-04 07:06] — Palette pill button width must match the effects list width.
**3.9** `216c96f0` [2026-05-04 20:19] — Replace the Colours icon on the bottom nav bar with white text reading 'Colours', keeping the same hyperlink.
**3.10** `216c96f0` [2026-05-04 20:19] — Replace the Effects icon on the bottom nav bar with white text reading 'Effects', keeping the same hyperlink.
**3.11** `216c96f0` [2026-05-04 20:19] — Apply a dark style to the bottom navigation bar.
**3.12** `216c96f0` [2026-05-04 20:19] — Remove the Effects Search pill button.
**3.13** `e118703a` [2026-05-04 21:07] — Apply a keypad/PIN lock to the Config button to restrict access.
  **3.13.1** `e118703a` [2026-05-04 21:07] — The PIN code for the Config button keypad lock is 3865.
~~**3.14** `e118703a` [2026-05-04 21:07] — Add a demo mode alert message under the top nav bar with background #eb8634 and white text reading 'Limited app functionality in demo model'.~~ *(superseded by REQ 3.16)*
**3.16** `pending` [2026-05-20] — Remove the demo mode alert banner entirely from the UI.
**3.15** `233c6081` [2026-05-12 22:13] — Display the current Git commit ID on the WLED Info page.
~~**3.5** `54f70d1e` [2026-05-03 17:58] — Top navigation bar must contain exactly these buttons: Power, Home, Nodes, Config, and PC Mode.~~ *(superseded by REQ 3.19)*
~~**3.13** `e118703a` [2026-05-04 21:07] — Apply a keypad/PIN lock to the Config button to restrict access.~~ *(superseded by REQ 3.18)*
~~**3.13.1** `e118703a` [2026-05-04 21:07] — The PIN code for the Config button keypad lock is 3865.~~ *(superseded by REQ 3.18)*
**3.17** `pending` [2026-05-20 21:00] — Add a PIN-protected "Factory Settings" pill button at the bottom of the settings page (PIN: 3865).
  **3.17.1** `pending` [2026-05-20 21:00] — Factory Settings page contains: Segments, LED & Hardware, Pin Info, 2D Configuration, User Interface (moved from settings page).
  **3.17.2** `pending` [2026-05-20] — Factory Settings page also contains: Sync Interfaces, Time & Macros, Usermods, Security & Updates (moved from settings page).
**3.18** `pending` [2026-05-20 21:00] — Remove the PIN lock from the Config/Settings cog; navigates directly to /settings with no PIN.
**3.19** `pending` [2026-05-20 21:00] — Top navigation bar contains Power, Home, and brightness slider only (Nodes, Config, PC Mode removed from top nav).
  **3.19.1** `pending` [2026-05-20 21:00] — Bottom navigation bar contains Colours, Effects, and a Settings cog (gear icon navigating directly to /settings).
**3.20** `pending` [2026-05-20 21:00] — Brightness slider shrunk to 70% of original width with a 🔅 (low brightness) icon on the left side only.
~~**3.21** `pending` [2026-05-20] — Remove all navigation pill buttons (Presets, Info) from the settings page; settings page shows functional settings and Factory Settings only.~~ *(superseded by REQ 3.22)*
~~**3.17.2** `pending` [2026-05-20] — Factory Settings page also contains: Sync Interfaces, Time & Macros, Usermods, Security & Updates (moved from settings page).~~ *(superseded by REQ 3.22)*
**3.22** `pending` [2026-05-20] — Settings page shows: Presets, WiFi & Network, Info, Sync Interfaces, and Factory Settings only. All other buttons (Segments, LED & Hardware, Pin Info, 2D Configuration, User Interface, Time & Macros, Usermods, Security & Updates) are on the Factory Settings page.
**3.23** `pending` [2026-05-20] — Effects tab pill buttons show plain text only (no palette/type icons). Each pill has a + button (green) to add the effect as a cycling preset; once added, a × button (red) replaces it to remove the preset. Toast message confirms each action. Preset IDs are assigned sequentially from 6; preset 101 (z_cycle_preset) P2 value is updated automatically to reflect the total count. User-added presets persist in localStorage (key: wled_user_fx_presets).

---

## 4. Rules

**4.1** — Make changes to the mobile view only.
**4.2** — Do not make changes to the core workings of the code.
**4.3** — Most changes are cosmetic.
**4.4** — Where possible put configurations into a centralised file for easy changes in the future.
**4.5** — LittleFS serves web UI files with priority over the embedded firmware version. Any change to wled00/data/ requires cmd 3 (uploadfs) to be deployed — without it the device silently serves the old version.
