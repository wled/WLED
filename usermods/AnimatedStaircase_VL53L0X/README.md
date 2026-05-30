# Usermod: Animated Staircase for VL53L0X Sensors

This usermod is based on the excellent work done in the original [Animated_Staircase](https://github.com/Aircoookie/WLED/tree/main/usermods/Animated_Staircase) WLED usermod by [@rolfje](https://github.com/rolfje) and [@blazoncek](https://github.com/blazoncek). It retains the same beautiful functionality of animating your staircase lighting based on user movement — now enhanced with support for **VL53L0X time-of-flight distance sensors**.

## 🚶‍♂️ What It Does

- Lights up the steps in the direction you're walking (up or down).
- Animates off behind you after a configurable timeout.
- Responds to multiple people using the stairs in either direction.
- Automatically handles bidirectional triggers with two VL53L0X sensors.

## 🧠 What's New in This Version

This mod replaces the original PIR or ultrasonic sensors with two **VL53L0X** I²C time-of-flight sensors. These are more compact, and immune to acoustic interference. Changes include:

- VL53L0X support for both top and bottom sensors
- Optional `xshut` pins for I²C address assignment
- Configurable sensor threshold distance in millimeters
- Preserves original animation behavior and API
- New config options:
  - `xshut-top-pin`
  - `xshut-bottom-pin`
  - `trigger-threshold-mm`

## 🔧 Installation & Setup

### 1. Prerequisites

You must [compile WLED from source](https://kno.wled.ge/advanced/compiling-wled/) to use this usermod.

To include this usermod in your WLED setup, you have to be able to [compile WLED from source](https://kno.wled.ge/advanced/compiling-wled/).

Before compiling, you have to make the following modifications:

Copy platformio_override_example.ini to the same folder as platformio.ini

## 🔌 Wiring

- **VL53L0X** sensors share the I²C bus (SDA, SCL).
- Use optional `XSHUT` GPIOs to assign unique I²C addresses (0x30 and 0x31).
- Wire your LEDs and segments the same as the original mod.
- Example VL53L0X wiring:
  - SDA → GPIO 21 (or 4)
  - SCL → GPIO 22 (or 5)
  - XSHUT_TOP → GPIO 17
  - XSHUT_BOTTOM → GPIO 16

## ⚙️ WLED Configuration

1. Create one segment per step.
2. Save your segment layout as a preset.
3. Set that preset to apply at boot.
4. Open the **Usermod Settings** page in WLED UI:
   - Enable the mod
   - Set `xshut-top-pin`, `xshut-bottom-pin`, and optionally adjust animation timings
   - Set `trigger-threshold-mm` (900 is a good start)

## 📡 API & MQTT

### Enable / Disable

```bash
curl -X POST -H "Content-Type: application/json" \
     -d '{"staircase-vl53":{"enabled":true}}' \
     http://<WLED-IP>/json/state
```

### Trigger Sensors via API

```bash
curl -X POST -H "Content-Type: application/json" \
     -d '{"staircase-vl53":{"bottom-sensor":true}}' \
     http://<WLED-IP>/json/state
```

```bash
curl -X POST -H "Content-Type: application/json" \
     -d '{"staircase-vl53":{"top-sensor":true}}' \
     http://<WLED-IP>/json/state
```

### MQTT

Publish to `/swipe`:
- `"up"` or `"down"` triggers animation
- `"on"` or `"off"` enables/disables the usermod

## 📁 JSON Configuration Example

```json
{
  "staircase-vl53": {
    "enabled": true,
    "segment-delay-ms": 150,
    "on-time-s": 30,
    "toggle-on-off": false,
    "xshut-top-pin": 17,
    "xshut-bottom-pin": 16,
    "trigger-threshold-mm": 900
  }
}
```

## 🙏 Credits

- Original concept: [@rolfje](https://github.com/rolfje)
- Runtime configuration and MQTT: [@blazoncek](https://github.com/blazoncek)
- VL53L0X integration and enhancements: [@Hoverman1977](https://github.com/Hoverman1977)

## 📜 Changelog

**2026-05**
- Initial release of VL53L0X-compatible version
- Migrated from ultrasonic to I²C time-of-flight sensors
- Added XSHUT pin config and threshold control
