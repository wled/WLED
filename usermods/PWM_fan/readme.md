# PWM fan

v2 Usermod to to control PWM fan with RPM feedback and temperature control

This usermod requires the Dallas Temperature usermod to obtain temperature information. If it's not available, the fan will run at 100% speed.
If the fan does not have _tachometer_ (RPM) output you can set the _tachometer-pin_ to -1 to disable that feature.

You can also set the threshold temperature at which fan runs at lowest speed. If the measured temperature is 3°C greater than the threshold temperature, the fan will run at 100%.

If the _tachometer_ is supported, the current speed (in RPM) will be displayed on the WLED Info page.

## Installation

Add the compile-time option `-D USERMOD_PWM_FAN` to your `platformio.ini` (or `platformio_override.ini`) or use `#define USERMOD_PWM_FAN` in `myconfig.h`.
You will also need `-D USERMOD_DALLASTEMPERATURE`.

### Define Your Options

All of the parameters are configured during run-time using Usermods settings page.
This includes:

* PWM output pin (can be configured at compile time `-D PWM_PIN=xx`)
* tachometer input pin (can be configured at compile time `-D TACHO_PIN=xx`)
* sampling frequency in seconds
* threshold temperature in degrees Celsius

_NOTE:_ You may also need to tweak Dallas Temperature usermod sampling frequency to match PWM fan sampling frequency.

### PlatformIO requirements

No special requirements.

## Control PWM fan speed using JSON API

e.g. you can use `{"PWM-fan":{"speed":30,"lock":true}}` to lock fan speed to 30 percent of maximum. (replace 30 with an arbitrary value between 0 and 100)
If you include `speed` property you can set fan speed as a percentage (%) of maximum speed.
If you include `lock` property you can lock (_true_) or unlock (_false_) the fan speed.
If the fan speed is unlocked, it will revert to temperature controlled speed on the next update cycle. Once fan speed is locked it will remain so until it is unlocked by the next API call.

## Change Log

2021-10
* First public release
2022-05
* Added JSON API call to allow changing of speed
