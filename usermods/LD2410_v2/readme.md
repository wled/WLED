# BH1750 usermod

> This usermod requires a second UART and was only tested on the ESP32


This usermod will read from a LD2410 movement/presence sensor.

The movement and presence state are displayed in both the Info section of the web UI, as well as published to the `/movement` and `/stationary` MQTT topics respectively.

## Dependencies
- Libraries
  - `ncmreynolds/ld2410@^0.1.3`
- Data is published over MQTT - make sure you've enabled the MQTT sync interface.

## Compilation

To enable, compile with `LD2140` in `custom_usermods` (e.g. in `platformio_override.ini`)
```ini
[env:usermod_USERMOD_LD2410_esp32dev]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods} LD2140
```

### Configuration Options
The Usermod screen allows you to:
- enable/disable the usermod
- Configure the RX/TX pins

## Change log
-  2024-06 Created by @wesleygas (https://github.com/wesleygas/)
