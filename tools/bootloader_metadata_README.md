# Bootloader Metadata Tool

This tool adds bootloader version requirements to WLED firmware binaries to prevent incompatible OTA updates.

## Usage

```bash
python3 tools/add_bootloader_metadata.py <binary_file> <required_version>
```

Example:
```bash
python3 tools/add_bootloader_metadata.py firmware.bin 4
```

## Bootloader Versions

- **Version 2**: Legacy bootloader (ESP-IDF < 4.4)
- **Version 3**: Intermediate bootloader (ESP-IDF 4.4+)  
- **Version 4**: Modern bootloader (ESP-IDF 5.0+) with rollback support

## How It Works

1. The script appends a metadata tag `WLED_BOOTLOADER:X` to the binary file
2. During OTA upload, WLED checks the first 512 bytes for this metadata
3. If found, WLED compares the required version with the current bootloader
4. The update is blocked if the current bootloader is incompatible

## Metadata Format

The metadata is a simple ASCII string: `WLED_BOOTLOADER:X` where X is the required bootloader version (1-9).

This approach was chosen over filename-based detection because users often rename firmware files.

## Integration with Build Process

To automatically add metadata during builds, add this to your platformio.ini:

```ini
[env:your_env]
extra_scripts = post:add_metadata.py
```

Create `add_metadata.py`:
```python
Import("env")
import subprocess

def add_metadata(source, target, env):
    firmware_path = str(target[0])
    subprocess.run(["python3", "tools/add_bootloader_metadata.py", firmware_path, "4"])

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", add_metadata)
```