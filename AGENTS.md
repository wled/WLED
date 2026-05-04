# AGENTS.md — WLED Coding Agent Reference

WLED is C++ firmware for ESP32/ESP8266 microcontrollers controlling addressable LEDs,
with a web UI (HTML/JS/CSS). Built with PlatformIO (Arduino framework) and Node.js tooling.

See also: `.github/copilot-instructions.md`, `.github/agent-build.instructions.md`,
`docs/cpp.instructions.md`, `docs/web.instructions.md`, `docs/cicd.instructions.md`.

## Build Commands

| Command | Purpose | Timeout |
|---|---|---|
| `npm ci` | Install Node.js deps (required first) | 30s |
| `npm run build` | Build web UI into `wled00/html_*.h` / `wled00/js_*.h` | 30s |
| `npm test` | Run test suite (Node.js built-in `node --test`) | 2 min |
| `npm run dev` | Watch mode — auto-rebuilds web UI on changes | continuous |
| `pio run -e esp32dev` | Build firmware (ESP32, most common target) | 5 min |
| `pio run -e nodemcuv2` | Build firmware (ESP8266) | 5 min |

**Always run `npm ci && npm run build` before `pio run`.** The web UI build generates
required C headers for firmware compilation.

### Running a Single Test

Tests use Node.js built-in test runner (`node:test`). The single test file is
`tools/cdata-test.js`. Run it with:

```sh
npm test                   # runs all tests via `node --test`
node --test tools/cdata-test.js  # run just that file directly
```

There are no C++ unit tests. Firmware is validated by successful compilation across
target environments. Always build after code changes: `pio run -e esp32dev`.

### Common Firmware Environments

`esp32dev`, `nodemcuv2`, `esp8266_2m`, `esp32c3dev`, `esp32s3dev_8MB_opi`, `lolin_s2_mini`

### Recovery / Troubleshooting

```sh
npm run build -- -f              # force web UI rebuild
rm -f wled00/html_*.h wled00/js_*.h && npm run build  # clean + rebuild UI
pio run --target clean           # clean PlatformIO build artifacts
rm -rf node_modules && npm ci    # reinstall Node.js deps
```

## Project Structure

```
wled00/              # Main firmware source (C++)
  data/              # Web UI source (HTML/JS/CSS) — tabs for indentation
  html_*.h, js_*.h   # Auto-generated (NEVER edit or commit)
  src/               # Sub-modules: fonts, bundled dependencies (ArduinoJSON)
usermods/            # Community usermods (each has library.json + .cpp/.h)
platformio.ini       # Build configuration and environments
pio-scripts/         # PlatformIO build scripts (Python)
tools/               # Node.js build tools (cdata.js) and tests
docs/                # Coding convention docs
.github/workflows/   # CI/CD (GitHub Actions)
```

## C++ Code Style (wled00/, usermods/)

### Formatting
- **2-space indentation** (no tabs in C++ files)
- K&R brace style preferred (opening brace on same line)
- Single-statement `if` bodies may omit braces: `if (a == b) doStuff(a);`
- Space after keywords (`if (...)`, `for (...)`), no space before function parens (`doStuff(a)`)
- No enforced line-length limit

### Naming Conventions
| Kind | Convention | Examples |
|---|---|---|
| Functions, variables | camelCase | `setRandomColor()`, `effectCurrent` |
| Classes, structs | PascalCase | `BusConfig`, `UsermodTemperature` |
| Macros, constants | UPPER_CASE | `WLED_MAX_USERMODS`, `FX_MODE_STATIC` |
| Private members | _camelCase | `_type`, `_bri`, `_len` |
| Enum values | PascalCase | `PinOwner::BusDigital` |

### Includes
- Include `"wled.h"` as the primary project header
- Project headers first, then platform/Arduino, then third-party
- Platform-conditional includes wrapped in `#ifdef ARDUINO_ARCH_ESP32` / `#ifdef ESP8266`

### Types and Const
- Prefer `const &` for read-only function parameters
- Mark getter/query methods `const`; use `static` for methods not accessing instance state
- Prefer `constexpr` over `#define` for compile-time constants when possible
- Use `static_assert` over `#if ... #error`
- Use `uint_fast16_t` / `uint_fast8_t` in hot-path code

### Error Handling
- **No C++ exceptions** — some builds disable them
- Use return codes (`false`, `-1`) and global flags (`errorFlag = ERR_LOW_MEM`)
- Use early returns as guard clauses: `if (!enabled || (strip.isUpdating() && (millis() - last_time < MAX_USERMOD_DELAY))) return;`
- Debug output: `DEBUG_PRINTF()` / `DEBUG_PRINTLN()` (compiled out unless `-D WLED_DEBUG`)

