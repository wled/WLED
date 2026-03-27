# Zigbee RGB Light Usermod - Research Notes

Development notes and lessons learned for future work on the Zigbee RGB Light usermod for ESP32-C6.

## Architecture Overview

The usermod exposes WLED as a Zigbee HA Color Dimmable Light (On/Off + Level Control + Color Control clusters). It runs as a Zigbee End Device and allows pairing with coordinators such as Zigbee2MQTT or ZHA.

### Key Components

- **`usermod_zigbee_rgb_light.h`** - Main usermod class. All code is guarded by `#ifdef CONFIG_IDF_TARGET_ESP32C6`.
- **`zigbee_rgb_light.cpp`** - Static instance + `REGISTER_USERMOD()` registration.
- **`library.json`** - PlatformIO library descriptor; declares dependencies on `esp-zigbee-lib` and `esp-zboss-lib`.
- **`platformio_override.ini`** - Build environment `[env:esp32c6_zigbee]` extending `esp32c6dev_8MB`.
- **`tools/WLED_ESP32_8MB_Zigbee.csv`** - Custom partition table with `zb_storage` and `zb_fct` partitions.

### Threading Model

The Zigbee stack runs in its own FreeRTOS task (`zigbeeTask`, 8KB stack, priority 5). The WLED main loop runs on the Arduino core. Communication between the two is via shared `volatile` state variables protected by a FreeRTOS mutex (`zbStateMutex`).

```
Zigbee task (zigbeeTask)          WLED main loop (loop())
  esp_zb_stack_main_loop()          checks stateChanged flag
  -> attribute write callback       -> applyState()
  -> updates zbHue/zbSat/zbBri     -> writes to WLED colPri/bri
  -> sets stateChanged = true       -> calls stateUpdated()
```

### Startup Sequence

1. **`setup()`** - Called from `UsermodManager::setup()` at `wled.cpp:530`. Enables WiFi/802.15.4 coexistence, creates the mutex, configures the Zigbee platform. Does NOT start the Zigbee task yet.
2. **`connected()`** - Called by WLED when WiFi STA gets an IP address. Creates the Zigbee FreeRTOS task, which initialises the stack as an End Device and enters network steering.
3. **`loop()`** - Checks the `stateChanged` flag and applies Zigbee state to WLED when needed.

## Critical Pitfalls

### 1. Coexistence Init Timing

`esp_coex_wifi_i154_enable()` MUST be called:
- **After** the IDF subsystems are initialised (i.e., NOT in the constructor - static constructors run during `do_global_ctors` before IDF is ready)
- **Before** `WiFi.mode(WIFI_STA)` at `wled.cpp:543` (which triggers `esp_wifi_init()`)

The safe window is `setup()`, because `UsermodManager::setup()` runs at line 530, before `WiFi.mode()` at line 543.

If called from the constructor, you get:
```
Guru Meditation Error: Load access fault
MCAUSE: 0x00000005
MTVAL:  0x00000010  (null pointer dereference)
```

### 2. RMT Peripheral Conflict

The ESP32-C6 has only 2 RMT channels. The 802.15.4 radio shares the RMT peripheral, causing `rmt_tx_wait_all_done() flush timeout` errors if any RMT-based LED bus is active.

Solution: set `-DWLED_MAX_DIGITAL_CHANNELS=0` in `build_flags` to prevent creation of RMT/I2S LED buses. Use analog (PWM/LEDC) outputs or an external SPI LED controller instead.

The `#ifndef` guard in `wled00/const.h` allows this command-line override to take effect without redefinition warnings.

### 3. Partition Table

The Zigbee stack requires two dedicated partitions:
- `zb_storage` (0x4000 bytes, type `fat`) - Network parameters, bindings, groups
- `zb_fct` (0x1000 bytes, type `fat`) - Factory configuration test data

These are defined in `tools/WLED_ESP32_8MB_Zigbee.csv`. If you flash firmware built with a different partition layout, then flash this one, the bootloader's `otadata` may still point to the old layout. The `board_build.flash_extra_images` in `platformio_override.ini` forces the partition table and otadata to be re-flashed alongside firmware to avoid stale otadata issues.

### 4. Zigbee Channel Selection

The usermod pins Zigbee to channel 26 (2480 MHz) via `esp_zb_set_primary_network_channel_set()`. Channel 26 has zero spectral overlap with any WiFi channel (the 802.11 2.4GHz spectrum ends at ~2472 MHz). This minimises coexistence interference.

If your coordinator uses a different channel, you'll need to either:
- Reconfigure the coordinator to use channel 26, or
- Change the channel mask in `zigbeeTask()` to match your coordinator's channel

