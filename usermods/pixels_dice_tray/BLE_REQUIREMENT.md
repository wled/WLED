# pixels_dice_tray Usermod - BLE Requirement Notice

## Important: This Usermod Requires Special Configuration

The `pixels_dice_tray` usermod requires **ESP32 BLE (Bluetooth Low Energy)** support, which is not available in all WLED build configurations.

### Why is library.json disabled?

The `library.json` file has been renamed to `library.json.disabled` to prevent this usermod from being automatically included in builds that use `custom_usermods = *` (like the `usermods` environment in platformio.ini).

The Tasmota Arduino ESP32 platform used by WLED does not include Arduino BLE library by default, which causes compilation failures when this usermod is auto-included.

### How to Use This Usermod

This usermod **requires a custom build configuration**. You cannot simply enable it with `custom_usermods = *`.

1. **Copy the sample configuration:**
   ```bash
   cp platformio_override.ini.sample ../../../platformio_override.ini
   ```

2. **Edit `platformio_override.ini`** to match your ESP32 board configuration

3. **Build with the custom environment:**
   ```bash
   pio run -e t_qt_pro_8MB_dice
   # or
   pio run -e esp32s3dev_8MB_qspi_dice
   ```

### Platform Requirements

- ESP32-S3 or compatible ESP32 board with BLE support
- Custom platformio environment (see `platformio_override.ini.sample`)
- Cannot be used with ESP8266 or ESP32-S2

### Re-enabling for Custom Builds

If you want to use this usermod in a custom build:

1. Rename `library.json.disabled` back to `library.json`
2. Manually add it to your custom environment's `custom_usermods` list
3. Ensure your platform includes BLE support

### References

- See `README.md` for full usermod documentation
- See `platformio_override.ini.sample` for build configuration examples
