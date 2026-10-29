# Sendspin Prototype Debugging Log

Last updated: 2026-10-18

This file tracks diagnostics, bench-state changes, and cleanup decisions made while bringing up the Sendspin prototype.

## Current Result

The ESP32-C3 measurements below record the completed proof phase. Active development has moved to the XIAO ESP32-S3 target, where Sendspin spectrum data is reaching WLED effects; final build-size and extended-runtime measurements are still pending.

- `magwled_sendspin` compiles successfully with pioarduino ESP-IDF 5.5.4.
- Firmware uses 2,158,357 bytes of the 3,080,192-byte temporary no-OTA application partition.
- Static RAM usage is 81,796 of 327,680 bytes before runtime task and heap allocations.
- WLED joins Wi-Fi and serves its UI.
- Sendspin connects to Music Assistant, identifies the server, and synchronizes its clock. The redacted protocol trace reaches `server/time received`.
- The connection failure was caused by the `noise-c` package manifest compiling only its ESPHome-specific `Noise_NNpsk0` protocol. The builder override did not recognize the library because its source directory basename is `src`; matching the full `.../noise-c/src` path enabled the protocol-name tables required for Sendspin's `Noise_KKpsk2` suite.
- Connected-idle `/json/info` showed 27,012 bytes free with an 18,420-byte largest block. This is a tight margin and still requires streaming and extended-runtime validation.
- Music Assistant recognizes the client as a visualizer. Its generic `command to player failed` response occurred because playback was targeted directly at the non-audio visualizer; it must instead be a grouped follower of an audio-capable player, with playback started on that leader. No `stream/start` is expected for a direct visualizer play command.
- A later connected capture with 190 configured LEDs showed 23,160 bytes free and a 14,324-byte largest block. WLED's low-memory watchdog threshold is 14,336 bytes (`MIN_HEAP_SIZE - 1024`), so it purged effect data, reset segments, and could eventually reset the strip/network. This explains the `Effect RAM depleted` UI warning, severe control lag, and transient Sendspin retries without an uptime reset.
- A C3-only fork experiment disabled the inbound server. Connected captures then showed 30,324-32,700 bytes free and a stable 23,540-byte largest block, and WLED responsiveness and effect updates recovered.
- Disabling Sendspin and waiting for teardown recovered free heap to 109,756 bytes. This indicates resources are released when the adapter is destroyed rather than permanently leaked across disable/re-enable.
- Music Assistant's endpoint at `ws://<music-assistant-host>:8927/sendspin` returned a valid HTTP `101 Switching Protocols` response from another LAN client.

## Retained Info-Page Diagnostics

The following bounded, non-secret rows are intentionally retained under `/json/info`:

- Client started state and NVS readiness.
- Outbound connection-attempt count and age.
- Latest whitelisted lifecycle/protocol milestone or fixed failure reason and its age.
- Current free heap and largest contiguous internal heap block.
- Connected server name, clock state, visualizer state, and last-frame age.
- Current raw Sendspin loudness and spectrum, mapped raw/smoothed WLED volume, last onset strength, and separate beat/downbeat/transient ages.
- Current display offset, last requested offset, and config-read count while investigating settings persistence.
- Latest raw, WLED-mapped, and provided spectrum snapshots; callback/accept/coalesce/invalid/drop/apply/UM-request counters; independent spectrum ages and stale-transition count; and bounded 10-second band-0 statistics.
- Active spectrum mapping mode, floor, ceiling, pre-trim input gate, and bass/mid/treble trims.
- Active-stream beat and downbeat capabilities plus observed beat, downbeat, and transient callback counts.

`sendspin_arduino_compat.h` enables the upstream redacted `SENDSPIN_ENABLE_MESSAGE_TRACE` facility. `sendspin_message_trace.cpp` compiles its implementation, and the adapter reduces every callback immediately to a fixed stage name. It never stores or displays message bodies, client/server IDs, PSKs, pairing tokens, pairing records, or arbitrary log text. Outbound trace stages are labeled `send attempted` because the callback runs immediately before the transport send.

Sendspin's log macros route to a no-output diagnostic sink that compares only compile-time format strings against a fixed failure allowlist. It does not print to ROM/UART, change ESP-IDF runtime log levels, or format arbitrary log arguments. The sole consumed argument is the library-owned nursery stage enum string used to distinguish setup timeouts.

The separate Arduino `DISPLAY` macro workaround in `sendspin_arduino_compat.h` remains required independently of diagnostics.

## Spectrum Compatibility Test

The compatibility mapper is opt-in. Raw mode remains the default and retains the previous `(value + 128) / 257` behavior. Compile-time assertions cover raw endpoints, a known raw sample, exact silence with positive trim, nonzero mapping, high-value clipping, and gate-before-trim ordering.

Use this procedure for the first hardware comparison:

1. Select Raw mode and play the same section of "Lose Yourself to Dance" at a fixed Music Assistant and WLED volume. Record `Sendspin spectrum input`, `Sendspin spectrum WLED`, and `Sendspin band 0 (10 s)` while Akemi and GEQ run.
2. Pause playback or play a known-silent source and observe the raw spectrum long enough to establish its maximum background level. Convert a raw value to dB with `-60 + 60 * raw / 65535`, round upward to a whole dB, and use that evidence as the input gate. If every raw bin is exactly zero, leave the gate at -60 dB.
3. Select WLED compatibility mode with floor -60 dB, ceiling 0 dB, bass trim +15 dB, mid trim 0 dB, treble trim 0 dB, and the measured input gate. These are diagnostic settings, not production defaults.
4. Replay the same section without changing music or volume. Confirm Akemi band 0 exceeds 128 on strong bass passages, silence maps to all zeros, and GEQ does not remain pinned at 255.
5. Save, reboot, and verify `/json/cfg` and the `Sendspin spectrum mapping` and `Sendspin spectrum trims` rows retain the same values. An invalid combination such as floor 0 dB and ceiling 0 dB must show `invalid setting rejected` and leave every prior value unchanged.

