# bq2589x

The BQ25895 is a highly-integrated 5-A switch-mode battery charge management
and system power path management device for single cell Li-Ion and Li-polymer
battery. The devices support high input voltage fast charging.

Version 1.0


## Installation 

Use this code for your platformio.ini or platformio_override.ini

```ini
[env:esp32bq2589x]
extends = env:esp32c3dev
custom_usermods = usermod_v2_bq2589x # add "custom_usermds" to the extensible environment if it is not there
build_flags = ${common.build_flags_esp32}
              -DUSERMOD_V2_BQ2589X
lib_deps = ${esp32c3.lib_deps}
           memeexpert/bq2589x@^1.0.3
```


## Compilation

You can change default parameters by add build flags like this:

```ini
-D BQ2589X_OTG_PIN=12                    # for change OTG default pin
-D BQ2589X_NCE_PIN=4                     # disable charger
-D BQ2589X_INT_PIN=2                     # input pin for interrupt by bq
-D BQ2589X_SYS_PIN=1                     # input pin for reading sys voltage by ADC of your controller.
-D BQ2589X_DEFAULT_REQUEST_INTERVAL=5000 # changing request interval
-D BQ2589X_DEFAULT_BAT_MAX_CC=2100       # changing max charger current (main charge stage)
-D BQ2589X_MAX_TEMPERATURE=80            # change maximal temperatures
-DWLED_DEBUG                             # for enable debug print

```


## Usage

After compiling and upload your code you can see some params from your battery
by click "info" button in UI.  
You can change battery max/min voltages, request frequency, pins and charger
charger current in usermods config.


## Tips

Be sure to use a capacitor on the OTG line and the host controller power supply
to avoid "blinking" and voltage drops in the host controller supply.


## Documentation

- [Original TI datasheet](https://www.ti.com/lit/ds/symlink/bq25895.pdf).
- [Community comments](https://gist.github.com/somebox/b6cd88731a4b4acb6d2a2f2a00b18c7f)


## Developers

Maintainer: [memeexpert](https://github.com/memExpert).
Write me an email (hancharmaksimq@gmail.com or pottermax2000@gmail.com) if you
found a bug or inaccuracy or would like to jointly improve the code. There is
room for development of this usermod.  

Original bq2589x library author: [Spencer](https://github.com/spencer1979)


## List of possible additions

- Add MQTT support;
- Add more config features (like another battery type, precharge current control and etc);
