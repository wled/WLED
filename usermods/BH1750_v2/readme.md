# BH1750 usermod

This usermod will read from an ambient light sensor like the BH1750 sensor.
The luminance is displayed both in the Info section of the web UI as well as published to the `/luminance` MQTT topic if enabled.

## Installation

Copy the example `platformio_override.ini` to the root directory.  This file should be placed in the same directory as `platformio.ini`.

### Define Your Options

*   `USERMOD_BH1750`                                - define this to have this user mod included wled00\usermods_list.cpp
*   `USERMOD_BH1750_MAX_MEASUREMENT_INTERVAL`       - the max number of milliseconds between measurements, defaults to 10000ms
*   `USERMOD_BH1750_MIN_MEASUREMENT_INTERVAL`       - the min number of milliseconds between measurements, defaults to 500ms
*   `USERMOD_BH1750_FIRST_MEASUREMENT_AT`           - the number of milliseconds after boot to take first measurement, defaults to 10 seconds
*   `USERMOD_BH1750_OFFSET_VALUE`                   - the offset value to report on, defaults to 1

All parameters can be configured at runtime using Usermods settings page.

### PlatformIO requirements

If you are using `platformio_override.ini`, you should be able to refresh the task list and see your custom task, for example `env:usermod_BH1750_d1_mini`.

## Change Log
