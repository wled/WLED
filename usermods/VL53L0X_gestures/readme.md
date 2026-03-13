# Description

Implements support of simple hand gestures via a VL53L0X sensor: on/off and brightness adjustment.
Useful for controlling strips when you want to avoid touching anything.
 - on/off - swipe your hand below the sensor ("shortPressAction" is called. Can be customized via WLED macros)
 - brightness adjustment - hold your hand below the sensor for 1 second to switch to "brightness" mode.
                           adjust the brightness by changing the distance between your hand and the sensor (see parameters below for customization).
   
## Installation

1. Attach VL53L0X sensor to i2c pins according to default pins for your board.
2. Add `-D USERMOD_VL53L0X_GESTURES` to your build flags at platformio.ini (plaformio_override.ini) for needed environment.

