# SD-card mod

## Build
- modify `platformio.ini` and add to the `build_flags` of your configuration the following
- choose the way your SD is connected
  1. via `-D WLED_USE_SD_MMC` when connected via MMC
  2. via `-D WLED_USE_SD_SPI` when connected via SPI (use usermod page to setup SPI pins)

### Test
- enable `-D SD_PRINT_HOME_DIR` and `-D WLED_DEBUG`
- this will print all files in `/` on boot via serial

## Configuration
### MMC
- The MMC port / pins needs no configuration as they are specified by Espressif
### SPI
- The SPI port / pins can be modified via the WLED web-UI: `Config → Usermod → SD Card`
  | option            | effect                                                                                           | default |
  | ----------------- | ------------------------------------------------------------------------------------------------ | ------- |
  | `pinSourceSelect` | GPIO that is connected to SD's `SS`(source select) / `CS`(chip select)                           | 16      |
  | `pinSourceClock`  | GPIO that is connected to SD's `SCLK` (source clock) / `CLK`(clock)                              | 14      |
  | `pinPoci`         | GPIO that is connected to SD's `POCI`<sup>☨</sup> (Peripheral-Out-Ctrl-In) / `MISO` (deprecated) | 36      |
  | `pinPico`         | GPIO that is connected to SD's `PICO`<sup>☨</sup> (Peripheral-In-Ctrl-Out) / `MOSI` (deprecated) | 14      |
  | `sdEnable`        | Enable to read data from the SD-card                                                             | true    |

  <sup>☨</sup><sub>Following new naming convention of [OSHWA](https://www.oshwa.org/a-resolution-to-redefine-spi-signal-names/)</sub>

## Usage in other mods
- creates a macro `SD_ADAPTER` which is either mapped to `SD` or `SD_MMC` (see `SD_Test.ino` how to use SD / SD_MMC functions)

-  checks if the specified file is available on the SD card
   ```cpp
   bool file_onSD(const char *filepath) {...}
   ```