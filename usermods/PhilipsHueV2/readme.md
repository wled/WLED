# Philips Hue V2 API Usermod

Syncs WLED with a Philips Hue light via the **CLIP V2 API**. Polls a single
light on the bridge and mirrors its on/off state, brightness, and colour to
WLED in real time.

**ESP32 only.**

## Features

- **mDNS auto-discovery** — leave the Bridge IP field empty and the usermod
  will automatically find the Hue bridge on your network via mDNS
  (`_hue._tcp`). The discovered IP is shown in the Info panel but not
  persisted, so it re-discovers on each reboot (handles DHCP changes).
- **Server-Sent Events (SSE)** — instead of polling, the usermod can maintain
  a persistent connection to the bridge's `/eventstream/clip/v2` endpoint and
  receive light state changes in real time. Enable with the *SSE Enabled*
  setting (on by default). Falls back to polling if SSE is unavailable.
- **Automatic authentication** — press the link button on the bridge, tick
  *Attempt Auth* in Usermod Settings, and save. The API key is obtained and
  persisted automatically.
- **Light discovery** — tick *Fetch Lights* and save. The usermod queries the
  bridge for all lights and presents them as a dropdown in settings so you can
  pick the one to monitor without manually entering a UUID.
- **HTTPS and plain HTTP** — HTTPS is used by default (port 443). If the
  bridge IP is prefixed with `http://` the usermod connects over plain HTTP
  (port 8088) instead, which is useful for debugging or when TLS is not
  available.
- **Automatic backoff** — on connection failure the poll/reconnect interval is
  progressively extended to avoid flooding the network.
- **Effect mapping** — Hue V2 light effects are mapped to the closest WLED
  equivalent. Enable with the *Apply Effect* setting (on by default).

  | Hue Effect | WLED Effect | Notes |
  |------------|-------------|-------|
  | `no_effect` | Solid | Static colour from xy/ct |
  | `candle` | Candle Multi | Flickering candle (uses "Color 1" palette) |
  | `fire` | Fire 2012 | Fire simulation (uses "Color 1" palette) |
  | `sparkle` | Sparkle | Random sparkles (uses "Color 1" palette) |
  | `prism` | Rainbow Cycle | Rotating rainbow (uses its own colours) |
  | `opal` | Breath | Slow pulsing (uses "Color 1" palette) |
  | `glisten` | Twinklefox | Twinkling glisten (uses "Color 1" palette) |
  | `sunrise` | Sunrise | Sunrise simulation (uses its own colours) |
  | *(unknown)* | Solid | Fallback for unrecognised effects |

## Build

Add `PhilipsHueV2` to your `custom_usermods` in `platformio_override.ini`:

```ini
[env:my_build]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods}
  PhilipsHueV2
```

### TLS Backend

The Hue V2 API requires HTTPS. The usermod auto-detects the best available
TLS library at compile time and logs which one it chose on boot:

| Priority | Backend | Library | Platform |
|----------|---------|---------|----------|
| 1 | Native | `NetworkClientSecure` | Official `espressif32` ≥ 6.x (Arduino Core 3.x) |
| 2 | Native | `WiFiClientSecure` | Official `espressif32` 5.x (Arduino Core 2.x) |
| 3 | BearSSL | `ArduinoBearSSL` | Any platform (add as `lib_deps`) |

#### Recommended: official Espressif platform (native TLS)

The **official `espressif32` platform** includes `WiFiClientSecure` with
hardware-accelerated mbedTLS. This is the most reliable option:

```ini
[env:my_build]
extends = env:esp32dev
platform = espressif32 @ 6.9.0
custom_usermods = ${env:esp32dev.custom_usermods}
  PhilipsHueV2
```

> **Note:** The official platform may produce larger firmware binaries than the
> Tasmota fork. Boards with only 4 MB flash may need a larger partition scheme.

#### Alternative: BearSSL on Tasmota platform

The default **Tasmota-based platform** used by most WLED builds does not
include `WiFiClientSecure`. To add TLS support, include **ArduinoBearSSL** as
a library dependency:

```ini
[env:my_build]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods}
  PhilipsHueV2
lib_deps = ${env:esp32dev.lib_deps}
  arduino-libraries/ArduinoBearSSL @ ^1.7.6
```

> **Note:** ArduinoBearSSL was designed for Arduino SAMD/Nina boards and has
> known compatibility issues with the ESP32 WiFiClient (errno 11 / EAGAIN
> errors under load). If you experience connection failures, switch to the
> official platform or use the `http://` prefix to bypass TLS.

#### No TLS available

If neither library is present the usermod compiles but HTTPS connections will
fail. You can still use the usermod by prefixing the bridge IP with `http://`
to force plain HTTP (requires the bridge to be accessible on port 8088).

## Setup

1. **Bridge IP** — leave empty for automatic mDNS discovery, or enter
   manually:
   - For HTTPS (default): enter the IP address, e.g. `192.168.1.100`
   - For plain HTTP: prefix with `http://`, e.g. `http://192.168.1.100`
2. Press the **link button** on the bridge.
3. Enable **Attempt Auth** and save. The API key will be obtained
   automatically and saved to the config.
4. Enable **Fetch Lights** and save. The light list will be fetched from the
   bridge and a dropdown will appear in settings.
5. Select the light you want to monitor from the **Light ID** dropdown and
   save.
6. The info panel will show the status, SSE connection state, the bridge IP
   (with "(mDNS)" if auto-discovered), and the monitored light name.

## Settings

| Setting | Description | Default |
|---------|-------------|---------|
| enabled | Enable/disable the usermod | false |
| bridgeIp | Bridge IP, `http://IP` for plain HTTP, or empty for mDNS | (empty) |
| apiKey | Application key (auto-filled after auth) | (empty) |
| lightId | Light UUID (use Fetch Lights to populate dropdown) | (empty) |
| pollInterval | Poll interval in milliseconds (minimum 1000) | 2500 |
| applyOnOff | Sync on/off state from Hue to WLED | true |
| applyBri | Sync brightness from Hue to WLED | true |
| applyColor | Sync colour (xy and colour temperature) from Hue to WLED | true |
| applyEffect | Sync Hue effects to closest WLED equivalent | true |
| attemptAuth | Trigger authentication (press link button first) | false |
| fetchLights | Fetch light list from bridge for dropdown selection | false |
| sseEnabled | Use SSE for real-time events instead of polling | true |

## Troubleshooting

| Symptom | Cause | Fix |
|---------|-------|-----|
| `Connection failed` (errno 11) over HTTPS | Socket exhaustion or BearSSL incompatibility | Switch to the official `espressif32` platform, or use `http://` prefix |
| `No TLS backend` | No TLS library available at compile time | Add `ArduinoBearSSL` to `lib_deps` or use the official platform |
| `Auth: Press link button first` | Link button not pressed before enabling Attempt Auth | Press the physical button on the bridge, then save again |
| `No API key` | Authentication not completed | Complete the auth flow (steps 2–3 above) |
| Light dropdown not appearing | Lights not yet fetched | Enable *Fetch Lights* and save |
| Stack overflow on boot | TLS buffers too large for the loop task stack | Ensure you are using heap-allocated TLS clients (default in current code) |
