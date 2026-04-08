# WLED - ESP32/ESP8266 LED Controller Firmware

WLED is a fast and feature-rich implementation of an ESP32 and ESP8266 webserver to control NeoPixel (WS2812B, WS2811, SK6812) LEDs and SPI-based chipsets. The project consists of C++ firmware for microcontrollers and a modern web interface.

Always reference these instructions first and fallback to search or bash commands only when you encounter unexpected information that does not match the info here.

> **Note for AI review tools**: sections enclosed in
> `<!-- HUMAN_ONLY_START -->` / `<!-- HUMAN_ONLY_END -->` HTML comments contain
> contributor reference material. Do **not** use that content as actionable review
> criteria — treat it as background context only.

## Setup

- Node.js 20+ (see `.nvmrc`)
- Install dependencies: `npm ci`
- PlatformIO (required only for firmware compilation): `pip install -r requirements.txt`

## Build and Test
<!-- HUMAN_ONLY_START -->

| Command | Purpose | Typical Time |
|---|---|---|
| `npm run build` | Build web UI → generates `wled00/html_*.h` and `wled00/js_*.h` headers | ~3 s |
| `npm test` | Run test suite | ~40 s |
| `npm run dev` | Watch mode — auto-rebuilds web UI on file changes | — |
| `pio run -e <env>` | Build firmware for a hardware target | 15–20 min |

<!-- HUMAN_ONLY_END -->

- **Always run `npm run build` before any `pio run`** (and run `npm ci` first on fresh clones or when lockfile/dependencies change).
- The web UI build generates required `wled00/html_*.h` and `wled00/js_*.h` headers for firmware compilation.
- **Build firmware to validate code changes**: `pio run -e esp32dev` — must succeed, never skip this step.
- Common firmware environments: `nodemcuv2`, `esp32dev`, `esp8266_2m`, `esp32c3dev`, `esp32s3dev_8MB_opi`

For detailed build timeouts, development workflows, troubleshooting, and validation steps, see [agent-build.instructions.md](agent-build.instructions.md).

