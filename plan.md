# Sendspin Visualizer for WLED on MagWLED-1

Last updated: 2026-10-16

## Objective

Build a WLED firmware for the MagWLED-1 that appears to Music Assistant as a Sendspin visualizer and drives existing WLED sound-reactive effects from synchronized Sendspin visualization data.

The intended user flow is:

1. Flash the MagWLED-1 with the custom WLED build.
2. Configure Wi-Fi and the Music Assistant Sendspin WebSocket URL.
3. Enable the Sendspin visualizer integration.
4. Add the visualizer as a follower of an audio-capable Music Assistant player/group; do not target the non-audio visualizer directly for playback.
5. Select ordinary WLED sound-reactive effects and presets.
6. Receive synchronized visualization data from Music Assistant without a microphone, I2S input, local audio decoder, or local FFT.

## Status

| Phase | Status | Notes |
| --- | --- | --- |
| Planning and architecture | Complete | This document records the initial design and checkpoints. |
| Reproducible build tooling | Validated | `Dockerfile.build` and `compose.build.yaml` built stock `esp32c3dev` successfully on the macOS Docker host. |
| Stock MagWLED-1 build validation | Build complete; hardware validation pending | Firmware artifacts and initial flash/static-RAM measurements are available; GPIO behavior still requires the physical board. |
| Visualizer-only Sendspin build spike | Build complete; hardware validation pending | Links under pioarduino and fits the temporary 4 MB no-OTA proof layout at 70.1%. |
| Standalone Sendspin telemetry | Connected; streaming validation pending | Music Assistant connection, Noise handshake, server identification, and clock synchronization succeed on hardware. |
| WLED audio-data integration | Implemented; hardware validation pending | Sendspin is the sole AudioReactive compatibility provider in the MagWLED environment. |
| Runtime and memory validation | In progress | Connected idle has 27,012 bytes free with an 18,420-byte largest block; streaming and endurance measurements remain. |
| User documentation | Initial draft complete | Usermod readme covers build, setup, persistence, and hardware warnings. |
| Upstream preparation | Not started | Depends on prototype results and dependency packaging. |

Update this table and the Decision Log after each implementation phase.

Implementation checkpoint (2026-10-16):

- Added `usermods/sendspin/` with an ArduinoJson-isolated adapter, visualizer-only source selection, NVS persistence, bounded callback mailbox, reconnect lifecycle, settings, Info status, and the eight-field AudioReactive compatibility object.
- Added `DEBUG.md` to distinguish temporary logging/RMT workarounds from required dependency integration and to provide an explicit removal checklist.
- Added `magwled_sendspin` to `platformio_override.sample.ini` and an ignored local `platformio_override.ini`. The environment replaces, rather than inherits, `audioreactive`.
- Pinned the `sendspin-cpp` submodule at `b95cf854a784ed308b1f8fe9697f414b02768ddd`; the adapter requests ArduinoJson 7.4.2, noise-c at `62c2fdca7a874ea898761147bb062ca1cde9f36d`, and esp-protocols `websocket-v1.6.1` at `b7f3de500a4fd4b58e4248093067058959833260`.
- No WLED effect, core usermod API, generated web header, or main `platformio.ini` change was required.
- The macOS Docker host compiled all WLED, Sendspin, noise-c, WebSocket, and adapter translation units and linked `firmware.elf` under pioarduino. PlatformIO then rejected the image at its application-slot size check.
- The default Tasmota framework compiled the same sources but could not link `esp_websocket_client`: `libmbedtls.a` contains no `mbedtls_ssl_read` symbol. The supported pioarduino alternative supplies TLS and links, but adds enough code that the image exceeds the OTA slot by 599,189 bytes.
- For hardware proof only, `tools/WLED_ESP32_4MB_noOTA.csv` supplies one 3,080,192-byte application slot and the target defines `WLED_DISABLE_OTA`. The resulting 2,158,361-byte image builds successfully at 70.1% of that slot. Returning to OTA requires USB flashing a smaller image and OTA partition table.
- In the agent container, `npm ci` still cannot run because `/workspace/node_modules` is read-only; host Compose runs completed `npm ci` and `npm run build` successfully.

