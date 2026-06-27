# Weather temperature — Open-Meteo

Fetches the current outdoor temperature from [Open-Meteo](https://open-meteo.com)
(**free, no API key**) and feeds it to the **Word Clock 16x16** usermod's
`WARM/COOL/HOT/COLD` words via the weak-symbol bridge (`wc16_setLiveTempC`). No hard
dependency between usermods and **no edits to `wled00/`**.

Open-Meteo returns `temperature_2m` in **°C**, which is what the bridge expects.

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

## Settings (Usermod Settings → OpenMeteo)
- `enabled`, `feedWordClock`
- `useWledLocation`, `latitude`, `longitude`
- `intervalMinutes` (default 15)

Current temperature is shown on the WLED **Info** page.

## Notes
- The HTTP GET is synchronous and runs only every `intervalMinutes`, so the brief stall
  is infrequent. First fetch is ~30 s after boot (once WiFi is up).
- If both this and the I²C sensor usermod feed the clock, the most recent push wins
  (30 min TTL) — enable whichever source you prefer.
