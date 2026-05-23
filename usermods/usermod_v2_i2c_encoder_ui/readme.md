# I2C Rotary Encoder UI

A WLED usermod that provides the same rotary-encoder-driven UI as the **Rotary Encoder UI** usermod, but for the **M5Stack Encoder Unit** (UNIT-ENCODER, SKU: U135) — an I2C device that handles quadrature decoding internally.

No encoder GPIO pins are needed. The encoder communicates over the I2C bus configured globally in **WLED Settings → Hardware**.

## Compatible Hardware

| Device | I2C Address | Notes |
|---|---|---|
| M5Stack Encoder Unit (U135) | 0x40 (default) | Verified against V1 firmware protocol |

## Controls

| Action | Effect |
|---|---|
| Rotate | Adjust the current parameter |
| Short press | Cycle to the next parameter |
| Double press | Toggle LEDs on/off |
| Hold 3 s | Show network info (requires FourLineDisplay usermod) |

## Parameters cycled by button press

Without FourLineDisplay: Brightness → Effect Speed → Effect Intensity → Color Palette → Effect  
With FourLineDisplay: adds Main Color (hue) → Saturation → CCT → Preset → Custom 1/2/3

CCT only appears when the active segment supports it. Preset only appears when preset-low and preset-high are both set.

## Setup

1. In **WLED Settings → Hardware**, set the I2C SDA and SCL pins for your board.
2. Connect the M5Stack Encoder Unit to those pins (and 3.3 V / GND).
3. Enable this usermod in **WLED Settings → Usermods**.
4. The default I2C address is **64 (0x40)**. Change it if you have re-soldered the address pads.

## Configuration options

| Option | Default | Description |
|---|---|---|
| enabled | true | Enable or disable the usermod |
| i2c-address | 64 (0x40) | I2C address of the encoder unit (decimal) |
| preset-low | 0 | Lower bound of preset cycling range |
| preset-high | 0 | Upper bound of preset cycling range (0 disables preset mode) |
| apply-2-all-seg | true | Apply changes to all active segments, not just the main one |

## Building

Add to your `platformio_override.ini` build environment:

```ini
build_flags =
    -D USERMOD_I2C_ENCODER_UI
```

Or enable it from the WLED usermod settings page if your build includes it by default.

Optionally pair with FourLineDisplay for the full set of controls:

```ini
build_flags =
    -D USERMOD_I2C_ENCODER_UI
    -D USERMOD_FOUR_LINE_DISPLAY
```
