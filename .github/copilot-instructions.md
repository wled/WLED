# WLED - ESP32/ESP8266 LED Controller Firmware

WLED is a fast and feature-rich implementation of an ESP32 and ESP8266 webserver to control NeoPixel (WS2812B, WS2811, SK6812) LEDs and SPI-based chipsets. The project consists of C++ firmware for microcontrollers and a modern web interface.

Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

## Architecture Overview

**Two-layer system**: Web UI (JavaScript/HTML/CSS) + Embedded firmware (C++).

1. **Web UI layer** (`wled00/data/`): Frontend served by ESP device or HTTP server
   - Single-page app using vanilla JavaScript (no frameworks) 
   - State management in global variables (e.g., `isOn`, `selectedFx`, `segCount`)
   - Color picker (iro.js library) and effects driven by JSON API
   - Pages generated via template pattern: `index.htm` (main), `settings*.htm` (config), `*.htm` (utilities)

2. **Firmware layer** (`wled00/*.cpp/.h`): C++ on microcontroller
   - Effect system: `FX.cpp` (100+ effects), `palettes.cpp` (50+ color palettes)
   - LED management: `bus_manager.h` (multi-strip support), `pin_manager.h` (GPIO allocation)
   - Protocol handlers: `json.cpp` (REST API), `ws.cpp` (WebSocket), `mqtt.cpp`, `udp.cpp`, `e131.cpp`
   - Usermod system: Plugin architecture for extensions (v1 simple, v2 class-based)

3. **Build bridge**: `tools/cdata.js` embeds minified web UI into C++ headers (`html_*.h`)
   - Inlines CSS/JS, gzips for size, generates C arrays for firmware
   - These headers are included in firmware binary—cannot edit directly

## Working Effectively

### Initial Setup
- Install Node.js 20+ (specified in `.nvmrc`): Check your version with `node --version`
- Install dependencies: `npm ci` (takes ~5 seconds)
- Install PlatformIO for hardware builds: `pip install -r requirements.txt` (takes ~60 seconds)

### Build and Test Workflow
- **ALWAYS build web UI first**: `npm run build` -- takes 3 seconds. NEVER CANCEL.
- **Run tests**: `npm test` -- takes 40 seconds. NEVER CANCEL. Set timeout to 2+ minutes.
- **Development mode**: `npm run dev` -- monitors file changes and auto-rebuilds web UI
- **Hardware firmware build**: `pio run -e [environment]` -- takes 15+ minutes. NEVER CANCEL. Set timeout to 30+ minutes.

### Build Process Details
The build has two main phases:
1. **Web UI Generation** (`npm run build`):
   - Processes files in `wled00/data/` (HTML, CSS, JS)
   - Minifies and compresses web content 
   - Generates `wled00/html_*.h` files with embedded web content
   - **CRITICAL**: Must be done before any hardware build

2. **Hardware Compilation** (`pio run`):
   - Compiles C++ firmware for various ESP32/ESP8266 targets
   - Common environments: `nodemcuv2`, `esp32dev`, `esp8266_2m`
   - List all targets: `pio run --list-targets`

## Before Finishing Work

**CRITICAL: You MUST complete ALL of these steps before marking your work as complete:**

1. **Run the test suite**: `npm test` -- Set timeout to 2+ minutes. NEVER CANCEL.
   - All tests MUST pass
   - If tests fail, fix the issue before proceeding

2. **Build at least one hardware environment**: `pio run -e esp32dev` -- Set timeout to 30+ minutes. NEVER CANCEL.
   - Choose `esp32dev` as it's a common, representative environment
   - See "Hardware Compilation" section above for the full list of common environments
   - The build MUST complete successfully without errors
   - If the build fails, fix the issue before proceeding
   - **DO NOT skip this step** - it validates that firmware compiles with your changes

3. **For web UI changes only**: Manually test the interface
   - See "Manual Testing Scenarios" section below
   - Verify the UI loads and functions correctly

**If any of these validation steps fail, you MUST fix the issues before finishing. Do NOT mark work as complete with failing builds or tests.**

## Validation and Testing

### Web UI Testing
- **ALWAYS validate web UI changes manually**:
  - Start local server: `cd wled00/data && python3 -m http.server 8080`
  - Open `http://localhost:8080/index.htm` in browser
  - Test basic functionality: color picker, effects, settings pages
- **Check for JavaScript errors** in browser console

### Code Validation
- **No automated linting configured** - follow existing code style in files you edit
- **Code style**: Use tabs for web files (.html/.css/.js), spaces (2 per level) for C++ files
- **C++ formatting available**: `clang-format` is installed but not in CI
- **Always run tests before finishing**: `npm test`
- **MANDATORY: Always run a hardware build before finishing** (see "Before Finishing Work" section below)

### Manual Testing Scenarios
After making changes to web UI, always test:
- **Load main interface**: Verify index.htm loads without errors
- **Navigation**: Test switching between main page and settings pages
- **Color controls**: Verify color picker and brightness controls work
- **Effects**: Test effect selection and parameter changes
- **Settings**: Test form submission and validation

## Code Patterns and Conventions

### Web UI Patterns
- **Global state variables**: `isOn`, `nlA` (nightlight active), `selectedFx`, `selectedPal`, `csel` (color slot), `segCount`
- **Utility functions** (`common.js`): `gId()` (getElementById), `cE()` (createElement), `isN()` (isNumeric), `isO()` (isObject)
- **JSON API communication**: `requestJson()` to GET `/json/state`, `SetV()` to update UI from response
- **Form submission**: Settings pages POST to `/settings/` endpoints; web UI listens for config with `preGetV()` hook before `GetV()`
- **Tabs system**: `toggle()` function hides/shows divs with `.hide` class
- **Color management**: Color slots use HTML dataset attributes (`data-r`, `data-g`, `data-b`, `data-w`)