The raw 16-bit spectrum remains unchanged in both modes. Only the `Sendspin spectrum WLED` values supplied through the AudioReactive interface are mapped.

## Blurz Isolation Test

Blurz remains a separate rendering investigation; do not change the effect implementation or matrix configuration. While Blurz is active, capture `/json/state` and retain segment `on`, `bri`, bounds, `col`, `pal`, `sx`, `ix`, `o`, and `m12`. Test with `m12=0`, a reliably bright palette, a black secondary color, fade rate `sx=0`, and blur `ix=0`. If pixels appear, restore blur first and fade rate second. If the controlled configuration stays black, the next bounded diagnostic should capture the injected color immediately before `setPixelColor()` and confirm the selected segment buffer receives it.

## Removed Temporary Diagnostics

- Removed `SENDSPIN_DEBUG` from both build environments.
- Removed all `LOG_LOCAL_LEVEL=ESP_LOG_DEBUG` build overrides.
- Removed `esp_log_level_set()` runtime tag changes.
- Removed ROM-console output and arbitrary `vsnprintf()` log formatting.
- Removed the blocking TCP reachability probe and its Info-page row.

## RMT Failure

The addressable LED output repeatedly emitted:

```text
E (...) rmt: rmt_tx_wait_all_done(593): flush timeout
```

The errors occurred continuously with the normal addressable output on GPIO2 under the pioarduino framework. An unattached strip should not cause RMT completion timeouts, so this is considered a framework/NeoPixelBus integration defect rather than expected disconnected-strip behavior.

To quiet the error during Sendspin isolation, the device's persisted LED configuration was temporarily changed to:

```text
count: 1
type: 41 (analog/PWM)
pin: 21
```

The persisted configuration no longer represents the intended MagWLED-1 addressable output. Restore an addressable output on GPIO2 only after resolving the RMT issue. Do not assign GPIO5 or GPIO9 as LED data pins.

## No-OTA Proof Layout

The following are temporary for hardware feasibility testing but are not merely logging diagnostics:

- `tools/WLED_ESP32_4MB_noOTA.csv`
- `WLED_DISABLE_OTA` in the MagWLED environment
- `board_build.partitions = tools/WLED_ESP32_4MB_noOTA.csv`

The standard 4 MB OTA slot is 1,572,864 bytes. The Sendspin image does not fit it. Removing the no-OTA setup requires either reducing the image below an OTA slot or moving to hardware with more flash, then reflashing a new partition table over USB.

## Required Build Workarounds

These are not debug hooks and should not be removed with logging:

- The pioarduino platform selection: the default Tasmota framework omits mbedTLS SSL symbols required by `esp_websocket_client`.
- `lib_ignore = libsodium`: pioarduino already supplies libsodium; compiling noise-c's transitive copy causes duplicate symbols.
- The `noise-c` builder override in `build_sendspin.py`: ESPHome's package manifest disables protocol-name tables and otherwise permits only `Noise_NNpsk0`; Sendspin requires `Noise_KKpsk2`. The override matches the full builder source path because its basename is `src`, not `noise-c`, and fails the build if that dependency cannot be configured.
- The pinned, verified ESP Protocols checkout in `build_sendspin.py`: PlatformIO strips ESP-IDF component directories from generic library packages.
- ArduinoJson 6/7 translation-unit isolation.
- The Arduino `DISPLAY` macro workaround.

## Retired C3 Outbound-Only Mitigation

The C3 proof temporarily set `SendspinClientConfig::server_max_connections` to `0`. Fork commit `183fec406328db6d7c415c31d586552f0d38eb50` treated zero as disabling the inbound WebSocket server while preserving outbound `connect_to()` connections. This avoided the unused ESP HTTP server's minimum 4,608-byte task stack and its additional runtime allocations.

That experiment has been retired now that the XIAO ESP32-S3 replaces the constrained C3. The submodule points to upstream-equivalent commit `90cbd80864bf763fb6a4100f23b1ad2e128c8a75`, the override has been removed, and the client again uses the library's default inbound server while retaining outbound `connect_to()` support. The experimental commit remains on the fork's `wled` branch if C3 work resumes.

The C3 measurements remain useful historical evidence but do not define the XIAO configuration.

## Build and Flash

Build:

```bash
PIO_ENV=xiao_esp32s3_sendspin docker compose -f compose.build.yaml run --rm --build wled-build
```

The first XIAO installation must write the complete factory image at offset `0x0`:

```bash
esptool --chip esp32s3 --port /dev/cu.usbmodemXXXX write-flash 0x0 .pio/build/xiao_esp32s3_sendspin/firmware.factory.bin
```

## Remaining Validation

1. Rebuild the XIAO target and record firmware size and static RAM after diagnostic cleanup.
2. Confirm the image fits both 3.19 MiB OTA slots and validate an application update through WLED OTA.
3. Validate PSRAM detection and internal heap while disconnected, connected idle, and streaming.
4. Validate both the default inbound listener and the configured outbound Music Assistant connection.
5. Confirm sustained grouped visualizer streaming with addressable output on GPIO2.
6. Run `npm test` in the Docker environment; it remains explicitly deferred.

## Separate Follow-Ups

- If ESP32-C3 support resumes, investigate its RMT timeouts and re-evaluate the preserved outbound-only fork commit against current `sendspin-cpp` before reuse.
