# Matter (Project CHIP) WiFi-only Usermod

This usermod adds **Matter smart-home protocol** support to WLED-MM, allowing the device to appear natively in **Apple Home, Google Home, and Amazon Alexa** Matter ecosystems — without requiring Bluetooth/BLE.

## Features

| Feature | Details |
|---|---|
| **Device type** | Extended Color Light (`0x010D`) |
| **Commissioning** | WiFi only — Soft-AP or on-network (mDNS) |
| **BLE requirement** | None (`CONFIG_BT_ENABLED=n`) |
| **Clusters** | On/Off · Level Control (brightness) · Color Control (HSV + Color Temperature) |
| **State sync** | Bidirectional — Matter ↔ WLED UI / API / presets |

## How It Works

1. **WLED connects to WiFi** as usual (via the WLED web UI or AP setup).
2. The **Matter stack** starts and advertises the device via **mDNS** on the local network.
3. A Matter commissioner (Apple Home, Google Home, etc.) discovers the device and commissions it using the **setup PIN** and **discriminator**.
4. Once commissioned, the controller can turn the light on/off, adjust brightness, and change colour — all changes are bridged to WLED's internal colour state.
5. Changes made through the WLED web UI, HTTP API, or presets are **automatically synced back** to the Matter fabric.

If the device has **no WiFi credentials** at first boot, the Matter stack creates a **Soft-AP** (`CHIP-XXXX`) so the commissioner can provision both the WiFi network and the Matter fabric in one step.

## Build Requirements

| Requirement | Minimum version |
|---|---|
| ESP-IDF | v5.1+ |
| `esp_matter` component | latest from [Espressif Component Registry](https://components.espressif.com/components/espressif/esp_matter) |
| Target SoC | ESP32-S3, ESP32-C3, or ESP32-H2 (classic ESP32 has limited support) |
| PlatformIO platform | `espressif32` ≥ 6.4.0 (ESP-IDF v5.1 based) |

> **Note:** The Matter SDK is large. Builds with this usermod require an ESP32-S3 (or similar) with at least 8 MB flash. PSRAM is recommended.

## Build Environment

A ready-made PlatformIO build environment is provided in `platformio.ini`:

```ini
[env:esp32s3_matter_wifi]
```

To build:

```bash
# Build web UI first (required before any firmware build)
npm run build

# Build the Matter-enabled firmware
pio run -e esp32s3_matter_wifi
```

## Enabling in a Custom Build

Add these flags to your PlatformIO environment's `build_flags`:

```ini
build_flags =
  -D USERMOD_MATTER
  -D CONFIG_BT_ENABLED=0
```

And ensure `esp_matter` is available as a library/component dependency.

## Configuration

After flashing, the following settings are available in **WLED Settings → Usermods → Matter**:

| Setting | Default | Description |
|---|---|---|
| `passcode` | `20202021` | Matter setup PIN code (8-digit numeric). Change for production use. |
| `discriminator` | `3840` | 12-bit discriminator for device discovery. |

## Commissioning

### Using Apple Home
1. Open the **Home** app → **+** → **Add Accessory**.
2. Choose **More options…** → the WLED device should appear.
3. Enter the setup PIN when prompted (default: `20202021`).

### Using Google Home
1. Open the **Google Home** app → **+** → **Set up device** → **New device**.
2. Select **Matter** as the device type.
3. Enter the manual pairing code.

### Using a Matter Test Tool
```bash
chip-tool pairing onnetwork <node-id> 20202021
```

## Technical Details

- **Thread safety**: Matter callbacks run on the Matter task. Pending state is transferred to WLED via volatile flags and applied in the Arduino `loop()` context.
- **Colour mapping**: Matter hue (0–254) is scaled to WLED's `colorHStoRGB()` range (0–65535). Matter saturation (0–254) maps to WLED's 0–255 range. Colour temperature is passed as mireds to `colorCTtoRGB()`.
- **Sync interval**: WLED → Matter state is pushed every 250 ms to avoid flooding the Matter fabric with attribute reports.

## Memory Impact

With `CONFIG_BT_ENABLED=n`:
- **RAM**: ~40–60 KB additional (vs ~100–160 KB with BLE)
- **Flash**: ~400–600 KB additional for the Matter/CHIP stack

## Files

| File | Purpose |
|---|---|
| `usermods/usermod_v2_matter/usermod_v2_matter.h` | Usermod implementation |
| `usermods/usermod_v2_matter/readme.md` | This documentation |
