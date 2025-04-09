# Photoresister sensor with MQTT ## based on LDR_Dusk_Dawn_v2 usermod from  [@jeffwdh](https://github.com/jeffwdh)  
This usermod will obtain readings from a Light Dependent Resistor (LDR). This is useful for exterior lighting situations where you want the lights to only be on when it is dark out. but this mod is designed for more hass application. it will publish auto discovery message to hass mqtt and attach the light sensor to the wled integration automatically

# Installation
Add "photoresistor_sensor_mqtt_v2" to your platformio.ini environment's custom_usermods and build.

Example:
```
[env:usermod_LDR_Dusk_Dawn_esp32dev]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods} 
  photoresistor_sensor_mqtt_v2   # Enable LDR Dusk Dawn Usermod
```

# Usermod Settings
Setting | Description | Default
--- | --- | ---
Enabled | Enable/Disable the LDR functionality. | Disabled
LDR Pin | The analog capable pin your LDR is connected to. | A0
LDR update interval | update interval to read the analog pin ( in ms ) | 3000
LDR inverted | invert readings based on your own hardware setup ( invert mapping analog to digital value to  0V = 100%  or 3.3V = 100% ) | false
change threshould | the minimum limit to detect change and publish the value (in % ; float ; ex 0.8 ) . | 1.0

## Author
[@jeffrommo911wdh](https://github.com/rommo911)  
based on :
[@jeffwdh](https://github.com/jeffwdh)  
jeffwdh@tarball.ca

Enables attaching a photoresistor sensor like the KY-018 and publishing the readings as a percentage, via MQTT. The frequency of MQTT messages is user definable.
A threshold value can be set so significant changes in the readings are published immediately vice waiting for the next update. This was found to be a good compromise between excessive MQTT traffic and delayed updates.

I also found it useful to limit the frequency of analog pin reads, otherwise the board hangs.

This usermod has only been tested with the KY-018 sensor though it should work for any other analog pin sensor.
Note: this does not control the LED strip directly, it only publishes MQTT readings for use with other integrations like Home Assistant.

