# Wordclock MK2 | WLED 16x16 w/ ESP32

This usermod adds an **Effect** named **"Word Clock 16x16"** to WLED. Because it is a
regular effect (not an overlay like the older `usermod_v2_word_clock`), it can be
transitioned/crossfaded, colored, palette-mapped, and saved per-preset like any other
effect.

It shows the time in English with **exact-minute** phrasing plus the period of day, e.g.
`IT IS TWENTY ONE MINUTES PAST SEVEN IN THE EVENING`.

## Install / Build

1. The usermod lives entirely in `usermods/usermod_v2_word_clock_16x16/` — it makes no
   changes outside this folder (it uses the default `USERMOD_ID_UNSPECIFIED`, so no edit
   to `wled00/const.h` is needed).
2. Add it to your build environment's `custom_usermods` in `platformio.ini` (or a
   `platformio_override.ini`), e.g.:
   ```ini
   custom_usermods = usermod_v2_word_clock_16x16
   ```
3. Build & flash for your ESP32 (Wemos Lolin32).

## Usage

1. In WLED **LED Preferences**, configure a **2D matrix**: 16×16, and set the
   serpentine / orientation to match the physical wiring (the LED-ID table below is
   serpentine). The effect works in logical X/Y (0–15), so all wiring specifics are
   handled here, not in code.
2. Make sure the device clock is set (NTP + time zone in **Time & Macros**).
3. Select the **Word Clock 16x16** effect on the matrix segment.
   - **Color 1** sets the lit-word color (or pick a **palette** to spread color across
     the matrix).
   - The **Background** slider (intensity) optionally lights all letters faintly behind
     the active phrase (0 = classic all-off look).
   - Minute-to-minute changes **crossfade** using the WLED **Transition** time (the
     "Transition: x.x s" control), so words fade in/out instead of snapping. Set the
     transition to 0 for instant changes.
4. Usermod settings: `enabled` (registers the effect; reboot to apply),
   `showPeriodOfDay` (toggle the MORNING/AFTERNOON/EVENING/NIGHT words), and the
   temperature options below.

### Grammar
- On the hour: `IT IS <hour> O'CLOCK`
- 1–30 past: `IT IS <minutes> MINUTES PAST <hour>` (`A QUARTER PAST` at :15, `HALF PAST` at :30)
- 31–59: `IT IS <minutes> MINUTES UNTIL <next hour>` (`A QUARTER UNTIL` at :45)
- Period: `IN THE MORNING` (00–11), `IN THE AFTERNOON` (12–16), `IN THE EVENING` (17–20),
  `AT NIGHT` (21–23).

### Temperature words (bottom row)
When `showTemperature` is on, one of `COLD / COOL / WARM / HOT` is lit on the bottom row
(folded into the same crossfade as the time). Bands are picked by numeric thresholds:

| Word | Condition |
| ---- | --------- |
| COLD | `temp < coldBelow` |
| COOL | `coldBelow ≤ temp < coolBelow` |
| WARM | `coolBelow ≤ temp < warmBelow` |
| HOT  | `temp ≥ warmBelow` |

All temperature numbers — thresholds, `manualTemp`, and the JSON-API value — are in the
unit chosen by `fahrenheit` (off = °C). Defaults are °C: 10 / 18 / 27.

When a temperature word is shown, the `&` tile (bottom-left, LED 240) lights too.

**Temperature source:** either the built-in Open-Meteo client (below), or push a live
value via the JSON API (state object):
```json
{"WordClock16x16":{"temp":22.5}}
```
The live value (whichever source) is used for 30 min, then falls back to `manualTemp`.
The current temperature + weather state is shown on the WLED **Info** page.

