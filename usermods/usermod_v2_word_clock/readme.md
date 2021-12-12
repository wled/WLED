# Word Clock Usermod V2

This usermod can be used to drive a wordclock with a 11x10 pixel matrix with WLED. There are also 4 additional dots for the minutes. 
The visualisation is desribed in 4 mask with LED numbers (single dots for minutes, minutes, hours and "clock/Uhr").
There are 2 parameters to chnage the behaviour:
 
active: enable/disable usermod
diplayItIs: enable/disable display of "Es ist" on the clock.

## Installation

Copy and update the example `platformio_override.ini.sample` 
from the Rotary Encoder UI usermode folder to the root directory of your particular build.
This file should be placed in the same directory as `platformio.ini`.

### Define Your Options

* `USERMOD_WORDCLOCK`   - define this to have this the Auto Save usermod included wled00\usermods_list.cpp

### PlatformIO requirements

No special requirements.

## Change Log

2022/03/30 initial commit