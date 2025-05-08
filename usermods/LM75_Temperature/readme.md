# LM75 temperature sensor with overtemperature protection

This usermod utilizes LM75 style I2C temperature sensors.
It is based on the regular temperature usermod for One Wire sensors.

## Parameters:
| Parameter              | Effect                                                                                                                                                               | Default |
| ---------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------- |
| Temperature Enable     | Activates the LM75 mod                                                                                                                                               | Enabled |
| DegC                   | Switch between °C and °F                                                                                                                                             | °C      |
| Read Interval          | Time between temperature measurements                                                                                                                                | 60 s    |
| Overtemperature Enable | Enables or disables temperature protection. According to the defined limits                                                                                          | Enabled |
| Shutdown Temperature   | Temperature at which the WLED output is switched off. Can be between 1 and 100                                                                                       | 60°C    |
| Reactivate Temperature | Switches the WLED output back on once the temperature drops below set value. Has a hystersis of minimum 1 towards the shutdown temperature. Can be beween 0 and 99°C | 40°C  |

## Compilation:
- modify `platformio.ini` and add wanted `build_flags` to your configuration
<br>


- `-D USERMOD_LM75TEMPERATURE_MEASUREMENT_INTERVAL=60000`
  Parameter time in ms
<br>

- `-D USERMOD_LM75TEMPERATURE_AUTO_OFF_ENABLED=true` 
  Paremeter true or flase
<br>

- `-D USERMOD_LM75TEMPERATURE_AUTO_OFF_HIGH_THRESHOLD=60`
  Parameter 1 to 212 °C
<br>

- `-D USERMOD_LM75TEMPERATURE_AUTO_OFF_LOW_THRESHOLD=40`
   Parameter 0 to 211 °C
<br>

- `-D USERMOD_LM75TEMPERATURE_AUTO_TEMPERATURE_OFF_ENABLED=true`
  Paremeter true or flase
<br>

- `-D USERMOD_LM75TEMPERATURE_I2C_ADRESS=0x48` 
  Parameter I2C Adress of the LM75 IC in Hex 0x**

## Remarks
The °F setting is not optimal to be used in combination with the overtemperature protection feature. WLED only allows uint8 values as inputs. Hence the maxium Temperature at capped at 212 °F or 100 °C. Also be carefull not to set the lower limit below 32 °F to avoid negative values in the inner workings of the mod.

<br>

To avoid flickering of the WLED output the overtemperature feature has a hystersis of minimum 1. It is advised to use a bigger hystersis when setting the limits. Still the mod checks the inputs and adjust the lower value not to equal or higher than the upper limit. 

<br>

Temperature can be published via MQTT, but the feature is as of now untested. 

<br>

During power up the mod tries to scan for the I2C devices under the configured adress. The info page shows an error message if the sensor can not be reached. 

