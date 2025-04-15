# ADC sensor with MQTT ## based on LDR_Dusk_Dawn_v2 usermod from  [@jeffwdh](https://github.com/jeffwdh)  
This usermod will obtain readings from adc pin. This is useful for ldr for example for exterior lighting situations where you want the lights to only be on when it is dark out. but this mod is designed for more hass application. it will publish auto discovery message to hass mqtt and attach the sensor to the wled integration automatically

# Installation
Add "adc_sensor_mqtt" to your platformio.ini environment's custom_usermods and build.

Example:
```
[env:adc_sensor_mqtt_esp32dev]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods} 
  adc_sensor_mqtt   # Enable LDR Dusk Dawn Usermod
```

# Usermod Settings
Setting | Description | Default
--- | --- | ---
Enabled | Enable/Disable the LDR functionality. | Disabled
 Pin | The analog capable pin your LDR is connected to. | A0
 update interval | update interval to read the analog pin ( in ms ) | 3000
 inverted | invert readings based on your own hardware setup ( invert mapping analog to digital value to  0V = 100%  or 3.3V = 100% ) | false
change threshould | the minimum limit to detect change and publish the value (in % ; float ; ex 0.8 ) . | 1.0