## Target Hardware

| Item | MagWLED-1 requirement |
| --- | --- |
| Product | MagWLED-1 / MWLED1, DGRZ, Amazon ASIN B0CYKYDH64 |
| MCU | Espressif ESP32-C3-MINI-1, single-core RISC-V, up to 160 MHz |
| RAM | Approximately 400 KB SRAM; no PSRAM may be assumed |
| Flash | 4 MB |
| Base WLED target | `esp32c3dev`, board `esp32-c3-devkitm-1`, Arduino on ESP-IDF 5.x |
| LED output | GPIO2, one addressable LED output |
| LED power switch | GPIO5, onboard MOSFET with inverted relay behavior; never use as general-purpose GPIO |
| Button / boot | GPIO9; never use as general-purpose GPIO |
| Available breakout pins | GPIO6, GPIO7, GPIO10 |
| USB | Native USB Serial/JTAG; build with `ARDUINO_USB_CDC_ON_BOOT=1` |
| Local audio | No onboard microphone; do not initialize ADC, I2S, DMA, or microphone pins |

The physical microphone documentation lists GPIO10 for SD, GPIO7 for BCLK, and GPIO6 for WS. A manufacturer WLED environment instead lists GPIO8 for WS. This discrepancy is intentionally irrelevant to this project: the Sendspin build must not initialize or allocate an I2S audio source.

The local development environment should inherit from `esp32c3dev` and apply the known board definitions without changing the repository's main `platformio.ini`:

| Define | Value |
| --- | --- |
| `WLED_RELEASE_NAME` | `MagWLED-1` |
| `WLED_WATCHDOG_TIMEOUT` | `0` |
| `ARDUINO_USB_CDC_ON_BOOT` | `1` |
| `LEDPIN` | `2` |
| `RLYPIN` | `5` |
| `BTNPIN` | `9` |
| `SERVERNAME` | `MagWLED-1` |

The relay inversion setting must be verified against the current WLED relay implementation and physical hardware before strip power is tested. Do not guess or repurpose GPIO5.

## Hardware Safety

Firmware work does not alter these electrical requirements:

- Set the board's physical voltage selector to 12 V for a 12 V WS2815 strip.
- Use a USB-C PD supply that explicitly advertises a 12 V profile.
- Never exceed 12 V at the external input.
- Never power the board from USB-C and external DC simultaneously.
- Add an external inline fuse when an external-power arrangement bypasses the onboard fuse.
- Connect strip VCC to board V+, strip ground to board ground, and strip DI to board DATA.
- Do not add an external logic-level shifter or data resistor; both are onboard.

## Architecture Decision

Do not copy or fork the existing AudioReactive usermod. Its microphone acquisition, I2S/ADC implementations, local FFT task, AGC, and UDP synchronization are unnecessary for server-generated Sendspin visualization data and are costly on a no-PSRAM ESP32-C3.

The first working integration should be a purpose-built `sendspin` usermod that:

- Compiles only the Sendspin core, ESP networking, and visualizer role.
- Does not compile Sendspin player, source, controller, metadata, artwork, color, FLAC, Opus, or PCM decoding support.
- Does not initialize local microphone, ADC, I2S, DMA, or FFT processing.
- Publishes the existing eight-field WLED AudioReactive `um_data_t` contract.
- Is compiled instead of the existing `audioreactive` usermod for this firmware target.
- Uses `USERMOD_ID_AUDIOREACTIVE` only as a deliberate compatibility provider, never alongside the real AudioReactive usermod.

This gives existing effects the data shape they already expect without modifying every effect. It also guarantees that no dormant microphone or FFT task consumes RAM or CPU.

The compatibility approach is suitable for the fork and for an out-of-tree proof of concept. Before proposing an upstream WLED merge, replace the implicit shared usermod ID with a maintainer-approved generic audio-data-provider contract or another explicit backend mechanism. Do not present duplicate usermod IDs as the final upstream architecture.

## Data Flow

