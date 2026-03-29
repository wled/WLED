# Matter Usermod — Development Research Notes

Hard-won discoveries from integrating Matter (Project CHIP) into WLED on an ESP32-S3.
Intended as a reference for future development, debugging, and porting work.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Build System Architecture](#build-system-architecture)
3. [WLED Usermod Registration System](#wled-usermod-registration-system)
4. [WiFi Conflict Between WLED and Matter](#wifi-conflict-between-wled-and-matter)
5. [AP Mode Safety](#ap-mode-safety)
6. [Matter Color Control](#matter-color-control)
7. [Color Sync and Feedback Loop](#color-sync-and-feedback-loop)
8. [NVS Layout and Factory Reset](#nvs-layout-and-factory-reset)
9. [Commissioning Credentials](#commissioning-credentials)
10. [PIMPL Pattern: Why It Is Required](#pimpl-pattern-why-it-is-required)
11. [WLED Info Page (addToJsonInfo)](#wled-info-page-addtojsoninfo)
12. [Known Constraints and Limits](#known-constraints-and-limits)
13. [Key File Locations](#key-file-locations)

---

## Project Overview

This usermod exposes WLED as a **Matter Extended Color Light** (device type `0x010D`) over WiFi only
(no BLE/Bluetooth required). It is built for ESP32-S3 using:

- PlatformIO environment: `esp32s3_matter_wifi`
- Framework: `arduino, espidf` (dual framework — both run simultaneously)
- Platform: `pioarduino/platform-espressif32 @ 55.03.37`
- ESP-IDF component: `espressif/esp_matter` (pulled via `idf_component.yml`)

Clusters exposed:
- `0x0006` On/Off
- `0x0008` Level Control → WLED brightness
- `0x0300` Color Control → WLED primary color (HSV + Color Temperature)

---

## Build System Architecture

### Dual framework (`arduino, espidf`)

When `framework = arduino, espidf` is specified in `platformio.ini`, PlatformIO runs **two separate
build systems simultaneously**:

1. **CMake / ESP-IDF** — used only for generating the IDF code model and compiling ESP-IDF
   components (including `esp_matter`). CMake does *not* compile Arduino or usermod code.
2. **SCons (PlatformIO's own)** — compiles all Arduino framework code, WLED source, and usermod
   libraries. This is where `build_flags` apply.

This means:
- `CMakeLists.txt` controls flags for IDF components (e.g. `gnu++20` for the Matter SDK).
- `platformio.ini` `build_flags` controls flags for WLED/Arduino code.
- You cannot use CMake `target_compile_options` to influence Arduino code, and vice versa.

### `env["BUILD_DIR"]` returns an unexpanded string

In PlatformIO `extra_scripts`, `env["BUILD_DIR"]` returns the raw SCons variable reference
(e.g. `"$BUILD_DIR"`) rather than the resolved path. **Always use `env.subst("$BUILD_DIR")`**
to get the real filesystem path.

```python
# Wrong:
build_dir = Path(env["BUILD_DIR"])       # → Path("$BUILD_DIR")

# Correct:
build_dir = Path(env.subst("$BUILD_DIR")).resolve()
```

This affected `pio-scripts/generate_embed_files.py` (all 5 cert embed operations).

### `extra_scripts` override in env-specific stanzas

If a `[env:foo]` stanza defines its own `extra_scripts =` list, it **completely replaces**
`scripts_defaults.extra_scripts` — it does not append. Any shared scripts from `[env:defaults]`
must be explicitly re-listed in the env stanza.

### `sdkconfig.esp32s3_matter_wifi`

A frozen sdkconfig (3254 lines) is committed as `sdkconfig.esp32s3_matter_wifi`. Key settings:
- `CONFIG_BT_ENABLED` is **not set** — BLE is disabled. Do not define `CONFIG_BT_ENABLED=0` in
  `build_flags` either: `#ifdef CONFIG_BT_ENABLED` checks in managed_components will match `=0`.
- `CONFIG_RENDEZVOUS_MODE_WIFI=1` enables WiFi-only commissioning.
- `CONFIG_ESP_MATTER_NVS_PART_NAME="nvs"` — Matter attribute KVS uses the default NVS partition.

### `mbedtls_sha1_shim.cpp`

ESP-IDF ≥ 5.2 disables mbedTLS SHA-1 by default (`CONFIG_MBEDTLS_SHA1_C` not set). The WLED
WebSockets implementation calls the old `mbedtls_sha1_*` API directly. A shim in
`wled00/mbedtls_sha1_shim.cpp` wraps the new `esp_sha()` API under the old symbol names.
Guard: `#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0) && !CONFIG_MBEDTLS_SHA1_C`.

---

## WLED Usermod Registration System

WLED V5 uses a **linker-section dynarray** for usermod registration — no central list file or
call site in `wled.cpp` is needed.

Each usermod `.cpp` declares a static instance and calls `REGISTER_USERMOD`:

```cpp
static MatterUsermod matter_usermod;
REGISTER_USERMOD(matter_usermod);
```

`REGISTER_USERMOD(x)` expands to `DYNARRAY_MEMBER(Usermod*, usermods, um_##x, 1) = &x`,
which places a pointer into the `.dynarray.usermods.1` linker section.
`UsermodManager` (in `wled00/um_manager.cpp`) iterates between the begin/end sentinels of
that section at runtime — no explicit registration call anywhere.

### Why `dynarray.h` must be included explicitly in this usermod

All other usermods include `wled.h` which transitively includes `fcn_declare.h` (which defines
`REGISTER_USERMOD` and includes `dynarray.h`). This usermod cannot include `wled.h` because it
also includes `esp_matter.h` — the two headers conflict via `AsyncTCP.h`.

Instead, `dynarray.h` is included directly (it is fully standalone — zero Arduino/AsyncTCP
dependencies) and `REGISTER_USERMOD` is defined locally to match `fcn_declare.h`:

```cpp
#include <dynarray.h>
#ifndef REGISTER_USERMOD
#define REGISTER_USERMOD(x) DYNARRAY_MEMBER(Usermod*, usermods, um_##x, 1) = &x
#endif
```

The `#ifndef` guard means if the macro ever becomes available through some other include path
in a future refactor, the local definition is silently skipped.

### `wled00/usermods_list.cpp`

This file exists but is **empty**. It is kept as a placeholder; the old pattern of
`#include`-ing usermod headers there and calling `UsermodManager::add()` is gone.

---

## WiFi Conflict Between WLED and Matter

### The problem

Matter manages WiFi via ESP-IDF's native `esp_netif` stack. WLED's `initConnection()` calls
`WiFi.mode(WIFI_MODE_NULL)` on every reconnect attempt. This destroys **all esp_netif interfaces**,
including the one Matter's WiFi driver owns — breaking the Matter fabric connection.

Additionally, `WLEDNetwork.isConnected()` and `Network::localIP()` query the Arduino `WiFi` object,
which reports `WIFI_MODE_NULL` (and thus "not connected") once Matter owns the interface — even
though the device is fully connected and functional at the IP layer.

### The fix: `externalWiFiManager` flag

A global flag `externalWiFiManager` (defined in `wled00/wled.h`, line ~612) gates the destructive
`WiFi.mode()` call:

```cpp
// wled00/wled.h
WLED_GLOBAL bool externalWiFiManager _INIT(false);
```

```cpp
// wled00/wled.cpp initConnection() (~line 703)
if (!externalWiFiManager) {
    WiFi.mode(WIFI_MODE_NULL);
    apActive = false;
}
```

When `externalWiFiManager = true`, `Network.cpp` bypasses the Arduino WiFi object and queries
`esp_netif` directly:

```cpp
// wled00/src/dependencies/network/Network.cpp
if (externalWiFiManager) {
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    return sta && (esp_netif_is_netif_up(sta));
}
```

The flag is set in the Matter usermod's `connected()` callback (after WLED joins WiFi), **not**
in `setup()`. Setting it in `setup()` would suppress AP mode before the user can enter WiFi
credentials on first boot.

---

## AP Mode Safety

### The problem

Calling `esp_matter::start()` unconditionally in `setup()` causes Matter's internal WiFi driver
(`chip[DL]`) to take over and destroy WLED's softAP — preventing WiFi credential entry on
first boot.

### The fix: deferred start

In `setup()`, check `isWiFiConfigured()` (defined in `wled00/network.cpp` line 349). This returns
`true` if a non-default SSID is saved:

```cpp
void MatterUsermod::setup() {
    // ... build node and endpoint ...

    if (isWiFiConfigured()) {
        externalWiFiManager = true;     // set before esp_matter::start()
        esp_matter::start(_eventCb);
        pImpl->mStarted = true;
    }
    // else: defer to connected()
}

void MatterUsermod::connected() {
    if (pImpl->mStarted || !pImpl->mNodeReady) return;
    externalWiFiManager = true;
    esp_matter::start(_eventCb);
    pImpl->mStarted = true;
}
```

`connected()` is called by WLED after successfully joining WiFi as a STA client
(`wled00/wled.cpp`). It is safe to start Matter there because the softAP is no longer needed.

---

## Matter Color Control

### `hue_saturation` feature is not added by default

`esp_matter::endpoint::extended_color_light::create()` hardcodes only `color_temperature` and
`xy` features in the color_control cluster. The `hue_saturation` feature (capability bit `0x01`)
is **never added** by default.

Without it, Matter controllers (including Google Home) do not expose RGB color controls — they
only show a color temperature slider.

### The fix: explicitly add the feature after endpoint creation

```cpp
esp_matter::cluster_t *cc = esp_matter::cluster::get(
    pImpl->mEndpoint, 0x0300 /* MATTER_CL_COLOR_CTRL */);
if (cc) {
    esp_matter::cluster::color_control::feature::hue_saturation::config_t hsCfg;
    hsCfg.current_hue        = 0;
    hsCfg.current_saturation = 0;
    esp_matter::cluster::color_control::feature::hue_saturation::add(cc, &hsCfg);
}
```

This must be done *after* `extended_color_light::create()` returns but *before*
`esp_matter::start()` is called.

### Color value ranges

| Domain       | Hue        | Saturation  | Notes                          |
|--------------|------------|-------------|--------------------------------|
| Matter       | 0 – 254    | 0 – 254     | 255 is reserved in both        |
| WLED `colorHStoRGB` | 0 – 65535 | 0 – 255 | 16-bit hue, 8-bit sat |
| WLED `colPri[0..2]` | —     | —           | RGB bytes directly             |

Conversion (Matter → WLED):
```cpp
uint16_t wledHue = (uint32_t)matterHue * 65535U / 254U;
uint8_t  wledSat = (matterSat >= 254) ? 255 : (matterSat * 255U / 254U);
```

Color temperature is passed as **mireds** (Matter `ColorTemperatureMired` attribute) directly to
WLED's `colorCTtoRGB(uint16_t mired, byte* rgb)`.

---

## Color Sync and Feedback Loop

### The problem

`esp_matter::attribute::update()` fires `_attrCb` with `POST_UPDATE` **synchronously** in the
same call stack. Without protection, `syncToMatter()` calling `attribute::update()` would
trigger `_attrCb`, which sets dirty flags, which causes `applyPending()` to overwrite WLED's
state on the next loop iteration — a feedback loop.

### The fix: `mSyncing` flag

Set `mSyncing = true` before calling any `attribute::update()` and clear it immediately after.
The `_attrCb` callback exits early when `mSyncing` is set:

```cpp
static esp_err_t _attrCb(...) {
    if (_implInstance->mSyncing) return ESP_OK;  // suppress re-entry
    // ... set dirty flags ...
}

static void syncToMatter(MatterUsermod::Impl *d) {
    d->mSyncing = true;
    esp_matter::attribute::update(...);  // triggers _attrCb → ignored
    d->mSyncing = false;
}
```

### Per-attribute dirty flags

Separate dirty flags (`mPendingOnDirty`, `mPendingBriDirty`, `mPendingHSDirty`, `mPendingCTDirty`)
ensure that `applyPending()` only applies state that actually arrived from Matter. Without this,
a brightness command from Matter would also reset the color (because the color pending value is
stale/zero).

### Thread safety model

Matter attribute callbacks run on the **Matter RTOS task** (separate from the Arduino `loop()`
task). The dirty-flag transfer model is:

1. Matter task: sets `mPendingXxx` value + `mPendingXxxDirty = true`
2. Arduino loop: reads flags, snapshots values, clears flags, applies to WLED

No mutex is used — this is safe on single-core consumption (Arduino loop runs on Core 1,
Matter task on Core 0 on ESP32-S3 under the current config). If the target is changed to
a single-core chip, a `portENTER_CRITICAL` / `portEXIT_CRITICAL` pair should be added
around the snapshot-and-clear step in `applyPending()`.

---

## NVS Layout and Factory Reset

### NVS partition

All Matter and WLED data lives in the default `nvs` partition:
- Address: `0x9000`, size: `0x5000` (per `tools/WLED_ESP32_8MB_3MB_APP.csv`)
- Partition label: `nvs` (also used by `CONFIG_ESP_MATTER_NVS_PART_NAME`)

### NVS namespaces used by Matter

| Namespace       | Contents                                              | Source                        |
|-----------------|-------------------------------------------------------|-------------------------------|
| `chip-factory`  | Serial, certs, passcode, discriminator, Spake2p data  | `ESP32Config.cpp`             |
| `chip-config`   | Commissioning state, fabric info                      | `ESP32Config.cpp`             |
| `chip-counters` | Boot counters, reboot reason                          | `ESP32Config.cpp`             |
| `CHIP_KVS`      | Fabric keys, ACLs, group keys (KeyValueStoreMgr)      | `KeyValueStoreManagerImpl.h`  |
| `esp_matter_kvs`| Attribute persistent storage                          | `esp_matter_nvs.h`            |
| `node`          | Minimum endpoint ID counter                           | `esp_matter_core.cpp`         |

WLED saves its own WiFi credentials and settings in **separate NVS namespaces** (e.g. `wled`).

### Full NVS erase (brute-force)

Erases **everything** including WLED WiFi credentials:
```bash
esptool.py --port /dev/ttyACM1 erase_region 0x9000 0x5000
```
After this, the device boots into AP mode (`WLED-AP`). Re-enter WiFi credentials via
`http://192.168.4.1` before attempting to recommission Matter.

### Selective Matter-only factory reset (implemented)

Trigger via the WLED JSON state API — no serial/flash tool required:

```bash
curl -X POST http://<device-ip>/json/state \
     -H 'Content-Type: application/json' \
     -d '{"Matter":{"factoryReset":true}}'
```

The request is received by `MatterUsermod::readFromJsonState()`, which sets
`mFactoryResetPending = true`. On the next `loop()` iteration, `matterFactoryReset()` is called.
It opens each Matter NVS namespace with the IDF `nvs_open()` / `nvs_erase_all()` / `nvs_commit()`
/ `nvs_close()` sequence, then calls `esp_restart()`. WiFi credentials (stored in the `wled`
namespace) are untouched.

After restart the device reconnects to WiFi automatically and Matter starts in un-commissioned
state, ready to pair again.

Namespaces erased by `matterFactoryReset()`:

| Namespace       | Contents erased                                       |
|-----------------|-------------------------------------------------------|
| `chip-config`   | Commissioning state, fabric info                      |
| `chip-counters` | Boot/reboot counters                                  |
| `CHIP_KVS`      | Fabric keys, ACLs, group keys                         |
| `esp_matter_kvs`| Persisted cluster attribute values                    |
| `node`          | Endpoint ID counter                                   |

`chip-factory` is deliberately **not** erased — it holds device attestation certificates and
the passcode/discriminator, which should survive recommissioning.

Implementation note: the CHIP SDK's own `ConfigurationManagerImpl::DoFactoryReset()` does the
same thing but also calls `esp_wifi_restore()` (which wipes WiFi credentials) and always
calls `esp_restart()`. We bypass it and use raw `nvs.h` calls to avoid wiping WiFi.

---

## Commissioning Credentials

### Test defaults (publicly known — change before production)

| Parameter      | Default value | Notes                                 |
|----------------|---------------|---------------------------------------|
| Passcode       | `20202021`    | 8-digit Matter setup PIN              |
| Discriminator  | `3840`        | 12-bit device discovery value         |
| Vendor ID      | `0xFFF1`      | Matter test vendor ID                 |
| Product ID     | `0x8000`      | Matter test product ID                |

Passcode and discriminator are persisted to `cfg.json` via `addToConfig()` / `readFromConfig()`.

### Manual pairing code (11-digit)

Computed per Matter Core Spec §5.1.4:

```
chunk1 (1 digit)  = discriminator >> 10
chunk2 (5 digits) = ((discriminator & 0x300) << 6) | (passcode & 0x3FFF)
chunk3 (4 digits) = (passcode >> 14) & 0x1FFF
check  (1 digit)  = Verhoeff checksum of digits 0..9
```

Example with defaults: `34970112332`

### QR payload (base-38)

Computed per Matter Core Spec §5.1.3. The 88-bit payload encodes vendor ID, product ID,
commissioning flow (0=Standard), rendezvous info (4=OnNetwork), discriminator, and passcode
into a base-38 string prefixed with `MT:`.

Example with defaults: `MT:Y.K90AFN00-W362MV6`

QR image can be rendered by any QR encoder, e.g.:
```
https://api.qrserver.com/v1/create-qr-code/?size=300x300&data=MT:Y.K90AFN00-W362MV6
```

---

## PIMPL Pattern: Why It Is Required

`esp_matter.h` and related CHIP SDK headers cannot coexist in the same translation unit as
`wled.h` because:

1. `wled.h` transitively includes `AsyncTCP.h` (via the Arduino framework).
2. `esp_matter.h` includes CHIP SDK headers that define types conflicting with Arduino types
   at the preprocessor/symbol level.
3. `ArduinoJson-v6.h` is required in both contexts but is on the library include path only
   from within WLED's source tree.

The PIMPL (`Pointer to IMPLementation`) pattern isolates the CHIP SDK to a single `.cpp` file:

- **`usermod_v2_matter.h`** — visible to `wled.h` / `usermods_list.cpp`. Contains only:
  - A minimal `Usermod` base class shim (when compiled without `wled.h`)
  - `MatterUsermod` declaration with opaque `struct Impl`
- **`usermod_v2_matter.cpp`** — compiled as a standalone PlatformIO library. Includes
  `esp_matter.h` and all CHIP headers. `wled.h` is **not** included; required globals are
  declared as `extern`.

The minimal `Usermod` shim in the header exactly mirrors the vtable order in `wled00/fcn_declare.h`.
**Keep them in sync** — any virtual method added to the real `Usermod` class must be added in
the same position in the shim, or the vtable will be wrong and the call will land on the wrong
function at runtime.

---

## WLED Info Page (`addToJsonInfo`)

The WLED info page iterates `response["u"]` — a JSON object where each key is a label and each
value is an array `[value]` or `[value, " unit"]`. HTML is rendered as-is (not escaped).

```cpp
void MatterUsermod::addToJsonInfo(JsonObject &obj) {
    JsonObject user = obj[F("u")];
    if (user.isNull()) user = obj.createNestedObject(F("u"));

    JsonArray arr = user.createNestedArray(F("My Label"));
    arr.add(F("my value"));   // plain text

    // HTML hyperlink example:
    JsonArray linkArr = user.createNestedArray(F("QR Code"));
    linkArr.add(F("<a href=\"https://...\" target=\"_blank\">link text</a>"));
}
```

Source reference: `wled00/data/index.js` line ~689 for the rendering loop.

---

## Known Constraints and Limits

### IRAM

IRAM on ESP32-S3 with this build is at (or very near) 100% utilization. If adding new code
that is placed in IRAM (ISRs, time-critical functions marked `IRAM_ATTR`), the linker will
error with `IRAM segment overflow`. Mitigation options:

- Move non-ISR code out of IRAM (remove `IRAM_ATTR` where safe to do so)
- Reduce enabled features in `platformio.ini` (e.g. disable unused usermods)
- Check `CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH=y` in sdkconfig (moves FreeRTOS to flash)

### Flash size

8 MB flash required. Partition table: `tools/WLED_ESP32_8MB_3MB_APP.csv` (3 MB app partitions).
Firmware is currently ~2.49 MB of a 3 MB limit (~83%).

### Classic ESP32 is not supported

The Matter SDK requires a **dual-core** SoC and significantly more flash than the original
ESP32 provides. Minimum tested target: ESP32-S3. ESP32-C3 and ESP32-H2 should work in principle
but have not been tested with this usermod.

### BLE is disabled

`CONFIG_BT_ENABLED` is not set in sdkconfig. WiFi-only commissioning (on-network / mDNS) is
used instead. BLE commissioning would require re-enabling Bluetooth in sdkconfig, adding the
BLE transport in sdkconfig, and significantly more flash/RAM. It would likely push the build
over the IRAM and flash limits in the current configuration.

### `Wifi state: 254` is expected

Once Matter owns the WiFi netif, the Arduino `WiFi` object reports `WIFI_MODE_NULL` (state 254).
This appears in the WLED debug info page but does not indicate a malfunction. The actual
connection state is correctly reported by querying `esp_netif` directly.

---

## Key File Locations

### Modified project files

| File | Change |
|------|--------|
| `CMakeLists.txt` | `gnu++20` compiler flag for Matter SDK headers |
| `platformio.ini` | `[env:esp32s3_matter_wifi]` full environment config |
| `wled00/wled.h` | Added `externalWiFiManager` global (~line 612) |
| `wled00/wled.cpp` | Skips `WiFi.mode(NULL)` when `externalWiFiManager` |
| `wled00/usermods_list.cpp` | Empty placeholder — registration done via `REGISTER_USERMOD` in the usermod `.cpp` |
| `wled00/mbedtls_sha1_shim.cpp` | SHA-1 compatibility shim for ESP-IDF ≥ 5.2 |
| `wled00/src/dependencies/network/Network.cpp` | `isConnected()` and `localIP()` query `esp_netif` when `externalWiFiManager` |
| `pio-scripts/generate_embed_files.py` | Fixed `env.subst()`, all 5 cert embed operations |
| `pio-scripts/validate_modules.py` | Guard for missing `WLED_MODULES` key |
| `sdkconfig.defaults` | Shared sdkconfig baseline |
| `sdkconfig.esp32s3_matter_wifi` | Full frozen sdkconfig for the matter env (3254 lines) |
| `tools/WLED_ESP32_8MB_3MB_APP.csv` | 3 MB app partition table |

### Matter usermod files

| File | Purpose |
|------|---------|
| `usermod_v2_matter.h` | PIMPL class declaration + minimal `Usermod` shim |
| `usermod_v2_matter.cpp` | Full implementation (Matter node, endpoint, callbacks, WLED sync) |
| `library.json` | PlatformIO library manifest |
| `idf_component.yml` | ESP-IDF component dependency (`espressif/esp_matter`) |
| `readme.md` | User-facing documentation |
| `research.md` | This file |

### Do not modify

| Path | Reason |
|------|--------|
| `managed_components/espressif__esp_matter/` | Reverts on next `idf.py update-dependencies` or `pio run` |
| `managed_components/` (any) | Same — treat as read-only third-party code |
