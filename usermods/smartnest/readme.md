# Smartnest

Enables integration with `smartnest.cz` service which provides MQTT integration with voice assistants, for example Google Home, Alexa, Siri, Home Assistant and more!

In order to setup Smartnest follow the [documentation](https://www.docu.smartnest.cz/).
 - You can create up to 5 different devices
 - To add the project to Google Home you can find the information [here](https://www.docu.smartnest.cz/google-home-integration)
 - To add the project to Alexa you can find the information [here](https://www.docu.smartnest.cz/alexa-integration)

## MQTT API

The API is described in the Smartnest [Github repo](https://github.com/aososam/Smartnest/blob/master/Devices/lightRgb/lightRgb.ino).

## Usermod installation

1. Use `#define USERMOD_SMARTNEST` in wled.h or `-D USERMOD_SMARTNEST` in your platformio.ini (recommended).

## Configuration

Usermod has no configuration, but it relies on the MQTT configuration.\
Under Config > Sync Interfaces > MQTT:

* Enable `MQTT` check box.
* Set the `Broker` field to: `smartnest.cz` or `3.122.209.170`(both work).
* Set the `Port` field to: `1883`
* The `Username` and `Password` fields are the login information from the `smartnest.cz` website (It is located above in the 3 points).
* `Client ID` field is obtained from the device configuration panel in `smartnest.cz`.
* `Device Topic` is obtained by entering the ClientID/report , remember to replace ClientId with your real information (Because they can ban your device).
* `Group Topic` keep the same Group Topic.

Wait `1 minute` after turning it on, as it usually takes a while.  

## Change log

2022-09
 * First implementation.
  
2024-05
 * Solved code.
 * Updated documentation.
 * Second implementation.