```text
Music Assistant
  -> Sendspin encrypted WebSocket connection
  -> Sendspin clock synchronization
  -> Sendspin VisualizerRole scheduled callbacks
  -> fixed-size thread-safe latest-value mailbox
  -> Sendspin usermod main-loop snapshot
  -> WLED AudioReactive-compatible UMData
  -> existing WLED sound-reactive effects and presets
```

Sendspin's visualizer role already supplies playback-synchronized loudness, spectrum, dominant frequency, beat, and onset information. The device must not receive or decode the audio stream just to recompute those values.

## AudioReactive Compatibility Contract

The provider must retain stable writable storage for the full contract used by WLED effects:

| Index | WLED value | Sendspin source or behavior |
| --- | --- | --- |
| 0 | Smoothed volume (`float`) | Loudness mapped from 0-65535 to the WLED range; optional lightweight temporal smoothing only if runtime tests require it |
| 1 | Raw volume (`int16_t`) | Loudness mapped from 0-65535 to 0-255 |
| 2 | 16 spectrum bins (`uint8_t[16]`) | Request exactly 16 Sendspin display bins and map each from 0-65535 to 0-255 |
| 3 | Peak flag (`bool`) | One-shot flag from Sendspin onset and/or beat events |
| 4 | Dominant frequency (`float`) | Sendspin `f_peak` frequency, constrained to the range expected by WLED |
| 5 | Dominant magnitude (`float`) | Sendspin `f_peak` amplitude mapped and calibrated during runtime testing |
| 6 | Legacy maximum-volume setting (`uint8_t`) | Stable writable storage retained for effects that write through the contract |
| 7 | Legacy selected-bin setting (`uint8_t`) | Stable writable storage retained for effects that write through the contract |

Initial requested visualizer stream:

| Setting | Initial value |
| --- | --- |
| Types | Loudness, spectrum, dominant frequency, peak, beat |
| Maximum frame rate | 30 frames per second |
| Spectrum bins | 16 |
| Spectrum scale | Logarithmic |
| Frequency range | Approximately 43-9259 Hz to resemble current AudioReactive coverage |
| Display offset | User configurable; 0 ms default |
| Visualizer buffer | Start small and bounded; determine the minimum reliable value during the build spike |

Do not apply microphone gain, squelch, AGC, or FFT scaling to Sendspin's already normalized values. Add a separate bounded visual sensitivity adjustment only if physical testing shows that direct mapping is unsuitable.

When enabled but disconnected, the provider should publish silence rather than return simulated audio. When disabled, it may return no data and allow WLED's existing simulation behavior.

## Threading and Timing

Sendspin visualizer data callbacks run on the library's visualizer drain thread. WLED effects consume data from the main loop. The implementation must not write directly into effect-visible fields from a Sendspin callback.

Use a bounded, fixed-size latest-value mailbox with these properties:

- No allocation in callbacks or render loops.
- Spectrum data copied immediately because the Sendspin vector is reused after the callback.
- Short, bounded synchronization using a FreeRTOS queue, mutex with bounded acquisition, or another WLED-appropriate mechanism.
- Main-loop ownership of the public AudioReactive-compatible fields.
- Latest value wins when the renderer falls behind.
- Stream end, role clear, disconnect, and stale-data timeout atomically request silence.
- Beat and peak events remain pending until consumed once by the main loop.

Use `VisualizerRoleConfig::display_offset_ms` for synchronization adjustment. Label the sign clearly in the UI: positive values deliver earlier; negative values deliver later. Recreating the client after an offset change is acceptable for the first version.

## Configuration and UI

Use WLED's generated usermod settings UI rather than adding a new global settings page.

Initial persistent settings:

| Setting | Behavior |
| --- | --- |
| Enabled | Starts or stops the Sendspin integration |
| WebSocket URL | Full `ws://.../sendspin` URL; a blank value may mean inbound/listen-only mode if that remains viable on the C3 |
| Device name | Defaults to the WLED server description / `MagWLED-1` |
| Visual offset | Signed milliseconds, clamped to a conservative documented range |
| Allow unpaired access | Explicit opt-in, default off |

