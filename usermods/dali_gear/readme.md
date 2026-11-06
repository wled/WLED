# DALI Gear Usermod

Makes WLED act as a **DALI control gear** (IEC 62386) — i.e. a light that responds to commands from an external DALI master (wall dimmer, BMS, building automation system, etc.).

DALI (Digital Addressable Lighting Interface) is a standardised two-wire bus protocol for lighting control. This usermod puts WLED on the bus as a gear device: the DALI master sends brightness/on/off/colour commands, and WLED adjusts its LEDs accordingly.

> **ESP32 only.** The hardware timer API used for Manchester decoding is not available on ESP8266.

## Hardware

You need a DALI bus interface circuit to convert between the DALI bus voltage (9.5–22.5 V) and the ESP32's 3.3 V GPIO levels.

### Minimal DIY circuit (from [qqqlab/DALI-Lighting-Interface](https://github.com/qqqlab/DALI-Lighting-Interface))

```text
3.3V ESP32         5.6V               ___
                   Zener        +----|___|---- 12V Power Supply
            ___    Diode        |     220 Ω
RX ---+-----|___|---|>|----------+------------- DALI+
      |      10K                |
     +-+                        |
     | | 100K          ___    |/   PNP             DALI BUS
     +-+        TX ---|___|----|    Transistor
      |                 1K     |\
      |                          V
GND --+---------------------------+------------- DALI-
```

> **⚠️ Warning:** This is a conceptual schematic for experimentation — it is **not isolated** and exposes the ESP32 GPIO to DALI bus voltages through a resistor divider only. Do not use it in a production installation. The PNP transistor produces a **single inversion**: GPIO HIGH drives the bus LOW (asserted). Enable **TX Inverted** in the usermod settings when using this circuit.

Commercial DALI interface modules with proper isolation (e.g. Waveshare Pico-DALI2, Mikroe DALI Click) are strongly recommended for any real installation.

### Pin assignment

| Signal | Direction | Description |
|---|---|---|
| RX | Input | Reads DALI bus state (high = bus idle, low = bus asserted) |
| TX | Output | Drives DALI bus — needed for backward frame responses to QUERY commands |

Default pins are **RX=14, TX=17** (Waveshare Pico-DALI2).

Configure both pins in the WLED usermod settings page.

## Configuration

| Setting | Default | Description |
|---|---|---|
| Enabled | false | Enable/disable the usermod |
| pin_rx | 14 | GPIO for DALI bus RX |
| pin_tx | 17 | GPIO for DALI bus TX |
| tx_inverted | false | Invert TX polarity. Enable for single-stage inverting circuits (e.g. DIY PNP). Leave off for Waveshare Pico-DALI2 and other NPN+opto-isolated boards. |
| daliAddr | -1 | Short address (0–63) to respond to, or -1 to respond to broadcast only |

## DALI commands handled

### Direct Arc Power Control (DAPC)

When the master sends a DAPC frame, the arc level (0–254) is mapped linearly to WLED brightness (0–255). WLED's existing gamma correction handles perceptual uniformity at the LED output.

| DALI arc level | WLED behaviour |
|---|---|
| 0 | Turn off |
| 1–254 | Set brightness proportionally, turn on |
| 255 (mask) | Ignored (no change) |

### Indirect commands

| Command | Number | WLED action |
|---|---|---|
| OFF | 0 | Turn off |
| UP | 1 | Increase brightness by 10 |
| DOWN | 2 | Decrease brightness by 10 |
| STEP UP | 3 | Increase brightness by 1 |
| STEP DOWN | 4 | Decrease brightness by 1 |
| RECALL MAX LEVEL | 5 | Set brightness to 255, turn on |
| RECALL MIN LEVEL | 6 | Set brightness to 1, turn on |
| STEP DOWN AND OFF | 7 | Decrease by 1; turn off if at minimum |
| ON AND STEP UP | 8 | Turn on if off, then increase by 10 |
| GO TO LAST ACTIVE LEVEL | 10 | Restore last brightness before turn-off |

### Query commands (backward frame responses)

