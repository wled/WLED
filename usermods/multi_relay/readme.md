# Multi Relay

This usermod-v2 modification allows the connection of multiple relays each with individual delay and on/off mode.

## HTTP API
All responses are returned as JSON. 

* Status Request: `http://[device-ip]/relays`
* Switch Command: `http://[device-ip]/relays?switch=1,0,1,1`

The number of numbers behind the switch parameter must correspond to the number of relays. The number 1 switches the relay on. The number 0 switches the relay off. 

* Toggle Command: `http://[device-ip]/relays?toggle=1,0,1,1`

The number of numbers behind the parameter switch must correspond to the number of relays. The number 1 causes a toggling of the relay. The number 0 leaves the state of the device.

Examples
1. total of 4 relays, relay 2 will be toggled: `http://[device-ip]/relays?toggle=0,1,0,0`
2. total of 3 relays, relay 1&3 will be switched on: `http://[device-ip]/relays?switch=1,0,1`

## JSON API
You can switch relay state using the following JSON object transmitted to: `http://[device-ip]/json`


Switch relay 0 on: `{"MultiRelay":{"relay":0,"on":true}}`

Switch relay4 3 & 4 off: `{"MultiRelay":[{"relay":2,"on":false},{"relay":3,"on":false}]}`

## MQTT API

* `wled`/_deviceMAC_/`relay`/`0`/`command` `on`|`off`|`toggle`
* `wled`/_deviceMAC_/`relay`/`1`/`command` `on`|`off`|`toggle`

When relay is switched it will publish a message:

* `wled`/_deviceMAC_/`relay`/`0` `on`|`off`


## Usermod installation

1. Register the usermod by adding `#include "../usermods/multi_relay/usermod_multi_relay.h"` at the top and `usermods.add(new MultiRelay());` at the bottom of `usermods_list.cpp`.
or
2. Use `#define USERMOD_MULTI_RELAY` in wled.h or `-D USERMOD_MULTI_RELAY` in your platformio.ini

You can override the default maximum number (4) of relays by defining MULTI_RELAY_MAX_RELAYS.

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

Usermod can be configured in Usermods settings page.

* `enabled` - enable/disable usermod
* `pin` - GPIO pin where relay is attached to ESP (can be configured at compile time `-D MULTI_RELAY_PINS=xx,xx,...`)
* `delay-s` - delay in seconds after on/off command is received
* `active-high` - toggle high/low activation of relay (can be used to reverse relay states)
* `external` - if enabled WLED does not control relay, it can only be triggered by external command (MQTT, HTTP, JSON or button)
* `button` - button (from LED Settings) that controls this relay
* `broadcast`- time in seconds between state broadcasts using MQTT
* `HA-discovery`- enable Home Assistant auto discovery

If there is no MultiRelay section, just save current configuration and re-open Usermods settings page. 

Have fun - @blazoncek

## Change log
2021-04
* First implementation.

2021-11
* Added information about dynamic configuration options
* Added button support.