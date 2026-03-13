# Philips Hue V2 API Usermod

Syncs WLED with a Philips Hue light via the **CLIP V2 API** (HTTPS).

**ESP32 only** — requires a TLS library (see [TLS Backend](#tls-backend) below).

## Build

Add `PhilipsHueV2` to your `custom_usermods` in `platformio_override.ini`:

```ini
[env:my_build]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods}
  PhilipsHueV2
```

### TLS Backend

The Hue V2 API requires HTTPS. The usermod auto-detects the available TLS
library at compile time:

| Backend | Library | Platform |
|---------|---------|----------|
| Native | `WiFiClientSecure` / `NetworkClientSecure` | Official `espressif32` platform |
| BearSSL | `ArduinoBearSSL` | Any platform (add as `lib_deps`) |

The default Tasmota-based platform used by WLED strips `WiFiClientSecure`.
To get TLS on Tasmota, add **ArduinoBearSSL**:

```ini
[env:my_build]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods}
  PhilipsHueV2
lib_deps = ${env:esp32dev.lib_deps}
  arduino-libraries/ArduinoBearSSL @ ^1.7.6
build_flags = ${env:esp32dev.build_flags}
  -DARDUINO_DISABLE_ECCX08
```

Or switch to the **official Espressif platform** which includes
`WiFiClientSecure` natively (note: may exceed 4MB flash):

```ini
[env:my_build]
extends = env:esp32dev
platform = espressif32
custom_usermods = ${env:esp32dev.custom_usermods}
  PhilipsHueV2
```

When no TLS library is available the usermod compiles but shows
*"SSL not available"* in the WLED info page.

## Setup

1. Enter your **Bridge IP** in Usermod Settings.
2. Press the **link button** on the bridge, enable **Attempt Auth**, save.
3. Enter the **Light Resource UUID** (query `https://<bridge>/clip/v2/resource/light`).

## Settings

| Setting | Description | Default |
|---------|-------------|---------|
| enabled | Enable/disable | false |
| bridgeIp | Bridge IP address | (empty) |
| apiKey | Application key | (empty) |
| lightId | Light UUID | (empty) |
| pollInterval | Poll interval (ms) | 2500 |
| applyOnOff | Sync on/off | true |
| applyBri | Sync brightness | true |
| applyColor | Sync color | true |
| attemptAuth | Trigger auth | false |
