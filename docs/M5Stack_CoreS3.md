# M5Stack CoreS3 Support for WLED

The M5Stack CoreS3 integration combines multiple usermods and CoreS3-specific build support, including the local touch UI, power management, built-in microphone Audio Reactive support, and ESP32-S3 RMT stabilization.

The complete documentation is maintained with the CoreS3 Display usermod:

- **English:** [M5Stack CoreS3 Documentation](../usermods/CoreS3_Display/readme.md)
- **日本語:** [M5Stack CoreS3 日本語ドキュメント](../usermods/CoreS3_Display/readme_jp.md)

## Main Components

- `usermods/CoreS3_Power/`
- `usermods/CoreS3_Display/`
- `usermods/CoreS3_Audio/`
- `usermods/audioreactive/`
- `pio-scripts/cores3_upload_watchdog_reset.py`
- `pio-scripts/cores3_v17_neopixelbus_patch.py`
- `usermods/CoreS3_Display/platformio_override.ini.example`

The CoreS3-specific implementation is kept outside the WLED core as much as practical, with only a minimal integration hook in `wled00/wled.cpp`, to make future upstream synchronization easier.
