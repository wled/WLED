# DEPRECATION NOTICE
This usermod is deprecated and no longer maintained. It will be removed in a future WLED release. Please use usermod multi_relay which has more features.


# MQTT controllable switches
This usermod allows controlling switches (e.g. relays) via MQTT.

## Usermod installation

1. Copy the file `usermod_mqtt_switch.h` to the `wled00` directory.
2. Register the usermod by adding `#include "usermod_mqtt_switch.h"` in the top and `registerUsermod(new UsermodMqttSwitch());` in the bottom of `usermods_list.cpp`.


Example `usermods_list.cpp`:

```
#include "wled.h"
#include "usermod_mqtt_switch.h"

void registerUsermods()
{
  usermods.add(new UsermodMqttSwitch());
}
```

## Define pins
Add a define for MQTTSWITCHPINS to platformio_override.ini.
The following example defines 3 switches connected to the GPIO pins 13, 5 and 2:

```
[env:livingroom]
board = esp12e
platform = ${common.platform_wled_default}
board_build.ldscript = ${common.ldscript_4m1m}
build_flags = ${common.build_flags_esp8266} 
   -D LEDPIN=3
   -D BTNPIN=4
   -D RLYPIN=12
   -D RLYMDE=1
   -D STATUSPIN=15
   -D MQTTSWITCHPINS="13, 5, 2"
```

Pins can be inverted by setting `MQTTSWITCHINVERT`. For example `-D MQTTSWITCHINVERT="false, false, true"` would invert the switch on pin 2 in the previous example.

The default state after booting before any MQTT message can be set by `MQTTSWITCHDEFAULTS`. For example `-D MQTTSWITCHDEFAULTS="ON, OFF, OFF"` would power on the switch on pin 13 and power off switches on pins 5 and 2.
   
## MQTT topics
This usermod listens on `[mqttDeviceTopic]/switch/0/set` (where 0 is replaced with the index of the switch) for commands. Anything starting with `ON` turns on the switch, everything else turns it off.
Feedback about the current state is provided at `[mqttDeviceTopic]/switch/0/state`.

### Home Assistant auto-discovery
Auto-discovery information is automatically published and you shoudn't have to do anything to register the switches in Home Assistant.
 