Expose non-secret status in WLED's Info page:

- Disabled, waiting for network, connecting, connected, or retrying.
- Server name when connected.
- Time synchronization state.
- Visualizer stream active/inactive state.
- Last frame age.
- Optional heap diagnostics in development builds only.

Do not place pairing PSKs, pairing tokens, or other secret material in `cfg.json`, `/json/info`, logs, or generated browser JavaScript.

## Identity and Pairing Persistence

Implement Sendspin's persistence provider over a dedicated ESP32 NVS/Preferences namespace. Store the opaque blobs exactly as provided by the library.

Requirements:

- Stable Sendspin identity across reboots.
- Persistent pairing records and Pairing PSK.
- No parsing or transformation of library-owned blobs.
- Bounded writes from the WLED main-loop context.
- Clear error reporting without logging secret values.
- A documented future method to clear Sendspin identity/pairings.

For initial bring-up, explicitly enabling unpaired access is acceptable on the isolated test LAN. Pairing UI is not required before proving visualizer viability. If Music Assistant requires pairing for the desired role, add a minimal pairing flow only after the connection and memory spike succeeds.

## Connection Lifecycle

The usermod state machine should be explicit and non-blocking:

```text
Disabled
WaitingForNetwork
Starting
ConnectingOrListening
Connected
Streaming
RetryBackoff
StoppingForOTA
```

Lifecycle requirements:

- Call the Sendspin main-loop function regularly without blocking WLED rendering.
- Start only after WLED networking is available.
- Retry outbound connections conservatively because `connect_to()` does not automatically reconnect and an ESP DNS/connect attempt can remain active for a long bounded interval.
- Avoid repeatedly replacing an in-flight connection attempt.
- Clear visualization data immediately on stream end and after a short stale-frame timeout.
- Stop synchronously before destroying listeners, providers, or the client.
- Stop for OTA and resume cleanly afterward.
- Restore Wi-Fi power-saving behavior when high-performance networking is released.
- Do not interfere with WLED's existing AsyncWebServer on port 80.

## Build Integration

`sendspin-cpp` is currently an ESP-IDF CMake component, not a drop-in PlatformIO Arduino library. It requires ESP-IDF 5.5.2 or newer and C++20. The current WLED ESP32-C3 platform uses a compatible ESP-IDF generation, but the following must be proven rather than assumed:

- Visualizer-only source selection works under PlatformIO.
- Sendspin translation units compile as C++20 without changing all WLED translation units unnecessarily.
- `esp_websocket_client`, Noise, and ArduinoJson 7 dependencies can be built under the Arduino environment.
- The precompiled Arduino framework has the HTTP server WebSocket support required by Sendspin's inbound server.
- The separate WebSocket transmit-lock configuration required by Sendspin can be supplied correctly.
- ArduinoJson 7 remains isolated from WLED's embedded ArduinoJson 6 translation units.
- The resulting 4 MB firmware partition has sufficient application space.

Keep the `sendspin-cpp` submodule unchanged during the first spike if practical. Put compatibility build logic in a clearly isolated wrapper so it can later become either:

- PlatformIO support contributed to `sendspin-cpp` itself.
- A separately packaged WLED usermod dependency.
- A temporary fork-only build adapter, if neither upstream accepts the packaging immediately.

Do not edit the main `platformio.ini` without explicit WLED maintainer approval. Use a local ignored `platformio_override.ini` for the MagWLED-1 environment during development. A distributable board example may later be proposed in `platformio_override.ini.sample`.

## ESP32-C3 Resource Strategy

The no-PSRAM ESP32-C3 is the limiting target. Optimize for bounded capacity, not feature completeness.

Compile out or omit:

- Sendspin player and source roles.
- FLAC, Opus, and PCM audio decoding/output.
- Metadata, artwork, color, and controller roles.
- Local AudioReactive microphone acquisition.
- Local FFT and its task, buffers, and ArduinoFFT dependency where feasible.
- I2S and ADC initialization.
- Bluetooth functionality.
- Message tracing outside temporary diagnostics.

