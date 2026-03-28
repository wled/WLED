# Zigbee RGB Light Usermod

This usermod exposes WLED as a **Zigbee HA Color Dimmable Light** on the ESP32-C6, allowing Zigbee coordinators to control power, brightness, and color. It has been tested and confirmed working with:

- **Philips Hue Bridge v2** (square model)
- Zigbee2MQTT / ZHA (should work, uses standard HA profile)

| Zigbee Cluster | Function | Color Space |
|---|---|---|
| On/Off (0x0006) | Power on/off | -- |
| Level Control (0x0008) | Brightness (0-254) | -- |
| Color Control (0x0300) | Color | CIE XY (Hue) and Hue/Saturation |

## Hardware Requirements

- **ESP32-C6** -- the only ESP32 variant with a native IEEE 802.15.4 radio required for Zigbee.
- Standard WS2812B / SK6812 / NeoPixel LED strip connected via analog (PWM/LEDC) output.

> **Note:** Classic ESP32, ESP32-S2, ESP32-S3, ESP32-C3, and ESP8266 do **not** have an 802.15.4 radio and cannot run this usermod.

> **Note:** The 802.15.4 radio shares the RMT peripheral on the ESP32-C6. Digital LED buses (WS2812B via RMT) will conflict -- use `-DWLED_MAX_DIGITAL_CHANNELS=0` and analog/PWM outputs instead.

## Quick Start

### 1. Build environment

Create or edit `platformio_override.ini` in the WLED project root:

```ini
[platformio]
default_envs = esp32c6_zigbee

[env:esp32c6_zigbee]
extends = env:esp32c6dev_8MB
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip
platform_packages =
board_build.partitions = tools/WLED_ESP32_8MB_Zigbee.csv
custom_usermods = zigbee_rgb_light
build_unflags =
  -D WLED_RELEASE_NAME="ESP32-C6_8MB"
  -D CORE_DEBUG_LEVEL=0
  -DCORE_DEBUG_LEVEL=0
build_flags = ${env:esp32c6dev_8MB.build_flags}
               -D WLED_RELEASE_NAME="ESP32C6_Zigbee"
               -DZIGBEE_MODE_ED=1
               -DUSERMOD_ZIGBEE_RGB_LIGHT
               -DCORE_DEBUG_LEVEL=3
               -DWLED_MAX_DIGITAL_CHANNELS=0
```

### 2. Partition table

The Zigbee stack requires two dedicated flash partitions. Use the provided `tools/WLED_ESP32_8MB_Zigbee.csv`:

| Partition | Type | Size | Purpose |
|-----------|------|------|---------|
| `zb_storage` | fat | 16 KB | Network parameters, bindings, groups |
| `zb_fct` | fat | 4 KB | Factory configuration test data |

### 3. Build and flash

```bash
pio run -e esp32c6_zigbee --target upload
```

### 4. Initial WiFi setup

On first boot, WLED creates its access point (`WLED-AP`). Connect to it and configure your WiFi network through the WLED web UI. The Zigbee task starts **after** WiFi STA connects (so the AP works normally for initial setup).

### 5. Pair with your coordinator

#### Philips Hue Bridge v2

1. Open the Hue app: **Settings > Lights > Add light > Search**, or press the physical button on top of the bridge, or use the API:
   ```bash
   curl -sk -X POST https://<bridge-ip>/api/<api-key>/lights -d '{}'
   ```
2. The device should pair within ~10 seconds and appear as a "Color light".

#### Zigbee2MQTT / ZHA

1. Enable permit-join on your coordinator.
2. The device should appear as an HA Color Dimmable Light.

## How It Works

### Architecture