### 5. `Error: no _pixels!` Debug Spam

When `-DWLED_DEBUG` is enabled and `WLED_MAX_DIGITAL_CHANNELS=0`, the `WS2812FX::show()` function is called every frame but has no pixel buffer allocated, printing `Error: no _pixels!` via `DEBUGFX_PRINTLN` at high frequency. This is harmless (the function returns early) but floods the serial console.

Options:
- Remove `-DWLED_DEBUG` from `build_flags` (keeps `-DCORE_DEBUG_LEVEL=3` for ESP-IDF/Zigbee logs)
- Ignore it (functionally harmless)

## Build Configuration

### Required Build Flags

| Flag | Purpose |
|------|---------|
| `-DZIGBEE_MODE_ED=1` | Configures ESP-IDF to build Zigbee End Device support |
| `-DUSERMOD_ZIGBEE_RGB_LIGHT` | Enables the usermod (checked in `const.h` for `USERMOD_ID`) |
| `-DWLED_MAX_DIGITAL_CHANNELS=0` | Disables RMT LED buses to avoid peripheral conflict |
| `-DCORE_DEBUG_LEVEL=3` | Enables ESP_LOGI/ESP_LOGE output from the Zigbee stack |

### Build Unflags

The parent `esp32c6dev_8MB` environment sets `CORE_DEBUG_LEVEL=0` and a different `WLED_RELEASE_NAME`. These must be removed with `build_unflags` before our overrides take effect. Note both `-D CORE_DEBUG_LEVEL=0` (with space) and `-DCORE_DEBUG_LEVEL=0` (without space) need to be unflagged, as PlatformIO config variations may use either form.

### Platform

Currently pinned to `pioarduino/platform-espressif32` release `55.03.37`. This bundles ESP-IDF v5.5.x with the esp-zigbee-lib and esp-zboss-lib. The `platform_packages =` line is intentionally empty to avoid pulling in any extra packages that might conflict.

## Zigbee ZCL Details

### Endpoint Configuration

- Endpoint number: 10 (configurable via `ZIGBEE_RGB_LIGHT_ENDPOINT`)
- Device type: HA Color Dimmable Light (`ESP_ZB_DEFAULT_COLOR_DIMMABLE_LIGHT_CONFIG`)
- Clusters: On/Off, Level Control, Color Control (hue/saturation mode)

### Attribute Mapping

| Zigbee Cluster | Zigbee Attribute | Range | WLED Mapping |
|----------------|-----------------|-------|--------------|
| On/Off | `ON_OFF_ID` | bool | `bri = 0` when off |
| Level Control | `CURRENT_LEVEL_ID` | 0-254 | `bri` (direct) |
| Color Control | `CURRENT_HUE_ID` | 0-254 | Scaled to 0-65535, converted via `colorHStoRGB()` |
| Color Control | `CURRENT_SATURATION_ID` | 0-254 | Scaled to 0-255, converted via `colorHStoRGB()` |

### Signal Handler

`esp_zb_app_signal_handler()` is an `extern "C"` function required by the Zigbee stack. Key signals:
- `ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP` - Stack ready, begin BDB initialisation
- `ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START` / `DEVICE_REBOOT` - BDB init complete, start network steering
- `ESP_ZB_BDB_SIGNAL_STEERING` - Steering result. On failure, retries after 5 seconds via `esp_zb_scheduler_alarm()`

## Future Work

- **Color temperature support** - Add XY color mode and color temperature cluster for white-channel LEDs
- **Bidirectional state sync** - Report WLED state changes (e.g. from web UI, MQTT) back to the Zigbee coordinator via attribute reporting
- **Zigbee groups** - Support group addressing for multi-device scenes
- **OTA via Zigbee** - The Zigbee OTA upgrade cluster could allow firmware updates through the coordinator
- **Multi-endpoint** - Expose individual WLED segments as separate Zigbee endpoints
- **Coordinator/router mode** - Currently End Device only; router mode would allow the C6 to extend the Zigbee mesh
- **Configurable channel** - Expose the Zigbee channel as a usermod config option rather than hardcoding channel 26

## References

- [Espressif Zigbee SDK Programming Guide](https://docs.espressif.com/projects/esp-zigbee-sdk/en/latest/)
- [ESP-IDF WiFi/802.15.4 Coexistence](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c6/api-guides/coexist.html)
- [Zigbee HA Color Dimmable Light example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_HA_sample/HA_color_dimmable_light)
- [ESP32-C6 Zigbee Gateway example](https://github.com/espressif/esp-zigbee-sdk/tree/main/examples/esp_zigbee_gateway) (WiFi+Zigbee coexistence pattern)