Keep small and bounded:

- Sendspin inbound visualizer ring.
- JSON arena.
- Noise fragmentation and reassembly buffers where the protocol/library safely permits tuning.
- Protocol, WebSocket, HTTP server, and visualizer task stacks.
- WLED LED count during initial validation.

Do not lower a task stack below the library's documented call-graph minimum merely to make the build fit. If the inbound Sendspin server consumes too much RAM, prefer a supported outbound-only compile option. If the library has no such option, discuss an upstream `sendspin-cpp` capability rather than applying a fragile WLED-only patch.

## Implementation Phases

### Phase 1: Baseline Board Build

- Create an ignored local MagWLED-1 environment extending `esp32c3dev`.
- Build stock WLED without Sendspin.
- Flash and verify native USB recovery.
- Verify LED output on GPIO2.
- Verify the GPIO5 power switch turns the strip off and on with the correct inversion.
- Verify the GPIO9 button without changing its boot behavior.
- Record firmware size, free heap, minimum free heap, and largest free block after Wi-Fi connects.

Exit criterion: stock WLED operates the board safely and baseline resource measurements are recorded.

### Phase 2: Visualizer-Only Build Spike

- Build only Sendspin core, ESP networking, cryptography, time synchronization, persistence codec, and visualizer role.
- Resolve PlatformIO packaging without enabling any audio codec or player source.
- Start with reduced visualizer buffers appropriate to the C3.
- Confirm the firmware links within the 4 MB partition.
- Record static RAM, flash, and task-stack costs where available.

Exit criterion: WLED plus a started/stopped visualizer-only Sendspin client builds for `esp32c3dev` without microphone or FFT code running.

Stop condition: if the required dependencies cannot run in the Arduino framework, or safe stack/buffer minimums leave inadequate heap, document the measured blocker before changing architecture.

### Phase 3: Standalone Sendspin Bring-Up

- Connect to Music Assistant using an explicit WebSocket URL.
- Persist identity in NVS.
- Log only bounded non-secret diagnostics.
- Observe synchronized timestamp, loudness, 16 spectrum bins, dominant frequency, peak, and beat/downbeat events.
- Drive a temporary timestamped peak indication through the normal WLED segment/effect path, not direct GPIO manipulation.
- Measure free heap before client start, after connection, during playback, and after disconnect/reconnect.
- Measure minimum free heap and largest free block during an extended stream.

Exit criterion: synchronized visualizer frames are reliable on physical hardware with an acceptable memory margin and no WLED rendering instability.

### Phase 4: WLED Audio Data Provider

- Implement the stable eight-field AudioReactive-compatible data object.
- Transfer Sendspin mailbox values to the public object on the WLED main loop.
- Register the Sendspin usermod as the sole provider for the AudioReactive compatibility ID in this build.
- Ensure the existing AudioReactive usermod is not linked into the same target.
- Clear to silence on stale data and disconnect.
- Verify representative volume-, peak-, and spectrum-reactive effects.
- Verify normal non-audio effects remain unchanged.

Exit criterion: ordinary WLED sound-reactive effects respond to Music Assistant playback without effect-specific changes.

### Phase 5: Settings and Reliability

- Add persistent URL, enable, offset, name, and unpaired-access settings.
- Add connection and stream status to the Info page.
- Add conservative reconnect backoff.
- Handle Wi-Fi loss, server restart, role removal, stream clear, user disable, configuration changes, and OTA.
- Confirm no secrets appear in configuration responses or logs.

Exit criterion: the device recovers without reboot from routine Wi-Fi and Music Assistant interruptions.

### Phase 6: Preset and Timing Validation

- Test at least one volume-reactive effect.
- Test at least one peak/beat-reactive effect.
- Test at least one 16-band frequency-reactive effect.
- Save and reload presets using those effects.
- Compare visible timing against an audible Music Assistant player.
- Adjust the visual offset in both directions and verify the documented sign.
- Check whether presets relying on AudioReactive's dynamically registered palettes need explicit support.