```
                    Hue -> WLED (Zigbee ZCL commands)
                    ─────────────────────────────────

Zigbee task (FreeRTOS)                 WLED main loop
  esp_zb_stack_main_loop()               loop()
    |                                      |
    v                                      v
  Raw ZCL command handler ----mutex---> stateChanged flag
  (parses on/off, level,                   |
   move_to_color, off_with_effect,         v
   hue/sat, manufacturer scenes)        applyState()
                                        -> colPri[], bri
                                        -> colorUpdated(CALL_MODE_ZIGBEE)


                    WLED -> Hue (HTTP API push)
                    ───────────────────────────

  WLED state change (web UI, MQTT, button, etc.)
    |
    v
  onStateChange(mode)
    |  (skip if mode == CALL_MODE_ZIGBEE to prevent echo)
    |
    +---> prepareReport()          Zigbee attribute reports
    |     -> sendPendingReports()  (via esp_zb_lock + binding table)
    |
    +---> hueHttpPending = true    Hue bridge HTTP API sync
          -> sendHueHttpSync()     (HTTPS PUT to /api/.../lights/.../state)
          -> echo suppression      (1.5s window ignores Hue echo-back)
```

### Startup sequence

1. **`setup()`** -- enables WiFi/802.15.4 coexistence (`esp_coex_wifi_i154_enable()`), sets 802.15.4 coex priority to MIDDLE, creates the mutex, configures the Zigbee platform. Does NOT start the Zigbee task yet.
2. **`connected()`** -- called when WiFi STA gets an IP. Creates the Zigbee FreeRTOS task.
3. **`zigbeeTask()`** -- initializes the Zigbee stack as an End Device with distributed security, registers clusters and the raw command handler, configures ED polling (1s long poll + turbo poll retry), starts network steering.
4. **`loop()`** -- checks the `stateChanged` flag and applies Zigbee state to WLED. Sends pending Zigbee attribute reports and Hue HTTP sync requests.

### Bidirectional state sync

**Zigbee -> WLED (Hue commands):** The raw ZCL command handler intercepts incoming commands (on/off, off_with_effect, level, move_to_color, move_to_hue/saturation), parses payloads directly, and sets shared state variables under a mutex. The WLED `loop()` picks up changes and calls `colorUpdated(CALL_MODE_ZIGBEE)`.

**WLED -> Zigbee (attribute reports):** `onStateChange()` converts the current WLED RGB to CIE XY and stores a pending report. `loop()` calls `sendPendingReports()` which acquires the Zigbee lock, updates the ZCL attribute cache, and sends attribute reports via APS bindings.

**WLED -> Hue (HTTP API push):** The Hue bridge ignores unsolicited Zigbee attribute reports from lights (it tracks state from the commands it sends). To work around this, `onStateChange()` also queues an HTTP sync. `loop()` calls `sendHueHttpSync()` which makes an HTTPS PUT request to the Hue bridge REST API (`/api/{key}/lights/{id}/state`). This requires configuring `hueBridgeIp`, `hueApiKey`, and `hueLightId` in the usermod settings.

**Echo suppression:** When WLED pushes state to Hue via HTTP, the bridge echoes it back as Zigbee commands within ~200-500ms. A 1.5-second suppression window after each HTTP send causes incoming Zigbee commands to be ignored if they arrive within that window, preventing the echo from overwriting WLED's state.

`CALL_MODE_ZIGBEE` (value 13, defined in `wled00/const.h`) prevents echo loops -- changes originating from Zigbee are not reported back.

### Color conversion

The Hue bridge uses CIE 1931 XY color space. The usermod provides:

- **`xyToRGB()`** -- converts ZCL XY (16-bit, 0xFEFF = 1.0) to sRGB chromaticity, using the standard D65 matrix and sRGB gamma curve. The result is normalized so the maximum channel reaches 255 (preserving chromaticity). Brightness is NOT baked in -- WLED's global `bri` handles dimming separately.
- **`rgbToXY()`** -- converts sRGB to CIE XY for reporting state back to the coordinator.

Hue/Saturation mode is also supported for coordinators that use it.

## Philips Hue Bridge v2 -- Critical Pairing Requirements

Pairing with the Hue bridge requires several non-obvious fixes that took significant debugging to discover. Without **all** of these, the device will not join or will be rejected.

### 1. Distributed security (the main blocker)

