# DMX Output Board Compatibility

## Boards Without DMX Output Support

The following ESP32 variants do NOT support DMX serial output in WLED:

- ESP32-C3
- ESP32-S2

This is because WLED uses the SparkFun DMX driver for ESP32 boards,
which requires Serial2. These boards do not have Serial2 available.

## Recommended Alternatives

If you need DMX output on a small board, use ESP32-S3 Mini or
ESP32-S3 Zero instead.

## Additional Hardware

DMX output requires an RS485 transceiver such as the MAX485 chip
connected between your ESP board and your DMX fixtures.
