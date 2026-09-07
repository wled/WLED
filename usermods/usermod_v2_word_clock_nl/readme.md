# Word Clock NL Usermod V2

This usermod turns a grid of LEDs into a Dutch word clock. Each LED sits behind
one letter in a matrix of seemingly random characters. The letters form words
that can be lit to show the current time. Here is an example 12x12 matrix:

    NEUEHETHETHT
    NFEYIEISISVT
    VIJFKWARTNAA
    AGBETIENOAEE
    AINOVERVOORA
    TFUHALFIEENY
    OEHIBUZEVENV
    NBNMTWEEELFN
    DRIEVIERVIJF
    NEGENZESTIEN
    TWAALFACHTNT
    BXNHWEUUROAD

The matrix contains hidden words that can be combined into Dutch sentences
describing the time in five-minute steps, such as "HET IS TIEN OVER HALF
ZEVEN". The usermod brightens the letters needed for the current sentence and
darkens the other letters.

This is what the same 12x12 matrix would look like at 6:40:

    .......HET..
    ......IS....
    ............
    ....TIEN....
    ...OVER.....
    ...HALF.....
    ......ZEVEN.
    ............
    ............
    ............
    ............
    ............

The usermod does not choose the LED colors. WLED continues to control the
colors and effects; this usermod only changes their brightness based on the
time.

These settings control the word clock:

    * `Active`: turn the word clock on or off.
    * `Brightness Active`: brightness of the letters used for the current time. Use 0 for off and 255 for full brightness.
    * `Brightness Inactive`: brightness of the other letters. Use 0 for off and 255 for full brightness.
    * `Meander`: set to `false` when the LED strip runs left to right on every row. Set to `true` when each row alternates direction.
    * `Character Matrix`: the uppercase letters in your clock face. Include all the words needed to display the Dutch time sentences, with each row placed directly after the previous row.
    * `Character Matrix Width`: the number of letters in each row. It cannot be greater than the total number of letters or smaller than the longest word the clock needs to display.
    * `Matrix Char Offset`: the number of letters to skip at the beginning of the matrix when matching letters to LEDs. This is useful when the first physical LEDs do not correspond to the first letters.
    * `Test Hour`: the hour to display for testing, from 0 to 23. Set it to -1 to use the real time.
    * `Test Minute`: the minute to display for testing, from 0 to 59.

Words are highlighted only when they are found and fit completely within one
matrix row. The clock rounds the current time to the nearest five minutes.


## Installation

1. Copy `platformio_override.sample.ini` from the main WLED folder to
    `platformio_override.ini`. Update the board and serial port settings to
    match your hardware. For example:

    [platformio]
    default_envs = wordclock_nl

    [env:wordclock_nl]
    extends = env:esp32dev
    # Use `ls /dev/cu.*` to find the correct port for your connected board
    upload_port = /dev/cu.wchusbserial123 
    upload_speed = 921600
    monitor_port = /dev/cu.wchusbserial123
    monitor_speed = 115200
    custom_usermods = ${env:esp32dev.custom_usermods} usermod_v2_word_clock_nl

2. Make sure `USERMOD_ID_WORDCLOCK_NL` is defined in `wled00/const.h`.

3. Build WLED and upload it to your controller:

    npm run build
    pio run -e wordclock_nl --target upload

4. Open WLED and activate the usermod at Config > Usermods > Word Clock NL.


## Customization

This usermod is designed for Dutch and uses the character matrix from the
usermod settings. To create a custom matrix, open
`woordklok-matrix-generator.html`. When the matrix is ready, click
"KOPIEER TEXT" and paste the result into `Character Matrix` in
WLED > Config > Usermods > Word Clock NL. Remove all line breaks, and set
`Character Matrix Width` to the number of columns in each row.

Using a language other than Dutch requires changes to the code that creates and
matches time sentences in both `woordklok-matrix-generator.html` and
`usermod_v2_word_clock_nl.cpp`. This is an advanced customization. Test the
result thoroughly with the HTML generator so that every supported time is
displayed correctly.
