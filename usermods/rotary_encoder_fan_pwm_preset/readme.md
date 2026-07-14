# Rotary Encoder Fan PWM Preset Usermod

This usermod adds rotary encoder fan control to WLED and integrates with Home Assistant via MQTT auto-discovery.

It is designed for a 5-pin encoder module (CLK, DT, SW, VCC, GND) and a PWM-controlled fan.

## Features

- Rotary fan speed control (0-100% duty).
- Short button press: cycle to next existing WLED preset.
- Preset cycle wraps back to the first existing preset.
- Long button press (3 seconds): toggle both fan and lights power.
- Restores previous fan speed and WLED brightness/state when toggled back on.
- WLED Info panel controls:
	- Fan on/off button
	- Fan speed slider
- JSON API support for external control.
- MQTT state and command topics.
- Home Assistant MQTT auto-discovery for a native fan entity.

## Default pins (ESP8266 D1 mini)

- Encoder CLK: GPIO5 (D1)
- Encoder DT: GPIO4 (D2)
- Encoder SW: GPIO14 (D5)
- Fan PWM: GPIO12 (D6)

All pins are configurable in Usermod settings.

## Build enable

Add this define to your build flags:

- `-D USERMOD_ROTARY_ENCODER_FAN_PWM_PRESET`

## Control behavior

### Encoder rotation

- Clockwise/counter-clockwise increments/decrements fan duty by `step-percent`.
- Direction can be inverted with `invert-encoder`.

### Encoder button

- Short press: next preset.
- Long press (>= 3000 ms):
	- If either fan or lights are on: turns both off.
	- If both are off: turns both on to previous values where possible.

## WLED JSON API

The usermod exposes state in `/json/state` under:

- `RotaryFanPWM.on` (bool)
- `RotaryFanPWM.speed` (0-100)

Examples:

Turn fan on:

```json
{"RotaryFanPWM":{"on":true}}
```

Turn fan off:

```json
{"RotaryFanPWM":{"on":false}}
```

Set speed to 65%:

```json
{"RotaryFanPWM":{"speed":65}}
```

Set speed and force on:

```json
{"RotaryFanPWM":{"speed":65,"on":true}}
```

## MQTT integration

### Required

MQTT must be enabled in WLED config (`Config -> Sync Interfaces -> MQTT`).

### Published state topics

Using your WLED device topic (for example `wled/2fa7ad`):

- `wled/2fa7ad/RotaryFanPWM/state` with payload `ON` or `OFF`
- `wled/2fa7ad/RotaryFanPWM/speed` with payload `0..100`

### Subscribed command topics

- `wled/2fa7ad/RotaryFanPWM/set`
	- accepts `ON`, `OFF`, `TRUE`, `FALSE`, `1`, `0`
- `wled/2fa7ad/RotaryFanPWM/speed/set`
	- accepts integer `0..100`

## Home Assistant auto-discovery

When MQTT is connected and `ha-discovery` is enabled, the usermod publishes discovery config for a fan entity.

Discovery topic:

- `homeassistant/fan/<mac>_rotaryfan/config`

If Home Assistant MQTT discovery is enabled, a fan entity should appear automatically.

### Why it does not appear in native WLED integration

The built-in Home Assistant WLED integration only exposes core WLED entities. Usermod custom features are not auto-mapped there. Use MQTT discovery for this usermod fan entity.

## Usermod settings keys

- `enabled`
- `clk-pin`
- `dt-pin`
- `sw-pin`
- `pwm-pin`
- `duty-percent`
- `step-percent`
- `pulses-per-step`
- `invert-encoder`
- `pwm-frequency`
- `button-debounce-ms`
- `on`
- `ha-discovery`

## Wiring notes

- Encoder inputs are active-low with internal pull-ups enabled.
- Typical 4-pin PC fan PWM input expects ~25 kHz logic PWM.
- Default `pwm-frequency` is `25000` Hz.
- Ensure common ground between fan control electronics and ESP board.

## ESP8266 note

On ESP8266, PWM frequency is global for software PWM. Changing `pwm-frequency` affects other `analogWrite()` outputs.

## Troubleshooting

### Fan entity not showing in Home Assistant

- Confirm WLED MQTT is enabled and connected.
- Confirm Home Assistant MQTT integration + discovery are enabled.
- Verify discovery topic exists in broker.
- Reboot WLED once after MQTT config changes.

### HA fan toggles back to previous state

- Confirm commands are sent to `.../RotaryFanPWM/set`.
- Confirm state topic updates are received.

### Preset cycle behavior

- The usermod cycles only across existing preset slots.
- Empty/deleted preset slots are skipped.