### Weather (Open-Meteo, built in)
Turn on `fetchWeather` to pull the current outdoor temperature and condition from
[Open-Meteo](https://open-meteo.com) (free, no API key) every `fetchMinutes` (default 15).
Plain HTTP, so no TLS library / `lib_deps`.

- **Location:** uses WLED **Time settings** lat/lon when `useWledLocation` is on, else the
  `latitude`/`longitude` set here. `(0,0)` disables fetching.
- **Weather → preset:** turn on `weatherPresets`, then map each weather state to a preset
  (these fields are **dropdowns** of your saved presets, from `/presets.json`; 0 = none).
  When the detected state changes, that preset is applied. WMO codes are grouped as:

  | State | WMO codes | Setting |
  | ----- | --------- | ------- |
  | clear   | 0, 1         | `presetClear` |
  | clouds  | 2, 3         | `presetClouds` |
  | fog     | 45, 48       | `presetFog` |
  | drizzle | 51–57        | `presetDrizzle` |
  | rain    | 61–67, 80–82 | `presetRain` |
  | snow    | 71–77, 85,86 | `presetSnow` |
  | thunder | 95, 96, 99   | `presetThunder` |

  The preset is applied only on a **change** of state (including the first reading after
  boot). First fetch is ~30 s after boot once WiFi is up.

## Not yet implemented (future)
- The 4 corner RGBW LEDs and 4 corner buttons — use **native WLED** config (extra LED
  output/bus + segment for the corners; WLED's button config for the inputs)

## Resources
- NAS-00:/hardware/X-Carve/Projects/2017/Word Clock
- [Wordclock MK1 - w/ Text Shift / Rotation (Adobe Illistrator)](https://docs.google.com/spreadsheets/d/1PluM_poY26YVuXqRocmyo1mvG5tT44V26rKZcX5UzbI/edit?gid=0#gid=0)
- [Wordclock MK1 (Arduino w/ Firmata /w NeoPixel & Raspberry Pi Zero w/ Node-RED) - Build Sheet](https://docs.google.com/spreadsheets/d/1UgpLxv2-_UMIiSN81n5ciU93GWFkNPKmxRbwsBQ3MRw/edit?gid=35318254#gid=35318254)

## Hardware
- Controller: Wemos Lolin32 w/ SSD1306 64x128 [its what I had on-hand...]
- LED Strips: 256x + 4x WS2814 RGBW (GRB)
- Buttons: 
- Sensors
  - 


## Layout
A 16×16 RGBW LED matrix occupies the center of the display for the word clock functionality. Outside this matrix, each corner contains a dedicated push button and a corresponding addressable RGBW LED on a seperate strip from the 16x16 matrix. (4x discrete RGBW LEDs and 4x push buttons).

### Layout (Words)

|   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |
| - | - | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| I | T | K | I | S | S | T | W | E | N | T | Y | T | O | N | E |
| T | W | O | W | T | E | N | J | T | H | I | R | T | E | E | N |
| F | I | V | E | M | E | L | E | V | E | N | B | F | O | U | R |
| T | H | R | E | E | P | N | I | N | E | T | E | E | N | S | U |
| F | O | U | R | T | E | E | N | M | I | D | N | I | G | H | T |
| S | I | X | T | E | E | N | A | E | I | G | H | T | E | E | N |
| S | E | V | E | N | T | E | E | N | Y | T | W | E | L | V | E |
| M | I | N | U | T | E | S | D | Q | U | A | R | T | E | R | B |
| H | A | L | F | J | P | A | S | T | Q | U | N | T | I | L | L |
| S | E | V | E | N | T | H | R | E | E | E | L | E | V | E | N |
| E | I | G | H | T | E | N | I | N | E | T | W | E | L | V | E |
| S | I | X | F | I | V | E | F | O | U | R | T | W | O | N | E |
| Z | O | C | L | O | C | K | J | A | T | I | N | X | T | H | E |
| A | F | T | E | R | N | O | O | N | M | O | R | N | I | N | G |
| A | T | K | N | I | G | H | T | Z | E | V | E | N | I | N | G |
| & | W | A | R | M | C | O | O | L | H | O | T | C | O | L | D |

### Layout (LED IDs)
> NOTE: 16x16, serpentine.

|     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |     |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0   | 1   | 2   | 3   | 4   | 5   | 6   | 7   | 8   | 9   | 10  | 11  | 12  | 13  | 14  | 15  |
| 16  | 17  | 18  | 19  | 20  | 21  | 22  | 23  | 24  | 25  | 26  | 27  | 28  | 29  | 30  | 31  |
| 32  | 33  | 34  | 35  | 36  | 37  | 38  | 39  | 40  | 41  | 42  | 43  | 44  | 45  | 46  | 47  |
| 48  | 49  | 50  | 51  | 52  | 53  | 54  | 55  | 56  | 57  | 58  | 59  | 60  | 61  | 62  | 63  |
| 64  | 65  | 66  | 67  | 68  | 69  | 70  | 71  | 72  | 73  | 74  | 75  | 76  | 77  | 78  | 79  |
| 80  | 81  | 82  | 83  | 84  | 85  | 86  | 87  | 88  | 89  | 90  | 91  | 92  | 93  | 94  | 95  |
| 96  | 97  | 98  | 99  | 100 | 101 | 102 | 103 | 104 | 105 | 106 | 107 | 108 | 109 | 110 | 111 |
| 112 | 113 | 114 | 115 | 116 | 117 | 118 | 119 | 120 | 121 | 122 | 123 | 124 | 125 | 126 | 127 |
| 128 | 129 | 130 | 131 | 132 | 133 | 134 | 135 | 136 | 137 | 138 | 139 | 140 | 141 | 142 | 143 |
| 144 | 145 | 146 | 147 | 148 | 149 | 150 | 151 | 152 | 153 | 154 | 155 | 156 | 157 | 158 | 159 |
| 160 | 161 | 162 | 163 | 164 | 165 | 166 | 167 | 168 | 169 | 170 | 171 | 172 | 173 | 174 | 175 |
| 176 | 177 | 178 | 179 | 180 | 181 | 182 | 183 | 184 | 185 | 186 | 187 | 188 | 189 | 190 | 191 |
| 192 | 193 | 194 | 195 | 196 | 197 | 198 | 199 | 200 | 201 | 202 | 203 | 204 | 205 | 206 | 207 |
| 208 | 209 | 210 | 211 | 212 | 213 | 214 | 215 | 216 | 217 | 218 | 219 | 220 | 221 | 222 | 223 |
| 224 | 225 | 226 | 227 | 228 | 229 | 230 | 231 | 232 | 233 | 234 | 235 | 236 | 237 | 238 | 239 |
| 240 | 241 | 242 | 243 | 244 | 245 | 246 | 247 | 248 | 249 | 250 | 251 | 252 | 253 | 254 | 255 |


---

## Pin Info

| Pin    | Used by     | Pin Notes          |
| ------ | ----------- | ------------------ |
| GPIO0  |  Button     | Touch, Flash Boot  |
| GPIO1  | Available   | \-                 |
| GPIO2  | Available   | Touch, Bootstrap   |
| GPIO3  | Available   | \-                 |
| GPIO4  | I2C         | Touch              |
| GPIO5  | I2C         | \-                 |
| GPIO6  | System      | \-                 |
| GPIO7  | System      | \-                 |
| GPIO8  | System      | \-                 |
| GPIO9  | System      | \-                 |
| GPIO10 | System      | \-                 |
| GPIO11 | System      | \-                 |
| GPIO12 | Available   | Touch, Bootstrap   |  < Button Top Left
| GPIO13 | Available   | Touch              |  < Button Top Right
| GPIO14 | Available   | Touch              |  < Button Bottom Left
| GPIO15 | Available   | Touch              |  < Button Bottom Right
| GPIO16 | LED Digital | \-                 |
| GPIO17 | Available   | \-                 |
| GPIO18 | Available   | \-                 |
| GPIO19 | Available   | \-                 |
| GPIO20 | Available   | \-                 |
| GPIO21 | Available   | \-                 |
| GPIO22 | Available   | \-                 |
| GPIO23 | Available   | \-                 |
| GPIO24 | System      | \-                 |
| GPIO25 | Available   | \-                 |  < LEDs 256x WS2814 | Matrix
| GPIO26 | Available   | \-                 |  < LEDs   4x WS2814 | Corners 
| GPIO27 | Available   | Touch              |
| GPIO28 | System      | \-                 |
| GPIO29 | System      | \-                 |
| GPIO30 | System      | \-                 |
| GPIO31 | System      | \-                 |
| GPIO32 | Available   | Touch, Analog      |
| GPIO33 | Available   | Touch, Analog      |
| GPIO34 | Available   | Input Only, Analog |
| GPIO35 | Available   | Input Only, Analog |
| GPIO36 | Available   | Input Only, Analog |
| GPIO37 | Available   | Input Only, Analog |
| GPIO38 | Available   | Input Only, Analog |
| GPIO39 | Available   | Input Only, Analog |
