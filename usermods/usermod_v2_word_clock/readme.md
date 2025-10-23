# Word Clock Usermod V2

This usermod drives an 11x10 or a 11x11 pixel matrix wordclock with WLED. There are 4 additional dots for the minutes.
The visualisation is described by 4 masks with LED numbers (single dots for minutes, minutes, hours and "clock"). The index of the LEDs in the masks always starts at 0, even if the ledOffset is not 0.
There are 3 parameters that control behavior:

active: enable/disable usermod
diplayItIs: enable/disable display of "Es ist" on the clock
ledOffset: number of LEDs before the wordclock LEDs

## Update for alternative wiring pattern

Based on this fantastic work I added an alternative wiring pattern.
The original used a long wire to connect DO to DI, from one line to the next line.

I wired my clock in meander style. So the first LED in the second line is on the right.
With this method, every other line was inverted and showed the wrong letter.

I added a switch in usermod called "meanderwiring" to enable/disable the alternate wiring pattern and a switch for the 11x11 grid.

## 11x11 Grid
I integreated the Grafics from https://github.com/panbachi/wordclock/blob/master/graphics/plate/de_DE_s1.svg  

## Installation

Copy and update the example `platformio_override.ini.sample`
from the Rotary Encoder UI usermod folder to the root directory of your particular build.
This file should be placed in the same directory as `platformio.ini`.

### Define Your Options

* `USERMOD_WORDCLOCK`   - define this to have this usermod included wled00\usermods_list.cpp

### PlatformIO requirements

No special requirements.

## Change Log

2022/08/18 added meander wiring pattern.

2022/03/30 initial commit

## Home Assistant Configuration
Now, we will create the necessary entities in Home Assistant to display the switch and send commands to WLED.

Step 2.1: Edit configuration.yaml

Add the following complete code block to your configuration.yaml file.

YAML

`configuration.yaml`
```
# 1. REST command to send on/off commands to the WLED usermod
rest_command:
  wled_wordclock_set:
    # REPLACE: Enter the IP address of your WLED device here
    url: "http://YOUR_WLED_IP_ADDRESS/json/state"
    method: "POST"
    headers:
      Content-Type: "application/json"
    # Forces 'true'/'false' to be lowercase to generate valid JSON
    payload: '{"WordClockUsermod":{"active":{{ active | string | lower }}}}'

# 2. REST sensor to read the current state of the usermod
rest:
  - resource: "http://YOUR_WLED_IP_ADDRESS/json/state"
    scan_interval: 30
    sensor:
      - name: "WLED Wordclock Status"
        unique_id: wled_wordclock_status_sensor
        value_template: "{{ value_json.WordClockUsermod.active }}"

# 3. Template switch that combines the commands and the sensor into a UI element
template:
  - switch:
      - name: "WLED Word Clock Active"
        unique_id: wled_wordclock_active_switch
        # Action on turning on
        turn_on:
          service: rest_command.wled_wordclock_set
          data:
            active: true
        # Action on turning off
        turn_off:
          service: rest_command.wled_wordclock_set
          data:
            active: false
        # Uses the state of the sensor to display the switch state (on/off)
        state: >
          {{ is_state('sensor.wled_wordclock_status', 'True') }}
        # Changes the icon depending on the state
        icon: >-
          {% if this.state == 'on' %}
            mdi:clock-digital
          {% else %}
            mdi:clock-outline
          {% endif %}
```
Step 2.2: Adjust the IP Address

In the YAML code, replace the placeholder YOUR_WLED_IP_ADDRESS in both places with the actual IP address of your WLED controller.

Step 2.3: Restart Home Assistant

Save the `configuration.yaml` file.

Check the configuration under Settings > System > Restart > Check Configuration.

If the configuration is valid, restart Home Assistant.
