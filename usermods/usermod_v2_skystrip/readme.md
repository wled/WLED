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

Enter the latitude and longitude for the desired forecast. There are
several ways to do this:
1. Enter the latitude and longitude as signed floating point numbers
   in the `Latitude` and `Longitude` config fields.
2. Enter a combined lat/long string in the `Location` field, examples:
- `54.9352° S, 67.6059° W`
- `-54.9352, -67.6059`
- `-54.9352 -67.6059`
- `S54°42'7", W67°40'33"`
3. Enter a geo location string like `oakland,ca,us` in the `Location` field.

## Interpretation

Please see the [Interpretation FAQ](./FAQ.md) for more information on how to
interpret the forecast views.
