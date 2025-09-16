# Wemos D1 mini and Wemos32 mini shield
-   Installation of file: Copy and replace file in wled00 directory
-   For BME280 sensor use usermod_bme280.cpp. Copy to wled00 and rename to usermod.cpp
-   Added third choice of controller Heltec WiFi-Kit-8. Totally DIY but with OLED display.
## Project repository
-   [Original repository](https://github.com/srg74/WLED-wemos-shield) - WLED Wemos shield repository
-   [Wemos shield project Wiki](https://github.com/srg74/WLED-wemos-shield/wiki)
-   [Precompiled WLED firmware](https://github.com/srg74/WLED-wemos-shield/tree/master/resources/Firmware)
## Features
-   SSD1306 128x32 or 128x64 I2C OLED display
-   On screen IP address, SSID and controller status (e.g. ON or OFF, recent effect)
-   Auto display shutoff for extending display lifetime
-   Dallas temperature sensor
-   Reporting temperature to MQTT broker
-   Relay for saving energy

## Hardware
![Shield](https://github.com/srg74/WLED-wemos-shield/blob/master/resources/Images/Assembly_8.jpg)

## Functionality checked with

-   Wemos D1 mini original v3.1 and clones
-   Wemos32 mini
-   PlatformIO
-   SSD1306 128x32 I2C OLED display
-   DS18B20 (temperature sensor)
-   BME280 (temperature, humidity and pressure sensor)
-   Push button (N.O. momentary switch)

### Platformio requirements

For Dallas sensor uncomment `U8g2@~2.27.3`,`DallasTemperature@~3.8.0`,`OneWire@~2.3.5 under` `[common]` section in `platformio.ini`:
```ini
# platformio.ini
...
[platformio]
...
; default_envs = esp07
default_envs = d1_mini
...
[common]
...
lib_deps_external =
  ...
  #For use SSD1306 OLED display uncomment following
  U8g2@~2.27.3
  #For Dallas sensor uncomment following 2 lines
  DallasTemperature@~3.8.0
  OneWire@~2.3.5
...
```

For BME280 sensor uncomment `U8g2@~2.27.3`,`BME280@~3.0.0 under` `[common]` section in `platformio.ini`:
```ini
# platformio.ini
...
[platformio]
...
; default_envs = esp07
default_envs = d1_mini
...
[common]
...
lib_deps_external =
  ...
  #For use SSD1306 OLED display uncomment following
  U8g2@~2.27.3
  #For BME280 sensor uncomment following
  BME280@~3.0.0
...
```
