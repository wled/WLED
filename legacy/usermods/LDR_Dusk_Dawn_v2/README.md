# LDR_Dusk_Dawn_v2
This usermod will obtain readings from a Light Dependent Resistor (LDR) and will turn on/off specific presets based on those readings. This is useful for exterior lighting situations where you want the lights to only be on when it is dark out.

# Installation
Add "-D USERMOD_LDR_DUSK_DAWN" to your platformio.ini [common] build_flags and build.

Example:
```
[common]
build_flags =
  -D USERMOD_LDR_DUSK_DAWN   # Enable LDR Dusk Dawn Usermod
```

# Usermod Settings
Setting | Description | Default
--- | --- | ---
Enabled | Enable/Disable the LDR functionality. | Disabled
LDR Pin | The analog capable pin your LDR is connected to. | 34
Threshold Minutes | The number of minutes of consistent readings above/below the on/off threshold before the LED state will change. | 5
Threshold | The analog read value threshold from the LDR. Readings lower than this number will count towards changing the LED state to off. You can see the current LDR reading by going into the info section when LDR functionality is enabled. | 1000
On Preset | The WLED preset to be used for the LED on state. | 1
Off Preset | The WLED preset to be used for the LED off state. | 2

## Author
[@jeffwdh](https://github.com/jeffwdh)  
jeffwdh@tarball.ca
