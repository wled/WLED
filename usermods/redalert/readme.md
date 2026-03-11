# Red Alert (Pikud HaOref) usermod

WLED usermod that polls the Israeli Home Front Command (Pikud HaOref / Oref) alerts API and switches presets based on alert state for your area.

## Features

- Polls `https://www.oref.org.il/WarningMessages/alert/alerts.json` (or a custom HTTP/HTTPS URL).
- Area-based matching: configure a target area name (e.g. "תל אביב - מרכז") or "Match all areas".
- **Alert states** (each with optional preset):
  - **Pre-alert** (category 14) → pre-alert preset
  - **Alert** (all other positive categories) → alert preset
  - **End / clear** (categories 10, 13 — "leave shelter, stay nearby") → end preset
  - **OK** (no matching alert) → optional OK preset
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
- **States**: Per-state toggles and preset IDs for Alert, Pre-alert, End, OK.
- **Idle**: Enable idle fallback, Idle timeout (sec), Idle preset.

No category toggles are exposed; all categories are enabled. Semantics:

- **Category 14** → pre-alert.
- **Categories 10, 13** → end/clear (“leave shelter, stay nearby”).
- **Category 0 or missing** → ignored (no state change).
- **Any other category** → alert.

## API notes

- The official endpoint is HTTPS-only and may require requests from Israeli IPs or with specific headers (`Referer`, `X-Requested-With`). The usermod sends a simple `User-Agent`.
- Response format: single JSON object with `id`, `cat`, `title`, `data` (array of city names), `desc`. The usermod strips leading non-JSON bytes (e.g. BOM) before parsing.

## Files

- `redalert.cpp` — usermod implementation.
- `redalert_text_utils.h` — area name normalization and Unicode escape decoding.
- `library.json` — usermod metadata.
