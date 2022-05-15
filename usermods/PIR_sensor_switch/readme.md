# PIR sensor switch

This usermod-v2 modification allows the connection of a PIR sensor to switch on the LED strip when motion is detected. The switch-off occurs ten minutes after no more motion is detected.

_Story:_

I use the PIR Sensor to automatically turn on the WLED analog clock in my home office room when I am there.
The LED strip is switched [using a relay](https://github.com/Aircoookie/WLED/wiki/Control-a-relay-with-WLED) to keep the power consumption low when it is switched off.

## Webinterface

The info page in the web interface shows the remaining time of the off timer. Usermod can also be temporarily disbled/enabled from the info page by clicking PIR button.

## Sensor connection

My setup uses an HC-SR501 or HC-SR602 sensor, a HC-SR505 should also work.

The usermod uses GPIO13 (D1 mini pin D7) by default for the sensor signal but can be changed in the Usermod settings page.
[This example page](http://www.esp8266learning.com/wemos-mini-pir-sensor-example.php) describes how to connect the sensor.

Use the potentiometers on the sensor to set the time-delay to the minimum and the sensitivity to about half, or slightly above.
You can also use usermod's off timer instead of sensor's. In such case rotate the potentiometer to its shortest time possible (or use SR602 which lacks such potentiometer).

## Usermod installation

1. Copy the file `usermod_PIR_sensor_switch.h` to the `wled00` directory.
2. Register the usermod by adding `#include "usermod_PIR_sensor_switch.h"` in the top and `registerUsermod(new PIRsensorSwitch());` in the bottom of `usermods_list.cpp`.

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
//#include "usermod_v2_empty.h"
#include "usermod_PIR_sensor_switch.h"

void registerUsermods()
{
  /*
   * Add your usermod class name here
   * || || ||
   * \/ \/ \/
   */
  //usermods.add(new MyExampleUsermod());
  //usermods.add(new UsermodTemperature());
  //usermods.add(new UsermodRenameMe());
  usermods.add(new PIRsensorSwitch());

}
```

**NOTE:** Usermod has been included in master branch of WLED so it can be compiled in directly just by defining `-D USERMOD_PIRSWITCH` and optionaly `-D PIR_SENSOR_PIN=16` to override default pin.

## API to enable/disable the PIR sensor from outside. For example from another usermod.

To query or change the PIR sensor state the methods `bool PIRsensorEnabled()` and `void EnablePIRsensor(bool enable)` are available.

When the PIR sensor state changes an MQTT message is broadcasted with topic `wled/deviceMAC/motion` and message `on` or `off`.
Usermod can also be configured to just send MQTT message and not change WLED state using settings page as well as responding to motion only during nighttime (assuming NTP and lattitude/longitude are set to determine sunrise/sunset times).

### There are two options to get access to the usermod instance:

1. Include `usermod_PIR_sensor_switch.h` **before** you include the other usermod in `usermods_list.cpp'

or

2. Use `#include "usermod_PIR_sensor_switch.h"` at the top of the `usermod.h` where you need it.

**Example usermod.h :**
```cpp
#include "wled.h"

#include "usermod_PIR_sensor_switch.h"

class MyUsermod : public Usermod {
  //...

  void togglePIRSensor() {
    #ifdef USERMOD_PIR_SENSOR_SWITCH
    PIRsensorSwitch *PIRsensor = (PIRsensorSwitch::*) usermods.lookup(USERMOD_ID_PIRSWITCH);
    if (PIRsensor != nullptr) {
      PIRsensor->EnablePIRsensor(!PIRsensor->PIRsensorEnabled());
    }
    #endif
  }
  //...
};
```

### Configuration options

Usermod can be configured in Usermods settings page.

* `PIRenabled` - enable/disable usermod
* `pin` - dynamically change GPIO pin where PIR sensor is attached to ESP
* `PIRoffSec` - number of seconds after PIR sensor deactivates when usermod triggers Off preset (or turns WLED off)
* `on-preset` - preset triggered when PIR activates (if this is 0 it will just turn WLED on)
* `off-preset` - preset triggered when PIR deactivates (if this is 0 it will just turn WLED off)
* `nighttime-only` - enable triggering only between sunset and sunrise (you will need to set up _NTP_, _Lat_ & _Lon_ in Time & Macro settings)
* `mqtt-only` - only send MQTT messages, do not interact with WLED
* `off-only` - only trigger presets or turn WLED on/off in WLED is not already on (displaying effect)
* `notifications` - enable or disable sending notifications to other WLED instances using Sync button


Have fun - @gegu & @blazoncek

## Change log
2021-04
* Adaptation for runtime configuration.

2021-11
* Added information about dynamic configuration options
* Added option to temporary enable/disble usermod from WLED UI (Info dialog)