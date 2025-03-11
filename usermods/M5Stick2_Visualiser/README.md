# Using the M5Stick C Plus2

TBA


## Hardware

***
![Hardware](images/m5stick_c_plus2.webp)

Connect the data pin of your strips to any of the GPIO pins. Connect GND to any of the GND pins.

## Library used

[Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI)

## Setup

In the `platformio.ini` file, you must change the environment setup to build for just the m5stick platform by adding the following target and commeting the other ones:

```
[env:m5stick]
board = esp32dev
build_unflags = ${common.build_unflags}
build_flags = ${common.build_flags_esp32} 
  # enable AR mod
  ${esp32.AR_build_flags}
  -D BTNPIN=39
  -D LEDPIN=19
  -D IRPIN=19
  # don't need these
  -D WLED_DISABLE_MQTT
  -D WLED_DISABLE_LOXONE
  -D WLED_DISABLE_ALEXA
  # Display config
  -D ST7789_DRIVER=1
  -D USERMOD_M5STICK2_VISUALISER=1
  -D TFT_WIDTH=135
  -D TFT_HEIGHT=240
  -D TFT_MOSI=15
  -D TFT_BL=27
  -D TFT_SCLK=13
  -D TFT_DC=14
  -D TFT_RST=12
  -D TFT_CS=5
  -D SPI_FREQUENCY=20000000
  # Mic config
  -D I2S_SDPIN=34 -D I2S_WSPIN=0 -D I2S_CKPIN=-1 -D MCLK_PIN=-1  ;; PDM microphone pins
  -D DMENABLED=5 -D SR_SQUELCH=5 -D SR_GAIN=75    ;; SPM1423 specific sound settings
  -D USER_SETUP_LOADED
lib_deps = 
  ${esp32.lib_deps}
  ${esp32.AR_lib_deps} 
  TFT_eSPI @ ^2.3.70
  https://github.com/kosme/arduinoFFT#419d7b0
platform = ${esp32.platform}
platform_packages = ${esp32.platform_packages}
board_build.partitions = ${esp32.default_partitions}
```

Change the default_env like this:
```
; default_envs = wemos_shield_esp32
; default_envs = m5atom
  default_envs = m5stick
; default_envs = esp32_eth
; default_envs = esp32dev_qio80
```
