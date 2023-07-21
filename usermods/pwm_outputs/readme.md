# PWM outputs

v2 Usermod to add generic PWM outputs to WLED. Usermode could be used to control servo motors, LED brightness or any other device controlled by PWM signal.

## Installation

Add the compile-time option `-D USERMOD_PWM_OUTPUTS` to your `platformio.ini` (or `platformio_override.ini`). By default upt to 3 PWM outputs could be configured, to increase that limit add build argument `-D USERMOD_PWM_OUTPUT_PINS=10` (replace 10 by desired amount).

Currently only ESP32 is supported.

## Configuration

By default PWM outputs are disabled, navigate to Usermods settings and configure desired PWM pins and frequencies.

## Usage

If PWM output is configured, it starts to publish its duty cycle value (0-1) both to state JSON and to info JSON (visible in UI info panel). To set PWM duty cycle, use JSON api (over HTTP or over Serial)

```json
{
    "pwm": {
        "0": {"duty": 0.1},
        "1": {"duty": 0.2},
        ...
    }
}
```