Exit criterion: normal preset workflows work and visual timing can be aligned predictably.

### Phase 7: Documentation and Sharing

- Document the exact build environment and dependency revisions.
- Document flashing and USB recovery.
- Document MagWLED-1 pin ownership and 12 V power warnings.
- Document Music Assistant URL, discovery/pairing limitations, and visual offset.
- Document that this target replaces, rather than coexists with, the microphone AudioReactive usermod.
- Record measured flash and RAM usage.
- Separate any required `sendspin-cpp` packaging changes from WLED integration changes.

Exit criterion: another MagWLED-1 owner can build, flash, connect, select a sound-reactive preset, and reproduce the result.

## Validation Matrix

| Test | Required result |
| --- | --- |
| Stock `esp32c3dev` build | Continues to compile without Sendspin support |
| MagWLED-1 local build | Compiles for 4 MB ESP32-C3 with native USB |
| Sendspin role set | Visualizer only; no player/source/codecs or unrelated roles |
| Boot without Wi-Fi | WLED remains responsive and waits without tight reconnect loops |
| Music Assistant unavailable | WLED effects and UI remain responsive |
| Connect and stream | Time sync completes and visual data updates at the requested rate |
| Stream stop | Audio data clears to silence promptly |
| Wi-Fi reconnect | Sendspin reconnects without reboot or stale animation |
| Server restart | Sendspin retries with bounded backoff |
| OTA start/failure | Client stops safely and can resume after a failed update |
| Long playback | No progressive heap loss or largest-block collapse |
| GPIO ownership | GPIO2/5/9 retain LED, relay, and button functions; GPIO6/7/10 remain unallocated |
| Preset persistence | Sound-reactive presets save, reload, and continue using Sendspin data |

Required host validation command:

```bash
PIO_ENV=magwled_sendspin docker compose -f compose.build.yaml run --rm --build wled-build
```

The Compose command runs `npm ci`, `npm run build`, and `pio run -e magwled_sendspin` in that order. It succeeds with the explicitly temporary no-OTA proof partition. Run `npm test` separately in the same writable Docker dependency volume. Per repository policy, web assets must be built before firmware compilation even though no web source changed.

## Resource Measurements

Record actual values here during implementation.

| Measurement | Stock WLED | Sendspin started | Connected idle | Streaming | Minimum observed |
| --- | ---: | ---: | ---: | ---: | ---: |
| Firmware size | 1,313,280 bytes | 2,158,361 bytes | N/A | N/A | N/A |
| Application slot usage | 83.5% of 1,572,864-byte OTA slot | 70.1% of 3,080,192-byte no-OTA slot | N/A | N/A | N/A |
| Linked `.data` + `.bss` | 74,080 bytes | 81,828 bytes | N/A | N/A | N/A |
| Link-time primary DRAM after heap start | 202,656 bytes | TBD | N/A | N/A | N/A |
| Free heap | TBD | TBD | 27,012 bytes | TBD | 27,012 bytes |
| Largest free block | TBD | TBD | 18,420 bytes | TBD | 18,420 bytes |
| Internal RAM used by tasks/buffers | TBD | TBD | TBD | TBD | N/A |

The implementation is not considered viable merely because it links. It must retain enough contiguous internal heap for WLED rendering, networking, JSON handling, and transient operations. The acceptable margin will be decided from measurements under the intended LED count and workload rather than from total free heap alone.

Initial board measurement with the temporary PWM output: disabling Sendspin recovered free heap to 109,756 bytes. During outbound retries, free heap measured 40,992 bytes after the first attempt and 39,928 bytes after the second; the largest contiguous block fell from 24,564 to 19,444 bytes. Disabling the integration recovered the memory, so teardown works, but the remaining connection-time margin is not yet acceptable without a successful handshake and streaming test.

After correcting the `noise-c` protocol-table build override, the client completed its Noise handshake, connected to Music Assistant, and synchronized its clock. Connected idle measured 27,012 bytes free with an 18,420-byte largest block. The visualizer stream had not yet been activated, so this is not the final runtime minimum.

