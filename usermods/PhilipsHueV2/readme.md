# Philips Hue V2 API Usermod

Syncs WLED with a Philips Hue light via the **CLIP V2 API** (HTTPS).

**ESP32 only** — requires `WiFiClientSecure` or `NetworkClientSecure`.

## Build

Add to `platformio_override.ini`:

```ini
[env:my_build]
extends = env:esp32dev
build_flags = ${env:esp32dev.build_flags} -D USERMOD_HUE_V2
```

The default Tasmota-based platform strips SSL. You may need the official
Espressif platform or a third-party SSL library for HTTPS to work. When SSL
is unavailable the usermod compiles but shows "SSL not available".

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
