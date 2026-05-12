# MQTT Animated Staircase Usermod

This usermod is a highly advanced, MQTT-enabled version of the standard WLED Animated Staircase. It allows you to trigger stunning, sequential staircase LED animations using physical motion sensors (PIR/Ultrasonic) OR remotely via MQTT payloads from Home Assistant, Node-RED, or other smart home hubs.

## 🌟 Features
* **Dual Trigger Support:** Trigger animations using physical top/bottom sensors, or digitally via MQTT.
* **WLED 0.16+ Ready:** Built entirely on the new WLED v2 Usermod architecture for seamless compiling.
* **Smart Pin Management:** Safely reserves hardware pins to prevent conflicts with LED data lines.
* **Dynamic MQTT Integration:** Seamlessly uses WLED's native MQTT settings—no hardcoded topics!

## 🛠️ Hardware Requirements
* Any ESP32 microcontroller (Classic, S2, S3, or C3).
* Addressable LED strip installed on staircase steps.
* (Optional) 2x PIR motion sensors or ultrasonic distance sensors for physical triggering.

## 🚀 Installation (WLED 0.16+)

Because this usermod uses the modern WLED v0.16 library structure, you do not need to modify any core WLED files to install it.

1. Copy the `MQTT_Animated_Staircase` folder into your WLED `usermods/` directory.
2. Open your `platformio_override.ini` (or `platformio.ini`) file.
3. Add the usermod to your specific board environment:
   ```ini
   custom_usermods = MQTT_Animated_Staircase
   Compile and flash using PlatformIO.

## ⚙️ Configuration

Once flashed, navigate to Config > Usermods in the WLED web interface. You will find the following settings:

*    Top Sensor Pin: The GPIO pin connected to the top stair sensor.

*    Bottom Sensor Pin: The GPIO pin connected to the bottom stair sensor.

(Note: Ensure your standard WLED MQTT settings under Config > Sync Interfaces are filled out and connected to your broker).
📡 MQTT API

This usermod dynamically listens to your configured WLED Device Topic. It appends /staircase/trigger to your root topic.
Trigger the Stairs

*    Topic: [Your_WLED_Device_Topic]/staircase/trigger (Example: wled/stairs/staircase/trigger)

*    Payloads:

*        top - Triggers the animation starting from the top step downwards.

*        bottom - Triggers the animation starting from the bottom step upwards.

*        ON - Triggers a default activation.

Created by watak