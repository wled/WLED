# Multi Relay

This usermod-v2 modification allows the connection of multiple relays, each with individual delay and on/off mode.
Usermod supports PCF8574 I2C port expander to reduce GPIO use.
PCF8574 supports 8 outputs and each output corresponds to a relay in WLED (relay 0 = port 0, etc). I you are using more than 8 relays with multiple PCF8574 make sure their addresses are set in sequence (e.g. 0x20 and 0x21). You can set address of first expander in settings.
(**NOTE:** Will require Wire library and global I2C pins defined.)

## HTTP API
All responses are returned in JSON format. 

* Status Request: `http://[device-ip]/relays`
* Switch Command: `http://[device-ip]/relays?switch=1,0,1,1`

The number of values behind the switch parameter must correspond to the number of relays. The value 1 switches the relay on, 0 switches it off. 

* Toggle Command: `http://[device-ip]/relays?toggle=1,0,1,1`

The number of values behind the parameter switch must correspond to the number of relays. The value 1 causes the relay to toggle, 0 leaves its state unchanged.

Examples:
1. total of 4 relays, relay 2 will be toggled: `http://[device-ip]/relays?toggle=0,1,0,0`
2. total of 3 relays, relay 1&3 will be switched on: `http://[device-ip]/relays?switch=1,0,1`

## JSON API
You can toggle the relay state by sending the following JSON object to: `http://[device-ip]/json`

Switch relay 0 on: `{"MultiRelay":{"relay":0,"on":true}}`

Switch relay 3 and 4 off: `{"MultiRelay":[{"relay":2,"on":false},{"relay":3,"on":false}]}`


## MQTT API

* `wled`/_deviceMAC_/`relay`/`0`/`command` `on`|`off`|`toggle`
* `wled`/_deviceMAC_/`relay`/`1`/`command` `on`|`off`|`toggle`

When a relay is switched, a message is published:

* `wled`/_deviceMAC_/`relay`/`0` `on`|`off`


## Usermod installation

1. Register the usermod by adding `#include "../usermods/multi_relay/usermod_multi_relay.h"` at the top and `usermods.add(new MultiRelay());` at the bottom of `usermods_list.cpp`.
or
2. Use `#define USERMOD_MULTI_RELAY` in wled.h or `-D USERMOD_MULTI_RELAY` in your platformio.ini

You can override the default maximum number of relays (which is 4) by defining MULTI_RELAY_MAX_RELAYS.

Example **usermods_list.cpp**:

```cpp
#include "wled.h"
/*
 * Register your v2 usermods here!
 *   (for v1 usermods using just usermod.cpp, you can ignore this file)
 */

/*
 * Add/uncomment your usermod filename here (and once more below)
 * || || ||
 * \/ \/ \/
 */
//#include "usermod_v2_example.h"
//#include "usermod_temperature.h"
#include "../usermods/usermod_multi_relay.h"

void registerUsermods()
{
  /*
   * Add your usermod class name here
   * || || ||
   * \/ \/ \/
   */
  //usermods.add(new MyExampleUsermod());
  //usermods.add(new UsermodTemperature());
  usermods.add(new MultiRelay());

}
```

## Configuration

Usermod can be configured via the Usermods settings page.

* `enabled` - enable/disable usermod
* `use-PCF8574` - use PCF8574 port expander instead of GPIO pins
* `first-PCF8574` - I2C address of first expander (WARNING: enter *decimal* value)
* `broadcast`- time in seconds between MQTT relay-state broadcasts
* `HA-discovery`- enable Home Assistant auto discovery
* `pin` - ESP GPIO pin the relay is connected to (can be configured at compile time `-D MULTI_RELAY_PINS=xx,xx,...`)
* `delay-s` - delay in seconds after on/off command is received
* `active-high` - assign high/low activation of relay (can be used to reverse relay states)
* `external` - if enabled, WLED does not control relay, it can only be triggered by an external command (MQTT, HTTP, JSON or button)
* `button` - button (from LED Settings) that controls this relay

If there is no MultiRelay section, just save current configuration and re-open Usermods settings page. 

Have fun - @blazoncek

## Change log
2021-04
* First implementation.

2021-11
* Added information about dynamic configuration options
* Added button support.

2023-05
* Added support for PCF8574 I2C port expander (multiple)