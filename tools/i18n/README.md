# WLED i18n Toolchain

Build-time internationalization for WLED Web UI. Translates HTML/JS strings at compile time with **zero runtime overhead**.

## Architecture

```
WLED (core repo)                    WLED-translations (community repo)
tools/i18n/                         <locale>/
├── extract.py   ──────────────────→ static.json (Layer 1: HTML)
├── build.py     ←─ applies ─────── js.json     (Layer 2: JS)
├── ARCHITECTURE.md                 effects.json (Layer 3: PROGMEM)
└── README.md                       palettes.json(Layer 4: PROGMEM)
```

**Key principle:** Translations are maintained **out-of-tree** by community contributors, similar to usermods. Users build translated firmware locally.

## Quick Start

### 1. Clone both repos

```bash
git clone https://github.com/wled/WLED.git
git clone https://github.com/wled/WLED-translations.git
```

### 2. Extract strings (optional, for translators)

```bash
cd WLED
python3 tools/i18n/extract.py --stats
# Output: tools/i18n/locales/_template.json
```

### 3. Validate translations

```bash
# Check coverage for a locale
python3 tools/i18n/extract.py --validate zh_CN

# Output:
# Validating locale: zh_CN
# ========================================
# Coverage: 429/429 (100.0%)
# PASSED: All strings translated
```

### 4. Build translated firmware

```bash
cd WLED

# Apply translations (Layer 1 + 2: HTML/JS)
python3 tools/i18n/build.py --locale zh_CN \
    --source-dir wled00/data \
    --translation-dir ../WLED-translations/zh_CN \
    --output-dir build/i18n/zh_CN

# Build web UI headers
npm ci && npm run build

# Build firmware
pio run -e esp32dev
```

### 5. PlatformIO integration (automatic)

Add to `platformio.ini`:

```ini
[env:esp32dev_zh_CN]
extends = env:esp32dev
build_flags = ${env:esp32dev.build_flags} -D WLED_LOCALE=zh_CN
extra_scripts = pre:tools/i18n/build.py
```

Then just: `pio run -e esp32dev_zh_CN`

## Four-Layer Translation System

| Layer | Content | Source | Method |
|-------|---------|--------|--------|
| **L1** | Static HTML | `static.json` | Regex replacement |
| **L2** | JS strings | `js.json` | Script block regex |
| **L3** | Effect names | `effects.json` | PROGMEM `#undef` + redefine |
| **L4** | Palette names | `palettes.json` | PROGMEM array replacement |

## For Translators

1. Fork [WLED-translations](https://github.com/wled/WLED-translations)
2. Copy `en_template/` to `<locale>/`
3. Edit JSON files — fill in `"translation"` fields
4. Validate: `python3 tools/i18n/extract.py --validate <locale>`
5. Submit PR to WLED-translations repo

## For Maintainers

### Adding this toolchain to WLED core

This PR adds only `tools/i18n/` — no changes to existing build pipeline. The tool is a pre-build step that runs before `npm run build`.

### CI/CD integration

```yaml
# .github/workflows/i18n-validate.yml
- name: Validate translations
  run: |
    git clone https://github.com/wled/WLED-translations.git
    for locale in WLED-translations/*/; do
      python3 tools/i18n/extract.py --validate $(basename $locale)
    done
```

## Limitations

1. **No runtime language switching** — language is fixed at build time
2. **External tools** (pixelforge, pixelmagic) — always English, downloaded on-the-fly
3. **C++ server-side strings** — ~12 strings in `xml.cpp` need separate handling
4. **User presets** — user-defined names are not translated (by design)
