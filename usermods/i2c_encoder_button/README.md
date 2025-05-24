# usermod_i2c_encoder

This usermod enables the use of a [DUPPA I2CEncoder V2.1](https://www.tindie.com/products/saimon/i2cencoder-v21-connect-rotary-encoder-on-i2c-bus/) rotary encoder + pushbutton to control WLED.

Settings will be available on the Usermods page of the web UI. Here you can define which pins are used for interrupt, SCL, and SDA. Restart is needed for new values to take effect.

## Features

- On/off
  - Integrated button switch turns the strip on and off
- Brightness adjust
  - Turn the encoder knob to adjust brightness
- Effect adjust (encoder LED turns red)
  - Hold the button for 1 second to switch operating mode to effect adjust mode
  - When in effect adjust mode the integrated LED turns red
  - Rotating the knob cycles through all the effects
- Reset
  - When WLED is off (brightness 0) hold the button to reset and load Preset 1. Preset 1 must be defined for this to work.

## Hardware

This usermod is intended to work with the I2CEncoder V2.1 with the following configuration:

- Rotary encoder: Illuminated RGB Encoder
  - This encoder includes a pushbutton switch and an internal RGB LED to illuminate the shaft and any know attached to it.
  - This is the encoder: https://www.sparkfun.com/products/15141
- Knob: Any knob works, but the black knob has a transparent ring that lets the internal LED light through for a nice glow.
- Connectors: any
- LEDs: none (this is separate from the LED included in the encoder above)

## Compiling

Simply add `custom_usermods = i2c_encoder_button` to your platformio_override.ini environment to enable this usermod in your build.

See `platformio_override.sample.ini` for example usage.

Warning: if this usermod is enabled and no i2c encoder is connected you will have problems!
