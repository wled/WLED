# Tetris AI effect usermod

This usermod adds a self-playing Tetris game as an 'effect'. The mod requires version 0.14 or higher as it relies on matrix support. The effect was tested on an ESP32 4MB with a WS2812B 16x16 matrix.

Version 1.0

## Installation 

Just activate the usermod with `-D USERMOD_TETRISAI` and the effect will become available under the name 'Tetris AI'. If you are running out of flash memory, use a different memory layout (e.g. [WLED_ESP32_4MB_256KB_FS.csv](https://github.com/wled-dev/WLED/blob/main/tools/WLED_ESP32_4MB_256KB_FS.csv)).

If needed simply add to `platformio_override.ini` (or `platformio_override.ini`):

```ini
board_build.partitions = tools/WLED_ESP32_4MB_256KB_FS.csv
```

## Usage

It is best to set the background color to black 🖤, the border color to light grey 🤍, the game over color (foreground) to dark grey 🩶, and color palette to 'Rainbow' 🌈.

### Sliders and boxes

#### Sliders

* speed: speed the game plays
* look ahead: how many pieces is the AI allowed to know the next pieces (0 - 2)
* intelligence: how good the AI will play
* Rotate color: make the colors shift (rotate) every few moves
* Mistakes free: how many good moves between mistakes (if enabled)

#### Checkboxes

* show next: if true, a space of 5 pixels from the right will be used to show the next pieces. Otherwise the whole segment is used for the grid.
* show border: if true an additional column of 1 pixel is used to draw a border between the grid and the next pieces
* mistakes: if true, the worst decision will be made every few moves instead of the best (see above).

## Best results

 If the speed is set to be a little bit faster than a good human could play with maximal intelligence and very few mistakes it makes people furious/happy at a party 😉.

## Limits
The game grid is limited to a maximum width of 32 and a maximum height of 255 due to the internal structure of the code. The canvas of the effect will be centred in the segment if the segment exceeds the maximum width or height.