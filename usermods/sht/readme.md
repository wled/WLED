# SHT
Usermod to support various SHT i2c sensors like the SHT30, SHT31, SHT35 and SHT85

## Requirements
* "SHT85" by Rob Tillaart, v0.2 or higher: https://github.com/RobTillaart/SHT85

## Usermod installation

Simply copy the below block (build task) to your `platformio_override.ini` and compile WLED using this new build task. Or use an existing one, add the custom_usermod `sht`.

ESP32:
```
[env:custom_esp32dev_usermod_sht]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods} sht
```

ESP8266:
```
[env:custom_d1_mini_usermod_sht]
extends = env:d1_mini
custom_usermods = ${env:d1_mini.custom_usermods} sht
```

## MQTT Discovery for Home Assistant
If you're using Home Assistant and want to have the temperature and humidity available as entities in HA, you can tick the "Add-To-Home-Assistant-MQTT-Discovery" option in the usermod settings. If you have an MQTT broker configured under "Sync Settings" and it is connected, the mod will publish the auto discovery message to your broker and HA will instantly find it and create an entity each for the temperature and humidity.

### Publishing readings via MQTT
Regardless of having MQTT discovery ticked or not, the mod will always report temperature and humidity to the WLED MQTT topic of that instance, if you have a broker configured and it's connected.

## Configuration
Navigate to the "Config" and then to the "Usermods" section. If you compiled WLED with `-D USERMOD_SHT`, you will see the config for it there:
* SHT-Type:
  * What it does: Select the SHT sensor type you want to use
  * Possible values: SHT30, SHT31, SHT35, SHT85
  * Default: SHT30
* Unit:
  * What it does: Select which unit should be used to display the temperature in the info section. Also used when sending via MQTT discovery, see below.
  * Possible values: Celsius, Fahrenheit
  * Default: Celsius
* Add-To-HA-MQTT-Discovery:
  * What it does: Makes the temperature and humidity available via MQTT discovery, so they're automatically added to Home Assistant, because that way it's typesafe.
  * Possible values: Enabled/Disabled
  * Default: Disabled

## Change log
2022-12
* First implementation.

## Credits
ezcGman | Andy: Find me on the Intermit.Tech (QuinLED) Discord server: https://discord.gg/WdbAauG
