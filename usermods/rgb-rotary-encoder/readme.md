# RGB Encoder Board

This usermod-v2 adds support for the awesome RGB Rotary Encoder Board by Adam Zeloof / "Isotope Engineering" to control the overall brightness of your WLED instance: https://github.com/isotope-engineering/RGB-Encoder-Board. A great DIY rotary encoder with 20 tiny SK6805 / "NeoPixel Nano" LEDs.

https://user-images.githubusercontent.com/3090131/124680599-0180ab80-dec7-11eb-9065-a6d08ebe0287.mp4

## Credits
The actual / original code that controls the LED modes is from Adam Zeloof. I take no credit for it. I ported it to WLED, which involved replacing the LED library he used, (because WLED already has one, so no need to add another one) plus the rotary encoder library because it was not compatible with ESP, only Arduino.
It was quite a bit more work than I hoped, but I got there eventually :)

## How to connect the board to your ESP
We'll need (minimum) three or (maximum) four GPIOs for the board:
* "ea": reports the encoder direction
* "eb": Same thing, opposite direction
* "di": LED data in.
* *(optional)* "sw": The integrated switch in the rotary encoder. Can be omitted for the bare functionality of controlling only the brightness

We'll also need power:

* "vdd": Needs to be connected to **+5V**.
* "gnd": Ground.

You can freely pick the GPIOs, it doesn't matter. Those will be configured in the "Usermods" section of the WLED web panel:

## Configuration
Navigate to the "Config" and then to the "Usermods" section. If you compiled WLED with `-D RGB_ROTARY_ENCODER`, you will see the config for it there. The settings there are the aforementioned GPIOs, (*Note: The switch pin is not there, as this can just be configured the "normal" button on the "LED Preferences" page*) plus a few more:
* LED pin:
  * Possible values: Any valid and available GPIO
  * Default: 3
  * What it does: controls the LED ring
* ea pin:
  * Possible values: Any valid and available GPIO
  * Default: 15
  * What it does: First of the two rotary encoder pins
* eb pin:
  * Possible values: Any valid and available GPIO
  * Default: 32
  * What it does: Second of the two rotary encoder pins
* LED Mode:
  * Possible values: 1-3
  * Default: 3
  * What it does: The usermod provides three different modes of how the LEDs can appear. Here's an example: https://github.com/isotope-engineering/RGB-Encoder-Board/blob/master/images/rgb-encoder-animations.gif
    * Up left is "1"
    * Up right is not supported / doesn't make sense for brightness control
    * Bottom left is "2"
    * Bottom right is "3"
* LED Brightness:
  * Possible values: 1-255
  * Default: 64
  * What it does: sets LED ring Brightness
* Steps per click:
  * Possible values: Any positive number
  * Default: 4
  * What it does: With each "click", a rotary encoder actually increments its "steps". Most rotary encoders produce four "steps" per "click". Leave this at the default value unless your rotary encoder behaves strangely. e.g. with one click, it makes two LEDs light up, or you need two clicks for one LED. If that's the case, adjust this value or write a small sketch using the same "ESP Rotary" library and read out the steps it produce.
* Increment per click:
  * Possible values: Any positive number
  * Default: 5
  * What it does: Most rotary encoders have 20 "clicks" or positions. This value should be set to 100/`number of clicks`

## Change log
2021-07
* First implementation.
