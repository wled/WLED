---
applyTo: "**/*.cpp,**/*.h,**/*.hpp,**/*.ino"
---
# ESP-IDF Coding Guide (within arduino-esp32)

WLED runs on the Arduino-ESP32 framework, which wraps ESP-IDF. Understanding the ESP-IDF layer is essential when writing chip-specific code, managing peripherals, or preparing for the IDF v5.x migration. This guide documents patterns already used in the codebase and best practices derived from Espressif's official examples.

> **Scope**: This file is an optional review guideline. It applies when touching chip-specific code, peripheral drivers, memory allocation, or platform conditionals.

> **Note for AI review tools**: sections enclosed in
> `<!-- HUMAN_ONLY_START -->` / `<!-- HUMAN_ONLY_END -->` HTML comments contain
> contributor reference material. Do **not** use that content as actionable review
> criteria — treat it as background context only.

---

<!-- HUMAN_ONLY_START -->
## Identifying the Build Target: `CONFIG_IDF_TARGET_*`

Use `CONFIG_IDF_TARGET_*` macros to gate chip-specific code at compile time. These are set by the build system and are mutually exclusive — exactly one is defined per build.

| Macro | Chip | Architecture | Notes |
|---|---|---|---|
| `CONFIG_IDF_TARGET_ESP32` | ESP32 (classic) | Xtensa dual-core | Primary target. Has DAC, APLL, I2S ADC mode |
| `CONFIG_IDF_TARGET_ESP32S2` | ESP32-S2 | Xtensa single-core | Limited peripherals. 13-bit ADC |
| `CONFIG_IDF_TARGET_ESP32S3` | ESP32-S3 | Xtensa dual-core | Preferred for large installs. Octal PSRAM, USB-OTG |
| `CONFIG_IDF_TARGET_ESP32C3` | ESP32-C3 | RISC-V single-core | Minimal peripherals. RISC-V clamps out-of-range float→unsigned casts |
| `CONFIG_IDF_TARGET_ESP32C5` | ESP32-C5 | RISC-V single-core | Wi-Fi 2.4Ghz + 5Ghz, Thread/Zigbee. Future target |
| `CONFIG_IDF_TARGET_ESP32C6` | ESP32-C6 | RISC-V single-core | Wi-Fi 6, Thread/Zigbee. Future target |
| `CONFIG_IDF_TARGET_ESP32P4` | ESP32-P4 | RISC-V dual-core | High performance. Future target |

<!-- HUMAN_ONLY_END -->
### Build-time validation
WLED validates at compile time that exactly one target is defined and that it is a supported chip (`wled.cpp` lines 39–61). Follow this pattern when adding new chip-specific branches:

<!-- HUMAN_ONLY_START -->
```cpp
#if defined(CONFIG_IDF_TARGET_ESP32)
  // classic ESP32 path
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  // S3-specific path
#elif defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32P4)
  // RISC-V common path
#else
  #warning "Untested chip — review peripheral availability"
#endif
```

<!-- HUMAN_ONLY_END -->
### Guidelines

- **Always test on the actual chip** before claiming support. Simulators and cross-compilation can hide peripheral differences.
- **Prefer `#elif` chains** over nested `#ifdef` for readability.
- **Do not use `CONFIG_IDF_TARGET_*` for feature detection.** Use `SOC_*` capability macros instead (see next section). For example, use `SOC_I2S_SUPPORTS_ADC` instead of `CONFIG_IDF_TARGET_ESP32` to check for I2S ADC support.
- When a feature must be disabled on certain chips, use explicit `static_assert()` or `#warning` directives so the build clearly reports what is missing.

---

## Hardware Capability Detection: `SOC_*` Macros

`SOC_*` macros (from `soc/soc_caps.h`) describe what the current chip supports. They are the correct way to check for peripheral features — they stay accurate when new chips are added, unlike `CONFIG_IDF_TARGET_*` checks.

<!-- HUMAN_ONLY_START -->
### Important `SOC_*` macros used in WLED

| Macro | Type | Used in | Purpose |
|---|---|---|---|
| `SOC_I2S_NUM` | `int` | `audio_source.h` | Number of I2S peripherals (1 or 2) |
| `SOC_I2S_SUPPORTS_ADC` | `bool` | `usermods/audioreactive/audio_source.h` | I2S ADC sampling mode (ESP32 only) |
| `SOC_I2S_SUPPORTS_APLL` | `bool` | `usermods/audioreactive/audio_source.h` | Audio PLL for precise sample rates |
| `SOC_I2S_SUPPORTS_PDM_RX` | `bool` | `usermods/audioreactive/audio_source.h` | PDM microphone input |
| `SOC_ADC_MAX_BITWIDTH` | `int` | `util.cpp` | ADC resolution (12 or 13 bits). Renamed to `CONFIG_SOC_ADC_RTC_MAX_BITWIDTH` in IDF v5 |
| `SOC_ADC_CHANNEL_NUM(unit)` | `int` | `pin_manager.cpp` | ADC channels per unit |
| `SOC_UART_NUM` | `int` | `dmx_input.cpp` | Number of UART peripherals |
| `SOC_DRAM_LOW` / `SOC_DRAM_HIGH` | `addr` | `util.cpp` | DRAM address boundaries for validation |

<!-- HUMAN_ONLY_END -->
### Key pitfall
`SOC_ADC_MAX_BITWIDTH` (ADC resolution 12 or 13 bits) was renamed to `CONFIG_SOC_ADC_RTC_MAX_BITWIDTH` in IDF v5.

<!-- HUMAN_ONLY_START -->
### Less commonly used but valuable

| Macro | Purpose |
|---|---|
| `SOC_RMT_TX_CANDIDATES_PER_GROUP` | Number of RMT TX channels (varies 2–8 by chip) |
| `SOC_LEDC_CHANNEL_NUM` | Number of LEDC (PWM) channels |
| `SOC_GPIO_PIN_COUNT` | Total GPIO pin count |
| `SOC_DAC_SUPPORTED` | Whether the chip has a DAC (ESP32/S2 only) |
| `SOC_SPIRAM_SUPPORTED` | Whether PSRAM interface exists |
| `SOC_CPU_CORES_NUM` | Core count (1 or 2) — useful for task pinning decisions |

