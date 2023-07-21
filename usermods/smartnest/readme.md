# Smartnest

Enables integration with `smartnest.cz` service which provides MQTT integration with voice assistants.
In order to setup Smartnest follow the [documentation](https://www.docu.smartnest.cz/).

## MQTT API

The API is described in the Smartnest [Github repo](https://github.com/aososam/Smartnest/blob/master/Devices/lightRgb/lightRgb.ino).


## Usermod installation

1. Register the usermod by adding `#include "../usermods/smartnest/usermod_smartnest.h"` at the top and `usermods.add(new Smartnest());` at the bottom of `usermods_list.cpp`.
or
2. Use `#define USERMOD_SMARTNEST` in wled.h or `-D USERMOD_SMARTNEST` in your platformio.ini


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
#include "../usermods/usermod_smartnest.h"

void registerUsermods()
{
  /*
   * Add your usermod class name here
   * || || ||
   * \/ \/ \/
   */
  //usermods.add(new MyExampleUsermod());
  //usermods.add(new UsermodTemperature());
  usermods.add(new Smartnest());

}
```

## Configuration

Usermod has no configuration, but it relies on the MQTT configuration.\
Under Config > Sync Interfaces > MQTT:
* Enable MQTT check box
* Set the `Broker` field to: `smartnest.cz`
* The `Username` and `Password` fields are the login information from the `smartnest.cz` website.
* `Client ID` field is obtained from the device configuration panel in `smartnest.cz`.

## Change log
2022-09
* First implementation.
