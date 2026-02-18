## Description

A 2D falling comet effect similar to "Matrix" but with a fire particle simulation to enhance the comet trail visuals. Works with custom color palettes, defaulting to "Fire". Supports "small" and "large" comets which are 1px and 3px wide respectively.

Demo: [https://imgur.com/a/i1v5WAy](https://imgur.com/a/i1v5WAy)

## Installation

To activate the usermod, add the following line to your platformio_override.ini
```ini
custom_usermods = ps_comet
```
Or if you are already using a usermod, append ps_comet to the list
```ini
custom_usermods = audioreactive ps_comet
```

You should now see "PS Comet" appear in your effect list.

## Parameters

1. **Falling Speed** sets how fast the comets fall
2. **Comet Frequency** determines how many comets are on screen at a time
3. **Large Comet Probability** determines how often large 3px wide comets spawn
4. **Comet Length** sets how far comet trails stretch vertically