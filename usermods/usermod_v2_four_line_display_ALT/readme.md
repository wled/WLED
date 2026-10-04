# I2C/SPI 4 Line Display Usermod ALT

This usermod could be used in compination with `usermod_v2_rotary_encoder_ui_ALT`.

## Functionalities

Press the encoder to cycle through the options:

* Brightness
* Speed
* Intensity
* Palette
* Effect
* Main Color
* Saturation

Press and hold the encoder to display Network Info. If AP is active, it will display the AP, SSID and Password

Also shows if the timer is enabled.

[See the pair of usermods in action](https://www.youtube.com/watch?v=ulZnBt9z3TI)

## Installation

Copy the example `platformio_override.sample.ini` to the root directory of your particular build.

## Configuration

These options are configurable in Config > Usermods

### Usermod Setup

* Global I2C GPIOs (HW) - Set the SDA and SCL pins

### 4LineDisplay

* `enabled` - enable/disable usermod
* `type` - display type in numeric format
  * 1 = I2C SSD1306 128x32
  * 2 = I2C SH1106 128x32
  * 3 = I2C SSD1306 128x64 (4 double-height lines)
  * 4 = I2C SSD1305 128x32
  * 5 = I2C SSD1305 128x64 (4 double-height lines)
  * 6 = SPI SSD1306 128x32
  * 7 = SPI SSD1306 128x64 (4 double-height lines)
  * 8 = SPI SSD1309 128x64 (4 double-height lines)
  * 9 = I2C SSD1309 128x64 (4 double-height lines)
* `pin` - GPIO pins used for display; SPI displays can use SCK, MOSI, CS, DC & RST
* `flip` - flip/rotate display 180°
* `contrast` - set display contrast (higher contrast may reduce display lifetime)
* `screenTimeOutSec` - screen saver time-out in seconds
* `sleepMode` - enable/disable screen saver
* `clockMode` - enable/disable clock display in screen saver mode
* `showSeconds` - Show seconds on the clock display
* `i2c-freq-kHz` - I2C clock frequency in kHz (may help reduce dropped frames, range: 400-3400)

### PlatformIO requirements

Note: the Four Line Display usermod requires the libraries `U8g2` and `Wire`.

## Change Log

2021-10

* First public release
