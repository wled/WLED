# Send sensor data To Home Assistant

Publishes BMP280, CCS811 and Si7021 measurements to Home Assistant via MQTT.

Uses Home Assistant Automatic Device Discovery.

The use of Home Assistant is not mandatory. The mod will publish sensor values via MQTT just fine without it.

Uses the MQTT connection set in the WLED web user interface.

## Maintainer

twitter.com/mpronk89

## Features

- Reads BMP280, CCS811 and Si7021 senors
- Publishes via MQTT, configured via WLED webUI
- Announces device in Home Assistant for easy setup
- Efficient energy usage
- Updates every 60 seconds

## Example MQTT topics:

`$mqttDeviceTopic` is set in webui of WLED!

```
temperature: $mqttDeviceTopic/temperature
pressure: $mqttDeviceTopic/pressure
humidity: $mqttDeviceTopic/humidity
tvoc: $mqttDeviceTopic/tvoc
eCO2:  $mqttDeviceTopic/eco2
IAQ:  $mqttDeviceTopic/iaq
```

# Installation

## Hardware

### Requirements

1. BMP280/CCS811/Si7021 sensor. E.g. https://aliexpress.com/item/32979998543.html
2. A microcontroller that supports i2c. e.g. esp32

### installation

Attach the sensor to the i2c interface.

Default PINs esp32:

```
SCL_PIN = 22;
SDA_PIN = 21;
```

Default PINs ESP8266:

```
SCL_PIN = 5;
SDA_PIN = 4;
```

# Credits

- Aircoookie for making WLED
- Other usermod creators for example code
- Bouke_Regnerus for https://community.home-assistant.io/t/example-indoor-air-quality-text-sensor-using-ccs811-sensor/125854
- You, for reading this
