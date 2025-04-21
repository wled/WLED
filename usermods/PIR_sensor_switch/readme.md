# PIR sensor switch

This usermod-v2 modification allows the connection of a PIR sensor to switch on the LED strip when motion is detected. The switch-off occurs ten minutes after no more motion is detected.

_Story:_

I use the PIR Sensor to automatically turn on the WLED analog clock in my home office room when I am there.
The LED strip is switched [using a relay](https://kno.wled.ge/features/relay-control/) to keep the power consumption low when it is switched off.

## Web interface

The info page in the web interface shows the remaining time of the off timer. Usermod can also be temporarily disbled/enabled from the info page by clicking PIR button.

## Sensor connection

My setup uses an HC-SR501 or HC-SR602 sensor, an HC-SR505 should also work.

The usermod uses GPIO13 (D1 mini pin D7) by default for the sensor signal, but can be changed in the Usermod settings page.
[This example page](http://www.esp8266learning.com/wemos-mini-pir-sensor-example.php) describes how to connect the sensor.

Use the potentiometers on the sensor to set the time delay to the minimum and the sensitivity to about half, or slightly above.
You can also use usermod's off timer instead of sensor's. In such case rotate the potentiometer to its shortest time possible (or use SR602 which lacks such potentiometer).

## Usermod installation

**NOTE:** Usermod has been included in master branch of WLED so it can be compiled in directly just by defining `-D USERMOD_PIRSWITCH` and optionally `-D PIR_SENSOR_PIN=16` to override default pin. You can also change the default off time by adding `-D PIR_SENSOR_OFF_SEC=30`.

## API to enable/disable the PIR sensor from outside. For example from another usermod:

To query or change the PIR sensor state the methods `bool PIRsensorEnabled()` and `void EnablePIRsensor(bool enable)` are available.

When the PIR sensor state changes an MQTT message is broadcasted with topic `wled/deviceMAC/motion` and message `on` or `off`.
Usermod can also be configured to send just the MQTT message but not change WLED state using settings page as well as responding to motion only at night
(assuming NTP and latitude/longitude are set to determine sunrise/sunset times).

### There are two options to get access to the usermod instance:

1. Include `usermod_PIR_sensor_switch.h` **before** you include other usermods in `usermods_list.cpp'

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
    PIRsensorSwitch *PIRsensor = (PIRsensorSwitch::*) UsermodManager::lookup(USERMOD_ID_PIRSWITCH);
    if (PIRsensor != nullptr) {
      PIRsensor->EnablePIRsensor(!PIRsensor->PIRsensorEnabled());
    }
    #endif
  }
  //...
};
```

### Configuration options

Usermod can be configured via the Usermods settings page.

* `PIRenabled` - enable/disable usermod
* `pin` - dynamically change GPIO pin where PIR sensor is attached to ESP
* `PIRoffSec` - number of seconds after PIR sensor deactivates when usermod triggers Off preset (or turns WLED off)
* `on-preset` - preset triggered when PIR activates (if this is 0 it will just turn WLED on)
* `off-preset` - preset triggered when PIR deactivates (if this is 0 it will just turn WLED off)
* `nighttime-only` - enable triggering only between sunset and sunrise (you will need to set up _NTP_, _Lat_ & _Lon_ in Time & Macro settings)
* `mqtt-only` - send only MQTT messages, do not interact with WLED
* `off-only` - only trigger presets or turn WLED on/off if WLED is not already on (displaying effect)
* `notifications` - enable or disable sending notifications to other WLED instances using Sync button
* `HA-discovery` - enable automatic discovery in Home Assistant
* `override` - override PIR input when WLED state is changed using UI
* `domoticz-idx` - Domoticz virtual switch ID (used with MQTT `domoticz/in`)


Have fun - @gegu & @blazoncek

## Change log
2021-04
* Adaptation for runtime configuration.

2021-11
* Added information about dynamic configuration options
* Added option to temporary enable/disable usermod from WLED UI (Info dialog)

2022-11
* Added compile time option for off timer.
* Added Home Assistant autodiscovery MQTT broadcast.
* Updated info on compiling.

2023-??
* Override option
* Domoticz virtual switch ID (used with MQTT `domoticz/in`)

2024-02
* Added compile time option to expand number of PIR sensors (they are logically ORed) `-D PIR_SENSOR_MAX_SENSORS=3`