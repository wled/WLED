# Rotary Encoder (Brightness and Color)

V2 usermod that enables changing brightness and color using a rotary encoder 
change between modes by pressing a button (many encoders have one included)

it will wait for AUTOSAVE_SETTLE_MS milliseconds. a "settle" 
period in case there are other changes (any change will 
extend the "settle" period).

It will additionally load preset AUTOSAVE_PRESET_NUM at startup.
during the first `loop()`.  Reasoning below.

AutoSaveUsermod is standalone, but if FourLineDisplayUsermod is installed, it will notify the user of the saved changes.

Note: WLED doesn't respect the brightness of the preset being auto loaded, so the AutoSaveUsermod will set the AUTOSAVE_PRESET_NUM preset in the first loop, so brightness IS honored. This means WLED will effectively ignore Default brightness and Apply N preset at boot when the AutoSaveUsermod is installed.

## Installation

define `USERMOD_ROTARY_ENCODER_BRIGHTNESS_COLOR` e.g.

`#define USERMOD_ROTARY_ENCODER_BRIGHTNESS_COLOR` in my_config.h

or add `-D USERMOD_ROTARY_ENCODER_BRIGHTNESS_COLOR` to `build_flags` in platformio_override.ini

### Define Your Options

Open Usermod Settings in WLED to change settings:

`fadeAmount` - how many points to fade the Neopixel with each step of the rotary encoder (default 5)
`pin[3]` - pins to connect to the rotary encoder:
- `pin[0]` is pin A on your rotary encoder
- `pin[1]` is pin B on your rotary encoder
- `pin[2]` is the button on your rotary encoder (optional, set to -1 to disable the button and the rotary encoder will control brightness only)

### PlatformIO requirements

No special requirements.

## Change Log

2021-07
* Upgraded to work with the latest WLED code, and make settings configurable in Usermod Settings
