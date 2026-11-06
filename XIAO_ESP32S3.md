# XIAO ESP32-S3 Sendspin Target

This target supports the standard, non-Sense Seeed Studio XIAO ESP32-S3 with 8 MB QIO flash and 8 MB OPI PSRAM. It replaces the MagWLED-1 proof target for active development without removing that historical environment.

## Build Configuration

The `xiao_esp32s3_sendspin` environment uses:

- PlatformIO board `seeed_xiao_esp32s3`.
- WLED's ESP32-S3 `qio_opi` memory topology.
- The pioarduino Arduino 3.3.9 / ESP-IDF 5.5.4 platform required by the Sendspin WebSocket TLS dependency.
- Only the `sendspin` usermod, replacing the microphone-based AudioReactive usermod.
- GPIO2 (XIAO D1) as the default addressable LED data output.
- No relay or LED-power-control GPIO.
- No WLED button GPIO; GPIO0 remains dedicated to the BOOT/recovery button.
- Bounded, non-secret Sendspin lifecycle and memory diagnostics under `/json/info`.

Build from the repository root:

```bash
PIO_ENV=xiao_esp32s3_sendspin docker compose -f compose.build.yaml run --rm --build wled-build
```

Artifacts are written under `.pio/build/xiao_esp32s3_sendspin/`.

## Flash Layout

`tools/WLED_ESP32_8MB_3MB_OTA.csv` provides:

| Partition | Size |
| --- | ---: |
| NVS | 20 KiB |
| OTA metadata | 8 KiB |
| OTA application 0 | 3.19 MiB |
| OTA application 1 | 3.19 MiB |
| LittleFS | 1.5 MiB |
| Coredump | 64 KiB |

The larger application slots are required because the Sendspin firmware exceeded WLED's normal 2 MiB slot on the ESP32-C3 proof build. OTA remains enabled, but the first installation must flash the complete factory image so the bootloader and partition table match.

## Initial USB Flash

Back up any existing configuration before changing partition layouts. Connect the XIAO directly over native USB, with the LED strip disconnected for initial validation.

List candidate ports on macOS:

```bash
ls /dev/cu.usbmodem*
```

Erase and flash the merged factory image:

```bash
esptool --chip esp32s3 --port /dev/cu.usbmodemXXXX erase-flash
esptool --chip esp32s3 --port /dev/cu.usbmodemXXXX write-flash 0x0 .pio/build/xiao_esp32s3_sendspin/firmware.factory.bin
```

For recovery, hold BOOT, tap RESET, release BOOT, and check the serial-device name again. Native USB may disconnect and re-enumerate during reset or flashing because the board has no separate USB-UART bridge.

After the factory image is installed, application-only updates can use WLED's OTA updater while the firmware remains below the 3.19 MiB slot limit.

## PSRAM Use

The usermod checks `psramFound()` and `ESP.getPsramSize()` at runtime. When PSRAM is available, Sendspin's capacity-oriented inbound ring and Noise buffers prefer external RAM; on a board without usable PSRAM they fall back to internal RAM. Task stacks remain internal for predictable hot-path performance and compatibility.

After boot, `/json/info` should report an 8 MB PSRAM size and substantial free PSRAM. Validate internal free heap and largest-block margin while disconnected, connected idle, and streaming; PSRAM does not replace internal memory needed by Wi-Fi, task stacks, DMA descriptors, or cache-disabled operations.

## LED Wiring And Power

The XIAO has no onboard level shifter, LED connector, fuse, or strip-power MOSFET.

- Connect GPIO2/D1 to the strip data input.
- Connect XIAO ground and strip-power ground together.
- Use a 74AHCT125, 74HCT245, or equivalent 5 V logic-level shifter for reliable 5 V WS2812 data, especially with longer wiring.
- Power a substantial strip from a separately sized, fused 5 V supply. Do not route high strip current through the XIAO's 3.3 V regulator or assume its USB/VBUS traces can power an arbitrary LED count.
- WLED's default 850 mA brightness limiter is a software estimate, not electrical protection and not a substitute for a correctly sized supply and fuse.
- Never connect two independent 5 V supplies together unless the power design explicitly supports it. During USB programming with an externally powered strip, share ground but prevent unintended 5 V backfeeding between supplies.

The firmware imposes no board-specific safe LED-count claim. Determine the deployed count from the external supply, wire gauge, fusing, voltage drop, injection points, and strip specifications.

## Sendspin Connection

The target uses upstream Sendspin connection behavior: it listens for inbound connections on port 8928 and can also connect to the configured Music Assistant URL. The client is visualizer-only, so add it as a follower of an audio-capable Music Assistant player/group and start playback on that leader rather than directly on the WLED visualizer.
