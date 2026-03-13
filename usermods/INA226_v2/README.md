# Usermod INA226

This Usermod is designed to read values from an INA226 sensor and output the following:
- Current
- Voltage
- Power
- Shunt Voltage
- Overflow status

## Configuration

The following settings can be configured in the Usermod Menu:
- **Enabled**: Enable or disable the usermod.
- **I2CAddress**: The I2C address in decimal. Default is 64 (0x40).
- **CheckInterval**: Number of seconds between readings. This should be higher than the time it takes to make a reading, determined by the two next options.
- **INASamples**: The number of samples to configure the INA226 to use for a measurement. Higher counts provide more accuracy. See the 'Understanding Samples and Conversion Times' section for more details.
- **INAConversionTime**: The time to use on converting and preparing readings on the INA226. Higher times provide more precision. See the 'Understanding Samples and Conversion Times' section for more details.
- **Decimals**: Number of decimals in the output.
- **ShuntResistor**: Shunt resistor value in milliohms. An R100 shunt resistor should be written as "100", while R010 should be "10".
- **CurrentRange**: Expected maximum current in milliamps (e.g., 5 A = 5000 mA).
- **MqttPublish**: Enable or disable MQTT publishing.
- **MqttPublishAlways**: Publish always, regardless if there is a change.
- **MqttHomeAssistantDiscovery**: Enable Home Assistant discovery.


## Understanding Samples and Conversion Times

The INA226 uses a programmable ADC with configurable conversion times and averaging to optimize the measurement accuracy and speed. The conversion time and number of samples are determined based on the `INASamples` and `INAConversionTime` settings. The following table outlines the possible combinations:

| Conversion Time (μs) | 1 Sample | 4 Samples | 16 Samples | 64 Samples | 128 Samples | 256 Samples | 512 Samples | 1024 Samples |
|----------------------|----------|-----------|------------|------------|-------------|-------------|-------------|--------------|
| 140                  | 0.28 ms  | 1.12 ms   | 4.48 ms    | 17.92 ms   | 35.84 ms    | 71.68 ms    | 143.36 ms   | 286.72 ms    |
| 204                  | 0.408 ms | 1.632 ms  | 6.528 ms   | 26.112 ms  | 52.224 ms   | 104.448 ms  | 208.896 ms  | 417.792 ms   |
| 332                  | 0.664 ms | 2.656 ms  | 10.624 ms  | 42.496 ms  | 84.992 ms   | 169.984 ms  | 339.968 ms  | 679.936 ms   |
| 588                  | 1.176 ms | 4.704 ms  | 18.816 ms  | 75.264 ms  | 150.528 ms  | 301.056 ms  | 602.112 ms  | 1204.224 ms  |
| 1100                 | 2.2 ms   | 8.8 ms    | 35.2 ms    | 140.8 ms   | 281.6 ms    | 563.2 ms    | 1126.4 ms   | 2252.8 ms    |
| 2116                 | 4.232 ms | 16.928 ms | 67.712 ms  | 270.848 ms | 541.696 ms  | 1083.392 ms | 2166.784 ms | 4333.568 ms  |
| 4156                 | 8.312 ms | 33.248 ms | 132.992 ms | 531.968 ms | 1063.936 ms | 2127.872 ms | 4255.744 ms | 8511.488 ms  |
| 8244                 | 16.488 ms| 65.952 ms | 263.808 ms | 1055.232 ms| 2110.464 ms | 4220.928 ms | 8441.856 ms | 16883.712 ms |

It is important to pick a combination that provides the needed balance between accuracy and precision while ensuring new readings within the `CheckInterval` setting. When `USERMOD_INA226_DEBUG` is defined, the info pane contains the expected time to make a reading, which can be seen in the table above.

As an example, if you want a new reading every 5 seconds (`CheckInterval`), a valid combination is `256 samples` and `4156 μs` which would provide new values every 2.1 seconds.

The picked values also slightly affect power usage. If the `CheckInterval` is set to more than 20 seconds, the INA226 is configured in `triggered` reading mode, where it only uses power as long as it's working. Then the conversion time and average samples counts determine how long the chip stays turned on every `CheckInterval` time.

### Calculating Current and Power

The INA226 calculates current by measuring the differential voltage across a shunt resistor and using the calibration register value to convert this measurement into current. Power is calculated by multiplying the current by the bus voltage.

For detailed programming information and register configurations, refer to the [INA226 datasheet](https://www.ti.com/product/INA226).

## Author
[@LordMike](https://github.com/LordMike)

## Compiling

To enable, compile with `INA226` in `custom_usermods` (e.g. in `platformio_override.ini`).

```ini
[env:ina226_example]
extends = env:esp32dev
custom_usermods = ${env:esp32dev.custom_usermods} INA226
build_flags = ${env:esp32dev.build_flags}
  ; -D USERMOD_INA226_DEBUG ; -- add a debug status to the info modal
```