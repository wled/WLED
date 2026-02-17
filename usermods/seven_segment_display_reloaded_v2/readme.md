# Seven Segment Display Reloaded

Uses the overlay feature to create a configurable seven segment display.
Optimized for maximum configurability and use with seven segment clocks by [parallyze](https://www.instructables.com/member/parallyze/instructables/).  
Very loosely based on the existing usermod "seven segment display".

## Installation

Add the compile-time option `-D USERMOD_SSDR` to your `platformio.ini` (or `platformio_override.ini`) or use `#define USERMOD_SSDR` in `my_config.h`.

# Compiling

To enable, add `seven_segment_display_reloaded_v2` to your `custom_usermods`  (e.g. in `platformio_override.ini`)
```ini
[env:usermod_ssdr_d1_mini]
extends = env:d1_mini
custom_usermods = ${env:d1_mini.custom_usermods} seven_segment_display_reloaded_v2
```

For the auto brightness option, the usermod **SN_Photoresistor** or **BH1750_V2** has to be installed as well. See [SN_Photoresistor/readme.md](SN_Photoresistor/readme.md) or [BH1750_V2/readme.md](BH1750_V2/readme.md) for instructions.

## Available Compile-Time Parameters

These parameters can be configured at compile time using `#define` statements in `my_config.h`. The following table summarizes the available options:

| Parameter                         | Default Value | Description |
|-----------------------------------|---------------|-------------|
| umSSDR_ENABLED                    | false         | Enable SSDR usermod |
| umSSDR_ENABLE_AUTO_BRIGHTNESS     | false         | Enable auto brightness (requires USERMOD_SN_PHOTORESISTOR or USERMOD_BH1750) |
| umSSDR_BRIGHTNESS_MIN             | 0             | Minimum brightness value for auto brightness mapping |
| umSSDR_BRIGHTNESS_MAX             | 255           | Maximum brightness value for auto brightness mapping |
| umSSDR_INVERT_AUTO_BRIGHTNESS     | false         | Invert brightness mapping (maps lux min to brightness max) |
| umSSDR_LUX_MIN                    | 0             | Minimum lux level for brightness mapping |
| umSSDR_LUX_MAX                    | 1000          | Maximum lux level for brightness mapping |
| umSSDR_INVERTED                   | false         | Inverted display (background on, digits off) |
| umSSDR_COLONBLINK                 | true          | Enable blinking colon(s) |
| umSSDR_LEADING_ZERO               | false         | Show leading zero for hours (e.g., "07" instead of "7") |
| umSSDR_DISPLAY_MASK               | "H:m"         | Display mask for time format (see below) |
| umSSDR_HOURS                      | (see example) | LED definition for hours digits |
| umSSDR_MINUTES                    | (see example) | LED definition for minutes digits |
| umSSDR_SECONDS                    | ""            | Reserved for seconds if needed |
| umSSDR_COLONS                     | ""            | Segment range for colon separators |
| umSSDR_LIGHT                      | ""            | Segment range for light indicator |
| umSSDR_DAYS                       | ""            | Reserved for day display if needed |
| umSSDR_MONTHS                     | ""            | Reserved for month display if needed |
| umSSDR_YEARS                      | ""            | Reserved for year display if needed |

Additionally, the usermod allows overriding the internal LED segment number mapping with the optional macro:
- umSSDR_NUMBERS

## Settings

All settings can be controlled via the usermod settings page.
Some settings can also be controlled through MQTT with a raw payload or via a JSON request to `/json/state`.

### Parameters Controlled in the Settings Page

- **enabled**
  Enables/disables this usermod.

- **inverted**
  Enables the inverted mode in which the background is lit and the digits are off (black).

- **Colon-blinking**
  Enables the blinking colon(s) if they are defined.

- **Leading-Zero**
  Shows a leading zero for hours when applicable (e.g., "07" instead of "7").

- **enable-auto-brightness**
  Enables the auto brightness feature. This works only when the usermod **SN_Photoresistor** or **BH1750_V2** is installed.

- **auto-brightness-min / auto-brightness-max**
  Maps the lux value from the SN_Photoresistor or BH1750_V2 to brightness values.
  The mapping (default: 0–1000 lux) is mapped to the defined auto brightness limits. Note that WLED current protection might override the calculated value if it is too high.

- **lux-min**
  Defines the minimum lux level for brightness mapping. When the lux value is at or below this level, the display brightness is set to the minimum brightness. Default is `0` lux.

- **lux-max**
  Defines the maximum lux level for brightness mapping. When the lux value is at or above this level, the display brightness is set to the maximum brightness. Default is `1000` lux.

- **invert-auto-brightness**
  Inverts the mapping logic for brightness. When enabled (`true`), `lux-min` maps to the maximum brightness and `lux-max` maps to the minimum brightness. When disabled (`false`), `lux-min` maps to the minimum brightness and `lux-max` to the maximum brightness.

- **Display-Mask**
  Defines the layout for the time/date display. For example, `"H:m"` is the default. The mask characters include:
  - **H** - 00–23 hours
  - **h** - 01–12 hours
  - **k** - 01–24 hours
  - **m** - 00–59 minutes
  - **s** - 00–59 seconds
  - **d** - 01–31 day of month
  - **M** - 01–12 month
  - **y** - Last two digits of year
  - **Y** - Full year (e.g., 2021)
  - **L** - Light LED indicator
  - **:** - Colon separator

- **LED-Numbers**
  LED segment definitions for various parts of the display:
  - LED-Numbers-Hours
  - LED-Numbers-Minutes
  - LED-Numbers-Seconds
  - LED-Numbers-Colons
  - LED-Numbers-Light
  - LED-Numbers-Day
  - LED-Numbers-Month
  - LED-Numbers-Year

## Example LED Definitions

The following is an example of an LED layout for a seven segment display. The diagram below shows the segment positions:

```
  <  A  >
/\       /\
F         B
\/       \/
  <  G  >
/\       /\
E         C
\/       \/
  <  D  >
```

A digit segment can consist of single LED numbers and LED ranges, separated by commas (,)
An example would be 1,3,6-8,23,30-32. In this example the LEDs with the numbers 1,3,6,7,8,23,30,31 and 32 would make up a Segment.
Segments for each digit are separated by semicolons (;) and digits are separated by a colon (:).

### Example for a Clock Display

- **Hour Definition Example:**
  
  59,46;47-48;50-51;52-53;54-55;57-58;49,56:0,13;1-2;4-5;6-7;8-9;11-12;3,10
  
  The definition above represents two digits (separated by ":"):
  
  **First digit (of the hour):**
  - Segment A: 59, 46  
  - Segment B: 47, 48  
  - Segment C: 50, 51  
  - Segment D: 52, 53  
  - Segment E: 54, 55  
  - Segment F: 57, 58  
  - Segment G: 49, 56  

  **Second digit (of the hour):**
  - Segment A: 0, 13  
  - Segment B: 1, 2  
  - Segment C: 4, 5  
  - Segment D: 6, 7  
  - Segment E: 8, 9  
  - Segment F: 11, 12  
  - Segment G: 3, 10

- **Minute Definition Example:**
  
  37-38;39-40;42-43;44,31;32-33;35-36;34,41:21-22;23-24;26-27;28,15;16-17;19-20;18,25
  
  (Definitions can be adjusted according to the physical orientation of your LEDs.)

## Additional Notes

- **Dynamic Brightness Control:**  
Auto brightness is computed using sensor readings from either the SN_Photoresistor or BH1750. The value is mapped between the defined brightness and lux limits, with an option to invert the mapping.

- **Disabling LED Output:**  
A public function `disableOutputFunction(bool state)` is provided to externally disable or enable the LED output. When disabled, the overlay is cleared so the time/date segments are not shown.

## Additional Projects

### 1. Giant Hidden Shelf Edge Clock

This project, available on [Thingiverse](https://www.thingiverse.com/thing:4207524), uses a large hidden shelf edge as the display for a clock. If you build the modified clock that also shows 24 hours, use the following settings in your configuration:

my_config.h Settings:
--------------------------------

```
#define USERMOD_SSDR

#define umSSDR_ENABLED                  true        // Enable SSDR usermod
#define umSSDR_ENABLE_AUTO_BRIGHTNESS   false       // Enable auto brightness (requires USERMOD_SN_PHOTORESISTOR)
#define umSSDR_INVERTED                 false       // Inverted display
#define umSSDR_COLONBLINK               true        // Colon blink enabled
#define umSSDR_LEADING_ZERO             false       // Leading zero disabled
#define umSSDR_DISPLAY_MASK             "H:mL"      // Display mask for time format

// Segment definitions for hours, minutes, seconds, colons, light, days, months, and years
#define umSSDR_HOURS                    "135-143;126-134;162-170;171-179;180-188;144-152;153-161:198-206;189-197;225-233;234-242;243-251;207-215;216-224"
#define umSSDR_MINUTES                  "9-17;0-8;36-44;45-53;54-62;18-26;27-35:72-80;63-71;99-107;108-116;117-125;81-89;90-98"
#define umSSDR_SECONDS                  ""
#define umSSDR_COLONS                   "266-275"    // Segment range for colons
#define umSSDR_LIGHT                    "252-265"    // Segment range for light indicator (added for this project)
#define umSSDR_DAYS                     ""           // Reserved for days if needed
#define umSSDR_MONTHS                   ""           // Reserved for months if needed
#define umSSDR_YEARS                    ""           // Reserved for years if needed

#define umSSDR_INVERT_AUTO_BRIGHTNESS   true
#define umSSDR_LUX_MIN                  50
#define umSSDR_LUX_MAX                  1000

// Brightness limits
#define umSSDR_BRIGHTNESS_MIN           0            // Minimum brightness
#define umSSDR_BRIGHTNESS_MAX           128          // Maximum brightness
```

--------------------------------

*Note:* For this project, the `umSSDR_LIGHT` parameter was added to provide a dedicated segment for a light indicator.

---

### 2. EleksTube Retro Glows Analog Nixie Tube Clock (Non-IPS Version)

The EleksTube project, available at [EleksTube Retro Glows Analog Nixie Tube Clock](https://elekstube.com/products/elekstube-r2-6-bit-electronic-led-luminous-retro-glows-analog-nixie-tube-clock). With the following settings, the SSDR usermod becomes more versatile and can be used with this clock as well:

my_config.h Settings:
--------------------------------

```
#define umSSDR_ENABLED                  true        // Enable SSDR usermod
#define umSSDR_ENABLE_AUTO_BRIGHTNESS   false       // Enable auto brightness (requires USERMOD_SN_PHOTORESISTOR)
#define umSSDR_INVERTED                 false       // Inverted display
#define umSSDR_COLONBLINK               false       // Colon blink disabled
#define umSSDR_LEADING_ZERO             true        // Leading zero enabled
#define umSSDR_DISPLAY_MASK             "H:m:s"     // Display mask for time format

// Segment definitions for hours, minutes, seconds, colons, light, days, months, and years
#define umSSDR_HOURS                    "20,30;21,31;22,32;23,33;24,34;25,35;26,36;27,37;28,38;29,39:0,10;1,11;2,12;3,13;4,14;5,15;6,16;7,17;8,18;9,19"
#define umSSDR_MINUTES                  "60,70;61,71;62,72;63,73;64,74;65,75;66,76;67,77;68,78;69,79:40,50;41,51;42,52;43,53;44,54;45,55;46,56;47,57;48,58;49,59"
#define umSSDR_SECONDS                  "100,110;101,111;102,112;103,113;104,114;105,115;106,116;107,117;108,118;109,119:80,90;81,91;82,92;83,93;84,94;85,95;86,96;87,97;88,98;89,99"
#define umSSDR_COLONS                   ""           // No colon segment mapping needed
#define umSSDR_LIGHT                    ""           // No light indicator defined
#define umSSDR_DAYS                     ""           // Reserved for days if needed
#define umSSDR_MONTHS                   ""           // Reserved for months if needed
#define umSSDR_YEARS                    ""           // Reserved for years if needed

#define umSSDR_INVERT_AUTO_BRIGHTNESS   true
#define umSSDR_LUX_MIN                  50
#define umSSDR_LUX_MAX                  1000

// Brightness limits
#define umSSDR_BRIGHTNESS_MIN           0            // Minimum brightness
#define umSSDR_BRIGHTNESS_MAX           128          // Maximum brightness

#define umSSDR_NUMBERS { \
    { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0 }, /* 0 */ \
    { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 }, /* 1 */ \
    { 0, 0, 0, 0, 0, 0, 1, 0, 0, 0 }, /* 2 */ \
    { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 }, /* 3 */ \
    { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0 }, /* 4 */ \
    { 0, 0, 1, 0, 0, 0, 0, 0, 0, 0 }, /* 5 */ \
    { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 }, /* 6 */ \
    { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 }, /* 7 */ \
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }, /* 8 */ \
    { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0 }, /* 9 */ \
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }  /* Blank */ \
}
```

### 3. Lazy Clock(s) by paralyze
[Lazy 7 Quick build edition](https://www.instructables.com/Lazy-7-Quick-Build-Edition/). The SSDR usermod can be used to drive this and other clock designs by [paralyze](https://www.instructables.com/member/parallyze/)
For example the Lazy 7 Quick build edition has the following settings - depending on the orientation.

- hour "59,46;47-48;50-51;52-53;54-55;57-58;49,56:0,13;1-2;4-5;6-7;8-9;11-12;3,10"

- minute "37-38;39-40;42-43;44,31;32-33;35-36;34,41:21-22;23-24;26-27;28,15;16-17;19-20;18,25"

or

- hour "6,7;8,9;11,12;13,0;1,2;4,5;3,10:52,53;54,55;57,58;59,46;47,48;50,51;49,56"

- minute "15,28;16,17;19,20;21,22;23,24;26,27;18,25:31,44;32,33;35,36;37,38;39,40;42,43;34,41"

--------------------------------

With these modifications, the SSDR usermod becomes even more versatile, allowing it to be used on a wide variety of segment clocks and projects.

*Note:* The ability to override the LED segment mapping via `umSSDR_NUMBERS` provides additional flexibility for adapting to different physical displays.
