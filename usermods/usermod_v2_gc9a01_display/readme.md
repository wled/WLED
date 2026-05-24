# GC9A01 Round Display Usermod

Displays WLED state on a **1.28" round IPS LCD** (240×240, GC9A01 driver, 4-wire SPI).
Tested with the Waveshare 1.28" Round LCD Module.

## Watch-face layout

```
         ╭──────────────────╮
      ╱  │  ░░░░░░░░░░░░░░  │  ╲   ← brightness arc (yellow, 0–360°)
    ╱    │   [Effect Name]  │    ╲
   │     │  ╭────────────╮  │     │
   │     │  │            │  │     │  ← colour disc (primary segment colour)
   │     │  │    75 %    │  │     │
   │     │  ╰────────────╯  │     │
    ╲    │   192.168.1.10   │    ╱
      ╲  │                  │  ╱
         ╰──────────────────╯
```

| Element | Content |
|---|---|
| Outer arc | Brightness 0-100 % (yellow on dark grey ring) |
| Centre disc | Primary colour of the main segment |
| Disc text | Brightness percentage (contrast-adaptive colour) |
| Upper text | Current effect name |
| Lower text | IP address / AP mode address / "No WiFi" |

## Wiring

| Display | ESP32 (default) |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| DIN (MOSI) | GPIO 23 |
| CLK | GPIO 18 |
| CS | GPIO 15 |
| DC | GPIO 2 |
| RST | GPIO 4 |
| BL | GPIO 32 |

## Building

Copy `platformio_override.sample.ini` to `platformio_override.ini` and adjust pins for your board. Then build the `esp32dev_gc9a01` environment:

```bash
pio run -e esp32dev_gc9a01
```

All eight pin values **must** be set as build flags — TFT_eSPI does not support runtime pin selection.

## Configuration (WLED Settings → Usermods)

| Option | Default | Description |
|---|---|---|
| `enabled` | true | Enable/disable the display |
| `screenTimeoutSec` | 300 | Seconds of inactivity before display blanks (0 = never) |
| `flip` | false | Rotate display 180° |

The four pin fields shown in the UI are read-only — they reflect the compile-time values.

## Public API (for pairing with other usermods)

```cpp
GC9A01DisplayUsermod *disp =
    (GC9A01DisplayUsermod *)UsermodManager::lookup(USERMOD_ID_GC9A01_DISPLAY);

if (disp) {
    disp->wakeDisplay();                        // wake from sleep; returns true if was off
    disp->showOverlay("Brightness", "75%", 1500); // 2-line overlay for 1.5 s
    disp->forceRedraw();                        // immediate full refresh
}
```