These allow a DALI master to detect gear presence and read basic status. Responses are sent as DALI backward frames 4 ms after the query, within the IEC 62386-102 required window of 7Te–22Te (≈2.9–9.2 ms).

| Command | Byte | Response |
|---|---|---|
| QUERY STATUS | 0x90 | Status byte: bit 2 = lamp on, bit 6 = no short address |
| QUERY CONTROL GEAR PRESENT | 0x91 | `0xFF` (Yes, I am here) |
| QUERY DEVICE TYPE | 0x18 | `0x08` (device type 8 = colour control) |
| QUERY ACTUAL LEVEL | 0xA0 | Current arc level (0–254) derived from WLED brightness |

### DT8 colour temperature (IEC 62386-209)

Colour temperature commands from a DALI master are mapped to WLED's CCT value via `strip.setCCT()`. The mired value is converted to Kelvin (`K = 1,000,000 / mireds`). WLED's accepted range is 1900–10091 K; values outside this range are clamped.

Two CCT application flows are supported:

**Standard flow (IEC 62386-209 §11.3.4.1):**

1. `SET DTR0` — lower byte of colour temperature in mireds
2. `SET DTR1` — upper byte of colour temperature in mireds
3. `ENABLE DEVICE TYPE 8` — activates DT8 interpretation
4. `SET TEMPORARY COLOUR TEMPERATURE` (0xE1) — loads DTR0+DTR1 into temporary register
5. `ACTIVATE` (0xE2) — applies the temporary colour temperature

**Non-standard combined flow (observed in some masters):**

Some DALI masters skip the `0xE1` + `0xE2` sequence and instead apply the colour temperature implicitly alongside the subsequent DAPC command. The usermod detects this: if DTR0/DTR1 are set and DT8 is active when a DAPC frame arrives, the CCT is applied at the same time as the brightness change.

**QUERY COLOUR TYPE (0xF7):**

Some masters query the gear's colour capabilities before sending CCT commands. Per IEC 62386-209 §11.3.4.2, this command is `0xF7`. The usermod responds with `0x02` (bit 1 = Tc colour temperature supported), sent 4 ms after the query frame to meet the DALI spec requirement (7Te–22Te ≈ 2.9–9.2 ms settling window). Command `0xE7` does not generate a backward frame response (it is not a query command per the spec).

`SET DTR0`, `SET DTR1`, and `ENABLE DEVICE TYPE 8` are sniffed as broadcast-level frames regardless of the configured `daliAddr`.

### CCT on RGB-only strips

`strip.setCCT()` adjusts the colour temperature via WLED's internal CCT pipeline. For an RGB-only strip (no dedicated white or CCT channel), **White Balance Correction** must be enabled in WLED LED settings (Config → LED Preferences → White Balance Correction) for the CCT value to affect the LED output. This makes WLED apply a colour temperature correction on the RGB channels.

## Addressing

DALI addressing works as follows:

- **Broadcast** (`0xFE`/`0xFF`): always handled regardless of `daliAddr` setting
- **Short address** (0–63): set `daliAddr` to the address the DALI master has assigned to this device
- **Group address**: not handled

Set `daliAddr` to `-1` (default) to respond only to broadcast commands. This is useful for a single-gear installation.

## Enabling the usermod

Add to your `platformio_override.ini`:

```ini
[env:esp32dev]
custom_usermods = dali_gear
lib_deps =
    ${env.lib_deps}
    https://github.com/netmindz/DALI-Lighting-Interface.git#fix/esp32-volatile-cast
```

## Limitations

- No short address commissioning via DALI bus (set the address manually in WLED config)
- No group address support
- No DALI scene mapping

## Dependencies

- [qqqlab/DALI-Lighting-Interface](https://github.com/qqqlab/DALI-Lighting-Interface) (GPL-3.0)
  Low-level Manchester-encoded DALI bus driver by qqqlab.
  This usermod uses the fork at `https://github.com/netmindz/DALI-Lighting-Interface.git#fix/esp32-volatile-cast` which includes an ESP32 volatile-cast fix.