The Hue bridge uses **distributed security**, NOT the centralized Trust Center model that most Zigbee documentation describes. Without enabling distributed join support, the device tries centralized TC key exchange, causing `"Have not got nwk key - authentication failed"`.

```c
esp_zb_enable_joining_to_distributed(true);
```

You also need the **ZLL (Zigbee Light Link) distributed link key**:

```c
uint8_t zll_key[] = {
    0x81, 0x42, 0x86, 0x86, 0x5D, 0xC1, 0xC8, 0xB2,
    0xC8, 0xCB, 0xC5, 0x2E, 0x5D, 0x65, 0xD1, 0xB8
};
esp_zb_secur_TC_standard_distributed_key_set(zll_key);
```

### 2. Minimum join LQI = 0

The ESP32-C6 DevKitC-1 PCB antenna produces very low LQI values (0-30) even at 1m. The ZBOSS stack's default minimum LQI threshold silently rejects all beacons as "low lqi":

```c
esp_zb_secur_network_min_join_lqi_set(0);
```

### 3. app_device_version = 1

The Hue bridge requires `app_device_version = 1` in the endpoint descriptor. The default helper functions set this to 0:

```c
esp_zb_endpoint_config_t endpoint_config = {
    .endpoint = ZIGBEE_RGB_LIGHT_ENDPOINT,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_COLOR_DIMMABLE_LIGHT_DEVICE_ID,
    .app_device_version = 1,
};
```

### 4. End Device mode

The Hue bridge **rejects Router joins** with ZDO Leave. Must use `ESP_ZB_DEVICE_TYPE_ED`.

### 5. Extra on/off attributes

The Hue bridge expects `global_scene_control`, `on_time`, and `off_wait_time` attributes in the On/Off cluster.

### 6. "Off with effect" command handling