### Firmware Patterns (C++)
- **Version format**: `#define VERSION 2506160` (yymmddb format)
- **Feature flags**: Conditional compilation via `#define WLED_ENABLE_*` / `#define WLED_DISABLE_*` in `wled.h`
- **Usermod v2 API**: Inherit from `Usermod` class, override `setup()`, `connected()`, `loop()`, `addToConfig()`, `readFromConfig()`
- **Usermod v1 API**: Simple callbacks `userSetup()`, `userConnected()`, `userLoop()` — limited but useful for small mods
- **Segment system**: LEDs grouped into "segments" with individual colors/effects—core WLED feature
- **EEPROM storage**: Config persisted in EEPROM; usermod v1 uses bytes 2551-2559 (8 bytes) or 2750+ (custom size)

### Build System Conventions
- **Tabs in web files** (`.html`, `.css`, `.js`), **2-space indentation in C++** (`.cpp`, `.h`)
- **Header files auto-generated**: Never edit `html_*.h` — always edit source in `wled00/data/` instead
- **Environment selection**: Use `platformio_override.ini` for custom boards/usermods, don't modify main `platformio.ini`
- **Usermod registration**: Add `#include "usermod_*.h"` + `registerUsermod(new ClassName())` in `usermods_list.cpp`
- **Custom usermods**: Specify in `platformio.ini` with `custom_usermods = mod1,mod2` or in `platformio_override.ini`

## Common Tasks

### Repository Structure
```
wled00/                 # Main firmware source (C++)
  ├── data/            # Web interface files 
  │   ├── index.htm    # Main UI
  │   ├── settings*.htm # Settings pages
  │   └── *.js/*.css   # Frontend resources
  ├── *.cpp/*.h        # Firmware source files
  └── html_*.h         # Generated embedded web files (DO NOT EDIT)
tools/                 # Build tools (Node.js)
  ├── cdata.js         # Web UI build script
  └── cdata-test.js    # Test suite
platformio.ini         # Hardware build configuration
package.json           # Node.js dependencies and scripts
.github/workflows/     # CI/CD pipelines
```

### Key Files and Their Purpose
- `wled00/data/index.htm` - Main web interface
- `wled00/data/settings*.htm` - Configuration pages  
- `tools/cdata.js` - Converts web files to C++ headers
- `wled00/wled.h` - Main firmware configuration
- `platformio.ini` - Hardware build targets and settings

### Development Workflow
1. **For web UI changes**:
   - Edit files in `wled00/data/`
   - Run `npm run build` to regenerate headers
   - Test with local HTTP server
   - Run `npm test` to validate build system

2. **For firmware changes**:
   - Edit files in `wled00/` (but NOT `html_*.h` files)
   - Ensure web UI is built first (`npm run build`)
   - Build firmware: `pio run -e [target]`
   - Flash to device: `pio run -e [target] --target upload`

3. **For both web and firmware**:
   - Always build web UI first
   - Test web interface manually
   - Build and test firmware if making firmware changes

## Build Timing and Timeouts

**IMPORTANT: Use these timeout values when running builds:**

- **Web UI build** (`npm run build`): 3 seconds typical - Set timeout to 30 seconds minimum
- **Test suite** (`npm test`): 40 seconds typical - Set timeout to 120 seconds (2 minutes) minimum  
- **Hardware builds** (`pio run -e [target]`): 15-20 minutes typical for first build - Set timeout to 1800 seconds (30 minutes) minimum
  - Subsequent builds are faster due to caching
  - First builds download toolchains and dependencies which takes significant time
- **NEVER CANCEL long-running builds** - PlatformIO downloads and compilation require patience

**When validating your changes before finishing, you MUST wait for the hardware build to complete successfully. Set the timeout appropriately and be patient.**

## Troubleshooting

### Common Issues
- **Build fails with missing html_*.h**: Run `npm run build` first
- **Web UI looks broken**: Check browser console for JavaScript errors
- **PlatformIO network errors**: Try again, downloads can be flaky
- **Node.js version issues**: Ensure Node.js 20+ is installed (check `.nvmrc`)

### When Things Go Wrong
- **Clear generated files**: `rm -f wled00/html_*.h` then rebuild
- **Force web UI rebuild**: `npm run build -- --force` or `npm run build -- -f`
- **Clean PlatformIO cache**: `pio run --target clean`
- **Reinstall dependencies**: `rm -rf node_modules && npm install`

## Important Notes

- **DO NOT edit `wled00/html_*.h` files** - they are auto-generated
- **Always commit both source files AND generated html_*.h files**
- **Web UI must be built before firmware compilation**
- **Test web interface manually after any web UI changes**
- **Use VS Code with PlatformIO extension for best development experience**
- **Hardware builds require appropriate ESP32/ESP8266 development board**

## CI/CD Pipeline

**The GitHub Actions CI workflow will:**
1. Installs Node.js and Python dependencies
2. Runs `npm test` to validate build system (MUST pass)
3. Builds web UI with `npm run build` (automatically run by PlatformIO)
4. Compiles firmware for ALL hardware targets listed in `default_envs` (MUST succeed for all)
5. Uploads build artifacts

**To ensure CI success, you MUST locally:**
- Run `npm test` and ensure it passes
- Run `pio run -e esp32dev` (or another common environment from "Hardware Compilation" section) and ensure it completes successfully
- If either fails locally, it WILL fail in CI

**Match this workflow in your local development to ensure CI success. Do not mark work complete until you have validated builds locally.**