<!-- HUMAN_ONLY_END -->

### Best practices

```cpp
// Good: feature-based detection
#if SOC_I2S_SUPPORTS_PDM_RX
  _config.mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
#else
  #warning "PDM microphones not supported on this chip"
#endif

// Avoid: chip-name-based detection
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3)
  // happens to be correct today, but breaks when a new chip adds PDM support
#endif
```

<!-- HUMAN_ONLY_START -->
### PSRAM capability macros

For PSRAM presence, mode, and DMA access patterns:

| Macro | Meaning |
|---|---|
| `CONFIG_SPIRAM` / `BOARD_HAS_PSRAM` | PSRAM is present in the build configuration |
| `CONFIG_SPIRAM_MODE_QUAD` | Quad-SPI PSRAM (standard, used on ESP32 classic and some S2/S3 boards) |
| `CONFIG_SPIRAM_MODE_OCT` | Octal-SPI PSRAM — 8 data lines, DTR mode. Used on ESP32-S3 with octal PSRAM (e.g. N8R8 / N16R8 modules). Reserves GPIO 33–37 for the PSRAM bus — **do not allocate these pins** when this macro is defined. `wled.cpp` uses this to gate GPIO reservation. |
| `CONFIG_SPIRAM_MODE_HEX` | Hex-SPI (16-line) PSRAM — future interface on ESP32-P4 running at up to 200 MHz. Used in `json.cpp` to report the PSRAM mode. |
| `CONFIG_SOC_PSRAM_DMA_CAPABLE` | PSRAM buffers can be used with DMA (ESP32-S3 with octal PSRAM) |
| `CONFIG_SOC_MEMSPI_FLASH_PSRAM_INDEPENDENT` | SPI flash and PSRAM on separate buses (no speed contention) |

<!-- HUMAN_ONLY_END -->

#### Detecting octal/hex flash

On ESP32-S3 modules with OPI flash (e.g. N8R8 modules where the SPI flash itself runs in Octal-PI mode), the build system sets:

| Macro | Meaning |
|---|---|
| `CONFIG_ESPTOOLPY_FLASHMODE_OPI` | Octal-PI flash mode. On S3, implies GPIO 33–37 are used by the flash/PSRAM interface — the same GPIO block as octal PSRAM. `wled.cpp` uses `CONFIG_ESPTOOLPY_FLASHMODE_OPI \|\| (CONFIG_SPIRAM_MODE_OCT && BOARD_HAS_PSRAM)` to decide whether to reserve these GPIOs. `json.cpp` uses this to report the flash mode string as `"🚀OPI"`. |
| `CONFIG_ESPTOOLPY_FLASHMODE_HEX` | Hex flash mode (ESP32-P4). Reported as `"🚀🚀HEX"` in `json.cpp`. |

**Pattern used in WLED** (from `wled.cpp`) to reserve the octal-bus GPIOs on S3:
```cpp
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #if CONFIG_ESPTOOLPY_FLASHMODE_OPI || (CONFIG_SPIRAM_MODE_OCT && defined(BOARD_HAS_PSRAM))
    // S3: GPIO 33-37 are used by the octal PSRAM/flash bus
    managed_pin_type pins[] = { {33, true}, {34, true}, {35, true}, {36, true}, {37, true} };
    pinManager.allocateMultiplePins(pins, sizeof(pins)/sizeof(managed_pin_type), PinOwner::SPI_RAM);
  #endif
#endif
```

---

## ESP-IDF Version Conditionals

<!-- HUMAN_ONLY_START -->
### Checking the IDF version

```cpp
#include <esp_idf_version.h>

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  // IDF v5+ code path
#elif ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
  // IDF v4.4+ code path
#else
  // Legacy IDF v3/v4.x path
#endif
```

### Key ESP-IDF version thresholds for WLED

| Version | What changed |
|---|---|
| **4.0.0** | Filesystem API (`SPIFFS`/`LittleFS`), GPIO driver overhaul |
| **4.2.0** | ADC/GPIO API updates; `esp_adc_cal` introduced |
| **4.4.0** | I2S driver refactored (legacy API remains); `adc_deprecated.h` headers appear for newer targets |
| **4.4.4–4.4.8** | Known I2S channel-swap regression on ESP32 (workaround in `audio_source.h`) |
| **5.0.0** | **Major breaking changes** — RMT, I2S, ADC, SPI flash APIs replaced (see migration section) |
| **5.1.0** | Matter protocol support; new `esp_flash` API stable |
| **5.3+** | arduino-esp32 v3.x compatibility; C6/P4 support |

<!-- HUMAN_ONLY_END -->
### Guidelines

- When adding a version guard, **always include a comment** explaining *what* changed and *why* the guard is needed.
- Avoid version ranges that silently break — prefer `>=` over exact version matches.
- Known regressions should use explicit range guards:
  ```cpp
  // IDF 4.4.4–4.4.8 swapped I2S left/right channels (fixed in 4.4.9)
  #if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 4)) && \
      (ESP_IDF_VERSION <= ESP_IDF_VERSION_VAL(4, 4, 8))
    #define I2S_CHANNELS_SWAPPED
  #endif
  ```

---

## Migrating from ESP-IDF v4.4.x to v5.x

The jump from IDF v4.4 (arduino-esp32 v2.x) to IDF v5.x (arduino-esp32 v3.x) is the largest API break in ESP-IDF history. This section documents the critical changes and recommended migration patterns based on the upstream WLED `V5-C6` branch (`https://github.com/wled/WLED/tree/V5-C6`). Note: WLED has not yet migrated to IDF v5 — these patterns prepare for the future migration.

<!-- HUMAN_ONLY_START -->
### Compiler changes

IDF v5.x ships a much newer GCC toolchain. Key versions:

