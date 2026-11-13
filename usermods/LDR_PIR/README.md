# LDR_PIR

This usermod uses a Light Dependent Resistor (LDR) and Passive Infrared (PIR) sensor to activate a preset when motion is detected *and* the reading on the LDR is below a configurable threshold. If the LDR reading is above the threshold, then motion from the PIR does not activate any presets. This allows the strip to only be activated by motion when it is dark.

# Installation

Add "LDR_PIR" to your platformio.ini environment's custom_usermods and build.

Example:
```
[env:usermod_LDR_PIR_esp32dev]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods} LDR_PIR
```

# Usermod Settings

Setting | Description | Default
--- | --- | ---
Enabled | Enable/Disable the LDR PIR functionality | False
Ldr Pin | The analog-capable pin to which the LDR is connected | 
Pir Pin | The pin to which the PIR is connected | 
LDR On Threshold | LDR reading, as a decimal between 0 and 1, below which motion from the PIR will activate the On preset | 0.4
LDR Off Threshold | LDR reading, as a decimal between 0 and 1, above which the Off preset will be activated | 0.5
PIR On Preset | The WLED preset to be used when motion is detected | 1
PIR Off Preset | The WLED preset to be used when motion is no longer detected | 10
PIR Off Delay | The amount of time, in seconds, that motion is not detected before activating the Off preset | 60
Home Assistant Discovery | Enable/Disable sending MQTT Discovery entries for sensors to Home Assistant. | False

# MQTT

If MQTT is enabled in the Sync Setup menu, then the LDR and PIR will publish to the following topics:

Sensor | MQTT topic
--- | ---
LDR | `<deviceTopic>/light`
PIR | `<deviceTopic>/motion`

## Author
[@pseud0sphere](https://github.com/pseud0sphere)
