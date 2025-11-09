# Usermod user FX

This usermod is a common place to put various users’ WLED effects. It lets you load your own custom effects or bring back deprecated ones—without touching core WLED source code.

Multiple Effects can be specified inside this single usermod, as we will illustrate below.  You will be able to define them with custom names, sliders, etc. as with any other Effect.

* [How The Usermod Works](./README.md#how-the-usermod-works)
* [Basic Syntax for WLED Effect Creation](./README.md#basic-syntax-for-wled-effect-creation)
* [Understanding 2D WLED Effects](./README.md#understanding-2d-wled-effects)
* [The Metadata String](./README.md#the-metadata-string)
* [Understanding 1D WLED Effects](./README.md#understanding-1d-wled-effects)
* [Combining Multiple Effects in this Usermod](./README.md#combining-multiple-effects-in-this-usermod)
* [Compiling](./README.md#compiling)
* [Change Log](./README.md#change-log)
* [Contact Us](./README.md#contact-us)

## How The Usermod Works

The `user_fx.cpp` file can be broken down into four main parts:
* **static effect definition** - This is a static LED setting that is displayed if an effect fails to initialize.
* **User FX function definition(s)** - This area is where you place the FX code for all of the custom effects you want to use.  This mainly includes the FX code and the static variable containing the [metadata string](https://kno.wled.ge/interfaces/json-api/#effect-metadata). 
* **Usermod Class definition(s)** - The class definition defines the blueprint from which all your custom Effects (or any usermod, for that matter) are created.
* **Usermod registration** - All usermods have to be registered so that they are able to be compiled into your binary.

We will go into greater detail on how custom effects work in the usermod and how to go about creating your own in the section below.


## Basic Syntax for WLED Effect Creation

WLED effects generally follow a certain procedure for their operation:
1. Determine dimension of segment
2. Calculate new state if needed
3. Implement a loop that calculates color for each pixel and sets it using `SEGMENT.setPixelColor()`
4. The function is called at current frame rate.

Below are some helpful variables and functions to know as you start your journey towards WLED effect creation:

| Syntax Element                                  | Size   | Description |
| :---------------------------------------------- | :----- | :---------- |
| [`SEGMENT.speed / intensity / custom1 / custom2`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX.h#L450)   | 8-bit  | These read-only variables help you control aspects of your custom effect using the UI sliders.  You can edit these variables through the UI sliders when WLED is running your effect. (These variables can be controlled by the API as well.) Note that while `SEGMENT.intensity` through `SEGMENT.custom2` are 8-bit variables, `SEGMENT.custom3` is actually 5-bit.  The other three bits are used by the boolean parameters `SEGMENT.check1` through `SEGMENT.check3` and are bit-packed to conserve data size and memory. |
| [`SEGMENT.custom3`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX.h#L454) | 5-bit  | Another optional UI slider for custom effect control.  While `SEGMENT.speed` through `SEGMENT.custom2` are 8-bit variables, `SEGMENT.custom3` is actually 5-bit.  |
| [`SEGMENT.check1 / check2 / check3`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX.h#L455) | 1-bit  | These variables are boolean parameters which show up as checkbox options in the User Interface. They are bit-packed along with `SEGMENT.custom3` to conserve data size and memory.  |
| [`SEGENV.aux0 / aux1`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX.h#L467) | 16-bit | These are state variables that persists between function calls, and they are free to be overwritten by the user for any use case. |
| [`SEGENV.step`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX.h#L465) | 32-bit | This is a timestamp variable that contains the last update time.  It is initially set during effect initialization to 0, and then it updates with the elapsed time after each frame runs. |
| [`SEGENV.call`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX.h#L466) | 32-bit | A counter for how many times this effect function has been invoked since it started. |
| [`strip.now`](https://github.com/wled/WLED/blob/main/wled00/FX.h)                  | 32-bit | Current timestamp in milliseconds.  (Equivalent to `millis()`, but use `strip.now()` instead.)  `strip.now` respects the timebase, which can be used to advance or reset effects in a preset.  This can be useful to sync multiple segments. |
| [`SEGLEN / SEG_W / SEG_H`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX.h#L116) | 16-bit | These variables are macros that help define the length and width of your LED strip/matrix segment. |
| [`SEGPALETTE`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX.h#L115) | --- | Macro that gets the currently selected palette for the currently processing segment. |
| [`hw_random8()`](https://github.com/wled/WLED/blob/7b0075d3754fa883fc1bbc9fbbe82aa23a9b97b8/wled00/fcn_declare.h#L548) | 8-bit  | One of several functions that generates a random integer.  (All of the "hw_" functions are similar to the FastLED library's random functions, but in WLED they use true hardware-based randomness instead of a pseudo random number. In short, they are better and faster.) |
| [`SEGCOLOR(x)`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX.h#L114) | 32-bit | Macro that gets user-selected colors from UI, where x is an integer 1, 2, or 3 for primary, secondary, and tertiary colors, respectively. |
| [`SEGMENT.setPixelColor`](https://github.com/WLED/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX_fcn.cpp) / [`setPixelColorXY`](https://github.com/WLED/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX_2Dfcn.cpp) | 32-bit | Function that paints one pixel. `setPixelColor` is 1‑D; `setPixelColorXY` expects `(x, y)` and an RGBW color value. |
| [`SEGMENT.color_wheel()`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX_fcn.cpp#L1092) | 32-bit | Input 0–255 to get a color. Transitions r→g→b→r. In HSV terms, `pos` is H. Note: only returns palette color unless the Default palette is selected. |
| [`SEGMENT.color_from_palette()`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX_fcn.cpp#L1093) | 32-bit | Gets a single color from the currently selected palette for a segment. (This function which should be favoured over `ColorFromPalette()` because this function returns an RGBW color with white from the `SEGCOLOR` passed, while also respecting the setting for palette wrapping.  On the other hand, `ColorFromPalette()` simply gets the RGB palette color.) |
| [`fade_out()`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX_fcn.cpp#L1012) | ---    | fade out function, higher rate = quicker fade.  fading is highly dependent on frame rate (higher frame rates, faster fading).  each frame will fade at max 9% or as little as 0.8%.  |
| [`fadeToBlackBy()`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX_fcn.cpp#L1043) | ---    | can be used to fade all pixels to black.  |
|  [`fadeToSecondaryBy()`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX_fcn.cpp#L1043) | ---    | fades all pixels to secondary color.  |
| [`move()`](https://github.com/WLED/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX_fcn.cpp) | ---    | Moves/shifts pixels in the desired direction. |
| [`blur / blur2d`](https://github.com/wled/WLED/blob/75f6de9dc29fc7da5f301fc1388ada228dcb3b6e/wled00/FX_fcn.cpp#L1053) | ---    | Blurs all pixels for the desired segment. Blur also has the boolean option `smear`, which, when activated, does not fade the blurred pixel(s). |

 You will see how these syntax elements work in the examples below.



## Understanding 2D WLED Effects

In this section we give some advice to those who are new to WLED Effect creation.  We will illustrate how to load in multiple Effects using this single usermod, and we will do a deep dive into the anatomy of a 1D Effect as well as a 2D Effect.
(Special thanks to @mryndzionek for offering this "Diffusion Fire" 2D Effect for this tutorial.)

### Imports
The first line of the code imports the [wled.h](https://github.com/wled/WLED/blob/main/wled00/wled.h) file into this module. Importing `wled.h` brings all of the variables, files, and functions listed in the table above (and more) into your custom effect for you to use.

```cpp
#include "wled.h"
```

### Static Effect Definition
The next code block is the `mode_static` definition.  This is usually left as `SEGMENT.fill(SEGCOLOR(0));` to leave all pixels off if the effect fails to load, but in theory one could use this as a 'fallback effect' to take on a different behavior, such as displaying some other color instead of leaving the pixels off.

### User Effect Definitions
Pre-loaded in this template is an example 2D Effect called "Diffusion Fire".  (This is the name that would be shown in the UI once the binary is compiled and run on your device, as defined in the metadata string.)
The effect starts off by checking to see if the segment that the effect is being applied to is a 2D Matrix, and if it is not, then it returns the static effect which displays no pattern:
```cpp
if (!strip.isMatrix || !SEGMENT.is2D()) 
return mode_static();  // not a 2D set-up 
```
The next code block contains several constant variable definitions which essentially serve to extract the dimensions of the user's 2D matrix and allow WLED to interpret the matrix as a 1D coordinate system (WLED must do this for all 2D animations):
```cpp
const int cols = SEG_W;
const int rows = SEG_H;
const auto XY = [&](int x, int y) { return x + y * cols; };
```
* The first line assigns the number of columns (width) in the active segment to cols.
  * SEG_W is a macro defined in WLED that expands to SEGMENT.width().  This value is the width of your 2D matrix segment, used to traverse the matrix correctly.
* Next, we assign the number of rows (height) in the segment to rows.
  * SEG_H is a macro for SEGMENT.height(). Combined with cols, this allows pixel addressing in 2D (x, y) space.
* The third line declares a lambda function named `XY` to map (x, y) matrix coordinates into a 1D index in the LED array.  This assumes row-major order (left to right, top to bottom).
  * This lambda helps with mapping a local 1D array to a 2D one.

The next lines of code further the setup process by defining variables that allow the effect's settings to be configurable using the UI sliders (or alternatively, through API calls):
```cpp
const uint8_t refresh_hz = map(SEGMENT.speed, 0, 255, 20, 80);
const unsigned refresh_ms = 1000 / refresh_hz;
const int16_t diffusion = map(SEGMENT.custom1, 0, 255, 0, 100);
const uint8_t spark_rate = SEGMENT.intensity;
const uint8_t turbulence = SEGMENT.custom2;
```
* The first line maps the SEGMENT.speed (user-controllable parameter from 0–255) to a value between 20 and 80 Hz.
  * This determines how often the effect should refresh per second (Higher speed = more frames per second).
* Next we convert refresh rate from Hz to milliseconds. (It’s easier to schedule animation updates in WLED using elapsed time in milliseconds.)
  * This value is used to time when to update the effect.
* The third line utilizes the `custom1` control (0–255 range, usually exposed via sliders) to define the diffusion rate, mapped to 0–100.
  * This controls how much "heat" spreads to neighboring pixels — more diffusion = smoother flame spread.
* Next we assign `SEGMENT.intensity` (user input 0–255) to a variable named `spark_rate`.
  * This controls how frequently new "spark" pixels appear at the bottom of the matrix.
  * A higher value means more frequent ignition of flame points.
* The final line stores the user-defined `custom2` value to a variable called `turbulence`.
  * This is used to introduce randomness in spark generation or flow — more turbulence means more chaotic behavior.

Next we will look at some lines of code that handle memory allocation and effect initialization:

```cpp
unsigned dataSize = cols * rows;  // SEGLEN (virtual length) is equivalent to vWidth()*vHeight() for 2D
```
* This part calculates how much memory we need to represent per-pixel state.
  * `cols * rows` or `(or SEGLEN)` returns the total number of pixels in the current segment.
  * This fire effect models heat values per pixel (not just colors), so we need persistent storage — one uint8_t per pixel — for the entire effect.
  > **_NOTE:_** Virtual lengths `vWidth()` and `vHeight()` will be evaluated differently based on your own custom effect, and based on what other settings are active.  For example: If you have an LED strip of length = 60 and you enable grouping = 2, then the virtual length will be 30, so the FX will render 30 pixels instead of 60.  This is also true for mirroring or adding gaps--it halves the size.  For a 1D strip mapped to 2D, the virtual length depends on selected mode.  Keep these things in mind during your custom effect's creation.

```cpp
if (!SEGENV.allocateData(dataSize))
return mode_static(); // allocation failed
```
* Upon the first call, this section allocates a persistent data buffer tied to the segment environment (`SEGENV.data`). All subsequent calls simply ensure that the data is still valid.
* The syntax `SEGENV.allocateData(n)` requests a buffer of size n bytes (1 byte per pixel here).
* If allocation fails (e.g., out of memory), it returns false, and the effect can’t proceed.
* It calls previously defined `mode_static()` fallback effect, which just fills the segment with a static color.  We need to do this because WLED needs a fail-safe behavior if a custom effect can't run properly due to memory constraints.


The next lines of code clear the LEDs and initialize timing:
```cpp
if (SEGENV.call == 0) {
  SEGMENT.fill(BLACK);
  SEGENV.step = 0;
}
```
* The first line checks whether this is the first time the effect is being run; `SEGENV.call` is a counter for how many times this effect function has been invoked since it started.
* If `SEGENV.call` equals 0 (which it does on the very first call, making it useful for initialization), then it clears the LED segment by filling it with black (turns off all LEDs).
* This gives a clean starting point for the fire animation.
* It also initializes `SEGENV.step`, a timing marker, to 0.  This value is later used as a timestamp to control when the next animation frame should occur (based on elapsed time).
  

The next block of code is where the animation update logic starts to kick in:
```cpp
if ((strip.now - SEGENV.step) >= refresh_ms) {
  uint8_t tmp_row[cols];  // Keep for ≤~1 KiB; otherwise consider heap or reuse SEGENV.data as scratch.
  SEGENV.step = strip.now;
  // scroll up
  for (unsigned y = 1; y < rows; y++)
    for (unsigned x = 0; x < cols; x++) {
      unsigned src = XY(x, y);
      unsigned dst = XY(x, y - 1);
      SEGENV.data[dst] = SEGENV.data[src];
    }
```
* The first line checks if it's time to update the effect frame.  `strip.now` is the current timestamp in milliseconds; `SEGENV.step` is the last update time (set during initialization or previous frame).  `refresh_ms` is how long to wait between frames, computed earlier based on SEGMENT.speed.
* The conditional statement in the first line of code ensures the effect updates on a fixed interval — e.g., every 20 ms for 50 Hz.
* The second line of code declares a temporary row buffer for intermediate diffusion results that is one byte per column (horizontal position), so this buffer holds one row's worth of heat values.
* You'll see later that it writes results here before updating `SEGENV.data`.
  * Note: this is allocated on the stack each frame. Keep such VLAs ≤ ~1 KiB; for larger sizes, prefer a buffer in `SEGENV.data`.

> **_IMPORTANT NOTE:_** Creating variable‑length arrays (VLAs) is non‑standard C++, but this practice is used throughout WLED and works in practice. But be aware that VLAs live on the stack, which is limited. If the array scales with segment length (1D), it can overflow the stack and crash. Keep VLAs ≲ ~1 KiB; an array with 4000 LEDs is ~4 KiB and will likely crash. It’s worse with `uint16_t`. Anything larger than ~1 KiB should go into `SEGENV.data`, which has a higher limit.


Now we get to the spark generation portion, where new bursts of heat appear at the bottom of the matrix:
```cpp
if (hw_random8() > turbulence) {
  // create new sparks at bottom row
  for (unsigned x = 0; x < cols; x++) {
    uint8_t p = hw_random8();
    if (p < spark_rate) {
      unsigned dst = XY(x, rows - 1);
      SEGENV.data[dst] = 255;
    }
  }
}
```
* The first line randomizes whether we even attempt to spawn sparks this frame.
  * `hw_random8()` gives a random number between 0–255 using a fast hardware RNG.
  * `turbulence` is a user-controlled parameter (SEGMENT.custom2, set earlier).
  * Higher turbulence means this block is less likely to run (because `hw_random8()` is less likely to exceed a high threshold).
  * This adds randomness to when sparks appear — simulating natural flicker and chaotic fire.
* The next line loops over all columns in the bottom row (row `rows - 1`).
* Another random number, `p`, is used to probabilistically decide whether a spark appears at this (x, `rows-1`) position.
* Next is a conditional statement.  The lower spark_rate is, the fewer sparks will appear.
  * `spark_rate` comes from `SEGMENT.intensity` (0–255).
  * High intensity means more frequent ignition.
* `dst` calculates the destination index in the bottom row at column x.
* The final line here sets the heat at this pixel to maximum (255).
  * This simulates a fresh burst of flame, which will diffuse and move upward over time in subsequent frames.

Next we reach the first part of the core of the fire simulation, which is diffusion (how heat spreads to neighboring pixels):
```cpp
// diffuse
for (unsigned y = 0; y < rows; y++) {
  for (unsigned x = 0; x < cols; x++) {
    unsigned v = SEGENV.data[XY(x, y)];
    if (x > 0) {
      v += SEGENV.data[XY(x - 1, y)];
    }
    if (x < (cols - 1)) {
      v += SEGENV.data[XY(x + 1, y)];
    }
    tmp_row[x] = min(255, (int)(v * 100 / (300 + diffusion)));
  }
```
* This block of code starts by looping over each row from top to bottom.  (We will do diffusion for each pixel row.)
* Next we start an inner loop which iterates across each column in the current row.
* Starting with the current heat value of pixel (x, y) assigned `v`:
  * if there’s a pixel to the left, add its heat to the total.
  * If there’s a pixel to the right, add its heat as well.
  * So essentially, what the two `if` statements accomplish is: `v = center + left + right`.
* The final line of code applies diffusion smoothing:
  * The denominator controls how much the neighboring heat contributes. `300 + diffusion` means that with higher diffusion, you get more smoothing (since the sum is divided more).
  * The `v * 100` scales things before dividing (preserving some dynamic range).
  * `min(255, ...)` clamps the result to 8-bit range.
  * This entire line of code stores the smoothed heat into the temporary row buffer.

After calculating tmp_row, we now handle rendering the pixels by updating the actual segment data and turning 'heat' into visible colors:
```cpp
  for (unsigned x = 0; x < cols; x++) {
    SEGENV.data[XY(x, y)] = tmp_row[x];
    if (SEGMENT.check1) {
      uint32_t color = SEGMENT.color_from_palette(tmp_row[x], true, false, 0);
      SEGMENT.setPixelColorXY(x, y, color);
    } else {
      uint32_t base = SEGCOLOR(0);
      SEGMENT.setPixelColorXY(x, y, color_fade(base, tmp_row[x]));
    }
  }
}
```
* This next loop starts iterating over each row from top to bottom.  (We're now doing this for color-rendering for each pixel row.)
* Next we update the main segment data with the smoothed value for this pixel.
* The if statement creates a conditional rendering path — the user can toggle this.  If `check1` is enabled in the effect metadata, we use a color palette to display the flame.
* The next line converts the heat value (`tmp_row[x]`) into a `color` from the current palette with 255 brightness, and no wrapping in palette lookup.
  * This creates rich gradient flames (e.g., yellow → red → black).
* Finally we set the rendered color for the pixel (x, y).
  * This repeats for each pixel  in each row.
* If palette use is disabled, we fallback to fading a base color.
* `SEGCOLOR(0)` gets the first user-selected color for the segment.
* The final line of code fades that base color according to the heat value (acts as brightness multiplier).

The final piece of this custom effect returns the frame time:
```cpp
}
return FRAMETIME;
}
```
* The first bracket closes the earlier `if ((strip.now - SEGENV.step) >= refresh_ms)` block.
  * It ensures that the fire simulation (scrolling, sparking, diffusion, rendering) only runs when enough time has passed since the last update.
* returning the frame time tells WLED how soon this effect wants to be called again.
  * `FRAMETIME` is a predefined macro in WLED, typically set to ~16ms, corresponding to ~60 FPS (frames per second).
  * Even though the effect logic itself controls when to update based on refresh_ms, WLED will still call this function at roughly FRAMETIME intervals to check whether an update is needed.
* ⚠️ Important: Because the actual frame logic is gated by strip.now - SEGENV.step, returning FRAMETIME here doesn’t cause excessive updates — it just keeps the engine responsive.  **Also note that an Effect should ALWAYS return FRAMETIME.  Not doing so can cause glitches.**
* The final bracket closes the `mode_diffusionfire()` function itself.


### The Metadata String
At the end of every effect is an important line of code called the **metadata string**.
It defines how the effect is to be interacted with in the UI:
```cpp
static const char _data_FX_MODE_DIFFUSIONFIRE[] PROGMEM = "Diffusion Fire@!,Spark rate,Diffusion Speed,Turbulence,,Use palette;;Color;;2;pal=35";
```
This metadata string is passed into `strip.addEffect()` and parsed by WLED to determine how your effect appears and behaves in the UI. 
The string follows the syntax of `<Effect Parameters>;<Colors>;<Palette>;<Flags>;<Defaults>`, where Effect Parameters are specified by a comma-separated list.
The values for Effect Parameters will always follow the convention in the table below:

| Parameter	| Default tooltip label |
| :-------- | :-------------------- |
| sx	       | Effect Speed |
| ix	       | Effect Intensity |
| c1	       | Custom 1 |
| c2	       | Custom 2 |
| c3	       | Custom 3 |
| o1       	| Checkbox 1 |
| o2	       | Checkbox 2 |
| o3	       | Checkbox 3 |

Using this info, let’s split the Metadata string above into logical sections:

| Syntax Element                                  | Description |
| :---------------------------------------------- | :---------- |
| "Diffusion Fire@!      | Name.  (The @ symbol marks the end of the Effect Name, and the beginning of the Parameter String elements.) |
| !, | Use default UI entry; for the first space, this will automatically create a slider for Speed |
| Spark rate, Diffusion Speed, Turbulence,              | UI sliders for Spark Rate, Diffusion Speed, and Turbulence. Defining slider 2 as "Spark Rate" overwrites the default value of Intensity. |
| (blank),                        | unused (empty field with not even a space)  |
| Use palette;                 | This occupies the spot for the 6th effect parameter, which automatically makes this a checkbox argument `o1` called Use palette in the UI. When this is enabled, the effect uses `SEGMENT.color_from_palette(...)` (RGBW-aware, respects wrap), otherwise it fades from `SEGCOLOR(0)`.  The first semicolon marks the end of the Effect Parameters and the beginning of the `Colors` parameter. |
| Color;                  | Custom color field `(SEGCOLOR(0))` |
| (blank);                  | Empty means the effect does not allow Palettes to be selected by the user.  But used in conjunction with the checkbox argument, palette use can be turned on/off by the user.  |
| 2;                  | Flag specifying that the effect requires a 2D matrix setup |
| pal=35"                  | Default Palette ID.  this is the setting that the effect starts up with. |

More information on metadata strings can be found [here](https://kno.wled.ge/interfaces/json-api/#effect-metadata).


## Understanding 1D WLED Effects

Next, we will look at a 1D WLED effect called `Sinelon`.  This one is an especially interesting example because it shows how a single effect function can be used to create several different selectable effects in the UI.
We will break this effect down step by step.
(This effect was originally one of the FastLED example effects; more information on FastLED can be found [here](https://fastled.io/).)

```cpp
static uint16_t sinelon_base(bool dual, bool rainbow=false) {
```
* The first line of code defines `sinelon base` as static helper function.  This is how all effects are initially defined.
* Notice that it has some optional flags; these parameters will allow us to easily define the effect in different ways in the UI.

```cpp
  if (SEGLEN <= 1) return mode_static();
```
* If segment length ≤ 1, there’s nothing to animate. Just show static mode.

The line of code helps create the "Fade Out" Trail:
```cpp
  SEGMENT.fade_out(SEGMENT.intensity);
```
* Gradually dims all LEDs each frame using SEGMENT.intensity as fade amount.
* Creates the trailing "comet" effect by leaving a fading path behind the moving dot.

Next, the effect computes some position information for the actively changing pixel, and the rest of the pixels as well:
```cpp
  unsigned pos = beatsin16_t(SEGMENT.speed/10, 0, SEGLEN-1);
  if (SEGENV.call == 0) SEGENV.aux0 = pos;
```
* Calculates a sine-based oscillation to move the dot smoothly back and forth.
  * `beatsin16_t` is an improved version of FastLED’s beatsin16 function, generating smooth oscillations
  * SEGMENT.speed / 10: affects oscillation speed. Higher = faster.
  * 0: minimum position.
  * SEGLEN-1: maximum position.
* On first call `(SEGENV.call == 0)`, stores initial position in `SEGENV.aux0`. (`SEGENV.aux0` is a temporary state variable to keep track of last position.)

The next lines of code help determine the colors to be used:
```cpp
  uint32_t color1 = SEGMENT.color_from_palette(pos, true, false, 0);
  uint32_t color2 = SEGCOLOR(2);
```
* `color1`: main moving dot color, chosen from palette using the current position as index.
* `color2`: secondary color from user-configured color slot 2.

The next part takes into account the optional argument for if a Rainbow colored palette is in use:
```cpp
  if (rainbow) {
    color1 = SEGMENT.color_wheel((pos & 0x07) * 32);
  }
```
* If `rainbow` is true, override color1 using a rainbow wheel, producing rainbow cycling colors.
* `(pos & 0x07) * 32` ensures the color changes gradually with position.

```cpp
    SEGMENT.setPixelColor(pos, color1);
```
* Lights up the computed position with the selected color.

The next line takes into account another one of the optional arguments for the effect to potentially handle dual mirrored dots which create the animation:
```cpp
  if (dual) {
    if (!color2) color2 = SEGMENT.color_from_palette(pos, true, false, 0);
    if (rainbow) color2 = color1; // share rainbow color
    SEGMENT.setPixelColor(SEGLEN-1-pos, color2);
  }
```
* If dual is true:
  * Uses `color2` for mirrored dot on opposite side.
  * If `color2` is not set (0), fallback to same palette color as `color1`.
  * In `rainbow` mode, force both dots to share the rainbow color.
  * Sets pixel at `SEGLEN-1-pos` to `color2`.

This final part of the effect function will fill in the 'trailing' pixels to complete the animation:
```cpp
    if (SEGENV.aux0 < pos) {
      for (unsigned i = SEGENV.aux0; i < pos ; i++) {
        SEGMENT.setPixelColor(i, color1);
        if (dual) SEGMENT.setPixelColor(SEGLEN-1-i, color2);
      }
    } else {
      for (unsigned i = SEGENV.aux0; i > pos ; i--) {
        SEGMENT.setPixelColor(i, color1);
        if (dual) SEGMENT.setPixelColor(SEGLEN-1-i, color2);
      }
    }
    SEGENV.aux0 = pos;
  }
```
* The first line checks if current position has changed since last frame.  (Prevents holes if the dot moves quickly and "skips" pixels.)  If the position has changed, then it will implement the logic to update the rest of the pixels.
* Fills in all pixels between previous position (SEGENV.aux0) and new position (pos) to ensure smooth continuous trail.
  * Works in both directions: Forward (if new pos > old pos), and Backward (if new pos < old pos).
* Updates `SEGENV.aux0` to current position at the end.

Finally, we return the `FRAMETIME`, as with all effect functions:
```cpp
  return FRAMETIME;
}
```
* Returns `FRAMETIME` constant to set effect update rate (usually ~16 ms).

The last part of this effect has the Wrapper functions for different Sinelon modes.
Notice that there are three different modes that we can define from the single effect definition by leveraging the arguments in the function:
```cpp
uint16_t mode_sinelon(void) {
  return sinelon_base(false);
}
// Calls sinelon_base with dual = false and rainbow = false 

uint16_t mode_sinelon_dual(void) {
  return sinelon_base(true);
}
// Calls sinelon_base with dual = true and rainbow = false 

uint16_t mode_sinelon_rainbow(void) {
  return sinelon_base(false, true);
}
// Calls sinelon_base with dual = false and rainbow = true 
```

And then the last part defines the metadata strings for each effect to specify how it will be portrayed in the UI:
```cpp
static const char _data_FX_MODE_SINELON[] PROGMEM = "Sinelon@!,Trail;!,!,!;!";
static const char _data_FX_MODE_SINELON_DUAL[] PROGMEM = "Sinelon Dual@!,Trail;!,!,!;!";
static const char _data_FX_MODE_SINELON_RAINBOW[] PROGMEM = "Sinelon Rainbow@!,Trail;,,!;!";
```
Refer to the section above for guidance on understanding metadata strings.


### The UserFxUsermod Class

The `UserFxUsermod` class registers the `mode_diffusionfire` effect with WLED. This section starts right after the effect function and metadata string, and is responsible for making the effect usable in the WLED interface:
```cpp
class UserFxUsermod : public Usermod {
 private:
 public:
  void setup() override {
    strip.addEffect(255, &mode_diffusionfire, _data_FX_MODE_DIFFUSIONFIRE);

    ////////////////////////////////////////
    //  add your effect function(s) here  //
    ////////////////////////////////////////

    // use id=255 for all custom user FX (the final id is assigned when adding the effect)

    // strip.addEffect(255, &mode_your_effect, _data_FX_MODE_YOUR_EFFECT);
    // strip.addEffect(255, &mode_your_effect2, _data_FX_MODE_YOUR_EFFECT2);
    // strip.addEffect(255, &mode_your_effect3, _data_FX_MODE_YOUR_EFFECT3);
  }
  void loop() override {} // nothing to do in the loop
  uint16_t getId() override { return USERMOD_ID_USER_FX; }
};
```
* The first line declares a new class called UserFxUsermod. It inherits from `Usermod`, which is the base class WLED uses for any pluggable user-defined modules.
  * This makes UserFxUsermod a valid WLED extension that can hook into `setup()`, `loop()`, and other lifecycle events.
* The `void setup()` function runs once when WLED initializes the usermod.
  * It's where you should register your effects, initialize hardware, or do any other setup logic.
  * `override` ensures that this matches the Usermod base class definition.
* The `strip.addEffect` line is an important one that registers the custom effect so WLED knows about it.
  * 255: Temporary ID — WLED will assign a unique ID automatically.  (**Create all custom effects with the 255 ID.**)
  * `&mode_diffusionfire`: Pointer to the effect function.
  * `_data_FX_MODE_DIFFUSIONFIRE`: Metadata string stored in PROGMEM, describing the effect name and UI fields (like sliders).
  * After this, your custom effect shows up in the WLED effects list.
* The `loop()` function remains empty because this usermod doesn’t need to do anything continuously. WLED still calls this every main loop, but nothing is done here.
  * If your usermod had to respond to input or update state, you'd do it here.
* The last part returns a unique ID constant used to identify this usermod.
  * USERMOD_ID_USER_FX is defined in [const.h](https://github.com/wled/WLED/blob/main/wled00/const.h). WLED uses this for tracking, debugging, or referencing usermods internally.

The final part of this file handles instantiation and initialization:
```cpp
static UserFxUsermod user_fx;
REGISTER_USERMOD(user_fx);
```
* The first line creates a single, global instance of your usermod class.
* The last line is a macro that tells WLED: “This is a valid usermod — load it during startup.”
  * WLED adds it to the list of active usermods, calls `setup()` and `loop()`, and lets it interact with the system.



## Combining Multiple Effects in this Usermod

So now let's say that you wanted add the effects  "Diffusion Fire" and "Sinelon" through this same Usermod file:
* Navigate to [the code for Sinelon](https://github.com/wled/WLED/blob/7b0075d3754fa883fc1bbc9fbbe82aa23a9b97b8/wled00/FX.cpp#L3110).
* Copy this code, and place it below the metadata string for Diffusion Fire.  Be sure to get the metadata string as well--and to name it something different than what's already inside the core WLED code.  (Refer to the metadata String section above for more information.)
* Register the effect using the `addEffect` function in the Usermod class.
* Compile the code!

## Compiling
Compiling WLED yourself is beyond the scope of this tutorial, but [the complete guide to compiling WLED can be found here](https://kno.wled.ge/advanced/compiling-wled/), on the official WLED documentation website.

## Change Log

### Version 1.0.0

* First version of the custom effect creation guide

## Contact Us

This custom effect tutorial guide is still in development.
If you have suggestions on what should be added, or if you've found any parts of this guide which seem incorrect, feel free to reach out [here](mailto:aregis1992@gmail.com) and help us improve this guide for future creators.
