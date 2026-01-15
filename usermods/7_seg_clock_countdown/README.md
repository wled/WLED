# WLED Usermod: 7‑Segment Countdown Overlay

## Status & resources
- 3D-print files (enclosures/mounts) and PCB design files will be published soon:
  - 3D-print: MakerWorld
  - PCB: GitHub, in this usermod’s folder
- Active development happens in the WLED fork by DereIBims:
  - [github.com/DereIBims/WLED](https://github.com/DereIBims/WLED)

A usermod that renders a six‑digit, two‑separator seven‑segment display as an overlay mask on top of WLED’s normal effects/colors. Lit “segments” preserve the underlying pixel color; all other pixels are forced to black. This lets you show a clock or a countdown without losing the active effect.

## What it shows
- Clock: HH:MM:SS with configurable separator behavior (on/off/blink)
- Countdown to a target date/time
  - > 999 days: `'o'ddd d`
  - > 99 days: `' 'ddd d`
  - ≤ 99 days: `dd:hh:mm`
  - ≤ 99 hours: `hh:mm:ss`
  - ≤ 99 minutes: `MM·SS:hh` (upper dot between minutes and seconds, plus hundredths 00–99)
- If the target time is in the past, it counts up:
  - < 60 s after target: `xxSSxx`
  - < 60 min after target: `xxMM:SS`
  - ≥ 60 min after target: falls back to the current clock

Hundredths update smoothly within the current second and are clamped to 00–99.

## Panel geometry (defaults)
- 6 digits, each with 7 segments
- `LEDS_PER_SEG = 5` → 35 LEDs per digit
- 2 separators between digits 2/3 and 4/5 with `SEP_LEDS = 10` each
- Total panel LEDs: `6×35 + 2×10 = 230`

A physical‑to‑logical segment map (`PHYS_TO_LOG`) allows arbitrary wiring order while you draw by logical segments A..G.

> Note: The current implementation hard‑codes digit and separator base indices for the defaults above. If you change `LEDS_PER_SEG` or `SEP_LEDS`, update `digitBase()` and `sep1Base()/sep2Base()` accordingly in the header so indices remain correct.

## How it works (overlay mask)
- The usermod builds a per‑LED mask (vector of 0/1).
- For each frame, digits/separators set mask entries to 1.
- During `applyMaskToStrip()`, pixels with mask 0 are cleared to black; mask 1 keeps the existing effect color.

## Build and enable
This usermod is located at `usermods/7_seg_clock_countdown` and can be enabled by adding the folder name to your PlatformIO environment’s `custom_usermods` setting.

- Example: `custom_usermods = 7_seg_clock_countdown`
- To include all usermods, you can also use an environment that sets `custom_usermods = *`.

> Ensure the device time is correct (NTP/timezone) so clock/countdown display accurately.

### Required core tweak for smooth hundredths
To guarantee hundredths update every frame without lag, the static effect must not idle the render loop. In `wled00/FX.cpp`, adjust `mode_static()` to always return `FRAMETIME`. This keeps the render loop continuous so hundredths (00–99) refresh in real time.

## Runtime configuration
All settings live under the `7seg` object in state/config JSON.

- `enabled` (bool): Master on/off (default: true)
- `showClock` (bool): Show clock view (default: true)
- `showCountdown` (bool): Show countdown view (default: false)
- `alternatingTime` (uint, seconds): When both views are enabled, alternates every N seconds (default: 10)
- `SeperatorOn` (bool): Force separators on in clock mode
- `SeperatorOff` (bool): Force separators off in clock mode
  - Both true or both false → blinking separators once per second
- Target date/time (local time):
  - `targetYear` (int, 1970–2099)
  - `targetMonth` (1–12)
  - `targetDay` (1–31)
  - `targetHour` (0–23)
  - `targetMinute` (0–59)

On any target change, the module validates/clamps values and recomputes `targetUnix`.

## JSON API examples
Send JSON to WLED (HTTP or WebSocket). Minimal examples below use HTTP POST to `/json/state`.

Enable overlay and show clock only:
```json
{
  "7seg": {
    "enabled": true,
    "showClock": true,
    "showCountdown": false,
    "SeperatorOn": true,
    "SeperatorOff": false
  }
}
```

Set a countdown target and enable alternating views every 15 seconds:
```json
{
  "7seg": {
    "enabled": true,
    "targetYear": 2026,
    "targetMonth": 1,
    "targetDay": 1,
    "targetHour": 0,
    "targetMinute": 0,
    "showClock": true,
    "showCountdown": true,
    "alternatingTime": 15
  }
}
```

Turn the overlay off:
```json
{ "7seg": { "enabled": false } }
```

## Custom MQTT API
Publish to your device topic with the `7seg` subpath. Topic format:
- {deviceTopic}/7seg/{key}

Accepted keys and payloads:
- Boolean keys (payload: "true"/"1" or "false"/"0"):
  - enabled
  - showClock
  - showCountdown
  - SeperatorOn
  - SeperatorOff
- Numeric keys (payload: integer):
  - targetYear (1970–2099)
  - targetMonth (1–12)
  - targetDay (1–31)
  - targetHour (0–23)
  - targetMinute (0–59)
  - alternatingTime (seconds)

Examples (mosquitto_pub):
- Enable overlay:
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/enabled" -m "true"
- Clock only, separators always on:
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/showClock" -m "true"
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/showCountdown" -m "false"
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/SeperatorOn" -m "true"
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/SeperatorOff" -m "false"
- Set countdown target to 2026‑01‑01 00:00:
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/targetYear" -m "2026"
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/targetMonth" -m "1"
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/targetDay" -m "1"
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/targetHour" -m "0"
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/targetMinute" -m "0"
- Alternate clock/countdown every 15s:
  - mosquitto_pub -h <broker> -t "wled/mystrip/7seg/alternatingTime" -m "15"

Notes:
- Device topic is the WLED “Device Topic” you configured (e.g. wled/mystrip).
- Payloads are simple strings; booleans accept "true"/"1" and "false"/"0".

## UI integration
- The usermod adds a compact block to the “Info” screen:
  - A small toggle button to enable/disable the overlay
  - Status line showing active/disabled
  - Clock separator policy (On / Off / Blinking)
  - Mode (Clock / Countdown / Both)
  - Target date and total panel LEDs

## Time source
The usermod uses WLED’s `localTime`. Configure NTP and timezone in WLED so the clock and countdown are correct.

## Notes and limitations
- This is an overlay: it does not draw its own colors.
- For non‑default panel geometries, update the index helpers as noted above.
- Separator half‑lighting is used to place an upper dot between minutes and seconds in the ≤99‑minute view.

## Files
- `usermods/7_seg_clock_countdown/7_seg_clock_countdown.h` — constants, geometry, masks, helpers
- `usermods/7_seg_clock_countdown/7_seg_clock_countdown.cpp` — rendering, JSON/config wiring, registration

## License
This usermod follows the WLED project license (see repository `LICENSE`).
