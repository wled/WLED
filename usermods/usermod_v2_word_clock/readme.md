# Word Clock Usermod V2

This usermod drives an 11x10 pixel matrix wordclock with WLED. There are 4 additional dots for the minutes.
The visualisation is described by 4 masks with LED numbers (single dots for minutes, minutes, hours and "clock"). The index of the LEDs in the masks always starts at 0, even if the ledOffset is not 0.
There are 6 parameters that control behavior:

active: enable/disable usermod
diplayItIs: enable/disable display of "Es ist" on the clock
ledOffset: number of LEDs before the wordclock LEDs
Meander wiring?: enable/disable the alternative meander wiring pattern (see below)
Norddeutsch: show "Viertel nach" / "Viertel vor" instead of "Viertel" / "Dreiviertel"
minuteDots: enable/disable the 4 minute dots (default: enabled). Disable this if your clock has no minute dots, e.g. when LEDs after the 11x10 matrix are used for something else like a backlight. The usermod then leaves these LEDs untouched.

## Update for alternative wiring pattern

Based on this fantastic work I added an alternative wiring pattern.
The original used a long wire to connect DO to DI, from one line to the next line.

I wired my clock in meander style. So the first LED in the second line is on the right.
With this method, every other line was inverted and showed the wrong letter.

I added a switch in usermod called "meander wiring?" to enable/disable the alternate wiring pattern.

## Installation

Copy and update the example `platformio_override.ini.sample`
from the Rotary Encoder UI usermod folder to the root directory of your particular build.
This file should be placed in the same directory as `platformio.ini`.

### Define Your Options

* `USERMOD_WORDCLOCK`   - define this to have this usermod included wled00\usermods_list.cpp

### PlatformIO requirements

No special requirements.

## Change Log

2026/11/04 added minuteDots setting for clocks without minute dots.

2022/08/18 added meander wiring pattern.

2022/03/30 initial commit
