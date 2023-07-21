# Usermod BME280
This Usermod is designed to read a `BME280` or `BMP280` sensor and output the following:
- Temperature
- Humidity (`BME280` only)
- Pressure
- Heat Index (`BME280` only)
- Dew Point (`BME280` only)

Configuration is performed via the Usermod menu.  There are no parameters to set in code!  The following settings can be configured in the Usermod Menu:
- Temperature Decimals (number of decimal places to output)
- Humidity Decimals
- Pressure Decimals
- Temperature Interval (how many seconds between temperature and humidity measurements)
- Pressure Interval
- Publish Always (turn off to only publish changes, on to publish whether or not value changed)
- Use Celsius (turn off to use Fahrenheit)
- Home Assistant Discovery (turn on to sent MQTT Discovery entries for Home Assistant)
- SCL/SDA GPIO Pins

Dependencies
- Libraries
  - `BME280@~3.0.0` (by [finitespace](https://github.com/finitespace/BME280))
  - `Wire`
  - These must be added under `lib_deps` in your `platform.ini` (or `platform_override.ini`).
- Data is published over MQTT - make sure you've enabled the MQTT sync interface.
- This usermod also writes to serial (GPIO1 on ESP8266). Please make sure nothing else is listening to the serial TX pin or your board will get confused by log messages!

In addition to outputting via MQTT, you can read the values from the Info Screen on the dashboard page of the device's web interface.

Methods also exist to read the read/calculated values from other WLED modules through code.
- `getTemperatureC()`
- `getTemperatureF()`
- `getHumidity()`
- `getPressure()`
- `getDewPointC()`
- `getDewPointF()`
- `getHeatIndexC()`
- `getHeatIndexF()`

# Compiling

To enable, compile with `USERMOD_BME280` defined  (e.g. in `platformio_override.ini`)
```ini
[env:usermod_bme280_d1_mini]
extends = env:d1_mini
build_flags =
  ${common.build_flags_esp8266}
  -D USERMOD_BME280
lib_deps = 
  ${esp8266.lib_deps}
  BME280@~3.0.0
  Wire
```


# MQTT
MQTT topics are as follows (`<deviceTopic>` is set in MQTT section of Sync Setup menu):
Measurement type | MQTT topic
--- | ---
Temperature | `<deviceTopic>/temperature`
Humidity | `<deviceTopic>/humidity`
Pressure | `<deviceTopic>/pressure`
Heat index | `<deviceTopic>/heat_index`
Dew point | `<deviceTopic>/dew_point`

If you are using Home Assistant, and `Home Assistant Discovery` is turned on, Home Assistant should automatically detect a new device, provided you have the MQTT integration installed.  The device is separate from the main WLED device and will contain sensors for Pressure, Humidity, Temperature, Dew Point and Heat Index.

# Revision History
Jul 2022
- Added Home Assistant Discovery
- Added API interface to output data
- Removed compile-time variables
- Added usermod menu interface
- Added value outputs to info screen
- Updated `readme.md`
- Registered usermod
- Implemented PinManager for usermod
- Implemented reallocation of pins without reboot

Apr 2021
- Added `Publish Always` option

Dec 2020
- Ported to V2 Usermod format
- Customizable `measure intervals`
- Customizable number of `decimal places` in published sensor values
- Pressure measured in units of hPa instead of Pa
- Calculation of heat index (apparent temperature) and dew point
- `16x oversampling` of sensor during measurement
- Values only published if they are different from the previous value
