# Clock Overlay usermod

This usermod displays a digital clock on top of the WLED effects.
It works best using a 32x8 matrix LED. 
TODO => on a 16x16 matrix the characters won't fit sideways, so use the 'Hours over minutes'option to display the hours at the top left and the minutes in the right bottom corners.
TODO => make cloxel website

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


All parameters are runtime configurable.



## Release notes

2025-05 Initial implementation by @myriadbits (AKA Jochem Bakker)