## Scope Boundaries

Included in the first useful version:

- ESP32-C3 MagWLED-1 target.
- Sendspin visualizer role.
- Existing WLED sound-reactive effect compatibility.
- Manual Music Assistant URL.
- Visual timing offset.
- Persistent identity.
- Explicit unpaired-access option.
- Connection status and robust reconnect behavior.

Deferred unless required for basic operation:

- Sendspin audio playback.
- FLAC, Opus, or PCM decoding.
- Speaker or I2S output.
- Microphone or line input.
- Artwork, metadata, controller, source, and color roles.
- mDNS discovery UI.
- Full pairing-token or QR-code UI.
- Multiple simultaneous Sendspin servers.
- Changes to individual WLED effects.
- Support for ESP8266 or older ESP-IDF targets.
- AudioReactive dynamic palettes if ordinary presets do not require them.

## Upstream Strategy

The prototype should remain separable into three concerns:

1. PlatformIO/Arduino packaging for visualizer-only `sendspin-cpp`.
2. A small WLED Sendspin usermod and AudioReactive-compatible provider.
3. MagWLED-1 board configuration and user documentation.

Likely upstream sequence:

1. Contribute generally useful PlatformIO support or an outbound-only constrained-device option to `sendspin-cpp` if required.
2. Publish the Sendspin WLED integration as an out-of-tree usermod for testing by other users.
3. Discuss a typed/generic audio-data-provider interface with WLED maintainers before proposing in-tree support.
4. Submit a focused WLED PR only after runtime measurements and user testing demonstrate that the ESP32-C3 target is reliable.

Do not include the development submodule in a WLED PR unless maintainers explicitly request that dependency form. Pin external dependency revisions immutably in any distributable build.

## Risks and Decision Gates

| Risk | Decision gate or mitigation |
| --- | --- |
| PlatformIO does not process Sendspin's IDF component metadata | Build a visualizer-only wrapper; prefer upstream packaging over broad WLED build changes |
| Required HTTP WebSocket support is absent from the precompiled Arduino framework | Prefer outbound-only Sendspin support; otherwise stop and document the framework blocker |
| C++20 flags affect WLED globally | Compile Sendspin in an isolated library environment if PlatformIO permits it |
| Noise/WebSocket dependencies consume excessive flash or RAM | Measure before AudioReactive integration; remove unrelated roles and tune only documented buffers |
| Sendspin server task is too expensive on C3 | Propose a supported outbound-only library option |
| Duplicate AudioReactive usermod ID is unsuitable upstream | Keep providers mutually exclusive in the prototype; design a maintainer-approved provider interface before merge |
| Dynamic AudioReactive palettes are absent | Test real presets; add compatible palette support only if needed |
| Visualizer normalization differs from WLED FFT output | Calibrate mappings using runtime captures without reintroducing local FFT |
| Unpaired access is undesirable | Default it off and store pairing material securely in NVS |
| GPIO5 inversion is wrong | Validate on stock firmware and inspect current relay semantics before energizing a strip |

## Open Questions

- What Music Assistant version and Sendspin provider version will be used for physical testing?
- Does Music Assistant expose the desired visualizer client through an outbound connection without pairing, or is a minimal pairing flow required?
- Does the installed Arduino ESP32-C3 framework include the HTTP server WebSocket configuration needed for inbound Sendspin connections?
- Can `sendspin-cpp` and its dependencies be packaged for PlatformIO without modifying the submodule?
- Is an outbound-only Sendspin build necessary to fit the ESP32-C3 memory budget?
- What LED count and effect frame rate should define the final stress test?
- Which existing sound-reactive presets rely on AudioReactive's dynamic palettes?

## Decision Log

