# TFT_eSPI_compat

A compatibility wrapper for the [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) library in WLED usermods.  Handles two things that every TFT_eSPI usermod would otherwise need to sort out independently:

1. **SPIFFS stub** — TFT_eSPI pulls in `SPIFFS.h` when `SMOOTH_FONT` is enabled, but SPIFFS is not available on modern ESP32 Arduino platforms.  A minimal stub header is injected automatically, but placed last on the include search path.  This ensures that, if present, the real header is found first and used instead.

2. **Display setup** — TFT_eSPI requires a *User Setup* header to be processed before any of its headers.  This wrapper generates a `tft_setup.h` at build time (using TFT_eSPI's own `__has_include` hook) and injects it into the compiler's search path, so usermods don't need their own build scripts for this.

---

## Using TFT_eSPI_compat in a usermod

Depend on `TFT_eSPI_compat` instead of `TFT_eSPI` directly.  In your usermod's `library.json`:

```json
{
  "name": "my_display_usermod",
  "build": { "libArchive": false },
  "dependencies": {
    "TFT_eSPI_compat": "*"
  }
}
```

TFT_eSPI itself is pulled in transitively.  In your source, include it normally:

```cpp
#include <TFT_eSPI.h>
```

---

## Configuring the display setup

TFT_eSPI needs to know which display driver and pin mapping to use.  Set `custom_display_setup` in your environment's `platformio_override.ini`:

```ini
custom_display_setup = User_Setups/Setup23_ST7789.h
```

The value can be:
- A path relative to any include directory (e.g. a filename inside TFT_eSPI's `User_Setups/` folder), or
- An absolute path to a custom setup header anywhere on disk.

If `custom_display_setup` is not set, no display setup is applied — your usermod must either set a default (see below) or configure the display another way (e.g. via global `build_flags`).

---

## Providing a default setup (for usermod authors)

If your usermod targets a specific display, you can declare a preferred default that users can still override via `custom_display_setup`.  Add a short `extraScript` to your `library.json`:

```json
"build": {
  "extraScript": "set_build_flags.py",
  "libArchive": false
}
```

```python
# set_build_flags.py
Import("env")

global_env = DefaultEnvironment()
global_env.SetDefault(TFT_DISPLAY_SETUP_DEFAULT="User_Setups/Setup25_TTGO_T_Display.h")

# Add any other usermod-specific build flags here (pin defaults, etc.)
```

`SetDefault` only takes effect when neither `custom_display_setup` nor `TFT_DISPLAY_SETUP_DEFAULT` has already been set, so it never overrides a user's explicit configuration.

`TFT_DISPLAY_SETUP_DEFAULT` is read at build time (when `tft_setup.h` is generated), after all library extraScripts have run.

---

## How it works

`configure.py` appends two directories to the compiler search path for all libraries (including TFT_eSPI) — appended rather than prepended, so a real `SPIFFS.h` found anywhere else on the path always takes priority over the stub:

- `shims/` — contains `SPIFFS.h` (stub) and `User_Setup.h` (dependency-tracking shim)
- `$BUILD_DIR/TFT_eSPI_compat/` — contains the generated `tft_setup.h`

TFT_eSPI.h checks `__has_include(<tft_setup.h>)` at compile time; because `tft_setup.h` is built by a SCons `Command` node before any TFT_eSPI source is compiled, it is always present.  `User_Setup.h` carries a plain `#include <tft_setup.h>` that gives SCons's header scanner the dependency edge it needs to guarantee ordering.

---

## TFT_eSPI version

The pinned TFT_eSPI version is declared in `library.json`.  All usermods depending on `TFT_eSPI_compat` share this version; update it in one place when needed.
