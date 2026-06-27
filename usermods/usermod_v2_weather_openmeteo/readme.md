# Weather — Open-Meteo

Fetches current weather from [Open-Meteo](https://open-meteo.com) (**free, no API key**) and:

1. **Temperature → Word Clock** — feeds `temperature_2m` (°C) to the **Word Clock 16x16**
   usermod's `WARM/COOL/HOT/COLD` words via the weak-symbol bridge (`wc16_setLiveTempC`).
   No hard dependency between usermods and **no edits to `wled00/`**.
2. **Weather → Preset** — maps the WMO `weather_code` to a weather *state* and, when the
   state changes, applies a preset you choose for that state (e.g. show your "rain" preset
   when it starts raining).

## Build
```ini
custom_usermods = usermod_v2_word_clock_16x16 usermod_v2_weather_openmeteo
```
No API key. Uses the ESP `HTTPClient` over plain HTTP (Open-Meteo serves the API on
port 80), so no TLS library is needed and there are no extra `lib_deps`.

## Location
- Default: uses WLED's **Time settings** latitude/longitude (`useWledLocation`).
- Or set `latitude`/`longitude` in this usermod's settings (with `useWledLocation` off).
- `(0, 0)` means "unset" → fetching is disabled (Info shows "set location").

## Weather states → presets
Set `weatherPresets` on, then map each state to a preset id (0 = no preset). When the
detected state changes, that preset is applied. WMO codes are grouped as:

| State | WMO codes |
| ----- | --------- |
| clear   | 0, 1 |
| clouds  | 2, 3 |
| fog     | 45, 48 |
| drizzle | 51–57 |
| rain    | 61–67, 80–82 |
| snow    | 71–77, 85, 86 |
| thunder | 95, 96, 99 |

The preset is applied only on a **change** of state (including the first reading after
boot), so it won't keep re-triggering. Build each preset in WLED however you like (effect,
colors, the word-clock effect tinted blue for rain, etc.).

## Settings (Usermod Settings → OpenMeteo)
- `enabled`, `feedWordClock`
- `useWledLocation`, `latitude`, `longitude`
- `intervalMinutes` (default 15)
- `weatherPresets` + `presetClear/Clouds/Fog/Drizzle/Rain/Snow/Thunder`

Current temperature + weather state are shown on the WLED **Info** page.

## Notes
- The HTTP GET is synchronous and runs only every `intervalMinutes`, so the brief stall
  is infrequent. First fetch is ~30 s after boot (once WiFi is up).
