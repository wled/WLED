# Bootloader Compatibility Checking

As of WLED 0.16, the firmware includes bootloader version checking to prevent incompatible OTA updates that could cause boot loops.

## Background

ESP32 devices use different bootloader versions:
- **V2 Bootloaders**: Legacy bootloaders (ESP-IDF < 4.4)
- **V3 Bootloaders**: Intermediate bootloaders (ESP-IDF 4.4+)
- **V4 Bootloaders**: Modern bootloaders (ESP-IDF 5.0+) with rollback support

WLED 0.16+ requires V4 bootloaders for full compatibility and safety features.

## Checking Your Bootloader Version

### Method 1: Web Interface
Visit your WLED device at: `http://your-device-ip/json/bootloader`

This will return JSON like:
```json
{
  "version": 4,
  "rollback_capable": true,
  "esp_idf_version": 50002
}
```

### Method 2: Serial Console
Enable debug output and look for bootloader version messages during startup.

## OTA Update Behavior

When uploading firmware via OTA:

1. **Compatible Bootloader**: Update proceeds normally
2. **Incompatible Bootloader**: Update is blocked with error message:
   > "Bootloader incompatible! Please update to a newer bootloader first."
3. **No Metadata**: Update proceeds (for backward compatibility with older firmware)

## Upgrading Your Bootloader

If you have an incompatible bootloader, you have several options:

### Option 1: Serial Flash (Recommended)
Use the [WLED web installer](https://install.wled.me) to flash via USB cable. This will install the latest bootloader and firmware.

### Option 2: Staged Update
1. First update to WLED 0.15.x (which supports your current bootloader)
2. Then update to WLED 0.16+ (0.15.x may include bootloader update)

### Option 3: ESP Tool
Use esptool.py to manually flash a new bootloader (advanced users only).

## For Firmware Builders

When building custom firmware that requires V4 bootloader:

```bash
# Add bootloader requirement to your binary
python3 tools/add_bootloader_metadata.py firmware.bin 4
```

## Technical Details

- Metadata format: ASCII string `WLED_BOOTLOADER:X` where X is required version (1-9)
- Checked in first 512 bytes of uploaded firmware
- Uses ESP-IDF version and rollback capability to detect current bootloader
- Backward compatible with firmware without metadata

## Troubleshooting

**Error: "Bootloader incompatible!"**
- Use web installer to update via USB
- Or use staged update through 0.15.x

**How to check if I need an update?**
- Visit `/json/bootloader` endpoint
- If version < 4, you may need to update for future firmware

**Can I force an update?**
- Not recommended - could brick your device
- Use proper upgrade path instead