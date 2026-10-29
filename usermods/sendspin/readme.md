# Sendspin Visualizer

This usermod presents WLED as a Sendspin visualizer and publishes Sendspin's synchronized loudness, spectrum, dominant-frequency, beat, and onset data through WLED's existing AudioReactive usermod data contract.

> **Prototype status:** the visualizer-only implementation streams successfully on the active XIAO ESP32-S3 target. The target has 8 MB flash, 8 MB PSRAM, and two 3.19 MiB OTA application slots; final image-size and extended-runtime measurements are still pending.

See [`XIAO_ESP32S3.md`](../../XIAO_ESP32S3.md) for the active Seeed Studio XIAO ESP32-S3 target, including its OTA layout, PSRAM use, pin assignment, power constraints, and native-USB recovery procedure.

[`FLASHING.md`](../../FLASHING.md) retains the no-OTA ESP32-C3 proof procedure for historical reference.

It intentionally replaces the `audioreactive` usermod. Do not select both usermods in the same build: both use `USERMOD_ID_AUDIOREACTIVE` for effect compatibility.

## Build

Initialize the pinned `sendspin-cpp` submodule, then select only this usermod in a custom ESP32 environment:

```ini
custom_usermods = sendspin
```

This repository's sample and ignored local override provide `xiao_esp32s3_sendspin`. Build it with the repository's Docker toolchain:

```sh
PIO_ENV=xiao_esp32s3_sendspin docker compose -f compose.build.yaml run --rm --build wled-build
```

The firmware artifacts are written under `.pio/build/xiao_esp32s3_sendspin/`. When PlatformIO has direct access to the board's native USB port, flash with:

```sh
pio run -e xiao_esp32s3_sendspin --target upload
```

The first installation must use the complete `firmware.factory.bin` image so the bootloader and partition table match. Later application updates may use WLED OTA while the image fits one OTA slot.

The build adapter compiles only Sendspin core, ESP networking, cryptography, time synchronization, persistence, and the visualizer role. Player, source, controller, metadata, artwork, color, FLAC, Opus, local microphone, I2S, and FFT code are excluded.

The current dependency adapter expects `sendspin-cpp`, ArduinoJson 7.4.2, noise-c commit `62c2fdca7a874ea898761147bb062ca1cde9f36d`, and esp-protocols commit `b7f3de500a4fd4b58e4248093067058959833260` (`websocket-v1.6.1`). PlatformIO strips ESP-IDF component directories from generic library packages, so the isolated build adapter shallow-fetches that exact ESP Protocols commit into generated `.pio/libdeps` storage and verifies its revision before compiling only `esp_websocket_client`. The target uses WLED's supported pioarduino ESP-IDF 5.5.4 platform because the default Tasmota framework omits mbedTLS SSL symbols required by `esp_websocket_client`. WLED's embedded ArduinoJson 6 remains isolated in a separate translation unit.

## Settings

- `enabled`: Start the integration. While enabled but disconnected, sound-reactive effects receive silence instead of simulated audio.
- `url`: Optional full outbound `ws://` or `wss://` Sendspin server URL. URLs containing credentials, query strings, or fragments are rejected. A blank URL leaves the client available for inbound connections on the default Sendspin port without initiating an outbound connection.
- `device-name`: Sendspin display name. Leave blank to use WLED's configured device name.
- `display-offset-ms`: Clamped to -2000 through 2000 ms. Positive values deliver visualization earlier; negative values deliver it later.
- `allow-unpaired`: Explicit opt-in, disabled by default. Use only on an isolated network for initial setup, then pair the device and disable it.
- `peak-source`: Select whether WLED peak-reactive effects trigger from Sendspin beat events, transient/onset events, or both. `Transient only` is the compatibility default because WLED's `samplePeak` represents an onset rather than a tempo-grid event. Existing saved selections are preserved. Changing this setting does not reconnect Sendspin.
- `spectrum-mapping`: `Raw` preserves the direct Sendspin mapping and remains the default. `WLED compatibility` enables the gated dB-window mapper below.
- `spectrum-floor-db` and `spectrum-ceiling-db`: Map the selected Sendspin dB interval to WLED's 0-255 FFT range. Both must be within -60 through 0 dB and the floor must be below the ceiling. Defaults are -60 and 0 dB.
- `spectrum-bass-trim-db`, `spectrum-mid-trim-db`, and `spectrum-treble-trim-db`: Apply -24 through +24 dB to bins 0-3, 4-11, and 12-15 respectively. All default to 0 dB.
- `spectrum-input-gate-db`: Suppresses inputs at or below this untrimmed Sendspin level before any band trim is applied. It accepts -60 through 0 dB and must be below the ceiling. The -60 dB default guarantees exact protocol silence stays zero but does not claim to reject measured background noise.

