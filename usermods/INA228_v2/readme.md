# INA228_v2 Usermod

This usermod integrates the INA228 power monitoring IC with WLED to measure and display:
- **Bus Voltage** (direct 5V supply, or commonly used buck converters up to 85V)
- **Current** (bidirectional through shunt resistor)
- **Power** (calculated)
- **Temperature** (±1°C die temperature)
- **Emergency Shutdown** (automatic LED shutoff on temperature/power limits)
- **PWM Fan Control** (optional temperature-controlled cooling)
- **Piezo Buzzer Alarm** (optional alarm that sounds when an emergency shutdown occurs)

## Hardware

The INA228 is a precision power monitor with a 20-bit ADC. It's available on several breakout boards:

- **Adafruit INA228**: 0.015Ω shunt, 10A max - [Link](https://www.adafruit.com/product/5832)
- **Mateksys I2C-INA-BM**: 0.0002Ω shunt, 204A max - [Link](https://www.mateksys.com/?portfolio=i2c-ina-bm)

## Wiring

Connect the INA228 to your ESP32/ESP8266:
- **VCC** → 3.3V
- **GND** → GND
- **SDA** → GPIO pin configured in WLED (Settings → Usermods → Usermods setup)
- **SCL** → GPIO pin configured in WLED (Settings → Usermods → Usermods setup)
- **A0, A1** → Set I2C address (default: 0x40)
- **ALERT** → Optional GPIO pin for emergency shutdown (active low, needs pullup but a hardware pullup may already exist on some dev boards)

If using an INA228 breakout board, ensure the device is connected in series with your power supply.

**Important**: This usermod uses the global I2C GPIO pins configured in WLED. Set the I2C pins in **Settings** → **Usermods** → **Usermods setup** section.

## Installation

Update your platformio_overide.ini environment with lines to include:

- ```custom_usermods = INA228_v2```
- ```lib_deps = robtillaart/INA228@^0.4.0``` (in addition to other deps)

For serial debug output, add ```-D WLED_DEBUG``` to build_flags.

## Configuration

After installation, configure the usermod in the WLED web interface under **Settings** → **Usermods**:

- **Enabled**: Enable/disable the usermod (default: true)
- **I2C Address**: I2C address in hex (default: 0x40 / 64 decimal)
- **Check Interval**: How often to read sensor in milliseconds (default: 1000)
- **Shunt Resistor**: Value in Ohms - e.g., 0.015 for 15mΩ (default: 0.015)
- **Max Current**: Maximum expected current from the input power supply in Amperes (default: 1.5A)
  - Set above the theoretical max current that could possibly be consumed at input supply voltage. This is not a "limit". It is used for calibration of the INA228 LSB calculations. If actual current exceeds this limit, the INA228 will report a current of 0A.
  - Example: Using the INA228 in series with a 24V buck converter to drive 100 WS2812 LEDs and an ESP32. The LEDs can consume up to 5.5A at 5V. The ESP32 could consume ~500mA at 3.3V. Max system usage would theoretically be 29W which equates to 1.2A from the 24V source. To account for losses and headroom, the current limit should be set to something like 1.5A for best accuracy. If using the INA228 in series with a 5V supply, the current limit should be set much higher for this scenario.
- **Average Samples**: Number of samples to average (default: 4 samples)
  - Options: 1, 4, 16, 64, 128, 256, 512, 1024 samples
  - More samples = more accurate but slower readings

### Fan Control (Optional)

- **Fan Enabled**: Enable temperature-controlled PWM fan (default: false)
- **Fan PWM Pin**: GPIO pin for PWM output to fan (default: -1, disabled)
- **Fan Tacho Pin**: GPIO pin for tachometer input (optional, default: -1)
- **Fan Start Temp**: Temperature (°C) to start fan (default: 30.0)
- **Fan Max Temp**: Temperature (°C) for maximum fan speed (default: 50.0)

The fan speed scales linearly from 0% to 100% between the start and max temperature thresholds. Below the start temp, the fan is off. Above the max temp, the fan runs at full speed.

### Emergency Shutdown (Optional)

- **Shutdown Enabled**: Enable emergency shutdown protection (default: false)
- **Alert Pin**: GPIO pin connected to INA228 ALERT pin (default: -1, software-only mode)
  - The INA228 ALERT pin is active LOW and requires a pullup resistor
  - When configured, provides hardware-level alerting for faster response
  - Set to -1 to use software-only monitoring
- **Temp Limit**: Temperature limit in °C for shutdown (default: 85.0, set to 0 to disable)
  - When temperature reaches this limit, LEDs are turned off immediately
  - Die temperature, not ambient - typically safe up to 85-125°C depending on operating conditions
- **Power Limit**: Power limit in Watts for shutdown (default: 0.0, disabled)
  - When power exceeds this limit, LEDs are turned off immediately
  - Set to 0 to disable power-based shutdown
  - Useful for protecting power supplies from overload

**How it works:**
- When **Alert Pin** is configured: The INA228 hardware asserts the ALERT pin (pulls low) when limits are exceeded, providing instant notification
- Software monitoring also runs every check interval as a fallback
- When triggered, the usermod immediately:
  - Sets LED brightness to 0 (turns off all LEDs)
  - Sets fan to 100% speed (if fan control enabled)
  - Sets shutdown flag in JSON API
  - Logs the event to serial debug
- The shutdown persists until the alert is cleared with the button in the info page, or WLED is restarted.
- Status is visible in the WLED info page and JSON API

## Usage

Once configured, the INA228 readings will appear in the WLED info page:
- **INA228 Current**: Current consumption in Amperes
- **INA228 Bus Voltage**: Supply voltage in Volts
- **INA228 Power**: Calculated power consumption in Watts
- **INA228 Temperature**: Die temperature in °C
- **Fan Speed**: Fan speed percentage (if fan control enabled)
- **Emergency Shutdown**: Status showing "✓ Armed" or "🚨 TRIGGERED" (if shutdown enabled)

Sensor data is also available via the JSON API at `/json/state` under the `INA228` object.

## Notes

- Current is calculated by the INA228 IC based on bus voltage. If using a buck converter or voltage that is different from the operating voltage of your LEDs, the displayed current will be different from the estimated current calculated by WLED which is based on the estimated current consumed by just the LED segments. For best results, you should use the INA228 device on the input power supply (up to 85V). This will ensure that other device's power consumption (MCU, PWM fan, sensors, ...) is included in calculations for total system power draw.
- The temperature reported by this usermod is the die temp of the INA228. If using an INA228 breakout board, the temperature will not reflect the actual temperature of the LEDs, but can still be used to estimate LED temp by coorelation. For LED matricies with an INA228 on the same PCB, the temperature reading is more valuable. The INA228 also supports an external temperature sensor, but that is not implemented in this usermod.
- The INA228 IC also supports battery charge calculation. This is supported by the library developed by robtillaart but not implemented in this user mod. Users that need this feature should feel free to add support with a pull request.
- PWM fan control and piezo buzzers are not standard on INA228 devices and are included in this usermod for convienence. Noctua PWM fans (like the NF-A4x10 5V PWM) are recomended and known to work directly with ESP32 logic levels so no breakout board is needed. Active piezo buzzers are commonly available on breakout boards.
- MQTT support is not included in this version, but could be added later. Feel free to submit a pull request.

## Credits

Depends on:
- [RobTillaart's INA228 library](https://github.com/RobTillaart/INA228)

## License

MIT License 

## Change Log

December 14, 2025

- Initial implementation
- Initial documentation



