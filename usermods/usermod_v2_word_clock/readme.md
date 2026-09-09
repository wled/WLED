# Word Clock Usermod

This usermod turns a grid of LEDs into a word clock. Each LED sits behind one
letter in a matrix of seemingly random characters. The letters form words that
can be lit to show the current time.

German is selected by default for compatibility with the original WLED word
clock. Dutch can be selected at compile time with `-D WORD_CLOCK_LANGUAGE_NL`.
The language is not a runtime setting.

The default German matrix has 11 columns and 10 rows, plus four minute-dot
markers (`1234`) at the end of the layout. The dots are cumulative: at 12:03,
dots 1, 2, and 3 are lit. Adding these minute dots is optional; a marker-free
custom matrix uses nearest-five-minute rounding instead.

The generator can create Dutch or German example matrices. For example, a
Dutch matrix can be generated with the language selector in
`word-clock-matrix-generator.html`:

The default German matrix is 11 columns wide and 10 rows high, followed by
four minute-dot positions:

    ESXISTXFÜNF
    ZEHNZWANZIG
    DREIVIERTEL
    VORXXXXNACH
    HALBXELFÜNF
    EINSXXXZWEI
    DREIXXXVIER
    SECHSXXACHT
    SIEBENZWÖLF
    ZEHNEUNXUHR
    1234

At 6:43 (06:40 and 3 minutes), the default German clock displays `ZWANZIG VOR SIEBEN`
and lights 3 of the minute-dot markers:

    ES.IST.....
    ....ZWANZIG
    ...........
    VOR........
    ...........
    ...........
    ...........
    ...........
    SIEBEN.....
    ...........
    123.

The usermod does not choose LED colors. WLED continues to control the colors
and effects; this usermod only changes their brightness based on the time.

These settings control the word clock:

    * `Active`: turn the word clock on or off.
    * `Brightness Active`: brightness of the letters used for the current time. Use 0 for off and 255 for full brightness.
    * `Brightness Inactive`: brightness of the other letters. Use 0 for off and 255 for full brightness.
    * `Meander`: set to `false` when the LED strip runs left to right on every row. Set to `true` when each row alternates direction.
    * `Character Matrix`: the layout letters and optional minute markers. Include all words needed by the selected language, with each row placed directly after the previous row.
    * `Character Matrix Width`: the number of letters in each row. It cannot be greater than the total number of letters or smaller than the longest word the clock needs to display.
    * `Led Offset`: the number of physical LEDs before the first word-clock letter.
    * `Display It Is`: include the `ES IST` or `HET IS` prefix when supported by the selected language.
    * `Test Hour`: the hour to display for testing, from 0 to 23. Set it to -1 to use the real time.
    * `Test Minute`: the minute to display for testing, from 0 to 59.

Words are highlighted only when they are found and fit completely within one
matrix row. A complete `1234` marker set enables floor rounding and cumulative
minute dots; without markers, the clock rounds to the nearest five minutes.


## Installation

1. Copy `platformio_override.ini.sample` from the `usermods/usermod_v2_word_clock`
   folder to `platformio_override.ini` in the top WLED folder. Update the board
   and serial port settings to match your hardware. For example:

    ```ini
    [platformio]
    default_envs = wordclock

    [env:wordclock]
    extends = env:esp32dev
    # Use `ls /dev/cu.*` to find the correct port for your connected board
    upload_port = /dev/cu.wchusbserial123
    upload_speed = 921600
    monitor_port = /dev/cu.wchusbserial123
    monitor_speed = 115200
    custom_usermods = ${env:esp32dev.custom_usermods} usermod_v2_word_clock

    # Optional: uncomment to enable specific language
    # build_flags =
    #   ${env:esp32dev.build_flags}
    #   -D WORD_CLOCK_LANGUAGE_NL
    ```

2. Build WLED and upload it to your controller:

    npm run build
    pio run -e wordclock --target upload

3. Open WLED and activate the usermod at Config > Usermods > Word Clock.


## Customization

To create a custom matrix, open
`word-clock-matrix-generator.html`. When the matrix is ready, click
"Copy text" and paste the result into `Character Matrix` in
WLED > Config > Usermods > Word Clock. Remove all line breaks, and set
`Character Matrix Width` to the number of columns in each row.

The generator supports Dutch and German configurations, but its JavaScript
language data is maintained separately from the firmware language packs.
Changing the language grammar in firmware is an advanced customization.

The merged usermod stores settings under the `Word Clock` configuration
object. Existing settings under `WordClockUsermod` are imported and rewritten
under the new object when the merged usermod starts.
The friendly setting names `Led Offset` and `Display It Is` migrate existing
`ledOffset` and `displayItIs` values automatically.


### Define Your Options

The usermod is activated with `custom_usermods = usermod_v2_word_clock`.
Use `-D WORD_CLOCK_LANGUAGE_NL` to select Dutch instead of the default German.


### PlatformIO requirements

No special requirements.


## Change Log

* 2026-09-08 Rewrote logic to be more generic, added Dutch language and matrix generator
* 2022-08-18 Added meander wiring pattern.
* 2022-03-30 Initial commit