| Date | Decision | Reason |
| --- | --- | --- |
| 2026-10-15 | Target MagWLED-1 / ESP32-C3 rather than ESP32-S3 | Hardware was identified as MWLED1 with 4 MB flash and no PSRAM |
| 2026-10-15 | Use Sendspin VisualizerRole rather than PlayerRole | Music Assistant already computes synchronized visualization features; audio decode/output is unnecessary |
| 2026-10-15 | Do not fork/copy AudioReactive | Avoid duplicated microphone, FFT, AGC, and network-sync code |
| 2026-10-15 | Begin with a standalone Sendspin compatibility provider | Guarantees no local audio task on the constrained C3 and minimizes WLED core changes |
| 2026-10-15 | Keep AudioReactive and Sendspin providers mutually exclusive | Both would otherwise compete for the same effect-facing data identity |
| 2026-10-15 | Validate Sendspin build and memory before effect integration | Dependency/toolchain and no-PSRAM feasibility are the main risks |
| 2026-10-15 | Use local board overrides during development | Changes to the main `platformio.ini` require maintainer approval |
| 2026-10-15 | Use a Docker Compose build environment | The agent container has no Python, PlatformIO, or Docker daemon, and its host-mounted `node_modules` is read-only |
| 2026-10-16 | Accept the stock `esp32c3dev` Docker build as the compile baseline | The build produced firmware, ELF, linker map, bootloader, and partition artifacts within the standard 4 MB OTA layout |
| 2026-10-16 | Isolate Sendspin behind a POD adapter translation-unit boundary | Prevents ArduinoJson 7 and Sendspin C++20 headers from colliding with WLED's embedded ArduinoJson 6 translation units |
| 2026-10-16 | Use a 2 KB visualizer quota and minimum documented task stacks for the first build | Bounds the no-PSRAM C3 prototype without lowering Sendspin's measured stack minima |
| 2026-10-16 | Reject URL credentials, query strings, and fragments | Prevents likely secret material from entering `cfg.json`, browser settings data, or Sendspin URL diagnostics |
| 2026-10-16 | Keep GPIO5 relay inversion unset | The current relay semantics and physical board behavior must be verified before strip power is controlled |
| 2026-10-16 | Use WLED's pioarduino platform for the Sendspin spike | The default Tasmota framework's precompiled `libmbedtls.a` omits SSL symbols required by esp_websocket_client; pioarduino on the same IDF generation links successfully |
| 2026-10-16 | Stop at the Phase 2 application-size gate | The linked 2,172,053-byte image exceeds the standard OTA slot by 599,189 bytes; silently disabling OTA or changing partitions would violate the target requirements |
| 2026-10-16 | Add an explicitly temporary no-OTA proof target after user approval | The available 4 MB board can validate Sendspin and runtime memory before buying larger-flash hardware; USB remains the only update/recovery path |
| 2026-10-17 | Match the full `noise-c` builder source path when enabling protocol-name tables | The package defaults to ESPHome's `Noise_NNpsk0`; the previous basename check saw `src` and left Sendspin's required `Noise_KKpsk2` unavailable |

## References

Repository references:

- `usermods/audioreactive/audio_reactive.cpp`: existing AudioReactive data contract, UDP synchronization, palettes, and lifecycle.
- `usermods/audioreactive/audio_source.h`: local microphone/I2S/ADC source abstraction that this target must not initialize.
- `wled00/FX.cpp`: sound-reactive effect data lookup and consumption.
- `wled00/fcn_declare.h`: `um_data_t` and usermod API.
- `sendspin-cpp/include/sendspin/visualizer_role.h`: synchronized visualizer callbacks and value ranges.
- `sendspin-cpp/include/sendspin/config.h`: visualizer buffer, spectrum, timing, task, and memory settings.
- `sendspin-cpp/docs/integration-guide.md`: client lifecycle, providers, persistence, pairing, and thread rules.
- `sendspin-cpp/cmake/sources.cmake`: core, platform, and per-role source separation.
- `sendspin-cpp/idf_component.yml`: ESP-IDF and dependency requirements.

Hardware references supplied for planning:

- Espressif ESP32-C3 datasheet: https://documentation.espressif.com/esp32-c3_datasheet_en.html
- MagWLED-1 product page: https://magwled.com/pages/magwled-1
- MagWLED-1 support documentation: https://support.magwled.com/getting-started/about-magwled-1/
- Amazon product identifier: B0CYKYDH64
