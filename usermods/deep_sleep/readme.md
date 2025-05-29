# Deep Sleep usermod

This usermod unleashes the low power capabilities of th ESP: when you power off your LEDs (using the UI power button or a macro) the ESP will be put into deep sleep mode, reducing power consumption to a minimum.
During deep sleep the ESP is shut down completely: no WiFi, no CPU, no outputs. The only way to wake it up is to use an external signal or a button. Once it wakes from deep sleep it reboots so ***make sure to use a boot-up preset.***

# A word of warning

When you disable the WLED option 'Turn LEDs on after power up/reset' and 'DelaySleep' is set to zero the ESP will go into deep sleep directly after power-up and only start WLED after it has been woken up.
If the ESP can not be awoken from deep sleep due to a wrong configuration it has to be factory reset, disabling sleep at power-up. There is no other way to wake it up.

# Power Consumption in deep sleep

The current drawn by the ESP in deep sleep mode depends on the type and is in the range of 5uA-20uA (as in micro Amperes):
- ESP32: 10uA
- ESP32 S3: 8uA
- ESP32 S2: 20uA
- ESP32 C3: 5uA
- ESP8266: 20uA (not supported in this usermod)

However, there is usually additional components on a controller that increase the value:
- Power LED: the power LED on a ESP board draws 500uA - 1mA
- LDO: the voltage regulator also draws idle current. Depending on the type used this can be around 50uA up to 10mA (LM1117). Special low power LDOs with very low idle currents do exist
- Digital LEDs: WS2812 for example draw a current of about 1mA per LED. To make good use of this usermod it is required to power them off using MOSFETs or a Relay

For lowest power consumption, remove the Power LED and make sure your board does not use an LM1117. On a ESP32 C3 Supermini with the power LED removed (no other modifications) powered through the 5V pin I measured a current draw of 50uA in deep sleep.

# Useable GPIOs

The GPIOs that can be used to wake the ESP from deep sleep are limited. Only pins connected to the internal RTC unit can be used:

- ESP32: GPIO 0, 2, 4, 12-15, 25-39
- ESP32 S3: GPIO 0-21
- ESP32 S2: GPIO 0-21
- ESP32 C3: GPIO 0-5
- ESP8266 is not supported in this usermod

You can however use the selected wake-up pin normally in WLED, it only gets activated as a wake-up pin when your LEDs are powered down.

# Limitations

To keep this usermod simple and easy to use, it is a very basic implementation of the low-power capabilities provided by the ESP. If you need more advanced control you are welcome to implement your own version based on this usermod.

## Usermod installation

Add `deep_sleep` to `custom_usermods` in your platformio.ini. Settings can be changed in the usermod config UI.

### Define Settings

There are five parameters you can set:

- GPIO: the pin to use for wake-up
- WakeWhen High/Low: the pin state that triggers the wake-up
- Pull-up/down disable: enable or disable the internal pullup resistors during sleep (does not affect normal use while running)
- Wake after: if set larger than 0, ESP will automatically wake-up after this many seconds (Turn LEDs on after power up/reset is overriden, it will always turn on)
- Delay sleep: if set larger than 0, ESP will not go to sleep for this many seconds after you power it off. Timer is reset when switched back on during this time.

To override the default settings, place the `#define` in wled.h or add `-D DEEPSLEEP_xxx` to your platformio_override.ini build flags

* `DEEPSLEEP_WAKEUPPIN x`    - define the pin to be used for wake-up, see list of useable pins above. The pin can be used normally as a button pin in WLED.
* `DEEPSLEEP_WAKEWHENHIGH`   - if defined, wakes up when pin goes high (default is low)
* `DEEPSLEEP_DISABLEPULL`    - if defined, internal pullup/pulldown is disabled in deep sleep (default is ebnabled)
* `DEEPSLEEP_WAKEUPINTERVAL` - number of seconds after which a wake-up happens automatically, sooner if button is pressed. 0 = never. accuracy is about 2%
* `DEEPSLEEP_DELAY`          - delay between power-off and sleep

example for env build flags:
 `-D USERMOD_DEEP_SLEEP`
 `-D DEEPSLEEP_WAKEUPPIN=4`
 `-D DEEPSLEEP_DISABLEPULL=0` ;enable pull-up/down resistors by default
 `-D DEEPSLEEP_WAKEUPINTERVAL=43200` ;wake up after 12 hours (or when button is pressed)

### Hardware Setup

To wake from deep-sleep an external trigger signal on the configured GPIO is required. When using timed-only wake-up, use a GPIO that has an on-board pull-up resistor (GPIO0 on most boards). When using push-buttons it is highly recommended to use an external pull-up resistor: not all IO's on all devices have properly working internal resistors.

Using sensors like PIR, IR, touch sensors or any other sensor with a digital output can be used instead of a button.

now go on and save some power
@dedehai

## Change log
2024-09
* Initial version
2024-10
* Changed from #define configuration to UI configuration