# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Working Style

Do not make any changes until you have 95% confidence in what you need to build. Ask follow-up questions until you reach that confidence.

When you are done with a change, start and end with the word 'DONE' so that I know you are finished.

When I need to update the esp32 device, you must tell me which IDs I need to run, so that I don't waste my time.
1 = npm run build
2 = & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e lolin_s2_mini --target upload 
3 = & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e lolin_s2_mini --target uploadfs

## Skills

Skills are located in `.claude/skills/`. Read the relevant skill file before starting any task that matches its description.

- `session-handoff` — end of session handoff document generator. Trigger on /session-handoff or when wrapping up a session.

## Setup and Build

- Node.js 20+ required (see `.nvmrc`). Install deps: `npm ci`
- PlatformIO required for firmware: `pip install -r requirements.txt`

| Command | Purpose | Timeout |
|---|---|---|
| `npm ci` | Install Node.js deps (required first) | 30 s |
| `npm run build` | Build web UI → `wled00/html_*.h` / `wled00/js_*.h` headers | 30 s |
| `npm test` | Run test suite (Node.js `node:test`) | 2 min |
| `npm run dev` | Watch mode — auto-rebuilds web UI on changes | continuous |
| `pio run -e esp32dev` | Build firmware for ESP32 (most common target) | 30 min |
| `pio run -e nodemcuv2` | Build firmware for ESP8266 | 30 min |

**Always run `npm ci && npm run build` before any `pio run`.** The web UI build generates required C headers that firmware compilation depends on.

### Running a single test

```sh
npm test                           # all tests
node --test tools/cdata-test.js    # single file directly
```

There are no C++ unit tests — firmware is validated by successful compilation. After any source code change, run `pio run -e esp32dev` to confirm it compiles.

### Common firmware environments

`esp32dev`, `nodemcuv2`, `esp8266_2m`, `esp32c3dev`, `esp32s3dev_8MB_opi`, `lolin_s2_mini`

### Recovery

```sh
npm run build -- -f                                   # force web UI rebuild
rm -f wled00/html_*.h wled00/js_*.h && npm run build  # clean + rebuild UI
pio run --target clean                                 # clean PlatformIO artifacts
rm -rf node_modules && npm ci                          # reinstall Node deps
```

## Architecture Overview

WLED is C++ firmware (Arduino framework via PlatformIO) for ESP32/ESP8266 microcontrollers controlling addressable LEDs, plus a web UI built separately and embedded as C headers.

```
wled00/              # Main firmware source (C++)
  data/              # Web UI source (HTML/JS/CSS) — tab-indented
  html_*.h, js_*.h   # Auto-generated headers — NEVER edit or commit
  src/               # Sub-modules: fonts, bundled dependencies (ArduinoJSON)
  wled.h             # Primary project header — include this
  const.h            # Constants, feature flag definitions, usermod IDs
  FX.h / FX.cpp      # LED effect definitions (100+ effects)
  FX_fcn.cpp         # Segment/strip management — core hot path
  bus_manager.cpp    # LED bus abstraction layer
  wled_main.cpp      # Arduino setup()/loop() entry points
  cfg.cpp            # Config read/write (settings persistence)
  json.cpp           # JSON API
  wled_server.cpp    # HTTP server / routing
usermods/            # Community add-ons (each has library.json + .cpp/.h)
platformio.ini       # Build targets — changes require maintainer approval
tools/               # Node.js build scripts (cdata.js) and tests
docs/                # Coding convention docs
.github/workflows/   # CI/CD (GitHub Actions)
```

**Pixel pipeline hot path**: `Segment → Strip (WS2812FX) → BusManager → Bus → LED driver`. Files `FX_fcn.cpp`, `FX_2Dfcn.cpp`, `bus_manager.cpp`, `colors.cpp` are performance-critical.

## C++ Code Conventions (`wled00/`, `usermods/`)

- **2-space indentation** (no tabs), K&R brace style preferred
- **camelCase** functions/variables, **PascalCase** classes/structs, **UPPER_CASE** macros
- Include `"wled.h"` as the primary header
- No C++ exceptions — use return codes (`false`/`-1`) and `DEBUG_PRINTF()` / `DEBUG_PRINTLN()` (compiled out unless `-D WLED_DEBUG`)
- Use `F("string")` for string literals (saves RAM on ESP8266); `PSTR()` with `DEBUG_PRINTF_P()` for format strings
- Use `d_malloc()` (DRAM-preferred) / `p_malloc()` (PSRAM-preferred) — no VLAs
- Mark getters `const`; use `static` for methods that don't access instance state

