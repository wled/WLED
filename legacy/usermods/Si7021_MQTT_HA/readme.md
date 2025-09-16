# Si7021 to MQTT (with Home Assistant Auto Discovery) usermod

This usermod implements support for [Si7021 I²C temperature and humidity sensors](https://www.silabs.com/documents/public/data-sheets/Si7021-A20.pdf).

As of this writing, the sensor data will *not* be shown on the WLED UI, but it _is_ published via MQTT to WLED's "built-in" MQTT device topic. 

```
temperature: $mqttDeviceTopic/si7021_temperature
humidity: $mqttDeviceTopic/si7021_humidity
```

The following sensors can also be published:

```
heat_index: $mqttDeviceTopic/si7021_heat_index
dew_point: $mqttDeviceTopic/si7021_dew_point
absolute_humidity: $mqttDeviceTopic/si7021_absolute_humidity
```

Sensor data will be updated/sent every 60 seconds.

This usermod also supports Home Assistant Auto Discovery.

## Settings via Usermod Setup

- `enabled`: Enables this usermod
- `Send Dew Point, Abs. Humidity and Heat Index`: Enables additional sensors
- `Home Assistant MQTT Auto-Discovery`: Enables Home Assistant Auto Discovery

# Installation

## Hardware

Attach the Si7021 sensor to the I²C interface.

Default PINs ESP32:

```
SCL_PIN = 22;
SDA_PIN = 21;
```

Default PINs ESP8266:

```
SCL_PIN = 5;
SDA_PIN = 4;
```

## Software

Add to `build_flags` in platformio.ini:

```
   -D USERMOD_SI7021_MQTT_HA
```

Add to `lib_deps` in platformio.ini:

```
   adafruit/Adafruit Si7021 Library @ 1.4.0
   BME280@~3.0.0
```

# Credits

- Aircoookie for making WLED
- Other usermod creators for example code (`sensors_to_mqtt` and `multi_relay` especially)
- You, for reading this
