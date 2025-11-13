# Chase Race Usermod

This folder hosts the dedicated usermod for the **Chase Race** effect. The
effect renders three "cars" (solid color bars) that run nose-to-tail along the
active segment. Blank pixels between cars simulate the open road, and once the
lead car crosses the finish line the entire pack immediately wraps to the
start, creating an endless race loop.

## Files

- `chase_race_usermod.cpp` registers the usermod/effect and implements the race
  logic.
- `library.json` keeps PlatformIO happy (`libArchive=false` ensures the code is
  linked directly into the firmware).

## Building

1. Select the `chase_race` PlatformIO environment (already the default in the
   workspace-level `platformio.ini`).
2. Compile/upload with `pio run -e chase_race` or from the IDE.

The environment enables the `USERMOD_CHASE_RACE` build flag and instructs
`pio-scripts/load_usermods.py` to include this folder automatically.

## Effect controls

- **Speed slider ("Pace")** - how fast the convoy advances.
- **Intensity slider ("Car length")** - LED length of each car.
- **Custom 1 slider ("Gap size")** - blank spacing between cars. If there
  aren't enough pixels to honor the requested spacing, the usermod shrinks the
  gaps automatically (down to zero when physically necessary).
- **Color slots ("Car 1 / Car 2 / Car 3")** - individual body colors. Using the
  same color for multiple cars is perfectly fine.

Enable the Chase Race effect from the WLED UI or JSON API and it will animate
the currently selected segment using these controls.