| ESP-IDF | GCC | C++ default | Notes |
|---|---|---|---|
| 4.4.x (current) | **8.4.0** | C++17 (gnu++17) | Xtensa + RISC-V |
| 5.1–5.3 | **13.2** | C++20 (gnu++2b) | Significant warning changes |
| 5.4–5.5 | **14.2** | C++23 (gnu++2b) | Latest; stricter diagnostics |

Notable behavioral differences:

| Change | Impact | Action |
|---|---|---|
| Stricter `-Werror=enum-conversion` | Implicit int-to-enum casts now error | Use explicit `static_cast<>` or typed enums |
| C++20/23 features available | `consteval`, `concepts`, `std::span`, `std::expected` | Use judiciously — ESP8266 builds still require GCC 10.x with C++17 |
| `-Wdeprecated-declarations` enforced | Deprecated API calls become warnings/errors | Migrate to new APIs (see below) |
| `-Wdangling-reference` (GCC 13+) | Warns when a reference binds to a temporary that will be destroyed | Fix the lifetime issue; do not suppress the warning |
| `-fno-common` default (GCC 12+) | Duplicate tentative definitions across translation units cause linker errors | Use `extern` declarations in headers, define in exactly one `.cpp` |
| RISC-V codegen improvements | C3/C6/P4 benefit from better register allocation | No action needed — automatic |

### C++ language features: GCC 8 → GCC 14

The jump from GCC 8.4 to GCC 14.2 spans six major compiler releases. This section lists features that become available and patterns that need updating.

#### Features safe to use after migration

These work in GCC 13+/14+ but **not** in GCC 8.4. Guard with `#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)` if the code must compile on both IDF v4 and v5.

| Feature | Standard | Example | Benefit |
|---|---|---|---|
| Designated initializers (C++20) | C++20 | `gpio_config_t cfg = { .mode = GPIO_MODE_OUTPUT };` | Already used as a GNU extension in GCC 8; becomes standard and portable in C++20 |
| `[[likely]]` / `[[unlikely]]` | C++20 | `if (err != ESP_OK) [[unlikely]] { ... }` | Hints for branch prediction; useful in hot paths |
| `[[nodiscard("reason")]]` | C++20 | `[[nodiscard("leak if ignored")]] void* allocBuffer();` | Enforces checking return values — helpful for `esp_err_t` wrappers |
| `std::span<T>` | C++20 | `void process(std::span<uint8_t> buf)` | Safe, non-owning view of contiguous memory — replaces raw pointer + length pairs |
| `consteval` | C++20 | `consteval uint32_t packColor(...)` | Guarantees compile-time evaluation; useful for color constants |
| `constinit` | C++20 | `constinit static int counter = 0;` | Prevents static initialization order fiasco |
| Concepts / `requires` | C++20 | `template<typename T> requires std::integral<T>` | Clearer constraints than SFINAE; improves error messages |
| Three-way comparison (`<=>`) | C++20 | `auto operator<=>(const Version&) const = default;` | Less boilerplate for comparable types |
| `std::bit_cast` | C++20 | `float f = std::bit_cast<float>(uint32_val);` | Type-safe reinterpretation — replaces `memcpy` or `union` tricks |
| `if consteval` | C++23 | `if consteval { /* compile-time */ } else { /* runtime */ }` | Cleaner than `std::is_constant_evaluated()` |
| `std::expected<T, E>` | C++23 | `std::expected<int, esp_err_t> readSensor()` | Monadic error handling — cleaner than returning error codes |
| `std::to_underlying` | C++23 | `auto val = std::to_underlying(myEnum);` | Replaces `static_cast<int>(myEnum)` |

#### Features already available in GCC 8 (C++17)

These work on both IDF v4.4 and v5.x — prefer them now:

| Feature | Example | Notes |
|---|---|---|
| `if constexpr` | `if constexpr (sizeof(T) == 4) { ... }` | Compile-time branching; already used in WLED |
| `std::optional<T>` | `std::optional<uint8_t> pin;` | Nullable value without sentinel values like `-1` |
| `std::string_view` | `void log(std::string_view msg)` | Non-owning, non-allocating string reference |
| Structured bindings | `auto [err, value] = readSensor();` | Useful with `std::pair` / `std::tuple` returns |
| Fold expressions | `(addSegment(args), ...);` | Variadic template expansion |
| Inline variables | `inline constexpr int MAX_PINS = 50;` | Avoids ODR issues with header-defined constants |
| `[[maybe_unused]]` | `[[maybe_unused]] int debug_only = 0;` | Suppresses unused-variable warnings cleanly |
| `[[fallthrough]]` | `case 1: doA(); [[fallthrough]]; case 2:` | Documents intentional switch fallthrough |
| Nested namespaces | `namespace wled::audio { }` | Shorter than nested `namespace` blocks |

#### Patterns that break or change behavior

| Pattern | GCC 8 behavior | GCC 14 behavior | Fix |
|---|---|---|---|
| `int x; enum E e = x;` | Warning (often ignored) | Error with `-Werror=enum-conversion` | `E e = static_cast<E>(x);` |
| `int g;` in two `.cpp` files | Both compile, linker merges (tentative definition) | Error: multiple definitions (`-fno-common`) | `extern int g;` in header, `int g;` in one `.cpp` |
| `const char* ref = std::string(...).c_str();` | Silent dangling pointer | Warning (`-Wdangling-reference`) | Extend lifetime: store the `std::string` in a local variable |
| `register int x;` | Accepted (ignored) | Warning or error (`register` removed in C++17) | Remove `register` keyword |
| Narrowing in aggregate init | Warning | Error | Use explicit cast or wider type |
| Implicit `this` capture in lambdas | Accepted in `[=]` | Deprecated warning; error in C++20 mode | Use `[=, this]` or `[&]` |

<!-- HUMAN_ONLY_END -->
#### Recommendations

