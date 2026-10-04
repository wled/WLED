# Rotary Encoder UI Usermod ALT

This usermod supports the UI of the `usermod_v2_rotary_encoder_ui_ALT`.

## Functionalities

Press the encoder to cycle through the options:

* Brightness
* Speed
* Intensity
* Palette
* Effect
* Main Color (only if display is used)
* Saturation (only if display is used)

Press and hold the encoder to display Network Info. If AP is active, it will display the AP, SSID and Password

Also shows if the timer is enabled.

[See the pair of usermods in action](https://www.youtube.com/watch?v=ulZnBt9z3TI)

## Installation

Copy the example `platformio_override.sample.ini` to the root directory of your particular build.

### Define Your Options

* `ENCODER_DT_PIN`                  - defaults to 18
* `ENCODER_CLK_PIN`                 - defaults to 5
* `ENCODER_SW_PIN`                  - defaults to 19
* `USERMOD_ROTARY_ENCODER_GPIO`     - GPIO functionality:
                                        `INPUT_PULLUP` to use internal pull-up
                                        `INPUT` to use pull-up on the PCB

### PlatformIO requirements

No special requirements.

## Change Log

2021-10

* First public release
