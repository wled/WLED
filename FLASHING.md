# Building and Flashing the MagWLED-1 Sendspin Prototype

This document applies to the temporary `magwled_sendspin` proof-of-concept target. It uses a single large application partition on the MagWLED-1's 4 MB flash and deliberately disables OTA updates.

## Important Limitations

- This is a bench-validation build, not the intended final partition layout.
- Updates require a USB connection. WLED's browser-based OTA updater is compiled out.
- There is no second application image for rollback if an update is interrupted or a new image fails to boot.
- Returning to an OTA layout requires flashing a smaller firmware image and a new partition table over USB.
- Back up WLED configuration and presets before changing partition layouts. Filesystem offsets differ and should not be expected to survive repartitioning.

The proof layout provides:

| Partition | Size |
| --- | ---: |
| Application | 3,080,192 bytes |
| LittleFS | 983,040 bytes |
| Coredump | 65,536 bytes |

The validated Sendspin image uses 2,158,361 bytes, or 70.1% of the application partition.

## Electrical Safety

For the initial flash and boot test:

1. Disconnect the LED strip.
2. Disconnect external DC power.
3. Power the MagWLED-1 only through USB-C.

Do not connect USB-C and external DC simultaneously. Do not connect a 12 V strip until GPIO5 power-switch inversion has been verified on the physical board.

## Build With Docker

Requirements:

- Docker Desktop is running on macOS.
- The repository and its `sendspin-cpp` submodule are checked out.

Initialize the submodule if necessary:

```bash
git submodule update --init --recursive sendspin-cpp
```

Build the proof target from the repository root:

```bash
PIO_ENV=magwled_sendspin docker compose -f compose.build.yaml run --rm --build wled-build
```

The command installs Node dependencies, builds the WLED web assets, and compiles the firmware. A successful build ends with output similar to:

```text
Environment       Status
magwled_sendspin  SUCCESS
```

## Firmware Artifacts

Use this merged image for a complete USB flash:

```text
.pio/build/magwled_sendspin/firmware.factory.bin
```

It contains the bootloader, partition table, boot application, and WLED application at their correct offsets.

Do not write the app-only files below at address `0x0`:

```text
.pio/build/magwled_sendspin/firmware.bin
build_output/release/WLED_17.0.0-devV5_MagWLED-1.bin
```

Those files contain only the application and normally belong at offset `0x10000` with an already matching bootloader and partition table.

## Flash From macOS

Install Espressif's flashing utility:

```bash
brew install esptool
```

Connect the board over USB-C and list candidate serial devices:

```bash
ls /dev/cu.usbmodem*
```

Replace `/dev/cu.usbmodemXXXX` in the following commands with the detected port.

Because this build changes the partition table, erase the existing flash first:

```bash
esptool --chip esp32c3 --port /dev/cu.usbmodemXXXX erase-flash
```

Flash the merged factory image at address `0x0`:

```bash
esptool --chip esp32c3 --port /dev/cu.usbmodemXXXX write-flash 0x0 .pio/build/magwled_sendspin/firmware.factory.bin
```

Reset or reconnect the board after flashing. WLED should start its normal setup access point when no Wi-Fi configuration exists.

## Enter the ROM Bootloader

Espressif chips contain a mask-ROM download bootloader that is independent of the application and flash partition table. A bad application or partition table therefore does not normally prevent USB recovery.

If esptool cannot connect, use the board's documented bootloader procedure. The common ESP32-C3 sequence is:

1. Hold the BOOT button connected to GPIO9.
2. Tap RESET, or reconnect USB while continuing to hold BOOT.
3. Release BOOT.
4. Check `/dev/cu.usbmodem*` again because the port name can change in download mode.
5. Repeat the erase and write commands.

The firmware does not enable secure boot, flash encryption, or burn security eFuses. Those features can alter recovery behavior and must not be enabled during this prototype.

## Can the Board Be Bricked?

A firmware mistake, failed write, or invalid partition table can make the application unbootable, but it generally does not permanently brick an ESP32-C3. The ROM bootloader can erase and rewrite the complete flash when GPIO9 is held in download mode.

Permanent or difficult-to-recover failures are more commonly caused by:

- Electrical damage, especially incorrect voltage or simultaneous USB and external power.
- Deliberately burning security eFuses that disable download or debugging features.
- Physical damage to the BOOT/USB circuitry.
- Flash wear or component failure.

None of those operations are part of this build or flashing procedure.

## Consequences of Removing OTA

Removing OTA has these practical effects:

- Firmware updates cannot be installed through the WLED web interface or over Wi-Fi.
- Every firmware update requires physical USB access.
- There is no inactive application slot to receive and verify a new image before reboot.
- There is no automatic rollback to the previous firmware if a new image fails.
- The space formerly reserved for the second application becomes available to the larger Sendspin firmware.
- Network configuration, presets, and Sendspin identity still use NVS/LittleFS normally while this partition layout remains unchanged.
- Repartitioning later should be treated as a fresh installation even though the NVS offset is unchanged.

These limitations are acceptable for proving Sendspin connectivity and runtime memory on a bench device, but not for the intended final deployable build.

## Restore an OTA Layout Later

To restore OTA support:

1. Back up WLED configuration and presets.
2. Reduce the firmware below the selected OTA slot size, or move to hardware with larger flash.
3. Remove `WLED_DISABLE_OTA` from the MagWLED build flags.
4. Select an OTA-capable partition CSV, such as WLED's standard 4 MB layout.
5. Build a new merged factory image.
6. Enter the ROM bootloader and erase the flash over USB.
7. Flash the new factory image at `0x0`.
8. Restore the saved configuration and presets.

Reference: [Espressif ESP32-C3 boot mode documentation](https://docs.espressif.com/projects/esptool/en/latest/esp32c3/advanced-topics/boot-mode-selection.html).
