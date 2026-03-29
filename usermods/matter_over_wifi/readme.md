# Matter (Project CHIP) WiFi-only Usermod

This usermod adds **Matter smart-home protocol** support to WLED, allowing the device to appear natively in **Apple Home, Google Home, and Amazon Alexa** Matter ecosystems — without requiring Bluetooth/BLE.

## Features

| Feature | Details |
|---|---|
| **Device type** | Extended Color Light (`0x010D`) |
| **Commissioning** | On-network (mDNS) — requires WiFi STA connection first |
| **BLE requirement** | None (`CONFIG_BT_ENABLED=n`) |
| **Clusters** | On/Off · Level Control (brightness) · Color Control (HSV + Color Temperature) |
| **State sync** | Bidirectional — Matter ↔ WLED UI / API / presets |
| **AP mode safe** | Matter stack only starts after WLED joins WiFi as a STA client |

## How It Works

1. **WLED connects to WiFi** as usual (via the WLED web UI or AP setup page at `192.168.4.1`).
2. Once connected as a STA client, the **Matter stack starts** and advertises the device via **mDNS** (`_matterc._udp`).
3. A Matter commissioner (Apple Home, Google Home, etc.) discovers the device and commissions it using the **manual pairing code** or **QR code**.
4. Once commissioned, the controller can turn the light on/off, adjust brightness, and change colour — all changes are bridged to WLED's internal colour state in real time.
5. Changes made through the WLED web UI, HTTP API, or presets are **automatically synced back** to the Matter fabric.

> **Important:** If the device has no WiFi credentials (first boot), WLED opens its normal `WLED-AP` access point. The Matter stack does **not** start until after WiFi credentials are entered and the device connects as a STA client. This ensures the AP is always reachable for initial setup.

## Build Requirements

| Requirement | Details |
|---|---|
| Target SoC | ESP32-S3 (tested), ESP32-C3, ESP32-H2 |
| Flash | 8 MB minimum |
| ESP-IDF | v5.1+ (via PlatformIO `espressif32` platform) |
| `esp_matter` component | Pulled automatically via `idf_component.yml` |

> **Note:** Classic ESP32 is not supported — the Matter SDK requires a dual-core SoC with sufficient flash. PSRAM is not required but helps.

## Build Environment

A ready-made PlatformIO environment is provided in `platformio.ini`:

```bash
pio run -e esp32s3_matter_wifi
pio run -e esp32s3_matter_wifi -t upload
```

## Configuration

After flashing, the following settings are available in **WLED Settings → Usermods → Matter**:

| Setting | Default | Description |
|---|---|---|
| `passcode` | `20202021` | Matter setup PIN (8-digit). Change before production use. |
| `discriminator` | `3840` | 12-bit value used during device discovery. |

> **Warning:** `20202021` and `3840` are the Matter SDK test defaults and are publicly known. Change them before deploying on a shared network.

## Commissioning

The **manual pairing code** and a **QR code link** are shown on the WLED info page (`http://<device-ip>` → Info tab) once the device is connected and Matter is running.

### Google Home
1. Open Google Home → **+** → **Set up device** → **New device**.
2. Select **Matter** → enter the 11-digit manual pairing code shown on the info page.

### Apple Home
1. Open the **Home** app → **+** → **Add Accessory** → **More options…**
2. The WLED device should appear for on-network commissioning.

### chip-tool (for development)
```bash
chip-tool pairing onnetwork <node-id> 20202021
```

## Factory Reset

To recommission the device with a new controller (e.g. after removing it from Google Home):

### Software reset (recommended) — preserves WiFi credentials

Send a `POST` to the WLED JSON state API:

```bash
curl -X POST http://<device-ip>/json/state \
     -H 'Content-Type: application/json' \
     -d '{"Matter":{"factoryReset":true}}'
```

The device erases only the Matter NVS namespaces (`chip-config`, `chip-counters`, `CHIP_KVS`,
`esp_matter_kvs`, `node`) and restarts. WiFi credentials are preserved — the device reconnects
automatically and Matter starts in un-commissioned state, ready to pair again.

### Hardware NVS erase — also clears WiFi credentials

```bash
esptool.py --port /dev/ttyACM1 erase_region 0x9000 0x5000
```

**Note:** This erases everything including WLED's saved WiFi credentials. After erasing,
connect to the `WLED-AP` access point and re-enter WiFi credentials before attempting
to recommission.

## Technical Details

- **WiFi ownership**: Once the Matter stack starts (after WLED connects to WiFi), it takes ownership of the WiFi netif. The `externalWiFiManager` flag prevents WLED from calling `WiFi.mode(WIFI_MODE_NULL)` on reconnect attempts, which would otherwise destroy the netif.
- **Thread safety**: Matter attribute callbacks run on the Matter RTOS task. Pending state is transferred to WLED via per-attribute volatile dirty flags and applied in the Arduino `loop()` context. A `mSyncing` flag suppresses re-entrant callbacks triggered by `attribute::update()` calls in the WLED→Matter sync path.
- **Colour mapping**: Matter hue (0–254) → WLED `colorHStoRGB()` (0–65535). Matter saturation (0–254) → WLED (0–255). Colour temperature passed as mireds to `colorCTtoRGB()`.
- **RGB capability**: `hue_saturation` feature is explicitly added to the color_control cluster after endpoint creation, because `extended_color_light::create()` only adds `color_temperature` and `xy` by default.

## Files

| File | Purpose |
|---|---|
| `usermod_v2_matter.h` | Class declaration (PIMPL pattern — hides CHIP SDK types from wled.h) |
| `usermod_v2_matter.cpp` | Full implementation including Matter node, endpoint, callbacks, and WLED sync |
| `library.json` | PlatformIO library manifest |
| `idf_component.yml` | ESP-IDF component dependency (`espressif/esp_matter`) |
