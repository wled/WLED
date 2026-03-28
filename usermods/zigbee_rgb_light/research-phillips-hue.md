# Research: ESP32-C6 Zigbee Pairing with Philips Hue Bridge v2

## TL;DR -- The Solution

**Pairing an ESP32-C6 with a Philips Hue Bridge v2 requires THREE non-obvious fixes.**
Without all three, the device will never join or will join but not be recognised.

### 1. Distributed security (the main blocker)

The Hue bridge uses **distributed security**, NOT the centralized Trust Center model
that most Zigbee documentation describes. Without enabling distributed join support,
the device attempts a centralized TC key exchange that the bridge never responds to,
causing `"Have not got nwk key - authentication failed"`.

```c
esp_zb_enable_joining_to_distributed(true);
```

You also need to set the **ZLL (Zigbee Light Link) distributed link key**:

```c
static uint8_t zll_distributed_key[] = {
    0x81, 0x42, 0x86, 0x86, 0x5D, 0xC1, 0xC8, 0xB2,
    0xC8, 0xCB, 0xC5, 0x2E, 0x5D, 0x65, 0xD1, 0xB8
};
esp_zb_secur_TC_standard_distributed_key_set(zll_distributed_key);
```

### 2. Minimum join LQI = 0

The ESP32-C6 DevKitC-1 PCB antenna produces very low LQI values (0-30) even at 1m.
The ZBOSS stack's default minimum LQI threshold silently rejects all beacons from the
bridge as `"low lqi"`. Set it to zero:

```c
esp_zb_secur_network_min_join_lqi_set(0);
```

### 3. app_device_version = 1

The Hue bridge requires `app_device_version = 1` in the endpoint descriptor. The
default Espressif helper functions set this to 0. You must create the endpoint manually:

```c
esp_zb_endpoint_config_t endpoint_config = {
    .endpoint = HA_ESP_LIGHT_ENDPOINT,
    .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
    .app_device_id = ESP_ZB_HA_ON_OFF_LIGHT_DEVICE_ID,
    .app_device_version = 1,
};
esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
esp_zb_ep_list_add_ep(ep_list, cluster_list, endpoint_config);
```

### Additional requirements

- **End Device mode** (`ESP_ZB_DEVICE_TYPE_ED`): Hue bridge rejects Router joins
- **rx_on_when_idle = true**: `esp_zb_set_rx_on_when_idle(true)` (mains-powered light)
- **Extra on_off attributes**: Add `global_scene_control`, `on_time`, `off_wait_time`
  to the on/off cluster for Hue compatibility
- **NVRAM erase at start** during development: `esp_zb_nvram_erase_at_start(true)`

### Result

With these fixes, the ESP32-C6 joins the Hue bridge network in ~3 seconds, appears as
a controllable light in the Hue app, and responds to on/off commands via the Hue API.
Confirmed working with esp-zigbee-lib v1.6.8 on ESP-IDF v5.5.2.

### Key references

