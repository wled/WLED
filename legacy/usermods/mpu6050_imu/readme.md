# MPU-6050 Six-Axis (Gyro + Accelerometer) Driver

v2 of this usermod enables connection of a MPU-6050 IMU sensor to
work with effects controlled by the orientation or motion of the WLED Device.

The MPU6050 has a built in "Digital Motion Processor" which does the "heavy lifting"
integrating the gyro and accelerometer measurements to get potentially more
useful gravity vector and orientation output.

It is fairly straightforward to comment out variables being read from the device if they're not needed. Saves CPU/Memory/Bandwidth.

_Story:_

As a memento to a long trip I was on, I built an icosahedron globe. I put lights inside to indicate cities I travelled to.

I wanted to integrate an IMU to allow either on-board, or off-board effects that would
react to the globes orientation. See the blog post on building it <https://www.robopenguins.com/icosahedron-travel-globe/> or a video demo <https://youtu.be/zYjybxHBsHM> .

## Adding Dependencies

I2Cdev and MPU6050 must be installed.

To install them, add electroniccats/MPU6050@1.0.1 to lib_deps in the platformio.ini file.

For example:

```
lib_deps =
    FastLED@3.3.2
    NeoPixelBus@2.5.7
    ESPAsyncTCP@1.2.0
    ESPAsyncUDP@697c75a025
    AsyncTCP@1.0.3
    Esp Async WebServer@1.2.0
    IRremoteESP8266@2.7.3
    electroniccats/MPU6050@1.0.1
```

## Wiring

The connections needed to the MPU6050 are as follows:
```
  VCC     VU (5V USB)   Not available on all boards so use 3.3V if needed.
  GND     G             Ground
  SCL     D1 (GPIO05)   I2C clock
  SDA     D2 (GPIO04)   I2C data
  XDA     not connected
  XCL     not connected
  AD0     not connected
  INT     D8 (GPIO15)   Interrupt pin
```

You could probably modify the code not to need an interrupt, but I used the
setup directly from the example.

## JSON API

This code adds:
```json
"u":{
  "IMU":{
    "Quat":        [w, x, y, z],
    "Euler":       [psi, theta, phi],
    "Gyro":        [x, y, z],
    "Accel":       [x, y, z],
    "RealAccel":   [x, y, z],
    "WorldAccel":  [x, y, z],
    "Gravity":     [x, y, z],
    "Orientation": [yaw, pitch, roll]
  }
}
```
to the info object

## Usermod installation

1. Copy the file `usermod_mpu6050_imu.h` to the `wled00` directory.
2. Register the usermod by adding `#include "usermod_mpu6050_imu.h"` in the top and `registerUsermod(new MPU6050Driver());` in the bottom of `usermods_list.cpp`.

Example **usermods_list.cpp**:

```cpp
#include "wled.h"

#include "usermod_mpu6050_imu.h"

void registerUsermods()
{
  UsermodManager::add(new MPU6050Driver());
}
```
