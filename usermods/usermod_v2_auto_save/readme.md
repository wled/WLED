# Auto Save

v2 Usermod to automatically save settings
to preset number AUTOSAVE_PRESET_NUM after a change to any of:

* brightness
* effect speed
* effect intensity
* mode (effect)
* palette

but it will wait for AUTOSAVE_AFTER_SEC seconds,
a "settle" period in case there are other changes (any change will extend the "settle" period).

It will additionally load preset AUTOSAVE_PRESET_NUM at startup during the first `loop()`.

AutoSaveUsermod is standalone, but if FourLineDisplayUsermod is installed, it will notify the user of the saved changes.

Note: WLED doesn't respect the brightness of the preset being auto loaded, so the AutoSaveUsermod will set the AUTOSAVE_PRESET_NUM preset in the first loop, so brightness IS honored. This means WLED will effectively ignore Default brightness and Apply N preset at boot when the AutoSaveUsermod is installed.

## Installation

Copy and update the example `platformio_override.ini.sample`
from the Rotary Encoder UI usermode folder to the root directory of your particular build.
This file should be placed in the same directory as `platformio.ini`.

### Define Your Options

* `USERMOD_AUTO_SAVE`           - define this to have this usermod included wled00\usermods_list.cpp
* `AUTOSAVE_AFTER_SEC`          - define the delay time after the settings auto-saving routine should be executed
* `AUTOSAVE_PRESET_NUM`         - define the preset number used by autosave usermod
* `USERMOD_AUTO_SAVE_ON_BOOT`   - define if autosave should be enabled on boot
* `USERMOD_FOUR_LINE_DISPLAY`   - define this to have this the Four Line Display mod included wled00\usermods_list.cpp
                                    also tells this usermod that the display is available
                                    (see the Four Line Display usermod `readme.md` for more details)

Example to add in platformio_override:
  -D USERMOD_AUTO_SAVE
  -D AUTOSAVE_AFTER_SEC=10
  -D AUTOSAVE_PRESET_NUM=100
  -D USERMOD_AUTO_SAVE_ON_BOOT=true

You can also configure auto-save parameters using Usermods settings page.

### PlatformIO requirements

No special requirements.

Note: the Four Line Display usermod requires the libraries `U8g2` and `Wire`.

## Change Log

2021-02

* First public release

2021-04

* Adaptation for runtime configuration.
