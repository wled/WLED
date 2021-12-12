# ST7789 TFT IPS Color display 240x240pxwith ESP32 boards

This usermod allow to use 240x240 display to display following:

* Network SSID;
* IP address;
* Brightness;
* Chosen effect;
* Chosen palette;
* Estimated current in mA;

## Hardware

***
![Hardware](images/ST7789_Guide.jpg)

## Library used

[Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)

## Setup

***

### Platformio.ini changes

In the `platformio.ini` file, uncomment the `TFT_eSPI` line within the [common] section, under `lib_deps`:

```ini
# platformio.ini
...
[common]
...
lib_deps =
    ...
  #For use of the TTGO T-Display ESP32 Module with integrated TFT display uncomment the following line  
    #TFT_eSPI
...
```

Also, while in the `platformio.ini` file, you must change the environment setup to build for just the esp32dev platform as follows:

Add lines to section:

```ini
default_envs = esp32dev
build_flags = ${common.build_flags_esp32}
  -D USERMOD_ST7789_DISPLAY

```

Save the `platformio.ini` file.  Once this is saved, the required library files should be automatically downloaded for modifications in a later step.

### TFT_eSPI Library Adjustments

We need to modify a file in the `TFT_eSPI` library. If you followed the directions to modify and save the `platformio.ini` file above, the `User_Setup_Select.h` file can be found in the `/.pio/libdeps/esp32dev/TFT_eSPI` folder.

Modify the  `User_Setup_Select.h` file as follows:

* Comment out the following line (which is the 'default' setup file):

```ini
//#include <User_Setup.h>           // Default setup is root library folder
```

* Add following line:

```ini
#include <User_Setups/Setup_ST7789_Display.h>    // Setup file for ESP32 ST7789V SPI bus TFT
```

* Copy file `"Setup_ST7789_Display.h"` from usermod folder to `/.pio/libdeps/esp32dev/TFT_eSPI/User_Setups`
