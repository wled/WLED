# ESP-NOW JSON Handler Usermod

This usermod handles fragmented JSON messages received via ESP-NOW, allowing you to control WLED state from ESP-NOW enabled devices.

## Features

- Receives fragmented JSON messages over ESP-NOW
- Reassembles fragments into complete JSON payloads
- Deduplication to prevent processing duplicate messages when  broadcasting on multiple channels

## Fragment Protocol

Messages are fragmented with a 3-byte header:

| Byte | Description |
|------|-------------|
| 0 | Message ID (unique identifier for reassembly) |
| 1 | Fragment Index |
| 2 | Total Fragments |
| 3+ | JSON data payload |

## Usage

1. Enable ESP-NOW in WLED settings
2. Add the sender's MAC address to the linked remotes list
3. Send fragmented JSON payloads following the protocol above

## Compilation

Add the following to your `platformio_override.ini`:

```ini
[env:yourenv]
extends = env:esp32dev
custom_usermods = espnow_json_handler
```

## Notes

- ESP-NOW must be enabled (`WLED_DISABLE_ESPNOW` must NOT be defined)
- The sender MAC address must be in the linked remotes list
- Maximum ESP-NOW payload per fragment is 250 bytes (247 bytes of JSON data) for V1 versions and 1470 bytes (1467 bytes of JSON data) for V2 versions