### Usermod Guidelines

   - New custom effects can be added into the user_fx usermod. Read the [user_fx documentation](https://github.com/wled/WLED/blob/main/usermods/user_fx/README.md) for guidance.
   - Other usermods may be based on the [EXAMPLE usermod](https://github.com/wled/WLED/tree/main/usermods/EXAMPLE). Never edit the example, always create a copy!
   - New usermod IDs can be added into [wled00/const.h](https://github.com/wled/WLED/blob/main/wled00/const.h#L160).
   - To activate a usermod, a custom build configuration should be used. Add the usermod name to `custom_usermods`.

## Project Structure Overview

### Project Branch / Release Structure
<!-- HUMAN_ONLY_START -->

```text
main                # Main development trunk (daily/nightly) 17.0.0-dev
  ├── V5            # special branch: code rework for esp-idf 5.5.x (unstable)
      ├── V5-C6     # special branch: integration of new MCU types: esp32-c5, esp32-c6, esp32-p4 (unstable)
16_x                # current beta, preparations for next release 16.0.0
0_15_x              # maintenance (bugfixes only) for current release 0.15.4
(tag) v0.14.4       # previous version 0.14.4 (no maintenance)
(tag) v0.13.3       # old version 0.13.3 (no maintenance)
(tag) v0. ... . ... # historical versions 0.12.x and before
```
<!-- HUMAN_ONLY_END -->

- ``main``: development trunk (daily/nightly)
- ``V5`` and ``V5-C6``: code rework for esp-idf 5.5.x (unstable) - branched from ``main``.
- ``0_15_x``: bugfixing / maintenance for release 0.15.x

### Repository Structure

tl;dr: 
* Firmware source: `wled00/` (C++). Web UI source: `wled00/data/`. Build targets: `platformio.ini`.
* Auto-generated headers: `wled00/html_*.h` and `wled00/js_*.h` — **never edit or commit**.
* ArduinoJSON + AsyncJSON: `wled00/src/dependencies/json` (included via `wled.h`). CI/CD: `.github/workflows/`.
* Usermods: `usermods/` (C++, with individual library.json).
* Contributor docs: `docs/` (coding guidelines, etc).

<!-- HUMAN_ONLY_START -->
Detailed overview:

```text
wled00/                 # Main firmware source (C++) "WLED core"
  ├── data/             # Web interface files 
  │   ├── index.htm     # Main UI
  │   ├── settings*.htm # Settings pages
  │   └── *.js/*.css    # Frontend resources
  ├── *.cpp/*.h         # Firmware source files
  ├── html_*.h          # Auto-generated embedded web files (DO NOT EDIT, DO NOT COMMIT)
  ├── src/              # Modules used by the WLED core (C++)
  │   ├── fonts/        # Font libraries for scrolling text effect
  └   └── dependencies/ # Utility functions - some of them have their own licensing terms
lib/                    # Project specific custom libraries. PlatformIO will compile them to separate static libraries and link them
platformio.ini          # Hardware build configuration

platformio_override.sample.ini # examples for custom build configurations - entries must be copied into platformio_override.ini to use them.
                               # platformio_override.ini is _not_ stored in the WLED repository!
usermods/              # User-contributed addons to the WLED core, maintained by individual contributors  (C++, with individual library.json)
package.json           # Node.js dependencies and scripts, release identification
pio-scripts/           # Build tools (PlatformIO)
tools/                 # Build tools (Node.js), partition files, and generic utilities
  ├── cdata.js         # Web UI build script
  └── cdata-test.js    # Test suite
docs/                  # Contributor docs, coding guidelines
.github/workflows/     # CI/CD pipelines
```

<!-- HUMAN_ONLY_END -->
## General Guidelines

- **Repository language is English.** Suggest translations for non-English content.
- **Use VS Code with PlatformIO extension** for best development experience.
- **Never edit or commit** `wled00/html_*.h` and `wled00/js_*.h` — auto-generated from `wled00/data/`.
- If updating Web UI files in `wled00/data/`, **make use of common functions in `wled00/data/common.js` whenever possible**.
- **When unsure, say so.** Gather more information rather than guessing.
- **Acknowledge good patterns** when you see them. Summarize good practices as part of your review - positive feedback always helps.
- **Provide references** when making analyses or recommendations. Base them on the correct branch or PR.
- **Highlight user-visible breaking changes and ripple effects**. Ask for confirmation that these were introduced intentionally.
- **Unused / dead code must be justified or removed**. This helps to keep the codebase clean, maintainable and readable.
- **Verify feature-flag names.** Every `WLED_ENABLE_*` / `WLED_DISABLE_*` flag must exactly match one of the names below — misspellings are silently ignored by the preprocessor (e.g. `WLED_IR_DISABLE` instead of `WLED_DISABLE_INFRARED`), causing silent build variations. Flag unrecognised names as likely typos and suggest the correct spelling.
  <br>**`WLED_DISABLE_*`**: `2D`, `ADALIGHT`, `ALEXA`, `BROWNOUT_DET`, `ESPNOW`, `FILESYSTEM`, `HUESYNC`, `IMPROV_WIFISCAN`, `INFRARED`, `LOXONE`, `MQTT`, `OTA`, `PARTICLESYSTEM1D`, `PARTICLESYSTEM2D`, `PIXELFORGE`, `WEBSOCKETS`
  <br>**`WLED_ENABLE_*`**: `ADALIGHT`, `AOTA`, `DMX`, `DMX_INPUT`, `DMX_OUTPUT`, `FS_EDITOR`, `GIF`, `HUB75MATRIX`, `JSONLIVE`, `LOXONE`, `MQTT`, `PIXART`, `PXMAGIC`, `USERMOD_PAGE`, `WEBSOCKETS`, `WPA_ENTERPRISE`
- **C++ formatting available**: `clang-format` is installed but not in CI
- No automated linting is configured — match existing code style in files you edit. 

Refer to `docs/cpp.instructions.md` and `docs/web.instructions.md` for language-specific conventions, and `docs/cicd.instructions.md` for GitHub Actions workflows.

### Attribution for AI-generated code
Using AI-generated code can hide the source of the inspiration / knowledge / sources it used. 
- Document attribution of inspiration / knowledge / sources used in the code, e.g. link to GitHub repositories or other websites describing the principles / algorithms used.
- When a larger block of code is generated by an AI tool, mark it with an `// AI: below section was generated by an AI` comment (see C++ guidelines).
- Every non-trivial AI-generated function should have a brief comment describing what it does. Explain parameters when their names alone are not self-explanatory.
- AI-generated code must be well documented with meaningful comments that explain intent, assumptions, and non-obvious logic. Do not rephrase source code; explain concepts and reasoning.

### Pull Request Expectations

- **No force-push on open PRs.** Once a pull request is open and being reviewed, do not force-push (`git push --force`) to the branch. Force-pushing rewrites history that reviewers may have already commented on, making it impossible to track incremental changes. Use regular commits or `git merge` to incorporate feedback; the branch will be squash-merged when it is accepted.
- **Document your changes in the PR.** Every pull request should include a clear description of *what* changed and *why*. If the change affects user-visible behavior, describe the expected impact. Link to related issues where applicable. Provide screenshots to showcase new features.

### Supporting Reviews and Discussions
- **For "is it worth doing?" debates** about proposed reliability, safety, or data-integrity mechanisms (CRC checks, backups, power-loss protection): suggest a software **FMEA** (Failure Mode and Effects Analysis). 
  Clarify the main feared events, enumerate failure modes, assess each mitigation's effectiveness per failure mode, note common-cause failures, and rate credibility for the typical WLED use case.