The Hue bridge sends `off_with_effect` (cmd 0x40) instead of plain `off` (cmd 0x00). ZBOSS silently ignores this command ([issue #519](https://github.com/espressif/esp-zigbee-sdk/issues/519)). The raw command handler intercepts it, updates the attribute cache, and sends the response.

### 7. Manufacturer-specific scene commands

The Hue bridge sends manufacturer-specific scene commands (manufacturer code `0x100b`) that crash ZBOSS with an assertion failure ([issue #681](https://github.com/espressif/esp-zigbee-sdk/issues/681)). The raw command handler intercepts these and responds with FAIL status.

### References

- [espressif/esp-zigbee-sdk#358](https://github.com/espressif/esp-zigbee-sdk/issues/358) -- distributed security config
- [espressif/esp-zigbee-sdk#519](https://github.com/espressif/esp-zigbee-sdk/issues/519) -- off_with_effect ignored
- [espressif/esp-zigbee-sdk#681](https://github.com/espressif/esp-zigbee-sdk/issues/681) -- Hue scene crash

## Configuration

The usermod adds a **ZigbeeRGBLight** section to the WLED settings:

```json
{
  "ZigbeeRGBLight": {
    "enabled": true,
    "hueBridgeIp": "192.168.178.216",
    "hueApiKey": "your-hue-api-key-here",
    "hueLightId": "23"
  }
}
```

| Field | Description |
|---|---|
| `enabled` | Enable/disable the usermod |
| `hueBridgeIp` | Philips Hue Bridge IP address (for WLED->Hue HTTP sync) |
| `hueApiKey` | Hue Bridge REST API key ([how to obtain](https://developers.meethue.com/develop/get-started-2/)) |
| `hueLightId` | Light ID on the Hue bridge (visible in the API at `/api/{key}/lights`) |

When all three Hue fields are set, WLED->Hue HTTP sync is automatically enabled. Without them, only Hue->WLED (Zigbee) direction works.

### Compile-time defines

| Define | Default | Description |
|---|---|---|
| `ZIGBEE_RGB_LIGHT_ENDPOINT` | `10` | Zigbee endpoint number |
| `ZIGBEE_TASK_STACK_SIZE` | `16384` | FreeRTOS task stack size (bytes) |
| `ZIGBEE_TASK_PRIORITY` | `5` | FreeRTOS task priority |

## Troubleshooting

### Device doesn't pair

- **Hue bridge**: Ensure you're using the Zigbee pairing flow (app > Lights > Add light > Search), NOT the Matter/QR code flow.
- **Check serial output**: Use `CORE_DEBUG_LEVEL=3` to see ESP_LOGI messages from the Zigbee stack.
- **Erase Zigbee NVRAM**: If the device has stale network state from a previous coordinator, erase the `zb_storage` partition:
  ```bash
  python3 -m esptool --chip esp32c6 --port /dev/ttyACM0 erase_region 0x65B000 0x4000
  ```
- **"BDB init failed, retrying..."**: Normal when Zigbee starts after WiFi. The retry mechanism handles it automatically (~6s delay).
- **"ZDO Config Ready, status: ESP_FAIL"**: Normal. Means no production configuration in flash (expected for dev boards).

### Serial port resets the device

The ESP32-C6-DevKitC-1 resets when the serial port is opened (DTR/RTS toggling). Use the WLED JSON API (`http://<device-ip>/json/state`) and the Hue API to verify state instead of serial monitoring.

### RMT timeout errors

If you see `rmt_tx_wait_all_done() flush timeout`, you need `-DWLED_MAX_DIGITAL_CHANNELS=0` in build flags. The 802.15.4 radio shares the RMT peripheral.

## WiFi / Zigbee Coexistence

The ESP32-C6 has separate radios for WiFi and IEEE 802.15.4. Both operate simultaneously via `esp_coex_wifi_i154_enable()`. WiFi-based features (web UI, MQTT, HTTP API, OTA) continue working alongside Zigbee.

Tips:
- Keep the Zigbee coordinator close to the device for a strong 802.15.4 link.
- Zigbee channels 15, 20, 25 have the least overlap with WiFi channels 1, 6, 11.
- Channel 26 (2480 MHz) has zero overlap with any WiFi channel.

## Integration Testing

A Python integration test script is provided at `test_zigbee_hue.py` with 25 tests covering:

- **Hue -> WLED:** on/off, brightness (full range + min), 6 named colors (red, green, blue, white, yellow, cyan, magenta), combined commands
- **WLED -> Hue:** on/off, brightness, 4 named colors (red, green, blue, white) via HTTP API sync
- **Echo suppression:** verifies WLED state persists after Hue echo-back
- **Rapid changes:** stress test with 5 colors in quick succession
- **Info fields:** verifies Zigbee debug fields appear in WLED info JSON

```bash
pip install requests
python3 usermods/zigbee_rgb_light/test_zigbee_hue.py \
    --bridge-ip 192.168.178.216 \
    --api-key XBfT0n000WWp2FV6DxcOnbhcxV5X7SFlKpB53Bix \
    --light-id 23 \
    --wled-ip 192.168.178.107
```

See `test_zigbee_hue.py --help` for all options.

## File Structure

```
usermods/zigbee_rgb_light/
  usermod_zigbee_rgb_light.h   -- Main implementation (~1770 lines)
  zigbee_rgb_light.cpp         -- Static instance + REGISTER_USERMOD()
  library.json                 -- PlatformIO library descriptor
  README.md                    -- This file
  research.md                  -- Architecture notes and pitfalls
  research-phillips-hue.md     -- Detailed Hue pairing research log
  test_zigbee_hue.py           -- Integration test script (25 tests, Hue + WLED APIs)

wled00/const.h                 -- CALL_MODE_ZIGBEE = 13 added
tools/WLED_ESP32_8MB_Zigbee.csv -- Partition table with zb_storage/zb_fct
```

## Future Work

- Color temperature cluster for white-channel LEDs
- Zigbee group addressing for multi-device scenes
- OTA firmware update via Zigbee OTA cluster
- Multiple endpoints for individual WLED segments
- Configurable Zigbee channel via usermod settings UI
- Auto-discovery of Hue bridge (mDNS / N-UPnP) and light ID
- Persistent HTTP connections or HTTP/1.1 keep-alive for lower sync latency
