# SkyStrip Interpretation Guide

This FAQ explains how to interpret the SkyStrip display.
There are 4 strips, from top to bottom:
- Cloud View
- Wind View
- Temperature View
- 24 Hour Delta View

## Cloud View (CV)

Displays the next 48 hour forecast of clouds and rain, with sunrise/sunset markers.

### Features
- **Sunrise/sunset markers** appear as single orange pixels at time of sunrise/sunset.

- **Day/night clouds** pale yellow in daylight and turn darker at night.

- **Cloud coverage** from sparse to full.

- **Precipitation** rain is blue, snow is purple. Pastel shades
  represent less chance of precipitation, bold colors indicate it's
  likely. Brightness indicates forecast accumulation.

### Config notes
- `RainMaxInHr`: sets the precipitation rate that maps to full
  brightness (default 1.0 in/hr), so lower values make lighter rates pop
  brighter.
- `CloudWaveHalfPx`: half-cycle length of the dithering wave in pixels
  (default 2.2); lower values make the on/off cloud bands tighter.


## Wind View (WV)

Displays the next 48 hour forecast of wind direction and velocity.

### Features
- **Color shows where the wind is coming from.** Hue walks around the
  compass so you can read direction at a glance (e.g., blue for north,
  orange for east, yellow for south, green for west, with smooth
  blends in between).

- **Brightness tracks how windy it feels.** Value uses the stronger of
  sustained speed or gusts, dimming calm periods and pushing toward
  full brightness as winds climb toward 50 mph.

- **Saturation highlights gustiness.** Colors stay pastel when gusts
  match the steady wind, then grow more vivid as gusts pull ahead,
  making choppy conditions easy to spot.

| Direction | Color  | Hue (°) |
|-----------|--------|---------|
| N         | Blue   | 240     |
| NE        | Purple | 300     |
| E         | Orange | 30      |
| SE        | Gold   | 45      |
| S         | Yellow | 60      |
| SW        | Lime   | 90      |
| W         | Green  | 120     |
| NW        | Cyan   | 180     |
| N         | Blue   | 240     |

Note: Hues wrap at 360°, so “N” repeats at the boundary.


## Temperature View (TV)

Displays the next 48 hours forecast of temperature and humidity.

### Features
- **Hue traces the temperature.** Colors shift through the palette as the
  forecast warms or cools, so you can read temperature at a glance.
- **Degree crossings are marked.** Single ticks mark each 5 °F step, and
  double ticks mark the 10 °F steps to anchor the scale.
- **Time notches keep you oriented.** Small notches appear every three
  hours, with larger ones at noon and midnight so you can line up events
  to the clock.
- **Saturation reflects humidity.** Colors wash out when it’s muggy and
  stay vivid when the air is dry.

| Center (°F) | Color    | Hue (°) |
|-------------|----------|---------|
| -45         | yellow   | 60      |
| -30         | orange   | 30      |
| -15         | red      | 0       |
| 0           | magenta  | 300     |
| 15          | purple   | 275     |
| 30          | blue     | 220     |
| 45          | cyan     | 185     |
| 60          | green    | 130     |
| 75          | yellow   | 60      |
| 90          | orange   | 30      |
| 105         | red      | 0       |
| 120         | magenta  | 300     |
| 135         | purple   | 275     |
| 150         | blue     | 220     |

Colors wrap outside the 0–120 °F range—it’s “so cold it’s hot” or “so hot
it’s cold.”

### Config notes
- `ColorMap`: pipe-separated `center:hue` pairs set the palette (hues can be
  numbers or names); defaults to 15 °F steps using the table above.
- `MoveOffset`: optional integer shift, applied on save, that moves every
  color stop by the same °F amount without editing each entry.


## 24-Hour Delta View (DV)

Shows how much warmer or colder it is forecast to be compared to the
same time 24 hours prior.

Color indicates how much change is forecast:

| Change vs 24h prior | Strip color | Brightness |
|---------------------|-------------|------------|
| Colder than -15°    | Purple      | Shout      |
| -15° to -10°        | Indigo      | Strong     |
| -10° to -5°         | Cyan-blue   | Moderate   |
| -5° to +5°          | Off (blank) | Off        |
| +5° to +10°         | Yellow      | Moderate   |
| +10° to +15°        | Orange      | Strong     |
| Warmer than +15°    | Red         | Shout      |

### Config notes
- Default thresholds are 5 / 10 / 15 °F (configurable with
  `DeltaThresholds`).


## Debug Pixel

You can use the debug pixel to examine details of each view at a
particular point on the display.

### Choose a pixel

Set the **Pixel index** to the LED you want to inspect (0 is the left edge,
141 is the right), replacing the default `-1` disabled value.

### Save and spot the blink

Click **Save** to lock in the index. The selected pixel position blinks as
a vertical column across all strips until you turn **Debug pixel** back
off. This makes it easy to confirm you’re watching the right spot.

### Read the per-view values

Reopen the config screen to see the captured inputs and outputs for
that pixel in the **Debug pixel** box. Change the index and save again
any time you want a fresh sample.  Set the **Pixel Index** back to
`-1` when you are done.
