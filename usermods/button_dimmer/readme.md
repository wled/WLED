# Button Dimmer

Turns one WLED push button into a wall-dimmer style control. Everything is set up on the usermod settings page, no presets or API commands are needed for the common cases.

| Gesture | Default | Configurable |
|---|---|---|
| Single click | Toggle on/off | any action |
| Double click | Full brightness | any action |
| 3, 4, ... clicks | - | any action |
| Hold | Smooth brightness ramp | ramp, or any action fired once |

While the button is held, the brightness ramps smoothly. Each new hold starts in the opposite direction of the previous one, and the ramp turns around when it reaches the upper or lower limit (after a short pause, so it is easy to stop exactly at an end). Holding while the light is off switches it on at the lowest level and ramps up.

## Installation

Add `button_dimmer` to `custom_usermods` in your PlatformIO environment, for example in `platformio_override.ini`:

```ini
[env:esp32dev_button_dimmer]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods} button_dimmer
```

Optional build flag: `-D BUTTON_DIMMER_MAX_CLICKS=4` sets the highest click count that can get its own action.

## Setup

1. Configure the button as usual in *Config -> LED Preferences* (type *Pushbutton*, *Pushbutton inverted* or *Touch*).
2. Open *Config -> Usermods -> ButtonDimmer*, pick the button and the actions, and save.

The *Level* drop-down only changes how many settings are shown. Settings of a hidden level keep their values.

| Level | Settings |
|---|---|
| Easy | enable, button, action for single click, double click and hold |
| Medium | actions for 3 and more clicks, brightness limits of the ramp, turn-around on/off |
| Expert | all timings, ramp step size, sync behaviour |

### Actions

| Action | Value field |
|---|---|
| None | |
| Toggle on/off, On, Off | |
| Full brightness | (uses the *max brightness* of the ramp) |
| Set brightness | 1-255 |
| Preset | preset ID - use this for effects, playlists, colours of single segments or any API command |
| Colour | `RRGGBB` or `RRGGBBWW` (hex), sets the primary colour of the selected segments |
| Random colour | |
| Dim while held | (hold only) |

A single click is carried out as soon as it is certain that no further click follows. If only the single click has an action, it fires immediately on release; otherwise it waits for the *multi click gap* (300 ms by default).

### Expert settings

| Setting | Default | Meaning |
|---|---|---|
| Debounce | 50 ms | shorter presses are ignored |
| Hold time | 400 ms | a press longer than this is a hold |
| Multi click gap | 300 ms | longest pause between the clicks of a multi-click |
| Step interval | 25 ms | time between two ramp steps |
| Step size | 3 | brightness change per ramp step |
| End pause | 500 ms | rest at an end of the ramp before turning around |
| Sync while dimming | off | send sync (UDP) notifications on every ramp step instead of once when the button is released |

## Limitations

- The usermod takes over the selected button completely. The stock functions of that button are not available while the usermod is enabled: button presets from *Time & Macros*, MQTT button events, and on button 0 the long holds for AP mode (5 s) and factory reset (10 s).
- Only momentary buttons are handled (push button, inverted push button, touch). Switches, PIR sensors and analog inputs are left to WLED.
- One button per device.
