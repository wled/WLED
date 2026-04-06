# Usermod AHT10
This Usermod is designed to read a `AHT10`, `AHT15` or `AHT20` sensor and output the following:
- Temperature
- Humidity

Configuration is performed via the Usermod menu. The following settings can be configured in the Usermod Menu:
- I2CAddress: The i2c address in decimal. Set it to either 56 (0x38, the default) or 57 (0x39).
- SensorType, one of:
  - 0 - AHT10
  - 1 - AHT15
  - 2 - AHT20
- CheckInterval: Number of seconds between readings
- Decimals: Number of decimals to put in the output

Dependencies, These must be added under `lib_deps` in your `platform.ini` (or `platform_override.ini`).
- Libraries
  - `enjoyneering/AHT10@~1.1.0` (by [enjoyneering](https://registry.platformio.org/libraries/enjoyneering/AHT10))
  - `Wire`

## Author
[@LordMike](https://github.com/LordMike)

# Compiling

To enable, add 'AHT10' to `custom_usermods` in your platformio encrionment  (e.g. in `platformio_override.ini`)
```ini
[env:aht10_example]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods} AHT10
```
