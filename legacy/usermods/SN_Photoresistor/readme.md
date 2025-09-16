# SN_Photoresistor usermod

This usermod will read from an attached photoresistor sensor like the KY-018.
The luminance is displayed in both the Info section of the web UI as well as published to the `/luminance` MQTT topic, if enabled.

## Installation

Copy the example `platformio_override.ini` to the root directory.  This file should be placed in the same directory as `platformio.ini`.

### Define Your Options

* `USERMOD_SN_PHOTORESISTOR`                      - Enables this user mod. wled00\usermods_list.cpp
* `USERMOD_SN_PHOTORESISTOR_MEASUREMENT_INTERVAL` - Number of milliseconds between measurements. Defaults to 60000 ms
* `USERMOD_SN_PHOTORESISTOR_FIRST_MEASUREMENT_AT` - Number of milliseconds after boot to take first measurement. Defaults to 20000 ms
* `USERMOD_SN_PHOTORESISTOR_REFERENCE_VOLTAGE`    - Voltage supplied to the sensor. Defaults to 5v
* `USERMOD_SN_PHOTORESISTOR_ADC_PRECISION`        - ADC precision. Defaults to 10 bits
* `USERMOD_SN_PHOTORESISTOR_RESISTOR_VALUE`       - Resistor size, defaults to 10000.0 (10K Ohms)
* `USERMOD_SN_PHOTORESISTOR_OFFSET_VALUE`         - Offset value to report on. Defaults to 25

All parameters can be configured at runtime via the Usermods settings page.

## Project link

* [QuinLED-Dig-Uno](https://quinled.info/2018/09/15/quinled-dig-uno/) - Project link

### PlatformIO requirements

If you are using `platformio_override.ini`, you should be able to refresh the task list and see your custom task, for example `env:usermod_sn_photoresistor_d1_mini`.

## Change Log
