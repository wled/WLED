# BARTdepart

Render upcoming Bay Area Rapid Transit (BART) departures on LED segments.
Each platform gets its own segment; colored markers sweep toward “now” as
departure times approach.

## Features

- Virtual‑segment aware rendering (reverse, mirroring, grouping honored).
- Configurable per‑platform segment mapping; disable any platform with `-1`.
- Graceful overlay behavior (no freezing the segment while drawing).
- Simple debug tooling via a blinking debug pixel index.

## Installation

Add `usermod_v2_bartdepart` to your PlatformIO environment in
`platformio_override.ini`:

```
custom_usermods = usermod_v2_bartdepart
```

Build and flash as usual:
- Build: `pio run -e <your_env>`
- Upload: `pio run -e <your_env> -t upload`

## Configuration

Open the WLED web UI → Config → Usermods → “BartDepart”. The following keys are
available.

- BartDepart.Enabled: enable/disable the module.

Source: LegacyBartSource (BART ETD API)
- UpdateSecs: fetch interval in seconds (default 60).
- ApiBase: base URL for the ETD endpoint.
- ApiKey: API key. The default demo key works for testing; consider providing
  your own key for reliability.
- ApiStation: station abbreviation (e.g., `19th`, `embr`, `mont`).

Views: PlatformView1 … PlatformView4
- SegmentId: WLED segment index to render that platform (`-1` disables).

Notes
- Virtual Segments: Rendering uses `virtualLength()` and `beginDraw()`, so
  segment reverse/mirror/group settings are respected automatically. If the
  displayed direction seems wrong, toggle the segment’s Reverse or adjust
  mapping settings in WLED.
- Multiple Segments: Map different platforms to different segments to visualize
  each platform independently. You can also map multiple PlatformViews to the
  same segment if you prefer, but overlapping markers will blend.

## Usage

1) Assign `SegmentId` for the platform views you want (set others to `-1`).
2) Set `ApiStation` to your station abbreviation; keep `UpdateSecs` at 60 to
   start.
3) Save and reboot. After a short boot safety delay, the module fetches ETDs
   and starts rendering. Dots represent upcoming trains; their color reflects
   the train line.

## Troubleshooting

- Nothing shows: Ensure `BartDepart.Enabled = true`, a valid `SegmentId` is set
  (and the segment has non‑zero length), and the station abbreviation is
  correct. Confirm the controller has Wi‑Fi and time (NTP) sync.
- Direction looks reversed: Toggle the segment’s Reverse setting in WLED.
- Flicker or odd blending: Multiple departures can overlap on adjacent bins; the
  renderer favors the brighter color on a short cadence. Increase pixel count or
  split platforms across separate segments for more resolution.
- API errors/backoff: On fetch errors the module exponentially backs off up to
  16× `UpdateSecs`. Verify connectivity and API settings.
