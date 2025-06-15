# Clock Overlay usermod

This usermod displays a digital clock on top of the WLED effects. Since it combines a clock with pixels, we named it Cloxel.
See the following instructable for more information:
https://www.instructables.com/Cloxel-WLED-Pixel-Clock/

Future work:
- Make the Clock overlay compatible with 16x16 matrix as well. Add an option to display the hours at the top left and the minutes in the right bottom corners.
- Alternating weekday/date/time modes
- Message integration into main WLED screen 

## Installation 

Add `clock_overlay` to `custom_usermods` in your PlatformIO environment.

## Configuration

Notes for configuring the 32x8 LED matrix:
- 2D Configuration, use the Matrix generator:
  - Select 2D Matrix
  - 1 Panel
  - 1st LED is either Bottom-Right or Top-Left (depends on how you put in the matrix)
  - Orientation: Vertical
  - Make sure Serpentine is selected
  - Dimensions are 32x8
  - Offset X and Y are both 0

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
The text 'PIZZA!' will blink for 3 seconds in the configured pattern/colors. After 3s the normal time will be displayed again.

## Release notes

2025-06 Initial implementation by @myriadbits (AKA Jochem Bakker)