- **Do not raise the minimum C++ standard yet.** WLED must still build on IDF v4.4 (GCC 8.4, C++17). Use `#if __cplusplus > 201703L` to gate C++20 features.
- **Mark intentional fallthrough** with `[[fallthrough]]` — GCC 14 warns on unmarked fallthrough by default.
<!-- HUMAN_ONLY_START -->
- **Prefer `std::optional` over sentinel values** (e.g., `-1` for "no pin") in new code — it works on both compilers.
- **Use `std::string_view`** for read-only string parameters instead of `const char*` or `const String&` — zero-copy and works on GCC 8+.
- **Avoid raw `union` type punning** — prefer `memcpy` (GCC 8) or `std::bit_cast` (GCC 13+) for strict-aliasing safety.

<!-- HUMAN_ONLY_END -->
### Deprecated and removed APIs

#### RMT (Remote Control Transceiver)

The legacy `rmt_*` functions are removed in IDF v5. Do not introduce new legacy RMT calls.

<!-- HUMAN_ONLY_START -->
The new API is channel-based:

| IDF v4 (legacy) | IDF v5 (new) | Notes |
|---|---|---|
| `rmt_config()` + `rmt_driver_install()` | `rmt_new_tx_channel()` / `rmt_new_rx_channel()` | Channels are now objects |
| `rmt_write_items()` | `rmt_transmit()` with encoder | Requires `rmt_encoder_t` |
| `rmt_set_idle_level()` | Configure in channel config | Set at creation time |
| `rmt_item32_t` | `rmt_symbol_word_t` | Different struct layout |

<!-- HUMAN_ONLY_END -->
**WLED impact**: NeoPixelBus LED output and IR receiver both use legacy RMT. The upstream `V5-C6` branch adds `-D WLED_USE_SHARED_RMT` and disables IR until the library is ported.

#### I2S (Inter-IC Sound)

Legacy `i2s_driver_install()` + `i2s_read()` API is deprecated. When touching audio source code, wrap legacy I2S init and reading in `#if ESP_IDF_VERSION_MAJOR < 5` / `#else`.

<!-- HUMAN_ONLY_START -->
 The new API uses channel handles:
| IDF v4 (legacy) | IDF v5 (new) | Notes |
|---|---|---|
| `i2s_driver_install()` | `i2s_channel_init_std_mode()` | Separate STD/PDM/TDM modes |
| `i2s_set_pin()` | Pin config in `i2s_std_gpio_config_t` | Set at init time |
| `i2s_read()` | `i2s_channel_read()` | Uses channel handle |
| `i2s_set_clk()` | `i2s_channel_reconfig_std_clk()` | Reconfigure running channel |
| `i2s_config_t` | `i2s_std_config_t` | Separate config for each mode |

**Migration pattern** (from Espressif examples):
```cpp
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  #include "driver/i2s_std.h"
  i2s_chan_handle_t rx_handle;
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  i2s_new_channel(&chan_cfg, NULL, &rx_handle);

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(22050),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = { .din = GPIO_NUM_32, .mclk = I2S_GPIO_UNUSED, ... },
  };
  i2s_channel_init_std_mode(rx_handle, &std_cfg);
  i2s_channel_enable(rx_handle);
#else
  // Legacy i2s_driver_install() path
#endif
```
<!-- HUMAN_ONLY_END -->
**WLED impact**: The audioreactive usermod (`audio_source.h`) heavily uses legacy I2S. Migration requires rewriting the `I2SSource` class for channel-based API.

<!-- HUMAN_ONLY_START -->
#### ADC (Analog-to-Digital Converter)

Legacy `adc1_get_raw()` and `esp_adc_cal_*` are deprecated:

| IDF v4 (legacy) | IDF v5 (new) | Notes |
|---|---|---|
| `adc1_config_width()` + `adc1_get_raw()` | `adc_oneshot_new_unit()` + `adc_oneshot_read()` | Object-based API |
| `esp_adc_cal_characterize()` | `adc_cali_create_scheme_*()` | Calibration is now scheme-based |
| `adc_continuous_*` (old) | `adc_continuous_*` (restructured) | Config struct changes |

#### SPI Flash

| IDF v4 (legacy) | IDF v5 (new) |
|---|---|
| `spi_flash_read()` | `esp_flash_read()` |
| `spi_flash_write()` | `esp_flash_write()` |
| `spi_flash_erase_range()` | `esp_flash_erase_region()` |

WLED already has a compatibility shim in `ota_update.cpp` that maps old names to new ones.

#### GPIO

| IDF v4 (legacy) | IDF v5 (recommended) |
|---|---|
| `gpio_pad_select_gpio()` | `esp_rom_gpio_pad_select_gpio()` (or use `gpio_config()`) |
| `gpio_set_direction()` + `gpio_set_pull_mode()` | `gpio_config()` with `gpio_config_t` struct |

### Features disabled in IDF v5 builds

The upstream `V5-C6` branch explicitly disables features with incompatible library dependencies:

```ini
# platformio.ini [esp32_idf_V5]
-D WLED_DISABLE_INFRARED       # IR library uses legacy RMT
-D WLED_DISABLE_MQTT            # AsyncMqttClient incompatible with IDF v5
-D ESP32_ARDUINO_NO_RGB_BUILTIN # Prevents RMT driver conflict with built-in LED
-D WLED_USE_SHARED_RMT          # Use new shared RMT driver for NeoPixel output
```

<!-- HUMAN_ONLY_END -->
### Migration checklist for new code

1. **Never use a removed API without a version guard.** Always provide both old and new paths, or disable the feature on IDF v5.
2. **Test on both IDF v4.4 and v5.x builds** if the code must be backward-compatible.
3. **Prefer the newer API** when writing new code — wrap the old API in an `#else` block.
4. **Mark migration TODOs** with `// TODO(idf5):` so they are easy to find later.

---

## Memory Management: `heap_caps_*` Best Practices

ESP32 has multiple memory regions with different capabilities. Using the right allocator is critical for performance and stability.

<!-- HUMAN_ONLY_START -->
### Memory regions

