# ADS1115 Usermod

Reads values from an ADS1115 16-bit ADC and exposes them in the `Info` tab.

## Features
- Reads values from an ADS1115 over I2C.
- Supports 8 ADS1115 input modes:
	- 4 single-ended inputs (`AIN0` to `AIN3`)
	- 4 differential pairs (`AIN0-AIN1`, `AIN0-AIN3`, `AIN1-AIN3`, `AIN2-AIN3`)
- Per-channel configuration in the Usermod settings:
	- Enable/disable
	- Display name
	- Units
	- Multiplier and offset
	- Decimal precision
- Configurable measurement loop interval.
- Publishes configured channel values to the `Info` tab.

## Compatibility
- Requires an ADS1115 module connected via I2C.
- Works on targets with I2C support.
- Default ADC gain is `1x` (input range `+/-4.096V`).

## Installation
- Add `ADS1115` to `custom_usermods` in your `platformio.ini` (or `platformio_override.ini`).

## Author
- Dima Zhemkov [@dima-zhemkov](https://github.com/dima-zhemkov)

