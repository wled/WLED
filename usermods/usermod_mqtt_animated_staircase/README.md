# MQTT Animated Staircase Usermod

This usermod is a highly advanced, MQTT-enabled version of the standard WLED Animated Staircase. It allows you to trigger stunning, sequential staircase LED animations using physical motion sensors (PIR/Ultrasonic) OR remotely via MQTT payloads from Home Assistant, Node-RED, or other smart home hubs.

## 🌟 Features
* **Dual Trigger Support:** Trigger animations using physical top/bottom sensors, or digitally via MQTT.
* **WLED 0.16+ Ready:** Built entirely on the new WLED v2 Usermod architecture for seamless compiling.
* **Smart Pin Management:** Safely reserves hardware pins to prevent conflicts with LED data lines.
* **Dynamic MQTT Integration:** Seamlessly uses WLED's native MQTT settings—no hardcoded topics!

## 🛠️ Hardware Requirements
* Any ESP8266 or ESP32 microcontroller (Classic, S2, S3, or C3).
* Addressable LED strip installed on staircase steps.
* (Optional) 2x PIR motion sensors or ultrasonic distance sensors for physical triggering.

## 🚀 Installation (WLED 0.16+)

Because this usermod uses the modern WLED v0.16 library structure, you do not need to modify any core WLED files to install it.

1. Open your `platformio_override.ini` (or `platformio.ini`) file.
2. Add the usermod to your specific board environment:
   ```ini
   custom_usermods = MQTT_Animated_Staircase
   ```
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

* Payloads:

* `top` - Triggers the animation starting from the top step downwards.
* `bottom` - Triggers the animation starting from the bottom step upwards.
* `ON` - Triggers a default activation.

## 🎨 Required WLED Presets

For the usermod to know how your stairs are physically wired and how to animate them, you must create two specific presets in the WLED interface. The usermod dynamically reads these presets to execute the sequential swipe.

### 1. The "Staircase Layout" (Save strictly as Preset ID 1)
This preset teaches the usermod how many steps you have and how many LEDs are on each step.
* Open the WLED UI and go to the **Segments** tab.
* Create a distinct segment for *every single step* on your staircase (e.g., Segment 0: LEDs 0-12, Segment 1: LEDs 12-24, etc.).
* Set your desired colors, brightness, and effects for the stairs.
* Go to the **Presets** tab, click **Create Preset**, and save it explicitly as **ID 1**. *(Check the "Save to ID" box to ensure it forces it to slot 1).*

### 2. The "Blackout" State (Save strictly as Preset ID 3) --> this is to avoid a bug in which the whole strip flashes when it's turned "off". it's in WLED itself, not the usermod
This preset is used by the usermod to instantly clear the stairs before triggering a new directional swipe.
* While your step segments from Preset 1 are still active, change the primary color of all segments to completely **Black** (or toggle the segments off) 
* Go to the **Presets** tab, click **Create Preset**, and save this explicitly as **ID 3**.

## 🏡 Home Assistant Integration

If you use Home Assistant, you can easily integrate your physical staircase sensors and trigger animations via YAML.

### 1. Motion Sensors (`configuration.yaml`)
If you have physical sensors wired to the ESP32, the usermod publishes their state to MQTT. You can read them in Home Assistant by adding this to your `configuration.yaml` (replace `<device_topic>` with your actual WLED MQTT topic):

```yaml
mqtt:
  binary_sensor:
    - unique_id: landing_staircase_motion_sensor
      name: "Landing Staircase Motion Sensor (Top)"
      state_topic: "<device_topic>/motion/0"
      payload_on: "on"
      payload_off: "off"
      qos: 1
      device_class: motion

    - unique_id: lounge_staircase_motion_sensor
      name: "Lounge Staircase Motion Sensor (Bottom)"
      state_topic: "<device_topic>/motion/1"
      payload_on: "on"
      payload_off: "off"
      qos: 1
      device_class: motion


alias: "Staircase – Swipe Down from Landing"
description: "Triggers WLED swipe-down animation via MQTT"
mode: single
triggers:
  - trigger: state
    entity_id: binary_sensor.landing_staircase_motion_sensor
    from: "off"
    to: "on"
conditions:
  - condition: state
    entity_id: light.staircase_light
    state: "off"
actions:
  - action: mqtt.publish
    data:
      topic: "<device_topic>/swipe"
      payload: "on-down"
  - delay:
      seconds: 30

Created by watak