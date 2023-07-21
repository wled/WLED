# Photoresister sensor with MQTT

Enables attaching a photoresistor sensor like the KY-018 and publishing the readings as a percentage, via MQTT. The frequency of MQTT messages is user definable.
A threshold value can be set so significant changes in the readings are published immediately vice waiting for the next update. This was found to be a good compromise between excessive MQTT traffic and delayed updates.

I also found it useful to limit the frequency of analog pin reads, otherwise the board hangs.

This usermod has only been tested with the KY-018 sensor though it should work for any other analog pin sensor.
Note: this does not control the LED strip directly, it only publishes MQTT readings for use with other integrations like Home Assistant.

## Installation

Copy and replace the file `usermod.cpp` in wled00 directory.
