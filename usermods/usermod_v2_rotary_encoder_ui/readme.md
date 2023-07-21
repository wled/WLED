# Rotary Encoder UI Usermod

First, thanks to the authors of other Rotary Encoder usermods.

This usermod starts to provide a relatively complete on-device
UI when paired with the Four Line Display usermod. I strongly
encourage you to try them together.

[See the pair of usermods in action](https://www.youtube.com/watch?v=tITQY80rIOA)

## Installation

Copy and update the example `platformio_override.ini.sample` to the root directory of your particular build.
This file should be placed in the same directory as `platformio.ini`.

### Define Your Options

* `USERMOD_ROTARY_ENCODER_UI`       - define this to have this user mod included wled00\usermods_list.cpp
* `USERMOD_ROTARY_ENCODER_GPIO`     - define the GPIO function (INPUT, INPUT_PULLUP, etc...)
* `USERMOD_FOUR_LINE_DISPLAY`       - define this to have this the Four Line Display mod included wled00\usermods_list.cpp
                                        also tells this usermod that the display is available
                                        (see the Four Line Display usermod `readme.md` for more details)
* `ENCODER_DT_PIN`                  &nbsp;&nbsp;- defaults to 12
* `ENCODER_CLK_PIN`                 - defaults to 14
* `ENCODER_SW_PIN`                  &nbsp;&nbsp;- defaults to 13
* `USERMOD_ROTARY_ENCODER_GPIO`     - GPIO functionality:
                                        `INPUT_PULLUP` to use internal pull-up
                                        `INPUT` to use pull-up on the PCB

### PlatformIO requirements

No special requirements.

Note: the Four Line Display usermod requires the libraries `U8g2` and `Wire`.

## Change Log

2021-02
* First public release
