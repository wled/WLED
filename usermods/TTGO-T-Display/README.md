# TTGO T-Display ESP32 with 240x135 TFT via SPI with TFT_eSPI

This usermod is a TFT display driver for WLED, using the TFT_eSPI library, originally designed for the TTGO 240x135 T-Display ESP32 module.  It can drive any TFT display with sufficient resolution that is supported by TFT_eSPI.  The supplied `platformio_override.ini` provides a ready-made configuration for the TTGO T-Display board specifically — see [Using with other boards](#using-with-other-boards) if you have different hardware.

When running, the display shows:

* Current SSID
* IP address
  * In station mode: IP address and current brightness percentage
  * In AP mode: AP SSID, IP address, and password
* Current effect
* Current palette
* Estimated current in mA (requires correct LED type in LED preferences)

The display turns off automatically after 5 minutes with no state change, and wakes on the next detected change.  The usermod can also be fully disabled via the WLED config UI, which turns off the backlight and skips all display work.  Display on/off status is shown in the WLED Info panel under **TFT Display**.

When using the T-Display environment, button pin 35 is mapped to the onboard button adjacent to the reset button (right side of the USB-C connector when oriented downward — see hardware photo).  The button can be configured via the Macros section of the WLED web UI, e.g. to cycle presets.

I have designed a 3D printed case around this board and an ["ElectroCookie"](https://amzn.to/2WCNeeA) project board, a [level shifter](https://amzn.to/3hbKu18), a [buck regulator](https://amzn.to/3mLMy0W), and a DC [power jack](https://amzn.to/3phj9NZ).  I use 12V WS2815 LED strips for my projects, and power them with 12V power supplies. The regulator supplies 5V for the ESP module and the level shifter.  If there is any interest in this case which elevates the board and display on custom extended standoffs to place the screen at the top of the enclosure (with accessible buttons), let me know, and I will post the STL files.  It is a bit tricky to get the height correct, so I also designed a one-time use 3D printed solder fixture to set the board in the right location and at the correct height for the housing.  (It is one-time use because it has to be cut off after soldering to be able to remove it).  I didn't think the effort to make it in multiple pieces was worthwhile.

Based on a rework of the ssd1306_i2c_oled_u8g2 usermod from the WLED repo.

## Hardware

![Hardware](assets/ttgo_hardware1.png)
![Enclosure](assets/ttgo-tdisplay-enclosure1a.png)
![Enclosure](assets/ttgo-tdisplay-enclosure2a.png)
![Enclosure](assets/ttgo-tdisplay-enclosure3a.png)
![Enclosure](assets/ttgo-tdisplay-enclosure4a.png)

## Github reference for TTGO-T-Display

* [TTGO T-Display](https://github.com/Xinyuan-LilyGO/TTGO-T-Display)

## Requirements

Functionality checked with:

* TTGO T-Display (ESP32, 240×135 ST7789 display)
* PlatformIO
* A group of 4 individual Neopixels from Adafruit and several full strings of 12V WS2815 LEDs.
* The hardware design shown above should be limited to shorter strings.  For larger strings, I use a different setup with a dedicated 12v power supply and power them directly from said supply (in addition to dropping the 12v to 5v with a buck regulator for the ESP module and level shifter).

## Setup (TTGO T-Display board)

The following steps are specific to the TTGO T-Display board.  For other hardware, see [Using with other boards](#using-with-other-boards).

### 1. Copy `platformio_override.ini`

Copy `usermods/TTGO-T-Display/platformio_override.ini` to the root of your WLED project folder.

This file:
- Sets the active build environment to `WLED_T-Display`
- Sets `DATA_PINS=2` and `BTNPIN=35` for the T-Display's onboard button and LED data pin
- Includes this usermod via `custom_usermods`

Once the platformio_override.ini file has been copied as described above, the platformio.ini file isn't actually changed, but it is helpful to save the platformio.ini file in the VS Code application.  This should trigger the download of the library dependencies.

### 2. Build and flash

Select the `WLED_T-Display` environment in PlatformIO (it should appear in the bottom toolbar) and build.  The display will show **Loading…** on first boot, then the connection info.

## Using with other boards

The display driver itself requires only a TFT_eSPI-compatible display with at least 240×135 pixels.  To adapt it to different hardware:

### Quick setup

1. **Create a `platformio_override.ini` file** with a new environment for your board.  Use `extends` to reference one of the pre-existing board environments from `platformio.ini` to avoid copy-and-pasting the complete configuration.

2. **Add this usermod** to your environment by appending `TTGO-T-Display` to your `custom_usermods` line.

3. **Set `custom_display_setup` to point to your display setup**.  See the detailed discussion below.

### Selecting a display setup

TFT_eSPI requires a hardware configuration header.  The default for this usermod is `User_Setups/Setup25_TTGO_T_Display.h` from the TFT_eSPI library, which has the definitions for the standard TTGO T-Display board.

To use a different display, add `custom_display_setup` to your `[env:]` section in `platformio_override.ini`:

```ini
# Use a different built-in TFT_eSPI setup:
custom_display_setup = User_Setups/Setup23_ST7789.h

# Or point to a custom header anywhere on disk or in wled00/:
custom_display_setup = /absolute/path/to/MyDisplay.h
custom_display_setup = MyDisplay.h
```

The value is resolved in this order:
1. As an absolute path (used directly if the file exists)
2. Relative to any include directory: `wled00/`, project include dir, and all TFT_eSPI include paths

A custom header must define all required TFT_eSPI `#define`s for your hardware (driver, pins, SPI frequency, etc.).  See the `User_Setups/` folder of TFT_eSPI for examples.

You do not need to set `USER_SETUP_LOADED`.

> **Note:** Display setup injection is handled by the `TFT_eSPI_compat` shim library in `lib/`.  This library also provides workarounds for some library bugs, such as a hard dependency on the SPIFFS stub so SMOOTH_FONT can be compiled on platforms without SPIFFS support.  

> **Note:** Many TFT_eSPI usage examples suggest defining the setup as `-D XX_PIN=N` settings in `build_flags`.  That approach will not work correctly with the `TFT_eSPI_compat` library; use of a display setup `.h` file is required.

## Pin conflict detection

During `setup()`, the usermod registers all TFT pins (MOSI, SCLK, CS, DC, RST, BL, and optionally MISO/TOUCH_CS) with WLED's PinManager.  If any pin is already claimed by another usermod or WLED core function, the display will be disabled at startup and a debug message printed.

## Runtime configuration

The usermod exposes one config key visible in the WLED settings UI:

| Key | Default | Description |
|-----|---------|-------------|
| `enabled` | `true` | Enable or disable the display. When disabled, the backlight is turned off immediately. |

Settings are persisted in `cfg.json` under the `"TFT Display"` key.