### Strings and Memory
- Use `F("string")` for string constants (saves RAM on ESP8266)
- Use `PSTR()` with `DEBUG_PRINTF_P()` for format strings
- Avoid `String` in hot paths; acceptable in config/setup code
- Use `d_malloc()` (DRAM-preferred) / `p_malloc()` (PSRAM-preferred) for allocation
- No VLAs — use fixed arrays or heap allocation
- Call `reserve()` on strings/vectors to pre-allocate and avoid fragmentation

### Preprocessor / Feature Flags
- Feature toggling: `WLED_DISABLE_*` and `WLED_ENABLE_*` flags (exact names matter!)
- `WLED_DISABLE_*`: `2D`, `ADALIGHT`, `ALEXA`, `MQTT`, `OTA`, `INFRARED`, `WEBSOCKETS`, etc.
- `WLED_ENABLE_*`: `DMX`, `GIF`, `HUB75MATRIX`, `JSONLIVE`, `WEBSOCKETS`, etc.
- Platform: `ARDUINO_ARCH_ESP32`, `ESP8266`, `CONFIG_IDF_TARGET_ESP32S3`

### Comments
- `//` for inline (always space after), `/* */` for block comments
- AI-generated blocks: mark with `// AI: below section was generated by an AI` / `// AI: end`

### Math Functions
- Use `sin8_t()`, `cos8_t()` — NOT `sin8()`, `cos8()` (removed, won't compile)
- Use `sin_approx()` / `cos_approx()` instead of `sinf()` / `cosf()`
- Replace `inoise8` / `inoise16` with `perlin8` / `perlin16`

### Hot-Path Code (Pixel Pipeline)
- Use function attributes: `IRAM_ATTR`, `WLED_O2_ATTR`, `__attribute__((hot))`
- Cache class members to locals before loops
- Pre-compute invariants outside loops; use reciprocals to avoid division
- Unsigned range checks: `if ((uint_fast16_t)(pix - start) < len)`

### ESP32 Tasks
- `delay(1)` in custom FreeRTOS tasks (NOT `yield()`) — feeds IDLE watchdog
- Do not use `delay()` in effects (FX.cpp) or hot pixel path

## Web UI Code Style (wled00/data/)

- **Tab indentation** for HTML, JS, and CSS
- camelCase for JS functions/variables
- Reuse helpers from `common.js` — do not duplicate utilities
- After editing, run `npm run build` to regenerate headers
- **Never edit** `wled00/html_*.h` or `wled00/js_*.h` directly

## Usermod Pattern

Usermods live in `usermods/<name>/` with a `.cpp`, optional `.h`, `library.json`, and `readme.md`.

```cpp
class MyUsermod : public Usermod {
  private:
    bool enabled = false;
    static const char _name[];
  public:
    void setup() override { /* ... */ }
    void loop() override { /* ... */ }
    void addToConfig(JsonObject& root) override { /* ... */ }
    bool readFromConfig(JsonObject& root) override { /* ... */ }
    uint16_t getId() override { return USERMOD_ID_MYMOD; }
};
const char MyUsermod::_name[] PROGMEM = "MyUsermod";
static MyUsermod myUsermod;
REGISTER_USERMOD(myUsermod);
```

- Add usermod IDs to `wled00/const.h`
- Activate via `custom_usermods` in platformio build config
- Base new usermods on `usermods/EXAMPLE/` (never edit the example directly)
- Store repeated strings as `static const char[] PROGMEM`

## CI/CD

CI runs on every push/PR via GitHub Actions (`.github/workflows/wled-ci.yml`):
1. `npm test` (web UI build validation)
2. Firmware compilation for all default environments (~22 targets)
3. Post-link validation of usermod linkage (`validate_modules.py`)

No automated linting is configured. Match existing code style in files you edit.

## General Rules

- Repository language is English
- The `docs/` folder is for developer/contributor information (coding conventions, architecture, etc.). User documentation is maintained in the [wled/WLED-Docs](https://github.com/wled/WLED-Docs) repository.
- Never edit or commit auto-generated `wled00/html_*.h` / `wled00/js_*.h`
- When updating an existing PR, retain the original description. Only modify it to ensure technical accuracy. Add change logs after the existing description.
- No force-push on open PRs
- Changes to `platformio.ini` require maintainer approval
- Remove dead/unused code — justify or delete it
- Verify feature-flag spelling exactly (misspellings are silently ignored by preprocessor)
