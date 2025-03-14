# QuinLED-An-Penta
The (un)official usermod to get the best out of the QuinLED-An-Penta (https://quinled.info/quinled-an-penta/), e.g. using the OLED and the SHT30 temperature/humidity sensor.

## Requirements
* "u8g2" by olikraus, v2.28 or higher: https://github.com/olikraus/u8g2
* "SHT85" by Rob Tillaart, v0.2 or higher: https://github.com/RobTillaart/SHT85

## Usermod installation
Simply copy the below block (build task) to your `platformio_override.ini` and compile WLED using this new build task. Or use an existing one, add the buildflag `-D QUINLED_AN_PENTA` and the below library dependencies.

ESP32 (**without** ethernet):
```
[env:custom_esp32dev_usermod_quinled_an_penta]
extends = env:esp32dev
build_flags = ${common.build_flags_esp32} -D WLED_RELEASE_NAME=ESP32 -D QUINLED_AN_PENTA
lib_deps = ${esp32.lib_deps}
    olikraus/U8g2@~2.28.8
    robtillaart/SHT85@~0.2.0
```

ESP32 (**with** ethernet):
```
[env:custom_esp32dev_usermod_quinled_an_penta]
extends = env:esp32dev
build_flags = ${common.build_flags_esp32} -D WLED_RELEASE_NAME=ESP32_Ethernet -D WLED_USE_ETHERNET -D QUINLED_AN_PENTA
lib_deps = ${esp32.lib_deps}
    olikraus/U8g2@~2.28.8
    robtillaart/SHT85@~0.2.0
```

## Some words about the (optional) OLED
This mod has been optimized for an SSD1306 driven 128x64 OLED. Using a smaller OLED or an OLED using a different driver will result in unexpected results.
I highly recommend using these "two color monochromatic OLEDs", which have the first 16 pixels in a different color than the other 48, e.g. a yellow/blue OLED.
Note: you _must_ use an **SPI** driven OLED, **not an i2c one**!

### Limitations combined with Ethernet
The initial development of this mod was done with a beta version of the QuinLED-An-Penta, which had a different IO layout for the OLED: The CS pin _was_ IO_0, but has been changed to IO27 with the first v1 public release. Unfortunately, IO27 is used by Ethernet boards, so WLED will not let you enable the OLED screen, if you're using it with Ethernet. Unfortunately, that makes the development I've done to support/show Ethernet information invalid, as it cannot be used.
However, (and I've not tried this, as I don't own a v1 board) you can modify this usermod and try to use IO27 for the OLED and share it with the Ethernet board. It is "just" the chip select pin, so there is a chance that both can coexist and use the same IO. You need to skip WLEDs PinManager for the CS pin, so WLED will not block using it. If you don't know how this works, don't change it. If you know what I'm talking about, try it and please let me know on the Intermit.Tech (QuinLED) Discord server: https://discord.gg/WdbAauG

### My OLED flickers after some time, what should I do?
That's a tricky one. During development I saw that the OLED sometimes starts to "drop out" / flicker and won't work anymore. This seems to be caused by the high PWM interference the board produces. It seems to lose its settings then doesn't know how to draw anymore. Turns out the only way to fix this is to call the libraries `begin()` method again which re-initializes the display.
If you're facing this issue, you can enable a setting which will call the `begin()` roughly every 60 seconds between page changes. This will make the page change take ~500ms, but will fix the display.


## Configuration
Navigate to the "Config" and then to the "Usermods" section. If you compiled WLED with `-D QUINLED_AN_PENTA`, you will see the config for it there:
* Enable-OLED:
  * What it does: Enables the optional SPI driven OLED that can be mounted to the 7-pin female header. Won't work with Ethernet, read above.
  * Possible values: Enabled/Disabled
  * Default: Disabled
* OLED-Use-Progress-Bars:
  * What it does: Toggle between showing percentage numbers or a progress-bar-like visualization for overall brightness and each LED channels brightness level
  * Possible values: Enabled/Disabled
  * Default: Disabled
* OLED-Flip-Screen-180:
  * What it does: Flips the screen 180°
  * Possible values: Enabled/Disabled
  * Default: Disabled
* OLED-Seconds-Per-Page:
  * What it does: Number of seconds the OLED should stay on one page before changing pages
  * Possible values: Enabled/Disabled
  * Default: 10
* OLED-Fix-Bugged-Screen:
  * What it does: Enable this if your OLED flickers after some time. For more info read above under ["My OLED flickers after some time, what should I do?"](#My-OLED-flickers-after-some-time-what-should-I-do)
  * Possible values: Enabled/Disabled
  * Default: Disabled
* Enable-SHT30-Temp-Humidity-Sensor:
  * What it does: Enables the onboard SHT30 temperature and humidity sensor
  * Possible values: Enabled/Disabled
  * Default: Disabled

## Change log
2021-12
* Adjusted IO layout to match An-Penta v1r1
2021-10
* First implementation.

## Credits
ezcGman | Andy: Find me on the Intermit.Tech (QuinLED) Discord server: https://discord.gg/WdbAauG
