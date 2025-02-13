# Update Brightness Follow Sun

This UserMod can set brightness by mapping [minimum-maximum-minimum] from [sunrise-suntop-sunset], I use this UserMod to adjust the brightness of my plant growth light (pwm led), and I think it will make my plants happy.

This UserMod will adjust brightness from sunrise to sunset, reaching maximum brightness at the zenith of the sun. It can also maintain the lowest brightness within 0-6 hours before sunrise and after sunset according to the settings.

## Installation

define `USERMOD_BRIGHTNESS_FOLLOW_SUN` e.g. `#define USERMOD_BRIGHTNESS_FOLLOW_SUN` in my_config.h

or add `-D USERMOD_BRIGHTNESS_FOLLOW_SUN` to `build_flags` in platformio_override.ini


### Options
Open Usermod Settings in WLED to change settings:

`Enable` - When checked `Enable`, turn on the `Brightness Follow Sun` Usermod, which will automatically turn on the lights, adjust the brightness, and turn off the lights. If you need to completely turn off the lights, please unchecked `Enable`.

`Update Interval Sec` - The unit is seconds, and the brightness will be automatically refreshed according to the set parameters.

`Min Brightness` - set brightness by map of min-max-min : sunrise-suntop-sunset

`Max Brightness` - It needs to be set to a value greater than `Min Brightness`, otherwise it will always remain at `Min Brightness`.

`Relax Hour` - The unit is in hours, with an effective range of 0-6. According to the settings, maintain the lowest brightness for 0-6 hours before sunrise and after sunset.


### PlatformIO requirements

No special requirements.

## Change Log

2025-01-02
* init
