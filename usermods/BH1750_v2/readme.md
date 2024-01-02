# BH1750 usermod

This usermod will read from an ambient light sensor like the BH1750.
The luminance is displayed in both the Info section of the web UI, as well as published to the `/luminance` MQTT topic if enabled.

## Dependencies
- Libraries
  - `claws/BH1750 @^1.2.0`
  - This must be added under `lib_deps` in your `platformio.ini` (or `platformio_override.ini`).
- Data is published over MQTT - make sure you've enabled the MQTT sync interface.

## Compilation

To enable, compile with `USERMOD_BH1750` defined  (e.g. in `platformio_override.ini`)
```ini
[env:usermod_BH1750_d1_mini]
extends = env:d1_mini
build_flags =
    ${common.build_flags_esp8266}
    -D USERMOD_BH1750
lib_deps = 
    ${esp8266.lib_deps}
    claws/BH1750 @ ^1.2.0
```

### Configuration Options
The following settings can be set at compile-time but are configurable on the usermod menu (except First Measurement time):
*   `USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL` - the max number of milliseconds between measurements, defaults to 10000ms
*   `USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL` - the min number of milliseconds between measurements, defaults to 500ms
*   `USERMOD_BH1750_OFFSET_VALUE` - the offset value to report on, defaults to 1
*   `USERMOD_BH1750_FIRST_MEASUREMENT_AT` - the number of milliseconds after boot to take first measurement, defaults to 10000 ms

In addition, the Usermod screen allows you to:
- enable/disable the usermod
- Enable Home Assistant Discovery of usermod
- Configure the SCL/SDA pins

## API
The following method is available to interact with the usermod from other code modules:
- `getIlluminance` read the brightness from the sensor

## Change Log
Jul 2022
- Added Home Assistant Discovery
- Implemented PinManager to register pins
- Made pins configurable in usermod menu
- Added API call to read luminance from other modules
- Enhanced info-screen outputs
- Updated `readme.md`