| Region | Flag | Speed | DMA | Size | Use for |
|---|---|---|---|---|---|
| DRAM | `MALLOC_CAP_INTERNAL \| MALLOC_CAP_8BIT` | Fast | Yes (ESP32) | 200–320 KB | Hot-path buffers, task stacks, small allocations |
| IRAM | `MALLOC_CAP_EXEC` | Fastest | No | 32–128 KB | Code (automatic via `IRAM_ATTR`) |
| PSRAM (SPIRAM) | `MALLOC_CAP_SPIRAM \| MALLOC_CAP_8BIT` | Slower | Chip-dependent | 2–16 MB | Large buffers, JSON documents, image data |
| RTC RAM | `MALLOC_CAP_RTCRAM` | Moderate | No | 8 KB | Data surviving deep sleep; small persistent buffers |

<!-- HUMAN_ONLY_END -->

### WLED allocation wrappers

WLED provides convenience wrappers with automatic fallback. **Always prefer these over raw `heap_caps_*` calls**:

| Function | Allocation preference | Use case |
|---|---|---|
| `d_malloc(size)` | RTC → DRAM → PSRAM | General-purpose; prefers fast memory |
| `d_calloc(n, size)` | Same as `d_malloc`, zero-initialized | Arrays, structs |
| `p_malloc(size)` | PSRAM → DRAM | Large buffers; prefers abundant memory |
| `p_calloc(n, size)` | Same as `p_malloc`, zero-initialized | Large arrays |
| `d_malloc_only(size)` | RTC → DRAM (no PSRAM fallback) | DMA buffers, time-critical data |

### PSRAM guidelines

- **Check availability**: always test `psramFound()` before assuming PSRAM is present.
- **DMA compatibility**: on ESP32 (classic), PSRAM buffers are **not DMA-capable** — use `d_malloc_only()` to allocate DMA buffers in DRAM only. On ESP32-S3 with octal PSRAM (`CONFIG_SPIRAM_MODE_OCT`), PSRAM buffers *can* be used with DMA when `CONFIG_SOC_PSRAM_DMA_CAPABLE` is defined.
- **JSON documents**: use the `PSRAMDynamicJsonDocument` allocator (defined in `wled.h`) to put large JSON documents in PSRAM:
  ```cpp
  PSRAMDynamicJsonDocument doc(16384);  // allocated in PSRAM if available
  ```
- **Fragmentation**: PSRAM allocations fragment less than DRAM because the region is larger. But avoid mixing small and large allocations in PSRAM — small allocations waste the MMU page granularity.
- **Heap validation**: use `d_measureHeap()` and `d_measureContiguousFreeHeap()` to monitor remaining DRAM. Allocations that would drop free DRAM below `MIN_HEAP_SIZE` should go to PSRAM instead.
- **Performance**: Keep hot-path data in DRAM. Prefer PSRAM for capacity-oriented buffers and monitor contiguous DRAM headroom.
<!-- HUMAN_ONLY_START -->
   PSRAM access is up to 15× slower than DRAM on ESP32, 3–10× slower than DRAM on ESP32-S3/-S2 with quad-SPI bus. On ESP32-S3 with octal PSRAM (`CONFIG_SPIRAM_MODE_OCT`), the penalty is smaller (~2×) because the 8-line DTR bus can transfer 8 bits in parallel at 80 MHz (120 MHz is possible with CONFIG_SPIRAM_SPEED_120M, which requires enabling experimental ESP-IDF features). On ESP32-P4 with hex PSRAM (`CONFIG_SPIRAM_MODE_HEX`), the 16-line bus runs at 200 MHz which brings it on-par with DRAM. Keep hot-path data in DRAM regardless, but consider that ESP32 often crashes when the largest DRAM chunk gets below 10 KB.
<!-- HUMAN_ONLY_END -->

<!-- HUMAN_ONLY_START -->
### Pattern: preference-based allocation

When you need a buffer that works on boards with or without PSRAM:

```cpp
// Prefer PSRAM for large buffers, fall back to DRAM
uint8_t* buf = (uint8_t*)heap_caps_malloc_prefer(bufSize, 2,
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,   // first choice: PSRAM
    MALLOC_CAP_DEFAULT);                    // fallback: any available
// Or simply:
uint8_t* buf = (uint8_t*)p_malloc(bufSize);
```

<!-- HUMAN_ONLY_END -->
---

## I2S Audio: Best Practices

The audioreactive usermod uses I2S for microphone input. Key patterns:

### Port selection

```cpp
constexpr i2s_port_t AR_I2S_PORT = I2S_NUM_0;
// I2S_NUM_1 has limitations: no MCLK routing, no ADC support, no PDM support
```

Always use `I2S_NUM_0` unless you have a specific reason and have verified support on all target chips.

### DMA buffer tuning

DMA buffer size controls latency vs. reliability:

| Scenario | `dma_buf_count` | `dma_buf_len` | Latency | Notes |
|---|---|---|---|---|
| With HUB75 matrix | 18 | 128 | ~100 ms | Higher count prevents I2S starvation during matrix DMA |
| Without PSRAM | 24 | 128 | ~140 ms | More buffers compensate for slower interrupt response |
| Default | 8 | 128 | ~46 ms | Acceptable for most setups |

### Interrupt priority

Choose interrupt priority based on coexistence with other drivers:

```cpp
#ifdef WLED_ENABLE_HUB75MATRIX
  .intr_alloc_flags = ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL1,  // level 1 (lowest) to avoid starving HUB75
#else
  .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2 | ESP_INTR_FLAG_LEVEL3,  // accept level 2 or 3 (allocator picks available)
#endif
```

### APLL (Audio PLL) usage

The ESP32 has an audio PLL for precise sample rates. Rules:

- Enable APLL when an MCLK pin is provided and precision matters.
- **Disable APLL** when Ethernet or HUB75 is active — they also use the APLL.
- APLL is broken on ESP32 revision 0 silicon.
- Not all chips have APLL — gate with `SOC_I2S_SUPPORTS_APLL`.

```cpp
#if !defined(SOC_I2S_SUPPORTS_APLL)
  _config.use_apll = false;
#elif defined(WLED_USE_ETHERNET) || defined(WLED_ENABLE_HUB75MATRIX)
  _config.use_apll = false;  // APLL conflict
#endif
```

### PDM microphone caveats

