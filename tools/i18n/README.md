# WLED i18n Toolchain

Build-time internationalization for WLED Web UI. Translates HTML/JS strings at compile time with **zero runtime overhead** and **zero flash overhead** (replaces, not adds).

## How It Works

```
English HTM files (wled00/data/)
        ↓
   extract.py  →  locales/_template.json
        ↓
  Translator fills in locales/zh_CN.json
        ↓
    build.py   →  Translated HTM files
        ↓
  npm run build  →  html_*.h / js_*.h (C headers)
        ↓
    pio run      →  Firmware with translated UI
```

## Three-Layer Coverage

| Layer | Content | Method | Coverage |
|-------|---------|--------|----------|
| HTML text | Labels, buttons, placeholders | DOM text matching | ~60% |
| JS strings | `alert()`, `innerHTML`, `innerText` | Script block regex | ~30% |
| Attributes | `placeholder`, `title`, `alt` | Attribute replacement | ~10% |

**Not covered** (by design):
- `settingsScript.print()` — server-side runtime injection (~12 status strings in C++ `xml.cpp`). These are error/status messages that would need `#ifdef WLED_LOCALE_*` in C++ source.
- Template literals with `${...}` variables — partial strings can't be safely replaced.
- Hardware/protocol names (WS281x, PWM CCT, etc.) — industry standard, not translated.

## Quick Start

### 1. Extract strings (generate template)

```bash
# Generate English template from upstream HTM files
python3 tools/i18n/extract.py --stats

# Output: tools/i18n/locales/_template.json
```

### 2. Create translation

```bash
# Copy template to your locale
cp tools/i18n/locales/_template.json tools/i18n/locales/zh_CN.json

# Edit zh_CN.json — fill in "translation" fields
# Each entry has: {"en": "English text", "translation": "", "context": "file:line (type)"}
```

### 3. Build translated firmware

```bash
# Build translated HTM files
python3 tools/i18n/build.py --locale zh_CN --source-dir wled00/data --output-dir wled00/data

# Build web UI headers
npm ci && npm run build

# Build firmware
pio run -e esp32dev
```

### 4. PlatformIO integration (automatic)

Add to `platformio.ini`:

```ini
[env:esp32dev_zh_CN]
extends = env:esp32dev
build_flags = ${env:esp32dev.build_flags} -D WLED_LOCALE=zh_CN
extra_scripts = pre:tools/i18n/build.py
```

Then just: `pio run -e esp32dev_zh_CN`

## Translation JSON Format

```json
{
  "index.htm": {
    "html:body > div#btns > a:nth-of-type(1):text": {
      "en": "Power",
      "translation": "电源",
      "context": "index.htm: (html_text)"
    },
    "js:index.htm:45:a1b2c3d4": {
      "en": "Loading...",
      "translation": "加载中...",
      "context": "index.htm:45 (js_innerHTML)"
    }
  }
}
```

## Adding a New Language

1. Run `python3 tools/i18n/extract.py` to generate the template
2. Copy `locales/_template.json` to `locales/<locale>.json` (e.g. `de_DE.json`)
3. Fill in translations
4. Add locale to `LOCALE_LANG` dict in `build.py`
5. Add build env to `platformio.ini`

## Current Status (zh_CN)

- **380** translatable strings extracted from 22 HTM files
- **259** translated (68% of total, ~88% of actually translatable content)
- Remaining: brand names, version numbers, template literals with variables

## Limitations

1. **No runtime language switching** — language is fixed at build time
2. **BeautifulSoup not required at build time** — `build.py` uses pure regex for surgical string replacement, preserving original HTML formatting exactly
3. **JS template literals with `${...}`** — partial strings can't be safely replaced
4. **C++ server-side strings** — ~12 strings in `xml.cpp` need separate handling

## Architecture Decisions

- **Regex over DOM parsing for build**: BeautifulSoup changes HTML formatting (attribute order, self-closing tags, whitespace). For ESP32 firmware where every byte matters, we use exact string replacement to preserve the original HTML byte-for-byte (except for translated text).
- **Stable keys**: HTML strings use CSS selector paths, JS strings use content hash. Keys are stable across minor HTML edits.
- **External toolchain**: Zero changes to existing WLED build pipeline. The i18n tool is a pre-build step that generates files before `npm run build`.
