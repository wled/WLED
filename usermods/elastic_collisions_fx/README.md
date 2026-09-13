## Description

**Elastic Collisions** simulates balls randomly hitting each other and bouncing elastically. Balls also bounce off the edges of a display. You can control; their Speed (velocity), Number of balls; 1-30, Uniformity 0-255, and regeneration time 15 seconds to 1 hour. Ball colors are random indices into the current palette.

Balls have a mass that is the cube of their diameter. Collisions conserve their energy and momentum as per the laws of physics.

A Uniformity of 255 is special: The balls are initialized with equal mass in a row and only one moving. When it collides with another ball, all momentum is transferred to the next ball and it stops, much like the swinging ball puzzle in "Professor T".

a few seconds before regenerating a new set, the wall sides "collapse" and the balls drift off the display.

It works very well on both 2D and 1D displays.

## Installation

To activate the usermod, add the following line to your platformio_override.ini
```ini
custom_usermods = elastic_collisions
```
Or if you are already using a usermod, append elastic_collisions to the list
```ini
custom_usermods = audioreactive elastic_collisions
```

You should now see "Elastic Collisions" appear in your effect list.

## Note

When you save an effect in *Presets*, it is saved as an ordinal effect number called "fx". These are fixed for standard effects, but may have a different value for effects that are usermods and how many you've included. So if you change your included usermods, this value may have to be revised in your presets.

## Parameters

1. **Speed** The average initial velocity of balls over a wide range.
2. **Count** 1-30 balls
3. **Uniformity** 0-100%. 100% uniformity is special as discussed above.
4. **Lifetime** Regeneration time from 15 seconds to 1 hour. 