- Not supported on ESP32-C3 (`SOC_I2S_SUPPORTS_PDM_RX` not defined).
- ESP32-S3 PDM has known issues: sample rate at 50% of expected, very low amplitude.
  - **16-bit data width**: Espressif's IDF documentation states that in PDM mode the data unit width is always 16 bits, regardless of the configured `bits_per_sample`.
  - See [espressif/esp-idf#8660](https://github.com/espressif/esp-idf/issues/8660) for the upstream issue.
  - **Flag `bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT` in PDM mode** — this causes the S3 low-amplitude symptom.
- No clock pin (`I2S_CKPIN = -1`) triggers PDM mode in WLED.

---

## HUB75 LED Matrix: Best Practices

WLED uses the `ESP32-HUB75-MatrixPanel-I2S-DMA` library for HUB75 matrix output.

### Chip-specific panel limits

```cpp
#if defined(CONFIG_IDF_TARGET_ESP32S3) && defined(BOARD_HAS_PSRAM)
  maxChainLength = 6;   // S3 + PSRAM: up to 6 panels (DMA-capable PSRAM)
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  maxChainLength = 2;   // S2: limited DMA channels
#else
  maxChainLength = 4;   // Classic ESP32: default
#endif
```

### Color depth vs. pixel count

The driver dynamically reduces color depth for larger displays to stay within DMA buffer limits:

| Pixel count | Color depth | Bits per pixel |
|---|---|---|
| ≤ `MAX_PIXELS_10BIT` | 10-bit (30-bit color) | High quality (experimental) |
| ≤ `MAX_PIXELS_8BIT` | 8-bit (24-bit color) | Full quality |
| ≤ `MAX_PIXELS_6BIT` | 6-bit (18-bit color) | Slight banding |
| ≤ `MAX_PIXELS_4BIT` | 4-bit (12-bit color) | Visible banding |
| larger | 3-bit (9-bit color) | Minimal color range |

### Resource conflicts

- **APLL**: HUB75 I2S DMA uses the APLL. Disable APLL in the audio I2S driver when HUB75 is active.
- **I2S peripheral**: HUB75 uses `I2S_NUM_1` (or `I2S_NUM_0` on single-I2S chips). Audio must use the other port.
- **Pin count**: HUB75 requires 13–14 GPIO pins. On ESP32-S2 this severely limits remaining GPIO.
- **Reboot required**: on ESP32-S3, changing HUB75 driver options requires a full reboot — the I2S DMA cannot be reconfigured at runtime.

---

<!-- HUMAN_ONLY_START -->
## GPIO Best Practices

### Prefer `gpio_config()` over individual calls

```cpp
// Preferred: single struct-based configuration
gpio_config_t io_conf = {
  .pin_bit_mask = (1ULL << pin),
  .mode         = GPIO_MODE_OUTPUT,
  .pull_up_en   = GPIO_PULLUP_DISABLE,
  .pull_down_en = GPIO_PULLDOWN_DISABLE,
  .intr_type    = GPIO_INTR_DISABLE,
};
gpio_config(&io_conf);

// Avoid: multiple separate calls (more error-prone, deprecated in IDF v5)
gpio_set_direction(pin, GPIO_MODE_OUTPUT);
gpio_set_pull_mode(pin, GPIO_FLOATING);
```
<!-- HUMAN_ONLY_END -->

### Pin manager integration

Always allocate pins through WLED's `pinManager` before using GPIO APIs:

```cpp
if (!pinManager.allocatePin(myPin, true, PinOwner::UM_MyUsermod)) {
  return;  // pin in use by another module
}
// Now safe to configure
```

---

## Timer Best Practices

### Microsecond timing

For high-resolution timing, prefer `esp_timer_get_time()` (microsecond resolution, 64-bit) over `millis()` or `micros()`.
<!-- HUMAN_ONLY_START -->

```cpp
#include <esp_timer.h>
int64_t now_us = esp_timer_get_time();  // monotonic, not affected by NTP
```

> **Note**: In arduino-esp32, both `millis()` and `micros()` are thin wrappers around `esp_timer_get_time()` — they share the same monotonic clock source. Prefer the direct call when you need the full 64-bit value or ISR-safe access without truncation:
> ```cpp
> // arduino-esp32 internals (cores/esp32/esp32-hal-misc.c):
> // unsigned long micros() { return (unsigned long)(esp_timer_get_time()); }
> // unsigned long millis() { return (unsigned long)(esp_timer_get_time() / 1000ULL); }
> ```
<!-- HUMAN_ONLY_END -->
<!-- HUMAN_ONLY_START -->
### Periodic timers

For periodic tasks with sub-millisecond precision, use `esp_timer`:

```cpp
esp_timer_handle_t timer;
esp_timer_create_args_t args = {
  .callback = myCallback,
  .arg = nullptr,
  .dispatch_method = ESP_TIMER_TASK,  // run in timer task (not ISR)
  .name = "my_timer",
};
esp_timer_create(&args, &timer);
esp_timer_start_periodic(timer, 1000);  // 1 ms period
```
<!-- HUMAN_ONLY_END -->

Always prefer `ESP_TIMER_TASK` dispatch over `ESP_TIMER_ISR` unless you need ISR-level latency — ISR callbacks have severe restrictions (no logging, no heap allocation, no FreeRTOS API calls).

### Precision waiting: coarse delay then spin-poll

When waiting for a precise future deadline (e.g., FPS limiting, protocol timing), avoid spinning the entire duration — that wastes CPU and starves other tasks. Instead, yield to FreeRTOS while time allows, then spin only for the final window.
<!-- HUMAN_ONLY_START -->
```cpp
// Wait until 'target_us' (a micros() / esp_timer_get_time() timestamp)
long time_to_wait = (long)(target_us - micros());
// Coarse phase: yield to FreeRTOS while we have more than ~2 ms remaining.
// vTaskDelay(1) suspends the task for one RTOS tick, letting other task run freely.
while (time_to_wait > 2000) {
  vTaskDelay(1);
  time_to_wait = (long)(target_us - micros());
}
// Fine phase: busy-poll the last ≤2 ms for microsecond accuracy.
// micros() wraps esp_timer_get_time() so this is low-overhead.
while ((long)(target_us - micros()) > 0) { /* spin */ }
```
<!-- HUMAN_ONLY_END -->

> The threshold (2000 µs as an example) should be at least one RTOS tick (default 1 ms on ESP32) plus some margin. A value of 1500–3000 µs works well in practice.

---

## ADC Best Practices

<!-- HUMAN_ONLY_START -->
### Version-aware ADC code

ADC is one of the most fragmented APIs across IDF versions:

```cpp
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  // IDF v5: oneshot driver
  #include "esp_adc/adc_oneshot.h"
  #include "esp_adc/adc_cali.h"
  adc_oneshot_unit_handle_t adc_handle;
  adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
  adc_oneshot_new_unit(&unit_cfg, &adc_handle);
#else
  // IDF v4: legacy driver
  #include "driver/adc.h"
  #include "esp_adc_cal.h"
  adc1_config_width(ADC_WIDTH_BIT_12);
  int raw = adc1_get_raw(ADC1_CHANNEL_0);
#endif
```
<!-- HUMAN_ONLY_END -->

### Bit width portability

Not all chips have 12-bit ADC. `SOC_ADC_MAX_BITWIDTH` reports the maximum resolution (12 or 13 bits). Note that in IDF v5, this macro was renamed to `CONFIG_SOC_ADC_RTC_MAX_BITWIDTH`. Write version-aware guards:

```cpp
// IDF v4: SOC_ADC_MAX_BITWIDTH   IDF v5: CONFIG_SOC_ADC_RTC_MAX_BITWIDTH
#if defined(CONFIG_SOC_ADC_RTC_MAX_BITWIDTH)   // IDF v5+
  #define MY_ADC_MAX_BITWIDTH CONFIG_SOC_ADC_RTC_MAX_BITWIDTH
#elif defined(SOC_ADC_MAX_BITWIDTH)             // IDF v4
  #define MY_ADC_MAX_BITWIDTH SOC_ADC_MAX_BITWIDTH
#else
  #define MY_ADC_MAX_BITWIDTH 12                // safe fallback
#endif

#if MY_ADC_MAX_BITWIDTH == 13
  adc1_config_width(ADC_WIDTH_BIT_13);  // ESP32-S2
#else
  adc1_config_width(ADC_WIDTH_BIT_12);  // ESP32, S3, C3, etc.
#endif
```

WLED's `util.cpp` uses the IDF v4 form (`SOC_ADC_MAX_BITWIDTH`) — this will need updating when the codebase migrates to IDF v5.

---

<!-- HUMAN_ONLY_START -->
## RMT Best Practices

### Current usage in WLED

RMT drives NeoPixel LED output (via NeoPixelBus) and IR receiver input. Both use the legacy API that is removed in IDF v5.

### Migration notes

- The upstream `V5-C6` branch uses `-D WLED_USE_SHARED_RMT` to switch to the new RMT driver for NeoPixel output.
- IR is disabled on IDF v5 until the IR library is ported.
- New chips (C6, P4) have different RMT channel counts — use `SOC_RMT_TX_CANDIDATES_PER_GROUP` to check availability.
- The new RMT API requires an "encoder" object (`rmt_encoder_t`) to translate data formats — this is more flexible but requires more setup code.

<!-- HUMAN_ONLY_END -->
---

## Espressif Best Practices (from official examples)

### Error handling

Always check `esp_err_t` return values. Use `ESP_ERROR_CHECK()` in initialization code, but handle errors gracefully in runtime code.
<!-- HUMAN_ONLY_START -->
```cpp
// Initialization — crash early on failure
ESP_ERROR_CHECK(i2s_driver_install(I2S_NUM_0, &config, 0, nullptr));

// Runtime — log and recover
esp_err_t err = i2s_read(I2S_NUM_0, buf, len, &bytes_read, portMAX_DELAY);
if (err != ESP_OK) {
  DEBUGSR_PRINTF("I2S read failed: %s\n", esp_err_to_name(err));
  return;
}
```
<!-- HUMAN_ONLY_END -->

For situations between these two extremes — where you want the `ESP_ERROR_CHECK` formatted log message (file, line, error name) but must not abort — use `ESP_ERROR_CHECK_WITHOUT_ABORT()`.

<!-- HUMAN_ONLY_START -->
```cpp
// Logs in the same format as ESP_ERROR_CHECK, but returns the error code instead of aborting.
// Useful for non-fatal driver calls where you want visibility without crashing.
esp_err_t err = ESP_ERROR_CHECK_WITHOUT_ABORT(i2s_set_clk(AR_I2S_PORT, rate, bits, ch));
if (err != ESP_OK) return;  // handle as needed
```

<!-- HUMAN_ONLY_END -->
### Logging

WLED uses its own logging macros — **not** `ESP_LOGx()`. For application-level code, always use the WLED macros defined in `wled.h`:

| Macro family | Defined in | Controlled by | Use for |
|---|---|---|---|
| `DEBUG_PRINT` / `DEBUG_PRINTLN` / `DEBUG_PRINTF` | `wled.h` | `WLED_DEBUG` build flag | Development/diagnostic output; compiled out in release builds |

All of these wrap `Serial` output through the `DEBUGOUT` / `DEBUGOUTLN` / `DEBUGOUTF` macros.

**Exception — low-level driver code**: When writing code that interacts directly with ESP-IDF APIs (e.g., I2S initialization, RMT setup), use `ESP_LOGx()` macros instead. They support tag-based filtering and compile-time log level control:

<!-- HUMAN_ONLY_START -->
```cpp
static const char* TAG = "my_module";
ESP_LOGI(TAG, "Initialized with %d buffers", count);
ESP_LOGW(TAG, "PSRAM not available, falling back to DRAM");
ESP_LOGE(TAG, "Failed to allocate %u bytes", size);
```
<!-- HUMAN_ONLY_END -->
### Task creation and pinning

<!-- HUMAN_ONLY_START -->
On dual-core chips (ESP32, S3, P4), pin latency-sensitive tasks to a specific core:

```cpp
xTaskCreatePinnedToCore(
  audioTask,        // function
  "audio",          // name
  4096,             // stack size
  nullptr,          // parameter
  5,                // priority (higher = more important)
  &audioTaskHandle, // handle
  0                 // core ID (0 = protocol core, 1 = app core)
);
```
<!-- HUMAN_ONLY_END -->

Guidelines:
- Pin network/protocol tasks to core 0 (where Wi-Fi runs).
- Pin real-time tasks (audio, LED output) to core 1.
- On single-core chips (S2, C3, C5, C6), only core 0 exists — pinning to core 1 will fail. Use `SOC_CPU_CORES_NUM > 1` guards or `tskNO_AFFINITY`.
- Use `SOC_CPU_CORES_NUM` to conditionally pin tasks:
  ```cpp
  #if SOC_CPU_CORES_NUM > 1
    xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 5, &handle, 1);
  #else
    xTaskCreate(audioTask, "audio", 4096, nullptr, 5, &handle);
  #endif
  ```

**Tip: use xTaskCreateUniversal()** - from arduino-esp32 - to avoid the conditional on `SOC_CPU_CORES_NUM`. This function has the same signature as ``xTaskCreatePinnedToCore()``, but automatically falls back to ``xTaskCreate()`` on single-core MCUs.

### `delay()`, `yield()`, and the IDLE task

FreeRTOS on ESP32 is **preemptive** — all tasks are scheduled by priority regardless of `yield()` calls. This is fundamentally different from ESP8266 cooperative multitasking.

<!-- HUMAN_ONLY_START -->
| Call | What it does | Reaches IDLE (priority 0)? |
|---|---|---|
| `delay(ms)` / `vTaskDelay(ticks)` | Suspends calling task; scheduler runs all other ready tasks | ✅ Yes |
| `yield()` / `vTaskDelay(0)` | Hint to switch to tasks at **equal or higher** priority only | ❌ No |
| `taskYIELD()` | Same as `vTaskDelay(0)` | ❌ No |
| Blocking API (`xQueueReceive`, `ulTaskNotifyTake`, `vTaskDelayUntil`) | Suspends task until event or timeout; IDLE runs freely | ✅ Yes |

<!-- HUMAN_ONLY_END -->

**`delay()` in `loopTask` is safe.** Arduino's `loop()` runs inside `loopTask`. Calling `delay()` suspends only `loopTask` — all other FreeRTOS tasks (Wi-Fi stack, audio FFT, LED DMA) continue uninterrupted on either core.

**`yield()` does not yield to IDLE.** Any task that loops with only `yield()` calls will starve the IDLE task, causing the IDLE watchdog to fire. Always use `delay(1)` (or a blocking FreeRTOS call) in tight task loops. Note: WLED redefines `yield()` as an empty macro on ESP32 WLEDMM_FASTPATH builds.

#### Why the IDLE task is not optional
<!-- HUMAN_ONLY_START -->

The FreeRTOS IDLE task (one per core on dual-core ESP32 and ESP32-S3; single instance on single-core chips) is not idle in the casual sense — it performs essential system housekeeping:

- **Frees deleted task memory**: when a task calls `vTaskDelete()`, the IDLE task reclaims its TCB and stack. Without IDLE running, deleted tasks leak memory permanently.
- **Runs the idle hook**: when `configUSE_IDLE_HOOK = 1`, the IDLE task calls `vApplicationIdleHook()` on every iteration — some ESP-IDF components register low-priority background work here.
- **Implements tickless idle / light sleep**: on battery-powered devices, IDLE is the entry point for low-power sleep. A permanently starved IDLE task disables light sleep entirely.
- **Runs registered idle hooks**: ESP-IDF components register callbacks via `esp_register_freertos_idle_hook()` (e.g., Wi-Fi background maintenance, Bluetooth housekeeping). These only fire when IDLE runs.

<!-- HUMAN_ONLY_END -->

In short: **starving IDLE corrupts memory cleanup, breaks background activities, disables low-power sleep, and prevents Wi-Fi/BT maintenance.** The IDLE watchdog panic is a symptom — the real damage happens before the watchdog fires.

### Watchdog management

Long-running operations may trigger the task watchdog. Feed it explicitly:

```cpp
#include <esp_task_wdt.h>
esp_task_wdt_reset();  // feed the watchdog in long loops
```

For tasks that intentionally block for extended periods, consider subscribing/unsubscribing from the TWDT:

```cpp
esp_task_wdt_delete(NULL);  // remove current task from TWDT (IDF v4.4)
// ... long blocking operation ...
esp_task_wdt_add(NULL);     // re-register
```

> **IDF v5 note**: In IDF v5, `esp_task_wdt_add()` and `esp_task_wdt_delete()` require an explicit `TaskHandle_t`. Use `xTaskGetCurrentTaskHandle()` instead of `NULL`.

<!-- HUMAN_ONLY_START -->
---

## Quick Reference: IDF v4 → v5 API Mapping

| Component | IDF v4 Header | IDF v5 Header | Key Change |
|---|---|---|---|
| I2S | `driver/i2s.h` | `driver/i2s_std.h` | Channel-based API |
| ADC (oneshot) | `driver/adc.h` | `esp_adc/adc_oneshot.h` | Unit/channel handles |
| ADC (calibration) | `esp_adc_cal.h` | `esp_adc/adc_cali.h` | Scheme-based calibration |
| RMT | `driver/rmt.h` | `driver/rmt_tx.h` / `rmt_rx.h` | Encoder-based transmit |
| SPI Flash | `spi_flash.h` | `esp_flash.h` | `esp_flash_*` functions |
| GPIO | `driver/gpio.h` | `driver/gpio.h` | `gpio_pad_select_gpio()` removed |
| Timer | `driver/timer.h` | `driver/gptimer.h` | General-purpose timer handles |
| PCNT | `driver/pcnt.h` | `driver/pulse_cnt.h` | Handle-based API |
<!-- HUMAN_ONLY_END -->
