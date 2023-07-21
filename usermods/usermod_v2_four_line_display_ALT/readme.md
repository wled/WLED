# I2C 4 Line Display Usermod ALT

Thank you to the authors of the original version of these usermods. It would not have been possible without them!
"usermod_v2_four_line_display"
"usermod_v2_rotary_encoder_ui"

The core of these usermods are a copy of the originals. The main changes are to the FourLineDisplay usermod.
The display usermod UI has been completely changed.


The changes made to the RotaryEncoder usermod were made to support the new UI in the display usermod. 
Without the display it, functions identical to the original.
The original "usermod_v2_auto_save" will not work with the display just yet.

Press the encoder to cycle through the options:
    *Brightness
    *Speed
    *Intensity
    *Palette
    *Effect
    *Main Color (only if display is used)
    *Saturation (only if display is used)

Press and hold the encoder to display Network Info
    if AP is active, it will display AP, SSID and password

Also shows if the timer is enabled

[See the pair of usermods in action](https://www.youtube.com/watch?v=ulZnBt9z3TI)

## Installation

Please refer to the original `usermod_v2_rotary_encoder_ui` readme for the main instructions
Then to activate this alternative usermod add `#define USE_ALT_DISPlAY` to the `usermods_list.cpp` file,
                                        or add `-D USE_ALT_DISPlAY` to the original `platformio_override.ini.sample` file


### PlatformIO requirements

Note: the Four Line Display usermod requires the libraries `U8g2` and `Wire`.

## Change Log

2021-10
* First public release
