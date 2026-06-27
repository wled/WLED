# Environment Sensor (3-in-1, I²C)

Reads the "3-in-1" I²C module (**HTU21D** temp/humidity + **BMP180** pressure/temp +
**BH1750** light). **v1 scope: temperature only** — HTU21D primary, BMP180 fallback.

It feeds the temperature to the **Word Clock 16x16** usermod (for its `WARM/COOL/HOT/COLD`
words) through a **weak-symbol bridge** (`wc16_setLiveTempC`), so there is no hard
dependency between the two usermods and **no edits to `wled00/`**. If the word clock isn't
compiled in, the sensor simply skips the call.

## Build
Add to your build env's `custom_usermods` (alongside the word clock):
```ini
custom_usermods = usermod_v2_word_clock_16x16 usermod_v2_environment_3in1
```
No external libraries are required (raw `Wire` reads). Set I²C pins in WLED LED prefs (or
`-D I2CSDAPIN=/-D I2CSCLPIN=`); WLED initialises `Wire` at boot.

## I²C addresses
| Sensor | Addr | Used in v1 |
| ------ | ---- | ---------- |
| HTU21D | 0x40 | yes (temperature) |
| BMP180 | 0x77 | yes (fallback temperature) |
| BH1750 | 0x23 | not yet |

## Settings (Usermod Settings → Env3in1)
- `enabled`
- `feedWordClock` — push temperature to the word clock
- `readSeconds` — interval between readings (default 30)

Current temperature is shown on the WLED **Info** page.

> Note: reads are non-blocking for HTU21D (trigger then read ~50 ms later). The BMP180
> fallback uses a single ~5 ms `delay()` (within WLED's 10 ms guidance). Sensor I/O has
> been written to datasheet but **needs on-device verification**.

## Future
- Humidity (HTU21D), pressure (BMP180), lux (BH1750) → Info / JSON
- Configurable I²C addresses; selectable temperature source
