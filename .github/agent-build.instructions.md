---
applyTo: "**"
---
# Agent-Mode Build & Test Instructions

Detailed build workflow, timeouts, and troubleshooting for making code changes in agent mode. Always reference these instructions first when running builds or validating changes.

## Build Timing and Timeouts

Use these timeout values when running builds:

| Command | Typical Time | Minimum Timeout | Notes |
|---|---|---|---|
| `npm run build` | ~3 s | 30 s | Web UI → `wled00/html_*.h` `wled00/js_*.h` headers |
| `npm test` | ~40 s | 2 min | Validates build system |
| `npm run dev` | continuous | — | Watch mode, auto-rebuilds on changes |
| `pio run -e <env>` | 15–20 min | 30 min | First build downloads toolchains; subsequent builds are faster |

**NEVER cancel long-running builds.** PlatformIO downloads and compilation require patience.

## Development Workflow

### Code Style Summary
- **C++** files in `wled00/` and `usermods/`: 2-space indentation (no tabs), camelCase functions/variables, PascalCase classes, UPPER_CASE macros. No C++ exceptions — use return codes and debug macros.
- **Web UI** files in `wled00/data`: indent HTML and JavaScript with tabs, CSS with tabs.
- **CI/CD workflows** in `.github/workflows`: 2-space indentation, descriptive `name:` on every workflow/job/step. Third-party actions must be pinned to a specific version tag — branch pins such as `@main` or `@master` are not allowed. SHA pinning recommended.

### Web UI Changes

1. Edit files in `wled00/data/`
2. Run `npm run build` to regenerate `wled00/html_*.h` `wled00/js_*.h` headers
3. Test with local HTTP server (see Manual Testing below)
4. Run `npm test` to validate

### Firmware Changes

1. Edit files in `wled00/` (but **never** `html_*.h` and `js_*.h` files)
2. Ensure web UI is built first: `npm run build`
3. Build firmware: `pio run -e esp32dev` (set timeout ≥ 30 min)
4. Flash to device: `pio run -e [target] --target upload`

### Combined Web + Firmware Changes

1. Always build web UI first
2. Test web interface manually
3. Then build and test firmware


## Before Finishing Work - Testing

**You MUST complete ALL of these before marking work as done:**

1. **Run tests**: `npm test` — must pass
2. **Build firmware**: `pio run -e esp32dev` — must succeed after source code changes, **never skip this step**.
   - Set timeout to 30+ minutes, **never cancel**
   - Choose `esp32dev` as a common, representative environment
   - If the build fails, fix the issue before proceeding
3. **For web UI changes**: manually test the interface (see below)

If any step fails, fix the issue. **Do NOT mark work complete with failing builds or tests.**

## Manual Web UI Testing

Start a local server:

```sh
cd wled00/data && python3 -m http.server 8080
# Open http://localhost:8080/index.htm
```

Test these scenarios after every web UI change:

- **Load**: `index.htm` loads without JavaScript errors (check browser console)
- **Navigation**: switching between main page and settings pages works
- **Color controls**: color picker and brightness controls function correctly
- **Effects**: effect selection and parameter changes work
- **Settings**: form submission and validation work

## Troubleshooting

### Common Build Issues

| Problem | Solution |
|---|---|
| Missing `html_*.h` | Run `npm ci; npm run build` |
| Web UI looks broken | Check browser console for JS errors |
| PlatformIO network errors | Retry — downloads can be flaky |
| Node.js version mismatch | Ensure Node.js 20+ (check `.nvmrc`) |

### Recovery Steps

- **Force web UI rebuild**: `npm run build -- -f`
- **Clear generated files**: `rm -f wled00/html_*.h wled00/js_*.h` then `npm run build`
- **Clean PlatformIO build artifacts**: `pio run --target clean`
- **Reinstall Node deps**: `rm -rf node_modules && npm ci`

## CI/CD Validation

The GitHub Actions CI workflow will:
1. Install Node.js and Python dependencies
2. Run `npm test`
3. Build web UI (automatic via PlatformIO)
4. Compile firmware for **all** `default_envs` targets

**To ensure CI success, always validate locally:**
- Run `npm test` and ensure it passes
- Run `pio run -e esp32dev` (or another common firmware environment, see next section) and ensure it completes successfully
- If either fails locally, it WILL fail in CI

Match this workflow in local development to catch failures before pushing.

## Important Reminders

- Always **commit source code**
- **Never edit or commit** `wled00/html_*.h` and  `wled00/js_*.h` — auto-generated from `wled00/data/`
- After modifying source code files, check that any **previous comments have been preserved** or updated to reflect the new behaviour.
- Web UI rebuild is part of the PlatformIO firmware compilation pipeline
- Common environments:  `nodemcuv2`, `esp32dev`, `esp8266_2m`, `esp32c3dev`, `esp32s3dev_8MB_opi`
- List all PlatformIO targets: `pio run --list-targets`
