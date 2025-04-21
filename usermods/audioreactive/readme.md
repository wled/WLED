# Audioreactive usermod

Enables controlling LEDs via audio input. Audio source can be a microphone or analog-in (AUX) using an appropriate adapter.
Supported microphones range from analog (MAX4466, MAX9814, ...) to digital (INMP441, ICS-43434, ...).

Does audio processing and provides data structure that specially written effects can use.

**does not** provide effects or draw anything to an LED strip/matrix.

## Additional Documentation
This usermod is an evolution of [SR-WLED](https://github.com/atuline/WLED), and a lot of documentation and information can be found in the [SR-WLED wiki](https://github.com/atuline/WLED/wiki):
* [getting started with audio](https://github.com/atuline/WLED/wiki/First-Time-Setup#sound)
* [Sound settings](https://github.com/atuline/WLED/wiki/Sound-Settings) - similar to options on the usemod settings page in WLED.
* [Digital Audio](https://github.com/atuline/WLED/wiki/Digital-Microphone-Hookup)
* [Analog Audio](https://github.com/atuline/WLED/wiki/Analog-Audio-Input-Options)
* [UDP Sound sync](https://github.com/atuline/WLED/wiki/UDP-Sound-Sync)


## Supported MCUs
This audioreactive usermod works best on "classic ESP32" (dual core), and on ESP32-S3 which also has dual core and hardware floating point support. 

It will compile successfully for ESP32-S2 and ESP32-C3, however might not work well, as other WLED functions will become slow. Audio processing requires a lot of computing power, which can be problematic on smaller MCUs like -S2 and -C3. 

Analog audio is only possible on "classic" ESP32, but not on other MCUs like ESP32-S3.

Currently ESP8266 is not supported, due to low speed and small RAM of this chip. 
There are however plans to create a lightweight audioreactive for the 8266, with reduced features.
## Installation 

Add 'ADS1115_v2' to `custom_usermods` in your platformio environment.

## Configuration

All parameters are runtime configurable. Some may require a hard reset after changing them (I2S microphone or selected GPIOs).

If you want to define default GPIOs during compile time, use the following (default values in parentheses):

- `-D SR_DMTYPE=x` : defines digital microphone type: 0=analog, 1=generic I2S (default), 2=ES7243 I2S, 3=SPH0645 I2S, 4=generic I2S with master clock, 5=PDM I2S
- `-D AUDIOPIN=x`  : GPIO for analog microphone/AUX-in (36)
- `-D I2S_SDPIN=x` : GPIO for SD pin on digital microphone (32)
- `-D I2S_WSPIN=x` : GPIO for WS pin on digital microphone (15)
- `-D I2S_CKPIN=x` : GPIO for SCK pin on digital microphone (14)
- `-D MCLK_PIN=x`  : GPIO for master clock pin on digital Line-In boards (-1)
- `-D ES7243_SDAPIN` : GPIO for I2C SDA pin on ES7243 microphone (-1)
- `-D ES7243_SCLPIN` : GPIO for I2C SCL pin on ES7243 microphone (-1)

Other options:

- `-D UM_AUDIOREACTIVE_ENABLE` : makes usermod default enabled (not the same as include into build option!)
- `-D UM_AUDIOREACTIVE_DYNAMICS_LIMITER_OFF` : disables rise/fall limiter default

**NOTE** I2S is used for analog audio sampling. Hence, the analog *buttons* (i.e. potentiometers) are disabled when running this usermod with an analog microphone.

### Advanced Compile-Time Options
You can use the following additional flags in your `build_flags`
* `-D SR_SQUELCH=x`  : Default "squelch" setting (10)
* `-D SR_GAIN=x`     : Default "gain" setting (60)
* `-D I2S_USE_RIGHT_CHANNEL`: Use RIGHT instead of LEFT channel (not recommended unless you strictly need this).
* `-D I2S_USE_16BIT_SAMPLES`: Use 16bit instead of 32bit for internal sample buffers. Reduces sampling quality, but frees some RAM ressources (not recommended unless you absolutely need this).
* `-D I2S_GRAB_ADC1_COMPLETELY`: Experimental: continuously sample analog ADC microphone. Only effective on ESP32. WARNING this _will_ cause conflicts(lock-up) with any analogRead() call.
* `-D MIC_LOGGER`     : (debugging) Logs samples from the microphone to serial USB. Use with serial plotter (Arduino IDE)
* `-D SR_DEBUG`       : (debugging) Additional error diagnostics and debug info on serial USB.

## Release notes

* 2022-06 Ported from [soundreactive WLED](https://github.com/atuline/WLED) - by @blazoncek (AKA Blaz Kristan) and the [SR-WLED team](https://github.com/atuline/WLED/wiki#sound-reactive-wled-fork-team).
* 2022-11 Updated to align with "[MoonModules/WLED](https://amg.wled.me)" audioreactive usermod - by @softhack007 (AKA Frank M&ouml;hle).
