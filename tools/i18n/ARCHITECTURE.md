# WLED i18n Architecture

## Overview

Two-repository architecture separating **build toolchain** (core repo) from **translation files** (community repo).

```
┌─────────────────────────────────────────────────────────────┐
│  Core Repo (WLED/tools/i18n/)                               │
│  ├── extract.py    # Extract translatable strings from HTML │
│  ├── build.py      # Apply translations at build time       │
│  └── locales/      # Locale configuration                   │
└─────────────────────────────────────────────────────────────┘
                           ↓ calls
┌─────────────────────────────────────────────────────────────┐
│  Translation Repo (WLED-translations/<locale>/)             │
│  ├── static.json   # Layer 1: Static HTML (429 entries)     │
│  ├── js.json       # Layer 2: JS strings (45 entries)       │
│  ├── effects.json  # Layer 3: Effect names (216 entries)    │
│  ├── palettes.json # Layer 4: Palette names (72 entries)    │
│  └── metadata.json # Version, coverage, maintainer          │
└─────────────────────────────────────────────────────────────┘
```

---

## Four-Layer Translation Architecture

| Layer | Content | File | Implementation | Coverage |
|-------|---------|------|----------------|----------|
| **L1** | Static HTML | `static.json` | Regex replacement in HTML text | 429 strings |
| **L2** | JS strings | `js.json` | Replace JS string literals | 45 strings |
| **L3** | Effect names | `effects.json` | C++ PROGMEM `#undef` + redefine | 216/216 (100%) |
| **L4** | Palette names | `palettes.json` | C++ PROGMEM array replacement | 72/72 (100%) |

---

## Build Flow

### PlatformIO Configuration

```ini
# platformio.ini
[env:esp32dev_zh_CN]
extends = env:esp32dev
build_flags = ${env:esp32dev.build_flags} -D WLED_LOCALE=zh_CN
extra_scripts = 
    ${env:esp32dev.extra_scripts}
    pre:tools/i18n/build.py
```

### Build Steps

1. `build.py` reads `wled00/data/*.htm` (English source)
2. Applies L1/L2 translations via regex replacement
3. Generates `i18n_effects.h` / `i18n_palettes.h` for L3/L4 (PROGMEM replacement)
4. Output to `build/i18n/<locale>/`

---

## How Dynamic Content Works

The key insight: **PROGMEM replacement happens at compile time**, so JSON endpoints return translated strings automatically.

```
Browser                          ESP32 Firmware
   │                               │
   ├─ GET /json/palettes ─────────→│ PROGMEM array was replaced at compile time
   │  ← {"0":"默认","1":"* 随机循环",...}    ↓
   │                               │ palettes.json → i18n_palettes.h
   ├─ GET /json/effects  ─────────→│ #undef _data_FX_MODE_STATIC
   │  ← {"0":"常亮","1":"闪烁",...}│ #define _data_FX_MODE_STATIC "常亮"
```

No firmware code changes needed. The C++ PROGMEM strings are the single source of truth.

---

## Grammar and Word Order

WLED UI uses **short labels**, not full sentences:

| Pattern | Example | i18n Impact |
|---------|---------|-------------|
| Single word | "Brightness", "Speed" | ✅ No issue |
| Label + value | "255 segments" | ✅ Works ("255 个段") |
| Full sentences | Almost none | ✅ N/A |
| Plural forms | Not used | ✅ N/A |
| Date formats | Not used in UI | ✅ N/A |

The architecture **intentionally avoids** complex i18n patterns (ICU MessageFormat, plural rules) because WLED's UI doesn't need them.

---

## What's NOT Translated (By Design)

| Content | Reason |
|---------|--------|
| User-defined preset names | Belongs to user |
| Usermod settings pages | Dynamic HTML from firmware, varies by hardware |
| System info (IP, memory) | Universal data |
| Effect slider tooltips | Generated from mode data arrays |

---

## Repository Structure

### Core repo (WLED)

```
tools/i18n/
├── extract.py       # String extraction tool
├── build.py         # Build-time translation applicator
├── ARCHITECTURE.md  # This file
├── README.md        # Usage documentation
└── locales/         # Locale configs
```

### Translation repo (WLED-translations)

```
<locale>/
├── static.json      # Layer 1: Static HTML text
├── js.json          # Layer 2: JavaScript strings
├── effects.json     # Layer 3: Effect names (PROGMEM)
├── palettes.json    # Layer 4: Palette names (PROGMEM)
└── metadata.json    # {"version":"1.0","coverage":"100%","maintainer":"..."}
en_template/         # English template for translators
```

---

## Adding a New Language

1. Fork `WLED-translations`
2. Copy `en_template/` to `<locale>/`
3. Translate JSON files
4. Submit PR to translation repo

No changes to WLED core needed.

---

## Coverage Summary (zh_CN)

| Layer | Content | Count | Status |
|-------|---------|-------|--------|
| L1 | Static HTML | 429 | ✅ Complete |
| L2 | JS strings | 45 | ✅ Complete |
| L3 | Effect names | 216/216 | ✅ 100% |
| L4 | Palette names | 72/72 | ✅ 100% |
| **Total** | | **762** | **100%** |
