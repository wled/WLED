# ADC sensor with MQTT & HASS support ## 
This usermod will obtain readings from adc pin. This is useful for ldr for example for exterior lighting situations where you want the lights to only be on when it is dark out. but this mod is designed for more hass application. it will publish auto discovery message to hass mqtt and attach the sensor to the wled integration automatically

# Installation
Add "adc_sensor_mqtt" to your platformio.ini environment's custom_usermods and build.
Example:
```
[env:adc_sensor_mqtt_esp32dev]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods} 
  adc_sensor_mqtt   # Enable  ADC sensor with MQTT
```

# Usermod Settings
Setting | Description | Default
--- | --- | ---
Enabled | Enable/Disable the LDR functionality. | Disabled
Pin | The analog capable pin your LDR is connected to. | A0 is forced in esp8266 and its not optional
AdcUpdateInterval | update interval to read the analog pin ( in ms ) | 3000
Inverted | invert readings based on your own hardware setup ( invert mapping analog to digital value to  0V = 100%  or 3.3V = 100% ) | false
ChangeThreshold | the minimum limit to detect change and publish the value ; checks the raw value ( int ;  5 ) . | 1
HASS | Enable home assistant mqtt discovery message with wled integration | true 
Raw | publish the ADC value as raw measurement (0-4096 for ESP32 / 0-1024 for ESP8266) | false
DeviceClass | HASS sensor discovery device class | voltage 
UnitOfMeas | HASS sensor discovery device unit of measurements | V  

## Author
[@rommo911] (https://github.com/rommo911)  
