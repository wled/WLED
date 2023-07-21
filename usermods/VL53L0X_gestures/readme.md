# Description

Implements support of simple hand gestures via a VL53L0X sensor: on/off and brightness adjustment.
Useful for controlling strips when you want to avoid touching anything.
 - on/off - swipe your hand below the sensor ("shortPressAction" is called. Can be customized via WLED macros)
 - brightness adjustment - hold your hand below the sensor for 1 second to switch to "brightness" mode.
                           adjust the brightness by changing the distance between your hand and the sensor (see parameters below for customization).
   
## Installation

1. Attach VL53L0X sensor to i2c pins according to default pins for your board.
2. Add `-D USERMOD_VL53L0X_GESTURES` to your build flags at platformio.ini (plaformio_override.ini) for needed environment.
In my case, for example: `build_flags = ${env.build_flags} -D USERMOD_VL53L0X_GESTURES`
3. Add "pololu/VL53L0X" dependency below to `lib_deps` like this:
```ini
lib_deps = ${env.lib_deps}
      pololu/VL53L0X @ ^1.3.0
```

My entire `platformio_override.ini` for example (for nodemcu board):
```ini
[platformio]
default_envs = nodemcuv2

[env:nodemcuv2]
build_flags = ${env.build_flags} -D USERMOD_VL53L0X_GESTURES
lib_deps = ${env.lib_deps}
           pololu/VL53L0X @ ^1.3.0
```
