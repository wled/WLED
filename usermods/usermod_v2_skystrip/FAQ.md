# SkyStrip Interpretation Guide

This FAQ explains how to read the various HSV-based views of the
`usermod_v2_skystrip` module. Each view maps weather data onto hue,
saturation, and value (brightness) along the LED strip.


## Cloud View (CV)

Markers for sunrise or sunset show as orange pixels. During
precipitation, hue denotes type—deep blue for rain, lavender for snow,
and indigo for mixed—while value scales with probability. In the
absence of precipitation, hue differentiates day from night: daylight
clouds appear pale yellow, nighttime clouds desaturate toward
white. For clouds, saturation is low and value grows with coverage,
keeping even thin clouds visible. Thus, a bright blue pixel highlights
likely rain, whereas a soft yellow glow marks daytime cloud cover.


## Wind View (WV)

The hue encodes wind direction around the compass: blue (240°) points
north, orange (~30°) east, yellow (~60°) south, and green (~120°)
west, with intermediate shades for diagonal winds. Saturation rises
with gustiness—calm breezes stay washed out while strong gusts drive
the color toward full intensity. Value scales with wind strength,
boosting brightness as the highest of sustained speed or gust
approaches 50 mph (or equivalent). For example, a saturated blue pixel
indicates gusty north winds, while a dim pastel green suggests a
gentle westerly breeze.

The mapping between wind direction and hue can be approximated as:

| Direction | Hue (°) | Color  |
|-----------|---------|--------|
| N         | 240     | Blue   |
| NE        | 300     | Purple |
| E         | 30      | Orange |
| SE        | 45      | Gold   |
| S         | 60      | Yellow |
| SW        | 90      | Lime   |
| W         | 120     | Green  |
| NW        | 180     | Cyan   |
| N         | 240     | Blue   |

Note: Hues wrap at 360°, so “N” repeats at the boundary.


## Temperature View (TV)

Hue follows a calibrated cold→hot gradient tuned for pleasing segment
appearance: deep blues near 14 °F transition through cyan and green to
warm yellows at 77 °F and reds at ~104 °F and above. Saturation
reflects humidity via dew‑point spread; muggy air produces softer,
desaturated colors, whereas dry air yields vivid tones. Value is fixed
at mid‑brightness, but local time markers (e.g., noon, midnight)
temporarily darken pixels to mark time. A bright orange‑red pixel thus
signifies hot, dry conditions around 95 °F, whereas a pale cyan pixel
indicates a cool, humid day near 50 °F.

The actual temperature→hue stops used by the renderer are:

| Temp (°F) | Hue (°) | Color       |
|-----------|---------|-------------|
| ≤14       | 234.9   | Deep blue   |
| 32        | 207.0   | Blue/cyan   |
| 50        | 180.0   | Cyan        |
| 68        | 138.8   | Greenish    |
| 77        | 60.0    | Yellow      |
| 86        | 38.8    | Orange      |
| 95        | 18.8    | Orange‑red  |
| ≥104      | 0.0     | Red         |


## 24 Hour Delta View (DV)

Hue represents the temperature change relative to the previous day:
blues for cooling, greens for steady conditions, and yellows through
reds for warming. Saturation encodes humidity trend—the color
intensifies as the air grows drier and fades toward pastels when
becoming more humid. Value increases with the magnitude of change,
combining temperature and humidity shifts, so bright pixels flag
larger swings. A dim blue pixel therefore means a slight cool‑down
with more moisture, while a bright saturated red indicates rapid
warming coupled with drying.

Approximate mapping of day-to-day deltas to color attributes:

| Temperature | Hue (Color) |
|-------------|-------------|
| Cooling     | Blue tones  |
| Steady      | Green       |
| Warming     | Yellow→Red  |

| Humidity   | Saturation |
|------------|------------|
| More humid | Low/Pastel |
| Stable     | Medium     |
| Drier      | High/Vivid |


## Test Pattern View (TP)

This diagnostic view simply interpolates hue, saturation, and value
between configured start and end points along the segment. Hue shifts
steadily from the starting hue to the ending hue, with saturation and
brightness following the same linear ramp. It carries no weather
meaning; a common example is a gradient from black to white to verify
LED orientation.
