# Clock Overlay usermod

This usermod displays a digital clock on top of the WLED effects. Since it combines a clock with pixel we named it Cloxel.
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
- 2D Configuration 
    - Use the Matrix generator:
        - Select 2D Matrix
        - 1 Panel
        - 1st LED is either Bottom-Right or Top-Left (depends on how you put in the matrix)
        - Orientation: Vertical
        - Make sure Serpentine is selected
        - Dimensions are 32x8
        - Offset X and Y are both 0

## Release notes

2025-06 Initial implementation by @myriadbits (AKA Jochem Bakker)