### Math functions — critical aliases

Old FastLED names **don't compile** in this codebase:

| ❌ Do not use | ✅ Use instead |
|---|---|
| `sin8()`, `cos8()` | `sin8_t()`, `cos8_t()` |
| `sin16()`, `cos16()` | `sin16_t()`, `cos16_t()` |
| `sinf()`, `cosf()` | `sin_approx()`, `cos_approx()` |
| `inoise8`, `inoise16` | `perlin8`, `perlin16` |

### Hot-path code

Apply function attributes in performance-critical pixel pipeline code:
```cpp
void IRAM_ATTR_YN WLED_O2_ATTR __attribute__((hot)) Segment::setPixelColor(unsigned i, uint32_t c)
```
Cache class members to locals before loops; pre-compute invariants; use unsigned range checks: `if ((uint_fast16_t)(pix - start) < len)`.

### ESP32 custom FreeRTOS tasks

Use `delay(1)` (not `yield()`) in custom task loops — `yield()` starves the IDLE task and its watchdog.

### Feature flags

Every `WLED_ENABLE_*` / `WLED_DISABLE_*` flag must exactly match one of these names — misspellings are silently ignored by the preprocessor:

- **`WLED_DISABLE_*`**: `2D`, `ADALIGHT`, `ALEXA`, `BROWNOUT_DET`, `ESPNOW`, `FILESYSTEM`, `HUESYNC`, `IMPROV_WIFISCAN`, `INFRARED`, `LOXONE`, `MQTT`, `OTA`, `PARTICLESYSTEM1D`, `PARTICLESYSTEM2D`, `PIXELFORGE`, `WEBSOCKETS`
- **`WLED_ENABLE_*`**: `ADALIGHT`, `AOTA`, `DMX`, `DMX_INPUT`, `DMX_OUTPUT`, `FS_EDITOR`, `GIF`, `HUB75MATRIX`, `JSONLIVE`, `LOXONE`, `MQTT`, `PIXART`, `PXMAGIC`, `USERMOD_PAGE`, `WEBSOCKETS`, `WPA_ENTERPRISE`

## Web UI Conventions (`wled00/data/`)

- **Tab indentation** for HTML, JS, and CSS
- camelCase for JS functions/variables; `gId()` for `getElementById()`, `d` for `document`
- Reuse helpers from `common.js` — do not duplicate utilities
- After any edit: `npm run build` to regenerate headers
- **Never edit** `wled00/html_*.h` or `wled00/js_*.h`

Manual testing: `cd wled00/data && python3 -m http.server 8080` then open `http://localhost:8080/index.htm`.

## Usermod Pattern

Usermods live in `usermods/<name>/` with a `.cpp`, optional `.h`, `library.json`, and `readme.md`. Base new usermods on `usermods/EXAMPLE/` (never edit the example). New custom effects go into the `user_fx` usermod.

```cpp
class MyUsermod : public Usermod {
  static const char _name[];
public:
  void setup() override {}
  void loop() override {}
  void addToConfig(JsonObject& root) override {}
  bool readFromConfig(JsonObject& root) override { return true; }
  uint16_t getId() override { return USERMOD_ID_MYMOD; }
};
const char MyUsermod::_name[] PROGMEM = "MyUsermod";
static MyUsermod myUsermod;
REGISTER_USERMOD(myUsermod);
```

Add usermod IDs to `wled00/const.h`. Activate via `custom_usermods` in the platformio build config.

## General Rules

- **Never edit or commit** `wled00/html_*.h` / `wled00/js_*.h`
- Changes to `platformio.ini` require maintainer approval
- No force-push on open PRs — use regular commits; branches are squash-merged
- Repository language is English
- Remove unused/dead code — justify or delete it
- AI-generated code blocks: wrap with `// AI: below section was generated by an AI` / `// AI: end`

Detailed coding conventions: [docs/cpp.instructions.md](docs/cpp.instructions.md), [docs/web.instructions.md](docs/web.instructions.md), [docs/cicd.instructions.md](docs/cicd.instructions.md).
