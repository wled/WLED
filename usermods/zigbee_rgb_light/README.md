# Zigbee RGB Light Usermod

This usermod exposes WLED as a **Zigbee Home Automation (HA) Color Dimmable Light**, allowing any Zigbee coordinator (Zigbee2MQTT, ZHA, deCONZ, etc.) to control:

| Zigbee Cluster | Function |
|---|---|
| On/Off (0x0006) | Power on / off |
| Level Control (0x0008) | Brightness (0–254) |
| Color Control (0x0300) | Hue + Saturation → RGB via WLED's `colorHStoRGB()` |

## Hardware Requirements

* **ESP32-C6** — the only ESP32 variant with a native IEEE 802.15.4 radio required for Zigbee.
* Standard WS2812B / SK6812 / NeoPixel LED strip connected to a GPIO on the C6.

> **Note:** Classic ESP32, ESP32-S2, ESP32-S3, ESP32-C3, and ESP8266 do **not** have an 802.15.4 radio and cannot run this usermod.

## Enabling the Usermod

Add the following build flag to your PlatformIO environment:

```ini
build_flags =
  -D USERMOD_ZIGBEE_RGB_LIGHT
```

The usermod source is compiled into the firmware via `wled00/usermods_list.cpp`, which conditionally includes the header when `USERMOD_ZIGBEE_RGB_LIGHT` is defined. The entire implementation is additionally guarded by `#ifdef CONFIG_IDF_TARGET_ESP32C6`, so accidentally defining the flag for a non-C6 target is harmless (the code is simply excluded).

## WiFi / Zigbee Coexistence

The ESP32-C6 has **separate radios** for WiFi (2.4 GHz) and IEEE 802.15.4 (Zigbee). Both can operate simultaneously, so WLED's WiFi-based features (web UI, MQTT, HTTP API, etc.) continue to work alongside Zigbee.

However, because both radios share the 2.4 GHz band, you may see minor throughput reduction under heavy traffic. In practice this is rarely noticeable for LED control workloads.

### Tips

* Keep the Zigbee coordinator close to the C6 device to maintain a strong 802.15.4 link.
* If you experience connectivity issues, try separating the WiFi and Zigbee channels (Zigbee channels 15, 20, or 25 have the least overlap with WiFi channels 1, 6, and 11).

## `platformio_override.ini` Example for ESP32-C6

Create or edit `platformio_override.ini` in the project root:

```ini
; platformio_override.ini  –  Zigbee RGB Light on ESP32-C6
[env:esp32c6_zigbee]
extends = esp32c6
board = esp32-c6-devkitm-1
build_flags = ${esp32c6.build_flags}
  -D USERMOD_ZIGBEE_RGB_LIGHT
  -D WLED_RELEASE_NAME=\"ESP32C6_Zigbee\"
lib_deps = ${esp32c6.lib_deps}
  espressif/esp-zigbee-lib@~1.0
  espressif/esp-zboss-lib@~1.0
```

> **Important:** The `esp-zigbee-lib` and `esp-zboss-lib` libraries are required for the Zigbee stack. Make sure they are listed in `lib_deps`.

## Configuration

Once flashed, the usermod adds a **ZigbeeRGBLight** section to the WLED settings JSON (`cfg.json`):

```json
{
  "ZigbeeRGBLight": {
    "enabled": true
  }
}
```

You can enable or disable the Zigbee light endpoint without reflashing.

## How It Works

1. On boot, a dedicated **FreeRTOS task** is created that initialises the Zigbee stack (`esp_zb_start`) and continuously runs `esp_zb_main_loop_iteration`.
2. When the coordinator writes to the On/Off, Level Control, or Color Control cluster attributes, the Zigbee stack fires an **attribute-set callback** which caches the new values.
3. On the next iteration of the WLED main `loop()`, the cached values are applied to the strip:
   * **On/Off** → sets `bri` to 0 (off) or the last level (on).
   * **Level Control** → maps the Zigbee level (0–254) to WLED brightness.
   * **Color Control** → converts Zigbee hue (0–254) and saturation (0–254) to RGB using the existing `colorHStoRGB()` helper.

## Customisation Defines

| Define | Default | Description |
|---|---|---|
| `ZIGBEE_RGB_LIGHT_ENDPOINT` | `10` | Zigbee endpoint number for the light |
| `ZIGBEE_TASK_STACK_SIZE` | `4096` | FreeRTOS task stack size (bytes) |
| `ZIGBEE_TASK_PRIORITY` | `5` | FreeRTOS task priority |

Override them in `build_flags` if needed:

```ini
build_flags =
  -D USERMOD_ZIGBEE_RGB_LIGHT
  -D ZIGBEE_RGB_LIGHT_ENDPOINT=1
  -D ZIGBEE_TASK_STACK_SIZE=8192
```
