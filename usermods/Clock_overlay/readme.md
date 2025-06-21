# Clock Overlay Usermod
This usermod displays a digital clock on top of the WLED effects. Since it combines a clock with pixels, we named it Cloxel.
See the following instructable for more information:
[Cloxel WLED Pixel Clock Instructable](https://www.instructables.com/Cloxel-WLED-Pixel-Clock/)

## Installation 
Add `clock_overlay` to `custom_usermods` in your PlatformIO environment.

## Configuration
Notes for configuring the 32x8 LED matrix:
- 2D Configuration, use the Matrix generator:
  - Select `2D Matrix`
  - Set `Number of panels` to `1`
  - 1st LED is either `Bottom-Right` or `Top-Left` (depends on how you put in the matrix)
  - Set `Orientation` to `Vertical`
  - Make sure `Serpentine` is selected
  - Dimensions are `32x8`
  - Offset `X` and `Y` are both `0`
- Under `Time and Macros` enable the `Get time from NTP server`
  - Select the appropriate `Time zone`

## User configuration
This user mod contains the following configuration items:
- `Segment Id`, select the correct segment to display the clock on.
- `Time Options`, select HH:MM or HH:MM:ss as display options.
- `Time Font`, select the font to use for the time display. Fonts are custom made fonts in different sizes.
- `Time Color`, enter the time color in RRGGBB hexadecimal format (FFFFFF for white).
- `Background Fade`, how much should the background (the WLED effects) be faded (0 = no WLED effects visible, 255 = WLED effects are not faded).
- `Message Options`, different blinking colors/options when displaying the message.

## Using the message functionality
On the 'state' URL api call: http://<IP address or localname>/json/state.
POST the following json data:
```json
{
  "clock_overlay": {
    "msg": "PIZZA!",
    "time": 3
  }
}
```
The text 'PIZZA!' will blink for 3 seconds in the configured pattern/colors. After 3s the normal time will be displayed again.

## Future work
- Make the Clock overlay compatible with 16x16 matrix as well. Add an option to display the hours at the top left and the minutes in the right bottom corners.
- Alternating weekday/date/time modes.
- Message integration into main WLED screen.

## Release notes
2025-06 Initial implementation by @myriadbits (AKA Jochem Bakker)