- GitHub issue [#358](https://github.com/espressif/esp-zigbee-sdk/issues/358) --
  confirmed working distributed security config from users `wejn` and `m4nu-el`
- GitHub issue [#519](https://github.com/espressif/esp-zigbee-sdk/issues/519) --
  `Off with effect` ZCL command silently ignored by ZBOSS
- GitHub issue [#681](https://github.com/espressif/esp-zigbee-sdk/issues/681) --
  manufacturer-specific scene commands from Hue can crash ZBOSS

---

## Overview

This document tracks all attempts to pair an ESP32-C6 (DevKitC-1, PCB antenna) as a
Zigbee light with a **Philips Hue Bridge v2 (square model)**. The bridge operates on
**Zigbee channel 25** and is approximately **1 metre** from the device during testing.

The goal is to have the ESP32-C6 join the Hue bridge's Zigbee network as an HA Color
Dimmable Light, with full bidirectional state sync between WLED and Zigbee.

**Current status: PAIRING WORKS. The device joins the bridge, is discovered as a light,
and responds to on/off commands. See TL;DR above for the solution.**

---

## Hardware & Environment

| Item | Detail |
|------|--------|
| Device | ESP32-C6-DevKitC-1 (bare devkit, PCB antenna) |
| Serial port | `/dev/ttyACM0` |
| Coordinator | Philips Hue Bridge v2 (square) |
| Zigbee channel | 25 |
| Distance | ~1 metre |
| ESP-IDF version (standalone) | v5.5.2, installed at `~/esp/esp-idf/` |
| PlatformIO Arduino framework | pioarduino platform 55.03.37 |
| Device EUI64 | `40:4C:CA:FF:FE:57:2A:08` |

---

## How to Trigger Pairing on the Hue Bridge

There are two confirmed methods to open permit-join on the Hue bridge:

1. **Physical button**: Press the button on top of the Hue bridge. Opens permit-join
   for ~30 seconds.
2. **Hue app**: Settings > Lights > Add light > Search. Opens permit-join for ~40-60
   seconds.

**Important**: The "6-character setup code / QR code" screen in the Hue app is for
**Matter**, not Zigbee. Do not use it.

Both methods are confirmed working -- the ESP32-C6 observes `ZDO Leave` signals (i.e.
the bridge sees the device and actively responds) during the permit-join window, and
plain `ESP_FAIL` with no `ZDO Leave` after the window closes.

---

## Test Environments

### 1. Standalone IDF test project (`~/esp/zigbee_test/`)

A minimal Zigbee on/off light based on the Espressif `HA_on_off_light` example. No
WiFi, no WLED, no complexity. Purpose: isolate whether the pairing failure is
hardware/protocol-level or WLED-specific.

**Result: Same failure as WLED. Proves the problem is NOT WiFi and NOT WLED-specific.**

Files:
- `~/esp/zigbee_test/main/esp_zb_light.c` -- Main source
- `~/esp/zigbee_test/main/esp_zb_light.h` -- Config header
- `~/esp/zigbee_test/main/light_driver.c` -- Stub LED driver (just logs)
- `~/esp/zigbee_test/sdkconfig.defaults` -- `CONFIG_ZB_ZED=y` (End Device)
- `~/esp/zigbee_test/partitions.csv` -- Partition table with `zb_storage` and `zb_fct`

Build: `idf.py -C ~/esp/zigbee_test build`
Flash: `idf.py -C ~/esp/zigbee_test -p /dev/ttyACM0 flash`
(Requires IDF tooling setup -- see commands in project notes)

### 2. WLED Zigbee usermod (`/home/will/netmindz/WLED/`)

Full WLED build with the `zigbee_rgb_light` usermod. HA Color Dimmable Light endpoint,
WiFi/802.15.4 coexistence, bidirectional state sync. Currently on hold until the
standalone test succeeds.

Build: `pio run -e esp32c6_zigbee`
Flash: `pio run -e esp32c6_zigbee --target upload`

### Serial monitoring

`pio device monitor` does not work reliably. Use:
```bash
python3 -c "
import serial, time
s = serial.Serial('/dev/ttyACM0', 115200, timeout=0.5)
s.dtr = False
s.rts = False
end = time.time() + 90
while time.time() < end:
    data = s.read(512)
    if data:
        print(data.decode('utf-8', errors='replace'), end='', flush=True)
s.close()
"
```

---

## What Has Been Tried (and Failed)

### 1. Router mode (ESP_ZB_DEVICE_TYPE_ROUTER)

**Rationale**: Zigbee lights should normally be routers (mains-powered, always on,
extend the mesh).

**Result**: Bridge rejects with `ZDO Leave` every attempt. The bridge sees the device
(it responds with Leave during the permit-join window) but actively kicks it out.

**Tested in**: Both WLED usermod and standalone IDF test.

### 2. End Device mode (ESP_ZB_DEVICE_TYPE_ED)

**Rationale**: The official Espressif HA_on_off_light example defaults to End Device
(`CONFIG_ZB_ZED=y`). Some coordinators (particularly Hue) may have stricter validation
for Router joins.

**Result**: Same failure -- `ZDO Leave` rejection. No difference from Router mode.

**Tested in**: Standalone IDF test. WLED usermod still has Router mode but should be
switched to ED once pairing works.

### 3. Explicit HA Trust Center link key (`ZigBeeAlliance09`)

**Rationale**: The well-known HA Trust Center link key is
`{0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39}`
("ZigBeeAlliance09"). The ZBOSS stack may or may not provide this automatically.
Setting it explicitly via `esp_zb_secur_TC_standard_preconfigure_key_set()` ensures the
device offers the correct key during join.

**API**: `esp_zb_secur_TC_standard_preconfigure_key_set(uint8_t *key)` -- called after
`esp_zb_init()` but before `esp_zb_start()`.

**Result**: No change. Bridge still rejects with `ZDO Leave`.

**Tested in**: Standalone IDF test (End Device mode).

### 4. Disabling TC link key exchange requirement

**Rationale**: The Zigbee 3.0 BDB process includes a Trust Center Link Key Exchange
(TCLKE) step after association. If the Hue bridge expects the device to just use the
pre-configured key without doing the exchange handshake, the exchange attempt itself
might be what triggers the rejection.

**API**: `esp_zb_secur_link_key_exchange_required_set(false)` -- called after
`esp_zb_init()` but before `esp_zb_start()`.

**Result**: Not yet conclusive -- built and flashed but monitoring was interrupted.
Needs re-test.

**Tested in**: Standalone IDF test (End Device mode, with TC key also set).

### 5. All channels scan (ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK)

**Background**: Initially the channel mask was wrong -- using `(1 << (channel - 11))`
instead of `(1 << channel)`. The Zigbee channel mask uses absolute bit positions (bit
11 = channel 11, bit 26 = channel 26). Now using
`ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK` (`0x07FFF800`, all channels 11-26).

**Result**: The device does find the bridge (evidenced by the `ZDO Leave` responses),
so the channel scanning is working. The problem is post-discovery.

### 6. NVRAM erase at start

**API**: `esp_zb_nvram_erase_at_start(true)` -- called before `esp_zb_init()`.

**Purpose**: Ensures a clean join state every boot. No stale network keys, no cached
credentials from previous failed attempts. Should be removed once pairing works.

**Result**: Necessary for testing (prevents "already commissioned" false positives) but
doesn't fix the pairing rejection.

### 7. Install code approach (abandoned)

**Rationale**: Some Zigbee 3.0 networks require install codes for joining.

**Result**: `esp_zb_secur_ic_set()` returns `ESP_FAIL` on this device. Install codes
require a pre-programmed production config in flash (via Espressif's mfg_tool). The
Hue bridge uses the standard HA Trust Center link key for consumer devices, not install
codes. Entire approach was wrong and was removed.

### 8. WiFi coexistence (WLED-specific)

**Approach in WLED usermod**: 
- `esp_coex_wifi_i154_enable()` called in `setup()` before WiFi initialises
- WiFi is killed during pairing (loop disables WiFi while `zbPaired == false`)
- WiFi restored after successful join

**Result**: WiFi coexistence was not the issue -- the standalone test has no WiFi at
all and exhibits the same failure.

### 9. ZCL version fix

**Bug found**: The ZCL version was being overridden to 1 (ancient HA profile). Fixed
to use the default (8 = Zigbee 3.0).

**Result**: Necessary fix but didn't resolve the pairing rejection.

### 10. Color capabilities

Set to `0x0009` (HS + XY). The Hue bridge primarily uses XY colour space. This is
correct for a colour dimmable light but irrelevant to the join failure (join happens
before ZCL attribute interrogation).

---

## What Has Been Ruled Out

| Possibility | How ruled out |
|-------------|---------------|
| WiFi interference | Standalone test has no WiFi; same failure |
| WLED-specific bug | Standalone test has no WLED; same failure |
| Wrong channel | Device finds bridge (ZDO Leave proves radio contact) |
| Distance/RF | 1m distance, ZDO Leave responses confirm good RF |
| Wrong endpoint type | Tested with simple on/off light (official example); same failure |
| Install codes required | Hue bridge uses standard HA TC link key for consumer devices |
| Stale NVRAM state | NVRAM erased every boot |
| Wrong ZCL version | Fixed to default (8 / Zigbee 3.0) |
| Channel mask bug | Fixed; all channels scanned; bridge found successfully |

---

## Key Observations

### The `ZDO Leave` pattern

The most important clue is the `ZDO Leave (0x3), status: ESP_OK` signal:

1. When permit-join is **open** (bridge button pressed or app searching): The device
   gets `ZDO Leave` responses every ~3-4 seconds, followed by steering failure. This
   means the bridge **sees the association request**, lets the device partially through,
   then **actively kicks it out**.

2. When permit-join is **closed**: The device gets plain `ESP_FAIL` steering failures
   with no `ZDO Leave`. This means the bridge either ignores the request or doesn't
   respond at all.

3. The transition from ZDO Leave (during permit-join) to plain ESP_FAIL (after
   permit-join closes) is clearly visible in the serial output and correlates with the
   bridge's permit-join timer.

### What `ZDO Leave` likely means

The `ZDO Leave` is being sent by the bridge's Trust Center. The most likely failure
point is during the **security key negotiation** that happens immediately after
802.15.4 association:

1. Device sends Association Request
2. Bridge sends Association Response (device gets a short address)
3. Device attempts to use the HA TC link key for key transport
4. Bridge's Trust Center **rejects** the key exchange and sends `ZDO Leave`

The question is: **why** is the TC rejecting the key exchange?

---

## Untested Ideas (Priority Order)

### HIGH PRIORITY

#### A. Enable verbose ZBOSS debug logging

**Why**: We need to see exactly what happens during the security handshake. The current
log output only shows high-level signals (ZDO Leave, steering failure) but not the
underlying NWK/APS/ZDO frames being exchanged.

**How**: Set `CONFIG_ZB_DEBUG_MODE=y` in `sdkconfig.defaults` (or equivalent). May
also need `CONFIG_LOG_DEFAULT_LEVEL_DEBUG=y` or `CONFIG_LOG_MAXIMUM_LEVEL_DEBUG=y` and
raise the log level for the ZBOSS component.

**Expected outcome**: Should reveal the specific security frame that triggers the
rejection (e.g., failed key transport, wrong key type, TC policy mismatch).

#### B. Try a different coordinator (zigbee2mqtt with CC2652 USB stick)

**Why**: Would immediately confirm whether the problem is Hue-specific or a fundamental
issue with the ESP32-C6's Zigbee implementation. If the device pairs with zigbee2mqtt
but not Hue, the problem is specifically Hue's TC policy.

**How**: Set up zigbee2mqtt with a CC2652/CC2538 USB coordinator, put it in pairing
mode, and see if the ESP32-C6 joins.

#### C. Sniff the Zigbee traffic with Wireshark

**Why**: A packet capture would show the exact frame-by-frame exchange between the
ESP32-C6 and the Hue bridge. Would reveal whether the device is sending the wrong
key type, whether the TC is sending a specific rejection reason, etc.

**How**: Use a second CC2652 or nRF52840 dongle as a Zigbee sniffer (e.g., with
`zigbee2mqtt`'s sniff mode or TI's `SmartRF Packet Sniffer 2`). Set it to channel 25.
The TC link key (`ZigBeeAlliance09`) must be loaded into Wireshark to decrypt the
traffic.

#### D. Try `esp_zb_secur_network_min_join_lqi_set(0)`

**Why**: The bridge might have a minimum LQI (Link Quality Indicator) requirement. Even
at 1m, the PCB antenna on the DevKit might produce borderline LQI. Setting the
device-side minimum to 0 won't help (it's the bridge's setting that matters), but the
API also exists for coordinator-side -- worth checking if there's a way to influence
this. Actually, this probably only affects the **device's** willingness to join a
network with low LQI, not the bridge's willingness to accept. May not be relevant.

#### E. Try setting the distributed key instead of (or in addition to) TC preconfigure key

**Why**: Zigbee 3.0 supports two security modes: centralized (TC-based, which is what
Hue uses) and distributed. The `esp_zb_secur_TC_standard_distributed_key_set()` API
sets the distributed network key. It's unlikely Hue uses distributed security, but
worth a try if TC preconfigure key doesn't work.

**API**: `esp_zb_secur_TC_standard_distributed_key_set(uint8_t *key)`

### MEDIUM PRIORITY

#### F. TouchLink commissioning

**Why**: TouchLink is a close-proximity pairing mechanism (devices must be within ~10cm)
used by some Zigbee lights. Some Hue-compatible third-party lights (e.g., IKEA Tradfri,
Innr) use TouchLink. Hue bridge supports it.

**How**: Use `ESP_ZB_BDB_MODE_FINDING_N_BINDING` or specific TouchLink APIs in the
esp-zigbee-lib. The device and bridge need to be very close together.

**Risk**: More complex to implement. If standard steering doesn't work, TouchLink might
also fail for the same underlying security reason.

#### G. Match a known working device's ZCL profile exactly

**Why**: The Hue bridge is known to be picky about which devices it accepts. It might
be checking specific attributes during or immediately after join (manufacturer name,
model identifier, device type, profile ID). Examining what a known working third-party
light (e.g., IKEA Tradfri bulb) advertises and matching it exactly might help.

**How**: Capture the ZCL descriptors of a working third-party light and replicate them.
Or try using manufacturer name / model ID strings that the Hue bridge whitelist
recognises (e.g., IKEA strings). Note: Hue added support for ZLL/ZCL lights without a
whitelist in later firmware, so this might not be the issue.

#### H. Try ZLL (Zigbee Light Link) profile instead of HA (Home Automation)

**Why**: The Hue bridge was originally a ZLL coordinator, not HA. While Zigbee 3.0
unified the profiles, the bridge might still prefer or require ZLL profile ID
(`0xC05E`) for lights instead of HA profile ID (`0x0104`).

**How**: Change the endpoint profile ID from `ESP_ZB_AF_HA_PROFILE_ID` to
`0xC05E`. May also need to change the device ID to match ZLL device types (e.g.,
`0x0200` for color light).

**Risk**: The esp-zigbee-lib helpers create HA-profile endpoints. Creating a ZLL
endpoint would require manual cluster construction.

#### I. Check if the Hue bridge requires a specific `node_descriptor` or device type

**Why**: The Hue bridge might filter on the Node Descriptor (logical type, manufacturer
code, etc.) or the Simple Descriptor (device ID, profile ID) during or immediately
after join.

**How**: Check the esp-zigbee-lib APIs for setting the node descriptor / manufacturer
code. Some reports suggest Hue requires specific Zigbee Alliance manufacturer codes.

### LOW PRIORITY / LONG SHOTS

#### J. Try an older esp-zigbee-lib version

**Why**: Regression in the ZBOSS stack. The ESP-IDF v5.5.2 esp-zigbee-lib might have
a bug in the security handshake that wasn't present in earlier versions.

**How**: Pin to an older version of `espressif/esp-zigbee-lib` in `idf_component.yml`.

#### K. Try ESP-IDF v5.3 or v5.4

**Why**: Same as above -- the Zigbee stack or radio driver might have a regression
in v5.5.2.

#### L. File an issue on espressif/esp-zigbee-sdk

**Why**: If all else fails, this might be a known issue or an Espressif engineer might
be able to identify the problem from the debug logs.

**Repository**: https://github.com/espressif/esp-zigbee-sdk

#### M. Try with `esp_zb_secur_network_security_enable(false)`

**Why**: Extremely unlikely to work (Hue requires security), but if the ZBOSS stack is
somehow double-encrypting or malforming the security frames, disabling security as a
test would confirm whether the issue is in the security layer.

**API**: `esp_zb_secur_network_security_enable(bool enabled)` -- must be called before
joining.

#### N. Try the `esp_zb_secur_multi_TC_standard_preconfigure_key_add()` API

**Why**: There's a "multi" variant of the TC key API that adds a key to a list rather
than replacing the single key. The ZBOSS stack might handle these differently
internally. Worth trying if the single-key API doesn't work.

**API**: `esp_zb_secur_multi_TC_standard_preconfigure_key_add(uint8_t *key)` -- returns
`ESP_OK` or `ESP_FAIL`.

---

## Known Bugs Fixed During Development

### Channel mask calculation
- **Bug**: `(1 << (channel - 11))` -- wrong; Zigbee channel mask uses absolute bit
  positions
- **Fix**: Use `ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK` (scans all channels 11-26)

### ZCL version override
- **Bug**: Was forcing ZCL version to 1 (ancient HA profile)
- **Fix**: Use default (8 = Zigbee 3.0)

### EUI64 read timing
- **Bug**: `esp_read_mac(mac, ESP_MAC_IEEE802154)` returns all zeros from `setup()`
  (too early)
- **Fix**: Read EUI64 in the signal handler using `esp_zb_get_long_address()` after
  `ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START`

### Install code API failure
- **Bug**: `esp_zb_secur_ic_set()` always returns `ESP_FAIL` (no production config in
  flash)
- **Fix**: Removed install code approach entirely. Hue uses standard TC link key.

### WLED-specific: ZCZR/ZED library mismatch
- **Bug**: PlatformIO Arduino framework's prebuilt sdkconfig has `CONFIG_ZB_ZED=y`
  (headers define `ZB_ED_ROLE`), but `-DZIGBEE_MODE_ZCZR=1` in `platformio_override.ini`
  links ZCZR libraries. Compile-time headers say ED while link-time libraries say
  Router.
- **Status**: Not yet resolved. May need `-DCONFIG_ZB_ZCZR=1` in build flags if Router
  mode is needed. Currently irrelevant since standalone test (which has correct
  ZED config) also fails.

---

## WLED Usermod Architecture Notes

The WLED usermod (`usermods/zigbee_rgb_light/`) is ~747 lines and implements:

- **HA Color Dimmable Light endpoint** (on/off + level + color control clusters)
- **Bidirectional state sync**: Zigbee -> WLED via `stateChanged` flag + `applyState()`;
  WLED -> Zigbee via `onStateChange()` + `esp_zb_scheduler_alarm()` callback
- **CIE 1931 XY <-> RGB conversion** (Hue uses XY colour space)
- **Thread safety**: Mutex (`zbStateMutex`) for shared state between Zigbee task and
  WLED loop; `esp_zb_scheduler_alarm()` to run Zigbee API calls in Zigbee task context
- **Singleton pattern**: `s_zb_instance`, `s_zb_paired_flag`, `s_zb_eui64_str` for
  `extern "C"` signal handler access
- **`CALL_MODE_ZIGBEE = 13`** in `wled00/const.h` to prevent echo loops
- **WiFi kill during pairing**: Disables WiFi while Zigbee is unjoined, re-enables
  after join
- **Zigbee task started from `setup()`** (before WiFi) to give 802.15.4 radio early
  access

---

## Available Security APIs (esp_zigbee_secur.h)

For reference, the full list of security-related APIs available:

| API | Purpose | Tested? |
|-----|---------|---------|
| `esp_zb_secur_TC_standard_preconfigure_key_set()` | Set TC pre-configured key | Yes, no effect |
| `esp_zb_secur_link_key_exchange_required_set(false)` | Skip TC link key exchange | Yes, inconclusive (needs re-test) |
| `esp_zb_secur_TC_standard_distributed_key_set()` | Set distributed network key | No |
| `esp_zb_secur_network_security_enable()` | Enable/disable NWK security | No |
| `esp_zb_secur_network_key_set()` | Set NWK key directly | No |
| `esp_zb_secur_network_min_join_lqi_set()` | Set min LQI for joining | No |
| `esp_zb_secur_ic_set()` | Set install code | Yes, returns ESP_FAIL |
| `esp_zb_secur_ic_str_set()` | Set install code (string) | No |
| `esp_zb_secur_multi_TC_standard_preconfigure_key_add()` | Add TC key to list | No |
| `esp_zb_secur_multi_standard_distributed_key_add()` | Add distributed key to list | No |
| `esp_zb_secur_ic_only_enable()` | Require install codes only | No (would make things worse) |

---

## Typical Serial Output (Failure Pattern)

```
I (390) ZB_TEST_LIGHT: Set HA well-known TC link key (ZigBeeAlliance09)
I (400) main_task: Returned from app_main()
I (410) ZB_TEST_LIGHT: ZDO signal: ZDO Config Ready (0x17), status: ESP_FAIL  ← normal, no production config
I (410) ZB_TEST_LIGHT: Initialize Zigbee stack
I (410) ZB_TEST_LIGHT: EUI64: 40:4c:ca:ff:fe:57:2a:08
I (410) ZB_TEST_LIGHT: Deferred driver initialization successful
I (420) ZB_TEST_LIGHT: Device started up in  factory-reset mode
I (430) ZB_TEST_LIGHT: Start network steering

--- Bridge permit-join OPEN (button pressed or app searching) ---
I (3220) ZB_TEST_LIGHT: ZDO signal: ZDO Leave (0x3), status: ESP_OK     ← bridge saw us, kicked us out
I (3220) ZB_TEST_LIGHT: Network steering was not successful (status: ESP_FAIL)
I (7180) ZB_TEST_LIGHT: ZDO signal: ZDO Leave (0x3), status: ESP_OK
I (7180) ZB_TEST_LIGHT: Network steering was not successful (status: ESP_FAIL)
[... repeats every ~3-4 seconds while permit-join is open ...]

--- Bridge permit-join CLOSED ---
I (25770) ZB_TEST_LIGHT: Network steering was not successful (status: ESP_FAIL)  ← no ZDO Leave
I (29020) ZB_TEST_LIGHT: Network steering was not successful (status: ESP_FAIL)
[... repeats every ~3 seconds, no ZDO Leave ...]
```

The `ZDO Config Ready (0x17), status: ESP_FAIL` is normal and just means no production
configuration was found in flash (expected for bare dev boards).

---

## Timeline of Changes

1. Initial WLED usermod implementation (Router mode, WiFi coex, color dimmable light)
2. Fixed channel mask bug (was `1 << (ch-11)`, now `ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK`)
3. Fixed ZCL version (was 1, now default 8)
4. Fixed EUI64 read timing (from setup → signal handler)
5. Removed install code approach (ESP_FAIL, wrong for Hue)
6. Switched endpoint creation to `esp_zb_color_dimmable_light_ep_create()` helper
7. Added CIE XY colour support (bidirectional)
8. Moved Zigbee task to `setup()` (before WiFi)
9. Created standalone IDF test project (no WiFi, no WLED)
10. Tested standalone in Router mode → same ZDO Leave rejection
11. Switched standalone to End Device mode → same ZDO Leave rejection
12. Added explicit TC preconfigure key → same ZDO Leave rejection
13. Added `esp_zb_secur_link_key_exchange_required_set(false)` → needs re-test
14. **Next**: Enable verbose ZBOSS logging OR try alternative coordinator
