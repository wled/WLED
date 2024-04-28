# I2C 4 Line Display Usermod

First, thanks to the authors of the ssd11306_i2c_oled_u8g2 mod.

Provides a four line display using either
128x32 or 128x64 OLED displays.
It can operate independently, but starts to provide
a relatively complete on-device UI when paired with the 
Rotary Encoder UI usermod. I strongly encourage you to use 
them together.

[See the pair of usermods in action](https://www.youtube.com/watch?v=tITQY80rIOA)

## Installation

Copy and update the example `platformio_override.ini.sample` 
from the Rotary Encoder UI usermode folder to the root directory of your particular build.
This file should be placed in the same directory as `platformio.ini`.

### Define Your Options

* `USERMOD_FOUR_LINE_DISPLAY`  - define this to have this mod included wled00\usermods_list.cpp - also tells Rotary Encoder usermod, if installed, the display is available
* `FLD_PIN_SCL`                - The display SCL pin, defaults to 5
* `FLD_PIN_SDA`                - The display SDA pin, defaults to 4

All of the parameters can be configured via the Usermods settings page, including GPIO pins.

### PlatformIO requirements

This usermod requires the `U8g2` and `Wire` libraries. See the 
`platformio_override.ini.sample` found in the Rotary Encoder
UI usermod folder for how to include these using `platformio_override.ini`.

## Configuration

* `enabled` - enable/disable usermod
* `pin` - GPIO pins used for display; I2C displays use Clk & Data; SPI displays can use SCK, MOSI, CS, DC & RST
* `type` - display type in numeric format
    * 1 = I2C SSD1306 128x32
    * 2 = I2C SH1106 128x32
    * 3 = I2C SSD1306 128x64 (4 double-height lines)
    * 4 = I2C SSD1305 128x32
    * 5 = I2C SSD1305 128x64 (4 double-height lines)
    * 6 = SPI SSD1306 128x32
    * 7 = SPI SSD1306 128x64 (4 double-height lines)
* `contrast` - set display contrast (higher contrast may reduce display lifetime)
* `refreshRateSec` - display refresh time in seconds
* `screenTimeOutSec` - screen saver time-out in seconds
* `flip` - flip/rotate display 180°
* `sleepMode` - enable/disable screen saver
* `clockMode` - enable/disable clock display in screen saver mode
* `i2c-freq-kHz` - I2C clock frequency in kHz (may help reduce dropped frames, range: 400-3400)

## Change Log

2021-02
* First public release

2021-04
* Adaptation for runtime configuration.

2021-11
* Added configuration option description.
