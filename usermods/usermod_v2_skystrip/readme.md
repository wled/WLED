# SkyStrip

This usermod displays the weather forecast on several parallel LED strips.
It currently includes Cloud, Wind, Temperature, 24 Hour Delta, and TestPattern views.

## Installation

Add `usermod_v2_skystrip` to `custom_usermods` in your PlatformIO environment.

## Configuration

Acquire an API key from
[OpenWeatherMap](https://openweathermap.org/api/one-call-3). The SkyStrip
module makes one API call per hour, plus up to 24 calls on first startup.
This typically stays within free-tier limits, but check your current plan.

Enter the latitude and longitude for the desired forecast. You can:
1. Enter signed floating-point values in the `Latitude` and `Longitude` fields.
2. Enter a combined lat/long string in the `Location` field, for example:
   - `54.9352° S, 67.6059° W`
   - `-54.9352, -67.6059`
   - `-54.9352 -67.6059`
   - `S54°42'7", W67°40'33"`
3. Enter a geo-location string (e.g., `oakland,ca,us`) in the `Location` field.

## Interpretation

Please see the [Interpretation FAQ](./FAQ.md) for more information on how to
interpret the forecast views.

## Hardware/Platform notes

- SkyStrip was developed/tested using the
  [Athom esp32-based LED strip controller](https://www.athom.tech/blank-1/wled-esp32-rf433-music-addressable-led-strip-controller).
- Display used for development: four
  [WS2815 LED strips](https://www.superlightingled.com/dc12v-ws2815-upgraded-ws2812b-1m-144-leds-individually-addressable-digital-led-strip-lights-dual-signal-wires-waterproof-dream-color-programmable-5050-rgb-flexible-led-ribbon-light-p-2134:fd57dd8a8ac1ee0e78f5493a35b28792.html)
  , each 1 m long,
  12 V, 5050 RGB, 144 LEDs/m, individually addressable with
  dual-signal (backup) line; arranged side‑by‑side (physically
  parallel). Any equivalent WS281x‑compatible strip of similar density
  should work; adjust power and wiring accordingly.
- SkyStrip makes 25 API calls to the
  [OpenWeatherMap One Call API](https://openweathermap.org/api/one-call-3)
  when it first starts running and one API call per hour after that.
- Based on comparisons with a baseline build SkyStrip uses:
  * RAM: +2080 bytes
  * Flash: +153,812 bytes
