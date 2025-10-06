# GC9A01 Display Usermod

A fully-featured WLED usermod providing comprehensive visual interface on GC9A01 240x240 round TFT displays with complete rotary encoder integration.

## Features

### Visual Interface

- **Circular Design**: Optimized for 240x240 round displays with blue bezel theming
- **Real-time Clock**: Large digital clock display (12/24-hour formats)
- **WiFi Signal Strength**: Visual signal strength indicator with 4-level bars (25%, 50%, 75%, 100%)
- **Power Status**: Dynamic "OFF [switch] ON" layout with contextual text
- **Brightness Arc**: Semicircular brightness visualization
- **Color Controls**: Three color buttons (FX, BG, CS) with live color preview and automatic updates
- **Effect Display**: Current effect name
- **Palette Display**: Current palette name
- **Startup Logo**: WLED logo display during initialization

### Modes

- **Sleep Mode**: Automatic display sleep after configurable timeout (5-300 seconds)
- **Clock Mode**: Alternative to sleep - shows clock instead of turning off display
- **Wake on Interaction**: Automatic wake on rotary encoder use or button press
- **Unified Timeout**: Single configurable timeout controls both sleep and clock modes
- **Backlight Control**: PWM-based brightness control (0-100%)

### Performance Features

- **State Caching**: Minimal redraws using comprehensive change detection
- **Smart Updates**: Automatic color button updates when colors change from any source (web UI, API, etc.)
- **Non-blocking Updates**: Asynchronous display updates following Four Line Display ALT pattern
- **Memory Optimized**: Efficient memory usage with proper cleanup
- **Debug Support**: Comprehensive debug logging with WLED macros

## Hardware Requirements

- ESP32 development board
- GC9A01 240x240 TFT display (round)
- Optional: Rotary encoder (usermod_v2_rotary_encoder_ui_ALT)

## Wiring

### GC9A01 Display

| GC9A01 Pin | ESP32 Pin | Function    | Description |
|------------|-----------|-------------|-------------|
| VCC        | 3.3V      | Power       | 3.3V power supply |
| GND        | GND       | Ground      | Common ground |
| SCL/SCLK   | GPIO18    | SPI Clock   | SPI clock signal |
| SDA/MOSI   | GPIO23    | SPI MOSI    | SPI data out |
| RES/RST    | GPIO17    | Reset       | Display reset |
| DC         | GPIO15    | Data/Command| Data/Command control |
| CS         | GPIO5     | Chip Select | SPI chip select |
| BL         | GPIO26    | Backlight   | Backlight control |

**Note**: Pin assignments can be customized via build flags (see Configuration section).

### Basic Setup

Add to your `platformio_override.ini`:

```ini
[env:esp32_gc9a01]
extends = env:esp32dev
upload_speed = 460800
monitor_speed = 115200
custom_usermods =
  usermod_v2_gc9a01_display
  usermod_v2_rotary_encoder_ui_ALT
build_flags = ${common.build_flags} ${esp32_idf_V4.build_flags}
  -D WLED_DEBUG
  -D WLED_DISABLE_BROWNOUT_DET
  -D USERMOD_GC9A01_DISPLAY
  -DUSER_SETUP_LOADED=1
  -DGC9A01_DRIVER=1
  -DTFT_WIDTH=240
  -DTFT_HEIGHT=240
  -DTFT_MOSI=23
  -DTFT_SCLK=18
  -DTFT_CS=5
  -DTFT_DC=15
  -DTFT_RST=17
  -DTFT_BL=26
  -DTOUCH_CS=-1
  -DLOAD_GLCD=1
  -DLOAD_FONT2=1
  -DLOAD_FONT4=1
  -DLOAD_FONT6=1
  -DLOAD_FONT7=1
  -DLOAD_FONT8=1
  -DLOAD_GFXFF=1
  -DSMOOTH_FONT=1
  -DSPI_FREQUENCY=27000000
# Rotary encoder settings
  -D ENCODER_DT_PIN=25
  -D ENCODER_CLK_PIN=32
  -D ENCODER_SW_PIN=27
```

### Debug Information

Enable debug output with `-D WLED_DEBUG` to see detailed logging:

- Display initialization status and TFT_eSPI configuration
- Update timing information and redraw triggers
- State change detection (brightness, effect, colors, etc.)
- Sleep/wake events and timeout tracking
- Error conditions and recovery attempts

Debug output appears in Serial Monitor at 115200 baud.

## Change log

- **v1** (2025-10-06):
  - First public release