Sendspin loudness is mapped immediately to WLED's raw volume. Smoothed volume uses WLED-style slew-rate limiting with the default 80 ms attack and 1400 ms decay constants; this does not replicate the complete microphone or Audio Sync volume pipeline because Sendspin supplies only one loudness value. Raw spectrum mode directly maps 0-65535 to 0-255. Compatibility mode first evaluates the untrimmed input gate, adds the configured trim in Sendspin's linear dB representation, maps the floor/ceiling interval to 0-255, and clips the result. It adds no extra temporal smoothing. `/json/info` exposes the mapping configuration alongside raw Sendspin, mapped WLED, and most recently provided spectra. Current audio, event ages, snapshots, and rolling statistics clear when the stream is cleared or disconnected; flow counters remain cumulative for the adapter session and reset when the adapter restarts.

The `Sendspin beat stream` row reports whether the active visualizer stream advertises beat data, whether it marks downbeats, and how many beat, downbeat, and transient callbacks have been observed since that stream started. These are stream capabilities and observations, not definitive track-analysis metadata: a stream may advertise beats before its first beat arrives, and the visualizer protocol does not identify why a particular track emits none.

Invalid or missing settings, out-of-range dB values, a floor at or above the ceiling, or an input gate at or above the ceiling reject the complete update and preserve the current configuration. `/json/info` names the rejected field or parameter combination. Pre-v1 Sendspin settings are not migrated; erase the development device's flash when the schema changes.

The legacy WLED `maxVol` and `binNum` compatibility fields remain writable because peak effects expect the full AudioReactive data contract. As with stock WLED audio-sync receive mode, they do not alter externally supplied peak events; `peak-source` controls which Sendspin events feed `samplePeak`.

The signal semantics follow the Sendspin [`visualizer@v1` specification](https://github.com/Sendspin/spec/blob/main/roles/visualizer/v1.md). Compatibility behavior is based on WLED's local and audio-sync receive paths in [`audio_reactive.cpp`](../audioreactive/audio_reactive.cpp).

For an outbound Music Assistant connection, use `ws://<music-assistant-host>:8927/sendspin`. For example: `ws://music-assistant.local:8927/sendspin`. Use the host and port shown by the Music Assistant installation rather than assuming the example values.

Music Assistant registers this visualizer-only client as a non-audio visualizer, not as a directly playable output. Add it as a grouped follower of an audio-capable player, then start playback on that audio player or group leader. Starting playback directly on the WLED visualizer fails with Music Assistant's generic `command to player failed` message and sends no `stream/start` because WLED intentionally advertises no Sendspin player/audio role.

Sendspin identity, pairing PSK, and pairing records are stored as opaque blobs in the ESP32 NVS namespace `sendspin`. They are never returned by `/json/cfg`, displayed in `/json/info`, or logged by this usermod. WLED's filesystem factory reset does not currently clear this namespace.

## Retired MagWLED-1 Proof

The `magwled_sendspin` environment and its hardware-specific procedure remain for historical reference. Its 4 MB no-PSRAM ESP32-C3 required a no-OTA application layout and had insufficient internal-memory margin with the upstream inbound server enabled. It is not the active deployment target.

This integration initializes no microphone, ADC, I2S, DMA, or local FFT resources.
