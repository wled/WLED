# Red Alert (Pikud HaOref) usermod

WLED usermod that polls the Israeli Home Front Command (Pikud HaOref / Oref) alerts API and switches presets based on alert state for your area.

## Features

- Polls `https://www.oref.org.il/WarningMessages/alert/alerts.json` (or a custom HTTP/HTTPS URL).
- Area-based matching: configure a target area name (e.g. "תל אביב - מרכז") or "Match all areas".
- **Alert states** (each with optional preset):
  - **Pre-alert** (category 10 with title “בדקות הקרובות צפויות להתקבל התרעות באזורך”) → pre-alert preset
  - **End / clear** (category 10 with title “האירוע הסתיים”) → end preset
  - **Alert** (all other positive categories, e.g. 13, 14) → alert preset
- Idle fallback: after a configurable time in the same state, optionally apply an idle preset.
- HTTPS supported on ESP32 when built with a platform that provides `WiFiClientSecure` (e.g. stock `espressif32`).

## Requirements

- **ESP32** build with TLS support for the official Oref HTTPS endpoint.  
For WLED’s default Tasmota-based ESP32 platform, HTTPS is not available to usermods; use a build that uses the stock **espressif32** platform (e.g. via `platformio_override.ini`) if you need HTTPS.
- WiFi connectivity.

## Installation

1. Add `redalert` to `custom_usermods` in your PlatformIO environment, for example in `platformio.ini` or `platformio_override.ini`:
  ```ini
   [env:esp32dev]
   custom_usermods = redalert
  ```
2. If you need **HTTPS** (official Oref API), override the ESP32 platform to stock espressif32 and use a partition layout that fits the larger firmware (see main WLED docs or this usermod’s discussion for an example `platformio_override.ini`).
3. Build and upload.

## Configuration (WLED UI)

- **Core**: Enabled, API URL, Poll interval (ms), Verbose logs.
- **Area**: Match all areas (checkbox), Target area name.
- **States**: Per-state toggles and preset IDs for Alert, Pre-alert, End.
- **Idle**: Enable idle fallback, Idle timeout (sec), Idle preset.

No category toggles are exposed; all categories are enabled. Semantics:

- **Category 10** is used for both pre-alert and end; the API `title` field differentiates:
  - `"האירוע הסתיים"` → end/clear
  - `"בדקות הקרובות צפויות להתקבל התרעות באזורך"` → pre-alert
- **Category 0 or missing** → ignored (no state change).
- **Any other category** (e.g. 13, 14) → alert.

## API notes

- The official endpoint is HTTPS-only and may require requests from Israeli IPs or with specific headers (`Referer`, `X-Requested-With`). The usermod sends a simple `User-Agent`.
- Response format: single JSON object with `id`, `cat`, `title`, `data` (array of city names), `desc`. The usermod strips leading non-JSON bytes (e.g. BOM) before parsing.

## Live log over WebSocket //TODO!!!

WLED exposes a single WebSocket at `**/ws`** for state and live LED updates. A usermod can reuse it to stream log lines wirelessly without core changes.

**Usermod side**

- Declare `extern AsyncWebSocket ws;` (from `wled.h` / the build).
- Whenever you want to send a log line, call e.g. `ws.textAll("{\"log\":\"RedAlert: ...\"}");` so every connected client receives the same text frame.
- Use a stable JSON shape (e.g. `{"log":"message"}` or `{"source":"redalert","msg":"..."}`) so clients can parse and display only log lines.

**Client side**

- Connect to `ws://<device-ip>/ws` (or `wss://` if you add TLS in front).
- Listen for `onmessage`; treat frames that parse as your log format as live log lines and append them to a console/view (browser, Node script, or custom dashboard). Ignore other frames (e.g. WLED state) or filter by a `source` field.

**Caveats**

- All WebSocket clients receive the same stream; WLED does not separate “log” from “state” traffic. Avoid sending huge or high-frequency log bursts to not interfere with the main UI.
- The official WLED UI has no “Logs” tab; you need your own page or tool to connect to `/ws` and show the log lines.
- No persistence: only clients connected at the time receive each message.

## Files

- `redalert.cpp` — usermod implementation.
- `redalert_text_utils.h` — area name normalization and Unicode escape decoding.
- `library.json` — usermod metadata.

